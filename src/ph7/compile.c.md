# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3890/5009 lines (77.66%)

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
|    2922 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2924 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8201 |  131 | `	for(;;){` |
|   16404 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2816 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2816 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2790 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      13 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13616 |  140 | `		pBlock = pBlock->pParent;` |
|   13616 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1463 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  567096 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  567098 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  567098 |  162 | `	pBlock->pUserData   = pUserData;` |
|  567098 |  163 | `	pBlock->pGen        = pGen;` |
|  567098 |  164 | `	pBlock->iFlags      = iType;` |
|  567098 |  165 | `	pBlock->pParent     = 0;` |
|  567098 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  567098 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  567098 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  564440 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  564442 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  564442 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  564442 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  564442 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  564442 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  564442 |  200 | `	pGen->pCurrent = pBlock;` |
|  564442 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  273142 |  203 | `		*ppBlock = pBlock;` |
|  136570 |  204 | `	}` |
|  564442 |  205 | `	return SXRET_OK;` |
|  282222 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  564432 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  564434 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  564434 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  564434 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  564432 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  564434 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  564434 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  564434 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  564434 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  564432 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  564434 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  564434 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  564434 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  564434 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  564434 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  564434 |  244 | `	return SXRET_OK;` |
|  282218 |  245 |  |
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
|  172096 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  172098 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  172098 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  172098 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  172098 |  265 | `	return rc;` |
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
|  401900 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  401902 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  737240 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  335340 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  130590 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  204752 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32658 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  172096 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  172096 |  298 | `		if( pInstr ){` |
|  172096 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  172096 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  172096 |  302 | `			aFix[n].nJumpType = -1;` |
|   86047 |  303 | `		}` |
|   86049 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  401902 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  153398 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  153400 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  153546 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  153398 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  153530 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  153398 |  358 | `	return SXRET_OK;` |
|   76701 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  499450 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  499452 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  499452 |  367 | `	if( pEntry == 0 ){` |
|  246292 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  253162 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  253162 |  371 | `	return SXRET_OK;` |
|  249727 |  372 |  |
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
|  246290 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  246292 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  246292 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  123145 |  387 | `	}` |
|  246292 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   87618 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   87620 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   87620 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   87620 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   87620 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   87620 |  408 | `	return pObj;` |
|   43811 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Implementation of the PHP language constructs.` |
|       - |  412 | ` */` |
|       - |  413 | `/* Forward declaration */` |
|       - |  414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  415 | `/*` |
|       - |  416 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |  417 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |  418 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |  419 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |  420 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |  421 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |  422 | ` * inputs like a thousand-digit number.` |
|       - |  423 | ` */` |
|       - |  424 | `#define GEN_NUM_SCRATCH 128` |
|       - |  425 | `/*` |
|       - |  426 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |  427 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |  428 | ` *   base  2 => 0 or 1` |
|       - |  429 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |  430 | ` *              decimal scan in the lexer)` |
|       - |  431 | ` */` |
|    1076 |  432 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |  433 |  |
|    1078 |  434 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |  435 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |  436 | `	return SyisDigit(c);` |
|     540 |  437 |  |
|       - |  438 | `/*` |
|       - |  439 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |  440 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |  441 | ` * the exact wording PHP uses:` |
|       - |  442 | ` *` |
|       - |  443 | ` *   syntax error, unexpected identifier "X"` |
|       - |  444 | ` *` |
|       - |  445 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |  446 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |  447 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |  448 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |  449 | ` * no forward rescan needed.` |
|       - |  450 | ` *` |
|       - |  451 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |  452 | ` * returns 0 when it is well-formed.` |
|       - |  453 | ` */` |
|   88130 |  454 | `static int GenStateFindBadNumericSeparator(` |
|       - |  455 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |  456 |  |
|   88132 |  457 | `	const char *z = pRaw->zString;` |
|   88132 |  458 | `	sxu32 n = pRaw->nByte;` |
|   88132 |  459 | `	int base = 10;` |
|       - |  460 | `	sxu32 i, start;` |
|   88132 |  461 | `	if( n < 2 ) return 0;` |
|    7962 |  462 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |  463 | `		base = 16;` |
|    7927 |  464 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |  465 | `		base = 2;` |
|     139 |  466 | `	}` |
|   29800 |  467 | `	for( i = 0; i < n; ++i ){` |
|   21854 |  468 | `		if( z[i] != '_' ) continue;` |
|     814 |  469 | `		if( i > 0 && i + 1 < n` |
|     543 |  470 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |  471 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |  472 | `			continue; /* well-placed separator */` |
|       - |  473 | `		}` |
|       - |  474 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |  475 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |  476 | `		start = i;` |
|      20 |  477 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |  478 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |  479 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |  480 | `		}` |
|      15 |  481 | `		*pBadStart = &z[start];` |
|      15 |  482 | `		*pBadLen = n - start;` |
|      15 |  483 | `		return 1;` |
|     ! 0 |  484 | `	}` |
|    7948 |  485 | `	return 0;` |
|   44067 |  486 |  |
|       - |  487 | `/*` |
|       - |  488 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |  489 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |  490 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |  491 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |  492 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |  493 | ` * so callers can bail from the current construct).` |
|       - |  494 | ` */` |
|   88130 |  495 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |  496 |  |
|   88132 |  497 | `	const char *zBad = 0;` |
|   88132 |  498 | `	sxu32 nBad = 0;` |
|       - |  499 | `	SyString sBad;` |
|       - |  500 | `	sxi32 rc;` |
|   88132 |  501 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   88118 |  502 | `		return SXRET_OK;` |
|       - |  503 | `	}` |
|      15 |  504 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |  505 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  506 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |  507 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  508 | `		return SXERR_ABORT;` |
|       - |  509 | `	}` |
|      15 |  510 | `	return SXERR_SYNTAX;` |
|   44067 |  511 |  |
|       - |  512 | `/*` |
|       - |  513 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |  514 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |  515 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |  516 | ` *` |
|       - |  517 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |  518 | ` * and *pzAlloc is set to NULL.` |
|       - |  519 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |  520 | ` * and *pzAlloc is set to NULL.` |
|       - |  521 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |  522 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |  523 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |  524 | ` *` |
|       - |  525 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |  526 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |  527 | ` */` |
|   88116 |  528 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |  529 | `	SyMemBackend *pAlloc,` |
|       - |  530 | `	const SyString *pToken,` |
|       - |  531 | `	char *zScratch, sxu32 nScratch,` |
|       - |  532 | `	SyString *pOut, char **pzAlloc)` |
|       2 |  533 |  |
|       - |  534 | `	sxu32 i, j;` |
|   88118 |  535 | `	int hasUnderscore = 0;` |
|       - |  536 | `	char *zBuf;` |
|   88118 |  537 | `	*pzAlloc = 0;` |
|  188060 |  538 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  100196 |  539 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   49973 |  540 | `	}` |
|   88118 |  541 | `	if( !hasUnderscore ){` |
|   87866 |  542 | `		SyStringDupPtr(pOut, pToken);` |
|   87866 |  543 | `		return SXRET_OK;` |
|       - |  544 | `	}` |
|     253 |  545 | `	if( pToken->nByte <= nScratch ){` |
|     251 |  546 | `		zBuf = zScratch;` |
|     126 |  547 | `	}else{` |
|       3 |  548 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |  549 | `		if( zBuf == 0 ){` |
|     ! 0 |  550 | `			return SXERR_ABORT;` |
|       - |  551 | `		}` |
|       3 |  552 | `		*pzAlloc = zBuf;` |
|       - |  553 | `	}` |
|     253 |  554 | `	j = 0;` |
|    2895 |  555 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |  556 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |  557 | `	}` |
|     253 |  558 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |  559 | `	return SXRET_OK;` |
|   44060 |  560 |  |
|       - |  561 | `/*` |
|       - |  562 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  563 | ` * Notes on the integer type.` |
|       - |  564 | ` *  According to the PHP language reference manual` |
|       - |  565 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  566 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  567 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  568 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  569 | ` * Symisc eXtension to the integer type.` |
|       - |  570 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  571 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  572 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  573 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  574 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  575 | ` *  documentation.` |
|       - |  576 | ` */` |
|   88102 |  577 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  578 |  |
|   88104 |  579 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   88104 |  580 | `	sxu32 nIdx = 0;` |
|       - |  581 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   88104 |  582 | `	char *zAlloc = 0;` |
|       - |  583 | `	SyString sNum;` |
|       - |  584 | `	sxi32 rc;` |
|   44051 |  585 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   88104 |  586 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   88104 |  587 | `	if( rc != SXRET_OK ){` |
|      11 |  588 | `		return rc;` |
|       - |  589 | `	}` |
|  132140 |  590 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   44046 |  591 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   88094 |  592 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  593 | `		return SXERR_ABORT;` |
|       - |  594 | `	}` |
|   88094 |  595 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  596 | `		ph7_value *pObj;` |
|       - |  597 | `		sxi64 iValue;` |
|   87620 |  598 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   87620 |  599 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   87620 |  600 | `		if( pObj == 0 ){` |
|     ! 0 |  601 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  602 | `			return SXERR_ABORT;` |
|       - |  603 | `		}` |
|   87620 |  604 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   43811 |  605 | `	}else{` |
|       - |  606 | `		/* Real number */` |
|       - |  607 | `		ph7_value *pObj;` |
|       - |  608 | `		/* Reserve a new constant */` |
|     476 |  609 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     476 |  610 | `		if( pObj == 0 ){` |
|     ! 0 |  611 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  612 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  613 | `			return SXERR_ABORT;` |
|       - |  614 | `		}` |
|     476 |  615 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     476 |  616 | `		PH7_MemObjToReal(pObj);` |
|       - |  617 | `	}` |
|   88094 |  618 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  619 | `	/* Emit the load constant instruction */` |
|   88094 |  620 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  621 | `	/* Node successfully compiled */` |
|   88094 |  622 | `	return SXRET_OK;` |
|   44053 |  623 |  |
|       - |  624 | `/*` |
|       - |  625 | ` * Compile a single quoted string.` |
|       - |  626 | ` * According to the PHP language reference manual:` |
|       - |  627 | ` *` |
|       - |  628 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  629 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  630 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  631 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  632 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  633 | ` *` |
|       - |  634 | ` */` |
|   57264 |  635 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  636 |  |
|   57266 |  637 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  638 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  639 | `	ph7_value *pObj;` |
|       - |  640 | `	sxu32 nIdx;` |
|   57266 |  641 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  642 | `	/* Delimit the string */` |
|   57266 |  643 | `	zIn  = pStr->zString;` |
|   57266 |  644 | `	zEnd = &zIn[pStr->nByte];` |
|   57266 |  645 | `	if( zIn >= zEnd ){` |
|       - |  646 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  647 | `		 * rather than reserving a new object each time. */` |
|     138 |  648 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  649 | `		return SXRET_OK;` |
|       - |  650 | `	}` |
|   57130 |  651 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  652 | `		/* Already processed,emit the load constant instruction` |
|       - |  653 | `		 * and return.` |
|       - |  654 | `		 */` |
|   16892 |  655 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16892 |  656 | `		return SXRET_OK;` |
|       - |  657 | `	}` |
|       - |  658 | `	/* Reserve a new constant */` |
|   40240 |  659 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   40240 |  660 | `	if( pObj == 0 ){` |
|     ! 0 |  661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  662 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  663 | `		return SXERR_ABORT;` |
|       - |  664 | `	}` |
|   40240 |  665 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  666 | `	/* Compile the node */` |
|   40280 |  667 | `	for(;;){` |
|   80562 |  668 | `		if( zIn >= zEnd ){` |
|       - |  669 | `			/* End of input */` |
|   40240 |  670 | `			break;` |
|       - |  671 | `		}` |
|   40324 |  672 | `		zCur = zIn;` |
|  640354 |  673 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  600032 |  674 | `			zIn++;` |
|       2 |  675 | `		}` |
|   40324 |  676 | `		if( zIn > zCur ){` |
|       - |  677 | `			/* Append raw contents*/` |
|   40304 |  678 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20151 |  679 | `		}` |
|   40324 |  680 | `		zIn++;` |
|   40324 |  681 | `		if( zIn < zEnd ){` |
|     105 |  682 | `			if( zIn[0] == '\\' ){` |
|       - |  683 | `				/* A literal backslash */` |
|      23 |  684 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |  685 | `			}else if( zIn[0] == '\'' ){` |
|       - |  686 | `				/* A single quote */` |
|      11 |  687 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  688 | `			}else{` |
|       - |  689 | `				/* verbatim copy */` |
|      73 |  690 | `				zIn--;` |
|      73 |  691 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |  692 | `				zIn++;` |
|       - |  693 | `			}` |
|      52 |  694 | `		}` |
|       - |  695 | `		/* Advance the stream cursor */` |
|   40324 |  696 | `		zIn++;` |
|       2 |  697 | `	}` |
|       - |  698 | `	/* Emit the load constant instruction */` |
|   40240 |  699 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   40240 |  700 | `	if( pStr->nByte < 1024 ){` |
|       - |  701 | `		/* Install in the literal table */` |
|   40240 |  702 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20119 |  703 | `	}` |
|       - |  704 | `	/* Node successfully compiled */` |
|   40240 |  705 | `	return SXRET_OK;` |
|   28634 |  706 |  |
|       - |  707 | `/*` |
|       - |  708 | ` * Compile a nowdoc string.` |
|       - |  709 | ` * According to the PHP language reference manual:` |
|       - |  710 | ` *` |
|       - |  711 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  712 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  713 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  714 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  715 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  716 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  717 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  718 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  719 | ` *  of the closing identifier.` |
|       - |  720 | ` */` |
|      28 |  721 | `static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  722 |  |
|      29 |  723 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  724 | `	ph7_value *pObj;` |
|       - |  725 | `	sxu32 nIdx;` |
|      29 |  726 | `	nIdx = 0; /* Prevent compiler warning */` |
|      29 |  727 | `	if( pStr->nByte <= 0 ){` |
|       - |  728 | `		/* Empty string,load NULL */` |
|     ! 0 |  729 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 |  730 | `		return SXRET_OK;` |
|       - |  731 | `	}` |
|       - |  732 | `	/* Reserve a new constant */` |
|      29 |  733 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  734 | `	if( pObj == 0 ){` |
|     ! 0 |  735 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  736 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  737 | `		return SXERR_ABORT;` |
|       - |  738 | `	}` |
|       - |  739 | `	/* No processing is done here, simply a memcpy() operation */` |
|      29 |  740 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  741 | `	/* Emit the load constant instruction */` |
|      29 |  742 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  743 | `	/* Node successfully compiled */` |
|      29 |  744 | `	return SXRET_OK;` |
|      15 |  745 |  |
|       - |  746 | `/*` |
|       - |  747 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  748 | ` * According to the PHP language reference manual` |
|       - |  749 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  750 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  751 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  752 | ` *  property in a string with a minimum of effort.` |
|       - |  753 | ` *  Simple syntax` |
|       - |  754 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  755 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  756 | ` *   the end of the name.` |
|       - |  757 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  758 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  759 | ` *   as to simple variables.` |
|       - |  760 | ` *  Complex (curly) syntax` |
|       - |  761 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  762 | ` *   of complex expressions.` |
|       - |  763 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  764 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  765 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  766 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  767 | ` */` |
|    1714 |  768 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  769 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  770 | `	sxu32 nLine,         /* Line number */` |
|       - |  771 | `	const char *zIn,     /* Raw expression */` |
|       - |  772 | `	const char *zEnd     /* End of the expression */` |
|       - |  773 | `	)` |
|       2 |  774 |  |
|       - |  775 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  776 | `	SySet sToken;` |
|       - |  777 | `	sxi32 rc;` |
|       - |  778 | `	/* Initialize the token set */` |
|    1716 |  779 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  780 | `	/* Preallocate some slots */` |
|    1716 |  781 | `	SySetAlloc(&sToken,0x08);` |
|       - |  782 | `	/* Tokenize the text */` |
|    1716 |  783 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  784 | `	/* Swap delimiter */` |
|    1716 |  785 | `	pTmpIn  = pGen->pIn;` |
|    1716 |  786 | `	pTmpEnd = pGen->pEnd;` |
|    1716 |  787 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1716 |  788 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  789 | `	/* Compile the expression */` |
|    1716 |  790 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  791 | `	/* Restore token stream */` |
|    1716 |  792 | `	pGen->pIn  = pTmpIn;` |
|    1716 |  793 | `	pGen->pEnd = pTmpEnd;` |
|       - |  794 | `	/* Release the token set */` |
|    1716 |  795 | `	SySetRelease(&sToken);` |
|       - |  796 | `	/* Compilation result */` |
|    1716 |  797 | `	return rc;` |
|       2 |  798 |  |
|       - |  799 | `/*` |
|       - |  800 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  801 | ` */` |
|   16840 |  802 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  803 |  |
|       - |  804 | `	ph7_value *pConstObj;` |
|   16842 |  805 | `	sxu32 nIdx = 0;` |
|       - |  806 | `	/* Reserve a new constant */` |
|   16842 |  807 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16842 |  808 | `	if( pConstObj == 0 ){` |
|     ! 0 |  809 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  810 | `		return 0;` |
|       - |  811 | `	}` |
|   16842 |  812 | `	(*pCount)++;` |
|   16842 |  813 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  814 | `	/* Emit the load constant instruction */` |
|   16842 |  815 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16842 |  816 | `	return pConstObj;` |
|    8422 |  817 |  |
|       - |  818 | `/*` |
|       - |  819 | ` * Compile a double quoted/heredoc string.` |
|       - |  820 | ` * According to the PHP language reference manual` |
|       - |  821 | ` * Heredoc` |
|       - |  822 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  823 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  824 | ` *  to close the quotation.` |
|       - |  825 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  826 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  827 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  828 | ` *  Warning` |
|       - |  829 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  830 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  831 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  832 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  833 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  834 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  835 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  836 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  837 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  838 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  839 | ` * Double quoted` |
|       - |  840 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  841 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  842 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  843 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  844 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  845 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  846 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  847 | ` *  \\ backslash` |
|       - |  848 | ` *  \$ dollar sign` |
|       - |  849 | ` *  \" double-quote` |
|       - |  850 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  851 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  852 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  853 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  854 | ` * See string parsing for details.` |
|       - |  855 | ` */` |
|   15634 |  856 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  857 |  |
|   15636 |  858 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  859 | `	const char *zIn,*zCur,*zEnd;` |
|   15636 |  860 | `	ph7_value *pObj = 0;` |
|       - |  861 | `	sxi32 iCons;` |
|       - |  862 | `	sxi32 rc;` |
|       - |  863 | `	/* Delimit the string */` |
|   15636 |  864 | `	zIn  = pStr->zString;` |
|   15636 |  865 | `	zEnd = &zIn[pStr->nByte];` |
|   15636 |  866 | `	if( zIn >= zEnd ){` |
|       - |  867 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  868 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  869 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  870 | `		 */` |
|     226 |  871 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  872 | `		return SXRET_OK;` |
|       - |  873 | `	}` |
|   15412 |  874 | `	zCur = 0;` |
|       - |  875 | `	/* Compile the node */` |
|   15412 |  876 | `	iCons = 0;` |
|    8562 |  877 | `	for(;;){` |
|   25970 |  878 | `		zCur = zIn;` |
|  138906 |  879 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  114652 |  880 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  881 | `				break;` |
|  114568 |  882 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1632 |  883 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     816 |  884 | `					break;` |
|       - |  885 | `			}` |
|  112938 |  886 | `			zIn++;` |
|       2 |  887 | `		}` |
|   25970 |  888 | `		if( zIn > zCur ){` |
|   11900 |  889 | `			if( pObj == 0 ){` |
|   11624 |  890 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11624 |  891 | `				if( pObj == 0 ){` |
|     ! 0 |  892 | `					return SXERR_ABORT;` |
|       - |  893 | `				}` |
|    5811 |  894 | `			}` |
|   11900 |  895 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5949 |  896 | `		}` |
|   25970 |  897 | `		if( zIn >= zEnd ){` |
|   15412 |  898 | `			break;` |
|       - |  899 | `		}` |
|   10560 |  900 | `		if( zIn[0] == '\\' ){` |
|    8846 |  901 | `			const char *zPtr = 0;` |
|       - |  902 | `			sxu32 n;` |
|    8846 |  903 | `			zIn++;` |
|    8846 |  904 | `			if( zIn >= zEnd ){` |
|     ! 0 |  905 | `				break;` |
|       - |  906 | `			}` |
|    8846 |  907 | `			if( pObj == 0 ){` |
|    5220 |  908 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5220 |  909 | `				if( pObj == 0 ){` |
|     ! 0 |  910 | `					return SXERR_ABORT;` |
|       - |  911 | `				}` |
|    2609 |  912 | `			}` |
|    8846 |  913 | `			n = sizeof(char); /* size of conversion */` |
|    8846 |  914 | `			switch( zIn[0] ){` |
|       3 |  915 | `			case '$':` |
|       - |  916 | `				/* Dollar sign */` |
|       7 |  917 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  918 | `				break;` |
|      38 |  919 | `			case '\\':` |
|       - |  920 | `				/* A literal backslash */` |
|      78 |  921 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  922 | `				break;` |
|       2 |  923 | `			case 'a':` |
|       - |  924 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  925 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  926 | `				break;` |
|       2 |  927 | `			case 'b':` |
|       - |  928 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  929 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  930 | `				break;` |
|       4 |  931 | `			case 'f':` |
|       - |  932 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  933 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  934 | `				break;` |
|    4057 |  935 | `			case 'n':` |
|       - |  936 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8116 |  937 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8116 |  938 | `				break;` |
|      19 |  939 | `			case 'r':` |
|       - |  940 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  941 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  942 | `				break;` |
|      24 |  943 | `			case 't':` |
|       - |  944 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  945 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  946 | `				break;` |
|       3 |  947 | `			case 'v':` |
|       - |  948 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  949 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  950 | `				break;` |
|       1 |  951 | `			case '\'':` |
|       - |  952 | `				/* Single quote */` |
|       3 |  953 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  954 | `				break;` |
|      50 |  955 | `			case '"':` |
|       - |  956 | `				/* Double quote */` |
|     102 |  957 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  958 | `				break;` |
|       5 |  959 | `			case '0':` |
|       - |  960 | `				/* NUL byte */` |
|      11 |  961 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  962 | `				break;` |
|     188 |  963 | `			case 'x':` |
|     377 |  964 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  965 | `					int c;` |
|       - |  966 | `					/* Hex digit */` |
|     363 |  967 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  968 | `					if( &zIn[2] < zEnd ){` |
|     363 |  969 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  970 | `					}` |
|       - |  971 | `					/* Output char */` |
|     363 |  972 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  973 | `					n += sizeof(char) * 2;` |
|     182 |  974 | `				}else{` |
|       - |  975 | `					/* Output literal character  */` |
|      15 |  976 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  977 | `				}` |
|     377 |  978 | `				break;` |
|      15 |  979 | `			case 'o':` |
|      31 |  980 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  981 | `					/* Octal digit stream */` |
|       - |  982 | `					int c;` |
|      21 |  983 | `					c = 0;` |
|      21 |  984 | `					zIn++;` |
|      61 |  985 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  986 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  987 | `							break;` |
|       - |  988 | `						}` |
|      41 |  989 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  990 | `					}` |
|      21 |  991 | `					if ( c > 0 ){` |
|      15 |  992 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  993 | `					}` |
|      21 |  994 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  995 | `				}else{` |
|       - |  996 | `					/* Output literal character  */` |
|      11 |  997 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  998 | `				}` |
|      31 |  999 | `				break;` |
|      11 | 1000 | `			default:` |
|       - | 1001 | `				/* Output without a slash */` |
|      23 | 1002 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 | 1003 | `				break;` |
|       - | 1004 | `			}` |
|       - | 1005 | `			/* Advance the stream cursor */` |
|    8846 | 1006 | `			zIn += n;` |
|    8846 | 1007 | `			continue;` |
|       - | 1008 | `		}` |
|    1716 | 1009 | `		if( zIn[0] == '{' ){` |
|       - | 1010 | `			/* Curly syntax */` |
|       - | 1011 | `			const char *zExpr;` |
|      87 | 1012 | `			sxi32 iNest = 1;` |
|      87 | 1013 | `			zIn++;` |
|      87 | 1014 | `			zExpr = zIn;` |
|       - | 1015 | `			/* Synchronize with the next closing curly braces */` |
|     985 | 1016 | `			while( zIn < zEnd ){` |
|     985 | 1017 | `				if( zIn[0] == '{' ){` |
|       - | 1018 | `					/* Increment nesting level */` |
|       9 | 1019 | `					iNest++;` |
|     981 | 1020 | `				}else if(zIn[0] == '}' ){` |
|       - | 1021 | `					/* Decrement nesting level */` |
|      95 | 1022 | `					iNest--;` |
|      95 | 1023 | `					if( iNest <= 0 ){` |
|      87 | 1024 | `						break;` |
|       - | 1025 | `					}` |
|       4 | 1026 | `				}` |
|     899 | 1027 | `				zIn++;` |
|       1 | 1028 | `			}` |
|       - | 1029 | `			/* Process the expression */` |
|      87 | 1030 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      87 | 1031 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1032 | `				return SXERR_ABORT;` |
|       - | 1033 | `			}` |
|      87 | 1034 | `			if( rc != SXERR_EMPTY ){` |
|      87 | 1035 | `				++iCons;` |
|      43 | 1036 | `			}` |
|      87 | 1037 | `			if( zIn < zEnd ){` |
|       - | 1038 | `				/* Jump the trailing curly */` |
|      87 | 1039 | `				zIn++;` |
|      43 | 1040 | `			}` |
|      44 | 1041 | `		}else{` |
|       - | 1042 | `			/* Simple syntax */` |
|    1630 | 1043 | `			const char *zExpr = zIn;` |
|       - | 1044 | `			/* Assemble variable name */` |
|     814 | 1045 | `			for(;;){` |
|       - | 1046 | `				/* Jump leading dollars */` |
|    3258 | 1047 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1630 | 1048 | `					zIn++;` |
|       2 | 1049 | `				}` |
|     814 | 1050 | `				for(;;){` |
|   10076 | 1051 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7634 | 1052 | `						zIn++;` |
|       2 | 1053 | `					}` |
|    1630 | 1054 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1055 | `						/* UTF-8 stream */` |
|     ! 0 | 1056 | `						zIn++;` |
|     ! 0 | 1057 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1058 | `							zIn++;` |
|     ! 0 | 1059 | `						}` |
|     ! 0 | 1060 | `						continue;` |
|       - | 1061 | `					}` |
|    1630 | 1062 | `					break;` |
|     ! 0 | 1063 | `				}` |
|    1630 | 1064 | `				if( zIn >= zEnd ){` |
|      96 | 1065 | `					break;` |
|       - | 1066 | `				}` |
|    1536 | 1067 | `				if( zIn[0] == '[' ){` |
|       9 | 1068 | `					sxi32 iSquare = 1;` |
|       9 | 1069 | `					zIn++;` |
|      17 | 1070 | `					while( zIn < zEnd ){` |
|      17 | 1071 | `						if( zIn[0] == '[' ){` |
|     ! 0 | 1072 | `							iSquare++;` |
|      17 | 1073 | `						}else if (zIn[0] == ']' ){` |
|       9 | 1074 | `							iSquare--;` |
|       9 | 1075 | `							if( iSquare <= 0 ){` |
|       9 | 1076 | `								break;` |
|       - | 1077 | `							}` |
|     ! 0 | 1078 | `						}` |
|       9 | 1079 | `						zIn++;` |
|       1 | 1080 | `					}` |
|       9 | 1081 | `					if( zIn < zEnd ){` |
|       9 | 1082 | `						zIn++;` |
|       4 | 1083 | `					}` |
|       9 | 1084 | `					break;` |
|    1528 | 1085 | `				}else if(zIn[0] == '{' ){` |
|       6 | 1086 | `					sxi32 iCurly = 1;` |
|       6 | 1087 | `					zIn++;` |
|      18 | 1088 | `					while( zIn < zEnd ){` |
|      16 | 1089 | `						if( zIn[0] == '{' ){` |
|     ! 0 | 1090 | `							iCurly++;` |
|      16 | 1091 | `						}else if (zIn[0] == '}' ){` |
|       3 | 1092 | `							iCurly--;` |
|       3 | 1093 | `							if( iCurly <= 0 ){` |
|       3 | 1094 | `								break;` |
|       - | 1095 | `							}` |
|     ! 0 | 1096 | `						}` |
|      14 | 1097 | `						zIn++;` |
|       2 | 1098 | `					}` |
|       6 | 1099 | `					if( zIn < zEnd ){` |
|       3 | 1100 | `						zIn++;` |
|       1 | 1101 | `					}` |
|       6 | 1102 | `					break;` |
|    1524 | 1103 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - | 1104 | `					/* Member access operator '->' */` |
|     ! 0 | 1105 | `					zIn += 2;` |
|    1524 | 1106 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - | 1107 | `					/* Static member access operator '::' */` |
|     ! 0 | 1108 | `					zIn += 2;` |
|     ! 0 | 1109 | `				}else{` |
|     763 | 1110 | `					break;` |
|       - | 1111 | `				}` |
|     ! 0 | 1112 | `			}` |
|       - | 1113 | `			/* Process the expression */` |
|    1630 | 1114 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1630 | 1115 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1116 | `				return SXERR_ABORT;` |
|       - | 1117 | `			}` |
|    1630 | 1118 | `			if( rc != SXERR_EMPTY ){` |
|    1628 | 1119 | `				++iCons;` |
|     813 | 1120 | `			}` |
|       - | 1121 | `		}` |
|       - | 1122 | `		/* Invalidate the previously used constant */` |
|    1716 | 1123 | `		pObj = 0;` |
|       2 | 1124 | `	}/*for(;;)*/` |
|   15412 | 1125 | `	if( iCons > 1 ){` |
|       - | 1126 | `		/* Concatenate all compiled constants */` |
|    1288 | 1127 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     643 | 1128 | `	}` |
|       - | 1129 | `	/* Node successfully compiled */` |
|   15412 | 1130 | `	return SXRET_OK;` |
|    7819 | 1131 |  |
|       - | 1132 | `/*` |
|       - | 1133 | ` * Compile a double quoted string.` |
|       - | 1134 | ` *  See the block-comment above for more information.` |
|       - | 1135 | ` */` |
|   15608 | 1136 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1137 |  |
|       - | 1138 | `	sxi32 rc;` |
|   15610 | 1139 | `	rc = GenStateCompileString(&(*pGen));` |
|    7804 | 1140 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1141 | `	/* Compilation result */` |
|   15610 | 1142 | `	return rc;` |
|       2 | 1143 |  |
|       - | 1144 | `/*` |
|       - | 1145 | ` * Compile a Heredoc string.` |
|       - | 1146 | ` *  See the block-comment above for more information.` |
|       - | 1147 | ` */` |
|      26 | 1148 | `static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1149 |  |
|      28 | 1150 | `	GenStateCompileString(&(*pGen));` |
|      13 | 1151 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1152 | `	/* Compilation result */` |
|      28 | 1153 | `	return SXRET_OK;` |
|       2 | 1154 |  |
|       - | 1155 | `/*` |
|       - | 1156 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1157 | ` *  Notes on array entries.` |
|       - | 1158 | ` *  According to the PHP language reference manual` |
|       - | 1159 | ` *  An array can be created by the array() language construct.` |
|       - | 1160 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1161 | ` *  array(  key =>  value` |
|       - | 1162 | ` *    , ...` |
|       - | 1163 | ` *    )` |
|       - | 1164 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1165 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1166 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1167 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1168 | ` *  contain integer and string indices.` |
|       - | 1169 | ` *  A value can be any PHP type.` |
|       - | 1170 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1171 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1172 | ` *  is specified, that value will be overwritten.` |
|       - | 1173 | ` */` |
|   15916 | 1174 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1175 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1176 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1177 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1178 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1179 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1180 | `	)` |
|       2 | 1181 |  |
|       - | 1182 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1183 | `	sxi32 rc;` |
|       - | 1184 | `	/* Swap token stream */` |
|   15918 | 1185 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1186 | `	/* Compile the expression*/` |
|   15918 | 1187 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1188 | `	/* Restore token stream */` |
|   15918 | 1189 | `	RE_SWAP_DELIMITER(pGen);` |
|   15918 | 1190 | `	return rc;` |
|       2 | 1191 |  |
|       - | 1192 | `/*` |
|       - | 1193 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1194 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1195 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1196 | ` * error message.` |
|       - | 1197 | ` * See the routine responible of compiling the array language construct` |
|       - | 1198 | ` * for more inforation.` |
|       - | 1199 | ` */` |
|      30 | 1200 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1201 |  |
|      32 | 1202 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1203 | `	if( pRoot->pOp ){` |
|      19 | 1204 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1205 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1206 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1207 | `			/* Unexpected expression */` |
|      11 | 1208 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1209 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1210 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1211 | `				rc = SXERR_INVALID;` |
|       5 | 1212 | `			}` |
|       7 | 1213 | `		}` |
|      25 | 1214 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1215 | `		/* Unexpected expression */` |
|       3 | 1216 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1217 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1218 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1219 | `			rc = SXERR_INVALID;` |
|       1 | 1220 | `		}` |
|       1 | 1221 | `	}` |
|      32 | 1222 | `	return rc;` |
|       2 | 1223 |  |
|       - | 1224 | `/*` |
|       - | 1225 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1226 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1227 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1228 | ` */` |
|   23246 | 1229 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1230 |  |
|       - | 1231 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1232 | `	SyToken *pKey,*pCur;` |
|   23248 | 1233 | `	sxi32 iEmitRef = 0;` |
|   23248 | 1234 | `	sxi32 nPair = 0;` |
|       - | 1235 | `	sxi32 iNest;` |
|       - | 1236 | `	sxi32 rc;` |
|   23248 | 1237 | `	xValidator = 0;` |
|   18921 | 1238 | `	for(;;){` |
|       - | 1239 | `		/* Jump leading commas */` |
|   42812 | 1240 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4970 | 1241 | `			pGen->pIn++;` |
|       2 | 1242 | `		}` |
|   37844 | 1243 | `		pCur = pGen->pIn;` |
|   37844 | 1244 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1245 | `			/* No more entry to process */` |
|   23236 | 1246 | `			break;` |
|       - | 1247 | `		}` |
|   14610 | 1248 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1249 | `			continue;` |
|       - | 1250 | `		}` |
|       - | 1251 | `		/* Compile the key if available */` |
|   14610 | 1252 | `		pKey = pCur;` |
|   14610 | 1253 | `		iNest = 0;` |
|   40522 | 1254 | `		while( pCur < pGen->pIn ){` |
|   27116 | 1255 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1204 | 1256 | `				break;` |
|       - | 1257 | `			}` |
|   25914 | 1258 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1259 | `				iNest++;` |
|   25876 | 1260 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1261 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1262 | `				 * parser will shortly detect any syntax error.` |
|       - | 1263 | `				 */` |
|      78 | 1264 | `				iNest--;` |
|      38 | 1265 | `			}` |
|   25914 | 1266 | `			pCur++;` |
|       2 | 1267 | `		}` |
|   14610 | 1268 | `		rc = SXERR_EMPTY;` |
|   14610 | 1269 | `		if( pCur < pGen->pIn ){` |
|    1204 | 1270 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1271 | `				/* Missing value */` |
|      11 | 1272 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1273 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1274 | `					return SXERR_ABORT;` |
|       - | 1275 | `				}` |
|      11 | 1276 | `				return SXRET_OK;` |
|       - | 1277 | `			}` |
|       - | 1278 | `			/* Compile the expression holding the key */` |
|    1194 | 1279 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1280 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1194 | 1281 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1282 | `				return SXERR_ABORT;` |
|       - | 1283 | `			}` |
|    1194 | 1284 | `			pCur++; /* Jump the '=>' operator */` |
|   14004 | 1285 | `		}else if( pKey == pCur ){` |
|       - | 1286 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1287 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1288 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1289 | `		}else{` |
|       - | 1290 | `			/* Reset back the cursor and point to the entry value */` |
|   13408 | 1291 | `			pCur = pKey;` |
|       - | 1292 | `		}` |
|   14600 | 1293 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1294 | `			/* No available key,load NULL */` |
|   13410 | 1295 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6704 | 1296 | `		}` |
|   14600 | 1297 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1298 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1299 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1300 | `			iEmitRef = 1;` |
|      34 | 1301 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1302 | `			if( pCur >= pGen->pIn ){` |
|       - | 1303 | `				/* Missing value */` |
|       3 | 1304 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1305 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1306 | `					return SXERR_ABORT;` |
|       - | 1307 | `				}` |
|       3 | 1308 | `				return SXRET_OK;` |
|       - | 1309 | `			}` |
|      15 | 1310 | `		}` |
|       - | 1311 | `		/* Compile indice value */` |
|   14598 | 1312 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14598 | 1313 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1314 | `			return SXERR_ABORT;` |
|       - | 1315 | `		}` |
|   14598 | 1316 | `		if( iEmitRef ){` |
|       - | 1317 | `			/* Emit the load reference instruction */` |
|      32 | 1318 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1319 | `		}` |
|   14598 | 1320 | `		xValidator = 0;` |
|   14598 | 1321 | `		iEmitRef = 0;` |
|   14598 | 1322 | `		nPair++;` |
|       2 | 1323 | `	}` |
|       - | 1324 | `	/* Emit the load map instruction */` |
|   23236 | 1325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1326 | `	/* Node successfully compiled */` |
|   23236 | 1327 | `	return SXRET_OK;` |
|   11625 | 1328 |  |
|       - | 1329 | `/*` |
|       - | 1330 | ` * Compile the 'array' language construct.` |
|       - | 1331 | ` *	 According to the PHP language reference manual` |
|       - | 1332 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1333 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1334 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1335 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1336 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1337 | ` */` |
|   23004 | 1338 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1339 |  |
|       - | 1340 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   23006 | 1341 | `	pGen->pIn += 2;` |
|   23006 | 1342 | `	pGen->pEnd--;` |
|   11502 | 1343 | `	SXUNUSED(iCompileFlag);` |
|   23006 | 1344 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1345 |  |
|       - | 1346 | `/*` |
|       - | 1347 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1348 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1349 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1350 | ` */` |
|     242 | 1351 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1352 |  |
|       - | 1353 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     244 | 1354 | `	pGen->pIn++;` |
|     244 | 1355 | `	pGen->pEnd--;` |
|     121 | 1356 | `	SXUNUSED(iCompileFlag);` |
|     244 | 1357 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1358 |  |
|       - | 1359 | `/*` |
|       - | 1360 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1361 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1362 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1363 | ` * error message.` |
|       - | 1364 | ` * See the routine responible of compiling the list language construct` |
|       - | 1365 | ` * for more inforation.` |
|       - | 1366 | ` */` |
|     128 | 1367 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1368 |  |
|     130 | 1369 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1370 | `	if( pRoot->pOp ){` |
|     ! 0 | 1371 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1372 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1373 | `				/* Unexpected expression */` |
|     ! 0 | 1374 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1375 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1376 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1377 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1378 | `				}` |
|     ! 0 | 1379 | `		}` |
|     130 | 1380 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1381 | `		/* Unexpected expression */` |
|       5 | 1382 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1383 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1384 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1385 | `			rc = SXERR_INVALID;` |
|       2 | 1386 | `		}` |
|       2 | 1387 | `	}` |
|     130 | 1388 | `	return rc;` |
|       2 | 1389 |  |
|       - | 1390 | `/*` |
|       - | 1391 | ` * Compile the 'list' language construct.` |
|       - | 1392 | ` *  According to the PHP language reference` |
|       - | 1393 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1394 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1395 | ` *  Description` |
|       - | 1396 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1397 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1398 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1399 | ` *  Parameters` |
|       - | 1400 | ` *   $varname: A variable.` |
|       - | 1401 | ` *  Return Values` |
|       - | 1402 | ` *   The assigned array.` |
|       - | 1403 | ` */` |
|       - | 1404 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1405 | `struct NestedListEntry {` |
|       - | 1406 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1407 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1408 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1409 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1410 | `};` |
|       - | 1411 | `/*` |
|       - | 1412 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1413 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1414 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1415 | ` */` |
|      74 | 1416 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1417 |  |
|       - | 1418 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1419 | `	SyToken *pNext;` |
|       - | 1420 | `	sxi32 nExpr;` |
|       - | 1421 | `	sxi32 rc;` |
|      76 | 1422 | `	nExpr = 0;` |
|      76 | 1423 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1424 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1425 | `		if( pGen->pIn < pNext ){` |
|       - | 1426 | `			/* Check for nested list() */` |
|     144 | 1427 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1428 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1429 | `				/* Record this nested list for post-processing */` |
|       3 | 1430 | `				SyToken *pListEnd = 0;` |
|       3 | 1431 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1432 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1433 | `				}` |
|       3 | 1434 | `				if( pListEnd ){` |
|       - | 1435 | `					struct NestedListEntry sEntry;` |
|       3 | 1436 | `					sEntry.nIndex = nExpr;` |
|       3 | 1437 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1438 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1439 | `					sEntry.isShort = 0;` |
|       3 | 1440 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1441 | `				}` |
|       - | 1442 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1443 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1444 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1445 | `				/* Nested short destructuring [...] */` |
|      13 | 1446 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1447 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1448 | `				if( pBracketEnd ){` |
|       - | 1449 | `					struct NestedListEntry sEntry;` |
|      13 | 1450 | `					sEntry.nIndex = nExpr;` |
|      13 | 1451 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1452 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1453 | `					sEntry.isShort = 1;` |
|      13 | 1454 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1455 | `				}` |
|       - | 1456 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1457 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1458 | `			}else{` |
|       - | 1459 | `				/* Compile the expression holding the variable */` |
|     130 | 1460 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1461 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1462 | `					SySetRelease(&sNested);` |
|     ! 0 | 1463 | `					return SXRET_OK;` |
|       - | 1464 | `				}` |
|       - | 1465 | `			}` |
|      73 | 1466 | `		}else{` |
|       - | 1467 | `			/* Empty entry,load NULL */` |
|      13 | 1468 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1469 | `		}` |
|     156 | 1470 | `		nExpr++;` |
|       - | 1471 | `		/* Advance the stream cursor */` |
|     156 | 1472 | `		pGen->pIn = &pNext[1];` |
|       2 | 1473 | `	}` |
|       - | 1474 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1475 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1476 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1477 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1478 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1479 | `	 */` |
|      76 | 1480 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1481 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1482 | `		sxu32 i;` |
|      27 | 1483 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1484 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1485 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1486 | `			ph7_value *pIdx;` |
|       - | 1487 | `			sxu32 nConstIdx;` |
|       - | 1488 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1489 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1490 | `			/* Push the integer index for this nested entry */` |
|      15 | 1491 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1492 | `			if( pIdx == 0 ){` |
|     ! 0 | 1493 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1494 | `				SySetRelease(&sNested);` |
|     ! 0 | 1495 | `				return SXERR_ABORT;` |
|       - | 1496 | `			}` |
|      15 | 1497 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1498 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1499 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1500 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1501 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1502 | `			 */` |
|      15 | 1503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1504 | `			/* Recursively compile the inner list */` |
|      15 | 1505 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1506 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1507 | `			if( apNested[i].isShort ){` |
|      13 | 1508 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1509 | `			}else{` |
|       3 | 1510 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1511 | `			}` |
|      15 | 1512 | `			pGen->pIn = pSavedIn;` |
|      15 | 1513 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1514 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1515 | `				SySetRelease(&sNested);` |
|     ! 0 | 1516 | `				return SXERR_ABORT;` |
|       - | 1517 | `			}` |
|       - | 1518 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1519 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1520 | `		}` |
|       6 | 1521 | `	}` |
|      76 | 1522 | `	SySetRelease(&sNested);` |
|       - | 1523 | `	/* Node successfully compiled */` |
|      76 | 1524 | `	return SXRET_OK;` |
|      39 | 1525 |  |
|      32 | 1526 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1527 |  |
|       - | 1528 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1529 | `	pGen->pIn += 2;` |
|      34 | 1530 | `	pGen->pEnd--;` |
|      16 | 1531 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1532 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1533 |  |
|      42 | 1534 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1535 |  |
|       - | 1536 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1537 | `	pGen->pIn++;` |
|      44 | 1538 | `	pGen->pEnd--;` |
|      21 | 1539 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1540 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1541 |  |
|       - | 1542 | `/* Forward declarations */` |
|       - | 1543 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1544 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1545 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1546 | `/*` |
|       - | 1547 | ` * Compile an annoynmous function or a closure.` |
|       - | 1548 | ` * According to the PHP language reference` |
|       - | 1549 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1550 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1551 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1552 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1553 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1554 | ` *  Example Anonymous function variable assignment example` |
|       - | 1555 | ` * <?php` |
|       - | 1556 | ` * $greet = function($name)` |
|       - | 1557 | ` * {` |
|       - | 1558 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1559 | ` * };` |
|       - | 1560 | ` * $greet('World');` |
|       - | 1561 | ` * $greet('PHP');` |
|       - | 1562 | ` * ?>` |
|       - | 1563 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1564 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1565 | ` */` |
|     168 | 1566 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1567 |  |
|       - | 1568 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1569 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1570 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1571 | `							  * one thread is allowed to compile the script.` |
|       - | 1572 | `						      */` |
|       - | 1573 | `	ph7_value *pObj;` |
|       - | 1574 | `	SyString sName;` |
|       - | 1575 | `	sxu32 nIdx;` |
|       - | 1576 | `	sxu32 nLen;` |
|       - | 1577 | `	sxi32 rc;` |
|       - | 1578 |  |
|     170 | 1579 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     170 | 1580 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1581 | `		pGen->pIn++;` |
|     ! 0 | 1582 | `	}` |
|       - | 1583 | `	/* Reserve a constant for the lambda */` |
|     170 | 1584 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 | 1585 | `	if( pObj == 0 ){` |
|     ! 0 | 1586 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1587 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1588 | `		return SXERR_ABORT;` |
|       - | 1589 | `	}` |
|       - | 1590 | `	/* Generate a unique name */` |
|     170 | 1591 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1592 | `	/* Make sure the generated name is unique */` |
|     170 | 1593 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1594 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1595 | `	}` |
|     170 | 1596 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     170 | 1597 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1598 | `	/* Compile the lambda body */` |
|     170 | 1599 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     170 | 1600 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1601 | `		return SXERR_ABORT;` |
|       - | 1602 | `	}` |
|     170 | 1603 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1604 | `		/* Emit the load closure instruction */` |
|      16 | 1605 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 | 1606 | `	}else{` |
|       - | 1607 | `		/* Emit the load constant instruction */` |
|     156 | 1608 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1609 | `	}` |
|       - | 1610 | `	/* Node successfully compiled */` |
|     170 | 1611 | `	return SXRET_OK;` |
|      86 | 1612 |  |
|       - | 1613 | `/*` |
|       - | 1614 | ` * Compile a backtick quoted string.` |
|       - | 1615 | ` */` |
|       4 | 1616 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1617 |  |
|       - | 1618 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1619 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1620 | `	 */` |
|       7 | 1621 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1622 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1623 | `		ph7_lib_version()` |
|       - | 1624 | `		);` |
|       - | 1625 | `	/* Load NULL */` |
|       5 | 1626 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1627 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1628 | `	/* Node successfully compiled */` |
|       5 | 1629 | `	return SXRET_OK;` |
|       1 | 1630 |  |
|       - | 1631 | `/*` |
|       - | 1632 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1633 | ` * construct.` |
|       - | 1634 | ` */` |
|      72 | 1635 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1636 |  |
|       - | 1637 | `	SyString *pName;` |
|       - | 1638 | `	sxu32 nKeyID;` |
|       - | 1639 | `	sxi32 rc;` |
|       - | 1640 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1641 | `	pName = &pGen->pIn->sData;` |
|      74 | 1642 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1643 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1644 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1645 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1646 | `		/* Compile arguments one after one */` |
|       9 | 1647 | `		pTmp = pGen->pEnd;` |
|       - | 1648 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1649 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1650 | `		 *  mean that the following expression is valid:` |
|       - | 1651 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1652 | `		 */` |
|       9 | 1653 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1654 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1655 | `			if( pGen->pIn < pNext ){` |
|       9 | 1656 | `				pGen->pEnd = pNext;` |
|       9 | 1657 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1658 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1659 | `					return SXERR_ABORT;` |
|       - | 1660 | `				}` |
|       9 | 1661 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1662 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1663 | `					 * without the overhead of a function call.` |
|       - | 1664 | `					 * This is a very powerful optimization that improve` |
|       - | 1665 | `					 * performance greatly.` |
|       - | 1666 | `					 */` |
|       9 | 1667 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1668 | `				}` |
|       4 | 1669 | `			}` |
|       - | 1670 | `			/* Jump trailing commas */` |
|       9 | 1671 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1672 | `				pNext++;` |
|     ! 0 | 1673 | `			}` |
|       9 | 1674 | `			pGen->pIn = pNext;` |
|       1 | 1675 | `		}` |
|       - | 1676 | `		/* Restore token stream */` |
|       9 | 1677 | `		pGen->pEnd = pTmp;` |
|       5 | 1678 | `	}else{` |
|      66 | 1679 | `		sxi32 nArg = 0;` |
|      66 | 1680 | `		sxu32 nIdx = 0;` |
|      66 | 1681 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1682 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1683 | `			return SXERR_ABORT;` |
|      66 | 1684 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1685 | `			nArg = 1;` |
|      32 | 1686 | `		}` |
|      66 | 1687 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1688 | `			ph7_value *pObj;` |
|       - | 1689 | `			/* Emit the call instruction */` |
|      20 | 1690 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1691 | `			if( pObj == 0 ){` |
|     ! 0 | 1692 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1693 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1694 | `				return SXERR_ABORT;` |
|       - | 1695 | `			}` |
|      20 | 1696 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1697 | `			/* Install in the literal table */` |
|      20 | 1698 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1699 | `		}` |
|       - | 1700 | `		/* Emit the call instruction */` |
|      66 | 1701 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1702 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1703 | `	}` |
|       - | 1704 | `	/* Node successfully compiled */` |
|      74 | 1705 | `	return SXRET_OK;` |
|      38 | 1706 |  |
|       - | 1707 | `/*` |
|       - | 1708 | ` * Compile a node holding a variable declaration.` |
|       - | 1709 | ` * According to the PHP language reference` |
|       - | 1710 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1711 | ` *  The variable name is case-sensitive.` |
|       - | 1712 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1713 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1714 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1715 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1716 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1717 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1718 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1719 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1720 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1721 | ` *  the chapter on Expressions.` |
|       - | 1722 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1723 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1724 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1725 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1726 | ` *  is being assigned (the source variable).` |
|       - | 1727 | ` */` |
|  776168 | 1728 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1729 |  |
|  776170 | 1730 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1731 | `	sxi32 iVv;` |
|       - | 1732 | `	sxi32 iP1;` |
|       - | 1733 | `	void *p3;` |
|       - | 1734 | `	sxi32 rc;` |
|  776170 | 1735 | `	iVv = -1; /* Variable variable counter */` |
| 1552350 | 1736 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  776182 | 1737 | `		pGen->pIn++;` |
|  776182 | 1738 | `		iVv++;` |
|       2 | 1739 | `	}` |
|  776170 | 1740 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1741 | `		/* Invalid variable name */` |
|     ! 0 | 1742 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1743 | `		if( rc == SXERR_ABORT ){` |
|       - | 1744 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1745 | `			return SXERR_ABORT;` |
|       - | 1746 | `		}` |
|     ! 0 | 1747 | `		return SXRET_OK;` |
|       - | 1748 | `	}` |
|  776170 | 1749 | `	p3  = 0;` |
|  776170 | 1750 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1751 | `		/* Dynamic variable creation */` |
|      18 | 1752 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1753 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1754 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1755 | `			/* Empty expression */` |
|       3 | 1756 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1757 | `			return SXRET_OK;` |
|       - | 1758 | `		}` |
|       - | 1759 | `		/* Compile the expression holding the variable name */` |
|      16 | 1760 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1761 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1762 | `			return SXERR_ABORT;` |
|      16 | 1763 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1764 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1765 | `			return SXRET_OK;` |
|       - | 1766 | `		}` |
|       7 | 1767 | `	}else{` |
|       - | 1768 | `		SyHashEntry *pEntry;` |
|       - | 1769 | `		SyString *pName;` |
|  776154 | 1770 | `		char *zName = 0;` |
|       - | 1771 | `		/* Extract variable name */` |
|  776154 | 1772 | `		pName = &pGen->pIn->sData;` |
|       - | 1773 | `		/* Advance the stream cursor */` |
|  776154 | 1774 | `		pGen->pIn++;` |
|  776154 | 1775 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  776154 | 1776 | `		if( pEntry == 0 ){` |
|       - | 1777 | `			/* Duplicate name */` |
|  111584 | 1778 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  111584 | 1779 | `			if( zName == 0 ){` |
|     ! 0 | 1780 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1781 | `				return SXERR_ABORT;` |
|       - | 1782 | `			}` |
|       - | 1783 | `			/* Install in the hashtable */` |
|  111584 | 1784 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   55793 | 1785 | `		}else{` |
|       - | 1786 | `			/* Name already available */` |
|  664572 | 1787 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1788 | `		}` |
|  776154 | 1789 | `		p3 = (void *)zName;` |
|       - | 1790 | `	}` |
|  776166 | 1791 | `	iP1 = 0;` |
|  776166 | 1792 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  298436 | 1793 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1794 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  292420 | 1795 | `			iP1 = 1;` |
|  146209 | 1796 | `		}` |
|  149217 | 1797 | `	}` |
|       - | 1798 | `	/* Emit the load instruction */` |
|  776166 | 1799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  776178 | 1800 | `	while( iVv > 0 ){` |
|      13 | 1801 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1802 | `		iVv--;` |
|       1 | 1803 | `	}` |
|       - | 1804 | `	/* Node successfully compiled */` |
|  776166 | 1805 | `	return SXRET_OK;` |
|  388086 | 1806 |  |
|       - | 1807 | `/*` |
|       - | 1808 | ` * Load a literal.` |
|       - | 1809 | ` */` |
|  520636 | 1810 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1811 |  |
|  520638 | 1812 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1813 | `	ph7_value *pObj;` |
|       - | 1814 | `	SyString *pStr;` |
|       - | 1815 | `	sxu32 nIdx;` |
|       - | 1816 | `	/* Extract token value */` |
|  520638 | 1817 | `	pStr = &pToken->sData;` |
|       - | 1818 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  520638 | 1819 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   94548 | 1820 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1821 | `			/* NULL constant are always indexed at 0 */` |
|   40198 | 1822 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   40198 | 1823 | `			return SXRET_OK;` |
|   54352 | 1824 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1825 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1826 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1827 | `			return SXRET_OK;` |
|       2 | 1828 | `		}` |
|  494089 | 1829 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   82130 | 1830 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1831 | `			/* FALSE constant are always indexed at 2 */` |
|   35086 | 1832 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   35086 | 1833 | `			return SXRET_OK;` |
|  427300 | 1834 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   72584 | 1835 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1836 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5316 | 1837 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5316 | 1838 | `			if( pObj == 0 ){` |
|     ! 0 | 1839 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1840 | `				return SXERR_ABORT;` |
|       - | 1841 | `			}` |
|    5316 | 1842 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1843 | `			/* Emit the load constant instruction */` |
|    5316 | 1844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5316 | 1845 | `			return SXRET_OK;` |
|  399092 | 1846 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   26796 | 1847 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1848 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1849 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1850 | `			if( pObj == 0 ){` |
|     ! 0 | 1851 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1852 | `				return SXERR_ABORT;` |
|       - | 1853 | `			}` |
|       7 | 1854 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1855 | `				SyString sNs;` |
|       7 | 1856 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1857 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1858 | `			}else{` |
|     ! 0 | 1859 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1860 | `			}` |
|       7 | 1861 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1862 | `			return SXRET_OK;` |
|  398279 | 1863 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11216 | 1864 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  392665 | 1865 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13972 | 1866 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1867 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1868 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1869 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1870 | `				/* Point to the upper block */` |
|      11 | 1871 | `				pBlock = pBlock->pParent;` |
|       1 | 1872 | `			}` |
|      11 | 1873 | `			if( pBlock == 0 ){` |
|       - | 1874 | `				/* Called in the global scope,load NULL */` |
|       5 | 1875 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1876 | `			}else{` |
|       - | 1877 | `				/* Extract the target function/method */` |
|       7 | 1878 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1879 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1880 | `					/* Not a class method,Load null */` |
|       3 | 1881 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1882 | `				}else{` |
|       5 | 1883 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1884 | `					if( pObj == 0 ){` |
|     ! 0 | 1885 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1886 | `						return SXERR_ABORT;` |
|       - | 1887 | `					}` |
|       5 | 1888 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1889 | `					/* Emit the load constant instruction */` |
|       5 | 1890 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1891 | `				}` |
|       - | 1892 | `			}` |
|      11 | 1893 | `			return SXRET_OK;` |
|       - | 1894 | `	}` |
|       - | 1895 | `	/* Query literal table */` |
|  439542 | 1896 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1897 | `		ph7_value *pLitObj;` |
|       - | 1898 | `		/* Unknown literal,install it in the literal table */` |
|  205654 | 1899 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  205654 | 1900 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1901 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1902 | `			return SXERR_ABORT;` |
|       - | 1903 | `		}` |
|  205654 | 1904 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  205654 | 1905 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  102826 | 1906 | `	}` |
|       - | 1907 | `	/* Emit the load constant instruction */` |
|  439542 | 1908 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  439542 | 1909 | `	return SXRET_OK;` |
|  260320 | 1910 |  |
|       - | 1911 | `/*` |
|       - | 1912 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1913 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1914 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1915 | ` * Otherwise, load the simple literal directly.` |
|       - | 1916 | ` */` |
|  520660 | 1917 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1918 |  |
|       - | 1919 | `	sxi32 rc;` |
|  520662 | 1920 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1921 | `		return SXRET_OK;` |
|       - | 1922 | `	}` |
|       - | 1923 | `	/* Check if this is a multi-token namespace path */` |
|  520662 | 1924 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1925 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1926 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1927 | `		int isAbsolute = 0;` |
|      26 | 1928 | `		SyBlobReset(pWorker);` |
|       - | 1929 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1930 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1931 | `			isAbsolute = 1;` |
|      24 | 1932 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1933 | `		}` |
|       - | 1934 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1935 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1936 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1937 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1938 | `		}` |
|       - | 1939 | `		/* Collect all path components */` |
|     102 | 1940 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1941 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1942 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1943 | `			}else{` |
|      64 | 1944 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1945 | `			}` |
|     102 | 1946 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1947 | `				pGen->pIn++;` |
|      26 | 1948 | `				break;` |
|       - | 1949 | `			}` |
|      78 | 1950 | `			pGen->pIn++;` |
|       2 | 1951 | `		}` |
|      26 | 1952 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1953 | `			ph7_value *pObj;` |
|       - | 1954 | `			SyString sPath;` |
|       - | 1955 | `			sxu32 nIdx;` |
|      26 | 1956 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1957 | `			/* Install in the literal table */` |
|      26 | 1958 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1959 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1960 | `				if( pObj == 0 ){` |
|     ! 0 | 1961 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1962 | `					return SXERR_ABORT;` |
|       - | 1963 | `				}` |
|      13 | 1964 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1965 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1966 | `			}` |
|       - | 1967 | `			/* Emit the load constant instruction.` |
|       - | 1968 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1969 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1970 | `			return SXRET_OK;` |
|       - | 1971 | `		}` |
|     ! 0 | 1972 | `	}` |
|       - | 1973 | `	/* Single-token literal: load directly */` |
|  520638 | 1974 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  520638 | 1975 | `	return rc;` |
|  260332 | 1976 |  |
|       - | 1977 | `/*` |
|       - | 1978 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1979 | ` */` |
|  520660 | 1980 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1981 |  |
|       - | 1982 | `	sxi32 rc;` |
|  520662 | 1983 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  520662 | 1984 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1985 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1986 | `		return rc;` |
|       - | 1987 | `	}` |
|       - | 1988 | `	/* Node successfully compiled */` |
|  520662 | 1989 | `	return SXRET_OK;` |
|  260332 | 1990 |  |
|       - | 1991 | `/*` |
|       - | 1992 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1993 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1994 | ` */` |
|       8 | 1995 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1996 |  |
|       - | 1997 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1998 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1999 | `		pGen->pIn++;` |
|       1 | 2000 | `	}` |
|       9 | 2001 | `	return SXRET_OK;` |
|       1 | 2002 |  |
|       - | 2003 | `/*` |
|       - | 2004 | ` * Check if the given identifier name is reserved or not.` |
|       - | 2005 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 2006 | ` */` |
|      56 | 2007 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 2008 |  |
|      58 | 2009 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 2010 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 2011 | `			return TRUE;` |
|      24 | 2012 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 2013 | `			return TRUE;` |
|       2 | 2014 | `		}` |
|      43 | 2015 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 2016 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 2017 | `			return TRUE;` |
|       - | 2018 | `		}` |
|     ! 0 | 2019 | `	}` |
|       - | 2020 | `	/* Not a reserved constant */` |
|      50 | 2021 | `	return FALSE;` |
|      30 | 2022 |  |
|       - | 2023 | `/*` |
|       - | 2024 | ` * Compile the 'const' statement.` |
|       - | 2025 | ` * According to the PHP language reference` |
|       - | 2026 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 2027 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 2028 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 2029 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 2030 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2031 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 2032 | ` *  Syntax` |
|       - | 2033 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 2034 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 2035 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 2036 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 2037 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 2038 | ` *  to get a list of all defined constants.` |
|       - | 2039 | ` *` |
|       - | 2040 | ` * Symisc eXtension.` |
|       - | 2041 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 2042 | ` *  would allow only simple scalar value.` |
|       - | 2043 | ` *  Example` |
|       - | 2044 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2045 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2046 | ` */` |
|      32 | 2047 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 2048 |  |
|       - | 2049 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 2050 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2051 | `	SyString *pName;` |
|       - | 2052 | `	sxi32 rc;` |
|      34 | 2053 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 2054 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2055 | `		/* Invalid constant name */` |
|       7 | 2056 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 2057 | `		if( rc == SXERR_ABORT ){` |
|       - | 2058 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2059 | `			return SXERR_ABORT;` |
|       - | 2060 | `		}` |
|       7 | 2061 | `		goto Synchronize;` |
|       - | 2062 | `	}` |
|       - | 2063 | `	/* Peek constant name */` |
|      28 | 2064 | `	pName = &pGen->pIn->sData;` |
|       - | 2065 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 2066 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 2067 | `		/* Reserved constant */` |
|       9 | 2068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 2069 | `		if( rc == SXERR_ABORT ){` |
|       - | 2070 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2071 | `			return SXERR_ABORT;` |
|       - | 2072 | `		}` |
|       9 | 2073 | `		goto Synchronize;` |
|       - | 2074 | `	}` |
|      20 | 2075 | `	pGen->pIn++;` |
|      20 | 2076 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 2077 | `		/* Invalid statement*/` |
|       5 | 2078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 2079 | `		if( rc == SXERR_ABORT ){` |
|       - | 2080 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2081 | `			return SXERR_ABORT;` |
|       - | 2082 | `		}` |
|       5 | 2083 | `		goto Synchronize;` |
|       - | 2084 | `	}` |
|      15 | 2085 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 2086 | `	/* Allocate a new constant value container */` |
|      15 | 2087 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 2088 | `	if( pConsCode == 0 ){` |
|     ! 0 | 2089 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2090 | `		return SXERR_ABORT;` |
|       - | 2091 | `	}` |
|      15 | 2092 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 2093 | `	/* Swap bytecode container */` |
|      15 | 2094 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 2095 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 2096 | `	/* Compile constant value */` |
|      15 | 2097 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2098 | `	/* Emit the done instruction */` |
|      15 | 2099 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 2100 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 2101 | `	if( rc == SXERR_ABORT ){` |
|       - | 2102 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2103 | `		return SXERR_ABORT;` |
|       - | 2104 | `	}` |
|      15 | 2105 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 2106 | `	/* Register the constant with namespace-qualified name */` |
|       - | 2107 | `	{` |
|       - | 2108 | `		SyBlob sFQN;` |
|       - | 2109 | `		SyString sFQNStr;` |
|      15 | 2110 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 2111 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 2112 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 2113 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 2114 | `		SyBlobRelease(&sFQN);` |
|       - | 2115 | `	}` |
|      15 | 2116 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2117 | `		SySetRelease(pConsCode);` |
|     ! 0 | 2118 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 2119 | `	}` |
|      15 | 2120 | `	return SXRET_OK;` |
|       9 | 2121 | `Synchronize:` |
|       - | 2122 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 2123 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 2124 | `		pGen->pIn++;` |
|       1 | 2125 | `	}` |
|      19 | 2126 | `	return SXRET_OK;` |
|      18 | 2127 |  |
|       - | 2128 | `/*` |
|       - | 2129 | ` * Compile the 'continue' statement.` |
|       - | 2130 | ` * According to the PHP language reference` |
|       - | 2131 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 2132 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 2133 | ` *  iteration.` |
|       - | 2134 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 2135 | ` *  the purposes of continue.` |
|       - | 2136 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 2137 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 2138 | ` *  Note:` |
|       - | 2139 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 2140 | ` */` |
|       - | 2141 | `/*` |
|       - | 2142 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 2143 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 2144 | ` * break/continue crosses a try boundary.` |
|       - | 2145 | ` *` |
|       - | 2146 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 2147 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 2148 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 2149 | ` */` |
|    2784 | 2150 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 2151 |  |
|    2786 | 2152 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16250 | 2153 | `	while( pBlock && pBlock != pTarget ){` |
|   13466 | 2154 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 2155 | `			if( pBlock->pUserData ){` |
|       - | 2156 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 2157 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 2158 | `			}else{` |
|       - | 2159 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 2160 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 2161 | `				 * exception context from a sub-execution.` |
|       - | 2162 | `				 */` |
|     ! 0 | 2163 | `				break;` |
|       - | 2164 | `			}` |
|       1 | 2165 | `		}` |
|   13466 | 2166 | `		pBlock = pBlock->pParent;` |
|       2 | 2167 | `	}` |
|    2786 | 2168 |  |
|    2700 | 2169 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2170 |  |
|       - | 2171 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2172 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2173 | `	sxu32 nLineLocal;` |
|       - | 2174 | `	sxi32 rc;` |
|    2702 | 2175 | `	nLineLocal = pGen->pIn->nLine;` |
|    2702 | 2176 | `	iLevel = 0;` |
|       - | 2177 | `	/* Jump the 'continue' keyword */` |
|    2702 | 2178 | `	pGen->pIn++;` |
|    2702 | 2179 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2180 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2181 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2182 | `		 */` |
|       - | 2183 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2184 | `		char *zAlloc = 0;` |
|       - | 2185 | `		SyString sNum;` |
|      16 | 2186 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2187 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2188 | `			return SXERR_ABORT;` |
|       - | 2189 | `		}` |
|      16 | 2190 | `		if( rc == SXRET_OK ){` |
|      20 | 2191 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2192 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2193 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2194 | `				return SXERR_ABORT;` |
|       - | 2195 | `			}` |
|      14 | 2196 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2197 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2198 | `		}` |
|      16 | 2199 | `		if( iLevel < 2 ){` |
|       3 | 2200 | `			iLevel = 0;` |
|       1 | 2201 | `		}` |
|      16 | 2202 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2203 | `	}` |
|       - | 2204 | `	/* Point to the target loop */` |
|    2702 | 2205 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2702 | 2206 | `	if( pLoop == 0 ){` |
|       - | 2207 | `		/* Illegal continue */` |
|      11 | 2208 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2209 | `		if( rc == SXERR_ABORT ){` |
|       - | 2210 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2211 | `			return SXERR_ABORT;` |
|       - | 2212 | `		}` |
|       6 | 2213 | `	}else{` |
|    2692 | 2214 | `		sxu32 nInstrIdx = 0;` |
|       - | 2215 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2692 | 2216 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2692 | 2217 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2218 | `			/* According to the PHP language reference manual` |
|       - | 2219 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2220 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2221 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2222 | `			 */` |
|       5 | 2223 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2224 | `			if( rc == SXRET_OK ){` |
|       5 | 2225 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2226 | `			}` |
|       3 | 2227 | `		}else{` |
|       - | 2228 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2688 | 2229 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2688 | 2230 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2231 | `				JumpFixup sJumpFix;` |
|       - | 2232 | `				/* Post-continue */` |
|      14 | 2233 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 | 2234 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 | 2235 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 | 2236 | `			}` |
|       - | 2237 | `		}` |
|       - | 2238 | `	}` |
|    2702 | 2239 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2240 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2241 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2242 | `	}` |
|       - | 2243 | `	/* Statement successfully compiled */` |
|    2702 | 2244 | `	return SXRET_OK;` |
|    1352 | 2245 |  |
|       - | 2246 | `/*` |
|       - | 2247 | ` * Compile the 'break' statement.` |
|       - | 2248 | ` * According to the PHP language reference` |
|       - | 2249 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2250 | ` *  structure.` |
|       - | 2251 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2252 | ` *  enclosing structures are to be broken out of.` |
|       - | 2253 | ` */` |
|     110 | 2254 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2255 |  |
|       - | 2256 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2257 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2258 | `	sxi32 rc;` |
|     112 | 2259 | `	iLevel = 0;` |
|       - | 2260 | `	/* Jump the 'break' keyword */` |
|     112 | 2261 | `	pGen->pIn++;` |
|     112 | 2262 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2263 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2264 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2265 | `		 */` |
|       - | 2266 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2267 | `		char *zAlloc = 0;` |
|       - | 2268 | `		SyString sNum;` |
|      16 | 2269 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2270 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2271 | `			return SXERR_ABORT;` |
|       - | 2272 | `		}` |
|      16 | 2273 | `		if( rc == SXRET_OK ){` |
|      20 | 2274 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2275 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2276 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2277 | `				return SXERR_ABORT;` |
|       - | 2278 | `			}` |
|      14 | 2279 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2280 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2281 | `		}` |
|      16 | 2282 | `		if( iLevel < 2 ){` |
|       3 | 2283 | `			iLevel = 0;` |
|       1 | 2284 | `		}` |
|      16 | 2285 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2286 | `	}` |
|       - | 2287 | `	/* Extract the target loop */` |
|     112 | 2288 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 | 2289 | `	if( pLoop == 0 ){` |
|       - | 2290 | `		/* Illegal break */` |
|      17 | 2291 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2292 | `		if( rc == SXERR_ABORT ){` |
|       - | 2293 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2294 | `			return SXERR_ABORT;` |
|       - | 2295 | `		}` |
|       9 | 2296 | `	}else{` |
|       - | 2297 | `		sxu32 nInstrIdx;` |
|       - | 2298 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 | 2299 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 | 2300 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 | 2301 | `		if( rc == SXRET_OK ){` |
|       - | 2302 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 | 2303 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 | 2304 | `		}` |
|       - | 2305 | `	}` |
|     112 | 2306 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2307 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2308 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2309 | `	}` |
|       - | 2310 | `	/* Statement successfully compiled */` |
|     112 | 2311 | `	return SXRET_OK;` |
|      57 | 2312 |  |
|       - | 2313 | `/*` |
|       - | 2314 | ` * Compile or record a label.` |
|       - | 2315 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2316 | ` * Example` |
|       - | 2317 | ` *  goto LABEL;` |
|       - | 2318 | ` *   echo 'Foo';` |
|       - | 2319 | ` *  LABEL:` |
|       - | 2320 | ` *   echo 'Bar';` |
|       - | 2321 | ` */` |
|     112 | 2322 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2323 |  |
|       - | 2324 | `	GenBlock *pBlock;` |
|       - | 2325 | `	Label sLabel;` |
|       - | 2326 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2327 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2328 | `	if( pBlock ){` |
|       - | 2329 | `		sxi32 rc;` |
|       7 | 2330 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2331 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2332 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2333 | `			return SXERR_ABORT;` |
|       - | 2334 | `		}` |
|       3 | 2335 | `	}else{` |
|     110 | 2336 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2337 | `		char *zDup;` |
|       - | 2338 | `		/* Initialize label fields */` |
|     110 | 2339 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2340 | `		/* Duplicate label name */` |
|     110 | 2341 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2342 | `		if( zDup == 0 ){` |
|     ! 0 | 2343 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2344 | `			return SXERR_ABORT;` |
|       - | 2345 | `		}` |
|     110 | 2346 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2347 | `		sLabel.bRef  = FALSE;` |
|     110 | 2348 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2349 | `		pBlock = pGen->pCurrent;` |
|     218 | 2350 | `		while( pBlock ){` |
|     130 | 2351 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2352 | `				break;` |
|       - | 2353 | `			}` |
|       - | 2354 | `			/* Point to the upper block */` |
|     110 | 2355 | `			pBlock = pBlock->pParent;` |
|       2 | 2356 | `		}` |
|     110 | 2357 | `		if( pBlock ){` |
|      22 | 2358 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2359 | `		}else{` |
|      90 | 2360 | `			sLabel.pFunc = 0;` |
|       - | 2361 | `		}` |
|       - | 2362 | `		/* Insert in label set */` |
|     110 | 2363 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2364 | `	}` |
|     114 | 2365 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2366 | `	return SXRET_OK;` |
|      58 | 2367 |  |
|       - | 2368 | `/*` |
|       - | 2369 | ` * Compile the so hated 'goto' statement.` |
|       - | 2370 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2371 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2372 | ` * a compiler it has to do this.` |
|       - | 2373 | ` * According to the PHP language reference manual` |
|       - | 2374 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2375 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2376 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2377 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2378 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2379 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2380 | ` *   of a multi-level break` |
|       - | 2381 | ` */` |
|     152 | 2382 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2383 |  |
|       - | 2384 | `	JumpFixup sJump;` |
|       - | 2385 | `	sxi32 rc;` |
|     154 | 2386 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2387 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2388 | `		/* Missing label */` |
|     ! 0 | 2389 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2390 | `		if( rc == SXERR_ABORT ){` |
|       - | 2391 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2392 | `			return SXERR_ABORT;` |
|       - | 2393 | `		}` |
|     ! 0 | 2394 | `		return SXRET_OK;` |
|       - | 2395 | `	}` |
|     154 | 2396 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2397 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2398 | `		if( rc == SXERR_ABORT ){` |
|       - | 2399 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2400 | `			return SXERR_ABORT;` |
|       - | 2401 | `		}` |
|       3 | 2402 | `	}else{` |
|     150 | 2403 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2404 | `		GenBlock *pBlock;` |
|       - | 2405 | `		char *zDup;` |
|       - | 2406 | `		/* Prepare the jump destination */` |
|     150 | 2407 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2408 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2409 | `		/* Duplicate label name */` |
|     150 | 2410 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2411 | `		if( zDup == 0 ){` |
|     ! 0 | 2412 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2413 | `			return SXERR_ABORT;` |
|       - | 2414 | `		}` |
|     150 | 2415 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2416 | `		pBlock = pGen->pCurrent;` |
|     312 | 2417 | `		while( pBlock ){` |
|     196 | 2418 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2419 | `				break;` |
|       - | 2420 | `			}` |
|       - | 2421 | `			/* Point to the upper block */` |
|     164 | 2422 | `			pBlock = pBlock->pParent;` |
|       2 | 2423 | `		}` |
|     150 | 2424 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2425 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2426 | `			if( rc == SXERR_ABORT ){` |
|       - | 2427 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2428 | `				return SXERR_ABORT;` |
|       - | 2429 | `			}` |
|       3 | 2430 | `		}` |
|     150 | 2431 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2432 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2433 | `		}else{` |
|     124 | 2434 | `			sJump.pFunc = 0;` |
|       - | 2435 | `		}` |
|       - | 2436 | `		/* Emit the unconditional jump */` |
|     150 | 2437 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2438 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2439 | `		}` |
|       - | 2440 | `	}` |
|     154 | 2441 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2443 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2444 | `	}` |
|       - | 2445 | `	/* Statement successfully compiled */` |
|     154 | 2446 | `	return SXRET_OK;` |
|      78 | 2447 |  |
|       - | 2448 | `/*` |
|       - | 2449 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2450 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2451 | ` * failure.` |
|       - | 2452 | ` */` |
|      20 | 2453 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2454 |  |
|       - | 2455 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2456 | `	sxu32 nRawObj;` |
|      10 | 2457 | `	sxu32 nObjIdx;` |
|       - | 2458 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2459 | `	 * a PHP block.` |
|       - | 2460 | `	 */` |
|      10 | 2461 | `Consume:` |
|      21 | 2462 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2463 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2464 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2465 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2466 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2467 | `			return SXERR_ABORT;` |
|       - | 2468 | `		}` |
|       - | 2469 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2470 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2471 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2472 | `		++nRawObj;` |
|     ! 0 | 2473 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2474 | `	}` |
|      21 | 2475 | `	if( nRawObj > 0 ){` |
|       - | 2476 | `		/* Emit the consume instruction */` |
|     ! 0 | 2477 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2478 | `	}` |
|      21 | 2479 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2480 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2481 | `		/* Reset the token set */` |
|     ! 0 | 2482 | `		SySetReset(pTokenSet);` |
|       - | 2483 | `		/* Tokenize input */` |
|     ! 0 | 2484 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2485 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2486 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2487 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2488 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2489 | `		/* Advance the stream cursor */` |
|     ! 0 | 2490 | `		pGen->pRawIn++;` |
|       - | 2491 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2492 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2493 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2494 | `			sxi32 rc;` |
|       - | 2495 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2496 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2497 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2498 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2499 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2500 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2501 | `				return SXERR_ABORT;` |
|     ! 0 | 2502 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2503 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2504 | `			}` |
|     ! 0 | 2505 | `			goto Consume;` |
|       - | 2506 | `		}` |
|     ! 0 | 2507 | `	}else{` |
|       - | 2508 | `		/* No more chunks to process */` |
|      21 | 2509 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2510 | `		return SXERR_EOF;` |
|       - | 2511 | `	}` |
|     ! 0 | 2512 | `	return SXRET_OK;` |
|      11 | 2513 |  |
|       - | 2514 | `/*` |
|       - | 2515 | ` * Compile a PHP block.` |
|       - | 2516 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2517 | ` * optionally delimited by braces {}.` |
|       - | 2518 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2519 | ` * and this function takes care of generating the appropriate error` |
|       - | 2520 | ` * message.` |
|       - | 2521 | ` */` |
|  292710 | 2522 | `static sxi32 PH7_CompileBlock(` |
|       - | 2523 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2524 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2525 | `	)` |
|       2 | 2526 |  |
|       - | 2527 | `	sxi32 rc;` |
|       - | 2528 | `	sxu32 nLine;` |
|  292712 | 2529 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  291302 | 2530 | `		nLine = pGen->pIn->nLine;` |
|  291302 | 2531 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  291302 | 2532 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2533 | `			return SXERR_ABORT;` |
|       - | 2534 | `		}` |
|  291302 | 2535 | `		pGen->pIn++;` |
|       - | 2536 | `		/* Compile until we hit the closing braces '}' */` |
|  402132 | 2537 | `		for(;;){` |
|  804266 | 2538 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2539 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2540 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2541 | `			 	   return SXERR_ABORT;` |
|       - | 2542 | `				}` |
|      21 | 2543 | `				if( rc == SXERR_EOF ){` |
|       - | 2544 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2545 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2546 | `					break;` |
|       - | 2547 | `				}` |
|     ! 0 | 2548 | `			}` |
|  804246 | 2549 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2550 | `				/* Closing braces found,break immediately*/` |
|  291282 | 2551 | `				pGen->pIn++;` |
|  291282 | 2552 | `				break;` |
|       - | 2553 | `			}` |
|       - | 2554 | `			/* Compile a single statement */` |
|  512966 | 2555 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  512966 | 2556 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2557 | `				return SXERR_ABORT;` |
|       - | 2558 | `			}` |
|       2 | 2559 | `		}` |
|  291302 | 2560 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  147062 | 2561 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2562 | `		pGen->pIn++;` |
|     ! 0 | 2563 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2564 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2565 | `			return SXERR_ABORT;` |
|       - | 2566 | `		}` |
|       - | 2567 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2568 | `		for(;;){` |
|     ! 0 | 2569 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2570 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2571 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2572 | `			 	   return SXERR_ABORT;` |
|       - | 2573 | `				}` |
|     ! 0 | 2574 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2575 | `					/* No more token to process */` |
|     ! 0 | 2576 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2577 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2578 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2579 | `					}` |
|     ! 0 | 2580 | `					break;` |
|       - | 2581 | `				}` |
|     ! 0 | 2582 | `			}` |
|     ! 0 | 2583 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2584 | `				sxi32 nKwrd;` |
|       - | 2585 | `				/* Keyword found */` |
|     ! 0 | 2586 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2587 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2588 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2589 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2590 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2591 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2592 | `						}` |
|     ! 0 | 2593 | `						break;` |
|       - | 2594 | `				}` |
|     ! 0 | 2595 | `			}` |
|       - | 2596 | `			/* Compile a single statement */` |
|     ! 0 | 2597 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2598 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2599 | `				return SXERR_ABORT;` |
|       - | 2600 | `			}` |
|     ! 0 | 2601 | `		}` |
|     ! 0 | 2602 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2603 | `	}else{` |
|       - | 2604 | `		/* Compile a single statement */` |
|    1412 | 2605 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1412 | 2606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2607 | `			return SXERR_ABORT;` |
|       - | 2608 | `		}` |
|       - | 2609 | `	}` |
|       - | 2610 | `	/* Jump trailing semi-colons ';' */` |
|  292712 | 2611 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2612 | `		pGen->pIn++;` |
|     ! 0 | 2613 | `	}` |
|  292712 | 2614 | `	return SXRET_OK;` |
|  146357 | 2615 |  |
|       - | 2616 | `/*` |
|       - | 2617 | ` * Compile the gentle 'while' statement.` |
|       - | 2618 | ` * According to the PHP language reference` |
|       - | 2619 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2620 | ` *  The basic form of a while statement is:` |
|       - | 2621 | ` *  while (expr)` |
|       - | 2622 | ` *   statement` |
|       - | 2623 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2624 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2625 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2626 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2627 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2628 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2629 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2630 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2631 | ` *  while (expr):` |
|       - | 2632 | ` *    statement` |
|       - | 2633 | ` *   endwhile;` |
|       - | 2634 | ` */` |
|   10726 | 2635 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2636 |  |
|   10728 | 2637 | `	GenBlock *pWhileBlock = 0;` |
|   10728 | 2638 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2639 | `	sxu32 nFalseJump;` |
|       - | 2640 | `	sxu32 nLine;` |
|       - | 2641 | `	sxi32 rc;` |
|   10728 | 2642 | `	nLine = pGen->pIn->nLine;` |
|       - | 2643 | `	/* Jump the 'while' keyword */` |
|   10728 | 2644 | `	pGen->pIn++;` |
|   10728 | 2645 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2646 | `		/* Syntax error */` |
|     ! 0 | 2647 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2648 | `		if( rc == SXERR_ABORT ){` |
|       - | 2649 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2650 | `			return SXERR_ABORT;` |
|       - | 2651 | `		}` |
|     ! 0 | 2652 | `		goto Synchronize;` |
|       - | 2653 | `	}` |
|       - | 2654 | `	/* Jump the left parenthesis '(' */` |
|   10728 | 2655 | `	pGen->pIn++;` |
|       - | 2656 | `	/* Create the loop block */` |
|   10728 | 2657 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10728 | 2658 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2659 | `		return SXERR_ABORT;` |
|       - | 2660 | `	}` |
|       - | 2661 | `	/* Delimit the condition */` |
|   10728 | 2662 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10728 | 2663 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2664 | `		/* Empty expression */` |
|       3 | 2665 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2666 | `		if( rc == SXERR_ABORT ){` |
|       - | 2667 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2668 | `			return SXERR_ABORT;` |
|       - | 2669 | `		}` |
|       1 | 2670 | `	}` |
|       - | 2671 | `	/* Swap token streams */` |
|   10728 | 2672 | `	pTmp = pGen->pEnd;` |
|   10728 | 2673 | `	pGen->pEnd = pEnd;` |
|       - | 2674 | `	/* Compile the expression */` |
|   10728 | 2675 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10728 | 2676 | `	if( rc == SXERR_ABORT ){` |
|       - | 2677 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2678 | `		return SXERR_ABORT;` |
|       - | 2679 | `	}` |
|       - | 2680 | `	/* Update token stream */` |
|   10728 | 2681 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2682 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2683 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2684 | `			return SXERR_ABORT;` |
|       - | 2685 | `		}` |
|     ! 0 | 2686 | `		pGen->pIn++;` |
|     ! 0 | 2687 | `	}` |
|       - | 2688 | `	/* Synchronize pointers */` |
|   10728 | 2689 | `	pGen->pIn  = &pEnd[1];` |
|   10728 | 2690 | `	pGen->pEnd = pTmp;` |
|       - | 2691 | `	/* Emit the false jump */` |
|   10728 | 2692 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2693 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10728 | 2694 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2695 | `	/* Compile the loop body */` |
|   10728 | 2696 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10728 | 2697 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2698 | `		return SXERR_ABORT;` |
|       - | 2699 | `	}` |
|       - | 2700 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10728 | 2701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2702 | `	/* Fix all jumps now the destination is resolved */` |
|   10728 | 2703 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2704 | `	/* Release the loop block */` |
|   10728 | 2705 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2706 | `	/* Statement successfully compiled */` |
|   10728 | 2707 | `	return SXRET_OK;` |
|     ! 0 | 2708 | `Synchronize:` |
|       - | 2709 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2710 | `	 * compiling this erroneous block.` |
|       - | 2711 | `	 */` |
|     ! 0 | 2712 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2713 | `		pGen->pIn++;` |
|     ! 0 | 2714 | `	}` |
|     ! 0 | 2715 | `	return SXRET_OK;` |
|    5365 | 2716 |  |
|       - | 2717 | `/*` |
|       - | 2718 | ` * Compile the ugly do..while() statement.` |
|       - | 2719 | ` * According to the PHP language reference` |
|       - | 2720 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2721 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2722 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2723 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2724 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2725 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2726 | ` *  would end immediately).` |
|       - | 2727 | ` *  There is just one syntax for do-while loops:` |
|       - | 2728 | ` *  <?php` |
|       - | 2729 | ` *  $i = 0;` |
|       - | 2730 | ` *  do {` |
|       - | 2731 | ` *   echo $i;` |
|       - | 2732 | ` *  } while ($i > 0);` |
|       - | 2733 | ` * ?>` |
|       - | 2734 | ` */` |
|       2 | 2735 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2736 |  |
|       3 | 2737 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2738 | `	GenBlock *pDoBlock = 0;` |
|       - | 2739 | `	sxu32 nLine;` |
|       - | 2740 | `	sxi32 rc;` |
|       3 | 2741 | `	nLine = pGen->pIn->nLine;` |
|       - | 2742 | `	/* Jump the 'do' keyword */` |
|       3 | 2743 | `	pGen->pIn++;` |
|       - | 2744 | `	/* Create the loop block */` |
|       3 | 2745 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2746 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2747 | `		return SXERR_ABORT;` |
|       - | 2748 | `	}` |
|       - | 2749 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2750 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2751 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2752 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2753 | `		return SXERR_ABORT;` |
|       - | 2754 | `	}` |
|       3 | 2755 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2756 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2757 | `	}` |
|       3 | 2758 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2759 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2760 | `			/* Missing 'while' statement */` |
|       3 | 2761 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2762 | `			if( rc == SXERR_ABORT ){` |
|       - | 2763 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2764 | `				return SXERR_ABORT;` |
|       - | 2765 | `			}` |
|       3 | 2766 | `			goto Synchronize;` |
|       - | 2767 | `	}` |
|       - | 2768 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2769 | `	pGen->pIn++;` |
|     ! 0 | 2770 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2771 | `		/* Syntax error */` |
|     ! 0 | 2772 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2773 | `		if( rc == SXERR_ABORT ){` |
|       - | 2774 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2775 | `			return SXERR_ABORT;` |
|       - | 2776 | `		}` |
|     ! 0 | 2777 | `		goto Synchronize;` |
|       - | 2778 | `	}` |
|       - | 2779 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2780 | `	pGen->pIn++;` |
|       - | 2781 | `	/* Delimit the condition */` |
|     ! 0 | 2782 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2783 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2784 | `		/* Empty expression */` |
|     ! 0 | 2785 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2786 | `		if( rc == SXERR_ABORT ){` |
|       - | 2787 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2788 | `			return SXERR_ABORT;` |
|       - | 2789 | `		}` |
|     ! 0 | 2790 | `		goto Synchronize;` |
|       - | 2791 | `	}` |
|       - | 2792 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2793 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2794 | `		JumpFixup *aPost;` |
|       - | 2795 | `		VmInstr *pInstr;` |
|       - | 2796 | `		sxu32 nJumpDest;` |
|       - | 2797 | `		sxu32 n;` |
|     ! 0 | 2798 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2799 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2800 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2801 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2802 | `			if( pInstr ){` |
|       - | 2803 | `				/* Fix */` |
|     ! 0 | 2804 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2805 | `			}` |
|     ! 0 | 2806 | `		}` |
|     ! 0 | 2807 | `	}` |
|       - | 2808 | `	/* Swap token streams */` |
|     ! 0 | 2809 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2810 | `	pGen->pEnd = pEnd;` |
|       - | 2811 | `	/* Compile the expression */` |
|     ! 0 | 2812 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2813 | `	if( rc == SXERR_ABORT ){` |
|       - | 2814 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2815 | `		return SXERR_ABORT;` |
|       - | 2816 | `	}` |
|       - | 2817 | `	/* Update token stream */` |
|     ! 0 | 2818 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2819 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2820 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2821 | `			return SXERR_ABORT;` |
|       - | 2822 | `		}` |
|     ! 0 | 2823 | `		pGen->pIn++;` |
|     ! 0 | 2824 | `	}` |
|     ! 0 | 2825 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2826 | `	pGen->pEnd = pTmp;` |
|       - | 2827 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2828 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2829 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2830 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2831 | `	/* Release the loop block */` |
|     ! 0 | 2832 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2833 | `	/* Statement successfully compiled */` |
|     ! 0 | 2834 | `	return SXRET_OK;` |
|       1 | 2835 | `Synchronize:` |
|       - | 2836 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2837 | `	 * compiling this erroneous block.` |
|       - | 2838 | `	 */` |
|       3 | 2839 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2840 | `		pGen->pIn++;` |
|     ! 0 | 2841 | `	}` |
|       3 | 2842 | `	return SXRET_OK;` |
|       2 | 2843 |  |
|       - | 2844 | `/*` |
|       - | 2845 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2846 | ` * According to the PHP language reference` |
|       - | 2847 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2848 | ` *  The syntax of a for loop is:` |
|       - | 2849 | ` *  for (expr1; expr2; expr3)` |
|       - | 2850 | ` *   statement` |
|       - | 2851 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2852 | ` *  the beginning of the loop.` |
|       - | 2853 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2854 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2855 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2856 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2857 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2858 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2859 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2860 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2861 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2862 | ` *  of using the for truth expression.` |
|       - | 2863 | ` */` |
|   10722 | 2864 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2865 |  |
|   10724 | 2866 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10724 | 2867 | `	GenBlock *pForBlock = 0;` |
|       - | 2868 | `	sxu32 nFalseJump;` |
|       - | 2869 | `	sxu32 nLine;` |
|       - | 2870 | `	sxi32 rc;` |
|   10724 | 2871 | `	nLine = pGen->pIn->nLine;` |
|       - | 2872 | `	/* Jump the 'for' keyword */` |
|   10724 | 2873 | `	pGen->pIn++;` |
|   10724 | 2874 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2875 | `		/* Syntax error */` |
|     ! 0 | 2876 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2877 | `		if( rc == SXERR_ABORT ){` |
|       - | 2878 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2879 | `			return SXERR_ABORT;` |
|       - | 2880 | `		}` |
|     ! 0 | 2881 | `		return SXRET_OK;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Jump the left parenthesis '(' */` |
|   10724 | 2884 | `	pGen->pIn++;` |
|       - | 2885 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10724 | 2886 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10724 | 2887 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2888 | `		/* Empty expression */` |
|     ! 0 | 2889 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2890 | `		if( rc == SXERR_ABORT ){` |
|       - | 2891 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2892 | `			return SXERR_ABORT;` |
|       - | 2893 | `		}` |
|       - | 2894 | `		/* Synchronize */` |
|     ! 0 | 2895 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2896 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2897 | `			pGen->pIn++;` |
|     ! 0 | 2898 | `		}` |
|     ! 0 | 2899 | `		return SXRET_OK;` |
|       - | 2900 | `	}` |
|       - | 2901 | `	/* Swap token streams */` |
|   10724 | 2902 | `	pTmp = pGen->pEnd;` |
|   10724 | 2903 | `	pGen->pEnd = pEnd;` |
|       - | 2904 | `	/* Compile initialization expressions if available */` |
|   10724 | 2905 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2906 | `	/* Pop operand lvalues */` |
|   10724 | 2907 | `	if( rc == SXERR_ABORT ){` |
|       - | 2908 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2909 | `		return SXERR_ABORT;` |
|   10724 | 2910 | `	}else if( rc != SXERR_EMPTY ){` |
|   10722 | 2911 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5360 | 2912 | `	}` |
|   10724 | 2913 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2914 | `		/* Syntax error */` |
|     ! 0 | 2915 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2916 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2917 | `		if( rc == SXERR_ABORT ){` |
|       - | 2918 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2919 | `			return SXERR_ABORT;` |
|       - | 2920 | `		}` |
|     ! 0 | 2921 | `		return SXRET_OK;` |
|       - | 2922 | `	}` |
|       - | 2923 | `	/* Jump the trailing ';' */` |
|   10724 | 2924 | `	pGen->pIn++;` |
|       - | 2925 | `	/* Create the loop block */` |
|   10724 | 2926 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10724 | 2927 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2928 | `		return SXERR_ABORT;` |
|       - | 2929 | `	}` |
|       - | 2930 | `	/* Deffer continue jumps */` |
|   10724 | 2931 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2932 | `	/* Compile the condition */` |
|   10724 | 2933 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10724 | 2934 | `	if( rc == SXERR_ABORT ){` |
|       - | 2935 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2936 | `		return SXERR_ABORT;` |
|   10724 | 2937 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2938 | `		/* Emit the false jump */` |
|   10722 | 2939 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2940 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10722 | 2941 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5360 | 2942 | `	}` |
|   10724 | 2943 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2944 | `		/* Syntax error */` |
|       5 | 2945 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2946 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2947 | `		if( rc == SXERR_ABORT ){` |
|       - | 2948 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2949 | `			return SXERR_ABORT;` |
|       - | 2950 | `		}` |
|       5 | 2951 | `		return SXRET_OK;` |
|       - | 2952 | `	}` |
|       - | 2953 | `	/* Jump the trailing ';' */` |
|   10720 | 2954 | `	pGen->pIn++;` |
|       - | 2955 | `	/* Save the post condition stream */` |
|   10720 | 2956 | `	pPostStart = pGen->pIn;` |
|       - | 2957 | `	/* Compile the loop body */` |
|   10720 | 2958 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10720 | 2959 | `	pGen->pEnd = pTmp;` |
|   10720 | 2960 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10720 | 2961 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2962 | `		return SXERR_ABORT;` |
|       - | 2963 | `	}` |
|       - | 2964 | `	/* Fix post-continue jumps */` |
|   10720 | 2965 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2966 | `		JumpFixup *aPost;` |
|       - | 2967 | `		VmInstr *pInstr;` |
|       - | 2968 | `		sxu32 nJumpDest;` |
|       - | 2969 | `		sxu32 n;` |
|      14 | 2970 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 | 2971 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 | 2972 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 | 2973 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 | 2974 | `			if( pInstr ){` |
|       - | 2975 | `				/* Fix jump */` |
|      14 | 2976 | `				pInstr->iP2 = nJumpDest;` |
|       6 | 2977 | `			}` |
|       8 | 2978 | `		}` |
|       6 | 2979 | `	}` |
|       - | 2980 | `	/* compile the post-expressions if available */` |
|   10720 | 2981 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2982 | `		pPostStart++;` |
|     ! 0 | 2983 | `	}` |
|   10720 | 2984 | `	if( pPostStart < pEnd ){` |
|       - | 2985 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10720 | 2986 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10720 | 2987 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10720 | 2988 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2989 | `			/* Syntax error */` |
|     ! 0 | 2990 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2991 | `			if( rc == SXERR_ABORT ){` |
|       - | 2992 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2993 | `				return SXERR_ABORT;` |
|       - | 2994 | `			}` |
|     ! 0 | 2995 | `			return SXRET_OK;` |
|       - | 2996 | `		}` |
|   10720 | 2997 | `		RE_SWAP_DELIMITER(pGen);` |
|   10720 | 2998 | `		if( rc == SXERR_ABORT ){` |
|       - | 2999 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3000 | `			return SXERR_ABORT;` |
|   10720 | 3001 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 3002 | `			/* Pop operand lvalue */` |
|   10720 | 3003 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5359 | 3004 | `		}` |
|    5359 | 3005 | `	}` |
|       - | 3006 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10720 | 3007 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 3008 | `	/* Fix all jumps now the destination is resolved */` |
|   10720 | 3009 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3010 | `	/* Release the loop block */` |
|   10720 | 3011 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3012 | `	/* Statement successfully compiled */` |
|   10720 | 3013 | `	return SXRET_OK;` |
|    5363 | 3014 |  |
|       - | 3015 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 3016 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 3017 | ` * are allowed.` |
|       - | 3018 | ` */` |
|    5710 | 3019 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 3020 |  |
|    5712 | 3021 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5712 | 3022 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 3023 | `		/* Unexpected expression */` |
|     ! 0 | 3024 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 3025 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 3026 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 3027 | `			rc = SXERR_INVALID;` |
|     ! 0 | 3028 | `		}` |
|     ! 0 | 3029 | `	}` |
|    5712 | 3030 | `	return rc;` |
|       2 | 3031 |  |
|       - | 3032 | `/*` |
|       - | 3033 | ` * Compile the 'foreach' statement.` |
|       - | 3034 | ` * According to the PHP language reference` |
|       - | 3035 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 3036 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 3037 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 3038 | ` *  is a minor but useful extension of the first:` |
|       - | 3039 | ` *  foreach (array_expression as $value)` |
|       - | 3040 | ` *    statement` |
|       - | 3041 | ` *  foreach (array_expression as $key => $value)` |
|       - | 3042 | ` *   statement` |
|       - | 3043 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 3044 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 3045 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 3046 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 3047 | ` *  to the variable $key on each loop.` |
|       - | 3048 | ` *  Note:` |
|       - | 3049 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 3050 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 3051 | ` *  Note:` |
|       - | 3052 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 3053 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 3054 | ` *  or after the foreach without resetting it.` |
|       - | 3055 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 3056 | ` *  of copying the value.` |
|       - | 3057 | ` */` |
|    2906 | 3058 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 3059 |  |
|    2908 | 3060 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2908 | 3061 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2908 | 3062 | `	GenBlock *pForeachBlock = 0;` |
|       - | 3063 | `	ph7_foreach_info *pInfo;` |
|       - | 3064 | `	sxu32 nFalseJump;` |
|       - | 3065 | `	VmInstr *pInstr;` |
|       - | 3066 | `	sxu32 nLine;` |
|       - | 3067 | `	sxi32 rc;` |
|    2908 | 3068 | `	nLine = pGen->pIn->nLine;` |
|       - | 3069 | `	/* Jump the 'foreach' keyword */` |
|    2908 | 3070 | `	pGen->pIn++;` |
|    2908 | 3071 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3072 | `		/* Syntax error */` |
|     ! 0 | 3073 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 3074 | `		if( rc == SXERR_ABORT ){` |
|       - | 3075 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3076 | `			return SXERR_ABORT;` |
|       - | 3077 | `		}` |
|     ! 0 | 3078 | `		goto Synchronize;` |
|       - | 3079 | `	}` |
|       - | 3080 | `	/* Jump the left parenthesis '(' */` |
|    2908 | 3081 | `	pGen->pIn++;` |
|       - | 3082 | `	/* Create the loop block */` |
|    2908 | 3083 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2908 | 3084 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3085 | `		return SXERR_ABORT;` |
|       - | 3086 | `	}` |
|       - | 3087 | `	/* Delimit the expression */` |
|    2908 | 3088 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2908 | 3089 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3090 | `		/* Empty expression */` |
|     ! 0 | 3091 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 3092 | `		if( rc == SXERR_ABORT ){` |
|       - | 3093 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3094 | `			return SXERR_ABORT;` |
|       - | 3095 | `		}` |
|       - | 3096 | `		/* Synchronize */` |
|     ! 0 | 3097 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3098 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3099 | `			pGen->pIn++;` |
|     ! 0 | 3100 | `		}` |
|     ! 0 | 3101 | `		return SXRET_OK;` |
|       - | 3102 | `	}` |
|       - | 3103 | `	/* Compile the array expression */` |
|    2908 | 3104 | `	pCur = pGen->pIn;` |
|   19450 | 3105 | `	while( pCur < pEnd ){` |
|   19450 | 3106 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2918 | 3107 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2918 | 3108 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 3109 | `				/* Break with the first 'as' found */` |
|    2908 | 3110 | `				break;` |
|       - | 3111 | `			}` |
|       5 | 3112 | `		}` |
|       - | 3113 | `		/* Advance the stream cursor */` |
|   16544 | 3114 | `		pCur++;` |
|       2 | 3115 | `	}` |
|    2908 | 3116 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 3117 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 3118 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 3119 | `		if( rc == SXERR_ABORT ){` |
|       - | 3120 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3121 | `			return SXERR_ABORT;` |
|       - | 3122 | `		}` |
|     ! 0 | 3123 | `		goto Synchronize;` |
|       - | 3124 | `	}` |
|       - | 3125 | `	/* Swap token streams */` |
|    2908 | 3126 | `	pTmp = pGen->pEnd;` |
|    2908 | 3127 | `	pGen->pEnd = pCur;` |
|    2908 | 3128 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2908 | 3129 | `	if( rc == SXERR_ABORT ){` |
|       - | 3130 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3131 | `		return SXERR_ABORT;` |
|       - | 3132 | `	}` |
|       - | 3133 | `	/* Update token stream */` |
|    2908 | 3134 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 3135 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3136 | `		if( rc == SXERR_ABORT ){` |
|       - | 3137 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3138 | `			return SXERR_ABORT;` |
|       - | 3139 | `		}` |
|     ! 0 | 3140 | `		pGen->pIn++;` |
|     ! 0 | 3141 | `	}` |
|    2908 | 3142 | `	pCur++; /* Jump the 'as' keyword */` |
|    2908 | 3143 | `	pGen->pIn = pCur;` |
|    2908 | 3144 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3145 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 3146 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3147 | `			return SXERR_ABORT;` |
|       - | 3148 | `		}` |
|     ! 0 | 3149 | `	}` |
|       - | 3150 | `	/* Create the foreach context */` |
|    2908 | 3151 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2908 | 3152 | `	if( pInfo == 0 ){` |
|     ! 0 | 3153 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3154 | `		return SXERR_ABORT;` |
|       - | 3155 | `	}` |
|       - | 3156 | `	/* Zero the structure */` |
|    2908 | 3157 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 3158 | `	/* Initialize structure fields */` |
|    2908 | 3159 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 3160 | `	/* Check if we have a key field */` |
|    8770 | 3161 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5864 | 3162 | `		pCur++;` |
|       2 | 3163 | `	}` |
|    2908 | 3164 | `	if( pCur < pEnd ){` |
|       - | 3165 | `		/* Compile the expression holding the key name */` |
|    2816 | 3166 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 3167 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 3168 | `			if( rc == SXERR_ABORT ){` |
|       - | 3169 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3170 | `				return SXERR_ABORT;` |
|       - | 3171 | `			}` |
|     ! 0 | 3172 | `		}else{` |
|    2816 | 3173 | `			pGen->pEnd = pCur;` |
|    2816 | 3174 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2816 | 3175 | `			if( rc == SXERR_ABORT ){` |
|       - | 3176 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3177 | `				return SXERR_ABORT;` |
|       - | 3178 | `			}` |
|    2816 | 3179 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2816 | 3180 | `			if( pInstr->p3 ){` |
|       - | 3181 | `				/* Record key name */` |
|    2816 | 3182 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1407 | 3183 | `			}` |
|    2816 | 3184 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 3185 | `		}` |
|    2816 | 3186 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1407 | 3187 | `	}` |
|    2908 | 3188 | `	pGen->pEnd = pEnd;` |
|    2908 | 3189 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3190 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 3191 | `		if( rc == SXERR_ABORT ){` |
|       - | 3192 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3193 | `			return SXERR_ABORT;` |
|       - | 3194 | `		}` |
|     ! 0 | 3195 | `		goto Synchronize;` |
|       - | 3196 | `	}` |
|    2908 | 3197 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3198 | `		pGen->pIn++;` |
|       - | 3199 | `		/* Pass by reference  */` |
|      11 | 3200 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3201 | `	}` |
|       - | 3202 | `	/* Check if the value target is list() */` |
|    2908 | 3203 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3204 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3205 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3206 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3207 | `		 */` |
|       - | 3208 | `		static int iForeachListCnt = 0;` |
|       - | 3209 | `		char zTmp[128];` |
|       - | 3210 | `		sxu32 nLen;` |
|       - | 3211 | `		char *zDup;` |
|      10 | 3212 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3213 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3214 | `		if( zDup == 0 ){` |
|     ! 0 | 3215 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3216 | `			return SXERR_ABORT;` |
|       - | 3217 | `		}` |
|      10 | 3218 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3219 | `		/* Save list() token boundaries */` |
|      10 | 3220 | `		pListStart = pGen->pIn;` |
|       - | 3221 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3222 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3223 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3224 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3225 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3226 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3227 | `				return SXERR_ABORT;` |
|       - | 3228 | `			}` |
|       3 | 3229 | `			goto Synchronize;` |
|       - | 3230 | `		}` |
|       7 | 3231 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3232 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3233 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3234 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3235 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3236 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3237 | `				return SXERR_ABORT;` |
|       - | 3238 | `			}` |
|     ! 0 | 3239 | `			goto Synchronize;` |
|       - | 3240 | `		}` |
|       7 | 3241 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3242 | `		pListEnd = pGen->pIn;` |
|       7 | 3243 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2903 | 3244 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3245 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3246 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3247 | `		 */` |
|       - | 3248 | `		static int iForeachShortListCnt = 0;` |
|       - | 3249 | `		char zTmp[128];` |
|       - | 3250 | `		sxu32 nLen;` |
|       - | 3251 | `		char *zDup;` |
|       3 | 3252 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3253 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3254 | `		if( zDup == 0 ){` |
|     ! 0 | 3255 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3256 | `			return SXERR_ABORT;` |
|       - | 3257 | `		}` |
|       3 | 3258 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3259 | `		/* Save [...] token boundaries */` |
|       3 | 3260 | `		pListStart = pGen->pIn;` |
|       - | 3261 | `		/* Advance past [...] */` |
|       3 | 3262 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3263 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3264 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3265 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3266 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3267 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3268 | `				return SXERR_ABORT;` |
|       - | 3269 | `			}` |
|     ! 0 | 3270 | `			goto Synchronize;` |
|       - | 3271 | `		}` |
|       3 | 3272 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3273 | `		pListEnd = pGen->pIn;` |
|       3 | 3274 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3275 | `	}else{` |
|       - | 3276 | `		/* Compile the expression holding the value name */` |
|    2898 | 3277 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2898 | 3278 | `		if( rc == SXERR_ABORT ){` |
|       - | 3279 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3280 | `			return SXERR_ABORT;` |
|       - | 3281 | `		}` |
|    2898 | 3282 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2898 | 3283 | `		if( pInstr->p3 ){` |
|       - | 3284 | `			/* Record value name */` |
|    2898 | 3285 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1448 | 3286 | `		}` |
|       - | 3287 | `	}` |
|       - | 3288 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2906 | 3289 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3290 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2906 | 3291 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3292 | `	/* Record the first instruction to execute */` |
|    2906 | 3293 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3294 | `	/* Emit the FOREACH_STEP instruction */` |
|    2906 | 3295 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3296 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2906 | 3297 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3298 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2906 | 3299 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3300 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3301 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3302 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3303 | `		 */` |
|       9 | 3304 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3305 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3306 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3307 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3308 | `		 */` |
|       9 | 3309 | `		pSavedIn = pGen->pIn;` |
|       9 | 3310 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3311 | `		pGen->pIn = pListStart;` |
|       9 | 3312 | `		pGen->pEnd = pListEnd;` |
|       9 | 3313 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3314 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3315 | `		}else{` |
|       7 | 3316 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3317 | `		}` |
|       9 | 3318 | `		pGen->pIn = pSavedIn;` |
|       9 | 3319 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3320 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3321 | `			return SXERR_ABORT;` |
|       - | 3322 | `		}` |
|       - | 3323 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3324 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3325 | `	}` |
|       - | 3326 | `	/* Compile the loop body */` |
|    2906 | 3327 | `	pGen->pIn = &pEnd[1];` |
|    2906 | 3328 | `	pGen->pEnd = pTmp;` |
|    2906 | 3329 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2906 | 3330 | `	if( rc == SXERR_ABORT ){` |
|       - | 3331 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3332 | `		return SXERR_ABORT;` |
|       - | 3333 | `	}` |
|       - | 3334 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2906 | 3335 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3336 | `	/* Fix all jumps now the destination is resolved */` |
|    2906 | 3337 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3338 | `	/* Release the loop block */` |
|    2906 | 3339 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3340 | `	/* Statement successfully compiled */` |
|    2906 | 3341 | `	return SXRET_OK;` |
|       1 | 3342 | `Synchronize:` |
|       - | 3343 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3344 | `	 * compiling this erroneous block.` |
|       - | 3345 | `	 */` |
|       3 | 3346 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3347 | `		pGen->pIn++;` |
|     ! 0 | 3348 | `	}` |
|       3 | 3349 | `	return SXRET_OK;` |
|    1455 | 3350 |  |
|       - | 3351 | `/*` |
|       - | 3352 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3353 | ` * According to the PHP language reference` |
|       - | 3354 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3355 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3356 | ` *  that is similar to that of C:` |
|       - | 3357 | ` *  if (expr)` |
|       - | 3358 | ` *   statement` |
|       - | 3359 | ` *  else construct:` |
|       - | 3360 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3361 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3362 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3363 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3364 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3365 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3366 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3367 | ` *  elseif` |
|       - | 3368 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3369 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3370 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3371 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3372 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3373 | ` *   <?php` |
|       - | 3374 | ` *    if ($a > $b) {` |
|       - | 3375 | ` *     echo "a is bigger than b";` |
|       - | 3376 | ` *    } elseif ($a == $b) {` |
|       - | 3377 | ` *     echo "a is equal to b";` |
|       - | 3378 | ` *    } else {` |
|       - | 3379 | ` *     echo "a is smaller than b";` |
|       - | 3380 | ` *    }` |
|       - | 3381 | ` *    ?>` |
|       - | 3382 | ` */` |
|  106624 | 3383 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3384 |  |
|  106626 | 3385 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  106626 | 3386 | `	GenBlock *pCondBlock = 0;` |
|       - | 3387 | `	sxu32 nJumpIdx;` |
|       - | 3388 | `	sxu32 nKeyID;` |
|       - | 3389 | `	sxi32 rc;` |
|       - | 3390 | `	/* Jump the 'if' keyword */` |
|  106626 | 3391 | `	pGen->pIn++;` |
|  106626 | 3392 | `	pToken = pGen->pIn;` |
|       - | 3393 | `	/* Create the conditional block */` |
|  106626 | 3394 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  106626 | 3395 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3396 | `		return SXERR_ABORT;` |
|       - | 3397 | `	}` |
|       - | 3398 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   58636 | 3399 | `	for(;;){` |
|  117274 | 3400 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3401 | `			/* Syntax error */` |
|     ! 0 | 3402 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3403 | `				pToken--;` |
|     ! 0 | 3404 | `			}` |
|     ! 0 | 3405 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3406 | `			if( rc == SXERR_ABORT ){` |
|       - | 3407 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3408 | `				return SXERR_ABORT;` |
|       - | 3409 | `			}` |
|     ! 0 | 3410 | `			goto Synchronize;` |
|       - | 3411 | `		}` |
|       - | 3412 | `		/* Jump the left parenthesis '(' */` |
|  117274 | 3413 | `		pToken++;` |
|       - | 3414 | `		/* Delimit the condition */` |
|  117274 | 3415 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  117274 | 3416 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3417 | `			/* Syntax error */` |
|     ! 0 | 3418 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3419 | `				pToken--;` |
|     ! 0 | 3420 | `			}` |
|     ! 0 | 3421 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3422 | `			if( rc == SXERR_ABORT ){` |
|       - | 3423 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3424 | `				return SXERR_ABORT;` |
|       - | 3425 | `			}` |
|     ! 0 | 3426 | `			goto Synchronize;` |
|       - | 3427 | `		}` |
|       - | 3428 | `		/* Swap token streams */` |
|  117274 | 3429 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3430 | `		/* Compile the condition */` |
|  117274 | 3431 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3432 | `		/* Update token stream */` |
|  117274 | 3433 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3434 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3435 | `			pGen->pIn++;` |
|     ! 0 | 3436 | `		}` |
|  117274 | 3437 | `		pGen->pIn  = &pEnd[1];` |
|  117274 | 3438 | `		pGen->pEnd = pTmp;` |
|  117274 | 3439 | `		if( rc == SXERR_ABORT ){` |
|       - | 3440 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3441 | `			return SXERR_ABORT;` |
|       - | 3442 | `		}` |
|       - | 3443 | `		/* Emit the false jump */` |
|  117274 | 3444 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3445 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  117274 | 3446 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3447 | `		/* Compile the body */` |
|  117274 | 3448 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  117274 | 3449 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3450 | `			return SXERR_ABORT;` |
|       - | 3451 | `		}` |
|  117274 | 3452 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   31565 | 3453 | `			break;` |
|       - | 3454 | `		}` |
|       - | 3455 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   54148 | 3456 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54148 | 3457 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   34810 | 3458 | `			break;` |
|       - | 3459 | `		}` |
|       - | 3460 | `		/* Emit the unconditional jump */` |
|   19340 | 3461 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3462 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19340 | 3463 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19340 | 3464 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14004 | 3465 | `			pToken = &pGen->pIn[1];` |
|   14004 | 3466 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5342 | 3467 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4347 | 3468 | `					break;` |
|       - | 3469 | `			}` |
|    5314 | 3470 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2656 | 3471 | `		}` |
|   10650 | 3472 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3473 | `		/* Synchronize cursors */` |
|   10650 | 3474 | `		pToken = pGen->pIn;` |
|       - | 3475 | `		/* Fix the false jump */` |
|   10650 | 3476 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3477 | `	} /* For(;;) */` |
|       - | 3478 | `	/* Fix the false jump */` |
|  106626 | 3479 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  106626 | 3480 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   43498 | 3481 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3482 | `			/* Compile the else block */` |
|    8692 | 3483 | `			pGen->pIn++;` |
|    8692 | 3484 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8692 | 3485 | `			if( rc == SXERR_ABORT ){` |
|       - | 3486 |  |
|     ! 0 | 3487 | `				return SXERR_ABORT;` |
|       - | 3488 | `			}` |
|    4345 | 3489 | `	}` |
|  106626 | 3490 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3491 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  106626 | 3492 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3493 | `	/* Release the conditional block */` |
|  106626 | 3494 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3495 | `	/* Statement successfully compiled */` |
|  106626 | 3496 | `	return SXRET_OK;` |
|     ! 0 | 3497 | `Synchronize:` |
|       - | 3498 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3499 | `	 */` |
|     ! 0 | 3500 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3501 | `		pGen->pIn++;` |
|     ! 0 | 3502 | `	}` |
|     ! 0 | 3503 | `	return SXRET_OK;` |
|   53314 | 3504 |  |
|       - | 3505 | `/*` |
|       - | 3506 | ` * Compile the global construct.` |
|       - | 3507 | ` * According to the PHP language reference` |
|       - | 3508 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3509 | ` *  to be used in that function.` |
|       - | 3510 | ` *  Example #1 Using global` |
|       - | 3511 | ` *  <?php` |
|       - | 3512 | ` *   $a = 1;` |
|       - | 3513 | ` *   $b = 2;` |
|       - | 3514 | ` *   function Sum()` |
|       - | 3515 | ` *   {` |
|       - | 3516 | ` *    global $a, $b;` |
|       - | 3517 | ` *    $b = $a + $b;` |
|       - | 3518 | ` *   }` |
|       - | 3519 | ` *   Sum();` |
|       - | 3520 | ` *   echo $b;` |
|       - | 3521 | ` *  ?>` |
|       - | 3522 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3523 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3524 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3525 | ` */` |
|      26 | 3526 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3527 |  |
|      28 | 3528 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3529 | `	sxi32 nExpr;` |
|       - | 3530 | `	sxi32 rc;` |
|       - | 3531 | `	/* Jump the 'global' keyword */` |
|      28 | 3532 | `	pGen->pIn++;` |
|      28 | 3533 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3534 | `		/* Nothing to process */` |
|     ! 0 | 3535 | `		return SXRET_OK;` |
|       - | 3536 | `	}` |
|      28 | 3537 | `	pTmp = pGen->pEnd;` |
|      28 | 3538 | `	nExpr = 0;` |
|      56 | 3539 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3540 | `		if( pGen->pIn < pNext ){` |
|      30 | 3541 | `			pGen->pEnd = pNext;` |
|      30 | 3542 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3543 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3544 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3545 | `					return SXERR_ABORT;` |
|       - | 3546 | `				}` |
|     ! 0 | 3547 | `			}else{` |
|      30 | 3548 | `				pGen->pIn++;` |
|      30 | 3549 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3550 | `					/* Emit a warning */` |
|     ! 0 | 3551 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3552 | `				}else{` |
|      30 | 3553 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3554 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3555 | `						return SXERR_ABORT;` |
|      30 | 3556 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3557 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3558 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3559 | `							/* Variable name, not a constant */` |
|      30 | 3560 | `							pLast->iP1 = 0;` |
|      14 | 3561 | `						}` |
|      30 | 3562 | `						nExpr++;` |
|      14 | 3563 | `					}` |
|       - | 3564 | `				}` |
|       - | 3565 | `			}` |
|      14 | 3566 | `		}` |
|       - | 3567 | `		/* Next expression in the stream */` |
|      30 | 3568 | `		pGen->pIn = pNext;` |
|       - | 3569 | `		/* Jump trailing commas */` |
|      32 | 3570 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3571 | `			pGen->pIn++;` |
|       1 | 3572 | `		}` |
|       2 | 3573 | `	}` |
|       - | 3574 | `	/* Restore token stream */` |
|      28 | 3575 | `	pGen->pEnd = pTmp;` |
|      28 | 3576 | `	if( nExpr > 0 ){` |
|       - | 3577 | `		/* Emit the uplink instruction */` |
|      28 | 3578 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3579 | `	}` |
|      28 | 3580 | `	return SXRET_OK;` |
|      15 | 3581 |  |
|       - | 3582 | `/*` |
|       - | 3583 | ` * Compile the return statement.` |
|       - | 3584 | ` * According to the PHP language reference` |
|       - | 3585 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3586 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3587 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3588 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3589 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3590 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3591 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3592 | ` *  from within the main script file, then script execution end.` |
|       - | 3593 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3594 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3595 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3596 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3597 | ` */` |
|  154732 | 3598 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3599 |  |
|  154734 | 3600 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3601 | `	sxi32 rc;` |
|       - | 3602 | `	/* Jump the 'return' keyword */` |
|  154734 | 3603 | `	pGen->pIn++;` |
|  154734 | 3604 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3605 | `		/* Compile the expression */` |
|  154712 | 3606 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  154712 | 3607 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3608 | `			return SXERR_ABORT;` |
|  154712 | 3609 | `		}else if(rc != SXERR_EMPTY ){` |
|  154712 | 3610 | `			nRet = 1;` |
|   77355 | 3611 | `		}` |
|   77355 | 3612 | `	}` |
|       - | 3613 | `	/* Emit the done instruction */` |
|  154734 | 3614 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  154734 | 3615 | `	return SXRET_OK;` |
|   77368 | 3616 |  |
|       - | 3617 | `/*` |
|       - | 3618 | ` * Compile a yield expression.` |
|       - | 3619 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3620 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3621 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3622 | ` */` |
|      32 | 3623 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3624 |  |
|       - | 3625 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3626 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3627 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3628 | `	sxi32 rc;` |
|      16 | 3629 | `	(void)iCompileFlag;` |
|       - | 3630 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3631 | `	pGen->pIn++;` |
|       - | 3632 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3633 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3634 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3635 | `		/* Bare yield — no value */` |
|     ! 0 | 3636 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3637 | `		return SXRET_OK;` |
|       - | 3638 | `	}` |
|       - | 3639 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3640 | `	pSplit = 0;` |
|       - | 3641 | `	{` |
|      34 | 3642 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3643 | `		sxi32 nNest = 0;` |
|      78 | 3644 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3645 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3646 | `				nNest++;` |
|      52 | 3647 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3648 | `				nNest--;` |
|      52 | 3649 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3650 | `				pSplit = pCur;` |
|       7 | 3651 | `				break;` |
|       - | 3652 | `			}` |
|      46 | 3653 | `			pCur++;` |
|       2 | 3654 | `		}` |
|       - | 3655 | `	}` |
|      34 | 3656 | `	pTmp = pGen->pEnd;` |
|      34 | 3657 | `	if( pSplit ){` |
|       - | 3658 | `		/* yield $key => $value */` |
|       7 | 3659 | `		pGen->pEnd = pSplit;` |
|       7 | 3660 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3661 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3662 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3663 | `		pGen->pEnd = pTmp;` |
|       7 | 3664 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3665 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3666 | `		iP1 = 1;` |
|       7 | 3667 | `		iP2 = 1;` |
|       4 | 3668 | `	}else{` |
|       - | 3669 | `		/* yield $value */` |
|      28 | 3670 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3671 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3672 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3673 | `			iP1 = 1;` |
|      13 | 3674 | `		}` |
|       - | 3675 | `	}` |
|      34 | 3676 | `	pGen->pEnd = pTmp;` |
|      34 | 3677 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3678 | `	return SXRET_OK;` |
|      18 | 3679 |  |
|       - | 3680 | `/*` |
|       - | 3681 | ` * Compile the die/exit language construct.` |
|       - | 3682 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3683 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3684 | ` */` |
|      88 | 3685 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3686 |  |
|      90 | 3687 | `	sxi32 nExpr = 0;` |
|       - | 3688 | `	sxi32 rc;` |
|       - | 3689 | `	/* Jump the die/exit keyword */` |
|      90 | 3690 | `	pGen->pIn++;` |
|      90 | 3691 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3692 | `		/* Compile the expression */` |
|      90 | 3693 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3694 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3695 | `			return SXERR_ABORT;` |
|      90 | 3696 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3697 | `			nExpr = 1;` |
|      44 | 3698 | `		}` |
|      44 | 3699 | `	}` |
|       - | 3700 | `	/* Emit the HALT instruction */` |
|      90 | 3701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3702 | `	return SXRET_OK;` |
|      46 | 3703 |  |
|       - | 3704 | `/*` |
|       - | 3705 | ` * Compile the 'echo' language construct.` |
|       - | 3706 | ` */` |
|   11038 | 3707 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3708 |  |
|   11040 | 3709 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3710 | `	sxi32 rc;` |
|       - | 3711 | `	/* Jump the 'echo' keyword */` |
|   11040 | 3712 | `	pGen->pIn++;` |
|       - | 3713 | `	/* Compile arguments one after one */` |
|   11040 | 3714 | `	pTmp = pGen->pEnd;` |
|   22466 | 3715 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11428 | 3716 | `		if( pGen->pIn < pNext ){` |
|   11428 | 3717 | `			pGen->pEnd = pNext;` |
|   11428 | 3718 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11428 | 3719 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3720 | `				return SXERR_ABORT;` |
|   11428 | 3721 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3722 | `				/* Emit the consume instruction */` |
|   11404 | 3723 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5701 | 3724 | `			}` |
|    5713 | 3725 | `		}` |
|       - | 3726 | `		/* Jump trailing commas */` |
|   11816 | 3727 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3728 | `			pNext++;` |
|       2 | 3729 | `		}` |
|   11428 | 3730 | `		pGen->pIn = pNext;` |
|       2 | 3731 | `	}` |
|       - | 3732 | `	/* Restore token stream */` |
|   11040 | 3733 | `	pGen->pEnd = pTmp;` |
|   11040 | 3734 | `	return SXRET_OK;` |
|    5521 | 3735 |  |
|       - | 3736 | `/*` |
|       - | 3737 | ` * Compile the static statement.` |
|       - | 3738 | ` * According to the PHP language reference` |
|       - | 3739 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3740 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3741 | ` *  when program execution leaves this scope.` |
|       - | 3742 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3743 | ` * Symisc eXtension.` |
|       - | 3744 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3745 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3746 | ` *  Example` |
|       - | 3747 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3748 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3749 | ` */` |
|       2 | 3750 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3751 |  |
|       - | 3752 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3753 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3754 | `	GenBlock *pBlock;` |
|       - | 3755 | `	SyString *pName;` |
|       - | 3756 | `	char *zDup;` |
|       - | 3757 | `	sxu32 nLine;` |
|       - | 3758 | `	sxi32 rc;` |
|       - | 3759 | `	/* Jump the static keyword */` |
|       3 | 3760 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3761 | `	pGen->pIn++;` |
|       - | 3762 | `	/* Extract the enclosing function if any */` |
|       3 | 3763 | `	pBlock = pGen->pCurrent;` |
|       5 | 3764 | `	while( pBlock ){` |
|       5 | 3765 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3766 | `			break;` |
|       - | 3767 | `		}` |
|       - | 3768 | `		/* Point to the upper block */` |
|       3 | 3769 | `		pBlock = pBlock->pParent;` |
|       1 | 3770 | `	}` |
|       3 | 3771 | `	if( pBlock == 0 ){` |
|       - | 3772 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3773 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3774 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3775 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3776 | `				return SXERR_ABORT;` |
|       - | 3777 | `			}` |
|     ! 0 | 3778 | `			goto Synchronize;` |
|       - | 3779 | `		}` |
|       - | 3780 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3781 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3783 | `			return SXERR_ABORT;` |
|     ! 0 | 3784 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3785 | `			/* Emit the POP instruction */` |
|     ! 0 | 3786 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3787 | `		}` |
|     ! 0 | 3788 | `		return SXRET_OK;` |
|       - | 3789 | `	}` |
|       3 | 3790 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3791 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3792 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3793 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3794 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3795 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3796 | `				return SXERR_ABORT;` |
|       - | 3797 | `			}` |
|       3 | 3798 | `			goto Synchronize;` |
|       - | 3799 | `	}` |
|     ! 0 | 3800 | `	pGen->pIn++;` |
|       - | 3801 | `	/* Extract variable name */` |
|     ! 0 | 3802 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3803 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3804 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3805 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3806 | `		goto Synchronize;` |
|       - | 3807 | `	}` |
|       - | 3808 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3809 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3810 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3811 | `	/* Duplicate variable name */` |
|     ! 0 | 3812 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3813 | `	if( zDup == 0 ){` |
|     ! 0 | 3814 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3815 | `		return SXERR_ABORT;` |
|       - | 3816 | `	}` |
|     ! 0 | 3817 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3818 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3819 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3820 | `		SySet *pInstrContainer;` |
|       - | 3821 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3822 | `		 * Static variable can take any complex expression including function` |
|       - | 3823 | `		 * call as their initialization value.` |
|       - | 3824 | `		 * Example:` |
|       - | 3825 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3826 | `		 */` |
|     ! 0 | 3827 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3828 | `		/* Swap bytecode container */` |
|     ! 0 | 3829 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3830 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3831 | `		/* Compile the expression */` |
|     ! 0 | 3832 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3833 | `		/* Emit the done instruction */` |
|     ! 0 | 3834 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3835 | `		/* Restore default bytecode container */` |
|     ! 0 | 3836 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3837 | `	}` |
|       - | 3838 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3839 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3840 | `	return SXRET_OK;` |
|       1 | 3841 | `Synchronize:` |
|       - | 3842 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3843 | `	 * statement.` |
|       - | 3844 | `	 */` |
|       5 | 3845 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3846 | `		pGen->pIn++;` |
|       1 | 3847 | `	}` |
|       3 | 3848 | `	return SXRET_OK;` |
|       2 | 3849 |  |
|       - | 3850 | `/*` |
|       - | 3851 | ` * Compile the var statement.` |
|       - | 3852 | ` * Symisc Extension:` |
|       - | 3853 | ` *      var statement can be used outside of a class definition.` |
|       - | 3854 | ` */` |
|       4 | 3855 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3856 |  |
|       - | 3857 | `	sxu32 nLine;` |
|       - | 3858 | `	sxi32 rc;` |
|       5 | 3859 | `	nLine = pGen->pIn->nLine;` |
|       - | 3860 | `	/* Jump the 'var' keyword */` |
|       5 | 3861 | `	pGen->pIn++;` |
|       5 | 3862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3863 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3864 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3865 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3866 | `			pGen->pIn++;` |
|     ! 0 | 3867 | `		}` |
|     ! 0 | 3868 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3869 | `			return SXERR_ABORT;` |
|       - | 3870 | `		}` |
|     ! 0 | 3871 | `	}else{` |
|       - | 3872 | `		/* Compile the expression */` |
|       5 | 3873 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3874 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3875 | `			return SXERR_ABORT;` |
|       5 | 3876 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3877 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3878 | `		}` |
|       - | 3879 | `	}` |
|       5 | 3880 | `	return SXRET_OK;` |
|       3 | 3881 |  |
|       - | 3882 | `/*` |
|       - | 3883 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3884 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3885 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3886 | ` */` |
|       - | 3887 | `/*` |
|       - | 3888 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 3889 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3890 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3891 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3892 | ` *` |
|       - | 3893 | ` * Resolution order:` |
|       - | 3894 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 3895 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 3896 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 3897 | ` *` |
|       - | 3898 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 3899 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 3900 | ` * Returns the (possibly new) literal index.` |
|       - | 3901 | ` */` |
|  317700 | 3902 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 3903 |  |
|       - | 3904 | `	ph7_value *pLit;` |
|       - | 3905 | `	const char *zLit;` |
|       - | 3906 | `	SyString sQualified;` |
|       - | 3907 | `	sxu32 nLit;` |
|       - | 3908 | `	sxu32 k;` |
|       - | 3909 | `	sxu32 nNewIdx;` |
|       - | 3910 | `	int hasNsSep;` |
|       - | 3911 | `	SyHashEntry *pImport;` |
|       - | 3912 | `	ph7_value *pNew;` |
|  317702 | 3913 | `	if( pFromImport ){` |
|  303834 | 3914 | `		*pFromImport = 0;` |
|  151916 | 3915 | `	}` |
|  317702 | 3916 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  317702 | 3917 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3918 | `		return nOrigIdx;` |
|       - | 3919 | `	}` |
|  317702 | 3920 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  317702 | 3921 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3922 | `	/* Skip if already qualified (contains backslash) */` |
|  317702 | 3923 | `	hasNsSep = 0;` |
| 3417222 | 3924 | `	for( k = 0; k < nLit; k++ ){` |
| 3099554 | 3925 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1549762 | 3926 | `	}` |
|  317702 | 3927 | `	if( hasNsSep ){` |
|      34 | 3928 | `		return nOrigIdx;` |
|       - | 3929 | `	}` |
|       - | 3930 | `	/* Check use imports first (works even outside namespaces) */` |
|  317670 | 3931 | `	SyBlobReset(&pGen->sWorker);` |
|  317670 | 3932 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  317670 | 3933 | `	if( pImport ){` |
|      38 | 3934 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 3935 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 3936 | `		if( pFromImport ){` |
|      18 | 3937 | `			*pFromImport = 1;` |
|       8 | 3938 | `		}` |
|      20 | 3939 | `	}else{` |
|  317634 | 3940 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  317552 | 3941 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 3942 | `		}` |
|       - | 3943 | `		/* Prepend current namespace */` |
|      84 | 3944 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      84 | 3945 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      84 | 3946 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3947 | `	}` |
|       - | 3948 | `	/* Look up or create a new literal for the qualified name */` |
|     120 | 3949 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     120 | 3950 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      52 | 3951 | `		return nNewIdx; /* Already interned */` |
|       - | 3952 | `	}` |
|      70 | 3953 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      70 | 3954 | `	if( pNew == 0 ){` |
|     ! 0 | 3955 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3956 | `	}` |
|      70 | 3957 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      70 | 3958 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      70 | 3959 | `	return nNewIdx;` |
|  158852 | 3960 |  |
|       - | 3961 | `/*` |
|       - | 3962 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3963 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3964 | ` */` |
|   26876 | 3965 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3966 |  |
|       - | 3967 | `	SyHashEntry *pImport;` |
|       - | 3968 | `	/* Check use imports first */` |
|   26878 | 3969 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   26878 | 3970 | `	if( pImport ){` |
|      12 | 3971 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 3972 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 3973 | `		return;` |
|       - | 3974 | `	}` |
|       - | 3975 | `	/* Prepend current namespace if active */` |
|   26868 | 3976 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 3977 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 3978 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 3979 | `	}` |
|   26868 | 3980 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13440 | 3981 |  |
|       - | 3982 | `/*` |
|       - | 3983 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3984 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3985 | ` * The caller must release pOut when done.` |
|       - | 3986 | ` */` |
|   45736 | 3987 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3988 |  |
|   45738 | 3989 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 3990 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      50 | 3991 | `		SyBlobAppend(pOut,"\\",1);` |
|      24 | 3992 | `	}` |
|   45738 | 3993 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   45738 | 3994 |  |
|       - | 3995 | `/*` |
|       - | 3996 | ` * Compile a namespace statement` |
|       - | 3997 | ` * According to the PHP language reference manual` |
|       - | 3998 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3999 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 4000 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 4001 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 4002 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 4003 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 4004 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 4005 | ` *  programming world.` |
|       - | 4006 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 4007 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 4008 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 4009 | ` *  classes/functions/constants.` |
|       - | 4010 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 4011 | ` *  readability of source code.` |
|       - | 4012 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 4013 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 4014 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 4015 | ` *       class MyClass {}` |
|       - | 4016 | ` *       function myfunction() {}` |
|       - | 4017 | ` *       const MYCONST = 1;` |
|       - | 4018 | ` *       $a = new MyClass;` |
|       - | 4019 | ` *       $c = new \my\name\MyClass;` |
|       - | 4020 | ` *       $a = strlen('hi');` |
|       - | 4021 | ` *       $d = namespace\MYCONST;` |
|       - | 4022 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 4023 | ` *       echo constant($d);` |
|       - | 4024 | ` * NOTE` |
|       - | 4025 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4026 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4027 | ` */` |
|       - | 4028 | `/*` |
|       - | 4029 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 4030 | ` */` |
|      10 | 4031 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 4032 |  |
|      11 | 4033 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       5 | 4034 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       5 | 4035 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       5 | 4036 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       5 | 4037 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       5 | 4038 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 4039 | `	return "token";` |
|       6 | 4040 |  |
|      96 | 4041 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 4042 |  |
|       - | 4043 | `	sxu32 nLine;` |
|       - | 4044 | `	sxi32 rc;` |
|      98 | 4045 | `	nLine = pGen->pIn->nLine;` |
|      98 | 4046 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 4047 | `	/* Reset namespace and clear previous use imports */` |
|      98 | 4048 | `	SyBlobReset(&pGen->sNamespace);` |
|      98 | 4049 | `	SyHashRelease(&pGen->hUseImports);` |
|      98 | 4050 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4051 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      98 | 4052 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4053 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      98 | 4054 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4055 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4056 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 4057 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4058 | `		return SXRET_OK;` |
|       - | 4059 | `	}` |
|      98 | 4060 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 4061 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 4062 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4063 | `		return SXRET_OK;` |
|       - | 4064 | `	}` |
|      98 | 4065 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 4066 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 4067 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4068 | `		return SXRET_OK;` |
|       - | 4069 | `	}` |
|       - | 4070 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     232 | 4071 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     136 | 4072 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 4073 | `			/* Append backslash separator */` |
|      21 | 4074 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 4075 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 4076 | `			}` |
|      11 | 4077 | `		}else{` |
|       - | 4078 | `			/* Append identifier */` |
|     116 | 4079 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 4080 | `		}` |
|     136 | 4081 | `		pGen->pIn++;` |
|       2 | 4082 | `	}` |
|       - | 4083 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 4084 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 4085 | `	{` |
|      98 | 4086 | `		char *zNsDup = 0;` |
|      98 | 4087 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     143 | 4088 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      94 | 4089 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      47 | 4090 | `		}` |
|      98 | 4091 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 4092 | `	}` |
|      98 | 4093 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 4094 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 4095 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 4096 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 4097 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4098 | `			return SXERR_ABORT;` |
|       - | 4099 | `		}` |
|       2 | 4100 | `	}` |
|      98 | 4101 | `	return SXRET_OK;` |
|      50 | 4102 |  |
|       - | 4103 | `/*` |
|       - | 4104 | ` * Compile the 'use' statement` |
|       - | 4105 | ` * According to the PHP language reference manual` |
|       - | 4106 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 4107 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 4108 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 4109 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 4110 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 4111 | ` *  a function or constant is not supported.` |
|       - | 4112 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 4113 | ` * NOTE` |
|       - | 4114 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4115 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4116 | ` */` |
|      66 | 4117 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 4118 |  |
|       - | 4119 | `	sxu32 nLine;` |
|       - | 4120 | `	sxi32 rc;` |
|       - | 4121 | `	SyBlob sPath;` |
|       - | 4122 | `	SyString sAlias;` |
|       - | 4123 | `	SyToken *pLast;` |
|       - | 4124 | `	char *zDup;` |
|       - | 4125 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 4126 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 4127 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 4128 | `	nLine = pGen->pIn->nLine;` |
|      68 | 4129 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 4130 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 4131 | `	iUseType = 0;` |
|      68 | 4132 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 4133 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 4134 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 4135 | `			iUseType = 1;` |
|      16 | 4136 | `			pGen->pIn++;` |
|      23 | 4137 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 4138 | `			iUseType = 2;` |
|      16 | 4139 | `			pGen->pIn++;` |
|       7 | 4140 | `		}` |
|      14 | 4141 | `	}` |
|       - | 4142 | `	/* Select target hash tables based on import type */` |
|      68 | 4143 | `	switch( iUseType ){` |
|       7 | 4144 | `		case 1:` |
|      16 | 4145 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 4146 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 4147 | `			break;` |
|       7 | 4148 | `		case 2:` |
|      16 | 4149 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 4150 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 4151 | `			break;` |
|      19 | 4152 | `		default:` |
|      40 | 4153 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 4154 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 4155 | `			break;` |
|       - | 4156 | `	}` |
|      68 | 4157 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 4158 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 4159 | `	for(;;){` |
|      70 | 4160 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 4161 | `			break;` |
|       - | 4162 | `		}` |
|      70 | 4163 | `		SyBlobReset(&sPath);` |
|      70 | 4164 | `		pLast = 0;` |
|       - | 4165 | `		/* Collect the full namespace path */` |
|     254 | 4166 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 4167 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 4168 | `				pLast = pGen->pIn;` |
|     126 | 4169 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 4170 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 4171 | `				}` |
|     126 | 4172 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 4173 | `			}` |
|     186 | 4174 | `			pGen->pIn++;` |
|       2 | 4175 | `		}` |
|      70 | 4176 | `		if( pLast == 0 ){` |
|       - | 4177 | `			/* Empty path */` |
|       5 | 4178 | `			break;` |
|       - | 4179 | `		}` |
|       - | 4180 | `		/* Default alias is the last component of the path */` |
|      66 | 4181 | `		sAlias = pLast->sData;` |
|       - | 4182 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 4183 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 4184 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 4185 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 4186 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 4187 | `				sAlias = pGen->pIn->sData;` |
|      18 | 4188 | `				pGen->pIn++;` |
|       8 | 4189 | `			}` |
|       8 | 4190 | `		}` |
|       - | 4191 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4192 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4193 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4194 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4195 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4196 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4197 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4198 | `				return SXERR_ABORT;` |
|       - | 4199 | `			}` |
|       2 | 4200 | `		}` |
|       - | 4201 | `		/* Register the import: alias -> FQN.` |
|       - | 4202 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4203 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4204 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4205 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4206 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4207 | `		if( zDup ){` |
|      66 | 4208 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4209 | `			if( pVmHash ){` |
|       - | 4210 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4211 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4212 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4213 | `				if( zAliasDup ){` |
|      38 | 4214 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4215 | `				}` |
|      18 | 4216 | `			}` |
|      66 | 4217 | `			if( iUseType == 2 ){` |
|       - | 4218 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4219 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4220 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4221 | `				if( zAliasDup ){` |
|       - | 4222 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4223 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4224 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4225 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4226 | `					if( azPair ){` |
|      16 | 4227 | `						azPair[0] = zAliasDup;` |
|      16 | 4228 | `						azPair[1] = zDup;` |
|      16 | 4229 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4230 | `					}` |
|       7 | 4231 | `				}` |
|       7 | 4232 | `			}` |
|      32 | 4233 | `		}` |
|       - | 4234 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4235 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4236 | `			pGen->pIn++;` |
|       2 | 4237 | `		}else{` |
|      33 | 4238 | `			break;` |
|       - | 4239 | `		}` |
|       1 | 4240 | `	}` |
|      68 | 4241 | `	SyBlobRelease(&sPath);` |
|      68 | 4242 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4243 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4244 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4245 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4246 | `			return SXERR_ABORT;` |
|       - | 4247 | `		}` |
|       1 | 4248 | `	}` |
|      68 | 4249 | `	return SXRET_OK;` |
|      35 | 4250 |  |
|       - | 4251 | `/*` |
|       - | 4252 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4253 | ` *` |
|       - | 4254 | ` * According to the PHP language reference manual.` |
|       - | 4255 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4256 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4257 | ` *  declare (directive)` |
|       - | 4258 | ` *   statement` |
|       - | 4259 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4260 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4261 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4262 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4263 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4264 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4265 | ` * <?php` |
|       - | 4266 | ` * // these are the same:` |
|       - | 4267 | ` * // you can use this:` |
|       - | 4268 | ` * declare(ticks=1) {` |
|       - | 4269 | ` *   // entire script here` |
|       - | 4270 | ` * }` |
|       - | 4271 | ` * // or you can use this:` |
|       - | 4272 | ` * declare(ticks=1);` |
|       - | 4273 | ` * // entire script here` |
|       - | 4274 | ` * ?>` |
|       - | 4275 | ` *` |
|       - | 4276 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4277 | ` */` |
|       8 | 4278 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4279 |  |
|       9 | 4280 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4281 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4282 | `	sxi32 rc;` |
|       9 | 4283 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4286 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4287 | `			return SXERR_ABORT;` |
|       - | 4288 | `		}` |
|       5 | 4289 | `		goto Synchro;` |
|       - | 4290 | `	}` |
|       5 | 4291 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4292 | `	/* Delimit the directive */` |
|       5 | 4293 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4294 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4295 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4296 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4297 | `			return SXERR_ABORT;` |
|       - | 4298 | `		}` |
|     ! 0 | 4299 | `		return SXRET_OK;` |
|       - | 4300 | `	}` |
|       - | 4301 | `	/* Update the cursor */` |
|       5 | 4302 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4303 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4304 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4305 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4306 | `			return SXERR_ABORT;` |
|       - | 4307 | `		}` |
|     ! 0 | 4308 | `	}` |
|       - | 4309 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4310 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4311 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4312 | `		ph7_lib_version()` |
|       - | 4313 | `		);` |
|       - | 4314 | `	/*All done */` |
|       5 | 4315 | `	return SXRET_OK;` |
|       2 | 4316 | `Synchro:` |
|       - | 4317 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4318 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4319 | `		pGen->pIn++;` |
|       1 | 4320 | `	}` |
|       5 | 4321 | `	return SXRET_OK;` |
|       5 | 4322 |  |
|       - | 4323 | `/*` |
|       - | 4324 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4325 | ` * as follows:` |
|       - | 4326 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4327 | ` * {` |
|       - | 4328 | ` *   return "Making a cup of $type.\n";` |
|       - | 4329 | ` * }` |
|       - | 4330 | ` * Symisc eXtension.` |
|       - | 4331 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4332 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4333 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4334 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4335 | ` *      {` |
|       - | 4336 | ` *       var_dump($a);` |
|       - | 4337 | ` *      }` |
|       - | 4338 | ` *     //call test without args` |
|       - | 4339 | ` *      test();` |
|       - | 4340 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4341 | ` *      Example:` |
|       - | 4342 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4343 | ` * 3 -) Function overloading!!` |
|       - | 4344 | ` *      Example:` |
|       - | 4345 | ` *      function foo($a) {` |
|       - | 4346 | ` *   	  return $a.PHP_EOL;` |
|       - | 4347 | ` *	    }` |
|       - | 4348 | ` *	    function foo($a, $b) {` |
|       - | 4349 | ` *   	  return $a + $b;` |
|       - | 4350 | ` *	    }` |
|       - | 4351 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4352 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4353 | ` *      // Same arg` |
|       - | 4354 | ` *	   function foo(string $a)` |
|       - | 4355 | ` *	   {` |
|       - | 4356 | ` *	     echo "a is a string\n";` |
|       - | 4357 | ` *	     var_dump($a);` |
|       - | 4358 | ` *	   }` |
|       - | 4359 | ` *	  function foo(int $a)` |
|       - | 4360 | ` *	  {` |
|       - | 4361 | ` *	    echo "a is integer\n";` |
|       - | 4362 | ` *	    var_dump($a);` |
|       - | 4363 | ` *	  }` |
|       - | 4364 | ` *	  function foo(array $a)` |
|       - | 4365 | ` *	  {` |
|       - | 4366 | ` * 	    echo "a is an array\n";` |
|       - | 4367 | ` * 	    var_dump($a);` |
|       - | 4368 | ` *	  }` |
|       - | 4369 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4370 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4371 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4372 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4373 | ` * introduced by the PH7 engine.` |
|       - | 4374 | ` */` |
|   42508 | 4375 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4376 |  |
|       - | 4377 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4378 | `	SySet *pInstrContainer;` |
|       - | 4379 | `	sxi32 rc;` |
|       - | 4380 | `	/* Swap token stream */` |
|   42510 | 4381 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   42510 | 4382 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   42510 | 4383 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4384 | `	/* Compile the expression holding the argument value */` |
|   42510 | 4385 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4386 | `	/* Emit the done instruction */` |
|   42510 | 4387 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   42510 | 4388 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   42510 | 4389 | `	RE_SWAP_DELIMITER(pGen);` |
|   42510 | 4390 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4391 | `		return SXERR_ABORT;` |
|       - | 4392 | `	}` |
|   42510 | 4393 | `	return SXRET_OK;` |
|   21256 | 4394 |  |
|       - | 4395 | `/*` |
|       - | 4396 | ` * Collect function arguments one after one.` |
|       - | 4397 | ` * According to the PHP language reference manual.` |
|       - | 4398 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4399 | ` * list of expressions.` |
|       - | 4400 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4401 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4402 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4403 | ` * for more information.` |
|       - | 4404 | ` * Example #1 Passing arrays to functions` |
|       - | 4405 | ` * <?php` |
|       - | 4406 | ` * function takes_array($input)` |
|       - | 4407 | ` * {` |
|       - | 4408 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4409 | ` * }` |
|       - | 4410 | ` * ?>` |
|       - | 4411 | ` * Making arguments be passed by reference` |
|       - | 4412 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4413 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4414 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4415 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4416 | ` * to the argument name in the function definition:` |
|       - | 4417 | ` * Example #2 Passing function parameters by reference` |
|       - | 4418 | ` * <?php` |
|       - | 4419 | ` * function add_some_extra(&$string)` |
|       - | 4420 | ` * {` |
|       - | 4421 | ` *   $string .= 'and something extra.';` |
|       - | 4422 | ` * }` |
|       - | 4423 | ` * $str = 'This is a string, ';` |
|       - | 4424 | ` * add_some_extra($str);` |
|       - | 4425 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4426 | ` * ?>` |
|       - | 4427 | ` *` |
|       - | 4428 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4429 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4430 | ` * on these extension.` |
|       - | 4431 | ` */` |
|   51068 | 4432 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4433 |  |
|       - | 4434 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4435 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4436 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4437 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4438 | `	sxi32 rc;` |
|       - | 4439 |  |
|   51070 | 4440 | `	pIn = pGen->pIn;` |
|   51070 | 4441 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4442 | `	/* Process arguments one after one */` |
|   64607 | 4443 | `	for(;;){` |
|  129216 | 4444 | `		if( pIn >= pEnd ){` |
|       - | 4445 | `			/* No more arguments to process */` |
|   51068 | 4446 | `			break;` |
|       - | 4447 | `		}` |
|   78150 | 4448 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   78150 | 4449 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4450 | `		/* Detect nullable prefix '?' on type hints */` |
|   78150 | 4451 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 4452 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      16 | 4453 | `			pIn++;` |
|       7 | 4454 | `		}` |
|       - | 4455 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   78150 | 4456 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4457 | `			pIn++;` |
|       2 | 4458 | `		}` |
|   78150 | 4459 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   53174 | 4460 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   47854 | 4461 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   47854 | 4462 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4463 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   47854 | 4464 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4465 | `					sArg.nType = MEMOBJ_BOOL;` |
|   47854 | 4466 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13292 | 4467 | `					sArg.nType = MEMOBJ_INT;` |
|   41209 | 4468 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   34548 | 4469 | `					sArg.nType = MEMOBJ_STRING;` |
|   17291 | 4470 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4471 | `					sArg.nType = MEMOBJ_REAL;` |
|      18 | 4472 | `				}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      16 | 4473 | `					sArg.nType = MEMOBJ_OBJ;` |
|       9 | 4474 | `				}else{` |
|       4 | 4475 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4476 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4477 | `						&pIn->sData);` |
|       - | 4478 | `				}` |
|   23928 | 4479 | `			}else{` |
|    5322 | 4480 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4481 | `				char *zDupLocal;` |
|       - | 4482 | `				/* Argument must be a class instance,record that*/` |
|    5322 | 4483 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5322 | 4484 | `				if( zDupLocal ){` |
|    5322 | 4485 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5322 | 4486 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2660 | 4487 | `				}` |
|       - | 4488 | `			}` |
|   53174 | 4489 | `			pIn++;` |
|   26586 | 4490 | `		}` |
|   78150 | 4491 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4492 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4493 | `			return rc;` |
|       - | 4494 | `		}` |
|   78150 | 4495 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4496 | `			/* Pass by reference,record that */` |
|    2682 | 4497 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2682 | 4498 | `			pIn++;` |
|    1340 | 4499 | `		}` |
|   78150 | 4500 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4501 | `			/* Variadic parameter: ...$args */` |
|      28 | 4502 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      28 | 4503 | `			pIn++;` |
|      13 | 4504 | `		}` |
|   78150 | 4505 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4506 | `			/* Invalid argument */` |
|     ! 0 | 4507 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4508 | `			return rc;` |
|       - | 4509 | `		}` |
|   78150 | 4510 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4511 | `		/* Copy argument name */` |
|   78150 | 4512 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   78150 | 4513 | `		if( zDup == 0 ){` |
|     ! 0 | 4514 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4515 | `			return SXERR_ABORT;` |
|       - | 4516 | `		}` |
|   78150 | 4517 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   78150 | 4518 | `		pIn++;` |
|   78150 | 4519 | `		if( pIn < pEnd ){` |
|   48348 | 4520 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4521 | `				SyToken *pDefend;` |
|   42512 | 4522 | `				sxi32 iNest = 0;` |
|   42512 | 4523 | `				pIn++; /* Jump the equal sign */` |
|   42512 | 4524 | `				pDefend = pIn;` |
|       - | 4525 | `				/* Process the default value associated with this argument */` |
|   90332 | 4526 | `				while( pDefend < pEnd ){` |
|   69070 | 4527 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   21250 | 4528 | `						break;` |
|       - | 4529 | `					}` |
|   47822 | 4530 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4531 | `						/* Increment nesting level */` |
|    2658 | 4532 | `						iNest++;` |
|   46494 | 4533 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4534 | `						/* Decrement nesting level */` |
|    2658 | 4535 | `						iNest--;` |
|    1328 | 4536 | `					}` |
|   47822 | 4537 | `					pDefend++;` |
|       2 | 4538 | `				}` |
|   42512 | 4539 | `				if( pIn >= pDefend ){` |
|       3 | 4540 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4541 | `					return rc;` |
|       - | 4542 | `				}` |
|       - | 4543 | `				/* Process default value */` |
|   42510 | 4544 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   42510 | 4545 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4546 | `					return rc;` |
|       - | 4547 | `				}` |
|       - | 4548 | `				/* Point beyond the default value */` |
|   42510 | 4549 | `				pIn = pDefend;` |
|   21254 | 4550 | `			}` |
|   48346 | 4551 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4552 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4553 | `				return rc;` |
|       - | 4554 | `			}` |
|   48346 | 4555 | `			pIn++; /* Jump the trailing comma */` |
|   24172 | 4556 | `		}` |
|       - | 4557 | `		/* Append argument signature */` |
|   78148 | 4558 | `		if( sArg.nType > 0 ){` |
|   53172 | 4559 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4560 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5322 | 4561 | `				int marker = 'o';` |
|    5322 | 4562 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5322 | 4563 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2662 | 4564 | `			}else{` |
|       - | 4565 | `				int c;` |
|   47852 | 4566 | `				c = 'n'; /* cc warning */` |
|       - | 4567 | `				/* Type leading character */` |
|   47852 | 4568 | `				switch(sArg.nType){` |
|     ! 0 | 4569 | `				case MEMOBJ_HASHMAP:` |
|       - | 4570 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4571 | `					c = 'h';` |
|     ! 0 | 4572 | `					break;` |
|    6645 | 4573 | `				case MEMOBJ_INT:` |
|       - | 4574 | `					/* Integer */` |
|   13292 | 4575 | `					c = 'i';` |
|   13292 | 4576 | `					break;` |
|     ! 0 | 4577 | `				case MEMOBJ_BOOL:` |
|       - | 4578 | `					/* Bool */` |
|     ! 0 | 4579 | `					c = 'b';` |
|     ! 0 | 4580 | `					break;` |
|     ! 0 | 4581 | `				case MEMOBJ_REAL:` |
|       - | 4582 | `					/* Float */` |
|     ! 0 | 4583 | `					c = 'f';` |
|     ! 0 | 4584 | `					break;` |
|   17273 | 4585 | `				case MEMOBJ_STRING:` |
|       - | 4586 | `					/* String */` |
|   34548 | 4587 | `					c = 's';` |
|   34548 | 4588 | `					break;` |
|       7 | 4589 | `				case MEMOBJ_OBJ:` |
|       - | 4590 | `					/* Object */` |
|      16 | 4591 | `					c = 'o';` |
|      14 | 4592 | `					break;` |
|     ! 0 | 4593 | `				default:` |
|     ! 0 | 4594 | `					break;` |
|       - | 4595 | `				}` |
|   47852 | 4596 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4597 | `			}` |
|   26587 | 4598 | `		}else{` |
|       - | 4599 | `			/* No type is associated with this parameter which mean` |
|       - | 4600 | `			 * that this function is not condidate for overloading.` |
|       - | 4601 | `			 */` |
|   24978 | 4602 | `			SyBlobRelease(&sSig);` |
|       - | 4603 | `		}` |
|       - | 4604 | `		/* Save in the argument set */` |
|   78148 | 4605 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4606 | `	}` |
|   51068 | 4607 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4608 | `		/* Save function signature */` |
|   31918 | 4609 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15958 | 4610 | `	}` |
|   51068 | 4611 | `	return SXRET_OK;` |
|   25536 | 4612 |  |
|       - | 4613 | `/*` |
|       - | 4614 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4615 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4616 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4617 | ` */` |
|  141902 | 4618 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4619 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4620 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4621 | `	)` |
|       2 | 4622 |  |
|       - | 4623 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4624 | `	GenBlock *pBlock;` |
|       - | 4625 | `	sxu32 nGotoOfft;` |
|       - | 4626 | `	sxi32 rc;` |
|       - | 4627 | `	/* Attach the new function */` |
|  141904 | 4628 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  141904 | 4629 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4630 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4631 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4632 | `		return SXERR_ABORT;` |
|       - | 4633 | `	}` |
|  141904 | 4634 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4635 | `	/* Swap bytecode containers */` |
|  141904 | 4636 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  141904 | 4637 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4638 | `	/* Compile the body */` |
|  141904 | 4639 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4640 | `	/* Fix exception jumps now the destination is resolved */` |
|  141904 | 4641 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4642 | `	/* Emit the final return if not yet done */` |
|  141904 | 4643 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4644 | `	/* Fix gotos jumps now the destination is resolved */` |
|  141904 | 4645 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4646 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4647 | `	}` |
|  141904 | 4648 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4649 | `	/* Restore the default container */` |
|  141904 | 4650 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4651 | `	/* Leave function block */` |
|  141904 | 4652 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  141904 | 4653 | `	if( rc == SXERR_ABORT ){` |
|       - | 4654 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4655 | `		return SXERR_ABORT;` |
|       - | 4656 | `	}` |
|       - | 4657 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4658 | `	{` |
|  141904 | 4659 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4660 | `		sxu32 i;` |
| 2945872 | 4661 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2803986 | 4662 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4663 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4664 | `				break;` |
|       - | 4665 | `			}` |
| 1401986 | 4666 | `		}` |
|       - | 4667 | `	}` |
|       - | 4668 | `	/* All done, function body compiled */` |
|  141904 | 4669 | `	return SXRET_OK;` |
|   70953 | 4670 |  |
|       - | 4671 | `/*` |
|       - | 4672 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4673 | ` * According to the PHP language reference manual.` |
|       - | 4674 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4675 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4676 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4677 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4678 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4679 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4680 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4681 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4682 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4683 | ` *` |
|       - | 4684 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4685 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4686 | ` * on these extension.` |
|       - | 4687 | ` */` |
|       - | 4688 | `/*` |
|       - | 4689 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4690 | ` */` |
|       6 | 4691 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4692 |  |
|       - | 4693 | `	sxu32 i;` |
|      31 | 4694 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4695 | `		int a = zA[i], b = zB[i];` |
|      25 | 4696 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4697 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4698 | `		if( a != b ) return a - b;` |
|      13 | 4699 | `	}` |
|       7 | 4700 | `	return 0;` |
|       4 | 4701 |  |
|       - | 4702 | `/*` |
|       - | 4703 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4704 | ` */` |
|       2 | 4705 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4706 |  |
|       3 | 4707 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4708 | `	if( zDup ){` |
|       3 | 4709 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4710 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4711 | `	}` |
|       3 | 4712 |  |
|       - | 4713 | `/*` |
|       - | 4714 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4715 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4716 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4717 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4718 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4719 | ` */` |
|  163198 | 4720 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4721 |  |
|  163200 | 4722 | `	SyToken *pCur = pGen->pIn;` |
|  163200 | 4723 | `	pFunc->nReturnType = 0;` |
|  163200 | 4724 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  163200 | 4725 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  163136 | 4726 | `		return; /* No return type */` |
|       - | 4727 | `	}` |
|      66 | 4728 | `	pCur++; /* Skip ':' */` |
|      66 | 4729 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4730 | `		pGen->pIn = pCur;` |
|     ! 0 | 4731 | `		return;` |
|       - | 4732 | `	}` |
|       - | 4733 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      66 | 4734 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4735 | `		pCur++;` |
|       7 | 4736 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4737 | `			pGen->pIn = pCur;` |
|     ! 0 | 4738 | `			return;` |
|       - | 4739 | `		}` |
|       3 | 4740 | `	}` |
|      66 | 4741 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      60 | 4742 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      60 | 4743 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4744 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      59 | 4745 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4746 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      57 | 4747 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      20 | 4748 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      47 | 4749 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      32 | 4750 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      23 | 4751 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4752 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       7 | 4753 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       3 | 4754 | `			pFunc->nReturnType = MEMOBJ_OBJ;` |
|       4 | 4755 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4756 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4757 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4758 | `		}` |
|      60 | 4759 | `		pCur++;` |
|      36 | 4760 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4761 | `		SyString *pType = &pCur->sData;` |
|       7 | 4762 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4763 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4764 | `		}else{` |
|       - | 4765 | `			/* Class/interface name */` |
|     ! 0 | 4766 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4767 | `		}` |
|       7 | 4768 | `		pCur++;` |
|       3 | 4769 | `	}` |
|      66 | 4770 | `	pGen->pIn = pCur;` |
|   81601 | 4771 |  |
|       - | 4772 |  |
|   35234 | 4773 | `static sxi32 GenStateCompileFunc(` |
|       - | 4774 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4775 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4776 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4777 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4778 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4779 | `	)` |
|       2 | 4780 |  |
|       - | 4781 | `	ph7_vm_func *pFunc;` |
|       - | 4782 | `	SyToken *pEnd;` |
|       - | 4783 | `	sxu32 nLine;` |
|       - | 4784 | `	char *zName;` |
|       - | 4785 | `	sxi32 rc;` |
|       - | 4786 | `	/* Extract line number */` |
|   35236 | 4787 | `	nLine = pGen->pIn->nLine;` |
|       - | 4788 | `	/* Jump the left parenthesis '(' */` |
|   35236 | 4789 | `	pGen->pIn++;` |
|       - | 4790 | `	/* Delimit the function signature */` |
|   35236 | 4791 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   35236 | 4792 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4793 | `		/* Syntax error */` |
|       7 | 4794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4795 | `		if( rc == SXERR_ABORT ){` |
|       - | 4796 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4797 | `			return SXERR_ABORT;` |
|       - | 4798 | `		}` |
|       7 | 4799 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4800 | `		return SXRET_OK;` |
|       - | 4801 | `	}` |
|       - | 4802 | `	/* Create the function state */` |
|   35230 | 4803 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   35230 | 4804 | `	if( pFunc == 0 ){` |
|     ! 0 | 4805 | `		goto OutOfMem;` |
|       - | 4806 | `	}` |
|       - | 4807 | `	/* Build the function name, prepending namespace if active */` |
|   35237 | 4808 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4809 | `		SyBlob sFQN;` |
|       - | 4810 | `		sxu32 nLen;` |
|      16 | 4811 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4812 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4813 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4814 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4815 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4816 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4817 | `		SyBlobRelease(&sFQN);` |
|      16 | 4818 | `		if( zName == 0 ){` |
|     ! 0 | 4819 | `			goto OutOfMem;` |
|       - | 4820 | `		}` |
|      16 | 4821 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4822 | `	}else{` |
|   35216 | 4823 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   35216 | 4824 | `		if( zName == 0 ){` |
|     ! 0 | 4825 | `			goto OutOfMem;` |
|       - | 4826 | `		}` |
|   35216 | 4827 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4828 | `	}` |
|   35230 | 4829 | `	if( pGen->pIn < pEnd ){` |
|       - | 4830 | `		/* Collect function arguments */` |
|   24422 | 4831 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   24422 | 4832 | `		if( rc == SXERR_ABORT ){` |
|       - | 4833 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4834 | `			return SXERR_ABORT;` |
|       - | 4835 | `		}` |
|   12210 | 4836 | `	}` |
|       - | 4837 | `	/* Point past ')' and parse optional return type ': type' */` |
|   35230 | 4838 | `	pGen->pIn = &pEnd[1];` |
|   35230 | 4839 | `	GenStateParseReturnType(pGen, pFunc);` |
|   35230 | 4840 | `	if( bHandleClosure ){` |
|       - | 4841 | `		ph7_vm_func_closure_env sEnv;` |
|     170 | 4842 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     168 | 4843 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      93 | 4844 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 | 4845 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4846 | `				/* Closure,record environment variable */` |
|      16 | 4847 | `				pGen->pIn++;` |
|      16 | 4848 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4849 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4850 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4851 | `						return SXERR_ABORT;` |
|       - | 4852 | `					}` |
|     ! 0 | 4853 | `				}` |
|      16 | 4854 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4855 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 | 4856 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 | 4857 | `					int iFlagsLocal = 0;` |
|      34 | 4858 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 | 4859 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 | 4860 | `						break;` |
|       - | 4861 | `					}` |
|      20 | 4862 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 | 4863 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4864 | `						/* Pass by reference,record that */` |
|     ! 0 | 4865 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4866 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4867 | `							);` |
|     ! 0 | 4868 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4869 | `						pGen->pIn++;` |
|     ! 0 | 4870 | `					}` |
|      18 | 4871 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 | 4872 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4873 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4874 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4875 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4876 | `								return SXERR_ABORT;` |
|       - | 4877 | `							}` |
|       - | 4878 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4879 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4880 | `								pGen->pIn++;` |
|     ! 0 | 4881 | `							}` |
|     ! 0 | 4882 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4883 | `								pGen->pIn++;` |
|     ! 0 | 4884 | `							}` |
|     ! 0 | 4885 | `							break;` |
|       - | 4886 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4887 | `					}else{` |
|       - | 4888 | `						SyString *pNameLocal;` |
|       - | 4889 | `						char *zDup;` |
|       - | 4890 | `						/* Duplicate variable name */` |
|      20 | 4891 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 | 4892 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 | 4893 | `						if( zDup ){` |
|       - | 4894 | `							/* Zero the structure */` |
|      20 | 4895 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 | 4896 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 | 4897 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 | 4898 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 | 4899 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4900 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4901 | `									got_this = 1;` |
|     ! 0 | 4902 | `							}` |
|       - | 4903 | `							/* Save imported variable */` |
|      20 | 4904 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 | 4905 | `						}else{` |
|     ! 0 | 4906 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4907 | `							 return SXERR_ABORT;` |
|       - | 4908 | `						}` |
|       - | 4909 | `					}` |
|      20 | 4910 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 | 4911 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4912 | `						/* Ignore trailing commas */` |
|       7 | 4913 | `						pGen->pIn++;` |
|       1 | 4914 | `					}` |
|       2 | 4915 | `				}` |
|      16 | 4916 | `				if( !got_this ){` |
|       - | 4917 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4918 | `					 * available to the closure environment.` |
|       - | 4919 | `					 */` |
|      16 | 4920 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4921 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 | 4922 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4923 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 | 4924 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 | 4925 | `				}` |
|      16 | 4926 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4927 | `					/* Mark as closure */` |
|      16 | 4928 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 | 4929 | `				}` |
|       7 | 4930 | `		}` |
|      84 | 4931 | `	}` |
|       - | 4932 | `	/* Compile the body */` |
|   35230 | 4933 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   35230 | 4934 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4935 | `		return SXERR_ABORT;` |
|       - | 4936 | `	}` |
|   35230 | 4937 | `	if( ppFunc ){` |
|     170 | 4938 | `		*ppFunc = pFunc;` |
|      84 | 4939 | `	}` |
|   35230 | 4940 | `	rc = SXRET_OK;` |
|   35230 | 4941 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4942 | `		/* Finally register the function */` |
|   35216 | 4943 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17607 | 4944 | `	}` |
|   35230 | 4945 | `	if( rc == SXRET_OK ){` |
|   35230 | 4946 | `		return SXRET_OK;` |
|       - | 4947 | `	}` |
|       - | 4948 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4949 | `OutOfMem:` |
|       - | 4950 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4951 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4952 | `	 */` |
|     ! 0 | 4953 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4954 | `	return SXERR_ABORT;` |
|   17619 | 4955 |  |
|       - | 4956 | `/*` |
|       - | 4957 | ` * Compile a standard PHP function.` |
|       - | 4958 | ` *  Refer to the block-comment above for more information.` |
|       - | 4959 | ` */` |
|   35072 | 4960 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4961 |  |
|       - | 4962 | `	SyString *pName;` |
|       - | 4963 | `	sxi32 iFlags;` |
|       - | 4964 | `	sxu32 nLine;` |
|       - | 4965 | `	sxi32 rc;` |
|       - | 4966 |  |
|   35074 | 4967 | `	nLine = pGen->pIn->nLine;` |
|   35074 | 4968 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   35074 | 4969 | `	iFlags = 0;` |
|   35074 | 4970 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4971 | `		/* Return by reference,remember that */` |
|       7 | 4972 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4973 | `		/* Jump the '&' token */` |
|       7 | 4974 | `		pGen->pIn++;` |
|       3 | 4975 | `	}` |
|   35074 | 4976 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4977 | `		/* Invalid function name */` |
|       5 | 4978 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4979 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4980 | `			return SXERR_ABORT;` |
|       - | 4981 | `		}` |
|       - | 4982 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4983 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4984 | `			pGen->pIn++;` |
|       1 | 4985 | `		}` |
|       5 | 4986 | `		return SXRET_OK;` |
|       - | 4987 | `	}` |
|   35070 | 4988 | `	pName = &pGen->pIn->sData;` |
|   35070 | 4989 | `	nLine = pGen->pIn->nLine;` |
|       - | 4990 | `	/* Jump the function name */` |
|   35070 | 4991 | `	pGen->pIn++;` |
|   35070 | 4992 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4993 | `		/* Syntax error */` |
|       3 | 4994 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4995 | `		if( rc == SXERR_ABORT ){` |
|       - | 4996 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4997 | `			return SXERR_ABORT;` |
|       - | 4998 | `		}` |
|       - | 4999 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 5000 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 5001 | `			pGen->pIn++;` |
|     ! 0 | 5002 | `		}` |
|       3 | 5003 | `		return SXRET_OK;` |
|       - | 5004 | `	}` |
|       - | 5005 | `	/* Compile function body */` |
|   35068 | 5006 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   35068 | 5007 | `	return rc;` |
|   17538 | 5008 |  |
|       - | 5009 | `/*` |
|       - | 5010 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 5011 | ` * According to the PHP language reference manual` |
|       - | 5012 | ` *  Visibility:` |
|       - | 5013 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 5014 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 5015 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 5016 | ` *  Members declared protected can be accessed only within the class` |
|       - | 5017 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 5018 | ` *  may only be accessed by the class that defines the member.` |
|       - | 5019 | ` */` |
|  162752 | 5020 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 5021 |  |
|  162754 | 5022 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8034 | 5023 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  154722 | 5024 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18636 | 5025 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 5026 | `	}` |
|       - | 5027 | `	/* Assume public by default */` |
|  136088 | 5028 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   81378 | 5029 |  |
|       - | 5030 | `/*` |
|       - | 5031 | ` * Compile a class constant.` |
|       - | 5032 | ` * According to the PHP language reference manual` |
|       - | 5033 | ` *  Class Constants` |
|       - | 5034 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 5035 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 5036 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 5037 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 5038 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 5039 | ` *   It's also possible for interfaces to have constants.` |
|       - | 5040 | ` * Symisc eXtension.` |
|       - | 5041 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 5042 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5043 | ` *  Example:` |
|       - | 5044 | ` *   class Test{` |
|       - | 5045 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5046 | ` *   };` |
|       - | 5047 | ` *   var_dump(TEST::MyConst);` |
|       - | 5048 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5049 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5050 | ` */` |
|      30 | 5051 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5052 |  |
|      32 | 5053 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5054 | `	SySet *pInstrContainer;` |
|       - | 5055 | `	ph7_class_attr *pCons;` |
|       - | 5056 | `	SyString *pName;` |
|       - | 5057 | `	sxi32 rc;` |
|       - | 5058 | `	/* Extract visibility level */` |
|      32 | 5059 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 5060 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 5061 | `loop:` |
|       - | 5062 | `	/* Mark as constant */` |
|      32 | 5063 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 5064 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5065 | `		/* Invalid constant name */` |
|     ! 0 | 5066 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 5067 | `		if( rc == SXERR_ABORT ){` |
|       - | 5068 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5069 | `			return SXERR_ABORT;` |
|       - | 5070 | `		}` |
|     ! 0 | 5071 | `		goto Synchronize;` |
|       - | 5072 | `	}` |
|       - | 5073 | `	/* Peek constant name */` |
|      32 | 5074 | `	pName = &pGen->pIn->sData;` |
|       - | 5075 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 5076 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 5077 | `		/* Reserved constant name */` |
|     ! 0 | 5078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 5079 | `		if( rc == SXERR_ABORT ){` |
|       - | 5080 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5081 | `			return SXERR_ABORT;` |
|       - | 5082 | `		}` |
|     ! 0 | 5083 | `		goto Synchronize;` |
|       - | 5084 | `	}` |
|       - | 5085 | `	/* Advance the stream cursor */` |
|      32 | 5086 | `	pGen->pIn++;` |
|      32 | 5087 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 5088 | `		/* Invalid declaration */` |
|     ! 0 | 5089 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 5090 | `		if( rc == SXERR_ABORT ){` |
|       - | 5091 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5092 | `			return SXERR_ABORT;` |
|       - | 5093 | `		}` |
|     ! 0 | 5094 | `		goto Synchronize;` |
|       - | 5095 | `	}` |
|      32 | 5096 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 5097 | `	/* Allocate a new class attribute */` |
|      32 | 5098 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 5099 | `	if( pCons == 0 ){` |
|     ! 0 | 5100 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5101 | `		return SXERR_ABORT;` |
|       - | 5102 | `	}` |
|       - | 5103 | `	/* Swap bytecode container */` |
|      32 | 5104 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 5105 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 5106 | `	/* Compile constant value.` |
|       - | 5107 | `	 */` |
|      32 | 5108 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 5109 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 5110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 5111 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5112 | `			return SXERR_ABORT;` |
|       - | 5113 | `		}` |
|       1 | 5114 | `	}` |
|       - | 5115 | `	/* Emit the done instruction */` |
|      32 | 5116 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 5117 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 5118 | `	if( rc == SXERR_ABORT ){` |
|       - | 5119 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5120 | `		return SXERR_ABORT;` |
|       - | 5121 | `	}` |
|       - | 5122 | `	/* All done,install the constant */` |
|      32 | 5123 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 5124 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5125 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5126 | `		return SXERR_ABORT;` |
|       - | 5127 | `	}` |
|      32 | 5128 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5129 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 5130 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5131 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5132 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5133 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5134 | `				pTok--;` |
|     ! 0 | 5135 | `			}` |
|     ! 0 | 5136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5137 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 5138 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5139 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5140 | `				return SXERR_ABORT;` |
|       - | 5141 | `			}` |
|     ! 0 | 5142 | `		}else{` |
|     ! 0 | 5143 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 5144 | `				goto loop;` |
|       - | 5145 | `			}` |
|       - | 5146 | `		}` |
|     ! 0 | 5147 | `	}` |
|      32 | 5148 | `	return SXRET_OK;` |
|     ! 0 | 5149 | `Synchronize:` |
|       - | 5150 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5151 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 5152 | `		pGen->pIn++;` |
|     ! 0 | 5153 | `	}` |
|     ! 0 | 5154 | `	return SXERR_CORRUPT;` |
|      17 | 5155 |  |
|       - | 5156 | `/*` |
|       - | 5157 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 5158 | ` * According to the PHP language reference manual` |
|       - | 5159 | ` *  Properties` |
|       - | 5160 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 5161 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 5162 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 5163 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 5164 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 5165 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 5166 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 5167 | ` * Symisc eXtension.` |
|       - | 5168 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 5169 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5170 | ` *  Example:` |
|       - | 5171 | ` *   class Test{` |
|       - | 5172 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5173 | ` *   };` |
|       - | 5174 | ` *   var_dump(TEST::myVar);` |
|       - | 5175 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5176 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5177 | ` */` |
|   34750 | 5178 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5179 |  |
|   34752 | 5180 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5181 | `	ph7_class_attr *pAttr;` |
|       - | 5182 | `	SyString *pName;` |
|       - | 5183 | `	sxi32 rc;` |
|       - | 5184 | `	/* Extract visibility level */` |
|   34752 | 5185 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17375 | 5186 | `loop:` |
|   34752 | 5187 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   34752 | 5188 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 5189 | `		/* Invalid attribute name */` |
|     ! 0 | 5190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 5191 | `		if( rc == SXERR_ABORT ){` |
|       - | 5192 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5193 | `			return SXERR_ABORT;` |
|       - | 5194 | `		}` |
|     ! 0 | 5195 | `		goto Synchronize;` |
|       - | 5196 | `	}` |
|       - | 5197 | `	/* Peek attribute name */` |
|   34752 | 5198 | `	pName = &pGen->pIn->sData;` |
|       - | 5199 | `	/* Advance the stream cursor */` |
|   34752 | 5200 | `	pGen->pIn++;` |
|   34752 | 5201 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5202 | `		/* Invalid declaration */` |
|       3 | 5203 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5204 | `		if( rc == SXERR_ABORT ){` |
|       - | 5205 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5206 | `			return SXERR_ABORT;` |
|       - | 5207 | `		}` |
|       3 | 5208 | `		goto Synchronize;` |
|       - | 5209 | `	}` |
|       - | 5210 | `	/* Allocate a new class attribute */` |
|   34750 | 5211 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   34750 | 5212 | `	if( pAttr == 0 ){` |
|     ! 0 | 5213 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5214 | `		return SXERR_ABORT;` |
|       - | 5215 | `	}` |
|   34750 | 5216 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5217 | `		SySet *pInstrContainer;` |
|   10792 | 5218 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5219 | `		/* Swap bytecode container */` |
|   10792 | 5220 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10792 | 5221 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5222 | `		/* Compile attribute value.` |
|       - | 5223 | `		 */` |
|   10792 | 5224 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10792 | 5225 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5226 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5227 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5228 | `				return SXERR_ABORT;` |
|       - | 5229 | `			}` |
|     ! 0 | 5230 | `		}` |
|       - | 5231 | `		/* Emit the done instruction */` |
|   10792 | 5232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10792 | 5233 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5395 | 5234 | `	}` |
|       - | 5235 | `	/* All done,install the attribute */` |
|   34750 | 5236 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   34750 | 5237 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5238 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5239 | `		return SXERR_ABORT;` |
|       - | 5240 | `	}` |
|   34750 | 5241 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5242 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5243 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5244 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5245 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5246 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5247 | `				pTok--;` |
|     ! 0 | 5248 | `			}` |
|     ! 0 | 5249 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5250 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5251 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5252 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5253 | `				return SXERR_ABORT;` |
|       - | 5254 | `			}` |
|     ! 0 | 5255 | `		}else{` |
|     ! 0 | 5256 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5257 | `				goto loop;` |
|       - | 5258 | `			}` |
|       - | 5259 | `		}` |
|     ! 0 | 5260 | `	}` |
|   34750 | 5261 | `	return SXRET_OK;` |
|       1 | 5262 | `Synchronize:` |
|       - | 5263 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5264 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5265 | `		pGen->pIn++;` |
|       1 | 5266 | `	}` |
|       3 | 5267 | `	return SXERR_CORRUPT;` |
|   17377 | 5268 |  |
|       - | 5269 | `/*` |
|       - | 5270 | ` * Compile a class method.` |
|       - | 5271 | ` *` |
|       - | 5272 | ` * Refer to the official documentation for more information` |
|       - | 5273 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5274 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5275 | ` * overloading and many more.` |
|       - | 5276 | ` */` |
|  127972 | 5277 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5278 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5279 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5280 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5281 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5282 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5283 | `	)` |
|       2 | 5284 |  |
|  127974 | 5285 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5286 | `	ph7_class_method *pMeth;` |
|       - | 5287 | `	sxi32 iFuncFlags;` |
|       - | 5288 | `	SyString *pName;` |
|       - | 5289 | `	SyToken *pEnd;` |
|       - | 5290 | `	sxi32 rc;` |
|       - | 5291 | `	/* Extract visibility level */` |
|  127974 | 5292 | `	iProtection = GetProtectionLevel(iProtection);` |
|  127974 | 5293 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  127974 | 5294 | `	iFuncFlags = 0;` |
|  127974 | 5295 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5296 | `		/* Invalid method name */` |
|     ! 0 | 5297 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5298 | `		if( rc == SXERR_ABORT ){` |
|       - | 5299 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5300 | `			return SXERR_ABORT;` |
|       - | 5301 | `		}` |
|     ! 0 | 5302 | `		goto Synchronize;` |
|       - | 5303 | `	}` |
|  127974 | 5304 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5305 | `		/* Return by reference,remember that */` |
|     ! 0 | 5306 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5307 | `		/* Jump the '&' token */` |
|     ! 0 | 5308 | `		pGen->pIn++;` |
|     ! 0 | 5309 | `	}` |
|  127974 | 5310 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5311 | `		/* Invalid method name */` |
|     ! 0 | 5312 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5313 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5314 | `			return SXERR_ABORT;` |
|       - | 5315 | `		}` |
|     ! 0 | 5316 | `		goto Synchronize;` |
|       - | 5317 | `	}` |
|       - | 5318 | `	/* Peek method name */` |
|  127974 | 5319 | `	pName = &pGen->pIn->sData;` |
|  127974 | 5320 | `	nLine = pGen->pIn->nLine;` |
|       - | 5321 | `	/* Jump the method name */` |
|  127974 | 5322 | `	pGen->pIn++;` |
|  127974 | 5323 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5324 | `		/* Abstract method */` |
|   21298 | 5325 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5326 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5327 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5328 | `				&pClass->sName,pName);` |
|     ! 0 | 5329 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5330 | `				return SXERR_ABORT;` |
|       - | 5331 | `			}` |
|     ! 0 | 5332 | `		}` |
|       - | 5333 | `		/* Assemble method signature only */` |
|   21298 | 5334 | `		doBody = FALSE;` |
|   10648 | 5335 | `	}` |
|  127974 | 5336 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5337 | `		/* Syntax error */` |
|     ! 0 | 5338 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5339 | `		if( rc == SXERR_ABORT ){` |
|       - | 5340 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5341 | `			return SXERR_ABORT;` |
|       - | 5342 | `		}` |
|     ! 0 | 5343 | `		goto Synchronize;` |
|       - | 5344 | `	}` |
|       - | 5345 | `	/* Allocate a new class_method instance */` |
|  127974 | 5346 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  127974 | 5347 | `	if( pMeth == 0 ){` |
|     ! 0 | 5348 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5349 | `		return SXERR_ABORT;` |
|       - | 5350 | `	}` |
|       - | 5351 | `	/* Jump the left parenthesis '(' */` |
|  127974 | 5352 | `	pGen->pIn++;` |
|  127974 | 5353 | `	pEnd = 0; /* cc warning */` |
|       - | 5354 | `	/* Delimit the method signature */` |
|  127974 | 5355 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  127974 | 5356 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5357 | `		/* Syntax error */` |
|       3 | 5358 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5359 | `		if( rc == SXERR_ABORT ){` |
|       - | 5360 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5361 | `			return SXERR_ABORT;` |
|       - | 5362 | `		}` |
|       3 | 5363 | `		goto Synchronize;` |
|       - | 5364 | `	}` |
|  127972 | 5365 | `	if( pGen->pIn < pEnd ){` |
|       - | 5366 | `		/* Collect method arguments */` |
|   26650 | 5367 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26650 | 5368 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5369 | `			return SXERR_ABORT;` |
|       - | 5370 | `		}` |
|   13324 | 5371 | `	}` |
|       - | 5372 | `	/* Point past ')' and parse optional return type ': type' */` |
|  127972 | 5373 | `	pGen->pIn = &pEnd[1];` |
|  127972 | 5374 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  127972 | 5375 | `	if( doBody ){` |
|       - | 5376 | `		/* Compile method body */` |
|  106676 | 5377 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  106676 | 5378 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5379 | `			return SXERR_ABORT;` |
|       - | 5380 | `		}` |
|   53339 | 5381 | `	}else{` |
|       - | 5382 | `		/* Only method signature is allowed */` |
|   21298 | 5383 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5384 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5385 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5386 | `				if( rc == SXERR_ABORT ){` |
|       - | 5387 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5388 | `					return SXERR_ABORT;` |
|       - | 5389 | `				}` |
|     ! 0 | 5390 | `				return SXERR_CORRUPT;` |
|       - | 5391 | `			}` |
|       - | 5392 | `	}` |
|       - | 5393 | `	/* All done,install the method */` |
|  127972 | 5394 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  127972 | 5395 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5396 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5397 | `		return SXERR_ABORT;` |
|       - | 5398 | `	}` |
|  127972 | 5399 | `	return SXRET_OK;` |
|       1 | 5400 | `Synchronize:` |
|       - | 5401 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5402 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5403 | `		pGen->pIn++;` |
|       1 | 5404 | `	}` |
|       3 | 5405 | `	return SXERR_CORRUPT;` |
|   63988 | 5406 |  |
|       - | 5407 | `/*` |
|       - | 5408 | ` * Compile an object interface.` |
|       - | 5409 | ` *  According to the PHP language reference manual` |
|       - | 5410 | ` *   Object Interfaces:` |
|       - | 5411 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5412 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5413 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5414 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5415 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5416 | ` */` |
|    8006 | 5417 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5418 |  |
|    8008 | 5419 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5420 | `	ph7_class *pClass,*pBase;` |
|       - | 5421 | `	SyToken *pEnd,*pTmp;` |
|       - | 5422 | `	SyString *pName;` |
|       - | 5423 | `	sxi32 nKwrd;` |
|       - | 5424 | `	sxi32 rc;` |
|       - | 5425 | `	/* Jump the 'interface' keyword */` |
|    8008 | 5426 | `	pGen->pIn++;` |
|       - | 5427 | `	/* Extract interface name */` |
|    8008 | 5428 | `	pName = &pGen->pIn->sData;` |
|       - | 5429 | `	/* Advance the stream cursor */` |
|    8008 | 5430 | `	pGen->pIn++;` |
|       - | 5431 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5432 | `		SyBlob sFQN;` |
|       - | 5433 | `		SyString sFQNStr;` |
|    8008 | 5434 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8008 | 5435 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8008 | 5436 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8008 | 5437 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8008 | 5438 | `		SyBlobRelease(&sFQN);` |
|       - | 5439 | `	}` |
|    8008 | 5440 | `	if( pClass == 0 ){` |
|     ! 0 | 5441 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5442 | `		return SXERR_ABORT;` |
|       - | 5443 | `	}` |
|       - | 5444 | `	/* Mark as an interface */` |
|    8008 | 5445 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5446 | `	/* Assume no base class is given */` |
|    8008 | 5447 | `	pBase = 0;` |
|    8008 | 5448 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5449 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5450 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5451 | `			SyString *pBaseName;` |
|       - | 5452 | `			/* Extract base interface */` |
|       3 | 5453 | `			pGen->pIn++;` |
|       3 | 5454 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5455 | `				/* Syntax error */` |
|     ! 0 | 5456 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5457 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5458 | `					pName);` |
|     ! 0 | 5459 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5460 | `				if( rc == SXERR_ABORT ){` |
|       - | 5461 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5462 | `					return SXERR_ABORT;` |
|       - | 5463 | `				}` |
|     ! 0 | 5464 | `				return SXRET_OK;` |
|       - | 5465 | `			}` |
|       3 | 5466 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5467 | `			{` |
|       - | 5468 | `				SyBlob sResolved;` |
|       3 | 5469 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5470 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5471 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5472 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5473 | `				SyBlobRelease(&sResolved);` |
|       - | 5474 | `			}` |
|       - | 5475 | `			/* Only interfaces is allowed */` |
|       3 | 5476 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5477 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5478 | `			}` |
|       3 | 5479 | `			if( pBase == 0 ){` |
|       - | 5480 | `				/* Inexistant interface */` |
|     ! 0 | 5481 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5482 | `				if( rc == SXERR_ABORT ){` |
|       - | 5483 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5484 | `					return SXERR_ABORT;` |
|       - | 5485 | `				}` |
|     ! 0 | 5486 | `			}` |
|       - | 5487 | `			/* Advance the stream cursor */` |
|       3 | 5488 | `			pGen->pIn++;` |
|       1 | 5489 | `		}` |
|       1 | 5490 | `	}` |
|    8008 | 5491 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5492 | `		/* Syntax error */` |
|     ! 0 | 5493 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5494 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5495 | `		if( rc == SXERR_ABORT ){` |
|       - | 5496 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5497 | `			return SXERR_ABORT;` |
|       - | 5498 | `		}` |
|     ! 0 | 5499 | `		return SXRET_OK;` |
|       - | 5500 | `	}` |
|    8008 | 5501 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8008 | 5502 | `	pEnd = 0; /* cc warning */` |
|       - | 5503 | `	/* Delimit the interface body */` |
|    8008 | 5504 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8008 | 5505 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5506 | `		/* Syntax error */` |
|     ! 0 | 5507 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5508 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5509 | `		if( rc == SXERR_ABORT ){` |
|       - | 5510 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5511 | `			return SXERR_ABORT;` |
|       - | 5512 | `		}` |
|     ! 0 | 5513 | `		return SXRET_OK;` |
|       - | 5514 | `	}` |
|       - | 5515 | `	/* Swap token stream */` |
|    8008 | 5516 | `	pTmp = pGen->pEnd;` |
|    8008 | 5517 | `	pGen->pEnd = pEnd;` |
|       - | 5518 | `	/* Start the parse process` |
|       - | 5519 | `	 * Note (According to the PHP reference manual):` |
|       - | 5520 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5521 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5522 | `	 */` |
|   14646 | 5523 | `	for(;;){` |
|       - | 5524 | `		/* Jump leading/trailing semi-colons */` |
|   50580 | 5525 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   21288 | 5526 | `			pGen->pIn++;` |
|       2 | 5527 | `		}` |
|   29294 | 5528 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5529 | `			/* End of interface body */` |
|    8006 | 5530 | `			break;` |
|       - | 5531 | `		}` |
|   21290 | 5532 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5533 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5534 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5535 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5536 | `			if( rc == SXERR_ABORT ){` |
|       - | 5537 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5538 | `				return SXERR_ABORT;` |
|       - | 5539 | `			}` |
|     ! 0 | 5540 | `			goto done;` |
|       - | 5541 | `		}` |
|       - | 5542 | `		/* Extract the current keyword */` |
|   21290 | 5543 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21290 | 5544 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5545 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 5546 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 5547 | `			const char *zKind = "member";` |
|       3 | 5548 | `			SyString *pMemberName = 0;` |
|       3 | 5549 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 5550 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 5551 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 5552 | `					zKind = "constant";` |
|       3 | 5553 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 5554 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 5555 | `					}` |
|       1 | 5556 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5557 | `					zKind = "method";` |
|     ! 0 | 5558 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 5559 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 5560 | `					}` |
|     ! 0 | 5561 | `				}` |
|       1 | 5562 | `			}` |
|       3 | 5563 | `			if( pMemberName ){` |
|       4 | 5564 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 5565 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 5566 | `			}else{` |
|     ! 0 | 5567 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5568 | `					"Access type for interface %s must be public",zKind);` |
|       - | 5569 | `			}` |
|       3 | 5570 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5571 | `				return SXERR_ABORT;` |
|       - | 5572 | `			}` |
|       3 | 5573 | `			goto done;` |
|       - | 5574 | `		}` |
|   21288 | 5575 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5577 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5578 | `			if( rc == SXERR_ABORT ){` |
|       - | 5579 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5580 | `				return SXERR_ABORT;` |
|       - | 5581 | `			}` |
|     ! 0 | 5582 | `			goto done;` |
|       - | 5583 | `		}` |
|   21288 | 5584 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5585 | `			/* Advance the stream cursor */` |
|   21284 | 5586 | `			pGen->pIn++;` |
|   21284 | 5587 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5588 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5589 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5590 | `				if( rc == SXERR_ABORT ){` |
|       - | 5591 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5592 | `					return SXERR_ABORT;` |
|       - | 5593 | `				}` |
|     ! 0 | 5594 | `				goto done;` |
|       - | 5595 | `			}` |
|   21284 | 5596 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21284 | 5597 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5598 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5599 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5600 | `				if( rc == SXERR_ABORT ){` |
|       - | 5601 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5602 | `					return SXERR_ABORT;` |
|       - | 5603 | `				}` |
|     ! 0 | 5604 | `				goto done;` |
|       - | 5605 | `			}` |
|   10641 | 5606 | `		}` |
|   21288 | 5607 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5608 | `			/* Parse constant */` |
|       3 | 5609 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5610 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5611 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5612 | `					return SXERR_ABORT;` |
|       - | 5613 | `				}` |
|     ! 0 | 5614 | `				goto done;` |
|       - | 5615 | `			}` |
|       2 | 5616 | `		}else{` |
|   21286 | 5617 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   21286 | 5618 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5619 | `				/* Static method,record that */` |
|     ! 0 | 5620 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5621 | `				/* Advance the stream cursor */` |
|     ! 0 | 5622 | `				pGen->pIn++;` |
|     ! 0 | 5623 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5624 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5625 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5626 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5627 | `						if( rc == SXERR_ABORT ){` |
|       - | 5628 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5629 | `							return SXERR_ABORT;` |
|       - | 5630 | `						}` |
|     ! 0 | 5631 | `						goto done;` |
|       - | 5632 | `				}` |
|     ! 0 | 5633 | `			}` |
|       - | 5634 | `			/* Process method signature (no body for interface methods) */` |
|   21286 | 5635 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   21286 | 5636 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5637 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5638 | `					return SXERR_ABORT;` |
|       - | 5639 | `				}` |
|     ! 0 | 5640 | `				goto done;` |
|       - | 5641 | `			}` |
|       - | 5642 | `		}` |
|       2 | 5643 | `	}` |
|       - | 5644 | `	/* Install the interface */` |
|    8006 | 5645 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8006 | 5646 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5647 | `		/* Inherit from the base interface */` |
|       3 | 5648 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5649 | `	}` |
|    8006 | 5650 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5651 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5652 | `		return SXERR_ABORT;` |
|       - | 5653 | `	}` |
|    4002 | 5654 | `done:` |
|       - | 5655 | `	/* Point beyond the interface body */` |
|    8008 | 5656 | `	pGen->pIn  = &pEnd[1];` |
|    8008 | 5657 | `	pGen->pEnd = pTmp;` |
|    8008 | 5658 | `	return PH7_OK;` |
|    4005 | 5659 |  |
|       - | 5660 | `/*` |
|       - | 5661 | ` * Compile a user-defined class.` |
|       - | 5662 | ` * According to the PHP language reference manual` |
|       - | 5663 | ` *  class` |
|       - | 5664 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5665 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5666 | ` *  of the properties and methods belonging to the class.` |
|       - | 5667 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5668 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5669 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5670 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5671 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5672 | ` *  (called "methods").` |
|       - | 5673 | ` */` |
|       - | 5674 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5675 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5676 | `struct TraitUseEntry {` |
|       - | 5677 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5678 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5679 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5680 | `};` |
|       - | 5681 | `/*` |
|       - | 5682 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5683 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5684 | ` */` |
|   37660 | 5685 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5686 |  |
|       - | 5687 | `	ph7_class **apIface;` |
|       - | 5688 | `	sxu32 nIface,i;` |
|       - | 5689 | `	sxi32 rc;` |
|   37662 | 5690 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5691 | `		return SXRET_OK;` |
|       - | 5692 | `	}` |
|   37662 | 5693 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   37662 | 5694 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   40356 | 5695 | `	for(i = 0; i < nIface; i++){` |
|    2696 | 5696 | `		ph7_class *pIface = apIface[i];` |
|       - | 5697 | `		SyHashEntry *pEntry;` |
|    2696 | 5698 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16054 | 5699 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13360 | 5700 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5701 | `			ph7_class_method *pImplMeth;` |
|   13360 | 5702 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5703 | `			/* Find the implementing method in the class */` |
|   13360 | 5704 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13360 | 5705 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5706 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5707 | `			}` |
|       - | 5708 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13346 | 5709 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5710 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5711 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5712 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5713 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5714 | `					return SXERR_ABORT;` |
|       - | 5715 | `				}` |
|       1 | 5716 | `			}` |
|       - | 5717 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5718 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5719 | `			 */` |
|       - | 5720 | `			{` |
|   13346 | 5721 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13346 | 5722 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13346 | 5723 | `				int sigError = 0;` |
|   13346 | 5724 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5725 | `					sigError = 1;` |
|   13345 | 5726 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5727 | `					/* Extra parameters must all have default values */` |
|       5 | 5728 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5729 | `					sxu32 k;` |
|       7 | 5730 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5731 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5732 | `							sigError = 1;` |
|       3 | 5733 | `							break;` |
|       - | 5734 | `						}` |
|       2 | 5735 | `					}` |
|       2 | 5736 | `				}` |
|   13346 | 5737 | `				if( sigError ){` |
|       - | 5738 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5739 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5740 | `					sxu32 j;` |
|       5 | 5741 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5742 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5743 | `					/* Build implementing method signature */` |
|       5 | 5744 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5745 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5746 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5747 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5748 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5749 | `					}` |
|       - | 5750 | `					/* Build interface method signature */` |
|       5 | 5751 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5752 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5753 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5754 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5755 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5756 | `					}` |
|       7 | 5757 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5758 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5759 | `						&pClass->sName,pMName,` |
|       4 | 5760 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5761 | `						&pIface->sName,pMName,` |
|       4 | 5762 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5763 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5764 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5765 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5766 | `						return SXERR_ABORT;` |
|       - | 5767 | `					}` |
|       2 | 5768 | `				}` |
|       - | 5769 | `			}` |
|       2 | 5770 | `		}` |
|    1349 | 5771 | `	}` |
|   37662 | 5772 | `	return SXRET_OK;` |
|   18832 | 5773 |  |
|       - | 5774 | `/*` |
|       - | 5775 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5776 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5777 | ` */` |
|   37660 | 5778 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5779 |  |
|       - | 5780 | `	ph7_class_method *pMeth;` |
|       - | 5781 | `	SyHashEntry *pEntry;` |
|       - | 5782 | `	sxu32 nAbstract;` |
|       - | 5783 | `	SyBlob sMsg;` |
|       - | 5784 | `	sxi32 rc;` |
|       - | 5785 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   37662 | 5786 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5787 | `		return SXRET_OK;` |
|       - | 5788 | `	}` |
|       - | 5789 | `	/* Count abstract methods */` |
|   37644 | 5790 | `	nAbstract = 0;` |
|   37644 | 5791 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  357226 | 5792 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  319584 | 5793 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  319584 | 5794 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5795 | `			nAbstract++;` |
|       8 | 5796 | `		}` |
|       2 | 5797 | `	}` |
|   37644 | 5798 | `	if( nAbstract == 0 ){` |
|   37630 | 5799 | `		return SXRET_OK;` |
|       - | 5800 | `	}` |
|       - | 5801 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5802 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5803 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5804 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5805 | `		&pClass->sName,nAbstract,` |
|       7 | 5806 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5807 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5808 | `	/* Second pass: list methods with origins */` |
|       - | 5809 | `	{` |
|      15 | 5810 | `		sxu32 nListed = 0;` |
|      15 | 5811 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5812 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5813 | `			ph7_class *pOrigin = 0;` |
|       - | 5814 | `			SyString *pMName;` |
|      19 | 5815 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5816 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5817 | `				continue;` |
|       - | 5818 | `			}` |
|      17 | 5819 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5820 | `			if( nListed > 0 ){` |
|       3 | 5821 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5822 | `			}` |
|       - | 5823 | `			/* Find the origin of this abstract method.` |
|       - | 5824 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5825 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5826 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5827 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5828 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5829 | `			 * class's namespace.` |
|       - | 5830 | `			 */` |
|       - | 5831 | `			{` |
|       - | 5832 | `				ph7_class **apIface;` |
|       - | 5833 | `				ph7_class **apTrait;` |
|       - | 5834 | `				ph7_class *pWalk;` |
|       - | 5835 | `				sxu32 i;` |
|       - | 5836 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5837 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5838 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5839 | `				 */` |
|      17 | 5840 | `				if( pClass->pBase ){` |
|       9 | 5841 | `					pWalk = pClass->pBase;` |
|      17 | 5842 | `					while( pWalk ){` |
|       - | 5843 | `						ph7_class_method *pParentMeth;` |
|      11 | 5844 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5845 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5846 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5847 | `							 * in this class's ancestor chain.` |
|       - | 5848 | `							 */` |
|      11 | 5849 | `							int fromIface = 0;` |
|      11 | 5850 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5851 | `							while( pAnc ){` |
|       - | 5852 | `								ph7_class **apPI;` |
|       - | 5853 | `								sxu32 j;` |
|      13 | 5854 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5855 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5856 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5857 | `										fromIface = 1;` |
|       9 | 5858 | `										break;` |
|       - | 5859 | `									}` |
|     ! 0 | 5860 | `								}` |
|      13 | 5861 | `								if( fromIface ) break;` |
|       5 | 5862 | `								pAnc = pAnc->pBase;` |
|       1 | 5863 | `							}` |
|      11 | 5864 | `							if( !fromIface ){` |
|       3 | 5865 | `								pOrigin = pWalk;` |
|       3 | 5866 | `								break;` |
|       - | 5867 | `							}` |
|       4 | 5868 | `						}` |
|       9 | 5869 | `						pWalk = pWalk->pBase;` |
|       1 | 5870 | `					}` |
|       4 | 5871 | `				}` |
|       - | 5872 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5873 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5874 | `				 */` |
|      17 | 5875 | `				if( !pOrigin ){` |
|      15 | 5876 | `					pWalk = pClass;` |
|      37 | 5877 | `					while( pWalk && !pOrigin ){` |
|      23 | 5878 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5879 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5880 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5881 | `							ph7_class *pDeepest = 0;` |
|      25 | 5882 | `							while( pIface ){` |
|      13 | 5883 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5884 | `									pDeepest = pIface;` |
|       6 | 5885 | `								}` |
|      13 | 5886 | `								pIface = pIface->pBase;` |
|       1 | 5887 | `							}` |
|      13 | 5888 | `							if( pDeepest ){` |
|      13 | 5889 | `								pOrigin = pDeepest;` |
|      13 | 5890 | `								break;` |
|       - | 5891 | `							}` |
|     ! 0 | 5892 | `						}` |
|      23 | 5893 | `						pWalk = pWalk->pBase;` |
|       1 | 5894 | `					}` |
|       7 | 5895 | `				}` |
|       - | 5896 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5897 | `				if( !pOrigin ){` |
|       3 | 5898 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5899 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5900 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5901 | `							pOrigin = pClass;` |
|       3 | 5902 | `							break;` |
|       - | 5903 | `						}` |
|     ! 0 | 5904 | `					}` |
|       1 | 5905 | `				}` |
|       - | 5906 | `			}` |
|      17 | 5907 | `			if( pOrigin ){` |
|      17 | 5908 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5909 | `			}else{` |
|       - | 5910 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5911 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5912 | `			}` |
|      17 | 5913 | `			nListed++;` |
|       1 | 5914 | `		}` |
|       - | 5915 | `	}` |
|      15 | 5916 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5917 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5918 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5919 | `	SyBlobRelease(&sMsg);` |
|      15 | 5920 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5921 | `		return SXERR_ABORT;` |
|       - | 5922 | `	}` |
|      15 | 5923 | `	return SXRET_OK;` |
|   18832 | 5924 |  |
|   37664 | 5925 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5926 |  |
|   37666 | 5927 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5928 | `	ph7_class *pClass,*pBase;` |
|       - | 5929 | `	SyToken *pEnd,*pTmp;` |
|       - | 5930 | `	sxi32 iProtection;` |
|       - | 5931 | `	SySet aInterfaces;` |
|       - | 5932 | `	SySet aUseEntries;` |
|       - | 5933 | `	sxi32 iAttrflags;` |
|       - | 5934 | `	SyString *pName;` |
|       - | 5935 | `	sxi32 nKwrd;` |
|       - | 5936 | `	sxi32 rc;` |
|       - | 5937 | `	/* Jump the 'class' keyword */` |
|   37666 | 5938 | `	pGen->pIn++;` |
|   37666 | 5939 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5940 | `		/* Syntax error */` |
|     ! 0 | 5941 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5942 | `		if( rc == SXERR_ABORT ){` |
|       - | 5943 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5944 | `			return SXERR_ABORT;` |
|       - | 5945 | `		}` |
|       - | 5946 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5947 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5948 | `			pGen->pIn++;` |
|     ! 0 | 5949 | `		}` |
|     ! 0 | 5950 | `		return SXRET_OK;` |
|       - | 5951 | `	}` |
|       - | 5952 | `	/* Extract class name */` |
|   37666 | 5953 | `	pName = &pGen->pIn->sData;` |
|       - | 5954 | `	/* Advance the stream cursor */` |
|   37666 | 5955 | `	pGen->pIn++;` |
|       - | 5956 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5957 | `		SyBlob sFQN;` |
|       - | 5958 | `		SyString sFQNStr;` |
|   37666 | 5959 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   37666 | 5960 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   37666 | 5961 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   37666 | 5962 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   37666 | 5963 | `		SyBlobRelease(&sFQN);` |
|       - | 5964 | `	}` |
|   37666 | 5965 | `	if( pClass == 0 ){` |
|     ! 0 | 5966 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5967 | `		return SXERR_ABORT;` |
|       - | 5968 | `	}` |
|       - | 5969 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   37666 | 5970 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   37666 | 5971 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5972 | `	/* Assume a standalone class */` |
|   37666 | 5973 | `	pBase = 0;` |
|   37666 | 5974 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5975 | `		SyString *pBaseName;` |
|   26710 | 5976 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26710 | 5977 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   24018 | 5978 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   24018 | 5979 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5980 | `				/* Syntax error */` |
|     ! 0 | 5981 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5982 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5983 | `					pName);` |
|     ! 0 | 5984 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5985 | `				if( rc == SXERR_ABORT ){` |
|       - | 5986 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5987 | `					return SXERR_ABORT;` |
|       - | 5988 | `				}` |
|     ! 0 | 5989 | `				return SXRET_OK;` |
|       - | 5990 | `			}` |
|       - | 5991 | `			/* Extract base class name and resolve through namespace/imports */` |
|   24018 | 5992 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5993 | `			{` |
|       - | 5994 | `				SyBlob sResolved;` |
|   24018 | 5995 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   24018 | 5996 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   36026 | 5997 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   24016 | 5998 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   24018 | 5999 | `				SyBlobRelease(&sResolved);` |
|       - | 6000 | `			}` |
|       - | 6001 | `			/* Interfaces are not allowed */` |
|   24018 | 6002 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 6003 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6004 | `			}` |
|   24018 | 6005 | `			if( pBase == 0 ){` |
|       - | 6006 | `				/* Inexistant base class */` |
|     ! 0 | 6007 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 6008 | `				if( rc == SXERR_ABORT ){` |
|       - | 6009 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6010 | `					return SXERR_ABORT;` |
|       - | 6011 | `				}` |
|     ! 0 | 6012 | `			}else{` |
|   24018 | 6013 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 6014 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 6015 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 6016 | `					if( rc == SXERR_ABORT ){` |
|       - | 6017 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6018 | `						return SXERR_ABORT;` |
|       - | 6019 | `					}` |
|     ! 0 | 6020 | `				}` |
|       - | 6021 | `			}` |
|       - | 6022 | `			/* Advance the stream cursor */` |
|   24018 | 6023 | `			pGen->pIn++;` |
|   12008 | 6024 | `		}` |
|   26710 | 6025 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 6026 | `			ph7_class *pInterface;` |
|       - | 6027 | `			SyString *pIntName;` |
|       - | 6028 | `			/* Interface implementation */` |
|    2696 | 6029 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1347 | 6030 | `			for(;;){` |
|    2696 | 6031 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6032 | `					/* Syntax error */` |
|     ! 0 | 6033 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6034 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 6035 | `						pName);` |
|     ! 0 | 6036 | `					if( rc == SXERR_ABORT ){` |
|       - | 6037 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6038 | `						return SXERR_ABORT;` |
|       - | 6039 | `					}` |
|     ! 0 | 6040 | `					break;` |
|       - | 6041 | `				}` |
|       - | 6042 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2696 | 6043 | `				pIntName = &pGen->pIn->sData;` |
|       - | 6044 | `				{` |
|       - | 6045 | `					SyBlob sResolved;` |
|    2696 | 6046 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2696 | 6047 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5390 | 6048 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2694 | 6049 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2696 | 6050 | `					SyBlobRelease(&sResolved);` |
|       - | 6051 | `				}` |
|       - | 6052 | `				/* Only interfaces are allowed */` |
|    2696 | 6053 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6054 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 6055 | `				}` |
|    2696 | 6056 | `				if( pInterface == 0 ){` |
|       - | 6057 | `					/* Inexistant interface */` |
|     ! 0 | 6058 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 6059 | `					if( rc == SXERR_ABORT ){` |
|       - | 6060 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6061 | `						return SXERR_ABORT;` |
|       - | 6062 | `					}` |
|     ! 0 | 6063 | `				}else{` |
|       - | 6064 | `					/* Register interface */` |
|    2696 | 6065 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 6066 | `				}` |
|       - | 6067 | `				/* Advance the stream cursor */` |
|    2696 | 6068 | `				pGen->pIn++;` |
|    2696 | 6069 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1349 | 6070 | `					break;` |
|       - | 6071 | `				}` |
|     ! 0 | 6072 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 6073 | `			}` |
|    1347 | 6074 | `		}` |
|   13354 | 6075 | `	}` |
|   37666 | 6076 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6077 | `		/* Syntax error */` |
|     ! 0 | 6078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 6079 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6080 | `		if( rc == SXERR_ABORT ){` |
|       - | 6081 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6082 | `			return SXERR_ABORT;` |
|       - | 6083 | `		}` |
|     ! 0 | 6084 | `		return SXRET_OK;` |
|       - | 6085 | `	}` |
|   37666 | 6086 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   37666 | 6087 | `	pEnd = 0; /* cc warning */` |
|       - | 6088 | `	/* Delimit the class body */` |
|   37666 | 6089 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   37666 | 6090 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6091 | `		/* Syntax error */` |
|     ! 0 | 6092 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 6093 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6094 | `		if( rc == SXERR_ABORT ){` |
|       - | 6095 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6096 | `			return SXERR_ABORT;` |
|       - | 6097 | `		}` |
|     ! 0 | 6098 | `		return SXRET_OK;` |
|       - | 6099 | `	}` |
|       - | 6100 | `	/* Swap token stream */` |
|   37666 | 6101 | `	pTmp = pGen->pEnd;` |
|   37666 | 6102 | `	pGen->pEnd = pEnd;` |
|       - | 6103 | `	/* Set the inherited flags */` |
|   37666 | 6104 | `	pClass->iFlags = iFlags;` |
|       - | 6105 | `	/* Start the parse process */` |
|   72165 | 6106 | `	for(;;){` |
|       - | 6107 | `		/* Jump leading/trailing semi-colons */` |
|  213906 | 6108 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   34806 | 6109 | `			pGen->pIn++;` |
|       2 | 6110 | `		}` |
|  179102 | 6111 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6112 | `			/* End of class body */` |
|   37662 | 6113 | `			break;` |
|       - | 6114 | `		}` |
|  141442 | 6115 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6116 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6117 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6118 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6119 | `			if( rc == SXERR_ABORT ){` |
|       - | 6120 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6121 | `				return SXERR_ABORT;` |
|       - | 6122 | `			}` |
|     ! 0 | 6123 | `			goto done;` |
|       - | 6124 | `		}` |
|       - | 6125 | `		/* Assume public visibility */` |
|  141442 | 6126 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  141442 | 6127 | `		iAttrflags = 0;` |
|  141442 | 6128 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 6129 | `			/* Extract the current keyword */` |
|  141442 | 6130 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  141442 | 6131 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6132 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 6133 | `				TraitUseEntry sUse;` |
|      41 | 6134 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 6135 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 6136 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 6137 | `				for(;;){` |
|       - | 6138 | `					ph7_class *pTrait;` |
|       - | 6139 | `					SyString *pTraitName;` |
|      49 | 6140 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6141 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6142 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 6143 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6144 | `							return SXERR_ABORT;` |
|       - | 6145 | `						}` |
|     ! 0 | 6146 | `						break;` |
|       - | 6147 | `					}` |
|      49 | 6148 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 6149 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 6150 | `						SyBlob sResolved;` |
|      49 | 6151 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 6152 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 6153 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 6154 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 6155 | `						SyBlobRelease(&sResolved);` |
|       - | 6156 | `					}` |
|       - | 6157 | `					/* Only traits are allowed */` |
|      49 | 6158 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6159 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 6160 | `					}` |
|      49 | 6161 | `					if( pTrait == 0 ){` |
|     ! 0 | 6162 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6163 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 6164 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6165 | `							return SXERR_ABORT;` |
|       - | 6166 | `						}` |
|     ! 0 | 6167 | `					}else{` |
|      49 | 6168 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 6169 | `					}` |
|      49 | 6170 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 6171 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 6172 | `						break;` |
|       - | 6173 | `					}` |
|       9 | 6174 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 6175 | `				}` |
|       - | 6176 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 6177 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 6178 | `					SyToken *pBlock;` |
|       9 | 6179 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 6180 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 6181 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 6182 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 6183 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 6184 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 6185 | `					}else{` |
|     ! 0 | 6186 | `						pGen->pIn = pGen->pEnd;` |
|       - | 6187 | `					}` |
|       4 | 6188 | `				}` |
|      41 | 6189 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 6190 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 6191 | `				continue;` |
|       - | 6192 | `			}` |
|  141402 | 6193 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  138636 | 6194 | `				iProtection = nKwrd;` |
|  138636 | 6195 | `				pGen->pIn++; /* Jump the visibility token */` |
|  138636 | 6196 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6197 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6198 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6199 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6200 | `					if( rc == SXERR_ABORT ){` |
|       - | 6201 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6202 | `						return SXERR_ABORT;` |
|       - | 6203 | `					}` |
|     ! 0 | 6204 | `					goto done;` |
|       - | 6205 | `				}` |
|  138636 | 6206 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6207 | `					/* Attribute declaration */` |
|   34730 | 6208 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   34730 | 6209 | `					if( rc != SXRET_OK ){` |
|       3 | 6210 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6211 | `							return SXERR_ABORT;` |
|       - | 6212 | `						}` |
|       3 | 6213 | `						goto done;` |
|       - | 6214 | `					}` |
|   34728 | 6215 | `					continue;` |
|       - | 6216 | `				}` |
|       - | 6217 | `				/* Extract the keyword */` |
|  103908 | 6218 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51953 | 6219 | `			}` |
|  106674 | 6220 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6221 | `				/* Process constant declaration */` |
|      30 | 6222 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 6223 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6224 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6225 | `						return SXERR_ABORT;` |
|       - | 6226 | `					}` |
|     ! 0 | 6227 | `					goto done;` |
|       - | 6228 | `				}` |
|      16 | 6229 | `			}else{` |
|  106646 | 6230 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6231 | `					/* Static method or attribute,record that */` |
|    2684 | 6232 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2684 | 6233 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2684 | 6234 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6235 | `						/* Extract the keyword */` |
|    2680 | 6236 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2680 | 6237 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6238 | `							iProtection = nKwrd;` |
|     ! 0 | 6239 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6240 | `						}` |
|    1339 | 6241 | `					}` |
|    2684 | 6242 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6243 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6244 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6245 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6246 | `						if( rc == SXERR_ABORT ){` |
|       - | 6247 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6248 | `							return SXERR_ABORT;` |
|       - | 6249 | `						}` |
|     ! 0 | 6250 | `						goto done;` |
|       - | 6251 | `					}` |
|    2684 | 6252 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6253 | `						/* Attribute declaration */` |
|       5 | 6254 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6255 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6256 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6257 | `								return SXERR_ABORT;` |
|       - | 6258 | `							}` |
|     ! 0 | 6259 | `							goto done;` |
|       - | 6260 | `						}` |
|       5 | 6261 | `						continue;` |
|       - | 6262 | `					}` |
|       - | 6263 | `					/* Extract the keyword */` |
|    2680 | 6264 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  105303 | 6265 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6266 | `					/* Abstract method,record that */` |
|      10 | 6267 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6268 | `					/* Mark the whole class as abstract */` |
|      10 | 6269 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6270 | `					/* Advance the stream cursor */` |
|      10 | 6271 | `					pGen->pIn++;` |
|      10 | 6272 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6273 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6274 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6275 | `							iProtection = nKwrd;` |
|       8 | 6276 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6277 | `						}` |
|       4 | 6278 | `					}` |
|      10 | 6279 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6280 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6281 | `							/* Static method */` |
|     ! 0 | 6282 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6283 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6284 | `					}` |
|      10 | 6285 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6286 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6287 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6288 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6289 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6290 | `							if( rc == SXERR_ABORT ){` |
|       - | 6291 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6292 | `								return SXERR_ABORT;` |
|       - | 6293 | `							}` |
|     ! 0 | 6294 | `							goto done;` |
|       - | 6295 | `					}` |
|      10 | 6296 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  103960 | 6297 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6298 | `					/* final method ,record that */` |
|       5 | 6299 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6300 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6301 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6302 | `						/* Extract the keyword */` |
|       5 | 6303 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6304 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6305 | `							iProtection = nKwrd;` |
|       5 | 6306 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6307 | `						}` |
|       2 | 6308 | `					}` |
|       5 | 6309 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6310 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6311 | `							/* Static method */` |
|     ! 0 | 6312 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6313 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6314 | `					}` |
|       5 | 6315 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6316 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6317 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6318 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6319 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6320 | `							if( rc == SXERR_ABORT ){` |
|       - | 6321 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6322 | `								return SXERR_ABORT;` |
|       - | 6323 | `							}` |
|     ! 0 | 6324 | `							goto done;` |
|       - | 6325 | `					}` |
|       5 | 6326 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6327 | `				}` |
|  106642 | 6328 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6329 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6330 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6331 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6332 | `						if( rc == SXERR_ABORT ){` |
|       - | 6333 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6334 | `							return SXERR_ABORT;` |
|       - | 6335 | `						}` |
|     ! 0 | 6336 | `						goto done;` |
|       - | 6337 | `				}` |
|  106642 | 6338 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6339 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6340 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6341 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6342 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6343 | `						if( rc == SXERR_ABORT ){` |
|       - | 6344 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6345 | `							return SXERR_ABORT;` |
|       - | 6346 | `						}` |
|     ! 0 | 6347 | `						goto done;` |
|       - | 6348 | `					}` |
|       - | 6349 | `					/* Attribute declaration */` |
|       7 | 6350 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6351 | `				}else{` |
|       - | 6352 | `					/* Process method declaration */` |
|  106636 | 6353 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6354 | `				}` |
|  106642 | 6355 | `				if( rc != SXRET_OK ){` |
|       3 | 6356 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6357 | `						return SXERR_ABORT;` |
|       - | 6358 | `					}` |
|       3 | 6359 | `					goto done;` |
|       - | 6360 | `				}` |
|       - | 6361 | `			}` |
|   53335 | 6362 | `		}else{` |
|       - | 6363 | `			/* Attribute declaration */` |
|     ! 0 | 6364 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6365 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6366 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6367 | `					return SXERR_ABORT;` |
|       - | 6368 | `				}` |
|     ! 0 | 6369 | `				goto done;` |
|       - | 6370 | `			}` |
|       - | 6371 | `		}` |
|       2 | 6372 | `	}` |
|       - | 6373 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6374 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6375 | `	 */` |
|       - | 6376 | `	{` |
|       - | 6377 | `		TraitUseEntry *apUse;` |
|       - | 6378 | `		sxu32 nU;` |
|   37662 | 6379 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   37702 | 6380 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6381 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6382 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6383 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6384 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6385 | `			sxu32 nT;` |
|      41 | 6386 | `			if( !hasResolution ){` |
|       - | 6387 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6388 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6389 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6390 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6391 | `						break;` |
|       - | 6392 | `					}` |
|      20 | 6393 | `				}` |
|      17 | 6394 | `			}else{` |
|       - | 6395 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6396 | `				 * then use the block to resolve method conflicts.` |
|       - | 6397 | `				 */` |
|       - | 6398 | `				SyToken *pR;` |
|      19 | 6399 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6400 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6401 | `					ph7_class_attr *pAR;` |
|       - | 6402 | `					SyHashEntry *pER;` |
|       - | 6403 | `					SyString *pNR;` |
|      11 | 6404 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6405 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6406 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6407 | `						pNR = &pAR->sName;` |
|     ! 0 | 6408 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6409 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6410 | `						}` |
|     ! 0 | 6411 | `					}` |
|      11 | 6412 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6413 | `				}` |
|       - | 6414 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6415 | `				pR = pUse->pResolvStart;` |
|      21 | 6416 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6417 | `					SyString sTrait,sMethod;` |
|       - | 6418 | `					ph7_class *pSrcTrait;` |
|       - | 6419 | `					ph7_class_method *pMeth;` |
|       - | 6420 | `					sxi32 nRKwrd;` |
|      33 | 6421 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6422 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6423 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6424 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6425 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6426 | `					sMethod = pR->sData;` |
|      13 | 6427 | `					pR++;` |
|      13 | 6428 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6429 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6430 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6431 | `							sTrait = sMethod;` |
|       7 | 6432 | `							pR++;` |
|       7 | 6433 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6434 | `							sMethod = pR->sData;` |
|       7 | 6435 | `							pR++;` |
|       3 | 6436 | `						}` |
|       3 | 6437 | `					}` |
|      13 | 6438 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6439 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6440 | `						continue;` |
|       - | 6441 | `					}` |
|      13 | 6442 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6443 | `					pR++;` |
|      13 | 6444 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6445 | `						pSrcTrait = 0;` |
|       7 | 6446 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6447 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6448 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6449 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6450 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6451 | `								break;` |
|       - | 6452 | `							}` |
|       2 | 6453 | `						}` |
|       5 | 6454 | `						if( pSrcTrait ){` |
|       5 | 6455 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6456 | `							if( pMeth ){` |
|       5 | 6457 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6458 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6459 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6460 | `								}` |
|       2 | 6461 | `							}` |
|       2 | 6462 | `						}` |
|       2 | 6463 | `					}` |
|      29 | 6464 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6465 | `				}` |
|       - | 6466 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6467 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6468 | `					ph7_class_method *pMR;` |
|       - | 6469 | `					SyHashEntry *pER;` |
|       - | 6470 | `					SyString *pNR;` |
|      11 | 6471 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6472 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6473 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6474 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6475 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6476 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6477 | `						}` |
|       1 | 6478 | `					}` |
|       6 | 6479 | `				}` |
|       - | 6480 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6481 | `				pR = pUse->pResolvStart;` |
|      21 | 6482 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6483 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6484 | `					ph7_class *pSrcTrait;` |
|       - | 6485 | `					ph7_class_method *pMeth;` |
|      21 | 6486 | `					int hasQual = 0;` |
|       - | 6487 | `					sxi32 nRKwrd;` |
|      33 | 6488 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6489 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6490 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6491 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6492 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6493 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6494 | `					sMethod = pR->sData;` |
|      13 | 6495 | `					pR++;` |
|      13 | 6496 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6497 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6498 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6499 | `							sTrait = sMethod;` |
|       7 | 6500 | `							hasQual = 1;` |
|       7 | 6501 | `							pR++;` |
|       7 | 6502 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6503 | `							sMethod = pR->sData;` |
|       7 | 6504 | `							pR++;` |
|       3 | 6505 | `						}` |
|       3 | 6506 | `					}` |
|      13 | 6507 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6508 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6509 | `						continue;` |
|       - | 6510 | `					}` |
|      13 | 6511 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6512 | `					pR++;` |
|      13 | 6513 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6514 | `						sxi32 iNewVis = -1;` |
|       9 | 6515 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6516 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6517 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6518 | `								iNewVis = nAK;` |
|       7 | 6519 | `								pR++;` |
|       3 | 6520 | `							}` |
|       3 | 6521 | `						}` |
|       9 | 6522 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6523 | `							sAlias = pR->sData;` |
|       7 | 6524 | `							pR++;` |
|       3 | 6525 | `						}` |
|       9 | 6526 | `						pMeth = 0;` |
|       9 | 6527 | `						if( hasQual ){` |
|       3 | 6528 | `							pSrcTrait = 0;` |
|       5 | 6529 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6530 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6531 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6532 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6533 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6534 | `									break;` |
|       - | 6535 | `								}` |
|       2 | 6536 | `							}` |
|       3 | 6537 | `							if( pSrcTrait ){` |
|       3 | 6538 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6539 | `							}` |
|       2 | 6540 | `						}else{` |
|       7 | 6541 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6542 | `						}` |
|       9 | 6543 | `						if( pMeth ){` |
|       9 | 6544 | `							if( sAlias.nByte > 0 ){` |
|       - | 6545 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6546 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6547 | `								 */` |
|       - | 6548 | `								ph7_class_method *pAlias;` |
|       - | 6549 | `								char *zAliasDup;` |
|       7 | 6550 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6551 | `								if( pAlias ){` |
|       7 | 6552 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6553 | `									if( iNewVis >= 0 ){` |
|       5 | 6554 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6555 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6556 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6557 | `									}` |
|       7 | 6558 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6559 | `									if( zAliasDup ){` |
|       7 | 6560 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6561 | `									}` |
|       4 | 6562 | `								}` |
|       6 | 6563 | `							}else if( iNewVis >= 0 ){` |
|       - | 6564 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6565 | `								ph7_class_method *pCopy;` |
|       3 | 6566 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6567 | `								if( pCopy ){` |
|       3 | 6568 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6569 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6570 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6571 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6572 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6573 | `									/* Replace the method in the class hash */` |
|       3 | 6574 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6575 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6576 | `								}` |
|       1 | 6577 | `							}` |
|       4 | 6578 | `						}` |
|       4 | 6579 | `						SXUNUSED(hasQual);` |
|       4 | 6580 | `					}` |
|      17 | 6581 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6582 | `				}` |
|       - | 6583 | `			}` |
|      41 | 6584 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6585 | `		}` |
|       - | 6586 | `	}` |
|       - | 6587 | `	/* Install the class */` |
|   37662 | 6588 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   37662 | 6589 | `	if( rc == SXRET_OK ){` |
|       - | 6590 | `		ph7_class **apInterface;` |
|       - | 6591 | `		sxu32 n;` |
|   37662 | 6592 | `		if( pBase ){` |
|       - | 6593 | `			/* Inherit from base class and mark as a subclass */` |
|   24018 | 6594 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12008 | 6595 | `		}` |
|   37662 | 6596 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   40356 | 6597 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6598 | `			/* Implements one or more interface */` |
|    2696 | 6599 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2696 | 6600 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6601 | `				break;` |
|       - | 6602 | `			}` |
|    1349 | 6603 | `		}` |
|       - | 6604 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   37662 | 6605 | `		if( rc == SXRET_OK ){` |
|   37662 | 6606 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   37662 | 6607 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6608 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6609 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6610 | `				return SXERR_ABORT;` |
|       - | 6611 | `			}` |
|   18830 | 6612 | `		}` |
|       - | 6613 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   37662 | 6614 | `		if( rc == SXRET_OK ){` |
|   37662 | 6615 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   37662 | 6616 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6617 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6618 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6619 | `				return SXERR_ABORT;` |
|       - | 6620 | `			}` |
|   18830 | 6621 | `		}` |
|   18830 | 6622 | `	}` |
|   37662 | 6623 | `	SySetRelease(&aUseEntries);` |
|   37662 | 6624 | `	SySetRelease(&aInterfaces);` |
|   37662 | 6625 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6626 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6627 | `		return SXERR_ABORT;` |
|       - | 6628 | `	}` |
|   18830 | 6629 | `done:` |
|       - | 6630 | `	/* Point beyond the class body */` |
|   37666 | 6631 | `	pGen->pIn = &pEnd[1];` |
|   37666 | 6632 | `	pGen->pEnd = pTmp;` |
|   37666 | 6633 | `	return PH7_OK;` |
|   18834 | 6634 |  |
|       - | 6635 | `/*` |
|       - | 6636 | ` * Compile a user-defined abstract class.` |
|       - | 6637 | ` *  According to the PHP language reference manual` |
|       - | 6638 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6639 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6640 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6641 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6642 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6643 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6644 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6645 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6646 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6647 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6648 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6649 | ` *   could differ.` |
|       - | 6650 | ` */` |
|      16 | 6651 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6652 |  |
|       - | 6653 | `	sxi32 rc;` |
|      18 | 6654 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6655 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6656 | `	return rc;` |
|       2 | 6657 |  |
|       - | 6658 | `/*` |
|       - | 6659 | ` * Compile a user-defined final class.` |
|       - | 6660 | ` *  According to the PHP language reference manual` |
|       - | 6661 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6662 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6663 | ` *    final then it cannot be extended.` |
|       - | 6664 | ` */` |
|       2 | 6665 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6666 |  |
|       - | 6667 | `	sxi32 rc;` |
|       3 | 6668 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6669 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6670 | `	return rc;` |
|       1 | 6671 |  |
|       - | 6672 | `/*` |
|       - | 6673 | ` * Compile a user-defined trait.` |
|       - | 6674 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6675 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6676 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6677 | ` */` |
|      52 | 6678 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6679 |  |
|      54 | 6680 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6681 | `	ph7_class *pClass;` |
|       - | 6682 | `	SyToken *pEnd,*pTmp;` |
|       - | 6683 | `	sxi32 iProtection;` |
|       - | 6684 | `	sxi32 iAttrflags;` |
|       - | 6685 | `	SyString *pName;` |
|       - | 6686 | `	sxi32 nKwrd;` |
|       - | 6687 | `	sxi32 rc;` |
|       - | 6688 | `	/* Jump the 'trait' keyword */` |
|      54 | 6689 | `	pGen->pIn++;` |
|      54 | 6690 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6691 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6692 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6693 | `			return SXERR_ABORT;` |
|       - | 6694 | `		}` |
|     ! 0 | 6695 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6696 | `			pGen->pIn++;` |
|     ! 0 | 6697 | `		}` |
|     ! 0 | 6698 | `		return SXRET_OK;` |
|       - | 6699 | `	}` |
|       - | 6700 | `	/* Extract trait name */` |
|      54 | 6701 | `	pName = &pGen->pIn->sData;` |
|      54 | 6702 | `	pGen->pIn++;` |
|       - | 6703 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6704 | `		SyBlob sFQN;` |
|       - | 6705 | `		SyString sFQNStr;` |
|      54 | 6706 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6707 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6708 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6709 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6710 | `		SyBlobRelease(&sFQN);` |
|       - | 6711 | `	}` |
|      54 | 6712 | `	if( pClass == 0 ){` |
|     ! 0 | 6713 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6714 | `		return SXERR_ABORT;` |
|       - | 6715 | `	}` |
|       - | 6716 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6717 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6718 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6719 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6720 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6721 | `			return SXERR_ABORT;` |
|       - | 6722 | `		}` |
|     ! 0 | 6723 | `		return SXRET_OK;` |
|       - | 6724 | `	}` |
|      54 | 6725 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6726 | `	pEnd = 0;` |
|      54 | 6727 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6728 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6729 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6730 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6731 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6732 | `			return SXERR_ABORT;` |
|       - | 6733 | `		}` |
|     ! 0 | 6734 | `		return SXRET_OK;` |
|       - | 6735 | `	}` |
|       - | 6736 | `	/* Swap token stream */` |
|      54 | 6737 | `	pTmp = pGen->pEnd;` |
|      54 | 6738 | `	pGen->pEnd = pEnd;` |
|       - | 6739 | `	/* Mark as trait */` |
|      54 | 6740 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6741 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6742 | `	for(;;){` |
|     144 | 6743 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6744 | `			pGen->pIn++;` |
|       1 | 6745 | `		}` |
|     124 | 6746 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6747 | `			break;` |
|       - | 6748 | `		}` |
|      71 | 6749 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6750 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6751 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6752 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6754 | `				return SXERR_ABORT;` |
|       - | 6755 | `			}` |
|     ! 0 | 6756 | `			goto done;` |
|       - | 6757 | `		}` |
|      71 | 6758 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6759 | `		iAttrflags = 0;` |
|      71 | 6760 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6761 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6762 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6763 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6764 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6765 | `				for(;;){` |
|       - | 6766 | `					ph7_class *pUsedTrait;` |
|       - | 6767 | `					SyString *pUsedName;` |
|       5 | 6768 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6769 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6770 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6771 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6772 | `							return SXERR_ABORT;` |
|       - | 6773 | `						}` |
|     ! 0 | 6774 | `						break;` |
|       - | 6775 | `					}` |
|       5 | 6776 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6777 | `					{` |
|       - | 6778 | `						SyBlob sResolved;` |
|       5 | 6779 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6780 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6781 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6782 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6783 | `						SyBlobRelease(&sResolved);` |
|       - | 6784 | `					}` |
|       5 | 6785 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6786 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6787 | `					}` |
|       5 | 6788 | `					if( pUsedTrait == 0 ){` |
|       4 | 6789 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6790 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6791 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6792 | `							return SXERR_ABORT;` |
|       - | 6793 | `						}` |
|       2 | 6794 | `					}else{` |
|       3 | 6795 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6796 | `					}` |
|       5 | 6797 | `					pGen->pIn++;` |
|       5 | 6798 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6799 | `						break;` |
|       - | 6800 | `					}` |
|     ! 0 | 6801 | `					pGen->pIn++;` |
|     ! 0 | 6802 | `				}` |
|       5 | 6803 | `				continue;` |
|       - | 6804 | `			}` |
|      67 | 6805 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6806 | `				iProtection = nKwrd;` |
|      63 | 6807 | `				pGen->pIn++;` |
|      63 | 6808 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6809 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6810 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6811 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6812 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6813 | `						return SXERR_ABORT;` |
|       - | 6814 | `					}` |
|     ! 0 | 6815 | `					goto done;` |
|       - | 6816 | `				}` |
|      63 | 6817 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6818 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6819 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6820 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6821 | `							return SXERR_ABORT;` |
|       - | 6822 | `						}` |
|     ! 0 | 6823 | `						goto done;` |
|       - | 6824 | `					}` |
|      11 | 6825 | `					continue;` |
|       - | 6826 | `				}` |
|      53 | 6827 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6828 | `			}` |
|      57 | 6829 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6830 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6831 | `					"Traits cannot have constants");` |
|     ! 0 | 6832 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6833 | `					return SXERR_ABORT;` |
|       - | 6834 | `				}` |
|     ! 0 | 6835 | `				goto done;` |
|     ! 0 | 6836 | `			}else{` |
|      57 | 6837 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6838 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6839 | `					pGen->pIn++;` |
|       5 | 6840 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6841 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6842 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6843 | `							iProtection = nKwrd;` |
|     ! 0 | 6844 | `							pGen->pIn++;` |
|     ! 0 | 6845 | `						}` |
|       1 | 6846 | `					}` |
|       5 | 6847 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6848 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6849 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6850 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6851 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6852 | `							return SXERR_ABORT;` |
|       - | 6853 | `						}` |
|     ! 0 | 6854 | `						goto done;` |
|       - | 6855 | `					}` |
|       5 | 6856 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6857 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6858 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6859 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6860 | `								return SXERR_ABORT;` |
|       - | 6861 | `							}` |
|     ! 0 | 6862 | `							goto done;` |
|       - | 6863 | `						}` |
|       3 | 6864 | `						continue;` |
|       - | 6865 | `					}` |
|       3 | 6866 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6867 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6868 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6869 | `					pGen->pIn++;` |
|       5 | 6870 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6871 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6872 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6873 | `							iProtection = nKwrd;` |
|       5 | 6874 | `							pGen->pIn++;` |
|       2 | 6875 | `						}` |
|       2 | 6876 | `					}` |
|       5 | 6877 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6878 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6879 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6880 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6881 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6882 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6883 | `							return SXERR_ABORT;` |
|       - | 6884 | `						}` |
|     ! 0 | 6885 | `						goto done;` |
|       - | 6886 | `					}` |
|       5 | 6887 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6888 | `				}` |
|      55 | 6889 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6890 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6891 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6892 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6893 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6894 | `						return SXERR_ABORT;` |
|       - | 6895 | `					}` |
|     ! 0 | 6896 | `					goto done;` |
|       - | 6897 | `				}` |
|      55 | 6898 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6899 | `					pGen->pIn++;` |
|     ! 0 | 6900 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6901 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6902 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6903 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6904 | `							return SXERR_ABORT;` |
|       - | 6905 | `						}` |
|     ! 0 | 6906 | `						goto done;` |
|       - | 6907 | `					}` |
|     ! 0 | 6908 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6909 | `				}else{` |
|      55 | 6910 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6911 | `				}` |
|      55 | 6912 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6913 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6914 | `						return SXERR_ABORT;` |
|       - | 6915 | `					}` |
|     ! 0 | 6916 | `					goto done;` |
|       - | 6917 | `				}` |
|       - | 6918 | `			}` |
|      28 | 6919 | `		}else{` |
|     ! 0 | 6920 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6921 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6922 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6923 | `					return SXERR_ABORT;` |
|       - | 6924 | `				}` |
|     ! 0 | 6925 | `				goto done;` |
|       - | 6926 | `			}` |
|       - | 6927 | `		}` |
|       1 | 6928 | `	}` |
|       - | 6929 | `	/* Install the trait */` |
|      54 | 6930 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6931 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6932 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6933 | `		return SXERR_ABORT;` |
|       - | 6934 | `	}` |
|      26 | 6935 | `done:` |
|       - | 6936 | `	/* Point beyond the trait body */` |
|      54 | 6937 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6938 | `	pGen->pEnd = pTmp;` |
|      54 | 6939 | `	return PH7_OK;` |
|      28 | 6940 |  |
|       - | 6941 | `/*` |
|       - | 6942 | ` * Compile a user-defined class.` |
|       - | 6943 | ` *  According to the PHP language reference manual` |
|       - | 6944 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6945 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6946 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6947 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6948 | ` *   and functions (called "methods").` |
|       - | 6949 | ` */` |
|   37646 | 6950 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6951 |  |
|       - | 6952 | `	sxi32 rc;` |
|   37648 | 6953 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   37648 | 6954 | `	return rc;` |
|       2 | 6955 |  |
|       - | 6956 | `/*` |
|       - | 6957 | ` * Exception handling.` |
|       - | 6958 | ` *  According to the PHP language reference manual` |
|       - | 6959 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6960 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6961 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6962 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6963 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6964 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6965 | ` *    (or re-thrown) within a catch block.` |
|       - | 6966 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6967 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6968 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6969 | ` *    been defined with set_exception_handler().` |
|       - | 6970 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6971 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6972 | ` */` |
|       - | 6973 | `/*` |
|       - | 6974 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6975 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6976 | ` * indicates failure.` |
|       - | 6977 | ` */` |
|    8028 | 6978 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6979 |  |
|    8030 | 6980 | `	sxi32 rc = SXRET_OK;` |
|    8030 | 6981 | `	if( pRoot->pOp ){` |
|    8024 | 6982 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4014 | 6983 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6984 | `			/* Unexpected expression */` |
|     ! 0 | 6985 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6986 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6987 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6988 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6989 | `			}` |
|       2 | 6990 | `		}` |
|    4017 | 6991 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6992 | `		/* Unexpected expression */` |
|     ! 0 | 6993 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6994 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6995 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6996 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6997 | `		}` |
|     ! 0 | 6998 | `	}` |
|    8030 | 6999 | `	return rc;` |
|       2 | 7000 |  |
|       - | 7001 | `/*` |
|       - | 7002 | ` * Compile a 'throw' statement.` |
|       - | 7003 | ` * throw: This is how you trigger an exception.` |
|       - | 7004 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 7005 | ` */` |
|    8028 | 7006 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 7007 |  |
|    8030 | 7008 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7009 | `	GenBlock *pBlock;` |
|       - | 7010 | `	sxu32 nIdx;` |
|       - | 7011 | `	sxi32 rc;` |
|    8030 | 7012 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 7013 | `	/* Compile the expression */` |
|    8030 | 7014 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8030 | 7015 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 7016 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 7017 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7018 | `			return SXERR_ABORT;` |
|       - | 7019 | `		}` |
|     ! 0 | 7020 | `		return SXRET_OK;` |
|       - | 7021 | `	}` |
|    8030 | 7022 | `	pBlock = pGen->pCurrent;` |
|       - | 7023 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   37306 | 7024 | `	while(pBlock->pParent){` |
|   37302 | 7025 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8026 | 7026 | `			break;` |
|       - | 7027 | `		}` |
|       - | 7028 | `		/* Point to the parent block */` |
|   29278 | 7029 | `		pBlock = pBlock->pParent;` |
|       2 | 7030 | `	}` |
|       - | 7031 | `	/* Emit the throw instruction */` |
|    8030 | 7032 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 7033 | `	/* Emit the jump */` |
|    8030 | 7034 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8030 | 7035 | `	return SXRET_OK;` |
|    4016 | 7036 |  |
|       - | 7037 | `/*` |
|       - | 7038 | ` * Compile a 'catch' block.` |
|       - | 7039 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 7040 | ` * an object containing the exception information.` |
|       - | 7041 | ` */` |
|      98 | 7042 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 7043 |  |
|     100 | 7044 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7045 | `	ph7_exception_block sCatch;` |
|       - | 7046 | `	SySet *pInstrContainer;` |
|       - | 7047 | `	SyString sClassName;` |
|       - | 7048 | `	GenBlock *pCatch;` |
|       - | 7049 | `	SyToken *pToken;` |
|       - | 7050 | `	SyString *pName;` |
|       - | 7051 | `	char *zDup;` |
|       - | 7052 | `	sxi32 rc;` |
|     100 | 7053 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 7054 | `	/* Zero the structure */` |
|     100 | 7055 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 7056 | `	/* Initialize fields */` |
|     100 | 7057 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     100 | 7058 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     100 | 7059 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 7060 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7061 | `			pToken = pGen->pIn;` |
|     ! 0 | 7062 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7063 | `				pToken--;` |
|     ! 0 | 7064 | `			}` |
|     ! 0 | 7065 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7066 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7067 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7068 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7069 | `				return SXERR_ABORT;` |
|       - | 7070 | `			}` |
|     ! 0 | 7071 | `			return SXERR_INVALID;` |
|       - | 7072 | `	}` |
|       - | 7073 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     100 | 7074 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      61 | 7075 | `	for(;;){` |
|     124 | 7076 | `		int isAbsolute = 0;` |
|       - | 7077 | `		SyBlob sName;` |
|     124 | 7078 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 7079 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     124 | 7080 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 7081 | `			isAbsolute = 1;` |
|       7 | 7082 | `			pGen->pIn++;` |
|       3 | 7083 | `		}` |
|     124 | 7084 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     122 | 7085 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 7086 | `			SyBlobRelease(&sName);` |
|       5 | 7087 | `			pToken = pGen->pIn;` |
|       5 | 7088 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7089 | `				pToken--;` |
|     ! 0 | 7090 | `			}` |
|       7 | 7091 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7092 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 7093 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 7094 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7095 | `				return SXERR_ABORT;` |
|       - | 7096 | `			}` |
|       5 | 7097 | `			return SXERR_INVALID;` |
|       - | 7098 | `		}` |
|       - | 7099 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     120 | 7100 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     120 | 7101 | `		pGen->pIn++;` |
|     183 | 7102 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      67 | 7103 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 7104 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 7105 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 7106 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 7107 | `			pGen->pIn++;` |
|       1 | 7108 | `		}` |
|       - | 7109 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     120 | 7110 | `		if( !isAbsolute ){` |
|       - | 7111 | `			SyString sRaw;` |
|       - | 7112 | `			SyBlob sResolved;` |
|     114 | 7113 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     114 | 7114 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     114 | 7115 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     170 | 7116 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     112 | 7117 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     114 | 7118 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     114 | 7119 | `			SyBlobRelease(&sResolved);` |
|      58 | 7120 | `		}else{` |
|       - | 7121 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 7122 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 7123 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 7124 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 7125 | `		}` |
|     120 | 7126 | `		SyBlobRelease(&sName);` |
|     120 | 7127 | `		if( zDup == 0 ){` |
|     ! 0 | 7128 | `			goto Mem;` |
|       - | 7129 | `		}` |
|     120 | 7130 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     120 | 7131 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7132 | `			goto Mem;` |
|       - | 7133 | `		}` |
|       - | 7134 | `		/* Check for '\|' (multi-catch separator) */` |
|     130 | 7135 | `		if( pGen->pIn < pGen->pEnd &&` |
|     118 | 7136 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 7137 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 7138 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 7139 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 7140 | `			continue;` |
|       - | 7141 | `		}` |
|      96 | 7142 | `		break;` |
|     ! 0 | 7143 | `	}` |
|     141 | 7144 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      96 | 7145 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 7146 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7147 | `			pToken = pGen->pIn;` |
|     ! 0 | 7148 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7149 | `				pToken--;` |
|     ! 0 | 7150 | `			}` |
|     ! 0 | 7151 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7152 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7153 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7154 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7155 | `				return SXERR_ABORT;` |
|       - | 7156 | `			}` |
|     ! 0 | 7157 | `			return SXERR_INVALID;` |
|       - | 7158 | `	}` |
|      96 | 7159 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 7160 | `	/* Duplicate instance name */` |
|      96 | 7161 | `	pName = &pGen->pIn->sData;` |
|      96 | 7162 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      96 | 7163 | `	if( zDup == 0 ){` |
|     ! 0 | 7164 | `		goto Mem;` |
|       - | 7165 | `	}` |
|      96 | 7166 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      96 | 7167 | `	pGen->pIn++;` |
|      96 | 7168 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 7169 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 7170 | `		pToken = pGen->pIn;` |
|     ! 0 | 7171 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7172 | `			pToken--;` |
|     ! 0 | 7173 | `		}` |
|     ! 0 | 7174 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7175 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7176 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7177 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7178 | `			return SXERR_ABORT;` |
|       - | 7179 | `		}` |
|     ! 0 | 7180 | `		return SXERR_INVALID;` |
|       - | 7181 | `	}` |
|       - | 7182 | `	/* Compile the block */` |
|      96 | 7183 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 7184 | `	/* Create the catch block */` |
|      96 | 7185 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      96 | 7186 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7187 | `		return SXERR_ABORT;` |
|       - | 7188 | `	}` |
|       - | 7189 | `	/* Swap bytecode container */` |
|      96 | 7190 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      96 | 7191 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 7192 | `	/* Compile the block */` |
|      96 | 7193 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 7194 | `	/* Fix forward jumps now the destination is resolved  */` |
|      96 | 7195 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7196 | `	/* Emit the DONE instruction */` |
|      96 | 7197 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7198 | `	/* Leave the block */` |
|      96 | 7199 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7200 | `	/* Restore the default container */` |
|      96 | 7201 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7202 | `	/* Install the catch block */` |
|      96 | 7203 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      96 | 7204 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7205 | `		goto Mem;` |
|       - | 7206 | `	}` |
|      96 | 7207 | `	return SXRET_OK;` |
|     ! 0 | 7208 | `Mem:` |
|     ! 0 | 7209 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7210 | `	return SXERR_ABORT;` |
|      51 | 7211 |  |
|       - | 7212 | `/*` |
|       - | 7213 | ` * Compile a 'try' block.` |
|       - | 7214 | ` * A function using an exception should be in a "try" block.` |
|       - | 7215 | ` * If the exception does not trigger, the code will continue` |
|       - | 7216 | ` * as normal. However if the exception triggers, an exception` |
|       - | 7217 | ` * is "thrown".` |
|       - | 7218 | ` */` |
|     106 | 7219 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 7220 |  |
|       - | 7221 | `	ph7_exception *pException;` |
|     108 | 7222 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7223 | `	GenBlock *pTry;` |
|       - | 7224 | `	sxu32 nJmpIdx;` |
|       - | 7225 | `	sxi32 rc;` |
|       - | 7226 | `	/* Create the exception container */` |
|     108 | 7227 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     108 | 7228 | `	if( pException == 0 ){` |
|     ! 0 | 7229 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 7230 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7231 | `		return SXERR_ABORT;` |
|       - | 7232 | `	}` |
|       - | 7233 | `	/* Zero the structure */` |
|     108 | 7234 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 7235 | `	/* Initialize fields */` |
|     108 | 7236 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     108 | 7237 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     108 | 7238 | `	pException->iHasFinally = 0;` |
|     108 | 7239 | `	pException->iFinallyDone = 0;` |
|     108 | 7240 | `	pException->pVm = pGen->pVm;` |
|       - | 7241 | `	/* Create the try block */` |
|     108 | 7242 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     108 | 7243 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7244 | `		return SXERR_ABORT;` |
|       - | 7245 | `	}` |
|       - | 7246 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     108 | 7247 | `	pTry->pUserData = pException;` |
|       - | 7248 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     108 | 7249 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 7250 | `	/* Fix the jump later when the destination is resolved */` |
|     108 | 7251 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     108 | 7252 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 7253 | `	/* Compile the block */` |
|     108 | 7254 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     108 | 7255 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7256 | `		return SXERR_ABORT;` |
|       - | 7257 | `	}` |
|       - | 7258 | `	/* Fix forward jumps now the destination is resolved */` |
|     108 | 7259 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7260 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     108 | 7261 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 7262 | `	/* Leave the block */` |
|     108 | 7263 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7264 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     108 | 7265 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     104 | 7266 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 7267 | `		/* Compile one or more catch blocks */` |
|      96 | 7268 | `		for(;;){` |
|     192 | 7269 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     154 | 7270 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      49 | 7271 | `					break;` |
|       - | 7272 | `			}` |
|     100 | 7273 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     100 | 7274 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7275 | `				return SXERR_ABORT;` |
|       - | 7276 | `			}` |
|       2 | 7277 | `		}` |
|      47 | 7278 | `	}` |
|       - | 7279 | `	/* Compile optional finally block */` |
|     108 | 7280 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      56 | 7281 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 7282 | `		SySet *pInstrContainer;` |
|       - | 7283 | `		GenBlock *pFinBlock;` |
|      32 | 7284 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 7285 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 7286 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 7287 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7288 | `			return SXERR_ABORT;` |
|       - | 7289 | `		}` |
|       - | 7290 | `		/* Swap bytecode container */` |
|      32 | 7291 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 7292 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7293 | `		/* Compile the finally body */` |
|      32 | 7294 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 7295 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7296 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7297 | `			return SXERR_ABORT;` |
|       - | 7298 | `		}` |
|       - | 7299 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 7300 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7301 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 7302 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7303 | `		/* Leave the block */` |
|      32 | 7304 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7305 | `		/* Restore the default container */` |
|      32 | 7306 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 7307 | `		pException->iHasFinally = 1;` |
|      15 | 7308 | `	}` |
|       - | 7309 | `	/* Must have at least one catch or finally */` |
|     108 | 7310 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 7311 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7312 | `			"Cannot use try without catch or finally");` |
|       7 | 7313 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7314 | `			return SXERR_ABORT;` |
|       - | 7315 | `		}` |
|       3 | 7316 | `	}` |
|     108 | 7317 | `	return SXRET_OK;` |
|      55 | 7318 |  |
|       - | 7319 | `/*` |
|       - | 7320 | ` * Compile a switch block.` |
|       - | 7321 | ` *  (See block-comment below for more information)` |
|       - | 7322 | ` */` |
|     108 | 7323 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7324 |  |
|     110 | 7325 | `	sxi32 rc = SXRET_OK;` |
|     110 | 7326 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7327 | `		/* Unexpected token */` |
|     ! 0 | 7328 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7329 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7330 | `			return SXERR_ABORT;` |
|       - | 7331 | `		}` |
|     ! 0 | 7332 | `		pGen->pIn++;` |
|     ! 0 | 7333 | `	}` |
|     110 | 7334 | `	pGen->pIn++;` |
|       - | 7335 | `	/* First instruction to execute in this block. */` |
|     110 | 7336 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 7337 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 7338 | `	 * or the '}' token */` |
|     182 | 7339 | `	for(;;){` |
|     366 | 7340 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7341 | `			/* No more input to process */` |
|     ! 0 | 7342 | `			break;` |
|       - | 7343 | `		}` |
|     366 | 7344 | `		rc = SXRET_OK;` |
|     366 | 7345 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 | 7346 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 | 7347 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 7348 | `					/* Unexpected token */` |
|     ! 0 | 7349 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7350 | `						&pGen->pIn->sData);` |
|     ! 0 | 7351 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7352 | `						return SXERR_ABORT;` |
|       - | 7353 | `					}` |
|       - | 7354 | `					/* FALL THROUGH */` |
|     ! 0 | 7355 | `				}` |
|      28 | 7356 | `				rc = SXERR_EOF;` |
|      28 | 7357 | `				break;` |
|       - | 7358 | `			}` |
|      23 | 7359 | `		}else{` |
|       - | 7360 | `			sxi32 nKwrd;` |
|       - | 7361 | `			/* Extract the keyword */` |
|     298 | 7362 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 | 7363 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 | 7364 | `				break;` |
|       - | 7365 | `			}` |
|     218 | 7366 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7367 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 7368 | `					/* Unexpected token */` |
|     ! 0 | 7369 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7370 | `						&pGen->pIn->sData);` |
|     ! 0 | 7371 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7372 | `						return SXERR_ABORT;` |
|       - | 7373 | `					}` |
|       - | 7374 | `					/* FALL THROUGH */` |
|     ! 0 | 7375 | `				}` |
|       - | 7376 | `				/* Block compiled */` |
|       3 | 7377 | `				break;` |
|       - | 7378 | `			}` |
|       - | 7379 | `		}` |
|       - | 7380 | `		/* Compile block */` |
|     258 | 7381 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 | 7382 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7383 | `			return SXERR_ABORT;` |
|       - | 7384 | `		}` |
|       2 | 7385 | `	}` |
|     110 | 7386 | `	return rc;` |
|      56 | 7387 |  |
|       - | 7388 | `/*` |
|       - | 7389 | ` * Compile a case eXpression.` |
|       - | 7390 | ` *  (See block-comment below for more information)` |
|       - | 7391 | ` */` |
|      88 | 7392 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7393 |  |
|       - | 7394 | `	SySet *pInstrContainer;` |
|       - | 7395 | `	SyToken *pEnd,*pTmp;` |
|      90 | 7396 | `	sxi32 iNest = 0;` |
|       - | 7397 | `	sxi32 rc;` |
|       - | 7398 | `	/* Delimit the expression */` |
|      90 | 7399 | `	pEnd = pGen->pIn;` |
|     186 | 7400 | `	while( pEnd < pGen->pEnd ){` |
|     186 | 7401 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7402 | `			/* Increment nesting level */` |
|       3 | 7403 | `			iNest++;` |
|     185 | 7404 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7405 | `			/* Decrement nesting level */` |
|       3 | 7406 | `			iNest--;` |
|     183 | 7407 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 | 7408 | `			break;` |
|       - | 7409 | `		}` |
|      98 | 7410 | `		pEnd++;` |
|       2 | 7411 | `	}` |
|      90 | 7412 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7414 | `		if( rc == SXERR_ABORT ){` |
|       - | 7415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7416 | `			return SXERR_ABORT;` |
|       - | 7417 | `		}` |
|     ! 0 | 7418 | `	}` |
|       - | 7419 | `	/* Swap token stream */` |
|      90 | 7420 | `	pTmp = pGen->pEnd;` |
|      90 | 7421 | `	pGen->pEnd = pEnd;` |
|      90 | 7422 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 | 7423 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 | 7424 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7425 | `	/* Emit the done instruction */` |
|      90 | 7426 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 | 7427 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7428 | `	/* Update token stream */` |
|      90 | 7429 | `	pGen->pIn  = pEnd;` |
|      90 | 7430 | `	pGen->pEnd = pTmp;` |
|      90 | 7431 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7432 | `		return SXERR_ABORT;` |
|       - | 7433 | `	}` |
|      90 | 7434 | `	return SXRET_OK;` |
|      46 | 7435 |  |
|       - | 7436 | `/*` |
|       - | 7437 | ` * Compile the smart switch statement.` |
|       - | 7438 | ` * According to the PHP language reference manual` |
|       - | 7439 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7440 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7441 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7442 | ` *  This is exactly what the switch statement is for.` |
|       - | 7443 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7444 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7445 | ` *  of the outer loop, use continue 2.` |
|       - | 7446 | ` *  Note that switch/case does loose comparision.` |
|       - | 7447 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7448 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7449 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7450 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7451 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7452 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7453 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7454 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7455 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7456 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7457 | ` *  list for the next case.` |
|       - | 7458 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7459 | ` *  or floating-point numbers and strings.` |
|       - | 7460 | ` */` |
|      28 | 7461 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7462 |  |
|       - | 7463 | `	GenBlock *pSwitchBlock;` |
|       - | 7464 | `	SyToken *pTmp,*pEnd;` |
|       - | 7465 | `	ph7_switch *pSwitch;` |
|       - | 7466 | `	sxu32 nToken;` |
|       - | 7467 | `	sxu32 nLine;` |
|       - | 7468 | `	sxi32 rc;` |
|      30 | 7469 | `	nLine = pGen->pIn->nLine;` |
|       - | 7470 | `	/* Jump the 'switch' keyword */` |
|      30 | 7471 | `	pGen->pIn++;` |
|      30 | 7472 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7473 | `		/* Syntax error */` |
|     ! 0 | 7474 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7475 | `		if( rc == SXERR_ABORT ){` |
|       - | 7476 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7477 | `			return SXERR_ABORT;` |
|       - | 7478 | `		}` |
|     ! 0 | 7479 | `		goto Synchronize;` |
|       - | 7480 | `	}` |
|       - | 7481 | `	/* Jump the left parenthesis '(' */` |
|      30 | 7482 | `	pGen->pIn++;` |
|      30 | 7483 | `	pEnd = 0; /* cc warning */` |
|       - | 7484 | `	/* Create the loop block */` |
|      44 | 7485 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 7486 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 | 7487 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7488 | `		return SXERR_ABORT;` |
|       - | 7489 | `	}` |
|       - | 7490 | `	/* Delimit the condition */` |
|      30 | 7491 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 | 7492 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7493 | `		/* Empty expression */` |
|     ! 0 | 7494 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7495 | `		if( rc == SXERR_ABORT ){` |
|       - | 7496 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7497 | `			return SXERR_ABORT;` |
|       - | 7498 | `		}` |
|     ! 0 | 7499 | `	}` |
|       - | 7500 | `	/* Swap token streams */` |
|      30 | 7501 | `	pTmp = pGen->pEnd;` |
|      30 | 7502 | `	pGen->pEnd = pEnd;` |
|       - | 7503 | `	/* Compile the expression */` |
|      30 | 7504 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 7505 | `	if( rc == SXERR_ABORT ){` |
|       - | 7506 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7507 | `		return SXERR_ABORT;` |
|       - | 7508 | `	}` |
|       - | 7509 | `	/* Update token stream */` |
|      30 | 7510 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7511 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7512 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7513 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7514 | `			return SXERR_ABORT;` |
|       - | 7515 | `		}` |
|     ! 0 | 7516 | `		pGen->pIn++;` |
|     ! 0 | 7517 | `	}` |
|      30 | 7518 | `	pGen->pIn  = &pEnd[1];` |
|      30 | 7519 | `	pGen->pEnd = pTmp;` |
|      30 | 7520 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 7521 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7522 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7523 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7524 | `				pTmp--;` |
|     ! 0 | 7525 | `			}` |
|       - | 7526 | `			/* Unexpected token */` |
|     ! 0 | 7527 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7528 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7529 | `				return SXERR_ABORT;` |
|       - | 7530 | `			}` |
|     ! 0 | 7531 | `			goto Synchronize;` |
|       - | 7532 | `	}` |
|       - | 7533 | `	/* Set the delimiter token */` |
|      30 | 7534 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7535 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7536 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7537 | `	}else{` |
|      28 | 7538 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7539 | `	}` |
|      30 | 7540 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7541 | `	/* Create the switch blocks container */` |
|      30 | 7542 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 | 7543 | `	if( pSwitch == 0 ){` |
|       - | 7544 | `		/* Abort compilation */` |
|     ! 0 | 7545 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7546 | `		return SXERR_ABORT;` |
|       - | 7547 | `	}` |
|       - | 7548 | `	/* Zero the structure */` |
|      30 | 7549 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7550 | `	/* Initialize fields */` |
|      30 | 7551 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7552 | `	/* Emit the switch instruction */` |
|      30 | 7553 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7554 | `	/* Compile case blocks */` |
|      96 | 7555 | `	for(;;){` |
|       - | 7556 | `		sxu32 nKwrd;` |
|     112 | 7557 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7558 | `			/* No more input to process */` |
|     ! 0 | 7559 | `			break;` |
|       - | 7560 | `		}` |
|     112 | 7561 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7562 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7563 | `				/* Unexpected token */` |
|     ! 0 | 7564 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7565 | `					&pGen->pIn->sData);` |
|     ! 0 | 7566 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7567 | `					return SXERR_ABORT;` |
|       - | 7568 | `				}` |
|       - | 7569 | `				/* FALL THROUGH */` |
|     ! 0 | 7570 | `			}` |
|       - | 7571 | `			/* Block compiled */` |
|     ! 0 | 7572 | `			break;` |
|       - | 7573 | `		}` |
|       - | 7574 | `		/* Extract the keyword */` |
|     112 | 7575 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 | 7576 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7577 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7578 | `				/* Unexpected token */` |
|     ! 0 | 7579 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7580 | `					&pGen->pIn->sData);` |
|     ! 0 | 7581 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7582 | `					return SXERR_ABORT;` |
|       - | 7583 | `				}` |
|       - | 7584 | `				/* FALL THROUGH */` |
|     ! 0 | 7585 | `			}` |
|       - | 7586 | `			/* Block compiled */` |
|       3 | 7587 | `			break;` |
|       - | 7588 | `		}` |
|     110 | 7589 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7590 | `			/*` |
|       - | 7591 | `			 * Accroding to the PHP language reference manual` |
|       - | 7592 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7593 | `			 *  that wasn't matched by the other cases.` |
|       - | 7594 | `			 */` |
|      22 | 7595 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7596 | `				/* Default case already compiled */` |
|     ! 0 | 7597 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7598 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7599 | `					return SXERR_ABORT;` |
|       - | 7600 | `				}` |
|     ! 0 | 7601 | `			}` |
|      22 | 7602 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7603 | `			/* Compile the default block */` |
|      22 | 7604 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 | 7605 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7606 | `				return SXERR_ABORT;` |
|      22 | 7607 | `			}else if( rc == SXERR_EOF ){` |
|      20 | 7608 | `				break;` |
|       1 | 7609 | `			}` |
|      91 | 7610 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7611 | `			ph7_case_expr sCase;` |
|       - | 7612 | `			/* Standard case block */` |
|      90 | 7613 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7614 | `			/* initialize the structure */` |
|      90 | 7615 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7616 | `			/* Compile the case expression */` |
|      90 | 7617 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 | 7618 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7619 | `				return SXERR_ABORT;` |
|       - | 7620 | `			}` |
|       - | 7621 | `			/* Compile the case block */` |
|      90 | 7622 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7623 | `			/* Insert in the switch container */` |
|      90 | 7624 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 | 7625 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7626 | `				return SXERR_ABORT;` |
|      90 | 7627 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7628 | `				break;` |
|       - | 7629 | `			}` |
|      42 | 7630 | `		}else{` |
|       - | 7631 | `			/* Unexpected token */` |
|     ! 0 | 7632 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7633 | `				&pGen->pIn->sData);` |
|     ! 0 | 7634 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7635 | `				return SXERR_ABORT;` |
|       - | 7636 | `			}` |
|     ! 0 | 7637 | `			break;` |
|       - | 7638 | `		}` |
|       2 | 7639 | `	}` |
|       - | 7640 | `	/* Fix all jumps now the destination is resolved */` |
|      30 | 7641 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 | 7642 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7643 | `	/* Release the loop block */` |
|      30 | 7644 | `	GenStateLeaveBlock(pGen,0);` |
|      30 | 7645 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7646 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 | 7647 | `		pGen->pIn++;` |
|      14 | 7648 | `	}` |
|       - | 7649 | `	/* Statement successfully compiled */` |
|      30 | 7650 | `	return SXRET_OK;` |
|     ! 0 | 7651 | `Synchronize:` |
|       - | 7652 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7653 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7654 | `		pGen->pIn++;` |
|     ! 0 | 7655 | `	}` |
|     ! 0 | 7656 | `	return SXRET_OK;` |
|      16 | 7657 |  |
|       - | 7658 | `/*` |
|       - | 7659 | ` * Generate bytecode for a given expression tree.` |
|       - | 7660 | ` * If something goes wrong while generating bytecode` |
|       - | 7661 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7662 | ` * this function takes care of generating the appropriate` |
|       - | 7663 | ` * error message.` |
|       - | 7664 | ` */` |
| 2390104 | 7665 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7666 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7667 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7668 | `	sxi32 iFlags /* Control flags */` |
|       - | 7669 | `	)` |
|       2 | 7670 |  |
|       - | 7671 | `	VmInstr *pInstr;` |
|       - | 7672 | `	sxu32 nJmpIdx;` |
| 2390106 | 7673 | `	sxi32 iP1 = 0;` |
| 2390106 | 7674 | `	sxu32 iP2 = 0;` |
| 2390106 | 7675 | `	void *p3  = 0;` |
|       - | 7676 | `	sxi32 iVmOp;` |
|       - | 7677 | `	sxi32 rc;` |
| 2390106 | 7678 | `	if( pNode->xCode ){` |
|       - | 7679 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7680 | `		/* Compile node */` |
| 1481432 | 7681 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1481432 | 7682 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1481432 | 7683 | `		RE_SWAP_DELIMITER(pGen);` |
| 1481432 | 7684 | `		return rc;` |
|       - | 7685 | `	}` |
|  908676 | 7686 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7687 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7688 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7689 | `		return SXERR_ABORT;` |
|       - | 7690 | `	}` |
|  908676 | 7691 | `	iVmOp = pNode->pOp->iVmOp;` |
|  908676 | 7692 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7693 | `		sxu32 nJz,nJmp;` |
|       - | 7694 | `		/* Ternary operator require special handling */` |
|       - | 7695 | `		/* Phase#1: Compile the condition */` |
|    1888 | 7696 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1888 | 7697 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7698 | `			return rc;` |
|       - | 7699 | `		}` |
|    1888 | 7700 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1888 | 7701 | `		if( pNode->pLeft ){` |
|       - | 7702 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7703 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1820 | 7704 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7705 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1820 | 7706 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1820 | 7707 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7708 | `				return rc;` |
|       - | 7709 | `			}` |
|     911 | 7710 | `		}else{` |
|       - | 7711 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7712 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7713 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7714 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7715 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7716 | `		}` |
|       - | 7717 | `		/* Phase#4: Emit the unconditional jump */` |
|    1888 | 7718 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7719 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1888 | 7720 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1888 | 7721 | `		if( pInstr ){` |
|    1888 | 7722 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     943 | 7723 | `		}` |
|    1888 | 7724 | `		if( !pNode->pLeft ){` |
|       - | 7725 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7726 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7727 | `		}` |
|       - | 7728 | `		/* Phase#6: Compile the 'else' expression */` |
|    1888 | 7729 | `		if( pNode->pRight ){` |
|    1888 | 7730 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1888 | 7731 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7732 | `				return rc;` |
|       - | 7733 | `			}` |
|     943 | 7734 | `		}` |
|    1888 | 7735 | `		if( nJmp > 0 ){` |
|       - | 7736 | `			/* Phase#7: Fix the unconditional jump */` |
|    1888 | 7737 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1888 | 7738 | `			if( pInstr ){` |
|    1888 | 7739 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     943 | 7740 | `			}` |
|     943 | 7741 | `		}` |
|       - | 7742 | `		/* All done */` |
|    1888 | 7743 | `		return SXRET_OK;` |
|       - | 7744 | `	}` |
|       - | 7745 | `	/* Generate code for the left tree */` |
|  906790 | 7746 | `	if( pNode->pLeft ){` |
|  906754 | 7747 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7748 | `			ph7_expr_node **apNode;` |
|  304408 | 7749 | `			int hasSpread = 0;` |
|       - | 7750 | `			sxi32 n;` |
|       - | 7751 | `			/* Recurse and generate bytecodes for function arguments */` |
|  304408 | 7752 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7753 | `			/* Read-only load */` |
|  304408 | 7754 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  608248 | 7755 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  303842 | 7756 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  303842 | 7757 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7758 | `					return rc;` |
|       - | 7759 | `				}` |
|  303842 | 7760 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7761 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7762 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7763 | `					hasSpread = 1;` |
|       7 | 7764 | `				}` |
|  151922 | 7765 | `			}` |
|       - | 7766 | `			/* Total number of given arguments */` |
|  304408 | 7767 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  304408 | 7768 | `			iP2 = hasSpread;` |
|       - | 7769 | `			/* Remove stale flags now */` |
|  304408 | 7770 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  152203 | 7771 | `		}` |
|  906754 | 7772 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  906754 | 7773 | `		if( rc != SXRET_OK ){` |
|      13 | 7774 | `			return rc;` |
|       - | 7775 | `		}` |
|  906742 | 7776 | `		if( iVmOp == PH7_OP_CALL ){` |
|  304408 | 7777 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  304408 | 7778 | `			if( pInstr ){` |
|  304408 | 7779 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  303834 | 7780 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7781 | `					sxu32 nQual;` |
|       - | 7782 | `					/* Prevent constant expansion */` |
|  303834 | 7783 | `					pInstr->iP1 = 0;` |
|       - | 7784 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7785 | `					 * Only check function imports — class imports must NOT` |
|       - | 7786 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7787 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7788 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7789 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7790 | `					 * class imports. */ {` |
|  303834 | 7791 | `						int fromImport = 0;` |
|  303834 | 7792 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  303834 | 7793 | `						pInstr->iP2 = (sxi32)nQual;` |
|  303834 | 7794 | `						if( nQual != nOrig ){` |
|       - | 7795 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7796 | `							 * NEW handler can recover the unqualified name. */` |
|      68 | 7797 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      68 | 7798 | `							if( !fromImport ){` |
|      58 | 7799 | `								p3 = (void *)1;` |
|      28 | 7800 | `							}` |
|      35 | 7801 | `						}` |
|       - | 7802 | `					}` |
|  152492 | 7803 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7804 | `					/* Method call,flag that */` |
|     552 | 7805 | `					pInstr->iP2 = 1;` |
|     275 | 7806 | `				}` |
|  152205 | 7807 | `			}` |
|  754539 | 7808 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7809 | `			ph7_expr_node **apNode;` |
|       - | 7810 | `			sxi32 n;` |
|       - | 7811 | `			/* Recurse and generate bytecodes for array index */` |
|   68180 | 7812 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  123018 | 7813 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   54840 | 7814 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   54840 | 7815 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7816 | `					return rc;` |
|       - | 7817 | `				}` |
|   27421 | 7818 | `			}` |
|   68180 | 7819 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   54840 | 7820 | `				iP1 = 1; /* Node have an index associated with it */` |
|   27419 | 7821 | `			}` |
|   68180 | 7822 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7823 | `				/* Create an empty entry when the desired index is not found */` |
|   26922 | 7824 | `				iP2 = 1;` |
|   13462 | 7825 | `			}` |
|  568247 | 7826 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7827 | `			/* POP the left node */` |
|      32 | 7828 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7829 | `		}` |
|  453370 | 7830 | `	}` |
|  906778 | 7831 | `	rc = SXRET_OK;` |
|  906778 | 7832 | `	nJmpIdx = 0;` |
|       - | 7833 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7834 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7835 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  906778 | 7836 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     236 | 7837 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     236 | 7838 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     236 | 7839 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     236 | 7840 | `			int isSpecial = 0;` |
|     236 | 7841 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     152 | 7842 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     152 | 7843 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     163 | 7844 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     131 | 7845 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      67 | 7846 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      86 | 7847 | `					isSpecial = 1;` |
|      42 | 7848 | `				}` |
|      96 | 7849 | `			}` |
|     278 | 7850 | `			pInstr->iP1 = 0;` |
|     278 | 7851 | `			if( !isSpecial ){` |
|     110 | 7852 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      54 | 7853 | `			}` |
|       - | 7854 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7855 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     194 | 7856 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     110 | 7857 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     110 | 7858 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7859 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7860 | `					return SXRET_OK;` |
|       - | 7861 | `				}` |
|      33 | 7862 | `			}` |
|      75 | 7863 | `		}` |
|     147 | 7864 | `	}` |
|       - | 7865 | `	/* Generate code for the right tree */` |
|  906702 | 7866 | `	if( pNode->pRight ){` |
|  473704 | 7867 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7868 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8390 | 7869 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  469510 | 7870 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7871 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2802 | 7872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  463916 | 7873 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7874 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7875 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7876 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  462501 | 7877 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  206620 | 7878 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  103309 | 7879 | `		}` |
|  473704 | 7880 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  473704 | 7881 | `		if( iVmOp == PH7_OP_STORE ){` |
|  203790 | 7882 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  203764 | 7883 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 7884 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 7885 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 7886 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 7887 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 7888 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 7889 | `				 */` |
|      54 | 7890 | `				iVmOp = 0;` |
|  203764 | 7891 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  203738 | 7892 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7893 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   45244 | 7894 | `					iP2 = 1;` |
|   22623 | 7895 | `				}else{` |
|  158496 | 7896 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7897 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26884 | 7898 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26884 | 7899 | `						iP1 = pInstr->iP1;` |
|   13443 | 7900 | `					}else{` |
|  131614 | 7901 | `						p3 = pInstr->p3;` |
|       - | 7902 | `					}` |
|       - | 7903 | `					/* POP the last dynamic load instruction */` |
|  158496 | 7904 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7905 | `				}` |
|  101870 | 7906 | `			}` |
|  371810 | 7907 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7908 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7909 | `			if( pInstr ){` |
|      46 | 7910 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7911 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7912 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7913 | `					 */` |
|      15 | 7914 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7915 | `					iP1 = pInstr->iP1;` |
|      15 | 7916 | `					iP2 = pInstr->iP2;` |
|      15 | 7917 | `					p3  = pInstr->p3;` |
|       8 | 7918 | `				}else{` |
|      32 | 7919 | `					p3 = pInstr->p3;` |
|       - | 7920 | `				}` |
|      22 | 7921 | `			}` |
|      22 | 7922 | `		}` |
|  236851 | 7923 | `	}` |
|  906702 | 7924 | `	if( iVmOp > 0 ){` |
|  906590 | 7925 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10870 | 7926 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7927 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7976 | 7928 | `				iP1 = 1;` |
|    3989 | 7929 | `			}` |
|  901156 | 7930 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7931 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13728 | 7932 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13728 | 7933 | `				VmInstr *pCallInstr = 0;` |
|   13728 | 7934 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13712 | 7935 | `					pCallInstr = pPeek;` |
|   13712 | 7936 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6855 | 7937 | `				}` |
|   13728 | 7938 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7939 | `					sxu32 nLitForClass;` |
|       - | 7940 | `					/* If the CALL handler already qualified the name using` |
|       - | 7941 | `					 * function imports, recover the original unqualified` |
|       - | 7942 | `					 * literal so we can re-qualify with class imports. */` |
|   13726 | 7943 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      32 | 7944 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      17 | 7945 | `					}else{` |
|   13696 | 7946 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 7947 | `					}` |
|   13726 | 7948 | `					pPeek->iP1 = 0;` |
|   13726 | 7949 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6862 | 7950 | `				}` |
|       - | 7951 | `			}` |
|   13728 | 7952 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13728 | 7953 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7954 | `				VmInstr *pPrev;` |
|   13712 | 7955 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13712 | 7956 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7957 | `					/* Pop the call instruction */` |
|   13712 | 7958 | `					iP1 = pInstr->iP1;` |
|   13712 | 7959 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6855 | 7960 | `				}` |
|    6857 | 7961 | `			}` |
|  888859 | 7962 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7963 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7964 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7965 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7966 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7967 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7968 | `				int isSpecialIs = 0;` |
|      50 | 7969 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7970 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7971 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7972 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7973 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7974 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7975 | `						isSpecialIs = 1;` |
|       5 | 7976 | `					}` |
|      23 | 7977 | `				}` |
|      52 | 7978 | `				pInstr->iP1 = 0;` |
|      52 | 7979 | `				if( !isSpecialIs ){` |
|      38 | 7980 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 7981 | `				}` |
|      25 | 7982 | `			}` |
|  881975 | 7983 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7984 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7985 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7986 | `			 * should not trigger constant lookup. */` |
|  101904 | 7987 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  101904 | 7988 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  101888 | 7989 | `				pInstr->iP1 = 0;` |
|   50943 | 7990 | `			}` |
|  101904 | 7991 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7992 | `				/* Static member access,remember that */` |
|     160 | 7993 | `				iP1 = 1;` |
|     160 | 7994 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     160 | 7995 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7996 | `					p3 = pInstr->p3;` |
|      10 | 7997 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7998 | `				}` |
|      79 | 7999 | `			}` |
|   50951 | 8000 | `		}` |
|       - | 8001 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  906588 | 8002 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  453293 | 8003 | `	}` |
|  906700 | 8004 | `	if( nJmpIdx > 0 ){` |
|       - | 8005 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11220 | 8006 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11220 | 8007 | `		if( pInstr ){` |
|   11220 | 8008 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5609 | 8009 | `		}` |
|    5609 | 8010 | `	}` |
|  906700 | 8011 | `	return rc;` |
| 1195036 | 8012 |  |
|       - | 8013 | `/*` |
|       - | 8014 | ` * Compile a PHP expression.` |
|       - | 8015 | ` * According to the PHP language reference manual:` |
|       - | 8016 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 8017 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 8018 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 8019 | ` *  is "anything that has a value".` |
|       - | 8020 | ` * If something goes wrong while compiling the expression,this` |
|       - | 8021 | ` * function takes care of generating the appropriate error` |
|       - | 8022 | ` * message.` |
|       - | 8023 | ` */` |
|  645520 | 8024 | `static sxi32 PH7_CompileExpr(` |
|       - | 8025 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8026 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 8027 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 8028 | `	)` |
|       2 | 8029 |  |
|       - | 8030 | `	ph7_expr_node *pRoot;` |
|       - | 8031 | `	SySet sExprNode;` |
|       - | 8032 | `	SyToken *pEnd;` |
|       - | 8033 | `	sxi32 nExpr;` |
|       - | 8034 | `	sxi32 iNest;` |
|       - | 8035 | `	sxi32 rc;` |
|       - | 8036 | `	/* Initialize worker variables */` |
|  645522 | 8037 | `	nExpr = 0;` |
|  645522 | 8038 | `	pRoot = 0;` |
|  645522 | 8039 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  645522 | 8040 | `	SySetAlloc(&sExprNode,0x10);` |
|  645522 | 8041 | `	rc = SXRET_OK;` |
|       - | 8042 | `	/* Delimit the expression */` |
|  645522 | 8043 | `	pEnd = pGen->pIn;` |
|  645522 | 8044 | `	iNest = 0;` |
| 4352004 | 8045 | `	while( pEnd < pGen->pEnd ){` |
| 4126804 | 8046 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8047 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     236 | 8048 | `			iNest++;` |
| 4126687 | 8049 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     244 | 8050 | `			iNest--;` |
| 4126449 | 8051 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  420528 | 8052 | `			if( iNest <= 0 ){` |
|  420322 | 8053 | `				break;` |
|       - | 8054 | `			}` |
|     103 | 8055 | `		}` |
| 3706484 | 8056 | `		pEnd++;` |
|       2 | 8057 | `	}` |
|  645522 | 8058 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10822 | 8059 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10822 | 8060 | `		iNest = 0;` |
|       - | 8061 | `		/* Stop at the first comma */` |
|   21666 | 8062 | `		while( pEnd2 < pEnd ){` |
|   10846 | 8063 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 8064 | `				iNest++;` |
|   10844 | 8065 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 8066 | `				iNest--;` |
|   10840 | 8067 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 8068 | `				if( iNest <= 0 ){` |
|     ! 0 | 8069 | `					break;` |
|       - | 8070 | `				}` |
|       2 | 8071 | `			}` |
|   10846 | 8072 | `			pEnd2++;` |
|       2 | 8073 | `		}` |
|   10822 | 8074 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 8075 | `			pEnd = pEnd2;` |
|     ! 0 | 8076 | `		}` |
|    5410 | 8077 | `	}` |
|  645522 | 8078 | `	if( pEnd > pGen->pIn ){` |
|  645512 | 8079 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 8080 | `		/* Swap delimiter */` |
|  645512 | 8081 | `		pGen->pEnd = pEnd;` |
|       - | 8082 | `		/* Try to get an expression tree */` |
|  645512 | 8083 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  645512 | 8084 | `		if( rc == SXRET_OK && pRoot ){` |
|  645348 | 8085 | `			rc = SXRET_OK;` |
|  645348 | 8086 | `			if( xTreeValidator ){` |
|       - | 8087 | `				/* Call the upper layer validator callback */` |
|   13898 | 8088 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6948 | 8089 | `			}` |
|  645348 | 8090 | `			if( rc != SXERR_ABORT ){` |
|       - | 8091 | `				/* Generate code for the given tree */` |
|  645348 | 8092 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  322673 | 8093 | `			}` |
|  645348 | 8094 | `			nExpr = 1;` |
|  322673 | 8095 | `		}` |
|       - | 8096 | `		/* Release the whole tree */` |
|  645512 | 8097 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 8098 | `		/* Synchronize token stream */` |
|  645512 | 8099 | `		pGen->pEnd = pTmp;` |
|  645512 | 8100 | `		pGen->pIn  = pEnd;` |
|  645512 | 8101 | `		if( rc == SXERR_ABORT ){` |
|       3 | 8102 | `			SySetRelease(&sExprNode);` |
|       3 | 8103 | `			return SXERR_ABORT;` |
|       - | 8104 | `		}` |
|  322754 | 8105 | `	}` |
|  645520 | 8106 | `	SySetRelease(&sExprNode);` |
|  645520 | 8107 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  322762 | 8108 |  |
|       - | 8109 | `/*` |
|       - | 8110 | ` * Return a pointer to the node construct handler associated` |
|       - | 8111 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 8112 | ` */` |
|  161110 | 8113 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 8114 |  |
|  161112 | 8115 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 8116 | `		/* Numeric literal: Either real or integer */` |
|   88174 | 8117 | `		return PH7_CompileNumLiteral;` |
|   72940 | 8118 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 8119 | `		/* Double quoted string */` |
|   15616 | 8120 | `		return PH7_CompileString;` |
|   57326 | 8121 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 8122 | `		/* Single quoted string */` |
|   57266 | 8123 | `		return PH7_CompileSimpleString;` |
|      62 | 8124 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 8125 | `		/* Heredoc */` |
|      28 | 8126 | `		return PH7_CompileHereDoc;` |
|      36 | 8127 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 8128 | `		/* Nowdoc */` |
|      29 | 8129 | `		return PH7_CompileNowDoc;` |
|       7 | 8130 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 8131 | `		/* Backtick quoted string */` |
|       5 | 8132 | `		return PH7_CompileBacktic;` |
|       - | 8133 | `	}` |
|       3 | 8134 | `	return 0;` |
|   80557 | 8135 |  |
|       - | 8136 | `/*` |
|       - | 8137 | ` * Compile an unset() statement.` |
|       - | 8138 | ` * unset($var, $arr[$key], ...);` |
|       - | 8139 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 8140 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 8141 | ` * parent array before extracting the element to unset.` |
|       - | 8142 | ` */` |
|    2576 | 8143 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 8144 |  |
|    2578 | 8145 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2578 | 8146 | `	sxu32 nIdx = 0;` |
|       - | 8147 | `	SyString sName;` |
|       - | 8148 | `	sxi32 rc;` |
|       - | 8149 | `	/* Jump the 'unset' keyword */` |
|    2578 | 8150 | `	pGen->pIn++;` |
|       - | 8151 | `	/* Save delimiter */` |
|    2578 | 8152 | `	pTmp = pGen->pEnd;` |
|       - | 8153 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2578 | 8154 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2578 | 8155 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 8156 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 8157 | `		SyToken *pClose;` |
|    2578 | 8158 | `		pGen->pIn++;   /* Skip '(' */` |
|    2578 | 8159 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2578 | 8160 | `		pEnd = pClose; /* Stop at ')' */` |
|    1288 | 8161 | `	}` |
|    2578 | 8162 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 8163 | `	/* Resolve the 'unset' builtin name once */` |
|    2578 | 8164 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     304 | 8165 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     304 | 8166 | `		if( pObj == 0 ){` |
|     ! 0 | 8167 | `			return SXERR_ABORT;` |
|       - | 8168 | `		}` |
|     304 | 8169 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     304 | 8170 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     151 | 8171 | `	}` |
|       - | 8172 | `	/* Compile each comma-separated argument */` |
|    8592 | 8173 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6016 | 8174 | `		if( pGen->pIn < pNext ){` |
|    6016 | 8175 | `			pGen->pEnd = pNext;` |
|    6016 | 8176 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 8177 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6016 | 8178 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8179 | `				return SXERR_ABORT;` |
|       - | 8180 | `			}` |
|    6016 | 8181 | `			if( rc != SXERR_EMPTY ){` |
|       - | 8182 | `				/* Emit call for this single argument */` |
|    6014 | 8183 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6014 | 8184 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6014 | 8185 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3006 | 8186 | `			}` |
|    3007 | 8187 | `		}` |
|       - | 8188 | `		/* Jump trailing commas */` |
|    9456 | 8189 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3442 | 8190 | `			pNext++;` |
|       2 | 8191 | `		}` |
|    6016 | 8192 | `		pGen->pIn = pNext;` |
|       2 | 8193 | `	}` |
|       - | 8194 | `	/* Skip past the closing ')' if present */` |
|    2578 | 8195 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2578 | 8196 | `		pGen->pIn++;` |
|    1288 | 8197 | `	}` |
|       - | 8198 | `	/* Restore token stream */` |
|    2578 | 8199 | `	pGen->pEnd = pTmp;` |
|    2578 | 8200 | `	return SXRET_OK;` |
|    1290 | 8201 |  |
|       - | 8202 | `/*` |
|       - | 8203 | ` * PHP Language construct table.` |
|       - | 8204 | ` */` |
|       - | 8205 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 8206 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 8207 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 8208 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 8209 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 8210 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 8211 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 8212 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 8213 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 8214 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 8215 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 8216 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 8217 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 8218 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 8219 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 8220 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 8221 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 8222 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 8223 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 8224 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 8225 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 8226 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 8227 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 8228 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 8229 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 8230 | `};` |
|       - | 8231 | `/*` |
|       - | 8232 | ` * Return a pointer to the statement handler routine associated` |
|       - | 8233 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 8234 | ` */` |
|  391700 | 8235 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 8236 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 8237 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 8238 | `	)` |
|       2 | 8239 |  |
|  391702 | 8240 | `	sxu32 n = 0;` |
| 1646048 | 8241 | `	for(;;){` |
| 3292098 | 8242 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   45858 | 8243 | `			break;` |
|       - | 8244 | `		}` |
| 3246242 | 8245 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  345846 | 8246 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 8247 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 8248 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 8249 | `					/* 'static' (class context),return null */` |
|     ! 0 | 8250 | `					return 0;` |
|       - | 8251 | `				}` |
|     ! 0 | 8252 | `			}` |
|       - | 8253 | `			/* Return a pointer to the handler.` |
|       - | 8254 | `			*/` |
|  345846 | 8255 | `			return aLangConstruct[n].xConstruct;` |
|       - | 8256 | `		}` |
| 2900398 | 8257 | `		n++;` |
|       2 | 8258 | `	}` |
|   45858 | 8259 | `	if( pLookahed ){` |
|   45858 | 8260 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8008 | 8261 | `			return PH7_CompileClassInterface;` |
|   37852 | 8262 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   37648 | 8263 | `			return PH7_CompileClass;` |
|     206 | 8264 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 8265 | `			return PH7_CompileTrait;` |
|     152 | 8266 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 8267 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 8268 | `				return PH7_CompileAbstractClass;` |
|     136 | 8269 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 8270 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 8271 | `				return PH7_CompileFinalClass;` |
|       - | 8272 | `		}` |
|      67 | 8273 | `	}` |
|       - | 8274 | `	/* Not a language construct */` |
|     136 | 8275 | `	return 0;` |
|  195852 | 8276 |  |
|       - | 8277 | `/*` |
|       - | 8278 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 8279 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 8280 | ` */` |
|     134 | 8281 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 8282 |  |
|       - | 8283 | `	int rc;` |
|     136 | 8284 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 8285 | `	if( rc == FALSE ){` |
|      40 | 8286 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 8287 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 8288 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 8289 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 8290 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 8291 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 8292 | `			*/` |
|       - | 8293 | `			){` |
|      34 | 8294 | `				rc = TRUE;` |
|      16 | 8295 | `		}` |
|      20 | 8296 | `	}` |
|     136 | 8297 | `	return rc;` |
|       2 | 8298 |  |
|       - | 8299 | `/*` |
|       - | 8300 | ` * Compile a PHP chunk.` |
|       - | 8301 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8302 | ` * takes care of generating the appropriate error message.` |
|       - | 8303 | ` */` |
|  525870 | 8304 | `static sxi32 GenStateCompileChunk(` |
|       - | 8305 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8306 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 8307 | `	)` |
|       2 | 8308 |  |
|       - | 8309 | `	ProcLangConstruct xCons;` |
|       - | 8310 | `	sxi32 rc;` |
|  525872 | 8311 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  314211 | 8312 | `	for(;;){` |
|  628424 | 8313 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8314 | `			/* No more input to process */` |
|   11496 | 8315 | `			break;` |
|       - | 8316 | `		}` |
|  616930 | 8317 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8318 | `			/* Compile block */` |
|      12 | 8319 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 8320 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8321 | `				break;` |
|       - | 8322 | `			}` |
|       7 | 8323 | `		}else{` |
|  616920 | 8324 | `			xCons = 0;` |
|  616920 | 8325 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  391702 | 8326 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 8327 | `				/* Try to extract a language construct handler */` |
|  391702 | 8328 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  391702 | 8329 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 8330 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 8331 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 8332 | `						&pGen->pIn->sData);` |
|       9 | 8333 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8334 | `						break;` |
|       - | 8335 | `					}` |
|       - | 8336 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 8337 | `					 * this erroneous statement.` |
|       - | 8338 | `					 */` |
|       9 | 8339 | `					xCons = PH7_ErrorRecover;` |
|       4 | 8340 | `				}` |
|  421070 | 8341 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   39450 | 8342 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 8343 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 8344 | `				xCons = PH7_CompileLabel;` |
|      56 | 8345 | `			}` |
|  616920 | 8346 | `			if( xCons == 0 ){` |
|       - | 8347 | `				/* Assume an expression an try to compile it */` |
|  225234 | 8348 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  225234 | 8349 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 8350 | `					/* Pop l-value */` |
|  225102 | 8351 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  112550 | 8352 | `				}` |
|  112618 | 8353 | `			}else{` |
|       - | 8354 | `				/* Go compile the sucker */` |
|  391688 | 8355 | `				rc = xCons(&(*pGen));` |
|       - | 8356 | `			}` |
|  616920 | 8357 | `			if( rc == SXERR_ABORT ){` |
|       - | 8358 | `				/* Request to abort compilation */` |
|       3 | 8359 | `				break;` |
|       - | 8360 | `			}` |
|       - | 8361 | `		}` |
|       - | 8362 | `		/* Ignore trailing semi-colons ';' */` |
| 1021782 | 8363 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  404856 | 8364 | `			pGen->pIn++;` |
|       2 | 8365 | `		}` |
|  616928 | 8366 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 8367 | `			/* Compile a single statement and return */` |
|  514376 | 8368 | `			break;` |
|       - | 8369 | `		}` |
|       - | 8370 | `		/* LOOP ONE */` |
|       - | 8371 | `		/* LOOP TWO */` |
|       - | 8372 | `		/* LOOP THREE */` |
|       - | 8373 | `		/* LOOP FOUR */` |
|       2 | 8374 | `	}` |
|       - | 8375 | `	/* Return compilation status */` |
|  525872 | 8376 | `	return rc;` |
|       2 | 8377 |  |
|       - | 8378 | `/*` |
|       - | 8379 | ` * Compile a Raw PHP chunk.` |
|       - | 8380 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8381 | ` * takes care of generating the appropriate error message.` |
|       - | 8382 | ` */` |
|   11498 | 8383 | `static sxi32 PH7_CompilePHP(` |
|       - | 8384 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8385 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8386 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8387 | `	)` |
|       2 | 8388 |  |
|   11500 | 8389 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8390 | `	sxi32 rc;` |
|       - | 8391 | `	/* Reset the token set */` |
|   11500 | 8392 | `	SySetReset(&(*pTokenSet));` |
|       - | 8393 | `	/* Mark as the default token set */` |
|   11500 | 8394 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8395 | `	/* Advance the stream cursor */` |
|   11500 | 8396 | `	pGen->pRawIn++;` |
|       - | 8397 | `	/* Tokenize the PHP chunk first */` |
|   11500 | 8398 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8399 | `	/* Point to the head and tail of the token stream. */` |
|   11500 | 8400 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11500 | 8401 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11500 | 8402 | `	if( is_expr ){` |
|     ! 0 | 8403 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8404 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8405 | `			/* A simple expression,compile it */` |
|     ! 0 | 8406 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8407 | `		}` |
|       - | 8408 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8409 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8410 | `		return SXRET_OK;` |
|       - | 8411 | `	}` |
|   11500 | 8412 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8413 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8414 | `		/*` |
|       - | 8415 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8416 | `		 * According to the PHP reference manual:` |
|       - | 8417 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8418 | `		 *  immediately follow` |
|       - | 8419 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8420 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8421 | `		 * Symisc extension:` |
|       - | 8422 | `		 *   This short syntax works with all PHP opening` |
|       - | 8423 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8424 | `		 *   only short tag.` |
|       - | 8425 | `		 */` |
|       - | 8426 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8427 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8428 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8429 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8430 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8431 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8432 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8433 | `		}` |
|       3 | 8434 | `		return SXRET_OK;` |
|       - | 8435 | `	}` |
|       - | 8436 | `	/* Compile the PHP chunk */` |
|   11498 | 8437 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8438 | `	/* Fix exceptions jumps */` |
|   11498 | 8439 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8440 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11498 | 8441 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8442 | `		rc = SXERR_ABORT;` |
|       1 | 8443 | `	}` |
|       - | 8444 | `	/* Reset container */` |
|   11498 | 8445 | `	SySetReset(&pGen->aGoto);` |
|   11498 | 8446 | `	SySetReset(&pGen->aLabel);` |
|       - | 8447 | `	/* Compilation result */` |
|   11498 | 8448 | `	return rc;` |
|    5751 | 8449 |  |
|       - | 8450 | `/*` |
|       - | 8451 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8452 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8453 | ` * This is the only compile interface exported from this file.` |
|       - | 8454 | ` */` |
|   13610 | 8455 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8456 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8457 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8458 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8459 | `	)` |
|       2 | 8460 |  |
|       - | 8461 | `	SySet aPhpToken,aRawToken;` |
|       - | 8462 | `	ph7_gen_state *pCodeGen;` |
|       - | 8463 | `	ph7_value *pRawObj;` |
|       - | 8464 | `	sxu32 nObjIdx;` |
|       - | 8465 | `	sxi32 nRawObj;` |
|       - | 8466 | `	int is_expr;` |
|       - | 8467 | `	sxi32 rc;` |
|   13612 | 8468 | `	if( pScript->nByte < 1 ){` |
|       - | 8469 | `		/* Nothing to compile */` |
|     ! 0 | 8470 | `		return PH7_OK;` |
|       - | 8471 | `	}` |
|       - | 8472 | `	/* Initialize the tokens containers */` |
|   13612 | 8473 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13612 | 8474 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13612 | 8475 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13612 | 8476 | `	is_expr = 0;` |
|   13612 | 8477 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8478 | `		SyToken sTmp;` |
|       - | 8479 | `		/* PHP only: -*/` |
|    2686 | 8480 | `		sTmp.nLine = 1;` |
|    2686 | 8481 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2686 | 8482 | `		sTmp.pUserData = 0;` |
|    2686 | 8483 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2686 | 8484 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2686 | 8485 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8486 | `			/* A simple PHP expression */` |
|     ! 0 | 8487 | `			is_expr = 1;` |
|     ! 0 | 8488 | `		}` |
|    1344 | 8489 | `	}else{` |
|       - | 8490 | `		/* Tokenize raw text */` |
|   10928 | 8491 | `		SySetAlloc(&aRawToken,32);` |
|   10928 | 8492 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8493 | `	}` |
|   13612 | 8494 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8495 | `	/* Process high-level tokens */` |
|   13612 | 8496 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13612 | 8497 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13612 | 8498 | `	rc = PH7_OK;` |
|   13612 | 8499 | `	if( is_expr ){` |
|       - | 8500 | `		/* Compile the expression */` |
|     ! 0 | 8501 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8502 | `		goto cleanup;` |
|       - | 8503 | `	}` |
|   13612 | 8504 | `	nObjIdx = 0;` |
|       - | 8505 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8506 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8507 | `	 * preventing namespace bleeding across include()d files. */` |
|   13612 | 8508 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8509 | `	/* Start the compilation process */` |
|   12272 | 8510 | `	for(;;){` |
|   36040 | 8511 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13608 | 8512 | `			break; /* No more tokens to process */` |
|       - | 8513 | `		}` |
|   22434 | 8514 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8515 | `			/* Compile the PHP chunk */` |
|   11500 | 8516 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11500 | 8517 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8518 | `				break;` |
|       - | 8519 | `			}` |
|   11496 | 8520 | `			continue;` |
|       - | 8521 | `		}` |
|       - | 8522 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10936 | 8523 | `		nRawObj = 0;` |
|   21870 | 8524 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8525 | `			/* Consume the raw chunk without any processing */` |
|   10936 | 8526 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10936 | 8527 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8528 | `				rc = SXERR_MEM;` |
|     ! 0 | 8529 | `				break;` |
|       - | 8530 | `			}` |
|       - | 8531 | `			/* Mark as constant and emit the load constant instruction */` |
|   10936 | 8532 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10936 | 8533 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10936 | 8534 | `			++nRawObj;` |
|   10936 | 8535 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8536 | `		}` |
|   10936 | 8537 | `		if( nRawObj > 0 ){` |
|       - | 8538 | `			/* Emit the consume instruction */` |
|   10936 | 8539 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5467 | 8540 | `		}` |
|    6807 | 8541 | `	}` |
|    6805 | 8542 | `cleanup:` |
|   13612 | 8543 | `	SySetRelease(&aRawToken);` |
|   13612 | 8544 | `	SySetRelease(&aPhpToken);` |
|   13612 | 8545 | `	return rc;` |
|    6807 | 8546 |  |
|       - | 8547 | `/*` |
|       - | 8548 | ` * Utility routines.Initialize the code generator.` |
|       - | 8549 | ` */` |
|    2656 | 8550 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8551 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8552 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8553 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8554 | `	)` |
|       2 | 8555 |  |
|    2658 | 8556 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8557 | `	/* Zero the structure */` |
|    2658 | 8558 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8559 | `	/* Initial state */` |
|    2658 | 8560 | `	pGen->pVm  = &(*pVm);` |
|    2658 | 8561 | `	pGen->xErr = xErr;` |
|    2658 | 8562 | `	pGen->pErrData = pErrData;` |
|    2658 | 8563 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2658 | 8564 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2658 | 8565 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2658 | 8566 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8567 | `	/* Error log buffer */` |
|    2658 | 8568 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8569 | `	/* General purpose working buffer */` |
|    2658 | 8570 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8571 | `	/* Namespace state */` |
|    2658 | 8572 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2658 | 8573 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2658 | 8574 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2658 | 8575 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8576 | `	/* Create the global scope */` |
|    2658 | 8577 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8578 | `	/* Point to the global scope */` |
|    2658 | 8579 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2658 | 8580 | `	return SXRET_OK;` |
|       2 | 8581 |  |
|       - | 8582 | `/*` |
|       - | 8583 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8584 | ` */` |
|   16000 | 8585 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8586 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8587 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8588 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8589 | `	)` |
|       2 | 8590 |  |
|   16002 | 8591 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8592 | `	GenBlock *pBlock,*pParent;` |
|       - | 8593 | `	/* Reset state */` |
|   16002 | 8594 | `	SySetReset(&pGen->aLabel);` |
|   16002 | 8595 | `	SySetReset(&pGen->aGoto);` |
|   16002 | 8596 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16002 | 8597 | `	SyBlobRelease(&pGen->sWorker);` |
|   16002 | 8598 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16002 | 8599 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16002 | 8600 | `	SyHashRelease(&pGen->hUseImports);` |
|   16002 | 8601 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16002 | 8602 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16002 | 8603 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16002 | 8604 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16002 | 8605 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8606 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8607 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8608 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8609 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8610 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8611 | `	 * number of unique names, which is acceptable. */` |
|       - | 8612 | `	/* Point to the global scope */` |
|   16002 | 8613 | `	pBlock = pGen->pCurrent;` |
|   16002 | 8614 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8615 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8616 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8617 | `		pBlock = pParent;` |
|     ! 0 | 8618 | `	}` |
|   16002 | 8619 | `	pGen->xErr = xErr;` |
|   16002 | 8620 | `	pGen->pErrData = pErrData;` |
|   16002 | 8621 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16002 | 8622 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16002 | 8623 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16002 | 8624 | `	pGen->nErr = 0;` |
|   16002 | 8625 | `	return SXRET_OK;` |
|       2 | 8626 |  |
|       - | 8627 | `/*` |
|       - | 8628 | ` * Generate a compile-time error message.` |
|       - | 8629 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8630 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8631 | ` * abort compilation immediately.` |
|       - | 8632 | ` */` |
|     486 | 8633 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8634 |  |
|     488 | 8635 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     488 | 8636 | `	const char *zErr = "Error";` |
|       - | 8637 | `	SyString *pFile;` |
|       - | 8638 | `	va_list ap;` |
|       - | 8639 | `	sxi32 rc;` |
|       - | 8640 | `	/* Reset the working buffer */` |
|     488 | 8641 | `	SyBlobReset(pWorker);` |
|       - | 8642 | `	/* Peek the processed file path if available */` |
|     488 | 8643 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     488 | 8644 | `	if( nErrType == E_ERROR ){` |
|       - | 8645 | `		/* Increment the error counter */` |
|     422 | 8646 | `		pGen->nErr++;` |
|     422 | 8647 | `		if( pGen->nErr > 15 ){` |
|       - | 8648 | `			/* Error count limit reached */` |
|       5 | 8649 | `			if( pGen->xErr ){` |
|       5 | 8650 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8651 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8652 | `				if( pFile ){` |
|       5 | 8653 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8654 | `				}` |
|       5 | 8655 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8656 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8657 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8658 | `				}` |
|       2 | 8659 | `			}` |
|       - | 8660 | `			/* Abort immediately */` |
|       5 | 8661 | `			return SXERR_ABORT;` |
|       - | 8662 | `		}` |
|     208 | 8663 | `	}` |
|     484 | 8664 | `	if( pGen->xErr == 0 ){` |
|       - | 8665 | `		/* No available error consumer,return immediately */` |
|       3 | 8666 | `		return SXRET_OK;` |
|       - | 8667 | `	}` |
|     481 | 8668 | `	switch(nErrType){` |
|     415 | 8669 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8670 | `	case E_WARNING: zErr = "Warning";     break;` |
|      31 | 8671 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8672 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8673 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8674 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8675 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8676 | `	default:` |
|     ! 0 | 8677 | `		break;` |
|       - | 8678 | `	}` |
|     481 | 8679 | `	rc = SXRET_OK;` |
|       - | 8680 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     481 | 8681 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     481 | 8682 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     481 | 8683 | `	va_start(ap,zFormat);` |
|     481 | 8684 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     481 | 8685 | `	va_end(ap);` |
|     481 | 8686 | `	if( pFile ){` |
|     481 | 8687 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     240 | 8688 | `	}` |
|       - | 8689 | `	/* Append a new line */` |
|     481 | 8690 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     481 | 8691 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8692 | `		/* Consume the generated error message */` |
|     481 | 8693 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     240 | 8694 | `	}` |
|     481 | 8695 | `	return rc;` |
|     245 | 8696 |  |
|       - | 8697 |  |
