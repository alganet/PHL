# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2866/3815 lines (75.12%)

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
|    2548 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2550 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7129 |  131 | `	for(;;){` |
|   14260 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2438 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2438 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2416 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   11846 |  140 | `		pBlock = pBlock->pParent;` |
|   11846 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1276 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  405176 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  405178 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  405178 |  162 | `	pBlock->pUserData   = pUserData;` |
|  405178 |  163 | `	pBlock->pGen        = pGen;` |
|  405178 |  164 | `	pBlock->iFlags      = iType;` |
|  405178 |  165 | `	pBlock->pParent     = 0;` |
|  405178 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  405178 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  405178 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  402866 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  402868 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  402868 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  402868 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  402868 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  402868 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  402868 |  200 | `	pGen->pCurrent = pBlock;` |
|  402868 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  193524 |  203 | `		*ppBlock = pBlock;` |
|   96761 |  204 | `	}` |
|  402868 |  205 | `	return SXRET_OK;` |
|  201435 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  402860 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  402862 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  402862 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  402862 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  402860 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  402862 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  402862 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  402862 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  402862 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  402860 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  402862 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  402862 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  402862 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  402862 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  402862 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  402862 |  244 | `	return SXRET_OK;` |
|  201432 |  245 |  |
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
|  149888 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  149890 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  149890 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  149890 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  149890 |  265 | `	return rc;` |
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
|  306226 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  306228 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  598490 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  292264 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  113876 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  178390 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   28504 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  149888 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  149888 |  298 | `		if( pInstr ){` |
|  149888 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  149888 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  149888 |  302 | `			aFix[n].nJumpType = -1;` |
|   74943 |  303 | `		}` |
|   74945 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  306228 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   89698 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   89700 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   89846 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   89698 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   89830 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   89698 |  358 | `	return SXRET_OK;` |
|   44851 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  388578 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  388580 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  388580 |  367 | `	if( pEntry == 0 ){` |
|  169890 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  218692 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  218692 |  371 | `	return SXRET_OK;` |
|  194291 |  372 |  |
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
|  169888 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  169890 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  169890 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   84944 |  387 | `	}` |
|  169890 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   75478 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   75480 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   75480 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   75480 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   75480 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   75480 |  408 | `	return pObj;` |
|   37741 |  409 |  |
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
|   75878 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   75880 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   75880 |  434 | `	sxu32 nIdx = 0;` |
|   75880 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   75480 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   75480 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   75480 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   75480 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   37741 |  445 | `	}else{` |
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
|   75880 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   75880 |  460 | `	return SXRET_OK;` |
|   37941 |  461 |  |
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
|   50204 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   50206 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   50206 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   50206 |  481 | `	zIn  = pStr->zString;` |
|   50206 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   50206 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   50096 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   15112 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15112 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   34986 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   34986 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   34986 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   35001 |  505 | `	for(;;){` |
|   70004 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   34986 |  508 | `			break;` |
|       - |  509 | `		}` |
|   35020 |  510 | `		zCur = zIn;` |
|  552906 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  517888 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   35020 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   35002 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   17500 |  517 | `		}` |
|   35020 |  518 | `		zIn++;` |
|   35020 |  519 | `		if( zIn < zEnd ){` |
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
|   35020 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   34986 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   34986 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   34986 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   17492 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   34986 |  543 | `	return SXRET_OK;` |
|   25104 |  544 |  |
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
|   14310 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14312 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14312 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14312 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14312 |  650 | `	(*pCount)++;` |
|   14312 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14312 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14312 |  654 | `	return pConstObj;` |
|    7157 |  655 |  |
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
|   13220 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13222 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13222 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13222 |  702 | `	zIn  = pStr->zString;` |
|   13222 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13222 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   13000 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   13000 |  714 | `	iCons = 0;` |
|    7256 |  715 | `	for(;;){` |
|   21796 |  716 | `		zCur = zIn;` |
|  126964 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  106684 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  106600 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1432 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     716 |  722 | `					break;` |
|       - |  723 | `			}` |
|  105170 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   21796 |  726 | `		if( zIn > zCur ){` |
|   10734 |  727 | `			if( pObj == 0 ){` |
|   10464 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10464 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5231 |  732 | `			}` |
|   10734 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5366 |  734 | `		}` |
|   21796 |  735 | `		if( zIn >= zEnd ){` |
|   13000 |  736 | `			break;` |
|       - |  737 | `		}` |
|    8798 |  738 | `		if( zIn[0] == '\\' ){` |
|    7284 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7284 |  741 | `			zIn++;` |
|    7284 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7284 |  745 | `			if( pObj == 0 ){` |
|    3850 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    3850 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    1924 |  750 | `			}` |
|    7284 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7284 |  752 | `			switch( zIn[0] ){` |
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
|    3279 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6560 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6560 |  776 | `				break;` |
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
|    7284 |  844 | `			zIn += n;` |
|    7284 |  845 | `			continue;` |
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
|   13000 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1164 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     581 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   13000 |  968 | `	return SXRET_OK;` |
|    6612 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13194 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13196 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6597 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13196 |  980 | `	return rc;` |
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
|   13798 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   13800 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   13800 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   13800 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   13800 | 1028 | `	return rc;` |
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
|   20210 | 1071 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1072 |  |
|       - | 1073 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1074 | `	SyToken *pKey,*pCur;` |
|   20212 | 1075 | `	sxi32 iEmitRef = 0;` |
|   20212 | 1076 | `	sxi32 nPair = 0;` |
|       - | 1077 | `	sxi32 iNest;` |
|       - | 1078 | `	sxi32 rc;` |
|       - | 1079 | `	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.` |
|       - | 1080 | `	 */` |
|   20212 | 1081 | `	pGen->pIn += 2;` |
|   20212 | 1082 | `	pGen->pEnd--;` |
|   20212 | 1083 | `	xValidator = 0;` |
|   10105 | 1084 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   16416 | 1085 | `	for(;;){` |
|       - | 1086 | `		/* Jump leading commas */` |
|   37116 | 1087 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4284 | 1088 | `			pGen->pIn++;` |
|       2 | 1089 | `		}` |
|   32834 | 1090 | `		pCur = pGen->pIn;` |
|   32834 | 1091 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1092 | `			/* No more entry to process */` |
|   20200 | 1093 | `			break;` |
|       - | 1094 | `		}` |
|   12636 | 1095 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1096 | `			continue;` |
|       - | 1097 | `		}` |
|       - | 1098 | `		/* Compile the key if available */` |
|   12636 | 1099 | `		pKey = pCur;` |
|   12636 | 1100 | `		iNest = 0;` |
|   34910 | 1101 | `		while( pCur < pGen->pIn ){` |
|   23412 | 1102 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1138 | 1103 | `				break;` |
|       - | 1104 | `			}` |
|   22276 | 1105 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      62 | 1106 | `				iNest++;` |
|   22246 | 1107 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1108 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1109 | `				 * parser will shortly detect any syntax error.` |
|       - | 1110 | `				 */` |
|      62 | 1111 | `				iNest--;` |
|      30 | 1112 | `			}` |
|   22276 | 1113 | `			pCur++;` |
|       2 | 1114 | `		}` |
|   12636 | 1115 | `		rc = SXERR_EMPTY;` |
|   12636 | 1116 | `		if( pCur < pGen->pIn ){` |
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
|   12063 | 1132 | `		}else if( pKey == pCur ){` |
|       - | 1133 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1134 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1135 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1136 | `		}else{` |
|       - | 1137 | `			/* Reset back the cursor and point to the entry value */` |
|   11500 | 1138 | `			pCur = pKey;` |
|       - | 1139 | `		}` |
|   12626 | 1140 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1141 | `			/* No available key,load NULL */` |
|   11502 | 1142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    5750 | 1143 | `		}` |
|   12626 | 1144 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   12624 | 1159 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   12624 | 1160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1161 | `			return SXERR_ABORT;` |
|       - | 1162 | `		}` |
|   12624 | 1163 | `		if( iEmitRef ){` |
|       - | 1164 | `			/* Emit the load reference instruction */` |
|      32 | 1165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1166 | `		}` |
|   12624 | 1167 | `		xValidator = 0;` |
|   12624 | 1168 | `		iEmitRef = 0;` |
|   12624 | 1169 | `		nPair++;` |
|       2 | 1170 | `	}` |
|       - | 1171 | `	/* Emit the load map instruction */` |
|   20200 | 1172 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1173 | `	/* Node successfully compiled */` |
|   20200 | 1174 | `	return SXRET_OK;` |
|   10107 | 1175 |  |
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
|  622224 | 1437 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1438 |  |
|  622226 | 1439 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1440 | `	sxi32 iVv;` |
|       - | 1441 | `	sxi32 iP1;` |
|       - | 1442 | `	void *p3;` |
|       - | 1443 | `	sxi32 rc;` |
|  622226 | 1444 | `	iVv = -1; /* Variable variable counter */` |
| 1244462 | 1445 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  622238 | 1446 | `		pGen->pIn++;` |
|  622238 | 1447 | `		iVv++;` |
|       2 | 1448 | `	}` |
|  622226 | 1449 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1450 | `		/* Invalid variable name */` |
|       3 | 1451 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1452 | `		if( rc == SXERR_ABORT ){` |
|       - | 1453 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1454 | `			return SXERR_ABORT;` |
|       - | 1455 | `		}` |
|       3 | 1456 | `		return SXRET_OK;` |
|       - | 1457 | `	}` |
|  622224 | 1458 | `	p3  = 0;` |
|  622224 | 1459 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  622208 | 1479 | `		char *zName = 0;` |
|       - | 1480 | `		/* Extract variable name */` |
|  622208 | 1481 | `		pName = &pGen->pIn->sData;` |
|       - | 1482 | `		/* Advance the stream cursor */` |
|  622208 | 1483 | `		pGen->pIn++;` |
|  622208 | 1484 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  622208 | 1485 | `		if( pEntry == 0 ){` |
|       - | 1486 | `			/* Duplicate name */` |
|   92168 | 1487 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   92168 | 1488 | `			if( zName == 0 ){` |
|     ! 0 | 1489 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1490 | `				return SXERR_ABORT;` |
|       - | 1491 | `			}` |
|       - | 1492 | `			/* Install in the hashtable */` |
|   92168 | 1493 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   46085 | 1494 | `		}else{` |
|       - | 1495 | `			/* Name already available */` |
|  530042 | 1496 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1497 | `		}` |
|  622208 | 1498 | `		p3 = (void *)zName;` |
|       - | 1499 | `	}` |
|  622220 | 1500 | `	iP1 = 0;` |
|  622220 | 1501 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  206858 | 1502 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1503 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  206854 | 1504 | `			iP1 = 1;` |
|  103426 | 1505 | `		}` |
|  103428 | 1506 | `	}` |
|       - | 1507 | `	/* Emit the load instruction */` |
|  622220 | 1508 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  622232 | 1509 | `	while( iVv > 0 ){` |
|      13 | 1510 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1511 | `		iVv--;` |
|       1 | 1512 | `	}` |
|       - | 1513 | `	/* Node successfully compiled */` |
|  622220 | 1514 | `	return SXRET_OK;` |
|  311114 | 1515 |  |
|       - | 1516 | `/*` |
|       - | 1517 | ` * Load a literal.` |
|       - | 1518 | ` */` |
|  402004 | 1519 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1520 |  |
|  402006 | 1521 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1522 | `	ph7_value *pObj;` |
|       - | 1523 | `	SyString *pStr;` |
|       - | 1524 | `	sxu32 nIdx;` |
|       - | 1525 | `	/* Extract token value */` |
|  402006 | 1526 | `	pStr = &pToken->sData;` |
|       - | 1527 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  402006 | 1528 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   75252 | 1529 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1530 | `			/* NULL constant are always indexed at 0 */` |
|   28010 | 1531 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   28010 | 1532 | `			return SXRET_OK;` |
|   47244 | 1533 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1534 | `			/* TRUE constant are always indexed at 1 */` |
|     462 | 1535 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     462 | 1536 | `			return SXRET_OK;` |
|       2 | 1537 | `		}` |
|  387001 | 1538 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   73708 | 1539 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1540 | `			/* FALSE constant are always indexed at 2 */` |
|   30556 | 1541 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   30556 | 1542 | `			return SXRET_OK;` |
|  327759 | 1543 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   63114 | 1544 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1545 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    4624 | 1546 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    4624 | 1547 | `			if( pObj == 0 ){` |
|     ! 0 | 1548 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1549 | `				return SXERR_ABORT;` |
|       - | 1550 | `			}` |
|    4624 | 1551 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1552 | `			/* Emit the load constant instruction */` |
|    4624 | 1553 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    4624 | 1554 | `			return SXRET_OK;` |
|  299775 | 1555 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   16390 | 1556 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  299046 | 1572 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    7450 | 1573 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  295315 | 1574 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    7496 | 1575 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  338348 | 1605 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1606 | `		ph7_value *pLitObj;` |
|       - | 1607 | `		/* Unknown literal,install it in the literal table */` |
|  134840 | 1608 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  134840 | 1609 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1610 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1611 | `			return SXERR_ABORT;` |
|       - | 1612 | `		}` |
|  134840 | 1613 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  134840 | 1614 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   67419 | 1615 | `	}` |
|       - | 1616 | `	/* Emit the load constant instruction */` |
|  338348 | 1617 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  338348 | 1618 | `	return SXRET_OK;` |
|  201004 | 1619 |  |
|       - | 1620 | `/*` |
|       - | 1621 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1622 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1623 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1624 | ` * Otherwise, load the simple literal directly.` |
|       - | 1625 | ` */` |
|  402024 | 1626 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1627 |  |
|       - | 1628 | `	sxi32 rc;` |
|  402026 | 1629 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1630 | `		return SXRET_OK;` |
|       - | 1631 | `	}` |
|       - | 1632 | `	/* Check if this is a multi-token namespace path */` |
|  402026 | 1633 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|      11 | 1668 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      11 | 1669 | `				if( pObj == 0 ){` |
|     ! 0 | 1670 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1671 | `					return SXERR_ABORT;` |
|       - | 1672 | `				}` |
|      11 | 1673 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      11 | 1674 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       5 | 1675 | `			}` |
|       - | 1676 | `			/* Emit the load constant instruction.` |
|       - | 1677 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      21 | 1678 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      21 | 1679 | `			return SXRET_OK;` |
|       - | 1680 | `		}` |
|     ! 0 | 1681 | `	}` |
|       - | 1682 | `	/* Single-token literal: load directly */` |
|  402006 | 1683 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  402006 | 1684 | `	return rc;` |
|  201014 | 1685 |  |
|       - | 1686 | `/*` |
|       - | 1687 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1688 | ` */` |
|  402024 | 1689 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1690 |  |
|       - | 1691 | `	sxi32 rc;` |
|  402026 | 1692 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  402026 | 1693 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1694 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1695 | `		return rc;` |
|       - | 1696 | `	}` |
|       - | 1697 | `	/* Node successfully compiled */` |
|  402026 | 1698 | `	return SXRET_OK;` |
|  201014 | 1699 |  |
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
|    2348 | 1842 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1843 |  |
|       - | 1844 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1845 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1846 | `	sxu32 nLineLocal;` |
|       - | 1847 | `	sxi32 rc;` |
|    2350 | 1848 | `	nLineLocal = pGen->pIn->nLine;` |
|    2350 | 1849 | `	iLevel = 0;` |
|       - | 1850 | `	/* Jump the 'continue' keyword */` |
|    2350 | 1851 | `	pGen->pIn++;` |
|    2350 | 1852 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2350 | 1863 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2350 | 1864 | `	if( pLoop == 0 ){` |
|       - | 1865 | `		/* Illegal continue */` |
|      11 | 1866 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1867 | `		if( rc == SXERR_ABORT ){` |
|       - | 1868 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1869 | `			return SXERR_ABORT;` |
|       - | 1870 | `		}` |
|       6 | 1871 | `	}else{` |
|    2340 | 1872 | `		sxu32 nInstrIdx = 0;` |
|    2340 | 1873 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2336 | 1885 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2336 | 1886 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1887 | `				JumpFixup sJumpFix;` |
|       - | 1888 | `				/* Post-continue */` |
|       8 | 1889 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       8 | 1890 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       8 | 1891 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       3 | 1892 | `			}` |
|       - | 1893 | `		}` |
|       - | 1894 | `	}` |
|    2350 | 1895 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1896 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1897 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1898 | `	}` |
|       - | 1899 | `	/* Statement successfully compiled */` |
|    2350 | 1900 | `	return SXRET_OK;` |
|    1176 | 1901 |  |
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
|  210640 | 2161 | `static sxi32 PH7_CompileBlock(` |
|       - | 2162 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2163 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2164 | `	)` |
|       2 | 2165 |  |
|       - | 2166 | `	sxi32 rc;` |
|       - | 2167 | `	sxu32 nLine;` |
|  210642 | 2168 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  209346 | 2169 | `		nLine = pGen->pIn->nLine;` |
|  209346 | 2170 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  209346 | 2171 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2172 | `			return SXERR_ABORT;` |
|       - | 2173 | `		}` |
|  209346 | 2174 | `		pGen->pIn++;` |
|       - | 2175 | `		/* Compile until we hit the closing braces '}' */` |
|  305730 | 2176 | `		for(;;){` |
|  611462 | 2177 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  611442 | 2188 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2189 | `				/* Closing braces found,break immediately*/` |
|  209326 | 2190 | `				pGen->pIn++;` |
|  209326 | 2191 | `				break;` |
|       - | 2192 | `			}` |
|       - | 2193 | `			/* Compile a single statement */` |
|  402118 | 2194 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  402118 | 2195 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2196 | `				return SXERR_ABORT;` |
|       - | 2197 | `			}` |
|       2 | 2198 | `		}` |
|  209346 | 2199 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  105970 | 2200 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1298 | 2244 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1298 | 2245 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2246 | `			return SXERR_ABORT;` |
|       - | 2247 | `		}` |
|       - | 2248 | `	}` |
|       - | 2249 | `	/* Jump trailing semi-colons ';' */` |
|  210642 | 2250 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2251 | `		pGen->pIn++;` |
|     ! 0 | 2252 | `	}` |
|  210642 | 2253 | `	return SXRET_OK;` |
|  105322 | 2254 |  |
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
|    9316 | 2274 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2275 |  |
|    9318 | 2276 | `	GenBlock *pWhileBlock = 0;` |
|    9318 | 2277 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2278 | `	sxu32 nFalseJump;` |
|       - | 2279 | `	sxu32 nLine;` |
|       - | 2280 | `	sxi32 rc;` |
|    9318 | 2281 | `	nLine = pGen->pIn->nLine;` |
|       - | 2282 | `	/* Jump the 'while' keyword */` |
|    9318 | 2283 | `	pGen->pIn++;` |
|    9318 | 2284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2285 | `		/* Syntax error */` |
|     ! 0 | 2286 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2287 | `		if( rc == SXERR_ABORT ){` |
|       - | 2288 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2289 | `			return SXERR_ABORT;` |
|       - | 2290 | `		}` |
|     ! 0 | 2291 | `		goto Synchronize;` |
|       - | 2292 | `	}` |
|       - | 2293 | `	/* Jump the left parenthesis '(' */` |
|    9318 | 2294 | `	pGen->pIn++;` |
|       - | 2295 | `	/* Create the loop block */` |
|    9318 | 2296 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    9318 | 2297 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2298 | `		return SXERR_ABORT;` |
|       - | 2299 | `	}` |
|       - | 2300 | `	/* Delimit the condition */` |
|    9318 | 2301 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9318 | 2302 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2303 | `		/* Empty expression */` |
|       3 | 2304 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2305 | `		if( rc == SXERR_ABORT ){` |
|       - | 2306 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2307 | `			return SXERR_ABORT;` |
|       - | 2308 | `		}` |
|       1 | 2309 | `	}` |
|       - | 2310 | `	/* Swap token streams */` |
|    9318 | 2311 | `	pTmp = pGen->pEnd;` |
|    9318 | 2312 | `	pGen->pEnd = pEnd;` |
|       - | 2313 | `	/* Compile the expression */` |
|    9318 | 2314 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9318 | 2315 | `	if( rc == SXERR_ABORT ){` |
|       - | 2316 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2317 | `		return SXERR_ABORT;` |
|       - | 2318 | `	}` |
|       - | 2319 | `	/* Update token stream */` |
|    9318 | 2320 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2321 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2322 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2323 | `			return SXERR_ABORT;` |
|       - | 2324 | `		}` |
|     ! 0 | 2325 | `		pGen->pIn++;` |
|     ! 0 | 2326 | `	}` |
|       - | 2327 | `	/* Synchronize pointers */` |
|    9318 | 2328 | `	pGen->pIn  = &pEnd[1];` |
|    9318 | 2329 | `	pGen->pEnd = pTmp;` |
|       - | 2330 | `	/* Emit the false jump */` |
|    9318 | 2331 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2332 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9318 | 2333 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2334 | `	/* Compile the loop body */` |
|    9318 | 2335 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    9318 | 2336 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2337 | `		return SXERR_ABORT;` |
|       - | 2338 | `	}` |
|       - | 2339 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9318 | 2340 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2341 | `	/* Fix all jumps now the destination is resolved */` |
|    9318 | 2342 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2343 | `	/* Release the loop block */` |
|    9318 | 2344 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2345 | `	/* Statement successfully compiled */` |
|    9318 | 2346 | `	return SXRET_OK;` |
|     ! 0 | 2347 | `Synchronize:` |
|       - | 2348 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2349 | `	 * compiling this erroneous block.` |
|       - | 2350 | `	 */` |
|     ! 0 | 2351 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2352 | `		pGen->pIn++;` |
|     ! 0 | 2353 | `	}` |
|     ! 0 | 2354 | `	return SXRET_OK;` |
|    4660 | 2355 |  |
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
|    9316 | 2503 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2504 |  |
|    9318 | 2505 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|    9318 | 2506 | `	GenBlock *pForBlock = 0;` |
|       - | 2507 | `	sxu32 nFalseJump;` |
|       - | 2508 | `	sxu32 nLine;` |
|       - | 2509 | `	sxi32 rc;` |
|    9318 | 2510 | `	nLine = pGen->pIn->nLine;` |
|       - | 2511 | `	/* Jump the 'for' keyword */` |
|    9318 | 2512 | `	pGen->pIn++;` |
|    9318 | 2513 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2514 | `		/* Syntax error */` |
|     ! 0 | 2515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2516 | `		if( rc == SXERR_ABORT ){` |
|       - | 2517 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2518 | `			return SXERR_ABORT;` |
|       - | 2519 | `		}` |
|     ! 0 | 2520 | `		return SXRET_OK;` |
|       - | 2521 | `	}` |
|       - | 2522 | `	/* Jump the left parenthesis '(' */` |
|    9318 | 2523 | `	pGen->pIn++;` |
|       - | 2524 | `	/* Delimit the init-expr;condition;post-expr */` |
|    9318 | 2525 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    9318 | 2526 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    9318 | 2541 | `	pTmp = pGen->pEnd;` |
|    9318 | 2542 | `	pGen->pEnd = pEnd;` |
|       - | 2543 | `	/* Compile initialization expressions if available */` |
|    9318 | 2544 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2545 | `	/* Pop operand lvalues */` |
|    9318 | 2546 | `	if( rc == SXERR_ABORT ){` |
|       - | 2547 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2548 | `		return SXERR_ABORT;` |
|    9318 | 2549 | `	}else if( rc != SXERR_EMPTY ){` |
|    9316 | 2550 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4657 | 2551 | `	}` |
|    9318 | 2552 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    9318 | 2563 | `	pGen->pIn++;` |
|       - | 2564 | `	/* Create the loop block */` |
|    9318 | 2565 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|    9318 | 2566 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2567 | `		return SXERR_ABORT;` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* Deffer continue jumps */` |
|    9318 | 2570 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2571 | `	/* Compile the condition */` |
|    9318 | 2572 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9318 | 2573 | `	if( rc == SXERR_ABORT ){` |
|       - | 2574 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2575 | `		return SXERR_ABORT;` |
|    9318 | 2576 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2577 | `		/* Emit the false jump */` |
|    9316 | 2578 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2579 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    9316 | 2580 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    4657 | 2581 | `	}` |
|    9318 | 2582 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|    9314 | 2593 | `	pGen->pIn++;` |
|       - | 2594 | `	/* Save the post condition stream */` |
|    9314 | 2595 | `	pPostStart = pGen->pIn;` |
|       - | 2596 | `	/* Compile the loop body */` |
|    9314 | 2597 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|    9314 | 2598 | `	pGen->pEnd = pTmp;` |
|    9314 | 2599 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|    9314 | 2600 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2601 | `		return SXERR_ABORT;` |
|       - | 2602 | `	}` |
|       - | 2603 | `	/* Fix post-continue jumps */` |
|    9314 | 2604 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|    9314 | 2620 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2621 | `		pPostStart++;` |
|     ! 0 | 2622 | `	}` |
|    9314 | 2623 | `	if( pPostStart < pEnd ){` |
|       - | 2624 | `		SyToken *pTmpIn,*pTmpEnd;` |
|    9314 | 2625 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|    9314 | 2626 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    9314 | 2627 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2628 | `			/* Syntax error */` |
|     ! 0 | 2629 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2630 | `			if( rc == SXERR_ABORT ){` |
|       - | 2631 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2632 | `				return SXERR_ABORT;` |
|       - | 2633 | `			}` |
|     ! 0 | 2634 | `			return SXRET_OK;` |
|       - | 2635 | `		}` |
|    9314 | 2636 | `		RE_SWAP_DELIMITER(pGen);` |
|    9314 | 2637 | `		if( rc == SXERR_ABORT ){` |
|       - | 2638 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2639 | `			return SXERR_ABORT;` |
|    9314 | 2640 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2641 | `			/* Pop operand lvalue */` |
|    9314 | 2642 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    4656 | 2643 | `		}` |
|    4656 | 2644 | `	}` |
|       - | 2645 | `	/* Emit the unconditional jump to the start of the loop */` |
|    9314 | 2646 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2647 | `	/* Fix all jumps now the destination is resolved */` |
|    9314 | 2648 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2649 | `	/* Release the loop block */` |
|    9314 | 2650 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2651 | `	/* Statement successfully compiled */` |
|    9314 | 2652 | `	return SXRET_OK;` |
|    4660 | 2653 |  |
|       - | 2654 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2655 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2656 | ` * are allowed.` |
|       - | 2657 | ` */` |
|    4952 | 2658 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2659 |  |
|    4954 | 2660 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    4954 | 2661 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2662 | `		/* Unexpected expression */` |
|     ! 0 | 2663 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2664 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2665 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2666 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2667 | `		}` |
|     ! 0 | 2668 | `	}` |
|    4954 | 2669 | `	return rc;` |
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
|    2498 | 2697 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2698 |  |
|    2500 | 2699 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2500 | 2700 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2701 | `	ph7_foreach_info *pInfo;` |
|       - | 2702 | `	sxu32 nFalseJump;` |
|       - | 2703 | `	VmInstr *pInstr;` |
|       - | 2704 | `	sxu32 nLine;` |
|       - | 2705 | `	sxi32 rc;` |
|    2500 | 2706 | `	nLine = pGen->pIn->nLine;` |
|       - | 2707 | `	/* Jump the 'foreach' keyword */` |
|    2500 | 2708 | `	pGen->pIn++;` |
|    2500 | 2709 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2710 | `		/* Syntax error */` |
|     ! 0 | 2711 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2712 | `		if( rc == SXERR_ABORT ){` |
|       - | 2713 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2714 | `			return SXERR_ABORT;` |
|       - | 2715 | `		}` |
|     ! 0 | 2716 | `		goto Synchronize;` |
|       - | 2717 | `	}` |
|       - | 2718 | `	/* Jump the left parenthesis '(' */` |
|    2500 | 2719 | `	pGen->pIn++;` |
|       - | 2720 | `	/* Create the loop block */` |
|    2500 | 2721 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2500 | 2722 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2723 | `		return SXERR_ABORT;` |
|       - | 2724 | `	}` |
|       - | 2725 | `	/* Delimit the expression */` |
|    2500 | 2726 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2500 | 2727 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    2500 | 2742 | `	pCur = pGen->pIn;` |
|   16768 | 2743 | `	while( pCur < pEnd ){` |
|   16768 | 2744 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2510 | 2745 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2510 | 2746 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2747 | `				/* Break with the first 'as' found */` |
|    2500 | 2748 | `				break;` |
|       - | 2749 | `			}` |
|       5 | 2750 | `		}` |
|       - | 2751 | `		/* Advance the stream cursor */` |
|   14270 | 2752 | `		pCur++;` |
|       2 | 2753 | `	}` |
|    2500 | 2754 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2755 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2756 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2757 | `		if( rc == SXERR_ABORT ){` |
|       - | 2758 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2759 | `			return SXERR_ABORT;` |
|       - | 2760 | `		}` |
|     ! 0 | 2761 | `		goto Synchronize;` |
|       - | 2762 | `	}` |
|       - | 2763 | `	/* Swap token streams */` |
|    2500 | 2764 | `	pTmp = pGen->pEnd;` |
|    2500 | 2765 | `	pGen->pEnd = pCur;` |
|    2500 | 2766 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2500 | 2767 | `	if( rc == SXERR_ABORT ){` |
|       - | 2768 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2769 | `		return SXERR_ABORT;` |
|       - | 2770 | `	}` |
|       - | 2771 | `	/* Update token stream */` |
|    2500 | 2772 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2773 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2774 | `		if( rc == SXERR_ABORT ){` |
|       - | 2775 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2776 | `			return SXERR_ABORT;` |
|       - | 2777 | `		}` |
|     ! 0 | 2778 | `		pGen->pIn++;` |
|     ! 0 | 2779 | `	}` |
|    2500 | 2780 | `	pCur++; /* Jump the 'as' keyword */` |
|    2500 | 2781 | `	pGen->pIn = pCur;` |
|    2500 | 2782 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2783 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2785 | `			return SXERR_ABORT;` |
|       - | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|       - | 2788 | `	/* Create the foreach context */` |
|    2500 | 2789 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2500 | 2790 | `	if( pInfo == 0 ){` |
|     ! 0 | 2791 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2792 | `		return SXERR_ABORT;` |
|       - | 2793 | `	}` |
|       - | 2794 | `	/* Zero the structure */` |
|    2500 | 2795 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2796 | `	/* Initialize structure fields */` |
|    2500 | 2797 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2798 | `	/* Check if we have a key field */` |
|    7498 | 2799 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5000 | 2800 | `		pCur++;` |
|       2 | 2801 | `	}` |
|    2500 | 2802 | `	if( pCur < pEnd ){` |
|       - | 2803 | `		/* Compile the expression holding the key name */` |
|    2456 | 2804 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2805 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2806 | `			if( rc == SXERR_ABORT ){` |
|       - | 2807 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2808 | `				return SXERR_ABORT;` |
|       - | 2809 | `			}` |
|     ! 0 | 2810 | `		}else{` |
|    2456 | 2811 | `			pGen->pEnd = pCur;` |
|    2456 | 2812 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2456 | 2813 | `			if( rc == SXERR_ABORT ){` |
|       - | 2814 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2815 | `				return SXERR_ABORT;` |
|       - | 2816 | `			}` |
|    2456 | 2817 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2456 | 2818 | `			if( pInstr->p3 ){` |
|       - | 2819 | `				/* Record key name */` |
|    2456 | 2820 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1227 | 2821 | `			}` |
|    2456 | 2822 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2823 | `		}` |
|    2456 | 2824 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1227 | 2825 | `	}` |
|    2500 | 2826 | `	pGen->pEnd = pEnd;` |
|    2500 | 2827 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2829 | `		if( rc == SXERR_ABORT ){` |
|       - | 2830 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2831 | `			return SXERR_ABORT;` |
|       - | 2832 | `		}` |
|     ! 0 | 2833 | `		goto Synchronize;` |
|       - | 2834 | `	}` |
|    2500 | 2835 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2836 | `		pGen->pIn++;` |
|       - | 2837 | `		/* Pass by reference  */` |
|       7 | 2838 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2839 | `	}` |
|       - | 2840 | `	/* Compile the expression holding the value name */` |
|    2500 | 2841 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2500 | 2842 | `	if( rc == SXERR_ABORT ){` |
|       - | 2843 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2844 | `		return SXERR_ABORT;` |
|       - | 2845 | `	}` |
|    2500 | 2846 | `	pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2500 | 2847 | `	if( pInstr->p3 ){` |
|       - | 2848 | `		/* Record value name */` |
|    2500 | 2849 | `		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1249 | 2850 | `	}` |
|       - | 2851 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2500 | 2852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2853 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2500 | 2854 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2855 | `	/* Record the first instruction to execute */` |
|    2500 | 2856 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2857 | `	/* Emit the FOREACH_STEP instruction */` |
|    2500 | 2858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2859 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2500 | 2860 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2861 | `	/* Compile the loop body */` |
|    2500 | 2862 | `	pGen->pIn = &pEnd[1];` |
|    2500 | 2863 | `	pGen->pEnd = pTmp;` |
|    2500 | 2864 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2500 | 2865 | `	if( rc == SXERR_ABORT ){` |
|       - | 2866 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2867 | `		return SXERR_ABORT;` |
|       - | 2868 | `	}` |
|       - | 2869 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2500 | 2870 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2871 | `	/* Fix all jumps now the destination is resolved */` |
|    2500 | 2872 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2873 | `	/* Release the loop block */` |
|    2500 | 2874 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2875 | `	/* Statement successfully compiled */` |
|    2500 | 2876 | `	return SXRET_OK;` |
|     ! 0 | 2877 | `Synchronize:` |
|       - | 2878 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2879 | `	 * compiling this erroneous block.` |
|       - | 2880 | `	 */` |
|     ! 0 | 2881 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2882 | `		pGen->pIn++;` |
|     ! 0 | 2883 | `	}` |
|     ! 0 | 2884 | `	return SXRET_OK;` |
|    1251 | 2885 |  |
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
|   93024 | 2918 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 2919 |  |
|   93026 | 2920 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|   93026 | 2921 | `	GenBlock *pCondBlock = 0;` |
|       - | 2922 | `	sxu32 nJumpIdx;` |
|       - | 2923 | `	sxu32 nKeyID;` |
|       - | 2924 | `	sxi32 rc;` |
|       - | 2925 | `	/* Jump the 'if' keyword */` |
|   93026 | 2926 | `	pGen->pIn++;` |
|   93026 | 2927 | `	pToken = pGen->pIn;` |
|       - | 2928 | `	/* Create the conditional block */` |
|   93026 | 2929 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|   93026 | 2930 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2931 | `		return SXERR_ABORT;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   51144 | 2934 | `	for(;;){` |
|  102290 | 2935 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  102290 | 2948 | `		pToken++;` |
|       - | 2949 | `		/* Delimit the condition */` |
|  102290 | 2950 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  102290 | 2951 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  102290 | 2964 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2965 | `		/* Compile the condition */` |
|  102290 | 2966 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2967 | `		/* Update token stream */` |
|  102290 | 2968 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2969 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2970 | `			pGen->pIn++;` |
|     ! 0 | 2971 | `		}` |
|  102290 | 2972 | `		pGen->pIn  = &pEnd[1];` |
|  102290 | 2973 | `		pGen->pEnd = pTmp;` |
|  102290 | 2974 | `		if( rc == SXERR_ABORT ){` |
|       - | 2975 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2976 | `			return SXERR_ABORT;` |
|       - | 2977 | `		}` |
|       - | 2978 | `		/* Emit the false jump */` |
|  102290 | 2979 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 2980 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  102290 | 2981 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 2982 | `		/* Compile the body */` |
|  102290 | 2983 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  102290 | 2984 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2985 | `			return SXERR_ABORT;` |
|       - | 2986 | `		}` |
|  102290 | 2987 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   27549 | 2988 | `			break;` |
|       - | 2989 | `		}` |
|       - | 2990 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   47196 | 2991 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   47196 | 2992 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   30282 | 2993 | `			break;` |
|       - | 2994 | `		}` |
|       - | 2995 | `		/* Emit the unconditional jump */` |
|   16916 | 2996 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 2997 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   16916 | 2998 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   16916 | 2999 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   12272 | 3000 | `			pToken = &pGen->pIn[1];` |
|   12272 | 3001 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    4650 | 3002 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    3827 | 3003 | `					break;` |
|       - | 3004 | `			}` |
|    4622 | 3005 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2310 | 3006 | `		}` |
|    9266 | 3007 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3008 | `		/* Synchronize cursors */` |
|    9266 | 3009 | `		pToken = pGen->pIn;` |
|       - | 3010 | `		/* Fix the false jump */` |
|    9266 | 3011 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3012 | `	} /* For(;;) */` |
|       - | 3013 | `	/* Fix the false jump */` |
|   93026 | 3014 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|   93026 | 3015 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   37930 | 3016 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3017 | `			/* Compile the else block */` |
|    7652 | 3018 | `			pGen->pIn++;` |
|    7652 | 3019 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    7652 | 3020 | `			if( rc == SXERR_ABORT ){` |
|       - | 3021 |  |
|     ! 0 | 3022 | `				return SXERR_ABORT;` |
|       - | 3023 | `			}` |
|    3825 | 3024 | `	}` |
|   93026 | 3025 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3026 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|   93026 | 3027 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3028 | `	/* Release the conditional block */` |
|   93026 | 3029 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3030 | `	/* Statement successfully compiled */` |
|   93026 | 3031 | `	return SXRET_OK;` |
|     ! 0 | 3032 | `Synchronize:` |
|       - | 3033 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3034 | `	 */` |
|     ! 0 | 3035 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3036 | `		pGen->pIn++;` |
|     ! 0 | 3037 | `	}` |
|     ! 0 | 3038 | `	return SXRET_OK;` |
|   46514 | 3039 |  |
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
|   97430 | 3133 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3134 |  |
|   97432 | 3135 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3136 | `	sxi32 rc;` |
|       - | 3137 | `	/* Jump the 'return' keyword */` |
|   97432 | 3138 | `	pGen->pIn++;` |
|   97432 | 3139 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3140 | `		/* Compile the expression */` |
|   97410 | 3141 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   97410 | 3142 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3143 | `			return SXERR_ABORT;` |
|   97410 | 3144 | `		}else if(rc != SXERR_EMPTY ){` |
|   97410 | 3145 | `			nRet = 1;` |
|   48704 | 3146 | `		}` |
|   48704 | 3147 | `	}` |
|       - | 3148 | `	/* Emit the done instruction */` |
|   97432 | 3149 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|   97432 | 3150 | `	return SXRET_OK;` |
|   48717 | 3151 |  |
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
|    9348 | 3179 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3180 |  |
|    9350 | 3181 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3182 | `	sxi32 rc;` |
|       - | 3183 | `	/* Jump the 'echo' keyword */` |
|    9350 | 3184 | `	pGen->pIn++;` |
|       - | 3185 | `	/* Compile arguments one after one */` |
|    9350 | 3186 | `	pTmp = pGen->pEnd;` |
|   18938 | 3187 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    9590 | 3188 | `		if( pGen->pIn < pNext ){` |
|    9590 | 3189 | `			pGen->pEnd = pNext;` |
|    9590 | 3190 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    9590 | 3191 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3192 | `				return SXERR_ABORT;` |
|    9590 | 3193 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3194 | `				/* Emit the consume instruction */` |
|    9566 | 3195 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4782 | 3196 | `			}` |
|    4794 | 3197 | `		}` |
|       - | 3198 | `		/* Jump trailing commas */` |
|    9830 | 3199 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     242 | 3200 | `			pNext++;` |
|       2 | 3201 | `		}` |
|    9590 | 3202 | `		pGen->pIn = pNext;` |
|       2 | 3203 | `	}` |
|       - | 3204 | `	/* Restore token stream */` |
|    9350 | 3205 | `	pGen->pEnd = pTmp;` |
|    9350 | 3206 | `	return SXRET_OK;` |
|    4676 | 3207 |  |
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
|  232506 | 3369 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
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
|  232508 | 3380 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  232402 | 3381 | `		return nOrigIdx; /* Not in a namespace */` |
|       - | 3382 | `	}` |
|     107 | 3383 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|     107 | 3384 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3385 | `		return nOrigIdx;` |
|       - | 3386 | `	}` |
|     107 | 3387 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|     107 | 3388 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3389 | `	/* Skip if already qualified (contains backslash) */` |
|     107 | 3390 | `	hasNsSep = 0;` |
|     521 | 3391 | `	for( k = 0; k < nLit; k++ ){` |
|     465 | 3392 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
|     208 | 3393 | `	}` |
|     107 | 3394 | `	if( hasNsSep ){` |
|      51 | 3395 | `		return nOrigIdx;` |
|       - | 3396 | `	}` |
|       - | 3397 | `	/* Build the qualified name into sWorker */` |
|      57 | 3398 | `	SyBlobReset(&pGen->sWorker);` |
|       - | 3399 | `	/* Check use imports first */` |
|      57 | 3400 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zLit,nLit);` |
|      57 | 3401 | `	if( pImport ){` |
|      15 | 3402 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 | 3403 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       8 | 3404 | `	}else{` |
|       - | 3405 | `		/* Prepend current namespace */` |
|      43 | 3406 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      43 | 3407 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      43 | 3408 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3409 | `	}` |
|       - | 3410 | `	/* Look up or create a new literal for the qualified name */` |
|      57 | 3411 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      57 | 3412 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      17 | 3413 | `		return nNewIdx; /* Already interned */` |
|       - | 3414 | `	}` |
|      41 | 3415 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      41 | 3416 | `	if( pNew == 0 ){` |
|     ! 0 | 3417 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3418 | `	}` |
|      41 | 3419 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      41 | 3420 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      41 | 3421 | `	return nNewIdx;` |
|  116255 | 3422 |  |
|       - | 3423 | `/*` |
|       - | 3424 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3425 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3426 | ` */` |
|   13914 | 3427 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3428 |  |
|       - | 3429 | `	SyHashEntry *pImport;` |
|       - | 3430 | `	/* Check use imports first */` |
|   13916 | 3431 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   13916 | 3432 | `	if( pImport ){` |
|       7 | 3433 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3434 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3435 | `		return;` |
|       - | 3436 | `	}` |
|       - | 3437 | `	/* Prepend current namespace if active */` |
|   13910 | 3438 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3439 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3440 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3441 | `	}` |
|   13910 | 3442 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    6959 | 3443 |  |
|       - | 3444 | `/*` |
|       - | 3445 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3446 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3447 | ` * The caller must release pOut when done.` |
|       - | 3448 | ` */` |
|   28026 | 3449 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3450 |  |
|   28028 | 3451 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3452 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3453 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3454 | `	}` |
|   28028 | 3455 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   28028 | 3456 |  |
|       - | 3457 | `/*` |
|       - | 3458 | ` * Compile a namespace statement` |
|       - | 3459 | ` * According to the PHP language reference manual` |
|       - | 3460 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3461 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3462 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3463 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3464 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3465 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3466 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3467 | ` *  programming world.` |
|       - | 3468 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3469 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3470 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3471 | ` *  classes/functions/constants.` |
|       - | 3472 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3473 | ` *  readability of source code.` |
|       - | 3474 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3475 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3476 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3477 | ` *       class MyClass {}` |
|       - | 3478 | ` *       function myfunction() {}` |
|       - | 3479 | ` *       const MYCONST = 1;` |
|       - | 3480 | ` *       $a = new MyClass;` |
|       - | 3481 | ` *       $c = new \my\name\MyClass;` |
|       - | 3482 | ` *       $a = strlen('hi');` |
|       - | 3483 | ` *       $d = namespace\MYCONST;` |
|       - | 3484 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3485 | ` *       echo constant($d);` |
|       - | 3486 | ` * NOTE` |
|       - | 3487 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3488 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3489 | ` */` |
|       - | 3490 | `/*` |
|       - | 3491 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3492 | ` */` |
|       6 | 3493 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3494 |  |
|       7 | 3495 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|     ! 0 | 3496 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|     ! 0 | 3497 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|     ! 0 | 3498 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|     ! 0 | 3499 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|     ! 0 | 3500 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|     ! 0 | 3501 | `	return "token";` |
|       4 | 3502 |  |
|      50 | 3503 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3504 |  |
|       - | 3505 | `	sxu32 nLine;` |
|       - | 3506 | `	sxi32 rc;` |
|      51 | 3507 | `	nLine = pGen->pIn->nLine;` |
|      51 | 3508 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3509 | `	/* Reset namespace and clear previous use imports */` |
|      51 | 3510 | `	SyBlobReset(&pGen->sNamespace);` |
|      51 | 3511 | `	SyHashRelease(&pGen->hUseImports);` |
|      51 | 3512 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      51 | 3513 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3514 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3515 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3516 | `		return SXRET_OK;` |
|       - | 3517 | `	}` |
|      51 | 3518 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3519 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3520 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3521 | `		return SXRET_OK;` |
|       - | 3522 | `	}` |
|      51 | 3523 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3524 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3525 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3526 | `		return SXRET_OK;` |
|       - | 3527 | `	}` |
|       - | 3528 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     131 | 3529 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      81 | 3530 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3531 | `			/* Append backslash separator */` |
|      17 | 3532 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3533 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3534 | `			}` |
|       9 | 3535 | `		}else{` |
|       - | 3536 | `			/* Append identifier */` |
|      65 | 3537 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3538 | `		}` |
|      81 | 3539 | `		pGen->pIn++;` |
|       1 | 3540 | `	}` |
|       - | 3541 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3542 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3543 | `	{` |
|      51 | 3544 | `		char *zNsDup = 0;` |
|      51 | 3545 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      73 | 3546 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      48 | 3547 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      24 | 3548 | `		}` |
|      51 | 3549 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3550 | `	}` |
|      51 | 3551 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3552 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3553 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3554 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3555 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3556 | `			return SXERR_ABORT;` |
|       - | 3557 | `		}` |
|       2 | 3558 | `	}` |
|      51 | 3559 | `	return SXRET_OK;` |
|      26 | 3560 |  |
|       - | 3561 | `/*` |
|       - | 3562 | ` * Compile the 'use' statement` |
|       - | 3563 | ` * According to the PHP language reference manual` |
|       - | 3564 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3565 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3566 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3567 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3568 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3569 | ` *  a function or constant is not supported.` |
|       - | 3570 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3571 | ` * NOTE` |
|       - | 3572 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3573 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3574 | ` */` |
|      22 | 3575 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3576 |  |
|       - | 3577 | `	sxu32 nLine;` |
|       - | 3578 | `	sxi32 rc;` |
|       - | 3579 | `	SyBlob sPath;` |
|       - | 3580 | `	SyString sAlias;` |
|       - | 3581 | `	SyToken *pLast;` |
|       - | 3582 | `	char *zDup;` |
|      23 | 3583 | `	nLine = pGen->pIn->nLine;` |
|      23 | 3584 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      23 | 3585 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3586 | `	/* Process one or more use declarations separated by commas */` |
|      12 | 3587 | `	for(;;){` |
|      25 | 3588 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3589 | `			break;` |
|       - | 3590 | `		}` |
|      25 | 3591 | `		SyBlobReset(&sPath);` |
|      25 | 3592 | `		pLast = 0;` |
|       - | 3593 | `		/* Collect the full namespace path */` |
|     101 | 3594 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      77 | 3595 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      49 | 3596 | `				pLast = pGen->pIn;` |
|      49 | 3597 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      29 | 3598 | `					SyBlobAppend(&sPath,"\\",1);` |
|      14 | 3599 | `				}` |
|      49 | 3600 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      24 | 3601 | `			}` |
|      77 | 3602 | `			pGen->pIn++;` |
|       1 | 3603 | `		}` |
|      25 | 3604 | `		if( pLast == 0 ){` |
|       - | 3605 | `			/* Empty path */` |
|       5 | 3606 | `			break;` |
|       - | 3607 | `		}` |
|       - | 3608 | `		/* Default alias is the last component of the path */` |
|      21 | 3609 | `		sAlias = pLast->sData;` |
|       - | 3610 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      20 | 3611 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      13 | 3612 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       5 | 3613 | `			pGen->pIn++; /* Jump 'as' */` |
|       5 | 3614 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       5 | 3615 | `				sAlias = pGen->pIn->sData;` |
|       5 | 3616 | `				pGen->pIn++;` |
|       2 | 3617 | `			}` |
|       2 | 3618 | `		}` |
|       - | 3619 | `		/* Register the import: alias -> FQN.` |
|       - | 3620 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3621 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3622 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      31 | 3623 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      20 | 3624 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      21 | 3625 | `		if( zDup ){` |
|       - | 3626 | `			char *zAliasDup;` |
|      21 | 3627 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3628 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      21 | 3629 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      21 | 3630 | `			if( zAliasDup ){` |
|      21 | 3631 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      10 | 3632 | `			}` |
|      10 | 3633 | `		}` |
|       - | 3634 | `		/* Check for comma (multiple use declarations) */` |
|      21 | 3635 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3636 | `			pGen->pIn++;` |
|       2 | 3637 | `		}else{` |
|      10 | 3638 | `			break;` |
|       - | 3639 | `		}` |
|       1 | 3640 | `	}` |
|      23 | 3641 | `	SyBlobRelease(&sPath);` |
|      23 | 3642 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3643 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3644 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3645 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3646 | `			return SXERR_ABORT;` |
|       - | 3647 | `		}` |
|       1 | 3648 | `	}` |
|      23 | 3649 | `	return SXRET_OK;` |
|      12 | 3650 |  |
|       - | 3651 | `/*` |
|       - | 3652 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3653 | ` *` |
|       - | 3654 | ` * According to the PHP language reference manual.` |
|       - | 3655 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3656 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3657 | ` *  declare (directive)` |
|       - | 3658 | ` *   statement` |
|       - | 3659 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3660 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3661 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3662 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3663 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3664 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3665 | ` * <?php` |
|       - | 3666 | ` * // these are the same:` |
|       - | 3667 | ` * // you can use this:` |
|       - | 3668 | ` * declare(ticks=1) {` |
|       - | 3669 | ` *   // entire script here` |
|       - | 3670 | ` * }` |
|       - | 3671 | ` * // or you can use this:` |
|       - | 3672 | ` * declare(ticks=1);` |
|       - | 3673 | ` * // entire script here` |
|       - | 3674 | ` * ?>` |
|       - | 3675 | ` *` |
|       - | 3676 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3677 | ` */` |
|       8 | 3678 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3679 |  |
|       9 | 3680 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3681 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3682 | `	sxi32 rc;` |
|       9 | 3683 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3684 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3686 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3687 | `			return SXERR_ABORT;` |
|       - | 3688 | `		}` |
|       5 | 3689 | `		goto Synchro;` |
|       - | 3690 | `	}` |
|       5 | 3691 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3692 | `	/* Delimit the directive */` |
|       5 | 3693 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3694 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3695 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3696 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3697 | `			return SXERR_ABORT;` |
|       - | 3698 | `		}` |
|     ! 0 | 3699 | `		return SXRET_OK;` |
|       - | 3700 | `	}` |
|       - | 3701 | `	/* Update the cursor */` |
|       5 | 3702 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3703 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3704 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3705 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3706 | `			return SXERR_ABORT;` |
|       - | 3707 | `		}` |
|     ! 0 | 3708 | `	}` |
|       - | 3709 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3710 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3711 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3712 | `		ph7_lib_version()` |
|       - | 3713 | `		);` |
|       - | 3714 | `	/*All done */` |
|       5 | 3715 | `	return SXRET_OK;` |
|       2 | 3716 | `Synchro:` |
|       - | 3717 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3718 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3719 | `		pGen->pIn++;` |
|       1 | 3720 | `	}` |
|       5 | 3721 | `	return SXRET_OK;` |
|       5 | 3722 |  |
|       - | 3723 | `/*` |
|       - | 3724 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3725 | ` * as follows:` |
|       - | 3726 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3727 | ` * {` |
|       - | 3728 | ` *   return "Making a cup of $type.\n";` |
|       - | 3729 | ` * }` |
|       - | 3730 | ` * Symisc eXtension.` |
|       - | 3731 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3732 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3733 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3734 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3735 | ` *      {` |
|       - | 3736 | ` *       var_dump($a);` |
|       - | 3737 | ` *      }` |
|       - | 3738 | ` *     //call test without args` |
|       - | 3739 | ` *      test();` |
|       - | 3740 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3741 | ` *      Example:` |
|       - | 3742 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3743 | ` * 3 -) Function overloading!!` |
|       - | 3744 | ` *      Example:` |
|       - | 3745 | ` *      function foo($a) {` |
|       - | 3746 | ` *   	  return $a.PHP_EOL;` |
|       - | 3747 | ` *	    }` |
|       - | 3748 | ` *	    function foo($a, $b) {` |
|       - | 3749 | ` *   	  return $a + $b;` |
|       - | 3750 | ` *	    }` |
|       - | 3751 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3752 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3753 | ` *      // Same arg` |
|       - | 3754 | ` *	   function foo(string $a)` |
|       - | 3755 | ` *	   {` |
|       - | 3756 | ` *	     echo "a is a string\n";` |
|       - | 3757 | ` *	     var_dump($a);` |
|       - | 3758 | ` *	   }` |
|       - | 3759 | ` *	  function foo(int $a)` |
|       - | 3760 | ` *	  {` |
|       - | 3761 | ` *	    echo "a is integer\n";` |
|       - | 3762 | ` *	    var_dump($a);` |
|       - | 3763 | ` *	  }` |
|       - | 3764 | ` *	  function foo(array $a)` |
|       - | 3765 | ` *	  {` |
|       - | 3766 | ` * 	    echo "a is an array\n";` |
|       - | 3767 | ` * 	    var_dump($a);` |
|       - | 3768 | ` *	  }` |
|       - | 3769 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3770 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3771 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3772 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3773 | ` * introduced by the PH7 engine.` |
|       - | 3774 | ` */` |
|   30038 | 3775 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3776 |  |
|       - | 3777 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3778 | `	SySet *pInstrContainer;` |
|       - | 3779 | `	sxi32 rc;` |
|       - | 3780 | `	/* Swap token stream */` |
|   30040 | 3781 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   30040 | 3782 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   30040 | 3783 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3784 | `	/* Compile the expression holding the argument value */` |
|   30040 | 3785 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3786 | `	/* Emit the done instruction */` |
|   30040 | 3787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   30040 | 3788 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   30040 | 3789 | `	RE_SWAP_DELIMITER(pGen);` |
|   30040 | 3790 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3791 | `		return SXERR_ABORT;` |
|       - | 3792 | `	}` |
|   30040 | 3793 | `	return SXRET_OK;` |
|   15021 | 3794 |  |
|       - | 3795 | `/*` |
|       - | 3796 | ` * Collect function arguments one after one.` |
|       - | 3797 | ` * According to the PHP language reference manual.` |
|       - | 3798 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3799 | ` * list of expressions.` |
|       - | 3800 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3801 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3802 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3803 | ` * for more information.` |
|       - | 3804 | ` * Example #1 Passing arrays to functions` |
|       - | 3805 | ` * <?php` |
|       - | 3806 | ` * function takes_array($input)` |
|       - | 3807 | ` * {` |
|       - | 3808 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3809 | ` * }` |
|       - | 3810 | ` * ?>` |
|       - | 3811 | ` * Making arguments be passed by reference` |
|       - | 3812 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3813 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3814 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3815 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3816 | ` * to the argument name in the function definition:` |
|       - | 3817 | ` * Example #2 Passing function parameters by reference` |
|       - | 3818 | ` * <?php` |
|       - | 3819 | ` * function add_some_extra(&$string)` |
|       - | 3820 | ` * {` |
|       - | 3821 | ` *   $string .= 'and something extra.';` |
|       - | 3822 | ` * }` |
|       - | 3823 | ` * $str = 'This is a string, ';` |
|       - | 3824 | ` * add_some_extra($str);` |
|       - | 3825 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3826 | ` * ?>` |
|       - | 3827 | ` *` |
|       - | 3828 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3829 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3830 | ` * on these extension.` |
|       - | 3831 | ` */` |
|   32780 | 3832 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3833 |  |
|       - | 3834 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3835 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3836 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3837 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3838 | `	sxi32 rc;` |
|       - | 3839 |  |
|   32782 | 3840 | `	pIn = pGen->pIn;` |
|   32782 | 3841 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3842 | `	/* Process arguments one after one */` |
|   44561 | 3843 | `	for(;;){` |
|   89124 | 3844 | `		if( pIn >= pEnd ){` |
|       - | 3845 | `			/* No more arguments to process */` |
|   32780 | 3846 | `			break;` |
|       - | 3847 | `		}` |
|   56346 | 3848 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   56346 | 3849 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   56346 | 3850 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   46204 | 3851 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   41584 | 3852 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   41584 | 3853 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3854 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   41584 | 3855 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3856 | `					sArg.nType = MEMOBJ_BOOL;` |
|   41584 | 3857 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   11552 | 3858 | `					sArg.nType = MEMOBJ_INT;` |
|   35809 | 3859 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   30032 | 3860 | `					sArg.nType = MEMOBJ_STRING;` |
|   15018 | 3861 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3862 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3863 | `				}else{` |
|       4 | 3864 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3865 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3866 | `						&pIn->sData);` |
|       - | 3867 | `				}` |
|   20793 | 3868 | `			}else{` |
|    4622 | 3869 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3870 | `				char *zDupLocal;` |
|       - | 3871 | `				/* Argument must be a class instance,record that*/` |
|    4622 | 3872 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    4622 | 3873 | `				if( zDupLocal ){` |
|    4622 | 3874 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    4622 | 3875 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2310 | 3876 | `				}` |
|       - | 3877 | `			}` |
|   46204 | 3878 | `			pIn++;` |
|   23101 | 3879 | `		}` |
|   56346 | 3880 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3881 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3882 | `			return rc;` |
|       - | 3883 | `		}` |
|   56346 | 3884 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3885 | `			/* Pass by reference,record that */` |
|    2332 | 3886 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2332 | 3887 | `			pIn++;` |
|    1165 | 3888 | `		}` |
|   56346 | 3889 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3890 | `			/* Invalid argument */` |
|     ! 0 | 3891 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3892 | `			return rc;` |
|       - | 3893 | `		}` |
|   56346 | 3894 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3895 | `		/* Copy argument name */` |
|   56346 | 3896 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   56346 | 3897 | `		if( zDup == 0 ){` |
|     ! 0 | 3898 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3899 | `			return SXERR_ABORT;` |
|       - | 3900 | `		}` |
|   56346 | 3901 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   56346 | 3902 | `		pIn++;` |
|   56346 | 3903 | `		if( pIn < pEnd ){` |
|   35128 | 3904 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3905 | `				SyToken *pDefend;` |
|   30042 | 3906 | `				sxi32 iNest = 0;` |
|   30042 | 3907 | `				pIn++; /* Jump the equal sign */` |
|   30042 | 3908 | `				pDefend = pIn;` |
|       - | 3909 | `				/* Process the default value associated with this argument */` |
|   64700 | 3910 | `				while( pDefend < pEnd ){` |
|   53140 | 3911 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   18482 | 3912 | `						break;` |
|       - | 3913 | `					}` |
|   34660 | 3914 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 3915 | `						/* Increment nesting level */` |
|    2312 | 3916 | `						iNest++;` |
|   33505 | 3917 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 3918 | `						/* Decrement nesting level */` |
|    2312 | 3919 | `						iNest--;` |
|    1155 | 3920 | `					}` |
|   34660 | 3921 | `					pDefend++;` |
|       2 | 3922 | `				}` |
|   30042 | 3923 | `				if( pIn >= pDefend ){` |
|       3 | 3924 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 3925 | `					return rc;` |
|       - | 3926 | `				}` |
|       - | 3927 | `				/* Process default value */` |
|   30040 | 3928 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   30040 | 3929 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3930 | `					return rc;` |
|       - | 3931 | `				}` |
|       - | 3932 | `				/* Point beyond the default value */` |
|   30040 | 3933 | `				pIn = pDefend;` |
|   15019 | 3934 | `			}` |
|   35126 | 3935 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3936 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 3937 | `				return rc;` |
|       - | 3938 | `			}` |
|   35126 | 3939 | `			pIn++; /* Jump the trailing comma */` |
|   17562 | 3940 | `		}` |
|       - | 3941 | `		/* Append argument signature */` |
|   56344 | 3942 | `		if( sArg.nType > 0 ){` |
|   46202 | 3943 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 3944 | `				/* Class name */` |
|    4622 | 3945 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2312 | 3946 | `			}else{` |
|       - | 3947 | `				int c;` |
|   41582 | 3948 | `				c = 'n'; /* cc warning */` |
|       - | 3949 | `				/* Type leading character */` |
|   41582 | 3950 | `				switch(sArg.nType){` |
|     ! 0 | 3951 | `				case MEMOBJ_HASHMAP:` |
|       - | 3952 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 3953 | `					c = 'h';` |
|     ! 0 | 3954 | `					break;` |
|    5775 | 3955 | `				case MEMOBJ_INT:` |
|       - | 3956 | `					/* Integer */` |
|   11552 | 3957 | `					c = 'i';` |
|   11552 | 3958 | `					break;` |
|     ! 0 | 3959 | `				case MEMOBJ_BOOL:` |
|       - | 3960 | `					/* Bool */` |
|     ! 0 | 3961 | `					c = 'b';` |
|     ! 0 | 3962 | `					break;` |
|     ! 0 | 3963 | `				case MEMOBJ_REAL:` |
|       - | 3964 | `					/* Float */` |
|     ! 0 | 3965 | `					c = 'f';` |
|     ! 0 | 3966 | `					break;` |
|   15015 | 3967 | `				case MEMOBJ_STRING:` |
|       - | 3968 | `					/* String */` |
|   30032 | 3969 | `					c = 's';` |
|   30030 | 3970 | `					break;` |
|     ! 0 | 3971 | `				default:` |
|     ! 0 | 3972 | `					break;` |
|       - | 3973 | `				}` |
|   41582 | 3974 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 3975 | `			}` |
|   23102 | 3976 | `		}else{` |
|       - | 3977 | `			/* No type is associated with this parameter which mean` |
|       - | 3978 | `			 * that this function is not condidate for overloading.` |
|       - | 3979 | `			 */` |
|   10144 | 3980 | `			SyBlobRelease(&sSig);` |
|       - | 3981 | `		}` |
|       - | 3982 | `		/* Save in the argument set */` |
|   56344 | 3983 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 3984 | `	}` |
|   32780 | 3985 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 3986 | `		/* Save function signature */` |
|   27722 | 3987 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   13860 | 3988 | `	}` |
|   32780 | 3989 | `	return SXRET_OK;` |
|   16392 | 3990 |  |
|       - | 3991 | `/*` |
|       - | 3992 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 3993 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3994 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 3995 | ` */` |
|   79276 | 3996 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 3997 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 3998 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 3999 | `	)` |
|       2 | 4000 |  |
|       - | 4001 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4002 | `	GenBlock *pBlock;` |
|       - | 4003 | `	sxu32 nGotoOfft;` |
|       - | 4004 | `	sxi32 rc;` |
|       - | 4005 | `	/* Attach the new function */` |
|   79278 | 4006 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   79278 | 4007 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4008 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4009 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4010 | `		return SXERR_ABORT;` |
|       - | 4011 | `	}` |
|   79278 | 4012 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4013 | `	/* Swap bytecode containers */` |
|   79278 | 4014 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   79278 | 4015 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4016 | `	/* Compile the body */` |
|   79278 | 4017 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4018 | `	/* Fix exception jumps now the destination is resolved */` |
|   79278 | 4019 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4020 | `	/* Emit the final return if not yet done */` |
|   79278 | 4021 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4022 | `	/* Fix gotos jumps now the destination is resolved */` |
|   79278 | 4023 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4024 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4025 | `	}` |
|   79278 | 4026 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4027 | `	/* Restore the default container */` |
|   79278 | 4028 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4029 | `	/* Leave function block */` |
|   79278 | 4030 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   79278 | 4031 | `	if( rc == SXERR_ABORT ){` |
|       - | 4032 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4033 | `		return SXERR_ABORT;` |
|       - | 4034 | `	}` |
|       - | 4035 | `	/* All done, function body compiled */` |
|   79278 | 4036 | `	return SXRET_OK;` |
|   39640 | 4037 |  |
|       - | 4038 | `/*` |
|       - | 4039 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4040 | ` * According to the PHP language reference manual.` |
|       - | 4041 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4042 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4043 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4044 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4045 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4046 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4047 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4048 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4049 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4050 | ` *` |
|       - | 4051 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4052 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4053 | ` * on these extension.` |
|       - | 4054 | ` */` |
|   30524 | 4055 | `static sxi32 GenStateCompileFunc(` |
|       - | 4056 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4057 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4058 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4059 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4060 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4061 | `	)` |
|       2 | 4062 |  |
|       - | 4063 | `	ph7_vm_func *pFunc;` |
|       - | 4064 | `	SyToken *pEnd;` |
|       - | 4065 | `	sxu32 nLine;` |
|       - | 4066 | `	char *zName;` |
|       - | 4067 | `	sxi32 rc;` |
|       - | 4068 | `	/* Extract line number */` |
|   30526 | 4069 | `	nLine = pGen->pIn->nLine;` |
|       - | 4070 | `	/* Jump the left parenthesis '(' */` |
|   30526 | 4071 | `	pGen->pIn++;` |
|       - | 4072 | `	/* Delimit the function signature */` |
|   30526 | 4073 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   30526 | 4074 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4075 | `		/* Syntax error */` |
|       7 | 4076 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4077 | `		if( rc == SXERR_ABORT ){` |
|       - | 4078 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4079 | `			return SXERR_ABORT;` |
|       - | 4080 | `		}` |
|       7 | 4081 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4082 | `		return SXRET_OK;` |
|       - | 4083 | `	}` |
|       - | 4084 | `	/* Create the function state */` |
|   30520 | 4085 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   30520 | 4086 | `	if( pFunc == 0 ){` |
|     ! 0 | 4087 | `		goto OutOfMem;` |
|       - | 4088 | `	}` |
|       - | 4089 | `	/* Build the function name, prepending namespace if active */` |
|   30524 | 4090 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4091 | `		SyBlob sFQN;` |
|       - | 4092 | `		sxu32 nLen;` |
|       9 | 4093 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4094 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4095 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4096 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4097 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4098 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4099 | `		SyBlobRelease(&sFQN);` |
|       9 | 4100 | `		if( zName == 0 ){` |
|     ! 0 | 4101 | `			goto OutOfMem;` |
|       - | 4102 | `		}` |
|       9 | 4103 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4104 | `	}else{` |
|   30512 | 4105 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   30512 | 4106 | `		if( zName == 0 ){` |
|     ! 0 | 4107 | `			goto OutOfMem;` |
|       - | 4108 | `		}` |
|   30512 | 4109 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4110 | `	}` |
|   30520 | 4111 | `	if( pGen->pIn < pEnd ){` |
|       - | 4112 | `		/* Collect function arguments */` |
|   21184 | 4113 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   21184 | 4114 | `		if( rc == SXERR_ABORT ){` |
|       - | 4115 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4116 | `			return SXERR_ABORT;` |
|       - | 4117 | `		}` |
|   10591 | 4118 | `	}` |
|       - | 4119 | `	/* Compile function body */` |
|   30520 | 4120 | `	pGen->pIn = &pEnd[1];` |
|   30520 | 4121 | `	if( bHandleClosure ){` |
|       - | 4122 | `		ph7_vm_func_closure_env sEnv;` |
|     130 | 4123 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     128 | 4124 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      70 | 4125 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 4126 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4127 | `				/* Closure,record environment variable */` |
|      10 | 4128 | `				pGen->pIn++;` |
|      10 | 4129 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4130 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4131 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4132 | `						return SXERR_ABORT;` |
|       - | 4133 | `					}` |
|     ! 0 | 4134 | `				}` |
|      10 | 4135 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4136 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 4137 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 4138 | `					int iFlagsLocal = 0;` |
|      18 | 4139 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 4140 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 4141 | `						break;` |
|       - | 4142 | `					}` |
|      10 | 4143 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 4144 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4145 | `						/* Pass by reference,record that */` |
|     ! 0 | 4146 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4147 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4148 | `							);` |
|     ! 0 | 4149 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4150 | `						pGen->pIn++;` |
|     ! 0 | 4151 | `					}` |
|       8 | 4152 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 4153 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4154 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4155 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4156 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4157 | `								return SXERR_ABORT;` |
|       - | 4158 | `							}` |
|       - | 4159 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4160 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4161 | `								pGen->pIn++;` |
|     ! 0 | 4162 | `							}` |
|     ! 0 | 4163 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4164 | `								pGen->pIn++;` |
|     ! 0 | 4165 | `							}` |
|     ! 0 | 4166 | `							break;` |
|       - | 4167 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4168 | `					}else{` |
|       - | 4169 | `						SyString *pNameLocal;` |
|       - | 4170 | `						char *zDup;` |
|       - | 4171 | `						/* Duplicate variable name */` |
|      10 | 4172 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 4173 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 4174 | `						if( zDup ){` |
|       - | 4175 | `							/* Zero the structure */` |
|      10 | 4176 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4177 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 4178 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4179 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 4180 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4181 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4182 | `									got_this = 1;` |
|     ! 0 | 4183 | `							}` |
|       - | 4184 | `							/* Save imported variable */` |
|      10 | 4185 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4186 | `						}else{` |
|     ! 0 | 4187 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4188 | `							 return SXERR_ABORT;` |
|       - | 4189 | `						}` |
|       - | 4190 | `					}` |
|      10 | 4191 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 4192 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4193 | `						/* Ignore trailing commas */` |
|     ! 0 | 4194 | `						pGen->pIn++;` |
|     ! 0 | 4195 | `					}` |
|       2 | 4196 | `				}` |
|      10 | 4197 | `				if( !got_this ){` |
|       - | 4198 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4199 | `					 * available to the closure environment.` |
|       - | 4200 | `					 */` |
|      10 | 4201 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4202 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 4203 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4204 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 4205 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 4206 | `				}` |
|      10 | 4207 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4208 | `					/* Mark as closure */` |
|      10 | 4209 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 4210 | `				}` |
|       4 | 4211 | `		}` |
|      64 | 4212 | `	}` |
|       - | 4213 | `	/* Compile the body */` |
|   30520 | 4214 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   30520 | 4215 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4216 | `		return SXERR_ABORT;` |
|       - | 4217 | `	}` |
|   30520 | 4218 | `	if( ppFunc ){` |
|     130 | 4219 | `		*ppFunc = pFunc;` |
|      64 | 4220 | `	}` |
|   30520 | 4221 | `	rc = SXRET_OK;` |
|   30520 | 4222 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4223 | `		/* Finally register the function */` |
|   30512 | 4224 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   15255 | 4225 | `	}` |
|   30520 | 4226 | `	if( rc == SXRET_OK ){` |
|   30520 | 4227 | `		return SXRET_OK;` |
|       - | 4228 | `	}` |
|       - | 4229 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4230 | `OutOfMem:` |
|       - | 4231 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4232 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4233 | `	 */` |
|     ! 0 | 4234 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4235 | `	return SXERR_ABORT;` |
|   15264 | 4236 |  |
|       - | 4237 | `/*` |
|       - | 4238 | ` * Compile a standard PHP function.` |
|       - | 4239 | ` *  Refer to the block-comment above for more information.` |
|       - | 4240 | ` */` |
|   30402 | 4241 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4242 |  |
|       - | 4243 | `	SyString *pName;` |
|       - | 4244 | `	sxi32 iFlags;` |
|       - | 4245 | `	sxu32 nLine;` |
|       - | 4246 | `	sxi32 rc;` |
|       - | 4247 |  |
|   30404 | 4248 | `	nLine = pGen->pIn->nLine;` |
|   30404 | 4249 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   30404 | 4250 | `	iFlags = 0;` |
|   30404 | 4251 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4252 | `		/* Return by reference,remember that */` |
|       7 | 4253 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4254 | `		/* Jump the '&' token */` |
|       7 | 4255 | `		pGen->pIn++;` |
|       3 | 4256 | `	}` |
|   30404 | 4257 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4258 | `		/* Invalid function name */` |
|       5 | 4259 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4260 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4261 | `			return SXERR_ABORT;` |
|       - | 4262 | `		}` |
|       - | 4263 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4264 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4265 | `			pGen->pIn++;` |
|       1 | 4266 | `		}` |
|       5 | 4267 | `		return SXRET_OK;` |
|       - | 4268 | `	}` |
|   30400 | 4269 | `	pName = &pGen->pIn->sData;` |
|   30400 | 4270 | `	nLine = pGen->pIn->nLine;` |
|       - | 4271 | `	/* Jump the function name */` |
|   30400 | 4272 | `	pGen->pIn++;` |
|   30400 | 4273 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4274 | `		/* Syntax error */` |
|       3 | 4275 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4276 | `		if( rc == SXERR_ABORT ){` |
|       - | 4277 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4278 | `			return SXERR_ABORT;` |
|       - | 4279 | `		}` |
|       - | 4280 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4281 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4282 | `			pGen->pIn++;` |
|     ! 0 | 4283 | `		}` |
|       3 | 4284 | `		return SXRET_OK;` |
|       - | 4285 | `	}` |
|       - | 4286 | `	/* Compile function body */` |
|   30398 | 4287 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   30398 | 4288 | `	return rc;` |
|   15203 | 4289 |  |
|       - | 4290 | `/*` |
|       - | 4291 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4292 | ` * According to the PHP language reference manual` |
|       - | 4293 | ` *  Visibility:` |
|       - | 4294 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4295 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4296 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4297 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4298 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4299 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4300 | ` */` |
|   90538 | 4301 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4302 |  |
|   90540 | 4303 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      40 | 4304 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   90502 | 4305 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   16198 | 4306 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4307 | `	}` |
|       - | 4308 | `	/* Assume public by default */` |
|   74306 | 4309 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   45271 | 4310 |  |
|       - | 4311 | `/*` |
|       - | 4312 | ` * Compile a class constant.` |
|       - | 4313 | ` * According to the PHP language reference manual` |
|       - | 4314 | ` *  Class Constants` |
|       - | 4315 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4316 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4317 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4318 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4319 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4320 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4321 | ` * Symisc eXtension.` |
|       - | 4322 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4323 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4324 | ` *  Example:` |
|       - | 4325 | ` *   class Test{` |
|       - | 4326 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4327 | ` *   };` |
|       - | 4328 | ` *   var_dump(TEST::MyConst);` |
|       - | 4329 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4330 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4331 | ` */` |
|      10 | 4332 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4333 |  |
|      12 | 4334 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4335 | `	SySet *pInstrContainer;` |
|       - | 4336 | `	ph7_class_attr *pCons;` |
|       - | 4337 | `	SyString *pName;` |
|       - | 4338 | `	sxi32 rc;` |
|       - | 4339 | `	/* Extract visibility level */` |
|      12 | 4340 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4341 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4342 | `loop:` |
|       - | 4343 | `	/* Mark as constant */` |
|      12 | 4344 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4345 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4346 | `		/* Invalid constant name */` |
|     ! 0 | 4347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4348 | `		if( rc == SXERR_ABORT ){` |
|       - | 4349 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4350 | `			return SXERR_ABORT;` |
|       - | 4351 | `		}` |
|     ! 0 | 4352 | `		goto Synchronize;` |
|       - | 4353 | `	}` |
|       - | 4354 | `	/* Peek constant name */` |
|      12 | 4355 | `	pName = &pGen->pIn->sData;` |
|       - | 4356 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4357 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4358 | `		/* Reserved constant name */` |
|     ! 0 | 4359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4360 | `		if( rc == SXERR_ABORT ){` |
|       - | 4361 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4362 | `			return SXERR_ABORT;` |
|       - | 4363 | `		}` |
|     ! 0 | 4364 | `		goto Synchronize;` |
|       - | 4365 | `	}` |
|       - | 4366 | `	/* Advance the stream cursor */` |
|      12 | 4367 | `	pGen->pIn++;` |
|      12 | 4368 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4369 | `		/* Invalid declaration */` |
|     ! 0 | 4370 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4371 | `		if( rc == SXERR_ABORT ){` |
|       - | 4372 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4373 | `			return SXERR_ABORT;` |
|       - | 4374 | `		}` |
|     ! 0 | 4375 | `		goto Synchronize;` |
|       - | 4376 | `	}` |
|      12 | 4377 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4378 | `	/* Allocate a new class attribute */` |
|      12 | 4379 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4380 | `	if( pCons == 0 ){` |
|     ! 0 | 4381 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4382 | `		return SXERR_ABORT;` |
|       - | 4383 | `	}` |
|       - | 4384 | `	/* Swap bytecode container */` |
|      12 | 4385 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4386 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4387 | `	/* Compile constant value.` |
|       - | 4388 | `	 */` |
|      12 | 4389 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4390 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4391 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4392 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4393 | `			return SXERR_ABORT;` |
|       - | 4394 | `		}` |
|       1 | 4395 | `	}` |
|       - | 4396 | `	/* Emit the done instruction */` |
|      12 | 4397 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4398 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4399 | `	if( rc == SXERR_ABORT ){` |
|       - | 4400 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4401 | `		return SXERR_ABORT;` |
|       - | 4402 | `	}` |
|       - | 4403 | `	/* All done,install the constant */` |
|      12 | 4404 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4405 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4406 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4407 | `		return SXERR_ABORT;` |
|       - | 4408 | `	}` |
|      12 | 4409 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4410 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4411 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4412 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4413 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4414 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4415 | `				pTok--;` |
|     ! 0 | 4416 | `			}` |
|     ! 0 | 4417 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4418 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4419 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4420 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4421 | `				return SXERR_ABORT;` |
|       - | 4422 | `			}` |
|     ! 0 | 4423 | `		}else{` |
|     ! 0 | 4424 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4425 | `				goto loop;` |
|       - | 4426 | `			}` |
|       - | 4427 | `		}` |
|     ! 0 | 4428 | `	}` |
|      12 | 4429 | `	return SXRET_OK;` |
|     ! 0 | 4430 | `Synchronize:` |
|       - | 4431 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4432 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4433 | `		pGen->pIn++;` |
|     ! 0 | 4434 | `	}` |
|     ! 0 | 4435 | `	return SXERR_CORRUPT;` |
|       7 | 4436 |  |
|       - | 4437 | `/*` |
|       - | 4438 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4439 | ` * According to the PHP language reference manual` |
|       - | 4440 | ` *  Properties` |
|       - | 4441 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4442 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4443 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4444 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4445 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4446 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4447 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4448 | ` * Symisc eXtension.` |
|       - | 4449 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4450 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4451 | ` *  Example:` |
|       - | 4452 | ` *   class Test{` |
|       - | 4453 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4454 | ` *   };` |
|       - | 4455 | ` *   var_dump(TEST::myVar);` |
|       - | 4456 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4457 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4458 | ` */` |
|   23274 | 4459 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4460 |  |
|   23276 | 4461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4462 | `	ph7_class_attr *pAttr;` |
|       - | 4463 | `	SyString *pName;` |
|       - | 4464 | `	sxi32 rc;` |
|       - | 4465 | `	/* Extract visibility level */` |
|   23276 | 4466 | `	iProtection = GetProtectionLevel(iProtection);` |
|   11637 | 4467 | `loop:` |
|   23276 | 4468 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   23276 | 4469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4470 | `		/* Invalid attribute name */` |
|     ! 0 | 4471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4472 | `		if( rc == SXERR_ABORT ){` |
|       - | 4473 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4474 | `			return SXERR_ABORT;` |
|       - | 4475 | `		}` |
|     ! 0 | 4476 | `		goto Synchronize;` |
|       - | 4477 | `	}` |
|       - | 4478 | `	/* Peek attribute name */` |
|   23276 | 4479 | `	pName = &pGen->pIn->sData;` |
|       - | 4480 | `	/* Advance the stream cursor */` |
|   23276 | 4481 | `	pGen->pIn++;` |
|   23276 | 4482 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4483 | `		/* Invalid declaration */` |
|       3 | 4484 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4485 | `		if( rc == SXERR_ABORT ){` |
|       - | 4486 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4487 | `			return SXERR_ABORT;` |
|       - | 4488 | `		}` |
|       3 | 4489 | `		goto Synchronize;` |
|       - | 4490 | `	}` |
|       - | 4491 | `	/* Allocate a new class attribute */` |
|   23274 | 4492 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   23274 | 4493 | `	if( pAttr == 0 ){` |
|     ! 0 | 4494 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4495 | `		return SXERR_ABORT;` |
|       - | 4496 | `	}` |
|   23274 | 4497 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4498 | `		SySet *pInstrContainer;` |
|    9378 | 4499 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4500 | `		/* Swap bytecode container */` |
|    9378 | 4501 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    9378 | 4502 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4503 | `		/* Compile attribute value.` |
|       - | 4504 | `		 */` |
|    9378 | 4505 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|    9378 | 4506 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4507 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4508 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4509 | `				return SXERR_ABORT;` |
|       - | 4510 | `			}` |
|     ! 0 | 4511 | `		}` |
|       - | 4512 | `		/* Emit the done instruction */` |
|    9378 | 4513 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|    9378 | 4514 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    4688 | 4515 | `	}` |
|       - | 4516 | `	/* All done,install the attribute */` |
|   23274 | 4517 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   23274 | 4518 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4519 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4520 | `		return SXERR_ABORT;` |
|       - | 4521 | `	}` |
|   23274 | 4522 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4523 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4524 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4525 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4526 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4527 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4528 | `				pTok--;` |
|     ! 0 | 4529 | `			}` |
|     ! 0 | 4530 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4531 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4532 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4533 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4534 | `				return SXERR_ABORT;` |
|       - | 4535 | `			}` |
|     ! 0 | 4536 | `		}else{` |
|     ! 0 | 4537 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4538 | `				goto loop;` |
|       - | 4539 | `			}` |
|       - | 4540 | `		}` |
|     ! 0 | 4541 | `	}` |
|   23274 | 4542 | `	return SXRET_OK;` |
|       1 | 4543 | `Synchronize:` |
|       - | 4544 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4545 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4546 | `		pGen->pIn++;` |
|       1 | 4547 | `	}` |
|       3 | 4548 | `	return SXERR_CORRUPT;` |
|   11639 | 4549 |  |
|       - | 4550 | `/*` |
|       - | 4551 | ` * Compile a class method.` |
|       - | 4552 | ` *` |
|       - | 4553 | ` * Refer to the official documentation for more information` |
|       - | 4554 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4555 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4556 | ` * overloading and many more.` |
|       - | 4557 | ` */` |
|   67254 | 4558 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4559 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4560 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4561 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4562 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4563 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4564 | `	)` |
|       2 | 4565 |  |
|   67256 | 4566 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4567 | `	ph7_class_method *pMeth;` |
|       - | 4568 | `	sxi32 iFuncFlags;` |
|       - | 4569 | `	SyString *pName;` |
|       - | 4570 | `	SyToken *pEnd;` |
|       - | 4571 | `	sxi32 rc;` |
|       - | 4572 | `	/* Extract visibility level */` |
|   67256 | 4573 | `	iProtection = GetProtectionLevel(iProtection);` |
|   67256 | 4574 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   67256 | 4575 | `	iFuncFlags = 0;` |
|   67256 | 4576 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4577 | `		/* Invalid method name */` |
|     ! 0 | 4578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4579 | `		if( rc == SXERR_ABORT ){` |
|       - | 4580 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4581 | `			return SXERR_ABORT;` |
|       - | 4582 | `		}` |
|     ! 0 | 4583 | `		goto Synchronize;` |
|       - | 4584 | `	}` |
|   67256 | 4585 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4586 | `		/* Return by reference,remember that */` |
|     ! 0 | 4587 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4588 | `		/* Jump the '&' token */` |
|     ! 0 | 4589 | `		pGen->pIn++;` |
|     ! 0 | 4590 | `	}` |
|   67256 | 4591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4592 | `		/* Invalid method name */` |
|     ! 0 | 4593 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4594 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4595 | `			return SXERR_ABORT;` |
|       - | 4596 | `		}` |
|     ! 0 | 4597 | `		goto Synchronize;` |
|       - | 4598 | `	}` |
|       - | 4599 | `	/* Peek method name */` |
|   67256 | 4600 | `	pName = &pGen->pIn->sData;` |
|   67256 | 4601 | `	nLine = pGen->pIn->nLine;` |
|       - | 4602 | `	/* Jump the method name */` |
|   67256 | 4603 | `	pGen->pIn++;` |
|   67256 | 4604 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4605 | `		/* Abstract method */` |
|       8 | 4606 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4607 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4608 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4609 | `				&pClass->sName,pName);` |
|     ! 0 | 4610 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4611 | `				return SXERR_ABORT;` |
|       - | 4612 | `			}` |
|     ! 0 | 4613 | `		}` |
|       - | 4614 | `		/* Assemble method signature only */` |
|       8 | 4615 | `		doBody = FALSE;` |
|       3 | 4616 | `	}` |
|   67256 | 4617 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4618 | `		/* Syntax error */` |
|     ! 0 | 4619 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4620 | `		if( rc == SXERR_ABORT ){` |
|       - | 4621 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4622 | `			return SXERR_ABORT;` |
|       - | 4623 | `		}` |
|     ! 0 | 4624 | `		goto Synchronize;` |
|       - | 4625 | `	}` |
|       - | 4626 | `	/* Allocate a new class_method instance */` |
|   67256 | 4627 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   67256 | 4628 | `	if( pMeth == 0 ){` |
|     ! 0 | 4629 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4630 | `		return SXERR_ABORT;` |
|       - | 4631 | `	}` |
|       - | 4632 | `	/* Jump the left parenthesis '(' */` |
|   67256 | 4633 | `	pGen->pIn++;` |
|   67256 | 4634 | `	pEnd = 0; /* cc warning */` |
|       - | 4635 | `	/* Delimit the method signature */` |
|   67256 | 4636 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   67256 | 4637 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4638 | `		/* Syntax error */` |
|       3 | 4639 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4640 | `		if( rc == SXERR_ABORT ){` |
|       - | 4641 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4642 | `			return SXERR_ABORT;` |
|       - | 4643 | `		}` |
|       3 | 4644 | `		goto Synchronize;` |
|       - | 4645 | `	}` |
|   67254 | 4646 | `	if( pGen->pIn < pEnd ){` |
|       - | 4647 | `		/* Collect method arguments */` |
|   11600 | 4648 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   11600 | 4649 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4650 | `			return SXERR_ABORT;` |
|       - | 4651 | `		}` |
|    5799 | 4652 | `	}` |
|       - | 4653 | `	/* Point beyond method signature */` |
|   67254 | 4654 | `	pGen->pIn = &pEnd[1];` |
|   67254 | 4655 | `	if( doBody ){` |
|       - | 4656 | `		/* Compile method body */` |
|   48760 | 4657 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   48760 | 4658 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4659 | `			return SXERR_ABORT;` |
|       - | 4660 | `		}` |
|   24381 | 4661 | `	}else{` |
|       - | 4662 | `		/* Only method signature is allowed */` |
|   18496 | 4663 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4664 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4665 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4666 | `				if( rc == SXERR_ABORT ){` |
|       - | 4667 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4668 | `					return SXERR_ABORT;` |
|       - | 4669 | `				}` |
|     ! 0 | 4670 | `				return SXERR_CORRUPT;` |
|       - | 4671 | `			}` |
|       - | 4672 | `	}` |
|       - | 4673 | `	/* All done,install the method */` |
|   67254 | 4674 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   67254 | 4675 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4676 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4677 | `		return SXERR_ABORT;` |
|       - | 4678 | `	}` |
|   67254 | 4679 | `	return SXRET_OK;` |
|       1 | 4680 | `Synchronize:` |
|       - | 4681 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4682 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4683 | `		pGen->pIn++;` |
|       1 | 4684 | `	}` |
|       3 | 4685 | `	return SXERR_CORRUPT;` |
|   33629 | 4686 |  |
|       - | 4687 | `/*` |
|       - | 4688 | ` * Compile an object interface.` |
|       - | 4689 | ` *  According to the PHP language reference manual` |
|       - | 4690 | ` *   Object Interfaces:` |
|       - | 4691 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4692 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4693 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4694 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4695 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4696 | ` */` |
|    6942 | 4697 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4698 |  |
|    6944 | 4699 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4700 | `	ph7_class *pClass,*pBase;` |
|       - | 4701 | `	SyToken *pEnd,*pTmp;` |
|       - | 4702 | `	SyString *pName;` |
|       - | 4703 | `	sxi32 nKwrd;` |
|       - | 4704 | `	sxi32 rc;` |
|       - | 4705 | `	/* Jump the 'interface' keyword */` |
|    6944 | 4706 | `	pGen->pIn++;` |
|       - | 4707 | `	/* Extract interface name */` |
|    6944 | 4708 | `	pName = &pGen->pIn->sData;` |
|       - | 4709 | `	/* Advance the stream cursor */` |
|    6944 | 4710 | `	pGen->pIn++;` |
|       - | 4711 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4712 | `		SyBlob sFQN;` |
|       - | 4713 | `		SyString sFQNStr;` |
|    6944 | 4714 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    6944 | 4715 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    6944 | 4716 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    6944 | 4717 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    6944 | 4718 | `		SyBlobRelease(&sFQN);` |
|       - | 4719 | `	}` |
|    6944 | 4720 | `	if( pClass == 0 ){` |
|     ! 0 | 4721 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4722 | `		return SXERR_ABORT;` |
|       - | 4723 | `	}` |
|       - | 4724 | `	/* Mark as an interface */` |
|    6944 | 4725 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4726 | `	/* Assume no base class is given */` |
|    6944 | 4727 | `	pBase = 0;` |
|    6944 | 4728 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4729 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4730 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4731 | `			SyString *pBaseName;` |
|       - | 4732 | `			/* Extract base interface */` |
|       3 | 4733 | `			pGen->pIn++;` |
|       3 | 4734 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4735 | `				/* Syntax error */` |
|     ! 0 | 4736 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4737 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4738 | `					pName);` |
|     ! 0 | 4739 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4740 | `				if( rc == SXERR_ABORT ){` |
|       - | 4741 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4742 | `					return SXERR_ABORT;` |
|       - | 4743 | `				}` |
|     ! 0 | 4744 | `				return SXRET_OK;` |
|       - | 4745 | `			}` |
|       3 | 4746 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4747 | `			{` |
|       - | 4748 | `				SyBlob sResolved;` |
|       3 | 4749 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 4750 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 4751 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 4752 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 4753 | `				SyBlobRelease(&sResolved);` |
|       - | 4754 | `			}` |
|       - | 4755 | `			/* Only interfaces is allowed */` |
|       3 | 4756 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4757 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4758 | `			}` |
|       3 | 4759 | `			if( pBase == 0 ){` |
|       - | 4760 | `				/* Inexistant interface */` |
|     ! 0 | 4761 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4762 | `				if( rc == SXERR_ABORT ){` |
|       - | 4763 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4764 | `					return SXERR_ABORT;` |
|       - | 4765 | `				}` |
|     ! 0 | 4766 | `			}` |
|       - | 4767 | `			/* Advance the stream cursor */` |
|       3 | 4768 | `			pGen->pIn++;` |
|       1 | 4769 | `		}` |
|       1 | 4770 | `	}` |
|    6944 | 4771 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4772 | `		/* Syntax error */` |
|     ! 0 | 4773 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4774 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4775 | `		if( rc == SXERR_ABORT ){` |
|       - | 4776 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4777 | `			return SXERR_ABORT;` |
|       - | 4778 | `		}` |
|     ! 0 | 4779 | `		return SXRET_OK;` |
|       - | 4780 | `	}` |
|    6944 | 4781 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    6944 | 4782 | `	pEnd = 0; /* cc warning */` |
|       - | 4783 | `	/* Delimit the interface body */` |
|    6944 | 4784 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    6944 | 4785 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4786 | `		/* Syntax error */` |
|     ! 0 | 4787 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4788 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4789 | `		if( rc == SXERR_ABORT ){` |
|       - | 4790 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4791 | `			return SXERR_ABORT;` |
|       - | 4792 | `		}` |
|     ! 0 | 4793 | `		return SXRET_OK;` |
|       - | 4794 | `	}` |
|       - | 4795 | `	/* Swap token stream */` |
|    6944 | 4796 | `	pTmp = pGen->pEnd;` |
|    6944 | 4797 | `	pGen->pEnd = pEnd;` |
|       - | 4798 | `	/* Start the parse process` |
|       - | 4799 | `	 * Note (According to the PHP reference manual):` |
|       - | 4800 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4801 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4802 | `	 */` |
|   12716 | 4803 | `	for(;;){` |
|       - | 4804 | `		/* Jump leading/trailing semi-colons */` |
|   43924 | 4805 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   18492 | 4806 | `			pGen->pIn++;` |
|       2 | 4807 | `		}` |
|   25434 | 4808 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4809 | `			/* End of interface body */` |
|    6944 | 4810 | `			break;` |
|       - | 4811 | `		}` |
|   18492 | 4812 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4813 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4814 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4815 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4816 | `			if( rc == SXERR_ABORT ){` |
|       - | 4817 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4818 | `				return SXERR_ABORT;` |
|       - | 4819 | `			}` |
|     ! 0 | 4820 | `			goto done;` |
|       - | 4821 | `		}` |
|       - | 4822 | `		/* Extract the current keyword */` |
|   18492 | 4823 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   18492 | 4824 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4825 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4826 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4827 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4828 | `		}` |
|   18492 | 4829 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4830 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4831 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4832 | `			if( rc == SXERR_ABORT ){` |
|       - | 4833 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4834 | `				return SXERR_ABORT;` |
|       - | 4835 | `			}` |
|     ! 0 | 4836 | `			goto done;` |
|       - | 4837 | `		}` |
|   18492 | 4838 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4839 | `			/* Advance the stream cursor */` |
|   18488 | 4840 | `			pGen->pIn++;` |
|   18488 | 4841 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4842 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4843 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4844 | `				if( rc == SXERR_ABORT ){` |
|       - | 4845 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4846 | `					return SXERR_ABORT;` |
|       - | 4847 | `				}` |
|     ! 0 | 4848 | `				goto done;` |
|       - | 4849 | `			}` |
|   18488 | 4850 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   18488 | 4851 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4852 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4853 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4854 | `				if( rc == SXERR_ABORT ){` |
|       - | 4855 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4856 | `					return SXERR_ABORT;` |
|       - | 4857 | `				}` |
|     ! 0 | 4858 | `				goto done;` |
|       - | 4859 | `			}` |
|    9243 | 4860 | `		}` |
|   18492 | 4861 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4862 | `			/* Parse constant */` |
|       3 | 4863 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4864 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4865 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4866 | `					return SXERR_ABORT;` |
|       - | 4867 | `				}` |
|     ! 0 | 4868 | `				goto done;` |
|       - | 4869 | `			}` |
|       2 | 4870 | `		}else{` |
|   18490 | 4871 | `			sxi32 iFlags = 0;` |
|   18490 | 4872 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4873 | `				/* Static method,record that */` |
|     ! 0 | 4874 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4875 | `				/* Advance the stream cursor */` |
|     ! 0 | 4876 | `				pGen->pIn++;` |
|     ! 0 | 4877 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4878 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4879 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4880 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4881 | `						if( rc == SXERR_ABORT ){` |
|       - | 4882 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4883 | `							return SXERR_ABORT;` |
|       - | 4884 | `						}` |
|     ! 0 | 4885 | `						goto done;` |
|       - | 4886 | `				}` |
|     ! 0 | 4887 | `			}` |
|       - | 4888 | `			/* Process method signature */` |
|   18490 | 4889 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   18490 | 4890 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4891 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4892 | `					return SXERR_ABORT;` |
|       - | 4893 | `				}` |
|     ! 0 | 4894 | `				goto done;` |
|       - | 4895 | `			}` |
|       - | 4896 | `		}` |
|       2 | 4897 | `	}` |
|       - | 4898 | `	/* Install the interface */` |
|    6944 | 4899 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    6944 | 4900 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4901 | `		/* Inherit from the base interface */` |
|       3 | 4902 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4903 | `	}` |
|    6944 | 4904 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4905 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4906 | `		return SXERR_ABORT;` |
|       - | 4907 | `	}` |
|    3471 | 4908 | `done:` |
|       - | 4909 | `	/* Point beyond the interface body */` |
|    6944 | 4910 | `	pGen->pIn  = &pEnd[1];` |
|    6944 | 4911 | `	pGen->pEnd = pTmp;` |
|    6944 | 4912 | `	return PH7_OK;` |
|    3473 | 4913 |  |
|       - | 4914 | `/*` |
|       - | 4915 | ` * Compile a user-defined class.` |
|       - | 4916 | ` * According to the PHP language reference manual` |
|       - | 4917 | ` *  class` |
|       - | 4918 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 4919 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 4920 | ` *  of the properties and methods belonging to the class.` |
|       - | 4921 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 4922 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 4923 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 4924 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4925 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 4926 | ` *  (called "methods").` |
|       - | 4927 | ` */` |
|   21084 | 4928 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 4929 |  |
|   21086 | 4930 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4931 | `	ph7_class *pClass,*pBase;` |
|       - | 4932 | `	SyToken *pEnd,*pTmp;` |
|       - | 4933 | `	sxi32 iProtection;` |
|       - | 4934 | `	SySet aInterfaces;` |
|       - | 4935 | `	sxi32 iAttrflags;` |
|       - | 4936 | `	SyString *pName;` |
|       - | 4937 | `	sxi32 nKwrd;` |
|       - | 4938 | `	sxi32 rc;` |
|       - | 4939 | `	/* Jump the 'class' keyword */` |
|   21086 | 4940 | `	pGen->pIn++;` |
|   21086 | 4941 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4942 | `		/* Syntax error */` |
|     ! 0 | 4943 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4944 | `		if( rc == SXERR_ABORT ){` |
|       - | 4945 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4946 | `			return SXERR_ABORT;` |
|       - | 4947 | `		}` |
|       - | 4948 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4949 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4950 | `			pGen->pIn++;` |
|     ! 0 | 4951 | `		}` |
|     ! 0 | 4952 | `		return SXRET_OK;` |
|       - | 4953 | `	}` |
|       - | 4954 | `	/* Extract class name */` |
|   21086 | 4955 | `	pName = &pGen->pIn->sData;` |
|       - | 4956 | `	/* Advance the stream cursor */` |
|   21086 | 4957 | `	pGen->pIn++;` |
|       - | 4958 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4959 | `		SyBlob sFQN;` |
|       - | 4960 | `		SyString sFQNStr;` |
|   21086 | 4961 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   21086 | 4962 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   21086 | 4963 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   21086 | 4964 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   21086 | 4965 | `		SyBlobRelease(&sFQN);` |
|       - | 4966 | `	}` |
|   21086 | 4967 | `	if( pClass == 0 ){` |
|     ! 0 | 4968 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4969 | `		return SXERR_ABORT;` |
|       - | 4970 | `	}` |
|       - | 4971 | `	/* implemented interfaces container */` |
|   21086 | 4972 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       - | 4973 | `	/* Assume a standalone class */` |
|   21086 | 4974 | `	pBase = 0;` |
|   21086 | 4975 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4976 | `		SyString *pBaseName;` |
|   13912 | 4977 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   13912 | 4978 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   13908 | 4979 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   13908 | 4980 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4981 | `				/* Syntax error */` |
|     ! 0 | 4982 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4983 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 4984 | `					pName);` |
|     ! 0 | 4985 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4986 | `				if( rc == SXERR_ABORT ){` |
|       - | 4987 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4988 | `					return SXERR_ABORT;` |
|       - | 4989 | `				}` |
|     ! 0 | 4990 | `				return SXRET_OK;` |
|       - | 4991 | `			}` |
|       - | 4992 | `			/* Extract base class name and resolve through namespace/imports */` |
|   13908 | 4993 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4994 | `			{` |
|       - | 4995 | `				SyBlob sResolved;` |
|   13908 | 4996 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   13908 | 4997 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   20861 | 4998 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   13906 | 4999 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   13908 | 5000 | `				SyBlobRelease(&sResolved);` |
|       - | 5001 | `			}` |
|       - | 5002 | `			/* Interfaces are not allowed */` |
|   13908 | 5003 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5004 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5005 | `			}` |
|   13908 | 5006 | `			if( pBase == 0 ){` |
|       - | 5007 | `				/* Inexistant base class */` |
|     ! 0 | 5008 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5009 | `				if( rc == SXERR_ABORT ){` |
|       - | 5010 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5011 | `					return SXERR_ABORT;` |
|       - | 5012 | `				}` |
|     ! 0 | 5013 | `			}else{` |
|   13908 | 5014 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5015 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5016 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5017 | `					if( rc == SXERR_ABORT ){` |
|       - | 5018 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5019 | `						return SXERR_ABORT;` |
|       - | 5020 | `					}` |
|     ! 0 | 5021 | `				}` |
|       - | 5022 | `			}` |
|       - | 5023 | `			/* Advance the stream cursor */` |
|   13908 | 5024 | `			pGen->pIn++;` |
|    6953 | 5025 | `		}` |
|   13912 | 5026 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5027 | `			ph7_class *pInterface;` |
|       - | 5028 | `			SyString *pIntName;` |
|       - | 5029 | `			/* Interface implementation */` |
|       8 | 5030 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       3 | 5031 | `			for(;;){` |
|       8 | 5032 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5033 | `					/* Syntax error */` |
|     ! 0 | 5034 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5035 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5036 | `						pName);` |
|     ! 0 | 5037 | `					if( rc == SXERR_ABORT ){` |
|       - | 5038 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5039 | `						return SXERR_ABORT;` |
|       - | 5040 | `					}` |
|     ! 0 | 5041 | `					break;` |
|       - | 5042 | `				}` |
|       - | 5043 | `				/* Extract interface name and resolve through namespace/imports */` |
|       8 | 5044 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5045 | `				{` |
|       - | 5046 | `					SyBlob sResolved;` |
|       8 | 5047 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 | 5048 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|      14 | 5049 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|       6 | 5050 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 | 5051 | `					SyBlobRelease(&sResolved);` |
|       - | 5052 | `				}` |
|       - | 5053 | `				/* Only interfaces are allowed */` |
|       8 | 5054 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5055 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5056 | `				}` |
|       8 | 5057 | `				if( pInterface == 0 ){` |
|       - | 5058 | `					/* Inexistant interface */` |
|     ! 0 | 5059 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5060 | `					if( rc == SXERR_ABORT ){` |
|       - | 5061 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5062 | `						return SXERR_ABORT;` |
|       - | 5063 | `					}` |
|     ! 0 | 5064 | `				}else{` |
|       - | 5065 | `					/* Register interface */` |
|       8 | 5066 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5067 | `				}` |
|       - | 5068 | `				/* Advance the stream cursor */` |
|       8 | 5069 | `				pGen->pIn++;` |
|       8 | 5070 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       5 | 5071 | `					break;` |
|       - | 5072 | `				}` |
|     ! 0 | 5073 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5074 | `			}` |
|       3 | 5075 | `		}` |
|    6955 | 5076 | `	}` |
|   21086 | 5077 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5078 | `		/* Syntax error */` |
|     ! 0 | 5079 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5080 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5081 | `		if( rc == SXERR_ABORT ){` |
|       - | 5082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5083 | `			return SXERR_ABORT;` |
|       - | 5084 | `		}` |
|     ! 0 | 5085 | `		return SXRET_OK;` |
|       - | 5086 | `	}` |
|   21086 | 5087 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   21086 | 5088 | `	pEnd = 0; /* cc warning */` |
|       - | 5089 | `	/* Delimit the class body */` |
|   21086 | 5090 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   21086 | 5091 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5092 | `		/* Syntax error */` |
|     ! 0 | 5093 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5094 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5095 | `		if( rc == SXERR_ABORT ){` |
|       - | 5096 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5097 | `			return SXERR_ABORT;` |
|       - | 5098 | `		}` |
|     ! 0 | 5099 | `		return SXRET_OK;` |
|       - | 5100 | `	}` |
|       - | 5101 | `	/* Swap token stream */` |
|   21086 | 5102 | `	pTmp = pGen->pEnd;` |
|   21086 | 5103 | `	pGen->pEnd = pEnd;` |
|       - | 5104 | `	/* Set the inherited flags */` |
|   21086 | 5105 | `	pClass->iFlags = iFlags;` |
|       - | 5106 | `	/* Start the parse process */` |
|   34931 | 5107 | `	for(;;){` |
|       - | 5108 | `		/* Jump leading/trailing semi-colons */` |
|  116416 | 5109 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   23288 | 5110 | `			pGen->pIn++;` |
|       2 | 5111 | `		}` |
|   93130 | 5112 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5113 | `			/* End of class body */` |
|   21082 | 5114 | `			break;` |
|       - | 5115 | `		}` |
|   72050 | 5116 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5117 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5118 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5119 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5120 | `			if( rc == SXERR_ABORT ){` |
|       - | 5121 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5122 | `				return SXERR_ABORT;` |
|       - | 5123 | `			}` |
|     ! 0 | 5124 | `			goto done;` |
|       - | 5125 | `		}` |
|       - | 5126 | `		/* Assume public visibility */` |
|   72050 | 5127 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   72050 | 5128 | `		iAttrflags = 0;` |
|   72050 | 5129 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5130 | `			/* Extract the current keyword */` |
|   72050 | 5131 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72050 | 5132 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   69652 | 5133 | `				iProtection = nKwrd;` |
|   69652 | 5134 | `				pGen->pIn++; /* Jump the visibility token */` |
|   69652 | 5135 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5136 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5137 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5138 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5139 | `					if( rc == SXERR_ABORT ){` |
|       - | 5140 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5141 | `						return SXERR_ABORT;` |
|       - | 5142 | `					}` |
|     ! 0 | 5143 | `					goto done;` |
|       - | 5144 | `				}` |
|   69652 | 5145 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5146 | `					/* Attribute declaration */` |
|   23266 | 5147 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   23266 | 5148 | `					if( rc != SXRET_OK ){` |
|       3 | 5149 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5150 | `							return SXERR_ABORT;` |
|       - | 5151 | `						}` |
|       3 | 5152 | `						goto done;` |
|       - | 5153 | `					}` |
|   23264 | 5154 | `					continue;` |
|       - | 5155 | `				}` |
|       - | 5156 | `				/* Extract the keyword */` |
|   46388 | 5157 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23193 | 5158 | `			}` |
|   48786 | 5159 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5160 | `				/* Process constant declaration */` |
|      10 | 5161 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5162 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5163 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5164 | `						return SXERR_ABORT;` |
|       - | 5165 | `					}` |
|     ! 0 | 5166 | `					goto done;` |
|       - | 5167 | `				}` |
|       6 | 5168 | `			}else{` |
|   48778 | 5169 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5170 | `					/* Static method or attribute,record that */` |
|      23 | 5171 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5172 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5173 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5174 | `						/* Extract the keyword */` |
|      19 | 5175 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5176 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5177 | `							iProtection = nKwrd;` |
|     ! 0 | 5178 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5179 | `						}` |
|       9 | 5180 | `					}` |
|      23 | 5181 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5182 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5183 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5184 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5185 | `						if( rc == SXERR_ABORT ){` |
|       - | 5186 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5187 | `							return SXERR_ABORT;` |
|       - | 5188 | `						}` |
|     ! 0 | 5189 | `						goto done;` |
|       - | 5190 | `					}` |
|      23 | 5191 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5192 | `						/* Attribute declaration */` |
|       5 | 5193 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5194 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5195 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5196 | `								return SXERR_ABORT;` |
|       - | 5197 | `							}` |
|     ! 0 | 5198 | `							goto done;` |
|       - | 5199 | `						}` |
|       5 | 5200 | `						continue;` |
|       - | 5201 | `					}` |
|       - | 5202 | `					/* Extract the keyword */` |
|      19 | 5203 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   48765 | 5204 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5205 | `					/* Abstract method,record that */` |
|       8 | 5206 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5207 | `					/* Mark the whole class as abstract */` |
|       8 | 5208 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5209 | `					/* Advance the stream cursor */` |
|       8 | 5210 | `					pGen->pIn++;` |
|       8 | 5211 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5212 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5213 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5214 | `							iProtection = nKwrd;` |
|       6 | 5215 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5216 | `						}` |
|       3 | 5217 | `					}` |
|       8 | 5218 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5219 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5220 | `							/* Static method */` |
|     ! 0 | 5221 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5222 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5223 | `					}` |
|       8 | 5224 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5225 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5226 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5227 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5228 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5229 | `							if( rc == SXERR_ABORT ){` |
|       - | 5230 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5231 | `								return SXERR_ABORT;` |
|       - | 5232 | `							}` |
|     ! 0 | 5233 | `							goto done;` |
|       - | 5234 | `					}` |
|       8 | 5235 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   48753 | 5236 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5237 | `					/* final method ,record that */` |
|       5 | 5238 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5239 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5240 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5241 | `						/* Extract the keyword */` |
|       5 | 5242 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5243 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5244 | `							iProtection = nKwrd;` |
|       5 | 5245 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5246 | `						}` |
|       2 | 5247 | `					}` |
|       5 | 5248 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5249 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5250 | `							/* Static method */` |
|     ! 0 | 5251 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5252 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5253 | `					}` |
|       5 | 5254 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5255 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5256 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5257 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5258 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5259 | `							if( rc == SXERR_ABORT ){` |
|       - | 5260 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5261 | `								return SXERR_ABORT;` |
|       - | 5262 | `							}` |
|     ! 0 | 5263 | `							goto done;` |
|       - | 5264 | `					}` |
|       5 | 5265 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5266 | `				}` |
|   48774 | 5267 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5268 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5269 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5270 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5271 | `						if( rc == SXERR_ABORT ){` |
|       - | 5272 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5273 | `							return SXERR_ABORT;` |
|       - | 5274 | `						}` |
|     ! 0 | 5275 | `						goto done;` |
|       - | 5276 | `				}` |
|   48774 | 5277 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5278 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5279 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5280 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5281 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5282 | `						if( rc == SXERR_ABORT ){` |
|       - | 5283 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5284 | `							return SXERR_ABORT;` |
|       - | 5285 | `						}` |
|     ! 0 | 5286 | `						goto done;` |
|       - | 5287 | `					}` |
|       - | 5288 | `					/* Attribute declaration */` |
|       7 | 5289 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5290 | `				}else{` |
|       - | 5291 | `					/* Process method declaration */` |
|   48768 | 5292 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5293 | `				}` |
|   48774 | 5294 | `				if( rc != SXRET_OK ){` |
|       3 | 5295 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5296 | `						return SXERR_ABORT;` |
|       - | 5297 | `					}` |
|       3 | 5298 | `					goto done;` |
|       - | 5299 | `				}` |
|       - | 5300 | `			}` |
|   24391 | 5301 | `		}else{` |
|       - | 5302 | `			/* Attribute declaration */` |
|     ! 0 | 5303 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5304 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5305 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5306 | `					return SXERR_ABORT;` |
|       - | 5307 | `				}` |
|     ! 0 | 5308 | `				goto done;` |
|       - | 5309 | `			}` |
|       - | 5310 | `		}` |
|       2 | 5311 | `	}` |
|       - | 5312 | `	/* Install the class */` |
|   21082 | 5313 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   21082 | 5314 | `	if( rc == SXRET_OK ){` |
|       - | 5315 | `		ph7_class **apInterface;` |
|       - | 5316 | `		sxu32 n;` |
|   21082 | 5317 | `		if( pBase ){` |
|       - | 5318 | `			/* Inherit from base class and mark as a subclass */` |
|   13908 | 5319 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    6953 | 5320 | `		}` |
|   21082 | 5321 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   21088 | 5322 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5323 | `			/* Implements one or more interface */` |
|       8 | 5324 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|       8 | 5325 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5326 | `				break;` |
|       - | 5327 | `			}` |
|       5 | 5328 | `		}` |
|   10540 | 5329 | `	}` |
|   21082 | 5330 | `	SySetRelease(&aInterfaces);` |
|   21082 | 5331 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5332 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5333 | `		return SXERR_ABORT;` |
|       - | 5334 | `	}` |
|   10540 | 5335 | `done:` |
|       - | 5336 | `	/* Point beyond the class body */` |
|   21086 | 5337 | `	pGen->pIn = &pEnd[1];` |
|   21086 | 5338 | `	pGen->pEnd = pTmp;` |
|   21086 | 5339 | `	return PH7_OK;` |
|   10544 | 5340 |  |
|       - | 5341 | `/*` |
|       - | 5342 | ` * Compile a user-defined abstract class.` |
|       - | 5343 | ` *  According to the PHP language reference manual` |
|       - | 5344 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5345 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5346 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5347 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5348 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5349 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5350 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5351 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5352 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5353 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5354 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5355 | ` *   could differ.` |
|       - | 5356 | ` */` |
|       4 | 5357 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5358 |  |
|       - | 5359 | `	sxi32 rc;` |
|       6 | 5360 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       6 | 5361 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       6 | 5362 | `	return rc;` |
|       2 | 5363 |  |
|       - | 5364 | `/*` |
|       - | 5365 | ` * Compile a user-defined final class.` |
|       - | 5366 | ` *  According to the PHP language reference manual` |
|       - | 5367 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5368 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5369 | ` *    final then it cannot be extended.` |
|       - | 5370 | ` */` |
|       2 | 5371 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5372 |  |
|       - | 5373 | `	sxi32 rc;` |
|       3 | 5374 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5375 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5376 | `	return rc;` |
|       1 | 5377 |  |
|       - | 5378 | `/*` |
|       - | 5379 | ` * Compile a user-defined class.` |
|       - | 5380 | ` *  According to the PHP language reference manual` |
|       - | 5381 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5382 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5383 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5384 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5385 | ` *   and functions (called "methods").` |
|       - | 5386 | ` */` |
|   21078 | 5387 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 5388 |  |
|       - | 5389 | `	sxi32 rc;` |
|   21080 | 5390 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   21080 | 5391 | `	return rc;` |
|       2 | 5392 |  |
|       - | 5393 | `/*` |
|       - | 5394 | ` * Exception handling.` |
|       - | 5395 | ` *  According to the PHP language reference manual` |
|       - | 5396 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 5397 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 5398 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 5399 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 5400 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 5401 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 5402 | ` *    (or re-thrown) within a catch block.` |
|       - | 5403 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 5404 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 5405 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 5406 | ` *    been defined with set_exception_handler().` |
|       - | 5407 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 5408 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 5409 | ` */` |
|       - | 5410 | `/*` |
|       - | 5411 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 5412 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 5413 | ` * indicates failure.` |
|       - | 5414 | ` */` |
|    6948 | 5415 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 5416 |  |
|    6950 | 5417 | `	sxi32 rc = SXRET_OK;` |
|    6950 | 5418 | `	if( pRoot->pOp ){` |
|    6946 | 5419 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3475 | 5420 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 5421 | `			/* Unexpected expression */` |
|     ! 0 | 5422 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5423 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 5424 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 5425 | `				rc = SXERR_INVALID;` |
|     ! 0 | 5426 | `			}` |
|       2 | 5427 | `		}` |
|    3476 | 5428 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 5429 | `		/* Unexpected expression */` |
|     ! 0 | 5430 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5431 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 5432 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 5433 | `			rc = SXERR_INVALID;` |
|     ! 0 | 5434 | `		}` |
|     ! 0 | 5435 | `	}` |
|    6950 | 5436 | `	return rc;` |
|       2 | 5437 |  |
|       - | 5438 | `/*` |
|       - | 5439 | ` * Compile a 'throw' statement.` |
|       - | 5440 | ` * throw: This is how you trigger an exception.` |
|       - | 5441 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 5442 | ` */` |
|    6948 | 5443 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 5444 |  |
|    6950 | 5445 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5446 | `	GenBlock *pBlock;` |
|       - | 5447 | `	sxu32 nIdx;` |
|       - | 5448 | `	sxi32 rc;` |
|    6950 | 5449 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 5450 | `	/* Compile the expression */` |
|    6950 | 5451 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    6950 | 5452 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5453 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 5454 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5455 | `			return SXERR_ABORT;` |
|       - | 5456 | `		}` |
|     ! 0 | 5457 | `		return SXRET_OK;` |
|       - | 5458 | `	}` |
|    6950 | 5459 | `	pBlock = pGen->pCurrent;` |
|       - | 5460 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   32378 | 5461 | `	while(pBlock->pParent){` |
|   32374 | 5462 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    6946 | 5463 | `			break;` |
|       - | 5464 | `		}` |
|       - | 5465 | `		/* Point to the parent block */` |
|   25430 | 5466 | `		pBlock = pBlock->pParent;` |
|       2 | 5467 | `	}` |
|       - | 5468 | `	/* Emit the throw instruction */` |
|    6950 | 5469 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 5470 | `	/* Emit the jump */` |
|    6950 | 5471 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    6950 | 5472 | `	return SXRET_OK;` |
|    3476 | 5473 |  |
|       - | 5474 | `/*` |
|       - | 5475 | ` * Compile a 'catch' block.` |
|       - | 5476 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 5477 | ` * an object containing the exception information.` |
|       - | 5478 | ` */` |
|      34 | 5479 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 5480 |  |
|      36 | 5481 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5482 | `	ph7_exception_block sCatch;` |
|       - | 5483 | `	SySet *pInstrContainer;` |
|       - | 5484 | `	GenBlock *pCatch;` |
|       - | 5485 | `	SyToken *pToken;` |
|       - | 5486 | `	SyString *pName;` |
|       - | 5487 | `	char *zDup;` |
|       - | 5488 | `	sxi32 rc;` |
|      36 | 5489 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 5490 | `	/* Zero the structure */` |
|      36 | 5491 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 5492 | `	/* Initialize fields */` |
|      36 | 5493 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      51 | 5494 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      36 | 5495 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5496 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5497 | `			pToken = pGen->pIn;` |
|     ! 0 | 5498 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5499 | `				pToken--;` |
|     ! 0 | 5500 | `			}` |
|     ! 0 | 5501 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5502 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 5503 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5504 | `				return SXERR_ABORT;` |
|       - | 5505 | `			}` |
|     ! 0 | 5506 | `			return SXERR_INVALID;` |
|       - | 5507 | `	}` |
|       - | 5508 | `	/* Extract the exception class */` |
|      36 | 5509 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 5510 | `	/* Duplicate class name */` |
|      36 | 5511 | `	pName = &pGen->pIn->sData;` |
|      36 | 5512 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      36 | 5513 | `	if( zDup == 0 ){` |
|     ! 0 | 5514 | `		goto Mem;` |
|       - | 5515 | `	}` |
|      36 | 5516 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      36 | 5517 | `	pGen->pIn++;` |
|      51 | 5518 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      36 | 5519 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5520 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5521 | `			pToken = pGen->pIn;` |
|     ! 0 | 5522 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5523 | `				pToken--;` |
|     ! 0 | 5524 | `			}` |
|     ! 0 | 5525 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5526 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 5527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5528 | `				return SXERR_ABORT;` |
|       - | 5529 | `			}` |
|     ! 0 | 5530 | `			return SXERR_INVALID;` |
|       - | 5531 | `	}` |
|      36 | 5532 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 5533 | `	/* Duplicate instance name */` |
|      36 | 5534 | `	pName = &pGen->pIn->sData;` |
|      36 | 5535 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      36 | 5536 | `	if( zDup == 0 ){` |
|     ! 0 | 5537 | `		goto Mem;` |
|       - | 5538 | `	}` |
|      36 | 5539 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      36 | 5540 | `	pGen->pIn++;` |
|      36 | 5541 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 5542 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 5543 | `		pToken = pGen->pIn;` |
|     ! 0 | 5544 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5545 | `			pToken--;` |
|     ! 0 | 5546 | `		}` |
|     ! 0 | 5547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5548 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 5549 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5550 | `			return SXERR_ABORT;` |
|       - | 5551 | `		}` |
|     ! 0 | 5552 | `		return SXERR_INVALID;` |
|       - | 5553 | `	}` |
|       - | 5554 | `	/* Compile the block */` |
|      36 | 5555 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 5556 | `	/* Create the catch block */` |
|      36 | 5557 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      36 | 5558 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5559 | `		return SXERR_ABORT;` |
|       - | 5560 | `	}` |
|       - | 5561 | `	/* Swap bytecode container */` |
|      36 | 5562 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      36 | 5563 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 5564 | `	/* Compile the block */` |
|      36 | 5565 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5566 | `	/* Fix forward jumps now the destination is resolved  */` |
|      36 | 5567 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5568 | `	/* Emit the DONE instruction */` |
|      36 | 5569 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5570 | `	/* Leave the block */` |
|      36 | 5571 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5572 | `	/* Restore the default container */` |
|      36 | 5573 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5574 | `	/* Install the catch block */` |
|      36 | 5575 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      36 | 5576 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5577 | `		goto Mem;` |
|       - | 5578 | `	}` |
|      36 | 5579 | `	return SXRET_OK;` |
|     ! 0 | 5580 | `Mem:` |
|     ! 0 | 5581 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5582 | `	return SXERR_ABORT;` |
|      19 | 5583 |  |
|       - | 5584 | `/*` |
|       - | 5585 | ` * Compile a 'try' block.` |
|       - | 5586 | ` * A function using an exception should be in a "try" block.` |
|       - | 5587 | ` * If the exception does not trigger, the code will continue` |
|       - | 5588 | ` * as normal. However if the exception triggers, an exception` |
|       - | 5589 | ` * is "thrown".` |
|       - | 5590 | ` */` |
|      36 | 5591 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 5592 |  |
|       - | 5593 | `	ph7_exception *pException;` |
|       - | 5594 | `	GenBlock *pTry;` |
|       - | 5595 | `	sxu32 nJmpIdx;` |
|       - | 5596 | `	sxi32 rc;` |
|       - | 5597 | `	/* Create the exception container */` |
|      38 | 5598 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      38 | 5599 | `	if( pException == 0 ){` |
|     ! 0 | 5600 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 5601 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5602 | `		return SXERR_ABORT;` |
|       - | 5603 | `	}` |
|       - | 5604 | `	/* Zero the structure */` |
|      38 | 5605 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 5606 | `	/* Initialize fields */` |
|      38 | 5607 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      38 | 5608 | `	pException->pVm = pGen->pVm;` |
|       - | 5609 | `	/* Create the try block */` |
|      38 | 5610 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      38 | 5611 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5612 | `		return SXERR_ABORT;` |
|       - | 5613 | `	}` |
|       - | 5614 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      38 | 5615 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 5616 | `	/* Fix the jump later when the destination is resolved */` |
|      38 | 5617 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      38 | 5618 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 5619 | `	/* Compile the block */` |
|      38 | 5620 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      38 | 5621 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5622 | `		return SXERR_ABORT;` |
|       - | 5623 | `	}` |
|       - | 5624 | `	/* Fix forward jumps now the destination is resolved */` |
|      38 | 5625 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5626 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      38 | 5627 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 5628 | `	/* Leave the block */` |
|      38 | 5629 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5630 | `	/* Compile the catch block */` |
|      38 | 5631 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      34 | 5632 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       3 | 5633 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 5634 | `			if( pTok >= pGen->pEnd ){` |
|       3 | 5635 | `				pTok--; /* Point back */` |
|       1 | 5636 | `			}` |
|       - | 5637 | `			/* Unexpected token */` |
|       4 | 5638 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,` |
|       1 | 5639 | `				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);` |
|       3 | 5640 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5641 | `				return SXERR_ABORT;` |
|       - | 5642 | `			}` |
|       3 | 5643 | `			return SXRET_OK;` |
|       - | 5644 | `	}` |
|       - | 5645 | `	/* Compile one or more catch blocks */` |
|      34 | 5646 | `	for(;;){` |
|      68 | 5647 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      47 | 5648 | `			\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       - | 5649 | `				/* No more blocks */` |
|      19 | 5650 | `				break;` |
|       - | 5651 | `		}` |
|       - | 5652 | `		/* Compile the catch block */` |
|      36 | 5653 | `		rc = PH7_CompileCatch(&(*pGen),pException);` |
|      36 | 5654 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5655 | `			return SXERR_ABORT;` |
|       - | 5656 | `		}` |
|       2 | 5657 | ` 	}` |
|      36 | 5658 | `	return SXRET_OK;` |
|      20 | 5659 |  |
|       - | 5660 | `/*` |
|       - | 5661 | ` * Compile a switch block.` |
|       - | 5662 | ` *  (See block-comment below for more information)` |
|       - | 5663 | ` */` |
|      84 | 5664 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 5665 |  |
|      86 | 5666 | `	sxi32 rc = SXRET_OK;` |
|      86 | 5667 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 5668 | `		/* Unexpected token */` |
|     ! 0 | 5669 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5670 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5671 | `			return SXERR_ABORT;` |
|       - | 5672 | `		}` |
|     ! 0 | 5673 | `		pGen->pIn++;` |
|     ! 0 | 5674 | `	}` |
|      86 | 5675 | `	pGen->pIn++;` |
|       - | 5676 | `	/* First instruction to execute in this block. */` |
|      86 | 5677 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 5678 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 5679 | `	 * or the '}' token */` |
|     151 | 5680 | `	for(;;){` |
|     304 | 5681 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5682 | `			/* No more input to process */` |
|     ! 0 | 5683 | `			break;` |
|       - | 5684 | `		}` |
|     304 | 5685 | `		rc = SXRET_OK;` |
|     304 | 5686 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 5687 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 5688 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 5689 | `					/* Unexpected token */` |
|     ! 0 | 5690 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5691 | `						&pGen->pIn->sData);` |
|     ! 0 | 5692 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5693 | `						return SXERR_ABORT;` |
|       - | 5694 | `					}` |
|       - | 5695 | `					/* FALL THROUGH */` |
|     ! 0 | 5696 | `				}` |
|      20 | 5697 | `				rc = SXERR_EOF;` |
|      20 | 5698 | `				break;` |
|       - | 5699 | `			}` |
|      23 | 5700 | `		}else{` |
|       - | 5701 | `			sxi32 nKwrd;` |
|       - | 5702 | `			/* Extract the keyword */` |
|     244 | 5703 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 5704 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 5705 | `				break;` |
|       - | 5706 | `			}` |
|     180 | 5707 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5708 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 5709 | `					/* Unexpected token */` |
|     ! 0 | 5710 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5711 | `						&pGen->pIn->sData);` |
|     ! 0 | 5712 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5713 | `						return SXERR_ABORT;` |
|       - | 5714 | `					}` |
|       - | 5715 | `					/* FALL THROUGH */` |
|     ! 0 | 5716 | `				}` |
|       - | 5717 | `				/* Block compiled */` |
|       3 | 5718 | `				break;` |
|       - | 5719 | `			}` |
|       - | 5720 | `		}` |
|       - | 5721 | `		/* Compile block */` |
|     220 | 5722 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 5723 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5724 | `			return SXERR_ABORT;` |
|       - | 5725 | `		}` |
|       2 | 5726 | `	}` |
|      86 | 5727 | `	return rc;` |
|      44 | 5728 |  |
|       - | 5729 | `/*` |
|       - | 5730 | ` * Compile a case eXpression.` |
|       - | 5731 | ` *  (See block-comment below for more information)` |
|       - | 5732 | ` */` |
|      70 | 5733 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 5734 |  |
|       - | 5735 | `	SySet *pInstrContainer;` |
|       - | 5736 | `	SyToken *pEnd,*pTmp;` |
|      72 | 5737 | `	sxi32 iNest = 0;` |
|       - | 5738 | `	sxi32 rc;` |
|       - | 5739 | `	/* Delimit the expression */` |
|      72 | 5740 | `	pEnd = pGen->pIn;` |
|     150 | 5741 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 5742 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 5743 | `			/* Increment nesting level */` |
|       3 | 5744 | `			iNest++;` |
|     149 | 5745 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 5746 | `			/* Decrement nesting level */` |
|       3 | 5747 | `			iNest--;` |
|     147 | 5748 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 5749 | `			break;` |
|       - | 5750 | `		}` |
|      80 | 5751 | `		pEnd++;` |
|       2 | 5752 | `	}` |
|      72 | 5753 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 5754 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 5755 | `		if( rc == SXERR_ABORT ){` |
|       - | 5756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5757 | `			return SXERR_ABORT;` |
|       - | 5758 | `		}` |
|     ! 0 | 5759 | `	}` |
|       - | 5760 | `	/* Swap token stream */` |
|      72 | 5761 | `	pTmp = pGen->pEnd;` |
|      72 | 5762 | `	pGen->pEnd = pEnd;` |
|      72 | 5763 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 5764 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 5765 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5766 | `	/* Emit the done instruction */` |
|      72 | 5767 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 5768 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5769 | `	/* Update token stream */` |
|      72 | 5770 | `	pGen->pIn  = pEnd;` |
|      72 | 5771 | `	pGen->pEnd = pTmp;` |
|      72 | 5772 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5773 | `		return SXERR_ABORT;` |
|       - | 5774 | `	}` |
|      72 | 5775 | `	return SXRET_OK;` |
|      37 | 5776 |  |
|       - | 5777 | `/*` |
|       - | 5778 | ` * Compile the smart switch statement.` |
|       - | 5779 | ` * According to the PHP language reference manual` |
|       - | 5780 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 5781 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 5782 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 5783 | ` *  This is exactly what the switch statement is for.` |
|       - | 5784 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 5785 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 5786 | ` *  of the outer loop, use continue 2.` |
|       - | 5787 | ` *  Note that switch/case does loose comparision.` |
|       - | 5788 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 5789 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 5790 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 5791 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 5792 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 5793 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 5794 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 5795 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 5796 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 5797 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 5798 | ` *  list for the next case.` |
|       - | 5799 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 5800 | ` *  or floating-point numbers and strings.` |
|       - | 5801 | ` */` |
|      20 | 5802 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 5803 |  |
|       - | 5804 | `	GenBlock *pSwitchBlock;` |
|       - | 5805 | `	SyToken *pTmp,*pEnd;` |
|       - | 5806 | `	ph7_switch *pSwitch;` |
|       - | 5807 | `	sxu32 nToken;` |
|       - | 5808 | `	sxu32 nLine;` |
|       - | 5809 | `	sxi32 rc;` |
|      22 | 5810 | `	nLine = pGen->pIn->nLine;` |
|       - | 5811 | `	/* Jump the 'switch' keyword */` |
|      22 | 5812 | `	pGen->pIn++;` |
|      22 | 5813 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5814 | `		/* Syntax error */` |
|     ! 0 | 5815 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 5816 | `		if( rc == SXERR_ABORT ){` |
|       - | 5817 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5818 | `			return SXERR_ABORT;` |
|       - | 5819 | `		}` |
|     ! 0 | 5820 | `		goto Synchronize;` |
|       - | 5821 | `	}` |
|       - | 5822 | `	/* Jump the left parenthesis '(' */` |
|      22 | 5823 | `	pGen->pIn++;` |
|      22 | 5824 | `	pEnd = 0; /* cc warning */` |
|       - | 5825 | `	/* Create the loop block */` |
|      32 | 5826 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 5827 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 5828 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5829 | `		return SXERR_ABORT;` |
|       - | 5830 | `	}` |
|       - | 5831 | `	/* Delimit the condition */` |
|      22 | 5832 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 5833 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 5834 | `		/* Empty expression */` |
|     ! 0 | 5835 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 5836 | `		if( rc == SXERR_ABORT ){` |
|       - | 5837 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5838 | `			return SXERR_ABORT;` |
|       - | 5839 | `		}` |
|     ! 0 | 5840 | `	}` |
|       - | 5841 | `	/* Swap token streams */` |
|      22 | 5842 | `	pTmp = pGen->pEnd;` |
|      22 | 5843 | `	pGen->pEnd = pEnd;` |
|       - | 5844 | `	/* Compile the expression */` |
|      22 | 5845 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 5846 | `	if( rc == SXERR_ABORT ){` |
|       - | 5847 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 5848 | `		return SXERR_ABORT;` |
|       - | 5849 | `	}` |
|       - | 5850 | `	/* Update token stream */` |
|      22 | 5851 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 5852 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5853 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5854 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5855 | `			return SXERR_ABORT;` |
|       - | 5856 | `		}` |
|     ! 0 | 5857 | `		pGen->pIn++;` |
|     ! 0 | 5858 | `	}` |
|      22 | 5859 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 5860 | `	pGen->pEnd = pTmp;` |
|      22 | 5861 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 5862 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 5863 | `			pTmp = pGen->pIn;` |
|     ! 0 | 5864 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 5865 | `				pTmp--;` |
|     ! 0 | 5866 | `			}` |
|       - | 5867 | `			/* Unexpected token */` |
|     ! 0 | 5868 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 5869 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5870 | `				return SXERR_ABORT;` |
|       - | 5871 | `			}` |
|     ! 0 | 5872 | `			goto Synchronize;` |
|       - | 5873 | `	}` |
|       - | 5874 | `	/* Set the delimiter token */` |
|      22 | 5875 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 5876 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 5877 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 5878 | `	}else{` |
|      20 | 5879 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 5880 | `	}` |
|      22 | 5881 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 5882 | `	/* Create the switch blocks container */` |
|      22 | 5883 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 5884 | `	if( pSwitch == 0 ){` |
|       - | 5885 | `		/* Abort compilation */` |
|     ! 0 | 5886 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5887 | `		return SXERR_ABORT;` |
|       - | 5888 | `	}` |
|       - | 5889 | `	/* Zero the structure */` |
|      22 | 5890 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 5891 | `	/* Initialize fields */` |
|      22 | 5892 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 5893 | `	/* Emit the switch instruction */` |
|      22 | 5894 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 5895 | `	/* Compile case blocks */` |
|      76 | 5896 | `	for(;;){` |
|       - | 5897 | `		sxu32 nKwrd;` |
|      88 | 5898 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5899 | `			/* No more input to process */` |
|     ! 0 | 5900 | `			break;` |
|       - | 5901 | `		}` |
|      88 | 5902 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5903 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 5904 | `				/* Unexpected token */` |
|     ! 0 | 5905 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5906 | `					&pGen->pIn->sData);` |
|     ! 0 | 5907 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5908 | `					return SXERR_ABORT;` |
|       - | 5909 | `				}` |
|       - | 5910 | `				/* FALL THROUGH */` |
|     ! 0 | 5911 | `			}` |
|       - | 5912 | `			/* Block compiled */` |
|     ! 0 | 5913 | `			break;` |
|       - | 5914 | `		}` |
|       - | 5915 | `		/* Extract the keyword */` |
|      88 | 5916 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 5917 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5918 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 5919 | `				/* Unexpected token */` |
|     ! 0 | 5920 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5921 | `					&pGen->pIn->sData);` |
|     ! 0 | 5922 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5923 | `					return SXERR_ABORT;` |
|       - | 5924 | `				}` |
|       - | 5925 | `				/* FALL THROUGH */` |
|     ! 0 | 5926 | `			}` |
|       - | 5927 | `			/* Block compiled */` |
|       3 | 5928 | `			break;` |
|       - | 5929 | `		}` |
|      86 | 5930 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 5931 | `			/*` |
|       - | 5932 | `			 * Accroding to the PHP language reference manual` |
|       - | 5933 | `			 *  A special case is the default case. This case matches anything` |
|       - | 5934 | `			 *  that wasn't matched by the other cases.` |
|       - | 5935 | `			 */` |
|      16 | 5936 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 5937 | `				/* Default case already compiled */` |
|     ! 0 | 5938 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 5939 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5940 | `					return SXERR_ABORT;` |
|       - | 5941 | `				}` |
|     ! 0 | 5942 | `			}` |
|      16 | 5943 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 5944 | `			/* Compile the default block */` |
|      16 | 5945 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 5946 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5947 | `				return SXERR_ABORT;` |
|      16 | 5948 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 5949 | `				break;` |
|       1 | 5950 | `			}` |
|      73 | 5951 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 5952 | `			ph7_case_expr sCase;` |
|       - | 5953 | `			/* Standard case block */` |
|      72 | 5954 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 5955 | `			/* initialize the structure */` |
|      72 | 5956 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5957 | `			/* Compile the case expression */` |
|      72 | 5958 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 5959 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5960 | `				return SXERR_ABORT;` |
|       - | 5961 | `			}` |
|       - | 5962 | `			/* Compile the case block */` |
|      72 | 5963 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 5964 | `			/* Insert in the switch container */` |
|      72 | 5965 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 5966 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5967 | `				return SXERR_ABORT;` |
|      72 | 5968 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 5969 | `				break;` |
|       - | 5970 | `			}` |
|      34 | 5971 | `		}else{` |
|       - | 5972 | `			/* Unexpected token */` |
|     ! 0 | 5973 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5974 | `				&pGen->pIn->sData);` |
|     ! 0 | 5975 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5976 | `				return SXERR_ABORT;` |
|       - | 5977 | `			}` |
|     ! 0 | 5978 | `			break;` |
|       - | 5979 | `		}` |
|       2 | 5980 | `	}` |
|       - | 5981 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 5982 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 5983 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5984 | `	/* Release the loop block */` |
|      22 | 5985 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 5986 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 5987 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 5988 | `		pGen->pIn++;` |
|      10 | 5989 | `	}` |
|       - | 5990 | `	/* Statement successfully compiled */` |
|      22 | 5991 | `	return SXRET_OK;` |
|     ! 0 | 5992 | `Synchronize:` |
|       - | 5993 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5994 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 5995 | `		pGen->pIn++;` |
|     ! 0 | 5996 | `	}` |
|     ! 0 | 5997 | `	return SXRET_OK;` |
|      12 | 5998 |  |
|       - | 5999 | `/*` |
|       - | 6000 | ` * Generate bytecode for a given expression tree.` |
|       - | 6001 | ` * If something goes wrong while generating bytecode` |
|       - | 6002 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 6003 | ` * this function takes care of generating the appropriate` |
|       - | 6004 | ` * error message.` |
|       - | 6005 | ` */` |
| 1929422 | 6006 | `static sxi32 GenStateEmitExprCode(` |
|       - | 6007 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6008 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 6009 | `	sxi32 iFlags /* Control flags */` |
|       - | 6010 | `	)` |
|       2 | 6011 |  |
|       - | 6012 | `	VmInstr *pInstr;` |
|       - | 6013 | `	sxu32 nJmpIdx;` |
| 1929424 | 6014 | `	sxi32 iP1 = 0;` |
| 1929424 | 6015 | `	sxu32 iP2 = 0;` |
| 1929424 | 6016 | `	void *p3  = 0;` |
|       - | 6017 | `	sxi32 iVmOp;` |
|       - | 6018 | `	sxi32 rc;` |
| 1929424 | 6019 | `	if( pNode->xCode ){` |
|       - | 6020 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 6021 | `		/* Compile node */` |
| 1184016 | 6022 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1184016 | 6023 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1184016 | 6024 | `		RE_SWAP_DELIMITER(pGen);` |
| 1184016 | 6025 | `		return rc;` |
|       - | 6026 | `	}` |
|  745410 | 6027 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 6028 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 6029 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 6030 | `		return SXERR_ABORT;` |
|       - | 6031 | `	}` |
|  745410 | 6032 | `	iVmOp = pNode->pOp->iVmOp;` |
|  745410 | 6033 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 6034 | `		sxu32 nJz,nJmp;` |
|       - | 6035 | `		/* Ternary operator require special handling */` |
|       - | 6036 | `		/* Phase#1: Compile the condition */` |
|    1742 | 6037 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1742 | 6038 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6039 | `			return rc;` |
|       - | 6040 | `		}` |
|    1742 | 6041 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1742 | 6042 | `		if( pNode->pLeft ){` |
|       - | 6043 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 6044 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1674 | 6045 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6046 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1674 | 6047 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1674 | 6048 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6049 | `				return rc;` |
|       - | 6050 | `			}` |
|     838 | 6051 | `		}else{` |
|       - | 6052 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 6053 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 6054 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 6055 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 6056 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6057 | `		}` |
|       - | 6058 | `		/* Phase#4: Emit the unconditional jump */` |
|    1742 | 6059 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 6060 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1742 | 6061 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1742 | 6062 | `		if( pInstr ){` |
|    1742 | 6063 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     870 | 6064 | `		}` |
|    1742 | 6065 | `		if( !pNode->pLeft ){` |
|       - | 6066 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 6067 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 6068 | `		}` |
|       - | 6069 | `		/* Phase#6: Compile the 'else' expression */` |
|    1742 | 6070 | `		if( pNode->pRight ){` |
|    1742 | 6071 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1742 | 6072 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6073 | `				return rc;` |
|       - | 6074 | `			}` |
|     870 | 6075 | `		}` |
|    1742 | 6076 | `		if( nJmp > 0 ){` |
|       - | 6077 | `			/* Phase#7: Fix the unconditional jump */` |
|    1742 | 6078 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1742 | 6079 | `			if( pInstr ){` |
|    1742 | 6080 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     870 | 6081 | `			}` |
|     870 | 6082 | `		}` |
|       - | 6083 | `		/* All done */` |
|    1742 | 6084 | `		return SXRET_OK;` |
|       - | 6085 | `	}` |
|       - | 6086 | `	/* Generate code for the left tree */` |
|  743670 | 6087 | `	if( pNode->pLeft ){` |
|  743652 | 6088 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 6089 | `			ph7_expr_node **apNode;` |
|       - | 6090 | `			sxi32 n;` |
|       - | 6091 | `			/* Recurse and generate bytecodes for function arguments */` |
|  220840 | 6092 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 6093 | `			/* Read-only load */` |
|  220840 | 6094 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  435510 | 6095 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  214672 | 6096 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  214672 | 6097 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6098 | `					return rc;` |
|       - | 6099 | `				}` |
|  107337 | 6100 | `			}` |
|       - | 6101 | `			/* Total number of given arguments */` |
|  220840 | 6102 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 6103 | `			/* Remove stale flags now */` |
|  220840 | 6104 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  110419 | 6105 | `		}` |
|  743652 | 6106 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  743652 | 6107 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6108 | `			return rc;` |
|       - | 6109 | `		}` |
|  743652 | 6110 | `		if( iVmOp == PH7_OP_CALL ){` |
|  220840 | 6111 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  220840 | 6112 | `			if( pInstr ){` |
|  220840 | 6113 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  220610 | 6114 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 6115 | `					sxu32 nQual;` |
|       - | 6116 | `					/* Prevent constant expansion */` |
|  220610 | 6117 | `					pInstr->iP1 = 0;` |
|       - | 6118 | `					/* Namespace-qualify the function name for CALL */` |
|  220610 | 6119 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  220610 | 6120 | `					pInstr->iP2 = (sxi32)nQual;` |
|  220610 | 6121 | `					if( nQual != nOrig ){` |
|       - | 6122 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 6123 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 6124 | `						 * and try the short name in hHostFunction. */` |
|      49 | 6125 | `						p3 = (void *)1;` |
|      26 | 6126 | `					}` |
|  110536 | 6127 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 6128 | `					/* Method call,flag that */` |
|     220 | 6129 | `					pInstr->iP2 = 1;` |
|     109 | 6130 | `				}` |
|  110421 | 6131 | `			}` |
|  633233 | 6132 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 6133 | `			ph7_expr_node **apNode;` |
|       - | 6134 | `			sxi32 n;` |
|       - | 6135 | `			/* Recurse and generate bytecodes for array index */` |
|   59144 | 6136 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  106688 | 6137 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   47546 | 6138 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   47546 | 6139 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6140 | `					return rc;` |
|       - | 6141 | `				}` |
|   23774 | 6142 | `			}` |
|   59144 | 6143 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   47546 | 6144 | `				iP1 = 1; /* Node have an index associated with it */` |
|   23772 | 6145 | `			}` |
|   59144 | 6146 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 6147 | `				/* Create an empty entry when the desired index is not found */` |
|   23382 | 6148 | `				iP2 = 1;` |
|   11692 | 6149 | `			}` |
|  493243 | 6150 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 6151 | `			/* POP the left node */` |
|      32 | 6152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 6153 | `		}` |
|  371825 | 6154 | `	}` |
|  743670 | 6155 | `	rc = SXRET_OK;` |
|  743670 | 6156 | `	nJmpIdx = 0;` |
|       - | 6157 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 6158 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 6159 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  743670 | 6160 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      88 | 6161 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      88 | 6162 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      88 | 6163 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      88 | 6164 | `			int isSpecial = 0;` |
|      88 | 6165 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      48 | 6166 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      48 | 6167 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      52 | 6168 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      46 | 6169 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 6170 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      34 | 6171 | `					isSpecial = 1;` |
|      16 | 6172 | `				}` |
|      33 | 6173 | `			}` |
|     108 | 6174 | `			pInstr->iP1 = 0;` |
|     108 | 6175 | `			if( !isSpecial ){` |
|      35 | 6176 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      17 | 6177 | `			}` |
|      33 | 6178 | `		}` |
|      67 | 6179 | `	}` |
|       - | 6180 | `	/* Generate code for the right tree */` |
|  743654 | 6181 | `	if( pNode->pRight ){` |
|  411216 | 6182 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 6183 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    7310 | 6184 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  407562 | 6185 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 6186 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2456 | 6187 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  402681 | 6188 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  179820 | 6189 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   89909 | 6190 | `		}` |
|  411216 | 6191 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  411216 | 6192 | `		if( iVmOp == PH7_OP_STORE ){` |
|  177396 | 6193 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  177396 | 6194 | `			if( pInstr ){` |
|  177396 | 6195 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 6196 | `					/* Hide the STORE instruction */` |
|      26 | 6197 | `					iVmOp = 0;` |
|  177384 | 6198 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 6199 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   39334 | 6200 | `					iP2 = 1;` |
|   19668 | 6201 | `				}else{` |
|  138040 | 6202 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6203 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   23380 | 6204 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   23380 | 6205 | `						iP1 = pInstr->iP1;` |
|   11691 | 6206 | `					}else{` |
|  114662 | 6207 | `						p3 = pInstr->p3;` |
|       - | 6208 | `					}` |
|       - | 6209 | `					/* POP the last dynamic load instruction */` |
|  138040 | 6210 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 6211 | `				}` |
|   88699 | 6212 | `			}` |
|  322519 | 6213 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      44 | 6214 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      44 | 6215 | `			if( pInstr ){` |
|      44 | 6216 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 6217 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 6218 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 6219 | `					 */` |
|      15 | 6220 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 6221 | `					iP1 = pInstr->iP1;` |
|      15 | 6222 | `					iP2 = pInstr->iP2;` |
|      15 | 6223 | `					p3  = pInstr->p3;` |
|       8 | 6224 | `				}else{` |
|      30 | 6225 | `					p3 = pInstr->p3;` |
|       - | 6226 | `				}` |
|      21 | 6227 | `			}` |
|      21 | 6228 | `		}` |
|  205607 | 6229 | `	}` |
|  743654 | 6230 | `	if( iVmOp > 0 ){` |
|  743600 | 6231 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    9446 | 6232 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 6233 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    6938 | 6234 | `				iP1 = 1;` |
|    3470 | 6235 | `			}` |
|  738878 | 6236 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 6237 | `			/* Namespace-qualify the class name for NEW */ {` |
|   11832 | 6238 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   11832 | 6239 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   11820 | 6240 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    5909 | 6241 | `				}` |
|   11832 | 6242 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 6243 | `					/* Prevent constant expansion for class name */` |
|   11830 | 6244 | `					pPeek->iP1 = 0;` |
|   11830 | 6245 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    5914 | 6246 | `				}` |
|       - | 6247 | `			}` |
|   11832 | 6248 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   11832 | 6249 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 6250 | `				VmInstr *pPrev;` |
|   11820 | 6251 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11820 | 6252 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 6253 | `					/* Pop the call instruction */` |
|   11820 | 6254 | `					iP1 = pInstr->iP1;` |
|   11820 | 6255 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    5909 | 6256 | `				}` |
|    5911 | 6257 | `			}` |
|  728241 | 6258 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 6259 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 6260 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 6261 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 6262 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 6263 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 6264 | `				int isSpecialIs = 0;` |
|      50 | 6265 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 6266 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 6267 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 6268 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 6269 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 6270 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 6271 | `						isSpecialIs = 1;` |
|       5 | 6272 | `					}` |
|      23 | 6273 | `				}` |
|      52 | 6274 | `				pInstr->iP1 = 0;` |
|      52 | 6275 | `				if( !isSpecialIs ){` |
|      38 | 6276 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 6277 | `				}` |
|      25 | 6278 | `			}` |
|  722305 | 6279 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 6280 | `			/* Prevent constant expansion for member/property names.` |
|       - | 6281 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 6282 | `			 * should not trigger constant lookup. */` |
|   88272 | 6283 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   88272 | 6284 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   88262 | 6285 | `				pInstr->iP1 = 0;` |
|   44130 | 6286 | `			}` |
|   88272 | 6287 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 6288 | `				/* Static member access,remember that */` |
|      72 | 6289 | `				iP1 = 1;` |
|      72 | 6290 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      72 | 6291 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       3 | 6292 | `					p3 = pInstr->p3;` |
|       3 | 6293 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       1 | 6294 | `				}` |
|      35 | 6295 | `			}` |
|   44135 | 6296 | `		}` |
|       - | 6297 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  743598 | 6298 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  743598 | 6299 | `		if( nJmpIdx > 0 ){` |
|       - | 6300 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|    9764 | 6301 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    9764 | 6302 | `			if( pInstr ){` |
|    9764 | 6303 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    4881 | 6304 | `			}` |
|    4881 | 6305 | `		}` |
|  371798 | 6306 | `	}` |
|  743652 | 6307 | `	return rc;` |
|  964704 | 6308 |  |
|       - | 6309 | `/*` |
|       - | 6310 | ` * Compile a PHP expression.` |
|       - | 6311 | ` * According to the PHP language reference manual:` |
|       - | 6312 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 6313 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 6314 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 6315 | ` *  is "anything that has a value".` |
|       - | 6316 | ` * If something goes wrong while compiling the expression,this` |
|       - | 6317 | ` * function takes care of generating the appropriate error` |
|       - | 6318 | ` * message.` |
|       - | 6319 | ` */` |
|  507344 | 6320 | `static sxi32 PH7_CompileExpr(` |
|       - | 6321 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6322 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 6323 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 6324 | `	)` |
|       2 | 6325 |  |
|       - | 6326 | `	ph7_expr_node *pRoot;` |
|       - | 6327 | `	SySet sExprNode;` |
|       - | 6328 | `	SyToken *pEnd;` |
|       - | 6329 | `	sxi32 nExpr;` |
|       - | 6330 | `	sxi32 iNest;` |
|       - | 6331 | `	sxi32 rc;` |
|       - | 6332 | `	/* Initialize worker variables */` |
|  507346 | 6333 | `	nExpr = 0;` |
|  507346 | 6334 | `	pRoot = 0;` |
|  507346 | 6335 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  507346 | 6336 | `	SySetAlloc(&sExprNode,0x10);` |
|  507346 | 6337 | `	rc = SXRET_OK;` |
|       - | 6338 | `	/* Delimit the expression */` |
|  507346 | 6339 | `	pEnd = pGen->pIn;` |
|  507346 | 6340 | `	iNest = 0;` |
| 3476088 | 6341 | `	while( pEnd < pGen->pEnd ){` |
| 3292562 | 6342 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6343 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     184 | 6344 | `			iNest++;` |
| 3292471 | 6345 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     192 | 6346 | `			iNest--;` |
| 3292285 | 6347 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  323958 | 6348 | `			if( iNest <= 0 ){` |
|  323820 | 6349 | `				break;` |
|       - | 6350 | `			}` |
|      69 | 6351 | `		}` |
| 2968744 | 6352 | `		pEnd++;` |
|       2 | 6353 | `	}` |
|  507346 | 6354 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    9388 | 6355 | `		SyToken *pEnd2 = pGen->pIn;` |
|    9388 | 6356 | `		iNest = 0;` |
|       - | 6357 | `		/* Stop at the first comma */` |
|   18794 | 6358 | `		while( pEnd2 < pEnd ){` |
|    9408 | 6359 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       3 | 6360 | `				iNest++;` |
|    9407 | 6361 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       3 | 6362 | `				iNest--;` |
|    9405 | 6363 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 6364 | `				if( iNest <= 0 ){` |
|     ! 0 | 6365 | `					break;` |
|       - | 6366 | `				}` |
|       2 | 6367 | `			}` |
|    9408 | 6368 | `			pEnd2++;` |
|       2 | 6369 | `		}` |
|    9388 | 6370 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 6371 | `			pEnd = pEnd2;` |
|     ! 0 | 6372 | `		}` |
|    4693 | 6373 | `	}` |
|  507346 | 6374 | `	if( pEnd > pGen->pIn ){` |
|  507338 | 6375 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 6376 | `		/* Swap delimiter */` |
|  507338 | 6377 | `		pGen->pEnd = pEnd;` |
|       - | 6378 | `		/* Try to get an expression tree */` |
|  507338 | 6379 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  507338 | 6380 | `		if( rc == SXRET_OK && pRoot ){` |
|  507176 | 6381 | `			rc = SXRET_OK;` |
|  507176 | 6382 | `			if( xTreeValidator ){` |
|       - | 6383 | `				/* Call the upper layer validator callback */` |
|   11982 | 6384 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    5990 | 6385 | `			}` |
|  507176 | 6386 | `			if( rc != SXERR_ABORT ){` |
|       - | 6387 | `				/* Generate code for the given tree */` |
|  507176 | 6388 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  253587 | 6389 | `			}` |
|  507176 | 6390 | `			nExpr = 1;` |
|  253587 | 6391 | `		}` |
|       - | 6392 | `		/* Release the whole tree */` |
|  507338 | 6393 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 6394 | `		/* Synchronize token stream */` |
|  507338 | 6395 | `		pGen->pEnd = pTmp;` |
|  507338 | 6396 | `		pGen->pIn  = pEnd;` |
|  507338 | 6397 | `		if( rc == SXERR_ABORT ){` |
|       3 | 6398 | `			SySetRelease(&sExprNode);` |
|       3 | 6399 | `			return SXERR_ABORT;` |
|       - | 6400 | `		}` |
|  253667 | 6401 | `	}` |
|  507344 | 6402 | `	SySetRelease(&sExprNode);` |
|  507344 | 6403 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  253674 | 6404 |  |
|       - | 6405 | `/*` |
|       - | 6406 | ` * Return a pointer to the node construct handler associated` |
|       - | 6407 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 6408 | ` */` |
|  139416 | 6409 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 6410 |  |
|  139418 | 6411 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 6412 | `		/* Numeric literal: Either real or integer */` |
|   75954 | 6413 | `		return PH7_CompileNumLiteral;` |
|   63466 | 6414 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 6415 | `		/* Double quoted string */` |
|   13202 | 6416 | `		return PH7_CompileString;` |
|   50266 | 6417 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 6418 | `		/* Single quoted string */` |
|   50206 | 6419 | `		return PH7_CompileSimpleString;` |
|      62 | 6420 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 6421 | `		/* Heredoc */` |
|      28 | 6422 | `		return PH7_CompileHereDoc;` |
|      36 | 6423 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 6424 | `		/* Nowdoc */` |
|      29 | 6425 | `		return PH7_CompileNowDoc;` |
|       7 | 6426 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 6427 | `		/* Backtick quoted string */` |
|       5 | 6428 | `		return PH7_CompileBacktic;` |
|       - | 6429 | `	}` |
|       3 | 6430 | `	return 0;` |
|   69710 | 6431 |  |
|       - | 6432 | `/*` |
|       - | 6433 | ` * PHP Language construct table.` |
|       - | 6434 | ` */` |
|       - | 6435 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 6436 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 6437 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 6438 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 6439 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 6440 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 6441 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 6442 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 6443 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 6444 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 6445 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 6446 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 6447 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 6448 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 6449 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 6450 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 6451 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 6452 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 6453 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 6454 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 6455 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 6456 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 6457 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 6458 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 6459 | `};` |
|       - | 6460 | `/*` |
|       - | 6461 | ` * Return a pointer to the statement handler routine associated` |
|       - | 6462 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 6463 | ` */` |
|  291698 | 6464 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 6465 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 6466 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 6467 | `	)` |
|       2 | 6468 |  |
|  291700 | 6469 | `	sxu32 n = 0;` |
| 1104077 | 6470 | `	for(;;){` |
| 2208156 | 6471 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   30546 | 6472 | `			break;` |
|       - | 6473 | `		}` |
| 2177612 | 6474 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  261156 | 6475 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 6476 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 6477 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 6478 | `					/* 'static' (class context),return null */` |
|     ! 0 | 6479 | `					return 0;` |
|       - | 6480 | `				}` |
|     ! 0 | 6481 | `			}` |
|       - | 6482 | `			/* Return a pointer to the handler.` |
|       - | 6483 | `			*/` |
|  261156 | 6484 | `			return aLangConstruct[n].xConstruct;` |
|       - | 6485 | `		}` |
| 1916458 | 6486 | `		n++;` |
|       2 | 6487 | `	}` |
|   30546 | 6488 | `	if( pLookahed ){` |
|   30546 | 6489 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    6944 | 6490 | `			return PH7_CompileClassInterface;` |
|   23604 | 6491 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   21080 | 6492 | `			return PH7_CompileClass;` |
|    2524 | 6493 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       7 | 6494 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       6 | 6495 | `				return PH7_CompileAbstractClass;` |
|    2520 | 6496 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 6497 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 6498 | `				return PH7_CompileFinalClass;` |
|       - | 6499 | `		}` |
|    1259 | 6500 | `	}` |
|       - | 6501 | `	/* Not a language construct */` |
|    2520 | 6502 | `	return 0;` |
|  145851 | 6503 |  |
|       - | 6504 | `/*` |
|       - | 6505 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 6506 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 6507 | ` */` |
|    2518 | 6508 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 6509 |  |
|       - | 6510 | `	int rc;` |
|    2520 | 6511 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2520 | 6512 | `	if( rc == FALSE ){` |
|      10 | 6513 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 6514 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 6515 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 6516 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 6517 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 6518 | `			*/` |
|       - | 6519 | `			){` |
|       3 | 6520 | `				rc = TRUE;` |
|       1 | 6521 | `		}` |
|       4 | 6522 | `	}` |
|    2520 | 6523 | `	return rc;` |
|       2 | 6524 |  |
|       - | 6525 | `/*` |
|       - | 6526 | ` * Compile a PHP chunk.` |
|       - | 6527 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6528 | ` * takes care of generating the appropriate error message.` |
|       - | 6529 | ` */` |
|  413834 | 6530 | `static sxi32 GenStateCompileChunk(` |
|       - | 6531 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6532 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 6533 | `	)` |
|       2 | 6534 |  |
|       - | 6535 | `	ProcLangConstruct xCons;` |
|       - | 6536 | `	sxi32 rc;` |
|  413836 | 6537 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  245545 | 6538 | `	for(;;){` |
|  491092 | 6539 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6540 | `			/* No more input to process */` |
|   10422 | 6541 | `			break;` |
|       - | 6542 | `		}` |
|  480672 | 6543 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6544 | `			/* Compile block */` |
|      12 | 6545 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 6546 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6547 | `				break;` |
|       - | 6548 | `			}` |
|       7 | 6549 | `		}else{` |
|  480662 | 6550 | `			xCons = 0;` |
|  480662 | 6551 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  291700 | 6552 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 6553 | `				/* Try to extract a language construct handler */` |
|  291700 | 6554 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  291700 | 6555 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      10 | 6556 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6557 | `						"Syntax error: Unexpected keyword '%z'",` |
|       6 | 6558 | `						&pGen->pIn->sData);` |
|       7 | 6559 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6560 | `						break;` |
|       - | 6561 | `					}` |
|       - | 6562 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 6563 | `					 * this erroneous statement.` |
|       - | 6564 | `					 */` |
|       7 | 6565 | `					xCons = PH7_ErrorRecover;` |
|       3 | 6566 | `				}` |
|  334813 | 6567 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   27304 | 6568 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 6569 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 6570 | `				xCons = PH7_CompileLabel;` |
|      56 | 6571 | `			}` |
|  480662 | 6572 | `			if( xCons == 0 ){` |
|       - | 6573 | `				/* Assume an expression an try to compile it */` |
|  191364 | 6574 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  191364 | 6575 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 6576 | `					/* Pop l-value */` |
|  191234 | 6577 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   95616 | 6578 | `				}` |
|   95683 | 6579 | `			}else{` |
|       - | 6580 | `				/* Go compile the sucker */` |
|  289300 | 6581 | `				rc = xCons(&(*pGen));` |
|       - | 6582 | `			}` |
|  480662 | 6583 | `			if( rc == SXERR_ABORT ){` |
|       - | 6584 | `				/* Request to abort compilation */` |
|       3 | 6585 | `				break;` |
|       - | 6586 | `			}` |
|       - | 6587 | `		}` |
|       - | 6588 | `		/* Ignore trailing semi-colons ';' */` |
|  788536 | 6589 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  307868 | 6590 | `			pGen->pIn++;` |
|       2 | 6591 | `		}` |
|  480670 | 6592 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 6593 | `			/* Compile a single statement and return */` |
|  403414 | 6594 | `			break;` |
|       - | 6595 | `		}` |
|       - | 6596 | `		/* LOOP ONE */` |
|       - | 6597 | `		/* LOOP TWO */` |
|       - | 6598 | `		/* LOOP THREE */` |
|       - | 6599 | `		/* LOOP FOUR */` |
|       2 | 6600 | `	}` |
|       - | 6601 | `	/* Return compilation status */` |
|  413836 | 6602 | `	return rc;` |
|       2 | 6603 |  |
|       - | 6604 | `/*` |
|       - | 6605 | ` * Compile a Raw PHP chunk.` |
|       - | 6606 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6607 | ` * takes care of generating the appropriate error message.` |
|       - | 6608 | ` */` |
|   10424 | 6609 | `static sxi32 PH7_CompilePHP(` |
|       - | 6610 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6611 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 6612 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 6613 | `	)` |
|       2 | 6614 |  |
|   10426 | 6615 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 6616 | `	sxi32 rc;` |
|       - | 6617 | `	/* Reset the token set */` |
|   10426 | 6618 | `	SySetReset(&(*pTokenSet));` |
|       - | 6619 | `	/* Mark as the default token set */` |
|   10426 | 6620 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 6621 | `	/* Advance the stream cursor */` |
|   10426 | 6622 | `	pGen->pRawIn++;` |
|       - | 6623 | `	/* Tokenize the PHP chunk first */` |
|   10426 | 6624 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 6625 | `	/* Point to the head and tail of the token stream. */` |
|   10426 | 6626 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10426 | 6627 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10426 | 6628 | `	if( is_expr ){` |
|     ! 0 | 6629 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 6630 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 6631 | `			/* A simple expression,compile it */` |
|     ! 0 | 6632 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 6633 | `		}` |
|       - | 6634 | `		/* Emit the DONE instruction */` |
|     ! 0 | 6635 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 6636 | `		return SXRET_OK;` |
|       - | 6637 | `	}` |
|   10426 | 6638 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 6639 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 6640 | `		/*` |
|       - | 6641 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 6642 | `		 * According to the PHP reference manual:` |
|       - | 6643 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 6644 | `		 *  immediately follow` |
|       - | 6645 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 6646 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 6647 | `		 * Symisc extension:` |
|       - | 6648 | `		 *   This short syntax works with all PHP opening` |
|       - | 6649 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 6650 | `		 *   only short tag.` |
|       - | 6651 | `		 */` |
|       - | 6652 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 6653 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 6654 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 6655 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 6656 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 6657 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 6658 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 6659 | `		}` |
|       3 | 6660 | `		return SXRET_OK;` |
|       - | 6661 | `	}` |
|       - | 6662 | `	/* Compile the PHP chunk */` |
|   10424 | 6663 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 6664 | `	/* Fix exceptions jumps */` |
|   10424 | 6665 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6666 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10424 | 6667 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 6668 | `		rc = SXERR_ABORT;` |
|       1 | 6669 | `	}` |
|       - | 6670 | `	/* Reset container */` |
|   10424 | 6671 | `	SySetReset(&pGen->aGoto);` |
|   10424 | 6672 | `	SySetReset(&pGen->aLabel);` |
|       - | 6673 | `	/* Compilation result */` |
|   10424 | 6674 | `	return rc;` |
|    5214 | 6675 |  |
|       - | 6676 | `/*` |
|       - | 6677 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 6678 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 6679 | ` * This is the only compile interface exported from this file.` |
|       - | 6680 | ` */` |
|   12168 | 6681 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 6682 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 6683 | `	SyString *pScript,  /* Script to compile */` |
|       - | 6684 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 6685 | `	)` |
|       2 | 6686 |  |
|       - | 6687 | `	SySet aPhpToken,aRawToken;` |
|       - | 6688 | `	ph7_gen_state *pCodeGen;` |
|       - | 6689 | `	ph7_value *pRawObj;` |
|       - | 6690 | `	sxu32 nObjIdx;` |
|       - | 6691 | `	sxi32 nRawObj;` |
|       - | 6692 | `	int is_expr;` |
|       - | 6693 | `	sxi32 rc;` |
|   12170 | 6694 | `	if( pScript->nByte < 1 ){` |
|       - | 6695 | `		/* Nothing to compile */` |
|     ! 0 | 6696 | `		return PH7_OK;` |
|       - | 6697 | `	}` |
|       - | 6698 | `	/* Initialize the tokens containers */` |
|   12170 | 6699 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12170 | 6700 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12170 | 6701 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12170 | 6702 | `	is_expr = 0;` |
|   12170 | 6703 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 6704 | `		SyToken sTmp;` |
|       - | 6705 | `		/* PHP only: -*/` |
|    2334 | 6706 | `		sTmp.nLine = 1;` |
|    2334 | 6707 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2334 | 6708 | `		sTmp.pUserData = 0;` |
|    2334 | 6709 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2334 | 6710 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2334 | 6711 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 6712 | `			/* A simple PHP expression */` |
|     ! 0 | 6713 | `			is_expr = 1;` |
|     ! 0 | 6714 | `		}` |
|    1168 | 6715 | `	}else{` |
|       - | 6716 | `		/* Tokenize raw text */` |
|    9838 | 6717 | `		SySetAlloc(&aRawToken,32);` |
|    9838 | 6718 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 6719 | `	}` |
|   12170 | 6720 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 6721 | `	/* Process high-level tokens */` |
|   12170 | 6722 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12170 | 6723 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12170 | 6724 | `	rc = PH7_OK;` |
|   12170 | 6725 | `	if( is_expr ){` |
|       - | 6726 | `		/* Compile the expression */` |
|     ! 0 | 6727 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 6728 | `		goto cleanup;` |
|       - | 6729 | `	}` |
|   12170 | 6730 | `	nObjIdx = 0;` |
|       - | 6731 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 6732 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 6733 | `	 * preventing namespace bleeding across include()d files. */` |
|   12170 | 6734 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 6735 | `	/* Start the compilation process */` |
|   11006 | 6736 | `	for(;;){` |
|   32434 | 6737 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12166 | 6738 | `			break; /* No more tokens to process */` |
|       - | 6739 | `		}` |
|   20270 | 6740 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 6741 | `			/* Compile the PHP chunk */` |
|   10426 | 6742 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10426 | 6743 | `			if( rc == SXERR_ABORT ){` |
|       5 | 6744 | `				break;` |
|       - | 6745 | `			}` |
|   10422 | 6746 | `			continue;` |
|       - | 6747 | `		}` |
|       - | 6748 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    9846 | 6749 | `		nRawObj = 0;` |
|   19690 | 6750 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 6751 | `			/* Consume the raw chunk without any processing */` |
|    9846 | 6752 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    9846 | 6753 | `			if( pRawObj == 0 ){` |
|     ! 0 | 6754 | `				rc = SXERR_MEM;` |
|     ! 0 | 6755 | `				break;` |
|       - | 6756 | `			}` |
|       - | 6757 | `			/* Mark as constant and emit the load constant instruction */` |
|    9846 | 6758 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    9846 | 6759 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    9846 | 6760 | `			++nRawObj;` |
|    9846 | 6761 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 6762 | `		}` |
|    9846 | 6763 | `		if( nRawObj > 0 ){` |
|       - | 6764 | `			/* Emit the consume instruction */` |
|    9846 | 6765 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    4922 | 6766 | `		}` |
|    6086 | 6767 | `	}` |
|    6084 | 6768 | `cleanup:` |
|   12170 | 6769 | `	SySetRelease(&aRawToken);` |
|   12170 | 6770 | `	SySetRelease(&aPhpToken);` |
|   12170 | 6771 | `	return rc;` |
|    6086 | 6772 |  |
|       - | 6773 | `/*` |
|       - | 6774 | ` * Utility routines.Initialize the code generator.` |
|       - | 6775 | ` */` |
|    2310 | 6776 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 6777 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6778 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6779 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6780 | `	)` |
|       2 | 6781 |  |
|    2312 | 6782 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6783 | `	/* Zero the structure */` |
|    2312 | 6784 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 6785 | `	/* Initial state */` |
|    2312 | 6786 | `	pGen->pVm  = &(*pVm);` |
|    2312 | 6787 | `	pGen->xErr = xErr;` |
|    2312 | 6788 | `	pGen->pErrData = pErrData;` |
|    2312 | 6789 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2312 | 6790 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2312 | 6791 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2312 | 6792 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 6793 | `	/* Error log buffer */` |
|    2312 | 6794 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 6795 | `	/* General purpose working buffer */` |
|    2312 | 6796 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 6797 | `	/* Namespace state */` |
|    2312 | 6798 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2312 | 6799 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 6800 | `	/* Create the global scope */` |
|    2312 | 6801 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 6802 | `	/* Point to the global scope */` |
|    2312 | 6803 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2312 | 6804 | `	return SXRET_OK;` |
|       2 | 6805 |  |
|       - | 6806 | `/*` |
|       - | 6807 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 6808 | ` */` |
|   14246 | 6809 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 6810 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6811 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6812 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6813 | `	)` |
|       2 | 6814 |  |
|   14248 | 6815 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6816 | `	GenBlock *pBlock,*pParent;` |
|       - | 6817 | `	/* Reset state */` |
|   14248 | 6818 | `	SySetReset(&pGen->aLabel);` |
|   14248 | 6819 | `	SySetReset(&pGen->aGoto);` |
|   14248 | 6820 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   14248 | 6821 | `	SyBlobRelease(&pGen->sWorker);` |
|   14248 | 6822 | `	SyBlobRelease(&pGen->sNamespace);` |
|   14248 | 6823 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   14248 | 6824 | `	SyHashRelease(&pGen->hUseImports);` |
|   14248 | 6825 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 6826 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 6827 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 6828 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 6829 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 6830 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 6831 | `	 * number of unique names, which is acceptable. */` |
|       - | 6832 | `	/* Point to the global scope */` |
|   14248 | 6833 | `	pBlock = pGen->pCurrent;` |
|   14248 | 6834 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 6835 | `		pParent = pBlock->pParent;` |
|     ! 0 | 6836 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 6837 | `		pBlock = pParent;` |
|     ! 0 | 6838 | `	}` |
|   14248 | 6839 | `	pGen->xErr = xErr;` |
|   14248 | 6840 | `	pGen->pErrData = pErrData;` |
|   14248 | 6841 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   14248 | 6842 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   14248 | 6843 | `	pGen->pIn = pGen->pEnd = 0;` |
|   14248 | 6844 | `	pGen->nErr = 0;` |
|   14248 | 6845 | `	return SXRET_OK;` |
|       2 | 6846 |  |
|       - | 6847 | `/*` |
|       - | 6848 | ` * Generate a compile-time error message.` |
|       - | 6849 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 6850 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 6851 | ` * abort compilation immediately.` |
|       - | 6852 | ` */` |
|     430 | 6853 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 6854 |  |
|     432 | 6855 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     432 | 6856 | `	const char *zErr = "Error";` |
|       - | 6857 | `	SyString *pFile;` |
|       - | 6858 | `	va_list ap;` |
|       - | 6859 | `	sxi32 rc;` |
|       - | 6860 | `	/* Reset the working buffer */` |
|     432 | 6861 | `	SyBlobReset(pWorker);` |
|       - | 6862 | `	/* Peek the processed file path if available */` |
|     432 | 6863 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     432 | 6864 | `	if( nErrType == E_ERROR ){` |
|       - | 6865 | `		/* Increment the error counter */` |
|     388 | 6866 | `		pGen->nErr++;` |
|     388 | 6867 | `		if( pGen->nErr > 15 ){` |
|       - | 6868 | `			/* Error count limit reached */` |
|       5 | 6869 | `			if( pGen->xErr ){` |
|       5 | 6870 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 6871 | `				SyBlobFormat(pWorker,"Error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 6872 | `				if( pFile ){` |
|       5 | 6873 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 6874 | `				}` |
|       5 | 6875 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 6876 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 6877 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 6878 | `				}` |
|       2 | 6879 | `			}` |
|       - | 6880 | `			/* Abort immediately */` |
|       5 | 6881 | `			return SXERR_ABORT;` |
|       - | 6882 | `		}` |
|     191 | 6883 | `	}` |
|     428 | 6884 | `	if( pGen->xErr == 0 ){` |
|       - | 6885 | `		/* No available error consumer,return immediately */` |
|       3 | 6886 | `		return SXRET_OK;` |
|       - | 6887 | `	}` |
|     425 | 6888 | `	switch(nErrType){` |
|      31 | 6889 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 6890 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 6891 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 6892 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 6893 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 6894 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     190 | 6895 | `	default:` |
|     380 | 6896 | `		break;` |
|       - | 6897 | `	}` |
|     425 | 6898 | `	rc = SXRET_OK;` |
|       - | 6899 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     425 | 6900 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     425 | 6901 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     425 | 6902 | `	va_start(ap,zFormat);` |
|     425 | 6903 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     425 | 6904 | `	va_end(ap);` |
|     425 | 6905 | `	if( pFile ){` |
|     425 | 6906 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     212 | 6907 | `	}` |
|       - | 6908 | `	/* Append a new line */` |
|     425 | 6909 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     425 | 6910 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6911 | `		/* Consume the generated error message */` |
|     425 | 6912 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     212 | 6913 | `	}` |
|     425 | 6914 | `	return rc;` |
|     217 | 6915 |  |
|       - | 6916 |  |
