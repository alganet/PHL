# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3581/4686 lines (76.42%)

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
|    3040 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    3042 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8598 |  131 | `	for(;;){` |
|   17198 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2930 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2930 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2908 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   14292 |  140 | `		pBlock = pBlock->pParent;` |
|   14292 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1522 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  595890 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  595892 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  595892 |  162 | `	pBlock->pUserData   = pUserData;` |
|  595892 |  163 | `	pBlock->pGen        = pGen;` |
|  595892 |  164 | `	pBlock->iFlags      = iType;` |
|  595892 |  165 | `	pBlock->pParent     = 0;` |
|  595892 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  595892 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  595892 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  593094 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  593096 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  593096 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  593096 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  593096 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  593096 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  593096 |  200 | `	pGen->pCurrent = pBlock;` |
|  593096 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  286966 |  203 | `		*ppBlock = pBlock;` |
|  143482 |  204 | `	}` |
|  593096 |  205 | `	return SXRET_OK;` |
|  296549 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  593086 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  593088 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  593088 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  593088 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  593086 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  593088 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  593088 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  593088 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  593088 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  593086 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  593088 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  593088 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  593088 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  593088 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  593088 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  593088 |  244 | `	return SXRET_OK;` |
|  296545 |  245 |  |
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
|  180750 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  180752 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  180752 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  180752 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  180752 |  265 | `	return rc;` |
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
|  421524 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  421526 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  773884 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  352360 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  137278 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  215084 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   34336 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  180750 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  180750 |  298 | `		if( pInstr ){` |
|  180750 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  180750 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  180750 |  302 | `			aFix[n].nJumpType = -1;` |
|   90374 |  303 | `		}` |
|   90376 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  421526 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  160464 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  160466 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  160612 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  160464 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  160596 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  160464 |  358 | `	return SXRET_OK;` |
|   80234 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  523398 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  523400 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  523400 |  367 | `	if( pEntry == 0 ){` |
|  258946 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  264456 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  264456 |  371 | `	return SXRET_OK;` |
|  261701 |  372 |  |
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
|  258944 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  258946 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  258946 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  129472 |  387 | `	}` |
|  258946 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   90942 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   90944 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   90944 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   90944 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   90944 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   90944 |  408 | `	return pObj;` |
|   45473 |  409 |  |
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
|   91344 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   91346 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   91346 |  434 | `	sxu32 nIdx = 0;` |
|   91346 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   90944 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   90944 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   90944 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   90944 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   45473 |  445 | `	}else{` |
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
|   91346 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   91346 |  460 | `	return SXRET_OK;` |
|   45674 |  461 |  |
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
|   59678 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   59680 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   59680 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   59680 |  481 | `	zIn  = pStr->zString;` |
|   59680 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   59680 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   59544 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   17162 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17162 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   42384 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   42384 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   42384 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   42428 |  505 | `	for(;;){` |
|   84858 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   42384 |  508 | `			break;` |
|       - |  509 | `		}` |
|   42476 |  510 | `		zCur = zIn;` |
|  672250 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  629776 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   42476 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   42456 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21227 |  517 | `		}` |
|   42476 |  518 | `		zIn++;` |
|   42476 |  519 | `		if( zIn < zEnd ){` |
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
|   42476 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   42384 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   42384 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   42384 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21191 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   42384 |  543 | `	return SXRET_OK;` |
|   29841 |  544 |  |
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
|    1618 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1620 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1620 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1620 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1620 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1620 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1620 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1620 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1620 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1620 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1620 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1620 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1620 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   15460 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   15462 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   15462 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   15462 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   15462 |  650 | `	(*pCount)++;` |
|   15462 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   15462 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   15462 |  654 | `	return pConstObj;` |
|    7732 |  655 |  |
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
|   14310 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   14312 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   14312 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   14312 |  702 | `	zIn  = pStr->zString;` |
|   14312 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   14312 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     224 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     224 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   14090 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   14090 |  714 | `	iCons = 0;` |
|    7853 |  715 | `	for(;;){` |
|   23692 |  716 | `		zCur = zIn;` |
|  133164 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  111092 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  111008 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1536 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     768 |  722 | `					break;` |
|       - |  723 | `			}` |
|  109474 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   23692 |  726 | `		if( zIn > zCur ){` |
|   11308 |  727 | `			if( pObj == 0 ){` |
|   11032 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11032 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5515 |  732 | `			}` |
|   11308 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5653 |  734 | `		}` |
|   23692 |  735 | `		if( zIn >= zEnd ){` |
|   14090 |  736 | `			break;` |
|       - |  737 | `		}` |
|    9604 |  738 | `		if( zIn[0] == '\\' ){` |
|    7986 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    7986 |  741 | `			zIn++;` |
|    7986 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    7986 |  745 | `			if( pObj == 0 ){` |
|    4432 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4432 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2215 |  750 | `			}` |
|    7986 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    7986 |  752 | `			switch( zIn[0] ){` |
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
|    3627 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7256 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7256 |  776 | `				break;` |
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
|    7986 |  844 | `			zIn += n;` |
|    7986 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1620 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1534 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     766 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3066 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1534 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     766 |  888 | `				for(;;){` |
|    9732 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7434 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1534 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1534 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1534 |  902 | `				if( zIn >= zEnd ){` |
|      90 |  903 | `					break;` |
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
|    1534 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1534 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1534 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1532 |  957 | `				++iCons;` |
|     765 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1620 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   14090 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1238 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     618 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   14090 |  968 | `	return SXRET_OK;` |
|    7157 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   14284 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   14286 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7142 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   14286 |  980 | `	return rc;` |
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
|   16184 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   16186 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   16186 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   16186 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   16186 | 1028 | `	return rc;` |
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
|   24270 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   24272 | 1071 | `	sxi32 iEmitRef = 0;` |
|   24272 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   24272 | 1075 | `	xValidator = 0;` |
|   19611 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   44218 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4996 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   39224 | 1081 | `		pCur = pGen->pIn;` |
|   39224 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   24260 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14966 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14966 | 1090 | `		pKey = pCur;` |
|   14966 | 1091 | `		iNest = 0;` |
|   41718 | 1092 | `		while( pCur < pGen->pIn ){` |
|   27932 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1180 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   26754 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   26716 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   26754 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14966 | 1106 | `		rc = SXERR_EMPTY;` |
|   14966 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   14372 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13788 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14956 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13790 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6894 | 1134 | `		}` |
|   14956 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14954 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14954 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14954 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14954 | 1158 | `		xValidator = 0;` |
|   14954 | 1159 | `		iEmitRef = 0;` |
|   14954 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   24260 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   24260 | 1165 | `	return SXRET_OK;` |
|   12137 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   24120 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   24122 | 1179 | `	pGen->pIn += 2;` |
|   24122 | 1180 | `	pGen->pEnd--;` |
|   12060 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   24122 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     150 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     152 | 1192 | `	pGen->pIn++;` |
|     152 | 1193 | `	pGen->pEnd--;` |
|      75 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     152 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
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
|     146 | 1362 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|     148 | 1375 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     148 | 1376 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1377 | `		pGen->pIn++;` |
|     ! 0 | 1378 | `	}` |
|       - | 1379 | `	/* Reserve a constant for the lambda */` |
|     148 | 1380 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     148 | 1381 | `	if( pObj == 0 ){` |
|     ! 0 | 1382 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1383 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1384 | `		return SXERR_ABORT;` |
|       - | 1385 | `	}` |
|       - | 1386 | `	/* Generate a unique name */` |
|     148 | 1387 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1388 | `	/* Make sure the generated name is unique */` |
|     148 | 1389 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1390 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1391 | `	}` |
|     148 | 1392 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     148 | 1393 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1394 | `	/* Compile the lambda body */` |
|     148 | 1395 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     148 | 1396 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1397 | `		return SXERR_ABORT;` |
|       - | 1398 | `	}` |
|     148 | 1399 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1400 | `		/* Emit the load closure instruction */` |
|      14 | 1401 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1402 | `	}else{` |
|       - | 1403 | `		/* Emit the load constant instruction */` |
|     136 | 1404 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1405 | `	}` |
|       - | 1406 | `	/* Node successfully compiled */` |
|     148 | 1407 | `	return SXRET_OK;` |
|      75 | 1408 |  |
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
|  814574 | 1524 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1525 |  |
|  814576 | 1526 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1527 | `	sxi32 iVv;` |
|       - | 1528 | `	sxi32 iP1;` |
|       - | 1529 | `	void *p3;` |
|       - | 1530 | `	sxi32 rc;` |
|  814576 | 1531 | `	iVv = -1; /* Variable variable counter */` |
| 1629162 | 1532 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  814588 | 1533 | `		pGen->pIn++;` |
|  814588 | 1534 | `		iVv++;` |
|       2 | 1535 | `	}` |
|  814576 | 1536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1537 | `		/* Invalid variable name */` |
|     ! 0 | 1538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1539 | `		if( rc == SXERR_ABORT ){` |
|       - | 1540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1541 | `			return SXERR_ABORT;` |
|       - | 1542 | `		}` |
|     ! 0 | 1543 | `		return SXRET_OK;` |
|       - | 1544 | `	}` |
|  814576 | 1545 | `	p3  = 0;` |
|  814576 | 1546 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  814560 | 1566 | `		char *zName = 0;` |
|       - | 1567 | `		/* Extract variable name */` |
|  814560 | 1568 | `		pName = &pGen->pIn->sData;` |
|       - | 1569 | `		/* Advance the stream cursor */` |
|  814560 | 1570 | `		pGen->pIn++;` |
|  814560 | 1571 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  814560 | 1572 | `		if( pEntry == 0 ){` |
|       - | 1573 | `			/* Duplicate name */` |
|  117280 | 1574 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  117280 | 1575 | `			if( zName == 0 ){` |
|     ! 0 | 1576 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1577 | `				return SXERR_ABORT;` |
|       - | 1578 | `			}` |
|       - | 1579 | `			/* Install in the hashtable */` |
|  117280 | 1580 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   58641 | 1581 | `		}else{` |
|       - | 1582 | `			/* Name already available */` |
|  697282 | 1583 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1584 | `		}` |
|  814560 | 1585 | `		p3 = (void *)zName;` |
|       - | 1586 | `	}` |
|  814572 | 1587 | `	iP1 = 0;` |
|  814572 | 1588 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  312980 | 1589 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1590 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  307134 | 1591 | `			iP1 = 1;` |
|  153566 | 1592 | `		}` |
|  156489 | 1593 | `	}` |
|       - | 1594 | `	/* Emit the load instruction */` |
|  814572 | 1595 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  814584 | 1596 | `	while( iVv > 0 ){` |
|      13 | 1597 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1598 | `		iVv--;` |
|       1 | 1599 | `	}` |
|       - | 1600 | `	/* Node successfully compiled */` |
|  814572 | 1601 | `	return SXRET_OK;` |
|  407289 | 1602 |  |
|       - | 1603 | `/*` |
|       - | 1604 | ` * Load a literal.` |
|       - | 1605 | ` */` |
|  546430 | 1606 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1607 |  |
|  546432 | 1608 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1609 | `	ph7_value *pObj;` |
|       - | 1610 | `	SyString *pStr;` |
|       - | 1611 | `	sxu32 nIdx;` |
|       - | 1612 | `	/* Extract token value */` |
|  546432 | 1613 | `	pStr = &pToken->sData;` |
|       - | 1614 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  546432 | 1615 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   99320 | 1616 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1617 | `			/* NULL constant are always indexed at 0 */` |
|   42244 | 1618 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   42244 | 1619 | `			return SXRET_OK;` |
|   57078 | 1620 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1621 | `			/* TRUE constant are always indexed at 1 */` |
|     474 | 1622 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     474 | 1623 | `			return SXRET_OK;` |
|       2 | 1624 | `		}` |
|  518551 | 1625 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   86270 | 1626 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1627 | `			/* FALSE constant are always indexed at 2 */` |
|   36896 | 1628 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   36896 | 1629 | `			return SXRET_OK;` |
|  448366 | 1630 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   76292 | 1631 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1632 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5596 | 1633 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5596 | 1634 | `			if( pObj == 0 ){` |
|     ! 0 | 1635 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1636 | `				return SXERR_ABORT;` |
|       - | 1637 | `			}` |
|    5596 | 1638 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1639 | `			/* Emit the load constant instruction */` |
|    5596 | 1640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5596 | 1641 | `			return SXRET_OK;` |
|  418717 | 1642 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   28182 | 1643 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  417783 | 1659 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11742 | 1660 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  411906 | 1661 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14590 | 1662 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  461214 | 1692 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1693 | `		ph7_value *pLitObj;` |
|       - | 1694 | `		/* Unknown literal,install it in the literal table */` |
|  216156 | 1695 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  216156 | 1696 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1697 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1698 | `			return SXERR_ABORT;` |
|       - | 1699 | `		}` |
|  216156 | 1700 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  216156 | 1701 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  108077 | 1702 | `	}` |
|       - | 1703 | `	/* Emit the load constant instruction */` |
|  461214 | 1704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  461214 | 1705 | `	return SXRET_OK;` |
|  273217 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1709 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1710 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1711 | ` * Otherwise, load the simple literal directly.` |
|       - | 1712 | ` */` |
|  546450 | 1713 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1714 |  |
|       - | 1715 | `	sxi32 rc;` |
|  546452 | 1716 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1717 | `		return SXRET_OK;` |
|       - | 1718 | `	}` |
|       - | 1719 | `	/* Check if this is a multi-token namespace path */` |
|  546452 | 1720 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  546432 | 1770 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  546432 | 1771 | `	return rc;` |
|  273227 | 1772 |  |
|       - | 1773 | `/*` |
|       - | 1774 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1775 | ` */` |
|  546450 | 1776 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1777 |  |
|       - | 1778 | `	sxi32 rc;` |
|  546452 | 1779 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  546452 | 1780 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1781 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1782 | `		return rc;` |
|       - | 1783 | `	}` |
|       - | 1784 | `	/* Node successfully compiled */` |
|  546452 | 1785 | `	return SXRET_OK;` |
|  273227 | 1786 |  |
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
|    2902 | 1938 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1939 |  |
|    2904 | 1940 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17044 | 1941 | `	while( pBlock && pBlock != pTarget ){` |
|   14142 | 1942 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   14142 | 1954 | `		pBlock = pBlock->pParent;` |
|       2 | 1955 | `	}` |
|    2904 | 1956 |  |
|    2836 | 1957 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 1958 |  |
|       - | 1959 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 1960 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 1961 | `	sxu32 nLineLocal;` |
|       - | 1962 | `	sxi32 rc;` |
|    2838 | 1963 | `	nLineLocal = pGen->pIn->nLine;` |
|    2838 | 1964 | `	iLevel = 0;` |
|       - | 1965 | `	/* Jump the 'continue' keyword */` |
|    2838 | 1966 | `	pGen->pIn++;` |
|    2838 | 1967 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2838 | 1978 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2838 | 1979 | `	if( pLoop == 0 ){` |
|       - | 1980 | `		/* Illegal continue */` |
|      11 | 1981 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 1982 | `		if( rc == SXERR_ABORT ){` |
|       - | 1983 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1984 | `			return SXERR_ABORT;` |
|       - | 1985 | `		}` |
|       6 | 1986 | `	}else{` |
|    2828 | 1987 | `		sxu32 nInstrIdx = 0;` |
|       - | 1988 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2828 | 1989 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2828 | 1990 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2824 | 2002 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2824 | 2003 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2004 | `				JumpFixup sJumpFix;` |
|       - | 2005 | `				/* Post-continue */` |
|      10 | 2006 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      10 | 2007 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      10 | 2008 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2009 | `			}` |
|       - | 2010 | `		}` |
|       - | 2011 | `	}` |
|    2838 | 2012 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2013 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2014 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2015 | `	}` |
|       - | 2016 | `	/* Statement successfully compiled */` |
|    2838 | 2017 | `	return SXRET_OK;` |
|    1420 | 2018 |  |
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
|  307482 | 2280 | `static sxi32 PH7_CompileBlock(` |
|       - | 2281 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2282 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2283 | `	)` |
|       2 | 2284 |  |
|       - | 2285 | `	sxi32 rc;` |
|       - | 2286 | `	sxu32 nLine;` |
|  307484 | 2287 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  306132 | 2288 | `		nLine = pGen->pIn->nLine;` |
|  306132 | 2289 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  306132 | 2290 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2291 | `			return SXERR_ABORT;` |
|       - | 2292 | `		}` |
|  306132 | 2293 | `		pGen->pIn++;` |
|       - | 2294 | `		/* Compile until we hit the closing braces '}' */` |
|  422738 | 2295 | `		for(;;){` |
|  845478 | 2296 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  845458 | 2307 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2308 | `				/* Closing braces found,break immediately*/` |
|  306112 | 2309 | `				pGen->pIn++;` |
|  306112 | 2310 | `				break;` |
|       - | 2311 | `			}` |
|       - | 2312 | `			/* Compile a single statement */` |
|  539348 | 2313 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  539348 | 2314 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2315 | `				return SXERR_ABORT;` |
|       - | 2316 | `			}` |
|       2 | 2317 | `		}` |
|  306132 | 2318 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  154419 | 2319 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|  307484 | 2369 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `	}` |
|  307484 | 2372 | `	return SXRET_OK;` |
|  153743 | 2373 |  |
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
|   11286 | 2393 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2394 |  |
|   11288 | 2395 | `	GenBlock *pWhileBlock = 0;` |
|   11288 | 2396 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2397 | `	sxu32 nFalseJump;` |
|       - | 2398 | `	sxu32 nLine;` |
|       - | 2399 | `	sxi32 rc;` |
|   11288 | 2400 | `	nLine = pGen->pIn->nLine;` |
|       - | 2401 | `	/* Jump the 'while' keyword */` |
|   11288 | 2402 | `	pGen->pIn++;` |
|   11288 | 2403 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2404 | `		/* Syntax error */` |
|     ! 0 | 2405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2406 | `		if( rc == SXERR_ABORT ){` |
|       - | 2407 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2408 | `			return SXERR_ABORT;` |
|       - | 2409 | `		}` |
|     ! 0 | 2410 | `		goto Synchronize;` |
|       - | 2411 | `	}` |
|       - | 2412 | `	/* Jump the left parenthesis '(' */` |
|   11288 | 2413 | `	pGen->pIn++;` |
|       - | 2414 | `	/* Create the loop block */` |
|   11288 | 2415 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11288 | 2416 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2417 | `		return SXERR_ABORT;` |
|       - | 2418 | `	}` |
|       - | 2419 | `	/* Delimit the condition */` |
|   11288 | 2420 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11288 | 2421 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2422 | `		/* Empty expression */` |
|       3 | 2423 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2424 | `		if( rc == SXERR_ABORT ){` |
|       - | 2425 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2426 | `			return SXERR_ABORT;` |
|       - | 2427 | `		}` |
|       1 | 2428 | `	}` |
|       - | 2429 | `	/* Swap token streams */` |
|   11288 | 2430 | `	pTmp = pGen->pEnd;` |
|   11288 | 2431 | `	pGen->pEnd = pEnd;` |
|       - | 2432 | `	/* Compile the expression */` |
|   11288 | 2433 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11288 | 2434 | `	if( rc == SXERR_ABORT ){` |
|       - | 2435 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2436 | `		return SXERR_ABORT;` |
|       - | 2437 | `	}` |
|       - | 2438 | `	/* Update token stream */` |
|   11288 | 2439 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2440 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2441 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2442 | `			return SXERR_ABORT;` |
|       - | 2443 | `		}` |
|     ! 0 | 2444 | `		pGen->pIn++;` |
|     ! 0 | 2445 | `	}` |
|       - | 2446 | `	/* Synchronize pointers */` |
|   11288 | 2447 | `	pGen->pIn  = &pEnd[1];` |
|   11288 | 2448 | `	pGen->pEnd = pTmp;` |
|       - | 2449 | `	/* Emit the false jump */` |
|   11288 | 2450 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2451 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11288 | 2452 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2453 | `	/* Compile the loop body */` |
|   11288 | 2454 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11288 | 2455 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2456 | `		return SXERR_ABORT;` |
|       - | 2457 | `	}` |
|       - | 2458 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11288 | 2459 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2460 | `	/* Fix all jumps now the destination is resolved */` |
|   11288 | 2461 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2462 | `	/* Release the loop block */` |
|   11288 | 2463 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2464 | `	/* Statement successfully compiled */` |
|   11288 | 2465 | `	return SXRET_OK;` |
|     ! 0 | 2466 | `Synchronize:` |
|       - | 2467 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2468 | `	 * compiling this erroneous block.` |
|       - | 2469 | `	 */` |
|     ! 0 | 2470 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2471 | `		pGen->pIn++;` |
|     ! 0 | 2472 | `	}` |
|     ! 0 | 2473 | `	return SXRET_OK;` |
|    5645 | 2474 |  |
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
|   11270 | 2622 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2623 |  |
|   11272 | 2624 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11272 | 2625 | `	GenBlock *pForBlock = 0;` |
|       - | 2626 | `	sxu32 nFalseJump;` |
|       - | 2627 | `	sxu32 nLine;` |
|       - | 2628 | `	sxi32 rc;` |
|   11272 | 2629 | `	nLine = pGen->pIn->nLine;` |
|       - | 2630 | `	/* Jump the 'for' keyword */` |
|   11272 | 2631 | `	pGen->pIn++;` |
|   11272 | 2632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2633 | `		/* Syntax error */` |
|     ! 0 | 2634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2635 | `		if( rc == SXERR_ABORT ){` |
|       - | 2636 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2637 | `			return SXERR_ABORT;` |
|       - | 2638 | `		}` |
|     ! 0 | 2639 | `		return SXRET_OK;` |
|       - | 2640 | `	}` |
|       - | 2641 | `	/* Jump the left parenthesis '(' */` |
|   11272 | 2642 | `	pGen->pIn++;` |
|       - | 2643 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11272 | 2644 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11272 | 2645 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   11272 | 2660 | `	pTmp = pGen->pEnd;` |
|   11272 | 2661 | `	pGen->pEnd = pEnd;` |
|       - | 2662 | `	/* Compile initialization expressions if available */` |
|   11272 | 2663 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2664 | `	/* Pop operand lvalues */` |
|   11272 | 2665 | `	if( rc == SXERR_ABORT ){` |
|       - | 2666 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2667 | `		return SXERR_ABORT;` |
|   11272 | 2668 | `	}else if( rc != SXERR_EMPTY ){` |
|   11270 | 2669 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5634 | 2670 | `	}` |
|   11272 | 2671 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11272 | 2682 | `	pGen->pIn++;` |
|       - | 2683 | `	/* Create the loop block */` |
|   11272 | 2684 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11272 | 2685 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2686 | `		return SXERR_ABORT;` |
|       - | 2687 | `	}` |
|       - | 2688 | `	/* Deffer continue jumps */` |
|   11272 | 2689 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2690 | `	/* Compile the condition */` |
|   11272 | 2691 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11272 | 2692 | `	if( rc == SXERR_ABORT ){` |
|       - | 2693 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2694 | `		return SXERR_ABORT;` |
|   11272 | 2695 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2696 | `		/* Emit the false jump */` |
|   11270 | 2697 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2698 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11270 | 2699 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5634 | 2700 | `	}` |
|   11272 | 2701 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11268 | 2712 | `	pGen->pIn++;` |
|       - | 2713 | `	/* Save the post condition stream */` |
|   11268 | 2714 | `	pPostStart = pGen->pIn;` |
|       - | 2715 | `	/* Compile the loop body */` |
|   11268 | 2716 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11268 | 2717 | `	pGen->pEnd = pTmp;` |
|   11268 | 2718 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11268 | 2719 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2720 | `		return SXERR_ABORT;` |
|       - | 2721 | `	}` |
|       - | 2722 | `	/* Fix post-continue jumps */` |
|   11268 | 2723 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   11268 | 2739 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2740 | `		pPostStart++;` |
|     ! 0 | 2741 | `	}` |
|   11268 | 2742 | `	if( pPostStart < pEnd ){` |
|       - | 2743 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11268 | 2744 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11268 | 2745 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11268 | 2746 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2747 | `			/* Syntax error */` |
|     ! 0 | 2748 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2749 | `			if( rc == SXERR_ABORT ){` |
|       - | 2750 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2751 | `				return SXERR_ABORT;` |
|       - | 2752 | `			}` |
|     ! 0 | 2753 | `			return SXRET_OK;` |
|       - | 2754 | `		}` |
|   11268 | 2755 | `		RE_SWAP_DELIMITER(pGen);` |
|   11268 | 2756 | `		if( rc == SXERR_ABORT ){` |
|       - | 2757 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2758 | `			return SXERR_ABORT;` |
|   11268 | 2759 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2760 | `			/* Pop operand lvalue */` |
|   11268 | 2761 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5633 | 2762 | `		}` |
|    5633 | 2763 | `	}` |
|       - | 2764 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11268 | 2765 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2766 | `	/* Fix all jumps now the destination is resolved */` |
|   11268 | 2767 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2768 | `	/* Release the loop block */` |
|   11268 | 2769 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2770 | `	/* Statement successfully compiled */` |
|   11268 | 2771 | `	return SXRET_OK;` |
|    5637 | 2772 |  |
|       - | 2773 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2774 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2775 | ` * are allowed.` |
|       - | 2776 | ` */` |
|    5974 | 2777 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2778 |  |
|    5976 | 2779 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5976 | 2780 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2781 | `		/* Unexpected expression */` |
|     ! 0 | 2782 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2783 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2784 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2785 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2786 | `		}` |
|     ! 0 | 2787 | `	}` |
|    5976 | 2788 | `	return rc;` |
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
|    3028 | 2816 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2817 |  |
|    3030 | 2818 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3030 | 2819 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3030 | 2820 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2821 | `	ph7_foreach_info *pInfo;` |
|       - | 2822 | `	sxu32 nFalseJump;` |
|       - | 2823 | `	VmInstr *pInstr;` |
|       - | 2824 | `	sxu32 nLine;` |
|       - | 2825 | `	sxi32 rc;` |
|    3030 | 2826 | `	nLine = pGen->pIn->nLine;` |
|       - | 2827 | `	/* Jump the 'foreach' keyword */` |
|    3030 | 2828 | `	pGen->pIn++;` |
|    3030 | 2829 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2830 | `		/* Syntax error */` |
|     ! 0 | 2831 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2832 | `		if( rc == SXERR_ABORT ){` |
|       - | 2833 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2834 | `			return SXERR_ABORT;` |
|       - | 2835 | `		}` |
|     ! 0 | 2836 | `		goto Synchronize;` |
|       - | 2837 | `	}` |
|       - | 2838 | `	/* Jump the left parenthesis '(' */` |
|    3030 | 2839 | `	pGen->pIn++;` |
|       - | 2840 | `	/* Create the loop block */` |
|    3030 | 2841 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3030 | 2842 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2843 | `		return SXERR_ABORT;` |
|       - | 2844 | `	}` |
|       - | 2845 | `	/* Delimit the expression */` |
|    3030 | 2846 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3030 | 2847 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    3030 | 2862 | `	pCur = pGen->pIn;` |
|   20368 | 2863 | `	while( pCur < pEnd ){` |
|   20368 | 2864 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3040 | 2865 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3040 | 2866 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2867 | `				/* Break with the first 'as' found */` |
|    3030 | 2868 | `				break;` |
|       - | 2869 | `			}` |
|       5 | 2870 | `		}` |
|       - | 2871 | `		/* Advance the stream cursor */` |
|   17340 | 2872 | `		pCur++;` |
|       2 | 2873 | `	}` |
|    3030 | 2874 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2875 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2876 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2877 | `		if( rc == SXERR_ABORT ){` |
|       - | 2878 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2879 | `			return SXERR_ABORT;` |
|       - | 2880 | `		}` |
|     ! 0 | 2881 | `		goto Synchronize;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Swap token streams */` |
|    3030 | 2884 | `	pTmp = pGen->pEnd;` |
|    3030 | 2885 | `	pGen->pEnd = pCur;` |
|    3030 | 2886 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3030 | 2887 | `	if( rc == SXERR_ABORT ){` |
|       - | 2888 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2889 | `		return SXERR_ABORT;` |
|       - | 2890 | `	}` |
|       - | 2891 | `	/* Update token stream */` |
|    3030 | 2892 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2893 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2894 | `		if( rc == SXERR_ABORT ){` |
|       - | 2895 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2896 | `			return SXERR_ABORT;` |
|       - | 2897 | `		}` |
|     ! 0 | 2898 | `		pGen->pIn++;` |
|     ! 0 | 2899 | `	}` |
|    3030 | 2900 | `	pCur++; /* Jump the 'as' keyword */` |
|    3030 | 2901 | `	pGen->pIn = pCur;` |
|    3030 | 2902 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2903 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2905 | `			return SXERR_ABORT;` |
|       - | 2906 | `		}` |
|     ! 0 | 2907 | `	}` |
|       - | 2908 | `	/* Create the foreach context */` |
|    3030 | 2909 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3030 | 2910 | `	if( pInfo == 0 ){` |
|     ! 0 | 2911 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2912 | `		return SXERR_ABORT;` |
|       - | 2913 | `	}` |
|       - | 2914 | `	/* Zero the structure */` |
|    3030 | 2915 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2916 | `	/* Initialize structure fields */` |
|    3030 | 2917 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2918 | `	/* Check if we have a key field */` |
|    9126 | 2919 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6098 | 2920 | `		pCur++;` |
|       2 | 2921 | `	}` |
|    3030 | 2922 | `	if( pCur < pEnd ){` |
|       - | 2923 | `		/* Compile the expression holding the key name */` |
|    2956 | 2924 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2925 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2926 | `			if( rc == SXERR_ABORT ){` |
|       - | 2927 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2928 | `				return SXERR_ABORT;` |
|       - | 2929 | `			}` |
|     ! 0 | 2930 | `		}else{` |
|    2956 | 2931 | `			pGen->pEnd = pCur;` |
|    2956 | 2932 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2956 | 2933 | `			if( rc == SXERR_ABORT ){` |
|       - | 2934 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2935 | `				return SXERR_ABORT;` |
|       - | 2936 | `			}` |
|    2956 | 2937 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2956 | 2938 | `			if( pInstr->p3 ){` |
|       - | 2939 | `				/* Record key name */` |
|    2956 | 2940 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1477 | 2941 | `			}` |
|    2956 | 2942 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2943 | `		}` |
|    2956 | 2944 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1477 | 2945 | `	}` |
|    3030 | 2946 | `	pGen->pEnd = pEnd;` |
|    3030 | 2947 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2948 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2949 | `		if( rc == SXERR_ABORT ){` |
|       - | 2950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2951 | `			return SXERR_ABORT;` |
|       - | 2952 | `		}` |
|     ! 0 | 2953 | `		goto Synchronize;` |
|       - | 2954 | `	}` |
|    3030 | 2955 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      12 | 2956 | `		pGen->pIn++;` |
|       - | 2957 | `		/* Pass by reference  */` |
|      12 | 2958 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 2959 | `	}` |
|       - | 2960 | `	/* Check if the value target is list() */` |
|    3030 | 2961 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    3022 | 3004 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3022 | 3005 | `		if( rc == SXERR_ABORT ){` |
|       - | 3006 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3007 | `			return SXERR_ABORT;` |
|       - | 3008 | `		}` |
|    3022 | 3009 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3022 | 3010 | `		if( pInstr->p3 ){` |
|       - | 3011 | `			/* Record value name */` |
|    3022 | 3012 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1510 | 3013 | `		}` |
|       - | 3014 | `	}` |
|       - | 3015 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3028 | 3016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3017 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3028 | 3018 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3019 | `	/* Record the first instruction to execute */` |
|    3028 | 3020 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3021 | `	/* Emit the FOREACH_STEP instruction */` |
|    3028 | 3022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3023 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3028 | 3024 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3025 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3028 | 3026 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    3028 | 3050 | `	pGen->pIn = &pEnd[1];` |
|    3028 | 3051 | `	pGen->pEnd = pTmp;` |
|    3028 | 3052 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3028 | 3053 | `	if( rc == SXERR_ABORT ){` |
|       - | 3054 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3055 | `		return SXERR_ABORT;` |
|       - | 3056 | `	}` |
|       - | 3057 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3028 | 3058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3059 | `	/* Fix all jumps now the destination is resolved */` |
|    3028 | 3060 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3061 | `	/* Release the loop block */` |
|    3028 | 3062 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3063 | `	/* Statement successfully compiled */` |
|    3028 | 3064 | `	return SXRET_OK;` |
|       1 | 3065 | `Synchronize:` |
|       - | 3066 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3067 | `	 * compiling this erroneous block.` |
|       - | 3068 | `	 */` |
|       3 | 3069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3070 | `		pGen->pIn++;` |
|     ! 0 | 3071 | `	}` |
|       3 | 3072 | `	return SXRET_OK;` |
|    1516 | 3073 |  |
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
|  112052 | 3106 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3107 |  |
|  112054 | 3108 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  112054 | 3109 | `	GenBlock *pCondBlock = 0;` |
|       - | 3110 | `	sxu32 nJumpIdx;` |
|       - | 3111 | `	sxu32 nKeyID;` |
|       - | 3112 | `	sxi32 rc;` |
|       - | 3113 | `	/* Jump the 'if' keyword */` |
|  112054 | 3114 | `	pGen->pIn++;` |
|  112054 | 3115 | `	pToken = pGen->pIn;` |
|       - | 3116 | `	/* Create the conditional block */` |
|  112054 | 3117 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  112054 | 3118 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3119 | `		return SXERR_ABORT;` |
|       - | 3120 | `	}` |
|       - | 3121 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   61630 | 3122 | `	for(;;){` |
|  123262 | 3123 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  123262 | 3136 | `		pToken++;` |
|       - | 3137 | `		/* Delimit the condition */` |
|  123262 | 3138 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  123262 | 3139 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  123262 | 3152 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3153 | `		/* Compile the condition */` |
|  123262 | 3154 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3155 | `		/* Update token stream */` |
|  123262 | 3156 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3157 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3158 | `			pGen->pIn++;` |
|     ! 0 | 3159 | `		}` |
|  123262 | 3160 | `		pGen->pIn  = &pEnd[1];` |
|  123262 | 3161 | `		pGen->pEnd = pTmp;` |
|  123262 | 3162 | `		if( rc == SXERR_ABORT ){` |
|       - | 3163 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3164 | `			return SXERR_ABORT;` |
|       - | 3165 | `		}` |
|       - | 3166 | `		/* Emit the false jump */` |
|  123262 | 3167 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3168 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  123262 | 3169 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3170 | `		/* Compile the body */` |
|  123262 | 3171 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  123262 | 3172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3173 | `			return SXERR_ABORT;` |
|       - | 3174 | `		}` |
|  123262 | 3175 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   33169 | 3176 | `			break;` |
|       - | 3177 | `		}` |
|       - | 3178 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   56928 | 3179 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   56928 | 3180 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   36612 | 3181 | `			break;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Emit the unconditional jump */` |
|   20318 | 3184 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3185 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   20318 | 3186 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   20318 | 3187 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14702 | 3188 | `			pToken = &pGen->pIn[1];` |
|   14702 | 3189 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5622 | 3190 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4556 | 3191 | `					break;` |
|       - | 3192 | `			}` |
|    5594 | 3193 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2796 | 3194 | `		}` |
|   11210 | 3195 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3196 | `		/* Synchronize cursors */` |
|   11210 | 3197 | `		pToken = pGen->pIn;` |
|       - | 3198 | `		/* Fix the false jump */` |
|   11210 | 3199 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3200 | `	} /* For(;;) */` |
|       - | 3201 | `	/* Fix the false jump */` |
|  112054 | 3202 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  112054 | 3203 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   45718 | 3204 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3205 | `			/* Compile the else block */` |
|    9110 | 3206 | `			pGen->pIn++;` |
|    9110 | 3207 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9110 | 3208 | `			if( rc == SXERR_ABORT ){` |
|       - | 3209 |  |
|     ! 0 | 3210 | `				return SXERR_ABORT;` |
|       - | 3211 | `			}` |
|    4554 | 3212 | `	}` |
|  112054 | 3213 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3214 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  112054 | 3215 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3216 | `	/* Release the conditional block */` |
|  112054 | 3217 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3218 | `	/* Statement successfully compiled */` |
|  112054 | 3219 | `	return SXRET_OK;` |
|     ! 0 | 3220 | `Synchronize:` |
|       - | 3221 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3222 | `	 */` |
|     ! 0 | 3223 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3224 | `		pGen->pIn++;` |
|     ! 0 | 3225 | `	}` |
|     ! 0 | 3226 | `	return SXRET_OK;` |
|   56028 | 3227 |  |
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
|  162734 | 3321 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3322 |  |
|  162736 | 3323 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3324 | `	sxi32 rc;` |
|       - | 3325 | `	/* Jump the 'return' keyword */` |
|  162736 | 3326 | `	pGen->pIn++;` |
|  162736 | 3327 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3328 | `		/* Compile the expression */` |
|  162714 | 3329 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  162714 | 3330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3331 | `			return SXERR_ABORT;` |
|  162714 | 3332 | `		}else if(rc != SXERR_EMPTY ){` |
|  162714 | 3333 | `			nRet = 1;` |
|   81356 | 3334 | `		}` |
|   81356 | 3335 | `	}` |
|       - | 3336 | `	/* Emit the done instruction */` |
|  162736 | 3337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  162736 | 3338 | `	return SXRET_OK;` |
|   81369 | 3339 |  |
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
|   10128 | 3430 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3431 |  |
|   10130 | 3432 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3433 | `	sxi32 rc;` |
|       - | 3434 | `	/* Jump the 'echo' keyword */` |
|   10130 | 3435 | `	pGen->pIn++;` |
|       - | 3436 | `	/* Compile arguments one after one */` |
|   10130 | 3437 | `	pTmp = pGen->pEnd;` |
|   20646 | 3438 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   10518 | 3439 | `		if( pGen->pIn < pNext ){` |
|   10518 | 3440 | `			pGen->pEnd = pNext;` |
|   10518 | 3441 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   10518 | 3442 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3443 | `				return SXERR_ABORT;` |
|   10518 | 3444 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3445 | `				/* Emit the consume instruction */` |
|   10494 | 3446 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5246 | 3447 | `			}` |
|    5258 | 3448 | `		}` |
|       - | 3449 | `		/* Jump trailing commas */` |
|   10906 | 3450 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3451 | `			pNext++;` |
|       2 | 3452 | `		}` |
|   10518 | 3453 | `		pGen->pIn = pNext;` |
|       2 | 3454 | `	}` |
|       - | 3455 | `	/* Restore token stream */` |
|   10130 | 3456 | `	pGen->pEnd = pTmp;` |
|   10130 | 3457 | `	return SXRET_OK;` |
|    5066 | 3458 |  |
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
|  333186 | 3620 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx)` |
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
|  333188 | 3631 | `	if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  333082 | 3632 | `		return nOrigIdx; /* Not in a namespace */` |
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
|  166595 | 3673 |  |
|       - | 3674 | `/*` |
|       - | 3675 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3676 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3677 | ` */` |
|   22522 | 3678 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3679 |  |
|       - | 3680 | `	SyHashEntry *pImport;` |
|       - | 3681 | `	/* Check use imports first */` |
|   22524 | 3682 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   22524 | 3683 | `	if( pImport ){` |
|       7 | 3684 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       7 | 3685 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       7 | 3686 | `		return;` |
|       - | 3687 | `	}` |
|       - | 3688 | `	/* Prepend current namespace if active */` |
|   22518 | 3689 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 3690 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 3691 | `		SyBlobAppend(pOut,"\\",1);` |
|       1 | 3692 | `	}` |
|   22518 | 3693 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   11263 | 3694 |  |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3697 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3698 | ` * The caller must release pOut when done.` |
|       - | 3699 | ` */` |
|   42412 | 3700 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3701 |  |
|   42414 | 3702 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      33 | 3703 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      33 | 3704 | `		SyBlobAppend(pOut,"\\",1);` |
|      16 | 3705 | `	}` |
|   42414 | 3706 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   42414 | 3707 |  |
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
|   44746 | 4036 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4037 |  |
|       - | 4038 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4039 | `	SySet *pInstrContainer;` |
|       - | 4040 | `	sxi32 rc;` |
|       - | 4041 | `	/* Swap token stream */` |
|   44748 | 4042 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   44748 | 4043 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   44748 | 4044 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4045 | `	/* Compile the expression holding the argument value */` |
|   44748 | 4046 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4047 | `	/* Emit the done instruction */` |
|   44748 | 4048 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   44748 | 4049 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   44748 | 4050 | `	RE_SWAP_DELIMITER(pGen);` |
|   44748 | 4051 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4052 | `		return SXERR_ABORT;` |
|       - | 4053 | `	}` |
|   44748 | 4054 | `	return SXRET_OK;` |
|   22375 | 4055 |  |
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
|   53610 | 4093 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4094 |  |
|       - | 4095 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4096 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4097 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4098 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4099 | `	sxi32 rc;` |
|       - | 4100 |  |
|   53612 | 4101 | `	pIn = pGen->pIn;` |
|   53612 | 4102 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4103 | `	/* Process arguments one after one */` |
|   67827 | 4104 | `	for(;;){` |
|  135656 | 4105 | `		if( pIn >= pEnd ){` |
|       - | 4106 | `			/* No more arguments to process */` |
|   53610 | 4107 | `			break;` |
|       - | 4108 | `		}` |
|   82048 | 4109 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   82048 | 4110 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   82048 | 4111 | `		if( pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|   55926 | 4112 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   50334 | 4113 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   50334 | 4114 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4115 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   50334 | 4116 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4117 | `					sArg.nType = MEMOBJ_BOOL;` |
|   50334 | 4118 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13982 | 4119 | `					sArg.nType = MEMOBJ_INT;` |
|   43344 | 4120 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   36352 | 4121 | `					sArg.nType = MEMOBJ_STRING;` |
|   18178 | 4122 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4123 | `					sArg.nType = MEMOBJ_REAL;` |
|     ! 0 | 4124 | `				}else{` |
|       4 | 4125 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4126 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4127 | `						&pIn->sData);` |
|       - | 4128 | `				}` |
|   25168 | 4129 | `			}else{` |
|    5594 | 4130 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4131 | `				char *zDupLocal;` |
|       - | 4132 | `				/* Argument must be a class instance,record that*/` |
|    5594 | 4133 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5594 | 4134 | `				if( zDupLocal ){` |
|    5594 | 4135 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5594 | 4136 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2796 | 4137 | `				}` |
|       - | 4138 | `			}` |
|   55926 | 4139 | `			pIn++;` |
|   27962 | 4140 | `		}` |
|   82048 | 4141 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4142 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4143 | `			return rc;` |
|       - | 4144 | `		}` |
|   82048 | 4145 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4146 | `			/* Pass by reference,record that */` |
|    2820 | 4147 | `			sArg.iFlags = VM_FUNC_ARG_BY_REF;` |
|    2820 | 4148 | `			pIn++;` |
|    1409 | 4149 | `		}` |
|   82048 | 4150 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4151 | `			/* Invalid argument */` |
|     ! 0 | 4152 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4153 | `			return rc;` |
|       - | 4154 | `		}` |
|   82048 | 4155 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4156 | `		/* Copy argument name */` |
|   82048 | 4157 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   82048 | 4158 | `		if( zDup == 0 ){` |
|     ! 0 | 4159 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4160 | `			return SXERR_ABORT;` |
|       - | 4161 | `		}` |
|   82048 | 4162 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   82048 | 4163 | `		pIn++;` |
|   82048 | 4164 | `		if( pIn < pEnd ){` |
|   50820 | 4165 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4166 | `				SyToken *pDefend;` |
|   44750 | 4167 | `				sxi32 iNest = 0;` |
|   44750 | 4168 | `				pIn++; /* Jump the equal sign */` |
|   44750 | 4169 | `				pDefend = pIn;` |
|       - | 4170 | `				/* Process the default value associated with this argument */` |
|   95088 | 4171 | `				while( pDefend < pEnd ){` |
|   72708 | 4172 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   22370 | 4173 | `						break;` |
|       - | 4174 | `					}` |
|   50340 | 4175 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4176 | `						/* Increment nesting level */` |
|    2798 | 4177 | `						iNest++;` |
|   48942 | 4178 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4179 | `						/* Decrement nesting level */` |
|    2798 | 4180 | `						iNest--;` |
|    1398 | 4181 | `					}` |
|   50340 | 4182 | `					pDefend++;` |
|       2 | 4183 | `				}` |
|   44750 | 4184 | `				if( pIn >= pDefend ){` |
|       3 | 4185 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4186 | `					return rc;` |
|       - | 4187 | `				}` |
|       - | 4188 | `				/* Process default value */` |
|   44748 | 4189 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   44748 | 4190 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4191 | `					return rc;` |
|       - | 4192 | `				}` |
|       - | 4193 | `				/* Point beyond the default value */` |
|   44748 | 4194 | `				pIn = pDefend;` |
|   22373 | 4195 | `			}` |
|   50818 | 4196 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4197 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4198 | `				return rc;` |
|       - | 4199 | `			}` |
|   50818 | 4200 | `			pIn++; /* Jump the trailing comma */` |
|   25408 | 4201 | `		}` |
|       - | 4202 | `		/* Append argument signature */` |
|   82046 | 4203 | `		if( sArg.nType > 0 ){` |
|   55924 | 4204 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4205 | `				/* Class name */` |
|    5594 | 4206 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2798 | 4207 | `			}else{` |
|       - | 4208 | `				int c;` |
|   50332 | 4209 | `				c = 'n'; /* cc warning */` |
|       - | 4210 | `				/* Type leading character */` |
|   50332 | 4211 | `				switch(sArg.nType){` |
|     ! 0 | 4212 | `				case MEMOBJ_HASHMAP:` |
|       - | 4213 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4214 | `					c = 'h';` |
|     ! 0 | 4215 | `					break;` |
|    6990 | 4216 | `				case MEMOBJ_INT:` |
|       - | 4217 | `					/* Integer */` |
|   13982 | 4218 | `					c = 'i';` |
|   13982 | 4219 | `					break;` |
|     ! 0 | 4220 | `				case MEMOBJ_BOOL:` |
|       - | 4221 | `					/* Bool */` |
|     ! 0 | 4222 | `					c = 'b';` |
|     ! 0 | 4223 | `					break;` |
|     ! 0 | 4224 | `				case MEMOBJ_REAL:` |
|       - | 4225 | `					/* Float */` |
|     ! 0 | 4226 | `					c = 'f';` |
|     ! 0 | 4227 | `					break;` |
|   18175 | 4228 | `				case MEMOBJ_STRING:` |
|       - | 4229 | `					/* String */` |
|   36352 | 4230 | `					c = 's';` |
|   36350 | 4231 | `					break;` |
|     ! 0 | 4232 | `				default:` |
|     ! 0 | 4233 | `					break;` |
|       - | 4234 | `				}` |
|   50332 | 4235 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4236 | `			}` |
|   27963 | 4237 | `		}else{` |
|       - | 4238 | `			/* No type is associated with this parameter which mean` |
|       - | 4239 | `			 * that this function is not condidate for overloading.` |
|       - | 4240 | `			 */` |
|   26124 | 4241 | `			SyBlobRelease(&sSig);` |
|       - | 4242 | `		}` |
|       - | 4243 | `		/* Save in the argument set */` |
|   82046 | 4244 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4245 | `	}` |
|   53610 | 4246 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4247 | `		/* Save function signature */` |
|   33556 | 4248 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16777 | 4249 | `	}` |
|   53610 | 4250 | `	return SXRET_OK;` |
|   26807 | 4251 |  |
|       - | 4252 | `/*` |
|       - | 4253 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4254 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4255 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4256 | ` */` |
|  149156 | 4257 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4258 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4259 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4260 | `	)` |
|       2 | 4261 |  |
|       - | 4262 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4263 | `	GenBlock *pBlock;` |
|       - | 4264 | `	sxu32 nGotoOfft;` |
|       - | 4265 | `	sxi32 rc;` |
|       - | 4266 | `	/* Attach the new function */` |
|  149158 | 4267 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  149158 | 4268 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4269 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4270 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4271 | `		return SXERR_ABORT;` |
|       - | 4272 | `	}` |
|  149158 | 4273 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4274 | `	/* Swap bytecode containers */` |
|  149158 | 4275 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  149158 | 4276 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4277 | `	/* Compile the body */` |
|  149158 | 4278 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4279 | `	/* Fix exception jumps now the destination is resolved */` |
|  149158 | 4280 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4281 | `	/* Emit the final return if not yet done */` |
|  149158 | 4282 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4283 | `	/* Fix gotos jumps now the destination is resolved */` |
|  149158 | 4284 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4285 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4286 | `	}` |
|  149158 | 4287 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4288 | `	/* Restore the default container */` |
|  149158 | 4289 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4290 | `	/* Leave function block */` |
|  149158 | 4291 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  149158 | 4292 | `	if( rc == SXERR_ABORT ){` |
|       - | 4293 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4294 | `		return SXERR_ABORT;` |
|       - | 4295 | `	}` |
|       - | 4296 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4297 | `	{` |
|  149158 | 4298 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4299 | `		sxu32 i;` |
| 3099094 | 4300 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2949954 | 4301 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      17 | 4302 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      17 | 4303 | `				break;` |
|       - | 4304 | `			}` |
| 1474970 | 4305 | `		}` |
|       - | 4306 | `	}` |
|       - | 4307 | `	/* All done, function body compiled */` |
|  149158 | 4308 | `	return SXRET_OK;` |
|   74580 | 4309 |  |
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
|       - | 4327 | `/*` |
|       - | 4328 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4329 | ` */` |
|       4 | 4330 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4331 |  |
|       - | 4332 | `	sxu32 i;` |
|      21 | 4333 | `	for( i = 0; i < n; i++ ){` |
|      17 | 4334 | `		int a = zA[i], b = zB[i];` |
|      17 | 4335 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      17 | 4336 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      17 | 4337 | `		if( a != b ) return a - b;` |
|       9 | 4338 | `	}` |
|       5 | 4339 | `	return 0;` |
|       3 | 4340 |  |
|       - | 4341 | `/*` |
|       - | 4342 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4343 | ` */` |
|       2 | 4344 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4345 |  |
|       3 | 4346 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4347 | `	if( zDup ){` |
|       3 | 4348 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4349 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4350 | `	}` |
|       3 | 4351 |  |
|       - | 4352 | `/*` |
|       - | 4353 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4354 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4355 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4356 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4357 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4358 | ` */` |
|  171572 | 4359 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4360 |  |
|  171574 | 4361 | `	SyToken *pCur = pGen->pIn;` |
|  171574 | 4362 | `	pFunc->nReturnType = 0;` |
|  171574 | 4363 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  171574 | 4364 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  171538 | 4365 | `		return; /* No return type */` |
|       - | 4366 | `	}` |
|      37 | 4367 | `	pCur++; /* Skip ':' */` |
|      37 | 4368 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4369 | `		pGen->pIn = pCur;` |
|     ! 0 | 4370 | `		return;` |
|       - | 4371 | `	}` |
|       - | 4372 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      37 | 4373 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4374 | `		pCur++;` |
|       7 | 4375 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4376 | `			pGen->pIn = pCur;` |
|     ! 0 | 4377 | `			return;` |
|       - | 4378 | `		}` |
|       3 | 4379 | `	}` |
|      37 | 4380 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      33 | 4381 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      33 | 4382 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4383 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      32 | 4384 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4385 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      30 | 4386 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      11 | 4387 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      24 | 4388 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      15 | 4389 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      12 | 4390 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4391 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       4 | 4392 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4393 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4394 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4395 | `		}` |
|      33 | 4396 | `		pCur++;` |
|      21 | 4397 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       5 | 4398 | `		SyString *pType = &pCur->sData;` |
|       5 | 4399 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       5 | 4400 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       3 | 4401 | `		}else{` |
|       - | 4402 | `			/* Class/interface name */` |
|     ! 0 | 4403 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4404 | `		}` |
|       5 | 4405 | `		pCur++;` |
|       2 | 4406 | `	}` |
|      37 | 4407 | `	pGen->pIn = pCur;` |
|   85788 | 4408 |  |
|       - | 4409 |  |
|   36930 | 4410 | `static sxi32 GenStateCompileFunc(` |
|       - | 4411 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4412 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4413 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4414 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4415 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4416 | `	)` |
|       2 | 4417 |  |
|       - | 4418 | `	ph7_vm_func *pFunc;` |
|       - | 4419 | `	SyToken *pEnd;` |
|       - | 4420 | `	sxu32 nLine;` |
|       - | 4421 | `	char *zName;` |
|       - | 4422 | `	sxi32 rc;` |
|       - | 4423 | `	/* Extract line number */` |
|   36932 | 4424 | `	nLine = pGen->pIn->nLine;` |
|       - | 4425 | `	/* Jump the left parenthesis '(' */` |
|   36932 | 4426 | `	pGen->pIn++;` |
|       - | 4427 | `	/* Delimit the function signature */` |
|   36932 | 4428 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   36932 | 4429 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4430 | `		/* Syntax error */` |
|       7 | 4431 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4432 | `		if( rc == SXERR_ABORT ){` |
|       - | 4433 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4434 | `			return SXERR_ABORT;` |
|       - | 4435 | `		}` |
|       7 | 4436 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4437 | `		return SXRET_OK;` |
|       - | 4438 | `	}` |
|       - | 4439 | `	/* Create the function state */` |
|   36926 | 4440 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   36926 | 4441 | `	if( pFunc == 0 ){` |
|     ! 0 | 4442 | `		goto OutOfMem;` |
|       - | 4443 | `	}` |
|       - | 4444 | `	/* Build the function name, prepending namespace if active */` |
|   36930 | 4445 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4446 | `		SyBlob sFQN;` |
|       - | 4447 | `		sxu32 nLen;` |
|       9 | 4448 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|       9 | 4449 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       9 | 4450 | `		SyBlobAppend(&sFQN,"\\",1);` |
|       9 | 4451 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|       9 | 4452 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|       9 | 4453 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|       9 | 4454 | `		SyBlobRelease(&sFQN);` |
|       9 | 4455 | `		if( zName == 0 ){` |
|     ! 0 | 4456 | `			goto OutOfMem;` |
|       - | 4457 | `		}` |
|       9 | 4458 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       5 | 4459 | `	}else{` |
|   36918 | 4460 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   36918 | 4461 | `		if( zName == 0 ){` |
|     ! 0 | 4462 | `			goto OutOfMem;` |
|       - | 4463 | `		}` |
|   36918 | 4464 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4465 | `	}` |
|   36926 | 4466 | `	if( pGen->pIn < pEnd ){` |
|       - | 4467 | `		/* Collect function arguments */` |
|   25572 | 4468 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   25572 | 4469 | `		if( rc == SXERR_ABORT ){` |
|       - | 4470 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4471 | `			return SXERR_ABORT;` |
|       - | 4472 | `		}` |
|   12785 | 4473 | `	}` |
|       - | 4474 | `	/* Point past ')' and parse optional return type ': type' */` |
|   36926 | 4475 | `	pGen->pIn = &pEnd[1];` |
|   36926 | 4476 | `	GenStateParseReturnType(pGen, pFunc);` |
|   36926 | 4477 | `	if( bHandleClosure ){` |
|       - | 4478 | `		ph7_vm_func_closure_env sEnv;` |
|     148 | 4479 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     146 | 4480 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      81 | 4481 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4482 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4483 | `				/* Closure,record environment variable */` |
|      14 | 4484 | `				pGen->pIn++;` |
|      14 | 4485 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4486 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4487 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4488 | `						return SXERR_ABORT;` |
|       - | 4489 | `					}` |
|     ! 0 | 4490 | `				}` |
|      14 | 4491 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4492 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4493 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4494 | `					int iFlagsLocal = 0;` |
|      28 | 4495 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4496 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4497 | `						break;` |
|       - | 4498 | `					}` |
|      16 | 4499 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4500 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4501 | `						/* Pass by reference,record that */` |
|     ! 0 | 4502 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4503 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4504 | `							);` |
|     ! 0 | 4505 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4506 | `						pGen->pIn++;` |
|     ! 0 | 4507 | `					}` |
|      14 | 4508 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4509 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4510 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4511 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4512 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4513 | `								return SXERR_ABORT;` |
|       - | 4514 | `							}` |
|       - | 4515 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4516 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4517 | `								pGen->pIn++;` |
|     ! 0 | 4518 | `							}` |
|     ! 0 | 4519 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4520 | `								pGen->pIn++;` |
|     ! 0 | 4521 | `							}` |
|     ! 0 | 4522 | `							break;` |
|       - | 4523 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4524 | `					}else{` |
|       - | 4525 | `						SyString *pNameLocal;` |
|       - | 4526 | `						char *zDup;` |
|       - | 4527 | `						/* Duplicate variable name */` |
|      16 | 4528 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4529 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4530 | `						if( zDup ){` |
|       - | 4531 | `							/* Zero the structure */` |
|      16 | 4532 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4533 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4534 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4535 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4536 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4537 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4538 | `									got_this = 1;` |
|     ! 0 | 4539 | `							}` |
|       - | 4540 | `							/* Save imported variable */` |
|      16 | 4541 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4542 | `						}else{` |
|     ! 0 | 4543 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4544 | `							 return SXERR_ABORT;` |
|       - | 4545 | `						}` |
|       - | 4546 | `					}` |
|      16 | 4547 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4548 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4549 | `						/* Ignore trailing commas */` |
|       3 | 4550 | `						pGen->pIn++;` |
|       1 | 4551 | `					}` |
|       2 | 4552 | `				}` |
|      14 | 4553 | `				if( !got_this ){` |
|       - | 4554 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4555 | `					 * available to the closure environment.` |
|       - | 4556 | `					 */` |
|      14 | 4557 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4558 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4559 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4560 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4561 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4562 | `				}` |
|      14 | 4563 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4564 | `					/* Mark as closure */` |
|      14 | 4565 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4566 | `				}` |
|       6 | 4567 | `		}` |
|      73 | 4568 | `	}` |
|       - | 4569 | `	/* Compile the body */` |
|   36926 | 4570 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   36926 | 4571 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4572 | `		return SXERR_ABORT;` |
|       - | 4573 | `	}` |
|   36926 | 4574 | `	if( ppFunc ){` |
|     148 | 4575 | `		*ppFunc = pFunc;` |
|      73 | 4576 | `	}` |
|   36926 | 4577 | `	rc = SXRET_OK;` |
|   36926 | 4578 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4579 | `		/* Finally register the function */` |
|   36914 | 4580 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   18456 | 4581 | `	}` |
|   36926 | 4582 | `	if( rc == SXRET_OK ){` |
|   36926 | 4583 | `		return SXRET_OK;` |
|       - | 4584 | `	}` |
|       - | 4585 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4586 | `OutOfMem:` |
|       - | 4587 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4588 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4589 | `	 */` |
|     ! 0 | 4590 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4591 | `	return SXERR_ABORT;` |
|   18467 | 4592 |  |
|       - | 4593 | `/*` |
|       - | 4594 | ` * Compile a standard PHP function.` |
|       - | 4595 | ` *  Refer to the block-comment above for more information.` |
|       - | 4596 | ` */` |
|   36790 | 4597 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4598 |  |
|       - | 4599 | `	SyString *pName;` |
|       - | 4600 | `	sxi32 iFlags;` |
|       - | 4601 | `	sxu32 nLine;` |
|       - | 4602 | `	sxi32 rc;` |
|       - | 4603 |  |
|   36792 | 4604 | `	nLine = pGen->pIn->nLine;` |
|   36792 | 4605 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   36792 | 4606 | `	iFlags = 0;` |
|   36792 | 4607 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4608 | `		/* Return by reference,remember that */` |
|       7 | 4609 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4610 | `		/* Jump the '&' token */` |
|       7 | 4611 | `		pGen->pIn++;` |
|       3 | 4612 | `	}` |
|   36792 | 4613 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4614 | `		/* Invalid function name */` |
|       5 | 4615 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4617 | `			return SXERR_ABORT;` |
|       - | 4618 | `		}` |
|       - | 4619 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4620 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4621 | `			pGen->pIn++;` |
|       1 | 4622 | `		}` |
|       5 | 4623 | `		return SXRET_OK;` |
|       - | 4624 | `	}` |
|   36788 | 4625 | `	pName = &pGen->pIn->sData;` |
|   36788 | 4626 | `	nLine = pGen->pIn->nLine;` |
|       - | 4627 | `	/* Jump the function name */` |
|   36788 | 4628 | `	pGen->pIn++;` |
|   36788 | 4629 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4630 | `		/* Syntax error */` |
|       3 | 4631 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4632 | `		if( rc == SXERR_ABORT ){` |
|       - | 4633 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4634 | `			return SXERR_ABORT;` |
|       - | 4635 | `		}` |
|       - | 4636 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4637 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4638 | `			pGen->pIn++;` |
|     ! 0 | 4639 | `		}` |
|       3 | 4640 | `		return SXRET_OK;` |
|       - | 4641 | `	}` |
|       - | 4642 | `	/* Compile function body */` |
|   36786 | 4643 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   36786 | 4644 | `	return rc;` |
|   18397 | 4645 |  |
|       - | 4646 | `/*` |
|       - | 4647 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4648 | ` * According to the PHP language reference manual` |
|       - | 4649 | ` *  Visibility:` |
|       - | 4650 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4651 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4652 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4653 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4654 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4655 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4656 | ` */` |
|  171220 | 4657 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4658 |  |
|  171222 | 4659 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8450 | 4660 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  162774 | 4661 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   19602 | 4662 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4663 | `	}` |
|       - | 4664 | `	/* Assume public by default */` |
|  143174 | 4665 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   85612 | 4666 |  |
|       - | 4667 | `/*` |
|       - | 4668 | ` * Compile a class constant.` |
|       - | 4669 | ` * According to the PHP language reference manual` |
|       - | 4670 | ` *  Class Constants` |
|       - | 4671 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4672 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4673 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4674 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4675 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4676 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4677 | ` * Symisc eXtension.` |
|       - | 4678 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4679 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4680 | ` *  Example:` |
|       - | 4681 | ` *   class Test{` |
|       - | 4682 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4683 | ` *   };` |
|       - | 4684 | ` *   var_dump(TEST::MyConst);` |
|       - | 4685 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4686 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4687 | ` */` |
|      10 | 4688 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4689 |  |
|      12 | 4690 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4691 | `	SySet *pInstrContainer;` |
|       - | 4692 | `	ph7_class_attr *pCons;` |
|       - | 4693 | `	SyString *pName;` |
|       - | 4694 | `	sxi32 rc;` |
|       - | 4695 | `	/* Extract visibility level */` |
|      12 | 4696 | `	iProtection = GetProtectionLevel(iProtection);` |
|      12 | 4697 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       5 | 4698 | `loop:` |
|       - | 4699 | `	/* Mark as constant */` |
|      12 | 4700 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      12 | 4701 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4702 | `		/* Invalid constant name */` |
|     ! 0 | 4703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4704 | `		if( rc == SXERR_ABORT ){` |
|       - | 4705 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4706 | `			return SXERR_ABORT;` |
|       - | 4707 | `		}` |
|     ! 0 | 4708 | `		goto Synchronize;` |
|       - | 4709 | `	}` |
|       - | 4710 | `	/* Peek constant name */` |
|      12 | 4711 | `	pName = &pGen->pIn->sData;` |
|       - | 4712 | `	/* Make sure the constant name isn't reserved */` |
|      12 | 4713 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4714 | `		/* Reserved constant name */` |
|     ! 0 | 4715 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4716 | `		if( rc == SXERR_ABORT ){` |
|       - | 4717 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4718 | `			return SXERR_ABORT;` |
|       - | 4719 | `		}` |
|     ! 0 | 4720 | `		goto Synchronize;` |
|       - | 4721 | `	}` |
|       - | 4722 | `	/* Advance the stream cursor */` |
|      12 | 4723 | `	pGen->pIn++;` |
|      12 | 4724 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4725 | `		/* Invalid declaration */` |
|     ! 0 | 4726 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4727 | `		if( rc == SXERR_ABORT ){` |
|       - | 4728 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4729 | `			return SXERR_ABORT;` |
|       - | 4730 | `		}` |
|     ! 0 | 4731 | `		goto Synchronize;` |
|       - | 4732 | `	}` |
|      12 | 4733 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4734 | `	/* Allocate a new class attribute */` |
|      12 | 4735 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      12 | 4736 | `	if( pCons == 0 ){` |
|     ! 0 | 4737 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4738 | `		return SXERR_ABORT;` |
|       - | 4739 | `	}` |
|       - | 4740 | `	/* Swap bytecode container */` |
|      12 | 4741 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      12 | 4742 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4743 | `	/* Compile constant value.` |
|       - | 4744 | `	 */` |
|      12 | 4745 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      12 | 4746 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4748 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4749 | `			return SXERR_ABORT;` |
|       - | 4750 | `		}` |
|       1 | 4751 | `	}` |
|       - | 4752 | `	/* Emit the done instruction */` |
|      12 | 4753 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      12 | 4754 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      12 | 4755 | `	if( rc == SXERR_ABORT ){` |
|       - | 4756 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4757 | `		return SXERR_ABORT;` |
|       - | 4758 | `	}` |
|       - | 4759 | `	/* All done,install the constant */` |
|      12 | 4760 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      12 | 4761 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4762 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4763 | `		return SXERR_ABORT;` |
|       - | 4764 | `	}` |
|      12 | 4765 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4766 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4767 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4768 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4769 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4770 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4771 | `				pTok--;` |
|     ! 0 | 4772 | `			}` |
|     ! 0 | 4773 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4774 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4775 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4776 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4777 | `				return SXERR_ABORT;` |
|       - | 4778 | `			}` |
|     ! 0 | 4779 | `		}else{` |
|     ! 0 | 4780 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4781 | `				goto loop;` |
|       - | 4782 | `			}` |
|       - | 4783 | `		}` |
|     ! 0 | 4784 | `	}` |
|      12 | 4785 | `	return SXRET_OK;` |
|     ! 0 | 4786 | `Synchronize:` |
|       - | 4787 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4788 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4789 | `		pGen->pIn++;` |
|     ! 0 | 4790 | `	}` |
|     ! 0 | 4791 | `	return SXERR_CORRUPT;` |
|       7 | 4792 |  |
|       - | 4793 | `/*` |
|       - | 4794 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4795 | ` * According to the PHP language reference manual` |
|       - | 4796 | ` *  Properties` |
|       - | 4797 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4798 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4799 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4800 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4801 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4802 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4803 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4804 | ` * Symisc eXtension.` |
|       - | 4805 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4806 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4807 | ` *  Example:` |
|       - | 4808 | ` *   class Test{` |
|       - | 4809 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4810 | ` *   };` |
|       - | 4811 | ` *   var_dump(TEST::myVar);` |
|       - | 4812 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4813 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4814 | ` */` |
|   36560 | 4815 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4816 |  |
|   36562 | 4817 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4818 | `	ph7_class_attr *pAttr;` |
|       - | 4819 | `	SyString *pName;` |
|       - | 4820 | `	sxi32 rc;` |
|       - | 4821 | `	/* Extract visibility level */` |
|   36562 | 4822 | `	iProtection = GetProtectionLevel(iProtection);` |
|   18280 | 4823 | `loop:` |
|   36562 | 4824 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   36562 | 4825 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4826 | `		/* Invalid attribute name */` |
|     ! 0 | 4827 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4828 | `		if( rc == SXERR_ABORT ){` |
|       - | 4829 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4830 | `			return SXERR_ABORT;` |
|       - | 4831 | `		}` |
|     ! 0 | 4832 | `		goto Synchronize;` |
|       - | 4833 | `	}` |
|       - | 4834 | `	/* Peek attribute name */` |
|   36562 | 4835 | `	pName = &pGen->pIn->sData;` |
|       - | 4836 | `	/* Advance the stream cursor */` |
|   36562 | 4837 | `	pGen->pIn++;` |
|   36562 | 4838 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 4839 | `		/* Invalid declaration */` |
|       3 | 4840 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 4841 | `		if( rc == SXERR_ABORT ){` |
|       - | 4842 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4843 | `			return SXERR_ABORT;` |
|       - | 4844 | `		}` |
|       3 | 4845 | `		goto Synchronize;` |
|       - | 4846 | `	}` |
|       - | 4847 | `	/* Allocate a new class attribute */` |
|   36560 | 4848 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   36560 | 4849 | `	if( pAttr == 0 ){` |
|     ! 0 | 4850 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4851 | `		return SXERR_ABORT;` |
|       - | 4852 | `	}` |
|   36560 | 4853 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 4854 | `		SySet *pInstrContainer;` |
|   11346 | 4855 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 4856 | `		/* Swap bytecode container */` |
|   11346 | 4857 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11346 | 4858 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 4859 | `		/* Compile attribute value.` |
|       - | 4860 | `		 */` |
|   11346 | 4861 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11346 | 4862 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 4863 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 4864 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4865 | `				return SXERR_ABORT;` |
|       - | 4866 | `			}` |
|     ! 0 | 4867 | `		}` |
|       - | 4868 | `		/* Emit the done instruction */` |
|   11346 | 4869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11346 | 4870 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5672 | 4871 | `	}` |
|       - | 4872 | `	/* All done,install the attribute */` |
|   36560 | 4873 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   36560 | 4874 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4875 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4876 | `		return SXERR_ABORT;` |
|       - | 4877 | `	}` |
|   36560 | 4878 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4879 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 4880 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4881 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4882 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4883 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4884 | `				pTok--;` |
|     ! 0 | 4885 | `			}` |
|     ! 0 | 4886 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4887 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 4888 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4889 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4890 | `				return SXERR_ABORT;` |
|       - | 4891 | `			}` |
|     ! 0 | 4892 | `		}else{` |
|     ! 0 | 4893 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 4894 | `				goto loop;` |
|       - | 4895 | `			}` |
|       - | 4896 | `		}` |
|     ! 0 | 4897 | `	}` |
|   36560 | 4898 | `	return SXRET_OK;` |
|       1 | 4899 | `Synchronize:` |
|       - | 4900 | `	/* Synchronize with the first semi-colon */` |
|       5 | 4901 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 4902 | `		pGen->pIn++;` |
|       1 | 4903 | `	}` |
|       3 | 4904 | `	return SXERR_CORRUPT;` |
|   18282 | 4905 |  |
|       - | 4906 | `/*` |
|       - | 4907 | ` * Compile a class method.` |
|       - | 4908 | ` *` |
|       - | 4909 | ` * Refer to the official documentation for more information` |
|       - | 4910 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 4911 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 4912 | ` * overloading and many more.` |
|       - | 4913 | ` */` |
|  134650 | 4914 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 4915 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4916 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 4917 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 4918 | `	int doBody,          /* TRUE to process method body */` |
|       - | 4919 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 4920 | `	)` |
|       2 | 4921 |  |
|  134652 | 4922 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4923 | `	ph7_class_method *pMeth;` |
|       - | 4924 | `	sxi32 iFuncFlags;` |
|       - | 4925 | `	SyString *pName;` |
|       - | 4926 | `	SyToken *pEnd;` |
|       - | 4927 | `	sxi32 rc;` |
|       - | 4928 | `	/* Extract visibility level */` |
|  134652 | 4929 | `	iProtection = GetProtectionLevel(iProtection);` |
|  134652 | 4930 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  134652 | 4931 | `	iFuncFlags = 0;` |
|  134652 | 4932 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4933 | `		/* Invalid method name */` |
|     ! 0 | 4934 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4935 | `		if( rc == SXERR_ABORT ){` |
|       - | 4936 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4937 | `			return SXERR_ABORT;` |
|       - | 4938 | `		}` |
|     ! 0 | 4939 | `		goto Synchronize;` |
|       - | 4940 | `	}` |
|  134652 | 4941 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4942 | `		/* Return by reference,remember that */` |
|     ! 0 | 4943 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4944 | `		/* Jump the '&' token */` |
|     ! 0 | 4945 | `		pGen->pIn++;` |
|     ! 0 | 4946 | `	}` |
|  134652 | 4947 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4948 | `		/* Invalid method name */` |
|     ! 0 | 4949 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 4950 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4951 | `			return SXERR_ABORT;` |
|       - | 4952 | `		}` |
|     ! 0 | 4953 | `		goto Synchronize;` |
|       - | 4954 | `	}` |
|       - | 4955 | `	/* Peek method name */` |
|  134652 | 4956 | `	pName = &pGen->pIn->sData;` |
|  134652 | 4957 | `	nLine = pGen->pIn->nLine;` |
|       - | 4958 | `	/* Jump the method name */` |
|  134652 | 4959 | `	pGen->pIn++;` |
|  134652 | 4960 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 4961 | `		/* Abstract method */` |
|   22418 | 4962 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 4963 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 4964 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 4965 | `				&pClass->sName,pName);` |
|     ! 0 | 4966 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4967 | `				return SXERR_ABORT;` |
|       - | 4968 | `			}` |
|     ! 0 | 4969 | `		}` |
|       - | 4970 | `		/* Assemble method signature only */` |
|   22418 | 4971 | `		doBody = FALSE;` |
|   11208 | 4972 | `	}` |
|  134652 | 4973 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4974 | `		/* Syntax error */` |
|     ! 0 | 4975 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 4976 | `		if( rc == SXERR_ABORT ){` |
|       - | 4977 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4978 | `			return SXERR_ABORT;` |
|       - | 4979 | `		}` |
|     ! 0 | 4980 | `		goto Synchronize;` |
|       - | 4981 | `	}` |
|       - | 4982 | `	/* Allocate a new class_method instance */` |
|  134652 | 4983 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  134652 | 4984 | `	if( pMeth == 0 ){` |
|     ! 0 | 4985 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4986 | `		return SXERR_ABORT;` |
|       - | 4987 | `	}` |
|       - | 4988 | `	/* Jump the left parenthesis '(' */` |
|  134652 | 4989 | `	pGen->pIn++;` |
|  134652 | 4990 | `	pEnd = 0; /* cc warning */` |
|       - | 4991 | `	/* Delimit the method signature */` |
|  134652 | 4992 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  134652 | 4993 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4994 | `		/* Syntax error */` |
|       3 | 4995 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 4996 | `		if( rc == SXERR_ABORT ){` |
|       - | 4997 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4998 | `			return SXERR_ABORT;` |
|       - | 4999 | `		}` |
|       3 | 5000 | `		goto Synchronize;` |
|       - | 5001 | `	}` |
|  134650 | 5002 | `	if( pGen->pIn < pEnd ){` |
|       - | 5003 | `		/* Collect method arguments */` |
|   28042 | 5004 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   28042 | 5005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5006 | `			return SXERR_ABORT;` |
|       - | 5007 | `		}` |
|   14020 | 5008 | `	}` |
|       - | 5009 | `	/* Point past ')' and parse optional return type ': type' */` |
|  134650 | 5010 | `	pGen->pIn = &pEnd[1];` |
|  134650 | 5011 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  134650 | 5012 | `	if( doBody ){` |
|       - | 5013 | `		/* Compile method body */` |
|  112234 | 5014 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  112234 | 5015 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5016 | `			return SXERR_ABORT;` |
|       - | 5017 | `		}` |
|   56118 | 5018 | `	}else{` |
|       - | 5019 | `		/* Only method signature is allowed */` |
|   22418 | 5020 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5021 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5022 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5023 | `				if( rc == SXERR_ABORT ){` |
|       - | 5024 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5025 | `					return SXERR_ABORT;` |
|       - | 5026 | `				}` |
|     ! 0 | 5027 | `				return SXERR_CORRUPT;` |
|       - | 5028 | `			}` |
|       - | 5029 | `	}` |
|       - | 5030 | `	/* All done,install the method */` |
|  134650 | 5031 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  134650 | 5032 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5033 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5034 | `		return SXERR_ABORT;` |
|       - | 5035 | `	}` |
|  134650 | 5036 | `	return SXRET_OK;` |
|       1 | 5037 | `Synchronize:` |
|       - | 5038 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5039 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5040 | `		pGen->pIn++;` |
|       1 | 5041 | `	}` |
|       3 | 5042 | `	return SXERR_CORRUPT;` |
|   67327 | 5043 |  |
|       - | 5044 | `/*` |
|       - | 5045 | ` * Compile an object interface.` |
|       - | 5046 | ` *  According to the PHP language reference manual` |
|       - | 5047 | ` *   Object Interfaces:` |
|       - | 5048 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5049 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5050 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5051 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5052 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5053 | ` */` |
|    8422 | 5054 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5055 |  |
|    8424 | 5056 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5057 | `	ph7_class *pClass,*pBase;` |
|       - | 5058 | `	SyToken *pEnd,*pTmp;` |
|       - | 5059 | `	SyString *pName;` |
|       - | 5060 | `	sxi32 nKwrd;` |
|       - | 5061 | `	sxi32 rc;` |
|       - | 5062 | `	/* Jump the 'interface' keyword */` |
|    8424 | 5063 | `	pGen->pIn++;` |
|       - | 5064 | `	/* Extract interface name */` |
|    8424 | 5065 | `	pName = &pGen->pIn->sData;` |
|       - | 5066 | `	/* Advance the stream cursor */` |
|    8424 | 5067 | `	pGen->pIn++;` |
|       - | 5068 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5069 | `		SyBlob sFQN;` |
|       - | 5070 | `		SyString sFQNStr;` |
|    8424 | 5071 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8424 | 5072 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8424 | 5073 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8424 | 5074 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8424 | 5075 | `		SyBlobRelease(&sFQN);` |
|       - | 5076 | `	}` |
|    8424 | 5077 | `	if( pClass == 0 ){` |
|     ! 0 | 5078 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5079 | `		return SXERR_ABORT;` |
|       - | 5080 | `	}` |
|       - | 5081 | `	/* Mark as an interface */` |
|    8424 | 5082 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5083 | `	/* Assume no base class is given */` |
|    8424 | 5084 | `	pBase = 0;` |
|    8424 | 5085 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5086 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5087 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5088 | `			SyString *pBaseName;` |
|       - | 5089 | `			/* Extract base interface */` |
|       3 | 5090 | `			pGen->pIn++;` |
|       3 | 5091 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5092 | `				/* Syntax error */` |
|     ! 0 | 5093 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5094 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5095 | `					pName);` |
|     ! 0 | 5096 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5097 | `				if( rc == SXERR_ABORT ){` |
|       - | 5098 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5099 | `					return SXERR_ABORT;` |
|       - | 5100 | `				}` |
|     ! 0 | 5101 | `				return SXRET_OK;` |
|       - | 5102 | `			}` |
|       3 | 5103 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5104 | `			{` |
|       - | 5105 | `				SyBlob sResolved;` |
|       3 | 5106 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5107 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5108 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5109 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5110 | `				SyBlobRelease(&sResolved);` |
|       - | 5111 | `			}` |
|       - | 5112 | `			/* Only interfaces is allowed */` |
|       3 | 5113 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5114 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5115 | `			}` |
|       3 | 5116 | `			if( pBase == 0 ){` |
|       - | 5117 | `				/* Inexistant interface */` |
|     ! 0 | 5118 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5119 | `				if( rc == SXERR_ABORT ){` |
|       - | 5120 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5121 | `					return SXERR_ABORT;` |
|       - | 5122 | `				}` |
|     ! 0 | 5123 | `			}` |
|       - | 5124 | `			/* Advance the stream cursor */` |
|       3 | 5125 | `			pGen->pIn++;` |
|       1 | 5126 | `		}` |
|       1 | 5127 | `	}` |
|    8424 | 5128 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5129 | `		/* Syntax error */` |
|     ! 0 | 5130 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5131 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5132 | `		if( rc == SXERR_ABORT ){` |
|       - | 5133 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5134 | `			return SXERR_ABORT;` |
|       - | 5135 | `		}` |
|     ! 0 | 5136 | `		return SXRET_OK;` |
|       - | 5137 | `	}` |
|    8424 | 5138 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8424 | 5139 | `	pEnd = 0; /* cc warning */` |
|       - | 5140 | `	/* Delimit the interface body */` |
|    8424 | 5141 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8424 | 5142 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5143 | `		/* Syntax error */` |
|     ! 0 | 5144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5145 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5146 | `		if( rc == SXERR_ABORT ){` |
|       - | 5147 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5148 | `			return SXERR_ABORT;` |
|       - | 5149 | `		}` |
|     ! 0 | 5150 | `		return SXRET_OK;` |
|       - | 5151 | `	}` |
|       - | 5152 | `	/* Swap token stream */` |
|    8424 | 5153 | `	pTmp = pGen->pEnd;` |
|    8424 | 5154 | `	pGen->pEnd = pEnd;` |
|       - | 5155 | `	/* Start the parse process` |
|       - | 5156 | `	 * Note (According to the PHP reference manual):` |
|       - | 5157 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5158 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5159 | `	 */` |
|   15414 | 5160 | `	for(;;){` |
|       - | 5161 | `		/* Jump leading/trailing semi-colons */` |
|   53236 | 5162 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   22408 | 5163 | `			pGen->pIn++;` |
|       2 | 5164 | `		}` |
|   30830 | 5165 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5166 | `			/* End of interface body */` |
|    8424 | 5167 | `			break;` |
|       - | 5168 | `		}` |
|   22408 | 5169 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5171 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5172 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5173 | `			if( rc == SXERR_ABORT ){` |
|       - | 5174 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5175 | `				return SXERR_ABORT;` |
|       - | 5176 | `			}` |
|     ! 0 | 5177 | `			goto done;` |
|       - | 5178 | `		}` |
|       - | 5179 | `		/* Extract the current keyword */` |
|   22408 | 5180 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22408 | 5181 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5182 | `			/* Emit a warning and switch to public visibility */` |
|     ! 0 | 5183 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");` |
|     ! 0 | 5184 | `			nKwrd = PH7_TKWRD_PUBLIC;` |
|     ! 0 | 5185 | `		}` |
|   22408 | 5186 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5187 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5188 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5189 | `			if( rc == SXERR_ABORT ){` |
|       - | 5190 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5191 | `				return SXERR_ABORT;` |
|       - | 5192 | `			}` |
|     ! 0 | 5193 | `			goto done;` |
|       - | 5194 | `		}` |
|   22408 | 5195 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5196 | `			/* Advance the stream cursor */` |
|   22404 | 5197 | `			pGen->pIn++;` |
|   22404 | 5198 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5199 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5200 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5201 | `				if( rc == SXERR_ABORT ){` |
|       - | 5202 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5203 | `					return SXERR_ABORT;` |
|       - | 5204 | `				}` |
|     ! 0 | 5205 | `				goto done;` |
|       - | 5206 | `			}` |
|   22404 | 5207 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22404 | 5208 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5209 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5210 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5211 | `				if( rc == SXERR_ABORT ){` |
|       - | 5212 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5213 | `					return SXERR_ABORT;` |
|       - | 5214 | `				}` |
|     ! 0 | 5215 | `				goto done;` |
|       - | 5216 | `			}` |
|   11201 | 5217 | `		}` |
|   22408 | 5218 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5219 | `			/* Parse constant */` |
|       3 | 5220 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5221 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5222 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5223 | `					return SXERR_ABORT;` |
|       - | 5224 | `				}` |
|     ! 0 | 5225 | `				goto done;` |
|       - | 5226 | `			}` |
|       2 | 5227 | `		}else{` |
|   22406 | 5228 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   22406 | 5229 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5230 | `				/* Static method,record that */` |
|     ! 0 | 5231 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5232 | `				/* Advance the stream cursor */` |
|     ! 0 | 5233 | `				pGen->pIn++;` |
|     ! 0 | 5234 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5235 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5236 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5237 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5238 | `						if( rc == SXERR_ABORT ){` |
|       - | 5239 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5240 | `							return SXERR_ABORT;` |
|       - | 5241 | `						}` |
|     ! 0 | 5242 | `						goto done;` |
|       - | 5243 | `				}` |
|     ! 0 | 5244 | `			}` |
|       - | 5245 | `			/* Process method signature (no body for interface methods) */` |
|   22406 | 5246 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   22406 | 5247 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5248 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5249 | `					return SXERR_ABORT;` |
|       - | 5250 | `				}` |
|     ! 0 | 5251 | `				goto done;` |
|       - | 5252 | `			}` |
|       - | 5253 | `		}` |
|       2 | 5254 | `	}` |
|       - | 5255 | `	/* Install the interface */` |
|    8424 | 5256 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8424 | 5257 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5258 | `		/* Inherit from the base interface */` |
|       3 | 5259 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5260 | `	}` |
|    8424 | 5261 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5262 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5263 | `		return SXERR_ABORT;` |
|       - | 5264 | `	}` |
|    4211 | 5265 | `done:` |
|       - | 5266 | `	/* Point beyond the interface body */` |
|    8424 | 5267 | `	pGen->pIn  = &pEnd[1];` |
|    8424 | 5268 | `	pGen->pEnd = pTmp;` |
|    8424 | 5269 | `	return PH7_OK;` |
|    4213 | 5270 |  |
|       - | 5271 | `/*` |
|       - | 5272 | ` * Compile a user-defined class.` |
|       - | 5273 | ` * According to the PHP language reference manual` |
|       - | 5274 | ` *  class` |
|       - | 5275 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5276 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5277 | ` *  of the properties and methods belonging to the class.` |
|       - | 5278 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5279 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5280 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5281 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5282 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5283 | ` *  (called "methods").` |
|       - | 5284 | ` */` |
|       - | 5285 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5286 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5287 | `struct TraitUseEntry {` |
|       - | 5288 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5289 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5290 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5291 | `};` |
|       - | 5292 | `/*` |
|       - | 5293 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5294 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5295 | ` */` |
|   33936 | 5296 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5297 |  |
|       - | 5298 | `	ph7_class **apIface;` |
|       - | 5299 | `	sxu32 nIface,i;` |
|       - | 5300 | `	sxi32 rc;` |
|   33938 | 5301 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5302 | `		return SXRET_OK;` |
|       - | 5303 | `	}` |
|   33938 | 5304 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   33938 | 5305 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   36772 | 5306 | `	for(i = 0; i < nIface; i++){` |
|    2836 | 5307 | `		ph7_class *pIface = apIface[i];` |
|       - | 5308 | `		SyHashEntry *pEntry;` |
|    2836 | 5309 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16894 | 5310 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   14060 | 5311 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5312 | `			ph7_class_method *pImplMeth;` |
|   14060 | 5313 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5314 | `			/* Find the implementing method in the class */` |
|   14060 | 5315 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   14060 | 5316 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5317 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5318 | `			}` |
|       - | 5319 | `			/* Check visibility: interface methods must be implemented as public */` |
|   14046 | 5320 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5321 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5322 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5323 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5324 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5325 | `					return SXERR_ABORT;` |
|       - | 5326 | `				}` |
|       1 | 5327 | `			}` |
|       - | 5328 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5329 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5330 | `			 */` |
|       - | 5331 | `			{` |
|   14046 | 5332 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   14046 | 5333 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   14046 | 5334 | `				int sigError = 0;` |
|   14046 | 5335 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5336 | `					sigError = 1;` |
|   14045 | 5337 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5338 | `					/* Extra parameters must all have default values */` |
|       5 | 5339 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5340 | `					sxu32 k;` |
|       7 | 5341 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5342 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5343 | `							sigError = 1;` |
|       3 | 5344 | `							break;` |
|       - | 5345 | `						}` |
|       2 | 5346 | `					}` |
|       2 | 5347 | `				}` |
|   14046 | 5348 | `				if( sigError ){` |
|       - | 5349 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5350 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5351 | `					sxu32 j;` |
|       5 | 5352 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5353 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5354 | `					/* Build implementing method signature */` |
|       5 | 5355 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5356 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5357 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5358 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5359 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5360 | `					}` |
|       - | 5361 | `					/* Build interface method signature */` |
|       5 | 5362 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5363 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5364 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5365 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5366 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5367 | `					}` |
|       7 | 5368 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5369 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5370 | `						&pClass->sName,pMName,` |
|       4 | 5371 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5372 | `						&pIface->sName,pMName,` |
|       4 | 5373 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5374 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5375 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5376 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5377 | `						return SXERR_ABORT;` |
|       - | 5378 | `					}` |
|       2 | 5379 | `				}` |
|       - | 5380 | `			}` |
|       2 | 5381 | `		}` |
|    1419 | 5382 | `	}` |
|   33938 | 5383 | `	return SXRET_OK;` |
|   16970 | 5384 |  |
|       - | 5385 | `/*` |
|       - | 5386 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5387 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5388 | ` */` |
|   33936 | 5389 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5390 |  |
|       - | 5391 | `	ph7_class_method *pMeth;` |
|       - | 5392 | `	SyHashEntry *pEntry;` |
|       - | 5393 | `	sxu32 nAbstract;` |
|       - | 5394 | `	SyBlob sMsg;` |
|       - | 5395 | `	sxi32 rc;` |
|       - | 5396 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   33938 | 5397 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5398 | `		return SXRET_OK;` |
|       - | 5399 | `	}` |
|       - | 5400 | `	/* Count abstract methods */` |
|   33920 | 5401 | `	nAbstract = 0;` |
|   33920 | 5402 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  319576 | 5403 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  285658 | 5404 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  285658 | 5405 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5406 | `			nAbstract++;` |
|       8 | 5407 | `		}` |
|       2 | 5408 | `	}` |
|   33920 | 5409 | `	if( nAbstract == 0 ){` |
|   33906 | 5410 | `		return SXRET_OK;` |
|       - | 5411 | `	}` |
|       - | 5412 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5413 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5414 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5415 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5416 | `		&pClass->sName,nAbstract,` |
|       7 | 5417 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5418 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5419 | `	/* Second pass: list methods with origins */` |
|       - | 5420 | `	{` |
|      15 | 5421 | `		sxu32 nListed = 0;` |
|      15 | 5422 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5423 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5424 | `			ph7_class *pOrigin = 0;` |
|       - | 5425 | `			SyString *pMName;` |
|      19 | 5426 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5427 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5428 | `				continue;` |
|       - | 5429 | `			}` |
|      17 | 5430 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5431 | `			if( nListed > 0 ){` |
|       3 | 5432 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5433 | `			}` |
|       - | 5434 | `			/* Find the origin of this abstract method.` |
|       - | 5435 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5436 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5437 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5438 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5439 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5440 | `			 * class's namespace.` |
|       - | 5441 | `			 */` |
|       - | 5442 | `			{` |
|       - | 5443 | `				ph7_class **apIface;` |
|       - | 5444 | `				ph7_class **apTrait;` |
|       - | 5445 | `				ph7_class *pWalk;` |
|       - | 5446 | `				sxu32 i;` |
|       - | 5447 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5448 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5449 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5450 | `				 */` |
|      17 | 5451 | `				if( pClass->pBase ){` |
|       9 | 5452 | `					pWalk = pClass->pBase;` |
|      17 | 5453 | `					while( pWalk ){` |
|       - | 5454 | `						ph7_class_method *pParentMeth;` |
|      11 | 5455 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5456 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5457 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5458 | `							 * in this class's ancestor chain.` |
|       - | 5459 | `							 */` |
|      11 | 5460 | `							int fromIface = 0;` |
|      11 | 5461 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5462 | `							while( pAnc ){` |
|       - | 5463 | `								ph7_class **apPI;` |
|       - | 5464 | `								sxu32 j;` |
|      13 | 5465 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5466 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5467 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5468 | `										fromIface = 1;` |
|       9 | 5469 | `										break;` |
|       - | 5470 | `									}` |
|     ! 0 | 5471 | `								}` |
|      13 | 5472 | `								if( fromIface ) break;` |
|       5 | 5473 | `								pAnc = pAnc->pBase;` |
|       1 | 5474 | `							}` |
|      11 | 5475 | `							if( !fromIface ){` |
|       3 | 5476 | `								pOrigin = pWalk;` |
|       3 | 5477 | `								break;` |
|       - | 5478 | `							}` |
|       4 | 5479 | `						}` |
|       9 | 5480 | `						pWalk = pWalk->pBase;` |
|       1 | 5481 | `					}` |
|       4 | 5482 | `				}` |
|       - | 5483 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5484 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5485 | `				 */` |
|      17 | 5486 | `				if( !pOrigin ){` |
|      15 | 5487 | `					pWalk = pClass;` |
|      37 | 5488 | `					while( pWalk && !pOrigin ){` |
|      23 | 5489 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5490 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5491 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5492 | `							ph7_class *pDeepest = 0;` |
|      25 | 5493 | `							while( pIface ){` |
|      13 | 5494 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5495 | `									pDeepest = pIface;` |
|       6 | 5496 | `								}` |
|      13 | 5497 | `								pIface = pIface->pBase;` |
|       1 | 5498 | `							}` |
|      13 | 5499 | `							if( pDeepest ){` |
|      13 | 5500 | `								pOrigin = pDeepest;` |
|      13 | 5501 | `								break;` |
|       - | 5502 | `							}` |
|     ! 0 | 5503 | `						}` |
|      23 | 5504 | `						pWalk = pWalk->pBase;` |
|       1 | 5505 | `					}` |
|       7 | 5506 | `				}` |
|       - | 5507 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5508 | `				if( !pOrigin ){` |
|       3 | 5509 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5510 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5511 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5512 | `							pOrigin = pClass;` |
|       3 | 5513 | `							break;` |
|       - | 5514 | `						}` |
|     ! 0 | 5515 | `					}` |
|       1 | 5516 | `				}` |
|       - | 5517 | `			}` |
|      17 | 5518 | `			if( pOrigin ){` |
|      17 | 5519 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5520 | `			}else{` |
|       - | 5521 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5522 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5523 | `			}` |
|      17 | 5524 | `			nListed++;` |
|       1 | 5525 | `		}` |
|       - | 5526 | `	}` |
|      15 | 5527 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5528 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5529 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5530 | `	SyBlobRelease(&sMsg);` |
|      15 | 5531 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5532 | `		return SXERR_ABORT;` |
|       - | 5533 | `	}` |
|      15 | 5534 | `	return SXRET_OK;` |
|   16970 | 5535 |  |
|   33940 | 5536 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5537 |  |
|   33942 | 5538 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5539 | `	ph7_class *pClass,*pBase;` |
|       - | 5540 | `	SyToken *pEnd,*pTmp;` |
|       - | 5541 | `	sxi32 iProtection;` |
|       - | 5542 | `	SySet aInterfaces;` |
|       - | 5543 | `	SySet aUseEntries;` |
|       - | 5544 | `	sxi32 iAttrflags;` |
|       - | 5545 | `	SyString *pName;` |
|       - | 5546 | `	sxi32 nKwrd;` |
|       - | 5547 | `	sxi32 rc;` |
|       - | 5548 | `	/* Jump the 'class' keyword */` |
|   33942 | 5549 | `	pGen->pIn++;` |
|   33942 | 5550 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5551 | `		/* Syntax error */` |
|     ! 0 | 5552 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5553 | `		if( rc == SXERR_ABORT ){` |
|       - | 5554 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5555 | `			return SXERR_ABORT;` |
|       - | 5556 | `		}` |
|       - | 5557 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5558 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5559 | `			pGen->pIn++;` |
|     ! 0 | 5560 | `		}` |
|     ! 0 | 5561 | `		return SXRET_OK;` |
|       - | 5562 | `	}` |
|       - | 5563 | `	/* Extract class name */` |
|   33942 | 5564 | `	pName = &pGen->pIn->sData;` |
|       - | 5565 | `	/* Advance the stream cursor */` |
|   33942 | 5566 | `	pGen->pIn++;` |
|       - | 5567 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5568 | `		SyBlob sFQN;` |
|       - | 5569 | `		SyString sFQNStr;` |
|   33942 | 5570 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   33942 | 5571 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   33942 | 5572 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   33942 | 5573 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   33942 | 5574 | `		SyBlobRelease(&sFQN);` |
|       - | 5575 | `	}` |
|   33942 | 5576 | `	if( pClass == 0 ){` |
|     ! 0 | 5577 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5578 | `		return SXERR_ABORT;` |
|       - | 5579 | `	}` |
|       - | 5580 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   33942 | 5581 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   33942 | 5582 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5583 | `	/* Assume a standalone class */` |
|   33942 | 5584 | `	pBase = 0;` |
|   33942 | 5585 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5586 | `		SyString *pBaseName;` |
|   22468 | 5587 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22468 | 5588 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   19636 | 5589 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   19636 | 5590 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5591 | `				/* Syntax error */` |
|     ! 0 | 5592 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5593 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5594 | `					pName);` |
|     ! 0 | 5595 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5596 | `				if( rc == SXERR_ABORT ){` |
|       - | 5597 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5598 | `					return SXERR_ABORT;` |
|       - | 5599 | `				}` |
|     ! 0 | 5600 | `				return SXRET_OK;` |
|       - | 5601 | `			}` |
|       - | 5602 | `			/* Extract base class name and resolve through namespace/imports */` |
|   19636 | 5603 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5604 | `			{` |
|       - | 5605 | `				SyBlob sResolved;` |
|   19636 | 5606 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   19636 | 5607 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   29453 | 5608 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   19634 | 5609 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   19636 | 5610 | `				SyBlobRelease(&sResolved);` |
|       - | 5611 | `			}` |
|       - | 5612 | `			/* Interfaces are not allowed */` |
|   19636 | 5613 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5614 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5615 | `			}` |
|   19636 | 5616 | `			if( pBase == 0 ){` |
|       - | 5617 | `				/* Inexistant base class */` |
|     ! 0 | 5618 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5619 | `				if( rc == SXERR_ABORT ){` |
|       - | 5620 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5621 | `					return SXERR_ABORT;` |
|       - | 5622 | `				}` |
|     ! 0 | 5623 | `			}else{` |
|   19636 | 5624 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5625 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5626 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5627 | `					if( rc == SXERR_ABORT ){` |
|       - | 5628 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5629 | `						return SXERR_ABORT;` |
|       - | 5630 | `					}` |
|     ! 0 | 5631 | `				}` |
|       - | 5632 | `			}` |
|       - | 5633 | `			/* Advance the stream cursor */` |
|   19636 | 5634 | `			pGen->pIn++;` |
|    9817 | 5635 | `		}` |
|   22468 | 5636 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5637 | `			ph7_class *pInterface;` |
|       - | 5638 | `			SyString *pIntName;` |
|       - | 5639 | `			/* Interface implementation */` |
|    2836 | 5640 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1417 | 5641 | `			for(;;){` |
|    2836 | 5642 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5643 | `					/* Syntax error */` |
|     ! 0 | 5644 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5645 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5646 | `						pName);` |
|     ! 0 | 5647 | `					if( rc == SXERR_ABORT ){` |
|       - | 5648 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5649 | `						return SXERR_ABORT;` |
|       - | 5650 | `					}` |
|     ! 0 | 5651 | `					break;` |
|       - | 5652 | `				}` |
|       - | 5653 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2836 | 5654 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5655 | `				{` |
|       - | 5656 | `					SyBlob sResolved;` |
|    2836 | 5657 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2836 | 5658 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5670 | 5659 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2834 | 5660 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2836 | 5661 | `					SyBlobRelease(&sResolved);` |
|       - | 5662 | `				}` |
|       - | 5663 | `				/* Only interfaces are allowed */` |
|    2836 | 5664 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5665 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5666 | `				}` |
|    2836 | 5667 | `				if( pInterface == 0 ){` |
|       - | 5668 | `					/* Inexistant interface */` |
|     ! 0 | 5669 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5670 | `					if( rc == SXERR_ABORT ){` |
|       - | 5671 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5672 | `						return SXERR_ABORT;` |
|       - | 5673 | `					}` |
|     ! 0 | 5674 | `				}else{` |
|       - | 5675 | `					/* Register interface */` |
|    2836 | 5676 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5677 | `				}` |
|       - | 5678 | `				/* Advance the stream cursor */` |
|    2836 | 5679 | `				pGen->pIn++;` |
|    2836 | 5680 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1419 | 5681 | `					break;` |
|       - | 5682 | `				}` |
|     ! 0 | 5683 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5684 | `			}` |
|    1417 | 5685 | `		}` |
|   11233 | 5686 | `	}` |
|   33942 | 5687 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5688 | `		/* Syntax error */` |
|     ! 0 | 5689 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5690 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5691 | `		if( rc == SXERR_ABORT ){` |
|       - | 5692 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5693 | `			return SXERR_ABORT;` |
|       - | 5694 | `		}` |
|     ! 0 | 5695 | `		return SXRET_OK;` |
|       - | 5696 | `	}` |
|   33942 | 5697 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   33942 | 5698 | `	pEnd = 0; /* cc warning */` |
|       - | 5699 | `	/* Delimit the class body */` |
|   33942 | 5700 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   33942 | 5701 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5702 | `		/* Syntax error */` |
|     ! 0 | 5703 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5704 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5705 | `		if( rc == SXERR_ABORT ){` |
|       - | 5706 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5707 | `			return SXERR_ABORT;` |
|       - | 5708 | `		}` |
|     ! 0 | 5709 | `		return SXRET_OK;` |
|       - | 5710 | `	}` |
|       - | 5711 | `	/* Swap token stream */` |
|   33942 | 5712 | `	pTmp = pGen->pEnd;` |
|   33942 | 5713 | `	pGen->pEnd = pEnd;` |
|       - | 5714 | `	/* Set the inherited flags */` |
|   33942 | 5715 | `	pClass->iFlags = iFlags;` |
|       - | 5716 | `	/* Start the parse process */` |
|   73072 | 5717 | `	for(;;){` |
|       - | 5718 | `		/* Jump leading/trailing semi-colons */` |
|  219320 | 5719 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   36596 | 5720 | `			pGen->pIn++;` |
|       2 | 5721 | `		}` |
|  182726 | 5722 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5723 | `			/* End of class body */` |
|   33938 | 5724 | `			break;` |
|       - | 5725 | `		}` |
|  148790 | 5726 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5727 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5728 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5729 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5730 | `			if( rc == SXERR_ABORT ){` |
|       - | 5731 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5732 | `				return SXERR_ABORT;` |
|       - | 5733 | `			}` |
|     ! 0 | 5734 | `			goto done;` |
|       - | 5735 | `		}` |
|       - | 5736 | `		/* Assume public visibility */` |
|  148790 | 5737 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  148790 | 5738 | `		iAttrflags = 0;` |
|  148790 | 5739 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5740 | `			/* Extract the current keyword */` |
|  148790 | 5741 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  148790 | 5742 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5743 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5744 | `				TraitUseEntry sUse;` |
|      41 | 5745 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5746 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5747 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5748 | `				for(;;){` |
|       - | 5749 | `					ph7_class *pTrait;` |
|       - | 5750 | `					SyString *pTraitName;` |
|      49 | 5751 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5752 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5753 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5754 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5755 | `							return SXERR_ABORT;` |
|       - | 5756 | `						}` |
|     ! 0 | 5757 | `						break;` |
|       - | 5758 | `					}` |
|      49 | 5759 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5760 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5761 | `						SyBlob sResolved;` |
|      49 | 5762 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5763 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5764 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5765 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5766 | `						SyBlobRelease(&sResolved);` |
|       - | 5767 | `					}` |
|       - | 5768 | `					/* Only traits are allowed */` |
|      49 | 5769 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5770 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5771 | `					}` |
|      49 | 5772 | `					if( pTrait == 0 ){` |
|     ! 0 | 5773 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5774 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5775 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5776 | `							return SXERR_ABORT;` |
|       - | 5777 | `						}` |
|     ! 0 | 5778 | `					}else{` |
|      49 | 5779 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5780 | `					}` |
|      49 | 5781 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5782 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5783 | `						break;` |
|       - | 5784 | `					}` |
|       9 | 5785 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5786 | `				}` |
|       - | 5787 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5788 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5789 | `					SyToken *pBlock;` |
|       9 | 5790 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5791 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5792 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5793 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5794 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5795 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5796 | `					}else{` |
|     ! 0 | 5797 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5798 | `					}` |
|       4 | 5799 | `				}` |
|      41 | 5800 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5801 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5802 | `				continue;` |
|       - | 5803 | `			}` |
|  148750 | 5804 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  145858 | 5805 | `				iProtection = nKwrd;` |
|  145858 | 5806 | `				pGen->pIn++; /* Jump the visibility token */` |
|  145858 | 5807 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5808 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5809 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5810 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 5811 | `					if( rc == SXERR_ABORT ){` |
|       - | 5812 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5813 | `						return SXERR_ABORT;` |
|       - | 5814 | `					}` |
|     ! 0 | 5815 | `					goto done;` |
|       - | 5816 | `				}` |
|  145858 | 5817 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5818 | `					/* Attribute declaration */` |
|   36540 | 5819 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   36540 | 5820 | `					if( rc != SXRET_OK ){` |
|       3 | 5821 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5822 | `							return SXERR_ABORT;` |
|       - | 5823 | `						}` |
|       3 | 5824 | `						goto done;` |
|       - | 5825 | `					}` |
|   36538 | 5826 | `					continue;` |
|       - | 5827 | `				}` |
|       - | 5828 | `				/* Extract the keyword */` |
|  109320 | 5829 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54659 | 5830 | `			}` |
|  112212 | 5831 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5832 | `				/* Process constant declaration */` |
|      10 | 5833 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 | 5834 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5835 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5836 | `						return SXERR_ABORT;` |
|       - | 5837 | `					}` |
|     ! 0 | 5838 | `					goto done;` |
|       - | 5839 | `				}` |
|       6 | 5840 | `			}else{` |
|  112204 | 5841 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5842 | `					/* Static method or attribute,record that */` |
|    2820 | 5843 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2820 | 5844 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2820 | 5845 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5846 | `						/* Extract the keyword */` |
|    2816 | 5847 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2816 | 5848 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 5849 | `							iProtection = nKwrd;` |
|     ! 0 | 5850 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 5851 | `						}` |
|    1407 | 5852 | `					}` |
|    2820 | 5853 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5854 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5855 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 5856 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5857 | `						if( rc == SXERR_ABORT ){` |
|       - | 5858 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5859 | `							return SXERR_ABORT;` |
|       - | 5860 | `						}` |
|     ! 0 | 5861 | `						goto done;` |
|       - | 5862 | `					}` |
|    2820 | 5863 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 5864 | `						/* Attribute declaration */` |
|       5 | 5865 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 5866 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 5867 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5868 | `								return SXERR_ABORT;` |
|       - | 5869 | `							}` |
|     ! 0 | 5870 | `							goto done;` |
|       - | 5871 | `						}` |
|       5 | 5872 | `						continue;` |
|       - | 5873 | `					}` |
|       - | 5874 | `					/* Extract the keyword */` |
|    2816 | 5875 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  110793 | 5876 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 5877 | `					/* Abstract method,record that */` |
|      10 | 5878 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 5879 | `					/* Mark the whole class as abstract */` |
|      10 | 5880 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 5881 | `					/* Advance the stream cursor */` |
|      10 | 5882 | `					pGen->pIn++;` |
|      10 | 5883 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 5884 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 5885 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 5886 | `							iProtection = nKwrd;` |
|       8 | 5887 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 5888 | `						}` |
|       4 | 5889 | `					}` |
|      10 | 5890 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 5891 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5892 | `							/* Static method */` |
|     ! 0 | 5893 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5894 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5895 | `					}` |
|      10 | 5896 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 5897 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5898 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5899 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 5900 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5901 | `							if( rc == SXERR_ABORT ){` |
|       - | 5902 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5903 | `								return SXERR_ABORT;` |
|       - | 5904 | `							}` |
|     ! 0 | 5905 | `							goto done;` |
|       - | 5906 | `					}` |
|      10 | 5907 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  109382 | 5908 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 5909 | `					/* final method ,record that */` |
|       5 | 5910 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 5911 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 5912 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5913 | `						/* Extract the keyword */` |
|       5 | 5914 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 5915 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 5916 | `							iProtection = nKwrd;` |
|       5 | 5917 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 5918 | `						}` |
|       2 | 5919 | `					}` |
|       5 | 5920 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 5921 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 5922 | `							/* Static method */` |
|     ! 0 | 5923 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 5924 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 5925 | `					}` |
|       5 | 5926 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 5927 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5928 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5929 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 5930 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 5931 | `							if( rc == SXERR_ABORT ){` |
|       - | 5932 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 5933 | `								return SXERR_ABORT;` |
|       - | 5934 | `							}` |
|     ! 0 | 5935 | `							goto done;` |
|       - | 5936 | `					}` |
|       5 | 5937 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 5938 | `				}` |
|  112200 | 5939 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 5940 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5941 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 5942 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 5943 | `						if( rc == SXERR_ABORT ){` |
|       - | 5944 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5945 | `							return SXERR_ABORT;` |
|       - | 5946 | `						}` |
|     ! 0 | 5947 | `						goto done;` |
|       - | 5948 | `				}` |
|  112200 | 5949 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 5950 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 5951 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 5952 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5953 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 5954 | `						if( rc == SXERR_ABORT ){` |
|       - | 5955 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5956 | `							return SXERR_ABORT;` |
|       - | 5957 | `						}` |
|     ! 0 | 5958 | `						goto done;` |
|       - | 5959 | `					}` |
|       - | 5960 | `					/* Attribute declaration */` |
|       7 | 5961 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 5962 | `				}else{` |
|       - | 5963 | `					/* Process method declaration */` |
|  112194 | 5964 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 5965 | `				}` |
|  112200 | 5966 | `				if( rc != SXRET_OK ){` |
|       3 | 5967 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5968 | `						return SXERR_ABORT;` |
|       - | 5969 | `					}` |
|       3 | 5970 | `					goto done;` |
|       - | 5971 | `				}` |
|       - | 5972 | `			}` |
|   56104 | 5973 | `		}else{` |
|       - | 5974 | `			/* Attribute declaration */` |
|     ! 0 | 5975 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 5976 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5977 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5978 | `					return SXERR_ABORT;` |
|       - | 5979 | `				}` |
|     ! 0 | 5980 | `				goto done;` |
|       - | 5981 | `			}` |
|       - | 5982 | `		}` |
|       2 | 5983 | `	}` |
|       - | 5984 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 5985 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 5986 | `	 */` |
|       - | 5987 | `	{` |
|       - | 5988 | `		TraitUseEntry *apUse;` |
|       - | 5989 | `		sxu32 nU;` |
|   33938 | 5990 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   33978 | 5991 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 5992 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 5993 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 5994 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 5995 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 5996 | `			sxu32 nT;` |
|      41 | 5997 | `			if( !hasResolution ){` |
|       - | 5998 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 5999 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6000 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6001 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6002 | `						break;` |
|       - | 6003 | `					}` |
|      20 | 6004 | `				}` |
|      17 | 6005 | `			}else{` |
|       - | 6006 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6007 | `				 * then use the block to resolve method conflicts.` |
|       - | 6008 | `				 */` |
|       - | 6009 | `				SyToken *pR;` |
|      19 | 6010 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6011 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6012 | `					ph7_class_attr *pAR;` |
|       - | 6013 | `					SyHashEntry *pER;` |
|       - | 6014 | `					SyString *pNR;` |
|      11 | 6015 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6016 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6017 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6018 | `						pNR = &pAR->sName;` |
|     ! 0 | 6019 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6020 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6021 | `						}` |
|     ! 0 | 6022 | `					}` |
|      11 | 6023 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6024 | `				}` |
|       - | 6025 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6026 | `				pR = pUse->pResolvStart;` |
|      21 | 6027 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6028 | `					SyString sTrait,sMethod;` |
|       - | 6029 | `					ph7_class *pSrcTrait;` |
|       - | 6030 | `					ph7_class_method *pMeth;` |
|       - | 6031 | `					sxi32 nRKwrd;` |
|      33 | 6032 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6033 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6034 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6035 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6036 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6037 | `					sMethod = pR->sData;` |
|      13 | 6038 | `					pR++;` |
|      13 | 6039 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6040 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6041 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6042 | `							sTrait = sMethod;` |
|       7 | 6043 | `							pR++;` |
|       7 | 6044 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6045 | `							sMethod = pR->sData;` |
|       7 | 6046 | `							pR++;` |
|       3 | 6047 | `						}` |
|       3 | 6048 | `					}` |
|      13 | 6049 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6050 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6051 | `						continue;` |
|       - | 6052 | `					}` |
|      13 | 6053 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6054 | `					pR++;` |
|      13 | 6055 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6056 | `						pSrcTrait = 0;` |
|       7 | 6057 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6058 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6059 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6060 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6061 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6062 | `								break;` |
|       - | 6063 | `							}` |
|       2 | 6064 | `						}` |
|       5 | 6065 | `						if( pSrcTrait ){` |
|       5 | 6066 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6067 | `							if( pMeth ){` |
|       5 | 6068 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6069 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6070 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6071 | `								}` |
|       2 | 6072 | `							}` |
|       2 | 6073 | `						}` |
|       2 | 6074 | `					}` |
|      29 | 6075 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6076 | `				}` |
|       - | 6077 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6078 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6079 | `					ph7_class_method *pMR;` |
|       - | 6080 | `					SyHashEntry *pER;` |
|       - | 6081 | `					SyString *pNR;` |
|      11 | 6082 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6083 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6084 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6085 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6086 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6087 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6088 | `						}` |
|       1 | 6089 | `					}` |
|       6 | 6090 | `				}` |
|       - | 6091 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6092 | `				pR = pUse->pResolvStart;` |
|      21 | 6093 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6094 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6095 | `					ph7_class *pSrcTrait;` |
|       - | 6096 | `					ph7_class_method *pMeth;` |
|      21 | 6097 | `					int hasQual = 0;` |
|       - | 6098 | `					sxi32 nRKwrd;` |
|      33 | 6099 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6100 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6101 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6102 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6103 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6104 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6105 | `					sMethod = pR->sData;` |
|      13 | 6106 | `					pR++;` |
|      13 | 6107 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6108 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6109 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6110 | `							sTrait = sMethod;` |
|       7 | 6111 | `							hasQual = 1;` |
|       7 | 6112 | `							pR++;` |
|       7 | 6113 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6114 | `							sMethod = pR->sData;` |
|       7 | 6115 | `							pR++;` |
|       3 | 6116 | `						}` |
|       3 | 6117 | `					}` |
|      13 | 6118 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6119 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6120 | `						continue;` |
|       - | 6121 | `					}` |
|      13 | 6122 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6123 | `					pR++;` |
|      13 | 6124 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6125 | `						sxi32 iNewVis = -1;` |
|       9 | 6126 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6127 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6128 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6129 | `								iNewVis = nAK;` |
|       7 | 6130 | `								pR++;` |
|       3 | 6131 | `							}` |
|       3 | 6132 | `						}` |
|       9 | 6133 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6134 | `							sAlias = pR->sData;` |
|       7 | 6135 | `							pR++;` |
|       3 | 6136 | `						}` |
|       9 | 6137 | `						pMeth = 0;` |
|       9 | 6138 | `						if( hasQual ){` |
|       3 | 6139 | `							pSrcTrait = 0;` |
|       5 | 6140 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6141 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6142 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6143 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6144 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6145 | `									break;` |
|       - | 6146 | `								}` |
|       2 | 6147 | `							}` |
|       3 | 6148 | `							if( pSrcTrait ){` |
|       3 | 6149 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6150 | `							}` |
|       2 | 6151 | `						}else{` |
|       7 | 6152 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6153 | `						}` |
|       9 | 6154 | `						if( pMeth ){` |
|       9 | 6155 | `							if( sAlias.nByte > 0 ){` |
|       - | 6156 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6157 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6158 | `								 */` |
|       - | 6159 | `								ph7_class_method *pAlias;` |
|       - | 6160 | `								char *zAliasDup;` |
|       7 | 6161 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6162 | `								if( pAlias ){` |
|       7 | 6163 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6164 | `									if( iNewVis >= 0 ){` |
|       5 | 6165 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6166 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6167 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6168 | `									}` |
|       7 | 6169 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6170 | `									if( zAliasDup ){` |
|       7 | 6171 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6172 | `									}` |
|       4 | 6173 | `								}` |
|       6 | 6174 | `							}else if( iNewVis >= 0 ){` |
|       - | 6175 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6176 | `								ph7_class_method *pCopy;` |
|       3 | 6177 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6178 | `								if( pCopy ){` |
|       3 | 6179 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6180 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6181 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6182 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6183 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6184 | `									/* Replace the method in the class hash */` |
|       3 | 6185 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6186 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6187 | `								}` |
|       1 | 6188 | `							}` |
|       4 | 6189 | `						}` |
|       4 | 6190 | `						SXUNUSED(hasQual);` |
|       4 | 6191 | `					}` |
|      17 | 6192 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6193 | `				}` |
|       - | 6194 | `			}` |
|      41 | 6195 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6196 | `		}` |
|       - | 6197 | `	}` |
|       - | 6198 | `	/* Install the class */` |
|   33938 | 6199 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   33938 | 6200 | `	if( rc == SXRET_OK ){` |
|       - | 6201 | `		ph7_class **apInterface;` |
|       - | 6202 | `		sxu32 n;` |
|   33938 | 6203 | `		if( pBase ){` |
|       - | 6204 | `			/* Inherit from base class and mark as a subclass */` |
|   19636 | 6205 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|    9817 | 6206 | `		}` |
|   33938 | 6207 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   36772 | 6208 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6209 | `			/* Implements one or more interface */` |
|    2836 | 6210 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2836 | 6211 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6212 | `				break;` |
|       - | 6213 | `			}` |
|    1419 | 6214 | `		}` |
|       - | 6215 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   33938 | 6216 | `		if( rc == SXRET_OK ){` |
|   33938 | 6217 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   33938 | 6218 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6219 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6220 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6221 | `				return SXERR_ABORT;` |
|       - | 6222 | `			}` |
|   16968 | 6223 | `		}` |
|       - | 6224 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   33938 | 6225 | `		if( rc == SXRET_OK ){` |
|   33938 | 6226 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   33938 | 6227 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6228 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6229 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6230 | `				return SXERR_ABORT;` |
|       - | 6231 | `			}` |
|   16968 | 6232 | `		}` |
|   16968 | 6233 | `	}` |
|   33938 | 6234 | `	SySetRelease(&aUseEntries);` |
|   33938 | 6235 | `	SySetRelease(&aInterfaces);` |
|   33938 | 6236 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6237 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6238 | `		return SXERR_ABORT;` |
|       - | 6239 | `	}` |
|   16968 | 6240 | `done:` |
|       - | 6241 | `	/* Point beyond the class body */` |
|   33942 | 6242 | `	pGen->pIn = &pEnd[1];` |
|   33942 | 6243 | `	pGen->pEnd = pTmp;` |
|   33942 | 6244 | `	return PH7_OK;` |
|   16972 | 6245 |  |
|       - | 6246 | `/*` |
|       - | 6247 | ` * Compile a user-defined abstract class.` |
|       - | 6248 | ` *  According to the PHP language reference manual` |
|       - | 6249 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6250 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6251 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6252 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6253 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6254 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6255 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6256 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6257 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6258 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6259 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6260 | ` *   could differ.` |
|       - | 6261 | ` */` |
|      16 | 6262 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6263 |  |
|       - | 6264 | `	sxi32 rc;` |
|      18 | 6265 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6266 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6267 | `	return rc;` |
|       2 | 6268 |  |
|       - | 6269 | `/*` |
|       - | 6270 | ` * Compile a user-defined final class.` |
|       - | 6271 | ` *  According to the PHP language reference manual` |
|       - | 6272 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6273 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6274 | ` *    final then it cannot be extended.` |
|       - | 6275 | ` */` |
|       2 | 6276 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6277 |  |
|       - | 6278 | `	sxi32 rc;` |
|       3 | 6279 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6280 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6281 | `	return rc;` |
|       1 | 6282 |  |
|       - | 6283 | `/*` |
|       - | 6284 | ` * Compile a user-defined trait.` |
|       - | 6285 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6286 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6287 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6288 | ` */` |
|      50 | 6289 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       1 | 6290 |  |
|      51 | 6291 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6292 | `	ph7_class *pClass;` |
|       - | 6293 | `	SyToken *pEnd,*pTmp;` |
|       - | 6294 | `	sxi32 iProtection;` |
|       - | 6295 | `	sxi32 iAttrflags;` |
|       - | 6296 | `	SyString *pName;` |
|       - | 6297 | `	sxi32 nKwrd;` |
|       - | 6298 | `	sxi32 rc;` |
|       - | 6299 | `	/* Jump the 'trait' keyword */` |
|      51 | 6300 | `	pGen->pIn++;` |
|      51 | 6301 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6302 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6303 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6304 | `			return SXERR_ABORT;` |
|       - | 6305 | `		}` |
|     ! 0 | 6306 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6307 | `			pGen->pIn++;` |
|     ! 0 | 6308 | `		}` |
|     ! 0 | 6309 | `		return SXRET_OK;` |
|       - | 6310 | `	}` |
|       - | 6311 | `	/* Extract trait name */` |
|      51 | 6312 | `	pName = &pGen->pIn->sData;` |
|      51 | 6313 | `	pGen->pIn++;` |
|       - | 6314 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6315 | `		SyBlob sFQN;` |
|       - | 6316 | `		SyString sFQNStr;` |
|      51 | 6317 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      51 | 6318 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      51 | 6319 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      51 | 6320 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      51 | 6321 | `		SyBlobRelease(&sFQN);` |
|       - | 6322 | `	}` |
|      51 | 6323 | `	if( pClass == 0 ){` |
|     ! 0 | 6324 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6325 | `		return SXERR_ABORT;` |
|       - | 6326 | `	}` |
|       - | 6327 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      51 | 6328 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6329 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6330 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6331 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6332 | `			return SXERR_ABORT;` |
|       - | 6333 | `		}` |
|     ! 0 | 6334 | `		return SXRET_OK;` |
|       - | 6335 | `	}` |
|      51 | 6336 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      51 | 6337 | `	pEnd = 0;` |
|      51 | 6338 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      51 | 6339 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6340 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6341 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6343 | `			return SXERR_ABORT;` |
|       - | 6344 | `		}` |
|     ! 0 | 6345 | `		return SXRET_OK;` |
|       - | 6346 | `	}` |
|       - | 6347 | `	/* Swap token stream */` |
|      51 | 6348 | `	pTmp = pGen->pEnd;` |
|      51 | 6349 | `	pGen->pEnd = pEnd;` |
|       - | 6350 | `	/* Mark as trait */` |
|      51 | 6351 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6352 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      52 | 6353 | `	for(;;){` |
|     141 | 6354 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6355 | `			pGen->pIn++;` |
|       1 | 6356 | `		}` |
|     121 | 6357 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      51 | 6358 | `			break;` |
|       - | 6359 | `		}` |
|      71 | 6360 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6361 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6362 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6363 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6364 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6365 | `				return SXERR_ABORT;` |
|       - | 6366 | `			}` |
|     ! 0 | 6367 | `			goto done;` |
|       - | 6368 | `		}` |
|      71 | 6369 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6370 | `		iAttrflags = 0;` |
|      71 | 6371 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6372 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6373 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6374 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6375 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6376 | `				for(;;){` |
|       - | 6377 | `					ph7_class *pUsedTrait;` |
|       - | 6378 | `					SyString *pUsedName;` |
|       5 | 6379 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6380 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6381 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6382 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6383 | `							return SXERR_ABORT;` |
|       - | 6384 | `						}` |
|     ! 0 | 6385 | `						break;` |
|       - | 6386 | `					}` |
|       5 | 6387 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6388 | `					{` |
|       - | 6389 | `						SyBlob sResolved;` |
|       5 | 6390 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6391 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6392 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6393 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6394 | `						SyBlobRelease(&sResolved);` |
|       - | 6395 | `					}` |
|       5 | 6396 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6397 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6398 | `					}` |
|       5 | 6399 | `					if( pUsedTrait == 0 ){` |
|       4 | 6400 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6401 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6402 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6403 | `							return SXERR_ABORT;` |
|       - | 6404 | `						}` |
|       2 | 6405 | `					}else{` |
|       3 | 6406 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6407 | `					}` |
|       5 | 6408 | `					pGen->pIn++;` |
|       5 | 6409 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6410 | `						break;` |
|       - | 6411 | `					}` |
|     ! 0 | 6412 | `					pGen->pIn++;` |
|     ! 0 | 6413 | `				}` |
|       5 | 6414 | `				continue;` |
|       - | 6415 | `			}` |
|      67 | 6416 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6417 | `				iProtection = nKwrd;` |
|      63 | 6418 | `				pGen->pIn++;` |
|      63 | 6419 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6420 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6421 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6422 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6423 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6424 | `						return SXERR_ABORT;` |
|       - | 6425 | `					}` |
|     ! 0 | 6426 | `					goto done;` |
|       - | 6427 | `				}` |
|      63 | 6428 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6429 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6430 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6431 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6432 | `							return SXERR_ABORT;` |
|       - | 6433 | `						}` |
|     ! 0 | 6434 | `						goto done;` |
|       - | 6435 | `					}` |
|      11 | 6436 | `					continue;` |
|       - | 6437 | `				}` |
|      53 | 6438 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6439 | `			}` |
|      57 | 6440 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6441 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6442 | `					"Traits cannot have constants");` |
|     ! 0 | 6443 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6444 | `					return SXERR_ABORT;` |
|       - | 6445 | `				}` |
|     ! 0 | 6446 | `				goto done;` |
|     ! 0 | 6447 | `			}else{` |
|      57 | 6448 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6449 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6450 | `					pGen->pIn++;` |
|       5 | 6451 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6452 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6453 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6454 | `							iProtection = nKwrd;` |
|     ! 0 | 6455 | `							pGen->pIn++;` |
|     ! 0 | 6456 | `						}` |
|       1 | 6457 | `					}` |
|       5 | 6458 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6459 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6460 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6461 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6462 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6463 | `							return SXERR_ABORT;` |
|       - | 6464 | `						}` |
|     ! 0 | 6465 | `						goto done;` |
|       - | 6466 | `					}` |
|       5 | 6467 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6468 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6469 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6470 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6471 | `								return SXERR_ABORT;` |
|       - | 6472 | `							}` |
|     ! 0 | 6473 | `							goto done;` |
|       - | 6474 | `						}` |
|       3 | 6475 | `						continue;` |
|       - | 6476 | `					}` |
|       3 | 6477 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6478 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6479 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6480 | `					pGen->pIn++;` |
|       5 | 6481 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6482 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6483 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6484 | `							iProtection = nKwrd;` |
|       5 | 6485 | `							pGen->pIn++;` |
|       2 | 6486 | `						}` |
|       2 | 6487 | `					}` |
|       5 | 6488 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6489 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6490 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6491 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6492 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6493 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6494 | `							return SXERR_ABORT;` |
|       - | 6495 | `						}` |
|     ! 0 | 6496 | `						goto done;` |
|       - | 6497 | `					}` |
|       5 | 6498 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6499 | `				}` |
|      55 | 6500 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6501 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6502 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6503 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6504 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6505 | `						return SXERR_ABORT;` |
|       - | 6506 | `					}` |
|     ! 0 | 6507 | `					goto done;` |
|       - | 6508 | `				}` |
|      55 | 6509 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6510 | `					pGen->pIn++;` |
|     ! 0 | 6511 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6512 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6513 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6514 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6515 | `							return SXERR_ABORT;` |
|       - | 6516 | `						}` |
|     ! 0 | 6517 | `						goto done;` |
|       - | 6518 | `					}` |
|     ! 0 | 6519 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6520 | `				}else{` |
|      55 | 6521 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6522 | `				}` |
|      55 | 6523 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6524 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6525 | `						return SXERR_ABORT;` |
|       - | 6526 | `					}` |
|     ! 0 | 6527 | `					goto done;` |
|       - | 6528 | `				}` |
|       - | 6529 | `			}` |
|      28 | 6530 | `		}else{` |
|     ! 0 | 6531 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6532 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6534 | `					return SXERR_ABORT;` |
|       - | 6535 | `				}` |
|     ! 0 | 6536 | `				goto done;` |
|       - | 6537 | `			}` |
|       - | 6538 | `		}` |
|       1 | 6539 | `	}` |
|       - | 6540 | `	/* Install the trait */` |
|      51 | 6541 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      51 | 6542 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6543 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6544 | `		return SXERR_ABORT;` |
|       - | 6545 | `	}` |
|      25 | 6546 | `done:` |
|       - | 6547 | `	/* Point beyond the trait body */` |
|      51 | 6548 | `	pGen->pIn = &pEnd[1];` |
|      51 | 6549 | `	pGen->pEnd = pTmp;` |
|      51 | 6550 | `	return PH7_OK;` |
|      26 | 6551 |  |
|       - | 6552 | `/*` |
|       - | 6553 | ` * Compile a user-defined class.` |
|       - | 6554 | ` *  According to the PHP language reference manual` |
|       - | 6555 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6556 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6557 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6558 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6559 | ` *   and functions (called "methods").` |
|       - | 6560 | ` */` |
|   33922 | 6561 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6562 |  |
|       - | 6563 | `	sxi32 rc;` |
|   33924 | 6564 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   33924 | 6565 | `	return rc;` |
|       2 | 6566 |  |
|       - | 6567 | `/*` |
|       - | 6568 | ` * Exception handling.` |
|       - | 6569 | ` *  According to the PHP language reference manual` |
|       - | 6570 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6571 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6572 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6573 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6574 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6575 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6576 | ` *    (or re-thrown) within a catch block.` |
|       - | 6577 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6578 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6579 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6580 | ` *    been defined with set_exception_handler().` |
|       - | 6581 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6582 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6583 | ` */` |
|       - | 6584 | `/*` |
|       - | 6585 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6586 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6587 | ` * indicates failure.` |
|       - | 6588 | ` */` |
|    8420 | 6589 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6590 |  |
|    8422 | 6591 | `	sxi32 rc = SXRET_OK;` |
|    8422 | 6592 | `	if( pRoot->pOp ){` |
|    8418 | 6593 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4211 | 6594 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6595 | `			/* Unexpected expression */` |
|     ! 0 | 6596 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6597 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6598 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6599 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6600 | `			}` |
|       2 | 6601 | `		}` |
|    4212 | 6602 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6603 | `		/* Unexpected expression */` |
|     ! 0 | 6604 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6605 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6606 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6607 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6608 | `		}` |
|     ! 0 | 6609 | `	}` |
|    8422 | 6610 | `	return rc;` |
|       2 | 6611 |  |
|       - | 6612 | `/*` |
|       - | 6613 | ` * Compile a 'throw' statement.` |
|       - | 6614 | ` * throw: This is how you trigger an exception.` |
|       - | 6615 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6616 | ` */` |
|    8420 | 6617 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6618 |  |
|    8422 | 6619 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6620 | `	GenBlock *pBlock;` |
|       - | 6621 | `	sxu32 nIdx;` |
|       - | 6622 | `	sxi32 rc;` |
|    8422 | 6623 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6624 | `	/* Compile the expression */` |
|    8422 | 6625 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8422 | 6626 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6627 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6629 | `			return SXERR_ABORT;` |
|       - | 6630 | `		}` |
|     ! 0 | 6631 | `		return SXRET_OK;` |
|       - | 6632 | `	}` |
|    8422 | 6633 | `	pBlock = pGen->pCurrent;` |
|       - | 6634 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   39210 | 6635 | `	while(pBlock->pParent){` |
|   39206 | 6636 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8418 | 6637 | `			break;` |
|       - | 6638 | `		}` |
|       - | 6639 | `		/* Point to the parent block */` |
|   30790 | 6640 | `		pBlock = pBlock->pParent;` |
|       2 | 6641 | `	}` |
|       - | 6642 | `	/* Emit the throw instruction */` |
|    8422 | 6643 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6644 | `	/* Emit the jump */` |
|    8422 | 6645 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8422 | 6646 | `	return SXRET_OK;` |
|    4212 | 6647 |  |
|       - | 6648 | `/*` |
|       - | 6649 | ` * Compile a 'catch' block.` |
|       - | 6650 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6651 | ` * an object containing the exception information.` |
|       - | 6652 | ` */` |
|      56 | 6653 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6654 |  |
|      58 | 6655 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6656 | `	ph7_exception_block sCatch;` |
|       - | 6657 | `	SySet *pInstrContainer;` |
|       - | 6658 | `	GenBlock *pCatch;` |
|       - | 6659 | `	SyToken *pToken;` |
|       - | 6660 | `	SyString *pName;` |
|       - | 6661 | `	char *zDup;` |
|       - | 6662 | `	sxi32 rc;` |
|      58 | 6663 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6664 | `	/* Zero the structure */` |
|      58 | 6665 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6666 | `	/* Initialize fields */` |
|      58 | 6667 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|      84 | 6668 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ \|\|` |
|      58 | 6669 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6670 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6671 | `			pToken = pGen->pIn;` |
|     ! 0 | 6672 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6673 | `				pToken--;` |
|     ! 0 | 6674 | `			}` |
|     ! 0 | 6675 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6676 | `				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);` |
|     ! 0 | 6677 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6678 | `				return SXERR_ABORT;` |
|       - | 6679 | `			}` |
|     ! 0 | 6680 | `			return SXERR_INVALID;` |
|       - | 6681 | `	}` |
|       - | 6682 | `	/* Extract the exception class */` |
|      58 | 6683 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|       - | 6684 | `	/* Duplicate class name */` |
|      58 | 6685 | `	pName = &pGen->pIn->sData;` |
|      58 | 6686 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6687 | `	if( zDup == 0 ){` |
|     ! 0 | 6688 | `		goto Mem;` |
|       - | 6689 | `	}` |
|      58 | 6690 | `	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);` |
|      58 | 6691 | `	pGen->pIn++;` |
|      84 | 6692 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      58 | 6693 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6694 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6695 | `			pToken = pGen->pIn;` |
|     ! 0 | 6696 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6697 | `				pToken--;` |
|     ! 0 | 6698 | `			}` |
|     ! 0 | 6699 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6700 | `				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);` |
|     ! 0 | 6701 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6702 | `				return SXERR_ABORT;` |
|       - | 6703 | `			}` |
|     ! 0 | 6704 | `			return SXERR_INVALID;` |
|       - | 6705 | `	}` |
|      58 | 6706 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6707 | `	/* Duplicate instance name */` |
|      58 | 6708 | `	pName = &pGen->pIn->sData;` |
|      58 | 6709 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      58 | 6710 | `	if( zDup == 0 ){` |
|     ! 0 | 6711 | `		goto Mem;` |
|       - | 6712 | `	}` |
|      58 | 6713 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      58 | 6714 | `	pGen->pIn++;` |
|      58 | 6715 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6716 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6717 | `		pToken = pGen->pIn;` |
|     ! 0 | 6718 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6719 | `			pToken--;` |
|     ! 0 | 6720 | `		}` |
|     ! 0 | 6721 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,` |
|     ! 0 | 6722 | `			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);` |
|     ! 0 | 6723 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6724 | `			return SXERR_ABORT;` |
|       - | 6725 | `		}` |
|     ! 0 | 6726 | `		return SXERR_INVALID;` |
|       - | 6727 | `	}` |
|       - | 6728 | `	/* Compile the block */` |
|      58 | 6729 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6730 | `	/* Create the catch block */` |
|      58 | 6731 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      58 | 6732 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6733 | `		return SXERR_ABORT;` |
|       - | 6734 | `	}` |
|       - | 6735 | `	/* Swap bytecode container */` |
|      58 | 6736 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      58 | 6737 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 6738 | `	/* Compile the block */` |
|      58 | 6739 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 6740 | `	/* Fix forward jumps now the destination is resolved  */` |
|      58 | 6741 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6742 | `	/* Emit the DONE instruction */` |
|      58 | 6743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6744 | `	/* Leave the block */` |
|      58 | 6745 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6746 | `	/* Restore the default container */` |
|      58 | 6747 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6748 | `	/* Install the catch block */` |
|      58 | 6749 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      58 | 6750 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6751 | `		goto Mem;` |
|       - | 6752 | `	}` |
|      58 | 6753 | `	return SXRET_OK;` |
|     ! 0 | 6754 | `Mem:` |
|     ! 0 | 6755 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6756 | `	return SXERR_ABORT;` |
|      30 | 6757 |  |
|       - | 6758 | `/*` |
|       - | 6759 | ` * Compile a 'try' block.` |
|       - | 6760 | ` * A function using an exception should be in a "try" block.` |
|       - | 6761 | ` * If the exception does not trigger, the code will continue` |
|       - | 6762 | ` * as normal. However if the exception triggers, an exception` |
|       - | 6763 | ` * is "thrown".` |
|       - | 6764 | ` */` |
|      68 | 6765 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 6766 |  |
|       - | 6767 | `	ph7_exception *pException;` |
|      70 | 6768 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6769 | `	GenBlock *pTry;` |
|       - | 6770 | `	sxu32 nJmpIdx;` |
|       - | 6771 | `	sxi32 rc;` |
|       - | 6772 | `	/* Create the exception container */` |
|      70 | 6773 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|      70 | 6774 | `	if( pException == 0 ){` |
|     ! 0 | 6775 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 6776 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6777 | `		return SXERR_ABORT;` |
|       - | 6778 | `	}` |
|       - | 6779 | `	/* Zero the structure */` |
|      70 | 6780 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 6781 | `	/* Initialize fields */` |
|      70 | 6782 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|      70 | 6783 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      70 | 6784 | `	pException->iHasFinally = 0;` |
|      70 | 6785 | `	pException->iFinallyDone = 0;` |
|      70 | 6786 | `	pException->pVm = pGen->pVm;` |
|       - | 6787 | `	/* Create the try block */` |
|      70 | 6788 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|      70 | 6789 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6790 | `		return SXERR_ABORT;` |
|       - | 6791 | `	}` |
|       - | 6792 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|      70 | 6793 | `	pTry->pUserData = pException;` |
|       - | 6794 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|      70 | 6795 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 6796 | `	/* Fix the jump later when the destination is resolved */` |
|      70 | 6797 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|      70 | 6798 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 6799 | `	/* Compile the block */` |
|      70 | 6800 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      70 | 6801 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6802 | `		return SXERR_ABORT;` |
|       - | 6803 | `	}` |
|       - | 6804 | `	/* Fix forward jumps now the destination is resolved */` |
|      70 | 6805 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6806 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|      70 | 6807 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 6808 | `	/* Leave the block */` |
|      70 | 6809 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6810 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|      70 | 6811 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      66 | 6812 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 6813 | `		/* Compile one or more catch blocks */` |
|      56 | 6814 | `		for(;;){` |
|     112 | 6815 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 | 6816 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      30 | 6817 | `					break;` |
|       - | 6818 | `			}` |
|      58 | 6819 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|      58 | 6820 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6821 | `				return SXERR_ABORT;` |
|       - | 6822 | `			}` |
|       2 | 6823 | `		}` |
|      28 | 6824 | `	}` |
|       - | 6825 | `	/* Compile optional finally block */` |
|      70 | 6826 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      36 | 6827 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 6828 | `		SySet *pInstrContainer;` |
|       - | 6829 | `		GenBlock *pFinBlock;` |
|      27 | 6830 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 6831 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      27 | 6832 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      27 | 6833 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6834 | `			return SXERR_ABORT;` |
|       - | 6835 | `		}` |
|       - | 6836 | `		/* Swap bytecode container */` |
|      27 | 6837 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      27 | 6838 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 6839 | `		/* Compile the finally body */` |
|      27 | 6840 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      27 | 6841 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6842 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 6843 | `			return SXERR_ABORT;` |
|       - | 6844 | `		}` |
|       - | 6845 | `		/* Fix forward jumps now the destination is resolved */` |
|      27 | 6846 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 6847 | `		/* Emit DONE to terminate the finally block */` |
|      27 | 6848 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 6849 | `		/* Leave the block */` |
|      27 | 6850 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 6851 | `		/* Restore the default container */` |
|      27 | 6852 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      27 | 6853 | `		pException->iHasFinally = 1;` |
|      13 | 6854 | `	}` |
|       - | 6855 | `	/* Must have at least one catch or finally */` |
|      70 | 6856 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       3 | 6857 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 6858 | `			"Cannot use try without catch or finally");` |
|       3 | 6859 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6860 | `			return SXERR_ABORT;` |
|       - | 6861 | `		}` |
|       1 | 6862 | `	}` |
|      70 | 6863 | `	return SXRET_OK;` |
|      36 | 6864 |  |
|       - | 6865 | `/*` |
|       - | 6866 | ` * Compile a switch block.` |
|       - | 6867 | ` *  (See block-comment below for more information)` |
|       - | 6868 | ` */` |
|      84 | 6869 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 6870 |  |
|      86 | 6871 | `	sxi32 rc = SXRET_OK;` |
|      86 | 6872 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 6873 | `		/* Unexpected token */` |
|     ! 0 | 6874 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 6875 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6876 | `			return SXERR_ABORT;` |
|       - | 6877 | `		}` |
|     ! 0 | 6878 | `		pGen->pIn++;` |
|     ! 0 | 6879 | `	}` |
|      86 | 6880 | `	pGen->pIn++;` |
|       - | 6881 | `	/* First instruction to execute in this block. */` |
|      86 | 6882 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 6883 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 6884 | `	 * or the '}' token */` |
|     151 | 6885 | `	for(;;){` |
|     304 | 6886 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6887 | `			/* No more input to process */` |
|     ! 0 | 6888 | `			break;` |
|       - | 6889 | `		}` |
|     304 | 6890 | `		rc = SXRET_OK;` |
|     304 | 6891 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      62 | 6892 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      20 | 6893 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 6894 | `					/* Unexpected token */` |
|     ! 0 | 6895 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6896 | `						&pGen->pIn->sData);` |
|     ! 0 | 6897 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6898 | `						return SXERR_ABORT;` |
|       - | 6899 | `					}` |
|       - | 6900 | `					/* FALL THROUGH */` |
|     ! 0 | 6901 | `				}` |
|      20 | 6902 | `				rc = SXERR_EOF;` |
|      20 | 6903 | `				break;` |
|       - | 6904 | `			}` |
|      23 | 6905 | `		}else{` |
|       - | 6906 | `			sxi32 nKwrd;` |
|       - | 6907 | `			/* Extract the keyword */` |
|     244 | 6908 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     244 | 6909 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      34 | 6910 | `				break;` |
|       - | 6911 | `			}` |
|     180 | 6912 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 6913 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 6914 | `					/* Unexpected token */` |
|     ! 0 | 6915 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 6916 | `						&pGen->pIn->sData);` |
|     ! 0 | 6917 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6918 | `						return SXERR_ABORT;` |
|       - | 6919 | `					}` |
|       - | 6920 | `					/* FALL THROUGH */` |
|     ! 0 | 6921 | `				}` |
|       - | 6922 | `				/* Block compiled */` |
|       3 | 6923 | `				break;` |
|       - | 6924 | `			}` |
|       - | 6925 | `		}` |
|       - | 6926 | `		/* Compile block */` |
|     220 | 6927 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 | 6928 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6929 | `			return SXERR_ABORT;` |
|       - | 6930 | `		}` |
|       2 | 6931 | `	}` |
|      86 | 6932 | `	return rc;` |
|      44 | 6933 |  |
|       - | 6934 | `/*` |
|       - | 6935 | ` * Compile a case eXpression.` |
|       - | 6936 | ` *  (See block-comment below for more information)` |
|       - | 6937 | ` */` |
|      70 | 6938 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 6939 |  |
|       - | 6940 | `	SySet *pInstrContainer;` |
|       - | 6941 | `	SyToken *pEnd,*pTmp;` |
|      72 | 6942 | `	sxi32 iNest = 0;` |
|       - | 6943 | `	sxi32 rc;` |
|       - | 6944 | `	/* Delimit the expression */` |
|      72 | 6945 | `	pEnd = pGen->pIn;` |
|     150 | 6946 | `	while( pEnd < pGen->pEnd ){` |
|     150 | 6947 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 6948 | `			/* Increment nesting level */` |
|       3 | 6949 | `			iNest++;` |
|     149 | 6950 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 6951 | `			/* Decrement nesting level */` |
|       3 | 6952 | `			iNest--;` |
|     147 | 6953 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      72 | 6954 | `			break;` |
|       - | 6955 | `		}` |
|      80 | 6956 | `		pEnd++;` |
|       2 | 6957 | `	}` |
|      72 | 6958 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 6959 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 6960 | `		if( rc == SXERR_ABORT ){` |
|       - | 6961 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6962 | `			return SXERR_ABORT;` |
|       - | 6963 | `		}` |
|     ! 0 | 6964 | `	}` |
|       - | 6965 | `	/* Swap token stream */` |
|      72 | 6966 | `	pTmp = pGen->pEnd;` |
|      72 | 6967 | `	pGen->pEnd = pEnd;` |
|      72 | 6968 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      72 | 6969 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      72 | 6970 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 6971 | `	/* Emit the done instruction */` |
|      72 | 6972 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      72 | 6973 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 6974 | `	/* Update token stream */` |
|      72 | 6975 | `	pGen->pIn  = pEnd;` |
|      72 | 6976 | `	pGen->pEnd = pTmp;` |
|      72 | 6977 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6978 | `		return SXERR_ABORT;` |
|       - | 6979 | `	}` |
|      72 | 6980 | `	return SXRET_OK;` |
|      37 | 6981 |  |
|       - | 6982 | `/*` |
|       - | 6983 | ` * Compile the smart switch statement.` |
|       - | 6984 | ` * According to the PHP language reference manual` |
|       - | 6985 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 6986 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 6987 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 6988 | ` *  This is exactly what the switch statement is for.` |
|       - | 6989 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 6990 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 6991 | ` *  of the outer loop, use continue 2.` |
|       - | 6992 | ` *  Note that switch/case does loose comparision.` |
|       - | 6993 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 6994 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 6995 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 6996 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 6997 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 6998 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 6999 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7000 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7001 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7002 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7003 | ` *  list for the next case.` |
|       - | 7004 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7005 | ` *  or floating-point numbers and strings.` |
|       - | 7006 | ` */` |
|      20 | 7007 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7008 |  |
|       - | 7009 | `	GenBlock *pSwitchBlock;` |
|       - | 7010 | `	SyToken *pTmp,*pEnd;` |
|       - | 7011 | `	ph7_switch *pSwitch;` |
|       - | 7012 | `	sxu32 nToken;` |
|       - | 7013 | `	sxu32 nLine;` |
|       - | 7014 | `	sxi32 rc;` |
|      22 | 7015 | `	nLine = pGen->pIn->nLine;` |
|       - | 7016 | `	/* Jump the 'switch' keyword */` |
|      22 | 7017 | `	pGen->pIn++;` |
|      22 | 7018 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7019 | `		/* Syntax error */` |
|     ! 0 | 7020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7021 | `		if( rc == SXERR_ABORT ){` |
|       - | 7022 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7023 | `			return SXERR_ABORT;` |
|       - | 7024 | `		}` |
|     ! 0 | 7025 | `		goto Synchronize;` |
|       - | 7026 | `	}` |
|       - | 7027 | `	/* Jump the left parenthesis '(' */` |
|      22 | 7028 | `	pGen->pIn++;` |
|      22 | 7029 | `	pEnd = 0; /* cc warning */` |
|       - | 7030 | `	/* Create the loop block */` |
|      32 | 7031 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      10 | 7032 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      22 | 7033 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7034 | `		return SXERR_ABORT;` |
|       - | 7035 | `	}` |
|       - | 7036 | `	/* Delimit the condition */` |
|      22 | 7037 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      22 | 7038 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7039 | `		/* Empty expression */` |
|     ! 0 | 7040 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7041 | `		if( rc == SXERR_ABORT ){` |
|       - | 7042 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7043 | `			return SXERR_ABORT;` |
|       - | 7044 | `		}` |
|     ! 0 | 7045 | `	}` |
|       - | 7046 | `	/* Swap token streams */` |
|      22 | 7047 | `	pTmp = pGen->pEnd;` |
|      22 | 7048 | `	pGen->pEnd = pEnd;` |
|       - | 7049 | `	/* Compile the expression */` |
|      22 | 7050 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      22 | 7051 | `	if( rc == SXERR_ABORT ){` |
|       - | 7052 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7053 | `		return SXERR_ABORT;` |
|       - | 7054 | `	}` |
|       - | 7055 | `	/* Update token stream */` |
|      22 | 7056 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7057 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7058 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7059 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7060 | `			return SXERR_ABORT;` |
|       - | 7061 | `		}` |
|     ! 0 | 7062 | `		pGen->pIn++;` |
|     ! 0 | 7063 | `	}` |
|      22 | 7064 | `	pGen->pIn  = &pEnd[1];` |
|      22 | 7065 | `	pGen->pEnd = pTmp;` |
|      22 | 7066 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      20 | 7067 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7068 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7069 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7070 | `				pTmp--;` |
|     ! 0 | 7071 | `			}` |
|       - | 7072 | `			/* Unexpected token */` |
|     ! 0 | 7073 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7074 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7075 | `				return SXERR_ABORT;` |
|       - | 7076 | `			}` |
|     ! 0 | 7077 | `			goto Synchronize;` |
|       - | 7078 | `	}` |
|       - | 7079 | `	/* Set the delimiter token */` |
|      22 | 7080 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7081 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7082 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7083 | `	}else{` |
|      20 | 7084 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7085 | `	}` |
|      22 | 7086 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7087 | `	/* Create the switch blocks container */` |
|      22 | 7088 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      22 | 7089 | `	if( pSwitch == 0 ){` |
|       - | 7090 | `		/* Abort compilation */` |
|     ! 0 | 7091 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7092 | `		return SXERR_ABORT;` |
|       - | 7093 | `	}` |
|       - | 7094 | `	/* Zero the structure */` |
|      22 | 7095 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7096 | `	/* Initialize fields */` |
|      22 | 7097 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7098 | `	/* Emit the switch instruction */` |
|      22 | 7099 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7100 | `	/* Compile case blocks */` |
|      76 | 7101 | `	for(;;){` |
|       - | 7102 | `		sxu32 nKwrd;` |
|      88 | 7103 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7104 | `			/* No more input to process */` |
|     ! 0 | 7105 | `			break;` |
|       - | 7106 | `		}` |
|      88 | 7107 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7108 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7109 | `				/* Unexpected token */` |
|     ! 0 | 7110 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7111 | `					&pGen->pIn->sData);` |
|     ! 0 | 7112 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7113 | `					return SXERR_ABORT;` |
|       - | 7114 | `				}` |
|       - | 7115 | `				/* FALL THROUGH */` |
|     ! 0 | 7116 | `			}` |
|       - | 7117 | `			/* Block compiled */` |
|     ! 0 | 7118 | `			break;` |
|       - | 7119 | `		}` |
|       - | 7120 | `		/* Extract the keyword */` |
|      88 | 7121 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      88 | 7122 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7123 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7124 | `				/* Unexpected token */` |
|     ! 0 | 7125 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7126 | `					&pGen->pIn->sData);` |
|     ! 0 | 7127 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7128 | `					return SXERR_ABORT;` |
|       - | 7129 | `				}` |
|       - | 7130 | `				/* FALL THROUGH */` |
|     ! 0 | 7131 | `			}` |
|       - | 7132 | `			/* Block compiled */` |
|       3 | 7133 | `			break;` |
|       - | 7134 | `		}` |
|      86 | 7135 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7136 | `			/*` |
|       - | 7137 | `			 * Accroding to the PHP language reference manual` |
|       - | 7138 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7139 | `			 *  that wasn't matched by the other cases.` |
|       - | 7140 | `			 */` |
|      16 | 7141 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7142 | `				/* Default case already compiled */` |
|     ! 0 | 7143 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7144 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7145 | `					return SXERR_ABORT;` |
|       - | 7146 | `				}` |
|     ! 0 | 7147 | `			}` |
|      16 | 7148 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7149 | `			/* Compile the default block */` |
|      16 | 7150 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      16 | 7151 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7152 | `				return SXERR_ABORT;` |
|      16 | 7153 | `			}else if( rc == SXERR_EOF ){` |
|      14 | 7154 | `				break;` |
|       1 | 7155 | `			}` |
|      73 | 7156 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7157 | `			ph7_case_expr sCase;` |
|       - | 7158 | `			/* Standard case block */` |
|      72 | 7159 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7160 | `			/* initialize the structure */` |
|      72 | 7161 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7162 | `			/* Compile the case expression */` |
|      72 | 7163 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      72 | 7164 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7165 | `				return SXERR_ABORT;` |
|       - | 7166 | `			}` |
|       - | 7167 | `			/* Compile the case block */` |
|      72 | 7168 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7169 | `			/* Insert in the switch container */` |
|      72 | 7170 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      72 | 7171 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7172 | `				return SXERR_ABORT;` |
|      72 | 7173 | `			}else if( rc == SXERR_EOF ){` |
|       7 | 7174 | `				break;` |
|       - | 7175 | `			}` |
|      34 | 7176 | `		}else{` |
|       - | 7177 | `			/* Unexpected token */` |
|     ! 0 | 7178 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7179 | `				&pGen->pIn->sData);` |
|     ! 0 | 7180 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7181 | `				return SXERR_ABORT;` |
|       - | 7182 | `			}` |
|     ! 0 | 7183 | `			break;` |
|       - | 7184 | `		}` |
|       2 | 7185 | `	}` |
|       - | 7186 | `	/* Fix all jumps now the destination is resolved */` |
|      22 | 7187 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      22 | 7188 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7189 | `	/* Release the loop block */` |
|      22 | 7190 | `	GenStateLeaveBlock(pGen,0);` |
|      22 | 7191 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7192 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      22 | 7193 | `		pGen->pIn++;` |
|      10 | 7194 | `	}` |
|       - | 7195 | `	/* Statement successfully compiled */` |
|      22 | 7196 | `	return SXRET_OK;` |
|     ! 0 | 7197 | `Synchronize:` |
|       - | 7198 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7199 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7200 | `		pGen->pIn++;` |
|     ! 0 | 7201 | `	}` |
|     ! 0 | 7202 | `	return SXRET_OK;` |
|      12 | 7203 |  |
|       - | 7204 | `/*` |
|       - | 7205 | ` * Generate bytecode for a given expression tree.` |
|       - | 7206 | ` * If something goes wrong while generating bytecode` |
|       - | 7207 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7208 | ` * this function takes care of generating the appropriate` |
|       - | 7209 | ` * error message.` |
|       - | 7210 | ` */` |
| 2503076 | 7211 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7212 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7213 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7214 | `	sxi32 iFlags /* Control flags */` |
|       - | 7215 | `	)` |
|       2 | 7216 |  |
|       - | 7217 | `	VmInstr *pInstr;` |
|       - | 7218 | `	sxu32 nJmpIdx;` |
| 2503078 | 7219 | `	sxi32 iP1 = 0;` |
| 2503078 | 7220 | `	sxu32 iP2 = 0;` |
| 2503078 | 7221 | `	void *p3  = 0;` |
|       - | 7222 | `	sxi32 iVmOp;` |
|       - | 7223 | `	sxi32 rc;` |
| 2503078 | 7224 | `	if( pNode->xCode ){` |
|       - | 7225 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7226 | `		/* Compile node */` |
| 1550932 | 7227 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1550932 | 7228 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1550932 | 7229 | `		RE_SWAP_DELIMITER(pGen);` |
| 1550932 | 7230 | `		return rc;` |
|       - | 7231 | `	}` |
|  952148 | 7232 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7233 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7234 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7235 | `		return SXERR_ABORT;` |
|       - | 7236 | `	}` |
|  952148 | 7237 | `	iVmOp = pNode->pOp->iVmOp;` |
|  952148 | 7238 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7239 | `		sxu32 nJz,nJmp;` |
|       - | 7240 | `		/* Ternary operator require special handling */` |
|       - | 7241 | `		/* Phase#1: Compile the condition */` |
|    1804 | 7242 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1804 | 7243 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7244 | `			return rc;` |
|       - | 7245 | `		}` |
|    1804 | 7246 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1804 | 7247 | `		if( pNode->pLeft ){` |
|       - | 7248 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7249 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1736 | 7250 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7251 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1736 | 7252 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1736 | 7253 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7254 | `				return rc;` |
|       - | 7255 | `			}` |
|     869 | 7256 | `		}else{` |
|       - | 7257 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7258 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7259 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7260 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7261 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7262 | `		}` |
|       - | 7263 | `		/* Phase#4: Emit the unconditional jump */` |
|    1804 | 7264 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7265 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1804 | 7266 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1804 | 7267 | `		if( pInstr ){` |
|    1804 | 7268 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     901 | 7269 | `		}` |
|    1804 | 7270 | `		if( !pNode->pLeft ){` |
|       - | 7271 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7272 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7273 | `		}` |
|       - | 7274 | `		/* Phase#6: Compile the 'else' expression */` |
|    1804 | 7275 | `		if( pNode->pRight ){` |
|    1804 | 7276 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1804 | 7277 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7278 | `				return rc;` |
|       - | 7279 | `			}` |
|     901 | 7280 | `		}` |
|    1804 | 7281 | `		if( nJmp > 0 ){` |
|       - | 7282 | `			/* Phase#7: Fix the unconditional jump */` |
|    1804 | 7283 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1804 | 7284 | `			if( pInstr ){` |
|    1804 | 7285 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     901 | 7286 | `			}` |
|     901 | 7287 | `		}` |
|       - | 7288 | `		/* All done */` |
|    1804 | 7289 | `		return SXRET_OK;` |
|       - | 7290 | `	}` |
|       - | 7291 | `	/* Generate code for the left tree */` |
|  950346 | 7292 | `	if( pNode->pLeft ){` |
|  950328 | 7293 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7294 | `			ph7_expr_node **apNode;` |
|       - | 7295 | `			sxi32 n;` |
|       - | 7296 | `			/* Recurse and generate bytecodes for function arguments */` |
|  319232 | 7297 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7298 | `			/* Read-only load */` |
|  319232 | 7299 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  637778 | 7300 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  318548 | 7301 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  318548 | 7302 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7303 | `					return rc;` |
|       - | 7304 | `				}` |
|  159275 | 7305 | `			}` |
|       - | 7306 | `			/* Total number of given arguments */` |
|  319232 | 7307 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 7308 | `			/* Remove stale flags now */` |
|  319232 | 7309 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  159615 | 7310 | `		}` |
|  950328 | 7311 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  950328 | 7312 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7313 | `			return rc;` |
|       - | 7314 | `		}` |
|  950328 | 7315 | `		if( iVmOp == PH7_OP_CALL ){` |
|  319232 | 7316 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  319232 | 7317 | `			if( pInstr ){` |
|  319232 | 7318 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  318740 | 7319 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7320 | `					sxu32 nQual;` |
|       - | 7321 | `					/* Prevent constant expansion */` |
|  318740 | 7322 | `					pInstr->iP1 = 0;` |
|       - | 7323 | `					/* Namespace-qualify the function name for CALL */` |
|  318740 | 7324 | `					nQual = GenStateNsQualifyName(pGen,nOrig);` |
|  318740 | 7325 | `					pInstr->iP2 = (sxi32)nQual;` |
|  318740 | 7326 | `					if( nQual != nOrig ){` |
|       - | 7327 | `						/* Name was compiler-qualified: flag CALL for host-function global fallback.` |
|       - | 7328 | `						 * p3 = (void*)1 tells the VM it's safe to strip the NS prefix` |
|       - | 7329 | `						 * and try the short name in hHostFunction. */` |
|      49 | 7330 | `						p3 = (void *)1;` |
|      26 | 7331 | `					}` |
|  159863 | 7332 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7333 | `					/* Method call,flag that */` |
|     472 | 7334 | `					pInstr->iP2 = 1;` |
|     235 | 7335 | `				}` |
|  159617 | 7336 | `			}` |
|  790713 | 7337 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7338 | `			ph7_expr_node **apNode;` |
|       - | 7339 | `			sxi32 n;` |
|       - | 7340 | `			/* Recurse and generate bytecodes for array index */` |
|   71636 | 7341 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  129238 | 7342 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   57604 | 7343 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   57604 | 7344 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7345 | `					return rc;` |
|       - | 7346 | `				}` |
|   28803 | 7347 | `			}` |
|   71636 | 7348 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   57604 | 7349 | `				iP1 = 1; /* Node have an index associated with it */` |
|   28801 | 7350 | `			}` |
|   71636 | 7351 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7352 | `				/* Create an empty entry when the desired index is not found */` |
|   28314 | 7353 | `				iP2 = 1;` |
|   14158 | 7354 | `			}` |
|  595281 | 7355 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7356 | `			/* POP the left node */` |
|      32 | 7357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7358 | `		}` |
|  475163 | 7359 | `	}` |
|  950346 | 7360 | `	rc = SXRET_OK;` |
|  950346 | 7361 | `	nJmpIdx = 0;` |
|       - | 7362 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7363 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7364 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  950346 | 7365 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     126 | 7366 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     126 | 7367 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     126 | 7368 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     126 | 7369 | `			int isSpecial = 0;` |
|     126 | 7370 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      86 | 7371 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      86 | 7372 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      90 | 7373 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      78 | 7374 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      39 | 7375 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      40 | 7376 | `					isSpecial = 1;` |
|      19 | 7377 | `				}` |
|      52 | 7378 | `			}` |
|     146 | 7379 | `			pInstr->iP1 = 0;` |
|     146 | 7380 | `			if( !isSpecial ){` |
|      68 | 7381 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      33 | 7382 | `			}` |
|      52 | 7383 | `		}` |
|      86 | 7384 | `	}` |
|       - | 7385 | `	/* Generate code for the right tree */` |
|  950330 | 7386 | `	if( pNode->pRight ){` |
|  496164 | 7387 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7388 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8806 | 7389 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  491762 | 7390 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7391 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2942 | 7392 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  485890 | 7393 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  216858 | 7394 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  108428 | 7395 | `		}` |
|  496164 | 7396 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  496164 | 7397 | `		if( iVmOp == PH7_OP_STORE ){` |
|  213920 | 7398 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  213920 | 7399 | `			if( pInstr ){` |
|  213920 | 7400 | `				if( pInstr->iOp == PH7_OP_LOAD_LIST ){` |
|       - | 7401 | `					/* Hide the STORE instruction */` |
|      26 | 7402 | `					iVmOp = 0;` |
|  213908 | 7403 | `				}else if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7404 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   47620 | 7405 | `					iP2 = 1;` |
|   23811 | 7406 | `				}else{` |
|  166278 | 7407 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7408 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   28276 | 7409 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   28276 | 7410 | `						iP1 = pInstr->iP1;` |
|   14139 | 7411 | `					}else{` |
|  138004 | 7412 | `						p3 = pInstr->p3;` |
|       - | 7413 | `					}` |
|       - | 7414 | `					/* POP the last dynamic load instruction */` |
|  166278 | 7415 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7416 | `				}` |
|  106961 | 7417 | `			}` |
|  389205 | 7418 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7419 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7420 | `			if( pInstr ){` |
|      46 | 7421 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7422 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7423 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7424 | `					 */` |
|      15 | 7425 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7426 | `					iP1 = pInstr->iP1;` |
|      15 | 7427 | `					iP2 = pInstr->iP2;` |
|      15 | 7428 | `					p3  = pInstr->p3;` |
|       8 | 7429 | `				}else{` |
|      32 | 7430 | `					p3 = pInstr->p3;` |
|       - | 7431 | `				}` |
|      22 | 7432 | `			}` |
|      22 | 7433 | `		}` |
|  248081 | 7434 | `	}` |
|  950330 | 7435 | `	if( iVmOp > 0 ){` |
|  950276 | 7436 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11418 | 7437 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7438 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8396 | 7439 | `				iP1 = 1;` |
|    4199 | 7440 | `			}` |
|  944568 | 7441 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7442 | `			/* Namespace-qualify the class name for NEW */ {` |
|   14350 | 7443 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   14350 | 7444 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   14338 | 7445 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7168 | 7446 | `				}` |
|   14350 | 7447 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7448 | `					/* Prevent constant expansion for class name */` |
|   14348 | 7449 | `					pPeek->iP1 = 0;` |
|   14348 | 7450 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pPeek->iP2);` |
|    7173 | 7451 | `				}` |
|       - | 7452 | `			}` |
|   14350 | 7453 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   14350 | 7454 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7455 | `				VmInstr *pPrev;` |
|   14338 | 7456 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   14338 | 7457 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7458 | `					/* Pop the call instruction */` |
|   14338 | 7459 | `					iP1 = pInstr->iP1;` |
|   14338 | 7460 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7168 | 7461 | `				}` |
|    7170 | 7462 | `			}` |
|  931686 | 7463 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7464 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7465 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7466 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7467 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7468 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7469 | `				int isSpecialIs = 0;` |
|      50 | 7470 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7471 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7472 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7473 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7474 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7475 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7476 | `						isSpecialIs = 1;` |
|       5 | 7477 | `					}` |
|      23 | 7478 | `				}` |
|      52 | 7479 | `				pInstr->iP1 = 0;` |
|      52 | 7480 | `				if( !isSpecialIs ){` |
|      38 | 7481 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2);` |
|      18 | 7482 | `				}` |
|      25 | 7483 | `			}` |
|  924491 | 7484 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7485 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7486 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7487 | `			 * should not trigger constant lookup. */` |
|  107090 | 7488 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  107090 | 7489 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  107074 | 7490 | `				pInstr->iP1 = 0;` |
|   53536 | 7491 | `			}` |
|  107090 | 7492 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7493 | `				/* Static member access,remember that */` |
|     110 | 7494 | `				iP1 = 1;` |
|     110 | 7495 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     110 | 7496 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7497 | `					p3 = pInstr->p3;` |
|      10 | 7498 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7499 | `				}` |
|      54 | 7500 | `			}` |
|   53544 | 7501 | `		}` |
|       - | 7502 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  950274 | 7503 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  950274 | 7504 | `		if( nJmpIdx > 0 ){` |
|       - | 7505 | `			/* Fix short-circuited jumps now the destination is resolved */` |
|   11746 | 7506 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11746 | 7507 | `			if( pInstr ){` |
|   11746 | 7508 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5872 | 7509 | `			}` |
|    5872 | 7510 | `		}` |
|  475136 | 7511 | `	}` |
|  950328 | 7512 | `	return rc;` |
| 1251531 | 7513 |  |
|       - | 7514 | `/*` |
|       - | 7515 | ` * Compile a PHP expression.` |
|       - | 7516 | ` * According to the PHP language reference manual:` |
|       - | 7517 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7518 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7519 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7520 | ` *  is "anything that has a value".` |
|       - | 7521 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7522 | ` * function takes care of generating the appropriate error` |
|       - | 7523 | ` * message.` |
|       - | 7524 | ` */` |
|  675250 | 7525 | `static sxi32 PH7_CompileExpr(` |
|       - | 7526 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7527 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7528 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7529 | `	)` |
|       2 | 7530 |  |
|       - | 7531 | `	ph7_expr_node *pRoot;` |
|       - | 7532 | `	SySet sExprNode;` |
|       - | 7533 | `	SyToken *pEnd;` |
|       - | 7534 | `	sxi32 nExpr;` |
|       - | 7535 | `	sxi32 iNest;` |
|       - | 7536 | `	sxi32 rc;` |
|       - | 7537 | `	/* Initialize worker variables */` |
|  675252 | 7538 | `	nExpr = 0;` |
|  675252 | 7539 | `	pRoot = 0;` |
|  675252 | 7540 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  675252 | 7541 | `	SySetAlloc(&sExprNode,0x10);` |
|  675252 | 7542 | `	rc = SXRET_OK;` |
|       - | 7543 | `	/* Delimit the expression */` |
|  675252 | 7544 | `	pEnd = pGen->pIn;` |
|  675252 | 7545 | `	iNest = 0;` |
| 4556948 | 7546 | `	while( pEnd < pGen->pEnd ){` |
| 4322968 | 7547 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7548 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     202 | 7549 | `			iNest++;` |
| 4322868 | 7550 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     210 | 7551 | `			iNest--;` |
| 4322664 | 7552 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  441444 | 7553 | `			if( iNest <= 0 ){` |
|  441272 | 7554 | `				break;` |
|       - | 7555 | `			}` |
|      86 | 7556 | `		}` |
| 3881698 | 7557 | `		pEnd++;` |
|       2 | 7558 | `	}` |
|  675252 | 7559 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11356 | 7560 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11356 | 7561 | `		iNest = 0;` |
|       - | 7562 | `		/* Stop at the first comma */` |
|   22734 | 7563 | `		while( pEnd2 < pEnd ){` |
|   11380 | 7564 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7565 | `				iNest++;` |
|   11378 | 7566 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7567 | `				iNest--;` |
|   11374 | 7568 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7569 | `				if( iNest <= 0 ){` |
|     ! 0 | 7570 | `					break;` |
|       - | 7571 | `				}` |
|       2 | 7572 | `			}` |
|   11380 | 7573 | `			pEnd2++;` |
|       2 | 7574 | `		}` |
|   11356 | 7575 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7576 | `			pEnd = pEnd2;` |
|     ! 0 | 7577 | `		}` |
|    5677 | 7578 | `	}` |
|  675252 | 7579 | `	if( pEnd > pGen->pIn ){` |
|  675242 | 7580 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7581 | `		/* Swap delimiter */` |
|  675242 | 7582 | `		pGen->pEnd = pEnd;` |
|       - | 7583 | `		/* Try to get an expression tree */` |
|  675242 | 7584 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  675242 | 7585 | `		if( rc == SXRET_OK && pRoot ){` |
|  675086 | 7586 | `			rc = SXRET_OK;` |
|  675086 | 7587 | `			if( xTreeValidator ){` |
|       - | 7588 | `				/* Call the upper layer validator callback */` |
|   14490 | 7589 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7244 | 7590 | `			}` |
|  675086 | 7591 | `			if( rc != SXERR_ABORT ){` |
|       - | 7592 | `				/* Generate code for the given tree */` |
|  675086 | 7593 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  337542 | 7594 | `			}` |
|  675086 | 7595 | `			nExpr = 1;` |
|  337542 | 7596 | `		}` |
|       - | 7597 | `		/* Release the whole tree */` |
|  675242 | 7598 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7599 | `		/* Synchronize token stream */` |
|  675242 | 7600 | `		pGen->pEnd = pTmp;` |
|  675242 | 7601 | `		pGen->pIn  = pEnd;` |
|  675242 | 7602 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7603 | `			SySetRelease(&sExprNode);` |
|       3 | 7604 | `			return SXERR_ABORT;` |
|       - | 7605 | `		}` |
|  337619 | 7606 | `	}` |
|  675250 | 7607 | `	SySetRelease(&sExprNode);` |
|  675250 | 7608 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  337627 | 7609 |  |
|       - | 7610 | `/*` |
|       - | 7611 | ` * Return a pointer to the node construct handler associated` |
|       - | 7612 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7613 | ` */` |
|  165438 | 7614 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7615 |  |
|  165440 | 7616 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7617 | `		/* Numeric literal: Either real or integer */` |
|   91412 | 7618 | `		return PH7_CompileNumLiteral;` |
|   74030 | 7619 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7620 | `		/* Double quoted string */` |
|   14292 | 7621 | `		return PH7_CompileString;` |
|   59740 | 7622 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7623 | `		/* Single quoted string */` |
|   59680 | 7624 | `		return PH7_CompileSimpleString;` |
|      62 | 7625 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7626 | `		/* Heredoc */` |
|      28 | 7627 | `		return PH7_CompileHereDoc;` |
|      36 | 7628 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7629 | `		/* Nowdoc */` |
|      29 | 7630 | `		return PH7_CompileNowDoc;` |
|       7 | 7631 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7632 | `		/* Backtick quoted string */` |
|       5 | 7633 | `		return PH7_CompileBacktic;` |
|       - | 7634 | `	}` |
|       3 | 7635 | `	return 0;` |
|   82721 | 7636 |  |
|       - | 7637 | `/*` |
|       - | 7638 | ` * Compile an unset() statement.` |
|       - | 7639 | ` * unset($var, $arr[$key], ...);` |
|       - | 7640 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7641 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7642 | ` * parent array before extracting the element to unset.` |
|       - | 7643 | ` */` |
|    2506 | 7644 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7645 |  |
|    2508 | 7646 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2508 | 7647 | `	sxu32 nIdx = 0;` |
|       - | 7648 | `	SyString sName;` |
|       - | 7649 | `	sxi32 rc;` |
|       - | 7650 | `	/* Jump the 'unset' keyword */` |
|    2508 | 7651 | `	pGen->pIn++;` |
|       - | 7652 | `	/* Save delimiter */` |
|    2508 | 7653 | `	pTmp = pGen->pEnd;` |
|       - | 7654 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2508 | 7655 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2508 | 7656 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7657 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7658 | `		SyToken *pClose;` |
|    2508 | 7659 | `		pGen->pIn++;   /* Skip '(' */` |
|    2508 | 7660 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2508 | 7661 | `		pEnd = pClose; /* Stop at ')' */` |
|    1253 | 7662 | `	}` |
|    2508 | 7663 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7664 | `	/* Resolve the 'unset' builtin name once */` |
|    2508 | 7665 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     344 | 7666 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     344 | 7667 | `		if( pObj == 0 ){` |
|     ! 0 | 7668 | `			return SXERR_ABORT;` |
|       - | 7669 | `		}` |
|     344 | 7670 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     344 | 7671 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     171 | 7672 | `	}` |
|       - | 7673 | `	/* Compile each comma-separated argument */` |
|    8352 | 7674 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    5846 | 7675 | `		if( pGen->pIn < pNext ){` |
|    5846 | 7676 | `			pGen->pEnd = pNext;` |
|    5846 | 7677 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7678 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    5846 | 7679 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7680 | `				return SXERR_ABORT;` |
|       - | 7681 | `			}` |
|    5846 | 7682 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7683 | `				/* Emit call for this single argument */` |
|    5844 | 7684 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5844 | 7685 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    5844 | 7686 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    2921 | 7687 | `			}` |
|    2922 | 7688 | `		}` |
|       - | 7689 | `		/* Jump trailing commas */` |
|    9184 | 7690 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3340 | 7691 | `			pNext++;` |
|       2 | 7692 | `		}` |
|    5846 | 7693 | `		pGen->pIn = pNext;` |
|       2 | 7694 | `	}` |
|       - | 7695 | `	/* Skip past the closing ')' if present */` |
|    2508 | 7696 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2508 | 7697 | `		pGen->pIn++;` |
|    1253 | 7698 | `	}` |
|       - | 7699 | `	/* Restore token stream */` |
|    2508 | 7700 | `	pGen->pEnd = pTmp;` |
|    2508 | 7701 | `	return SXRET_OK;` |
|    1255 | 7702 |  |
|       - | 7703 | `/*` |
|       - | 7704 | ` * PHP Language construct table.` |
|       - | 7705 | ` */` |
|       - | 7706 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 7707 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 7708 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 7709 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 7710 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 7711 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 7712 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 7713 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 7714 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 7715 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 7716 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 7717 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 7718 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 7719 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 7720 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 7721 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 7722 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 7723 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 7724 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 7725 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 7726 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 7727 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 7728 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 7729 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 7730 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 7731 | `};` |
|       - | 7732 | `/*` |
|       - | 7733 | ` * Return a pointer to the statement handler routine associated` |
|       - | 7734 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 7735 | ` */` |
|  404162 | 7736 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 7737 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 7738 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 7739 | `	)` |
|       2 | 7740 |  |
|  404164 | 7741 | `	sxu32 n = 0;` |
| 1654635 | 7742 | `	for(;;){` |
| 3309272 | 7743 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   42540 | 7744 | `			break;` |
|       - | 7745 | `		}` |
| 3266734 | 7746 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  361626 | 7747 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 7748 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 7749 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 7750 | `					/* 'static' (class context),return null */` |
|     ! 0 | 7751 | `					return 0;` |
|       - | 7752 | `				}` |
|     ! 0 | 7753 | `			}` |
|       - | 7754 | `			/* Return a pointer to the handler.` |
|       - | 7755 | `			*/` |
|  361626 | 7756 | `			return aLangConstruct[n].xConstruct;` |
|       - | 7757 | `		}` |
| 2905110 | 7758 | `		n++;` |
|       2 | 7759 | `	}` |
|   42540 | 7760 | `	if( pLookahed ){` |
|   42540 | 7761 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8424 | 7762 | `			return PH7_CompileClassInterface;` |
|   34118 | 7763 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   33924 | 7764 | `			return PH7_CompileClass;` |
|     196 | 7765 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      51 | 7766 | `			return PH7_CompileTrait;` |
|     144 | 7767 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 7768 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 7769 | `				return PH7_CompileAbstractClass;` |
|     128 | 7770 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 7771 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 7772 | `				return PH7_CompileFinalClass;` |
|       - | 7773 | `		}` |
|      63 | 7774 | `	}` |
|       - | 7775 | `	/* Not a language construct */` |
|     128 | 7776 | `	return 0;` |
|  202083 | 7777 |  |
|       - | 7778 | `/*` |
|       - | 7779 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 7780 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 7781 | ` */` |
|     126 | 7782 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 7783 |  |
|       - | 7784 | `	int rc;` |
|     128 | 7785 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     128 | 7786 | `	if( rc == FALSE ){` |
|      40 | 7787 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 7788 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 7789 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 7790 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 7791 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 7792 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 7793 | `			*/` |
|       - | 7794 | `			){` |
|      34 | 7795 | `				rc = TRUE;` |
|      16 | 7796 | `		}` |
|      20 | 7797 | `	}` |
|     128 | 7798 | `	return rc;` |
|       2 | 7799 |  |
|       - | 7800 | `/*` |
|       - | 7801 | ` * Compile a PHP chunk.` |
|       - | 7802 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7803 | ` * takes care of generating the appropriate error message.` |
|       - | 7804 | ` */` |
|  552006 | 7805 | `static sxi32 GenStateCompileChunk(` |
|       - | 7806 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7807 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 7808 | `	)` |
|       2 | 7809 |  |
|       - | 7810 | `	ProcLangConstruct xCons;` |
|       - | 7811 | `	sxi32 rc;` |
|  552008 | 7812 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  325816 | 7813 | `	for(;;){` |
|  651634 | 7814 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7815 | `			/* No more input to process */` |
|   11308 | 7816 | `			break;` |
|       - | 7817 | `		}` |
|  640328 | 7818 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7819 | `			/* Compile block */` |
|      12 | 7820 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 7821 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7822 | `				break;` |
|       - | 7823 | `			}` |
|       7 | 7824 | `		}else{` |
|  640318 | 7825 | `			xCons = 0;` |
|  640318 | 7826 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  404164 | 7827 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 7828 | `				/* Try to extract a language construct handler */` |
|  404164 | 7829 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  404164 | 7830 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 7831 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7832 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 7833 | `						&pGen->pIn->sData);` |
|       9 | 7834 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7835 | `						break;` |
|       - | 7836 | `					}` |
|       - | 7837 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 7838 | `					 * this erroneous statement.` |
|       - | 7839 | `					 */` |
|       9 | 7840 | `					xCons = PH7_ErrorRecover;` |
|       4 | 7841 | `				}` |
|  438237 | 7842 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   41288 | 7843 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 7844 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 7845 | `				xCons = PH7_CompileLabel;` |
|      56 | 7846 | `			}` |
|  640318 | 7847 | `			if( xCons == 0 ){` |
|       - | 7848 | `				/* Assume an expression an try to compile it */` |
|  236162 | 7849 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  236162 | 7850 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 7851 | `					/* Pop l-value */` |
|  236038 | 7852 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  118018 | 7853 | `				}` |
|  118082 | 7854 | `			}else{` |
|       - | 7855 | `				/* Go compile the sucker */` |
|  404158 | 7856 | `				rc = xCons(&(*pGen));` |
|       - | 7857 | `			}` |
|  640318 | 7858 | `			if( rc == SXERR_ABORT ){` |
|       - | 7859 | `				/* Request to abort compilation */` |
|       3 | 7860 | `				break;` |
|       - | 7861 | `			}` |
|       - | 7862 | `		}` |
|       - | 7863 | `		/* Ignore trailing semi-colons ';' */` |
| 1063562 | 7864 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  423238 | 7865 | `			pGen->pIn++;` |
|       2 | 7866 | `		}` |
|  640326 | 7867 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 7868 | `			/* Compile a single statement and return */` |
|  540700 | 7869 | `			break;` |
|       - | 7870 | `		}` |
|       - | 7871 | `		/* LOOP ONE */` |
|       - | 7872 | `		/* LOOP TWO */` |
|       - | 7873 | `		/* LOOP THREE */` |
|       - | 7874 | `		/* LOOP FOUR */` |
|       2 | 7875 | `	}` |
|       - | 7876 | `	/* Return compilation status */` |
|  552008 | 7877 | `	return rc;` |
|       2 | 7878 |  |
|       - | 7879 | `/*` |
|       - | 7880 | ` * Compile a Raw PHP chunk.` |
|       - | 7881 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 7882 | ` * takes care of generating the appropriate error message.` |
|       - | 7883 | ` */` |
|   11310 | 7884 | `static sxi32 PH7_CompilePHP(` |
|       - | 7885 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7886 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 7887 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 7888 | `	)` |
|       2 | 7889 |  |
|   11312 | 7890 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 7891 | `	sxi32 rc;` |
|       - | 7892 | `	/* Reset the token set */` |
|   11312 | 7893 | `	SySetReset(&(*pTokenSet));` |
|       - | 7894 | `	/* Mark as the default token set */` |
|   11312 | 7895 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 7896 | `	/* Advance the stream cursor */` |
|   11312 | 7897 | `	pGen->pRawIn++;` |
|       - | 7898 | `	/* Tokenize the PHP chunk first */` |
|   11312 | 7899 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 7900 | `	/* Point to the head and tail of the token stream. */` |
|   11312 | 7901 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11312 | 7902 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11312 | 7903 | `	if( is_expr ){` |
|     ! 0 | 7904 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 7905 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 7906 | `			/* A simple expression,compile it */` |
|     ! 0 | 7907 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 7908 | `		}` |
|       - | 7909 | `		/* Emit the DONE instruction */` |
|     ! 0 | 7910 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 7911 | `		return SXRET_OK;` |
|       - | 7912 | `	}` |
|   11312 | 7913 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 7914 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 7915 | `		/*` |
|       - | 7916 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 7917 | `		 * According to the PHP reference manual:` |
|       - | 7918 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 7919 | `		 *  immediately follow` |
|       - | 7920 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 7921 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 7922 | `		 * Symisc extension:` |
|       - | 7923 | `		 *   This short syntax works with all PHP opening` |
|       - | 7924 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 7925 | `		 *   only short tag.` |
|       - | 7926 | `		 */` |
|       - | 7927 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 7928 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 7929 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 7930 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 7931 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 7932 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 7933 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 7934 | `		}` |
|       3 | 7935 | `		return SXRET_OK;` |
|       - | 7936 | `	}` |
|       - | 7937 | `	/* Compile the PHP chunk */` |
|   11310 | 7938 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 7939 | `	/* Fix exceptions jumps */` |
|   11310 | 7940 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7941 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11310 | 7942 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 7943 | `		rc = SXERR_ABORT;` |
|       1 | 7944 | `	}` |
|       - | 7945 | `	/* Reset container */` |
|   11310 | 7946 | `	SySetReset(&pGen->aGoto);` |
|   11310 | 7947 | `	SySetReset(&pGen->aLabel);` |
|       - | 7948 | `	/* Compilation result */` |
|   11310 | 7949 | `	return rc;` |
|    5657 | 7950 |  |
|       - | 7951 | `/*` |
|       - | 7952 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 7953 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 7954 | ` * This is the only compile interface exported from this file.` |
|       - | 7955 | ` */` |
|   13242 | 7956 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 7957 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 7958 | `	SyString *pScript,  /* Script to compile */` |
|       - | 7959 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 7960 | `	)` |
|       2 | 7961 |  |
|       - | 7962 | `	SySet aPhpToken,aRawToken;` |
|       - | 7963 | `	ph7_gen_state *pCodeGen;` |
|       - | 7964 | `	ph7_value *pRawObj;` |
|       - | 7965 | `	sxu32 nObjIdx;` |
|       - | 7966 | `	sxi32 nRawObj;` |
|       - | 7967 | `	int is_expr;` |
|       - | 7968 | `	sxi32 rc;` |
|   13244 | 7969 | `	if( pScript->nByte < 1 ){` |
|       - | 7970 | `		/* Nothing to compile */` |
|     ! 0 | 7971 | `		return PH7_OK;` |
|       - | 7972 | `	}` |
|       - | 7973 | `	/* Initialize the tokens containers */` |
|   13244 | 7974 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13244 | 7975 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13244 | 7976 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13244 | 7977 | `	is_expr = 0;` |
|   13244 | 7978 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 7979 | `		SyToken sTmp;` |
|       - | 7980 | `		/* PHP only: -*/` |
|    2820 | 7981 | `		sTmp.nLine = 1;` |
|    2820 | 7982 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2820 | 7983 | `		sTmp.pUserData = 0;` |
|    2820 | 7984 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2820 | 7985 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2820 | 7986 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 7987 | `			/* A simple PHP expression */` |
|     ! 0 | 7988 | `			is_expr = 1;` |
|     ! 0 | 7989 | `		}` |
|    1411 | 7990 | `	}else{` |
|       - | 7991 | `		/* Tokenize raw text */` |
|   10426 | 7992 | `		SySetAlloc(&aRawToken,32);` |
|   10426 | 7993 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 7994 | `	}` |
|   13244 | 7995 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 7996 | `	/* Process high-level tokens */` |
|   13244 | 7997 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13244 | 7998 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13244 | 7999 | `	rc = PH7_OK;` |
|   13244 | 8000 | `	if( is_expr ){` |
|       - | 8001 | `		/* Compile the expression */` |
|     ! 0 | 8002 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8003 | `		goto cleanup;` |
|       - | 8004 | `	}` |
|   13244 | 8005 | `	nObjIdx = 0;` |
|       - | 8006 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8007 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8008 | `	 * preventing namespace bleeding across include()d files. */` |
|   13244 | 8009 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8010 | `	/* Start the compilation process */` |
|   11837 | 8011 | `	for(;;){` |
|   34982 | 8012 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13240 | 8013 | `			break; /* No more tokens to process */` |
|       - | 8014 | `		}` |
|   21744 | 8015 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8016 | `			/* Compile the PHP chunk */` |
|   11312 | 8017 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11312 | 8018 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8019 | `				break;` |
|       - | 8020 | `			}` |
|   11308 | 8021 | `			continue;` |
|       - | 8022 | `		}` |
|       - | 8023 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10434 | 8024 | `		nRawObj = 0;` |
|   20866 | 8025 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8026 | `			/* Consume the raw chunk without any processing */` |
|   10434 | 8027 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10434 | 8028 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8029 | `				rc = SXERR_MEM;` |
|     ! 0 | 8030 | `				break;` |
|       - | 8031 | `			}` |
|       - | 8032 | `			/* Mark as constant and emit the load constant instruction */` |
|   10434 | 8033 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10434 | 8034 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10434 | 8035 | `			++nRawObj;` |
|   10434 | 8036 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8037 | `		}` |
|   10434 | 8038 | `		if( nRawObj > 0 ){` |
|       - | 8039 | `			/* Emit the consume instruction */` |
|   10434 | 8040 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5216 | 8041 | `		}` |
|    6623 | 8042 | `	}` |
|    6621 | 8043 | `cleanup:` |
|   13244 | 8044 | `	SySetRelease(&aRawToken);` |
|   13244 | 8045 | `	SySetRelease(&aPhpToken);` |
|   13244 | 8046 | `	return rc;` |
|    6623 | 8047 |  |
|       - | 8048 | `/*` |
|       - | 8049 | ` * Utility routines.Initialize the code generator.` |
|       - | 8050 | ` */` |
|    2796 | 8051 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8052 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8053 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8054 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8055 | `	)` |
|       2 | 8056 |  |
|    2798 | 8057 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8058 | `	/* Zero the structure */` |
|    2798 | 8059 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8060 | `	/* Initial state */` |
|    2798 | 8061 | `	pGen->pVm  = &(*pVm);` |
|    2798 | 8062 | `	pGen->xErr = xErr;` |
|    2798 | 8063 | `	pGen->pErrData = pErrData;` |
|    2798 | 8064 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2798 | 8065 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2798 | 8066 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2798 | 8067 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8068 | `	/* Error log buffer */` |
|    2798 | 8069 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8070 | `	/* General purpose working buffer */` |
|    2798 | 8071 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8072 | `	/* Namespace state */` |
|    2798 | 8073 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2798 | 8074 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 8075 | `	/* Create the global scope */` |
|    2798 | 8076 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8077 | `	/* Point to the global scope */` |
|    2798 | 8078 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2798 | 8079 | `	return SXRET_OK;` |
|       2 | 8080 |  |
|       - | 8081 | `/*` |
|       - | 8082 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8083 | ` */` |
|   15778 | 8084 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8085 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8086 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8087 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8088 | `	)` |
|       2 | 8089 |  |
|   15780 | 8090 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8091 | `	GenBlock *pBlock,*pParent;` |
|       - | 8092 | `	/* Reset state */` |
|   15780 | 8093 | `	SySetReset(&pGen->aLabel);` |
|   15780 | 8094 | `	SySetReset(&pGen->aGoto);` |
|   15780 | 8095 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15780 | 8096 | `	SyBlobRelease(&pGen->sWorker);` |
|   15780 | 8097 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15780 | 8098 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15780 | 8099 | `	SyHashRelease(&pGen->hUseImports);` |
|   15780 | 8100 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|       - | 8101 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8102 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8103 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8104 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8105 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8106 | `	 * number of unique names, which is acceptable. */` |
|       - | 8107 | `	/* Point to the global scope */` |
|   15780 | 8108 | `	pBlock = pGen->pCurrent;` |
|   15780 | 8109 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8110 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8111 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8112 | `		pBlock = pParent;` |
|     ! 0 | 8113 | `	}` |
|   15780 | 8114 | `	pGen->xErr = xErr;` |
|   15780 | 8115 | `	pGen->pErrData = pErrData;` |
|   15780 | 8116 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15780 | 8117 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15780 | 8118 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15780 | 8119 | `	pGen->nErr = 0;` |
|   15780 | 8120 | `	return SXRET_OK;` |
|       2 | 8121 |  |
|       - | 8122 | `/*` |
|       - | 8123 | ` * Generate a compile-time error message.` |
|       - | 8124 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8125 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8126 | ` * abort compilation immediately.` |
|       - | 8127 | ` */` |
|     452 | 8128 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8129 |  |
|     454 | 8130 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     454 | 8131 | `	const char *zErr = "Error";` |
|       - | 8132 | `	SyString *pFile;` |
|       - | 8133 | `	va_list ap;` |
|       - | 8134 | `	sxi32 rc;` |
|       - | 8135 | `	/* Reset the working buffer */` |
|     454 | 8136 | `	SyBlobReset(pWorker);` |
|       - | 8137 | `	/* Peek the processed file path if available */` |
|     454 | 8138 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     454 | 8139 | `	if( nErrType == E_ERROR ){` |
|       - | 8140 | `		/* Increment the error counter */` |
|     412 | 8141 | `		pGen->nErr++;` |
|     412 | 8142 | `		if( pGen->nErr > 15 ){` |
|       - | 8143 | `			/* Error count limit reached */` |
|       5 | 8144 | `			if( pGen->xErr ){` |
|       5 | 8145 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8146 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8147 | `				if( pFile ){` |
|       5 | 8148 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8149 | `				}` |
|       5 | 8150 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8151 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8152 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8153 | `				}` |
|       2 | 8154 | `			}` |
|       - | 8155 | `			/* Abort immediately */` |
|       5 | 8156 | `			return SXERR_ABORT;` |
|       - | 8157 | `		}` |
|     203 | 8158 | `	}` |
|     450 | 8159 | `	if( pGen->xErr == 0 ){` |
|       - | 8160 | `		/* No available error consumer,return immediately */` |
|       3 | 8161 | `		return SXRET_OK;` |
|       - | 8162 | `	}` |
|     447 | 8163 | `	switch(nErrType){` |
|     405 | 8164 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8165 | `	case E_WARNING: zErr = "Warning";     break;` |
|       7 | 8166 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8167 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8168 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8169 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8170 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8171 | `	default:` |
|     ! 0 | 8172 | `		break;` |
|       - | 8173 | `	}` |
|     447 | 8174 | `	rc = SXRET_OK;` |
|       - | 8175 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     447 | 8176 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     447 | 8177 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     447 | 8178 | `	va_start(ap,zFormat);` |
|     447 | 8179 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     447 | 8180 | `	va_end(ap);` |
|     447 | 8181 | `	if( pFile ){` |
|     447 | 8182 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     223 | 8183 | `	}` |
|       - | 8184 | `	/* Append a new line */` |
|     447 | 8185 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     447 | 8186 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8187 | `		/* Consume the generated error message */` |
|     447 | 8188 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     223 | 8189 | `	}` |
|     447 | 8190 | `	return rc;` |
|     228 | 8191 |  |
|       - | 8192 |  |
