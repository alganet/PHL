# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2681/3572 lines (75.06%)

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
|       1 |  105 |  |
|       - |  106 | `	Label *aLabel;` |
|       - |  107 | `	sxu32 n;` |
|       - |  108 | `	/* Perform a linear scan on the label table */` |
|     149 |  109 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     329 |  110 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     273 |  111 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |  112 | `			/* Jump destination found */` |
|      93 |  113 | `			aLabel[n].bRef = TRUE;` |
|      93 |  114 | `			if( ppOut ){` |
|      93 |  115 | `				*ppOut = &aLabel[n];` |
|      46 |  116 | `			}` |
|      93 |  117 | `			return SXRET_OK;` |
|       - |  118 | `		}` |
|      91 |  119 | `	}` |
|       - |  120 | `	/* No such destination */` |
|      57 |  121 | `	return SXERR_NOTFOUND;` |
|      75 |  122 |  |
|       - |  123 | `/*` |
|       - |  124 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |  125 | ` * compiled blocks.` |
|       - |  126 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |  127 | ` */` |
|   10874 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       1 |  129 |  |
|   10875 |  130 | `	GenBlock *pBlock = pCurrent;` |
|   26839 |  131 | `	for(;;){` |
|   53679 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|   10763 |  133 | `			iCount--; /* Decrement nesting level */` |
|   10763 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|   10741 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   42939 |  140 | `		pBlock = pBlock->pParent;` |
|   42939 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      68 |  143 | `			break;` |
|       - |  144 | `		}` |
|       1 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     135 |  147 | `	return 0;` |
|    5438 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  887146 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       1 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  887147 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  887147 |  162 | `	pBlock->pUserData   = pUserData;` |
|  887147 |  163 | `	pBlock->pGen        = pGen;` |
|  887147 |  164 | `	pBlock->iFlags      = iType;` |
|  887147 |  165 | `	pBlock->pParent     = 0;` |
|  887147 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  887147 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  887147 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  881812 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       1 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  881813 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  881813 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  881813 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  881813 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  881813 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  881813 |  200 | `	pGen->pCurrent = pBlock;` |
|  881813 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  430423 |  203 | `		*ppBlock = pBlock;` |
|  215211 |  204 | `	}` |
|  881813 |  205 | `	return SXRET_OK;` |
|  440907 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  881806 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       1 |  211 |  |
|  881807 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  881807 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  881807 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  881806 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       1 |  219 |  |
|  881807 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  881807 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  881807 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  881807 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  881806 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       1 |  229 |  |
|  881807 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  881807 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  881807 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  881807 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  881807 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  881807 |  244 | `	return SXRET_OK;` |
|  440904 |  245 |  |
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
|  286634 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       1 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  286635 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  286635 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  286635 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  286635 |  265 | `	return rc;` |
|       1 |  266 |  |
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
|  651968 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       1 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  651969 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
| 1192851 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  540883 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  221533 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  319351 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32719 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  286633 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  286633 |  298 | `		if( pInstr ){` |
|  286633 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  286633 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  286633 |  302 | `			aFix[n].nJumpType = -1;` |
|  143316 |  303 | `		}` |
|  143317 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  651969 |  306 | `	return nFixed;` |
|       1 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  192636 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       1 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  192637 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  192783 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     149 |  326 | `		pJump = &aJumps[n];` |
|       - |  327 | `		/* Extract the target label */` |
|     149 |  328 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     149 |  329 | `		if( rc != SXRET_OK ){` |
|       - |  330 | `			/* No such label */` |
|      57 |  331 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      57 |  332 | `			if( rc == SXERR_ABORT ){` |
|       3 |  333 | `				return SXERR_ABORT;` |
|       - |  334 | `			}` |
|      55 |  335 | `			continue;` |
|       - |  336 | `		}` |
|       - |  337 | `		/* Make sure the target label is reachable */` |
|      93 |  338 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       9 |  339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       9 |  340 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  341 | `				return SXERR_ABORT;` |
|       - |  342 | `			}` |
|       4 |  343 | `		}` |
|       - |  344 | `		/* Fix the jump now the destination is resolved */` |
|      93 |  345 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      93 |  346 | `		if( pInstr ){` |
|      93 |  347 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |  348 | `		}` |
|      47 |  349 | `	}` |
|  192635 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  192767 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     133 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      67 |  357 | `	}` |
|  192635 |  358 | `	return SXRET_OK;` |
|   96319 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  764414 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       1 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  764415 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  764415 |  367 | `	if( pEntry == 0 ){` |
|  343963 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  420453 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  420453 |  371 | `	return SXRET_OK;` |
|  382208 |  372 |  |
|       - |  373 | `/*` |
|       - |  374 | ` * Install a given constant index in the literal table.` |
|       - |  375 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |  376 | ` */` |
|  343962 |  377 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       1 |  378 |  |
|  343963 |  379 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  343963 |  380 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  171981 |  381 | `	}` |
|  343963 |  382 | `	return SXRET_OK;` |
|       1 |  383 |  |
|       - |  384 | `/*` |
|       - |  385 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  386 | ` * in the constant table.` |
|       - |  387 | ` */` |
|  169524 |  388 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       1 |  389 |  |
|       - |  390 | `	ph7_value *pObj;` |
|  169525 |  391 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  392 | `	/* Reserve a new constant */` |
|  169525 |  393 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  169525 |  394 | `	if( pObj == 0 ){` |
|     ! 0 |  395 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  396 | `		return 0;` |
|       - |  397 | `	}` |
|  169525 |  398 | `	*pIdx = nIdx;` |
|       - |  399 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  400 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  401 | `	 */` |
|  169525 |  402 | `	return pObj;` |
|   84763 |  403 |  |
|       - |  404 | `/*` |
|       - |  405 | ` * Implementation of the PHP language constructs.` |
|       - |  406 | ` */` |
|       - |  407 | `/* Forward declaration */` |
|       - |  408 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  409 | `/*` |
|       - |  410 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  411 | ` * Notes on the integer type.` |
|       - |  412 | ` *  According to the PHP language reference manual` |
|       - |  413 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  414 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  415 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  416 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  417 | ` * Symisc eXtension to the integer type.` |
|       - |  418 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  419 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  420 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  421 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  422 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  423 | ` *  documentation.` |
|       - |  424 | ` */` |
|  169840 |  425 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  426 |  |
|  169841 |  427 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  169841 |  428 | `	sxu32 nIdx = 0;` |
|  169841 |  429 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  430 | `		ph7_value *pObj;` |
|       - |  431 | `		sxi64 iValue;` |
|  169525 |  432 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|  169525 |  433 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  169525 |  434 | `		if( pObj == 0 ){` |
|     ! 0 |  435 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  436 | `			return SXERR_ABORT;` |
|       - |  437 | `		}` |
|  169525 |  438 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   84763 |  439 | `	}else{` |
|       - |  440 | `		/* Real number */` |
|       - |  441 | `		ph7_value *pObj;` |
|       - |  442 | `		/* Reserve a new constant */` |
|     317 |  443 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     317 |  444 | `		if( pObj == 0 ){` |
|     ! 0 |  445 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  446 | `			return SXERR_ABORT;` |
|       - |  447 | `		}` |
|     317 |  448 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);` |
|     317 |  449 | `		PH7_MemObjToReal(pObj);` |
|       - |  450 | `	}` |
|       - |  451 | `	/* Emit the load constant instruction */` |
|  169841 |  452 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  453 | `	/* Node successfully compiled */` |
|  169841 |  454 | `	return SXRET_OK;` |
|   84921 |  455 |  |
|       - |  456 | `/*` |
|       - |  457 | ` * Compile a single quoted string.` |
|       - |  458 | ` * According to the PHP language reference manual:` |
|       - |  459 | ` *` |
|       - |  460 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  461 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  462 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  463 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  464 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  465 | ` *` |
|       - |  466 | ` */` |
|   71010 |  467 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  468 |  |
|   71011 |  469 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  470 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  471 | `	ph7_value *pObj;` |
|       - |  472 | `	sxu32 nIdx;` |
|   71011 |  473 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  474 | `	/* Delimit the string */` |
|   71011 |  475 | `	zIn  = pStr->zString;` |
|   71011 |  476 | `	zEnd = &zIn[pStr->nByte];` |
|   71011 |  477 | `	if( zIn >= zEnd ){` |
|       - |  478 | `		/* Empty string,load NULL */` |
|      85 |  479 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|      85 |  480 | `		return SXRET_OK;` |
|       - |  481 | `	}` |
|   70927 |  482 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  483 | `		/* Already processed,emit the load constant instruction` |
|       - |  484 | `		 * and return.` |
|       - |  485 | `		 */` |
|   16897 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16897 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|       - |  489 | `	/* Reserve a new constant */` |
|   54031 |  490 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   54031 |  491 | `	if( pObj == 0 ){` |
|     ! 0 |  492 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  493 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  494 | `		return SXERR_ABORT;` |
|       - |  495 | `	}` |
|   54031 |  496 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  497 | `	/* Compile the node */` |
|   54046 |  498 | `	for(;;){` |
|  108093 |  499 | `		if( zIn >= zEnd ){` |
|       - |  500 | `			/* End of input */` |
|   54031 |  501 | `			break;` |
|       - |  502 | `		}` |
|   54063 |  503 | `		zCur = zIn;` |
|  293601 |  504 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  239539 |  505 | `			zIn++;` |
|       1 |  506 | `		}` |
|   54063 |  507 | `		if( zIn > zCur ){` |
|       - |  508 | `			/* Append raw contents*/` |
|   54043 |  509 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   27021 |  510 | `		}` |
|   54063 |  511 | `		zIn++;` |
|   54063 |  512 | `		if( zIn < zEnd ){` |
|      55 |  513 | `			if( zIn[0] == '\\' ){` |
|       - |  514 | `				/* A literal backslash */` |
|      21 |  515 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      45 |  516 | `			}else if( zIn[0] == '\'' ){` |
|       - |  517 | `				/* A single quote */` |
|      11 |  518 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  519 | `			}else{` |
|       - |  520 | `				/* verbatim copy */` |
|      25 |  521 | `				zIn--;` |
|      25 |  522 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      25 |  523 | `				zIn++;` |
|       - |  524 | `			}` |
|      27 |  525 | `		}` |
|       - |  526 | `		/* Advance the stream cursor */` |
|   54063 |  527 | `		zIn++;` |
|       1 |  528 | `	}` |
|       - |  529 | `	/* Emit the load constant instruction */` |
|   54031 |  530 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   54031 |  531 | `	if( pStr->nByte < 1024 ){` |
|       - |  532 | `		/* Install in the literal table */` |
|   54031 |  533 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   27015 |  534 | `	}` |
|       - |  535 | `	/* Node successfully compiled */` |
|   54031 |  536 | `	return SXRET_OK;` |
|   35506 |  537 |  |
|       - |  538 | `/*` |
|       - |  539 | ` * Compile a nowdoc string.` |
|       - |  540 | ` * According to the PHP language reference manual:` |
|       - |  541 | ` *` |
|       - |  542 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  543 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  544 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  545 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  546 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  547 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  548 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  549 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  550 | ` *  of the closing identifier.` |
|       - |  551 | ` */` |
|      28 |  552 | `static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  553 |  |
|      29 |  554 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  555 | `	ph7_value *pObj;` |
|       - |  556 | `	sxu32 nIdx;` |
|      29 |  557 | `	nIdx = 0; /* Prevent compiler warning */` |
|      29 |  558 | `	if( pStr->nByte <= 0 ){` |
|       - |  559 | `		/* Empty string,load NULL */` |
|     ! 0 |  560 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 |  561 | `		return SXRET_OK;` |
|       - |  562 | `	}` |
|       - |  563 | `	/* Reserve a new constant */` |
|      29 |  564 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  565 | `	if( pObj == 0 ){` |
|     ! 0 |  566 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  567 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  568 | `		return SXERR_ABORT;` |
|       - |  569 | `	}` |
|       - |  570 | `	/* No processing is done here, simply a memcpy() operation */` |
|      29 |  571 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  572 | `	/* Emit the load constant instruction */` |
|      29 |  573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  574 | `	/* Node successfully compiled */` |
|      29 |  575 | `	return SXRET_OK;` |
|      15 |  576 |  |
|       - |  577 | `/*` |
|       - |  578 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  579 | ` * According to the PHP language reference manual` |
|       - |  580 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  581 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  582 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  583 | ` *  property in a string with a minimum of effort.` |
|       - |  584 | ` *  Simple syntax` |
|       - |  585 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  586 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  587 | ` *   the end of the name.` |
|       - |  588 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  589 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  590 | ` *   as to simple variables.` |
|       - |  591 | ` *  Complex (curly) syntax` |
|       - |  592 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  593 | ` *   of complex expressions.` |
|       - |  594 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  595 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  596 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  597 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  598 | ` */` |
|    1286 |  599 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  600 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  601 | `	sxu32 nLine,         /* Line number */` |
|       - |  602 | `	const char *zIn,     /* Raw expression */` |
|       - |  603 | `	const char *zEnd     /* End of the expression */` |
|       - |  604 | `	)` |
|       1 |  605 |  |
|       - |  606 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  607 | `	SySet sToken;` |
|       - |  608 | `	sxi32 rc;` |
|       - |  609 | `	/* Initialize the token set */` |
|    1287 |  610 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  611 | `	/* Preallocate some slots */` |
|    1287 |  612 | `	SySetAlloc(&sToken,0x08);` |
|       - |  613 | `	/* Tokenize the text */` |
|    1287 |  614 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  615 | `	/* Swap delimiter */` |
|    1287 |  616 | `	pTmpIn  = pGen->pIn;` |
|    1287 |  617 | `	pTmpEnd = pGen->pEnd;` |
|    1287 |  618 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1287 |  619 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  620 | `	/* Compile the expression */` |
|    1287 |  621 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  622 | `	/* Restore token stream */` |
|    1287 |  623 | `	pGen->pIn  = pTmpIn;` |
|    1287 |  624 | `	pGen->pEnd = pTmpEnd;` |
|       - |  625 | `	/* Release the token set */` |
|    1287 |  626 | `	SySetRelease(&sToken);` |
|       - |  627 | `	/* Compilation result */` |
|    1287 |  628 | `	return rc;` |
|       1 |  629 |  |
|       - |  630 | `/*` |
|       - |  631 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  632 | ` */` |
|   13062 |  633 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       1 |  634 |  |
|       - |  635 | `	ph7_value *pConstObj;` |
|   13063 |  636 | `	sxu32 nIdx = 0;` |
|       - |  637 | `	/* Reserve a new constant */` |
|   13063 |  638 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   13063 |  639 | `	if( pConstObj == 0 ){` |
|     ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  641 | `		return 0;` |
|       - |  642 | `	}` |
|   13063 |  643 | `	(*pCount)++;` |
|   13063 |  644 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  645 | `	/* Emit the load constant instruction */` |
|   13063 |  646 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   13063 |  647 | `	return pConstObj;` |
|    6532 |  648 |  |
|       - |  649 | `/*` |
|       - |  650 | ` * Compile a double quoted/heredoc string.` |
|       - |  651 | ` * According to the PHP language reference manual` |
|       - |  652 | ` * Heredoc` |
|       - |  653 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  654 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  655 | ` *  to close the quotation.` |
|       - |  656 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  657 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  658 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  659 | ` *  Warning` |
|       - |  660 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  661 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  662 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  663 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  664 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  665 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  666 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  667 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  668 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  669 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  670 | ` * Double quoted` |
|       - |  671 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  672 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  673 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  674 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  675 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  676 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  677 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  678 | ` *  \\ backslash` |
|       - |  679 | ` *  \$ dollar sign` |
|       - |  680 | ` *  \" double-quote` |
|       - |  681 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  682 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  683 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  684 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  685 | ` * See string parsing for details.` |
|       - |  686 | ` */` |
|   12092 |  687 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       1 |  688 |  |
|   12093 |  689 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  690 | `	const char *zIn,*zCur,*zEnd;` |
|   12093 |  691 | `	ph7_value *pObj = 0;` |
|       - |  692 | `	sxi32 iCons;` |
|       - |  693 | `	sxi32 rc;` |
|       - |  694 | `	/* Delimit the string */` |
|   12093 |  695 | `	zIn  = pStr->zString;` |
|   12093 |  696 | `	zEnd = &zIn[pStr->nByte];` |
|   12093 |  697 | `	if( zIn >= zEnd ){` |
|       - |  698 | `		/* Empty string,load NULL */` |
|     191 |  699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     191 |  700 | `		return SXRET_OK;` |
|       - |  701 | `	}` |
|   11903 |  702 | `	zCur = 0;` |
|       - |  703 | `	/* Compile the node */` |
|   11903 |  704 | `	iCons = 0;` |
|    6594 |  705 | `	for(;;){` |
|   19869 |  706 | `		zCur = zIn;` |
|  119443 |  707 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  100861 |  708 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  709 | `				break;` |
|  100777 |  710 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1204 |  711 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     601 |  712 | `					break;` |
|       - |  713 | `			}` |
|   99575 |  714 | `			zIn++;` |
|       1 |  715 | `		}` |
|   19869 |  716 | `		if( zIn > zCur ){` |
|    9895 |  717 | `			if( pObj == 0 ){` |
|    9671 |  718 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    9671 |  719 | `				if( pObj == 0 ){` |
|     ! 0 |  720 | `					return SXERR_ABORT;` |
|       - |  721 | `				}` |
|    4835 |  722 | `			}` |
|    9895 |  723 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    4947 |  724 | `		}` |
|   19869 |  725 | `		if( zIn >= zEnd ){` |
|   11903 |  726 | `			break;` |
|       - |  727 | `		}` |
|    7967 |  728 | `		if( zIn[0] == '\\' ){` |
|    6681 |  729 | `			const char *zPtr = 0;` |
|       - |  730 | `			sxu32 n;` |
|    6681 |  731 | `			zIn++;` |
|    6681 |  732 | `			if( zIn >= zEnd ){` |
|     ! 0 |  733 | `				break;` |
|       - |  734 | `			}` |
|    6681 |  735 | `			if( pObj == 0 ){` |
|    3393 |  736 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    3393 |  737 | `				if( pObj == 0 ){` |
|     ! 0 |  738 | `					return SXERR_ABORT;` |
|       - |  739 | `				}` |
|    1696 |  740 | `			}` |
|    6681 |  741 | `			n = sizeof(char); /* size of conversion */` |
|    6681 |  742 | `			switch( zIn[0] ){` |
|       3 |  743 | `			case '$':` |
|       - |  744 | `				/* Dollar sign */` |
|       7 |  745 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  746 | `				break;` |
|      31 |  747 | `			case '\\':` |
|       - |  748 | `				/* A literal backslash */` |
|      63 |  749 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      63 |  750 | `				break;` |
|       2 |  751 | `			case 'a':` |
|       - |  752 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  753 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  754 | `				break;` |
|       2 |  755 | `			case 'b':` |
|       - |  756 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  757 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  758 | `				break;` |
|       2 |  759 | `			case 'f':` |
|       - |  760 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       5 |  761 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       5 |  762 | `				break;` |
|    3011 |  763 | `			case 'n':` |
|       - |  764 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6023 |  765 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6023 |  766 | `				break;` |
|       6 |  767 | `			case 'r':` |
|       - |  768 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      13 |  769 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      13 |  770 | `				break;` |
|      18 |  771 | `			case 't':` |
|       - |  772 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      37 |  773 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      37 |  774 | `				break;` |
|       1 |  775 | `			case 'v':` |
|       - |  776 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       3 |  777 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       3 |  778 | `				break;` |
|       1 |  779 | `			case '\'':` |
|       - |  780 | `				/* Single quote */` |
|       3 |  781 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  782 | `				break;` |
|      47 |  783 | `			case '"':` |
|       - |  784 | `				/* Double quote */` |
|      95 |  785 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|      95 |  786 | `				break;` |
|       4 |  787 | `			case '0':` |
|       - |  788 | `				/* NUL byte */` |
|       9 |  789 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|       9 |  790 | `				break;` |
|     186 |  791 | `			case 'x':` |
|     373 |  792 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  793 | `					int c;` |
|       - |  794 | `					/* Hex digit */` |
|     359 |  795 | `					c = SyHexToint(zIn[1]) << 4;` |
|     359 |  796 | `					if( &zIn[2] < zEnd ){` |
|     359 |  797 | `						c +=  SyHexToint(zIn[2]);` |
|     179 |  798 | `					}` |
|       - |  799 | `					/* Output char */` |
|     359 |  800 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     359 |  801 | `					n += sizeof(char) * 2;` |
|     180 |  802 | `				}else{` |
|       - |  803 | `					/* Output literal character  */` |
|      15 |  804 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  805 | `				}` |
|     373 |  806 | `				break;` |
|      15 |  807 | `			case 'o':` |
|      31 |  808 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  809 | `					/* Octal digit stream */` |
|       - |  810 | `					int c;` |
|      21 |  811 | `					c = 0;` |
|      21 |  812 | `					zIn++;` |
|      61 |  813 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  814 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  815 | `							break;` |
|       - |  816 | `						}` |
|      41 |  817 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  818 | `					}` |
|      21 |  819 | `					if ( c > 0 ){` |
|      15 |  820 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  821 | `					}` |
|      21 |  822 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  823 | `				}else{` |
|       - |  824 | `					/* Output literal character  */` |
|      11 |  825 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  826 | `				}` |
|      31 |  827 | `				break;` |
|      11 |  828 | `			default:` |
|       - |  829 | `				/* Output without a slash */` |
|      23 |  830 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  831 | `				break;` |
|       - |  832 | `			}` |
|       - |  833 | `			/* Advance the stream cursor */` |
|    6681 |  834 | `			zIn += n;` |
|    6681 |  835 | `			continue;` |
|       - |  836 | `		}` |
|    1287 |  837 | `		if( zIn[0] == '{' ){` |
|       - |  838 | `			/* Curly syntax */` |
|       - |  839 | `			const char *zExpr;` |
|      87 |  840 | `			sxi32 iNest = 1;` |
|      87 |  841 | `			zIn++;` |
|      87 |  842 | `			zExpr = zIn;` |
|       - |  843 | `			/* Synchronize with the next closing curly braces */` |
|     985 |  844 | `			while( zIn < zEnd ){` |
|     985 |  845 | `				if( zIn[0] == '{' ){` |
|       - |  846 | `					/* Increment nesting level */` |
|       9 |  847 | `					iNest++;` |
|     981 |  848 | `				}else if(zIn[0] == '}' ){` |
|       - |  849 | `					/* Decrement nesting level */` |
|      95 |  850 | `					iNest--;` |
|      95 |  851 | `					if( iNest <= 0 ){` |
|      87 |  852 | `						break;` |
|       - |  853 | `					}` |
|       4 |  854 | `				}` |
|     899 |  855 | `				zIn++;` |
|       1 |  856 | `			}` |
|       - |  857 | `			/* Process the expression */` |
|      87 |  858 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      87 |  859 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  860 | `				return SXERR_ABORT;` |
|       - |  861 | `			}` |
|      87 |  862 | `			if( rc != SXERR_EMPTY ){` |
|      87 |  863 | `				++iCons;` |
|      43 |  864 | `			}` |
|      87 |  865 | `			if( zIn < zEnd ){` |
|       - |  866 | `				/* Jump the trailing curly */` |
|      87 |  867 | `				zIn++;` |
|      43 |  868 | `			}` |
|      44 |  869 | `		}else{` |
|       - |  870 | `			/* Simple syntax */` |
|    1201 |  871 | `			const char *zExpr = zIn;` |
|       - |  872 | `			/* Assemble variable name */` |
|     600 |  873 | `			for(;;){` |
|       - |  874 | `				/* Jump leading dollars */` |
|    2401 |  875 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1201 |  876 | `					zIn++;` |
|       1 |  877 | `				}` |
|     600 |  878 | `				for(;;){` |
|    7769 |  879 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    5969 |  880 | `						zIn++;` |
|       1 |  881 | `					}` |
|    1201 |  882 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  883 | `						/* UTF-8 stream */` |
|     ! 0 |  884 | `						zIn++;` |
|     ! 0 |  885 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  886 | `							zIn++;` |
|     ! 0 |  887 | `						}` |
|     ! 0 |  888 | `						continue;` |
|       - |  889 | `					}` |
|    1201 |  890 | `					break;` |
|     ! 0 |  891 | `				}` |
|    1201 |  892 | `				if( zIn >= zEnd ){` |
|      79 |  893 | `					break;` |
|       - |  894 | `				}` |
|    1123 |  895 | `				if( zIn[0] == '[' ){` |
|       9 |  896 | `					sxi32 iSquare = 1;` |
|       9 |  897 | `					zIn++;` |
|      17 |  898 | `					while( zIn < zEnd ){` |
|      17 |  899 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  900 | `							iSquare++;` |
|      17 |  901 | `						}else if (zIn[0] == ']' ){` |
|       9 |  902 | `							iSquare--;` |
|       9 |  903 | `							if( iSquare <= 0 ){` |
|       9 |  904 | `								break;` |
|       - |  905 | `							}` |
|     ! 0 |  906 | `						}` |
|       9 |  907 | `						zIn++;` |
|       1 |  908 | `					}` |
|       9 |  909 | `					if( zIn < zEnd ){` |
|       9 |  910 | `						zIn++;` |
|       4 |  911 | `					}` |
|       9 |  912 | `					break;` |
|    1115 |  913 | `				}else if(zIn[0] == '{' ){` |
|       5 |  914 | `					sxi32 iCurly = 1;` |
|       5 |  915 | `					zIn++;` |
|      17 |  916 | `					while( zIn < zEnd ){` |
|      15 |  917 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  918 | `							iCurly++;` |
|      15 |  919 | `						}else if (zIn[0] == '}' ){` |
|       3 |  920 | `							iCurly--;` |
|       3 |  921 | `							if( iCurly <= 0 ){` |
|       3 |  922 | `								break;` |
|       - |  923 | `							}` |
|     ! 0 |  924 | `						}` |
|      13 |  925 | `						zIn++;` |
|       1 |  926 | `					}` |
|       5 |  927 | `					if( zIn < zEnd ){` |
|       3 |  928 | `						zIn++;` |
|       1 |  929 | `					}` |
|       5 |  930 | `					break;` |
|    1111 |  931 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  932 | `					/* Member access operator '->' */` |
|     ! 0 |  933 | `					zIn += 2;` |
|    1111 |  934 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  935 | `					/* Static member access operator '::' */` |
|     ! 0 |  936 | `					zIn += 2;` |
|     ! 0 |  937 | `				}else{` |
|     556 |  938 | `					break;` |
|       - |  939 | `				}` |
|     ! 0 |  940 | `			}` |
|       - |  941 | `			/* Process the expression */` |
|    1201 |  942 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1201 |  943 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  944 | `				return SXERR_ABORT;` |
|       - |  945 | `			}` |
|    1201 |  946 | `			if( rc != SXERR_EMPTY ){` |
|    1199 |  947 | `				++iCons;` |
|     599 |  948 | `			}` |
|       - |  949 | `		}` |
|       - |  950 | `		/* Invalidate the previously used constant */` |
|    1287 |  951 | `		pObj = 0;` |
|       1 |  952 | `	}/*for(;;)*/` |
|   11903 |  953 | `	if( iCons > 1 ){` |
|       - |  954 | `		/* Concatenate all compiled constants */` |
|    1039 |  955 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     519 |  956 | `	}` |
|       - |  957 | `	/* Node successfully compiled */` |
|   11903 |  958 | `	return SXRET_OK;` |
|    6047 |  959 |  |
|       - |  960 | `/*` |
|       - |  961 | ` * Compile a double quoted string.` |
|       - |  962 | ` *  See the block-comment above for more information.` |
|       - |  963 | ` */` |
|   12062 |  964 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  965 |  |
|       - |  966 | `	sxi32 rc;` |
|   12063 |  967 | `	rc = GenStateCompileString(&(*pGen));` |
|    6031 |  968 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  969 | `	/* Compilation result */` |
|   12063 |  970 | `	return rc;` |
|       1 |  971 |  |
|       - |  972 | `/*` |
|       - |  973 | ` * Compile a Heredoc string.` |
|       - |  974 | ` *  See the block-comment above for more information.` |
|       - |  975 | ` */` |
|      30 |  976 | `static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  977 |  |
|      31 |  978 | `	GenStateCompileString(&(*pGen));` |
|      15 |  979 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  980 | `	/* Compilation result */` |
|      31 |  981 | `	return SXRET_OK;` |
|       1 |  982 |  |
|       - |  983 | `/*` |
|       - |  984 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  985 | ` *  Notes on array entries.` |
|       - |  986 | ` *  According to the PHP language reference manual` |
|       - |  987 | ` *  An array can be created by the array() language construct.` |
|       - |  988 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  989 | ` *  array(  key =>  value` |
|       - |  990 | ` *    , ...` |
|       - |  991 | ` *    )` |
|       - |  992 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  993 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  994 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  995 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  996 | ` *  contain integer and string indices.` |
|       - |  997 | ` *  A value can be any PHP type.` |
|       - |  998 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  999 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1000 | ` *  is specified, that value will be overwritten.` |
|       - | 1001 | ` */` |
|    2246 | 1002 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1003 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1004 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1005 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1006 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1007 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1008 | `	)` |
|       1 | 1009 |  |
|       - | 1010 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1011 | `	sxi32 rc;` |
|       - | 1012 | `	/* Swap token stream */` |
|    2247 | 1013 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1014 | `	/* Compile the expression*/` |
|    2247 | 1015 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1016 | `	/* Restore token stream */` |
|    2247 | 1017 | `	RE_SWAP_DELIMITER(pGen);` |
|    2247 | 1018 | `	return rc;` |
|       1 | 1019 |  |
|       - | 1020 | `/*` |
|       - | 1021 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1022 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1023 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1024 | ` * error message.` |
|       - | 1025 | ` * See the routine responible of compiling the array language construct` |
|       - | 1026 | ` * for more inforation.` |
|       - | 1027 | ` */` |
|      30 | 1028 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       1 | 1029 |  |
|      31 | 1030 | `	sxi32 rc = SXRET_OK;` |
|      31 | 1031 | `	if( pRoot->pOp ){` |
|      19 | 1032 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1033 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      13 | 1034 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1035 | `			/* Unexpected expression */` |
|      11 | 1036 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1037 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1038 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1039 | `				rc = SXERR_INVALID;` |
|       5 | 1040 | `			}` |
|       6 | 1041 | `		}` |
|      24 | 1042 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1043 | `		/* Unexpected expression */` |
|       3 | 1044 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1045 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1046 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1047 | `			rc = SXERR_INVALID;` |
|       1 | 1048 | `		}` |
|       1 | 1049 | `	}` |
|      31 | 1050 | `	return rc;` |
|       1 | 1051 |  |
|       - | 1052 | `/*` |
|       - | 1053 | ` * Compile the 'array' language construct.` |
|       - | 1054 | ` *	 According to the PHP language reference manual` |
|       - | 1055 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1056 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1057 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1058 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1059 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1060 | ` */` |
|   16860 | 1061 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1062 |  |
|       - | 1063 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1064 | `	SyToken *pKey,*pCur;` |
|   16861 | 1065 | `	sxi32 iEmitRef = 0;` |
|   16861 | 1066 | `	sxi32 nPair = 0;` |
|       - | 1067 | `	sxi32 iNest;` |
|       - | 1068 | `	sxi32 rc;` |
|       - | 1069 | `	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.` |
|       - | 1070 | `	 */` |
|   16861 | 1071 | `	pGen->pIn += 2;` |
|   16861 | 1072 | `	pGen->pEnd--;` |
|   16861 | 1073 | `	xValidator = 0;` |
|    8430 | 1074 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|    9249 | 1075 | `	for(;;){` |
|       - | 1076 | `		/* Jump leading commas */` |
|   19443 | 1077 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     945 | 1078 | `			pGen->pIn++;` |
|       1 | 1079 | `		}` |
|   18499 | 1080 | `		pCur = pGen->pIn;` |
|   18499 | 1081 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1082 | `			/* No more entry to process */` |
|   16847 | 1083 | `			break;` |
|       - | 1084 | `		}` |
|    1653 | 1085 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1086 | `			continue;` |
|       - | 1087 | `		}` |
|       - | 1088 | `		/* Compile the key if available */` |
|    1653 | 1089 | `		pKey = pCur;` |
|    1653 | 1090 | `		iNest = 0;` |
|    3685 | 1091 | `		while( pCur < pGen->pIn ){` |
|    2601 | 1092 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|     569 | 1093 | `				break;` |
|       - | 1094 | `			}` |
|    2033 | 1095 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      49 | 1096 | `				iNest++;` |
|    2009 | 1097 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1098 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1099 | `				 * parser will shortly detect any syntax error.` |
|       - | 1100 | `				 */` |
|      49 | 1101 | `				iNest--;` |
|      24 | 1102 | `			}` |
|    2033 | 1103 | `			pCur++;` |
|       1 | 1104 | `		}` |
|    1653 | 1105 | `		rc = SXERR_EMPTY;` |
|    1653 | 1106 | `		if( pCur < pGen->pIn ){` |
|     569 | 1107 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1108 | `				/* Missing value */` |
|      11 | 1109 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1110 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1111 | `					return SXERR_ABORT;` |
|       - | 1112 | `				}` |
|      11 | 1113 | `				return SXRET_OK;` |
|       - | 1114 | `			}` |
|       - | 1115 | `			/* Compile the expression holding the key */` |
|     559 | 1116 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1117 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|     559 | 1118 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1119 | `				return SXERR_ABORT;` |
|       - | 1120 | `			}` |
|     559 | 1121 | `			pCur++; /* Jump the '=>' operator */` |
|    1364 | 1122 | `		}else if( pKey == pCur ){` |
|       - | 1123 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1124 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1125 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1126 | `		}else{` |
|       - | 1127 | `			/* Reset back the cursor and point to the entry value */` |
|    1085 | 1128 | `			pCur = pKey;` |
|       - | 1129 | `		}` |
|    1643 | 1130 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1131 | `			/* No available key,load NULL */` |
|    1087 | 1132 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|     543 | 1133 | `		}` |
|    1643 | 1134 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1135 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      35 | 1136 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      35 | 1137 | `			iEmitRef = 1;` |
|      35 | 1138 | `			pCur++; /* Jump the '&' token */` |
|      35 | 1139 | `			if( pCur >= pGen->pIn ){` |
|       - | 1140 | `				/* Missing value */` |
|       5 | 1141 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       5 | 1142 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1143 | `					return SXERR_ABORT;` |
|       - | 1144 | `				}` |
|       5 | 1145 | `				return SXRET_OK;` |
|       - | 1146 | `			}` |
|      15 | 1147 | `		}` |
|       - | 1148 | `		/* Compile indice value */` |
|    1639 | 1149 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|    1639 | 1150 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1151 | `			return SXERR_ABORT;` |
|       - | 1152 | `		}` |
|    1639 | 1153 | `		if( iEmitRef ){` |
|       - | 1154 | `			/* Emit the load reference instruction */` |
|      31 | 1155 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1156 | `		}` |
|    1639 | 1157 | `		xValidator = 0;` |
|    1639 | 1158 | `		iEmitRef = 0;` |
|    1639 | 1159 | `		nPair++;` |
|       1 | 1160 | `	}` |
|       - | 1161 | `	/* Emit the load map instruction */` |
|   16847 | 1162 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1163 | `	/* Node successfully compiled */` |
|   16847 | 1164 | `	return SXRET_OK;` |
|    8431 | 1165 |  |
|       - | 1166 | `/*` |
|       - | 1167 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1168 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1169 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1170 | ` * error message.` |
|       - | 1171 | ` * See the routine responible of compiling the list language construct` |
|       - | 1172 | ` * for more inforation.` |
|       - | 1173 | ` */` |
|      50 | 1174 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       1 | 1175 |  |
|      51 | 1176 | `	sxi32 rc = SXRET_OK;` |
|      51 | 1177 | `	if( pRoot->pOp ){` |
|     ! 0 | 1178 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1179 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1180 | `				/* Unexpected expression */` |
|     ! 0 | 1181 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1182 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1183 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1184 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1185 | `				}` |
|     ! 0 | 1186 | `		}` |
|      51 | 1187 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1188 | `		/* Unexpected expression */` |
|       3 | 1189 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1190 | `			"list(): Expecting a variable not an expression");` |
|       3 | 1191 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1192 | `			rc = SXERR_INVALID;` |
|       1 | 1193 | `		}` |
|       1 | 1194 | `	}` |
|      51 | 1195 | `	return rc;` |
|       1 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Compile the 'list' language construct.` |
|       - | 1199 | ` *  According to the PHP language reference` |
|       - | 1200 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1201 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1202 | ` *  Description` |
|       - | 1203 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1204 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1205 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1206 | ` *  Parameters` |
|       - | 1207 | ` *   $varname: A variable.` |
|       - | 1208 | ` *  Return Values` |
|       - | 1209 | ` *   The assigned array.` |
|       - | 1210 | ` */` |
|      24 | 1211 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1212 |  |
|       - | 1213 | `	SyToken *pNext;` |
|       - | 1214 | `	sxi32 nExpr;` |
|       - | 1215 | `	sxi32 rc;` |
|      25 | 1216 | `	nExpr = 0;` |
|       - | 1217 | `	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */` |
|      25 | 1218 | `	pGen->pIn += 2;` |
|      25 | 1219 | `	pGen->pEnd--;` |
|      12 | 1220 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      79 | 1221 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|      55 | 1222 | `		if( pGen->pIn < pNext ){` |
|       - | 1223 | `			/* Compile the expression holding the variable */` |
|      51 | 1224 | `			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      51 | 1225 | `			if( rc != SXRET_OK ){` |
|       - | 1226 | `				/* Do not bother compiling this expression, it's broken anyway */` |
|     ! 0 | 1227 | `				return SXRET_OK;` |
|       - | 1228 | `			}` |
|      26 | 1229 | `		}else{` |
|       - | 1230 | `			/* Empty entry,load NULL */` |
|       5 | 1231 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1232 | `		}` |
|      55 | 1233 | `		nExpr++;` |
|       - | 1234 | `		/* Advance the stream cursor */` |
|      55 | 1235 | `		pGen->pIn = &pNext[1];` |
|       1 | 1236 | `	}` |
|       - | 1237 | `	/* Emit the LOAD_LIST instruction */` |
|      25 | 1238 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1239 | `	/* Node successfully compiled */` |
|      25 | 1240 | `	return SXRET_OK;` |
|      13 | 1241 |  |
|       - | 1242 | `/* Forward declaration */` |
|       - | 1243 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1244 | `/*` |
|       - | 1245 | ` * Compile an annoynmous function or a closure.` |
|       - | 1246 | ` * According to the PHP language reference` |
|       - | 1247 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1248 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1249 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1250 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1251 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1252 | ` *  Example Anonymous function variable assignment example` |
|       - | 1253 | ` * <?php` |
|       - | 1254 | ` * $greet = function($name)` |
|       - | 1255 | ` * {` |
|       - | 1256 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1257 | ` * };` |
|       - | 1258 | ` * $greet('World');` |
|       - | 1259 | ` * $greet('PHP');` |
|       - | 1260 | ` * ?>` |
|       - | 1261 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1262 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1263 | ` */` |
|      64 | 1264 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1265 |  |
|       - | 1266 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1267 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1268 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1269 | `							  * one thread is allowed to compile the script.` |
|       - | 1270 | `						      */` |
|       - | 1271 | `	ph7_value *pObj;` |
|       - | 1272 | `	SyString sName;` |
|       - | 1273 | `	sxu32 nIdx;` |
|       - | 1274 | `	sxu32 nLen;` |
|       - | 1275 | `	sxi32 rc;` |
|       - | 1276 |  |
|      65 | 1277 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      65 | 1278 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1279 | `		pGen->pIn++;` |
|     ! 0 | 1280 | `	}` |
|       - | 1281 | `	/* Reserve a constant for the lambda */` |
|      65 | 1282 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      65 | 1283 | `	if( pObj == 0 ){` |
|     ! 0 | 1284 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1285 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1286 | `		return SXERR_ABORT;` |
|       - | 1287 | `	}` |
|       - | 1288 | `	/* Generate a unique name */` |
|      65 | 1289 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1290 | `	/* Make sure the generated name is unique */` |
|      65 | 1291 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1292 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1293 | `	}` |
|      65 | 1294 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|      65 | 1295 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1296 | `	/* Compile the lambda body */` |
|      65 | 1297 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|      65 | 1298 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1299 | `		return SXERR_ABORT;` |
|       - | 1300 | `	}` |
|      65 | 1301 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1302 | `		/* Emit the load closure instruction */` |
|       7 | 1303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       4 | 1304 | `	}else{` |
|       - | 1305 | `		/* Emit the load constant instruction */` |
|      59 | 1306 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1307 | `	}` |
|       - | 1308 | `	/* Node successfully compiled */` |
|      65 | 1309 | `	return SXRET_OK;` |
|      33 | 1310 |  |
|       - | 1311 | `/*` |
|       - | 1312 | ` * Compile a backtick quoted string.` |
|       - | 1313 | ` */` |
|       4 | 1314 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1315 |  |
|       - | 1316 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1317 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1318 | `	 */` |
|       7 | 1319 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1320 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1321 | `		ph7_lib_version()` |
|       - | 1322 | `		);` |
|       - | 1323 | `	/* Load NULL */` |
|       5 | 1324 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1325 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1326 | `	/* Node successfully compiled */` |
|       5 | 1327 | `	return SXRET_OK;` |
|       1 | 1328 |  |
|       - | 1329 | `/*` |
|       - | 1330 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1331 | ` * construct.` |
|       - | 1332 | ` */` |
|      30 | 1333 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1334 |  |
|       - | 1335 | `	SyString *pName;` |
|       - | 1336 | `	sxu32 nKeyID;` |
|       - | 1337 | `	sxi32 rc;` |
|       - | 1338 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      31 | 1339 | `	pName = &pGen->pIn->sData;` |
|      31 | 1340 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      31 | 1341 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      31 | 1342 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1343 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1344 | `		/* Compile arguments one after one */` |
|       9 | 1345 | `		pTmp = pGen->pEnd;` |
|       - | 1346 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1347 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1348 | `		 *  mean that the following expression is valid:` |
|       - | 1349 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1350 | `		 */` |
|       9 | 1351 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1352 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1353 | `			if( pGen->pIn < pNext ){` |
|       9 | 1354 | `				pGen->pEnd = pNext;` |
|       9 | 1355 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1356 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1357 | `					return SXERR_ABORT;` |
|       - | 1358 | `				}` |
|       9 | 1359 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1360 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1361 | `					 * without the overhead of a function call.` |
|       - | 1362 | `					 * This is a very powerful optimization that improve` |
|       - | 1363 | `					 * performance greatly.` |
|       - | 1364 | `					 */` |
|       9 | 1365 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1366 | `				}` |
|       4 | 1367 | `			}` |
|       - | 1368 | `			/* Jump trailing commas */` |
|       9 | 1369 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1370 | `				pNext++;` |
|     ! 0 | 1371 | `			}` |
|       9 | 1372 | `			pGen->pIn = pNext;` |
|       1 | 1373 | `		}` |
|       - | 1374 | `		/* Restore token stream */` |
|       9 | 1375 | `		pGen->pEnd = pTmp;` |
|       5 | 1376 | `	}else{` |
|      23 | 1377 | `		sxi32 nArg = 0;` |
|      23 | 1378 | `		sxu32 nIdx = 0;` |
|      23 | 1379 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|      23 | 1380 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1381 | `			return SXERR_ABORT;` |
|      23 | 1382 | `		}else if(rc != SXERR_EMPTY ){` |
|      23 | 1383 | `			nArg = 1;` |
|      11 | 1384 | `		}` |
|      23 | 1385 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1386 | `			ph7_value *pObj;` |
|       - | 1387 | `			/* Emit the call instruction */` |
|      17 | 1388 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      17 | 1389 | `			if( pObj == 0 ){` |
|     ! 0 | 1390 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1391 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1392 | `				return SXERR_ABORT;` |
|       - | 1393 | `			}` |
|      17 | 1394 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1395 | `			/* Install in the literal table */` |
|      17 | 1396 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 | 1397 | `		}` |
|       - | 1398 | `		/* Emit the call instruction */` |
|      23 | 1399 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      23 | 1400 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1401 | `	}` |
|       - | 1402 | `	/* Node successfully compiled */` |
|      31 | 1403 | `	return SXRET_OK;` |
|      16 | 1404 |  |
|       - | 1405 | `/*` |
|       - | 1406 | ` * Compile a node holding a variable declaration.` |
|       - | 1407 | ` * According to the PHP language reference` |
|       - | 1408 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1409 | ` *  The variable name is case-sensitive.` |
|       - | 1410 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1411 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1412 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1413 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1414 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1415 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1416 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1417 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1418 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1419 | ` *  the chapter on Expressions.` |
|       - | 1420 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1421 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1422 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1423 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1424 | ` *  is being assigned (the source variable).` |
|       - | 1425 | ` */` |
| 1274222 | 1426 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1427 |  |
| 1274223 | 1428 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1429 | `	sxi32 iVv;` |
|       - | 1430 | `	sxi32 iP1;` |
|       - | 1431 | `	void *p3;` |
|       - | 1432 | `	sxi32 rc;` |
| 1274223 | 1433 | `	iVv = -1; /* Variable variable counter */` |
| 2548457 | 1434 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1274235 | 1435 | `		pGen->pIn++;` |
| 1274235 | 1436 | `		iVv++;` |
|       1 | 1437 | `	}` |
| 1274223 | 1438 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1439 | `		/* Invalid variable name */` |
|     ! 0 | 1440 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1441 | `		if( rc == SXERR_ABORT ){` |
|       - | 1442 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1443 | `			return SXERR_ABORT;` |
|       - | 1444 | `		}` |
|     ! 0 | 1445 | `		return SXRET_OK;` |
|       - | 1446 | `	}` |
| 1274223 | 1447 | `	p3  = 0;` |
| 1274223 | 1448 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1449 | `		/* Dynamic variable creation */` |
|      15 | 1450 | `		pGen->pIn++;  /* Jump the open curly */` |
|      15 | 1451 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      15 | 1452 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1453 | `			/* Empty expression */` |
|       3 | 1454 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1455 | `			return SXRET_OK;` |
|       - | 1456 | `		}` |
|       - | 1457 | `		/* Compile the expression holding the variable name */` |
|      13 | 1458 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      13 | 1459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1460 | `			return SXERR_ABORT;` |
|      13 | 1461 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1462 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1463 | `			return SXRET_OK;` |
|       - | 1464 | `		}` |
|       6 | 1465 | `	}else{` |
|       - | 1466 | `		SyHashEntry *pEntry;` |
|       - | 1467 | `		SyString *pName;` |
| 1274209 | 1468 | `		char *zName = 0;` |
|       - | 1469 | `		/* Extract variable name */` |
| 1274209 | 1470 | `		pName = &pGen->pIn->sData;` |
|       - | 1471 | `		/* Advance the stream cursor */` |
| 1274209 | 1472 | `		pGen->pIn++;` |
| 1274209 | 1473 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1274209 | 1474 | `		if( pEntry == 0 ){` |
|       - | 1475 | `			/* Duplicate name */` |
|  202665 | 1476 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  202665 | 1477 | `			if( zName == 0 ){` |
|     ! 0 | 1478 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1479 | `				return SXERR_ABORT;` |
|       - | 1480 | `			}` |
|       - | 1481 | `			/* Install in the hashtable */` |
|  202665 | 1482 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  101333 | 1483 | `		}else{` |
|       - | 1484 | `			/* Name already available */` |
| 1071545 | 1485 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1486 | `		}` |
| 1274209 | 1487 | `		p3 = (void *)zName;` |
|       - | 1488 | `	}` |
| 1274219 | 1489 | `	iP1 = 0;` |
| 1274219 | 1490 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  385295 | 1491 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1492 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  385291 | 1493 | `			iP1 = 1;` |
|  192645 | 1494 | `		}` |
|  192647 | 1495 | `	}` |
|       - | 1496 | `	/* Emit the load instruction */` |
| 1274219 | 1497 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1274231 | 1498 | `	while( iVv > 0 ){` |
|      13 | 1499 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1500 | `		iVv--;` |
|       1 | 1501 | `	}` |
|       - | 1502 | `	/* Node successfully compiled */` |
| 1274219 | 1503 | `	return SXRET_OK;` |
|  637112 | 1504 |  |
|       - | 1505 | `/*` |
|       - | 1506 | ` * Load a literal.` |
|       - | 1507 | ` */` |
|  843858 | 1508 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       1 | 1509 |  |
|  843859 | 1510 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1511 | `	ph7_value *pObj;` |
|       - | 1512 | `	SyString *pStr;` |
|       - | 1513 | `	sxu32 nIdx;` |
|       - | 1514 | `	/* Extract token value */` |
|  843859 | 1515 | `	pStr = &pToken->sData;` |
|       - | 1516 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  843859 | 1517 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  177081 | 1518 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1519 | `			/* NULL constant are always indexed at 0 */` |
|   69537 | 1520 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   69537 | 1521 | `			return SXRET_OK;` |
|  107545 | 1522 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1523 | `			/* TRUE constant are always indexed at 1 */` |
|     405 | 1524 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     405 | 1525 | `			return SXRET_OK;` |
|       1 | 1526 | `		}` |
|  804015 | 1527 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  167332 | 1528 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1529 | `			/* FALSE constant are always indexed at 2 */` |
|   69773 | 1530 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   69773 | 1531 | `			return SXRET_OK;` |
|  658710 | 1532 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  123406 | 1533 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1534 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10671 | 1535 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10671 | 1536 | `			if( pObj == 0 ){` |
|     ! 0 | 1537 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1538 | `				return SXERR_ABORT;` |
|       - | 1539 | `			}` |
|   10671 | 1540 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1541 | `			/* Emit the load constant instruction */` |
|   10671 | 1542 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10671 | 1543 | `			return SXRET_OK;` |
|  605371 | 1544 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   16351 | 1545 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  597189 | 1546 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   21720 | 1547 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1548 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1549 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1550 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1551 | `				/* Point to the upper block */` |
|      11 | 1552 | `				pBlock = pBlock->pParent;` |
|       1 | 1553 | `			}` |
|      11 | 1554 | `			if( pBlock == 0 ){` |
|       - | 1555 | `				/* Called in the global scope,load NULL */` |
|       5 | 1556 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1557 | `			}else{` |
|       - | 1558 | `				/* Extract the target function/method */` |
|       7 | 1559 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1560 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1561 | `					/* Not a class method,Load null */` |
|       3 | 1562 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1563 | `				}else{` |
|       5 | 1564 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1565 | `					if( pObj == 0 ){` |
|     ! 0 | 1566 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1567 | `						return SXERR_ABORT;` |
|       - | 1568 | `					}` |
|       5 | 1569 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1570 | `					/* Emit the load constant instruction */` |
|       5 | 1571 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1572 | `				}` |
|       - | 1573 | `			}` |
|      11 | 1574 | `			return SXRET_OK;` |
|       - | 1575 | `	}` |
|       - | 1576 | `	/* Query literal table */` |
|  693467 | 1577 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1578 | `		ph7_value *pLitObj;` |
|       - | 1579 | `		/* Unknown literal,install it in the literal table */` |
|  289917 | 1580 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  289917 | 1581 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1582 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1583 | `			return SXERR_ABORT;` |
|       - | 1584 | `		}` |
|  289917 | 1585 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  289917 | 1586 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  144958 | 1587 | `	}` |
|       - | 1588 | `	/* Emit the load constant instruction */` |
|  693467 | 1589 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  693467 | 1590 | `	return SXRET_OK;` |
|  421930 | 1591 |  |
|       - | 1592 | `/*` |
|       - | 1593 | ` * Resolve a namespace path or simply load a literal:` |
|       - | 1594 | ` * As of this version namespace support is disabled. If you need` |
|       - | 1595 | ` * a working version that implement namespace,please contact` |
|       - | 1596 | ` * symisc systems via contact@symisc.net` |
|       - | 1597 | ` */` |
|  843858 | 1598 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       1 | 1599 |  |
|  843859 | 1600 | `	int emit = 0;` |
|       - | 1601 | `	sxi32 rc;` |
|  843877 | 1602 | `	while( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1603 | `		/* Emit a warning */` |
|      19 | 1604 | `		if( !emit ){` |
|       4 | 1605 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 1606 | `				"Namespace support is disabled in the current release of the PH7(%s) engine",` |
|       1 | 1607 | `				ph7_lib_version()` |
|       - | 1608 | `				);` |
|       3 | 1609 | `			emit = 1;` |
|       1 | 1610 | `		}` |
|      19 | 1611 | `		pGen->pIn++; /* Ignore the token */` |
|       1 | 1612 | `	}` |
|       - | 1613 | `	/* Load literal */` |
|  843859 | 1614 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  843859 | 1615 | `	return rc;` |
|       1 | 1616 |  |
|       - | 1617 | `/*` |
|       - | 1618 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1619 | ` */` |
|  843858 | 1620 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1621 |  |
|       - | 1622 | `	sxi32 rc;` |
|  843859 | 1623 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  843859 | 1624 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1625 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1626 | `		return rc;` |
|       - | 1627 | `	}` |
|       - | 1628 | `	/* Node successfully compiled */` |
|  843859 | 1629 | `	return SXRET_OK;` |
|  421930 | 1630 |  |
|       - | 1631 | `/*` |
|       - | 1632 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1633 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1634 | ` */` |
|       6 | 1635 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1636 |  |
|       - | 1637 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      13 | 1638 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       7 | 1639 | `		pGen->pIn++;` |
|       1 | 1640 | `	}` |
|       7 | 1641 | `	return SXRET_OK;` |
|       1 | 1642 |  |
|       - | 1643 | `/*` |
|       - | 1644 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1645 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1646 | ` */` |
|      30 | 1647 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       1 | 1648 |  |
|      31 | 1649 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      11 | 1650 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1651 | `			return TRUE;` |
|       9 | 1652 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1653 | `			return TRUE;` |
|       1 | 1654 | `		}` |
|      23 | 1655 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1656 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1657 | `			return TRUE;` |
|       - | 1658 | `		}` |
|     ! 0 | 1659 | `	}` |
|       - | 1660 | `	/* Not a reserved constant */` |
|      23 | 1661 | `	return FALSE;` |
|      16 | 1662 |  |
|       - | 1663 | `/*` |
|       - | 1664 | ` * Compile the 'const' statement.` |
|       - | 1665 | ` * According to the PHP language reference` |
|       - | 1666 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1667 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1668 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1669 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1670 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1671 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1672 | ` *  Syntax` |
|       - | 1673 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1674 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1675 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1676 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1677 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1678 | ` *  to get a list of all defined constants.` |
|       - | 1679 | ` *` |
|       - | 1680 | ` * Symisc eXtension.` |
|       - | 1681 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1682 | ` *  would allow only simple scalar value.` |
|       - | 1683 | ` *  Example` |
|       - | 1684 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1685 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1686 | ` */` |
|      26 | 1687 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       1 | 1688 |  |
|       - | 1689 | `	SySet *pConsCode,*pInstrContainer;` |
|      27 | 1690 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1691 | `	SyString *pName;` |
|       - | 1692 | `	sxi32 rc;` |
|      27 | 1693 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      27 | 1694 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1695 | `		/* Invalid constant name */` |
|       7 | 1696 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1697 | `		if( rc == SXERR_ABORT ){` |
|       - | 1698 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1699 | `			return SXERR_ABORT;` |
|       - | 1700 | `		}` |
|       7 | 1701 | `		goto Synchronize;` |
|       - | 1702 | `	}` |
|       - | 1703 | `	/* Peek constant name */` |
|      21 | 1704 | `	pName = &pGen->pIn->sData;` |
|       - | 1705 | `	/* Make sure the constant name isn't reserved */` |
|      21 | 1706 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1707 | `		/* Reserved constant */` |
|       9 | 1708 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1709 | `		if( rc == SXERR_ABORT ){` |
|       - | 1710 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1711 | `			return SXERR_ABORT;` |
|       - | 1712 | `		}` |
|       9 | 1713 | `		goto Synchronize;` |
|       - | 1714 | `	}` |
|      13 | 1715 | `	pGen->pIn++;` |
|      13 | 1716 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1717 | `		/* Invalid statement*/` |
|       5 | 1718 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1719 | `		if( rc == SXERR_ABORT ){` |
|       - | 1720 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1721 | `			return SXERR_ABORT;` |
|       - | 1722 | `		}` |
|       5 | 1723 | `		goto Synchronize;` |
|       - | 1724 | `	}` |
|       9 | 1725 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1726 | `	/* Allocate a new constant value container */` |
|       9 | 1727 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       9 | 1728 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1729 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1730 | `		return SXERR_ABORT;` |
|       - | 1731 | `	}` |
|       9 | 1732 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1733 | `	/* Swap bytecode container */` |
|       9 | 1734 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       9 | 1735 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1736 | `	/* Compile constant value */` |
|       9 | 1737 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1738 | `	/* Emit the done instruction */` |
|       9 | 1739 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       9 | 1740 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       9 | 1741 | `	if( rc == SXERR_ABORT ){` |
|       - | 1742 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1743 | `		return SXERR_ABORT;` |
|       - | 1744 | `	}` |
|       9 | 1745 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1746 | `	/* Register the constant */` |
|       9 | 1747 | `	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);` |
|       9 | 1748 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1749 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1750 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1751 | `	}` |
|       9 | 1752 | `	return SXRET_OK;` |
|       9 | 1753 | `Synchronize:` |
|       - | 1754 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1755 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1756 | `		pGen->pIn++;` |
|       1 | 1757 | `	}` |
|      19 | 1758 | `	return SXRET_OK;` |
|      14 | 1759 |  |
|       - | 1760 | `/*` |
|       - | 1761 | ` * Compile the 'continue' statement.` |
|       - | 1762 | ` * According to the PHP language reference` |
|       - | 1763 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1764 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1765 | ` *  iteration.` |
|       - | 1766 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1767 | ` *  the purposes of continue.` |
|       - | 1768 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1769 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1770 | ` *  Note:` |
|       - | 1771 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1772 | ` */` |
|   10700 | 1773 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       1 | 1774 |  |
|       - | 1775 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1776 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1777 | `	sxu32 nLineLocal;` |
|       - | 1778 | `	sxi32 rc;` |
|   10701 | 1779 | `	nLineLocal = pGen->pIn->nLine;` |
|   10701 | 1780 | `	iLevel = 0;` |
|       - | 1781 | `	/* Jump the 'continue' keyword */` |
|   10701 | 1782 | `	pGen->pIn++;` |
|   10701 | 1783 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1784 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1785 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1786 | `		 */` |
|      11 | 1787 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      11 | 1788 | `		if( iLevel < 2 ){` |
|     ! 0 | 1789 | `			iLevel = 0;` |
|     ! 0 | 1790 | `		}` |
|      11 | 1791 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1792 | `	}` |
|       - | 1793 | `	/* Point to the target loop */` |
|   10701 | 1794 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   10701 | 1795 | `	if( pLoop == 0 ){` |
|       - | 1796 | `		/* Illegal continue */` |
|      11 | 1797 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1798 | `		if( rc == SXERR_ABORT ){` |
|       - | 1799 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1800 | `			return SXERR_ABORT;` |
|       - | 1801 | `		}` |
|       6 | 1802 | `	}else{` |
|   10691 | 1803 | `		sxu32 nInstrIdx = 0;` |
|   10691 | 1804 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 1805 | `			/* According to the PHP language reference manual` |
|       - | 1806 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 1807 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 1808 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 1809 | `			 */` |
|       5 | 1810 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 1811 | `			if( rc == SXRET_OK ){` |
|       5 | 1812 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 1813 | `			}` |
|       3 | 1814 | `		}else{` |
|       - | 1815 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|   10687 | 1816 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   10687 | 1817 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1818 | `				JumpFixup sJumpFix;` |
|       - | 1819 | `				/* Post-continue */` |
|    5343 | 1820 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    5343 | 1821 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    5343 | 1822 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    2671 | 1823 | `			}` |
|       - | 1824 | `		}` |
|       - | 1825 | `	}` |
|   10701 | 1826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1827 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1828 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1829 | `	}` |
|       - | 1830 | `	/* Statement successfully compiled */` |
|   10701 | 1831 | `	return SXRET_OK;` |
|    5351 | 1832 |  |
|       - | 1833 | `/*` |
|       - | 1834 | ` * Compile the 'break' statement.` |
|       - | 1835 | ` * According to the PHP language reference` |
|       - | 1836 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 1837 | ` *  structure.` |
|       - | 1838 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 1839 | ` *  enclosing structures are to be broken out of.` |
|       - | 1840 | ` */` |
|      62 | 1841 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       1 | 1842 |  |
|       - | 1843 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1844 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1845 | `	sxi32 rc;` |
|      63 | 1846 | `	iLevel = 0;` |
|       - | 1847 | `	/* Jump the 'break' keyword */` |
|      63 | 1848 | `	pGen->pIn++;` |
|      63 | 1849 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 1850 | `		/* optional numeric argument which tells us how many levels` |
|       - | 1851 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 1852 | `		 */` |
|      11 | 1853 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      11 | 1854 | `		if( iLevel < 2 ){` |
|     ! 0 | 1855 | `			iLevel = 0;` |
|     ! 0 | 1856 | `		}` |
|      11 | 1857 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 1858 | `	}` |
|       - | 1859 | `	/* Extract the target loop */` |
|      63 | 1860 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|      63 | 1861 | `	if( pLoop == 0 ){` |
|       - | 1862 | `		/* Illegal break */` |
|      17 | 1863 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 1864 | `		if( rc == SXERR_ABORT ){` |
|       - | 1865 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1866 | `			return SXERR_ABORT;` |
|       - | 1867 | `		}` |
|       9 | 1868 | `	}else{` |
|       - | 1869 | `		sxu32 nInstrIdx;` |
|      47 | 1870 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      47 | 1871 | `		if( rc == SXRET_OK ){` |
|       - | 1872 | `			/* Fix the jump later when the jump destination is resolved */` |
|      47 | 1873 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      23 | 1874 | `		}` |
|       - | 1875 | `	}` |
|      63 | 1876 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1877 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1878 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 1879 | `	}` |
|       - | 1880 | `	/* Statement successfully compiled */` |
|      63 | 1881 | `	return SXRET_OK;` |
|      32 | 1882 |  |
|       - | 1883 | `/*` |
|       - | 1884 | ` * Compile or record a label.` |
|       - | 1885 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 1886 | ` * Example` |
|       - | 1887 | ` *  goto LABEL;` |
|       - | 1888 | ` *   echo 'Foo';` |
|       - | 1889 | ` *  LABEL:` |
|       - | 1890 | ` *   echo 'Bar';` |
|       - | 1891 | ` */` |
|     112 | 1892 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       1 | 1893 |  |
|       - | 1894 | `	GenBlock *pBlock;` |
|       - | 1895 | `	Label sLabel;` |
|       - | 1896 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     113 | 1897 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     113 | 1898 | `	if( pBlock ){` |
|       - | 1899 | `		sxi32 rc;` |
|       7 | 1900 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 1901 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 1902 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1903 | `			return SXERR_ABORT;` |
|       - | 1904 | `		}` |
|       3 | 1905 | `	}else{` |
|     109 | 1906 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1907 | `		char *zDup;` |
|       - | 1908 | `		/* Initialize label fields */` |
|     109 | 1909 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 1910 | `		/* Duplicate label name */` |
|     109 | 1911 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     109 | 1912 | `		if( zDup == 0 ){` |
|     ! 0 | 1913 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1914 | `			return SXERR_ABORT;` |
|       - | 1915 | `		}` |
|     109 | 1916 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     109 | 1917 | `		sLabel.bRef  = FALSE;` |
|     109 | 1918 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     109 | 1919 | `		pBlock = pGen->pCurrent;` |
|     217 | 1920 | `		while( pBlock ){` |
|     129 | 1921 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      21 | 1922 | `				break;` |
|       - | 1923 | `			}` |
|       - | 1924 | `			/* Point to the upper block */` |
|     109 | 1925 | `			pBlock = pBlock->pParent;` |
|       1 | 1926 | `		}` |
|     109 | 1927 | `		if( pBlock ){` |
|      21 | 1928 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      11 | 1929 | `		}else{` |
|      89 | 1930 | `			sLabel.pFunc = 0;` |
|       - | 1931 | `		}` |
|       - | 1932 | `		/* Insert in label set */` |
|     109 | 1933 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 1934 | `	}` |
|     113 | 1935 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     113 | 1936 | `	return SXRET_OK;` |
|      57 | 1937 |  |
|       - | 1938 | `/*` |
|       - | 1939 | ` * Compile the so hated 'goto' statement.` |
|       - | 1940 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 1941 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 1942 | ` * a compiler it has to do this.` |
|       - | 1943 | ` * According to the PHP language reference manual` |
|       - | 1944 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 1945 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 1946 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 1947 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 1948 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 1949 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 1950 | ` *   of a multi-level break` |
|       - | 1951 | ` */` |
|     152 | 1952 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       1 | 1953 |  |
|       - | 1954 | `	JumpFixup sJump;` |
|       - | 1955 | `	sxi32 rc;` |
|     153 | 1956 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     153 | 1957 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1958 | `		/* Missing label */` |
|     ! 0 | 1959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 1960 | `		if( rc == SXERR_ABORT ){` |
|       - | 1961 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1962 | `			return SXERR_ABORT;` |
|       - | 1963 | `		}` |
|     ! 0 | 1964 | `		return SXRET_OK;` |
|       - | 1965 | `	}` |
|     153 | 1966 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 1967 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 1968 | `		if( rc == SXERR_ABORT ){` |
|       - | 1969 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1970 | `			return SXERR_ABORT;` |
|       - | 1971 | `		}` |
|       3 | 1972 | `	}else{` |
|     149 | 1973 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 1974 | `		GenBlock *pBlock;` |
|       - | 1975 | `		char *zDup;` |
|       - | 1976 | `		/* Prepare the jump destination */` |
|     149 | 1977 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     149 | 1978 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 1979 | `		/* Duplicate label name */` |
|     149 | 1980 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     149 | 1981 | `		if( zDup == 0 ){` |
|     ! 0 | 1982 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 1983 | `			return SXERR_ABORT;` |
|       - | 1984 | `		}` |
|     149 | 1985 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     149 | 1986 | `		pBlock = pGen->pCurrent;` |
|     311 | 1987 | `		while( pBlock ){` |
|     195 | 1988 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      33 | 1989 | `				break;` |
|       - | 1990 | `			}` |
|       - | 1991 | `			/* Point to the upper block */` |
|     163 | 1992 | `			pBlock = pBlock->pParent;` |
|       1 | 1993 | `		}` |
|     149 | 1994 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 1995 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 1996 | `			if( rc == SXERR_ABORT ){` |
|       - | 1997 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 1998 | `				return SXERR_ABORT;` |
|       - | 1999 | `			}` |
|       3 | 2000 | `		}` |
|     149 | 2001 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      27 | 2002 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      14 | 2003 | `		}else{` |
|     123 | 2004 | `			sJump.pFunc = 0;` |
|       - | 2005 | `		}` |
|       - | 2006 | `		/* Emit the unconditional jump */` |
|     149 | 2007 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     149 | 2008 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2009 | `		}` |
|       - | 2010 | `	}` |
|     153 | 2011 | `	pGen->pIn++; /* Jump the label name */` |
|     153 | 2012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2013 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2014 | `	}` |
|       - | 2015 | `	/* Statement successfully compiled */` |
|     153 | 2016 | `	return SXRET_OK;` |
|      77 | 2017 |  |
|       - | 2018 | `/*` |
|       - | 2019 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2020 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2021 | ` * failure.` |
|       - | 2022 | ` */` |
|      20 | 2023 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2024 |  |
|       - | 2025 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2026 | `	sxu32 nRawObj;` |
|      10 | 2027 | `	sxu32 nObjIdx;` |
|       - | 2028 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2029 | `	 * a PHP block.` |
|       - | 2030 | `	 */` |
|      10 | 2031 | `Consume:` |
|      21 | 2032 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2033 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2034 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2035 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2036 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2037 | `			return SXERR_ABORT;` |
|       - | 2038 | `		}` |
|       - | 2039 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2040 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2041 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2042 | `		++nRawObj;` |
|     ! 0 | 2043 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2044 | `	}` |
|      21 | 2045 | `	if( nRawObj > 0 ){` |
|       - | 2046 | `		/* Emit the consume instruction */` |
|     ! 0 | 2047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2048 | `	}` |
|      21 | 2049 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2050 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2051 | `		/* Reset the token set */` |
|     ! 0 | 2052 | `		SySetReset(pTokenSet);` |
|       - | 2053 | `		/* Tokenize input */` |
|     ! 0 | 2054 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2055 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2056 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2057 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2058 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2059 | `		/* Advance the stream cursor */` |
|     ! 0 | 2060 | `		pGen->pRawIn++;` |
|       - | 2061 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2062 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2063 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2064 | `			sxi32 rc;` |
|       - | 2065 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2066 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2067 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2068 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2069 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2070 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2071 | `				return SXERR_ABORT;` |
|     ! 0 | 2072 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2073 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2074 | `			}` |
|     ! 0 | 2075 | `			goto Consume;` |
|       - | 2076 | `		}` |
|     ! 0 | 2077 | `	}else{` |
|       - | 2078 | `		/* No more chunks to process */` |
|      21 | 2079 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2080 | `		return SXERR_EOF;` |
|       - | 2081 | `	}` |
|     ! 0 | 2082 | `	return SXRET_OK;` |
|      11 | 2083 |  |
|       - | 2084 | `/*` |
|       - | 2085 | ` * Compile a PHP block.` |
|       - | 2086 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2087 | ` * optionally delimited by braces {}.` |
|       - | 2088 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2089 | ` * and this function takes care of generating the appropriate error` |
|       - | 2090 | ` * message.` |
|       - | 2091 | ` */` |
|  452550 | 2092 | `static sxi32 PH7_CompileBlock(` |
|       - | 2093 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2094 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2095 | `	)` |
|       1 | 2096 |  |
|       - | 2097 | `	sxi32 rc;` |
|       - | 2098 | `	sxu32 nLine;` |
|  452551 | 2099 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  451391 | 2100 | `		nLine = pGen->pIn->nLine;` |
|  451391 | 2101 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  451391 | 2102 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2103 | `			return SXERR_ABORT;` |
|       - | 2104 | `		}` |
|  451391 | 2105 | `		pGen->pIn++;` |
|       - | 2106 | `		/* Compile until we hit the closing braces '}' */` |
|  664988 | 2107 | `		for(;;){` |
| 1329977 | 2108 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2109 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2110 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2111 | `			 	   return SXERR_ABORT;` |
|       - | 2112 | `				}` |
|      21 | 2113 | `				if( rc == SXERR_EOF ){` |
|       - | 2114 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2115 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2116 | `					break;` |
|       - | 2117 | `				}` |
|     ! 0 | 2118 | `			}` |
| 1329957 | 2119 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2120 | `				/* Closing braces found,break immediately*/` |
|  451371 | 2121 | `				pGen->pIn++;` |
|  451371 | 2122 | `				break;` |
|       - | 2123 | `			}` |
|       - | 2124 | `			/* Compile a single statement */` |
|  878587 | 2125 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  878587 | 2126 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2127 | `				return SXERR_ABORT;` |
|       - | 2128 | `			}` |
|       1 | 2129 | `		}` |
|  451391 | 2130 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  226856 | 2131 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2132 | `		pGen->pIn++;` |
|     ! 0 | 2133 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2134 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2135 | `			return SXERR_ABORT;` |
|       - | 2136 | `		}` |
|       - | 2137 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2138 | `		for(;;){` |
|     ! 0 | 2139 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2140 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2141 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2142 | `			 	   return SXERR_ABORT;` |
|       - | 2143 | `				}` |
|     ! 0 | 2144 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2145 | `					/* No more token to process */` |
|     ! 0 | 2146 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2147 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2148 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2149 | `					}` |
|     ! 0 | 2150 | `					break;` |
|       - | 2151 | `				}` |
|     ! 0 | 2152 | `			}` |
|     ! 0 | 2153 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2154 | `				sxi32 nKwrd;` |
|       - | 2155 | `				/* Keyword found */` |
|     ! 0 | 2156 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2157 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2158 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2159 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2160 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2161 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2162 | `						}` |
|     ! 0 | 2163 | `						break;` |
|       - | 2164 | `				}` |
|     ! 0 | 2165 | `			}` |
|       - | 2166 | `			/* Compile a single statement */` |
|     ! 0 | 2167 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2168 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2169 | `				return SXERR_ABORT;` |
|       - | 2170 | `			}` |
|     ! 0 | 2171 | `		}` |
|     ! 0 | 2172 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2173 | `	}else{` |
|       - | 2174 | `		/* Compile a single statement */` |
|    1161 | 2175 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1161 | 2176 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2177 | `			return SXERR_ABORT;` |
|       - | 2178 | `		}` |
|       - | 2179 | `	}` |
|       - | 2180 | `	/* Jump trailing semi-colons ';' */` |
|  452551 | 2181 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2182 | `		pGen->pIn++;` |
|     ! 0 | 2183 | `	}` |
|  452551 | 2184 | `	return SXRET_OK;` |
|  226276 | 2185 |  |
|       - | 2186 | `/*` |
|       - | 2187 | ` * Compile the gentle 'while' statement.` |
|       - | 2188 | ` * According to the PHP language reference` |
|       - | 2189 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2190 | ` *  The basic form of a while statement is:` |
|       - | 2191 | ` *  while (expr)` |
|       - | 2192 | ` *   statement` |
|       - | 2193 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2194 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2195 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2196 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2197 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2198 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2199 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2200 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2201 | ` *  while (expr):` |
|       - | 2202 | ` *    statement` |
|       - | 2203 | ` *   endwhile;` |
|       - | 2204 | ` */` |
|   21382 | 2205 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       1 | 2206 |  |
|   21383 | 2207 | `	GenBlock *pWhileBlock = 0;` |
|   21383 | 2208 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2209 | `	sxu32 nFalseJump;` |
|       - | 2210 | `	sxu32 nLine;` |
|       - | 2211 | `	sxi32 rc;` |
|   21383 | 2212 | `	nLine = pGen->pIn->nLine;` |
|       - | 2213 | `	/* Jump the 'while' keyword */` |
|   21383 | 2214 | `	pGen->pIn++;` |
|   21383 | 2215 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2216 | `		/* Syntax error */` |
|     ! 0 | 2217 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2218 | `		if( rc == SXERR_ABORT ){` |
|       - | 2219 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2220 | `			return SXERR_ABORT;` |
|       - | 2221 | `		}` |
|     ! 0 | 2222 | `		goto Synchronize;` |
|       - | 2223 | `	}` |
|       - | 2224 | `	/* Jump the left parenthesis '(' */` |
|   21383 | 2225 | `	pGen->pIn++;` |
|       - | 2226 | `	/* Create the loop block */` |
|   21383 | 2227 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   21383 | 2228 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2229 | `		return SXERR_ABORT;` |
|       - | 2230 | `	}` |
|       - | 2231 | `	/* Delimit the condition */` |
|   21383 | 2232 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   21383 | 2233 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2234 | `		/* Empty expression */` |
|       3 | 2235 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2236 | `		if( rc == SXERR_ABORT ){` |
|       - | 2237 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2238 | `			return SXERR_ABORT;` |
|       - | 2239 | `		}` |
|       1 | 2240 | `	}` |
|       - | 2241 | `	/* Swap token streams */` |
|   21383 | 2242 | `	pTmp = pGen->pEnd;` |
|   21383 | 2243 | `	pGen->pEnd = pEnd;` |
|       - | 2244 | `	/* Compile the expression */` |
|   21383 | 2245 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   21383 | 2246 | `	if( rc == SXERR_ABORT ){` |
|       - | 2247 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2248 | `		return SXERR_ABORT;` |
|       - | 2249 | `	}` |
|       - | 2250 | `	/* Update token stream */` |
|   21383 | 2251 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2252 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2253 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2254 | `			return SXERR_ABORT;` |
|       - | 2255 | `		}` |
|     ! 0 | 2256 | `		pGen->pIn++;` |
|     ! 0 | 2257 | `	}` |
|       - | 2258 | `	/* Synchronize pointers */` |
|   21383 | 2259 | `	pGen->pIn  = &pEnd[1];` |
|   21383 | 2260 | `	pGen->pEnd = pTmp;` |
|       - | 2261 | `	/* Emit the false jump */` |
|   21383 | 2262 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2263 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21383 | 2264 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2265 | `	/* Compile the loop body */` |
|   21383 | 2266 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   21383 | 2267 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2268 | `		return SXERR_ABORT;` |
|       - | 2269 | `	}` |
|       - | 2270 | `	/* Emit the unconditional jump to the start of the loop */` |
|   21383 | 2271 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2272 | `	/* Fix all jumps now the destination is resolved */` |
|   21383 | 2273 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2274 | `	/* Release the loop block */` |
|   21383 | 2275 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2276 | `	/* Statement successfully compiled */` |
|   21383 | 2277 | `	return SXRET_OK;` |
|     ! 0 | 2278 | `Synchronize:` |
|       - | 2279 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2280 | `	 * compiling this erroneous block.` |
|       - | 2281 | `	 */` |
|     ! 0 | 2282 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2283 | `		pGen->pIn++;` |
|     ! 0 | 2284 | `	}` |
|     ! 0 | 2285 | `	return SXRET_OK;` |
|   10692 | 2286 |  |
|       - | 2287 | `/*` |
|       - | 2288 | ` * Compile the ugly do..while() statement.` |
|       - | 2289 | ` * According to the PHP language reference` |
|       - | 2290 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2291 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2292 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2293 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2294 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2295 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2296 | ` *  would end immediately).` |
|       - | 2297 | ` *  There is just one syntax for do-while loops:` |
|       - | 2298 | ` *  <?php` |
|       - | 2299 | ` *  $i = 0;` |
|       - | 2300 | ` *  do {` |
|       - | 2301 | ` *   echo $i;` |
|       - | 2302 | ` *  } while ($i > 0);` |
|       - | 2303 | ` * ?>` |
|       - | 2304 | ` */` |
|       4 | 2305 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2306 |  |
|       5 | 2307 | `	SyToken *pTmp,*pEnd = 0;` |
|       5 | 2308 | `	GenBlock *pDoBlock = 0;` |
|       - | 2309 | `	sxu32 nLine;` |
|       - | 2310 | `	sxi32 rc;` |
|       5 | 2311 | `	nLine = pGen->pIn->nLine;` |
|       - | 2312 | `	/* Jump the 'do' keyword */` |
|       5 | 2313 | `	pGen->pIn++;` |
|       - | 2314 | `	/* Create the loop block */` |
|       5 | 2315 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       5 | 2316 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2317 | `		return SXERR_ABORT;` |
|       - | 2318 | `	}` |
|       - | 2319 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       5 | 2320 | `	pDoBlock->bPostContinue = TRUE;` |
|       5 | 2321 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       5 | 2322 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2323 | `		return SXERR_ABORT;` |
|       - | 2324 | `	}` |
|       5 | 2325 | `	if( pGen->pIn < pGen->pEnd ){` |
|       3 | 2326 | `		nLine = pGen->pIn->nLine;` |
|       1 | 2327 | `	}` |
|       5 | 2328 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|       2 | 2329 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2330 | `			/* Missing 'while' statement */` |
|       3 | 2331 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2332 | `			if( rc == SXERR_ABORT ){` |
|       - | 2333 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2334 | `				return SXERR_ABORT;` |
|       - | 2335 | `			}` |
|       3 | 2336 | `			goto Synchronize;` |
|       - | 2337 | `	}` |
|       - | 2338 | `	/* Jump the 'while' keyword */` |
|       3 | 2339 | `	pGen->pIn++;` |
|       3 | 2340 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2341 | `		/* Syntax error */` |
|     ! 0 | 2342 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2343 | `		if( rc == SXERR_ABORT ){` |
|       - | 2344 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2345 | `			return SXERR_ABORT;` |
|       - | 2346 | `		}` |
|     ! 0 | 2347 | `		goto Synchronize;` |
|       - | 2348 | `	}` |
|       - | 2349 | `	/* Jump the left parenthesis '(' */` |
|       3 | 2350 | `	pGen->pIn++;` |
|       - | 2351 | `	/* Delimit the condition */` |
|       3 | 2352 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|       3 | 2353 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2354 | `		/* Empty expression */` |
|     ! 0 | 2355 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2356 | `		if( rc == SXERR_ABORT ){` |
|       - | 2357 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2358 | `			return SXERR_ABORT;` |
|       - | 2359 | `		}` |
|     ! 0 | 2360 | `		goto Synchronize;` |
|       - | 2361 | `	}` |
|       - | 2362 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|       3 | 2363 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2364 | `		JumpFixup *aPost;` |
|       - | 2365 | `		VmInstr *pInstr;` |
|       - | 2366 | `		sxu32 nJumpDest;` |
|       - | 2367 | `		sxu32 n;` |
|     ! 0 | 2368 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2369 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2370 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2371 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2372 | `			if( pInstr ){` |
|       - | 2373 | `				/* Fix */` |
|     ! 0 | 2374 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2375 | `			}` |
|     ! 0 | 2376 | `		}` |
|     ! 0 | 2377 | `	}` |
|       - | 2378 | `	/* Swap token streams */` |
|       3 | 2379 | `	pTmp = pGen->pEnd;` |
|       3 | 2380 | `	pGen->pEnd = pEnd;` |
|       - | 2381 | `	/* Compile the expression */` |
|       3 | 2382 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       3 | 2383 | `	if( rc == SXERR_ABORT ){` |
|       - | 2384 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2385 | `		return SXERR_ABORT;` |
|       - | 2386 | `	}` |
|       - | 2387 | `	/* Update token stream */` |
|       3 | 2388 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2389 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2390 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2391 | `			return SXERR_ABORT;` |
|       - | 2392 | `		}` |
|     ! 0 | 2393 | `		pGen->pIn++;` |
|     ! 0 | 2394 | `	}` |
|       3 | 2395 | `	pGen->pIn  = &pEnd[1];` |
|       3 | 2396 | `	pGen->pEnd = pTmp;` |
|       - | 2397 | `	/* Emit the true jump to the beginning of the loop */` |
|       3 | 2398 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2399 | `	/* Fix all jumps now the destination is resolved */` |
|       3 | 2400 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2401 | `	/* Release the loop block */` |
|       3 | 2402 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2403 | `	/* Statement successfully compiled */` |
|       3 | 2404 | `	return SXRET_OK;` |
|       1 | 2405 | `Synchronize:` |
|       - | 2406 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2407 | `	 * compiling this erroneous block.` |
|       - | 2408 | `	 */` |
|       3 | 2409 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2410 | `		pGen->pIn++;` |
|     ! 0 | 2411 | `	}` |
|       3 | 2412 | `	return SXRET_OK;` |
|       3 | 2413 |  |
|       - | 2414 | `/*` |
|       - | 2415 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2416 | ` * According to the PHP language reference` |
|       - | 2417 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2418 | ` *  The syntax of a for loop is:` |
|       - | 2419 | ` *  for (expr1; expr2; expr3)` |
|       - | 2420 | ` *   statement` |
|       - | 2421 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2422 | ` *  the beginning of the loop.` |
|       - | 2423 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2424 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2425 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2426 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2427 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2428 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2429 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2430 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2431 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2432 | ` *  of using the for truth expression.` |
|       - | 2433 | ` */` |
|   21418 | 2434 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       1 | 2435 |  |
|   21419 | 2436 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   21419 | 2437 | `	GenBlock *pForBlock = 0;` |
|       - | 2438 | `	sxu32 nFalseJump;` |
|       - | 2439 | `	sxu32 nLine;` |
|       - | 2440 | `	sxi32 rc;` |
|   21419 | 2441 | `	nLine = pGen->pIn->nLine;` |
|       - | 2442 | `	/* Jump the 'for' keyword */` |
|   21419 | 2443 | `	pGen->pIn++;` |
|   21419 | 2444 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2445 | `		/* Syntax error */` |
|     ! 0 | 2446 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2447 | `		if( rc == SXERR_ABORT ){` |
|       - | 2448 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2449 | `			return SXERR_ABORT;` |
|       - | 2450 | `		}` |
|     ! 0 | 2451 | `		return SXRET_OK;` |
|       - | 2452 | `	}` |
|       - | 2453 | `	/* Jump the left parenthesis '(' */` |
|   21419 | 2454 | `	pGen->pIn++;` |
|       - | 2455 | `	/* Delimit the init-expr;condition;post-expr */` |
|   21419 | 2456 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   21419 | 2457 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2458 | `		/* Empty expression */` |
|     ! 0 | 2459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2460 | `		if( rc == SXERR_ABORT ){` |
|       - | 2461 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2462 | `			return SXERR_ABORT;` |
|       - | 2463 | `		}` |
|       - | 2464 | `		/* Synchronize */` |
|     ! 0 | 2465 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2466 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2467 | `			pGen->pIn++;` |
|     ! 0 | 2468 | `		}` |
|     ! 0 | 2469 | `		return SXRET_OK;` |
|       - | 2470 | `	}` |
|       - | 2471 | `	/* Swap token streams */` |
|   21419 | 2472 | `	pTmp = pGen->pEnd;` |
|   21419 | 2473 | `	pGen->pEnd = pEnd;` |
|       - | 2474 | `	/* Compile initialization expressions if available */` |
|   21419 | 2475 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2476 | `	/* Pop operand lvalues */` |
|   21419 | 2477 | `	if( rc == SXERR_ABORT ){` |
|       - | 2478 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2479 | `		return SXERR_ABORT;` |
|   21419 | 2480 | `	}else if( rc != SXERR_EMPTY ){` |
|   21417 | 2481 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   10708 | 2482 | `	}` |
|   21419 | 2483 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2484 | `		/* Syntax error */` |
|     ! 0 | 2485 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2486 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2487 | `		if( rc == SXERR_ABORT ){` |
|       - | 2488 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2489 | `			return SXERR_ABORT;` |
|       - | 2490 | `		}` |
|     ! 0 | 2491 | `		return SXRET_OK;` |
|       - | 2492 | `	}` |
|       - | 2493 | `	/* Jump the trailing ';' */` |
|   21419 | 2494 | `	pGen->pIn++;` |
|       - | 2495 | `	/* Create the loop block */` |
|   21419 | 2496 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   21419 | 2497 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2498 | `		return SXERR_ABORT;` |
|       - | 2499 | `	}` |
|       - | 2500 | `	/* Deffer continue jumps */` |
|   21419 | 2501 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2502 | `	/* Compile the condition */` |
|   21419 | 2503 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   21419 | 2504 | `	if( rc == SXERR_ABORT ){` |
|       - | 2505 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2506 | `		return SXERR_ABORT;` |
|   21419 | 2507 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2508 | `		/* Emit the false jump */` |
|   21417 | 2509 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2510 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21417 | 2511 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|   10708 | 2512 | `	}` |
|   21419 | 2513 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2514 | `		/* Syntax error */` |
|       5 | 2515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2516 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2517 | `		if( rc == SXERR_ABORT ){` |
|       - | 2518 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2519 | `			return SXERR_ABORT;` |
|       - | 2520 | `		}` |
|       5 | 2521 | `		return SXRET_OK;` |
|       - | 2522 | `	}` |
|       - | 2523 | `	/* Jump the trailing ';' */` |
|   21415 | 2524 | `	pGen->pIn++;` |
|       - | 2525 | `	/* Save the post condition stream */` |
|   21415 | 2526 | `	pPostStart = pGen->pIn;` |
|       - | 2527 | `	/* Compile the loop body */` |
|   21415 | 2528 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   21415 | 2529 | `	pGen->pEnd = pTmp;` |
|   21415 | 2530 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   21415 | 2531 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2532 | `		return SXERR_ABORT;` |
|       - | 2533 | `	}` |
|       - | 2534 | `	/* Fix post-continue jumps */` |
|   21415 | 2535 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2536 | `		JumpFixup *aPost;` |
|       - | 2537 | `		VmInstr *pInstr;` |
|       - | 2538 | `		sxu32 nJumpDest;` |
|       - | 2539 | `		sxu32 n;` |
|    5343 | 2540 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    5343 | 2541 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|   10685 | 2542 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    5343 | 2543 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|    5343 | 2544 | `			if( pInstr ){` |
|       - | 2545 | `				/* Fix jump */` |
|    5343 | 2546 | `				pInstr->iP2 = nJumpDest;` |
|    2671 | 2547 | `			}` |
|    2672 | 2548 | `		}` |
|    2671 | 2549 | `	}` |
|       - | 2550 | `	/* compile the post-expressions if available */` |
|   21415 | 2551 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2552 | `		pPostStart++;` |
|     ! 0 | 2553 | `	}` |
|   21415 | 2554 | `	if( pPostStart < pEnd ){` |
|       - | 2555 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   21415 | 2556 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   21415 | 2557 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   21415 | 2558 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2559 | `			/* Syntax error */` |
|     ! 0 | 2560 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2561 | `			if( rc == SXERR_ABORT ){` |
|       - | 2562 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2563 | `				return SXERR_ABORT;` |
|       - | 2564 | `			}` |
|     ! 0 | 2565 | `			return SXRET_OK;` |
|       - | 2566 | `		}` |
|   21415 | 2567 | `		RE_SWAP_DELIMITER(pGen);` |
|   21415 | 2568 | `		if( rc == SXERR_ABORT ){` |
|       - | 2569 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2570 | `			return SXERR_ABORT;` |
|   21415 | 2571 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2572 | `			/* Pop operand lvalue */` |
|   21415 | 2573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   10707 | 2574 | `		}` |
|   10707 | 2575 | `	}` |
|       - | 2576 | `	/* Emit the unconditional jump to the start of the loop */` |
|   21415 | 2577 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2578 | `	/* Fix all jumps now the destination is resolved */` |
|   21415 | 2579 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2580 | `	/* Release the loop block */` |
|   21415 | 2581 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2582 | `	/* Statement successfully compiled */` |
|   21415 | 2583 | `	return SXRET_OK;` |
|   10710 | 2584 |  |
|       - | 2585 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2586 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2587 | ` * are allowed.` |
|       - | 2588 | ` */` |
|   10808 | 2589 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       1 | 2590 |  |
|   10809 | 2591 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   10809 | 2592 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2593 | `		/* Unexpected expression */` |
|     ! 0 | 2594 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2595 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2596 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2597 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2598 | `		}` |
|     ! 0 | 2599 | `	}` |
|   10809 | 2600 | `	return rc;` |
|       1 | 2601 |  |
|       - | 2602 | `/*` |
|       - | 2603 | ` * Compile the 'foreach' statement.` |
|       - | 2604 | ` * According to the PHP language reference` |
|       - | 2605 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2606 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2607 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2608 | ` *  is a minor but useful extension of the first:` |
|       - | 2609 | ` *  foreach (array_expression as $value)` |
|       - | 2610 | ` *    statement` |
|       - | 2611 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2612 | ` *   statement` |
|       - | 2613 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2614 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2615 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2616 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2617 | ` *  to the variable $key on each loop.` |
|       - | 2618 | ` *  Note:` |
|       - | 2619 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2620 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2621 | ` *  Note:` |
|       - | 2622 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2623 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2624 | ` *  or after the foreach without resetting it.` |
|       - | 2625 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2626 | ` *  of copying the value.` |
|       - | 2627 | ` */` |
|    5418 | 2628 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       1 | 2629 |  |
|    5419 | 2630 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    5419 | 2631 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2632 | `	ph7_foreach_info *pInfo;` |
|       - | 2633 | `	sxu32 nFalseJump;` |
|       - | 2634 | `	VmInstr *pInstr;` |
|       - | 2635 | `	sxu32 nLine;` |
|       - | 2636 | `	sxi32 rc;` |
|    5419 | 2637 | `	nLine = pGen->pIn->nLine;` |
|       - | 2638 | `	/* Jump the 'foreach' keyword */` |
|    5419 | 2639 | `	pGen->pIn++;` |
|    5419 | 2640 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2641 | `		/* Syntax error */` |
|     ! 0 | 2642 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2643 | `		if( rc == SXERR_ABORT ){` |
|       - | 2644 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2645 | `			return SXERR_ABORT;` |
|       - | 2646 | `		}` |
|     ! 0 | 2647 | `		goto Synchronize;` |
|       - | 2648 | `	}` |
|       - | 2649 | `	/* Jump the left parenthesis '(' */` |
|    5419 | 2650 | `	pGen->pIn++;` |
|       - | 2651 | `	/* Create the loop block */` |
|    5419 | 2652 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    5419 | 2653 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2654 | `		return SXERR_ABORT;` |
|       - | 2655 | `	}` |
|       - | 2656 | `	/* Delimit the expression */` |
|    5419 | 2657 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    5419 | 2658 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2659 | `		/* Empty expression */` |
|     ! 0 | 2660 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2661 | `		if( rc == SXERR_ABORT ){` |
|       - | 2662 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2663 | `			return SXERR_ABORT;` |
|       - | 2664 | `		}` |
|       - | 2665 | `		/* Synchronize */` |
|     ! 0 | 2666 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2667 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2668 | `			pGen->pIn++;` |
|     ! 0 | 2669 | `		}` |
|     ! 0 | 2670 | `		return SXRET_OK;` |
|       - | 2671 | `	}` |
|       - | 2672 | `	/* Compile the array expression */` |
|    5419 | 2673 | `	pCur = pGen->pIn;` |
|   37597 | 2674 | `	while( pCur < pEnd ){` |
|   37597 | 2675 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    5429 | 2676 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    5429 | 2677 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2678 | `				/* Break with the first 'as' found */` |
|    5419 | 2679 | `				break;` |
|       - | 2680 | `			}` |
|       5 | 2681 | `		}` |
|       - | 2682 | `		/* Advance the stream cursor */` |
|   32179 | 2683 | `		pCur++;` |
|       1 | 2684 | `	}` |
|    5419 | 2685 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2686 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2687 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2688 | `		if( rc == SXERR_ABORT ){` |
|       - | 2689 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2690 | `			return SXERR_ABORT;` |
|       - | 2691 | `		}` |
|     ! 0 | 2692 | `		goto Synchronize;` |
|       - | 2693 | `	}` |
|       - | 2694 | `	/* Swap token streams */` |
|    5419 | 2695 | `	pTmp = pGen->pEnd;` |
|    5419 | 2696 | `	pGen->pEnd = pCur;` |
|    5419 | 2697 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    5419 | 2698 | `	if( rc == SXERR_ABORT ){` |
|       - | 2699 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2700 | `		return SXERR_ABORT;` |
|       - | 2701 | `	}` |
|       - | 2702 | `	/* Update token stream */` |
|    5419 | 2703 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2704 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2705 | `		if( rc == SXERR_ABORT ){` |
|       - | 2706 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2707 | `			return SXERR_ABORT;` |
|       - | 2708 | `		}` |
|     ! 0 | 2709 | `		pGen->pIn++;` |
|     ! 0 | 2710 | `	}` |
|    5419 | 2711 | `	pCur++; /* Jump the 'as' keyword */` |
|    5419 | 2712 | `	pGen->pIn = pCur;` |
|    5419 | 2713 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2714 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2715 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2716 | `			return SXERR_ABORT;` |
|       - | 2717 | `		}` |
|     ! 0 | 2718 | `	}` |
|       - | 2719 | `	/* Create the foreach context */` |
|    5419 | 2720 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    5419 | 2721 | `	if( pInfo == 0 ){` |
|     ! 0 | 2722 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2723 | `		return SXERR_ABORT;` |
|       - | 2724 | `	}` |
|       - | 2725 | `	/* Zero the structure */` |
|    5419 | 2726 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2727 | `	/* Initialize structure fields */` |
|    5419 | 2728 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2729 | `	/* Check if we have a key field */` |
|   16257 | 2730 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|   10839 | 2731 | `		pCur++;` |
|       1 | 2732 | `	}` |
|    5419 | 2733 | `	if( pCur < pEnd ){` |
|       - | 2734 | `		/* Compile the expression holding the key name */` |
|    5391 | 2735 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2736 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2737 | `			if( rc == SXERR_ABORT ){` |
|       - | 2738 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2739 | `				return SXERR_ABORT;` |
|       - | 2740 | `			}` |
|     ! 0 | 2741 | `		}else{` |
|    5391 | 2742 | `			pGen->pEnd = pCur;` |
|    5391 | 2743 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    5391 | 2744 | `			if( rc == SXERR_ABORT ){` |
|       - | 2745 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2746 | `				return SXERR_ABORT;` |
|       - | 2747 | `			}` |
|    5391 | 2748 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    5391 | 2749 | `			if( pInstr->p3 ){` |
|       - | 2750 | `				/* Record key name */` |
|    5391 | 2751 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2695 | 2752 | `			}` |
|    5391 | 2753 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2754 | `		}` |
|    5391 | 2755 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    2695 | 2756 | `	}` |
|    5419 | 2757 | `	pGen->pEnd = pEnd;` |
|    5419 | 2758 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2759 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2760 | `		if( rc == SXERR_ABORT ){` |
|       - | 2761 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2762 | `			return SXERR_ABORT;` |
|       - | 2763 | `		}` |
|     ! 0 | 2764 | `		goto Synchronize;` |
|       - | 2765 | `	}` |
|    5419 | 2766 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2767 | `		pGen->pIn++;` |
|       - | 2768 | `		/* Pass by reference  */` |
|       7 | 2769 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2770 | `	}` |
|       - | 2771 | `	/* Compile the expression holding the value name */` |
|    5419 | 2772 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    5419 | 2773 | `	if( rc == SXERR_ABORT ){` |
|       - | 2774 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2775 | `		return SXERR_ABORT;` |
|       - | 2776 | `	}` |
|    5419 | 2777 | `	pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    5419 | 2778 | `	if( pInstr->p3 ){` |
|       - | 2779 | `		/* Record value name */` |
|    5419 | 2780 | `		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    2709 | 2781 | `	}` |
|       - | 2782 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    5419 | 2783 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2784 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    5419 | 2785 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2786 | `	/* Record the first instruction to execute */` |
|    5419 | 2787 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2788 | `	/* Emit the FOREACH_STEP instruction */` |
|    5419 | 2789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2790 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    5419 | 2791 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2792 | `	/* Compile the loop body */` |
|    5419 | 2793 | `	pGen->pIn = &pEnd[1];` |
|    5419 | 2794 | `	pGen->pEnd = pTmp;` |
|    5419 | 2795 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    5419 | 2796 | `	if( rc == SXERR_ABORT ){` |
|       - | 2797 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2798 | `		return SXERR_ABORT;` |
|       - | 2799 | `	}` |
|       - | 2800 | `	/* Emit the unconditional jump to the start of the loop */` |
|    5419 | 2801 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2802 | `	/* Fix all jumps now the destination is resolved */` |
|    5419 | 2803 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2804 | `	/* Release the loop block */` |
|    5419 | 2805 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2806 | `	/* Statement successfully compiled */` |
|    5419 | 2807 | `	return SXRET_OK;` |
|     ! 0 | 2808 | `Synchronize:` |
|       - | 2809 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2810 | `	 * compiling this erroneous block.` |
|       - | 2811 | `	 */` |
|     ! 0 | 2812 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2813 | `		pGen->pIn++;` |
|     ! 0 | 2814 | `	}` |
|     ! 0 | 2815 | `	return SXRET_OK;` |
|    2710 | 2816 |  |
|       - | 2817 | `/*` |
|       - | 2818 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 2819 | ` * According to the PHP language reference` |
|       - | 2820 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 2821 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 2822 | ` *  that is similar to that of C:` |
|       - | 2823 | ` *  if (expr)` |
|       - | 2824 | ` *   statement` |
|       - | 2825 | ` *  else construct:` |
|       - | 2826 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 2827 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 2828 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 2829 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 2830 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 2831 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 2832 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 2833 | ` *  elseif` |
|       - | 2834 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 2835 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 2836 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 2837 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 2838 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 2839 | ` *   <?php` |
|       - | 2840 | ` *    if ($a > $b) {` |
|       - | 2841 | ` *     echo "a is bigger than b";` |
|       - | 2842 | ` *    } elseif ($a == $b) {` |
|       - | 2843 | ` *     echo "a is equal to b";` |
|       - | 2844 | ` *    } else {` |
|       - | 2845 | ` *     echo "a is smaller than b";` |
|       - | 2846 | ` *    }` |
|       - | 2847 | ` *    ?>` |
|       - | 2848 | ` */` |
|  200180 | 2849 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       1 | 2850 |  |
|  200181 | 2851 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  200181 | 2852 | `	GenBlock *pCondBlock = 0;` |
|       - | 2853 | `	sxu32 nJumpIdx;` |
|       - | 2854 | `	sxu32 nKeyID;` |
|       - | 2855 | `	sxi32 rc;` |
|       - | 2856 | `	/* Jump the 'if' keyword */` |
|  200181 | 2857 | `	pGen->pIn++;` |
|  200181 | 2858 | `	pToken = pGen->pIn;` |
|       - | 2859 | `	/* Create the conditional block */` |
|  200181 | 2860 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  200181 | 2861 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2862 | `		return SXERR_ABORT;` |
|       - | 2863 | `	}` |
|       - | 2864 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  105428 | 2865 | `	for(;;){` |
|  210857 | 2866 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2867 | `			/* Syntax error */` |
|     ! 0 | 2868 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2869 | `				pToken--;` |
|     ! 0 | 2870 | `			}` |
|     ! 0 | 2871 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 2872 | `			if( rc == SXERR_ABORT ){` |
|       - | 2873 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2874 | `				return SXERR_ABORT;` |
|       - | 2875 | `			}` |
|     ! 0 | 2876 | `			goto Synchronize;` |
|       - | 2877 | `		}` |
|       - | 2878 | `		/* Jump the left parenthesis '(' */` |
|  210857 | 2879 | `		pToken++;` |
|       - | 2880 | `		/* Delimit the condition */` |
|  210857 | 2881 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  210857 | 2882 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 2883 | `			/* Syntax error */` |
|     ! 0 | 2884 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 2885 | `				pToken--;` |
|     ! 0 | 2886 | `			}` |
|     ! 0 | 2887 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 2888 | `			if( rc == SXERR_ABORT ){` |
|       - | 2889 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2890 | `				return SXERR_ABORT;` |
|       - | 2891 | `			}` |
|     ! 0 | 2892 | `			goto Synchronize;` |
|       - | 2893 | `		}` |
|       - | 2894 | `		/* Swap token streams */` |
|  210857 | 2895 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 2896 | `		/* Compile the condition */` |
|  210857 | 2897 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2898 | `		/* Update token stream */` |
|  210857 | 2899 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 2900 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2901 | `			pGen->pIn++;` |
|     ! 0 | 2902 | `		}` |
|  210857 | 2903 | `		pGen->pIn  = &pEnd[1];` |
|  210857 | 2904 | `		pGen->pEnd = pTmp;` |
|  210857 | 2905 | `		if( rc == SXERR_ABORT ){` |
|       - | 2906 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2907 | `			return SXERR_ABORT;` |
|       - | 2908 | `		}` |
|       - | 2909 | `		/* Emit the false jump */` |
|  210857 | 2910 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 2911 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  210857 | 2912 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 2913 | `		/* Compile the body */` |
|  210857 | 2914 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  210857 | 2915 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2916 | `			return SXERR_ABORT;` |
|       - | 2917 | `		}` |
|  210857 | 2918 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   62337 | 2919 | `			break;` |
|       - | 2920 | `		}` |
|       - | 2921 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   86185 | 2922 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   86185 | 2923 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   64145 | 2924 | `			break;` |
|       - | 2925 | `		}` |
|       - | 2926 | `		/* Emit the unconditional jump */` |
|   22041 | 2927 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 2928 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   22041 | 2929 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   22041 | 2930 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   22033 | 2931 | `			pToken = &pGen->pIn[1];` |
|   22033 | 2932 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|   10708 | 2933 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5683 | 2934 | `					break;` |
|       - | 2935 | `			}` |
|   10669 | 2936 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    5334 | 2937 | `		}` |
|   10677 | 2938 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 2939 | `		/* Synchronize cursors */` |
|   10677 | 2940 | `		pToken = pGen->pIn;` |
|       - | 2941 | `		/* Fix the false jump */` |
|   10677 | 2942 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       1 | 2943 | `	} /* For(;;) */` |
|       - | 2944 | `	/* Fix the false jump */` |
|  200181 | 2945 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  200181 | 2946 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   75508 | 2947 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 2948 | `			/* Compile the else block */` |
|   11365 | 2949 | `			pGen->pIn++;` |
|   11365 | 2950 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11365 | 2951 | `			if( rc == SXERR_ABORT ){` |
|       - | 2952 |  |
|     ! 0 | 2953 | `				return SXERR_ABORT;` |
|       - | 2954 | `			}` |
|    5682 | 2955 | `	}` |
|  200181 | 2956 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2957 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  200181 | 2958 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 2959 | `	/* Release the conditional block */` |
|  200181 | 2960 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2961 | `	/* Statement successfully compiled */` |
|  200181 | 2962 | `	return SXRET_OK;` |
|     ! 0 | 2963 | `Synchronize:` |
|       - | 2964 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 2965 | `	 */` |
|     ! 0 | 2966 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2967 | `		pGen->pIn++;` |
|     ! 0 | 2968 | `	}` |
|     ! 0 | 2969 | `	return SXRET_OK;` |
|  100091 | 2970 |  |
|       - | 2971 | `/*` |
|       - | 2972 | ` * Compile the global construct.` |
|       - | 2973 | ` * According to the PHP language reference` |
|       - | 2974 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 2975 | ` *  to be used in that function.` |
|       - | 2976 | ` *  Example #1 Using global` |
|       - | 2977 | ` *  <?php` |
|       - | 2978 | ` *   $a = 1;` |
|       - | 2979 | ` *   $b = 2;` |
|       - | 2980 | ` *   function Sum()` |
|       - | 2981 | ` *   {` |
|       - | 2982 | ` *    global $a, $b;` |
|       - | 2983 | ` *    $b = $a + $b;` |
|       - | 2984 | ` *   }` |
|       - | 2985 | ` *   Sum();` |
|       - | 2986 | ` *   echo $b;` |
|       - | 2987 | ` *  ?>` |
|       - | 2988 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 2989 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 2990 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 2991 | ` */` |
|      16 | 2992 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       1 | 2993 |  |
|      17 | 2994 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 2995 | `	sxi32 nExpr;` |
|       - | 2996 | `	sxi32 rc;` |
|       - | 2997 | `	/* Jump the 'global' keyword */` |
|      17 | 2998 | `	pGen->pIn++;` |
|      17 | 2999 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3000 | `		/* Nothing to process */` |
|     ! 0 | 3001 | `		return SXRET_OK;` |
|       - | 3002 | `	}` |
|      17 | 3003 | `	pTmp = pGen->pEnd;` |
|      17 | 3004 | `	nExpr = 0;` |
|      35 | 3005 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      19 | 3006 | `		if( pGen->pIn < pNext ){` |
|      19 | 3007 | `			pGen->pEnd = pNext;` |
|      19 | 3008 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3009 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3010 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3011 | `					return SXERR_ABORT;` |
|       - | 3012 | `				}` |
|     ! 0 | 3013 | `			}else{` |
|      19 | 3014 | `				pGen->pIn++;` |
|      19 | 3015 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3016 | `					/* Emit a warning */` |
|     ! 0 | 3017 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3018 | `				}else{` |
|      19 | 3019 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      19 | 3020 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3021 | `						return SXERR_ABORT;` |
|      19 | 3022 | `					}else if(rc != SXERR_EMPTY ){` |
|      19 | 3023 | `						nExpr++;` |
|       9 | 3024 | `					}` |
|       - | 3025 | `				}` |
|       - | 3026 | `			}` |
|       9 | 3027 | `		}` |
|       - | 3028 | `		/* Next expression in the stream */` |
|      19 | 3029 | `		pGen->pIn = pNext;` |
|       - | 3030 | `		/* Jump trailing commas */` |
|      21 | 3031 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3032 | `			pGen->pIn++;` |
|       1 | 3033 | `		}` |
|       1 | 3034 | `	}` |
|       - | 3035 | `	/* Restore token stream */` |
|      17 | 3036 | `	pGen->pEnd = pTmp;` |
|      17 | 3037 | `	if( nExpr > 0 ){` |
|       - | 3038 | `		/* Emit the uplink instruction */` |
|      17 | 3039 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       8 | 3040 | `	}` |
|      17 | 3041 | `	return SXRET_OK;` |
|       9 | 3042 |  |
|       - | 3043 | `/*` |
|       - | 3044 | ` * Compile the return statement.` |
|       - | 3045 | ` * According to the PHP language reference` |
|       - | 3046 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3047 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3048 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3049 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3050 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3051 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3052 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3053 | ` *  from within the main script file, then script execution end.` |
|       - | 3054 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3055 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3056 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3057 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3058 | ` */` |
|  229598 | 3059 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       1 | 3060 |  |
|  229599 | 3061 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3062 | `	sxi32 rc;` |
|       - | 3063 | `	/* Jump the 'return' keyword */` |
|  229599 | 3064 | `	pGen->pIn++;` |
|  229599 | 3065 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3066 | `		/* Compile the expression */` |
|  229577 | 3067 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  229577 | 3068 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3069 | `			return SXERR_ABORT;` |
|  229577 | 3070 | `		}else if(rc != SXERR_EMPTY ){` |
|  229577 | 3071 | `			nRet = 1;` |
|  114788 | 3072 | `		}` |
|  114788 | 3073 | `	}` |
|       - | 3074 | `	/* Emit the done instruction */` |
|  229599 | 3075 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  229599 | 3076 | `	return SXRET_OK;` |
|  114800 | 3077 |  |
|       - | 3078 | `/*` |
|       - | 3079 | ` * Compile the die/exit language construct.` |
|       - | 3080 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3081 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3082 | ` */` |
|      66 | 3083 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       1 | 3084 |  |
|      67 | 3085 | `	sxi32 nExpr = 0;` |
|       - | 3086 | `	sxi32 rc;` |
|       - | 3087 | `	/* Jump the die/exit keyword */` |
|      67 | 3088 | `	pGen->pIn++;` |
|      67 | 3089 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3090 | `		/* Compile the expression */` |
|      67 | 3091 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      67 | 3092 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3093 | `			return SXERR_ABORT;` |
|      67 | 3094 | `		}else if(rc != SXERR_EMPTY ){` |
|      67 | 3095 | `			nExpr = 1;` |
|      33 | 3096 | `		}` |
|      33 | 3097 | `	}` |
|       - | 3098 | `	/* Emit the HALT instruction */` |
|      67 | 3099 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      67 | 3100 | `	return SXRET_OK;` |
|      34 | 3101 |  |
|       - | 3102 | `/*` |
|       - | 3103 | ` * Compile the 'echo' language construct.` |
|       - | 3104 | ` */` |
|    8402 | 3105 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       1 | 3106 |  |
|    8403 | 3107 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3108 | `	sxi32 rc;` |
|       - | 3109 | `	/* Jump the 'echo' keyword */` |
|    8403 | 3110 | `	pGen->pIn++;` |
|       - | 3111 | `	/* Compile arguments one after one */` |
|    8403 | 3112 | `	pTmp = pGen->pEnd;` |
|   16815 | 3113 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    8413 | 3114 | `		if( pGen->pIn < pNext ){` |
|    8413 | 3115 | `			pGen->pEnd = pNext;` |
|    8413 | 3116 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    8413 | 3117 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3118 | `				return SXERR_ABORT;` |
|    8413 | 3119 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3120 | `				/* Emit the consume instruction */` |
|    8385 | 3121 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4192 | 3122 | `			}` |
|    4206 | 3123 | `		}` |
|       - | 3124 | `		/* Jump trailing commas */` |
|    8423 | 3125 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      11 | 3126 | `			pNext++;` |
|       1 | 3127 | `		}` |
|    8413 | 3128 | `		pGen->pIn = pNext;` |
|       1 | 3129 | `	}` |
|       - | 3130 | `	/* Restore token stream */` |
|    8403 | 3131 | `	pGen->pEnd = pTmp;` |
|    8403 | 3132 | `	return SXRET_OK;` |
|    4202 | 3133 |  |
|       - | 3134 | `/*` |
|       - | 3135 | ` * Compile the static statement.` |
|       - | 3136 | ` * According to the PHP language reference` |
|       - | 3137 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3138 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3139 | ` *  when program execution leaves this scope.` |
|       - | 3140 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3141 | ` * Symisc eXtension.` |
|       - | 3142 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3143 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3144 | ` *  Example` |
|       - | 3145 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3146 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3147 | ` */` |
|       4 | 3148 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3149 |  |
|       - | 3150 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3151 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3152 | `	GenBlock *pBlock;` |
|       - | 3153 | `	SyString *pName;` |
|       - | 3154 | `	char *zDup;` |
|       - | 3155 | `	sxu32 nLine;` |
|       - | 3156 | `	sxi32 rc;` |
|       - | 3157 | `	/* Jump the static keyword */` |
|       5 | 3158 | `	nLine = pGen->pIn->nLine;` |
|       5 | 3159 | `	pGen->pIn++;` |
|       - | 3160 | `	/* Extract the enclosing function if any */` |
|       5 | 3161 | `	pBlock = pGen->pCurrent;` |
|       9 | 3162 | `	while( pBlock ){` |
|       9 | 3163 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       5 | 3164 | `			break;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Point to the upper block */` |
|       5 | 3167 | `		pBlock = pBlock->pParent;` |
|       1 | 3168 | `	}` |
|       5 | 3169 | `	if( pBlock == 0 ){` |
|       - | 3170 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3171 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3172 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3173 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3174 | `				return SXERR_ABORT;` |
|       - | 3175 | `			}` |
|     ! 0 | 3176 | `			goto Synchronize;` |
|       - | 3177 | `		}` |
|       - | 3178 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3179 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3180 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3181 | `			return SXERR_ABORT;` |
|     ! 0 | 3182 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3183 | `			/* Emit the POP instruction */` |
|     ! 0 | 3184 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3185 | `		}` |
|     ! 0 | 3186 | `		return SXRET_OK;` |
|       - | 3187 | `	}` |
|       5 | 3188 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3189 | `	/* Make sure we are dealing with a valid statement */` |
|       5 | 3190 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       2 | 3191 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3192 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3193 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3194 | `				return SXERR_ABORT;` |
|       - | 3195 | `			}` |
|       3 | 3196 | `			goto Synchronize;` |
|       - | 3197 | `	}` |
|       3 | 3198 | `	pGen->pIn++;` |
|       - | 3199 | `	/* Extract variable name */` |
|       3 | 3200 | `	pName = &pGen->pIn->sData;` |
|       3 | 3201 | `	pGen->pIn++; /* Jump the var name */` |
|       3 | 3202 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3203 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3204 | `		goto Synchronize;` |
|       - | 3205 | `	}` |
|       - | 3206 | `	/* Initialize the structure describing the static variable */` |
|       3 | 3207 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       3 | 3208 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3209 | `	/* Duplicate variable name */` |
|       3 | 3210 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       3 | 3211 | `	if( zDup == 0 ){` |
|     ! 0 | 3212 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3213 | `		return SXERR_ABORT;` |
|       - | 3214 | `	}` |
|       3 | 3215 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3216 | `	/* Check if we have an expression to compile */` |
|       3 | 3217 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3218 | `		SySet *pInstrContainer;` |
|       - | 3219 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3220 | `		 * Static variable can take any complex expression including function` |
|       - | 3221 | `		 * call as their initialization value.` |
|       - | 3222 | `		 * Example:` |
|       - | 3223 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3224 | `		 */` |
|       3 | 3225 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3226 | `		/* Swap bytecode container */` |
|       3 | 3227 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       3 | 3228 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3229 | `		/* Compile the expression */` |
|       3 | 3230 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3231 | `		/* Emit the done instruction */` |
|       3 | 3232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3233 | `		/* Restore default bytecode container */` |
|       3 | 3234 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       1 | 3235 | `	}` |
|       - | 3236 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       3 | 3237 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       3 | 3238 | `	return SXRET_OK;` |
|       1 | 3239 | `Synchronize:` |
|       - | 3240 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3241 | `	 * statement.` |
|       - | 3242 | `	 */` |
|       5 | 3243 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3244 | `		pGen->pIn++;` |
|       1 | 3245 | `	}` |
|       3 | 3246 | `	return SXRET_OK;` |
|       3 | 3247 |  |
|       - | 3248 | `/*` |
|       - | 3249 | ` * Compile the var statement.` |
|       - | 3250 | ` * Symisc Extension:` |
|       - | 3251 | ` *      var statement can be used outside of a class definition.` |
|       - | 3252 | ` */` |
|       4 | 3253 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3254 |  |
|       - | 3255 | `	sxu32 nLine;` |
|       - | 3256 | `	sxi32 rc;` |
|       5 | 3257 | `	nLine = pGen->pIn->nLine;` |
|       - | 3258 | `	/* Jump the 'var' keyword */` |
|       5 | 3259 | `	pGen->pIn++;` |
|       5 | 3260 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3261 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3262 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3263 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3264 | `			pGen->pIn++;` |
|     ! 0 | 3265 | `		}` |
|     ! 0 | 3266 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3267 | `			return SXERR_ABORT;` |
|       - | 3268 | `		}` |
|     ! 0 | 3269 | `	}else{` |
|       - | 3270 | `		/* Compile the expression */` |
|       5 | 3271 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3272 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3273 | `			return SXERR_ABORT;` |
|       5 | 3274 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3275 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3276 | `		}` |
|       - | 3277 | `	}` |
|       5 | 3278 | `	return SXRET_OK;` |
|       3 | 3279 |  |
|       - | 3280 | `/*` |
|       - | 3281 | ` * Compile a namespace statement` |
|       - | 3282 | ` * According to the PHP language reference manual` |
|       - | 3283 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3284 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3285 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3286 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3287 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3288 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3289 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3290 | ` *  programming world.` |
|       - | 3291 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3292 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3293 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3294 | ` *  classes/functions/constants.` |
|       - | 3295 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3296 | ` *  readability of source code.` |
|       - | 3297 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3298 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3299 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3300 | ` *       class MyClass {}` |
|       - | 3301 | ` *       function myfunction() {}` |
|       - | 3302 | ` *       const MYCONST = 1;` |
|       - | 3303 | ` *       $a = new MyClass;` |
|       - | 3304 | ` *       $c = new \my\name\MyClass;` |
|       - | 3305 | ` *       $a = strlen('hi');` |
|       - | 3306 | ` *       $d = namespace\MYCONST;` |
|       - | 3307 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3308 | ` *       echo constant($d);` |
|       - | 3309 | ` * NOTE` |
|       - | 3310 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3311 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3312 | ` */` |
|       6 | 3313 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3314 |  |
|       - | 3315 | `	sxu32 nLine;` |
|       7 | 3316 | `	nLine = pGen->pIn->nLine;` |
|       - | 3317 | `	sxi32 rc;` |
|       7 | 3318 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       7 | 3319 | `	if( pGen->pIn >= pGen->pEnd \|\|` |
|       6 | 3320 | `		(pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       3 | 3321 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 3322 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 3323 | `				pTok--;` |
|     ! 0 | 3324 | `			}` |
|       - | 3325 | `			/* Unexpected token */` |
|       3 | 3326 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace: Unexpected token '%z'",&pTok->sData);` |
|       3 | 3327 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3328 | `				return SXERR_ABORT;` |
|       - | 3329 | `			}` |
|       1 | 3330 | `	}` |
|       - | 3331 | `	/* Ignore the path */` |
|      19 | 3332 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP/*'\'*/\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 | 3333 | `		pGen->pIn++;` |
|       1 | 3334 | `	}` |
|       7 | 3335 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 3336 | `		/* Unexpected token */` |
|       7 | 3337 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       4 | 3338 | `			"Namespace: Unexpected token '%z',expecting ';' or '{'",&pGen->pIn->sData);` |
|       5 | 3339 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3340 | `			return SXERR_ABORT;` |
|       - | 3341 | `		}` |
|       2 | 3342 | `	}` |
|       - | 3343 | `	/* Emit a warning */` |
|      10 | 3344 | `	PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|       3 | 3345 | `		"Namespace support is disabled in the current release of the PH7(%s) engine",ph7_lib_version());` |
|       7 | 3346 | `	return SXRET_OK;` |
|       4 | 3347 |  |
|       - | 3348 | `/*` |
|       - | 3349 | ` * Compile the 'use' statement` |
|       - | 3350 | ` * According to the PHP language reference manual` |
|       - | 3351 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3352 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3353 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3354 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3355 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3356 | ` *  a function or constant is not supported.` |
|       - | 3357 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3358 | ` * NOTE` |
|       - | 3359 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3360 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3361 | ` */` |
|       8 | 3362 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3363 |  |
|       - | 3364 | `	sxu32 nLine;` |
|       9 | 3365 | `	nLine = pGen->pIn->nLine;` |
|       - | 3366 | `	sxi32 rc;` |
|       9 | 3367 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3368 | `	/* Assemeble one or more real namespace path */` |
|       4 | 3369 | `	for(;;){` |
|       9 | 3370 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3371 | `			break;` |
|       - | 3372 | `		}` |
|       - | 3373 | `		/* Ignore the path */` |
|      21 | 3374 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID))  ){` |
|      13 | 3375 | `			pGen->pIn++;` |
|       1 | 3376 | `		}` |
|       9 | 3377 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){` |
|     ! 0 | 3378 | `			pGen->pIn++; /* Jump the comma and process the next path */` |
|     ! 0 | 3379 | `		}else{` |
|       5 | 3380 | `			break;` |
|       - | 3381 | `		}` |
|     ! 0 | 3382 | `	}` |
|       9 | 3383 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       3 | 3384 | `		pGen->pIn++; /* Jump the 'as' keyword */` |
|       - | 3385 | `		/* Compile one or more aliasses */` |
|       1 | 3386 | `		for(;;){` |
|       3 | 3387 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3388 | `				break;` |
|       - | 3389 | `			}` |
|       5 | 3390 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|       3 | 3391 | `				pGen->pIn++;` |
|       1 | 3392 | `			}` |
|       3 | 3393 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){` |
|     ! 0 | 3394 | `				pGen->pIn++; /* Jump the comma and process the next alias */` |
|     ! 0 | 3395 | `			}else{` |
|       2 | 3396 | `				break;` |
|       - | 3397 | `			}` |
|     ! 0 | 3398 | `		}` |
|       1 | 3399 | `	}` |
|       9 | 3400 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|       - | 3401 | `		/* Unexpected token */` |
|       4 | 3402 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"use statement: Unexpected token '%z',expecting ';'",` |
|       2 | 3403 | `			&pGen->pIn->sData);` |
|       3 | 3404 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3405 | `			return SXERR_ABORT;` |
|       - | 3406 | `		}` |
|       1 | 3407 | `	}` |
|       - | 3408 | `	/* Emit a notice */` |
|      13 | 3409 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - | 3410 | `		"Namespace support is disabled in the current release of the PH7(%s) engine",` |
|       4 | 3411 | `		ph7_lib_version()` |
|       - | 3412 | `		);` |
|       9 | 3413 | `	return SXRET_OK;` |
|       5 | 3414 |  |
|       - | 3415 | `/*` |
|       - | 3416 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3417 | ` *` |
|       - | 3418 | ` * According to the PHP language reference manual.` |
|       - | 3419 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3420 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3421 | ` *  declare (directive)` |
|       - | 3422 | ` *   statement` |
|       - | 3423 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3424 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3425 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3426 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3427 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3428 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3429 | ` * <?php` |
|       - | 3430 | ` * // these are the same:` |
|       - | 3431 | ` * // you can use this:` |
|       - | 3432 | ` * declare(ticks=1) {` |
|       - | 3433 | ` *   // entire script here` |
|       - | 3434 | ` * }` |
|       - | 3435 | ` * // or you can use this:` |
|       - | 3436 | ` * declare(ticks=1);` |
|       - | 3437 | ` * // entire script here` |
|       - | 3438 | ` * ?>` |
|       - | 3439 | ` *` |
|       - | 3440 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3441 | ` */` |
|       8 | 3442 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3443 |  |
|       9 | 3444 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3445 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3446 | `	sxi32 rc;` |
|       9 | 3447 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3449 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3451 | `			return SXERR_ABORT;` |
|       - | 3452 | `		}` |
|       5 | 3453 | `		goto Synchro;` |
|       - | 3454 | `	}` |
|       5 | 3455 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3456 | `	/* Delimit the directive */` |
|       5 | 3457 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3458 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3461 | `			return SXERR_ABORT;` |
|       - | 3462 | `		}` |
|     ! 0 | 3463 | `		return SXRET_OK;` |
|       - | 3464 | `	}` |
|       - | 3465 | `	/* Update the cursor */` |
|       5 | 3466 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3467 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3468 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3469 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3470 | `			return SXERR_ABORT;` |
|       - | 3471 | `		}` |
|     ! 0 | 3472 | `	}` |
|       - | 3473 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3474 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3475 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3476 | `		ph7_lib_version()` |
|       - | 3477 | `		);` |
|       - | 3478 | `	/*All done */` |
|       5 | 3479 | `	return SXRET_OK;` |
|       2 | 3480 | `Synchro:` |
|       - | 3481 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3482 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3483 | `		pGen->pIn++;` |
|       1 | 3484 | `	}` |
|       5 | 3485 | `	return SXRET_OK;` |
|       5 | 3486 |  |
|       - | 3487 | `/*` |
|       - | 3488 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3489 | ` * as follows:` |
|       - | 3490 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3491 | ` * {` |
|       - | 3492 | ` *   return "Making a cup of $type.\n";` |
|       - | 3493 | ` * }` |
|       - | 3494 | ` * Symisc eXtension.` |
|       - | 3495 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3496 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3497 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3498 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3499 | ` *      {` |
|       - | 3500 | ` *       var_dump($a);` |
|       - | 3501 | ` *      }` |
|       - | 3502 | ` *     //call test without args` |
|       - | 3503 | ` *      test();` |
|       - | 3504 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3505 | ` *      Example:` |
|       - | 3506 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3507 | ` * 3 -) Function overloading!!` |
|       - | 3508 | ` *      Example:` |
|       - | 3509 | ` *      function foo($a) {` |
|       - | 3510 | ` *   	  return $a.PHP_EOL;` |
|       - | 3511 | ` *	    }` |
|       - | 3512 | ` *	    function foo($a, $b) {` |
|       - | 3513 | ` *   	  return $a + $b;` |
|       - | 3514 | ` *	    }` |
|       - | 3515 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3516 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3517 | ` *      // Same arg` |
|       - | 3518 | ` *	   function foo(string $a)` |
|       - | 3519 | ` *	   {` |
|       - | 3520 | ` *	     echo "a is a string\n";` |
|       - | 3521 | ` *	     var_dump($a);` |
|       - | 3522 | ` *	   }` |
|       - | 3523 | ` *	  function foo(int $a)` |
|       - | 3524 | ` *	  {` |
|       - | 3525 | ` *	    echo "a is integer\n";` |
|       - | 3526 | ` *	    var_dump($a);` |
|       - | 3527 | ` *	  }` |
|       - | 3528 | ` *	  function foo(array $a)` |
|       - | 3529 | ` *	  {` |
|       - | 3530 | ` * 	    echo "a is an array\n";` |
|       - | 3531 | ` * 	    var_dump($a);` |
|       - | 3532 | ` *	  }` |
|       - | 3533 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3534 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3535 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3536 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3537 | ` * introduced by the PH7 engine.` |
|       - | 3538 | ` */` |
|   69346 | 3539 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       1 | 3540 |  |
|       - | 3541 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3542 | `	SySet *pInstrContainer;` |
|       - | 3543 | `	sxi32 rc;` |
|       - | 3544 | `	/* Swap token stream */` |
|   69347 | 3545 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   69347 | 3546 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   69347 | 3547 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3548 | `	/* Compile the expression holding the argument value */` |
|   69347 | 3549 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3550 | `	/* Emit the done instruction */` |
|   69347 | 3551 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   69347 | 3552 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   69347 | 3553 | `	RE_SWAP_DELIMITER(pGen);` |
|   69347 | 3554 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3555 | `		return SXERR_ABORT;` |
|       - | 3556 | `	}` |
|   69347 | 3557 | `	return SXRET_OK;` |
|   34674 | 3558 |  |
|       - | 3559 | `/*` |
|       - | 3560 | ` * Collect function arguments one after one.` |
|       - | 3561 | ` * According to the PHP language reference manual.` |
|       - | 3562 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3563 | ` * list of expressions.` |
|       - | 3564 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3565 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3566 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3567 | ` * for more information.` |
|       - | 3568 | ` * Example #1 Passing arrays to functions` |
|       - | 3569 | ` * <?php` |
|       - | 3570 | ` * function takes_array($input)` |
|       - | 3571 | ` * {` |
|       - | 3572 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3573 | ` * }` |
|       - | 3574 | ` * ?>` |
|       - | 3575 | ` * Making arguments be passed by reference` |
|       - | 3576 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3577 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3578 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3579 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3580 | ` * to the argument name in the function definition:` |
|       - | 3581 | ` * Example #2 Passing function parameters by reference` |
|       - | 3582 | ` * <?php` |
|       - | 3583 | ` * function add_some_extra(&$string)` |
|       - | 3584 | ` * {` |
|       - | 3585 | ` *   $string .= 'and something extra.';` |
|       - | 3586 | ` * }` |
|       - | 3587 | ` * $str = 'This is a string, ';` |
|       - | 3588 | ` * add_some_extra($str);` |
|       - | 3589 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3590 | ` * ?>` |
|       - | 3591 | ` *` |
|       - | 3592 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3593 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3594 | ` * on these extension.` |
|       - | 3595 | ` */` |
|   80350 | 3596 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       1 | 3597 |  |
|       - | 3598 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3599 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3600 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3601 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3602 | `	sxi32 rc;` |
|       - | 3603 |  |
|   80351 | 3604 | `	pIn = pGen->pIn;` |
|   80351 | 3605 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3606 | `	/* Process arguments one after one */` |
|  109866 | 3607 | `	for(;;){` |
|  219733 | 3608 | `		if( pIn >= pEnd ){` |
|       - | 3609 | `			/* No more arguments to process */` |
|   80349 | 3610 | `			break;` |
|       - | 3611 | `		}` |
|  139385 | 3612 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  139385 | 3613 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  139385 | 3614 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|  106683 | 3615 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   96015 | 3616 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   96015 | 3617 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3618 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   96015 | 3619 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3620 | `					sArg.nType = MEMOBJ_BOOL;` |
|   96015 | 3621 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   26671 | 3622 | `					sArg.nType = MEMOBJ_INT;` |
|   82680 | 3623 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   69343 | 3624 | `					sArg.nType = MEMOBJ_STRING;` |
|   34674 | 3625 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3626 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3627 | `				}else{` |
|       4 | 3628 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3629 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3630 | `						&pIn->sData);` |
|       - | 3631 | `				}` |
|   48008 | 3632 | `			}else{` |
|   10669 | 3633 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3634 | `				char *zDupLocal;` |
|       - | 3635 | `				/* Argument must be a class instance,record that*/` |
|   10669 | 3636 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   10669 | 3637 | `				if( zDupLocal ){` |
|   10669 | 3638 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|   10669 | 3639 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    5334 | 3640 | `				}` |
|       - | 3641 | `			}` |
|  106683 | 3642 | `			pIn++;` |
|   53341 | 3643 | `		}` |
|  139385 | 3644 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3645 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3646 | `			return rc;` |
|       - | 3647 | `		}` |
|  139385 | 3648 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3649 | `			/* Pass by reference,record that */` |
|    5353 | 3650 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    5353 | 3651 | `			pIn++;` |
|    2676 | 3652 | `		}` |
|  139385 | 3653 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3654 | `			/* Invalid argument */` |
|     ! 0 | 3655 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3656 | `			return rc;` |
|       - | 3657 | `		}` |
|  139385 | 3658 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3659 | `		/* Copy argument name */` |
|  139385 | 3660 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  139385 | 3661 | `		if( zDup == 0 ){` |
|     ! 0 | 3662 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3663 | `			return SXERR_ABORT;` |
|       - | 3664 | `		}` |
|  139385 | 3665 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  139385 | 3666 | `		pIn++;` |
|  139385 | 3667 | `		if( pIn < pEnd ){` |
|   85713 | 3668 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 3669 | `				SyToken *pDefend;` |
|   69349 | 3670 | `				sxi32 iNest = 0;` |
|   69349 | 3671 | `				pIn++; /* Jump the equal sign */` |
|   69349 | 3672 | `				pDefend = pIn;` |
|       - | 3673 | `				/* Process the default value associated with this argument */` |
|  149363 | 3674 | `				while( pDefend < pEnd ){` |
|  122687 | 3675 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   42673 | 3676 | `						break;` |
|       - | 3677 | `					}` |
|   80015 | 3678 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 3679 | `						/* Increment nesting level */` |
|    5335 | 3680 | `						iNest++;` |
|   77348 | 3681 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 3682 | `						/* Decrement nesting level */` |
|    5335 | 3683 | `						iNest--;` |
|    2667 | 3684 | `					}` |
|   80015 | 3685 | `					pDefend++;` |
|       1 | 3686 | `				}` |
|   69349 | 3687 | `				if( pIn >= pDefend ){` |
|       3 | 3688 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 3689 | `					return rc;` |
|       - | 3690 | `				}` |
|       - | 3691 | `				/* Process default value */` |
|   69347 | 3692 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   69347 | 3693 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3694 | `					return rc;` |
|       - | 3695 | `				}` |
|       - | 3696 | `				/* Point beyond the default value */` |
|   69347 | 3697 | `				pIn = pDefend;` |
|   34673 | 3698 | `			}` |
|   85711 | 3699 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 3700 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 3701 | `				return rc;` |
|       - | 3702 | `			}` |
|   85711 | 3703 | `			pIn++; /* Jump the trailing comma */` |
|   42855 | 3704 | `		}` |
|       - | 3705 | `		/* Append argument signature */` |
|  139383 | 3706 | `		if( sArg.nType > 0 ){` |
|  106681 | 3707 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 3708 | `				/* Class name */` |
|   10669 | 3709 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    5335 | 3710 | `			}else{` |
|       - | 3711 | `				int c;` |
|   96013 | 3712 | `				c = 'n'; /* cc warning */` |
|       - | 3713 | `				/* Type leading character */` |
|   96013 | 3714 | `				switch(sArg.nType){` |
|     ! 0 | 3715 | `				case MEMOBJ_HASHMAP:` |
|       - | 3716 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 3717 | `					c = 'h';` |
|     ! 0 | 3718 | `					break;` |
|   13335 | 3719 | `				case MEMOBJ_INT:` |
|       - | 3720 | `					/* Integer */` |
|   26671 | 3721 | `					c = 'i';` |
|   26671 | 3722 | `					break;` |
|     ! 0 | 3723 | `				case MEMOBJ_BOOL:` |
|       - | 3724 | `					/* Bool */` |
|     ! 0 | 3725 | `					c = 'b';` |
|     ! 0 | 3726 | `					break;` |
|     ! 0 | 3727 | `				case MEMOBJ_REAL:` |
|       - | 3728 | `					/* Float */` |
|     ! 0 | 3729 | `					c = 'f';` |
|     ! 0 | 3730 | `					break;` |
|   34671 | 3731 | `				case MEMOBJ_STRING:` |
|       - | 3732 | `					/* String */` |
|   69343 | 3733 | `					c = 's';` |
|   69342 | 3734 | `					break;` |
|     ! 0 | 3735 | `				default:` |
|     ! 0 | 3736 | `					break;` |
|       - | 3737 | `				}` |
|   96013 | 3738 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 3739 | `			}` |
|   53341 | 3740 | `		}else{` |
|       - | 3741 | `			/* No type is associated with this parameter which mean` |
|       - | 3742 | `			 * that this function is not condidate for overloading.` |
|       - | 3743 | `			 */` |
|   32703 | 3744 | `			SyBlobRelease(&sSig);` |
|       - | 3745 | `		}` |
|       - | 3746 | `		/* Save in the argument set */` |
|  139383 | 3747 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       1 | 3748 | `	}` |
|   80349 | 3749 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 3750 | `		/* Save function signature */` |
|   64009 | 3751 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   32004 | 3752 | `	}` |
|   80349 | 3753 | `	return SXRET_OK;` |
|   40176 | 3754 |  |
|       - | 3755 | `/*` |
|       - | 3756 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 3757 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3758 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 3759 | ` */` |
|  181940 | 3760 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 3761 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 3762 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 3763 | `	)` |
|       1 | 3764 |  |
|       - | 3765 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 3766 | `	GenBlock *pBlock;` |
|       - | 3767 | `	sxu32 nGotoOfft;` |
|       - | 3768 | `	sxi32 rc;` |
|       - | 3769 | `	/* Attach the new function */` |
|  181941 | 3770 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  181941 | 3771 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3772 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 3773 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3774 | `		return SXERR_ABORT;` |
|       - | 3775 | `	}` |
|  181941 | 3776 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 3777 | `	/* Swap bytecode containers */` |
|  181941 | 3778 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  181941 | 3779 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 3780 | `	/* Compile the body */` |
|  181941 | 3781 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 3782 | `	/* Fix exception jumps now the destination is resolved */` |
|  181941 | 3783 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3784 | `	/* Emit the final return if not yet done */` |
|  181941 | 3785 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 3786 | `	/* Fix gotos jumps now the destination is resolved */` |
|  181941 | 3787 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 3788 | `		rc = SXERR_ABORT;` |
|     ! 0 | 3789 | `	}` |
|  181941 | 3790 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 3791 | `	/* Restore the default container */` |
|  181941 | 3792 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 3793 | `	/* Leave function block */` |
|  181941 | 3794 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  181941 | 3795 | `	if( rc == SXERR_ABORT ){` |
|       - | 3796 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3797 | `		return SXERR_ABORT;` |
|       - | 3798 | `	}` |
|       - | 3799 | `	/* All done, function body compiled */` |
|  181941 | 3800 | `	return SXRET_OK;` |
|   90971 | 3801 |  |
|       - | 3802 | `/*` |
|       - | 3803 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 3804 | ` * According to the PHP language reference manual.` |
|       - | 3805 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 3806 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 3807 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 3808 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 3809 | ` *  Functions need not be defined before they are referenced.` |
|       - | 3810 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 3811 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 3812 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 3813 | ` *  calls with over 32-64 recursion levels.` |
|       - | 3814 | ` *` |
|       - | 3815 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 3816 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 3817 | ` * on these extension.` |
|       - | 3818 | ` */` |
|   69738 | 3819 | `static sxi32 GenStateCompileFunc(` |
|       - | 3820 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 3821 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 3822 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 3823 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 3824 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 3825 | `	)` |
|       1 | 3826 |  |
|       - | 3827 | `	ph7_vm_func *pFunc;` |
|       - | 3828 | `	SyToken *pEnd;` |
|       - | 3829 | `	sxu32 nLine;` |
|       - | 3830 | `	char *zName;` |
|       - | 3831 | `	sxi32 rc;` |
|       - | 3832 | `	/* Extract line number */` |
|   69739 | 3833 | `	nLine = pGen->pIn->nLine;` |
|       - | 3834 | `	/* Jump the left parenthesis '(' */` |
|   69739 | 3835 | `	pGen->pIn++;` |
|       - | 3836 | `	/* Delimit the function signature */` |
|   69739 | 3837 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   69739 | 3838 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 3839 | `		/* Syntax error */` |
|       7 | 3840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 3841 | `		if( rc == SXERR_ABORT ){` |
|       - | 3842 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3843 | `			return SXERR_ABORT;` |
|       - | 3844 | `		}` |
|       7 | 3845 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 3846 | `		return SXRET_OK;` |
|       - | 3847 | `	}` |
|       - | 3848 | `	/* Create the function state */` |
|   69733 | 3849 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   69733 | 3850 | `	if( pFunc == 0 ){` |
|     ! 0 | 3851 | `		goto OutOfMem;` |
|       - | 3852 | `	}` |
|       - | 3853 | `	/* function ID */` |
|   69733 | 3854 | `	zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   69733 | 3855 | `	if( zName == 0 ){` |
|       - | 3856 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3857 | `		goto OutOfMem;` |
|       - | 3858 | `	}` |
|       - | 3859 | `	/* Initialize the function state */` |
|   69733 | 3860 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|   69733 | 3861 | `	if( pGen->pIn < pEnd ){` |
|       - | 3862 | `		/* Collect function arguments */` |
|   53637 | 3863 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   53637 | 3864 | `		if( rc == SXERR_ABORT ){` |
|       - | 3865 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3866 | `			return SXERR_ABORT;` |
|       - | 3867 | `		}` |
|   26818 | 3868 | `	}` |
|       - | 3869 | `	/* Compile function body */` |
|   69733 | 3870 | `	pGen->pIn = &pEnd[1];` |
|   69733 | 3871 | `	if( bHandleClosure ){` |
|       - | 3872 | `		ph7_vm_func_closure_env sEnv;` |
|      65 | 3873 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|      64 | 3874 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      36 | 3875 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|       7 | 3876 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 3877 | `				/* Closure,record environment variable */` |
|       7 | 3878 | `				pGen->pIn++;` |
|       7 | 3879 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 3880 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 3881 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3882 | `						return SXERR_ABORT;` |
|       - | 3883 | `					}` |
|     ! 0 | 3884 | `				}` |
|       7 | 3885 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 3886 | `				/* Compile until we hit the first closing parenthesis */` |
|      13 | 3887 | `				while( pGen->pIn < pGen->pEnd ){` |
|      13 | 3888 | `					int iFlagsLocal = 0;` |
|      13 | 3889 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|       7 | 3890 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|       7 | 3891 | `						break;` |
|       - | 3892 | `					}` |
|       7 | 3893 | `					nLineLocal = pGen->pIn->nLine;` |
|       7 | 3894 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 3895 | `						/* Pass by reference,record that */` |
|     ! 0 | 3896 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 3897 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 3898 | `							);` |
|     ! 0 | 3899 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 3900 | `						pGen->pIn++;` |
|     ! 0 | 3901 | `					}` |
|       6 | 3902 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|       7 | 3903 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 3904 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 3905 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 3906 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 3907 | `								return SXERR_ABORT;` |
|       - | 3908 | `							}` |
|       - | 3909 | `							/* Find the closing parenthesis */` |
|     ! 0 | 3910 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 3911 | `								pGen->pIn++;` |
|     ! 0 | 3912 | `							}` |
|     ! 0 | 3913 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 3914 | `								pGen->pIn++;` |
|     ! 0 | 3915 | `							}` |
|     ! 0 | 3916 | `							break;` |
|       - | 3917 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 3918 | `					}else{` |
|       - | 3919 | `						SyString *pNameLocal;` |
|       - | 3920 | `						char *zDup;` |
|       - | 3921 | `						/* Duplicate variable name */` |
|       7 | 3922 | `						pNameLocal = &pGen->pIn[1].sData;` |
|       7 | 3923 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|       7 | 3924 | `						if( zDup ){` |
|       - | 3925 | `							/* Zero the structure */` |
|       7 | 3926 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       7 | 3927 | `							sEnv.iFlags = iFlagsLocal;` |
|       7 | 3928 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       7 | 3929 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|       7 | 3930 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 3931 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 3932 | `									got_this = 1;` |
|     ! 0 | 3933 | `							}` |
|       - | 3934 | `							/* Save imported variable */` |
|       7 | 3935 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 3936 | `						}else{` |
|     ! 0 | 3937 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3938 | `							 return SXERR_ABORT;` |
|       - | 3939 | `						}` |
|       - | 3940 | `					}` |
|       7 | 3941 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|       7 | 3942 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 3943 | `						/* Ignore trailing commas */` |
|     ! 0 | 3944 | `						pGen->pIn++;` |
|     ! 0 | 3945 | `					}` |
|       1 | 3946 | `				}` |
|       7 | 3947 | `				if( !got_this ){` |
|       - | 3948 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 3949 | `					 * available to the closure environment.` |
|       - | 3950 | `					 */` |
|       7 | 3951 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       7 | 3952 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|       7 | 3953 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       7 | 3954 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|       7 | 3955 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       3 | 3956 | `				}` |
|       7 | 3957 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 3958 | `					/* Mark as closure */` |
|       7 | 3959 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       3 | 3960 | `				}` |
|       3 | 3961 | `		}` |
|      32 | 3962 | `	}` |
|       - | 3963 | `	/* Compile the body */` |
|   69733 | 3964 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   69733 | 3965 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3966 | `		return SXERR_ABORT;` |
|       - | 3967 | `	}` |
|   69733 | 3968 | `	if( ppFunc ){` |
|      65 | 3969 | `		*ppFunc = pFunc;` |
|      32 | 3970 | `	}` |
|   69733 | 3971 | `	rc = SXRET_OK;` |
|   69733 | 3972 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 3973 | `		/* Finally register the function */` |
|   69727 | 3974 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   34863 | 3975 | `	}` |
|   69733 | 3976 | `	if( rc == SXRET_OK ){` |
|   69733 | 3977 | `		return SXRET_OK;` |
|       - | 3978 | `	}` |
|       - | 3979 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 3980 | `OutOfMem:` |
|       - | 3981 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 3982 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 3983 | `	 */` |
|     ! 0 | 3984 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3985 | `	return SXERR_ABORT;` |
|   34870 | 3986 |  |
|       - | 3987 | `/*` |
|       - | 3988 | ` * Compile a standard PHP function.` |
|       - | 3989 | ` *  Refer to the block-comment above for more information.` |
|       - | 3990 | ` */` |
|   69680 | 3991 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       1 | 3992 |  |
|       - | 3993 | `	SyString *pName;` |
|       - | 3994 | `	sxi32 iFlags;` |
|       - | 3995 | `	sxu32 nLine;` |
|       - | 3996 | `	sxi32 rc;` |
|       - | 3997 |  |
|   69681 | 3998 | `	nLine = pGen->pIn->nLine;` |
|   69681 | 3999 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   69681 | 4000 | `	iFlags = 0;` |
|   69681 | 4001 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4002 | `		/* Return by reference,remember that */` |
|       7 | 4003 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4004 | `		/* Jump the '&' token */` |
|       7 | 4005 | `		pGen->pIn++;` |
|       3 | 4006 | `	}` |
|   69681 | 4007 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4008 | `		/* Invalid function name */` |
|       5 | 4009 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4010 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4011 | `			return SXERR_ABORT;` |
|       - | 4012 | `		}` |
|       - | 4013 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4014 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4015 | `			pGen->pIn++;` |
|       1 | 4016 | `		}` |
|       5 | 4017 | `		return SXRET_OK;` |
|       - | 4018 | `	}` |
|   69677 | 4019 | `	pName = &pGen->pIn->sData;` |
|   69677 | 4020 | `	nLine = pGen->pIn->nLine;` |
|       - | 4021 | `	/* Jump the function name */` |
|   69677 | 4022 | `	pGen->pIn++;` |
|   69677 | 4023 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4024 | `		/* Syntax error */` |
|       3 | 4025 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4026 | `		if( rc == SXERR_ABORT ){` |
|       - | 4027 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4028 | `			return SXERR_ABORT;` |
|       - | 4029 | `		}` |
|       - | 4030 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4031 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4032 | `			pGen->pIn++;` |
|     ! 0 | 4033 | `		}` |
|       3 | 4034 | `		return SXRET_OK;` |
|       - | 4035 | `	}` |
|       - | 4036 | `	/* Compile function body */` |
|   69675 | 4037 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   69675 | 4038 | `	return rc;` |
|   34841 | 4039 |  |
|       - | 4040 | `/*` |
|       - | 4041 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4042 | ` * According to the PHP language reference manual` |
|       - | 4043 | ` *  Visibility:` |
|       - | 4044 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4045 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4046 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4047 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4048 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4049 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4050 | ` */` |
|  208428 | 4051 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       1 | 4052 |  |
|  208429 | 4053 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      43 | 4054 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  208387 | 4055 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   37365 | 4056 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4057 | `	}` |
|       - | 4058 | `	/* Assume public by default */` |
|  171023 | 4059 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  104215 | 4060 |  |
|       - | 4061 | `/*` |
|       - | 4062 | ` * Compile a class constant.` |
|       - | 4063 | ` * According to the PHP language reference manual` |
|       - | 4064 | ` *  Class Constants` |
|       - | 4065 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4066 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4067 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4068 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4069 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4070 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4071 | ` * Symisc eXtension.` |
|       - | 4072 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4073 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4074 | ` *  Example:` |
|       - | 4075 | ` *   class Test{` |
|       - | 4076 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4077 | ` *   };` |
|       - | 4078 | ` *   var_dump(TEST::MyConst);` |
|       - | 4079 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4080 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4081 | ` */` |
|      10 | 4082 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       1 | 4083 |  |
|      11 | 4084 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4085 | `	SySet *pInstrContainer;` |
|       - | 4086 | `	ph7_class_attr *pCons;` |
|       - | 4087 | `	SyString *pName;` |
|       - | 4088 | `	sxi32 rc;` |
|       - | 4089 | `	/* Extract visibility level */` |
|      11 | 4090 | `	iProtection = GetProtectionLevel(iProtection);` |
|      11 | 4091 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4092 | `loop:` |
|       - | 4093 | `	/* Mark as constant */` |
|      11 | 4094 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      11 | 4095 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4096 | `		/* Invalid constant name */` |
|     ! 0 | 4097 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4098 | `		if( rc == SXERR_ABORT ){` |
|       - | 4099 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4100 | `			return SXERR_ABORT;` |
|       - | 4101 | `		}` |
|     ! 0 | 4102 | `		goto Synchronize;` |
|       - | 4103 | `	}` |
|       - | 4104 | `	/* Peek constant name */` |
|      11 | 4105 | `	pName = &pGen->pIn->sData;` |
|       - | 4106 | `	/* Make sure the constant name isn't reserved */` |
|      11 | 4107 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4108 | `		/* Reserved constant name */` |
|     ! 0 | 4109 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4110 | `		if( rc == SXERR_ABORT ){` |
|       - | 4111 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4112 | `			return SXERR_ABORT;` |
|       - | 4113 | `		}` |
|     ! 0 | 4114 | `		goto Synchronize;` |
|       - | 4115 | `	}` |
|       - | 4116 | `	/* Advance the stream cursor */` |
|      11 | 4117 | `	pGen->pIn++;` |
|      11 | 4118 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4119 | `		/* Invalid declaration */` |
|     ! 0 | 4120 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4121 | `		if( rc == SXERR_ABORT ){` |
|       - | 4122 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4123 | `			return SXERR_ABORT;` |
|       - | 4124 | `		}` |
|     ! 0 | 4125 | `		goto Synchronize;` |
|       - | 4126 | `	}` |
|      11 | 4127 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4128 | `	/* Allocate a new class attribute */` |
|      11 | 4129 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      11 | 4130 | `	if( pCons == 0 ){` |
|     ! 0 | 4131 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4132 | `		return SXERR_ABORT;` |
|       - | 4133 | `	}` |
|       - | 4134 | `	/* Swap bytecode container */` |
|      11 | 4135 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      11 | 4136 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4137 | `	/* Compile constant value.` |
|       - | 4138 | `	 */` |
|      11 | 4139 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      11 | 4140 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4141 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4142 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4143 | `			return SXERR_ABORT;` |
|       - | 4144 | `		}` |
|       1 | 4145 | `	}` |
|       - | 4146 | `	/* Emit the done instruction */` |
|      11 | 4147 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      11 | 4148 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      11 | 4149 | `	if( rc == SXERR_ABORT ){` |
|       - | 4150 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4151 | `		return SXERR_ABORT;` |
|       - | 4152 | `	}` |
|       - | 4153 | `	/* All done,install the constant */` |
|      11 | 4154 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      11 | 4155 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4156 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4157 | `		return SXERR_ABORT;` |
|       - | 4158 | `	}` |
|      11 | 4159 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4160 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4161 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4162 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4163 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4164 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4165 | `				pTok--;` |
|     ! 0 | 4166 | `			}` |
|     ! 0 | 4167 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4168 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4169 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4170 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4171 | `				return SXERR_ABORT;` |
|       - | 4172 | `			}` |
|     ! 0 | 4173 | `		}else{` |
|     ! 0 | 4174 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4175 | `				goto loop;` |
|       - | 4176 | `			}` |
|       - | 4177 | `		}` |
|     ! 0 | 4178 | `	}` |
|      11 | 4179 | `	return SXRET_OK;` |
|     ! 0 | 4180 | `Synchronize:` |
|       - | 4181 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4182 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4183 | `		pGen->pIn++;` |
|     ! 0 | 4184 | `	}` |
|     ! 0 | 4185 | `	return SXERR_CORRUPT;` |
|       6 | 4186 |  |
|       - | 4187 | `/*` |
|       - | 4188 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4189 | ` * According to the PHP language reference manual` |
|       - | 4190 | ` *  Properties` |
|       - | 4191 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4192 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4193 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4194 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4195 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4196 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4197 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4198 | ` * Symisc eXtension.` |
|       - | 4199 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4200 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4201 | ` *  Example:` |
|       - | 4202 | ` *   class Test{` |
|       - | 4203 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4204 | ` *   };` |
|       - | 4205 | ` *   var_dump(TEST::myVar);` |
|       - | 4206 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4207 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4208 | ` */` |
|   53524 | 4209 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       1 | 4210 |  |
|   53525 | 4211 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4212 | `	ph7_class_attr *pAttr;` |
|       - | 4213 | `	SyString *pName;` |
|       - | 4214 | `	sxi32 rc;` |
|       - | 4215 | `	/* Extract visibility level */` |
|   53525 | 4216 | `	iProtection = GetProtectionLevel(iProtection);` |
|   26762 | 4217 | `loop:` |
|   53525 | 4218 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   53525 | 4219 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4220 | `		/* Invalid attribute name */` |
|     ! 0 | 4221 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4222 | `		if( rc == SXERR_ABORT ){` |
|       - | 4223 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4224 | `			return SXERR_ABORT;` |
|       - | 4225 | `		}` |
|     ! 0 | 4226 | `		goto Synchronize;` |
|       - | 4227 | `	}` |
|       - | 4228 | `	/* Peek attribute name */` |
|   53525 | 4229 | `	pName = &pGen->pIn->sData;` |
|       - | 4230 | `	/* Advance the stream cursor */` |
|   53525 | 4231 | `	pGen->pIn++;` |
|   53525 | 4232 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4233 | `		/* Invalid declaration */` |
|       3 | 4234 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4235 | `		if( rc == SXERR_ABORT ){` |
|       - | 4236 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4237 | `			return SXERR_ABORT;` |
|       - | 4238 | `		}` |
|       3 | 4239 | `		goto Synchronize;` |
|       - | 4240 | `	}` |
|       - | 4241 | `	/* Allocate a new class attribute */` |
|   53523 | 4242 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   53523 | 4243 | `	if( pAttr == 0 ){` |
|     ! 0 | 4244 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4245 | `		return SXERR_ABORT;` |
|       - | 4246 | `	}` |
|   53523 | 4247 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4248 | `		SySet *pInstrContainer;` |
|   21483 | 4249 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4250 | `		/* Swap bytecode container */` |
|   21483 | 4251 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21483 | 4252 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4253 | `		/* Compile attribute value.` |
|       - | 4254 | `		 */` |
|   21483 | 4255 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21483 | 4256 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4257 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4258 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4259 | `				return SXERR_ABORT;` |
|       - | 4260 | `			}` |
|     ! 0 | 4261 | `		}` |
|       - | 4262 | `		/* Emit the done instruction */` |
|   21483 | 4263 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21483 | 4264 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10741 | 4265 | `	}` |
|       - | 4266 | `	/* All done,install the attribute */` |
|   53523 | 4267 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   53523 | 4268 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4269 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4270 | `		return SXERR_ABORT;` |
|       - | 4271 | `	}` |
|   53523 | 4272 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4273 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4274 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4275 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4276 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4277 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4278 | `				pTok--;` |
|     ! 0 | 4279 | `			}` |
|     ! 0 | 4280 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4281 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4282 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4283 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4284 | `				return SXERR_ABORT;` |
|       - | 4285 | `			}` |
|     ! 0 | 4286 | `		}else{` |
|     ! 0 | 4287 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4288 | `				goto loop;` |
|       - | 4289 | `			}` |
|       - | 4290 | `		}` |
|     ! 0 | 4291 | `	}` |
|   53523 | 4292 | `	return SXRET_OK;` |
|       1 | 4293 | `Synchronize:` |
|       - | 4294 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4295 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4296 | `		pGen->pIn++;` |
|       1 | 4297 | `	}` |
|       3 | 4298 | `	return SXERR_CORRUPT;` |
|   26763 | 4299 |  |
|       - | 4300 | `/*` |
|       - | 4301 | ` * Compile a class method.` |
|       - | 4302 | ` *` |
|       - | 4303 | ` * Refer to the official documentation for more information` |
|       - | 4304 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4305 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4306 | ` * overloading and many more.` |
|       - | 4307 | ` */` |
|  154894 | 4308 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4309 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4310 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4311 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4312 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4313 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4314 | `	)` |
|       1 | 4315 |  |
|  154895 | 4316 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4317 | `	ph7_class_method *pMeth;` |
|       - | 4318 | `	sxi32 iFuncFlags;` |
|       - | 4319 | `	SyString *pName;` |
|       - | 4320 | `	SyToken *pEnd;` |
|       - | 4321 | `	sxi32 rc;` |
|       - | 4322 | `	/* Extract visibility level */` |
|  154895 | 4323 | `	iProtection = GetProtectionLevel(iProtection);` |
|  154895 | 4324 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  154895 | 4325 | `	iFuncFlags = 0;` |
|  154895 | 4326 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4327 | `		/* Invalid method name */` |
|     ! 0 | 4328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4329 | `		if( rc == SXERR_ABORT ){` |
|       - | 4330 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4331 | `			return SXERR_ABORT;` |
|       - | 4332 | `		}` |
|     ! 0 | 4333 | `		goto Synchronize;` |
|       - | 4334 | `	}` |
|  154895 | 4335 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4336 | `		/* Return by reference,remember that */` |
|     ! 0 | 4337 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4338 | `		/* Jump the '&' token */` |
|     ! 0 | 4339 | `		pGen->pIn++;` |
|     ! 0 | 4340 | `	}` |
|  154895 | 4341 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4342 | `		/* Invalid method name */` |
|     ! 0 | 4343 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4344 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4345 | `			return SXERR_ABORT;` |
|       - | 4346 | `		}` |
|     ! 0 | 4347 | `		goto Synchronize;` |
|       - | 4348 | `	}` |
|       - | 4349 | `	/* Peek method name */` |
|  154895 | 4350 | `	pName = &pGen->pIn->sData;` |
|  154895 | 4351 | `	nLine = pGen->pIn->nLine;` |
|       - | 4352 | `	/* Jump the method name */` |
|  154895 | 4353 | `	pGen->pIn++;` |
|  154895 | 4354 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4355 | `		/* Abstract method */` |
|       7 | 4356 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4357 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4358 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4359 | `				&pClass->sName,pName);` |
|     ! 0 | 4360 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4361 | `				return SXERR_ABORT;` |
|       - | 4362 | `			}` |
|     ! 0 | 4363 | `		}` |
|       - | 4364 | `		/* Assemble method signature only */` |
|       7 | 4365 | `		doBody = FALSE;` |
|       3 | 4366 | `	}` |
|  154895 | 4367 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4368 | `		/* Syntax error */` |
|     ! 0 | 4369 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4370 | `		if( rc == SXERR_ABORT ){` |
|       - | 4371 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4372 | `			return SXERR_ABORT;` |
|       - | 4373 | `		}` |
|     ! 0 | 4374 | `		goto Synchronize;` |
|       - | 4375 | `	}` |
|       - | 4376 | `	/* Allocate a new class_method instance */` |
|  154895 | 4377 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  154895 | 4378 | `	if( pMeth == 0 ){` |
|     ! 0 | 4379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4380 | `		return SXERR_ABORT;` |
|       - | 4381 | `	}` |
|       - | 4382 | `	/* Jump the left parenthesis '(' */` |
|  154895 | 4383 | `	pGen->pIn++;` |
|  154895 | 4384 | `	pEnd = 0; /* cc warning */` |
|       - | 4385 | `	/* Delimit the method signature */` |
|  154895 | 4386 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  154895 | 4387 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4388 | `		/* Syntax error */` |
|       3 | 4389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4390 | `		if( rc == SXERR_ABORT ){` |
|       - | 4391 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4392 | `			return SXERR_ABORT;` |
|       - | 4393 | `		}` |
|       3 | 4394 | `		goto Synchronize;` |
|       - | 4395 | `	}` |
|  154893 | 4396 | `	if( pGen->pIn < pEnd ){` |
|       - | 4397 | `		/* Collect method arguments */` |
|   26715 | 4398 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26715 | 4399 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4400 | `			return SXERR_ABORT;` |
|       - | 4401 | `		}` |
|   13357 | 4402 | `	}` |
|       - | 4403 | `	/* Point beyond method signature */` |
|  154893 | 4404 | `	pGen->pIn = &pEnd[1];` |
|  154893 | 4405 | `	if( doBody ){` |
|       - | 4406 | `		/* Compile method body */` |
|  112209 | 4407 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  112209 | 4408 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4409 | `			return SXERR_ABORT;` |
|       - | 4410 | `		}` |
|   56105 | 4411 | `	}else{` |
|       - | 4412 | `		/* Only method signature is allowed */` |
|   42685 | 4413 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4415 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4416 | `				if( rc == SXERR_ABORT ){` |
|       - | 4417 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4418 | `					return SXERR_ABORT;` |
|       - | 4419 | `				}` |
|     ! 0 | 4420 | `				return SXERR_CORRUPT;` |
|       - | 4421 | `			}` |
|       - | 4422 | `	}` |
|       - | 4423 | `	/* All done,install the method */` |
|  154893 | 4424 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  154893 | 4425 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4426 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4427 | `		return SXERR_ABORT;` |
|       - | 4428 | `	}` |
|  154893 | 4429 | `	return SXRET_OK;` |
|       1 | 4430 | `Synchronize:` |
|       - | 4431 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4432 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4433 | `		pGen->pIn++;` |
|       1 | 4434 | `	}` |
|       3 | 4435 | `	return SXERR_CORRUPT;` |
|   77448 | 4436 |  |
|       - | 4437 | `/*` |
|       - | 4438 | ` * Compile an object interface.` |
|       - | 4439 | ` *  According to the PHP language reference manual` |
|       - | 4440 | ` *   Object Interfaces:` |
|       - | 4441 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4442 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4443 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4444 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4445 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4446 | ` */` |
|   16012 | 4447 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       1 | 4448 |  |
|   16013 | 4449 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4450 | `	ph7_class *pClass,*pBase;` |
|       - | 4451 | `	SyToken *pEnd,*pTmp;` |
|       - | 4452 | `	SyString *pName;` |
|       - | 4453 | `	sxi32 nKwrd;` |
|       - | 4454 | `	sxi32 rc;` |
|       - | 4455 | `	/* Jump the 'interface' keyword */` |
|   16013 | 4456 | `	pGen->pIn++;` |
|       - | 4457 | `	/* Extract interface name */` |
|   16013 | 4458 | `	pName = &pGen->pIn->sData;` |
|       - | 4459 | `	/* Advance the stream cursor */` |
|   16013 | 4460 | `	pGen->pIn++;` |
|       - | 4461 | `	/* Obtain a raw class */` |
|   16013 | 4462 | `	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);` |
|   16013 | 4463 | `	if( pClass == 0 ){` |
|     ! 0 | 4464 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4465 | `		return SXERR_ABORT;` |
|       - | 4466 | `	}` |
|       - | 4467 | `	/* Mark as an interface */` |
|   16013 | 4468 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4469 | `	/* Assume no base class is given */` |
|   16013 | 4470 | `	pBase = 0;` |
|   16013 | 4471 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4472 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4473 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4474 | `			SyString *pBaseName;` |
|       - | 4475 | `			/* Extract base interface */` |
|       3 | 4476 | `			pGen->pIn++;` |
|       3 | 4477 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4478 | `				/* Syntax error */` |
|     ! 0 | 4479 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4480 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4481 | `					pName);` |
|     ! 0 | 4482 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4483 | `				if( rc == SXERR_ABORT ){` |
|       - | 4484 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4485 | `					return SXERR_ABORT;` |
|       - | 4486 | `				}` |
|     ! 0 | 4487 | `				return SXRET_OK;` |
|       - | 4488 | `			}` |
|       3 | 4489 | `			pBaseName = &pGen->pIn->sData;` |
|       3 | 4490 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4491 | `			/* Only interfaces is allowed */` |
|       3 | 4492 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4493 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4494 | `			}` |
|       3 | 4495 | `			if( pBase == 0 ){` |
|       - | 4496 | `				/* Inexistant interface */` |
|     ! 0 | 4497 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4498 | `				if( rc == SXERR_ABORT ){` |
|       - | 4499 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4500 | `					return SXERR_ABORT;` |
|       - | 4501 | `				}` |
|     ! 0 | 4502 | `			}` |
|       - | 4503 | `			/* Advance the stream cursor */` |
|       3 | 4504 | `			pGen->pIn++;` |
|       1 | 4505 | `		}` |
|       1 | 4506 | `	}` |
|   16013 | 4507 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4508 | `		/* Syntax error */` |
|     ! 0 | 4509 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4510 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4511 | `		if( rc == SXERR_ABORT ){` |
|       - | 4512 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4513 | `			return SXERR_ABORT;` |
|       - | 4514 | `		}` |
|     ! 0 | 4515 | `		return SXRET_OK;` |
|       - | 4516 | `	}` |
|   16013 | 4517 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   16013 | 4518 | `	pEnd = 0; /* cc warning */` |
|       - | 4519 | `	/* Delimit the interface body */` |
|   16013 | 4520 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   16013 | 4521 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4522 | `		/* Syntax error */` |
|     ! 0 | 4523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4524 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4525 | `		if( rc == SXERR_ABORT ){` |
|       - | 4526 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4527 | `			return SXERR_ABORT;` |
|       - | 4528 | `		}` |
|     ! 0 | 4529 | `		return SXRET_OK;` |
|       - | 4530 | `	}` |
|       - | 4531 | `	/* Swap token stream */` |
|   16013 | 4532 | `	pTmp = pGen->pEnd;` |
|   16013 | 4533 | `	pGen->pEnd = pEnd;` |
|       - | 4534 | `	/* Start the parse process` |
|       - | 4535 | `	 * Note (According to the PHP reference manual):` |
|       - | 4536 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4537 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4538 | `	 */` |
|   29346 | 4539 | `	for(;;){` |
|       - | 4540 | `		/* Jump leading/trailing semi-colons */` |
|  101373 | 4541 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   42681 | 4542 | `			pGen->pIn++;` |
|       1 | 4543 | `		}` |
|   58693 | 4544 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4545 | `			/* End of interface body */` |
|   16013 | 4546 | `			break;` |
|       - | 4547 | `		}` |
|   42681 | 4548 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4549 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4550 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4551 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4552 | `			if( rc == SXERR_ABORT ){` |
|       - | 4553 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4554 | `				return SXERR_ABORT;` |
|       - | 4555 | `			}` |
|     ! 0 | 4556 | `			goto done;` |
|       - | 4557 | `		}` |
|       - | 4558 | `		/* Extract the current keyword */` |
|   42681 | 4559 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   42681 | 4560 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4561 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4562 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4563 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4564 | `		}` |
|   42681 | 4565 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4566 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4567 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4568 | `			if( rc == SXERR_ABORT ){` |
|       - | 4569 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4570 | `				return SXERR_ABORT;` |
|       - | 4571 | `			}` |
|     ! 0 | 4572 | `			goto done;` |
|       - | 4573 | `		}` |
|   42681 | 4574 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4575 | `			/* Advance the stream cursor */` |
|   42679 | 4576 | `			pGen->pIn++;` |
|   42679 | 4577 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4578 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4579 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4580 | `				if( rc == SXERR_ABORT ){` |
|       - | 4581 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4582 | `					return SXERR_ABORT;` |
|       - | 4583 | `				}` |
|     ! 0 | 4584 | `				goto done;` |
|       - | 4585 | `			}` |
|   42679 | 4586 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   42679 | 4587 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4589 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4590 | `				if( rc == SXERR_ABORT ){` |
|       - | 4591 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4592 | `					return SXERR_ABORT;` |
|       - | 4593 | `				}` |
|     ! 0 | 4594 | `				goto done;` |
|       - | 4595 | `			}` |
|   21339 | 4596 | `		}` |
|   42681 | 4597 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4598 | `			/* Parse constant */` |
|       3 | 4599 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4600 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4601 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4602 | `					return SXERR_ABORT;` |
|       - | 4603 | `				}` |
|     ! 0 | 4604 | `				goto done;` |
|       - | 4605 | `			}` |
|       2 | 4606 | `		}else{` |
|   42679 | 4607 | `			sxi32 iFlags = 0;` |
|   42679 | 4608 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4609 | `				/* Static method,record that */` |
|     ! 0 | 4610 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4611 | `				/* Advance the stream cursor */` |
|     ! 0 | 4612 | `				pGen->pIn++;` |
|     ! 0 | 4613 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4614 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4615 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4616 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4617 | `						if( rc == SXERR_ABORT ){` |
|       - | 4618 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4619 | `							return SXERR_ABORT;` |
|       - | 4620 | `						}` |
|     ! 0 | 4621 | `						goto done;` |
|       - | 4622 | `				}` |
|     ! 0 | 4623 | `			}` |
|       - | 4624 | `			/* Process method signature */` |
|   42679 | 4625 | `			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);` |
|   42679 | 4626 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4627 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4628 | `					return SXERR_ABORT;` |
|       - | 4629 | `				}` |
|     ! 0 | 4630 | `				goto done;` |
|       - | 4631 | `			}` |
|       - | 4632 | `		}` |
|       1 | 4633 | `	}` |
|       - | 4634 | `	/* Install the interface */` |
|   16013 | 4635 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   16013 | 4636 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 4637 | `		/* Inherit from the base interface */` |
|       3 | 4638 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 4639 | `	}` |
|   16013 | 4640 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4641 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4642 | `		return SXERR_ABORT;` |
|       - | 4643 | `	}` |
|    8006 | 4644 | `done:` |
|       - | 4645 | `	/* Point beyond the interface body */` |
|   16013 | 4646 | `	pGen->pIn  = &pEnd[1];` |
|   16013 | 4647 | `	pGen->pEnd = pTmp;` |
|   16013 | 4648 | `	return PH7_OK;` |
|    8007 | 4649 |  |
|       - | 4650 | `/*` |
|       - | 4651 | ` * Compile a user-defined class.` |
|       - | 4652 | ` * According to the PHP language reference manual` |
|       - | 4653 | ` *  class` |
|       - | 4654 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 4655 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 4656 | ` *  of the properties and methods belonging to the class.` |
|       - | 4657 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 4658 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 4659 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 4660 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4661 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 4662 | ` *  (called "methods").` |
|       - | 4663 | ` */` |
|   21590 | 4664 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       1 | 4665 |  |
|   21591 | 4666 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4667 | `	ph7_class *pClass,*pBase;` |
|       - | 4668 | `	SyToken *pEnd,*pTmp;` |
|       - | 4669 | `	sxi32 iProtection;` |
|       - | 4670 | `	SySet aInterfaces;` |
|       - | 4671 | `	sxi32 iAttrflags;` |
|       - | 4672 | `	SyString *pName;` |
|       - | 4673 | `	sxi32 nKwrd;` |
|       - | 4674 | `	sxi32 rc;` |
|       - | 4675 | `	/* Jump the 'class' keyword */` |
|   21591 | 4676 | `	pGen->pIn++;` |
|   21591 | 4677 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4678 | `		/* Syntax error */` |
|     ! 0 | 4679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 4680 | `		if( rc == SXERR_ABORT ){` |
|       - | 4681 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4682 | `			return SXERR_ABORT;` |
|       - | 4683 | `		}` |
|       - | 4684 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 4685 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 4686 | `			pGen->pIn++;` |
|     ! 0 | 4687 | `		}` |
|     ! 0 | 4688 | `		return SXRET_OK;` |
|       - | 4689 | `	}` |
|       - | 4690 | `	/* Extract class name */` |
|   21591 | 4691 | `	pName = &pGen->pIn->sData;` |
|       - | 4692 | `	/* Advance the stream cursor */` |
|   21591 | 4693 | `	pGen->pIn++;` |
|       - | 4694 | `	/* Obtain a raw class */` |
|   21591 | 4695 | `	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);` |
|   21591 | 4696 | `	if( pClass == 0 ){` |
|     ! 0 | 4697 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4698 | `		return SXERR_ABORT;` |
|       - | 4699 | `	}` |
|       - | 4700 | `	/* implemented interfaces container */` |
|   21591 | 4701 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|       - | 4702 | `	/* Assume a standalone class */` |
|   21591 | 4703 | `	pBase = 0;` |
|   21591 | 4704 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4705 | `		SyString *pBaseName;` |
|    5379 | 4706 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    5379 | 4707 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|    5375 | 4708 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5375 | 4709 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4710 | `				/* Syntax error */` |
|     ! 0 | 4711 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4712 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 4713 | `					pName);` |
|     ! 0 | 4714 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4715 | `				if( rc == SXERR_ABORT ){` |
|       - | 4716 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4717 | `					return SXERR_ABORT;` |
|       - | 4718 | `				}` |
|     ! 0 | 4719 | `				return SXRET_OK;` |
|       - | 4720 | `			}` |
|       - | 4721 | `			/* Extract base class name */` |
|    5375 | 4722 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4723 | `			/* Perform the query */` |
|    5375 | 4724 | `			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);` |
|       - | 4725 | `			/* Interfaces are not allowed */` |
|    5375 | 4726 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 4727 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4728 | `			}` |
|    5375 | 4729 | `			if( pBase == 0 ){` |
|       - | 4730 | `				/* Inexistant base class */` |
|     ! 0 | 4731 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 4732 | `				if( rc == SXERR_ABORT ){` |
|       - | 4733 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4734 | `					return SXERR_ABORT;` |
|       - | 4735 | `				}` |
|     ! 0 | 4736 | `			}else{` |
|    5375 | 4737 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 4738 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 4739 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 4740 | `					if( rc == SXERR_ABORT ){` |
|       - | 4741 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4742 | `						return SXERR_ABORT;` |
|       - | 4743 | `					}` |
|     ! 0 | 4744 | `				}` |
|       - | 4745 | `			}` |
|       - | 4746 | `			/* Advance the stream cursor */` |
|    5375 | 4747 | `			pGen->pIn++;` |
|    2687 | 4748 | `		}` |
|    5379 | 4749 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 4750 | `			ph7_class *pInterface;` |
|       - | 4751 | `			SyString *pIntName;` |
|       - | 4752 | `			/* Interface implementation */` |
|       5 | 4753 | `			pGen->pIn++; /* Advance the stream cursor */` |
|       2 | 4754 | `			for(;;){` |
|       5 | 4755 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4756 | `					/* Syntax error */` |
|     ! 0 | 4757 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4758 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 4759 | `						pName);` |
|     ! 0 | 4760 | `					if( rc == SXERR_ABORT ){` |
|       - | 4761 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4762 | `						return SXERR_ABORT;` |
|       - | 4763 | `					}` |
|     ! 0 | 4764 | `					break;` |
|       - | 4765 | `				}` |
|       - | 4766 | `				/* Extract interface name */` |
|       5 | 4767 | `				pIntName = &pGen->pIn->sData;` |
|       - | 4768 | `				/* Make sure the interface is already defined */` |
|       5 | 4769 | `				pInterface = PH7_VmExtractClass(pGen->pVm,pIntName->zString,pIntName->nByte,FALSE,0);` |
|       - | 4770 | `				/* Only interfaces are allowed */` |
|       5 | 4771 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4772 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 4773 | `				}` |
|       5 | 4774 | `				if( pInterface == 0 ){` |
|       - | 4775 | `					/* Inexistant interface */` |
|     ! 0 | 4776 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 4777 | `					if( rc == SXERR_ABORT ){` |
|       - | 4778 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4779 | `						return SXERR_ABORT;` |
|       - | 4780 | `					}` |
|     ! 0 | 4781 | `				}else{` |
|       - | 4782 | `					/* Register interface */` |
|       5 | 4783 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 4784 | `				}` |
|       - | 4785 | `				/* Advance the stream cursor */` |
|       5 | 4786 | `				pGen->pIn++;` |
|       5 | 4787 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 4788 | `					break;` |
|       - | 4789 | `				}` |
|     ! 0 | 4790 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 4791 | `			}` |
|       2 | 4792 | `		}` |
|    2689 | 4793 | `	}` |
|   21591 | 4794 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4795 | `		/* Syntax error */` |
|     ! 0 | 4796 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 4797 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4798 | `		if( rc == SXERR_ABORT ){` |
|       - | 4799 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4800 | `			return SXERR_ABORT;` |
|       - | 4801 | `		}` |
|     ! 0 | 4802 | `		return SXRET_OK;` |
|       - | 4803 | `	}` |
|   21591 | 4804 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   21591 | 4805 | `	pEnd = 0; /* cc warning */` |
|       - | 4806 | `	/* Delimit the class body */` |
|   21591 | 4807 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   21591 | 4808 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4809 | `		/* Syntax error */` |
|     ! 0 | 4810 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 4811 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4812 | `		if( rc == SXERR_ABORT ){` |
|       - | 4813 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4814 | `			return SXERR_ABORT;` |
|       - | 4815 | `		}` |
|     ! 0 | 4816 | `		return SXRET_OK;` |
|       - | 4817 | `	}` |
|       - | 4818 | `	/* Swap token stream */` |
|   21591 | 4819 | `	pTmp = pGen->pEnd;` |
|   21591 | 4820 | `	pGen->pEnd = pEnd;` |
|       - | 4821 | `	/* Set the inherited flags */` |
|   21591 | 4822 | `	pClass->iFlags = iFlags;` |
|       - | 4823 | `	/* Start the parse process */` |
|   66910 | 4824 | `	for(;;){` |
|       - | 4825 | `		/* Jump leading/trailing semi-colons */` |
|  240871 | 4826 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   53537 | 4827 | `			pGen->pIn++;` |
|       1 | 4828 | `		}` |
|  187335 | 4829 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4830 | `			/* End of class body */` |
|   21587 | 4831 | `			break;` |
|       - | 4832 | `		}` |
|  165749 | 4833 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4834 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4835 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4836 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4837 | `			if( rc == SXERR_ABORT ){` |
|       - | 4838 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4839 | `				return SXERR_ABORT;` |
|       - | 4840 | `			}` |
|     ! 0 | 4841 | `			goto done;` |
|       - | 4842 | `		}` |
|       - | 4843 | `		/* Assume public visibility */` |
|  165749 | 4844 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  165749 | 4845 | `		iAttrflags = 0;` |
|  165749 | 4846 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 4847 | `			/* Extract the current keyword */` |
|  165749 | 4848 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  165749 | 4849 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  160379 | 4850 | `				iProtection = nKwrd;` |
|  160379 | 4851 | `				pGen->pIn++; /* Jump the visibility token */` |
|  160379 | 4852 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4853 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4854 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4855 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 4856 | `					if( rc == SXERR_ABORT ){` |
|       - | 4857 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 4858 | `						return SXERR_ABORT;` |
|       - | 4859 | `					}` |
|     ! 0 | 4860 | `					goto done;` |
|       - | 4861 | `				}` |
|  160379 | 4862 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4863 | `					/* Attribute declaration */` |
|   53513 | 4864 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   53513 | 4865 | `					if( rc != SXRET_OK ){` |
|       3 | 4866 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 4867 | `							return SXERR_ABORT;` |
|       - | 4868 | `						}` |
|       3 | 4869 | `						goto done;` |
|       - | 4870 | `					}` |
|   53511 | 4871 | `					continue;` |
|       - | 4872 | `				}` |
|       - | 4873 | `				/* Extract the keyword */` |
|  106867 | 4874 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53433 | 4875 | `			}` |
|  112237 | 4876 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4877 | `				/* Process constant declaration */` |
|       9 | 4878 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|       9 | 4879 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4880 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4881 | `						return SXERR_ABORT;` |
|       - | 4882 | `					}` |
|     ! 0 | 4883 | `					goto done;` |
|       - | 4884 | `				}` |
|       5 | 4885 | `			}else{` |
|  112229 | 4886 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4887 | `					/* Static method or attribute,record that */` |
|      23 | 4888 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 4889 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 4890 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4891 | `						/* Extract the keyword */` |
|      19 | 4892 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 4893 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 4894 | `							iProtection = nKwrd;` |
|     ! 0 | 4895 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 4896 | `						}` |
|       9 | 4897 | `					}` |
|      23 | 4898 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 4899 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4900 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 4901 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4902 | `						if( rc == SXERR_ABORT ){` |
|       - | 4903 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4904 | `							return SXERR_ABORT;` |
|       - | 4905 | `						}` |
|     ! 0 | 4906 | `						goto done;` |
|       - | 4907 | `					}` |
|      23 | 4908 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 4909 | `						/* Attribute declaration */` |
|       5 | 4910 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 4911 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 4912 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4913 | `								return SXERR_ABORT;` |
|       - | 4914 | `							}` |
|     ! 0 | 4915 | `							goto done;` |
|       - | 4916 | `						}` |
|       5 | 4917 | `						continue;` |
|       - | 4918 | `					}` |
|       - | 4919 | `					/* Extract the keyword */` |
|      19 | 4920 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  112216 | 4921 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 4922 | `					/* Abstract method,record that */` |
|       7 | 4923 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 4924 | `					/* Mark the whole class as abstract */` |
|       7 | 4925 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 4926 | `					/* Advance the stream cursor */` |
|       7 | 4927 | `					pGen->pIn++;` |
|       7 | 4928 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       7 | 4929 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       7 | 4930 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 4931 | `							iProtection = nKwrd;` |
|       5 | 4932 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 4933 | `						}` |
|       3 | 4934 | `					}` |
|       7 | 4935 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 4936 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4937 | `							/* Static method */` |
|     ! 0 | 4938 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4939 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4940 | `					}` |
|       7 | 4941 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 4942 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4943 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4944 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 4945 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4946 | `							if( rc == SXERR_ABORT ){` |
|       - | 4947 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4948 | `								return SXERR_ABORT;` |
|       - | 4949 | `							}` |
|     ! 0 | 4950 | `							goto done;` |
|       - | 4951 | `					}` |
|       7 | 4952 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  112204 | 4953 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 4954 | `					/* final method ,record that */` |
|       5 | 4955 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 4956 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 4957 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 4958 | `						/* Extract the keyword */` |
|       5 | 4959 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 4960 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 4961 | `							iProtection = nKwrd;` |
|       5 | 4962 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 4963 | `						}` |
|       2 | 4964 | `					}` |
|       5 | 4965 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 4966 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 4967 | `							/* Static method */` |
|     ! 0 | 4968 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 4969 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 4970 | `					}` |
|       5 | 4971 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 4972 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4973 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4974 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 4975 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 4976 | `							if( rc == SXERR_ABORT ){` |
|       - | 4977 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 4978 | `								return SXERR_ABORT;` |
|       - | 4979 | `							}` |
|     ! 0 | 4980 | `							goto done;` |
|       - | 4981 | `					}` |
|       5 | 4982 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 4983 | `				}` |
|  112225 | 4984 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 4985 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4986 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 4987 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 4988 | `						if( rc == SXERR_ABORT ){` |
|       - | 4989 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4990 | `							return SXERR_ABORT;` |
|       - | 4991 | `						}` |
|     ! 0 | 4992 | `						goto done;` |
|       - | 4993 | `				}` |
|  112225 | 4994 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       9 | 4995 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       9 | 4996 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 4997 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4998 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 4999 | `						if( rc == SXERR_ABORT ){` |
|       - | 5000 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5001 | `							return SXERR_ABORT;` |
|       - | 5002 | `						}` |
|     ! 0 | 5003 | `						goto done;` |
|       - | 5004 | `					}` |
|       - | 5005 | `					/* Attribute declaration */` |
|       9 | 5006 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5007 | `				}else{` |
|       - | 5008 | `					/* Process method declaration */` |
|  112217 | 5009 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5010 | `				}` |
|  112225 | 5011 | `				if( rc != SXRET_OK ){` |
|       3 | 5012 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5013 | `						return SXERR_ABORT;` |
|       - | 5014 | `					}` |
|       3 | 5015 | `					goto done;` |
|       - | 5016 | `				}` |
|       - | 5017 | `			}` |
|   56116 | 5018 | `		}else{` |
|       - | 5019 | `			/* Attribute declaration */` |
|     ! 0 | 5020 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5021 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5022 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5023 | `					return SXERR_ABORT;` |
|       - | 5024 | `				}` |
|     ! 0 | 5025 | `				goto done;` |
|       - | 5026 | `			}` |
|       - | 5027 | `		}` |
|       1 | 5028 | `	}` |
|       - | 5029 | `	/* Install the class */` |
|   21587 | 5030 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   21587 | 5031 | `	if( rc == SXRET_OK ){` |
|       - | 5032 | `		ph7_class **apInterface;` |
|       - | 5033 | `		sxu32 n;` |
|   21587 | 5034 | `		if( pBase ){` |
|       - | 5035 | `			/* Inherit from base class and mark as a subclass */` |
|    5375 | 5036 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    2687 | 5037 | `		}` |
|   21587 | 5038 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   21591 | 5039 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5040 | `			/* Implements one or more interface */` |
|       5 | 5041 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|       5 | 5042 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5043 | `				break;` |
|       - | 5044 | `			}` |
|       3 | 5045 | `		}` |
|   10793 | 5046 | `	}` |
|   21587 | 5047 | `	SySetRelease(&aInterfaces);` |
|   21587 | 5048 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5049 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5050 | `		return SXERR_ABORT;` |
|       - | 5051 | `	}` |
|   10793 | 5052 | `done:` |
|       - | 5053 | `	/* Point beyond the class body */` |
|   21591 | 5054 | `	pGen->pIn = &pEnd[1];` |
|   21591 | 5055 | `	pGen->pEnd = pTmp;` |
|   21591 | 5056 | `	return PH7_OK;` |
|   10796 | 5057 |  |
|       - | 5058 | `/*` |
|       - | 5059 | ` * Compile a user-defined abstract class.` |
|       - | 5060 | ` *  According to the PHP language reference manual` |
|       - | 5061 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5062 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5063 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5064 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5065 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5066 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5067 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5068 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5069 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5070 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5071 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5072 | ` *   could differ.` |
|       - | 5073 | ` */` |
|       4 | 5074 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       1 | 5075 |  |
|       - | 5076 | `	sxi32 rc;` |
|       5 | 5077 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|       5 | 5078 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|       5 | 5079 | `	return rc;` |
|       1 | 5080 |  |
|       - | 5081 | `/*` |
|       - | 5082 | ` * Compile a user-defined final class.` |
|       - | 5083 | ` *  According to the PHP language reference manual` |
|       - | 5084 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 5085 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 5086 | ` *    final then it cannot be extended.` |
|       - | 5087 | ` */` |
|       2 | 5088 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 5089 |  |
|       - | 5090 | `	sxi32 rc;` |
|       3 | 5091 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 5092 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 5093 | `	return rc;` |
|       1 | 5094 |  |
|       - | 5095 | `/*` |
|       - | 5096 | ` * Compile a user-defined class.` |
|       - | 5097 | ` *  According to the PHP language reference manual` |
|       - | 5098 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 5099 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 5100 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 5101 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 5102 | ` *   and functions (called "methods").` |
|       - | 5103 | ` */` |
|   21584 | 5104 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       1 | 5105 |  |
|       - | 5106 | `	sxi32 rc;` |
|   21585 | 5107 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   21585 | 5108 | `	return rc;` |
|       1 | 5109 |  |
|       - | 5110 | `/*` |
|       - | 5111 | ` * Exception handling.` |
|       - | 5112 | ` *  According to the PHP language reference manual` |
|       - | 5113 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 5114 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 5115 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 5116 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 5117 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 5118 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 5119 | ` *    (or re-thrown) within a catch block.` |
|       - | 5120 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 5121 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 5122 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 5123 | ` *    been defined with set_exception_handler().` |
|       - | 5124 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 5125 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 5126 | ` */` |
|       - | 5127 | `/*` |
|       - | 5128 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 5129 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 5130 | ` * indicates failure.` |
|       - | 5131 | ` */` |
|      22 | 5132 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       1 | 5133 |  |
|      23 | 5134 | `	sxi32 rc = SXRET_OK;` |
|      23 | 5135 | `	if( pRoot->pOp ){` |
|      20 | 5136 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|      11 | 5137 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 5138 | `			/* Unexpected expression */` |
|     ! 0 | 5139 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5140 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 5141 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 5142 | `				rc = SXERR_INVALID;` |
|     ! 0 | 5143 | `			}` |
|       1 | 5144 | `		}` |
|      13 | 5145 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 5146 | `		/* Unexpected expression */` |
|     ! 0 | 5147 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 5148 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 5149 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 5150 | `			rc = SXERR_INVALID;` |
|     ! 0 | 5151 | `		}` |
|     ! 0 | 5152 | `	}` |
|      23 | 5153 | `	return rc;` |
|       1 | 5154 |  |
|       - | 5155 | `/*` |
|       - | 5156 | ` * Compile a 'throw' statement.` |
|       - | 5157 | ` * throw: This is how you trigger an exception.` |
|       - | 5158 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 5159 | ` */` |
|      22 | 5160 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       1 | 5161 |  |
|      23 | 5162 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5163 | `	GenBlock *pBlock;` |
|       - | 5164 | `	sxu32 nIdx;` |
|       - | 5165 | `	sxi32 rc;` |
|      23 | 5166 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 5167 | `	/* Compile the expression */` |
|      23 | 5168 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      23 | 5169 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5170 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 5171 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5172 | `			return SXERR_ABORT;` |
|       - | 5173 | `		}` |
|     ! 0 | 5174 | `		return SXRET_OK;` |
|       - | 5175 | `	}` |
|      23 | 5176 | `	pBlock = pGen->pCurrent;` |
|       - | 5177 | `	/* Point to the top most function or try block and emit the forward jump */` |
|      43 | 5178 | `	while(pBlock->pParent){` |
|      37 | 5179 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      17 | 5180 | `			break;` |
|       - | 5181 | `		}` |
|       - | 5182 | `		/* Point to the parent block */` |
|      21 | 5183 | `		pBlock = pBlock->pParent;` |
|       1 | 5184 | `	}` |
|       - | 5185 | `	/* Emit the throw instruction */` |
|      23 | 5186 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 5187 | `	/* Emit the jump */` |
|      23 | 5188 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      23 | 5189 | `	return SXRET_OK;` |
|      12 | 5190 |  |
|       - | 5191 | `/*` |
|       - | 5192 | ` * Compile a 'catch' block.` |
|       - | 5193 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 5194 | ` * an object containing the exception information.` |
|       - | 5195 | ` */` |
|      30 | 5196 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       1 | 5197 |  |
|      31 | 5198 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5199 | `	ph7_exception_block sCatch;` |
|       - | 5200 | `	SySet *pInstrContainer;` |
|       - | 5201 | `	GenBlock *pCatch;` |
|       - | 5202 | `	SyToken *pToken;` |
|       - | 5203 | `	SyString *pName;` |
|       - | 5204 | `	char *zDup;` |
|       - | 5205 | `	sxi32 rc;` |
|      31 | 5206 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 5207 | `	/* Zero the structure */` |
|      31 | 5208 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 5209 | `	/* Initialize fields */` |
|      31 | 5210 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      45 | 5211 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      31 | 5212 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5213 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5214 | `			pToken = pGen->pIn;` |
|     ! 0 | 5215 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5216 | `				pToken--;` |
|     ! 0 | 5217 | `			}` |
|     ! 0 | 5218 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5219 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 5220 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5221 | `				return SXERR_ABORT;` |
|       - | 5222 | `			}` |
|     ! 0 | 5223 | `			return SXERR_INVALID;` |
|       - | 5224 | `	}` |
|       - | 5225 | `	/* Extract the exception class */` |
|      31 | 5226 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 5227 | `	/* Duplicate class name */` |
|      31 | 5228 | `	pName = &pGen->pIn->sData;` |
|      31 | 5229 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      31 | 5230 | `	if( zDup == 0 ){` |
|     ! 0 | 5231 | `		goto Mem;` |
|       - | 5232 | `	}` |
|      31 | 5233 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      31 | 5234 | `	pGen->pIn++;` |
|      45 | 5235 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      31 | 5236 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5237 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 5238 | `			pToken = pGen->pIn;` |
|     ! 0 | 5239 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5240 | `				pToken--;` |
|     ! 0 | 5241 | `			}` |
|     ! 0 | 5242 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5243 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 5244 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5245 | `				return SXERR_ABORT;` |
|       - | 5246 | `			}` |
|     ! 0 | 5247 | `			return SXERR_INVALID;` |
|       - | 5248 | `	}` |
|      31 | 5249 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 5250 | `	/* Duplicate instance name */` |
|      31 | 5251 | `	pName = &pGen->pIn->sData;` |
|      31 | 5252 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      31 | 5253 | `	if( zDup == 0 ){` |
|     ! 0 | 5254 | `		goto Mem;` |
|       - | 5255 | `	}` |
|      31 | 5256 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      31 | 5257 | `	pGen->pIn++;` |
|      31 | 5258 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 5259 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 5260 | `		pToken = pGen->pIn;` |
|     ! 0 | 5261 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 5262 | `			pToken--;` |
|     ! 0 | 5263 | `		}` |
|     ! 0 | 5264 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 5265 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 5266 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5267 | `			return SXERR_ABORT;` |
|       - | 5268 | `		}` |
|     ! 0 | 5269 | `		return SXERR_INVALID;` |
|       - | 5270 | `	}` |
|       - | 5271 | `	/* Compile the block */` |
|      31 | 5272 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 5273 | `	/* Create the catch block */` |
|      31 | 5274 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      31 | 5275 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5276 | `		return SXERR_ABORT;` |
|       - | 5277 | `	}` |
|       - | 5278 | `	/* Swap bytecode container */` |
|      31 | 5279 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      31 | 5280 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 5281 | `	/* Compile the block */` |
|      31 | 5282 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5283 | `	/* Fix forward jumps now the destination is resolved  */` |
|      31 | 5284 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5285 | `	/* Emit the DONE instruction */` |
|      31 | 5286 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5287 | `	/* Leave the block */` |
|      31 | 5288 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5289 | `	/* Restore the default container */` |
|      31 | 5290 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5291 | `	/* Install the catch block */` |
|      31 | 5292 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      31 | 5293 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5294 | `		goto Mem;` |
|       - | 5295 | `	}` |
|      31 | 5296 | `	return SXRET_OK;` |
|     ! 0 | 5297 | `Mem:` |
|     ! 0 | 5298 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5299 | `	return SXERR_ABORT;` |
|      16 | 5300 |  |
|       - | 5301 | `/*` |
|       - | 5302 | ` * Compile a 'try' block.` |
|       - | 5303 | ` * A function using an exception should be in a "try" block.` |
|       - | 5304 | ` * If the exception does not trigger, the code will continue` |
|       - | 5305 | ` * as normal. However if the exception triggers, an exception` |
|       - | 5306 | ` * is "thrown".` |
|       - | 5307 | ` */` |
|      32 | 5308 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       1 | 5309 |  |
|       - | 5310 | `	ph7_exception *pException;` |
|       - | 5311 | `	GenBlock *pTry;` |
|       - | 5312 | `	sxu32 nJmpIdx;` |
|       - | 5313 | `	sxi32 rc;` |
|       - | 5314 | `	/* Create the exception container */` |
|      33 | 5315 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      33 | 5316 | `	if( pException == 0 ){` |
|     ! 0 | 5317 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 5318 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5319 | `		return SXERR_ABORT;` |
|       - | 5320 | `	}` |
|       - | 5321 | `	/* Zero the structure */` |
|      33 | 5322 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 5323 | `	/* Initialize fields */` |
|      33 | 5324 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      33 | 5325 | `	pException->pVm = pGen->pVm;` |
|       - | 5326 | `	/* Create the try block */` |
|      33 | 5327 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      33 | 5328 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5329 | `		return SXERR_ABORT;` |
|       - | 5330 | `	}` |
|       - | 5331 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      33 | 5332 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 5333 | `	/* Fix the jump later when the destination is resolved */` |
|      33 | 5334 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      33 | 5335 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 5336 | `	/* Compile the block */` |
|      33 | 5337 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      33 | 5338 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5339 | `		return SXERR_ABORT;` |
|       - | 5340 | `	}` |
|       - | 5341 | `	/* Fix forward jumps now the destination is resolved */` |
|      33 | 5342 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5343 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      33 | 5344 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 5345 | `	/* Leave the block */` |
|      33 | 5346 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 5347 | `	/* Compile the catch block */` |
|      33 | 5348 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      30 | 5349 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       3 | 5350 | `			SyToken *pTok = pGen->pIn;` |
|       3 | 5351 | `			if( pTok >= pGen->pEnd ){` |
|       3 | 5352 | `				pTok--; /* Point back */` |
|       1 | 5353 | `			}` |
|       - | 5354 | `			/* Unexpected token */` |
|       4 | 5355 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,` |
|       1 | 5356 | `				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);` |
|       3 | 5357 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5358 | `				return SXERR_ABORT;` |
|       - | 5359 | `			}` |
|       3 | 5360 | `			return SXRET_OK;` |
|       - | 5361 | `	}` |
|       - | 5362 | `	/* Compile one or more catch blocks */` |
|      30 | 5363 | `	for(;;){` |
|      60 | 5364 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      42 | 5365 | `			\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       - | 5366 | `				/* No more blocks */` |
|      16 | 5367 | `				break;` |
|       - | 5368 | `		}` |
|       - | 5369 | `		/* Compile the catch block */` |
|      31 | 5370 | `		rc = PH7_CompileCatch(&(*pGen),pException);` |
|      31 | 5371 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5372 | `			return SXERR_ABORT;` |
|       - | 5373 | `		}` |
|       1 | 5374 | ` 	}` |
|      31 | 5375 | `	return SXRET_OK;` |
|      17 | 5376 |  |
|       - | 5377 | `/*` |
|       - | 5378 | ` * Compile a switch block.` |
|       - | 5379 | ` *  (See block-comment below for more information)` |
|       - | 5380 | ` */` |
|      58 | 5381 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       1 | 5382 |  |
|      59 | 5383 | `	sxi32 rc = SXRET_OK;` |
|      59 | 5384 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 5385 | `		/* Unexpected token */` |
|     ! 0 | 5386 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5387 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5388 | `			return SXERR_ABORT;` |
|       - | 5389 | `		}` |
|     ! 0 | 5390 | `		pGen->pIn++;` |
|     ! 0 | 5391 | `	}` |
|      59 | 5392 | `	pGen->pIn++;` |
|       - | 5393 | `	/* First instruction to execute in this block. */` |
|      59 | 5394 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 5395 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 5396 | `	 * or the '}' token */` |
|      79 | 5397 | `	for(;;){` |
|     159 | 5398 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5399 | `			/* No more input to process */` |
|     ! 0 | 5400 | `			break;` |
|       - | 5401 | `		}` |
|     159 | 5402 | `		rc = SXRET_OK;` |
|     159 | 5403 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      35 | 5404 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      17 | 5405 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 5406 | `					/* Unexpected token */` |
|     ! 0 | 5407 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5408 | `						&pGen->pIn->sData);` |
|     ! 0 | 5409 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5410 | `						return SXERR_ABORT;` |
|       - | 5411 | `					}` |
|       - | 5412 | `					/* FALL THROUGH */` |
|     ! 0 | 5413 | `				}` |
|      17 | 5414 | `				rc = SXERR_EOF;` |
|      17 | 5415 | `				break;` |
|       - | 5416 | `			}` |
|      10 | 5417 | `		}else{` |
|       - | 5418 | `			sxi32 nKwrd;` |
|       - | 5419 | `			/* Extract the keyword */` |
|     125 | 5420 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     125 | 5421 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      21 | 5422 | `				break;` |
|       - | 5423 | `			}` |
|      85 | 5424 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5425 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 5426 | `					/* Unexpected token */` |
|     ! 0 | 5427 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 5428 | `						&pGen->pIn->sData);` |
|     ! 0 | 5429 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5430 | `						return SXERR_ABORT;` |
|       - | 5431 | `					}` |
|       - | 5432 | `					/* FALL THROUGH */` |
|     ! 0 | 5433 | `				}` |
|       - | 5434 | `				/* Block compiled */` |
|       3 | 5435 | `				break;` |
|       - | 5436 | `			}` |
|       - | 5437 | `		}` |
|       - | 5438 | `		/* Compile block */` |
|     101 | 5439 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     101 | 5440 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5441 | `			return SXERR_ABORT;` |
|       - | 5442 | `		}` |
|       1 | 5443 | `	}` |
|      59 | 5444 | `	return rc;` |
|      30 | 5445 |  |
|       - | 5446 | `/*` |
|       - | 5447 | ` * Compile a case eXpression.` |
|       - | 5448 | ` *  (See block-comment below for more information)` |
|       - | 5449 | ` */` |
|      46 | 5450 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       1 | 5451 |  |
|       - | 5452 | `	SySet *pInstrContainer;` |
|       - | 5453 | `	SyToken *pEnd,*pTmp;` |
|      47 | 5454 | `	sxi32 iNest = 0;` |
|       - | 5455 | `	sxi32 rc;` |
|       - | 5456 | `	/* Delimit the expression */` |
|      47 | 5457 | `	pEnd = pGen->pIn;` |
|     101 | 5458 | `	while( pEnd < pGen->pEnd ){` |
|     101 | 5459 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 5460 | `			/* Increment nesting level */` |
|       3 | 5461 | `			iNest++;` |
|     100 | 5462 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 5463 | `			/* Decrement nesting level */` |
|       3 | 5464 | `			iNest--;` |
|      98 | 5465 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      47 | 5466 | `			break;` |
|       - | 5467 | `		}` |
|      55 | 5468 | `		pEnd++;` |
|       1 | 5469 | `	}` |
|      47 | 5470 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 5471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 5472 | `		if( rc == SXERR_ABORT ){` |
|       - | 5473 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5474 | `			return SXERR_ABORT;` |
|       - | 5475 | `		}` |
|     ! 0 | 5476 | `	}` |
|       - | 5477 | `	/* Swap token stream */` |
|      47 | 5478 | `	pTmp = pGen->pEnd;` |
|      47 | 5479 | `	pGen->pEnd = pEnd;` |
|      47 | 5480 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      47 | 5481 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      47 | 5482 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5483 | `	/* Emit the done instruction */` |
|      47 | 5484 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      47 | 5485 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5486 | `	/* Update token stream */` |
|      47 | 5487 | `	pGen->pIn  = pEnd;` |
|      47 | 5488 | `	pGen->pEnd = pTmp;` |
|      47 | 5489 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5490 | `		return SXERR_ABORT;` |
|       - | 5491 | `	}` |
|      47 | 5492 | `	return SXRET_OK;` |
|      24 | 5493 |  |
|       - | 5494 | `/*` |
|       - | 5495 | ` * Compile the smart switch statement.` |
|       - | 5496 | ` * According to the PHP language reference manual` |
|       - | 5497 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 5498 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 5499 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 5500 | ` *  This is exactly what the switch statement is for.` |
|       - | 5501 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 5502 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 5503 | ` *  of the outer loop, use continue 2.` |
|       - | 5504 | ` *  Note that switch/case does loose comparision.` |
|       - | 5505 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 5506 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 5507 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 5508 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 5509 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 5510 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 5511 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 5512 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 5513 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 5514 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 5515 | ` *  list for the next case.` |
|       - | 5516 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 5517 | ` *  or floating-point numbers and strings.` |
|       - | 5518 | ` */` |
|      18 | 5519 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       1 | 5520 |  |
|       - | 5521 | `	GenBlock *pSwitchBlock;` |
|       - | 5522 | `	SyToken *pTmp,*pEnd;` |
|       - | 5523 | `	ph7_switch *pSwitch;` |
|       - | 5524 | `	sxu32 nToken;` |
|       - | 5525 | `	sxu32 nLine;` |
|       - | 5526 | `	sxi32 rc;` |
|      19 | 5527 | `	nLine = pGen->pIn->nLine;` |
|       - | 5528 | `	/* Jump the 'switch' keyword */` |
|      19 | 5529 | `	pGen->pIn++;` |
|      19 | 5530 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5531 | `		/* Syntax error */` |
|     ! 0 | 5532 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 5533 | `		if( rc == SXERR_ABORT ){` |
|       - | 5534 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5535 | `			return SXERR_ABORT;` |
|       - | 5536 | `		}` |
|     ! 0 | 5537 | `		goto Synchronize;` |
|       - | 5538 | `	}` |
|       - | 5539 | `	/* Jump the left parenthesis '(' */` |
|      19 | 5540 | `	pGen->pIn++;` |
|      19 | 5541 | `	pEnd = 0; /* cc warning */` |
|       - | 5542 | `	/* Create the loop block */` |
|      28 | 5543 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|       9 | 5544 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      19 | 5545 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5546 | `		return SXERR_ABORT;` |
|       - | 5547 | `	}` |
|       - | 5548 | `	/* Delimit the condition */` |
|      19 | 5549 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      19 | 5550 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 5551 | `		/* Empty expression */` |
|     ! 0 | 5552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 5553 | `		if( rc == SXERR_ABORT ){` |
|       - | 5554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5555 | `			return SXERR_ABORT;` |
|       - | 5556 | `		}` |
|     ! 0 | 5557 | `	}` |
|       - | 5558 | `	/* Swap token streams */` |
|      19 | 5559 | `	pTmp = pGen->pEnd;` |
|      19 | 5560 | `	pGen->pEnd = pEnd;` |
|       - | 5561 | `	/* Compile the expression */` |
|      19 | 5562 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      19 | 5563 | `	if( rc == SXERR_ABORT ){` |
|       - | 5564 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 5565 | `		return SXERR_ABORT;` |
|       - | 5566 | `	}` |
|       - | 5567 | `	/* Update token stream */` |
|      19 | 5568 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 5569 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5570 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 5571 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5572 | `			return SXERR_ABORT;` |
|       - | 5573 | `		}` |
|     ! 0 | 5574 | `		pGen->pIn++;` |
|     ! 0 | 5575 | `	}` |
|      19 | 5576 | `	pGen->pIn  = &pEnd[1];` |
|      19 | 5577 | `	pGen->pEnd = pTmp;` |
|      19 | 5578 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      18 | 5579 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 5580 | `			pTmp = pGen->pIn;` |
|     ! 0 | 5581 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 5582 | `				pTmp--;` |
|     ! 0 | 5583 | `			}` |
|       - | 5584 | `			/* Unexpected token */` |
|     ! 0 | 5585 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 5586 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5587 | `				return SXERR_ABORT;` |
|       - | 5588 | `			}` |
|     ! 0 | 5589 | `			goto Synchronize;` |
|       - | 5590 | `	}` |
|       - | 5591 | `	/* Set the delimiter token */` |
|      19 | 5592 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 5593 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 5594 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 5595 | `	}else{` |
|      17 | 5596 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 5597 | `	}` |
|      19 | 5598 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 5599 | `	/* Create the switch blocks container */` |
|      19 | 5600 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      19 | 5601 | `	if( pSwitch == 0 ){` |
|       - | 5602 | `		/* Abort compilation */` |
|     ! 0 | 5603 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5604 | `		return SXERR_ABORT;` |
|       - | 5605 | `	}` |
|       - | 5606 | `	/* Zero the structure */` |
|      19 | 5607 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 5608 | `	/* Initialize fields */` |
|      19 | 5609 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 5610 | `	/* Emit the switch instruction */` |
|      19 | 5611 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 5612 | `	/* Compile case blocks */` |
|      51 | 5613 | `	for(;;){` |
|       - | 5614 | `		sxu32 nKwrd;` |
|      61 | 5615 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5616 | `			/* No more input to process */` |
|     ! 0 | 5617 | `			break;` |
|       - | 5618 | `		}` |
|      61 | 5619 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5620 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 5621 | `				/* Unexpected token */` |
|     ! 0 | 5622 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5623 | `					&pGen->pIn->sData);` |
|     ! 0 | 5624 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5625 | `					return SXERR_ABORT;` |
|       - | 5626 | `				}` |
|       - | 5627 | `				/* FALL THROUGH */` |
|     ! 0 | 5628 | `			}` |
|       - | 5629 | `			/* Block compiled */` |
|     ! 0 | 5630 | `			break;` |
|       - | 5631 | `		}` |
|       - | 5632 | `		/* Extract the keyword */` |
|      61 | 5633 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      61 | 5634 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 5635 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 5636 | `				/* Unexpected token */` |
|     ! 0 | 5637 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5638 | `					&pGen->pIn->sData);` |
|     ! 0 | 5639 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5640 | `					return SXERR_ABORT;` |
|       - | 5641 | `				}` |
|       - | 5642 | `				/* FALL THROUGH */` |
|     ! 0 | 5643 | `			}` |
|       - | 5644 | `			/* Block compiled */` |
|       3 | 5645 | `			break;` |
|       - | 5646 | `		}` |
|      59 | 5647 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 5648 | `			/*` |
|       - | 5649 | `			 * Accroding to the PHP language reference manual` |
|       - | 5650 | `			 *  A special case is the default case. This case matches anything` |
|       - | 5651 | `			 *  that wasn't matched by the other cases.` |
|       - | 5652 | `			 */` |
|      13 | 5653 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 5654 | `				/* Default case already compiled */` |
|     ! 0 | 5655 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 5656 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5657 | `					return SXERR_ABORT;` |
|       - | 5658 | `				}` |
|     ! 0 | 5659 | `			}` |
|      13 | 5660 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 5661 | `			/* Compile the default block */` |
|      13 | 5662 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      13 | 5663 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5664 | `				return SXERR_ABORT;` |
|      13 | 5665 | `			}else if( rc == SXERR_EOF ){` |
|      11 | 5666 | `				break;` |
|       1 | 5667 | `			}` |
|      48 | 5668 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 5669 | `			ph7_case_expr sCase;` |
|       - | 5670 | `			/* Standard case block */` |
|      47 | 5671 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 5672 | `			/* initialize the structure */` |
|      47 | 5673 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5674 | `			/* Compile the case expression */` |
|      47 | 5675 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      47 | 5676 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5677 | `				return SXERR_ABORT;` |
|       - | 5678 | `			}` |
|       - | 5679 | `			/* Compile the case block */` |
|      47 | 5680 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 5681 | `			/* Insert in the switch container */` |
|      47 | 5682 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      47 | 5683 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 5684 | `				return SXERR_ABORT;` |
|      47 | 5685 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 5686 | `				break;` |
|       - | 5687 | `			}` |
|      21 | 5688 | `		}else{` |
|       - | 5689 | `			/* Unexpected token */` |
|     ! 0 | 5690 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 5691 | `				&pGen->pIn->sData);` |
|     ! 0 | 5692 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5693 | `				return SXERR_ABORT;` |
|       - | 5694 | `			}` |
|     ! 0 | 5695 | `			break;` |
|       - | 5696 | `		}` |
|       1 | 5697 | `	}` |
|       - | 5698 | `	/* Fix all jumps now the destination is resolved */` |
|      19 | 5699 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      19 | 5700 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5701 | `	/* Release the loop block */` |
|      19 | 5702 | `	GenStateLeaveBlock(pGen,0);` |
|      19 | 5703 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 5704 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      19 | 5705 | `		pGen->pIn++;` |
|       9 | 5706 | `	}` |
|       - | 5707 | `	/* Statement successfully compiled */` |
|      19 | 5708 | `	return SXRET_OK;` |
|     ! 0 | 5709 | `Synchronize:` |
|       - | 5710 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5711 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 5712 | `		pGen->pIn++;` |
|     ! 0 | 5713 | `	}` |
|     ! 0 | 5714 | `	return SXRET_OK;` |
|      10 | 5715 |  |
|       - | 5716 | `/*` |
|       - | 5717 | ` * Generate bytecode for a given expression tree.` |
|       - | 5718 | ` * If something goes wrong while generating bytecode` |
|       - | 5719 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 5720 | ` * this function takes care of generating the appropriate` |
|       - | 5721 | ` * error message.` |
|       - | 5722 | ` */` |
| 3897478 | 5723 | `static sxi32 GenStateEmitExprCode(` |
|       - | 5724 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 5725 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 5726 | `	sxi32 iFlags /* Control flags */` |
|       - | 5727 | `	)` |
|       1 | 5728 |  |
|       - | 5729 | `	VmInstr *pInstr;` |
|       - | 5730 | `	sxu32 nJmpIdx;` |
| 3897479 | 5731 | `	sxi32 iP1 = 0;` |
| 3897479 | 5732 | `	sxu32 iP2 = 0;` |
| 3897479 | 5733 | `	void *p3  = 0;` |
|       - | 5734 | `	sxi32 iVmOp;` |
|       - | 5735 | `	sxi32 rc;` |
| 3897479 | 5736 | `	if( pNode->xCode ){` |
|       - | 5737 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 5738 | `		/* Compile node */` |
| 2388033 | 5739 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2388033 | 5740 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2388033 | 5741 | `		RE_SWAP_DELIMITER(pGen);` |
| 2388033 | 5742 | `		return rc;` |
|       - | 5743 | `	}` |
| 1509447 | 5744 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 5745 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 5746 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 5747 | `		return SXERR_ABORT;` |
|       - | 5748 | `	}` |
| 1509447 | 5749 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1509447 | 5750 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 5751 | `		sxu32 nJz,nJmp;` |
|       - | 5752 | `		/* Ternary operator require special handling */` |
|       - | 5753 | `		/* Phase#1: Compile the condition */` |
|    1543 | 5754 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1543 | 5755 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5756 | `			return rc;` |
|       - | 5757 | `		}` |
|    1543 | 5758 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|       - | 5759 | `		/* Phase#2: Emit the false jump */` |
|    1543 | 5760 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|    1543 | 5761 | `		if( pNode->pLeft ){` |
|       - | 5762 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1543 | 5763 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1543 | 5764 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5765 | `				return rc;` |
|       - | 5766 | `			}` |
|     771 | 5767 | `		}` |
|       - | 5768 | `		/* Phase#4: Emit the unconditional jump */` |
|    1543 | 5769 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 5770 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1543 | 5771 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1543 | 5772 | `		if( pInstr ){` |
|    1543 | 5773 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     771 | 5774 | `		}` |
|       - | 5775 | `		/* Phase#6: Compile the 'else' expression */` |
|    1543 | 5776 | `		if( pNode->pRight ){` |
|    1543 | 5777 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1543 | 5778 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5779 | `				return rc;` |
|       - | 5780 | `			}` |
|     771 | 5781 | `		}` |
|    1543 | 5782 | `		if( nJmp > 0 ){` |
|       - | 5783 | `			/* Phase#7: Fix the unconditional jump */` |
|    1543 | 5784 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1543 | 5785 | `			if( pInstr ){` |
|    1543 | 5786 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     771 | 5787 | `			}` |
|     771 | 5788 | `		}` |
|       - | 5789 | `		/* All done */` |
|    1543 | 5790 | `		return SXRET_OK;` |
|       - | 5791 | `	}` |
|       - | 5792 | `	/* Generate code for the left tree */` |
| 1507905 | 5793 | `	if( pNode->pLeft ){` |
| 1507905 | 5794 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 5795 | `			ph7_expr_node **apNode;` |
|       - | 5796 | `			sxi32 n;` |
|       - | 5797 | `			/* Recurse and generate bytecodes for function arguments */` |
|  425305 | 5798 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 5799 | `			/* Read-only load */` |
|  425305 | 5800 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  815063 | 5801 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  389759 | 5802 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  389759 | 5803 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5804 | `					return rc;` |
|       - | 5805 | `				}` |
|  194880 | 5806 | `			}` |
|       - | 5807 | `			/* Total number of given arguments */` |
|  425305 | 5808 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 5809 | `			/* Remove stale flags now */` |
|  425305 | 5810 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  212652 | 5811 | `		}` |
| 1507905 | 5812 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1507905 | 5813 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5814 | `			return rc;` |
|       - | 5815 | `		}` |
| 1507905 | 5816 | `		if( iVmOp == PH7_OP_CALL ){` |
|  425305 | 5817 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  425305 | 5818 | `			if( pInstr ){` |
|  425305 | 5819 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|       - | 5820 | `					/* Prevent constant expansion */` |
|  425139 | 5821 | `					pInstr->iP1 = 0;` |
|  212736 | 5822 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 5823 | `					/* Method call,flag that */` |
|     153 | 5824 | `					pInstr->iP2 = 1;` |
|      76 | 5825 | `				}` |
|  212653 | 5826 | `			}` |
| 1295253 | 5827 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 5828 | `			ph7_expr_node **apNode;` |
|       - | 5829 | `			sxi32 n;` |
|       - | 5830 | `			/* Recurse and generate bytecodes for array index */` |
|  123475 | 5831 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  220247 | 5832 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   96773 | 5833 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   96773 | 5834 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5835 | `					return rc;` |
|       - | 5836 | `				}` |
|   48387 | 5837 | `			}` |
|  123475 | 5838 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   96773 | 5839 | `				iP1 = 1; /* Node have an index associated with it */` |
|   48386 | 5840 | `			}` |
|  123475 | 5841 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 5842 | `				/* Create an empty entry when the desired index is not found */` |
|   37603 | 5843 | `				iP2 = 1;` |
|   18802 | 5844 | `			}` |
| 1020864 | 5845 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 5846 | `			/* POP the left node */` |
|      31 | 5847 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 5848 | `		}` |
|  753952 | 5849 | `	}` |
| 1507905 | 5850 | `	rc = SXRET_OK;` |
| 1507905 | 5851 | `	nJmpIdx = 0;` |
|       - | 5852 | `	/* Generate code for the right tree */` |
| 1507905 | 5853 | `	if( pNode->pRight ){` |
|  856447 | 5854 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 5855 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   10959 | 5856 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  850968 | 5857 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 5858 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    5439 | 5859 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  842770 | 5860 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  378825 | 5861 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  189412 | 5862 | `		}` |
|  856447 | 5863 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  856447 | 5864 | `		if( iVmOp == PH7_OP_STORE ){` |
|  373383 | 5865 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  373383 | 5866 | `			if( pInstr ){` |
|  373383 | 5867 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 5868 | `					/* Hide the STORE instruction */` |
|      25 | 5869 | `					iVmOp = 0;` |
|  373371 | 5870 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 5871 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   90741 | 5872 | `					iP2 = 1;` |
|   45371 | 5873 | `				}else{` |
|  282619 | 5874 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 5875 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   37601 | 5876 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   37601 | 5877 | `						iP1 = pInstr->iP1;` |
|   18801 | 5878 | `					}else{` |
|  245019 | 5879 | `						p3 = pInstr->p3;` |
|       - | 5880 | `					}` |
|       - | 5881 | `					/* POP the last dynamic load instruction */` |
|  282619 | 5882 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 5883 | `				}` |
|  186692 | 5884 | `			}` |
|  669756 | 5885 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      45 | 5886 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      45 | 5887 | `			if( pInstr ){` |
|      45 | 5888 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 5889 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 5890 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 5891 | `					 */` |
|      13 | 5892 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      13 | 5893 | `					iP1 = pInstr->iP1;` |
|      13 | 5894 | `					iP2 = pInstr->iP2;` |
|      13 | 5895 | `					p3  = pInstr->p3;` |
|       7 | 5896 | `				}else{` |
|      33 | 5897 | `					p3 = pInstr->p3;` |
|       - | 5898 | `				}` |
|      22 | 5899 | `			}` |
|      22 | 5900 | `		}` |
|  428223 | 5901 | `	}` |
| 1507905 | 5902 | `	if( iVmOp > 0 ){` |
| 1507851 | 5903 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   21469 | 5904 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 5905 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   16009 | 5906 | `				iP1 = 1;` |
|    8005 | 5907 | `			}` |
| 1497117 | 5908 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|   10903 | 5909 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   10903 | 5910 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 5911 | `				VmInstr *pPrev;` |
|   10895 | 5912 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   10895 | 5913 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 5914 | `					/* Pop the call instruction */` |
|   10895 | 5915 | `					iP1 = pInstr->iP1;` |
|   10895 | 5916 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    5447 | 5917 | `				}` |
|    5448 | 5918 | `			}` |
| 1480932 | 5919 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|  203099 | 5920 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 5921 | `				/* Static member access,remember that */` |
|      53 | 5922 | `				iP1 = 1;` |
|      53 | 5923 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      53 | 5924 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       3 | 5925 | `					p3 = pInstr->p3;` |
|       3 | 5926 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       1 | 5927 | `				}` |
|      26 | 5928 | `			}` |
|  101549 | 5929 | `		}` |
|       - | 5930 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1507851 | 5931 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
| 1507851 | 5932 | `		if( nJmpIdx > 0 ){` |
|       - | 5933 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   16397 | 5934 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   16397 | 5935 | `			if( pInstr ){` |
|   16397 | 5936 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    8198 | 5937 | `			}` |
|    8198 | 5938 | `		}` |
|  753925 | 5939 | `	}` |
| 1507905 | 5940 | `	return rc;` |
| 1948740 | 5941 |  |
|       - | 5942 | `/*` |
|       - | 5943 | ` * Compile a PHP expression.` |
|       - | 5944 | ` * According to the PHP language reference manual:` |
|       - | 5945 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 5946 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 5947 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 5948 | ` *  is "anything that has a value".` |
|       - | 5949 | ` * If something goes wrong while compiling the expression,this` |
|       - | 5950 | ` * function takes care of generating the appropriate error` |
|       - | 5951 | ` * message.` |
|       - | 5952 | ` */` |
| 1042146 | 5953 | `static sxi32 PH7_CompileExpr(` |
|       - | 5954 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5955 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 5956 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 5957 | `	)` |
|       1 | 5958 |  |
|       - | 5959 | `	ph7_expr_node *pRoot;` |
|       - | 5960 | `	SySet sExprNode;` |
|       - | 5961 | `	SyToken *pEnd;` |
|       - | 5962 | `	sxi32 nExpr;` |
|       - | 5963 | `	sxi32 iNest;` |
|       - | 5964 | `	sxi32 rc;` |
|       - | 5965 | `	/* Initialize worker variables */` |
| 1042147 | 5966 | `	nExpr = 0;` |
| 1042147 | 5967 | `	pRoot = 0;` |
| 1042147 | 5968 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
| 1042147 | 5969 | `	SySetAlloc(&sExprNode,0x10);` |
| 1042147 | 5970 | `	rc = SXRET_OK;` |
|       - | 5971 | `	/* Delimit the expression */` |
| 1042147 | 5972 | `	pEnd = pGen->pIn;` |
| 1042147 | 5973 | `	iNest = 0;` |
| 6921891 | 5974 | `	while( pEnd < pGen->pEnd ){` |
| 6570575 | 5975 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 5976 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     113 | 5977 | `			iNest++;` |
| 6570519 | 5978 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     121 | 5979 | `			iNest--;` |
| 6570403 | 5980 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  690909 | 5981 | `			if( iNest <= 0 ){` |
|  690831 | 5982 | `				break;` |
|       - | 5983 | `			}` |
|      39 | 5984 | `		}` |
| 5879745 | 5985 | `		pEnd++;` |
|       1 | 5986 | `	}` |
| 1042147 | 5987 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21493 | 5988 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21493 | 5989 | `		iNest = 0;` |
|       - | 5990 | `		/* Stop at the first comma */` |
|   43005 | 5991 | `		while( pEnd2 < pEnd ){` |
|   21513 | 5992 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       3 | 5993 | `				iNest++;` |
|   21512 | 5994 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       3 | 5995 | `				iNest--;` |
|   21510 | 5996 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 5997 | `				if( iNest <= 0 ){` |
|     ! 0 | 5998 | `					break;` |
|       - | 5999 | `				}` |
|       2 | 6000 | `			}` |
|   21513 | 6001 | `			pEnd2++;` |
|       1 | 6002 | `		}` |
|   21493 | 6003 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 6004 | `			pEnd = pEnd2;` |
|     ! 0 | 6005 | `		}` |
|   10746 | 6006 | `	}` |
| 1042147 | 6007 | `	if( pEnd > pGen->pIn ){` |
| 1042139 | 6008 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 6009 | `		/* Swap delimiter */` |
| 1042139 | 6010 | `		pGen->pEnd = pEnd;` |
|       - | 6011 | `		/* Try to get an expression tree */` |
| 1042139 | 6012 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
| 1042139 | 6013 | `		if( rc == SXRET_OK && pRoot ){` |
| 1041973 | 6014 | `			rc = SXRET_OK;` |
| 1041973 | 6015 | `			if( xTreeValidator ){` |
|       - | 6016 | `				/* Call the upper layer validator callback */` |
|   10911 | 6017 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    5455 | 6018 | `			}` |
| 1041973 | 6019 | `			if( rc != SXERR_ABORT ){` |
|       - | 6020 | `				/* Generate code for the given tree */` |
| 1041973 | 6021 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  520986 | 6022 | `			}` |
| 1041973 | 6023 | `			nExpr = 1;` |
|  520986 | 6024 | `		}` |
|       - | 6025 | `		/* Release the whole tree */` |
| 1042139 | 6026 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 6027 | `		/* Synchronize token stream */` |
| 1042139 | 6028 | `		pGen->pEnd = pTmp;` |
| 1042139 | 6029 | `		pGen->pIn  = pEnd;` |
| 1042139 | 6030 | `		if( rc == SXERR_ABORT ){` |
|       3 | 6031 | `			SySetRelease(&sExprNode);` |
|       3 | 6032 | `			return SXERR_ABORT;` |
|       - | 6033 | `		}` |
|  521068 | 6034 | `	}` |
| 1042145 | 6035 | `	SySetRelease(&sExprNode);` |
| 1042145 | 6036 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  521074 | 6037 |  |
|       - | 6038 | `/*` |
|       - | 6039 | ` * Return a pointer to the node construct handler associated` |
|       - | 6040 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 6041 | ` */` |
|  253062 | 6042 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       1 | 6043 |  |
|  253063 | 6044 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 6045 | `		/* Numeric literal: Either real or integer */` |
|  169921 | 6046 | `		return PH7_CompileNumLiteral;` |
|   83143 | 6047 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 6048 | `		/* Double quoted string */` |
|   12069 | 6049 | `		return PH7_CompileString;` |
|   71075 | 6050 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 6051 | `		/* Single quoted string */` |
|   71011 | 6052 | `		return PH7_CompileSimpleString;` |
|      65 | 6053 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 6054 | `		/* Heredoc */` |
|      31 | 6055 | `		return PH7_CompileHereDoc;` |
|      35 | 6056 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 6057 | `		/* Nowdoc */` |
|      29 | 6058 | `		return PH7_CompileNowDoc;` |
|       7 | 6059 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 6060 | `		/* Backtick quoted string */` |
|       5 | 6061 | `		return PH7_CompileBacktic;` |
|       - | 6062 | `	}` |
|       3 | 6063 | `	return 0;` |
|  126532 | 6064 |  |
|       - | 6065 | `/*` |
|       - | 6066 | ` * PHP Language construct table.` |
|       - | 6067 | ` */` |
|       - | 6068 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 6069 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 6070 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 6071 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 6072 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 6073 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 6074 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 6075 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 6076 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 6077 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 6078 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 6079 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 6080 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 6081 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 6082 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 6083 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 6084 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 6085 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 6086 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 6087 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 6088 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 6089 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 6090 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 6091 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 6092 | `};` |
|       - | 6093 | `/*` |
|       - | 6094 | ` * Return a pointer to the statement handler routine associated` |
|       - | 6095 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 6096 | ` */` |
|  605400 | 6097 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 6098 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 6099 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 6100 | `	)` |
|       1 | 6101 |  |
|  605401 | 6102 | `	sxu32 n = 0;` |
| 2034118 | 6103 | `	for(;;){` |
| 4068237 | 6104 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   38195 | 6105 | `			break;` |
|       - | 6106 | `		}` |
| 4030043 | 6107 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  567207 | 6108 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 6109 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 6110 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 6111 | `					/* 'static' (class context),return null */` |
|     ! 0 | 6112 | `					return 0;` |
|       - | 6113 | `				}` |
|     ! 0 | 6114 | `			}` |
|       - | 6115 | `			/* Return a pointer to the handler.` |
|       - | 6116 | `			*/` |
|  567207 | 6117 | `			return aLangConstruct[n].xConstruct;` |
|       - | 6118 | `		}` |
| 3462837 | 6119 | `		n++;` |
|       1 | 6120 | `	}` |
|   38195 | 6121 | `	if( pLookahed ){` |
|   38195 | 6122 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   16013 | 6123 | `			return PH7_CompileClassInterface;` |
|   22183 | 6124 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   21585 | 6125 | `			return PH7_CompileClass;` |
|     598 | 6126 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       6 | 6127 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       5 | 6128 | `				return PH7_CompileAbstractClass;` |
|     594 | 6129 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       4 | 6130 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 6131 | `				return PH7_CompileFinalClass;` |
|       - | 6132 | `		}` |
|     296 | 6133 | `	}` |
|       - | 6134 | `	/* Not a language construct */` |
|     593 | 6135 | `	return 0;` |
|  302701 | 6136 |  |
|       - | 6137 | `/*` |
|       - | 6138 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 6139 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 6140 | ` */` |
|     592 | 6141 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       1 | 6142 |  |
|       - | 6143 | `	int rc;` |
|     593 | 6144 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     593 | 6145 | `	if( rc == FALSE ){` |
|       9 | 6146 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 6147 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 6148 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 6149 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 6150 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 6151 | `			*/` |
|       - | 6152 | `			){` |
|       3 | 6153 | `				rc = TRUE;` |
|       1 | 6154 | `		}` |
|       4 | 6155 | `	}` |
|     593 | 6156 | `	return rc;` |
|       1 | 6157 |  |
|       - | 6158 | `/*` |
|       - | 6159 | ` * Compile a PHP chunk.` |
|       - | 6160 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6161 | ` * takes care of generating the appropriate error message.` |
|       - | 6162 | ` */` |
|  890442 | 6163 | `static sxi32 GenStateCompileChunk(` |
|       - | 6164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6165 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 6166 | `	)` |
|       1 | 6167 |  |
|       - | 6168 | `	ProcLangConstruct xCons;` |
|       - | 6169 | `	sxi32 rc;` |
|  890443 | 6170 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  506235 | 6171 | `	for(;;){` |
| 1012471 | 6172 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6173 | `			/* No more input to process */` |
|   10695 | 6174 | `			break;` |
|       - | 6175 | `		}` |
| 1001777 | 6176 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 6177 | `			/* Compile block */` |
|      11 | 6178 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      11 | 6179 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6180 | `				break;` |
|       - | 6181 | `			}` |
|       6 | 6182 | `		}else{` |
| 1001767 | 6183 | `			xCons = 0;` |
| 1001767 | 6184 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  605401 | 6185 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 6186 | `				/* Try to extract a language construct handler */` |
|  605401 | 6187 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  605401 | 6188 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      10 | 6189 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6190 | `						"Syntax error: Unexpected keyword '%z'",` |
|       6 | 6191 | `						&pGen->pIn->sData);` |
|       7 | 6192 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6193 | `						break;` |
|       - | 6194 | `					}` |
|       - | 6195 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 6196 | `					 * this erroneous statement.` |
|       - | 6197 | `					 */` |
|       7 | 6198 | `					xCons = PH7_ErrorRecover;` |
|       3 | 6199 | `				}` |
|  699067 | 6200 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   59983 | 6201 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 6202 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     113 | 6203 | `				xCons = PH7_CompileLabel;` |
|      56 | 6204 | `			}` |
| 1001767 | 6205 | `			if( xCons == 0 ){` |
|       - | 6206 | `				/* Assume an expression an try to compile it */` |
|  396841 | 6207 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  396841 | 6208 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 6209 | `					/* Pop l-value */` |
|  396711 | 6210 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  198355 | 6211 | `				}` |
|  198421 | 6212 | `			}else{` |
|       - | 6213 | `				/* Go compile the sucker */` |
|  604927 | 6214 | `				rc = xCons(&(*pGen));` |
|       - | 6215 | `			}` |
| 1001767 | 6216 | `			if( rc == SXERR_ABORT ){` |
|       - | 6217 | `				/* Request to abort compilation */` |
|       3 | 6218 | `				break;` |
|       - | 6219 | `			}` |
|       - | 6220 | `		}` |
|       - | 6221 | `		/* Ignore trailing semi-colons ';' */` |
| 1647651 | 6222 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  645877 | 6223 | `			pGen->pIn++;` |
|       1 | 6224 | `		}` |
| 1001775 | 6225 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 6226 | `			/* Compile a single statement and return */` |
|  879747 | 6227 | `			break;` |
|       - | 6228 | `		}` |
|       - | 6229 | `		/* LOOP ONE */` |
|       - | 6230 | `		/* LOOP TWO */` |
|       - | 6231 | `		/* LOOP THREE */` |
|       - | 6232 | `		/* LOOP FOUR */` |
|       1 | 6233 | `	}` |
|       - | 6234 | `	/* Return compilation status */` |
|  890443 | 6235 | `	return rc;` |
|       1 | 6236 |  |
|       - | 6237 | `/*` |
|       - | 6238 | ` * Compile a Raw PHP chunk.` |
|       - | 6239 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 6240 | ` * takes care of generating the appropriate error message.` |
|       - | 6241 | ` */` |
|   10702 | 6242 | `static sxi32 PH7_CompilePHP(` |
|       - | 6243 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6244 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 6245 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 6246 | `	)` |
|       1 | 6247 |  |
|   10703 | 6248 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 6249 | `	sxi32 rc;` |
|       - | 6250 | `	/* Reset the token set */` |
|   10703 | 6251 | `	SySetReset(&(*pTokenSet));` |
|       - | 6252 | `	/* Mark as the default token set */` |
|   10703 | 6253 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 6254 | `	/* Advance the stream cursor */` |
|   10703 | 6255 | `	pGen->pRawIn++;` |
|       - | 6256 | `	/* Tokenize the PHP chunk first */` |
|   10703 | 6257 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 6258 | `	/* Point to the head and tail of the token stream. */` |
|   10703 | 6259 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10703 | 6260 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10703 | 6261 | `	if( is_expr ){` |
|       5 | 6262 | `		rc = SXERR_EMPTY;` |
|       5 | 6263 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 6264 | `			/* A simple expression,compile it */` |
|       5 | 6265 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       2 | 6266 | `		}` |
|       - | 6267 | `		/* Emit the DONE instruction */` |
|       5 | 6268 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       5 | 6269 | `		return SXRET_OK;` |
|       - | 6270 | `	}` |
|   10699 | 6271 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 6272 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 6273 | `		/*` |
|       - | 6274 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 6275 | `		 * According to the PHP reference manual:` |
|       - | 6276 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 6277 | `		 *  immediately follow` |
|       - | 6278 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 6279 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 6280 | `		 * Symisc extension:` |
|       - | 6281 | `		 *   This short syntax works with all PHP opening` |
|       - | 6282 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 6283 | `		 *   only short tag.` |
|       - | 6284 | `		 */` |
|       - | 6285 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 6286 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 6287 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 6288 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 6289 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 6290 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 6291 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 6292 | `		}` |
|       3 | 6293 | `		return SXRET_OK;` |
|       - | 6294 | `	}` |
|       - | 6295 | `	/* Compile the PHP chunk */` |
|   10697 | 6296 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 6297 | `	/* Fix exceptions jumps */` |
|   10697 | 6298 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6299 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10697 | 6300 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 6301 | `		rc = SXERR_ABORT;` |
|       1 | 6302 | `	}` |
|       - | 6303 | `	/* Reset container */` |
|   10697 | 6304 | `	SySetReset(&pGen->aGoto);` |
|   10697 | 6305 | `	SySetReset(&pGen->aLabel);` |
|       - | 6306 | `	/* Compilation result */` |
|   10697 | 6307 | `	return rc;` |
|    5352 | 6308 |  |
|       - | 6309 | `/*` |
|       - | 6310 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 6311 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 6312 | ` * This is the only compile interface exported from this file.` |
|       - | 6313 | ` */` |
|   10700 | 6314 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 6315 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 6316 | `	SyString *pScript,  /* Script to compile */` |
|       - | 6317 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 6318 | `	)` |
|       1 | 6319 |  |
|       - | 6320 | `	SySet aPhpToken,aRawToken;` |
|       - | 6321 | `	ph7_gen_state *pCodeGen;` |
|       - | 6322 | `	ph7_value *pRawObj;` |
|       - | 6323 | `	sxu32 nObjIdx;` |
|       - | 6324 | `	sxi32 nRawObj;` |
|       - | 6325 | `	int is_expr;` |
|       - | 6326 | `	sxi32 rc;` |
|   10701 | 6327 | `	if( pScript->nByte < 1 ){` |
|       - | 6328 | `		/* Nothing to compile */` |
|     ! 0 | 6329 | `		return PH7_OK;` |
|       - | 6330 | `	}` |
|       - | 6331 | `	/* Initialize the tokens containers */` |
|   10701 | 6332 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   10701 | 6333 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   10701 | 6334 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   10701 | 6335 | `	is_expr = 0;` |
|   10701 | 6336 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 6337 | `		SyToken sTmp;` |
|       - | 6338 | `		/* PHP only: -*/` |
|    5361 | 6339 | `		sTmp.nLine = 1;` |
|    5361 | 6340 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    5361 | 6341 | `		sTmp.pUserData = 0;` |
|    5361 | 6342 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    5361 | 6343 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    5361 | 6344 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 6345 | `			/* A simple PHP expression */` |
|       5 | 6346 | `			is_expr = 1;` |
|       2 | 6347 | `		}` |
|    2681 | 6348 | `	}else{` |
|       - | 6349 | `		/* Tokenize raw text */` |
|    5341 | 6350 | `		SySetAlloc(&aRawToken,32);` |
|    5341 | 6351 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 6352 | `	}` |
|   10701 | 6353 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 6354 | `	/* Process high-level tokens */` |
|   10701 | 6355 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   10701 | 6356 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   10701 | 6357 | `	rc = PH7_OK;` |
|   10701 | 6358 | `	if( is_expr ){` |
|       - | 6359 | `		/* Compile the expression */` |
|       5 | 6360 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       5 | 6361 | `		goto cleanup;` |
|       - | 6362 | `	}` |
|   10697 | 6363 | `	nObjIdx = 0;` |
|       - | 6364 | `	/* Start the compilation process */` |
|    8265 | 6365 | `	for(;;){` |
|   27225 | 6366 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   10693 | 6367 | `			break; /* No more tokens to process */` |
|       - | 6368 | `		}` |
|   16533 | 6369 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 6370 | `			/* Compile the PHP chunk */` |
|   10699 | 6371 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10699 | 6372 | `			if( rc == SXERR_ABORT ){` |
|       5 | 6373 | `				break;` |
|       - | 6374 | `			}` |
|   10695 | 6375 | `			continue;` |
|       - | 6376 | `		}` |
|       - | 6377 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    5835 | 6378 | `		nRawObj = 0;` |
|   11669 | 6379 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 6380 | `			/* Consume the raw chunk without any processing */` |
|    5835 | 6381 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    5835 | 6382 | `			if( pRawObj == 0 ){` |
|     ! 0 | 6383 | `				rc = SXERR_MEM;` |
|     ! 0 | 6384 | `				break;` |
|       - | 6385 | `			}` |
|       - | 6386 | `			/* Mark as constant and emit the load constant instruction */` |
|    5835 | 6387 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    5835 | 6388 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    5835 | 6389 | `			++nRawObj;` |
|    5835 | 6390 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       1 | 6391 | `		}` |
|    5835 | 6392 | `		if( nRawObj > 0 ){` |
|       - | 6393 | `			/* Emit the consume instruction */` |
|    5835 | 6394 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    2917 | 6395 | `		}` |
|    5349 | 6396 | `	}` |
|    5350 | 6397 | `cleanup:` |
|   10701 | 6398 | `	SySetRelease(&aRawToken);` |
|   10701 | 6399 | `	SySetRelease(&aPhpToken);` |
|   10701 | 6400 | `	return rc;` |
|    5351 | 6401 |  |
|       - | 6402 | `/*` |
|       - | 6403 | ` * Utility routines.Initialize the code generator.` |
|       - | 6404 | ` */` |
|    5334 | 6405 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 6406 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6407 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6408 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6409 | `	)` |
|       1 | 6410 |  |
|    5335 | 6411 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6412 | `	/* Zero the structure */` |
|    5335 | 6413 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 6414 | `	/* Initial state */` |
|    5335 | 6415 | `	pGen->pVm  = &(*pVm);` |
|    5335 | 6416 | `	pGen->xErr = xErr;` |
|    5335 | 6417 | `	pGen->pErrData = pErrData;` |
|    5335 | 6418 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    5335 | 6419 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    5335 | 6420 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    5335 | 6421 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 6422 | `	/* Error log buffer */` |
|    5335 | 6423 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 6424 | `	/* General purpose working buffer */` |
|    5335 | 6425 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 6426 | `	/* Create the global scope */` |
|    5335 | 6427 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 6428 | `	/* Point to the global scope */` |
|    5335 | 6429 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    5335 | 6430 | `	return SXRET_OK;` |
|       1 | 6431 |  |
|       - | 6432 | `/*` |
|       - | 6433 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 6434 | ` */` |
|   15792 | 6435 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 6436 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 6437 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 6438 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 6439 | `	)` |
|       1 | 6440 |  |
|   15793 | 6441 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 6442 | `	GenBlock *pBlock,*pParent;` |
|       - | 6443 | `	/* Reset state */` |
|   15793 | 6444 | `	SySetReset(&pGen->aLabel);` |
|   15793 | 6445 | `	SySetReset(&pGen->aGoto);` |
|   15793 | 6446 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15793 | 6447 | `	SyBlobRelease(&pGen->sWorker);` |
|       - | 6448 | `	/* Point to the global scope */` |
|   15793 | 6449 | `	pBlock = pGen->pCurrent;` |
|   15793 | 6450 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 6451 | `		pParent = pBlock->pParent;` |
|     ! 0 | 6452 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 6453 | `		pBlock = pParent;` |
|     ! 0 | 6454 | `	}` |
|   15793 | 6455 | `	pGen->xErr = xErr;` |
|   15793 | 6456 | `	pGen->pErrData = pErrData;` |
|   15793 | 6457 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15793 | 6458 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15793 | 6459 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15793 | 6460 | `	pGen->nErr = 0;` |
|   15793 | 6461 | `	return SXRET_OK;` |
|       1 | 6462 |  |
|       - | 6463 | `/*` |
|       - | 6464 | ` * Generate a compile-time error message.` |
|       - | 6465 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 6466 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 6467 | ` * abort compilation immediately.` |
|       - | 6468 | ` */` |
|     452 | 6469 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       1 | 6470 |  |
|     453 | 6471 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     453 | 6472 | `	const char *zErr = "Error";` |
|       - | 6473 | `	SyString *pFile;` |
|       - | 6474 | `	va_list ap;` |
|       - | 6475 | `	sxi32 rc;` |
|       - | 6476 | `	/* Reset the working buffer */` |
|     453 | 6477 | `	SyBlobReset(pWorker);` |
|       - | 6478 | `	/* Peek the processed file path if available */` |
|     453 | 6479 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     453 | 6480 | `	if( pFile && pGen->xErr ){` |
|       - | 6481 | `		/* Append file name */` |
|     451 | 6482 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|     451 | 6483 | `		SyBlobAppend(pWorker,(const void *)": ",sizeof(": ")-1);` |
|     225 | 6484 | `	}` |
|     453 | 6485 | `	if( nErrType == E_ERROR ){` |
|       - | 6486 | `		/* Increment the error counter */` |
|     397 | 6487 | `		pGen->nErr++;` |
|     397 | 6488 | `		if( pGen->nErr > 15 ){` |
|       - | 6489 | `			/* Error count limit reached */` |
|       5 | 6490 | `			if( pGen->xErr ){` |
|       5 | 6491 | `				SyBlobFormat(pWorker,"%u Error count limit reached,PH7 is aborting compilation\n",nLine);` |
|       5 | 6492 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6493 | `					/* Consume the generated error message */` |
|       5 | 6494 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 6495 | `				}` |
|       2 | 6496 | `			}` |
|       - | 6497 | `			/* Abort immediately */` |
|       5 | 6498 | `			return SXERR_ABORT;` |
|       - | 6499 | `		}` |
|     196 | 6500 | `	}` |
|     449 | 6501 | `	if( pGen->xErr == 0 ){` |
|       - | 6502 | `		/* No available error consumer,return immediately */` |
|       3 | 6503 | `		return SXRET_OK;` |
|       - | 6504 | `	}` |
|     447 | 6505 | `	switch(nErrType){` |
|      41 | 6506 | `	case E_WARNING: zErr = "Warning";     break;` |
|     ! 0 | 6507 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      17 | 6508 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 6509 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 6510 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 6511 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     195 | 6512 | `	default:` |
|     390 | 6513 | `		break;` |
|       - | 6514 | `	}` |
|     447 | 6515 | `	rc = SXRET_OK;` |
|       - | 6516 | `	/* Format the error message */` |
|     447 | 6517 | `	SyBlobFormat(pWorker,"%u %s: ",nLine,zErr);` |
|     447 | 6518 | `	va_start(ap,zFormat);` |
|     447 | 6519 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 6520 | `	va_end(ap);` |
|       - | 6521 | `	/* Append a new line */` |
|     447 | 6522 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 6523 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 6524 | `		/* Consume the generated error message */` |
|     447 | 6525 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 6526 | `	}` |
|     447 | 6527 | `	return rc;` |
|     227 | 6528 |  |
|       - | 6529 |  |
