# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3375/4462 lines (75.64%)

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
|    2746 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2748 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    7720 |  131 | `	for(;;){` |
|   15442 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2636 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2636 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2614 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   12830 |  140 | `		pBlock = pBlock->pParent;` |
|   12830 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1375 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  439308 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  439310 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  439310 |  162 | `	pBlock->pUserData   = pUserData;` |
|  439310 |  163 | `	pBlock->pGen        = pGen;` |
|  439310 |  164 | `	pBlock->iFlags      = iType;` |
|  439310 |  165 | `	pBlock->pParent     = 0;` |
|  439310 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  439310 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  439310 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  436802 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  436804 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  436804 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  436804 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  436804 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  436804 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  436804 |  200 | `	pGen->pCurrent = pBlock;` |
|  436804 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  209812 |  203 | `		*ppBlock = pBlock;` |
|  104905 |  204 | `	}` |
|  436804 |  205 | `	return SXRET_OK;` |
|  218403 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  436794 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  436796 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  436796 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  436796 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  436794 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  436796 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  436796 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  436796 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  436796 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  436794 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  436796 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  436796 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  436796 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  436796 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  436796 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  436796 |  244 | `	return SXRET_OK;` |
|  218399 |  245 |  |
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
|  162308 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  162310 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  162310 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  162310 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  162310 |  265 | `	return rc;` |
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
|  331238 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  331240 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  647688 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  316450 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  123290 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  193162 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   30856 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  162308 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  162308 |  298 | `		if( pInstr ){` |
|  162308 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  162308 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  162308 |  302 | `			aFix[n].nJumpType = -1;` |
|   81153 |  303 | `		}` |
|   81155 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  331240 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|   96790 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|   96792 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|   96938 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   96790 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|   96922 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|   96790 |  358 | `	return SXRET_OK;` |
|   48397 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  419896 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  419898 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  419898 |  367 | `	if( pEntry == 0 ){` |
|  184034 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  235866 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  235866 |  371 | `	return SXRET_OK;` |
|  209950 |  372 |  |
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
|  184032 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  184034 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  184034 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   92016 |  387 | `	}` |
|  184034 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   81640 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   81642 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   81642 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   81642 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   81642 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   81642 |  408 | `	return pObj;` |
|   40822 |  409 |  |
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
|   82040 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   82042 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   82042 |  434 | `	sxu32 nIdx = 0;` |
|   82042 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   81642 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   81642 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   81642 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   81642 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   40822 |  445 | `	}else{` |
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
|   82042 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   82042 |  460 | `	return SXRET_OK;` |
|   41022 |  461 |  |
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
|   53800 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   53802 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   53802 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   53802 |  481 | `	zIn  = pStr->zString;` |
|   53802 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   53802 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     112 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     112 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   53692 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   15908 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15908 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   37786 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   37786 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   37786 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   37801 |  505 | `	for(;;){` |
|   75604 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   37786 |  508 | `			break;` |
|       - |  509 | `		}` |
|   37820 |  510 | `		zCur = zIn;` |
|  597604 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  559786 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   37820 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   37802 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   18900 |  517 | `		}` |
|   37820 |  518 | `		zIn++;` |
|   37820 |  519 | `		if( zIn < zEnd ){` |
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
|   37820 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   37786 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   37786 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   37786 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   18892 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   37786 |  543 | `	return SXRET_OK;` |
|   26902 |  544 |  |
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
|    1546 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1548 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1548 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1548 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1548 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1548 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1548 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1548 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1548 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1548 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1548 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1548 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1548 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   14632 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   14634 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   14634 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   14634 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   14634 |  650 | `	(*pCount)++;` |
|   14634 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   14634 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   14634 |  654 | `	return pConstObj;` |
|    7318 |  655 |  |
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
|   13524 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   13526 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   13526 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   13526 |  702 | `	zIn  = pStr->zString;` |
|   13526 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   13526 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   13304 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   13304 |  714 | `	iCons = 0;` |
|    7424 |  715 | `	for(;;){` |
|   22350 |  716 | `		zCur = zIn;` |
|  128378 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  107576 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  107492 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1464 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     732 |  722 | `					break;` |
|       - |  723 | `			}` |
|  106030 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   22350 |  726 | `		if( zIn > zCur ){` |
|   10884 |  727 | `			if( pObj == 0 ){` |
|   10614 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   10614 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5306 |  732 | `			}` |
|   10884 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5441 |  734 | `		}` |
|   22350 |  735 | `		if( zIn >= zEnd ){` |
|   13304 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9048 |  738 | `		if( zIn[0] == '\\' ){` |
|    7502 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7502 |  741 | `			zIn++;` |
|    7502 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7502 |  745 | `			if( pObj == 0 ){` |
|    4022 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4022 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2010 |  750 | `			}` |
|    7502 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7502 |  752 | `			switch( zIn[0] ){` |
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
|    3388 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    6778 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    6778 |  776 | `				break;` |
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
|    7502 |  844 | `			zIn += n;` |
|    7502 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1548 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1462 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     730 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    2922 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1462 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     730 |  888 | `				for(;;){` |
|    9414 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7224 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1462 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1462 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1462 |  902 | `				if( zIn >= zEnd ){` |
|      79 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1384 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1376 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1372 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1372 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     687 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1462 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1462 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1462 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1460 |  957 | `				++iCons;` |
|     729 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1548 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   13304 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1182 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     590 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   13304 |  968 | `	return SXRET_OK;` |
|    6764 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   13498 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   13500 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    6749 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   13500 |  980 | `	return rc;` |
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
|   14832 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   14834 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   14834 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   14834 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   14834 | 1028 | `	return rc;` |
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
|   21876 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   21878 | 1071 | `	sxi32 iEmitRef = 0;` |
|   21878 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   21878 | 1075 | `	xValidator = 0;` |
|   17747 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   40098 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4604 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   35496 | 1081 | `		pCur = pGen->pIn;` |
|   35496 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   21866 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   13632 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   13632 | 1090 | `		pKey = pCur;` |
|   13632 | 1091 | `		iNest = 0;` |
|   37802 | 1092 | `		while( pCur < pGen->pIn ){` |
|   25338 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1168 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   24172 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      66 | 1097 | `				iNest++;` |
|   24140 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      66 | 1102 | `				iNest--;` |
|      32 | 1103 | `			}` |
|   24172 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   13632 | 1106 | `		rc = SXERR_EMPTY;` |
|   13632 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   13044 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   12466 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   13622 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   12468 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6233 | 1134 | `		}` |
|   13622 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   13620 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   13620 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   13620 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   13620 | 1158 | `		xValidator = 0;` |
|   13620 | 1159 | `		iEmitRef = 0;` |
|   13620 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   21866 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   21866 | 1165 | `	return SXRET_OK;` |
|   10940 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   21788 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   21790 | 1179 | `	pGen->pIn += 2;` |
|   21790 | 1180 | `	pGen->pEnd--;` |
|   10894 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   21790 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
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
|  673374 | 1458 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1459 |  |
|  673376 | 1460 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1461 | `	sxi32 iVv;` |
|       - | 1462 | `	sxi32 iP1;` |
|       - | 1463 | `	void *p3;` |
|       - | 1464 | `	sxi32 rc;` |
|  673376 | 1465 | `	iVv = -1; /* Variable variable counter */` |
| 1346762 | 1466 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  673388 | 1467 | `		pGen->pIn++;` |
|  673388 | 1468 | `		iVv++;` |
|       2 | 1469 | `	}` |
|  673376 | 1470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1471 | `		/* Invalid variable name */` |
|       3 | 1472 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1473 | `		if( rc == SXERR_ABORT ){` |
|       - | 1474 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1475 | `			return SXERR_ABORT;` |
|       - | 1476 | `		}` |
|       3 | 1477 | `		return SXRET_OK;` |
|       - | 1478 | `	}` |
|  673374 | 1479 | `	p3  = 0;` |
|  673374 | 1480 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  673358 | 1500 | `		char *zName = 0;` |
|       - | 1501 | `		/* Extract variable name */` |
|  673358 | 1502 | `		pName = &pGen->pIn->sData;` |
|       - | 1503 | `		/* Advance the stream cursor */` |
|  673358 | 1504 | `		pGen->pIn++;` |
|  673358 | 1505 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  673358 | 1506 | `		if( pEntry == 0 ){` |
|       - | 1507 | `			/* Duplicate name */` |
|   99892 | 1508 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   99892 | 1509 | `			if( zName == 0 ){` |
|     ! 0 | 1510 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1511 | `				return SXERR_ABORT;` |
|       - | 1512 | `			}` |
|       - | 1513 | `			/* Install in the hashtable */` |
|   99892 | 1514 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   49947 | 1515 | `		}else{` |
|       - | 1516 | `			/* Name already available */` |
|  573468 | 1517 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1518 | `		}` |
|  673358 | 1519 | `		p3 = (void *)zName;` |
|       - | 1520 | `	}` |
|  673370 | 1521 | `	iP1 = 0;` |
|  673370 | 1522 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  223398 | 1523 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1524 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  223394 | 1525 | `			iP1 = 1;` |
|  111696 | 1526 | `		}` |
|  111698 | 1527 | `	}` |
|       - | 1528 | `	/* Emit the load instruction */` |
|  673370 | 1529 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  673382 | 1530 | `	while( iVv > 0 ){` |
|      13 | 1531 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1532 | `		iVv--;` |
|       1 | 1533 | `	}` |
|       - | 1534 | `	/* Node successfully compiled */` |
|  673370 | 1535 | `	return SXRET_OK;` |
|  336689 | 1536 |  |
|       - | 1537 | `/*` |
|       - | 1538 | ` * Load a literal.` |
|       - | 1539 | ` */` |
|  435022 | 1540 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1541 |  |
|  435024 | 1542 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1543 | `	ph7_value *pObj;` |
|       - | 1544 | `	SyString *pStr;` |
|       - | 1545 | `	sxu32 nIdx;` |
|       - | 1546 | `	/* Extract token value */` |
|  435024 | 1547 | `	pStr = &pToken->sData;` |
|       - | 1548 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  435024 | 1549 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   81546 | 1550 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1551 | `			/* NULL constant are always indexed at 0 */` |
|   30362 | 1552 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   30362 | 1553 | `			return SXRET_OK;` |
|   51186 | 1554 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1555 | `			/* TRUE constant are always indexed at 1 */` |
|     464 | 1556 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     464 | 1557 | `			return SXRET_OK;` |
|       2 | 1558 | `		}` |
|  418672 | 1559 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   79662 | 1560 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1561 | `			/* FALSE constant are always indexed at 2 */` |
|   33106 | 1562 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   33106 | 1563 | `			return SXRET_OK;` |
|  354583 | 1564 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   68414 | 1565 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1566 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5016 | 1567 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5016 | 1568 | `			if( pObj == 0 ){` |
|     ! 0 | 1569 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1570 | `				return SXERR_ABORT;` |
|       - | 1571 | `			}` |
|    5016 | 1572 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1573 | `			/* Emit the load constant instruction */` |
|    5016 | 1574 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5016 | 1575 | `			return SXRET_OK;` |
|  324243 | 1576 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   17762 | 1577 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  323426 | 1593 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|    8046 | 1594 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  319397 | 1595 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|    8096 | 1596 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  366070 | 1626 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1627 | `		ph7_value *pLitObj;` |
|       - | 1628 | `		/* Unknown literal,install it in the literal table */` |
|  146184 | 1629 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  146184 | 1630 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1631 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1632 | `			return SXERR_ABORT;` |
|       - | 1633 | `		}` |
|  146184 | 1634 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  146184 | 1635 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|   73091 | 1636 | `	}` |
|       - | 1637 | `	/* Emit the load constant instruction */` |
|  366070 | 1638 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  366070 | 1639 | `	return SXRET_OK;` |
|  217513 | 1640 |  |
|       - | 1641 | `/*` |
|       - | 1642 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1643 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1644 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1645 | ` * Otherwise, load the simple literal directly.` |
|       - | 1646 | ` */` |
|  435042 | 1647 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1648 |  |
|       - | 1649 | `	sxi32 rc;` |
|  435044 | 1650 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1651 | `		return SXRET_OK;` |
|       - | 1652 | `	}` |
|       - | 1653 | `	/* Check if this is a multi-token namespace path */` |
|  435044 | 1654 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  435024 | 1704 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  435024 | 1705 | `	return rc;` |
|  217523 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1709 | ` */` |
|  435042 | 1710 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1711 |  |
|       - | 1712 | `	sxi32 rc;` |
|  435044 | 1713 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  435044 | 1714 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1715 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1716 | `		return rc;` |
|       - | 1717 | `	}` |
|       - | 1718 | `	/* Node successfully compiled */` |
|  435044 | 1719 | `	return SXRET_OK;` |
|  217523 | 1720 |  |
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
|    2544 | 1863 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1864 |  |
|       - | 1865 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1866 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1867 | `	sxu32 nLineLocal;` |
|       - | 1868 | `	sxi32 rc;` |
|    2546 | 1869 | `	nLineLocal = pGen->pIn->nLine;` |
|    2546 | 1870 | `	iLevel = 0;` |
|       - | 1871 | `	/* Jump the 'continue' keyword */` |
|    2546 | 1872 | `	pGen->pIn++;` |
|    2546 | 1873 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2546 | 1884 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2546 | 1885 | `	if( pLoop == 0 ){` |
|       - | 1886 | `		/* Illegal continue */` |
|      11 | 1887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1888 | `		if( rc == SXERR_ABORT ){` |
|       - | 1889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1890 | `			return SXERR_ABORT;` |
|       - | 1891 | `		}` |
|       6 | 1892 | `	}else{` |
|    2536 | 1893 | `		sxu32 nInstrIdx = 0;` |
|    2536 | 1894 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2532 | 1906 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2532 | 1907 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 1908 | `				JumpFixup sJumpFix;` |
|       - | 1909 | `				/* Post-continue */` |
|       8 | 1910 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       8 | 1911 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       8 | 1912 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       3 | 1913 | `			}` |
|       - | 1914 | `		}` |
|       - | 1915 | `	}` |
|    2546 | 1916 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 1917 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 1918 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 1919 | `	}` |
|       - | 1920 | `	/* Statement successfully compiled */` |
|    2546 | 1921 | `	return SXRET_OK;` |
|    1274 | 1922 |  |
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
|  228298 | 2182 | `static sxi32 PH7_CompileBlock(` |
|       - | 2183 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2184 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2185 | `	)` |
|       2 | 2186 |  |
|       - | 2187 | `	sxi32 rc;` |
|       - | 2188 | `	sxu32 nLine;` |
|  228300 | 2189 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  226994 | 2190 | `		nLine = pGen->pIn->nLine;` |
|  226994 | 2191 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  226994 | 2192 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2193 | `			return SXERR_ABORT;` |
|       - | 2194 | `		}` |
|  226994 | 2195 | `		pGen->pIn++;` |
|       - | 2196 | `		/* Compile until we hit the closing braces '}' */` |
|  331521 | 2197 | `		for(;;){` |
|  663044 | 2198 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  663024 | 2209 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2210 | `				/* Closing braces found,break immediately*/` |
|  226974 | 2211 | `				pGen->pIn++;` |
|  226974 | 2212 | `				break;` |
|       - | 2213 | `			}` |
|       - | 2214 | `			/* Compile a single statement */` |
|  436052 | 2215 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  436052 | 2216 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2217 | `				return SXERR_ABORT;` |
|       - | 2218 | `			}` |
|       2 | 2219 | `		}` |
|  226994 | 2220 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  114804 | 2221 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1308 | 2265 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1308 | 2266 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2267 | `			return SXERR_ABORT;` |
|       - | 2268 | `		}` |
|       - | 2269 | `	}` |
|       - | 2270 | `	/* Jump trailing semi-colons ';' */` |
|  228300 | 2271 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2272 | `		pGen->pIn++;` |
|     ! 0 | 2273 | `	}` |
|  228300 | 2274 | `	return SXRET_OK;` |
|  114151 | 2275 |  |
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
|   10100 | 2295 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2296 |  |
|   10102 | 2297 | `	GenBlock *pWhileBlock = 0;` |
|   10102 | 2298 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2299 | `	sxu32 nFalseJump;` |
|       - | 2300 | `	sxu32 nLine;` |
|       - | 2301 | `	sxi32 rc;` |
|   10102 | 2302 | `	nLine = pGen->pIn->nLine;` |
|       - | 2303 | `	/* Jump the 'while' keyword */` |
|   10102 | 2304 | `	pGen->pIn++;` |
|   10102 | 2305 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2306 | `		/* Syntax error */` |
|     ! 0 | 2307 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2308 | `		if( rc == SXERR_ABORT ){` |
|       - | 2309 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2310 | `			return SXERR_ABORT;` |
|       - | 2311 | `		}` |
|     ! 0 | 2312 | `		goto Synchronize;` |
|       - | 2313 | `	}` |
|       - | 2314 | `	/* Jump the left parenthesis '(' */` |
|   10102 | 2315 | `	pGen->pIn++;` |
|       - | 2316 | `	/* Create the loop block */` |
|   10102 | 2317 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10102 | 2318 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2319 | `		return SXERR_ABORT;` |
|       - | 2320 | `	}` |
|       - | 2321 | `	/* Delimit the condition */` |
|   10102 | 2322 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10102 | 2323 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2324 | `		/* Empty expression */` |
|       3 | 2325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2326 | `		if( rc == SXERR_ABORT ){` |
|       - | 2327 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2328 | `			return SXERR_ABORT;` |
|       - | 2329 | `		}` |
|       1 | 2330 | `	}` |
|       - | 2331 | `	/* Swap token streams */` |
|   10102 | 2332 | `	pTmp = pGen->pEnd;` |
|   10102 | 2333 | `	pGen->pEnd = pEnd;` |
|       - | 2334 | `	/* Compile the expression */` |
|   10102 | 2335 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10102 | 2336 | `	if( rc == SXERR_ABORT ){` |
|       - | 2337 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2338 | `		return SXERR_ABORT;` |
|       - | 2339 | `	}` |
|       - | 2340 | `	/* Update token stream */` |
|   10102 | 2341 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2342 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2343 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2344 | `			return SXERR_ABORT;` |
|       - | 2345 | `		}` |
|     ! 0 | 2346 | `		pGen->pIn++;` |
|     ! 0 | 2347 | `	}` |
|       - | 2348 | `	/* Synchronize pointers */` |
|   10102 | 2349 | `	pGen->pIn  = &pEnd[1];` |
|   10102 | 2350 | `	pGen->pEnd = pTmp;` |
|       - | 2351 | `	/* Emit the false jump */` |
|   10102 | 2352 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2353 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10102 | 2354 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2355 | `	/* Compile the loop body */` |
|   10102 | 2356 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10102 | 2357 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2358 | `		return SXERR_ABORT;` |
|       - | 2359 | `	}` |
|       - | 2360 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10102 | 2361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2362 | `	/* Fix all jumps now the destination is resolved */` |
|   10102 | 2363 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2364 | `	/* Release the loop block */` |
|   10102 | 2365 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2366 | `	/* Statement successfully compiled */` |
|   10102 | 2367 | `	return SXRET_OK;` |
|     ! 0 | 2368 | `Synchronize:` |
|       - | 2369 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2370 | `	 * compiling this erroneous block.` |
|       - | 2371 | `	 */` |
|     ! 0 | 2372 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2373 | `		pGen->pIn++;` |
|     ! 0 | 2374 | `	}` |
|     ! 0 | 2375 | `	return SXRET_OK;` |
|    5052 | 2376 |  |
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
|   10102 | 2524 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2525 |  |
|   10104 | 2526 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10104 | 2527 | `	GenBlock *pForBlock = 0;` |
|       - | 2528 | `	sxu32 nFalseJump;` |
|       - | 2529 | `	sxu32 nLine;` |
|       - | 2530 | `	sxi32 rc;` |
|   10104 | 2531 | `	nLine = pGen->pIn->nLine;` |
|       - | 2532 | `	/* Jump the 'for' keyword */` |
|   10104 | 2533 | `	pGen->pIn++;` |
|   10104 | 2534 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2535 | `		/* Syntax error */` |
|     ! 0 | 2536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2537 | `		if( rc == SXERR_ABORT ){` |
|       - | 2538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2539 | `			return SXERR_ABORT;` |
|       - | 2540 | `		}` |
|     ! 0 | 2541 | `		return SXRET_OK;` |
|       - | 2542 | `	}` |
|       - | 2543 | `	/* Jump the left parenthesis '(' */` |
|   10104 | 2544 | `	pGen->pIn++;` |
|       - | 2545 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10104 | 2546 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10104 | 2547 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   10104 | 2562 | `	pTmp = pGen->pEnd;` |
|   10104 | 2563 | `	pGen->pEnd = pEnd;` |
|       - | 2564 | `	/* Compile initialization expressions if available */` |
|   10104 | 2565 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2566 | `	/* Pop operand lvalues */` |
|   10104 | 2567 | `	if( rc == SXERR_ABORT ){` |
|       - | 2568 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2569 | `		return SXERR_ABORT;` |
|   10104 | 2570 | `	}else if( rc != SXERR_EMPTY ){` |
|   10102 | 2571 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5050 | 2572 | `	}` |
|   10104 | 2573 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   10104 | 2584 | `	pGen->pIn++;` |
|       - | 2585 | `	/* Create the loop block */` |
|   10104 | 2586 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10104 | 2587 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2588 | `		return SXERR_ABORT;` |
|       - | 2589 | `	}` |
|       - | 2590 | `	/* Deffer continue jumps */` |
|   10104 | 2591 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2592 | `	/* Compile the condition */` |
|   10104 | 2593 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10104 | 2594 | `	if( rc == SXERR_ABORT ){` |
|       - | 2595 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2596 | `		return SXERR_ABORT;` |
|   10104 | 2597 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2598 | `		/* Emit the false jump */` |
|   10102 | 2599 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2600 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10102 | 2601 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5050 | 2602 | `	}` |
|   10104 | 2603 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   10100 | 2614 | `	pGen->pIn++;` |
|       - | 2615 | `	/* Save the post condition stream */` |
|   10100 | 2616 | `	pPostStart = pGen->pIn;` |
|       - | 2617 | `	/* Compile the loop body */` |
|   10100 | 2618 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10100 | 2619 | `	pGen->pEnd = pTmp;` |
|   10100 | 2620 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10100 | 2621 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2622 | `		return SXERR_ABORT;` |
|       - | 2623 | `	}` |
|       - | 2624 | `	/* Fix post-continue jumps */` |
|   10100 | 2625 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   10100 | 2641 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2642 | `		pPostStart++;` |
|     ! 0 | 2643 | `	}` |
|   10100 | 2644 | `	if( pPostStart < pEnd ){` |
|       - | 2645 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10100 | 2646 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10100 | 2647 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10100 | 2648 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2649 | `			/* Syntax error */` |
|     ! 0 | 2650 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2651 | `			if( rc == SXERR_ABORT ){` |
|       - | 2652 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2653 | `				return SXERR_ABORT;` |
|       - | 2654 | `			}` |
|     ! 0 | 2655 | `			return SXRET_OK;` |
|       - | 2656 | `		}` |
|   10100 | 2657 | `		RE_SWAP_DELIMITER(pGen);` |
|   10100 | 2658 | `		if( rc == SXERR_ABORT ){` |
|       - | 2659 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2660 | `			return SXERR_ABORT;` |
|   10100 | 2661 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2662 | `			/* Pop operand lvalue */` |
|   10100 | 2663 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5049 | 2664 | `		}` |
|    5049 | 2665 | `	}` |
|       - | 2666 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10100 | 2667 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2668 | `	/* Fix all jumps now the destination is resolved */` |
|   10100 | 2669 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2670 | `	/* Release the loop block */` |
|   10100 | 2671 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2672 | `	/* Statement successfully compiled */` |
|   10100 | 2673 | `	return SXRET_OK;` |
|    5053 | 2674 |  |
|       - | 2675 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2676 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2677 | ` * are allowed.` |
|       - | 2678 | ` */` |
|    5366 | 2679 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2680 |  |
|    5368 | 2681 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5368 | 2682 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2683 | `		/* Unexpected expression */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2685 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2686 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2687 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2688 | `		}` |
|     ! 0 | 2689 | `	}` |
|    5368 | 2690 | `	return rc;` |
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
|    2712 | 2718 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2719 |  |
|    2714 | 2720 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2714 | 2721 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2714 | 2722 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2723 | `	ph7_foreach_info *pInfo;` |
|       - | 2724 | `	sxu32 nFalseJump;` |
|       - | 2725 | `	VmInstr *pInstr;` |
|       - | 2726 | `	sxu32 nLine;` |
|       - | 2727 | `	sxi32 rc;` |
|    2714 | 2728 | `	nLine = pGen->pIn->nLine;` |
|       - | 2729 | `	/* Jump the 'foreach' keyword */` |
|    2714 | 2730 | `	pGen->pIn++;` |
|    2714 | 2731 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2732 | `		/* Syntax error */` |
|     ! 0 | 2733 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2734 | `		if( rc == SXERR_ABORT ){` |
|       - | 2735 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2736 | `			return SXERR_ABORT;` |
|       - | 2737 | `		}` |
|     ! 0 | 2738 | `		goto Synchronize;` |
|       - | 2739 | `	}` |
|       - | 2740 | `	/* Jump the left parenthesis '(' */` |
|    2714 | 2741 | `	pGen->pIn++;` |
|       - | 2742 | `	/* Create the loop block */` |
|    2714 | 2743 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2714 | 2744 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2745 | `		return SXERR_ABORT;` |
|       - | 2746 | `	}` |
|       - | 2747 | `	/* Delimit the expression */` |
|    2714 | 2748 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2714 | 2749 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    2714 | 2764 | `	pCur = pGen->pIn;` |
|   18234 | 2765 | `	while( pCur < pEnd ){` |
|   18234 | 2766 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2724 | 2767 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2724 | 2768 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2769 | `				/* Break with the first 'as' found */` |
|    2714 | 2770 | `				break;` |
|       - | 2771 | `			}` |
|       5 | 2772 | `		}` |
|       - | 2773 | `		/* Advance the stream cursor */` |
|   15522 | 2774 | `		pCur++;` |
|       2 | 2775 | `	}` |
|    2714 | 2776 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2777 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2778 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2779 | `		if( rc == SXERR_ABORT ){` |
|       - | 2780 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2781 | `			return SXERR_ABORT;` |
|       - | 2782 | `		}` |
|     ! 0 | 2783 | `		goto Synchronize;` |
|       - | 2784 | `	}` |
|       - | 2785 | `	/* Swap token streams */` |
|    2714 | 2786 | `	pTmp = pGen->pEnd;` |
|    2714 | 2787 | `	pGen->pEnd = pCur;` |
|    2714 | 2788 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2714 | 2789 | `	if( rc == SXERR_ABORT ){` |
|       - | 2790 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2791 | `		return SXERR_ABORT;` |
|       - | 2792 | `	}` |
|       - | 2793 | `	/* Update token stream */` |
|    2714 | 2794 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2795 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2796 | `		if( rc == SXERR_ABORT ){` |
|       - | 2797 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2798 | `			return SXERR_ABORT;` |
|       - | 2799 | `		}` |
|     ! 0 | 2800 | `		pGen->pIn++;` |
|     ! 0 | 2801 | `	}` |
|    2714 | 2802 | `	pCur++; /* Jump the 'as' keyword */` |
|    2714 | 2803 | `	pGen->pIn = pCur;` |
|    2714 | 2804 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2805 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2807 | `			return SXERR_ABORT;` |
|       - | 2808 | `		}` |
|     ! 0 | 2809 | `	}` |
|       - | 2810 | `	/* Create the foreach context */` |
|    2714 | 2811 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2714 | 2812 | `	if( pInfo == 0 ){` |
|     ! 0 | 2813 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2814 | `		return SXERR_ABORT;` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Zero the structure */` |
|    2714 | 2817 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2818 | `	/* Initialize structure fields */` |
|    2714 | 2819 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2820 | `	/* Check if we have a key field */` |
|    8150 | 2821 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5438 | 2822 | `		pCur++;` |
|       2 | 2823 | `	}` |
|    2714 | 2824 | `	if( pCur < pEnd ){` |
|       - | 2825 | `		/* Compile the expression holding the key name */` |
|    2662 | 2826 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2827 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2828 | `			if( rc == SXERR_ABORT ){` |
|       - | 2829 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2830 | `				return SXERR_ABORT;` |
|       - | 2831 | `			}` |
|     ! 0 | 2832 | `		}else{` |
|    2662 | 2833 | `			pGen->pEnd = pCur;` |
|    2662 | 2834 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2662 | 2835 | `			if( rc == SXERR_ABORT ){` |
|       - | 2836 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2837 | `				return SXERR_ABORT;` |
|       - | 2838 | `			}` |
|    2662 | 2839 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2662 | 2840 | `			if( pInstr->p3 ){` |
|       - | 2841 | `				/* Record key name */` |
|    2662 | 2842 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1330 | 2843 | `			}` |
|    2662 | 2844 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2845 | `		}` |
|    2662 | 2846 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1330 | 2847 | `	}` |
|    2714 | 2848 | `	pGen->pEnd = pEnd;` |
|    2714 | 2849 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2850 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2851 | `		if( rc == SXERR_ABORT ){` |
|       - | 2852 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2853 | `			return SXERR_ABORT;` |
|       - | 2854 | `		}` |
|     ! 0 | 2855 | `		goto Synchronize;` |
|       - | 2856 | `	}` |
|    2714 | 2857 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       7 | 2858 | `		pGen->pIn++;` |
|       - | 2859 | `		/* Pass by reference  */` |
|       7 | 2860 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       3 | 2861 | `	}` |
|       - | 2862 | `	/* Check if the value target is list() */` |
|    2714 | 2863 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    2708 | 2906 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2708 | 2907 | `		if( rc == SXERR_ABORT ){` |
|       - | 2908 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2909 | `			return SXERR_ABORT;` |
|       - | 2910 | `		}` |
|    2708 | 2911 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2708 | 2912 | `		if( pInstr->p3 ){` |
|       - | 2913 | `			/* Record value name */` |
|    2708 | 2914 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1353 | 2915 | `		}` |
|       - | 2916 | `	}` |
|       - | 2917 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2712 | 2918 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 2919 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2712 | 2920 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 2921 | `	/* Record the first instruction to execute */` |
|    2712 | 2922 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2923 | `	/* Emit the FOREACH_STEP instruction */` |
|    2712 | 2924 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 2925 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2712 | 2926 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 2927 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2712 | 2928 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    2712 | 2952 | `	pGen->pIn = &pEnd[1];` |
|    2712 | 2953 | `	pGen->pEnd = pTmp;` |
|    2712 | 2954 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2712 | 2955 | `	if( rc == SXERR_ABORT ){` |
|       - | 2956 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2957 | `		return SXERR_ABORT;` |
|       - | 2958 | `	}` |
|       - | 2959 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2712 | 2960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 2961 | `	/* Fix all jumps now the destination is resolved */` |
|    2712 | 2962 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2963 | `	/* Release the loop block */` |
|    2712 | 2964 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2965 | `	/* Statement successfully compiled */` |
|    2712 | 2966 | `	return SXRET_OK;` |
|       1 | 2967 | `Synchronize:` |
|       - | 2968 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2969 | `	 * compiling this erroneous block.` |
|       - | 2970 | `	 */` |
|       3 | 2971 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2972 | `		pGen->pIn++;` |
|     ! 0 | 2973 | `	}` |
|       3 | 2974 | `	return SXRET_OK;` |
|    1358 | 2975 |  |
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
|  100674 | 3008 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3009 |  |
|  100676 | 3010 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  100676 | 3011 | `	GenBlock *pCondBlock = 0;` |
|       - | 3012 | `	sxu32 nJumpIdx;` |
|       - | 3013 | `	sxu32 nKeyID;` |
|       - | 3014 | `	sxi32 rc;` |
|       - | 3015 | `	/* Jump the 'if' keyword */` |
|  100676 | 3016 | `	pGen->pIn++;` |
|  100676 | 3017 | `	pToken = pGen->pIn;` |
|       - | 3018 | `	/* Create the conditional block */` |
|  100676 | 3019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  100676 | 3020 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3021 | `		return SXERR_ABORT;` |
|       - | 3022 | `	}` |
|       - | 3023 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   55361 | 3024 | `	for(;;){` |
|  110724 | 3025 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  110724 | 3038 | `		pToken++;` |
|       - | 3039 | `		/* Delimit the condition */` |
|  110724 | 3040 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  110724 | 3041 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  110724 | 3054 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3055 | `		/* Compile the condition */` |
|  110724 | 3056 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3057 | `		/* Update token stream */` |
|  110724 | 3058 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3059 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3060 | `			pGen->pIn++;` |
|     ! 0 | 3061 | `		}` |
|  110724 | 3062 | `		pGen->pIn  = &pEnd[1];` |
|  110724 | 3063 | `		pGen->pEnd = pTmp;` |
|  110724 | 3064 | `		if( rc == SXERR_ABORT ){` |
|       - | 3065 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3066 | `			return SXERR_ABORT;` |
|       - | 3067 | `		}` |
|       - | 3068 | `		/* Emit the false jump */` |
|  110724 | 3069 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3070 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  110724 | 3071 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3072 | `		/* Compile the body */` |
|  110724 | 3073 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  110724 | 3074 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3075 | `			return SXERR_ABORT;` |
|       - | 3076 | `		}` |
|  110724 | 3077 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   29801 | 3078 | `			break;` |
|       - | 3079 | `		}` |
|       - | 3080 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   51126 | 3081 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51126 | 3082 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   32840 | 3083 | `			break;` |
|       - | 3084 | `		}` |
|       - | 3085 | `		/* Emit the unconditional jump */` |
|   18288 | 3086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3087 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   18288 | 3088 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   18288 | 3089 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13252 | 3090 | `			pToken = &pGen->pIn[1];` |
|   13252 | 3091 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5042 | 3092 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4121 | 3093 | `					break;` |
|       - | 3094 | `			}` |
|    5014 | 3095 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2506 | 3096 | `		}` |
|   10050 | 3097 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3098 | `		/* Synchronize cursors */` |
|   10050 | 3099 | `		pToken = pGen->pIn;` |
|       - | 3100 | `		/* Fix the false jump */` |
|   10050 | 3101 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3102 | `	} /* For(;;) */` |
|       - | 3103 | `	/* Fix the false jump */` |
|  100676 | 3104 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  100676 | 3105 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   41076 | 3106 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3107 | `			/* Compile the else block */` |
|    8240 | 3108 | `			pGen->pIn++;` |
|    8240 | 3109 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8240 | 3110 | `			if( rc == SXERR_ABORT ){` |
|       - | 3111 |  |
|     ! 0 | 3112 | `				return SXERR_ABORT;` |
|       - | 3113 | `			}` |
|    4119 | 3114 | `	}` |
|  100676 | 3115 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3116 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  100676 | 3117 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3118 | `	/* Release the conditional block */` |
|  100676 | 3119 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3120 | `	/* Statement successfully compiled */` |
|  100676 | 3121 | `	return SXRET_OK;` |
|     ! 0 | 3122 | `Synchronize:` |
|       - | 3123 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3124 | `	 */` |
|     ! 0 | 3125 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3126 | `		pGen->pIn++;` |
|     ! 0 | 3127 | `	}` |
|     ! 0 | 3128 | `	return SXRET_OK;` |
|   50339 | 3129 |  |
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
|  105758 | 3223 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3224 |  |
|  105760 | 3225 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3226 | `	sxi32 rc;` |
|       - | 3227 | `	/* Jump the 'return' keyword */` |
|  105760 | 3228 | `	pGen->pIn++;` |
|  105760 | 3229 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3230 | `		/* Compile the expression */` |
|  105738 | 3231 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  105738 | 3232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3233 | `			return SXERR_ABORT;` |
|  105738 | 3234 | `		}else if(rc != SXERR_EMPTY ){` |
|  105738 | 3235 | `			nRet = 1;` |
|   52868 | 3236 | `		}` |
|   52868 | 3237 | `	}` |
|       - | 3238 | `	/* Emit the done instruction */` |
|  105760 | 3239 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  105760 | 3240 | `	return SXRET_OK;` |
|   52881 | 3241 |  |
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
|    9564 | 3269 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3270 |  |
|    9566 | 3271 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3272 | `	sxi32 rc;` |
|       - | 3273 | `	/* Jump the 'echo' keyword */` |
|    9566 | 3274 | `	pGen->pIn++;` |
|       - | 3275 | `	/* Compile arguments one after one */` |
|    9566 | 3276 | `	pTmp = pGen->pEnd;` |
|   19518 | 3277 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    9954 | 3278 | `		if( pGen->pIn < pNext ){` |
|    9954 | 3279 | `			pGen->pEnd = pNext;` |
|    9954 | 3280 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    9954 | 3281 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3282 | `				return SXERR_ABORT;` |
|    9954 | 3283 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3284 | `				/* Emit the consume instruction */` |
|    9930 | 3285 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    4964 | 3286 | `			}` |
|    4976 | 3287 | `		}` |
|       - | 3288 | `		/* Jump trailing commas */` |
|   10342 | 3289 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3290 | `			pNext++;` |
|       2 | 3291 | `		}` |
|    9954 | 3292 | `		pGen->pIn = pNext;` |
|       2 | 3293 | `	}` |
|       - | 3294 | `	/* Restore token stream */` |
|    9566 | 3295 | `	pGen->pEnd = pTmp;` |
|    9566 | 3296 | `	return SXRET_OK;` |
|    4784 | 3297 |  |
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
|  251270 | 3459 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
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
|  251272 | 3470 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  251166 | 3471 | `		return nOrigIdx; /* Not in a namespace */` |
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
|  125637 | 3512 |  |
|       - | 3513 | `/*` |
|       - | 3514 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3515 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3516 | ` */` |
|   15176 | 3517 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3518 |  |
|       - | 3519 | `	SyHashEntry *pImport;` |
|       - | 3520 | `	/* Check use imports first */` |
|   15178 | 3521 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   15178 | 3522 | `	if( pImport ){` |
|       7 | 3523 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3524 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3525 | `		return;` |
|       - | 3526 | `	}` |
|       - | 3527 | `	/* Prepend current namespace if active */` |
|   15172 | 3528 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3529 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3530 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3531 | `	}` |
|   15172 | 3532 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|    7590 | 3533 |  |
|       - | 3534 | `/*` |
|       - | 3535 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3536 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3537 | ` * The caller must release pOut when done.` |
|       - | 3538 | ` */` |
|   30514 | 3539 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3540 |  |
|   30516 | 3541 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3542 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3543 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3544 | `	}` |
|   30516 | 3545 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   30516 | 3546 |  |
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
|      52 | 3593 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       1 | 3594 |  |
|       - | 3595 | `	sxu32 nLine;` |
|       - | 3596 | `	sxi32 rc;` |
|      53 | 3597 | `	nLine = pGen->pIn->nLine;` |
|      53 | 3598 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3599 | `	/* Reset namespace and clear previous use imports */` |
|      53 | 3600 | `	SyBlobReset(&pGen->sNamespace);` |
|      53 | 3601 | `	SyHashRelease(&pGen->hUseImports);` |
|      53 | 3602 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      53 | 3603 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3604 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3605 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3606 | `		return SXRET_OK;` |
|       - | 3607 | `	}` |
|      53 | 3608 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3609 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3611 | `		return SXRET_OK;` |
|       - | 3612 | `	}` |
|      53 | 3613 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3614 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3615 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3616 | `		return SXRET_OK;` |
|       - | 3617 | `	}` |
|       - | 3618 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     135 | 3619 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      83 | 3620 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3621 | `			/* Append backslash separator */` |
|      17 | 3622 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      17 | 3623 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       8 | 3624 | `			}` |
|       9 | 3625 | `		}else{` |
|       - | 3626 | `			/* Append identifier */` |
|      67 | 3627 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3628 | `		}` |
|      83 | 3629 | `		pGen->pIn++;` |
|       1 | 3630 | `	}` |
|       - | 3631 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3632 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3633 | `	{` |
|      53 | 3634 | `		char *zNsDup = 0;` |
|      53 | 3635 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      76 | 3636 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      50 | 3637 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      25 | 3638 | `		}` |
|      53 | 3639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3640 | `	}` |
|      53 | 3641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3642 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3643 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3644 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3645 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3646 | `			return SXERR_ABORT;` |
|       - | 3647 | `		}` |
|       2 | 3648 | `	}` |
|      53 | 3649 | `	return SXRET_OK;` |
|      27 | 3650 |  |
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
|      26 | 3665 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       1 | 3666 |  |
|       - | 3667 | `	sxu32 nLine;` |
|       - | 3668 | `	sxi32 rc;` |
|       - | 3669 | `	SyBlob sPath;` |
|       - | 3670 | `	SyString sAlias;` |
|       - | 3671 | `	SyToken *pLast;` |
|       - | 3672 | `	char *zDup;` |
|      27 | 3673 | `	nLine = pGen->pIn->nLine;` |
|      27 | 3674 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|      27 | 3675 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3676 | `	/* Process one or more use declarations separated by commas */` |
|      14 | 3677 | `	for(;;){` |
|      29 | 3678 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3679 | `			break;` |
|       - | 3680 | `		}` |
|      29 | 3681 | `		SyBlobReset(&sPath);` |
|      29 | 3682 | `		pLast = 0;` |
|       - | 3683 | `		/* Collect the full namespace path */` |
|     117 | 3684 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      89 | 3685 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|      57 | 3686 | `				pLast = pGen->pIn;` |
|      57 | 3687 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      33 | 3688 | `					SyBlobAppend(&sPath,"\\",1);` |
|      16 | 3689 | `				}` |
|      57 | 3690 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      28 | 3691 | `			}` |
|      89 | 3692 | `			pGen->pIn++;` |
|       1 | 3693 | `		}` |
|      29 | 3694 | `		if( pLast == 0 ){` |
|       - | 3695 | `			/* Empty path */` |
|       5 | 3696 | `			break;` |
|       - | 3697 | `		}` |
|       - | 3698 | `		/* Default alias is the last component of the path */` |
|      25 | 3699 | `		sAlias = pLast->sData;` |
|       - | 3700 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      24 | 3701 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      15 | 3702 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       5 | 3703 | `			pGen->pIn++; /* Jump 'as' */` |
|       5 | 3704 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       5 | 3705 | `				sAlias = pGen->pIn->sData;` |
|       5 | 3706 | `				pGen->pIn++;` |
|       2 | 3707 | `			}` |
|       2 | 3708 | `		}` |
|       - | 3709 | `		/* Check for duplicate import alias */` |
|      25 | 3710 | `		if( SyHashGet(&pGen->hUseImports,sAlias.zString,sAlias.nByte) != 0 ){` |
|       4 | 3711 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 3712 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       2 | 3713 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       3 | 3714 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3715 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 3716 | `				return SXERR_ABORT;` |
|       - | 3717 | `			}` |
|       1 | 3718 | `		}` |
|       - | 3719 | `		/* Register the import: alias -> FQN.` |
|       - | 3720 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 3721 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 3722 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      37 | 3723 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      24 | 3724 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      25 | 3725 | `		if( zDup ){` |
|       - | 3726 | `			char *zAliasDup;` |
|      25 | 3727 | `			SyHashInsert(&pGen->hUseImports,sAlias.zString,sAlias.nByte,zDup);` |
|       - | 3728 | `			/* Duplicate the alias key for the VM hash (token pointers may not survive to runtime) */` |
|      25 | 3729 | `			zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      25 | 3730 | `			if( zAliasDup ){` |
|      25 | 3731 | `				SyHashInsert(&pGen->pVm->hUseImports,zAliasDup,sAlias.nByte,zDup);` |
|      12 | 3732 | `			}` |
|      12 | 3733 | `		}` |
|       - | 3734 | `		/* Check for comma (multiple use declarations) */` |
|      25 | 3735 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3736 | `			pGen->pIn++;` |
|       2 | 3737 | `		}else{` |
|      12 | 3738 | `			break;` |
|       - | 3739 | `		}` |
|       1 | 3740 | `	}` |
|      27 | 3741 | `	SyBlobRelease(&sPath);` |
|      27 | 3742 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 3743 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 3744 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 3745 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3746 | `			return SXERR_ABORT;` |
|       - | 3747 | `		}` |
|       1 | 3748 | `	}` |
|      27 | 3749 | `	return SXRET_OK;` |
|      14 | 3750 |  |
|       - | 3751 | `/*` |
|       - | 3752 | ` * Compile the stupid 'declare' language construct.` |
|       - | 3753 | ` *` |
|       - | 3754 | ` * According to the PHP language reference manual.` |
|       - | 3755 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 3756 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 3757 | ` *  declare (directive)` |
|       - | 3758 | ` *   statement` |
|       - | 3759 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 3760 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 3761 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 3762 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 3763 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 3764 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 3765 | ` * <?php` |
|       - | 3766 | ` * // these are the same:` |
|       - | 3767 | ` * // you can use this:` |
|       - | 3768 | ` * declare(ticks=1) {` |
|       - | 3769 | ` *   // entire script here` |
|       - | 3770 | ` * }` |
|       - | 3771 | ` * // or you can use this:` |
|       - | 3772 | ` * declare(ticks=1);` |
|       - | 3773 | ` * // entire script here` |
|       - | 3774 | ` * ?>` |
|       - | 3775 | ` *` |
|       - | 3776 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 3777 | ` */` |
|       8 | 3778 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 3779 |  |
|       9 | 3780 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 3781 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 3782 | `	sxi32 rc;` |
|       9 | 3783 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 3784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 3785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 3786 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3787 | `			return SXERR_ABORT;` |
|       - | 3788 | `		}` |
|       5 | 3789 | `		goto Synchro;` |
|       - | 3790 | `	}` |
|       5 | 3791 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 3792 | `	/* Delimit the directive */` |
|       5 | 3793 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 3794 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 3795 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 3796 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3797 | `			return SXERR_ABORT;` |
|       - | 3798 | `		}` |
|     ! 0 | 3799 | `		return SXRET_OK;` |
|       - | 3800 | `	}` |
|       - | 3801 | `	/* Update the cursor */` |
|       5 | 3802 | `	pGen->pIn = &pEnd[1];` |
|       5 | 3803 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 3804 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 3805 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3806 | `			return SXERR_ABORT;` |
|       - | 3807 | `		}` |
|     ! 0 | 3808 | `	}` |
|       - | 3809 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 3810 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 3811 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 3812 | `		ph7_lib_version()` |
|       - | 3813 | `		);` |
|       - | 3814 | `	/*All done */` |
|       5 | 3815 | `	return SXRET_OK;` |
|       2 | 3816 | `Synchro:` |
|       - | 3817 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 3818 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 3819 | `		pGen->pIn++;` |
|       1 | 3820 | `	}` |
|       5 | 3821 | `	return SXRET_OK;` |
|       5 | 3822 |  |
|       - | 3823 | `/*` |
|       - | 3824 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 3825 | ` * as follows:` |
|       - | 3826 | ` * function makecoffee($type = "cappuccino")` |
|       - | 3827 | ` * {` |
|       - | 3828 | ` *   return "Making a cup of $type.\n";` |
|       - | 3829 | ` * }` |
|       - | 3830 | ` * Symisc eXtension.` |
|       - | 3831 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 3832 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 3833 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 3834 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 3835 | ` *      {` |
|       - | 3836 | ` *       var_dump($a);` |
|       - | 3837 | ` *      }` |
|       - | 3838 | ` *     //call test without args` |
|       - | 3839 | ` *      test();` |
|       - | 3840 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 3841 | ` *      Example:` |
|       - | 3842 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 3843 | ` * 3 -) Function overloading!!` |
|       - | 3844 | ` *      Example:` |
|       - | 3845 | ` *      function foo($a) {` |
|       - | 3846 | ` *   	  return $a.PHP_EOL;` |
|       - | 3847 | ` *	    }` |
|       - | 3848 | ` *	    function foo($a, $b) {` |
|       - | 3849 | ` *   	  return $a + $b;` |
|       - | 3850 | ` *	    }` |
|       - | 3851 | ` *	    echo foo(5); // Prints "5"` |
|       - | 3852 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 3853 | ` *      // Same arg` |
|       - | 3854 | ` *	   function foo(string $a)` |
|       - | 3855 | ` *	   {` |
|       - | 3856 | ` *	     echo "a is a string\n";` |
|       - | 3857 | ` *	     var_dump($a);` |
|       - | 3858 | ` *	   }` |
|       - | 3859 | ` *	  function foo(int $a)` |
|       - | 3860 | ` *	  {` |
|       - | 3861 | ` *	    echo "a is integer\n";` |
|       - | 3862 | ` *	    var_dump($a);` |
|       - | 3863 | ` *	  }` |
|       - | 3864 | ` *	  function foo(array $a)` |
|       - | 3865 | ` *	  {` |
|       - | 3866 | ` * 	    echo "a is an array\n";` |
|       - | 3867 | ` * 	    var_dump($a);` |
|       - | 3868 | ` *	  }` |
|       - | 3869 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 3870 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 3871 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 3872 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 3873 | ` * introduced by the PH7 engine.` |
|       - | 3874 | ` */` |
|   32586 | 3875 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 3876 |  |
|       - | 3877 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 3878 | `	SySet *pInstrContainer;` |
|       - | 3879 | `	sxi32 rc;` |
|       - | 3880 | `	/* Swap token stream */` |
|   32588 | 3881 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   32588 | 3882 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   32588 | 3883 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 3884 | `	/* Compile the expression holding the argument value */` |
|   32588 | 3885 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3886 | `	/* Emit the done instruction */` |
|   32588 | 3887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   32588 | 3888 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   32588 | 3889 | `	RE_SWAP_DELIMITER(pGen);` |
|   32588 | 3890 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3891 | `		return SXERR_ABORT;` |
|       - | 3892 | `	}` |
|   32588 | 3893 | `	return SXRET_OK;` |
|   16295 | 3894 |  |
|       - | 3895 | `/*` |
|       - | 3896 | ` * Collect function arguments one after one.` |
|       - | 3897 | ` * According to the PHP language reference manual.` |
|       - | 3898 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 3899 | ` * list of expressions.` |
|       - | 3900 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 3901 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 3902 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 3903 | ` * for more information.` |
|       - | 3904 | ` * Example #1 Passing arrays to functions` |
|       - | 3905 | ` * <?php` |
|       - | 3906 | ` * function takes_array($input)` |
|       - | 3907 | ` * {` |
|       - | 3908 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 3909 | ` * }` |
|       - | 3910 | ` * ?>` |
|       - | 3911 | ` * Making arguments be passed by reference` |
|       - | 3912 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 3913 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 3914 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 3915 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 3916 | ` * to the argument name in the function definition:` |
|       - | 3917 | ` * Example #2 Passing function parameters by reference` |
|       - | 3918 | ` * <?php` |
|       - | 3919 | ` * function add_some_extra(&$string)` |
|       - | 3920 | ` * {` |
|       - | 3921 | ` *   $string .= 'and something extra.';` |
|       - | 3922 | ` * }` |
|       - | 3923 | ` * $str = 'This is a string, ';` |
|       - | 3924 | ` * add_some_extra($str);` |
|       - | 3925 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 3926 | ` * ?>` |
|       - | 3927 | ` *` |
|       - | 3928 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 3929 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 3930 | ` * on these extension.` |
|       - | 3931 | ` */` |
|   35548 | 3932 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 3933 |  |
|       - | 3934 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 3935 | `	SyToken *pIn;  /* Token stream */` |
|       - | 3936 | `	SyBlob sSig;         /* Function signature */` |
|       - | 3937 | `	char *zDup;          /* Copy of argument name */` |
|       - | 3938 | `	sxi32 rc;` |
|       - | 3939 |  |
|   35550 | 3940 | `	pIn = pGen->pIn;` |
|   35550 | 3941 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 3942 | `	/* Process arguments one after one */` |
|   48311 | 3943 | `	for(;;){` |
|   96624 | 3944 | `		if( pIn >= pEnd ){` |
|       - | 3945 | `			/* No more arguments to process */` |
|   35548 | 3946 | `			break;` |
|       - | 3947 | `		}` |
|   61078 | 3948 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   61078 | 3949 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   61078 | 3950 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   50124 | 3951 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   45112 | 3952 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   45112 | 3953 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 3954 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   45112 | 3955 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 3956 | `					sArg.nType = MEMOBJ_BOOL;` |
|   45112 | 3957 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   12532 | 3958 | `					sArg.nType = MEMOBJ_INT;` |
|   38847 | 3959 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   32580 | 3960 | `					sArg.nType = MEMOBJ_STRING;` |
|   16292 | 3961 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 3962 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 3963 | `				}else{` |
|       4 | 3964 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 3965 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 3966 | `						&pIn->sData);` |
|       - | 3967 | `				}` |
|   22557 | 3968 | `			}else{` |
|    5014 | 3969 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 3970 | `				char *zDupLocal;` |
|       - | 3971 | `				/* Argument must be a class instance,record that*/` |
|    5014 | 3972 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5014 | 3973 | `				if( zDupLocal ){` |
|    5014 | 3974 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5014 | 3975 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2506 | 3976 | `				}` |
|       - | 3977 | `			}` |
|   50124 | 3978 | `			pIn++;` |
|   25061 | 3979 | `		}` |
|   61078 | 3980 | `		if( pIn >= pEnd ){` |
|     ! 0 | 3981 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 3982 | `			return rc;` |
|       - | 3983 | `		}` |
|   61078 | 3984 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 3985 | `			/* Pass by reference,record that */` |
|    2528 | 3986 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2528 | 3987 | `			pIn++;` |
|    1263 | 3988 | `		}` |
|   61078 | 3989 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 3990 | `			/* Invalid argument */` |
|     ! 0 | 3991 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 3992 | `			return rc;` |
|       - | 3993 | `		}` |
|   61078 | 3994 | `		pIn++; /* Jump the dollar sign */` |
|       - | 3995 | `		/* Copy argument name */` |
|   61078 | 3996 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   61078 | 3997 | `		if( zDup == 0 ){` |
|     ! 0 | 3998 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 3999 | `			return SXERR_ABORT;` |
|       - | 4000 | `		}` |
|   61078 | 4001 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   61078 | 4002 | `		pIn++;` |
|   61078 | 4003 | `		if( pIn < pEnd ){` |
|   38072 | 4004 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4005 | `				SyToken *pDefend;` |
|   32590 | 4006 | `				sxi32 iNest = 0;` |
|   32590 | 4007 | `				pIn++; /* Jump the equal sign */` |
|   32590 | 4008 | `				pDefend = pIn;` |
|       - | 4009 | `				/* Process the default value associated with this argument */` |
|   70188 | 4010 | `				while( pDefend < pEnd ){` |
|   57648 | 4011 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   20050 | 4012 | `						break;` |
|       - | 4013 | `					}` |
|   37600 | 4014 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4015 | `						/* Increment nesting level */` |
|    2508 | 4016 | `						iNest++;` |
|   36347 | 4017 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4018 | `						/* Decrement nesting level */` |
|    2508 | 4019 | `						iNest--;` |
|    1253 | 4020 | `					}` |
|   37600 | 4021 | `					pDefend++;` |
|       2 | 4022 | `				}` |
|   32590 | 4023 | `				if( pIn >= pDefend ){` |
|       3 | 4024 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4025 | `					return rc;` |
|       - | 4026 | `				}` |
|       - | 4027 | `				/* Process default value */` |
|   32588 | 4028 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   32588 | 4029 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4030 | `					return rc;` |
|       - | 4031 | `				}` |
|       - | 4032 | `				/* Point beyond the default value */` |
|   32588 | 4033 | `				pIn = pDefend;` |
|   16293 | 4034 | `			}` |
|   38070 | 4035 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4036 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4037 | `				return rc;` |
|       - | 4038 | `			}` |
|   38070 | 4039 | `			pIn++; /* Jump the trailing comma */` |
|   19034 | 4040 | `		}` |
|       - | 4041 | `		/* Append argument signature */` |
|   61076 | 4042 | `		if( sArg.nType > 0 ){` |
|   50122 | 4043 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4044 | `				/* Class name */` |
|    5014 | 4045 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2508 | 4046 | `			}else{` |
|       - | 4047 | `				int c;` |
|   45110 | 4048 | `				c = 'n'; /* cc warning */` |
|       - | 4049 | `				/* Type leading character */` |
|   45110 | 4050 | `				switch(sArg.nType){` |
|     ! 0 | 4051 | `				case MEMOBJ_HASHMAP:` |
|       - | 4052 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4053 | `					c = 'h';` |
|     ! 0 | 4054 | `					break;` |
|    6265 | 4055 | `				case MEMOBJ_INT:` |
|       - | 4056 | `					/* Integer */` |
|   12532 | 4057 | `					c = 'i';` |
|   12532 | 4058 | `					break;` |
|     ! 0 | 4059 | `				case MEMOBJ_BOOL:` |
|       - | 4060 | `					/* Bool */` |
|     ! 0 | 4061 | `					c = 'b';` |
|     ! 0 | 4062 | `					break;` |
|     ! 0 | 4063 | `				case MEMOBJ_REAL:` |
|       - | 4064 | `					/* Float */` |
|     ! 0 | 4065 | `					c = 'f';` |
|     ! 0 | 4066 | `					break;` |
|   16289 | 4067 | `				case MEMOBJ_STRING:` |
|       - | 4068 | `					/* String */` |
|   32580 | 4069 | `					c = 's';` |
|   32578 | 4070 | `					break;` |
|     ! 0 | 4071 | `				default:` |
|     ! 0 | 4072 | `					break;` |
|       - | 4073 | `				}` |
|   45110 | 4074 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4075 | `			}` |
|   25062 | 4076 | `		}else{` |
|       - | 4077 | `			/* No type is associated with this parameter which mean` |
|       - | 4078 | `			 * that this function is not condidate for overloading.` |
|       - | 4079 | `			 */` |
|   10956 | 4080 | `			SyBlobRelease(&sSig);` |
|       - | 4081 | `		}` |
|       - | 4082 | `		/* Save in the argument set */` |
|   61076 | 4083 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4084 | `	}` |
|   35548 | 4085 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4086 | `		/* Save function signature */` |
|   30074 | 4087 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15036 | 4088 | `	}` |
|   35548 | 4089 | `	return SXRET_OK;` |
|   17776 | 4090 |  |
|       - | 4091 | `/*` |
|       - | 4092 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4093 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4094 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4095 | ` */` |
|   86076 | 4096 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4097 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4098 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4099 | `	)` |
|       2 | 4100 |  |
|       - | 4101 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4102 | `	GenBlock *pBlock;` |
|       - | 4103 | `	sxu32 nGotoOfft;` |
|       - | 4104 | `	sxi32 rc;` |
|       - | 4105 | `	/* Attach the new function */` |
|   86078 | 4106 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   86078 | 4107 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4109 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4110 | `		return SXERR_ABORT;` |
|       - | 4111 | `	}` |
|   86078 | 4112 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4113 | `	/* Swap bytecode containers */` |
|   86078 | 4114 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   86078 | 4115 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4116 | `	/* Compile the body */` |
|   86078 | 4117 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4118 | `	/* Fix exception jumps now the destination is resolved */` |
|   86078 | 4119 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4120 | `	/* Emit the final return if not yet done */` |
|   86078 | 4121 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4122 | `	/* Fix gotos jumps now the destination is resolved */` |
|   86078 | 4123 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4124 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4125 | `	}` |
|   86078 | 4126 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4127 | `	/* Restore the default container */` |
|   86078 | 4128 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4129 | `	/* Leave function block */` |
|   86078 | 4130 | `	GenStateLeaveBlock(&(*pGen),0);` |
|   86078 | 4131 | `	if( rc == SXERR_ABORT ){` |
|       - | 4132 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4133 | `		return SXERR_ABORT;` |
|       - | 4134 | `	}` |
|       - | 4135 | `	/* All done, function body compiled */` |
|   86078 | 4136 | `	return SXRET_OK;` |
|   43040 | 4137 |  |
|       - | 4138 | `/*` |
|       - | 4139 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4140 | ` * According to the PHP language reference manual.` |
|       - | 4141 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4142 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4143 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4144 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4145 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4146 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4147 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4148 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4149 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4150 | ` *` |
|       - | 4151 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4152 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4153 | ` * on these extension.` |
|       - | 4154 | ` */` |
|   33082 | 4155 | `static sxi32 GenStateCompileFunc(` |
|       - | 4156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4157 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4158 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4159 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4160 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4161 | `	)` |
|       2 | 4162 |  |
|       - | 4163 | `	ph7_vm_func *pFunc;` |
|       - | 4164 | `	SyToken *pEnd;` |
|       - | 4165 | `	sxu32 nLine;` |
|       - | 4166 | `	char *zName;` |
|       - | 4167 | `	sxi32 rc;` |
|       - | 4168 | `	/* Extract line number */` |
|   33084 | 4169 | `	nLine = pGen->pIn->nLine;` |
|       - | 4170 | `	/* Jump the left parenthesis '(' */` |
|   33084 | 4171 | `	pGen->pIn++;` |
|       - | 4172 | `	/* Delimit the function signature */` |
|   33084 | 4173 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   33084 | 4174 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4175 | `		/* Syntax error */` |
|       7 | 4176 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4177 | `		if( rc == SXERR_ABORT ){` |
|       - | 4178 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4179 | `			return SXERR_ABORT;` |
|       - | 4180 | `		}` |
|       7 | 4181 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4182 | `		return SXRET_OK;` |
|       - | 4183 | `	}` |
|       - | 4184 | `	/* Create the function state */` |
|   33078 | 4185 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   33078 | 4186 | `	if( pFunc == 0 ){` |
|     ! 0 | 4187 | `		goto OutOfMem;` |
|       - | 4188 | `	}` |
|       - | 4189 | `	/* Build the function name, prepending namespace if active */` |
|   33082 | 4190 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4191 | `		SyBlob sFQN;` |
|       - | 4192 | `		sxu32 nLen;` |
|       9 | 4193 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4194 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4195 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4196 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4197 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4198 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4199 | `		SyBlobRelease(&sFQN);` |
|       9 | 4200 | `		if( zName == 0 ){` |
|     ! 0 | 4201 | `			goto OutOfMem;` |
|       - | 4202 | `		}` |
|       9 | 4203 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4204 | `	}else{` |
|   33070 | 4205 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   33070 | 4206 | `		if( zName == 0 ){` |
|     ! 0 | 4207 | `			goto OutOfMem;` |
|       - | 4208 | `		}` |
|   33070 | 4209 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4210 | `	}` |
|   33078 | 4211 | `	if( pGen->pIn < pEnd ){` |
|       - | 4212 | `		/* Collect function arguments */` |
|   22948 | 4213 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   22948 | 4214 | `		if( rc == SXERR_ABORT ){` |
|       - | 4215 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4216 | `			return SXERR_ABORT;` |
|       - | 4217 | `		}` |
|   11473 | 4218 | `	}` |
|       - | 4219 | `	/* Compile function body */` |
|   33078 | 4220 | `	pGen->pIn = &pEnd[1];` |
|   33078 | 4221 | `	if( bHandleClosure ){` |
|       - | 4222 | `		ph7_vm_func_closure_env sEnv;` |
|     130 | 4223 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     128 | 4224 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      70 | 4225 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      10 | 4226 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4227 | `				/* Closure,record environment variable */` |
|      10 | 4228 | `				pGen->pIn++;` |
|      10 | 4229 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4230 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4231 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4232 | `						return SXERR_ABORT;` |
|       - | 4233 | `					}` |
|     ! 0 | 4234 | `				}` |
|      10 | 4235 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4236 | `				/* Compile until we hit the first closing parenthesis */` |
|      18 | 4237 | `				while( pGen->pIn < pGen->pEnd ){` |
|      18 | 4238 | `					int iFlagsLocal = 0;` |
|      18 | 4239 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      10 | 4240 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      10 | 4241 | `						break;` |
|       - | 4242 | `					}` |
|      10 | 4243 | `					nLineLocal = pGen->pIn->nLine;` |
|      10 | 4244 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4245 | `						/* Pass by reference,record that */` |
|     ! 0 | 4246 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4247 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4248 | `							);` |
|     ! 0 | 4249 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4250 | `						pGen->pIn++;` |
|     ! 0 | 4251 | `					}` |
|       8 | 4252 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      10 | 4253 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4254 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4255 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4256 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4257 | `								return SXERR_ABORT;` |
|       - | 4258 | `							}` |
|       - | 4259 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4260 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4261 | `								pGen->pIn++;` |
|     ! 0 | 4262 | `							}` |
|     ! 0 | 4263 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4264 | `								pGen->pIn++;` |
|     ! 0 | 4265 | `							}` |
|     ! 0 | 4266 | `							break;` |
|       - | 4267 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4268 | `					}else{` |
|       - | 4269 | `						SyString *pNameLocal;` |
|       - | 4270 | `						char *zDup;` |
|       - | 4271 | `						/* Duplicate variable name */` |
|      10 | 4272 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      10 | 4273 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      10 | 4274 | `						if( zDup ){` |
|       - | 4275 | `							/* Zero the structure */` |
|      10 | 4276 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4277 | `							sEnv.iFlags = iFlagsLocal;` |
|      10 | 4278 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4279 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      10 | 4280 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4281 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4282 | `									got_this = 1;` |
|     ! 0 | 4283 | `							}` |
|       - | 4284 | `							/* Save imported variable */` |
|      10 | 4285 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4286 | `						}else{` |
|     ! 0 | 4287 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4288 | `							 return SXERR_ABORT;` |
|       - | 4289 | `						}` |
|       - | 4290 | `					}` |
|      10 | 4291 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      10 | 4292 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4293 | `						/* Ignore trailing commas */` |
|     ! 0 | 4294 | `						pGen->pIn++;` |
|     ! 0 | 4295 | `					}` |
|       2 | 4296 | `				}` |
|      10 | 4297 | `				if( !got_this ){` |
|       - | 4298 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4299 | `					 * available to the closure environment.` |
|       - | 4300 | `					 */` |
|      10 | 4301 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      10 | 4302 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      10 | 4303 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      10 | 4304 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      10 | 4305 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       4 | 4306 | `				}` |
|      10 | 4307 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4308 | `					/* Mark as closure */` |
|      10 | 4309 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       4 | 4310 | `				}` |
|       4 | 4311 | `		}` |
|      64 | 4312 | `	}` |
|       - | 4313 | `	/* Compile the body */` |
|   33078 | 4314 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   33078 | 4315 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4316 | `		return SXERR_ABORT;` |
|       - | 4317 | `	}` |
|   33078 | 4318 | `	if( ppFunc ){` |
|     130 | 4319 | `		*ppFunc = pFunc;` |
|      64 | 4320 | `	}` |
|   33078 | 4321 | `	rc = SXRET_OK;` |
|   33078 | 4322 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4323 | `		/* Finally register the function */` |
|   33070 | 4324 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   16534 | 4325 | `	}` |
|   33078 | 4326 | `	if( rc == SXRET_OK ){` |
|   33078 | 4327 | `		return SXRET_OK;` |
|       - | 4328 | `	}` |
|       - | 4329 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4330 | `OutOfMem:` |
|       - | 4331 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4332 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4333 | `	 */` |
|     ! 0 | 4334 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4335 | `	return SXERR_ABORT;` |
|   16543 | 4336 |  |
|       - | 4337 | `/*` |
|       - | 4338 | ` * Compile a standard PHP function.` |
|       - | 4339 | ` *  Refer to the block-comment above for more information.` |
|       - | 4340 | ` */` |
|   32960 | 4341 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4342 |  |
|       - | 4343 | `	SyString *pName;` |
|       - | 4344 | `	sxi32 iFlags;` |
|       - | 4345 | `	sxu32 nLine;` |
|       - | 4346 | `	sxi32 rc;` |
|       - | 4347 |  |
|   32962 | 4348 | `	nLine = pGen->pIn->nLine;` |
|   32962 | 4349 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   32962 | 4350 | `	iFlags = 0;` |
|   32962 | 4351 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4352 | `		/* Return by reference,remember that */` |
|       7 | 4353 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4354 | `		/* Jump the '&' token */` |
|       7 | 4355 | `		pGen->pIn++;` |
|       3 | 4356 | `	}` |
|   32962 | 4357 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4358 | `		/* Invalid function name */` |
|       5 | 4359 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4360 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4361 | `			return SXERR_ABORT;` |
|       - | 4362 | `		}` |
|       - | 4363 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4364 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4365 | `			pGen->pIn++;` |
|       1 | 4366 | `		}` |
|       5 | 4367 | `		return SXRET_OK;` |
|       - | 4368 | `	}` |
|   32958 | 4369 | `	pName = &pGen->pIn->sData;` |
|   32958 | 4370 | `	nLine = pGen->pIn->nLine;` |
|       - | 4371 | `	/* Jump the function name */` |
|   32958 | 4372 | `	pGen->pIn++;` |
|   32958 | 4373 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4374 | `		/* Syntax error */` |
|       3 | 4375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4376 | `		if( rc == SXERR_ABORT ){` |
|       - | 4377 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4378 | `			return SXERR_ABORT;` |
|       - | 4379 | `		}` |
|       - | 4380 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4381 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4382 | `			pGen->pIn++;` |
|     ! 0 | 4383 | `		}` |
|       3 | 4384 | `		return SXRET_OK;` |
|       - | 4385 | `	}` |
|       - | 4386 | `	/* Compile function body */` |
|   32956 | 4387 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   32956 | 4388 | `	return rc;` |
|   16482 | 4389 |  |
|       - | 4390 | `/*` |
|       - | 4391 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4392 | ` * According to the PHP language reference manual` |
|       - | 4393 | ` *  Visibility:` |
|       - | 4394 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4395 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4396 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4397 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4398 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4399 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4400 | ` */` |
|   98370 | 4401 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4402 |  |
|   98372 | 4403 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|      62 | 4404 | `		return PH7_CLASS_PROT_PRIVATE;` |
|   98312 | 4405 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   17572 | 4406 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4407 | `	}` |
|       - | 4408 | `	/* Assume public by default */` |
|   80742 | 4409 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   49187 | 4410 |  |
|       - | 4411 | `/*` |
|       - | 4412 | ` * Compile a class constant.` |
|       - | 4413 | ` * According to the PHP language reference manual` |
|       - | 4414 | ` *  Class Constants` |
|       - | 4415 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4416 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4417 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4418 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4419 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4420 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4421 | ` * Symisc eXtension.` |
|       - | 4422 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4423 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4424 | ` *  Example:` |
|       - | 4425 | ` *   class Test{` |
|       - | 4426 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4427 | ` *   };` |
|       - | 4428 | ` *   var_dump(TEST::MyConst);` |
|       - | 4429 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4430 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4431 | ` */` |
|      10 | 4432 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4433 |  |
|      12 | 4434 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4435 | `	SySet *pInstrContainer;` |
|       - | 4436 | `	ph7_class_attr *pCons;` |
|       - | 4437 | `	SyString *pName;` |
|       - | 4438 | `	sxi32 rc;` |
|       - | 4439 | `	/* Extract visibility level */` |
|      12 | 4440 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4441 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4442 | `loop:` |
|       - | 4443 | `	/* Mark as constant */` |
|      12 | 4444 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4445 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4446 | `		/* Invalid constant name */` |
|     ! 0 | 4447 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4448 | `		if( rc == SXERR_ABORT ){` |
|       - | 4449 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4450 | `			return SXERR_ABORT;` |
|       - | 4451 | `		}` |
|     ! 0 | 4452 | `		goto Synchronize;` |
|       - | 4453 | `	}` |
|       - | 4454 | `	/* Peek constant name */` |
|      12 | 4455 | `	pName = &pGen->pIn->sData;` |
|       - | 4456 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4457 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4458 | `		/* Reserved constant name */` |
|     ! 0 | 4459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4460 | `		if( rc == SXERR_ABORT ){` |
|       - | 4461 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4462 | `			return SXERR_ABORT;` |
|       - | 4463 | `		}` |
|     ! 0 | 4464 | `		goto Synchronize;` |
|       - | 4465 | `	}` |
|       - | 4466 | `	/* Advance the stream cursor */` |
|      12 | 4467 | `	pGen->pIn++;` |
|      12 | 4468 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4469 | `		/* Invalid declaration */` |
|     ! 0 | 4470 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4471 | `		if( rc == SXERR_ABORT ){` |
|       - | 4472 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4473 | `			return SXERR_ABORT;` |
|       - | 4474 | `		}` |
|     ! 0 | 4475 | `		goto Synchronize;` |
|       - | 4476 | `	}` |
|      12 | 4477 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4478 | `	/* Allocate a new class attribute */` |
|      12 | 4479 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4480 | `	if( pCons == 0 ){` |
|     ! 0 | 4481 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4482 | `		return SXERR_ABORT;` |
|       - | 4483 | `	}` |
|       - | 4484 | `	/* Swap bytecode container */` |
|      12 | 4485 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4486 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4487 | `	/* Compile constant value.` |
|       - | 4488 | `	 */` |
|      12 | 4489 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4490 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4491 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4492 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4493 | `			return SXERR_ABORT;` |
|       - | 4494 | `		}` |
|       1 | 4495 | `	}` |
|       - | 4496 | `	/* Emit the done instruction */` |
|      12 | 4497 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4498 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4499 | `	if( rc == SXERR_ABORT ){` |
|       - | 4500 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4501 | `		return SXERR_ABORT;` |
|       - | 4502 | `	}` |
|       - | 4503 | `	/* All done,install the constant */` |
|      12 | 4504 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4505 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4506 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4507 | `		return SXERR_ABORT;` |
|       - | 4508 | `	}` |
|      12 | 4509 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4510 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4511 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4512 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4513 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4514 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4515 | `				pTok--;` |
|     ! 0 | 4516 | `			}` |
|     ! 0 | 4517 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4518 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4519 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4520 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4521 | `				return SXERR_ABORT;` |
|       - | 4522 | `			}` |
|     ! 0 | 4523 | `		}else{` |
|     ! 0 | 4524 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4525 | `				goto loop;` |
|       - | 4526 | `			}` |
|       - | 4527 | `		}` |
|     ! 0 | 4528 | `	}` |
|      12 | 4529 | `	return SXRET_OK;` |
|     ! 0 | 4530 | `Synchronize:` |
|       - | 4531 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4532 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4533 | `		pGen->pIn++;` |
|     ! 0 | 4534 | `	}` |
|     ! 0 | 4535 | `	return SXERR_CORRUPT;` |
|       7 | 4536 |  |
|       - | 4537 | `/*` |
|       - | 4538 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4539 | ` * According to the PHP language reference manual` |
|       - | 4540 | ` *  Properties` |
|       - | 4541 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4542 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4543 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4544 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4545 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4546 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4547 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4548 | ` * Symisc eXtension.` |
|       - | 4549 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4550 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4551 | ` *  Example:` |
|       - | 4552 | ` *   class Test{` |
|       - | 4553 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4554 | ` *   };` |
|       - | 4555 | ` *   var_dump(TEST::myVar);` |
|       - | 4556 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4557 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4558 | ` */` |
|   25270 | 4559 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4560 |  |
|   25272 | 4561 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4562 | `	ph7_class_attr *pAttr;` |
|       - | 4563 | `	SyString *pName;` |
|       - | 4564 | `	sxi32 rc;` |
|       - | 4565 | `	/* Extract visibility level */` |
|   25272 | 4566 | `	iProtection = GetProtectionLevel(iProtection);` |
|   12635 | 4567 | `loop:` |
|   25272 | 4568 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   25272 | 4569 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4570 | `		/* Invalid attribute name */` |
|     ! 0 | 4571 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4572 | `		if( rc == SXERR_ABORT ){` |
|       - | 4573 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4574 | `			return SXERR_ABORT;` |
|       - | 4575 | `		}` |
|     ! 0 | 4576 | `		goto Synchronize;` |
|       - | 4577 | `	}` |
|       - | 4578 | `	/* Peek attribute name */` |
|   25272 | 4579 | `	pName = &pGen->pIn->sData;` |
|       - | 4580 | `	/* Advance the stream cursor */` |
|   25272 | 4581 | `	pGen->pIn++;` |
|   25272 | 4582 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4583 | `		/* Invalid declaration */` |
|       3 | 4584 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4585 | `		if( rc == SXERR_ABORT ){` |
|       - | 4586 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4587 | `			return SXERR_ABORT;` |
|       - | 4588 | `		}` |
|       3 | 4589 | `		goto Synchronize;` |
|       - | 4590 | `	}` |
|       - | 4591 | `	/* Allocate a new class attribute */` |
|   25270 | 4592 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   25270 | 4593 | `	if( pAttr == 0 ){` |
|     ! 0 | 4594 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4595 | `		return SXERR_ABORT;` |
|       - | 4596 | `	}` |
|   25270 | 4597 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4598 | `		SySet *pInstrContainer;` |
|   10184 | 4599 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4600 | `		/* Swap bytecode container */` |
|   10184 | 4601 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10184 | 4602 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4603 | `		/* Compile attribute value.` |
|       - | 4604 | `		 */` |
|   10184 | 4605 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10184 | 4606 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4607 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4608 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4609 | `				return SXERR_ABORT;` |
|       - | 4610 | `			}` |
|     ! 0 | 4611 | `		}` |
|       - | 4612 | `		/* Emit the done instruction */` |
|   10184 | 4613 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10184 | 4614 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5091 | 4615 | `	}` |
|       - | 4616 | `	/* All done,install the attribute */` |
|   25270 | 4617 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   25270 | 4618 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4619 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4620 | `		return SXERR_ABORT;` |
|       - | 4621 | `	}` |
|   25270 | 4622 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4623 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4624 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4625 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4626 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4627 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4628 | `				pTok--;` |
|     ! 0 | 4629 | `			}` |
|     ! 0 | 4630 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4631 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4632 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4633 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4634 | `				return SXERR_ABORT;` |
|       - | 4635 | `			}` |
|     ! 0 | 4636 | `		}else{` |
|     ! 0 | 4637 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4638 | `				goto loop;` |
|       - | 4639 | `			}` |
|       - | 4640 | `		}` |
|     ! 0 | 4641 | `	}` |
|   25270 | 4642 | `	return SXRET_OK;` |
|       1 | 4643 | `Synchronize:` |
|       - | 4644 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4645 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4646 | `		pGen->pIn++;` |
|       1 | 4647 | `	}` |
|       3 | 4648 | `	return SXERR_CORRUPT;` |
|   12637 | 4649 |  |
|       - | 4650 | `/*` |
|       - | 4651 | ` * Compile a class method.` |
|       - | 4652 | ` *` |
|       - | 4653 | ` * Refer to the official documentation for more information` |
|       - | 4654 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4655 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4656 | ` * overloading and many more.` |
|       - | 4657 | ` */` |
|   73090 | 4658 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4659 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4660 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4661 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4662 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4663 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4664 | `	)` |
|       2 | 4665 |  |
|   73092 | 4666 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4667 | `	ph7_class_method *pMeth;` |
|       - | 4668 | `	sxi32 iFuncFlags;` |
|       - | 4669 | `	SyString *pName;` |
|       - | 4670 | `	SyToken *pEnd;` |
|       - | 4671 | `	sxi32 rc;` |
|       - | 4672 | `	/* Extract visibility level */` |
|   73092 | 4673 | `	iProtection = GetProtectionLevel(iProtection);` |
|   73092 | 4674 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   73092 | 4675 | `	iFuncFlags = 0;` |
|   73092 | 4676 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4677 | `		/* Invalid method name */` |
|     ! 0 | 4678 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4679 | `		if( rc == SXERR_ABORT ){` |
|       - | 4680 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4681 | `			return SXERR_ABORT;` |
|       - | 4682 | `		}` |
|     ! 0 | 4683 | `		goto Synchronize;` |
|       - | 4684 | `	}` |
|   73092 | 4685 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4686 | `		/* Return by reference,remember that */` |
|     ! 0 | 4687 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4688 | `		/* Jump the '&' token */` |
|     ! 0 | 4689 | `		pGen->pIn++;` |
|     ! 0 | 4690 | `	}` |
|   73092 | 4691 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){` |
|       - | 4692 | `		/* Invalid method name */` |
|     ! 0 | 4693 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4694 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4695 | `			return SXERR_ABORT;` |
|       - | 4696 | `		}` |
|     ! 0 | 4697 | `		goto Synchronize;` |
|       - | 4698 | `	}` |
|       - | 4699 | `	/* Peek method name */` |
|   73092 | 4700 | `	pName = &pGen->pIn->sData;` |
|   73092 | 4701 | `	nLine = pGen->pIn->nLine;` |
|       - | 4702 | `	/* Jump the method name */` |
|   73092 | 4703 | `	pGen->pIn++;` |
|   73092 | 4704 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4705 | `		/* Abstract method */` |
|   20090 | 4706 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4707 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4708 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4709 | `				&pClass->sName,pName);` |
|     ! 0 | 4710 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4711 | `				return SXERR_ABORT;` |
|       - | 4712 | `			}` |
|     ! 0 | 4713 | `		}` |
|       - | 4714 | `		/* Assemble method signature only */` |
|   20090 | 4715 | `		doBody = FALSE;` |
|   10044 | 4716 | `	}` |
|   73092 | 4717 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4718 | `		/* Syntax error */` |
|     ! 0 | 4719 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4720 | `		if( rc == SXERR_ABORT ){` |
|       - | 4721 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4722 | `			return SXERR_ABORT;` |
|       - | 4723 | `		}` |
|     ! 0 | 4724 | `		goto Synchronize;` |
|       - | 4725 | `	}` |
|       - | 4726 | `	/* Allocate a new class_method instance */` |
|   73092 | 4727 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|   73092 | 4728 | `	if( pMeth == 0 ){` |
|     ! 0 | 4729 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4730 | `		return SXERR_ABORT;` |
|       - | 4731 | `	}` |
|       - | 4732 | `	/* Jump the left parenthesis '(' */` |
|   73092 | 4733 | `	pGen->pIn++;` |
|   73092 | 4734 | `	pEnd = 0; /* cc warning */` |
|       - | 4735 | `	/* Delimit the method signature */` |
|   73092 | 4736 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   73092 | 4737 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4738 | `		/* Syntax error */` |
|       3 | 4739 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4740 | `		if( rc == SXERR_ABORT ){` |
|       - | 4741 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4742 | `			return SXERR_ABORT;` |
|       - | 4743 | `		}` |
|       3 | 4744 | `		goto Synchronize;` |
|       - | 4745 | `	}` |
|   73090 | 4746 | `	if( pGen->pIn < pEnd ){` |
|       - | 4747 | `		/* Collect method arguments */` |
|   12604 | 4748 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   12604 | 4749 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4750 | `			return SXERR_ABORT;` |
|       - | 4751 | `		}` |
|    6301 | 4752 | `	}` |
|       - | 4753 | `	/* Point beyond method signature */` |
|   73090 | 4754 | `	pGen->pIn = &pEnd[1];` |
|   73090 | 4755 | `	if( doBody ){` |
|       - | 4756 | `		/* Compile method body */` |
|   53002 | 4757 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|   53002 | 4758 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4759 | `			return SXERR_ABORT;` |
|       - | 4760 | `		}` |
|   26502 | 4761 | `	}else{` |
|       - | 4762 | `		/* Only method signature is allowed */` |
|   20090 | 4763 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 4764 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4765 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 4766 | `				if( rc == SXERR_ABORT ){` |
|       - | 4767 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4768 | `					return SXERR_ABORT;` |
|       - | 4769 | `				}` |
|     ! 0 | 4770 | `				return SXERR_CORRUPT;` |
|       - | 4771 | `			}` |
|       - | 4772 | `	}` |
|       - | 4773 | `	/* All done,install the method */` |
|   73090 | 4774 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|   73090 | 4775 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4776 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4777 | `		return SXERR_ABORT;` |
|       - | 4778 | `	}` |
|   73090 | 4779 | `	return SXRET_OK;` |
|       1 | 4780 | `Synchronize:` |
|       - | 4781 | `	/* Synchronize with the first semi-colon */` |
|       7 | 4782 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 4783 | `		pGen->pIn++;` |
|       1 | 4784 | `	}` |
|       3 | 4785 | `	return SXERR_CORRUPT;` |
|   36547 | 4786 |  |
|       - | 4787 | `/*` |
|       - | 4788 | ` * Compile an object interface.` |
|       - | 4789 | ` *  According to the PHP language reference manual` |
|       - | 4790 | ` *   Object Interfaces:` |
|       - | 4791 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 4792 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 4793 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 4794 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 4795 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 4796 | ` */` |
|    7546 | 4797 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 4798 |  |
|    7548 | 4799 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4800 | `	ph7_class *pClass,*pBase;` |
|       - | 4801 | `	SyToken *pEnd,*pTmp;` |
|       - | 4802 | `	SyString *pName;` |
|       - | 4803 | `	sxi32 nKwrd;` |
|       - | 4804 | `	sxi32 rc;` |
|       - | 4805 | `	/* Jump the 'interface' keyword */` |
|    7548 | 4806 | `	pGen->pIn++;` |
|       - | 4807 | `	/* Extract interface name */` |
|    7548 | 4808 | `	pName = &pGen->pIn->sData;` |
|       - | 4809 | `	/* Advance the stream cursor */` |
|    7548 | 4810 | `	pGen->pIn++;` |
|       - | 4811 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 4812 | `		SyBlob sFQN;` |
|       - | 4813 | `		SyString sFQNStr;` |
|    7548 | 4814 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7548 | 4815 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7548 | 4816 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7548 | 4817 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7548 | 4818 | `		SyBlobRelease(&sFQN);` |
|       - | 4819 | `	}` |
|    7548 | 4820 | `	if( pClass == 0 ){` |
|     ! 0 | 4821 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4822 | `		return SXERR_ABORT;` |
|       - | 4823 | `	}` |
|       - | 4824 | `	/* Mark as an interface */` |
|    7548 | 4825 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 4826 | `	/* Assume no base class is given */` |
|    7548 | 4827 | `	pBase = 0;` |
|    7548 | 4828 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 4829 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 4830 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 4831 | `			SyString *pBaseName;` |
|       - | 4832 | `			/* Extract base interface */` |
|       3 | 4833 | `			pGen->pIn++;` |
|       3 | 4834 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4835 | `				/* Syntax error */` |
|     ! 0 | 4836 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4837 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 4838 | `					pName);` |
|     ! 0 | 4839 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4840 | `				if( rc == SXERR_ABORT ){` |
|       - | 4841 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4842 | `					return SXERR_ABORT;` |
|       - | 4843 | `				}` |
|     ! 0 | 4844 | `				return SXRET_OK;` |
|       - | 4845 | `			}` |
|       3 | 4846 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 4847 | `			{` |
|       - | 4848 | `				SyBlob sResolved;` |
|       3 | 4849 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 4850 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 4851 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 4852 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 4853 | `				SyBlobRelease(&sResolved);` |
|       - | 4854 | `			}` |
|       - | 4855 | `			/* Only interfaces is allowed */` |
|       3 | 4856 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 4857 | `				pBase = pBase->pNextName;` |
|     ! 0 | 4858 | `			}` |
|       3 | 4859 | `			if( pBase == 0 ){` |
|       - | 4860 | `				/* Inexistant interface */` |
|     ! 0 | 4861 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 4862 | `				if( rc == SXERR_ABORT ){` |
|       - | 4863 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4864 | `					return SXERR_ABORT;` |
|       - | 4865 | `				}` |
|     ! 0 | 4866 | `			}` |
|       - | 4867 | `			/* Advance the stream cursor */` |
|       3 | 4868 | `			pGen->pIn++;` |
|       1 | 4869 | `		}` |
|       1 | 4870 | `	}` |
|    7548 | 4871 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 4872 | `		/* Syntax error */` |
|     ! 0 | 4873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 4874 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4875 | `		if( rc == SXERR_ABORT ){` |
|       - | 4876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4877 | `			return SXERR_ABORT;` |
|       - | 4878 | `		}` |
|     ! 0 | 4879 | `		return SXRET_OK;` |
|       - | 4880 | `	}` |
|    7548 | 4881 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7548 | 4882 | `	pEnd = 0; /* cc warning */` |
|       - | 4883 | `	/* Delimit the interface body */` |
|    7548 | 4884 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7548 | 4885 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4886 | `		/* Syntax error */` |
|     ! 0 | 4887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 4888 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 4889 | `		if( rc == SXERR_ABORT ){` |
|       - | 4890 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4891 | `			return SXERR_ABORT;` |
|       - | 4892 | `		}` |
|     ! 0 | 4893 | `		return SXRET_OK;` |
|       - | 4894 | `	}` |
|       - | 4895 | `	/* Swap token stream */` |
|    7548 | 4896 | `	pTmp = pGen->pEnd;` |
|    7548 | 4897 | `	pGen->pEnd = pEnd;` |
|       - | 4898 | `	/* Start the parse process` |
|       - | 4899 | `	 * Note (According to the PHP reference manual):` |
|       - | 4900 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 4901 | `	 *  Only 'public' visibility is allowed.` |
|       - | 4902 | `	 */` |
|   13813 | 4903 | `	for(;;){` |
|       - | 4904 | `		/* Jump leading/trailing semi-colons */` |
|   47708 | 4905 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   20082 | 4906 | `			pGen->pIn++;` |
|       2 | 4907 | `		}` |
|   27628 | 4908 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4909 | `			/* End of interface body */` |
|    7548 | 4910 | `			break;` |
|       - | 4911 | `		}` |
|   20082 | 4912 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4913 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4914 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 4915 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 4916 | `			if( rc == SXERR_ABORT ){` |
|       - | 4917 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4918 | `				return SXERR_ABORT;` |
|       - | 4919 | `			}` |
|     ! 0 | 4920 | `			goto done;` |
|       - | 4921 | `		}` |
|       - | 4922 | `		/* Extract the current keyword */` |
|   20082 | 4923 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20082 | 4924 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 4925 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 4926 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 4927 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 4928 | `		}` |
|   20082 | 4929 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4930 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4931 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4932 | `			if( rc == SXERR_ABORT ){` |
|       - | 4933 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4934 | `				return SXERR_ABORT;` |
|       - | 4935 | `			}` |
|     ! 0 | 4936 | `			goto done;` |
|       - | 4937 | `		}` |
|   20082 | 4938 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 4939 | `			/* Advance the stream cursor */` |
|   20078 | 4940 | `			pGen->pIn++;` |
|   20078 | 4941 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 4942 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4943 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4944 | `				if( rc == SXERR_ABORT ){` |
|       - | 4945 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4946 | `					return SXERR_ABORT;` |
|       - | 4947 | `				}` |
|     ! 0 | 4948 | `				goto done;` |
|       - | 4949 | `			}` |
|   20078 | 4950 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   20078 | 4951 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 4952 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4953 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 4954 | `				if( rc == SXERR_ABORT ){` |
|       - | 4955 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 4956 | `					return SXERR_ABORT;` |
|       - | 4957 | `				}` |
|     ! 0 | 4958 | `				goto done;` |
|       - | 4959 | `			}` |
|   10038 | 4960 | `		}` |
|   20082 | 4961 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 4962 | `			/* Parse constant */` |
|       3 | 4963 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 4964 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4965 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4966 | `					return SXERR_ABORT;` |
|       - | 4967 | `				}` |
|     ! 0 | 4968 | `				goto done;` |
|       - | 4969 | `			}` |
|       2 | 4970 | `		}else{` |
|   20080 | 4971 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   20080 | 4972 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 4973 | `				/* Static method,record that */` |
|     ! 0 | 4974 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 4975 | `				/* Advance the stream cursor */` |
|     ! 0 | 4976 | `				pGen->pIn++;` |
|     ! 0 | 4977 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 4978 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 4979 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 4980 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 4981 | `						if( rc == SXERR_ABORT ){` |
|       - | 4982 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 4983 | `							return SXERR_ABORT;` |
|       - | 4984 | `						}` |
|     ! 0 | 4985 | `						goto done;` |
|       - | 4986 | `				}` |
|     ! 0 | 4987 | `			}` |
|       - | 4988 | `			/* Process method signature (no body for interface methods) */` |
|   20080 | 4989 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   20080 | 4990 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 4991 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4992 | `					return SXERR_ABORT;` |
|       - | 4993 | `				}` |
|     ! 0 | 4994 | `				goto done;` |
|       - | 4995 | `			}` |
|       - | 4996 | `		}` |
|       2 | 4997 | `	}` |
|       - | 4998 | `	/* Install the interface */` |
|    7548 | 4999 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7548 | 5000 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5001 | `		/* Inherit from the base interface */` |
|       3 | 5002 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5003 | `	}` |
|    7548 | 5004 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5005 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5006 | `		return SXERR_ABORT;` |
|       - | 5007 | `	}` |
|    3773 | 5008 | `done:` |
|       - | 5009 | `	/* Point beyond the interface body */` |
|    7548 | 5010 | `	pGen->pIn  = &pEnd[1];` |
|    7548 | 5011 | `	pGen->pEnd = pTmp;` |
|    7548 | 5012 | `	return PH7_OK;` |
|    3775 | 5013 |  |
|       - | 5014 | `/*` |
|       - | 5015 | ` * Compile a user-defined class.` |
|       - | 5016 | ` * According to the PHP language reference manual` |
|       - | 5017 | ` *  class` |
|       - | 5018 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5019 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5020 | ` *  of the properties and methods belonging to the class.` |
|       - | 5021 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5022 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5023 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5024 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5025 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5026 | ` *  (called "methods").` |
|       - | 5027 | ` */` |
|       - | 5028 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5029 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5030 | `struct TraitUseEntry {` |
|       - | 5031 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5032 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5033 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5034 | `};` |
|       - | 5035 | `/*` |
|       - | 5036 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5037 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5038 | ` */` |
|   22918 | 5039 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5040 |  |
|       - | 5041 | `	ph7_class **apIface;` |
|       - | 5042 | `	sxu32 nIface,i;` |
|       - | 5043 | `	sxi32 rc;` |
|   22920 | 5044 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5045 | `		return SXRET_OK;` |
|       - | 5046 | `	}` |
|   22920 | 5047 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   22920 | 5048 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   22952 | 5049 | `	for(i = 0; i < nIface; i++){` |
|      34 | 5050 | `		ph7_class *pIface = apIface[i];` |
|       - | 5051 | `		SyHashEntry *pEntry;` |
|      34 | 5052 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|     106 | 5053 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|      74 | 5054 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5055 | `			ph7_class_method *pImplMeth;` |
|      74 | 5056 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5057 | `			/* Find the implementing method in the class */` |
|      74 | 5058 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|      74 | 5059 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5060 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5061 | `			}` |
|       - | 5062 | `			/* Check visibility: interface methods must be implemented as public */` |
|      60 | 5063 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5064 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5065 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5066 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5067 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5068 | `					return SXERR_ABORT;` |
|       - | 5069 | `				}` |
|       1 | 5070 | `			}` |
|       - | 5071 | `			/* Check parameter count: implementation must accept at least as many parameters */` |
|       - | 5072 | `			{` |
|      60 | 5073 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|      60 | 5074 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|      60 | 5075 | `				if( nImplArgs < nIfaceArgs ){` |
|       - | 5076 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5077 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5078 | `					sxu32 j;` |
|       3 | 5079 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       3 | 5080 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5081 | `					/* Build implementing method signature */` |
|       3 | 5082 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       5 | 5083 | `					for(j = 0; j < nImplArgs; j++){` |
|       3 | 5084 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       3 | 5085 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       3 | 5086 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       2 | 5087 | `					}` |
|       - | 5088 | `					/* Build interface method signature */` |
|       3 | 5089 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|       7 | 5090 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       5 | 5091 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       5 | 5092 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       5 | 5093 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       3 | 5094 | `					}` |
|       4 | 5095 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5096 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       1 | 5097 | `						&pClass->sName,pMName,` |
|       2 | 5098 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       1 | 5099 | `						&pIface->sName,pMName,` |
|       2 | 5100 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       3 | 5101 | `					SyBlobRelease(&sImplSig);` |
|       3 | 5102 | `					SyBlobRelease(&sIfaceSig);` |
|       3 | 5103 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5104 | `						return SXERR_ABORT;` |
|       - | 5105 | `					}` |
|       1 | 5106 | `				}` |
|       - | 5107 | `			}` |
|       2 | 5108 | `		}` |
|      18 | 5109 | `	}` |
|   22920 | 5110 | `	return SXRET_OK;` |
|   11461 | 5111 |  |
|       - | 5112 | `/*` |
|       - | 5113 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5114 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5115 | ` */` |
|   22918 | 5116 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5117 |  |
|       - | 5118 | `	ph7_class_method *pMeth;` |
|       - | 5119 | `	SyHashEntry *pEntry;` |
|       - | 5120 | `	sxu32 nAbstract;` |
|       - | 5121 | `	SyBlob sMsg;` |
|       - | 5122 | `	sxi32 rc;` |
|       - | 5123 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   22920 | 5124 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      18 | 5125 | `		return SXRET_OK;` |
|       - | 5126 | `	}` |
|       - | 5127 | `	/* Count abstract methods */` |
|   22904 | 5128 | `	nAbstract = 0;` |
|   22904 | 5129 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  208794 | 5130 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  185892 | 5131 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  185892 | 5132 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5133 | `			nAbstract++;` |
|       8 | 5134 | `		}` |
|       2 | 5135 | `	}` |
|   22904 | 5136 | `	if( nAbstract == 0 ){` |
|   22890 | 5137 | `		return SXRET_OK;` |
|       - | 5138 | `	}` |
|       - | 5139 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5140 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5141 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5142 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5143 | `		&pClass->sName,nAbstract,` |
|       7 | 5144 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5145 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5146 | `	/* Second pass: list methods with origins */` |
|       - | 5147 | `	{` |
|      15 | 5148 | `		sxu32 nListed = 0;` |
|      15 | 5149 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5150 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5151 | `			ph7_class *pOrigin = 0;` |
|       - | 5152 | `			SyString *pMName;` |
|      19 | 5153 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5154 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5155 | `				continue;` |
|       - | 5156 | `			}` |
|      17 | 5157 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5158 | `			if( nListed > 0 ){` |
|       3 | 5159 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5160 | `			}` |
|       - | 5161 | `			/* Find the origin of this abstract method.` |
|       - | 5162 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5163 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5164 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5165 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5166 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5167 | `			 * class's namespace.` |
|       - | 5168 | `			 */` |
|       - | 5169 | `			{` |
|       - | 5170 | `				ph7_class **apIface;` |
|       - | 5171 | `				ph7_class **apTrait;` |
|       - | 5172 | `				ph7_class *pWalk;` |
|       - | 5173 | `				sxu32 i;` |
|       - | 5174 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5175 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5176 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5177 | `				 */` |
|      17 | 5178 | `				if( pClass->pBase ){` |
|       9 | 5179 | `					pWalk = pClass->pBase;` |
|      17 | 5180 | `					while( pWalk ){` |
|       - | 5181 | `						ph7_class_method *pParentMeth;` |
|      11 | 5182 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5183 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5184 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5185 | `							 * in this class's ancestor chain.` |
|       - | 5186 | `							 */` |
|      11 | 5187 | `							int fromIface = 0;` |
|      11 | 5188 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5189 | `							while( pAnc ){` |
|       - | 5190 | `								ph7_class **apPI;` |
|       - | 5191 | `								sxu32 j;` |
|      13 | 5192 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5193 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5194 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5195 | `										fromIface = 1;` |
|       9 | 5196 | `										break;` |
|       - | 5197 | `									}` |
|     ! 0 | 5198 | `								}` |
|      13 | 5199 | `								if( fromIface ) break;` |
|       5 | 5200 | `								pAnc = pAnc->pBase;` |
|       1 | 5201 | `							}` |
|      11 | 5202 | `							if( !fromIface ){` |
|       3 | 5203 | `								pOrigin = pWalk;` |
|       3 | 5204 | `								break;` |
|       - | 5205 | `							}` |
|       4 | 5206 | `						}` |
|       9 | 5207 | `						pWalk = pWalk->pBase;` |
|       1 | 5208 | `					}` |
|       4 | 5209 | `				}` |
|       - | 5210 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5211 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5212 | `				 */` |
|      17 | 5213 | `				if( !pOrigin ){` |
|      15 | 5214 | `					pWalk = pClass;` |
|      37 | 5215 | `					while( pWalk && !pOrigin ){` |
|      23 | 5216 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5217 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5218 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5219 | `							ph7_class *pDeepest = 0;` |
|      25 | 5220 | `							while( pIface ){` |
|      13 | 5221 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5222 | `									pDeepest = pIface;` |
|       6 | 5223 | `								}` |
|      13 | 5224 | `								pIface = pIface->pBase;` |
|       1 | 5225 | `							}` |
|      13 | 5226 | `							if( pDeepest ){` |
|      13 | 5227 | `								pOrigin = pDeepest;` |
|      13 | 5228 | `								break;` |
|       - | 5229 | `							}` |
|     ! 0 | 5230 | `						}` |
|      23 | 5231 | `						pWalk = pWalk->pBase;` |
|       1 | 5232 | `					}` |
|       7 | 5233 | `				}` |
|       - | 5234 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5235 | `				if( !pOrigin ){` |
|       3 | 5236 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5237 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5238 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5239 | `							pOrigin = pClass;` |
|       3 | 5240 | `							break;` |
|       - | 5241 | `						}` |
|     ! 0 | 5242 | `					}` |
|       1 | 5243 | `				}` |
|       - | 5244 | `			}` |
|      17 | 5245 | `			if( pOrigin ){` |
|      17 | 5246 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5247 | `			}else{` |
|       - | 5248 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5249 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5250 | `			}` |
|      17 | 5251 | `			nListed++;` |
|       1 | 5252 | `		}` |
|       - | 5253 | `	}` |
|      15 | 5254 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5255 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5256 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5257 | `	SyBlobRelease(&sMsg);` |
|      15 | 5258 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5259 | `		return SXERR_ABORT;` |
|       - | 5260 | `	}` |
|      15 | 5261 | `	return SXRET_OK;` |
|   11461 | 5262 |  |
|   22922 | 5263 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5264 |  |
|   22924 | 5265 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5266 | `	ph7_class *pClass,*pBase;` |
|       - | 5267 | `	SyToken *pEnd,*pTmp;` |
|       - | 5268 | `	sxi32 iProtection;` |
|       - | 5269 | `	SySet aInterfaces;` |
|       - | 5270 | `	SySet aUseEntries;` |
|       - | 5271 | `	sxi32 iAttrflags;` |
|       - | 5272 | `	SyString *pName;` |
|       - | 5273 | `	sxi32 nKwrd;` |
|       - | 5274 | `	sxi32 rc;` |
|       - | 5275 | `	/* Jump the 'class' keyword */` |
|   22924 | 5276 | `	pGen->pIn++;` |
|   22924 | 5277 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5278 | `		/* Syntax error */` |
|     ! 0 | 5279 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5280 | `		if( rc == SXERR_ABORT ){` |
|       - | 5281 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5282 | `			return SXERR_ABORT;` |
|       - | 5283 | `		}` |
|       - | 5284 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5285 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5286 | `			pGen->pIn++;` |
|     ! 0 | 5287 | `		}` |
|     ! 0 | 5288 | `		return SXRET_OK;` |
|       - | 5289 | `	}` |
|       - | 5290 | `	/* Extract class name */` |
|   22924 | 5291 | `	pName = &pGen->pIn->sData;` |
|       - | 5292 | `	/* Advance the stream cursor */` |
|   22924 | 5293 | `	pGen->pIn++;` |
|       - | 5294 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5295 | `		SyBlob sFQN;` |
|       - | 5296 | `		SyString sFQNStr;` |
|   22924 | 5297 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   22924 | 5298 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   22924 | 5299 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   22924 | 5300 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   22924 | 5301 | `		SyBlobRelease(&sFQN);` |
|       - | 5302 | `	}` |
|   22924 | 5303 | `	if( pClass == 0 ){` |
|     ! 0 | 5304 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5305 | `		return SXERR_ABORT;` |
|       - | 5306 | `	}` |
|       - | 5307 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   22924 | 5308 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   22924 | 5309 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5310 | `	/* Assume a standalone class */` |
|   22924 | 5311 | `	pBase = 0;` |
|   22924 | 5312 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5313 | `		SyString *pBaseName;` |
|   15128 | 5314 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   15128 | 5315 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   15098 | 5316 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   15098 | 5317 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5318 | `				/* Syntax error */` |
|     ! 0 | 5319 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5320 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5321 | `					pName);` |
|     ! 0 | 5322 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5323 | `				if( rc == SXERR_ABORT ){` |
|       - | 5324 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5325 | `					return SXERR_ABORT;` |
|       - | 5326 | `				}` |
|     ! 0 | 5327 | `				return SXRET_OK;` |
|       - | 5328 | `			}` |
|       - | 5329 | `			/* Extract base class name and resolve through namespace/imports */` |
|   15098 | 5330 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5331 | `			{` |
|       - | 5332 | `				SyBlob sResolved;` |
|   15098 | 5333 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   15098 | 5334 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   22646 | 5335 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   15096 | 5336 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   15098 | 5337 | `				SyBlobRelease(&sResolved);` |
|       - | 5338 | `			}` |
|       - | 5339 | `			/* Interfaces are not allowed */` |
|   15098 | 5340 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5341 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5342 | `			}` |
|   15098 | 5343 | `			if( pBase == 0 ){` |
|       - | 5344 | `				/* Inexistant base class */` |
|     ! 0 | 5345 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5346 | `				if( rc == SXERR_ABORT ){` |
|       - | 5347 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5348 | `					return SXERR_ABORT;` |
|       - | 5349 | `				}` |
|     ! 0 | 5350 | `			}else{` |
|   15098 | 5351 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5352 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5353 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5354 | `					if( rc == SXERR_ABORT ){` |
|       - | 5355 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5356 | `						return SXERR_ABORT;` |
|       - | 5357 | `					}` |
|     ! 0 | 5358 | `				}` |
|       - | 5359 | `			}` |
|       - | 5360 | `			/* Advance the stream cursor */` |
|   15098 | 5361 | `			pGen->pIn++;` |
|    7548 | 5362 | `		}` |
|   15128 | 5363 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5364 | `			ph7_class *pInterface;` |
|       - | 5365 | `			SyString *pIntName;` |
|       - | 5366 | `			/* Interface implementation */` |
|      34 | 5367 | `			pGen->pIn++; /* Advance the stream cursor */` |
|      16 | 5368 | `			for(;;){` |
|      34 | 5369 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5370 | `					/* Syntax error */` |
|     ! 0 | 5371 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5372 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5373 | `						pName);` |
|     ! 0 | 5374 | `					if( rc == SXERR_ABORT ){` |
|       - | 5375 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5376 | `						return SXERR_ABORT;` |
|       - | 5377 | `					}` |
|     ! 0 | 5378 | `					break;` |
|       - | 5379 | `				}` |
|       - | 5380 | `				/* Extract interface name and resolve through namespace/imports */` |
|      34 | 5381 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5382 | `				{` |
|       - | 5383 | `					SyBlob sResolved;` |
|      34 | 5384 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      34 | 5385 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|      66 | 5386 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|      32 | 5387 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      34 | 5388 | `					SyBlobRelease(&sResolved);` |
|       - | 5389 | `				}` |
|       - | 5390 | `				/* Only interfaces are allowed */` |
|      34 | 5391 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5392 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5393 | `				}` |
|      34 | 5394 | `				if( pInterface == 0 ){` |
|       - | 5395 | `					/* Inexistant interface */` |
|     ! 0 | 5396 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5397 | `					if( rc == SXERR_ABORT ){` |
|       - | 5398 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5399 | `						return SXERR_ABORT;` |
|       - | 5400 | `					}` |
|     ! 0 | 5401 | `				}else{` |
|       - | 5402 | `					/* Register interface */` |
|      34 | 5403 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5404 | `				}` |
|       - | 5405 | `				/* Advance the stream cursor */` |
|      34 | 5406 | `				pGen->pIn++;` |
|      34 | 5407 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      18 | 5408 | `					break;` |
|       - | 5409 | `				}` |
|     ! 0 | 5410 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5411 | `			}` |
|      16 | 5412 | `		}` |
|    7563 | 5413 | `	}` |
|   22924 | 5414 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5415 | `		/* Syntax error */` |
|     ! 0 | 5416 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5417 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5418 | `		if( rc == SXERR_ABORT ){` |
|       - | 5419 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5420 | `			return SXERR_ABORT;` |
|       - | 5421 | `		}` |
|     ! 0 | 5422 | `		return SXRET_OK;` |
|       - | 5423 | `	}` |
|   22924 | 5424 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   22924 | 5425 | `	pEnd = 0; /* cc warning */` |
|       - | 5426 | `	/* Delimit the class body */` |
|   22924 | 5427 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   22924 | 5428 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5429 | `		/* Syntax error */` |
|     ! 0 | 5430 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5431 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5432 | `		if( rc == SXERR_ABORT ){` |
|       - | 5433 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5434 | `			return SXERR_ABORT;` |
|       - | 5435 | `		}` |
|     ! 0 | 5436 | `		return SXRET_OK;` |
|       - | 5437 | `	}` |
|       - | 5438 | `	/* Swap token stream */` |
|   22924 | 5439 | `	pTmp = pGen->pEnd;` |
|   22924 | 5440 | `	pGen->pEnd = pEnd;` |
|       - | 5441 | `	/* Set the inherited flags */` |
|   22924 | 5442 | `	pClass->iFlags = iFlags;` |
|       - | 5443 | `	/* Start the parse process */` |
|   37948 | 5444 | `	for(;;){` |
|       - | 5445 | `		/* Jump leading/trailing semi-colons */` |
|  126478 | 5446 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   25298 | 5447 | `			pGen->pIn++;` |
|       2 | 5448 | `		}` |
|  101182 | 5449 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5450 | `			/* End of class body */` |
|   22920 | 5451 | `			break;` |
|       - | 5452 | `		}` |
|   78264 | 5453 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5454 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5455 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5456 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5457 | `			if( rc == SXERR_ABORT ){` |
|       - | 5458 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5459 | `				return SXERR_ABORT;` |
|       - | 5460 | `			}` |
|     ! 0 | 5461 | `			goto done;` |
|       - | 5462 | `		}` |
|       - | 5463 | `		/* Assume public visibility */` |
|   78264 | 5464 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|   78264 | 5465 | `		iAttrflags = 0;` |
|   78264 | 5466 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5467 | `			/* Extract the current keyword */` |
|   78264 | 5468 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   78264 | 5469 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5470 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5471 | `				TraitUseEntry sUse;` |
|      35 | 5472 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      35 | 5473 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      35 | 5474 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      25 | 5475 | `				for(;;){` |
|       - | 5476 | `					ph7_class *pTrait;` |
|       - | 5477 | `					SyString *pTraitName;` |
|      43 | 5478 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5479 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5480 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5481 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5482 | `							return SXERR_ABORT;` |
|       - | 5483 | `						}` |
|     ! 0 | 5484 | `						break;` |
|       - | 5485 | `					}` |
|      43 | 5486 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5487 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5488 | `						SyBlob sResolved;` |
|      43 | 5489 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      43 | 5490 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      85 | 5491 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      42 | 5492 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      43 | 5493 | `						SyBlobRelease(&sResolved);` |
|       - | 5494 | `					}` |
|       - | 5495 | `					/* Only traits are allowed */` |
|      43 | 5496 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5497 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5498 | `					}` |
|      43 | 5499 | `					if( pTrait == 0 ){` |
|     ! 0 | 5500 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5501 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5502 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5503 | `							return SXERR_ABORT;` |
|       - | 5504 | `						}` |
|     ! 0 | 5505 | `					}else{` |
|      43 | 5506 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5507 | `					}` |
|      43 | 5508 | `					pGen->pIn++; /* Advance past trait name */` |
|      43 | 5509 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      18 | 5510 | `						break;` |
|       - | 5511 | `					}` |
|       9 | 5512 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5513 | `				}` |
|       - | 5514 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      35 | 5515 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5516 | `					SyToken *pBlock;` |
|       9 | 5517 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5518 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5519 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5520 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5521 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5522 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5523 | `					}else{` |
|     ! 0 | 5524 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5525 | `					}` |
|       4 | 5526 | `				}` |
|      35 | 5527 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5528 | `				/* The semicolon will be consumed by the outer loop */` |
|      35 | 5529 | `				continue;` |
|       - | 5530 | `			}` |
|   78230 | 5531 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|   75630 | 5532 | `				iProtection = nKwrd;` |
|   75630 | 5533 | `				pGen->pIn++; /* Jump the visibility token */` |
|   75630 | 5534 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5535 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5536 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5537 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5538 | `					if( rc == SXERR_ABORT ){` |
|       - | 5539 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5540 | `						return SXERR_ABORT;` |
|       - | 5541 | `					}` |
|     ! 0 | 5542 | `					goto done;` |
|       - | 5543 | `				}` |
|   75630 | 5544 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5545 | `					/* Attribute declaration */` |
|   25250 | 5546 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   25250 | 5547 | `					if( rc != SXRET_OK ){` |
|       3 | 5548 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5549 | `							return SXERR_ABORT;` |
|       - | 5550 | `						}` |
|       3 | 5551 | `						goto done;` |
|       - | 5552 | `					}` |
|   25248 | 5553 | `					continue;` |
|       - | 5554 | `				}` |
|       - | 5555 | `				/* Extract the keyword */` |
|   50382 | 5556 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   25190 | 5557 | `			}` |
|   52982 | 5558 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5559 | `				/* Process constant declaration */` |
|      10 | 5560 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5561 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5562 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5563 | `						return SXERR_ABORT;` |
|       - | 5564 | `					}` |
|     ! 0 | 5565 | `					goto done;` |
|       - | 5566 | `				}` |
|       6 | 5567 | `			}else{` |
|   52974 | 5568 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5569 | `					/* Static method or attribute,record that */` |
|      23 | 5570 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|      23 | 5571 | `					pGen->pIn++; /* Jump the static keyword */` |
|      23 | 5572 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5573 | `						/* Extract the keyword */` |
|      19 | 5574 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      19 | 5575 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5576 | `							iProtection = nKwrd;` |
|     ! 0 | 5577 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5578 | `						}` |
|       9 | 5579 | `					}` |
|      23 | 5580 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5581 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5582 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5583 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5584 | `						if( rc == SXERR_ABORT ){` |
|       - | 5585 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5586 | `							return SXERR_ABORT;` |
|       - | 5587 | `						}` |
|     ! 0 | 5588 | `						goto done;` |
|       - | 5589 | `					}` |
|      23 | 5590 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5591 | `						/* Attribute declaration */` |
|       5 | 5592 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5593 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5594 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5595 | `								return SXERR_ABORT;` |
|       - | 5596 | `							}` |
|     ! 0 | 5597 | `							goto done;` |
|       - | 5598 | `						}` |
|       5 | 5599 | `						continue;` |
|       - | 5600 | `					}` |
|       - | 5601 | `					/* Extract the keyword */` |
|      19 | 5602 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52961 | 5603 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5604 | `					/* Abstract method,record that */` |
|       8 | 5605 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5606 | `					/* Mark the whole class as abstract */` |
|       8 | 5607 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5608 | `					/* Advance the stream cursor */` |
|       8 | 5609 | `					pGen->pIn++;` |
|       8 | 5610 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 | 5611 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 | 5612 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 | 5613 | `							iProtection = nKwrd;` |
|       6 | 5614 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5615 | `						}` |
|       3 | 5616 | `					}` |
|       8 | 5617 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       6 | 5618 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5619 | `							/* Static method */` |
|     ! 0 | 5620 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5621 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5622 | `					}` |
|       8 | 5623 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       6 | 5624 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5625 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5626 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5627 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5628 | `							if( rc == SXERR_ABORT ){` |
|       - | 5629 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5630 | `								return SXERR_ABORT;` |
|       - | 5631 | `							}` |
|     ! 0 | 5632 | `							goto done;` |
|       - | 5633 | `					}` |
|       8 | 5634 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|   52949 | 5635 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5636 | `					/* final method ,record that */` |
|       5 | 5637 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5638 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5639 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5640 | `						/* Extract the keyword */` |
|       5 | 5641 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5642 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5643 | `							iProtection = nKwrd;` |
|       5 | 5644 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5645 | `						}` |
|       2 | 5646 | `					}` |
|       5 | 5647 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5648 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5649 | `							/* Static method */` |
|     ! 0 | 5650 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5651 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5652 | `					}` |
|       5 | 5653 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5654 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5655 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5656 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5657 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5658 | `							if( rc == SXERR_ABORT ){` |
|       - | 5659 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5660 | `								return SXERR_ABORT;` |
|       - | 5661 | `							}` |
|     ! 0 | 5662 | `							goto done;` |
|       - | 5663 | `					}` |
|       5 | 5664 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5665 | `				}` |
|   52970 | 5666 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5667 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5668 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5669 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5670 | `						if( rc == SXERR_ABORT ){` |
|       - | 5671 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5672 | `							return SXERR_ABORT;` |
|       - | 5673 | `						}` |
|     ! 0 | 5674 | `						goto done;` |
|       - | 5675 | `				}` |
|   52970 | 5676 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5677 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5678 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5679 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5680 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5681 | `						if( rc == SXERR_ABORT ){` |
|       - | 5682 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5683 | `							return SXERR_ABORT;` |
|       - | 5684 | `						}` |
|     ! 0 | 5685 | `						goto done;` |
|       - | 5686 | `					}` |
|       - | 5687 | `					/* Attribute declaration */` |
|       7 | 5688 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5689 | `				}else{` |
|       - | 5690 | `					/* Process method declaration */` |
|   52964 | 5691 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5692 | `				}` |
|   52970 | 5693 | `				if( rc != SXRET_OK ){` |
|       3 | 5694 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5695 | `						return SXERR_ABORT;` |
|       - | 5696 | `					}` |
|       3 | 5697 | `					goto done;` |
|       - | 5698 | `				}` |
|       - | 5699 | `			}` |
|   26489 | 5700 | `		}else{` |
|       - | 5701 | `			/* Attribute declaration */` |
|     ! 0 | 5702 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5703 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5704 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5705 | `					return SXERR_ABORT;` |
|       - | 5706 | `				}` |
|     ! 0 | 5707 | `				goto done;` |
|       - | 5708 | `			}` |
|       - | 5709 | `		}` |
|       2 | 5710 | `	}` |
|       - | 5711 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5712 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5713 | `	 */` |
|       - | 5714 | `	{` |
|       - | 5715 | `		TraitUseEntry *apUse;` |
|       - | 5716 | `		sxu32 nU;` |
|   22920 | 5717 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   22954 | 5718 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      35 | 5719 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      35 | 5720 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      35 | 5721 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      35 | 5722 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5723 | `			sxu32 nT;` |
|      35 | 5724 | `			if( !hasResolution ){` |
|       - | 5725 | `				/* No conflict resolution block: use standard trait application */` |
|      59 | 5726 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      33 | 5727 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      33 | 5728 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 5729 | `						break;` |
|       - | 5730 | `					}` |
|      17 | 5731 | `				}` |
|      14 | 5732 | `			}else{` |
|       - | 5733 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 5734 | `				 * then use the block to resolve method conflicts.` |
|       - | 5735 | `				 */` |
|       - | 5736 | `				SyToken *pR;` |
|      19 | 5737 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 5738 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 5739 | `					ph7_class_attr *pAR;` |
|       - | 5740 | `					SyHashEntry *pER;` |
|       - | 5741 | `					SyString *pNR;` |
|      11 | 5742 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 5743 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 5744 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 5745 | `						pNR = &pAR->sName;` |
|     ! 0 | 5746 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 5747 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 5748 | `						}` |
|     ! 0 | 5749 | `					}` |
|      11 | 5750 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 5751 | `				}` |
|       - | 5752 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 5753 | `				pR = pUse->pResolvStart;` |
|      21 | 5754 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5755 | `					SyString sTrait,sMethod;` |
|       - | 5756 | `					ph7_class *pSrcTrait;` |
|       - | 5757 | `					ph7_class_method *pMeth;` |
|       - | 5758 | `					sxi32 nRKwrd;` |
|      33 | 5759 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5760 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5761 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5762 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5763 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5764 | `					sMethod = pR->sData;` |
|      13 | 5765 | `					pR++;` |
|      13 | 5766 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5767 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5768 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5769 | `							sTrait = sMethod;` |
|       7 | 5770 | `							pR++;` |
|       7 | 5771 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5772 | `							sMethod = pR->sData;` |
|       7 | 5773 | `							pR++;` |
|       3 | 5774 | `						}` |
|       3 | 5775 | `					}` |
|      13 | 5776 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5777 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5778 | `						continue;` |
|       - | 5779 | `					}` |
|      13 | 5780 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5781 | `					pR++;` |
|      13 | 5782 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 5783 | `						pSrcTrait = 0;` |
|       7 | 5784 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 5785 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 5786 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 5787 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 5788 | `								pSrcTrait = apTrait[nT];` |
|       5 | 5789 | `								break;` |
|       - | 5790 | `							}` |
|       2 | 5791 | `						}` |
|       5 | 5792 | `						if( pSrcTrait ){` |
|       5 | 5793 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 5794 | `							if( pMeth ){` |
|       5 | 5795 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 5796 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 5797 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 5798 | `								}` |
|       2 | 5799 | `							}` |
|       2 | 5800 | `						}` |
|       2 | 5801 | `					}` |
|      29 | 5802 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5803 | `				}` |
|       - | 5804 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 5805 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 5806 | `					ph7_class_method *pMR;` |
|       - | 5807 | `					SyHashEntry *pER;` |
|       - | 5808 | `					SyString *pNR;` |
|      11 | 5809 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 5810 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 5811 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 5812 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 5813 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 5814 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 5815 | `						}` |
|       1 | 5816 | `					}` |
|       6 | 5817 | `				}` |
|       - | 5818 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 5819 | `				pR = pUse->pResolvStart;` |
|      21 | 5820 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 5821 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 5822 | `					ph7_class *pSrcTrait;` |
|       - | 5823 | `					ph7_class_method *pMeth;` |
|      21 | 5824 | `					int hasQual = 0;` |
|       - | 5825 | `					sxi32 nRKwrd;` |
|      33 | 5826 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 5827 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 5828 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 5829 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 5830 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 5831 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 5832 | `					sMethod = pR->sData;` |
|      13 | 5833 | `					pR++;` |
|      13 | 5834 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 5835 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 5836 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 5837 | `							sTrait = sMethod;` |
|       7 | 5838 | `							hasQual = 1;` |
|       7 | 5839 | `							pR++;` |
|       7 | 5840 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 5841 | `							sMethod = pR->sData;` |
|       7 | 5842 | `							pR++;` |
|       3 | 5843 | `						}` |
|       3 | 5844 | `					}` |
|      13 | 5845 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5846 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 5847 | `						continue;` |
|       - | 5848 | `					}` |
|      13 | 5849 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 5850 | `					pR++;` |
|      13 | 5851 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 5852 | `						sxi32 iNewVis = -1;` |
|       9 | 5853 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 5854 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 5855 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 5856 | `								iNewVis = nAK;` |
|       7 | 5857 | `								pR++;` |
|       3 | 5858 | `							}` |
|       3 | 5859 | `						}` |
|       9 | 5860 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 5861 | `							sAlias = pR->sData;` |
|       7 | 5862 | `							pR++;` |
|       3 | 5863 | `						}` |
|       9 | 5864 | `						pMeth = 0;` |
|       9 | 5865 | `						if( hasQual ){` |
|       3 | 5866 | `							pSrcTrait = 0;` |
|       5 | 5867 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 5868 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 5869 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 5870 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 5871 | `									pSrcTrait = apTrait[nT];` |
|       3 | 5872 | `									break;` |
|       - | 5873 | `								}` |
|       2 | 5874 | `							}` |
|       3 | 5875 | `							if( pSrcTrait ){` |
|       3 | 5876 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 5877 | `							}` |
|       2 | 5878 | `						}else{` |
|       7 | 5879 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 5880 | `						}` |
|       9 | 5881 | `						if( pMeth ){` |
|       9 | 5882 | `							if( sAlias.nByte > 0 ){` |
|       - | 5883 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 5884 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 5885 | `								 */` |
|       - | 5886 | `								ph7_class_method *pAlias;` |
|       - | 5887 | `								char *zAliasDup;` |
|       7 | 5888 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 5889 | `								if( pAlias ){` |
|       7 | 5890 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 5891 | `									if( iNewVis >= 0 ){` |
|       5 | 5892 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5893 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5894 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 5895 | `									}` |
|       7 | 5896 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 5897 | `									if( zAliasDup ){` |
|       7 | 5898 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 5899 | `									}` |
|       4 | 5900 | `								}` |
|       6 | 5901 | `							}else if( iNewVis >= 0 ){` |
|       - | 5902 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 5903 | `								ph7_class_method *pCopy;` |
|       3 | 5904 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 5905 | `								if( pCopy ){` |
|       3 | 5906 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 5907 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 5908 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 5909 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 5910 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 5911 | `									/* Replace the method in the class hash */` |
|       3 | 5912 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 5913 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 5914 | `								}` |
|       1 | 5915 | `							}` |
|       4 | 5916 | `						}` |
|       4 | 5917 | `						SXUNUSED(hasQual);` |
|       4 | 5918 | `					}` |
|      17 | 5919 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 5920 | `				}` |
|       - | 5921 | `			}` |
|      35 | 5922 | `			SySetRelease(&pUse->aTraits);` |
|      18 | 5923 | `		}` |
|       - | 5924 | `	}` |
|       - | 5925 | `	/* Install the class */` |
|   22920 | 5926 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   22920 | 5927 | `	if( rc == SXRET_OK ){` |
|       - | 5928 | `		ph7_class **apInterface;` |
|       - | 5929 | `		sxu32 n;` |
|   22920 | 5930 | `		if( pBase ){` |
|       - | 5931 | `			/* Inherit from base class and mark as a subclass */` |
|   15098 | 5932 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    7548 | 5933 | `		}` |
|   22920 | 5934 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   22952 | 5935 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 5936 | `			/* Implements one or more interface */` |
|      34 | 5937 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|      34 | 5938 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5939 | `				break;` |
|       - | 5940 | `			}` |
|      18 | 5941 | `		}` |
|       - | 5942 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   22920 | 5943 | `		if( rc == SXRET_OK ){` |
|   22920 | 5944 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   22920 | 5945 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5946 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5947 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5948 | `				return SXERR_ABORT;` |
|       - | 5949 | `			}` |
|   11459 | 5950 | `		}` |
|       - | 5951 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   22920 | 5952 | `		if( rc == SXRET_OK ){` |
|   22920 | 5953 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   22920 | 5954 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 5955 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 5956 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 5957 | `				return SXERR_ABORT;` |
|       - | 5958 | `			}` |
|   11459 | 5959 | `		}` |
|   11459 | 5960 | `	}` |
|   22920 | 5961 | `	SySetRelease(&aUseEntries);` |
|   22920 | 5962 | `	SySetRelease(&aInterfaces);` |
|   22920 | 5963 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5964 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5965 | `		return SXERR_ABORT;` |
|       - | 5966 | `	}` |
|   11459 | 5967 | `done:` |
|       - | 5968 | `	/* Point beyond the class body */` |
|   22924 | 5969 | `	pGen->pIn = &pEnd[1];` |
|   22924 | 5970 | `	pGen->pEnd = pTmp;` |
|   22924 | 5971 | `	return PH7_OK;` |
|   11463 | 5972 |  |
|       - | 5973 | `/*` |
|       - | 5974 | ` * Compile a user-defined abstract class.` |
|       - | 5975 | ` *  According to the PHP language reference manual` |
|       - | 5976 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 5977 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 5978 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 5979 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 5980 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 5981 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 5982 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 5983 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 5984 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 5985 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 5986 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 5987 | ` *   could differ.` |
|       - | 5988 | ` */` |
|      14 | 5989 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 5990 |  |
|       - | 5991 | `	sxi32 rc;` |
|      16 | 5992 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      16 | 5993 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      16 | 5994 | `	return rc;` |
|       2 | 5995 |  |
|       - | 5996 | `/*` |
|       - | 5997 | ` * Compile a user-defined final class.` |
|       - | 5998 | ` *  According to the PHP language reference manual` |
|       - | 5999 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6000 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6001 | ` *    final then it cannot be extended.` |
|       - | 6002 | ` */` |
|       2 | 6003 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6004 |  |
|       - | 6005 | `	sxi32 rc;` |
|       3 | 6006 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6007 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6008 | `	return rc;` |
|       1 | 6009 |  |
|       - | 6010 | `/*` |
|       - | 6011 | ` * Compile a user-defined trait.` |
|       - | 6012 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6013 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6014 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6015 | ` */` |
|      46 | 6016 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 6017 |  |
|      47 | 6018 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6019 | `	ph7_class *pClass;` |
|       - | 6020 | `	SyToken *pEnd,*pTmp;` |
|       - | 6021 | `	sxi32 iProtection;` |
|       - | 6022 | `	sxi32 iAttrflags;` |
|       - | 6023 | `	SyString *pName;` |
|       - | 6024 | `	sxi32 nKwrd;` |
|       - | 6025 | `	sxi32 rc;` |
|       - | 6026 | `	/* Jump the 'trait' keyword */` |
|      47 | 6027 | `	pGen->pIn++;` |
|      47 | 6028 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6029 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6030 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6031 | `			return SXERR_ABORT;` |
|       - | 6032 | `		}` |
|     ! 0 | 6033 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6034 | `			pGen->pIn++;` |
|     ! 0 | 6035 | `		}` |
|     ! 0 | 6036 | `		return SXRET_OK;` |
|       - | 6037 | `	}` |
|       - | 6038 | `	/* Extract trait name */` |
|      47 | 6039 | `	pName = &pGen->pIn->sData;` |
|      47 | 6040 | `	pGen->pIn++;` |
|       - | 6041 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6042 | `		SyBlob sFQN;` |
|       - | 6043 | `		SyString sFQNStr;` |
|      47 | 6044 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      47 | 6045 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      47 | 6046 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      47 | 6047 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      47 | 6048 | `		SyBlobRelease(&sFQN);` |
|       - | 6049 | `	}` |
|      47 | 6050 | `	if( pClass == 0 ){` |
|     ! 0 | 6051 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6052 | `		return SXERR_ABORT;` |
|       - | 6053 | `	}` |
|       - | 6054 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      47 | 6055 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6056 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6057 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6058 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6059 | `			return SXERR_ABORT;` |
|       - | 6060 | `		}` |
|     ! 0 | 6061 | `		return SXRET_OK;` |
|       - | 6062 | `	}` |
|      47 | 6063 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      47 | 6064 | `	pEnd = 0;` |
|      47 | 6065 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      47 | 6066 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6067 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6068 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6069 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6070 | `			return SXERR_ABORT;` |
|       - | 6071 | `		}` |
|     ! 0 | 6072 | `		return SXRET_OK;` |
|       - | 6073 | `	}` |
|       - | 6074 | `	/* Swap token stream */` |
|      47 | 6075 | `	pTmp = pGen->pEnd;` |
|      47 | 6076 | `	pGen->pEnd = pEnd;` |
|       - | 6077 | `	/* Mark as trait */` |
|      47 | 6078 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6079 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      48 | 6080 | `	for(;;){` |
|     133 | 6081 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6082 | `			pGen->pIn++;` |
|       1 | 6083 | `		}` |
|     113 | 6084 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      47 | 6085 | `			break;` |
|       - | 6086 | `		}` |
|      67 | 6087 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6088 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6089 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6090 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6091 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6092 | `				return SXERR_ABORT;` |
|       - | 6093 | `			}` |
|     ! 0 | 6094 | `			goto done;` |
|       - | 6095 | `		}` |
|      67 | 6096 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      67 | 6097 | `		iAttrflags = 0;` |
|      67 | 6098 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      67 | 6099 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      67 | 6100 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6101 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6102 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6103 | `				for(;;){` |
|       - | 6104 | `					ph7_class *pUsedTrait;` |
|       - | 6105 | `					SyString *pUsedName;` |
|       5 | 6106 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6107 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6108 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6109 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6110 | `							return SXERR_ABORT;` |
|       - | 6111 | `						}` |
|     ! 0 | 6112 | `						break;` |
|       - | 6113 | `					}` |
|       5 | 6114 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6115 | `					{` |
|       - | 6116 | `						SyBlob sResolved;` |
|       5 | 6117 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6118 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6119 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6120 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6121 | `						SyBlobRelease(&sResolved);` |
|       - | 6122 | `					}` |
|       5 | 6123 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6124 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6125 | `					}` |
|       5 | 6126 | `					if( pUsedTrait == 0 ){` |
|       4 | 6127 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6128 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6129 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6130 | `							return SXERR_ABORT;` |
|       - | 6131 | `						}` |
|       2 | 6132 | `					}else{` |
|       3 | 6133 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6134 | `					}` |
|       5 | 6135 | `					pGen->pIn++;` |
|       5 | 6136 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6137 | `						break;` |
|       - | 6138 | `					}` |
|     ! 0 | 6139 | `					pGen->pIn++;` |
|     ! 0 | 6140 | `				}` |
|       5 | 6141 | `				continue;` |
|       - | 6142 | `			}` |
|      63 | 6143 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      59 | 6144 | `				iProtection = nKwrd;` |
|      59 | 6145 | `				pGen->pIn++;` |
|      59 | 6146 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6147 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6148 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6149 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6150 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6151 | `						return SXERR_ABORT;` |
|       - | 6152 | `					}` |
|     ! 0 | 6153 | `					goto done;` |
|       - | 6154 | `				}` |
|      59 | 6155 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6156 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6157 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6158 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6159 | `							return SXERR_ABORT;` |
|       - | 6160 | `						}` |
|     ! 0 | 6161 | `						goto done;` |
|       - | 6162 | `					}` |
|      11 | 6163 | `					continue;` |
|       - | 6164 | `				}` |
|      49 | 6165 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      24 | 6166 | `			}` |
|      53 | 6167 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6168 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6169 | `					"Traits cannot have constants");` |
|     ! 0 | 6170 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6171 | `					return SXERR_ABORT;` |
|       - | 6172 | `				}` |
|     ! 0 | 6173 | `				goto done;` |
|     ! 0 | 6174 | `			}else{` |
|      53 | 6175 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6176 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6177 | `					pGen->pIn++;` |
|       5 | 6178 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6179 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6180 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6181 | `							iProtection = nKwrd;` |
|     ! 0 | 6182 | `							pGen->pIn++;` |
|     ! 0 | 6183 | `						}` |
|       1 | 6184 | `					}` |
|       5 | 6185 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6186 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6187 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6188 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6189 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6190 | `							return SXERR_ABORT;` |
|       - | 6191 | `						}` |
|     ! 0 | 6192 | `						goto done;` |
|       - | 6193 | `					}` |
|       5 | 6194 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6195 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6196 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6197 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6198 | `								return SXERR_ABORT;` |
|       - | 6199 | `							}` |
|     ! 0 | 6200 | `							goto done;` |
|       - | 6201 | `						}` |
|       3 | 6202 | `						continue;` |
|       - | 6203 | `					}` |
|       3 | 6204 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      50 | 6205 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6206 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6207 | `					pGen->pIn++;` |
|       5 | 6208 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6209 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6210 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6211 | `							iProtection = nKwrd;` |
|       5 | 6212 | `							pGen->pIn++;` |
|       2 | 6213 | `						}` |
|       2 | 6214 | `					}` |
|       5 | 6215 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6216 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6217 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6218 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6219 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6220 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6221 | `							return SXERR_ABORT;` |
|       - | 6222 | `						}` |
|     ! 0 | 6223 | `						goto done;` |
|       - | 6224 | `					}` |
|       5 | 6225 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6226 | `				}` |
|      51 | 6227 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6228 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6229 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6230 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6231 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6232 | `						return SXERR_ABORT;` |
|       - | 6233 | `					}` |
|     ! 0 | 6234 | `					goto done;` |
|       - | 6235 | `				}` |
|      51 | 6236 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6237 | `					pGen->pIn++;` |
|     ! 0 | 6238 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6239 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6240 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6241 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6242 | `							return SXERR_ABORT;` |
|       - | 6243 | `						}` |
|     ! 0 | 6244 | `						goto done;` |
|       - | 6245 | `					}` |
|     ! 0 | 6246 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6247 | `				}else{` |
|      51 | 6248 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6249 | `				}` |
|      51 | 6250 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6251 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6252 | `						return SXERR_ABORT;` |
|       - | 6253 | `					}` |
|     ! 0 | 6254 | `					goto done;` |
|       - | 6255 | `				}` |
|       - | 6256 | `			}` |
|      26 | 6257 | `		}else{` |
|     ! 0 | 6258 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6259 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6260 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6261 | `					return SXERR_ABORT;` |
|       - | 6262 | `				}` |
|     ! 0 | 6263 | `				goto done;` |
|       - | 6264 | `			}` |
|       - | 6265 | `		}` |
|       1 | 6266 | `	}` |
|       - | 6267 | `	/* Install the trait */` |
|      47 | 6268 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      47 | 6269 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6270 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6271 | `		return SXERR_ABORT;` |
|       - | 6272 | `	}` |
|      23 | 6273 | `done:` |
|       - | 6274 | `	/* Point beyond the trait body */` |
|      47 | 6275 | `	pGen->pIn = &pEnd[1];` |
|      47 | 6276 | `	pGen->pEnd = pTmp;` |
|      47 | 6277 | `	return PH7_OK;` |
|      24 | 6278 |  |
|       - | 6279 | `/*` |
|       - | 6280 | ` * Compile a user-defined class.` |
|       - | 6281 | ` *  According to the PHP language reference manual` |
|       - | 6282 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6283 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6284 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6285 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6286 | ` *   and functions (called "methods").` |
|       - | 6287 | ` */` |
|   22906 | 6288 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6289 |  |
|       - | 6290 | `	sxi32 rc;` |
|   22908 | 6291 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   22908 | 6292 | `	return rc;` |
|       2 | 6293 |  |
|       - | 6294 | `/*` |
|       - | 6295 | ` * Exception handling.` |
|       - | 6296 | ` *  According to the PHP language reference manual` |
|       - | 6297 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6298 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6299 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6300 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6301 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6302 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6303 | ` *    (or re-thrown) within a catch block.` |
|       - | 6304 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6305 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6306 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6307 | ` *    been defined with set_exception_handler().` |
|       - | 6308 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6309 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6310 | ` */` |
|       - | 6311 | `/*` |
|       - | 6312 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6313 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6314 | ` * indicates failure.` |
|       - | 6315 | ` */` |
|    7546 | 6316 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6317 |  |
|    7548 | 6318 | `	sxi32 rc = SXRET_OK;` |
|    7548 | 6319 | `	if( pRoot->pOp ){` |
|    7544 | 6320 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3774 | 6321 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6322 | `			/* Unexpected expression */` |
|     ! 0 | 6323 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6324 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6325 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6326 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6327 | `			}` |
|       2 | 6328 | `		}` |
|    3775 | 6329 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6330 | `		/* Unexpected expression */` |
|     ! 0 | 6331 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6332 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6333 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6334 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6335 | `		}` |
|     ! 0 | 6336 | `	}` |
|    7548 | 6337 | `	return rc;` |
|       2 | 6338 |  |
|       - | 6339 | `/*` |
|       - | 6340 | ` * Compile a 'throw' statement.` |
|       - | 6341 | ` * throw: This is how you trigger an exception.` |
|       - | 6342 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6343 | ` */` |
|    7546 | 6344 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6345 |  |
|    7548 | 6346 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6347 | `	GenBlock *pBlock;` |
|       - | 6348 | `	sxu32 nIdx;` |
|       - | 6349 | `	sxi32 rc;` |
|    7548 | 6350 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6351 | `	/* Compile the expression */` |
|    7548 | 6352 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7548 | 6353 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6354 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6355 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6356 | `			return SXERR_ABORT;` |
|       - | 6357 | `		}` |
|     ! 0 | 6358 | `		return SXRET_OK;` |
|       - | 6359 | `	}` |
|    7548 | 6360 | `	pBlock = pGen->pCurrent;` |
|       - | 6361 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   35142 | 6362 | `	while(pBlock->pParent){` |
|   35138 | 6363 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7544 | 6364 | `			break;` |
|       - | 6365 | `		}` |
|       - | 6366 | `		/* Point to the parent block */` |
|   27596 | 6367 | `		pBlock = pBlock->pParent;` |
|       2 | 6368 | `	}` |
|       - | 6369 | `	/* Emit the throw instruction */` |
|    7548 | 6370 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6371 | `	/* Emit the jump */` |
|    7548 | 6372 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7548 | 6373 | `	return SXRET_OK;` |
|    3775 | 6374 |  |
|       - | 6375 | `/*` |
|       - | 6376 | ` * Compile a 'catch' block.` |
|       - | 6377 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6378 | ` * an object containing the exception information.` |
|       - | 6379 | ` */` |
|      48 | 6380 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6381 |  |
|      50 | 6382 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6383 | `	ph7_exception_block sCatch;` |
|       - | 6384 | `	SySet *pInstrContainer;` |
|       - | 6385 | `	GenBlock *pCatch;` |
|       - | 6386 | `	SyToken *pToken;` |
|       - | 6387 | `	SyString *pName;` |
|       - | 6388 | `	char *zDup;` |
|       - | 6389 | `	sxi32 rc;` |
|      50 | 6390 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6391 | `	/* Zero the structure */` |
|      50 | 6392 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6393 | `	/* Initialize fields */` |
|      50 | 6394 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      72 | 6395 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      50 | 6396 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6397 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6398 | `			pToken = pGen->pIn;` |
|     ! 0 | 6399 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6400 | `				pToken--;` |
|     ! 0 | 6401 | `			}` |
|     ! 0 | 6402 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6403 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6404 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6405 | `				return SXERR_ABORT;` |
|       - | 6406 | `			}` |
|     ! 0 | 6407 | `			return SXERR_INVALID;` |
|       - | 6408 | `	}` |
|       - | 6409 | `	/* Extract the exception class */` |
|      50 | 6410 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6411 | `	/* Duplicate class name */` |
|      50 | 6412 | `	pName = &pGen->pIn->sData;` |
|      50 | 6413 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6414 | `	if( zDup == 0 ){` |
|     ! 0 | 6415 | `		goto Mem;` |
|       - | 6416 | `	}` |
|      50 | 6417 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      50 | 6418 | `	pGen->pIn++;` |
|      72 | 6419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      50 | 6420 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6421 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6422 | `			pToken = pGen->pIn;` |
|     ! 0 | 6423 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6424 | `				pToken--;` |
|     ! 0 | 6425 | `			}` |
|     ! 0 | 6426 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6427 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6428 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6429 | `				return SXERR_ABORT;` |
|       - | 6430 | `			}` |
|     ! 0 | 6431 | `			return SXERR_INVALID;` |
|       - | 6432 | `	}` |
|      50 | 6433 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6434 | `	/* Duplicate instance name */` |
|      50 | 6435 | `	pName = &pGen->pIn->sData;` |
|      50 | 6436 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      50 | 6437 | `	if( zDup == 0 ){` |
|     ! 0 | 6438 | `		goto Mem;` |
|       - | 6439 | `	}` |
|      50 | 6440 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      50 | 6441 | `	pGen->pIn++;` |
|      50 | 6442 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6443 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6444 | `		pToken = pGen->pIn;` |
|     ! 0 | 6445 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6446 | `			pToken--;` |
|     ! 0 | 6447 | `		}` |
|     ! 0 | 6448 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6449 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6451 | `			return SXERR_ABORT;` |
|       - | 6452 | `		}` |
|     ! 0 | 6453 | `		return SXERR_INVALID;` |
|       - | 6454 | `	}` |
|       - | 6455 | `	/* Compile the block */` |
|      50 | 6456 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6457 | `	/* Create the catch block */` |
|      50 | 6458 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      50 | 6459 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6460 | `		return SXERR_ABORT;` |
|       - | 6461 | `	}` |
|       - | 6462 | `	/* Swap bytecode container */` |
|      50 | 6463 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      50 | 6464 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6465 | `	/* Compile the block */` |
|      50 | 6466 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6467 | `	/* Fix forward jumps now the destination is resolved  */` |
|      50 | 6468 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6469 | `	/* Emit the DONE instruction */` |
|      50 | 6470 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6471 | `	/* Leave the block */` |
|      50 | 6472 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6473 | `	/* Restore the default container */` |
|      50 | 6474 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6475 | `	/* Install the catch block */` |
|      50 | 6476 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      50 | 6477 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6478 | `		goto Mem;` |
|       - | 6479 | `	}` |
|      50 | 6480 | `	return SXRET_OK;` |
|     ! 0 | 6481 | `Mem:` |
|     ! 0 | 6482 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6483 | `	return SXERR_ABORT;` |
|      26 | 6484 |  |
|       - | 6485 | `/*` |
|       - | 6486 | ` * Compile a 'try' block.` |
|       - | 6487 | ` * A function using an exception should be in a "try" block.` |
|       - | 6488 | ` * If the exception does not trigger, the code will continue` |
|       - | 6489 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6490 | ` * is "thrown".` |
|       - | 6491 | ` */` |
|      56 | 6492 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6493 |  |
|       - | 6494 | `	ph7_exception *pException;` |
|      58 | 6495 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6496 | `	GenBlock *pTry;` |
|       - | 6497 | `	sxu32 nJmpIdx;` |
|       - | 6498 | `	sxi32 rc;` |
|       - | 6499 | `	/* Create the exception container */` |
|      58 | 6500 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      58 | 6501 | `	if( pException == 0 ){` |
|     ! 0 | 6502 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6503 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6504 | `		return SXERR_ABORT;` |
|       - | 6505 | `	}` |
|       - | 6506 | `	/* Zero the structure */` |
|      58 | 6507 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6508 | `	/* Initialize fields */` |
|      58 | 6509 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      58 | 6510 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      58 | 6511 | `	pException->iHasFinally = 0;` |
|      58 | 6512 | `	pException->iFinallyDone = 0;` |
|      58 | 6513 | `	pException->pVm = pGen->pVm;` |
|       - | 6514 | `	/* Create the try block */` |
|      58 | 6515 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      58 | 6516 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6517 | `		return SXERR_ABORT;` |
|       - | 6518 | `	}` |
|       - | 6519 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      58 | 6520 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6521 | `	/* Fix the jump later when the destination is resolved */` |
|      58 | 6522 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      58 | 6523 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6524 | `	/* Compile the block */` |
|      58 | 6525 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      58 | 6526 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6527 | `		return SXERR_ABORT;` |
|       - | 6528 | `	}` |
|       - | 6529 | `	/* Fix forward jumps now the destination is resolved */` |
|      58 | 6530 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6531 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      58 | 6532 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6533 | `	/* Leave the block */` |
|      58 | 6534 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6535 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      58 | 6536 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      54 | 6537 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6538 | `		/* Compile one or more catch blocks */` |
|      48 | 6539 | `		for(;;){` |
|      96 | 6540 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      74 | 6541 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      26 | 6542 | `					break;` |
|       - | 6543 | `			}` |
|      50 | 6544 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      50 | 6545 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6546 | `				return SXERR_ABORT;` |
|       - | 6547 | `			}` |
|       2 | 6548 | `		}` |
|      24 | 6549 | `	}` |
|       - | 6550 | `	/* Compile optional finally block */` |
|      58 | 6551 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      24 | 6552 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6553 | `		SySet *pInstrContainer;` |
|       - | 6554 | `		GenBlock *pFinBlock;` |
|      21 | 6555 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6556 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      21 | 6557 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      21 | 6558 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6559 | `			return SXERR_ABORT;` |
|       - | 6560 | `		}` |
|       - | 6561 | `		/* Swap bytecode container */` |
|      21 | 6562 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      21 | 6563 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6564 | `		/* Compile the finally body */` |
|      21 | 6565 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 6566 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6567 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6568 | `			return SXERR_ABORT;` |
|       - | 6569 | `		}` |
|       - | 6570 | `		/* Fix forward jumps now the destination is resolved */` |
|      21 | 6571 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6572 | `		/* Emit DONE to terminate the finally block */` |
|      21 | 6573 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6574 | `		/* Leave the block */` |
|      21 | 6575 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6576 | `		/* Restore the default container */` |
|      21 | 6577 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      21 | 6578 | `		pException->iHasFinally = 1;` |
|      10 | 6579 | `	}` |
|       - | 6580 | `	/* Must have at least one catch or finally */` |
|      58 | 6581 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6582 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6583 | `			"Cannot use try without catch or finally");` |
|       3 | 6584 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6585 | `			return SXERR_ABORT;` |
|       - | 6586 | `		}` |
|       1 | 6587 | `	}` |
|      58 | 6588 | `	return SXRET_OK;` |
|      30 | 6589 |  |
|       - | 6590 | `/*` |
|       - | 6591 | ` * Compile a switch block.` |
|       - | 6592 | ` *  (See block-comment below for more information)` |
|       - | 6593 | ` */` |
|      84 | 6594 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6595 |  |
|      86 | 6596 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6597 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6598 | `		/* Unexpected token */` |
|     ! 0 | 6599 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6600 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6601 | `			return SXERR_ABORT;` |
|       - | 6602 | `		}` |
|     ! 0 | 6603 | `		pGen->pIn++;` |
|     ! 0 | 6604 | `	}` |
|      86 | 6605 | `	pGen->pIn++;` |
|       - | 6606 | `	/* First instruction to execute in this block. */` |
|      86 | 6607 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6608 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6609 | `	 * or the '}' token */` |
|     151 | 6610 | `	for(;;){` |
|     304 | 6611 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6612 | `			/* No more input to process */` |
|     ! 0 | 6613 | `			break;` |
|       - | 6614 | `		}` |
|     304 | 6615 | `		rc = SXRET_OK;` |
|     304 | 6616 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6617 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6618 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6619 | `					/* Unexpected token */` |
|     ! 0 | 6620 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6621 | `						&pGen->pIn->sData);` |
|     ! 0 | 6622 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6623 | `						return SXERR_ABORT;` |
|       - | 6624 | `					}` |
|       - | 6625 | `					/* FALL THROUGH */` |
|     ! 0 | 6626 | `				}` |
|      20 | 6627 | `				rc = SXERR_EOF;` |
|      20 | 6628 | `				break;` |
|       - | 6629 | `			}` |
|      23 | 6630 | `		}else{` |
|       - | 6631 | `			sxi32 nKwrd;` |
|       - | 6632 | `			/* Extract the keyword */` |
|     244 | 6633 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6634 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6635 | `				break;` |
|       - | 6636 | `			}` |
|     180 | 6637 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6638 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6639 | `					/* Unexpected token */` |
|     ! 0 | 6640 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6641 | `						&pGen->pIn->sData);` |
|     ! 0 | 6642 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6643 | `						return SXERR_ABORT;` |
|       - | 6644 | `					}` |
|       - | 6645 | `					/* FALL THROUGH */` |
|     ! 0 | 6646 | `				}` |
|       - | 6647 | `				/* Block compiled */` |
|       3 | 6648 | `				break;` |
|       - | 6649 | `			}` |
|       - | 6650 | `		}` |
|       - | 6651 | `		/* Compile block */` |
|     220 | 6652 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6653 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6654 | `			return SXERR_ABORT;` |
|       - | 6655 | `		}` |
|       2 | 6656 | `	}` |
|      86 | 6657 | `	return rc;` |
|      44 | 6658 |  |
|       - | 6659 | `/*` |
|       - | 6660 | ` * Compile a case eXpression.` |
|       - | 6661 | ` *  (See block-comment below for more information)` |
|       - | 6662 | ` */` |
|      70 | 6663 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6664 |  |
|       - | 6665 | `	SySet *pInstrContainer;` |
|       - | 6666 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6667 | `	sxi32 iNest = 0;` |
|       - | 6668 | `	sxi32 rc;` |
|       - | 6669 | `	/* Delimit the expression */` |
|      72 | 6670 | `	pEnd = pGen->pIn;` |
|     150 | 6671 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6672 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6673 | `			/* Increment nesting level */` |
|       3 | 6674 | `			iNest++;` |
|     149 | 6675 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6676 | `			/* Decrement nesting level */` |
|       3 | 6677 | `			iNest--;` |
|     147 | 6678 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6679 | `			break;` |
|       - | 6680 | `		}` |
|      80 | 6681 | `		pEnd++;` |
|       2 | 6682 | `	}` |
|      72 | 6683 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6685 | `		if( rc == SXERR_ABORT ){` |
|       - | 6686 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6687 | `			return SXERR_ABORT;` |
|       - | 6688 | `		}` |
|     ! 0 | 6689 | `	}` |
|       - | 6690 | `	/* Swap token stream */` |
|      72 | 6691 | `	pTmp = pGen->pEnd;` |
|      72 | 6692 | `	pGen->pEnd = pEnd;` |
|      72 | 6693 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6694 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6695 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6696 | `	/* Emit the done instruction */` |
|      72 | 6697 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6698 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6699 | `	/* Update token stream */` |
|      72 | 6700 | `	pGen->pIn  = pEnd;` |
|      72 | 6701 | `	pGen->pEnd = pTmp;` |
|      72 | 6702 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6703 | `		return SXERR_ABORT;` |
|       - | 6704 | `	}` |
|      72 | 6705 | `	return SXRET_OK;` |
|      37 | 6706 |  |
|       - | 6707 | `/*` |
|       - | 6708 | ` * Compile the smart switch statement.` |
|       - | 6709 | ` * According to the PHP language reference manual` |
|       - | 6710 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6711 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6712 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6713 | ` *  This is exactly what the switch statement is for.` |
|       - | 6714 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6715 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6716 | ` *  of the outer loop, use continue 2.` |
|       - | 6717 | ` *  Note that switch/case does loose comparision.` |
|       - | 6718 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6719 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6720 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6721 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6722 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6723 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6724 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 6725 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 6726 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 6727 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 6728 | ` *  list for the next case.` |
|       - | 6729 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 6730 | ` *  or floating-point numbers and strings.` |
|       - | 6731 | ` */` |
|      20 | 6732 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 6733 |  |
|       - | 6734 | `	GenBlock *pSwitchBlock;` |
|       - | 6735 | `	SyToken *pTmp,*pEnd;` |
|       - | 6736 | `	ph7_switch *pSwitch;` |
|       - | 6737 | `	sxu32 nToken;` |
|       - | 6738 | `	sxu32 nLine;` |
|       - | 6739 | `	sxi32 rc;` |
|      22 | 6740 | `	nLine = pGen->pIn->nLine;` |
|       - | 6741 | `	/* Jump the 'switch' keyword */` |
|      22 | 6742 | `	pGen->pIn++;` |
|      22 | 6743 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6744 | `		/* Syntax error */` |
|     ! 0 | 6745 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 6746 | `		if( rc == SXERR_ABORT ){` |
|       - | 6747 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6748 | `			return SXERR_ABORT;` |
|       - | 6749 | `		}` |
|     ! 0 | 6750 | `		goto Synchronize;` |
|       - | 6751 | `	}` |
|       - | 6752 | `	/* Jump the left parenthesis '(' */` |
|      22 | 6753 | `	pGen->pIn++;` |
|      22 | 6754 | `	pEnd = 0; /* cc warning */` |
|       - | 6755 | `	/* Create the loop block */` |
|      32 | 6756 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 6757 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 6758 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6759 | `		return SXERR_ABORT;` |
|       - | 6760 | `	}` |
|       - | 6761 | `	/* Delimit the condition */` |
|      22 | 6762 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 6763 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 6764 | `		/* Empty expression */` |
|     ! 0 | 6765 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 6766 | `		if( rc == SXERR_ABORT ){` |
|       - | 6767 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6768 | `			return SXERR_ABORT;` |
|       - | 6769 | `		}` |
|     ! 0 | 6770 | `	}` |
|       - | 6771 | `	/* Swap token streams */` |
|      22 | 6772 | `	pTmp = pGen->pEnd;` |
|      22 | 6773 | `	pGen->pEnd = pEnd;` |
|       - | 6774 | `	/* Compile the expression */` |
|      22 | 6775 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 6776 | `	if( rc == SXERR_ABORT ){` |
|       - | 6777 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 6778 | `		return SXERR_ABORT;` |
|       - | 6779 | `	}` |
|       - | 6780 | `	/* Update token stream */` |
|      22 | 6781 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 6782 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6783 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6784 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6785 | `			return SXERR_ABORT;` |
|       - | 6786 | `		}` |
|     ! 0 | 6787 | `		pGen->pIn++;` |
|     ! 0 | 6788 | `	}` |
|      22 | 6789 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 6790 | `	pGen->pEnd = pTmp;` |
|      22 | 6791 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 6792 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 6793 | `			pTmp = pGen->pIn;` |
|     ! 0 | 6794 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 6795 | `				pTmp--;` |
|     ! 0 | 6796 | `			}` |
|       - | 6797 | `			/* Unexpected token */` |
|     ! 0 | 6798 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 6799 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6800 | `				return SXERR_ABORT;` |
|       - | 6801 | `			}` |
|     ! 0 | 6802 | `			goto Synchronize;` |
|       - | 6803 | `	}` |
|       - | 6804 | `	/* Set the delimiter token */` |
|      22 | 6805 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 6806 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 6807 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 6808 | `	}else{` |
|      20 | 6809 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 6810 | `	}` |
|      22 | 6811 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 6812 | `	/* Create the switch blocks container */` |
|      22 | 6813 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 6814 | `	if( pSwitch == 0 ){` |
|       - | 6815 | `		/* Abort compilation */` |
|     ! 0 | 6816 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6817 | `		return SXERR_ABORT;` |
|       - | 6818 | `	}` |
|       - | 6819 | `	/* Zero the structure */` |
|      22 | 6820 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 6821 | `	/* Initialize fields */` |
|      22 | 6822 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 6823 | `	/* Emit the switch instruction */` |
|      22 | 6824 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 6825 | `	/* Compile case blocks */` |
|      76 | 6826 | `	for(;;){` |
|       - | 6827 | `		sxu32 nKwrd;` |
|      88 | 6828 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6829 | `			/* No more input to process */` |
|     ! 0 | 6830 | `			break;` |
|       - | 6831 | `		}` |
|      88 | 6832 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6833 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 6834 | `				/* Unexpected token */` |
|     ! 0 | 6835 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6836 | `					&pGen->pIn->sData);` |
|     ! 0 | 6837 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6838 | `					return SXERR_ABORT;` |
|       - | 6839 | `				}` |
|       - | 6840 | `				/* FALL THROUGH */` |
|     ! 0 | 6841 | `			}` |
|       - | 6842 | `			/* Block compiled */` |
|     ! 0 | 6843 | `			break;` |
|       - | 6844 | `		}` |
|       - | 6845 | `		/* Extract the keyword */` |
|      88 | 6846 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 6847 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6848 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 6849 | `				/* Unexpected token */` |
|     ! 0 | 6850 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6851 | `					&pGen->pIn->sData);` |
|     ! 0 | 6852 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6853 | `					return SXERR_ABORT;` |
|       - | 6854 | `				}` |
|       - | 6855 | `				/* FALL THROUGH */` |
|     ! 0 | 6856 | `			}` |
|       - | 6857 | `			/* Block compiled */` |
|       3 | 6858 | `			break;` |
|       - | 6859 | `		}` |
|      86 | 6860 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 6861 | `			/*` |
|       - | 6862 | `			 * Accroding to the PHP language reference manual` |
|       - | 6863 | `			 *  A special case is the default case. This case matches anything` |
|       - | 6864 | `			 *  that wasn't matched by the other cases.` |
|       - | 6865 | `			 */` |
|      16 | 6866 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 6867 | `				/* Default case already compiled */` |
|     ! 0 | 6868 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 6869 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6870 | `					return SXERR_ABORT;` |
|       - | 6871 | `				}` |
|     ! 0 | 6872 | `			}` |
|      16 | 6873 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 6874 | `			/* Compile the default block */` |
|      16 | 6875 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 6876 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6877 | `				return SXERR_ABORT;` |
|      16 | 6878 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 6879 | `				break;` |
|       1 | 6880 | `			}` |
|      73 | 6881 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 6882 | `			ph7_case_expr sCase;` |
|       - | 6883 | `			/* Standard case block */` |
|      72 | 6884 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 6885 | `			/* initialize the structure */` |
|      72 | 6886 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 6887 | `			/* Compile the case expression */` |
|      72 | 6888 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 6889 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6890 | `				return SXERR_ABORT;` |
|       - | 6891 | `			}` |
|       - | 6892 | `			/* Compile the case block */` |
|      72 | 6893 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 6894 | `			/* Insert in the switch container */` |
|      72 | 6895 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 6896 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 6897 | `				return SXERR_ABORT;` |
|      72 | 6898 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 6899 | `				break;` |
|       - | 6900 | `			}` |
|      34 | 6901 | `		}else{` |
|       - | 6902 | `			/* Unexpected token */` |
|     ! 0 | 6903 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 6904 | `				&pGen->pIn->sData);` |
|     ! 0 | 6905 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6906 | `				return SXERR_ABORT;` |
|       - | 6907 | `			}` |
|     ! 0 | 6908 | `			break;` |
|       - | 6909 | `		}` |
|       2 | 6910 | `	}` |
|       - | 6911 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 6912 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 6913 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6914 | `	/* Release the loop block */` |
|      22 | 6915 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 6916 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 6917 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 6918 | `		pGen->pIn++;` |
|      10 | 6919 | `	}` |
|       - | 6920 | `	/* Statement successfully compiled */` |
|      22 | 6921 | `	return SXRET_OK;` |
|     ! 0 | 6922 | `Synchronize:` |
|       - | 6923 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 6924 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 6925 | `		pGen->pIn++;` |
|     ! 0 | 6926 | `	}` |
|     ! 0 | 6927 | `	return SXRET_OK;` |
|      12 | 6928 |  |
|       - | 6929 | `/*` |
|       - | 6930 | ` * Generate bytecode for a given expression tree.` |
|       - | 6931 | ` * If something goes wrong while generating bytecode` |
|       - | 6932 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 6933 | ` * this function takes care of generating the appropriate` |
|       - | 6934 | ` * error message.` |
|       - | 6935 | ` */` |
| 2086342 | 6936 | `static sxi32 GenStateEmitExprCode(` |
|       - | 6937 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 6938 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 6939 | `	sxi32 iFlags /* Control flags */` |
|       - | 6940 | `	)` |
|       2 | 6941 |  |
|       - | 6942 | `	VmInstr *pInstr;` |
|       - | 6943 | `	sxu32 nJmpIdx;` |
| 2086344 | 6944 | `	sxi32 iP1 = 0;` |
| 2086344 | 6945 | `	sxu32 iP2 = 0;` |
| 2086344 | 6946 | `	void *p3  = 0;` |
|       - | 6947 | `	sxi32 iVmOp;` |
|       - | 6948 | `	sxi32 rc;` |
| 2086344 | 6949 | `	if( pNode->xCode ){` |
|       - | 6950 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 6951 | `		/* Compile node */` |
| 1279912 | 6952 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1279912 | 6953 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1279912 | 6954 | `		RE_SWAP_DELIMITER(pGen);` |
| 1279912 | 6955 | `		return rc;` |
|       - | 6956 | `	}` |
|  806434 | 6957 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 6958 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 6959 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 6960 | `		return SXERR_ABORT;` |
|       - | 6961 | `	}` |
|  806434 | 6962 | `	iVmOp = pNode->pOp->iVmOp;` |
|  806434 | 6963 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 6964 | `		sxu32 nJz,nJmp;` |
|       - | 6965 | `		/* Ternary operator require special handling */` |
|       - | 6966 | `		/* Phase#1: Compile the condition */` |
|    1748 | 6967 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1748 | 6968 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6969 | `			return rc;` |
|       - | 6970 | `		}` |
|    1748 | 6971 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1748 | 6972 | `		if( pNode->pLeft ){` |
|       - | 6973 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 6974 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1680 | 6975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6976 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1680 | 6977 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1680 | 6978 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6979 | `				return rc;` |
|       - | 6980 | `			}` |
|     841 | 6981 | `		}else{` |
|       - | 6982 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 6983 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 6984 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 6985 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 6986 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 6987 | `		}` |
|       - | 6988 | `		/* Phase#4: Emit the unconditional jump */` |
|    1748 | 6989 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 6990 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1748 | 6991 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1748 | 6992 | `		if( pInstr ){` |
|    1748 | 6993 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 6994 | `		}` |
|    1748 | 6995 | `		if( !pNode->pLeft ){` |
|       - | 6996 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 6997 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 6998 | `		}` |
|       - | 6999 | `		/* Phase#6: Compile the 'else' expression */` |
|    1748 | 7000 | `		if( pNode->pRight ){` |
|    1748 | 7001 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1748 | 7002 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7003 | `				return rc;` |
|       - | 7004 | `			}` |
|     873 | 7005 | `		}` |
|    1748 | 7006 | `		if( nJmp > 0 ){` |
|       - | 7007 | `			/* Phase#7: Fix the unconditional jump */` |
|    1748 | 7008 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1748 | 7009 | `			if( pInstr ){` |
|    1748 | 7010 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     873 | 7011 | `			}` |
|     873 | 7012 | `		}` |
|       - | 7013 | `		/* All done */` |
|    1748 | 7014 | `		return SXRET_OK;` |
|       - | 7015 | `	}` |
|       - | 7016 | `	/* Generate code for the left tree */` |
|  804688 | 7017 | `	if( pNode->pLeft ){` |
|  804670 | 7018 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7019 | `			ph7_expr_node **apNode;` |
|       - | 7020 | `			sxi32 n;` |
|       - | 7021 | `			/* Recurse and generate bytecodes for function arguments */` |
|  238656 | 7022 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7023 | `			/* Read-only load */` |
|  238656 | 7024 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  470054 | 7025 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  231400 | 7026 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  231400 | 7027 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7028 | `					return rc;` |
|       - | 7029 | `				}` |
|  115701 | 7030 | `			}` |
|       - | 7031 | `			/* Total number of given arguments */` |
|  238656 | 7032 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 7033 | `			/* Remove stale flags now */` |
|  238656 | 7034 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  119327 | 7035 | `		}` |
|  804670 | 7036 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  804670 | 7037 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7038 | `			return rc;` |
|       - | 7039 | `		}` |
|  804670 | 7040 | `		if( iVmOp == PH7_OP_CALL ){` |
|  238656 | 7041 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  238656 | 7042 | `			if( pInstr ){` |
|  238656 | 7043 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  238344 | 7044 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7045 | `					sxu32 nQual;` |
|       - | 7046 | `					/* Prevent constant expansion */` |
|  238344 | 7047 | `					pInstr->iP1 = 0;` |
|       - | 7048 | `					/* Namespace-qualify the function name for CALL */` |
|  238344 | 7049 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  238344 | 7050 | `					pInstr->iP2 = (sxi32)nQual;` |
|  238344 | 7051 | `					if( nQual != nOrig ){` |
|       - | 7052 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 7053 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 7054 | `						 * and try the short name in hHostFunction. */` |
|      49 | 7055 | `						p3 = (void *)1;` |
|      26 | 7056 | `					}` |
|  119485 | 7057 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7058 | `					/* Method call,flag that */` |
|     302 | 7059 | `					pInstr->iP2 = 1;` |
|     150 | 7060 | `				}` |
|  119329 | 7061 | `			}` |
|  685343 | 7062 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7063 | `			ph7_expr_node **apNode;` |
|       - | 7064 | `			sxi32 n;` |
|       - | 7065 | `			/* Recurse and generate bytecodes for array index */` |
|   64138 | 7066 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  115696 | 7067 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   51560 | 7068 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   51560 | 7069 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7070 | `					return rc;` |
|       - | 7071 | `				}` |
|   25781 | 7072 | `			}` |
|   64138 | 7073 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   51560 | 7074 | `				iP1 = 1; /* Node have an index associated with it */` |
|   25779 | 7075 | `			}` |
|   64138 | 7076 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7077 | `				/* Create an empty entry when the desired index is not found */` |
|   25342 | 7078 | `				iP2 = 1;` |
|   12672 | 7079 | `			}` |
|  533948 | 7080 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7081 | `			/* POP the left node */` |
|      32 | 7082 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7083 | `		}` |
|  402334 | 7084 | `	}` |
|  804688 | 7085 | `	rc = SXRET_OK;` |
|  804688 | 7086 | `	nJmpIdx = 0;` |
|       - | 7087 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7088 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7089 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  804688 | 7090 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      98 | 7091 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      98 | 7092 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      98 | 7093 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      98 | 7094 | `			int isSpecial = 0;` |
|      98 | 7095 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      58 | 7096 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      58 | 7097 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      62 | 7098 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      52 | 7099 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      26 | 7100 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      38 | 7101 | `					isSpecial = 1;` |
|      18 | 7102 | `				}` |
|      38 | 7103 | `			}` |
|     118 | 7104 | `			pInstr->iP1 = 0;` |
|     118 | 7105 | `			if( !isSpecial ){` |
|      42 | 7106 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      20 | 7107 | `			}` |
|      38 | 7108 | `		}` |
|      72 | 7109 | `	}` |
|       - | 7110 | `	/* Generate code for the right tree */` |
|  804672 | 7111 | `	if( pNode->pRight ){` |
|  445044 | 7112 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7113 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    7898 | 7114 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  441096 | 7115 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7116 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2652 | 7117 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  435823 | 7118 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  194608 | 7119 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|   97303 | 7120 | `		}` |
|  445044 | 7121 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  445044 | 7122 | `		if( iVmOp == PH7_OP_STORE ){` |
|  191986 | 7123 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  191986 | 7124 | `			if( pInstr ){` |
|  191986 | 7125 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7126 | `					/* Hide the STORE instruction */` |
|      26 | 7127 | `					iVmOp = 0;` |
|  191974 | 7128 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7129 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   42688 | 7130 | `					iP2 = 1;` |
|   21345 | 7131 | `				}else{` |
|  149276 | 7132 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7133 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   25340 | 7134 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   25340 | 7135 | `						iP1 = pInstr->iP1;` |
|   12671 | 7136 | `					}else{` |
|  123938 | 7137 | `						p3 = pInstr->p3;` |
|       - | 7138 | `					}` |
|       - | 7139 | `					/* POP the last dynamic load instruction */` |
|  149276 | 7140 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7141 | `				}` |
|   95994 | 7142 | `			}` |
|  349052 | 7143 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      44 | 7144 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      44 | 7145 | `			if( pInstr ){` |
|      44 | 7146 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7147 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7148 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7149 | `					 */` |
|      15 | 7150 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7151 | `					iP1 = pInstr->iP1;` |
|      15 | 7152 | `					iP2 = pInstr->iP2;` |
|      15 | 7153 | `					p3  = pInstr->p3;` |
|       8 | 7154 | `				}else{` |
|      30 | 7155 | `					p3 = pInstr->p3;` |
|       - | 7156 | `				}` |
|      21 | 7157 | `			}` |
|      21 | 7158 | `		}` |
|  222521 | 7159 | `	}` |
|  804672 | 7160 | `	if( iVmOp > 0 ){` |
|  804618 | 7161 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10244 | 7162 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7163 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7526 | 7164 | `				iP1 = 1;` |
|    3764 | 7165 | `			}` |
|  799497 | 7166 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7167 | `			/* Namespace-qualify the class name for NEW */ {` |
|   12856 | 7168 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   12856 | 7169 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   12844 | 7170 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6421 | 7171 | `				}` |
|   12856 | 7172 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7173 | `					/* Prevent constant expansion for class name */` |
|   12854 | 7174 | `					pPeek->iP1 = 0;` |
|   12854 | 7175 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    6426 | 7176 | `				}` |
|       - | 7177 | `			}` |
|   12856 | 7178 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   12856 | 7179 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7180 | `				VmInstr *pPrev;` |
|   12844 | 7181 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   12844 | 7182 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7183 | `					/* Pop the call instruction */` |
|   12844 | 7184 | `					iP1 = pInstr->iP1;` |
|   12844 | 7185 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6421 | 7186 | `				}` |
|    6423 | 7187 | `			}` |
|  787949 | 7188 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7189 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7190 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7191 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7192 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7193 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7194 | `				int isSpecialIs = 0;` |
|      50 | 7195 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7196 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7197 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7198 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7199 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7200 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7201 | `						isSpecialIs = 1;` |
|       5 | 7202 | `					}` |
|      23 | 7203 | `				}` |
|      52 | 7204 | `				pInstr->iP1 = 0;` |
|      52 | 7205 | `				if( !isSpecialIs ){` |
|      38 | 7206 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 7207 | `				}` |
|      25 | 7208 | `			}` |
|  781501 | 7209 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7210 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7211 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7212 | `			 * should not trigger constant lookup. */` |
|   95892 | 7213 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   95892 | 7214 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   95876 | 7215 | `				pInstr->iP1 = 0;` |
|   47937 | 7216 | `			}` |
|   95892 | 7217 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7218 | `				/* Static member access,remember that */` |
|      82 | 7219 | `				iP1 = 1;` |
|      82 | 7220 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      82 | 7221 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7222 | `					p3 = pInstr->p3;` |
|      10 | 7223 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7224 | `				}` |
|      40 | 7225 | `			}` |
|   47945 | 7226 | `		}` |
|       - | 7227 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  804616 | 7228 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  804616 | 7229 | `		if( nJmpIdx > 0 ){` |
|       - | 7230 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   10548 | 7231 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   10548 | 7232 | `			if( pInstr ){` |
|   10548 | 7233 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5273 | 7234 | `			}` |
|    5273 | 7235 | `		}` |
|  402307 | 7236 | `	}` |
|  804670 | 7237 | `	return rc;` |
| 1043164 | 7238 |  |
|       - | 7239 | `/*` |
|       - | 7240 | ` * Compile a PHP expression.` |
|       - | 7241 | ` * According to the PHP language reference manual:` |
|       - | 7242 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7243 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7244 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7245 | ` *  is "anything that has a value".` |
|       - | 7246 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7247 | ` * function takes care of generating the appropriate error` |
|       - | 7248 | ` * message.` |
|       - | 7249 | ` */` |
|  548654 | 7250 | `static sxi32 PH7_CompileExpr(` |
|       - | 7251 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7252 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7253 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7254 | `	)` |
|       2 | 7255 |  |
|       - | 7256 | `	ph7_expr_node *pRoot;` |
|       - | 7257 | `	SySet sExprNode;` |
|       - | 7258 | `	SyToken *pEnd;` |
|       - | 7259 | `	sxi32 nExpr;` |
|       - | 7260 | `	sxi32 iNest;` |
|       - | 7261 | `	sxi32 rc;` |
|       - | 7262 | `	/* Initialize worker variables */` |
|  548656 | 7263 | `	nExpr = 0;` |
|  548656 | 7264 | `	pRoot = 0;` |
|  548656 | 7265 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  548656 | 7266 | `	SySetAlloc(&sExprNode,0x10);` |
|  548656 | 7267 | `	rc = SXRET_OK;` |
|       - | 7268 | `	/* Delimit the expression */` |
|  548656 | 7269 | `	pEnd = pGen->pIn;` |
|  548656 | 7270 | `	iNest = 0;` |
| 3758282 | 7271 | `	while( pEnd < pGen->pEnd ){` |
| 3560148 | 7272 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7273 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     180 | 7274 | `			iNest++;` |
| 3560059 | 7275 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     188 | 7276 | `			iNest--;` |
| 3559877 | 7277 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  350658 | 7278 | `			if( iNest <= 0 ){` |
|  350522 | 7279 | `				break;` |
|       - | 7280 | `			}` |
|      68 | 7281 | `		}` |
| 3209628 | 7282 | `		pEnd++;` |
|       2 | 7283 | `	}` |
|  548656 | 7284 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10194 | 7285 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10194 | 7286 | `		iNest = 0;` |
|       - | 7287 | `		/* Stop at the first comma */` |
|   20410 | 7288 | `		while( pEnd2 < pEnd ){` |
|   10218 | 7289 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7290 | `				iNest++;` |
|   10216 | 7291 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7292 | `				iNest--;` |
|   10212 | 7293 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7294 | `				if( iNest <= 0 ){` |
|     ! 0 | 7295 | `					break;` |
|       - | 7296 | `				}` |
|       2 | 7297 | `			}` |
|   10218 | 7298 | `			pEnd2++;` |
|       2 | 7299 | `		}` |
|   10194 | 7300 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7301 | `			pEnd = pEnd2;` |
|     ! 0 | 7302 | `		}` |
|    5096 | 7303 | `	}` |
|  548656 | 7304 | `	if( pEnd > pGen->pIn ){` |
|  548646 | 7305 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7306 | `		/* Swap delimiter */` |
|  548646 | 7307 | `		pGen->pEnd = pEnd;` |
|       - | 7308 | `		/* Try to get an expression tree */` |
|  548646 | 7309 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  548646 | 7310 | `		if( rc == SXRET_OK && pRoot ){` |
|  548490 | 7311 | `			rc = SXRET_OK;` |
|  548490 | 7312 | `			if( xTreeValidator ){` |
|       - | 7313 | `				/* Call the upper layer validator callback */` |
|   13002 | 7314 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6500 | 7315 | `			}` |
|  548490 | 7316 | `			if( rc != SXERR_ABORT ){` |
|       - | 7317 | `				/* Generate code for the given tree */` |
|  548490 | 7318 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  274244 | 7319 | `			}` |
|  548490 | 7320 | `			nExpr = 1;` |
|  274244 | 7321 | `		}` |
|       - | 7322 | `		/* Release the whole tree */` |
|  548646 | 7323 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7324 | `		/* Synchronize token stream */` |
|  548646 | 7325 | `		pGen->pEnd = pTmp;` |
|  548646 | 7326 | `		pGen->pIn  = pEnd;` |
|  548646 | 7327 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7328 | `			SySetRelease(&sExprNode);` |
|       3 | 7329 | `			return SXERR_ABORT;` |
|       - | 7330 | `		}` |
|  274321 | 7331 | `	}` |
|  548654 | 7332 | `	SySetRelease(&sExprNode);` |
|  548654 | 7333 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  274329 | 7334 |  |
|       - | 7335 | `/*` |
|       - | 7336 | ` * Return a pointer to the node construct handler associated` |
|       - | 7337 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7338 | ` */` |
|  149472 | 7339 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7340 |  |
|  149474 | 7341 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7342 | `		/* Numeric literal: Either real or integer */` |
|   82110 | 7343 | `		return PH7_CompileNumLiteral;` |
|   67366 | 7344 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7345 | `		/* Double quoted string */` |
|   13506 | 7346 | `		return PH7_CompileString;` |
|   53862 | 7347 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7348 | `		/* Single quoted string */` |
|   53802 | 7349 | `		return PH7_CompileSimpleString;` |
|      62 | 7350 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7351 | `		/* Heredoc */` |
|      28 | 7352 | `		return PH7_CompileHereDoc;` |
|      36 | 7353 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7354 | `		/* Nowdoc */` |
|      29 | 7355 | `		return PH7_CompileNowDoc;` |
|       7 | 7356 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7357 | `		/* Backtick quoted string */` |
|       5 | 7358 | `		return PH7_CompileBacktic;` |
|       - | 7359 | `	}` |
|       3 | 7360 | `	return 0;` |
|   74738 | 7361 |  |
|       - | 7362 | `/*` |
|       - | 7363 | ` * PHP Language construct table.` |
|       - | 7364 | ` */` |
|       - | 7365 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7366 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7367 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7368 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7369 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7370 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7371 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7372 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7373 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7374 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7375 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7376 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7377 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7378 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7379 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7380 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7381 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7382 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7383 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7384 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7385 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7386 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7387 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7388 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */` |
|       - | 7389 | `};` |
|       - | 7390 | `/*` |
|       - | 7391 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7392 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7393 | ` */` |
|  315548 | 7394 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7395 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7396 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7397 | `	)` |
|       2 | 7398 |  |
|  315550 | 7399 | `	sxu32 n = 0;` |
| 1196173 | 7400 | `	for(;;){` |
| 2392348 | 7401 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   33038 | 7402 | `			break;` |
|       - | 7403 | `		}` |
| 2359312 | 7404 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  282514 | 7405 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7406 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7407 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7408 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7409 | `					return 0;` |
|       - | 7410 | `				}` |
|     ! 0 | 7411 | `			}` |
|       - | 7412 | `			/* Return a pointer to the handler.` |
|       - | 7413 | `			*/` |
|  282514 | 7414 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7415 | `		}` |
| 2076800 | 7416 | `		n++;` |
|       2 | 7417 | `	}` |
|   33038 | 7418 | `	if( pLookahed ){` |
|   33038 | 7419 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7548 | 7420 | `			return PH7_CompileClassInterface;` |
|   25492 | 7421 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   22908 | 7422 | `			return PH7_CompileClass;` |
|    2586 | 7423 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      47 | 7424 | `			return PH7_CompileTrait;` |
|    2538 | 7425 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      17 | 7426 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      16 | 7427 | `				return PH7_CompileAbstractClass;` |
|    2524 | 7428 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7429 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7430 | `				return PH7_CompileFinalClass;` |
|       - | 7431 | `		}` |
|    1261 | 7432 | `	}` |
|       - | 7433 | `	/* Not a language construct */` |
|    2524 | 7434 | `	return 0;` |
|  157776 | 7435 |  |
|       - | 7436 | `/*` |
|       - | 7437 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7438 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7439 | ` */` |
|    2522 | 7440 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7441 |  |
|       - | 7442 | `	int rc;` |
|    2524 | 7443 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|    2524 | 7444 | `	if( rc == FALSE ){` |
|      14 | 7445 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       - | 7446 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7447 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7448 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7449 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7450 | `			*/` |
|       - | 7451 | `			){` |
|       6 | 7452 | `				rc = TRUE;` |
|       2 | 7453 | `		}` |
|       6 | 7454 | `	}` |
|    2524 | 7455 | `	return rc;` |
|       2 | 7456 |  |
|       - | 7457 | `/*` |
|       - | 7458 | ` * Compile a PHP chunk.` |
|       - | 7459 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7460 | ` * takes care of generating the appropriate error message.` |
|       - | 7461 | ` */` |
|  448070 | 7462 | `static sxi32 GenStateCompileChunk(` |
|       - | 7463 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7464 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7465 | `	)` |
|       2 | 7466 |  |
|       - | 7467 | `	ProcLangConstruct xCons;` |
|       - | 7468 | `	sxi32 rc;` |
|  448072 | 7469 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  265313 | 7470 | `	for(;;){` |
|  530628 | 7471 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7472 | `			/* No more input to process */` |
|   10714 | 7473 | `			break;` |
|       - | 7474 | `		}` |
|  519916 | 7475 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7476 | `			/* Compile block */` |
|      12 | 7477 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7478 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7479 | `				break;` |
|       - | 7480 | `			}` |
|       7 | 7481 | `		}else{` |
|  519906 | 7482 | `			xCons = 0;` |
|  519906 | 7483 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  315550 | 7484 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7485 | `				/* Try to extract a language construct handler */` |
|  315550 | 7486 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  315550 | 7487 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7488 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7489 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7490 | `						&pGen->pIn->sData);` |
|       9 | 7491 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7492 | `						break;` |
|       - | 7493 | `					}` |
|       - | 7494 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7495 | `					 * this erroneous statement.` |
|       - | 7496 | `					 */` |
|       9 | 7497 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7498 | `				}` |
|  362132 | 7499 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   29464 | 7500 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7501 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7502 | `				xCons = PH7_CompileLabel;` |
|      56 | 7503 | `			}` |
|  519906 | 7504 | `			if( xCons == 0 ){` |
|       - | 7505 | `				/* Assume an expression an try to compile it */` |
|  206760 | 7506 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  206760 | 7507 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7508 | `					/* Pop l-value */` |
|  206634 | 7509 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  103316 | 7510 | `				}` |
|  103381 | 7511 | `			}else{` |
|       - | 7512 | `				/* Go compile the sucker */` |
|  313148 | 7513 | `				rc = xCons(&(*pGen));` |
|       - | 7514 | `			}` |
|  519906 | 7515 | `			if( rc == SXERR_ABORT ){` |
|       - | 7516 | `				/* Request to abort compilation */` |
|       3 | 7517 | `				break;` |
|       - | 7518 | `			}` |
|       - | 7519 | `		}` |
|       - | 7520 | `		/* Ignore trailing semi-colons ';' */` |
|  852526 | 7521 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  332614 | 7522 | `			pGen->pIn++;` |
|       2 | 7523 | `		}` |
|  519914 | 7524 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7525 | `			/* Compile a single statement and return */` |
|  437358 | 7526 | `			break;` |
|       - | 7527 | `		}` |
|       - | 7528 | `		/* LOOP ONE */` |
|       - | 7529 | `		/* LOOP TWO */` |
|       - | 7530 | `		/* LOOP THREE */` |
|       - | 7531 | `		/* LOOP FOUR */` |
|       2 | 7532 | `	}` |
|       - | 7533 | `	/* Return compilation status */` |
|  448072 | 7534 | `	return rc;` |
|       2 | 7535 |  |
|       - | 7536 | `/*` |
|       - | 7537 | ` * Compile a Raw PHP chunk.` |
|       - | 7538 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7539 | ` * takes care of generating the appropriate error message.` |
|       - | 7540 | ` */` |
|   10716 | 7541 | `static sxi32 PH7_CompilePHP(` |
|       - | 7542 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7543 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7544 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7545 | `	)` |
|       2 | 7546 |  |
|   10718 | 7547 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7548 | `	sxi32 rc;` |
|       - | 7549 | `	/* Reset the token set */` |
|   10718 | 7550 | `	SySetReset(&(*pTokenSet));` |
|       - | 7551 | `	/* Mark as the default token set */` |
|   10718 | 7552 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7553 | `	/* Advance the stream cursor */` |
|   10718 | 7554 | `	pGen->pRawIn++;` |
|       - | 7555 | `	/* Tokenize the PHP chunk first */` |
|   10718 | 7556 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7557 | `	/* Point to the head and tail of the token stream. */` |
|   10718 | 7558 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   10718 | 7559 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   10718 | 7560 | `	if( is_expr ){` |
|     ! 0 | 7561 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7562 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7563 | `			/* A simple expression,compile it */` |
|     ! 0 | 7564 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7565 | `		}` |
|       - | 7566 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7568 | `		return SXRET_OK;` |
|       - | 7569 | `	}` |
|   10718 | 7570 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7571 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7572 | `		/*` |
|       - | 7573 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7574 | `		 * According to the PHP reference manual:` |
|       - | 7575 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7576 | `		 *  immediately follow` |
|       - | 7577 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7578 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7579 | `		 * Symisc extension:` |
|       - | 7580 | `		 *   This short syntax works with all PHP opening` |
|       - | 7581 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7582 | `		 *   only short tag.` |
|       - | 7583 | `		 */` |
|       - | 7584 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7585 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7586 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7587 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7588 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7589 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7590 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7591 | `		}` |
|       3 | 7592 | `		return SXRET_OK;` |
|       - | 7593 | `	}` |
|       - | 7594 | `	/* Compile the PHP chunk */` |
|   10716 | 7595 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7596 | `	/* Fix exceptions jumps */` |
|   10716 | 7597 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7598 | `	/* Fix gotos now, the jump destination is resolved */` |
|   10716 | 7599 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7600 | `		rc = SXERR_ABORT;` |
|       1 | 7601 | `	}` |
|       - | 7602 | `	/* Reset container */` |
|   10716 | 7603 | `	SySetReset(&pGen->aGoto);` |
|   10716 | 7604 | `	SySetReset(&pGen->aLabel);` |
|       - | 7605 | `	/* Compilation result */` |
|   10716 | 7606 | `	return rc;` |
|    5360 | 7607 |  |
|       - | 7608 | `/*` |
|       - | 7609 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7610 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7611 | ` * This is the only compile interface exported from this file.` |
|       - | 7612 | ` */` |
|   12560 | 7613 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7614 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7615 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7616 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7617 | `	)` |
|       2 | 7618 |  |
|       - | 7619 | `	SySet aPhpToken,aRawToken;` |
|       - | 7620 | `	ph7_gen_state *pCodeGen;` |
|       - | 7621 | `	ph7_value *pRawObj;` |
|       - | 7622 | `	sxu32 nObjIdx;` |
|       - | 7623 | `	sxi32 nRawObj;` |
|       - | 7624 | `	int is_expr;` |
|       - | 7625 | `	sxi32 rc;` |
|   12562 | 7626 | `	if( pScript->nByte < 1 ){` |
|       - | 7627 | `		/* Nothing to compile */` |
|     ! 0 | 7628 | `		return PH7_OK;` |
|       - | 7629 | `	}` |
|       - | 7630 | `	/* Initialize the tokens containers */` |
|   12562 | 7631 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12562 | 7632 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   12562 | 7633 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   12562 | 7634 | `	is_expr = 0;` |
|   12562 | 7635 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7636 | `		SyToken sTmp;` |
|       - | 7637 | `		/* PHP only: -*/` |
|    2530 | 7638 | `		sTmp.nLine = 1;` |
|    2530 | 7639 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2530 | 7640 | `		sTmp.pUserData = 0;` |
|    2530 | 7641 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2530 | 7642 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2530 | 7643 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7644 | `			/* A simple PHP expression */` |
|     ! 0 | 7645 | `			is_expr = 1;` |
|     ! 0 | 7646 | `		}` |
|    1266 | 7647 | `	}else{` |
|       - | 7648 | `		/* Tokenize raw text */` |
|   10034 | 7649 | `		SySetAlloc(&aRawToken,32);` |
|   10034 | 7650 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7651 | `	}` |
|   12562 | 7652 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7653 | `	/* Process high-level tokens */` |
|   12562 | 7654 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   12562 | 7655 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   12562 | 7656 | `	rc = PH7_OK;` |
|   12562 | 7657 | `	if( is_expr ){` |
|       - | 7658 | `		/* Compile the expression */` |
|     ! 0 | 7659 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 7660 | `		goto cleanup;` |
|       - | 7661 | `	}` |
|   12562 | 7662 | `	nObjIdx = 0;` |
|       - | 7663 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 7664 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 7665 | `	 * preventing namespace bleeding across include()d files. */` |
|   12562 | 7666 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 7667 | `	/* Start the compilation process */` |
|   11300 | 7668 | `	for(;;){` |
|   33314 | 7669 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   12558 | 7670 | `			break; /* No more tokens to process */` |
|       - | 7671 | `		}` |
|   20758 | 7672 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 7673 | `			/* Compile the PHP chunk */` |
|   10718 | 7674 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   10718 | 7675 | `			if( rc == SXERR_ABORT ){` |
|       5 | 7676 | `				break;` |
|       - | 7677 | `			}` |
|   10714 | 7678 | `			continue;` |
|       - | 7679 | `		}` |
|       - | 7680 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10042 | 7681 | `		nRawObj = 0;` |
|   20082 | 7682 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 7683 | `			/* Consume the raw chunk without any processing */` |
|   10042 | 7684 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10042 | 7685 | `			if( pRawObj == 0 ){` |
|     ! 0 | 7686 | `				rc = SXERR_MEM;` |
|     ! 0 | 7687 | `				break;` |
|       - | 7688 | `			}` |
|       - | 7689 | `			/* Mark as constant and emit the load constant instruction */` |
|   10042 | 7690 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10042 | 7691 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10042 | 7692 | `			++nRawObj;` |
|   10042 | 7693 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 7694 | `		}` |
|   10042 | 7695 | `		if( nRawObj > 0 ){` |
|       - | 7696 | `			/* Emit the consume instruction */` |
|   10042 | 7697 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5020 | 7698 | `		}` |
|    6282 | 7699 | `	}` |
|    6280 | 7700 | `cleanup:` |
|   12562 | 7701 | `	SySetRelease(&aRawToken);` |
|   12562 | 7702 | `	SySetRelease(&aPhpToken);` |
|   12562 | 7703 | `	return rc;` |
|    6282 | 7704 |  |
|       - | 7705 | `/*` |
|       - | 7706 | ` * Utility routines.Initialize the code generator.` |
|       - | 7707 | ` */` |
|    2506 | 7708 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 7709 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7710 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7711 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7712 | `	)` |
|       2 | 7713 |  |
|    2508 | 7714 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7715 | `	/* Zero the structure */` |
|    2508 | 7716 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 7717 | `	/* Initial state */` |
|    2508 | 7718 | `	pGen->pVm  = &(*pVm);` |
|    2508 | 7719 | `	pGen->xErr = xErr;` |
|    2508 | 7720 | `	pGen->pErrData = pErrData;` |
|    2508 | 7721 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2508 | 7722 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2508 | 7723 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2508 | 7724 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 7725 | `	/* Error log buffer */` |
|    2508 | 7726 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 7727 | `	/* General purpose working buffer */` |
|    2508 | 7728 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 7729 | `	/* Namespace state */` |
|    2508 | 7730 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2508 | 7731 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7732 | `	/* Create the global scope */` |
|    2508 | 7733 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 7734 | `	/* Point to the global scope */` |
|    2508 | 7735 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2508 | 7736 | `	return SXRET_OK;` |
|       2 | 7737 |  |
|       - | 7738 | `/*` |
|       - | 7739 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 7740 | ` */` |
|   14808 | 7741 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 7742 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 7743 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 7744 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 7745 | `	)` |
|       2 | 7746 |  |
|   14810 | 7747 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 7748 | `	GenBlock *pBlock,*pParent;` |
|       - | 7749 | `	/* Reset state */` |
|   14810 | 7750 | `	SySetReset(&pGen->aLabel);` |
|   14810 | 7751 | `	SySetReset(&pGen->aGoto);` |
|   14810 | 7752 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   14810 | 7753 | `	SyBlobRelease(&pGen->sWorker);` |
|   14810 | 7754 | `	SyBlobRelease(&pGen->sNamespace);` |
|   14810 | 7755 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   14810 | 7756 | `	SyHashRelease(&pGen->hUseImports);` |
|   14810 | 7757 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 7758 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 7759 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 7760 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 7761 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 7762 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 7763 | `	 * number of unique names, which is acceptable. */` |
|       - | 7764 | `	/* Point to the global scope */` |
|   14810 | 7765 | `	pBlock = pGen->pCurrent;` |
|   14810 | 7766 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 7767 | `		pParent = pBlock->pParent;` |
|     ! 0 | 7768 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 7769 | `		pBlock = pParent;` |
|     ! 0 | 7770 | `	}` |
|   14810 | 7771 | `	pGen->xErr = xErr;` |
|   14810 | 7772 | `	pGen->pErrData = pErrData;` |
|   14810 | 7773 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   14810 | 7774 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   14810 | 7775 | `	pGen->pIn = pGen->pEnd = 0;` |
|   14810 | 7776 | `	pGen->nErr = 0;` |
|   14810 | 7777 | `	return SXRET_OK;` |
|       2 | 7778 |  |
|       - | 7779 | `/*` |
|       - | 7780 | ` * Generate a compile-time error message.` |
|       - | 7781 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 7782 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 7783 | ` * abort compilation immediately.` |
|       - | 7784 | ` */` |
|     452 | 7785 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 7786 |  |
|     454 | 7787 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     454 | 7788 | `	const char *zErr = "Error";` |
|       - | 7789 | `	SyString *pFile;` |
|       - | 7790 | `	va_list ap;` |
|       - | 7791 | `	sxi32 rc;` |
|       - | 7792 | `	/* Reset the working buffer */` |
|     454 | 7793 | `	SyBlobReset(pWorker);` |
|       - | 7794 | `	/* Peek the processed file path if available */` |
|     454 | 7795 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     454 | 7796 | `	if( nErrType == E_ERROR ){` |
|       - | 7797 | `		/* Increment the error counter */` |
|     412 | 7798 | `		pGen->nErr++;` |
|     412 | 7799 | `		if( pGen->nErr > 15 ){` |
|       - | 7800 | `			/* Error count limit reached */` |
|       5 | 7801 | `			if( pGen->xErr ){` |
|       5 | 7802 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 7803 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 7804 | `				if( pFile ){` |
|       5 | 7805 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 7806 | `				}` |
|       5 | 7807 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 7808 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 7809 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 7810 | `				}` |
|       2 | 7811 | `			}` |
|       - | 7812 | `			/* Abort immediately */` |
|       5 | 7813 | `			return SXERR_ABORT;` |
|       - | 7814 | `		}` |
|     203 | 7815 | `	}` |
|     450 | 7816 | `	if( pGen->xErr == 0 ){` |
|       - | 7817 | `		/* No available error consumer,return immediately */` |
|       3 | 7818 | `		return SXRET_OK;` |
|       - | 7819 | `	}` |
|     447 | 7820 | `	switch(nErrType){` |
|     405 | 7821 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 7822 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 7823 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 7824 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 7825 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 7826 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 7827 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 7828 | `	default:` |
|     ! 0 | 7829 | `		break;` |
|       - | 7830 | `	}` |
|     447 | 7831 | `	rc = SXRET_OK;` |
|       - | 7832 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     447 | 7833 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     447 | 7834 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     447 | 7835 | `	va_start(ap,zFormat);` |
|     447 | 7836 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 7837 | `	va_end(ap);` |
|     447 | 7838 | `	if( pFile ){` |
|     447 | 7839 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     223 | 7840 | `	}` |
|       - | 7841 | `	/* Append a new line */` |
|     447 | 7842 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 7843 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 7844 | `		/* Consume the generated error message */` |
|     447 | 7845 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 7846 | `	}` |
|     447 | 7847 | `	return rc;` |
|     228 | 7848 |  |
|       - | 7849 |  |
