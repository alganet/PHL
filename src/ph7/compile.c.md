# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3523/4623 lines (76.21%)

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
|    3020 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    3022 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8538 |  131 | `	for(;;){` |
|   17078 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2910 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2910 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2888 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   14192 |  140 | `		pBlock = pBlock->pParent;` |
|   14192 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1512 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  591626 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  591628 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  591628 |  162 | `	pBlock->pUserData   = pUserData;` |
|  591628 |  163 | `	pBlock->pGen        = pGen;` |
|  591628 |  164 | `	pBlock->iFlags      = iType;` |
|  591628 |  165 | `	pBlock->pParent     = 0;` |
|  591628 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  591628 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  591628 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  588850 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  588852 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  588852 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  588852 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  588852 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  588852 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  588852 |  200 | `	pGen->pCurrent = pBlock;` |
|  588852 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  284914 |  203 | `		*ppBlock = pBlock;` |
|  142456 |  204 | `	}` |
|  588852 |  205 | `	return SXRET_OK;` |
|  294427 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  588842 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  588844 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  588844 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  588844 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  588842 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  588844 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  588844 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  588844 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  588844 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  588842 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  588844 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  588844 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  588844 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  588844 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  588844 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  588844 |  244 | `	return SXRET_OK;` |
|  294423 |  245 |  |
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
|  179490 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  179492 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  179492 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  179492 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  179492 |  265 | `	return rc;` |
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
|  418582 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  418584 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  768482 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  349900 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  136318 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  213584 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   34096 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  179490 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  179490 |  298 | `		if( pInstr ){` |
|  179490 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  179490 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  179490 |  302 | `			aFix[n].nJumpType = -1;` |
|   89744 |  303 | `		}` |
|   89746 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  418584 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  159342 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  159344 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  159490 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  159342 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  159474 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  159342 |  358 | `	return SXRET_OK;` |
|   79673 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  519798 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  519800 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  519800 |  367 | `	if( pEntry == 0 ){` |
|  257110 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  262692 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  262692 |  371 | `	return SXRET_OK;` |
|  259901 |  372 |  |
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
|  257108 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  257110 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  257110 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  128554 |  387 | `	}` |
|  257110 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   90330 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   90332 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   90332 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   90332 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   90332 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   90332 |  408 | `	return pObj;` |
|   45167 |  409 |  |
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
|   90730 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   90732 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   90732 |  434 | `	sxu32 nIdx = 0;` |
|   90732 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   90332 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   90332 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   90332 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   90332 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   45167 |  445 | `	}else{` |
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
|   90732 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   90732 |  460 | `	return SXRET_OK;` |
|   45367 |  461 |  |
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
|   59318 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   59320 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   59320 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   59320 |  481 | `	zIn  = pStr->zString;` |
|   59320 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   59320 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   59184 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   17082 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17082 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   42104 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   42104 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   42104 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   42148 |  505 | `	for(;;){` |
|   84298 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   42104 |  508 | `			break;` |
|       - |  509 | `		}` |
|   42196 |  510 | `		zCur = zIn;` |
|  667710 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  625516 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   42196 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   42176 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21087 |  517 | `		}` |
|   42196 |  518 | `		zIn++;` |
|   42196 |  519 | `		if( zIn < zEnd ){` |
|     114 |  520 | `			if( zIn[0] == '\\' ){` |
|       - |  521 | `				/* A literal backslash */` |
|      23 |  522 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     103 |  523 | `			}else if( zIn[0] == '\'' ){` |
|       - |  524 | `				/* A single quote */` |
|      11 |  525 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  526 | `			}else{` |
|       - |  527 | `				/* verbatim copy */` |
|      82 |  528 | `				zIn--;` |
|      82 |  529 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      82 |  530 | `				zIn++;` |
|       - |  531 | `			}` |
|      56 |  532 | `		}` |
|       - |  533 | `		/* Advance the stream cursor */` |
|   42196 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   42104 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   42104 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   42104 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21051 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   42104 |  543 | `	return SXRET_OK;` |
|   29661 |  544 |  |
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
|    1616 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1618 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1618 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1618 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1618 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1618 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1618 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1618 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1618 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1618 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1618 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1618 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1618 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   15410 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   15412 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   15412 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   15412 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   15412 |  650 | `	(*pCount)++;` |
|   15412 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   15412 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15412 |  654 | `	return pConstObj;` |
|    7707 |  655 |  |
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
|   14260 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   14262 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   14262 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   14262 |  702 | `	zIn  = pStr->zString;` |
|   14262 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   14262 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   14040 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   14040 |  714 | `	iCons = 0;` |
|    7827 |  715 | `	for(;;){` |
|   23610 |  716 | `		zCur = zIn;` |
|  132946 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  110954 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  110870 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1534 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     767 |  722 | `					break;` |
|       - |  723 | `			}` |
|  109338 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   23610 |  726 | `		if( zIn > zCur ){` |
|   11284 |  727 | `			if( pObj == 0 ){` |
|   11008 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11008 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5503 |  732 | `			}` |
|   11284 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5641 |  734 | `		}` |
|   23610 |  735 | `		if( zIn >= zEnd ){` |
|   14040 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9572 |  738 | `		if( zIn[0] == '\\' ){` |
|    7956 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7956 |  741 | `			zIn++;` |
|    7956 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7956 |  745 | `			if( pObj == 0 ){` |
|    4406 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4406 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2202 |  750 | `			}` |
|    7956 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7956 |  752 | `			switch( zIn[0] ){` |
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
|    3612 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7226 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7226 |  776 | `				break;` |
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
|    7956 |  844 | `			zIn += n;` |
|    7956 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1618 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1532 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     765 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3062 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1532 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     765 |  888 | `				for(;;){` |
|    9721 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7426 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1532 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1532 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1532 |  902 | `				if( zIn >= zEnd ){` |
|      88 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1446 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1438 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1434 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1434 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     718 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1532 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1532 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1532 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1530 |  957 | `				++iCons;` |
|     764 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1618 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   14040 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1236 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     617 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   14040 |  968 | `	return SXRET_OK;` |
|    7132 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   14234 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   14236 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7117 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   14236 |  980 | `	return rc;` |
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
|   16098 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   16100 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   16100 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   16100 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   16100 | 1028 | `	return rc;` |
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
|   24108 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   24110 | 1071 | `	sxi32 iEmitRef = 0;` |
|   24110 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   24110 | 1075 | `	xValidator = 0;` |
|   19487 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   43946 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4972 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   38976 | 1081 | `		pCur = pGen->pIn;` |
|   38976 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   24098 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14880 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14880 | 1090 | `		pKey = pCur;` |
|   14880 | 1091 | `		iNest = 0;` |
|   41466 | 1092 | `		while( pCur < pGen->pIn ){` |
|   27766 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1180 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   26588 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   26550 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   26588 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14880 | 1106 | `		rc = SXERR_EMPTY;` |
|   14880 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   14286 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13702 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14870 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13704 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6851 | 1134 | `		}` |
|   14870 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14868 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14868 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14868 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14868 | 1158 | `		xValidator = 0;` |
|   14868 | 1159 | `		iEmitRef = 0;` |
|   14868 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   24098 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   24098 | 1165 | `	return SXRET_OK;` |
|   12056 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   23960 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   23962 | 1179 | `	pGen->pIn += 2;` |
|   23962 | 1180 | `	pGen->pEnd--;` |
|   11980 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   23962 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
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
|     142 | 1362 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|     144 | 1375 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     144 | 1376 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1377 | `		pGen->pIn++;` |
|     ! 0 | 1378 | `	}` |
|       - | 1379 | `	/* Reserve a constant for the lambda */` |
|     144 | 1380 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     144 | 1381 | `	if( pObj == 0 ){` |
|     ! 0 | 1382 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1383 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1384 | `		return SXERR_ABORT;` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* Generate a unique name */` |
|     144 | 1387 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1388 | `	/* Make sure the generated name is unique */` |
|     144 | 1389 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1390 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1391 | `	}` |
|     144 | 1392 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     144 | 1393 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1394 | `	/* Compile the lambda body */` |
|     144 | 1395 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     144 | 1396 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1397 | `		return SXERR_ABORT;` |
|       - | 1398 | `	}` |
|     144 | 1399 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1400 | `		/* Emit the load closure instruction */` |
|      14 | 1401 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1402 | `	}else{` |
|       - | 1403 | `		/* Emit the load constant instruction */` |
|     132 | 1404 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1405 | `	}` |
|       - | 1406 | `	/* Node successfully compiled */` |
|     144 | 1407 | `	return SXRET_OK;` |
|      73 | 1408 |  |
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
|  808918 | 1524 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1525 |  |
|  808920 | 1526 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1527 | `	sxi32 iVv;` |
|       - | 1528 | `	sxi32 iP1;` |
|       - | 1529 | `	void *p3;` |
|       - | 1530 | `	sxi32 rc;` |
|  808920 | 1531 | `	iVv = -1; /* Variable variable counter */` |
| 1617850 | 1532 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  808932 | 1533 | `		pGen->pIn++;` |
|  808932 | 1534 | `		iVv++;` |
|       2 | 1535 | `	}` |
|  808920 | 1536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1537 | `		/* Invalid variable name */` |
|     ! 0 | 1538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1539 | `		if( rc == SXERR_ABORT ){` |
|       - | 1540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1541 | `			return SXERR_ABORT;` |
|       - | 1542 | `		}` |
|     ! 0 | 1543 | `		return SXRET_OK;` |
|       - | 1544 | `	}` |
|  808920 | 1545 | `	p3  = 0;` |
|  808920 | 1546 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  808904 | 1566 | `		char *zName = 0;` |
|       - | 1567 | `		/* Extract variable name */` |
|  808904 | 1568 | `		pName = &pGen->pIn->sData;` |
|       - | 1569 | `		/* Advance the stream cursor */` |
|  808904 | 1570 | `		pGen->pIn++;` |
|  808904 | 1571 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  808904 | 1572 | `		if( pEntry == 0 ){` |
|       - | 1573 | `			/* Duplicate name */` |
|  116452 | 1574 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  116452 | 1575 | `			if( zName == 0 ){` |
|     ! 0 | 1576 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1577 | `				return SXERR_ABORT;` |
|       - | 1578 | `			}` |
|       - | 1579 | `			/* Install in the hashtable */` |
|  116452 | 1580 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   58227 | 1581 | `		}else{` |
|       - | 1582 | `			/* Name already available */` |
|  692454 | 1583 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1584 | `		}` |
|  808904 | 1585 | `		p3 = (void *)zName;` |
|       - | 1586 | `	}` |
|  808916 | 1587 | `	iP1 = 0;` |
|  808916 | 1588 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  310840 | 1589 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1590 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  304994 | 1591 | `			iP1 = 1;` |
|  152496 | 1592 | `		}` |
|  155419 | 1593 | `	}` |
|       - | 1594 | `	/* Emit the load instruction */` |
|  808916 | 1595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  808928 | 1596 | `	while( iVv > 0 ){` |
|      13 | 1597 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1598 | `		iVv--;` |
|       1 | 1599 | `	}` |
|       - | 1600 | `	/* Node successfully compiled */` |
|  808916 | 1601 | `	return SXRET_OK;` |
|  404461 | 1602 |  |
|       - | 1603 | `/*` |
|       - | 1604 | ` * Load a literal.` |
|       - | 1605 | ` */` |
|  542584 | 1606 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1607 |  |
|  542586 | 1608 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1609 | `	ph7_value *pObj;` |
|       - | 1610 | `	SyString *pStr;` |
|       - | 1611 | `	sxu32 nIdx;` |
|       - | 1612 | `	/* Extract token value */` |
|  542586 | 1613 | `	pStr = &pToken->sData;` |
|       - | 1614 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  542586 | 1615 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   98612 | 1616 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1617 | `			/* NULL constant are always indexed at 0 */` |
|   41940 | 1618 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   41940 | 1619 | `			return SXRET_OK;` |
|   56674 | 1620 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1621 | `			/* TRUE constant are always indexed at 1 */` |
|     472 | 1622 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     472 | 1623 | `			return SXRET_OK;` |
|       2 | 1624 | `		}` |
|  514909 | 1625 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   85664 | 1626 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1627 | `			/* FALSE constant are always indexed at 2 */` |
|   36636 | 1628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   36636 | 1629 | `			return SXRET_OK;` |
|  445215 | 1630 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   75746 | 1631 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1632 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5556 | 1633 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5556 | 1634 | `			if( pObj == 0 ){` |
|     ! 0 | 1635 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1636 | `				return SXERR_ABORT;` |
|       - | 1637 | `			}` |
|    5556 | 1638 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1639 | `			/* Emit the load constant instruction */` |
|    5556 | 1640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5556 | 1641 | `			return SXRET_OK;` |
|  415779 | 1642 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   27982 | 1643 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  414855 | 1659 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11662 | 1660 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  409018 | 1661 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14490 | 1662 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  457974 | 1692 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1693 | `		ph7_value *pLitObj;` |
|       - | 1694 | `		/* Unknown literal,install it in the literal table */` |
|  214600 | 1695 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  214600 | 1696 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|       - | 1699 | `		}` |
|  214600 | 1700 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  214600 | 1701 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  107299 | 1702 | `	}` |
|       - | 1703 | `	/* Emit the load constant instruction */` |
|  457974 | 1704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  457974 | 1705 | `	return SXRET_OK;` |
|  271294 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1709 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1710 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1711 | ` * Otherwise, load the simple literal directly.` |
|       - | 1712 | ` */` |
|  542604 | 1713 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1714 |  |
|       - | 1715 | `	sxi32 rc;` |
|  542606 | 1716 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1717 | `		return SXRET_OK;` |
|       - | 1718 | `	}` |
|       - | 1719 | `	/* Check if this is a multi-token namespace path */` |
|  542606 | 1720 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  542586 | 1770 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  542586 | 1771 | `	return rc;` |
|  271304 | 1772 |  |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1775 | ` */` |
|  542604 | 1776 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1777 |  |
|       - | 1778 | `	sxi32 rc;` |
|  542606 | 1779 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  542606 | 1780 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1781 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1782 | `		return rc;` |
|       - | 1783 | `	}` |
|       - | 1784 | `	/* Node successfully compiled */` |
|  542606 | 1785 | `	return SXRET_OK;` |
|  271304 | 1786 |  |
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
|    2882 | 1938 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1939 |  |
|    2884 | 1940 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16924 | 1941 | `	while( pBlock && pBlock != pTarget ){` |
|   14042 | 1942 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   14042 | 1954 | `		pBlock = pBlock->pParent;` |
|       2 | 1955 | `	}` |
|    2884 | 1956 |  |
|    2816 | 1957 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1958 |  |
|       - | 1959 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1960 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1961 | `	sxu32 nLineLocal;` |
|       - | 1962 | `	sxi32 rc;` |
|    2818 | 1963 | `	nLineLocal = pGen->pIn->nLine;` |
|    2818 | 1964 | `	iLevel = 0;` |
|       - | 1965 | `	/* Jump the 'continue' keyword */` |
|    2818 | 1966 | `	pGen->pIn++;` |
|    2818 | 1967 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2818 | 1978 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2818 | 1979 | `	if( pLoop == 0 ){` |
|       - | 1980 | `		/* Illegal continue */` |
|      11 | 1981 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1982 | `		if( rc == SXERR_ABORT ){` |
|       - | 1983 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1984 | `			return SXERR_ABORT;` |
|       - | 1985 | `		}` |
|       6 | 1986 | `	}else{` |
|    2808 | 1987 | `		sxu32 nInstrIdx = 0;` |
|       - | 1988 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2808 | 1989 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2808 | 1990 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2804 | 2002 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2804 | 2003 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2004 | `				JumpFixup sJumpFix;` |
|       - | 2005 | `				/* Post-continue */` |
|      10 | 2006 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      10 | 2007 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      10 | 2008 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2009 | `			}` |
|       - | 2010 | `		}` |
|       - | 2011 | `	}` |
|    2818 | 2012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2013 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2014 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2015 | `	}` |
|       - | 2016 | `	/* Statement successfully compiled */` |
|    2818 | 2017 | `	return SXRET_OK;` |
|    1410 | 2018 |  |
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
|  305290 | 2280 | `static sxi32 PH7_CompileBlock(` |
|       - | 2281 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2282 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2283 | `	)` |
|       2 | 2284 |  |
|       - | 2285 | `	sxi32 rc;` |
|       - | 2286 | `	sxu32 nLine;` |
|  305292 | 2287 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  303940 | 2288 | `		nLine = pGen->pIn->nLine;` |
|  303940 | 2289 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  303940 | 2290 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2291 | `			return SXERR_ABORT;` |
|       - | 2292 | `		}` |
|  303940 | 2293 | `		pGen->pIn++;` |
|       - | 2294 | `		/* Compile until we hit the closing braces '}' */` |
|  419718 | 2295 | `		for(;;){` |
|  839438 | 2296 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  839418 | 2307 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2308 | `				/* Closing braces found,break immediately*/` |
|  303920 | 2309 | `				pGen->pIn++;` |
|  303920 | 2310 | `				break;` |
|       - | 2311 | `			}` |
|       - | 2312 | `			/* Compile a single statement */` |
|  535500 | 2313 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  535500 | 2314 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2315 | `				return SXERR_ABORT;` |
|       - | 2316 | `			}` |
|       2 | 2317 | `		}` |
|  303940 | 2318 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  153323 | 2319 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1354 | 2363 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1354 | 2364 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `			return SXERR_ABORT;` |
|       - | 2366 | `		}` |
|       - | 2367 | `	}` |
|       - | 2368 | `	/* Jump trailing semi-colons ';' */` |
|  305292 | 2369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `	}` |
|  305292 | 2372 | `	return SXRET_OK;` |
|  152647 | 2373 |  |
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
|   11206 | 2393 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2394 |  |
|   11208 | 2395 | `	GenBlock *pWhileBlock = 0;` |
|   11208 | 2396 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2397 | `	sxu32 nFalseJump;` |
|       - | 2398 | `	sxu32 nLine;` |
|       - | 2399 | `	sxi32 rc;` |
|   11208 | 2400 | `	nLine = pGen->pIn->nLine;` |
|       - | 2401 | `	/* Jump the 'while' keyword */` |
|   11208 | 2402 | `	pGen->pIn++;` |
|   11208 | 2403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2404 | `		/* Syntax error */` |
|     ! 0 | 2405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2406 | `		if( rc == SXERR_ABORT ){` |
|       - | 2407 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2408 | `			return SXERR_ABORT;` |
|       - | 2409 | `		}` |
|     ! 0 | 2410 | `		goto Synchronize;` |
|       - | 2411 | `	}` |
|       - | 2412 | `	/* Jump the left parenthesis '(' */` |
|   11208 | 2413 | `	pGen->pIn++;` |
|       - | 2414 | `	/* Create the loop block */` |
|   11208 | 2415 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11208 | 2416 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2417 | `		return SXERR_ABORT;` |
|       - | 2418 | `	}` |
|       - | 2419 | `	/* Delimit the condition */` |
|   11208 | 2420 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11208 | 2421 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2422 | `		/* Empty expression */` |
|       3 | 2423 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2424 | `		if( rc == SXERR_ABORT ){` |
|       - | 2425 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2426 | `			return SXERR_ABORT;` |
|       - | 2427 | `		}` |
|       1 | 2428 | `	}` |
|       - | 2429 | `	/* Swap token streams */` |
|   11208 | 2430 | `	pTmp = pGen->pEnd;` |
|   11208 | 2431 | `	pGen->pEnd = pEnd;` |
|       - | 2432 | `	/* Compile the expression */` |
|   11208 | 2433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11208 | 2434 | `	if( rc == SXERR_ABORT ){` |
|       - | 2435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2436 | `		return SXERR_ABORT;` |
|       - | 2437 | `	}` |
|       - | 2438 | `	/* Update token stream */` |
|   11208 | 2439 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2440 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2441 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2442 | `			return SXERR_ABORT;` |
|       - | 2443 | `		}` |
|     ! 0 | 2444 | `		pGen->pIn++;` |
|     ! 0 | 2445 | `	}` |
|       - | 2446 | `	/* Synchronize pointers */` |
|   11208 | 2447 | `	pGen->pIn  = &pEnd[1];` |
|   11208 | 2448 | `	pGen->pEnd = pTmp;` |
|       - | 2449 | `	/* Emit the false jump */` |
|   11208 | 2450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11208 | 2452 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2453 | `	/* Compile the loop body */` |
|   11208 | 2454 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11208 | 2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2456 | `		return SXERR_ABORT;` |
|       - | 2457 | `	}` |
|       - | 2458 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11208 | 2459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2460 | `	/* Fix all jumps now the destination is resolved */` |
|   11208 | 2461 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2462 | `	/* Release the loop block */` |
|   11208 | 2463 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2464 | `	/* Statement successfully compiled */` |
|   11208 | 2465 | `	return SXRET_OK;` |
|     ! 0 | 2466 | `Synchronize:` |
|       - | 2467 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2468 | `	 * compiling this erroneous block.` |
|       - | 2469 | `	 */` |
|     ! 0 | 2470 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2471 | `		pGen->pIn++;` |
|     ! 0 | 2472 | `	}` |
|     ! 0 | 2473 | `	return SXRET_OK;` |
|    5605 | 2474 |  |
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
|   11190 | 2622 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2623 |  |
|   11192 | 2624 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11192 | 2625 | `	GenBlock *pForBlock = 0;` |
|       - | 2626 | `	sxu32 nFalseJump;` |
|       - | 2627 | `	sxu32 nLine;` |
|       - | 2628 | `	sxi32 rc;` |
|   11192 | 2629 | `	nLine = pGen->pIn->nLine;` |
|       - | 2630 | `	/* Jump the 'for' keyword */` |
|   11192 | 2631 | `	pGen->pIn++;` |
|   11192 | 2632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2633 | `		/* Syntax error */` |
|     ! 0 | 2634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2635 | `		if( rc == SXERR_ABORT ){` |
|       - | 2636 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2637 | `			return SXERR_ABORT;` |
|       - | 2638 | `		}` |
|     ! 0 | 2639 | `		return SXRET_OK;` |
|       - | 2640 | `	}` |
|       - | 2641 | `	/* Jump the left parenthesis '(' */` |
|   11192 | 2642 | `	pGen->pIn++;` |
|       - | 2643 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11192 | 2644 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11192 | 2645 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   11192 | 2660 | `	pTmp = pGen->pEnd;` |
|   11192 | 2661 | `	pGen->pEnd = pEnd;` |
|       - | 2662 | `	/* Compile initialization expressions if available */` |
|   11192 | 2663 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2664 | `	/* Pop operand lvalues */` |
|   11192 | 2665 | `	if( rc == SXERR_ABORT ){` |
|       - | 2666 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2667 | `		return SXERR_ABORT;` |
|   11192 | 2668 | `	}else if( rc != SXERR_EMPTY ){` |
|   11190 | 2669 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5594 | 2670 | `	}` |
|   11192 | 2671 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11192 | 2682 | `	pGen->pIn++;` |
|       - | 2683 | `	/* Create the loop block */` |
|   11192 | 2684 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11192 | 2685 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2686 | `		return SXERR_ABORT;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Deffer continue jumps */` |
|   11192 | 2689 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2690 | `	/* Compile the condition */` |
|   11192 | 2691 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11192 | 2692 | `	if( rc == SXERR_ABORT ){` |
|       - | 2693 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2694 | `		return SXERR_ABORT;` |
|   11192 | 2695 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2696 | `		/* Emit the false jump */` |
|   11190 | 2697 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2698 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11190 | 2699 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5594 | 2700 | `	}` |
|   11192 | 2701 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11188 | 2712 | `	pGen->pIn++;` |
|       - | 2713 | `	/* Save the post condition stream */` |
|   11188 | 2714 | `	pPostStart = pGen->pIn;` |
|       - | 2715 | `	/* Compile the loop body */` |
|   11188 | 2716 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11188 | 2717 | `	pGen->pEnd = pTmp;` |
|   11188 | 2718 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11188 | 2719 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2720 | `		return SXERR_ABORT;` |
|       - | 2721 | `	}` |
|       - | 2722 | `	/* Fix post-continue jumps */` |
|   11188 | 2723 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   11188 | 2739 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2740 | `		pPostStart++;` |
|     ! 0 | 2741 | `	}` |
|   11188 | 2742 | `	if( pPostStart < pEnd ){` |
|       - | 2743 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11188 | 2744 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11188 | 2745 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11188 | 2746 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2747 | `			/* Syntax error */` |
|     ! 0 | 2748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2749 | `			if( rc == SXERR_ABORT ){` |
|       - | 2750 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2751 | `				return SXERR_ABORT;` |
|       - | 2752 | `			}` |
|     ! 0 | 2753 | `			return SXRET_OK;` |
|       - | 2754 | `		}` |
|   11188 | 2755 | `		RE_SWAP_DELIMITER(pGen);` |
|   11188 | 2756 | `		if( rc == SXERR_ABORT ){` |
|       - | 2757 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2758 | `			return SXERR_ABORT;` |
|   11188 | 2759 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2760 | `			/* Pop operand lvalue */` |
|   11188 | 2761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5593 | 2762 | `		}` |
|    5593 | 2763 | `	}` |
|       - | 2764 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11188 | 2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2766 | `	/* Fix all jumps now the destination is resolved */` |
|   11188 | 2767 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2768 | `	/* Release the loop block */` |
|   11188 | 2769 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2770 | `	/* Statement successfully compiled */` |
|   11188 | 2771 | `	return SXRET_OK;` |
|    5597 | 2772 |  |
|       - | 2773 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2774 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2775 | ` * are allowed.` |
|       - | 2776 | ` */` |
|    5934 | 2777 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2778 |  |
|    5936 | 2779 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5936 | 2780 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2781 | `		/* Unexpected expression */` |
|     ! 0 | 2782 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2783 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2784 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2785 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|    5936 | 2788 | `	return rc;` |
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
|    3008 | 2816 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2817 |  |
|    3010 | 2818 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3010 | 2819 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3010 | 2820 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2821 | `	ph7_foreach_info *pInfo;` |
|       - | 2822 | `	sxu32 nFalseJump;` |
|       - | 2823 | `	VmInstr *pInstr;` |
|       - | 2824 | `	sxu32 nLine;` |
|       - | 2825 | `	sxi32 rc;` |
|    3010 | 2826 | `	nLine = pGen->pIn->nLine;` |
|       - | 2827 | `	/* Jump the 'foreach' keyword */` |
|    3010 | 2828 | `	pGen->pIn++;` |
|    3010 | 2829 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2830 | `		/* Syntax error */` |
|     ! 0 | 2831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2832 | `		if( rc == SXERR_ABORT ){` |
|       - | 2833 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2834 | `			return SXERR_ABORT;` |
|       - | 2835 | `		}` |
|     ! 0 | 2836 | `		goto Synchronize;` |
|       - | 2837 | `	}` |
|       - | 2838 | `	/* Jump the left parenthesis '(' */` |
|    3010 | 2839 | `	pGen->pIn++;` |
|       - | 2840 | `	/* Create the loop block */` |
|    3010 | 2841 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3010 | 2842 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2843 | `		return SXERR_ABORT;` |
|       - | 2844 | `	}` |
|       - | 2845 | `	/* Delimit the expression */` |
|    3010 | 2846 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3010 | 2847 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    3010 | 2862 | `	pCur = pGen->pIn;` |
|   20228 | 2863 | `	while( pCur < pEnd ){` |
|   20228 | 2864 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3020 | 2865 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3020 | 2866 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2867 | `				/* Break with the first 'as' found */` |
|    3010 | 2868 | `				break;` |
|       - | 2869 | `			}` |
|       5 | 2870 | `		}` |
|       - | 2871 | `		/* Advance the stream cursor */` |
|   17220 | 2872 | `		pCur++;` |
|       2 | 2873 | `	}` |
|    3010 | 2874 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2876 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2877 | `		if( rc == SXERR_ABORT ){` |
|       - | 2878 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2879 | `			return SXERR_ABORT;` |
|       - | 2880 | `		}` |
|     ! 0 | 2881 | `		goto Synchronize;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Swap token streams */` |
|    3010 | 2884 | `	pTmp = pGen->pEnd;` |
|    3010 | 2885 | `	pGen->pEnd = pCur;` |
|    3010 | 2886 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3010 | 2887 | `	if( rc == SXERR_ABORT ){` |
|       - | 2888 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2889 | `		return SXERR_ABORT;` |
|       - | 2890 | `	}` |
|       - | 2891 | `	/* Update token stream */` |
|    3010 | 2892 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2893 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2894 | `		if( rc == SXERR_ABORT ){` |
|       - | 2895 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2896 | `			return SXERR_ABORT;` |
|       - | 2897 | `		}` |
|     ! 0 | 2898 | `		pGen->pIn++;` |
|     ! 0 | 2899 | `	}` |
|    3010 | 2900 | `	pCur++; /* Jump the 'as' keyword */` |
|    3010 | 2901 | `	pGen->pIn = pCur;` |
|    3010 | 2902 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2903 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2905 | `			return SXERR_ABORT;` |
|       - | 2906 | `		}` |
|     ! 0 | 2907 | `	}` |
|       - | 2908 | `	/* Create the foreach context */` |
|    3010 | 2909 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3010 | 2910 | `	if( pInfo == 0 ){` |
|     ! 0 | 2911 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2912 | `		return SXERR_ABORT;` |
|       - | 2913 | `	}` |
|       - | 2914 | `	/* Zero the structure */` |
|    3010 | 2915 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2916 | `	/* Initialize structure fields */` |
|    3010 | 2917 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2918 | `	/* Check if we have a key field */` |
|    9066 | 2919 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6058 | 2920 | `		pCur++;` |
|       2 | 2921 | `	}` |
|    3010 | 2922 | `	if( pCur < pEnd ){` |
|       - | 2923 | `		/* Compile the expression holding the key name */` |
|    2936 | 2924 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2925 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2926 | `			if( rc == SXERR_ABORT ){` |
|       - | 2927 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2928 | `				return SXERR_ABORT;` |
|       - | 2929 | `			}` |
|     ! 0 | 2930 | `		}else{` |
|    2936 | 2931 | `			pGen->pEnd = pCur;` |
|    2936 | 2932 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2936 | 2933 | `			if( rc == SXERR_ABORT ){` |
|       - | 2934 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2935 | `				return SXERR_ABORT;` |
|       - | 2936 | `			}` |
|    2936 | 2937 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2936 | 2938 | `			if( pInstr->p3 ){` |
|       - | 2939 | `				/* Record key name */` |
|    2936 | 2940 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1467 | 2941 | `			}` |
|    2936 | 2942 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2943 | `		}` |
|    2936 | 2944 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1467 | 2945 | `	}` |
|    3010 | 2946 | `	pGen->pEnd = pEnd;` |
|    3010 | 2947 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2948 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2949 | `		if( rc == SXERR_ABORT ){` |
|       - | 2950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2951 | `			return SXERR_ABORT;` |
|       - | 2952 | `		}` |
|     ! 0 | 2953 | `		goto Synchronize;` |
|       - | 2954 | `	}` |
|    3010 | 2955 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      12 | 2956 | `		pGen->pIn++;` |
|       - | 2957 | `		/* Pass by reference  */` |
|      12 | 2958 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 2959 | `	}` |
|       - | 2960 | `	/* Check if the value target is list() */` |
|    3010 | 2961 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    3002 | 3004 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3002 | 3005 | `		if( rc == SXERR_ABORT ){` |
|       - | 3006 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3007 | `			return SXERR_ABORT;` |
|       - | 3008 | `		}` |
|    3002 | 3009 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3002 | 3010 | `		if( pInstr->p3 ){` |
|       - | 3011 | `			/* Record value name */` |
|    3002 | 3012 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1500 | 3013 | `		}` |
|       - | 3014 | `	}` |
|       - | 3015 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3008 | 3016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3017 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3008 | 3018 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3019 | `	/* Record the first instruction to execute */` |
|    3008 | 3020 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3021 | `	/* Emit the FOREACH_STEP instruction */` |
|    3008 | 3022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3023 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3008 | 3024 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3025 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3008 | 3026 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    3008 | 3050 | `	pGen->pIn = &pEnd[1];` |
|    3008 | 3051 | `	pGen->pEnd = pTmp;` |
|    3008 | 3052 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3008 | 3053 | `	if( rc == SXERR_ABORT ){` |
|       - | 3054 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3055 | `		return SXERR_ABORT;` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3008 | 3058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3059 | `	/* Fix all jumps now the destination is resolved */` |
|    3008 | 3060 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3061 | `	/* Release the loop block */` |
|    3008 | 3062 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3063 | `	/* Statement successfully compiled */` |
|    3008 | 3064 | `	return SXRET_OK;` |
|       1 | 3065 | `Synchronize:` |
|       - | 3066 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3067 | `	 * compiling this erroneous block.` |
|       - | 3068 | `	 */` |
|       3 | 3069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3070 | `		pGen->pIn++;` |
|     ! 0 | 3071 | `	}` |
|       3 | 3072 | `	return SXRET_OK;` |
|    1506 | 3073 |  |
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
|  111272 | 3106 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3107 |  |
|  111274 | 3108 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  111274 | 3109 | `	GenBlock *pCondBlock = 0;` |
|       - | 3110 | `	sxu32 nJumpIdx;` |
|       - | 3111 | `	sxu32 nKeyID;` |
|       - | 3112 | `	sxi32 rc;` |
|       - | 3113 | `	/* Jump the 'if' keyword */` |
|  111274 | 3114 | `	pGen->pIn++;` |
|  111274 | 3115 | `	pToken = pGen->pIn;` |
|       - | 3116 | `	/* Create the conditional block */` |
|  111274 | 3117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  111274 | 3118 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3119 | `		return SXERR_ABORT;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   61200 | 3122 | `	for(;;){` |
|  122402 | 3123 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  122402 | 3136 | `		pToken++;` |
|       - | 3137 | `		/* Delimit the condition */` |
|  122402 | 3138 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  122402 | 3139 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  122402 | 3152 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3153 | `		/* Compile the condition */` |
|  122402 | 3154 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3155 | `		/* Update token stream */` |
|  122402 | 3156 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3157 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3158 | `			pGen->pIn++;` |
|     ! 0 | 3159 | `		}` |
|  122402 | 3160 | `		pGen->pIn  = &pEnd[1];` |
|  122402 | 3161 | `		pGen->pEnd = pTmp;` |
|  122402 | 3162 | `		if( rc == SXERR_ABORT ){` |
|       - | 3163 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Emit the false jump */` |
|  122402 | 3167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3168 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  122402 | 3169 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3170 | `		/* Compile the body */` |
|  122402 | 3171 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  122402 | 3172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3173 | `			return SXERR_ABORT;` |
|       - | 3174 | `		}` |
|  122402 | 3175 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   32939 | 3176 | `			break;` |
|       - | 3177 | `		}` |
|       - | 3178 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   56528 | 3179 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   56528 | 3180 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   36352 | 3181 | `			break;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Emit the unconditional jump */` |
|   20178 | 3184 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3185 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   20178 | 3186 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   20178 | 3187 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14602 | 3188 | `			pToken = &pGen->pIn[1];` |
|   14602 | 3189 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5582 | 3190 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4526 | 3191 | `					break;` |
|       - | 3192 | `			}` |
|    5554 | 3193 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2776 | 3194 | `		}` |
|   11130 | 3195 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3196 | `		/* Synchronize cursors */` |
|   11130 | 3197 | `		pToken = pGen->pIn;` |
|       - | 3198 | `		/* Fix the false jump */` |
|   11130 | 3199 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3200 | `	} /* For(;;) */` |
|       - | 3201 | `	/* Fix the false jump */` |
|  111274 | 3202 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  111274 | 3203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   45398 | 3204 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3205 | `			/* Compile the else block */` |
|    9050 | 3206 | `			pGen->pIn++;` |
|    9050 | 3207 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9050 | 3208 | `			if( rc == SXERR_ABORT ){` |
|       - | 3209 |  |
|     ! 0 | 3210 | `				return SXERR_ABORT;` |
|       - | 3211 | `			}` |
|    4524 | 3212 | `	}` |
|  111274 | 3213 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3214 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  111274 | 3215 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3216 | `	/* Release the conditional block */` |
|  111274 | 3217 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3218 | `	/* Statement successfully compiled */` |
|  111274 | 3219 | `	return SXRET_OK;` |
|     ! 0 | 3220 | `Synchronize:` |
|       - | 3221 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3222 | `	 */` |
|     ! 0 | 3223 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3224 | `		pGen->pIn++;` |
|     ! 0 | 3225 | `	}` |
|     ! 0 | 3226 | `	return SXRET_OK;` |
|   55638 | 3227 |  |
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
|  161546 | 3321 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3322 |  |
|  161548 | 3323 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3324 | `	sxi32 rc;` |
|       - | 3325 | `	/* Jump the 'return' keyword */` |
|  161548 | 3326 | `	pGen->pIn++;` |
|  161548 | 3327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3328 | `		/* Compile the expression */` |
|  161526 | 3329 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  161526 | 3330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3331 | `			return SXERR_ABORT;` |
|  161526 | 3332 | `		}else if(rc != SXERR_EMPTY ){` |
|  161526 | 3333 | `			nRet = 1;` |
|   80762 | 3334 | `		}` |
|   80762 | 3335 | `	}` |
|       - | 3336 | `	/* Emit the done instruction */` |
|  161548 | 3337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  161548 | 3338 | `	return SXRET_OK;` |
|   80775 | 3339 |  |
|       - | 3340 | `/*` |
|       - | 3341 | ` * Compile a yield expression.` |
|       - | 3342 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3343 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3344 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3345 | ` */` |
|      32 | 3346 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       1 | 3347 |  |
|       - | 3348 | `	SyToken *pTmp, *pSplit;` |
|      33 | 3349 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      33 | 3350 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3351 | `	sxi32 rc;` |
|      16 | 3352 | `	(void)iCompileFlag;` |
|       - | 3353 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      33 | 3354 | `	pGen->pIn++;` |
|       - | 3355 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3356 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      33 | 3357 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3358 | `		/* Bare yield — no value */` |
|     ! 0 | 3359 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3360 | `		return SXRET_OK;` |
|       - | 3361 | `	}` |
|       - | 3362 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      33 | 3363 | `	pSplit = 0;` |
|       - | 3364 | `	{` |
|      33 | 3365 | `		SyToken *pCur = pGen->pIn;` |
|      33 | 3366 | `		sxi32 nNest = 0;` |
|      77 | 3367 | `		while( pCur < pGen->pEnd ){` |
|      51 | 3368 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3369 | `				nNest++;` |
|      51 | 3370 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3371 | `				nNest--;` |
|      51 | 3372 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3373 | `				pSplit = pCur;` |
|       7 | 3374 | `				break;` |
|       - | 3375 | `			}` |
|      45 | 3376 | `			pCur++;` |
|       1 | 3377 | `		}` |
|       - | 3378 | `	}` |
|      33 | 3379 | `	pTmp = pGen->pEnd;` |
|      33 | 3380 | `	if( pSplit ){` |
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
|      27 | 3393 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      27 | 3394 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      27 | 3395 | `		if( rc != SXERR_EMPTY ){` |
|      27 | 3396 | `			iP1 = 1;` |
|      13 | 3397 | `		}` |
|       - | 3398 | `	}` |
|      33 | 3399 | `	pGen->pEnd = pTmp;` |
|      33 | 3400 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      33 | 3401 | `	return SXRET_OK;` |
|      17 | 3402 |  |
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
|   10094 | 3430 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3431 |  |
|   10096 | 3432 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3433 | `	sxi32 rc;` |
|       - | 3434 | `	/* Jump the 'echo' keyword */` |
|   10096 | 3435 | `	pGen->pIn++;` |
|       - | 3436 | `	/* Compile arguments one after one */` |
|   10096 | 3437 | `	pTmp = pGen->pEnd;` |
|   20578 | 3438 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   10484 | 3439 | `		if( pGen->pIn < pNext ){` |
|   10484 | 3440 | `			pGen->pEnd = pNext;` |
|   10484 | 3441 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   10484 | 3442 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3443 | `				return SXERR_ABORT;` |
|   10484 | 3444 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3445 | `				/* Emit the consume instruction */` |
|   10460 | 3446 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5229 | 3447 | `			}` |
|    5241 | 3448 | `		}` |
|       - | 3449 | `		/* Jump trailing commas */` |
|   10872 | 3450 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3451 | `			pNext++;` |
|       2 | 3452 | `		}` |
|   10484 | 3453 | `		pGen->pIn = pNext;` |
|       2 | 3454 | `	}` |
|       - | 3455 | `	/* Restore token stream */` |
|   10096 | 3456 | `	pGen->pEnd = pTmp;` |
|   10096 | 3457 | `	return SXRET_OK;` |
|    5049 | 3458 |  |
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
|  330848 | 3620 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
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
|  330850 | 3631 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  330744 | 3632 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3633 | `	}` |
|     107 | 3634 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     107 | 3635 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3636 | `		return nOrigIdx;` |
|       - | 3637 | `	}` |
|     107 | 3638 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     107 | 3639 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3640 | `	/* Skip if already qualified (contains backslash) */` |
|     107 | 3641 | `	hasNsSep = 0;` |
|     521 | 3642 | `	for( k = 0; k < nLit; k++ ){` |
|     465 | 3643 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     208 | 3644 | `	}` |
|     107 | 3645 | `	if( hasNsSep ){` |
|      51 | 3646 | `		return nOrigIdx;` |
|       - | 3647 | `	}` |
|       - | 3648 | `	/* Build the qualified name into sWorker */` |
|      57 | 3649 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3650 | `	/* Check use imports first */` |
|      57 | 3651 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      57 | 3652 | `	if( pImport ){` |
|      15 | 3653 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 | 3654 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       8 | 3655 | `	}else{` |
|       - | 3656 | `		/* Prepend current namespace */` |
|      43 | 3657 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      43 | 3658 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      43 | 3659 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3660 | `	}` |
|       - | 3661 | `	/* Look up or create a new literal for the qualified name */` |
|      57 | 3662 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      57 | 3663 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      17 | 3664 | `		return nNewIdx; /* Already interned */` |
|       - | 3665 | `	}` |
|      41 | 3666 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      41 | 3667 | `	if( pNew == 0 ){` |
|     ! 0 | 3668 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3669 | `	}` |
|      41 | 3670 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      41 | 3671 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      41 | 3672 | `	return nNewIdx;` |
|  165426 | 3673 |  |
|       - | 3674 | `/*` |
|       - | 3675 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3676 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3677 | ` */` |
|   22358 | 3678 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3679 |  |
|       - | 3680 | `	SyHashEntry *pImport;` |
|       - | 3681 | `	/* Check use imports first */` |
|   22360 | 3682 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   22360 | 3683 | `	if( pImport ){` |
|       7 | 3684 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3685 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3686 | `		return;` |
|       - | 3687 | `	}` |
|       - | 3688 | `	/* Prepend current namespace if active */` |
|   22354 | 3689 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3690 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3691 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3692 | `	}` |
|   22354 | 3693 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   11181 | 3694 |  |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3697 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3698 | ` * The caller must release pOut when done.` |
|       - | 3699 | ` */` |
|   42102 | 3700 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3701 |  |
|   42104 | 3702 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3703 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3704 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3705 | `	}` |
|   42104 | 3706 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   42104 | 3707 |  |
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
|      56 | 3754 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3755 |  |
|       - | 3756 | `	sxu32 nLine;` |
|       - | 3757 | `	sxi32 rc;` |
|      57 | 3758 | `	nLine = pGen->pIn->nLine;` |
|      57 | 3759 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3760 | `	/* Reset namespace and clear previous use imports */` |
|      57 | 3761 | `	SyBlobReset(&pGen->sNamespace);` |
|      57 | 3762 | `	SyHashRelease(&pGen->hUseImports);` |
|      57 | 3763 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      57 | 3764 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3765 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3766 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3767 | `		return SXRET_OK;` |
|       - | 3768 | `	}` |
|      57 | 3769 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3770 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3771 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3772 | `		return SXRET_OK;` |
|       - | 3773 | `	}` |
|      57 | 3774 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3775 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3776 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3777 | `		return SXRET_OK;` |
|       - | 3778 | `	}` |
|       - | 3779 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     143 | 3780 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      87 | 3781 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3782 | `			/* Append backslash separator */` |
|      17 | 3783 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3784 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3785 | `			}` |
|       9 | 3786 | `		}else{` |
|       - | 3787 | `			/* Append identifier */` |
|      71 | 3788 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3789 | `		}` |
|      87 | 3790 | `		pGen->pIn++;` |
|       1 | 3791 | `	}` |
|       - | 3792 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3793 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3794 | `	{` |
|      57 | 3795 | `		char *zNsDup = 0;` |
|      57 | 3796 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      82 | 3797 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      54 | 3798 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      27 | 3799 | `		}` |
|      57 | 3800 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3801 | `	}` |
|      57 | 3802 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3803 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3804 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3805 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3807 | `			return SXERR_ABORT;` |
|       - | 3808 | `		}` |
|       2 | 3809 | `	}` |
|      57 | 3810 | `	return SXRET_OK;` |
|      29 | 3811 |  |
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
|      30 | 3826 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3827 |  |
|       - | 3828 | `	sxu32 nLine;` |
|       - | 3829 | `	sxi32 rc;` |
|       - | 3830 | `	SyBlob sPath;` |
|       - | 3831 | `	SyString sAlias;` |
|       - | 3832 | `	SyToken *pLast;` |
|       - | 3833 | `	char *zDup;` |
|      31 | 3834 | `	nLine = pGen->pIn->nLine;` |
|      31 | 3835 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      31 | 3836 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3837 | `	/* Process one or more use declarations separated by commas */` |
|      16 | 3838 | `	for(;;){` |
|      33 | 3839 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3840 | `			break;` |
|       - | 3841 | `		}` |
|      33 | 3842 | `		SyBlobReset(&sPath);` |
|      33 | 3843 | `		pLast = 0;` |
|       - | 3844 | `		/* Collect the full namespace path */` |
|     133 | 3845 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     101 | 3846 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      65 | 3847 | `				pLast = pGen->pIn;` |
|      65 | 3848 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      37 | 3849 | `					SyBlobAppend(&sPath,"\\",1);` |
|      18 | 3850 | `				}` |
|      65 | 3851 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      32 | 3852 | `			}` |
|     101 | 3853 | `			pGen->pIn++;` |
|       1 | 3854 | `		}` |
|      33 | 3855 | `		if( pLast == 0 ){` |
|       - | 3856 | `			/* Empty path */` |
|       5 | 3857 | `			break;` |
|       - | 3858 | `		}` |
|       - | 3859 | `		/* Default alias is the last component of the path */` |
|      29 | 3860 | `		sAlias = pLast->sData;` |
|       - | 3861 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      28 | 3862 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      18 | 3863 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       7 | 3864 | `			pGen->pIn++; /* Jump 'as' */` |
|       7 | 3865 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       7 | 3866 | `				sAlias = pGen->pIn->sData;` |
|       7 | 3867 | `				pGen->pIn++;` |
|       3 | 3868 | `			}` |
|       3 | 3869 | `		}` |
|       - | 3870 | `		/* Check for duplicate import alias */` |
|      29 | 3871 | `		if( SyHashGet(&pGen->hUseImports,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 3872 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3873 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 3874 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 3875 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3876 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3877 | `				return SXERR_ABORT;` |
|       - | 3878 | `			}` |
|       2 | 3879 | `		}` |
|       - | 3880 | `		/* Register the import: alias -> FQN.` |
|       - | 3881 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3882 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3883 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      43 | 3884 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      28 | 3885 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      29 | 3886 | `		if( zDup ){` |
|       - | 3887 | `			char *zAliasDup;` |
|      29 | 3888 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3889 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      29 | 3890 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      29 | 3891 | `			if( zAliasDup ){` |
|      29 | 3892 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      14 | 3893 | `			}` |
|      14 | 3894 | `		}` |
|       - | 3895 | `		/* Check for comma (multiple use declarations) */` |
|      29 | 3896 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3897 | `			pGen->pIn++;` |
|       2 | 3898 | `		}else{` |
|      14 | 3899 | `			break;` |
|       - | 3900 | `		}` |
|       1 | 3901 | `	}` |
|      31 | 3902 | `	SyBlobRelease(&sPath);` |
|      31 | 3903 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3904 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3905 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3906 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3907 | `			return SXERR_ABORT;` |
|       - | 3908 | `		}` |
|       1 | 3909 | `	}` |
|      31 | 3910 | `	return SXRET_OK;` |
|      16 | 3911 |  |
|       - | 3912 | `/*` |
|       - | 3913 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3914 | ` *` |
|       - | 3915 | ` * According to the PHP language reference manual.` |
|       - | 3916 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3917 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3918 | ` *  declare (directive)` |
|       - | 3919 | ` *   statement` |
|       - | 3920 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3921 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3922 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3923 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3924 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3925 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3926 | ` * <?php` |
|       - | 3927 | ` * // these are the same:` |
|       - | 3928 | ` * // you can use this:` |
|       - | 3929 | ` * declare(ticks=1) {` |
|       - | 3930 | ` *   // entire script here` |
|       - | 3931 | ` * }` |
|       - | 3932 | ` * // or you can use this:` |
|       - | 3933 | ` * declare(ticks=1);` |
|       - | 3934 | ` * // entire script here` |
|       - | 3935 | ` * ?>` |
|       - | 3936 | ` *` |
|       - | 3937 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3938 | ` */` |
|       8 | 3939 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3940 |  |
|       9 | 3941 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3942 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3943 | `	sxi32 rc;` |
|       9 | 3944 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3945 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3946 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3947 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3948 | `			return SXERR_ABORT;` |
|       - | 3949 | `		}` |
|       5 | 3950 | `		goto Synchro;` |
|       - | 3951 | `	}` |
|       5 | 3952 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3953 | `	/* Delimit the directive */` |
|       5 | 3954 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3955 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3956 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3957 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3958 | `			return SXERR_ABORT;` |
|       - | 3959 | `		}` |
|     ! 0 | 3960 | `		return SXRET_OK;` |
|       - | 3961 | `	}` |
|       - | 3962 | `	/* Update the cursor */` |
|       5 | 3963 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3964 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3965 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3966 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3967 | `			return SXERR_ABORT;` |
|       - | 3968 | `		}` |
|     ! 0 | 3969 | `	}` |
|       - | 3970 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3971 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3972 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3973 | `		ph7_lib_version()` |
|       - | 3974 | `		);` |
|       - | 3975 | `	/*All done */` |
|       5 | 3976 | `	return SXRET_OK;` |
|       2 | 3977 | `Synchro:` |
|       - | 3978 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3979 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3980 | `		pGen->pIn++;` |
|       1 | 3981 | `	}` |
|       5 | 3982 | `	return SXRET_OK;` |
|       5 | 3983 |  |
|       - | 3984 | `/*` |
|       - | 3985 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3986 | ` * as follows:` |
|       - | 3987 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3988 | ` * {` |
|       - | 3989 | ` *   return "Making a cup of $type.\n";` |
|       - | 3990 | ` * }` |
|       - | 3991 | ` * Symisc eXtension.` |
|       - | 3992 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3993 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3994 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3995 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3996 | ` *      {` |
|       - | 3997 | ` *       var_dump($a);` |
|       - | 3998 | ` *      }` |
|       - | 3999 | ` *     //call test without args` |
|       - | 4000 | ` *      test();` |
|       - | 4001 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4002 | ` *      Example:` |
|       - | 4003 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4004 | ` * 3 -) Function overloading!!` |
|       - | 4005 | ` *      Example:` |
|       - | 4006 | ` *      function foo($a) {` |
|       - | 4007 | ` *   	  return $a.PHP_EOL;` |
|       - | 4008 | ` *	    }` |
|       - | 4009 | ` *	    function foo($a, $b) {` |
|       - | 4010 | ` *   	  return $a + $b;` |
|       - | 4011 | ` *	    }` |
|       - | 4012 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4013 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4014 | ` *      // Same arg` |
|       - | 4015 | ` *	   function foo(string $a)` |
|       - | 4016 | ` *	   {` |
|       - | 4017 | ` *	     echo "a is a string\n";` |
|       - | 4018 | ` *	     var_dump($a);` |
|       - | 4019 | ` *	   }` |
|       - | 4020 | ` *	  function foo(int $a)` |
|       - | 4021 | ` *	  {` |
|       - | 4022 | ` *	    echo "a is integer\n";` |
|       - | 4023 | ` *	    var_dump($a);` |
|       - | 4024 | ` *	  }` |
|       - | 4025 | ` *	  function foo(array $a)` |
|       - | 4026 | ` *	  {` |
|       - | 4027 | ` * 	    echo "a is an array\n";` |
|       - | 4028 | ` * 	    var_dump($a);` |
|       - | 4029 | ` *	  }` |
|       - | 4030 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4031 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4032 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4033 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4034 | ` * introduced by the PH7 engine.` |
|       - | 4035 | ` */` |
|   44426 | 4036 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4037 |  |
|       - | 4038 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4039 | `	SySet *pInstrContainer;` |
|       - | 4040 | `	sxi32 rc;` |
|       - | 4041 | `	/* Swap token stream */` |
|   44428 | 4042 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   44428 | 4043 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   44428 | 4044 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4045 | `	/* Compile the expression holding the argument value */` |
|   44428 | 4046 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4047 | `	/* Emit the done instruction */` |
|   44428 | 4048 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   44428 | 4049 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   44428 | 4050 | `	RE_SWAP_DELIMITER(pGen);` |
|   44428 | 4051 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4052 | `		return SXERR_ABORT;` |
|       - | 4053 | `	}` |
|   44428 | 4054 | `	return SXRET_OK;` |
|   22215 | 4055 |  |
|       - | 4056 | `/*` |
|       - | 4057 | ` * Collect function arguments one after one.` |
|       - | 4058 | ` * According to the PHP language reference manual.` |
|       - | 4059 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4060 | ` * list of expressions.` |
|       - | 4061 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4062 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4063 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4064 | ` * for more information.` |
|       - | 4065 | ` * Example #1 Passing arrays to functions` |
|       - | 4066 | ` * <?php` |
|       - | 4067 | ` * function takes_array($input)` |
|       - | 4068 | ` * {` |
|       - | 4069 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4070 | ` * }` |
|       - | 4071 | ` * ?>` |
|       - | 4072 | ` * Making arguments be passed by reference` |
|       - | 4073 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4074 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4075 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4076 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4077 | ` * to the argument name in the function definition:` |
|       - | 4078 | ` * Example #2 Passing function parameters by reference` |
|       - | 4079 | ` * <?php` |
|       - | 4080 | ` * function add_some_extra(&$string)` |
|       - | 4081 | ` * {` |
|       - | 4082 | ` *   $string .= 'and something extra.';` |
|       - | 4083 | ` * }` |
|       - | 4084 | ` * $str = 'This is a string, ';` |
|       - | 4085 | ` * add_some_extra($str);` |
|       - | 4086 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4087 | ` * ?>` |
|       - | 4088 | ` *` |
|       - | 4089 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4090 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4091 | ` * on these extension.` |
|       - | 4092 | ` */` |
|   53228 | 4093 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4094 |  |
|       - | 4095 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4096 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4097 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4098 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4099 | `	sxi32 rc;` |
|       - | 4100 |  |
|   53230 | 4101 | `	pIn = pGen->pIn;` |
|   53230 | 4102 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4103 | `	/* Process arguments one after one */` |
|   67345 | 4104 | `	for(;;){` |
|  134692 | 4105 | `		if( pIn >= pEnd ){` |
|       - | 4106 | `			/* No more arguments to process */` |
|   53228 | 4107 | `			break;` |
|       - | 4108 | `		}` |
|   81466 | 4109 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   81466 | 4110 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   81466 | 4111 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   55524 | 4112 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   49972 | 4113 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   49972 | 4114 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4115 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   49972 | 4116 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4117 | `					sArg.nType = MEMOBJ_BOOL;` |
|   49972 | 4118 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13882 | 4119 | `					sArg.nType = MEMOBJ_INT;` |
|   43032 | 4120 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   36090 | 4121 | `					sArg.nType = MEMOBJ_STRING;` |
|   18047 | 4122 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4123 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4124 | `				}else{` |
|       4 | 4125 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4126 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4127 | `						&pIn->sData);` |
|       - | 4128 | `				}` |
|   24987 | 4129 | `			}else{` |
|    5554 | 4130 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4131 | `				char *zDupLocal;` |
|       - | 4132 | `				/* Argument must be a class instance,record that*/` |
|    5554 | 4133 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5554 | 4134 | `				if( zDupLocal ){` |
|    5554 | 4135 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5554 | 4136 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2776 | 4137 | `				}` |
|       - | 4138 | `			}` |
|   55524 | 4139 | `			pIn++;` |
|   27761 | 4140 | `		}` |
|   81466 | 4141 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4142 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4143 | `			return rc;` |
|       - | 4144 | `		}` |
|   81466 | 4145 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4146 | `			/* Pass by reference,record that */` |
|    2800 | 4147 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2800 | 4148 | `			pIn++;` |
|    1399 | 4149 | `		}` |
|   81466 | 4150 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4151 | `			/* Invalid argument */` |
|     ! 0 | 4152 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4153 | `			return rc;` |
|       - | 4154 | `		}` |
|   81466 | 4155 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4156 | `		/* Copy argument name */` |
|   81466 | 4157 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   81466 | 4158 | `		if( zDup == 0 ){` |
|     ! 0 | 4159 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4160 | `			return SXERR_ABORT;` |
|       - | 4161 | `		}` |
|   81466 | 4162 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   81466 | 4163 | `		pIn++;` |
|   81466 | 4164 | `		if( pIn < pEnd ){` |
|   50460 | 4165 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4166 | `				SyToken *pDefend;` |
|   44430 | 4167 | `				sxi32 iNest = 0;` |
|   44430 | 4168 | `				pIn++; /* Jump the equal sign */` |
|   44430 | 4169 | `				pDefend = pIn;` |
|       - | 4170 | `				/* Process the default value associated with this argument */` |
|   94408 | 4171 | `				while( pDefend < pEnd ){` |
|   72188 | 4172 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   22210 | 4173 | `						break;` |
|       - | 4174 | `					}` |
|   49980 | 4175 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4176 | `						/* Increment nesting level */` |
|    2778 | 4177 | `						iNest++;` |
|   48592 | 4178 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4179 | `						/* Decrement nesting level */` |
|    2778 | 4180 | `						iNest--;` |
|    1388 | 4181 | `					}` |
|   49980 | 4182 | `					pDefend++;` |
|       2 | 4183 | `				}` |
|   44430 | 4184 | `				if( pIn >= pDefend ){` |
|       3 | 4185 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4186 | `					return rc;` |
|       - | 4187 | `				}` |
|       - | 4188 | `				/* Process default value */` |
|   44428 | 4189 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   44428 | 4190 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4191 | `					return rc;` |
|       - | 4192 | `				}` |
|       - | 4193 | `				/* Point beyond the default value */` |
|   44428 | 4194 | `				pIn = pDefend;` |
|   22213 | 4195 | `			}` |
|   50458 | 4196 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4197 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4198 | `				return rc;` |
|       - | 4199 | `			}` |
|   50458 | 4200 | `			pIn++; /* Jump the trailing comma */` |
|   25228 | 4201 | `		}` |
|       - | 4202 | `		/* Append argument signature */` |
|   81464 | 4203 | `		if( sArg.nType > 0 ){` |
|   55522 | 4204 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4205 | `				/* Class name */` |
|    5554 | 4206 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2778 | 4207 | `			}else{` |
|       - | 4208 | `				int c;` |
|   49970 | 4209 | `				c = 'n'; /* cc warning */` |
|       - | 4210 | `				/* Type leading character */` |
|   49970 | 4211 | `				switch(sArg.nType){` |
|     ! 0 | 4212 | `				case MEMOBJ_HASHMAP:` |
|       - | 4213 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4214 | `					c = 'h';` |
|     ! 0 | 4215 | `					break;` |
|    6940 | 4216 | `				case MEMOBJ_INT:` |
|       - | 4217 | `					/* Integer */` |
|   13882 | 4218 | `					c = 'i';` |
|   13882 | 4219 | `					break;` |
|     ! 0 | 4220 | `				case MEMOBJ_BOOL:` |
|       - | 4221 | `					/* Bool */` |
|     ! 0 | 4222 | `					c = 'b';` |
|     ! 0 | 4223 | `					break;` |
|     ! 0 | 4224 | `				case MEMOBJ_REAL:` |
|       - | 4225 | `					/* Float */` |
|     ! 0 | 4226 | `					c = 'f';` |
|     ! 0 | 4227 | `					break;` |
|   18044 | 4228 | `				case MEMOBJ_STRING:` |
|       - | 4229 | `					/* String */` |
|   36090 | 4230 | `					c = 's';` |
|   36088 | 4231 | `					break;` |
|     ! 0 | 4232 | `				default:` |
|     ! 0 | 4233 | `					break;` |
|       - | 4234 | `				}` |
|   49970 | 4235 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4236 | `			}` |
|   27762 | 4237 | `		}else{` |
|       - | 4238 | `			/* No type is associated with this parameter which mean` |
|       - | 4239 | `			 * that this function is not condidate for overloading.` |
|       - | 4240 | `			 */` |
|   25944 | 4241 | `			SyBlobRelease(&sSig);` |
|       - | 4242 | `		}` |
|       - | 4243 | `		/* Save in the argument set */` |
|   81464 | 4244 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4245 | `	}` |
|   53228 | 4246 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4247 | `		/* Save function signature */` |
|   33314 | 4248 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16656 | 4249 | `	}` |
|   53228 | 4250 | `	return SXRET_OK;` |
|   26616 | 4251 |  |
|       - | 4252 | `/*` |
|       - | 4253 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4254 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4255 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4256 | ` */` |
|  148064 | 4257 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4258 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4259 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4260 | `	)` |
|       2 | 4261 |  |
|       - | 4262 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4263 | `	GenBlock *pBlock;` |
|       - | 4264 | `	sxu32 nGotoOfft;` |
|       - | 4265 | `	sxi32 rc;` |
|       - | 4266 | `	/* Attach the new function */` |
|  148066 | 4267 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  148066 | 4268 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4269 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4270 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4271 | `		return SXERR_ABORT;` |
|       - | 4272 | `	}` |
|  148066 | 4273 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4274 | `	/* Swap bytecode containers */` |
|  148066 | 4275 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  148066 | 4276 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4277 | `	/* Compile the body */` |
|  148066 | 4278 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4279 | `	/* Fix exception jumps now the destination is resolved */` |
|  148066 | 4280 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4281 | `	/* Emit the final return if not yet done */` |
|  148066 | 4282 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4283 | `	/* Fix gotos jumps now the destination is resolved */` |
|  148066 | 4284 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4285 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4286 | `	}` |
|  148066 | 4287 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4288 | `	/* Restore the default container */` |
|  148066 | 4289 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4290 | `	/* Leave function block */` |
|  148066 | 4291 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  148066 | 4292 | `	if( rc == SXERR_ABORT ){` |
|       - | 4293 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4294 | `		return SXERR_ABORT;` |
|       - | 4295 | `	}` |
|       - | 4296 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4297 | `	{` |
|  148066 | 4298 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4299 | `		sxu32 i;` |
| 3076858 | 4300 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2928810 | 4301 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      17 | 4302 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      17 | 4303 | `				break;` |
|       - | 4304 | `			}` |
| 1464398 | 4305 | `		}` |
|       - | 4306 | `	}` |
|       - | 4307 | `	/* All done, function body compiled */` |
|  148066 | 4308 | `	return SXRET_OK;` |
|   74034 | 4309 |  |
|       - | 4310 | `/*` |
|       - | 4311 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4312 | ` * According to the PHP language reference manual.` |
|       - | 4313 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4314 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4315 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4316 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4317 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4318 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4319 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4320 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4321 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4322 | ` *` |
|       - | 4323 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4324 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4325 | ` * on these extension.` |
|       - | 4326 | ` */` |
|   36646 | 4327 | `static sxi32 GenStateCompileFunc(` |
|       - | 4328 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4329 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4330 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4331 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4332 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4333 | `	)` |
|       2 | 4334 |  |
|       - | 4335 | `	ph7_vm_func *pFunc;` |
|       - | 4336 | `	SyToken *pEnd;` |
|       - | 4337 | `	sxu32 nLine;` |
|       - | 4338 | `	char *zName;` |
|       - | 4339 | `	sxi32 rc;` |
|       - | 4340 | `	/* Extract line number */` |
|   36648 | 4341 | `	nLine = pGen->pIn->nLine;` |
|       - | 4342 | `	/* Jump the left parenthesis '(' */` |
|   36648 | 4343 | `	pGen->pIn++;` |
|       - | 4344 | `	/* Delimit the function signature */` |
|   36648 | 4345 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   36648 | 4346 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4347 | `		/* Syntax error */` |
|       7 | 4348 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4349 | `		if( rc == SXERR_ABORT ){` |
|       - | 4350 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4351 | `			return SXERR_ABORT;` |
|       - | 4352 | `		}` |
|       7 | 4353 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4354 | `		return SXRET_OK;` |
|       - | 4355 | `	}` |
|       - | 4356 | `	/* Create the function state */` |
|   36642 | 4357 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   36642 | 4358 | `	if( pFunc == 0 ){` |
|     ! 0 | 4359 | `		goto OutOfMem;` |
|       - | 4360 | `	}` |
|       - | 4361 | `	/* Build the function name, prepending namespace if active */` |
|   36646 | 4362 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4363 | `		SyBlob sFQN;` |
|       - | 4364 | `		sxu32 nLen;` |
|       9 | 4365 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4366 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4367 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4368 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4369 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4370 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4371 | `		SyBlobRelease(&sFQN);` |
|       9 | 4372 | `		if( zName == 0 ){` |
|     ! 0 | 4373 | `			goto OutOfMem;` |
|       - | 4374 | `		}` |
|       9 | 4375 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4376 | `	}else{` |
|   36634 | 4377 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   36634 | 4378 | `		if( zName == 0 ){` |
|     ! 0 | 4379 | `			goto OutOfMem;` |
|       - | 4380 | `		}` |
|   36634 | 4381 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4382 | `	}` |
|   36642 | 4383 | `	if( pGen->pIn < pEnd ){` |
|       - | 4384 | `		/* Collect function arguments */` |
|   25390 | 4385 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   25390 | 4386 | `		if( rc == SXERR_ABORT ){` |
|       - | 4387 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4388 | `			return SXERR_ABORT;` |
|       - | 4389 | `		}` |
|   12694 | 4390 | `	}` |
|       - | 4391 | `	/* Compile function body */` |
|   36642 | 4392 | `	pGen->pIn = &pEnd[1];` |
|   36642 | 4393 | `	if( bHandleClosure ){` |
|       - | 4394 | `		ph7_vm_func_closure_env sEnv;` |
|     144 | 4395 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     142 | 4396 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      79 | 4397 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4398 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4399 | `				/* Closure,record environment variable */` |
|      14 | 4400 | `				pGen->pIn++;` |
|      14 | 4401 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4402 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4403 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4404 | `						return SXERR_ABORT;` |
|       - | 4405 | `					}` |
|     ! 0 | 4406 | `				}` |
|      14 | 4407 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4408 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4409 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4410 | `					int iFlagsLocal = 0;` |
|      28 | 4411 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4412 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4413 | `						break;` |
|       - | 4414 | `					}` |
|      16 | 4415 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4416 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4417 | `						/* Pass by reference,record that */` |
|     ! 0 | 4418 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4419 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4420 | `							);` |
|     ! 0 | 4421 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4422 | `						pGen->pIn++;` |
|     ! 0 | 4423 | `					}` |
|      14 | 4424 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4425 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4426 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4427 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4428 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4429 | `								return SXERR_ABORT;` |
|       - | 4430 | `							}` |
|       - | 4431 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4432 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4433 | `								pGen->pIn++;` |
|     ! 0 | 4434 | `							}` |
|     ! 0 | 4435 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4436 | `								pGen->pIn++;` |
|     ! 0 | 4437 | `							}` |
|     ! 0 | 4438 | `							break;` |
|       - | 4439 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4440 | `					}else{` |
|       - | 4441 | `						SyString *pNameLocal;` |
|       - | 4442 | `						char *zDup;` |
|       - | 4443 | `						/* Duplicate variable name */` |
|      16 | 4444 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4445 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4446 | `						if( zDup ){` |
|       - | 4447 | `							/* Zero the structure */` |
|      16 | 4448 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4449 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4450 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4451 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4452 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4453 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4454 | `									got_this = 1;` |
|     ! 0 | 4455 | `							}` |
|       - | 4456 | `							/* Save imported variable */` |
|      16 | 4457 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4458 | `						}else{` |
|     ! 0 | 4459 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4460 | `							 return SXERR_ABORT;` |
|       - | 4461 | `						}` |
|       - | 4462 | `					}` |
|      16 | 4463 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4464 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4465 | `						/* Ignore trailing commas */` |
|       3 | 4466 | `						pGen->pIn++;` |
|       1 | 4467 | `					}` |
|       2 | 4468 | `				}` |
|      14 | 4469 | `				if( !got_this ){` |
|       - | 4470 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4471 | `					 * available to the closure environment.` |
|       - | 4472 | `					 */` |
|      14 | 4473 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4474 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4475 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4476 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4477 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4478 | `				}` |
|      14 | 4479 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4480 | `					/* Mark as closure */` |
|      14 | 4481 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4482 | `				}` |
|       6 | 4483 | `		}` |
|      71 | 4484 | `	}` |
|       - | 4485 | `	/* Compile the body */` |
|   36642 | 4486 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   36642 | 4487 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4488 | `		return SXERR_ABORT;` |
|       - | 4489 | `	}` |
|   36642 | 4490 | `	if( ppFunc ){` |
|     144 | 4491 | `		*ppFunc = pFunc;` |
|      71 | 4492 | `	}` |
|   36642 | 4493 | `	rc = SXRET_OK;` |
|   36642 | 4494 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4495 | `		/* Finally register the function */` |
|   36630 | 4496 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   18314 | 4497 | `	}` |
|   36642 | 4498 | `	if( rc == SXRET_OK ){` |
|   36642 | 4499 | `		return SXRET_OK;` |
|       - | 4500 | `	}` |
|       - | 4501 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4502 | `OutOfMem:` |
|       - | 4503 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4504 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4505 | `	 */` |
|     ! 0 | 4506 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4507 | `	return SXERR_ABORT;` |
|   18325 | 4508 |  |
|       - | 4509 | `/*` |
|       - | 4510 | ` * Compile a standard PHP function.` |
|       - | 4511 | ` *  Refer to the block-comment above for more information.` |
|       - | 4512 | ` */` |
|   36510 | 4513 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4514 |  |
|       - | 4515 | `	SyString *pName;` |
|       - | 4516 | `	sxi32 iFlags;` |
|       - | 4517 | `	sxu32 nLine;` |
|       - | 4518 | `	sxi32 rc;` |
|       - | 4519 |  |
|   36512 | 4520 | `	nLine = pGen->pIn->nLine;` |
|   36512 | 4521 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   36512 | 4522 | `	iFlags = 0;` |
|   36512 | 4523 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4524 | `		/* Return by reference,remember that */` |
|       7 | 4525 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4526 | `		/* Jump the '&' token */` |
|       7 | 4527 | `		pGen->pIn++;` |
|       3 | 4528 | `	}` |
|   36512 | 4529 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4530 | `		/* Invalid function name */` |
|       5 | 4531 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4532 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4533 | `			return SXERR_ABORT;` |
|       - | 4534 | `		}` |
|       - | 4535 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4536 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4537 | `			pGen->pIn++;` |
|       1 | 4538 | `		}` |
|       5 | 4539 | `		return SXRET_OK;` |
|       - | 4540 | `	}` |
|   36508 | 4541 | `	pName = &pGen->pIn->sData;` |
|   36508 | 4542 | `	nLine = pGen->pIn->nLine;` |
|       - | 4543 | `	/* Jump the function name */` |
|   36508 | 4544 | `	pGen->pIn++;` |
|   36508 | 4545 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4546 | `		/* Syntax error */` |
|       3 | 4547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4548 | `		if( rc == SXERR_ABORT ){` |
|       - | 4549 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4550 | `			return SXERR_ABORT;` |
|       - | 4551 | `		}` |
|       - | 4552 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4553 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4554 | `			pGen->pIn++;` |
|     ! 0 | 4555 | `		}` |
|       3 | 4556 | `		return SXRET_OK;` |
|       - | 4557 | `	}` |
|       - | 4558 | `	/* Compile function body */` |
|   36506 | 4559 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   36506 | 4560 | `	return rc;` |
|   18257 | 4561 |  |
|       - | 4562 | `/*` |
|       - | 4563 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4564 | ` * According to the PHP language reference manual` |
|       - | 4565 | ` *  Visibility:` |
|       - | 4566 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4567 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4568 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4569 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4570 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4571 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4572 | ` */` |
|  169988 | 4573 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4574 |  |
|  169990 | 4575 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8390 | 4576 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  161602 | 4577 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   19462 | 4578 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4579 | `	}` |
|       - | 4580 | `	/* Assume public by default */` |
|  142142 | 4581 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   84996 | 4582 |  |
|       - | 4583 | `/*` |
|       - | 4584 | ` * Compile a class constant.` |
|       - | 4585 | ` * According to the PHP language reference manual` |
|       - | 4586 | ` *  Class Constants` |
|       - | 4587 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4588 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4589 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4590 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4591 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4592 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4593 | ` * Symisc eXtension.` |
|       - | 4594 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4595 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4596 | ` *  Example:` |
|       - | 4597 | ` *   class Test{` |
|       - | 4598 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4599 | ` *   };` |
|       - | 4600 | ` *   var_dump(TEST::MyConst);` |
|       - | 4601 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4602 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4603 | ` */` |
|      10 | 4604 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4605 |  |
|      12 | 4606 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4607 | `	SySet *pInstrContainer;` |
|       - | 4608 | `	ph7_class_attr *pCons;` |
|       - | 4609 | `	SyString *pName;` |
|       - | 4610 | `	sxi32 rc;` |
|       - | 4611 | `	/* Extract visibility level */` |
|      12 | 4612 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4613 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4614 | `loop:` |
|       - | 4615 | `	/* Mark as constant */` |
|      12 | 4616 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4617 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4618 | `		/* Invalid constant name */` |
|     ! 0 | 4619 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4620 | `		if( rc == SXERR_ABORT ){` |
|       - | 4621 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4622 | `			return SXERR_ABORT;` |
|       - | 4623 | `		}` |
|     ! 0 | 4624 | `		goto Synchronize;` |
|       - | 4625 | `	}` |
|       - | 4626 | `	/* Peek constant name */` |
|      12 | 4627 | `	pName = &pGen->pIn->sData;` |
|       - | 4628 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4629 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4630 | `		/* Reserved constant name */` |
|     ! 0 | 4631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4632 | `		if( rc == SXERR_ABORT ){` |
|       - | 4633 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4634 | `			return SXERR_ABORT;` |
|       - | 4635 | `		}` |
|     ! 0 | 4636 | `		goto Synchronize;` |
|       - | 4637 | `	}` |
|       - | 4638 | `	/* Advance the stream cursor */` |
|      12 | 4639 | `	pGen->pIn++;` |
|      12 | 4640 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4641 | `		/* Invalid declaration */` |
|     ! 0 | 4642 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4643 | `		if( rc == SXERR_ABORT ){` |
|       - | 4644 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4645 | `			return SXERR_ABORT;` |
|       - | 4646 | `		}` |
|     ! 0 | 4647 | `		goto Synchronize;` |
|       - | 4648 | `	}` |
|      12 | 4649 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4650 | `	/* Allocate a new class attribute */` |
|      12 | 4651 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4652 | `	if( pCons == 0 ){` |
|     ! 0 | 4653 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4654 | `		return SXERR_ABORT;` |
|       - | 4655 | `	}` |
|       - | 4656 | `	/* Swap bytecode container */` |
|      12 | 4657 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4658 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4659 | `	/* Compile constant value.` |
|       - | 4660 | `	 */` |
|      12 | 4661 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4662 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4663 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4664 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4665 | `			return SXERR_ABORT;` |
|       - | 4666 | `		}` |
|       1 | 4667 | `	}` |
|       - | 4668 | `	/* Emit the done instruction */` |
|      12 | 4669 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4670 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4671 | `	if( rc == SXERR_ABORT ){` |
|       - | 4672 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4673 | `		return SXERR_ABORT;` |
|       - | 4674 | `	}` |
|       - | 4675 | `	/* All done,install the constant */` |
|      12 | 4676 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4677 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4678 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4679 | `		return SXERR_ABORT;` |
|       - | 4680 | `	}` |
|      12 | 4681 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4682 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4683 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4684 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4685 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4686 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4687 | `				pTok--;` |
|     ! 0 | 4688 | `			}` |
|     ! 0 | 4689 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4690 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4691 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4692 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4693 | `				return SXERR_ABORT;` |
|       - | 4694 | `			}` |
|     ! 0 | 4695 | `		}else{` |
|     ! 0 | 4696 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4697 | `				goto loop;` |
|       - | 4698 | `			}` |
|       - | 4699 | `		}` |
|     ! 0 | 4700 | `	}` |
|      12 | 4701 | `	return SXRET_OK;` |
|     ! 0 | 4702 | `Synchronize:` |
|       - | 4703 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4704 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4705 | `		pGen->pIn++;` |
|     ! 0 | 4706 | `	}` |
|     ! 0 | 4707 | `	return SXERR_CORRUPT;` |
|       7 | 4708 |  |
|       - | 4709 | `/*` |
|       - | 4710 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4711 | ` * According to the PHP language reference manual` |
|       - | 4712 | ` *  Properties` |
|       - | 4713 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4714 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4715 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4716 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4717 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4718 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4719 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4720 | ` * Symisc eXtension.` |
|       - | 4721 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4722 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4723 | ` *  Example:` |
|       - | 4724 | ` *   class Test{` |
|       - | 4725 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4726 | ` *   };` |
|       - | 4727 | ` *   var_dump(TEST::myVar);` |
|       - | 4728 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4729 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4730 | ` */` |
|   36300 | 4731 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4732 |  |
|   36302 | 4733 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4734 | `	ph7_class_attr *pAttr;` |
|       - | 4735 | `	SyString *pName;` |
|       - | 4736 | `	sxi32 rc;` |
|       - | 4737 | `	/* Extract visibility level */` |
|   36302 | 4738 | `	iProtection = GetProtectionLevel(iProtection);` |
|   18150 | 4739 | `loop:` |
|   36302 | 4740 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   36302 | 4741 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4742 | `		/* Invalid attribute name */` |
|     ! 0 | 4743 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4744 | `		if( rc == SXERR_ABORT ){` |
|       - | 4745 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4746 | `			return SXERR_ABORT;` |
|       - | 4747 | `		}` |
|     ! 0 | 4748 | `		goto Synchronize;` |
|       - | 4749 | `	}` |
|       - | 4750 | `	/* Peek attribute name */` |
|   36302 | 4751 | `	pName = &pGen->pIn->sData;` |
|       - | 4752 | `	/* Advance the stream cursor */` |
|   36302 | 4753 | `	pGen->pIn++;` |
|   36302 | 4754 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4755 | `		/* Invalid declaration */` |
|       3 | 4756 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4757 | `		if( rc == SXERR_ABORT ){` |
|       - | 4758 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4759 | `			return SXERR_ABORT;` |
|       - | 4760 | `		}` |
|       3 | 4761 | `		goto Synchronize;` |
|       - | 4762 | `	}` |
|       - | 4763 | `	/* Allocate a new class attribute */` |
|   36300 | 4764 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   36300 | 4765 | `	if( pAttr == 0 ){` |
|     ! 0 | 4766 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4767 | `		return SXERR_ABORT;` |
|       - | 4768 | `	}` |
|   36300 | 4769 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4770 | `		SySet *pInstrContainer;` |
|   11266 | 4771 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4772 | `		/* Swap bytecode container */` |
|   11266 | 4773 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11266 | 4774 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4775 | `		/* Compile attribute value.` |
|       - | 4776 | `		 */` |
|   11266 | 4777 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11266 | 4778 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4779 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4780 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4781 | `				return SXERR_ABORT;` |
|       - | 4782 | `			}` |
|     ! 0 | 4783 | `		}` |
|       - | 4784 | `		/* Emit the done instruction */` |
|   11266 | 4785 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11266 | 4786 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5632 | 4787 | `	}` |
|       - | 4788 | `	/* All done,install the attribute */` |
|   36300 | 4789 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   36300 | 4790 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4791 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4792 | `		return SXERR_ABORT;` |
|       - | 4793 | `	}` |
|   36300 | 4794 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4795 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4796 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4797 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4798 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4799 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4800 | `				pTok--;` |
|     ! 0 | 4801 | `			}` |
|     ! 0 | 4802 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4803 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4804 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4805 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4806 | `				return SXERR_ABORT;` |
|       - | 4807 | `			}` |
|     ! 0 | 4808 | `		}else{` |
|     ! 0 | 4809 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4810 | `				goto loop;` |
|       - | 4811 | `			}` |
|       - | 4812 | `		}` |
|     ! 0 | 4813 | `	}` |
|   36300 | 4814 | `	return SXRET_OK;` |
|       1 | 4815 | `Synchronize:` |
|       - | 4816 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4817 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4818 | `		pGen->pIn++;` |
|       1 | 4819 | `	}` |
|       3 | 4820 | `	return SXERR_CORRUPT;` |
|   18152 | 4821 |  |
|       - | 4822 | `/*` |
|       - | 4823 | ` * Compile a class method.` |
|       - | 4824 | ` *` |
|       - | 4825 | ` * Refer to the official documentation for more information` |
|       - | 4826 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4827 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4828 | ` * overloading and many more.` |
|       - | 4829 | ` */` |
|  133678 | 4830 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4831 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4832 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4833 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4834 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4835 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4836 | `	)` |
|       2 | 4837 |  |
|  133680 | 4838 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4839 | `	ph7_class_method *pMeth;` |
|       - | 4840 | `	sxi32 iFuncFlags;` |
|       - | 4841 | `	SyString *pName;` |
|       - | 4842 | `	SyToken *pEnd;` |
|       - | 4843 | `	sxi32 rc;` |
|       - | 4844 | `	/* Extract visibility level */` |
|  133680 | 4845 | `	iProtection = GetProtectionLevel(iProtection);` |
|  133680 | 4846 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  133680 | 4847 | `	iFuncFlags = 0;` |
|  133680 | 4848 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4849 | `		/* Invalid method name */` |
|     ! 0 | 4850 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4851 | `		if( rc == SXERR_ABORT ){` |
|       - | 4852 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4853 | `			return SXERR_ABORT;` |
|       - | 4854 | `		}` |
|     ! 0 | 4855 | `		goto Synchronize;` |
|       - | 4856 | `	}` |
|  133680 | 4857 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4858 | `		/* Return by reference,remember that */` |
|     ! 0 | 4859 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4860 | `		/* Jump the '&' token */` |
|     ! 0 | 4861 | `		pGen->pIn++;` |
|     ! 0 | 4862 | `	}` |
|  133680 | 4863 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4864 | `		/* Invalid method name */` |
|     ! 0 | 4865 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4866 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4867 | `			return SXERR_ABORT;` |
|       - | 4868 | `		}` |
|     ! 0 | 4869 | `		goto Synchronize;` |
|       - | 4870 | `	}` |
|       - | 4871 | `	/* Peek method name */` |
|  133680 | 4872 | `	pName = &pGen->pIn->sData;` |
|  133680 | 4873 | `	nLine = pGen->pIn->nLine;` |
|       - | 4874 | `	/* Jump the method name */` |
|  133680 | 4875 | `	pGen->pIn++;` |
|  133680 | 4876 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4877 | `		/* Abstract method */` |
|   22254 | 4878 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4879 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4880 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4881 | `				&pClass->sName,pName);` |
|     ! 0 | 4882 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4883 | `				return SXERR_ABORT;` |
|       - | 4884 | `			}` |
|     ! 0 | 4885 | `		}` |
|       - | 4886 | `		/* Assemble method signature only */` |
|   22254 | 4887 | `		doBody = FALSE;` |
|   11126 | 4888 | `	}` |
|  133680 | 4889 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4890 | `		/* Syntax error */` |
|     ! 0 | 4891 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4892 | `		if( rc == SXERR_ABORT ){` |
|       - | 4893 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4894 | `			return SXERR_ABORT;` |
|       - | 4895 | `		}` |
|     ! 0 | 4896 | `		goto Synchronize;` |
|       - | 4897 | `	}` |
|       - | 4898 | `	/* Allocate a new class_method instance */` |
|  133680 | 4899 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  133680 | 4900 | `	if( pMeth == 0 ){` |
|     ! 0 | 4901 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4902 | `		return SXERR_ABORT;` |
|       - | 4903 | `	}` |
|       - | 4904 | `	/* Jump the left parenthesis '(' */` |
|  133680 | 4905 | `	pGen->pIn++;` |
|  133680 | 4906 | `	pEnd = 0; /* cc warning */` |
|       - | 4907 | `	/* Delimit the method signature */` |
|  133680 | 4908 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  133680 | 4909 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4910 | `		/* Syntax error */` |
|       3 | 4911 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4912 | `		if( rc == SXERR_ABORT ){` |
|       - | 4913 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4914 | `			return SXERR_ABORT;` |
|       - | 4915 | `		}` |
|       3 | 4916 | `		goto Synchronize;` |
|       - | 4917 | `	}` |
|  133678 | 4918 | `	if( pGen->pIn < pEnd ){` |
|       - | 4919 | `		/* Collect method arguments */` |
|   27842 | 4920 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   27842 | 4921 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4922 | `			return SXERR_ABORT;` |
|       - | 4923 | `		}` |
|   13920 | 4924 | `	}` |
|       - | 4925 | `	/* Point beyond method signature */` |
|  133678 | 4926 | `	pGen->pIn = &pEnd[1];` |
|  133678 | 4927 | `	if( doBody ){` |
|       - | 4928 | `		/* Compile method body */` |
|  111426 | 4929 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  111426 | 4930 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4931 | `			return SXERR_ABORT;` |
|       - | 4932 | `		}` |
|   55714 | 4933 | `	}else{` |
|       - | 4934 | `		/* Only method signature is allowed */` |
|   22254 | 4935 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4936 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4937 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4938 | `				if( rc == SXERR_ABORT ){` |
|       - | 4939 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4940 | `					return SXERR_ABORT;` |
|       - | 4941 | `				}` |
|     ! 0 | 4942 | `				return SXERR_CORRUPT;` |
|       - | 4943 | `			}` |
|       - | 4944 | `	}` |
|       - | 4945 | `	/* All done,install the method */` |
|  133678 | 4946 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  133678 | 4947 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4948 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4949 | `		return SXERR_ABORT;` |
|       - | 4950 | `	}` |
|  133678 | 4951 | `	return SXRET_OK;` |
|       1 | 4952 | `Synchronize:` |
|       - | 4953 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4954 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4955 | `		pGen->pIn++;` |
|       1 | 4956 | `	}` |
|       3 | 4957 | `	return SXERR_CORRUPT;` |
|   66841 | 4958 |  |
|       - | 4959 | `/*` |
|       - | 4960 | ` * Compile an object interface.` |
|       - | 4961 | ` *  According to the PHP language reference manual` |
|       - | 4962 | ` *   Object Interfaces:` |
|       - | 4963 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4964 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4965 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4966 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4967 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4968 | ` */` |
|    8360 | 4969 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4970 |  |
|    8362 | 4971 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4972 | `	ph7_class *pClass,*pBase;` |
|       - | 4973 | `	SyToken *pEnd,*pTmp;` |
|       - | 4974 | `	SyString *pName;` |
|       - | 4975 | `	sxi32 nKwrd;` |
|       - | 4976 | `	sxi32 rc;` |
|       - | 4977 | `	/* Jump the 'interface' keyword */` |
|    8362 | 4978 | `	pGen->pIn++;` |
|       - | 4979 | `	/* Extract interface name */` |
|    8362 | 4980 | `	pName = &pGen->pIn->sData;` |
|       - | 4981 | `	/* Advance the stream cursor */` |
|    8362 | 4982 | `	pGen->pIn++;` |
|       - | 4983 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4984 | `		SyBlob sFQN;` |
|       - | 4985 | `		SyString sFQNStr;` |
|    8362 | 4986 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8362 | 4987 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8362 | 4988 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8362 | 4989 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8362 | 4990 | `		SyBlobRelease(&sFQN);` |
|       - | 4991 | `	}` |
|    8362 | 4992 | `	if( pClass == 0 ){` |
|     ! 0 | 4993 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4994 | `		return SXERR_ABORT;` |
|       - | 4995 | `	}` |
|       - | 4996 | `	/* Mark as an interface */` |
|    8362 | 4997 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4998 | `	/* Assume no base class is given */` |
|    8362 | 4999 | `	pBase = 0;` |
|    8362 | 5000 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5001 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5002 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5003 | `			SyString *pBaseName;` |
|       - | 5004 | `			/* Extract base interface */` |
|       3 | 5005 | `			pGen->pIn++;` |
|       3 | 5006 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5007 | `				/* Syntax error */` |
|     ! 0 | 5008 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5009 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5010 | `					pName);` |
|     ! 0 | 5011 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5012 | `				if( rc == SXERR_ABORT ){` |
|       - | 5013 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5014 | `					return SXERR_ABORT;` |
|       - | 5015 | `				}` |
|     ! 0 | 5016 | `				return SXRET_OK;` |
|       - | 5017 | `			}` |
|       3 | 5018 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5019 | `			{` |
|       - | 5020 | `				SyBlob sResolved;` |
|       3 | 5021 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5022 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5023 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5024 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5025 | `				SyBlobRelease(&sResolved);` |
|       - | 5026 | `			}` |
|       - | 5027 | `			/* Only interfaces is allowed */` |
|       3 | 5028 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5029 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5030 | `			}` |
|       3 | 5031 | `			if( pBase == 0 ){` |
|       - | 5032 | `				/* Inexistant interface */` |
|     ! 0 | 5033 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5034 | `				if( rc == SXERR_ABORT ){` |
|       - | 5035 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5036 | `					return SXERR_ABORT;` |
|       - | 5037 | `				}` |
|     ! 0 | 5038 | `			}` |
|       - | 5039 | `			/* Advance the stream cursor */` |
|       3 | 5040 | `			pGen->pIn++;` |
|       1 | 5041 | `		}` |
|       1 | 5042 | `	}` |
|    8362 | 5043 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5044 | `		/* Syntax error */` |
|     ! 0 | 5045 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5046 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5047 | `		if( rc == SXERR_ABORT ){` |
|       - | 5048 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5049 | `			return SXERR_ABORT;` |
|       - | 5050 | `		}` |
|     ! 0 | 5051 | `		return SXRET_OK;` |
|       - | 5052 | `	}` |
|    8362 | 5053 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8362 | 5054 | `	pEnd = 0; /* cc warning */` |
|       - | 5055 | `	/* Delimit the interface body */` |
|    8362 | 5056 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8362 | 5057 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5058 | `		/* Syntax error */` |
|     ! 0 | 5059 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5060 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5061 | `		if( rc == SXERR_ABORT ){` |
|       - | 5062 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5063 | `			return SXERR_ABORT;` |
|       - | 5064 | `		}` |
|     ! 0 | 5065 | `		return SXRET_OK;` |
|       - | 5066 | `	}` |
|       - | 5067 | `	/* Swap token stream */` |
|    8362 | 5068 | `	pTmp = pGen->pEnd;` |
|    8362 | 5069 | `	pGen->pEnd = pEnd;` |
|       - | 5070 | `	/* Start the parse process` |
|       - | 5071 | `	 * Note (According to the PHP reference manual):` |
|       - | 5072 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5073 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5074 | `	 */` |
|   15302 | 5075 | `	for(;;){` |
|       - | 5076 | `		/* Jump leading/trailing semi-colons */` |
|   52850 | 5077 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   22246 | 5078 | `			pGen->pIn++;` |
|       2 | 5079 | `		}` |
|   30606 | 5080 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5081 | `			/* End of interface body */` |
|    8362 | 5082 | `			break;` |
|       - | 5083 | `		}` |
|   22246 | 5084 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5085 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5086 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5087 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5088 | `			if( rc == SXERR_ABORT ){` |
|       - | 5089 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5090 | `				return SXERR_ABORT;` |
|       - | 5091 | `			}` |
|     ! 0 | 5092 | `			goto done;` |
|       - | 5093 | `		}` |
|       - | 5094 | `		/* Extract the current keyword */` |
|   22246 | 5095 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22246 | 5096 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5097 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5098 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5099 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5100 | `		}` |
|   22246 | 5101 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5102 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5103 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5104 | `			if( rc == SXERR_ABORT ){` |
|       - | 5105 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5106 | `				return SXERR_ABORT;` |
|       - | 5107 | `			}` |
|     ! 0 | 5108 | `			goto done;` |
|       - | 5109 | `		}` |
|   22246 | 5110 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5111 | `			/* Advance the stream cursor */` |
|   22242 | 5112 | `			pGen->pIn++;` |
|   22242 | 5113 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5114 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5115 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5116 | `				if( rc == SXERR_ABORT ){` |
|       - | 5117 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5118 | `					return SXERR_ABORT;` |
|       - | 5119 | `				}` |
|     ! 0 | 5120 | `				goto done;` |
|       - | 5121 | `			}` |
|   22242 | 5122 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22242 | 5123 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5124 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5125 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5126 | `				if( rc == SXERR_ABORT ){` |
|       - | 5127 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5128 | `					return SXERR_ABORT;` |
|       - | 5129 | `				}` |
|     ! 0 | 5130 | `				goto done;` |
|       - | 5131 | `			}` |
|   11120 | 5132 | `		}` |
|   22246 | 5133 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5134 | `			/* Parse constant */` |
|       3 | 5135 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5136 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5137 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5138 | `					return SXERR_ABORT;` |
|       - | 5139 | `				}` |
|     ! 0 | 5140 | `				goto done;` |
|       - | 5141 | `			}` |
|       2 | 5142 | `		}else{` |
|   22244 | 5143 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   22244 | 5144 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5145 | `				/* Static method,record that */` |
|     ! 0 | 5146 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5147 | `				/* Advance the stream cursor */` |
|     ! 0 | 5148 | `				pGen->pIn++;` |
|     ! 0 | 5149 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5150 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5151 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5152 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5153 | `						if( rc == SXERR_ABORT ){` |
|       - | 5154 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5155 | `							return SXERR_ABORT;` |
|       - | 5156 | `						}` |
|     ! 0 | 5157 | `						goto done;` |
|       - | 5158 | `				}` |
|     ! 0 | 5159 | `			}` |
|       - | 5160 | `			/* Process method signature (no body for interface methods) */` |
|   22244 | 5161 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   22244 | 5162 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5163 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5164 | `					return SXERR_ABORT;` |
|       - | 5165 | `				}` |
|     ! 0 | 5166 | `				goto done;` |
|       - | 5167 | `			}` |
|       - | 5168 | `		}` |
|       2 | 5169 | `	}` |
|       - | 5170 | `	/* Install the interface */` |
|    8362 | 5171 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8362 | 5172 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5173 | `		/* Inherit from the base interface */` |
|       3 | 5174 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5175 | `	}` |
|    8362 | 5176 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5177 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5178 | `		return SXERR_ABORT;` |
|       - | 5179 | `	}` |
|    4180 | 5180 | `done:` |
|       - | 5181 | `	/* Point beyond the interface body */` |
|    8362 | 5182 | `	pGen->pIn  = &pEnd[1];` |
|    8362 | 5183 | `	pGen->pEnd = pTmp;` |
|    8362 | 5184 | `	return PH7_OK;` |
|    4182 | 5185 |  |
|       - | 5186 | `/*` |
|       - | 5187 | ` * Compile a user-defined class.` |
|       - | 5188 | ` * According to the PHP language reference manual` |
|       - | 5189 | ` *  class` |
|       - | 5190 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5191 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5192 | ` *  of the properties and methods belonging to the class.` |
|       - | 5193 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5194 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5195 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5196 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5197 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5198 | ` *  (called "methods").` |
|       - | 5199 | ` */` |
|       - | 5200 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5201 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5202 | `struct TraitUseEntry {` |
|       - | 5203 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5204 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5205 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5206 | `};` |
|       - | 5207 | `/*` |
|       - | 5208 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5209 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5210 | ` */` |
|   33688 | 5211 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5212 |  |
|       - | 5213 | `	ph7_class **apIface;` |
|       - | 5214 | `	sxu32 nIface,i;` |
|       - | 5215 | `	sxi32 rc;` |
|   33690 | 5216 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5217 | `		return SXRET_OK;` |
|       - | 5218 | `	}` |
|   33690 | 5219 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   33690 | 5220 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   36502 | 5221 | `	for(i = 0; i < nIface; i++){` |
|    2814 | 5222 | `		ph7_class *pIface = apIface[i];` |
|       - | 5223 | `		SyHashEntry *pEntry;` |
|    2814 | 5224 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16770 | 5225 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13958 | 5226 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5227 | `			ph7_class_method *pImplMeth;` |
|   13958 | 5228 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5229 | `			/* Find the implementing method in the class */` |
|   13958 | 5230 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13958 | 5231 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5232 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5233 | `			}` |
|       - | 5234 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13944 | 5235 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5236 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5237 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5238 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5239 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5240 | `					return SXERR_ABORT;` |
|       - | 5241 | `				}` |
|       1 | 5242 | `			}` |
|       - | 5243 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5244 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5245 | `			 */` |
|       - | 5246 | `			{` |
|   13944 | 5247 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13944 | 5248 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13944 | 5249 | `				int sigError = 0;` |
|   13944 | 5250 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5251 | `					sigError = 1;` |
|   13943 | 5252 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5253 | `					/* Extra parameters must all have default values */` |
|       5 | 5254 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5255 | `					sxu32 k;` |
|       7 | 5256 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5257 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5258 | `							sigError = 1;` |
|       3 | 5259 | `							break;` |
|       - | 5260 | `						}` |
|       2 | 5261 | `					}` |
|       2 | 5262 | `				}` |
|   13944 | 5263 | `				if( sigError ){` |
|       - | 5264 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5265 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5266 | `					sxu32 j;` |
|       5 | 5267 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5268 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5269 | `					/* Build implementing method signature */` |
|       5 | 5270 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5271 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5272 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5273 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5274 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5275 | `					}` |
|       - | 5276 | `					/* Build interface method signature */` |
|       5 | 5277 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5278 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5279 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5280 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5281 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5282 | `					}` |
|       7 | 5283 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5284 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5285 | `						&pClass->sName,pMName,` |
|       4 | 5286 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5287 | `						&pIface->sName,pMName,` |
|       4 | 5288 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5289 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5290 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5291 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5292 | `						return SXERR_ABORT;` |
|       - | 5293 | `					}` |
|       2 | 5294 | `				}` |
|       - | 5295 | `			}` |
|       2 | 5296 | `		}` |
|    1408 | 5297 | `	}` |
|   33690 | 5298 | `	return SXRET_OK;` |
|   16846 | 5299 |  |
|       - | 5300 | `/*` |
|       - | 5301 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5302 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5303 | ` */` |
|   33688 | 5304 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5305 |  |
|       - | 5306 | `	ph7_class_method *pMeth;` |
|       - | 5307 | `	SyHashEntry *pEntry;` |
|       - | 5308 | `	sxu32 nAbstract;` |
|       - | 5309 | `	SyBlob sMsg;` |
|       - | 5310 | `	sxi32 rc;` |
|       - | 5311 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   33690 | 5312 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      18 | 5313 | `		return SXRET_OK;` |
|       - | 5314 | `	}` |
|       - | 5315 | `	/* Count abstract methods */` |
|   33674 | 5316 | `	nAbstract = 0;` |
|   33674 | 5317 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  317282 | 5318 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  283610 | 5319 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  283610 | 5320 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5321 | `			nAbstract++;` |
|       8 | 5322 | `		}` |
|       2 | 5323 | `	}` |
|   33674 | 5324 | `	if( nAbstract == 0 ){` |
|   33660 | 5325 | `		return SXRET_OK;` |
|       - | 5326 | `	}` |
|       - | 5327 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5328 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5329 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5330 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5331 | `		&pClass->sName,nAbstract,` |
|       7 | 5332 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5333 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5334 | `	/* Second pass: list methods with origins */` |
|       - | 5335 | `	{` |
|      15 | 5336 | `		sxu32 nListed = 0;` |
|      15 | 5337 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5338 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5339 | `			ph7_class *pOrigin = 0;` |
|       - | 5340 | `			SyString *pMName;` |
|      19 | 5341 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5342 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5343 | `				continue;` |
|       - | 5344 | `			}` |
|      17 | 5345 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5346 | `			if( nListed > 0 ){` |
|       3 | 5347 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5348 | `			}` |
|       - | 5349 | `			/* Find the origin of this abstract method.` |
|       - | 5350 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5351 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5352 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5353 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5354 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5355 | `			 * class's namespace.` |
|       - | 5356 | `			 */` |
|       - | 5357 | `			{` |
|       - | 5358 | `				ph7_class **apIface;` |
|       - | 5359 | `				ph7_class **apTrait;` |
|       - | 5360 | `				ph7_class *pWalk;` |
|       - | 5361 | `				sxu32 i;` |
|       - | 5362 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5363 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5364 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5365 | `				 */` |
|      17 | 5366 | `				if( pClass->pBase ){` |
|       9 | 5367 | `					pWalk = pClass->pBase;` |
|      17 | 5368 | `					while( pWalk ){` |
|       - | 5369 | `						ph7_class_method *pParentMeth;` |
|      11 | 5370 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5371 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5372 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5373 | `							 * in this class's ancestor chain.` |
|       - | 5374 | `							 */` |
|      11 | 5375 | `							int fromIface = 0;` |
|      11 | 5376 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5377 | `							while( pAnc ){` |
|       - | 5378 | `								ph7_class **apPI;` |
|       - | 5379 | `								sxu32 j;` |
|      13 | 5380 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5381 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5382 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5383 | `										fromIface = 1;` |
|       9 | 5384 | `										break;` |
|       - | 5385 | `									}` |
|     ! 0 | 5386 | `								}` |
|      13 | 5387 | `								if( fromIface ) break;` |
|       5 | 5388 | `								pAnc = pAnc->pBase;` |
|       1 | 5389 | `							}` |
|      11 | 5390 | `							if( !fromIface ){` |
|       3 | 5391 | `								pOrigin = pWalk;` |
|       3 | 5392 | `								break;` |
|       - | 5393 | `							}` |
|       4 | 5394 | `						}` |
|       9 | 5395 | `						pWalk = pWalk->pBase;` |
|       1 | 5396 | `					}` |
|       4 | 5397 | `				}` |
|       - | 5398 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5399 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5400 | `				 */` |
|      17 | 5401 | `				if( !pOrigin ){` |
|      15 | 5402 | `					pWalk = pClass;` |
|      37 | 5403 | `					while( pWalk && !pOrigin ){` |
|      23 | 5404 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5405 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5406 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5407 | `							ph7_class *pDeepest = 0;` |
|      25 | 5408 | `							while( pIface ){` |
|      13 | 5409 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5410 | `									pDeepest = pIface;` |
|       6 | 5411 | `								}` |
|      13 | 5412 | `								pIface = pIface->pBase;` |
|       1 | 5413 | `							}` |
|      13 | 5414 | `							if( pDeepest ){` |
|      13 | 5415 | `								pOrigin = pDeepest;` |
|      13 | 5416 | `								break;` |
|       - | 5417 | `							}` |
|     ! 0 | 5418 | `						}` |
|      23 | 5419 | `						pWalk = pWalk->pBase;` |
|       1 | 5420 | `					}` |
|       7 | 5421 | `				}` |
|       - | 5422 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5423 | `				if( !pOrigin ){` |
|       3 | 5424 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5425 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5426 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5427 | `							pOrigin = pClass;` |
|       3 | 5428 | `							break;` |
|       - | 5429 | `						}` |
|     ! 0 | 5430 | `					}` |
|       1 | 5431 | `				}` |
|       - | 5432 | `			}` |
|      17 | 5433 | `			if( pOrigin ){` |
|      17 | 5434 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5435 | `			}else{` |
|       - | 5436 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5437 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5438 | `			}` |
|      17 | 5439 | `			nListed++;` |
|       1 | 5440 | `		}` |
|       - | 5441 | `	}` |
|      15 | 5442 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5443 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5444 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5445 | `	SyBlobRelease(&sMsg);` |
|      15 | 5446 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5447 | `		return SXERR_ABORT;` |
|       - | 5448 | `	}` |
|      15 | 5449 | `	return SXRET_OK;` |
|   16846 | 5450 |  |
|   33692 | 5451 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5452 |  |
|   33694 | 5453 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5454 | `	ph7_class *pClass,*pBase;` |
|       - | 5455 | `	SyToken *pEnd,*pTmp;` |
|       - | 5456 | `	sxi32 iProtection;` |
|       - | 5457 | `	SySet aInterfaces;` |
|       - | 5458 | `	SySet aUseEntries;` |
|       - | 5459 | `	sxi32 iAttrflags;` |
|       - | 5460 | `	SyString *pName;` |
|       - | 5461 | `	sxi32 nKwrd;` |
|       - | 5462 | `	sxi32 rc;` |
|       - | 5463 | `	/* Jump the 'class' keyword */` |
|   33694 | 5464 | `	pGen->pIn++;` |
|   33694 | 5465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5466 | `		/* Syntax error */` |
|     ! 0 | 5467 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5468 | `		if( rc == SXERR_ABORT ){` |
|       - | 5469 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5470 | `			return SXERR_ABORT;` |
|       - | 5471 | `		}` |
|       - | 5472 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5473 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5474 | `			pGen->pIn++;` |
|     ! 0 | 5475 | `		}` |
|     ! 0 | 5476 | `		return SXRET_OK;` |
|       - | 5477 | `	}` |
|       - | 5478 | `	/* Extract class name */` |
|   33694 | 5479 | `	pName = &pGen->pIn->sData;` |
|       - | 5480 | `	/* Advance the stream cursor */` |
|   33694 | 5481 | `	pGen->pIn++;` |
|       - | 5482 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5483 | `		SyBlob sFQN;` |
|       - | 5484 | `		SyString sFQNStr;` |
|   33694 | 5485 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   33694 | 5486 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   33694 | 5487 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   33694 | 5488 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   33694 | 5489 | `		SyBlobRelease(&sFQN);` |
|       - | 5490 | `	}` |
|   33694 | 5491 | `	if( pClass == 0 ){` |
|     ! 0 | 5492 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5493 | `		return SXERR_ABORT;` |
|       - | 5494 | `	}` |
|       - | 5495 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   33694 | 5496 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   33694 | 5497 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5498 | `	/* Assume a standalone class */` |
|   33694 | 5499 | `	pBase = 0;` |
|   33694 | 5500 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5501 | `		SyString *pBaseName;` |
|   22304 | 5502 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22304 | 5503 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   19494 | 5504 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   19494 | 5505 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5506 | `				/* Syntax error */` |
|     ! 0 | 5507 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5508 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5509 | `					pName);` |
|     ! 0 | 5510 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5511 | `				if( rc == SXERR_ABORT ){` |
|       - | 5512 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5513 | `					return SXERR_ABORT;` |
|       - | 5514 | `				}` |
|     ! 0 | 5515 | `				return SXRET_OK;` |
|       - | 5516 | `			}` |
|       - | 5517 | `			/* Extract base class name and resolve through namespace/imports */` |
|   19494 | 5518 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5519 | `			{` |
|       - | 5520 | `				SyBlob sResolved;` |
|   19494 | 5521 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   19494 | 5522 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   29240 | 5523 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   19492 | 5524 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   19494 | 5525 | `				SyBlobRelease(&sResolved);` |
|       - | 5526 | `			}` |
|       - | 5527 | `			/* Interfaces are not allowed */` |
|   19494 | 5528 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5529 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5530 | `			}` |
|   19494 | 5531 | `			if( pBase == 0 ){` |
|       - | 5532 | `				/* Inexistant base class */` |
|     ! 0 | 5533 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5534 | `				if( rc == SXERR_ABORT ){` |
|       - | 5535 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5536 | `					return SXERR_ABORT;` |
|       - | 5537 | `				}` |
|     ! 0 | 5538 | `			}else{` |
|   19494 | 5539 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5540 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5541 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5542 | `					if( rc == SXERR_ABORT ){` |
|       - | 5543 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5544 | `						return SXERR_ABORT;` |
|       - | 5545 | `					}` |
|     ! 0 | 5546 | `				}` |
|       - | 5547 | `			}` |
|       - | 5548 | `			/* Advance the stream cursor */` |
|   19494 | 5549 | `			pGen->pIn++;` |
|    9746 | 5550 | `		}` |
|   22304 | 5551 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5552 | `			ph7_class *pInterface;` |
|       - | 5553 | `			SyString *pIntName;` |
|       - | 5554 | `			/* Interface implementation */` |
|    2814 | 5555 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1406 | 5556 | `			for(;;){` |
|    2814 | 5557 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5558 | `					/* Syntax error */` |
|     ! 0 | 5559 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5560 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5561 | `						pName);` |
|     ! 0 | 5562 | `					if( rc == SXERR_ABORT ){` |
|       - | 5563 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5564 | `						return SXERR_ABORT;` |
|       - | 5565 | `					}` |
|     ! 0 | 5566 | `					break;` |
|       - | 5567 | `				}` |
|       - | 5568 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2814 | 5569 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5570 | `				{` |
|       - | 5571 | `					SyBlob sResolved;` |
|    2814 | 5572 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2814 | 5573 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5626 | 5574 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2812 | 5575 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2814 | 5576 | `					SyBlobRelease(&sResolved);` |
|       - | 5577 | `				}` |
|       - | 5578 | `				/* Only interfaces are allowed */` |
|    2814 | 5579 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5580 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5581 | `				}` |
|    2814 | 5582 | `				if( pInterface == 0 ){` |
|       - | 5583 | `					/* Inexistant interface */` |
|     ! 0 | 5584 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5585 | `					if( rc == SXERR_ABORT ){` |
|       - | 5586 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5587 | `						return SXERR_ABORT;` |
|       - | 5588 | `					}` |
|     ! 0 | 5589 | `				}else{` |
|       - | 5590 | `					/* Register interface */` |
|    2814 | 5591 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5592 | `				}` |
|       - | 5593 | `				/* Advance the stream cursor */` |
|    2814 | 5594 | `				pGen->pIn++;` |
|    2814 | 5595 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1408 | 5596 | `					break;` |
|       - | 5597 | `				}` |
|     ! 0 | 5598 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5599 | `			}` |
|    1406 | 5600 | `		}` |
|   11151 | 5601 | `	}` |
|   33694 | 5602 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5603 | `		/* Syntax error */` |
|     ! 0 | 5604 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5605 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5606 | `		if( rc == SXERR_ABORT ){` |
|       - | 5607 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5608 | `			return SXERR_ABORT;` |
|       - | 5609 | `		}` |
|     ! 0 | 5610 | `		return SXRET_OK;` |
|       - | 5611 | `	}` |
|   33694 | 5612 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   33694 | 5613 | `	pEnd = 0; /* cc warning */` |
|       - | 5614 | `	/* Delimit the class body */` |
|   33694 | 5615 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   33694 | 5616 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5617 | `		/* Syntax error */` |
|     ! 0 | 5618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5619 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5620 | `		if( rc == SXERR_ABORT ){` |
|       - | 5621 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5622 | `			return SXERR_ABORT;` |
|       - | 5623 | `		}` |
|     ! 0 | 5624 | `		return SXRET_OK;` |
|       - | 5625 | `	}` |
|       - | 5626 | `	/* Swap token stream */` |
|   33694 | 5627 | `	pTmp = pGen->pEnd;` |
|   33694 | 5628 | `	pGen->pEnd = pEnd;` |
|       - | 5629 | `	/* Set the inherited flags */` |
|   33694 | 5630 | `	pClass->iFlags = iFlags;` |
|       - | 5631 | `	/* Start the parse process */` |
|   72543 | 5632 | `	for(;;){` |
|       - | 5633 | `		/* Jump leading/trailing semi-colons */` |
|  217740 | 5634 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   36334 | 5635 | `			pGen->pIn++;` |
|       2 | 5636 | `		}` |
|  181408 | 5637 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5638 | `			/* End of class body */` |
|   33690 | 5639 | `			break;` |
|       - | 5640 | `		}` |
|  147720 | 5641 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5642 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5643 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5644 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5645 | `			if( rc == SXERR_ABORT ){` |
|       - | 5646 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5647 | `				return SXERR_ABORT;` |
|       - | 5648 | `			}` |
|     ! 0 | 5649 | `			goto done;` |
|       - | 5650 | `		}` |
|       - | 5651 | `		/* Assume public visibility */` |
|  147720 | 5652 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  147720 | 5653 | `		iAttrflags = 0;` |
|  147720 | 5654 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5655 | `			/* Extract the current keyword */` |
|  147720 | 5656 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  147720 | 5657 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5658 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5659 | `				TraitUseEntry sUse;` |
|      41 | 5660 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5661 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5662 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5663 | `				for(;;){` |
|       - | 5664 | `					ph7_class *pTrait;` |
|       - | 5665 | `					SyString *pTraitName;` |
|      49 | 5666 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5667 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5668 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5669 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5670 | `							return SXERR_ABORT;` |
|       - | 5671 | `						}` |
|     ! 0 | 5672 | `						break;` |
|       - | 5673 | `					}` |
|      49 | 5674 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5675 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5676 | `						SyBlob sResolved;` |
|      49 | 5677 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5678 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5679 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5680 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5681 | `						SyBlobRelease(&sResolved);` |
|       - | 5682 | `					}` |
|       - | 5683 | `					/* Only traits are allowed */` |
|      49 | 5684 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5685 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5686 | `					}` |
|      49 | 5687 | `					if( pTrait == 0 ){` |
|     ! 0 | 5688 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5689 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5690 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5691 | `							return SXERR_ABORT;` |
|       - | 5692 | `						}` |
|     ! 0 | 5693 | `					}else{` |
|      49 | 5694 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5695 | `					}` |
|      49 | 5696 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5697 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5698 | `						break;` |
|       - | 5699 | `					}` |
|       9 | 5700 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5701 | `				}` |
|       - | 5702 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5703 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5704 | `					SyToken *pBlock;` |
|       9 | 5705 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5706 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5707 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5708 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5709 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5710 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5711 | `					}else{` |
|     ! 0 | 5712 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5713 | `					}` |
|       4 | 5714 | `				}` |
|      41 | 5715 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5716 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5717 | `				continue;` |
|       - | 5718 | `			}` |
|  147680 | 5719 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  144810 | 5720 | `				iProtection = nKwrd;` |
|  144810 | 5721 | `				pGen->pIn++; /* Jump the visibility token */` |
|  144810 | 5722 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5723 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5724 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5725 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5726 | `					if( rc == SXERR_ABORT ){` |
|       - | 5727 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5728 | `						return SXERR_ABORT;` |
|       - | 5729 | `					}` |
|     ! 0 | 5730 | `					goto done;` |
|       - | 5731 | `				}` |
|  144810 | 5732 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5733 | `					/* Attribute declaration */` |
|   36280 | 5734 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   36280 | 5735 | `					if( rc != SXRET_OK ){` |
|       3 | 5736 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5737 | `							return SXERR_ABORT;` |
|       - | 5738 | `						}` |
|       3 | 5739 | `						goto done;` |
|       - | 5740 | `					}` |
|   36278 | 5741 | `					continue;` |
|       - | 5742 | `				}` |
|       - | 5743 | `				/* Extract the keyword */` |
|  108532 | 5744 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54265 | 5745 | `			}` |
|  111402 | 5746 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5747 | `				/* Process constant declaration */` |
|      10 | 5748 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5749 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5750 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5751 | `						return SXERR_ABORT;` |
|       - | 5752 | `					}` |
|     ! 0 | 5753 | `					goto done;` |
|       - | 5754 | `				}` |
|       6 | 5755 | `			}else{` |
|  111394 | 5756 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5757 | `					/* Static method or attribute,record that */` |
|    2800 | 5758 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2800 | 5759 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2800 | 5760 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5761 | `						/* Extract the keyword */` |
|    2796 | 5762 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2796 | 5763 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5764 | `							iProtection = nKwrd;` |
|     ! 0 | 5765 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5766 | `						}` |
|    1397 | 5767 | `					}` |
|    2800 | 5768 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5769 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5770 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5771 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5772 | `						if( rc == SXERR_ABORT ){` |
|       - | 5773 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5774 | `							return SXERR_ABORT;` |
|       - | 5775 | `						}` |
|     ! 0 | 5776 | `						goto done;` |
|       - | 5777 | `					}` |
|    2800 | 5778 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5779 | `						/* Attribute declaration */` |
|       5 | 5780 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5781 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5782 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5783 | `								return SXERR_ABORT;` |
|       - | 5784 | `							}` |
|     ! 0 | 5785 | `							goto done;` |
|       - | 5786 | `						}` |
|       5 | 5787 | `						continue;` |
|       - | 5788 | `					}` |
|       - | 5789 | `					/* Extract the keyword */` |
|    2796 | 5790 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  109993 | 5791 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5792 | `					/* Abstract method,record that */` |
|       8 | 5793 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5794 | `					/* Mark the whole class as abstract */` |
|       8 | 5795 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5796 | `					/* Advance the stream cursor */` |
|       8 | 5797 | `					pGen->pIn++;` |
|       8 | 5798 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5799 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5800 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5801 | `							iProtection = nKwrd;` |
|       6 | 5802 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5803 | `						}` |
|       3 | 5804 | `					}` |
|       8 | 5805 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5806 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5807 | `							/* Static method */` |
|     ! 0 | 5808 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5809 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5810 | `					}` |
|       8 | 5811 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5812 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5813 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5814 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5815 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5816 | `							if( rc == SXERR_ABORT ){` |
|       - | 5817 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5818 | `								return SXERR_ABORT;` |
|       - | 5819 | `							}` |
|     ! 0 | 5820 | `							goto done;` |
|       - | 5821 | `					}` |
|       8 | 5822 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  108593 | 5823 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5824 | `					/* final method ,record that */` |
|       5 | 5825 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5826 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5827 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5828 | `						/* Extract the keyword */` |
|       5 | 5829 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5830 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5831 | `							iProtection = nKwrd;` |
|       5 | 5832 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5833 | `						}` |
|       2 | 5834 | `					}` |
|       5 | 5835 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5836 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5837 | `							/* Static method */` |
|     ! 0 | 5838 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5839 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5840 | `					}` |
|       5 | 5841 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5842 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5843 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5844 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5845 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5846 | `							if( rc == SXERR_ABORT ){` |
|       - | 5847 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5848 | `								return SXERR_ABORT;` |
|       - | 5849 | `							}` |
|     ! 0 | 5850 | `							goto done;` |
|       - | 5851 | `					}` |
|       5 | 5852 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5853 | `				}` |
|  111390 | 5854 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5855 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5856 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5857 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5858 | `						if( rc == SXERR_ABORT ){` |
|       - | 5859 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5860 | `							return SXERR_ABORT;` |
|       - | 5861 | `						}` |
|     ! 0 | 5862 | `						goto done;` |
|       - | 5863 | `				}` |
|  111390 | 5864 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5865 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5866 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5867 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5868 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5869 | `						if( rc == SXERR_ABORT ){` |
|       - | 5870 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5871 | `							return SXERR_ABORT;` |
|       - | 5872 | `						}` |
|     ! 0 | 5873 | `						goto done;` |
|       - | 5874 | `					}` |
|       - | 5875 | `					/* Attribute declaration */` |
|       7 | 5876 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5877 | `				}else{` |
|       - | 5878 | `					/* Process method declaration */` |
|  111384 | 5879 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5880 | `				}` |
|  111390 | 5881 | `				if( rc != SXRET_OK ){` |
|       3 | 5882 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5883 | `						return SXERR_ABORT;` |
|       - | 5884 | `					}` |
|       3 | 5885 | `					goto done;` |
|       - | 5886 | `				}` |
|       - | 5887 | `			}` |
|   55699 | 5888 | `		}else{` |
|       - | 5889 | `			/* Attribute declaration */` |
|     ! 0 | 5890 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5891 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5892 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5893 | `					return SXERR_ABORT;` |
|       - | 5894 | `				}` |
|     ! 0 | 5895 | `				goto done;` |
|       - | 5896 | `			}` |
|       - | 5897 | `		}` |
|       2 | 5898 | `	}` |
|       - | 5899 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5900 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5901 | `	 */` |
|       - | 5902 | `	{` |
|       - | 5903 | `		TraitUseEntry *apUse;` |
|       - | 5904 | `		sxu32 nU;` |
|   33690 | 5905 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   33730 | 5906 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 5907 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 5908 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 5909 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 5910 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5911 | `			sxu32 nT;` |
|      41 | 5912 | `			if( !hasResolution ){` |
|       - | 5913 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 5914 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 5915 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 5916 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5917 | `						break;` |
|       - | 5918 | `					}` |
|      20 | 5919 | `				}` |
|      17 | 5920 | `			}else{` |
|       - | 5921 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 5922 | `				 * then use the block to resolve method conflicts.` |
|       - | 5923 | `				 */` |
|       - | 5924 | `				SyToken *pR;` |
|      19 | 5925 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 5926 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 5927 | `					ph7_class_attr *pAR;` |
|       - | 5928 | `					SyHashEntry *pER;` |
|       - | 5929 | `					SyString *pNR;` |
|      11 | 5930 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 5931 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 5932 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 5933 | `						pNR = &pAR->sName;` |
|     ! 0 | 5934 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 5935 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 5936 | `						}` |
|     ! 0 | 5937 | `					}` |
|      11 | 5938 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 5939 | `				}` |
|       - | 5940 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 5941 | `				pR = pUse->pResolvStart;` |
|      21 | 5942 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5943 | `					SyString sTrait,sMethod;` |
|       - | 5944 | `					ph7_class *pSrcTrait;` |
|       - | 5945 | `					ph7_class_method *pMeth;` |
|       - | 5946 | `					sxi32 nRKwrd;` |
|      33 | 5947 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5948 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5949 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5950 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5951 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5952 | `					sMethod = pR->sData;` |
|      13 | 5953 | `					pR++;` |
|      13 | 5954 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5955 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5956 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5957 | `							sTrait = sMethod;` |
|       7 | 5958 | `							pR++;` |
|       7 | 5959 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5960 | `							sMethod = pR->sData;` |
|       7 | 5961 | `							pR++;` |
|       3 | 5962 | `						}` |
|       3 | 5963 | `					}` |
|      13 | 5964 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5965 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5966 | `						continue;` |
|       - | 5967 | `					}` |
|      13 | 5968 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5969 | `					pR++;` |
|      13 | 5970 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 5971 | `						pSrcTrait = 0;` |
|       7 | 5972 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 5973 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 5974 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 5975 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 5976 | `								pSrcTrait = apTrait[nT];` |
|       5 | 5977 | `								break;` |
|       - | 5978 | `							}` |
|       2 | 5979 | `						}` |
|       5 | 5980 | `						if( pSrcTrait ){` |
|       5 | 5981 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 5982 | `							if( pMeth ){` |
|       5 | 5983 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 5984 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 5985 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 5986 | `								}` |
|       2 | 5987 | `							}` |
|       2 | 5988 | `						}` |
|       2 | 5989 | `					}` |
|      29 | 5990 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5991 | `				}` |
|       - | 5992 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 5993 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 5994 | `					ph7_class_method *pMR;` |
|       - | 5995 | `					SyHashEntry *pER;` |
|       - | 5996 | `					SyString *pNR;` |
|      11 | 5997 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 5998 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 5999 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6000 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6001 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6002 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6003 | `						}` |
|       1 | 6004 | `					}` |
|       6 | 6005 | `				}` |
|       - | 6006 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6007 | `				pR = pUse->pResolvStart;` |
|      21 | 6008 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6009 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6010 | `					ph7_class *pSrcTrait;` |
|       - | 6011 | `					ph7_class_method *pMeth;` |
|      21 | 6012 | `					int hasQual = 0;` |
|       - | 6013 | `					sxi32 nRKwrd;` |
|      33 | 6014 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6015 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6016 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6017 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6018 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6019 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6020 | `					sMethod = pR->sData;` |
|      13 | 6021 | `					pR++;` |
|      13 | 6022 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6023 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6024 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6025 | `							sTrait = sMethod;` |
|       7 | 6026 | `							hasQual = 1;` |
|       7 | 6027 | `							pR++;` |
|       7 | 6028 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6029 | `							sMethod = pR->sData;` |
|       7 | 6030 | `							pR++;` |
|       3 | 6031 | `						}` |
|       3 | 6032 | `					}` |
|      13 | 6033 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6034 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6035 | `						continue;` |
|       - | 6036 | `					}` |
|      13 | 6037 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6038 | `					pR++;` |
|      13 | 6039 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6040 | `						sxi32 iNewVis = -1;` |
|       9 | 6041 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6042 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6043 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6044 | `								iNewVis = nAK;` |
|       7 | 6045 | `								pR++;` |
|       3 | 6046 | `							}` |
|       3 | 6047 | `						}` |
|       9 | 6048 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6049 | `							sAlias = pR->sData;` |
|       7 | 6050 | `							pR++;` |
|       3 | 6051 | `						}` |
|       9 | 6052 | `						pMeth = 0;` |
|       9 | 6053 | `						if( hasQual ){` |
|       3 | 6054 | `							pSrcTrait = 0;` |
|       5 | 6055 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6056 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6057 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6058 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6059 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6060 | `									break;` |
|       - | 6061 | `								}` |
|       2 | 6062 | `							}` |
|       3 | 6063 | `							if( pSrcTrait ){` |
|       3 | 6064 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6065 | `							}` |
|       2 | 6066 | `						}else{` |
|       7 | 6067 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6068 | `						}` |
|       9 | 6069 | `						if( pMeth ){` |
|       9 | 6070 | `							if( sAlias.nByte > 0 ){` |
|       - | 6071 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6072 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6073 | `								 */` |
|       - | 6074 | `								ph7_class_method *pAlias;` |
|       - | 6075 | `								char *zAliasDup;` |
|       7 | 6076 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6077 | `								if( pAlias ){` |
|       7 | 6078 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6079 | `									if( iNewVis >= 0 ){` |
|       5 | 6080 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6081 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6082 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6083 | `									}` |
|       7 | 6084 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6085 | `									if( zAliasDup ){` |
|       7 | 6086 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6087 | `									}` |
|       4 | 6088 | `								}` |
|       6 | 6089 | `							}else if( iNewVis >= 0 ){` |
|       - | 6090 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6091 | `								ph7_class_method *pCopy;` |
|       3 | 6092 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6093 | `								if( pCopy ){` |
|       3 | 6094 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6095 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6096 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6097 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6098 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6099 | `									/* Replace the method in the class hash */` |
|       3 | 6100 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6101 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6102 | `								}` |
|       1 | 6103 | `							}` |
|       4 | 6104 | `						}` |
|       4 | 6105 | `						SXUNUSED(hasQual);` |
|       4 | 6106 | `					}` |
|      17 | 6107 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6108 | `				}` |
|       - | 6109 | `			}` |
|      41 | 6110 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6111 | `		}` |
|       - | 6112 | `	}` |
|       - | 6113 | `	/* Install the class */` |
|   33690 | 6114 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   33690 | 6115 | `	if( rc == SXRET_OK ){` |
|       - | 6116 | `		ph7_class **apInterface;` |
|       - | 6117 | `		sxu32 n;` |
|   33690 | 6118 | `		if( pBase ){` |
|       - | 6119 | `			/* Inherit from base class and mark as a subclass */` |
|   19494 | 6120 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    9746 | 6121 | `		}` |
|   33690 | 6122 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   36502 | 6123 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6124 | `			/* Implements one or more interface */` |
|    2814 | 6125 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2814 | 6126 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6127 | `				break;` |
|       - | 6128 | `			}` |
|    1408 | 6129 | `		}` |
|       - | 6130 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   33690 | 6131 | `		if( rc == SXRET_OK ){` |
|   33690 | 6132 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   33690 | 6133 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6134 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6135 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6136 | `				return SXERR_ABORT;` |
|       - | 6137 | `			}` |
|   16844 | 6138 | `		}` |
|       - | 6139 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   33690 | 6140 | `		if( rc == SXRET_OK ){` |
|   33690 | 6141 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   33690 | 6142 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6143 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6144 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6145 | `				return SXERR_ABORT;` |
|       - | 6146 | `			}` |
|   16844 | 6147 | `		}` |
|   16844 | 6148 | `	}` |
|   33690 | 6149 | `	SySetRelease(&aUseEntries);` |
|   33690 | 6150 | `	SySetRelease(&aInterfaces);` |
|   33690 | 6151 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6152 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6153 | `		return SXERR_ABORT;` |
|       - | 6154 | `	}` |
|   16844 | 6155 | `done:` |
|       - | 6156 | `	/* Point beyond the class body */` |
|   33694 | 6157 | `	pGen->pIn = &pEnd[1];` |
|   33694 | 6158 | `	pGen->pEnd = pTmp;` |
|   33694 | 6159 | `	return PH7_OK;` |
|   16848 | 6160 |  |
|       - | 6161 | `/*` |
|       - | 6162 | ` * Compile a user-defined abstract class.` |
|       - | 6163 | ` *  According to the PHP language reference manual` |
|       - | 6164 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6165 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6166 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6167 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6168 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6169 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6170 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6171 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6172 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6173 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6174 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6175 | ` *   could differ.` |
|       - | 6176 | ` */` |
|      14 | 6177 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6178 |  |
|       - | 6179 | `	sxi32 rc;` |
|      16 | 6180 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      16 | 6181 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      16 | 6182 | `	return rc;` |
|       2 | 6183 |  |
|       - | 6184 | `/*` |
|       - | 6185 | ` * Compile a user-defined final class.` |
|       - | 6186 | ` *  According to the PHP language reference manual` |
|       - | 6187 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6188 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6189 | ` *    final then it cannot be extended.` |
|       - | 6190 | ` */` |
|       2 | 6191 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6192 |  |
|       - | 6193 | `	sxi32 rc;` |
|       3 | 6194 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6195 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6196 | `	return rc;` |
|       1 | 6197 |  |
|       - | 6198 | `/*` |
|       - | 6199 | ` * Compile a user-defined trait.` |
|       - | 6200 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6201 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6202 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6203 | ` */` |
|      50 | 6204 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 6205 |  |
|      51 | 6206 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6207 | `	ph7_class *pClass;` |
|       - | 6208 | `	SyToken *pEnd,*pTmp;` |
|       - | 6209 | `	sxi32 iProtection;` |
|       - | 6210 | `	sxi32 iAttrflags;` |
|       - | 6211 | `	SyString *pName;` |
|       - | 6212 | `	sxi32 nKwrd;` |
|       - | 6213 | `	sxi32 rc;` |
|       - | 6214 | `	/* Jump the 'trait' keyword */` |
|      51 | 6215 | `	pGen->pIn++;` |
|      51 | 6216 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6217 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6218 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6219 | `			return SXERR_ABORT;` |
|       - | 6220 | `		}` |
|     ! 0 | 6221 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6222 | `			pGen->pIn++;` |
|     ! 0 | 6223 | `		}` |
|     ! 0 | 6224 | `		return SXRET_OK;` |
|       - | 6225 | `	}` |
|       - | 6226 | `	/* Extract trait name */` |
|      51 | 6227 | `	pName = &pGen->pIn->sData;` |
|      51 | 6228 | `	pGen->pIn++;` |
|       - | 6229 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6230 | `		SyBlob sFQN;` |
|       - | 6231 | `		SyString sFQNStr;` |
|      51 | 6232 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      51 | 6233 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      51 | 6234 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      51 | 6235 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      51 | 6236 | `		SyBlobRelease(&sFQN);` |
|       - | 6237 | `	}` |
|      51 | 6238 | `	if( pClass == 0 ){` |
|     ! 0 | 6239 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6240 | `		return SXERR_ABORT;` |
|       - | 6241 | `	}` |
|       - | 6242 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      51 | 6243 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6244 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6245 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6246 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6247 | `			return SXERR_ABORT;` |
|       - | 6248 | `		}` |
|     ! 0 | 6249 | `		return SXRET_OK;` |
|       - | 6250 | `	}` |
|      51 | 6251 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      51 | 6252 | `	pEnd = 0;` |
|      51 | 6253 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      51 | 6254 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6255 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6256 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6258 | `			return SXERR_ABORT;` |
|       - | 6259 | `		}` |
|     ! 0 | 6260 | `		return SXRET_OK;` |
|       - | 6261 | `	}` |
|       - | 6262 | `	/* Swap token stream */` |
|      51 | 6263 | `	pTmp = pGen->pEnd;` |
|      51 | 6264 | `	pGen->pEnd = pEnd;` |
|       - | 6265 | `	/* Mark as trait */` |
|      51 | 6266 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6267 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      52 | 6268 | `	for(;;){` |
|     141 | 6269 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6270 | `			pGen->pIn++;` |
|       1 | 6271 | `		}` |
|     121 | 6272 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      51 | 6273 | `			break;` |
|       - | 6274 | `		}` |
|      71 | 6275 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6276 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6277 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6278 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6279 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6280 | `				return SXERR_ABORT;` |
|       - | 6281 | `			}` |
|     ! 0 | 6282 | `			goto done;` |
|       - | 6283 | `		}` |
|      71 | 6284 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6285 | `		iAttrflags = 0;` |
|      71 | 6286 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6287 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6288 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6289 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6290 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6291 | `				for(;;){` |
|       - | 6292 | `					ph7_class *pUsedTrait;` |
|       - | 6293 | `					SyString *pUsedName;` |
|       5 | 6294 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6295 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6296 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6297 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6298 | `							return SXERR_ABORT;` |
|       - | 6299 | `						}` |
|     ! 0 | 6300 | `						break;` |
|       - | 6301 | `					}` |
|       5 | 6302 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6303 | `					{` |
|       - | 6304 | `						SyBlob sResolved;` |
|       5 | 6305 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6306 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6307 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6308 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6309 | `						SyBlobRelease(&sResolved);` |
|       - | 6310 | `					}` |
|       5 | 6311 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6312 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6313 | `					}` |
|       5 | 6314 | `					if( pUsedTrait == 0 ){` |
|       4 | 6315 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6316 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6317 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6318 | `							return SXERR_ABORT;` |
|       - | 6319 | `						}` |
|       2 | 6320 | `					}else{` |
|       3 | 6321 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6322 | `					}` |
|       5 | 6323 | `					pGen->pIn++;` |
|       5 | 6324 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6325 | `						break;` |
|       - | 6326 | `					}` |
|     ! 0 | 6327 | `					pGen->pIn++;` |
|     ! 0 | 6328 | `				}` |
|       5 | 6329 | `				continue;` |
|       - | 6330 | `			}` |
|      67 | 6331 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6332 | `				iProtection = nKwrd;` |
|      63 | 6333 | `				pGen->pIn++;` |
|      63 | 6334 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6335 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6336 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6337 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6338 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6339 | `						return SXERR_ABORT;` |
|       - | 6340 | `					}` |
|     ! 0 | 6341 | `					goto done;` |
|       - | 6342 | `				}` |
|      63 | 6343 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6344 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6345 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6346 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6347 | `							return SXERR_ABORT;` |
|       - | 6348 | `						}` |
|     ! 0 | 6349 | `						goto done;` |
|       - | 6350 | `					}` |
|      11 | 6351 | `					continue;` |
|       - | 6352 | `				}` |
|      53 | 6353 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6354 | `			}` |
|      57 | 6355 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6356 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6357 | `					"Traits cannot have constants");` |
|     ! 0 | 6358 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6359 | `					return SXERR_ABORT;` |
|       - | 6360 | `				}` |
|     ! 0 | 6361 | `				goto done;` |
|     ! 0 | 6362 | `			}else{` |
|      57 | 6363 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6364 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6365 | `					pGen->pIn++;` |
|       5 | 6366 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6367 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6368 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6369 | `							iProtection = nKwrd;` |
|     ! 0 | 6370 | `							pGen->pIn++;` |
|     ! 0 | 6371 | `						}` |
|       1 | 6372 | `					}` |
|       5 | 6373 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6374 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6375 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6376 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6377 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6378 | `							return SXERR_ABORT;` |
|       - | 6379 | `						}` |
|     ! 0 | 6380 | `						goto done;` |
|       - | 6381 | `					}` |
|       5 | 6382 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6383 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6384 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6385 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6386 | `								return SXERR_ABORT;` |
|       - | 6387 | `							}` |
|     ! 0 | 6388 | `							goto done;` |
|       - | 6389 | `						}` |
|       3 | 6390 | `						continue;` |
|       - | 6391 | `					}` |
|       3 | 6392 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6393 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6394 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6395 | `					pGen->pIn++;` |
|       5 | 6396 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6397 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6398 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6399 | `							iProtection = nKwrd;` |
|       5 | 6400 | `							pGen->pIn++;` |
|       2 | 6401 | `						}` |
|       2 | 6402 | `					}` |
|       5 | 6403 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6404 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6405 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6406 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6407 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6408 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6409 | `							return SXERR_ABORT;` |
|       - | 6410 | `						}` |
|     ! 0 | 6411 | `						goto done;` |
|       - | 6412 | `					}` |
|       5 | 6413 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6414 | `				}` |
|      55 | 6415 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6416 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6417 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6418 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6419 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6420 | `						return SXERR_ABORT;` |
|       - | 6421 | `					}` |
|     ! 0 | 6422 | `					goto done;` |
|       - | 6423 | `				}` |
|      55 | 6424 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6425 | `					pGen->pIn++;` |
|     ! 0 | 6426 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6427 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6428 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6429 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6430 | `							return SXERR_ABORT;` |
|       - | 6431 | `						}` |
|     ! 0 | 6432 | `						goto done;` |
|       - | 6433 | `					}` |
|     ! 0 | 6434 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6435 | `				}else{` |
|      55 | 6436 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6437 | `				}` |
|      55 | 6438 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6439 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6440 | `						return SXERR_ABORT;` |
|       - | 6441 | `					}` |
|     ! 0 | 6442 | `					goto done;` |
|       - | 6443 | `				}` |
|       - | 6444 | `			}` |
|      28 | 6445 | `		}else{` |
|     ! 0 | 6446 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6447 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6448 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6449 | `					return SXERR_ABORT;` |
|       - | 6450 | `				}` |
|     ! 0 | 6451 | `				goto done;` |
|       - | 6452 | `			}` |
|       - | 6453 | `		}` |
|       1 | 6454 | `	}` |
|       - | 6455 | `	/* Install the trait */` |
|      51 | 6456 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      51 | 6457 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6458 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6459 | `		return SXERR_ABORT;` |
|       - | 6460 | `	}` |
|      25 | 6461 | `done:` |
|       - | 6462 | `	/* Point beyond the trait body */` |
|      51 | 6463 | `	pGen->pIn = &pEnd[1];` |
|      51 | 6464 | `	pGen->pEnd = pTmp;` |
|      51 | 6465 | `	return PH7_OK;` |
|      26 | 6466 |  |
|       - | 6467 | `/*` |
|       - | 6468 | ` * Compile a user-defined class.` |
|       - | 6469 | ` *  According to the PHP language reference manual` |
|       - | 6470 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6471 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6472 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6473 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6474 | ` *   and functions (called "methods").` |
|       - | 6475 | ` */` |
|   33676 | 6476 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6477 |  |
|       - | 6478 | `	sxi32 rc;` |
|   33678 | 6479 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   33678 | 6480 | `	return rc;` |
|       2 | 6481 |  |
|       - | 6482 | `/*` |
|       - | 6483 | ` * Exception handling.` |
|       - | 6484 | ` *  According to the PHP language reference manual` |
|       - | 6485 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6486 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6487 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6488 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6489 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6490 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6491 | ` *    (or re-thrown) within a catch block.` |
|       - | 6492 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6493 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6494 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6495 | ` *    been defined with set_exception_handler().` |
|       - | 6496 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6497 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6498 | ` */` |
|       - | 6499 | `/*` |
|       - | 6500 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6501 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6502 | ` * indicates failure.` |
|       - | 6503 | ` */` |
|    8360 | 6504 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6505 |  |
|    8362 | 6506 | `	sxi32 rc = SXRET_OK;` |
|    8362 | 6507 | `	if( pRoot->pOp ){` |
|    8358 | 6508 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4181 | 6509 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6510 | `			/* Unexpected expression */` |
|     ! 0 | 6511 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6512 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6513 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6514 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6515 | `			}` |
|       2 | 6516 | `		}` |
|    4182 | 6517 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6518 | `		/* Unexpected expression */` |
|     ! 0 | 6519 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6520 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6521 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6522 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6523 | `		}` |
|     ! 0 | 6524 | `	}` |
|    8362 | 6525 | `	return rc;` |
|       2 | 6526 |  |
|       - | 6527 | `/*` |
|       - | 6528 | ` * Compile a 'throw' statement.` |
|       - | 6529 | ` * throw: This is how you trigger an exception.` |
|       - | 6530 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6531 | ` */` |
|    8360 | 6532 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6533 |  |
|    8362 | 6534 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6535 | `	GenBlock *pBlock;` |
|       - | 6536 | `	sxu32 nIdx;` |
|       - | 6537 | `	sxi32 rc;` |
|    8362 | 6538 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6539 | `	/* Compile the expression */` |
|    8362 | 6540 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8362 | 6541 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6542 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6543 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6544 | `			return SXERR_ABORT;` |
|       - | 6545 | `		}` |
|     ! 0 | 6546 | `		return SXRET_OK;` |
|       - | 6547 | `	}` |
|    8362 | 6548 | `	pBlock = pGen->pCurrent;` |
|       - | 6549 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   38930 | 6550 | `	while(pBlock->pParent){` |
|   38926 | 6551 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8358 | 6552 | `			break;` |
|       - | 6553 | `		}` |
|       - | 6554 | `		/* Point to the parent block */` |
|   30570 | 6555 | `		pBlock = pBlock->pParent;` |
|       2 | 6556 | `	}` |
|       - | 6557 | `	/* Emit the throw instruction */` |
|    8362 | 6558 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6559 | `	/* Emit the jump */` |
|    8362 | 6560 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8362 | 6561 | `	return SXRET_OK;` |
|    4182 | 6562 |  |
|       - | 6563 | `/*` |
|       - | 6564 | ` * Compile a 'catch' block.` |
|       - | 6565 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6566 | ` * an object containing the exception information.` |
|       - | 6567 | ` */` |
|      56 | 6568 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6569 |  |
|      58 | 6570 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6571 | `	ph7_exception_block sCatch;` |
|       - | 6572 | `	SySet *pInstrContainer;` |
|       - | 6573 | `	GenBlock *pCatch;` |
|       - | 6574 | `	SyToken *pToken;` |
|       - | 6575 | `	SyString *pName;` |
|       - | 6576 | `	char *zDup;` |
|       - | 6577 | `	sxi32 rc;` |
|      58 | 6578 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6579 | `	/* Zero the structure */` |
|      58 | 6580 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6581 | `	/* Initialize fields */` |
|      58 | 6582 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      84 | 6583 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      58 | 6584 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6585 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6586 | `			pToken = pGen->pIn;` |
|     ! 0 | 6587 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6588 | `				pToken--;` |
|     ! 0 | 6589 | `			}` |
|     ! 0 | 6590 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6591 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6592 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6593 | `				return SXERR_ABORT;` |
|       - | 6594 | `			}` |
|     ! 0 | 6595 | `			return SXERR_INVALID;` |
|       - | 6596 | `	}` |
|       - | 6597 | `	/* Extract the exception class */` |
|      58 | 6598 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6599 | `	/* Duplicate class name */` |
|      58 | 6600 | `	pName = &pGen->pIn->sData;` |
|      58 | 6601 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6602 | `	if( zDup == 0 ){` |
|     ! 0 | 6603 | `		goto Mem;` |
|       - | 6604 | `	}` |
|      58 | 6605 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      58 | 6606 | `	pGen->pIn++;` |
|      84 | 6607 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      58 | 6608 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6609 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6610 | `			pToken = pGen->pIn;` |
|     ! 0 | 6611 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6612 | `				pToken--;` |
|     ! 0 | 6613 | `			}` |
|     ! 0 | 6614 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6615 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6616 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6617 | `				return SXERR_ABORT;` |
|       - | 6618 | `			}` |
|     ! 0 | 6619 | `			return SXERR_INVALID;` |
|       - | 6620 | `	}` |
|      58 | 6621 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6622 | `	/* Duplicate instance name */` |
|      58 | 6623 | `	pName = &pGen->pIn->sData;` |
|      58 | 6624 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6625 | `	if( zDup == 0 ){` |
|     ! 0 | 6626 | `		goto Mem;` |
|       - | 6627 | `	}` |
|      58 | 6628 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      58 | 6629 | `	pGen->pIn++;` |
|      58 | 6630 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6631 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6632 | `		pToken = pGen->pIn;` |
|     ! 0 | 6633 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6634 | `			pToken--;` |
|     ! 0 | 6635 | `		}` |
|     ! 0 | 6636 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6637 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6638 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6639 | `			return SXERR_ABORT;` |
|       - | 6640 | `		}` |
|     ! 0 | 6641 | `		return SXERR_INVALID;` |
|       - | 6642 | `	}` |
|       - | 6643 | `	/* Compile the block */` |
|      58 | 6644 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6645 | `	/* Create the catch block */` |
|      58 | 6646 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      58 | 6647 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6648 | `		return SXERR_ABORT;` |
|       - | 6649 | `	}` |
|       - | 6650 | `	/* Swap bytecode container */` |
|      58 | 6651 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      58 | 6652 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6653 | `	/* Compile the block */` |
|      58 | 6654 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6655 | `	/* Fix forward jumps now the destination is resolved  */` |
|      58 | 6656 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6657 | `	/* Emit the DONE instruction */` |
|      58 | 6658 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6659 | `	/* Leave the block */` |
|      58 | 6660 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6661 | `	/* Restore the default container */` |
|      58 | 6662 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6663 | `	/* Install the catch block */` |
|      58 | 6664 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      58 | 6665 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6666 | `		goto Mem;` |
|       - | 6667 | `	}` |
|      58 | 6668 | `	return SXRET_OK;` |
|     ! 0 | 6669 | `Mem:` |
|     ! 0 | 6670 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6671 | `	return SXERR_ABORT;` |
|      30 | 6672 |  |
|       - | 6673 | `/*` |
|       - | 6674 | ` * Compile a 'try' block.` |
|       - | 6675 | ` * A function using an exception should be in a "try" block.` |
|       - | 6676 | ` * If the exception does not trigger, the code will continue` |
|       - | 6677 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6678 | ` * is "thrown".` |
|       - | 6679 | ` */` |
|      68 | 6680 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6681 |  |
|       - | 6682 | `	ph7_exception *pException;` |
|      70 | 6683 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6684 | `	GenBlock *pTry;` |
|       - | 6685 | `	sxu32 nJmpIdx;` |
|       - | 6686 | `	sxi32 rc;` |
|       - | 6687 | `	/* Create the exception container */` |
|      70 | 6688 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      70 | 6689 | `	if( pException == 0 ){` |
|     ! 0 | 6690 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6691 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6692 | `		return SXERR_ABORT;` |
|       - | 6693 | `	}` |
|       - | 6694 | `	/* Zero the structure */` |
|      70 | 6695 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6696 | `	/* Initialize fields */` |
|      70 | 6697 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      70 | 6698 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 6699 | `	pException->iHasFinally = 0;` |
|      70 | 6700 | `	pException->iFinallyDone = 0;` |
|      70 | 6701 | `	pException->pVm = pGen->pVm;` |
|       - | 6702 | `	/* Create the try block */` |
|      70 | 6703 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      70 | 6704 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6705 | `		return SXERR_ABORT;` |
|       - | 6706 | `	}` |
|       - | 6707 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      70 | 6708 | `	pTry->pUserData = pException;` |
|       - | 6709 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      70 | 6710 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6711 | `	/* Fix the jump later when the destination is resolved */` |
|      70 | 6712 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      70 | 6713 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6714 | `	/* Compile the block */` |
|      70 | 6715 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      70 | 6716 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6717 | `		return SXERR_ABORT;` |
|       - | 6718 | `	}` |
|       - | 6719 | `	/* Fix forward jumps now the destination is resolved */` |
|      70 | 6720 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6721 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      70 | 6722 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6723 | `	/* Leave the block */` |
|      70 | 6724 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6725 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      70 | 6726 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      66 | 6727 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6728 | `		/* Compile one or more catch blocks */` |
|      56 | 6729 | `		for(;;){` |
|     112 | 6730 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 | 6731 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      30 | 6732 | `					break;` |
|       - | 6733 | `			}` |
|      58 | 6734 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      58 | 6735 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6736 | `				return SXERR_ABORT;` |
|       - | 6737 | `			}` |
|       2 | 6738 | `		}` |
|      28 | 6739 | `	}` |
|       - | 6740 | `	/* Compile optional finally block */` |
|      70 | 6741 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      36 | 6742 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6743 | `		SySet *pInstrContainer;` |
|       - | 6744 | `		GenBlock *pFinBlock;` |
|      27 | 6745 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6746 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      27 | 6747 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      27 | 6748 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6749 | `			return SXERR_ABORT;` |
|       - | 6750 | `		}` |
|       - | 6751 | `		/* Swap bytecode container */` |
|      27 | 6752 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      27 | 6753 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6754 | `		/* Compile the finally body */` |
|      27 | 6755 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      27 | 6756 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6757 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6758 | `			return SXERR_ABORT;` |
|       - | 6759 | `		}` |
|       - | 6760 | `		/* Fix forward jumps now the destination is resolved */` |
|      27 | 6761 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6762 | `		/* Emit DONE to terminate the finally block */` |
|      27 | 6763 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6764 | `		/* Leave the block */` |
|      27 | 6765 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6766 | `		/* Restore the default container */` |
|      27 | 6767 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      27 | 6768 | `		pException->iHasFinally = 1;` |
|      13 | 6769 | `	}` |
|       - | 6770 | `	/* Must have at least one catch or finally */` |
|      70 | 6771 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6772 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6773 | `			"Cannot use try without catch or finally");` |
|       3 | 6774 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6775 | `			return SXERR_ABORT;` |
|       - | 6776 | `		}` |
|       1 | 6777 | `	}` |
|      70 | 6778 | `	return SXRET_OK;` |
|      36 | 6779 |  |
|       - | 6780 | `/*` |
|       - | 6781 | ` * Compile a switch block.` |
|       - | 6782 | ` *  (See block-comment below for more information)` |
|       - | 6783 | ` */` |
|      84 | 6784 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6785 |  |
|      86 | 6786 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6787 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6788 | `		/* Unexpected token */` |
|     ! 0 | 6789 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6790 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6791 | `			return SXERR_ABORT;` |
|       - | 6792 | `		}` |
|     ! 0 | 6793 | `		pGen->pIn++;` |
|     ! 0 | 6794 | `	}` |
|      86 | 6795 | `	pGen->pIn++;` |
|       - | 6796 | `	/* First instruction to execute in this block. */` |
|      86 | 6797 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6798 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6799 | `	 * or the '}' token */` |
|     151 | 6800 | `	for(;;){` |
|     304 | 6801 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6802 | `			/* No more input to process */` |
|     ! 0 | 6803 | `			break;` |
|       - | 6804 | `		}` |
|     304 | 6805 | `		rc = SXRET_OK;` |
|     304 | 6806 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6807 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6808 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6809 | `					/* Unexpected token */` |
|     ! 0 | 6810 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6811 | `						&pGen->pIn->sData);` |
|     ! 0 | 6812 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6813 | `						return SXERR_ABORT;` |
|       - | 6814 | `					}` |
|       - | 6815 | `					/* FALL THROUGH */` |
|     ! 0 | 6816 | `				}` |
|      20 | 6817 | `				rc = SXERR_EOF;` |
|      20 | 6818 | `				break;` |
|       - | 6819 | `			}` |
|      23 | 6820 | `		}else{` |
|       - | 6821 | `			sxi32 nKwrd;` |
|       - | 6822 | `			/* Extract the keyword */` |
|     244 | 6823 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6824 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6825 | `				break;` |
|       - | 6826 | `			}` |
|     180 | 6827 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6828 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6829 | `					/* Unexpected token */` |
|     ! 0 | 6830 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6831 | `						&pGen->pIn->sData);` |
|     ! 0 | 6832 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6833 | `						return SXERR_ABORT;` |
|       - | 6834 | `					}` |
|       - | 6835 | `					/* FALL THROUGH */` |
|     ! 0 | 6836 | `				}` |
|       - | 6837 | `				/* Block compiled */` |
|       3 | 6838 | `				break;` |
|       - | 6839 | `			}` |
|       - | 6840 | `		}` |
|       - | 6841 | `		/* Compile block */` |
|     220 | 6842 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6843 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6844 | `			return SXERR_ABORT;` |
|       - | 6845 | `		}` |
|       2 | 6846 | `	}` |
|      86 | 6847 | `	return rc;` |
|      44 | 6848 |  |
|       - | 6849 | `/*` |
|       - | 6850 | ` * Compile a case eXpression.` |
|       - | 6851 | ` *  (See block-comment below for more information)` |
|       - | 6852 | ` */` |
|      70 | 6853 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6854 |  |
|       - | 6855 | `	SySet *pInstrContainer;` |
|       - | 6856 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6857 | `	sxi32 iNest = 0;` |
|       - | 6858 | `	sxi32 rc;` |
|       - | 6859 | `	/* Delimit the expression */` |
|      72 | 6860 | `	pEnd = pGen->pIn;` |
|     150 | 6861 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6862 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6863 | `			/* Increment nesting level */` |
|       3 | 6864 | `			iNest++;` |
|     149 | 6865 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6866 | `			/* Decrement nesting level */` |
|       3 | 6867 | `			iNest--;` |
|     147 | 6868 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6869 | `			break;` |
|       - | 6870 | `		}` |
|      80 | 6871 | `		pEnd++;` |
|       2 | 6872 | `	}` |
|      72 | 6873 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6874 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6875 | `		if( rc == SXERR_ABORT ){` |
|       - | 6876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6877 | `			return SXERR_ABORT;` |
|       - | 6878 | `		}` |
|     ! 0 | 6879 | `	}` |
|       - | 6880 | `	/* Swap token stream */` |
|      72 | 6881 | `	pTmp = pGen->pEnd;` |
|      72 | 6882 | `	pGen->pEnd = pEnd;` |
|      72 | 6883 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6884 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6885 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6886 | `	/* Emit the done instruction */` |
|      72 | 6887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6888 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6889 | `	/* Update token stream */` |
|      72 | 6890 | `	pGen->pIn  = pEnd;` |
|      72 | 6891 | `	pGen->pEnd = pTmp;` |
|      72 | 6892 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6893 | `		return SXERR_ABORT;` |
|       - | 6894 | `	}` |
|      72 | 6895 | `	return SXRET_OK;` |
|      37 | 6896 |  |
|       - | 6897 | `/*` |
|       - | 6898 | ` * Compile the smart switch statement.` |
|       - | 6899 | ` * According to the PHP language reference manual` |
|       - | 6900 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6901 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6902 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6903 | ` *  This is exactly what the switch statement is for.` |
|       - | 6904 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6905 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6906 | ` *  of the outer loop, use continue 2.` |
|       - | 6907 | ` *  Note that switch/case does loose comparision.` |
|       - | 6908 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6909 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6910 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6911 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6912 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6913 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6914 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 6915 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 6916 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 6917 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 6918 | ` *  list for the next case.` |
|       - | 6919 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 6920 | ` *  or floating-point numbers and strings.` |
|       - | 6921 | ` */` |
|      20 | 6922 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 6923 |  |
|       - | 6924 | `	GenBlock *pSwitchBlock;` |
|       - | 6925 | `	SyToken *pTmp,*pEnd;` |
|       - | 6926 | `	ph7_switch *pSwitch;` |
|       - | 6927 | `	sxu32 nToken;` |
|       - | 6928 | `	sxu32 nLine;` |
|       - | 6929 | `	sxi32 rc;` |
|      22 | 6930 | `	nLine = pGen->pIn->nLine;` |
|       - | 6931 | `	/* Jump the 'switch' keyword */` |
|      22 | 6932 | `	pGen->pIn++;` |
|      22 | 6933 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6934 | `		/* Syntax error */` |
|     ! 0 | 6935 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 6936 | `		if( rc == SXERR_ABORT ){` |
|       - | 6937 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6938 | `			return SXERR_ABORT;` |
|       - | 6939 | `		}` |
|     ! 0 | 6940 | `		goto Synchronize;` |
|       - | 6941 | `	}` |
|       - | 6942 | `	/* Jump the left parenthesis '(' */` |
|      22 | 6943 | `	pGen->pIn++;` |
|      22 | 6944 | `	pEnd = 0; /* cc warning */` |
|       - | 6945 | `	/* Create the loop block */` |
|      32 | 6946 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 6947 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 6948 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6949 | `		return SXERR_ABORT;` |
|       - | 6950 | `	}` |
|       - | 6951 | `	/* Delimit the condition */` |
|      22 | 6952 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 6953 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 6954 | `		/* Empty expression */` |
|     ! 0 | 6955 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 6956 | `		if( rc == SXERR_ABORT ){` |
|       - | 6957 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6958 | `			return SXERR_ABORT;` |
|       - | 6959 | `		}` |
|     ! 0 | 6960 | `	}` |
|       - | 6961 | `	/* Swap token streams */` |
|      22 | 6962 | `	pTmp = pGen->pEnd;` |
|      22 | 6963 | `	pGen->pEnd = pEnd;` |
|       - | 6964 | `	/* Compile the expression */` |
|      22 | 6965 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 6966 | `	if( rc == SXERR_ABORT ){` |
|       - | 6967 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 6968 | `		return SXERR_ABORT;` |
|       - | 6969 | `	}` |
|       - | 6970 | `	/* Update token stream */` |
|      22 | 6971 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 6972 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6973 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6974 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6975 | `			return SXERR_ABORT;` |
|       - | 6976 | `		}` |
|     ! 0 | 6977 | `		pGen->pIn++;` |
|     ! 0 | 6978 | `	}` |
|      22 | 6979 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 6980 | `	pGen->pEnd = pTmp;` |
|      22 | 6981 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 6982 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 6983 | `			pTmp = pGen->pIn;` |
|     ! 0 | 6984 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 6985 | `				pTmp--;` |
|     ! 0 | 6986 | `			}` |
|       - | 6987 | `			/* Unexpected token */` |
|     ! 0 | 6988 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 6989 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6990 | `				return SXERR_ABORT;` |
|       - | 6991 | `			}` |
|     ! 0 | 6992 | `			goto Synchronize;` |
|       - | 6993 | `	}` |
|       - | 6994 | `	/* Set the delimiter token */` |
|      22 | 6995 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 6996 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 6997 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 6998 | `	}else{` |
|      20 | 6999 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7000 | `	}` |
|      22 | 7001 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7002 | `	/* Create the switch blocks container */` |
|      22 | 7003 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 7004 | `	if( pSwitch == 0 ){` |
|       - | 7005 | `		/* Abort compilation */` |
|     ! 0 | 7006 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7007 | `		return SXERR_ABORT;` |
|       - | 7008 | `	}` |
|       - | 7009 | `	/* Zero the structure */` |
|      22 | 7010 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7011 | `	/* Initialize fields */` |
|      22 | 7012 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7013 | `	/* Emit the switch instruction */` |
|      22 | 7014 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7015 | `	/* Compile case blocks */` |
|      76 | 7016 | `	for(;;){` |
|       - | 7017 | `		sxu32 nKwrd;` |
|      88 | 7018 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7019 | `			/* No more input to process */` |
|     ! 0 | 7020 | `			break;` |
|       - | 7021 | `		}` |
|      88 | 7022 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7023 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7024 | `				/* Unexpected token */` |
|     ! 0 | 7025 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7026 | `					&pGen->pIn->sData);` |
|     ! 0 | 7027 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7028 | `					return SXERR_ABORT;` |
|       - | 7029 | `				}` |
|       - | 7030 | `				/* FALL THROUGH */` |
|     ! 0 | 7031 | `			}` |
|       - | 7032 | `			/* Block compiled */` |
|     ! 0 | 7033 | `			break;` |
|       - | 7034 | `		}` |
|       - | 7035 | `		/* Extract the keyword */` |
|      88 | 7036 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 7037 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7038 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7039 | `				/* Unexpected token */` |
|     ! 0 | 7040 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7041 | `					&pGen->pIn->sData);` |
|     ! 0 | 7042 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7043 | `					return SXERR_ABORT;` |
|       - | 7044 | `				}` |
|       - | 7045 | `				/* FALL THROUGH */` |
|     ! 0 | 7046 | `			}` |
|       - | 7047 | `			/* Block compiled */` |
|       3 | 7048 | `			break;` |
|       - | 7049 | `		}` |
|      86 | 7050 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7051 | `			/*` |
|       - | 7052 | `			 * Accroding to the PHP language reference manual` |
|       - | 7053 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7054 | `			 *  that wasn't matched by the other cases.` |
|       - | 7055 | `			 */` |
|      16 | 7056 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7057 | `				/* Default case already compiled */` |
|     ! 0 | 7058 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7059 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7060 | `					return SXERR_ABORT;` |
|       - | 7061 | `				}` |
|     ! 0 | 7062 | `			}` |
|      16 | 7063 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7064 | `			/* Compile the default block */` |
|      16 | 7065 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 7066 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7067 | `				return SXERR_ABORT;` |
|      16 | 7068 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 7069 | `				break;` |
|       1 | 7070 | `			}` |
|      73 | 7071 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7072 | `			ph7_case_expr sCase;` |
|       - | 7073 | `			/* Standard case block */` |
|      72 | 7074 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7075 | `			/* initialize the structure */` |
|      72 | 7076 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7077 | `			/* Compile the case expression */` |
|      72 | 7078 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 7079 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7080 | `				return SXERR_ABORT;` |
|       - | 7081 | `			}` |
|       - | 7082 | `			/* Compile the case block */` |
|      72 | 7083 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7084 | `			/* Insert in the switch container */` |
|      72 | 7085 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 7086 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7087 | `				return SXERR_ABORT;` |
|      72 | 7088 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 7089 | `				break;` |
|       - | 7090 | `			}` |
|      34 | 7091 | `		}else{` |
|       - | 7092 | `			/* Unexpected token */` |
|     ! 0 | 7093 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7094 | `				&pGen->pIn->sData);` |
|     ! 0 | 7095 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7096 | `				return SXERR_ABORT;` |
|       - | 7097 | `			}` |
|     ! 0 | 7098 | `			break;` |
|       - | 7099 | `		}` |
|       2 | 7100 | `	}` |
|       - | 7101 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 7102 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 7103 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7104 | `	/* Release the loop block */` |
|      22 | 7105 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 7106 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7107 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 7108 | `		pGen->pIn++;` |
|      10 | 7109 | `	}` |
|       - | 7110 | `	/* Statement successfully compiled */` |
|      22 | 7111 | `	return SXRET_OK;` |
|     ! 0 | 7112 | `Synchronize:` |
|       - | 7113 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7114 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7115 | `		pGen->pIn++;` |
|     ! 0 | 7116 | `	}` |
|     ! 0 | 7117 | `	return SXRET_OK;` |
|      12 | 7118 |  |
|       - | 7119 | `/*` |
|       - | 7120 | ` * Generate bytecode for a given expression tree.` |
|       - | 7121 | ` * If something goes wrong while generating bytecode` |
|       - | 7122 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7123 | ` * this function takes care of generating the appropriate` |
|       - | 7124 | ` * error message.` |
|       - | 7125 | ` */` |
| 2485734 | 7126 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7127 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7128 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7129 | `	sxi32 iFlags /* Control flags */` |
|       - | 7130 | `	)` |
|       2 | 7131 |  |
|       - | 7132 | `	VmInstr *pInstr;` |
|       - | 7133 | `	sxu32 nJmpIdx;` |
| 2485736 | 7134 | `	sxi32 iP1 = 0;` |
| 2485736 | 7135 | `	sxu32 iP2 = 0;` |
| 2485736 | 7136 | `	void *p3  = 0;` |
|       - | 7137 | `	sxi32 iVmOp;` |
|       - | 7138 | `	sxi32 rc;` |
| 2485736 | 7139 | `	if( pNode->xCode ){` |
|       - | 7140 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7141 | `		/* Compile node */` |
| 1540240 | 7142 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1540240 | 7143 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1540240 | 7144 | `		RE_SWAP_DELIMITER(pGen);` |
| 1540240 | 7145 | `		return rc;` |
|       - | 7146 | `	}` |
|  945498 | 7147 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7148 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7149 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7150 | `		return SXERR_ABORT;` |
|       - | 7151 | `	}` |
|  945498 | 7152 | `	iVmOp = pNode->pOp->iVmOp;` |
|  945498 | 7153 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7154 | `		sxu32 nJz,nJmp;` |
|       - | 7155 | `		/* Ternary operator require special handling */` |
|       - | 7156 | `		/* Phase#1: Compile the condition */` |
|    1800 | 7157 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1800 | 7158 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7159 | `			return rc;` |
|       - | 7160 | `		}` |
|    1800 | 7161 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1800 | 7162 | `		if( pNode->pLeft ){` |
|       - | 7163 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7164 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1732 | 7165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7166 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1732 | 7167 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1732 | 7168 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7169 | `				return rc;` |
|       - | 7170 | `			}` |
|     867 | 7171 | `		}else{` |
|       - | 7172 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7173 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7174 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7175 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7176 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7177 | `		}` |
|       - | 7178 | `		/* Phase#4: Emit the unconditional jump */` |
|    1800 | 7179 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7180 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1800 | 7181 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1800 | 7182 | `		if( pInstr ){` |
|    1800 | 7183 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     899 | 7184 | `		}` |
|    1800 | 7185 | `		if( !pNode->pLeft ){` |
|       - | 7186 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7187 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7188 | `		}` |
|       - | 7189 | `		/* Phase#6: Compile the 'else' expression */` |
|    1800 | 7190 | `		if( pNode->pRight ){` |
|    1800 | 7191 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1800 | 7192 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7193 | `				return rc;` |
|       - | 7194 | `			}` |
|     899 | 7195 | `		}` |
|    1800 | 7196 | `		if( nJmp > 0 ){` |
|       - | 7197 | `			/* Phase#7: Fix the unconditional jump */` |
|    1800 | 7198 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1800 | 7199 | `			if( pInstr ){` |
|    1800 | 7200 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     899 | 7201 | `			}` |
|     899 | 7202 | `		}` |
|       - | 7203 | `		/* All done */` |
|    1800 | 7204 | `		return SXRET_OK;` |
|       - | 7205 | `	}` |
|       - | 7206 | `	/* Generate code for the left tree */` |
|  943700 | 7207 | `	if( pNode->pLeft ){` |
|  943682 | 7208 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7209 | `			ph7_expr_node **apNode;` |
|       - | 7210 | `			sxi32 n;` |
|       - | 7211 | `			/* Recurse and generate bytecodes for function arguments */` |
|  316988 | 7212 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7213 | `			/* Read-only load */` |
|  316988 | 7214 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  633346 | 7215 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  316360 | 7216 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  316360 | 7217 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7218 | `					return rc;` |
|       - | 7219 | `				}` |
|  158181 | 7220 | `			}` |
|       - | 7221 | `			/* Total number of given arguments */` |
|  316988 | 7222 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 7223 | `			/* Remove stale flags now */` |
|  316988 | 7224 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  158493 | 7225 | `		}` |
|  943682 | 7226 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  943682 | 7227 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7228 | `			return rc;` |
|       - | 7229 | `		}` |
|  943682 | 7230 | `		if( iVmOp == PH7_OP_CALL ){` |
|  316988 | 7231 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  316988 | 7232 | `			if( pInstr ){` |
|  316988 | 7233 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  316508 | 7234 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7235 | `					sxu32 nQual;` |
|       - | 7236 | `					/* Prevent constant expansion */` |
|  316508 | 7237 | `					pInstr->iP1 = 0;` |
|       - | 7238 | `					/* Namespace-qualify the function name for CALL */` |
|  316508 | 7239 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  316508 | 7240 | `					pInstr->iP2 = (sxi32)nQual;` |
|  316508 | 7241 | `					if( nQual != nOrig ){` |
|       - | 7242 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 7243 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 7244 | `						 * and try the short name in hHostFunction. */` |
|      49 | 7245 | `						p3 = (void *)1;` |
|      26 | 7246 | `					}` |
|  158735 | 7247 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7248 | `					/* Method call,flag that */` |
|     464 | 7249 | `					pInstr->iP2 = 1;` |
|     231 | 7250 | `				}` |
|  158495 | 7251 | `			}` |
|  785189 | 7252 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7253 | `			ph7_expr_node **apNode;` |
|       - | 7254 | `			sxi32 n;` |
|       - | 7255 | `			/* Recurse and generate bytecodes for array index */` |
|   71136 | 7256 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  128338 | 7257 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   57204 | 7258 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   57204 | 7259 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7260 | `					return rc;` |
|       - | 7261 | `				}` |
|   28603 | 7262 | `			}` |
|   71136 | 7263 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   57204 | 7264 | `				iP1 = 1; /* Node have an index associated with it */` |
|   28601 | 7265 | `			}` |
|   71136 | 7266 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7267 | `				/* Create an empty entry when the desired index is not found */` |
|   28114 | 7268 | `				iP2 = 1;` |
|   14058 | 7269 | `			}` |
|  591129 | 7270 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7271 | `			/* POP the left node */` |
|      32 | 7272 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7273 | `		}` |
|  471840 | 7274 | `	}` |
|  943700 | 7275 | `	rc = SXRET_OK;` |
|  943700 | 7276 | `	nJmpIdx = 0;` |
|       - | 7277 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7278 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7279 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  943700 | 7280 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     126 | 7281 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     126 | 7282 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     126 | 7283 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     126 | 7284 | `			int isSpecial = 0;` |
|     126 | 7285 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      86 | 7286 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      86 | 7287 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      90 | 7288 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      78 | 7289 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      39 | 7290 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      40 | 7291 | `					isSpecial = 1;` |
|      19 | 7292 | `				}` |
|      52 | 7293 | `			}` |
|     146 | 7294 | `			pInstr->iP1 = 0;` |
|     146 | 7295 | `			if( !isSpecial ){` |
|      68 | 7296 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      33 | 7297 | `			}` |
|      52 | 7298 | `		}` |
|      86 | 7299 | `	}` |
|       - | 7300 | `	/* Generate code for the right tree */` |
|  943684 | 7301 | `	if( pNode->pRight ){` |
|  492708 | 7302 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7303 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8746 | 7304 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  488336 | 7305 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7306 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2922 | 7307 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  482504 | 7308 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  215352 | 7309 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  107675 | 7310 | `		}` |
|  492708 | 7311 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  492708 | 7312 | `		if( iVmOp == PH7_OP_STORE ){` |
|  212434 | 7313 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  212434 | 7314 | `			if( pInstr ){` |
|  212434 | 7315 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7316 | `					/* Hide the STORE instruction */` |
|      26 | 7317 | `					iVmOp = 0;` |
|  212422 | 7318 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7319 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   47280 | 7320 | `					iP2 = 1;` |
|   23641 | 7321 | `				}else{` |
|  165132 | 7322 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7323 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   28076 | 7324 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   28076 | 7325 | `						iP1 = pInstr->iP1;` |
|   14039 | 7326 | `					}else{` |
|  137058 | 7327 | `						p3 = pInstr->p3;` |
|       - | 7328 | `					}` |
|       - | 7329 | `					/* POP the last dynamic load instruction */` |
|  165132 | 7330 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7331 | `				}` |
|  106218 | 7332 | `			}` |
|  386492 | 7333 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7334 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7335 | `			if( pInstr ){` |
|      46 | 7336 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7337 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7338 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7339 | `					 */` |
|      15 | 7340 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7341 | `					iP1 = pInstr->iP1;` |
|      15 | 7342 | `					iP2 = pInstr->iP2;` |
|      15 | 7343 | `					p3  = pInstr->p3;` |
|       8 | 7344 | `				}else{` |
|      32 | 7345 | `					p3 = pInstr->p3;` |
|       - | 7346 | `				}` |
|      22 | 7347 | `			}` |
|      22 | 7348 | `		}` |
|  246353 | 7349 | `	}` |
|  943684 | 7350 | `	if( iVmOp > 0 ){` |
|  943630 | 7351 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11338 | 7352 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7353 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8336 | 7354 | `				iP1 = 1;` |
|    4169 | 7355 | `			}` |
|  937962 | 7356 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7357 | `			/* Namespace-qualify the class name for NEW */ {` |
|   14244 | 7358 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   14244 | 7359 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   14232 | 7360 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7115 | 7361 | `				}` |
|   14244 | 7362 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7363 | `					/* Prevent constant expansion for class name */` |
|   14242 | 7364 | `					pPeek->iP1 = 0;` |
|   14242 | 7365 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    7120 | 7366 | `				}` |
|       - | 7367 | `			}` |
|   14244 | 7368 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   14244 | 7369 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7370 | `				VmInstr *pPrev;` |
|   14232 | 7371 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   14232 | 7372 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7373 | `					/* Pop the call instruction */` |
|   14232 | 7374 | `					iP1 = pInstr->iP1;` |
|   14232 | 7375 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7115 | 7376 | `				}` |
|    7117 | 7377 | `			}` |
|  925173 | 7378 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7379 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7380 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7381 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7382 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7383 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7384 | `				int isSpecialIs = 0;` |
|      50 | 7385 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7386 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7387 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7388 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7389 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7390 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7391 | `						isSpecialIs = 1;` |
|       5 | 7392 | `					}` |
|      23 | 7393 | `				}` |
|      52 | 7394 | `				pInstr->iP1 = 0;` |
|      52 | 7395 | `				if( !isSpecialIs ){` |
|      38 | 7396 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 7397 | `				}` |
|      25 | 7398 | `			}` |
|  918031 | 7399 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7400 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7401 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7402 | `			 * should not trigger constant lookup. */` |
|  106322 | 7403 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  106322 | 7404 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  106306 | 7405 | `				pInstr->iP1 = 0;` |
|   53152 | 7406 | `			}` |
|  106322 | 7407 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7408 | `				/* Static member access,remember that */` |
|     110 | 7409 | `				iP1 = 1;` |
|     110 | 7410 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     110 | 7411 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7412 | `					p3 = pInstr->p3;` |
|      10 | 7413 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7414 | `				}` |
|      54 | 7415 | `			}` |
|   53160 | 7416 | `		}` |
|       - | 7417 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  943628 | 7418 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  943628 | 7419 | `		if( nJmpIdx > 0 ){` |
|       - | 7420 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   11666 | 7421 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11666 | 7422 | `			if( pInstr ){` |
|   11666 | 7423 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5832 | 7424 | `			}` |
|    5832 | 7425 | `		}` |
|  471813 | 7426 | `	}` |
|  943682 | 7427 | `	return rc;` |
| 1242860 | 7428 |  |
|       - | 7429 | `/*` |
|       - | 7430 | ` * Compile a PHP expression.` |
|       - | 7431 | ` * According to the PHP language reference manual:` |
|       - | 7432 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7433 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7434 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7435 | ` *  is "anything that has a value".` |
|       - | 7436 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7437 | ` * function takes care of generating the appropriate error` |
|       - | 7438 | ` * message.` |
|       - | 7439 | ` */` |
|  670610 | 7440 | `static sxi32 PH7_CompileExpr(` |
|       - | 7441 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7442 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7443 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7444 | `	)` |
|       2 | 7445 |  |
|       - | 7446 | `	ph7_expr_node *pRoot;` |
|       - | 7447 | `	SySet sExprNode;` |
|       - | 7448 | `	SyToken *pEnd;` |
|       - | 7449 | `	sxi32 nExpr;` |
|       - | 7450 | `	sxi32 iNest;` |
|       - | 7451 | `	sxi32 rc;` |
|       - | 7452 | `	/* Initialize worker variables */` |
|  670612 | 7453 | `	nExpr = 0;` |
|  670612 | 7454 | `	pRoot = 0;` |
|  670612 | 7455 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  670612 | 7456 | `	SySetAlloc(&sExprNode,0x10);` |
|  670612 | 7457 | `	rc = SXRET_OK;` |
|       - | 7458 | `	/* Delimit the expression */` |
|  670612 | 7459 | `	pEnd = pGen->pIn;` |
|  670612 | 7460 | `	iNest = 0;` |
| 4525440 | 7461 | `	while( pEnd < pGen->pEnd ){` |
| 4292982 | 7462 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7463 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     198 | 7464 | `			iNest++;` |
| 4292884 | 7465 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     206 | 7466 | `			iNest--;` |
| 4292684 | 7467 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  438322 | 7468 | `			if( iNest <= 0 ){` |
|  438154 | 7469 | `				break;` |
|       - | 7470 | `			}` |
|      84 | 7471 | `		}` |
| 3854830 | 7472 | `		pEnd++;` |
|       2 | 7473 | `	}` |
|  670612 | 7474 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11276 | 7475 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11276 | 7476 | `		iNest = 0;` |
|       - | 7477 | `		/* Stop at the first comma */` |
|   22574 | 7478 | `		while( pEnd2 < pEnd ){` |
|   11300 | 7479 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7480 | `				iNest++;` |
|   11298 | 7481 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7482 | `				iNest--;` |
|   11294 | 7483 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7484 | `				if( iNest <= 0 ){` |
|     ! 0 | 7485 | `					break;` |
|       - | 7486 | `				}` |
|       2 | 7487 | `			}` |
|   11300 | 7488 | `			pEnd2++;` |
|       2 | 7489 | `		}` |
|   11276 | 7490 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7491 | `			pEnd = pEnd2;` |
|     ! 0 | 7492 | `		}` |
|    5637 | 7493 | `	}` |
|  670612 | 7494 | `	if( pEnd > pGen->pIn ){` |
|  670602 | 7495 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7496 | `		/* Swap delimiter */` |
|  670602 | 7497 | `		pGen->pEnd = pEnd;` |
|       - | 7498 | `		/* Try to get an expression tree */` |
|  670602 | 7499 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  670602 | 7500 | `		if( rc == SXRET_OK && pRoot ){` |
|  670446 | 7501 | `			rc = SXRET_OK;` |
|  670446 | 7502 | `			if( xTreeValidator ){` |
|       - | 7503 | `				/* Call the upper layer validator callback */` |
|   14390 | 7504 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7194 | 7505 | `			}` |
|  670446 | 7506 | `			if( rc != SXERR_ABORT ){` |
|       - | 7507 | `				/* Generate code for the given tree */` |
|  670446 | 7508 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  335222 | 7509 | `			}` |
|  670446 | 7510 | `			nExpr = 1;` |
|  335222 | 7511 | `		}` |
|       - | 7512 | `		/* Release the whole tree */` |
|  670602 | 7513 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7514 | `		/* Synchronize token stream */` |
|  670602 | 7515 | `		pGen->pEnd = pTmp;` |
|  670602 | 7516 | `		pGen->pIn  = pEnd;` |
|  670602 | 7517 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7518 | `			SySetRelease(&sExprNode);` |
|       3 | 7519 | `			return SXERR_ABORT;` |
|       - | 7520 | `		}` |
|  335299 | 7521 | `	}` |
|  670610 | 7522 | `	SySetRelease(&sExprNode);` |
|  670610 | 7523 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  335307 | 7524 |  |
|       - | 7525 | `/*` |
|       - | 7526 | ` * Return a pointer to the node construct handler associated` |
|       - | 7527 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7528 | ` */` |
|  164414 | 7529 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7530 |  |
|  164416 | 7531 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7532 | `		/* Numeric literal: Either real or integer */` |
|   90798 | 7533 | `		return PH7_CompileNumLiteral;` |
|   73620 | 7534 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7535 | `		/* Double quoted string */` |
|   14242 | 7536 | `		return PH7_CompileString;` |
|   59380 | 7537 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7538 | `		/* Single quoted string */` |
|   59320 | 7539 | `		return PH7_CompileSimpleString;` |
|      62 | 7540 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7541 | `		/* Heredoc */` |
|      28 | 7542 | `		return PH7_CompileHereDoc;` |
|      36 | 7543 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7544 | `		/* Nowdoc */` |
|      29 | 7545 | `		return PH7_CompileNowDoc;` |
|       7 | 7546 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7547 | `		/* Backtick quoted string */` |
|       5 | 7548 | `		return PH7_CompileBacktic;` |
|       - | 7549 | `	}` |
|       3 | 7550 | `	return 0;` |
|   82209 | 7551 |  |
|       - | 7552 | `/*` |
|       - | 7553 | ` * Compile an unset() statement.` |
|       - | 7554 | ` * unset($var, $arr[$key], ...);` |
|       - | 7555 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7556 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7557 | ` * parent array before extracting the element to unset.` |
|       - | 7558 | ` */` |
|    2506 | 7559 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7560 |  |
|    2508 | 7561 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2508 | 7562 | `	sxu32 nIdx = 0;` |
|       - | 7563 | `	SyString sName;` |
|       - | 7564 | `	sxi32 rc;` |
|       - | 7565 | `	/* Jump the 'unset' keyword */` |
|    2508 | 7566 | `	pGen->pIn++;` |
|       - | 7567 | `	/* Save delimiter */` |
|    2508 | 7568 | `	pTmp = pGen->pEnd;` |
|       - | 7569 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2508 | 7570 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2508 | 7571 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7572 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7573 | `		SyToken *pClose;` |
|    2508 | 7574 | `		pGen->pIn++;   /* Skip '(' */` |
|    2508 | 7575 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2508 | 7576 | `		pEnd = pClose; /* Stop at ')' */` |
|    1253 | 7577 | `	}` |
|    2508 | 7578 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7579 | `	/* Resolve the 'unset' builtin name once */` |
|    2508 | 7580 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     344 | 7581 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     344 | 7582 | `		if( pObj == 0 ){` |
|     ! 0 | 7583 | `			return SXERR_ABORT;` |
|       - | 7584 | `		}` |
|     344 | 7585 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     344 | 7586 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     171 | 7587 | `	}` |
|       - | 7588 | `	/* Compile each comma-separated argument */` |
|    8352 | 7589 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5846 | 7590 | `		if( pGen->pIn < pNext ){` |
|    5846 | 7591 | `			pGen->pEnd = pNext;` |
|    5846 | 7592 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7593 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5846 | 7594 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7595 | `				return SXERR_ABORT;` |
|       - | 7596 | `			}` |
|    5846 | 7597 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7598 | `				/* Emit call for this single argument */` |
|    5844 | 7599 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5844 | 7600 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5844 | 7601 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2921 | 7602 | `			}` |
|    2922 | 7603 | `		}` |
|       - | 7604 | `		/* Jump trailing commas */` |
|    9184 | 7605 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3340 | 7606 | `			pNext++;` |
|       2 | 7607 | `		}` |
|    5846 | 7608 | `		pGen->pIn = pNext;` |
|       2 | 7609 | `	}` |
|       - | 7610 | `	/* Skip past the closing ')' if present */` |
|    2508 | 7611 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2508 | 7612 | `		pGen->pIn++;` |
|    1253 | 7613 | `	}` |
|       - | 7614 | `	/* Restore token stream */` |
|    2508 | 7615 | `	pGen->pEnd = pTmp;` |
|    2508 | 7616 | `	return SXRET_OK;` |
|    1255 | 7617 |  |
|       - | 7618 | `/*` |
|       - | 7619 | ` * PHP Language construct table.` |
|       - | 7620 | ` */` |
|       - | 7621 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7622 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7623 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7624 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7625 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7626 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7627 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7628 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7629 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7630 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7631 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7632 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7633 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7634 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7635 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7636 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7637 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7638 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7639 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7640 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7641 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7642 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7643 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7644 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7645 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7646 | `};` |
|       - | 7647 | `/*` |
|       - | 7648 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7649 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7650 | ` */` |
|  401310 | 7651 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7652 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7653 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7654 | `	)` |
|       2 | 7655 |  |
|  401312 | 7656 | `	sxu32 n = 0;` |
| 1642867 | 7657 | `	for(;;){` |
| 3285736 | 7658 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   42230 | 7659 | `			break;` |
|       - | 7660 | `		}` |
| 3243508 | 7661 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  359084 | 7662 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7663 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7664 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7665 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7666 | `					return 0;` |
|       - | 7667 | `				}` |
|     ! 0 | 7668 | `			}` |
|       - | 7669 | `			/* Return a pointer to the handler.` |
|       - | 7670 | `			*/` |
|  359084 | 7671 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7672 | `		}` |
| 2884426 | 7673 | `		n++;` |
|       2 | 7674 | `	}` |
|   42230 | 7675 | `	if( pLookahed ){` |
|   42230 | 7676 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8362 | 7677 | `			return PH7_CompileClassInterface;` |
|   33870 | 7678 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   33678 | 7679 | `			return PH7_CompileClass;` |
|     194 | 7680 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      51 | 7681 | `			return PH7_CompileTrait;` |
|     142 | 7682 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      17 | 7683 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      16 | 7684 | `				return PH7_CompileAbstractClass;` |
|     128 | 7685 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7686 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7687 | `				return PH7_CompileFinalClass;` |
|       - | 7688 | `		}` |
|      63 | 7689 | `	}` |
|       - | 7690 | `	/* Not a language construct */` |
|     128 | 7691 | `	return 0;` |
|  200657 | 7692 |  |
|       - | 7693 | `/*` |
|       - | 7694 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7695 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7696 | ` */` |
|     126 | 7697 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7698 |  |
|       - | 7699 | `	int rc;` |
|     128 | 7700 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     128 | 7701 | `	if( rc == FALSE ){` |
|      40 | 7702 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 7703 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 7704 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7705 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7706 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7707 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7708 | `			*/` |
|       - | 7709 | `			){` |
|      34 | 7710 | `				rc = TRUE;` |
|      16 | 7711 | `		}` |
|      20 | 7712 | `	}` |
|     128 | 7713 | `	return rc;` |
|       2 | 7714 |  |
|       - | 7715 | `/*` |
|       - | 7716 | ` * Compile a PHP chunk.` |
|       - | 7717 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7718 | ` * takes care of generating the appropriate error message.` |
|       - | 7719 | ` */` |
|  548128 | 7720 | `static sxi32 GenStateCompileChunk(` |
|       - | 7721 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7722 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7723 | `	)` |
|       2 | 7724 |  |
|       - | 7725 | `	ProcLangConstruct xCons;` |
|       - | 7726 | `	sxi32 rc;` |
|  548130 | 7727 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  323560 | 7728 | `	for(;;){` |
|  647122 | 7729 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7730 | `			/* No more input to process */` |
|   11278 | 7731 | `			break;` |
|       - | 7732 | `		}` |
|  635846 | 7733 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7734 | `			/* Compile block */` |
|      12 | 7735 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7736 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7737 | `				break;` |
|       - | 7738 | `			}` |
|       7 | 7739 | `		}else{` |
|  635836 | 7740 | `			xCons = 0;` |
|  635836 | 7741 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  401312 | 7742 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7743 | `				/* Try to extract a language construct handler */` |
|  401312 | 7744 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  401312 | 7745 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7746 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7747 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7748 | `						&pGen->pIn->sData);` |
|       9 | 7749 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7750 | `						break;` |
|       - | 7751 | `					}` |
|       - | 7752 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7753 | `					 * this erroneous statement.` |
|       - | 7754 | `					 */` |
|       9 | 7755 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7756 | `				}` |
|  435181 | 7757 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   41004 | 7758 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7759 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7760 | `				xCons = PH7_CompileLabel;` |
|      56 | 7761 | `			}` |
|  635836 | 7762 | `			if( xCons == 0 ){` |
|       - | 7763 | `				/* Assume an expression an try to compile it */` |
|  234532 | 7764 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  234532 | 7765 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7766 | `					/* Pop l-value */` |
|  234408 | 7767 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  117203 | 7768 | `				}` |
|  117267 | 7769 | `			}else{` |
|       - | 7770 | `				/* Go compile the sucker */` |
|  401306 | 7771 | `				rc = xCons(&(*pGen));` |
|       - | 7772 | `			}` |
|  635836 | 7773 | `			if( rc == SXERR_ABORT ){` |
|       - | 7774 | `				/* Request to abort compilation */` |
|       3 | 7775 | `				break;` |
|       - | 7776 | `			}` |
|       - | 7777 | `		}` |
|       - | 7778 | `		/* Ignore trailing semi-colons ';' */` |
| 1056148 | 7779 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  420306 | 7780 | `			pGen->pIn++;` |
|       2 | 7781 | `		}` |
|  635844 | 7782 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7783 | `			/* Compile a single statement and return */` |
|  536852 | 7784 | `			break;` |
|       - | 7785 | `		}` |
|       - | 7786 | `		/* LOOP ONE */` |
|       - | 7787 | `		/* LOOP TWO */` |
|       - | 7788 | `		/* LOOP THREE */` |
|       - | 7789 | `		/* LOOP FOUR */` |
|       2 | 7790 | `	}` |
|       - | 7791 | `	/* Return compilation status */` |
|  548130 | 7792 | `	return rc;` |
|       2 | 7793 |  |
|       - | 7794 | `/*` |
|       - | 7795 | ` * Compile a Raw PHP chunk.` |
|       - | 7796 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7797 | ` * takes care of generating the appropriate error message.` |
|       - | 7798 | ` */` |
|   11280 | 7799 | `static sxi32 PH7_CompilePHP(` |
|       - | 7800 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7801 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7802 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7803 | `	)` |
|       2 | 7804 |  |
|   11282 | 7805 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7806 | `	sxi32 rc;` |
|       - | 7807 | `	/* Reset the token set */` |
|   11282 | 7808 | `	SySetReset(&(*pTokenSet));` |
|       - | 7809 | `	/* Mark as the default token set */` |
|   11282 | 7810 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7811 | `	/* Advance the stream cursor */` |
|   11282 | 7812 | `	pGen->pRawIn++;` |
|       - | 7813 | `	/* Tokenize the PHP chunk first */` |
|   11282 | 7814 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7815 | `	/* Point to the head and tail of the token stream. */` |
|   11282 | 7816 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11282 | 7817 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11282 | 7818 | `	if( is_expr ){` |
|     ! 0 | 7819 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7820 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7821 | `			/* A simple expression,compile it */` |
|     ! 0 | 7822 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7823 | `		}` |
|       - | 7824 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7825 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7826 | `		return SXRET_OK;` |
|       - | 7827 | `	}` |
|   11282 | 7828 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7829 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7830 | `		/*` |
|       - | 7831 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7832 | `		 * According to the PHP reference manual:` |
|       - | 7833 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7834 | `		 *  immediately follow` |
|       - | 7835 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7836 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7837 | `		 * Symisc extension:` |
|       - | 7838 | `		 *   This short syntax works with all PHP opening` |
|       - | 7839 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7840 | `		 *   only short tag.` |
|       - | 7841 | `		 */` |
|       - | 7842 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7843 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7844 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7845 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7846 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7847 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7848 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7849 | `		}` |
|       3 | 7850 | `		return SXRET_OK;` |
|       - | 7851 | `	}` |
|       - | 7852 | `	/* Compile the PHP chunk */` |
|   11280 | 7853 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7854 | `	/* Fix exceptions jumps */` |
|   11280 | 7855 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7856 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11280 | 7857 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7858 | `		rc = SXERR_ABORT;` |
|       1 | 7859 | `	}` |
|       - | 7860 | `	/* Reset container */` |
|   11280 | 7861 | `	SySetReset(&pGen->aGoto);` |
|   11280 | 7862 | `	SySetReset(&pGen->aLabel);` |
|       - | 7863 | `	/* Compilation result */` |
|   11280 | 7864 | `	return rc;` |
|    5642 | 7865 |  |
|       - | 7866 | `/*` |
|       - | 7867 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7868 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7869 | ` * This is the only compile interface exported from this file.` |
|       - | 7870 | ` */` |
|   13202 | 7871 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7872 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7873 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7874 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7875 | `	)` |
|       2 | 7876 |  |
|       - | 7877 | `	SySet aPhpToken,aRawToken;` |
|       - | 7878 | `	ph7_gen_state *pCodeGen;` |
|       - | 7879 | `	ph7_value *pRawObj;` |
|       - | 7880 | `	sxu32 nObjIdx;` |
|       - | 7881 | `	sxi32 nRawObj;` |
|       - | 7882 | `	int is_expr;` |
|       - | 7883 | `	sxi32 rc;` |
|   13204 | 7884 | `	if( pScript->nByte < 1 ){` |
|       - | 7885 | `		/* Nothing to compile */` |
|     ! 0 | 7886 | `		return PH7_OK;` |
|       - | 7887 | `	}` |
|       - | 7888 | `	/* Initialize the tokens containers */` |
|   13204 | 7889 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13204 | 7890 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13204 | 7891 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13204 | 7892 | `	is_expr = 0;` |
|   13204 | 7893 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7894 | `		SyToken sTmp;` |
|       - | 7895 | `		/* PHP only: -*/` |
|    2800 | 7896 | `		sTmp.nLine = 1;` |
|    2800 | 7897 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2800 | 7898 | `		sTmp.pUserData = 0;` |
|    2800 | 7899 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2800 | 7900 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2800 | 7901 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7902 | `			/* A simple PHP expression */` |
|     ! 0 | 7903 | `			is_expr = 1;` |
|     ! 0 | 7904 | `		}` |
|    1401 | 7905 | `	}else{` |
|       - | 7906 | `		/* Tokenize raw text */` |
|   10406 | 7907 | `		SySetAlloc(&aRawToken,32);` |
|   10406 | 7908 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7909 | `	}` |
|   13204 | 7910 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7911 | `	/* Process high-level tokens */` |
|   13204 | 7912 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13204 | 7913 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13204 | 7914 | `	rc = PH7_OK;` |
|   13204 | 7915 | `	if( is_expr ){` |
|       - | 7916 | `		/* Compile the expression */` |
|     ! 0 | 7917 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 7918 | `		goto cleanup;` |
|       - | 7919 | `	}` |
|   13204 | 7920 | `	nObjIdx = 0;` |
|       - | 7921 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 7922 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 7923 | `	 * preventing namespace bleeding across include()d files. */` |
|   13204 | 7924 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 7925 | `	/* Start the compilation process */` |
|   11807 | 7926 | `	for(;;){` |
|   34892 | 7927 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13200 | 7928 | `			break; /* No more tokens to process */` |
|       - | 7929 | `		}` |
|   21694 | 7930 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 7931 | `			/* Compile the PHP chunk */` |
|   11282 | 7932 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11282 | 7933 | `			if( rc == SXERR_ABORT ){` |
|       5 | 7934 | `				break;` |
|       - | 7935 | `			}` |
|   11278 | 7936 | `			continue;` |
|       - | 7937 | `		}` |
|       - | 7938 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10414 | 7939 | `		nRawObj = 0;` |
|   20826 | 7940 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 7941 | `			/* Consume the raw chunk without any processing */` |
|   10414 | 7942 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10414 | 7943 | `			if( pRawObj == 0 ){` |
|     ! 0 | 7944 | `				rc = SXERR_MEM;` |
|     ! 0 | 7945 | `				break;` |
|       - | 7946 | `			}` |
|       - | 7947 | `			/* Mark as constant and emit the load constant instruction */` |
|   10414 | 7948 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10414 | 7949 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10414 | 7950 | `			++nRawObj;` |
|   10414 | 7951 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 7952 | `		}` |
|   10414 | 7953 | `		if( nRawObj > 0 ){` |
|       - | 7954 | `			/* Emit the consume instruction */` |
|   10414 | 7955 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5206 | 7956 | `		}` |
|    6603 | 7957 | `	}` |
|    6601 | 7958 | `cleanup:` |
|   13204 | 7959 | `	SySetRelease(&aRawToken);` |
|   13204 | 7960 | `	SySetRelease(&aPhpToken);` |
|   13204 | 7961 | `	return rc;` |
|    6603 | 7962 |  |
|       - | 7963 | `/*` |
|       - | 7964 | ` * Utility routines.Initialize the code generator.` |
|       - | 7965 | ` */` |
|    2776 | 7966 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 7967 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7968 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7969 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7970 | `	)` |
|       2 | 7971 |  |
|    2778 | 7972 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7973 | `	/* Zero the structure */` |
|    2778 | 7974 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 7975 | `	/* Initial state */` |
|    2778 | 7976 | `	pGen->pVm  = &(*pVm);` |
|    2778 | 7977 | `	pGen->xErr = xErr;` |
|    2778 | 7978 | `	pGen->pErrData = pErrData;` |
|    2778 | 7979 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2778 | 7980 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2778 | 7981 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2778 | 7982 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 7983 | `	/* Error log buffer */` |
|    2778 | 7984 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 7985 | `	/* General purpose working buffer */` |
|    2778 | 7986 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 7987 | `	/* Namespace state */` |
|    2778 | 7988 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2778 | 7989 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7990 | `	/* Create the global scope */` |
|    2778 | 7991 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 7992 | `	/* Point to the global scope */` |
|    2778 | 7993 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2778 | 7994 | `	return SXRET_OK;` |
|       2 | 7995 |  |
|       - | 7996 | `/*` |
|       - | 7997 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 7998 | ` */` |
|   15718 | 7999 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8000 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8001 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8002 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8003 | `	)` |
|       2 | 8004 |  |
|   15720 | 8005 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8006 | `	GenBlock *pBlock,*pParent;` |
|       - | 8007 | `	/* Reset state */` |
|   15720 | 8008 | `	SySetReset(&pGen->aLabel);` |
|   15720 | 8009 | `	SySetReset(&pGen->aGoto);` |
|   15720 | 8010 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15720 | 8011 | `	SyBlobRelease(&pGen->sWorker);` |
|   15720 | 8012 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15720 | 8013 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15720 | 8014 | `	SyHashRelease(&pGen->hUseImports);` |
|   15720 | 8015 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 8016 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8017 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8018 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8019 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8020 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8021 | `	 * number of unique names, which is acceptable. */` |
|       - | 8022 | `	/* Point to the global scope */` |
|   15720 | 8023 | `	pBlock = pGen->pCurrent;` |
|   15720 | 8024 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8025 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8026 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8027 | `		pBlock = pParent;` |
|     ! 0 | 8028 | `	}` |
|   15720 | 8029 | `	pGen->xErr = xErr;` |
|   15720 | 8030 | `	pGen->pErrData = pErrData;` |
|   15720 | 8031 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15720 | 8032 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15720 | 8033 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15720 | 8034 | `	pGen->nErr = 0;` |
|   15720 | 8035 | `	return SXRET_OK;` |
|       2 | 8036 |  |
|       - | 8037 | `/*` |
|       - | 8038 | ` * Generate a compile-time error message.` |
|       - | 8039 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8040 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8041 | ` * abort compilation immediately.` |
|       - | 8042 | ` */` |
|     452 | 8043 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8044 |  |
|     454 | 8045 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     454 | 8046 | `	const char *zErr = "Error";` |
|       - | 8047 | `	SyString *pFile;` |
|       - | 8048 | `	va_list ap;` |
|       - | 8049 | `	sxi32 rc;` |
|       - | 8050 | `	/* Reset the working buffer */` |
|     454 | 8051 | `	SyBlobReset(pWorker);` |
|       - | 8052 | `	/* Peek the processed file path if available */` |
|     454 | 8053 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     454 | 8054 | `	if( nErrType == E_ERROR ){` |
|       - | 8055 | `		/* Increment the error counter */` |
|     412 | 8056 | `		pGen->nErr++;` |
|     412 | 8057 | `		if( pGen->nErr > 15 ){` |
|       - | 8058 | `			/* Error count limit reached */` |
|       5 | 8059 | `			if( pGen->xErr ){` |
|       5 | 8060 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8061 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8062 | `				if( pFile ){` |
|       5 | 8063 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8064 | `				}` |
|       5 | 8065 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8066 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8067 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8068 | `				}` |
|       2 | 8069 | `			}` |
|       - | 8070 | `			/* Abort immediately */` |
|       5 | 8071 | `			return SXERR_ABORT;` |
|       - | 8072 | `		}` |
|     203 | 8073 | `	}` |
|     450 | 8074 | `	if( pGen->xErr == 0 ){` |
|       - | 8075 | `		/* No available error consumer,return immediately */` |
|       3 | 8076 | `		return SXRET_OK;` |
|       - | 8077 | `	}` |
|     447 | 8078 | `	switch(nErrType){` |
|     405 | 8079 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8080 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8081 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8082 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8083 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8084 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8085 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8086 | `	default:` |
|     ! 0 | 8087 | `		break;` |
|       - | 8088 | `	}` |
|     447 | 8089 | `	rc = SXRET_OK;` |
|       - | 8090 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     447 | 8091 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     447 | 8092 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     447 | 8093 | `	va_start(ap,zFormat);` |
|     447 | 8094 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 8095 | `	va_end(ap);` |
|     447 | 8096 | `	if( pFile ){` |
|     447 | 8097 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     223 | 8098 | `	}` |
|       - | 8099 | `	/* Append a new line */` |
|     447 | 8100 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 8101 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8102 | `		/* Consume the generated error message */` |
|     447 | 8103 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 8104 | `	}` |
|     447 | 8105 | `	return rc;` |
|     228 | 8106 |  |
|       - | 8107 |  |
