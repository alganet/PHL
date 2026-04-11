# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4410/5661 lines (77.90%)

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
|    3014 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    3016 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8477 |  131 | `	for(;;){` |
|   16956 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2908 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2908 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2882 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      13 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   14076 |  140 | `		pBlock = pBlock->pParent;` |
|   14076 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1509 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  586698 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  586700 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  586700 |  162 | `	pBlock->pUserData   = pUserData;` |
|  586700 |  163 | `	pBlock->pGen        = pGen;` |
|  586700 |  164 | `	pBlock->iFlags      = iType;` |
|  586700 |  165 | `	pBlock->pParent     = 0;` |
|  586700 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  586700 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  586700 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  583950 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  583952 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  583952 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  583952 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  583952 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  583952 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  583952 |  200 | `	pGen->pCurrent = pBlock;` |
|  583952 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  282614 |  203 | `		*ppBlock = pBlock;` |
|  141306 |  204 | `	}` |
|  583952 |  205 | `	return SXRET_OK;` |
|  291977 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  583942 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  583944 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  583944 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  583944 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  583942 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  583944 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  583944 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  583944 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  583944 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  583942 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  583944 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  583944 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  583944 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  583944 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  583944 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  583944 |  244 | `	return SXRET_OK;` |
|  291973 |  245 |  |
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
|  177942 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  177944 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  177944 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  177944 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  177944 |  265 | `	return rc;` |
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
|  415628 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  415630 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  762340 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  346712 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  135012 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  211702 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   33762 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  177942 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  177942 |  298 | `		if( pInstr ){` |
|  177942 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  177942 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  177942 |  302 | `			aFix[n].nJumpType = -1;` |
|   88970 |  303 | `		}` |
|   88972 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  415630 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  158666 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  158668 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  158814 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  158666 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  158798 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  158666 |  358 | `	return SXRET_OK;` |
|   79335 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  516640 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  516642 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  516642 |  367 | `	if( pEntry == 0 ){` |
|  254790 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  261854 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  261854 |  371 | `	return SXRET_OK;` |
|  258322 |  372 |  |
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
|  254788 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  254790 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  254790 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  127394 |  387 | `	}` |
|  254790 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   90640 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   90642 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   90642 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   90642 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   90642 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   90642 |  408 | `	return pObj;` |
|   45322 |  409 |  |
|       - |  410 | `/*` |
|       - |  411 | ` * Implementation of the PHP language constructs.` |
|       - |  412 | ` */` |
|       - |  413 | `/* Forward declaration */` |
|       - |  414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |  415 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd);` |
|       - |  416 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |  417 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |  418 | `/*` |
|       - |  419 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |  420 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |  421 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |  422 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |  423 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |  424 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |  425 | ` * inputs like a thousand-digit number.` |
|       - |  426 | ` */` |
|       - |  427 | `#define GEN_NUM_SCRATCH 128` |
|       - |  428 | `/*` |
|       - |  429 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |  430 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |  431 | ` *   base  2 => 0 or 1` |
|       - |  432 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |  433 | ` *              decimal scan in the lexer)` |
|       - |  434 | ` */` |
|    1076 |  435 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |  436 |  |
|    1078 |  437 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |  438 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |  439 | `	return SyisDigit(c);` |
|     540 |  440 |  |
|       - |  441 | `/*` |
|       - |  442 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |  443 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |  444 | ` * the exact wording PHP uses:` |
|       - |  445 | ` *` |
|       - |  446 | ` *   syntax error, unexpected identifier "X"` |
|       - |  447 | ` *` |
|       - |  448 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |  449 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |  450 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |  451 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |  452 | ` * no forward rescan needed.` |
|       - |  453 | ` *` |
|       - |  454 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |  455 | ` * returns 0 when it is well-formed.` |
|       - |  456 | ` */` |
|   91160 |  457 | `static int GenStateFindBadNumericSeparator(` |
|       - |  458 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |  459 |  |
|   91162 |  460 | `	const char *z = pRaw->zString;` |
|   91162 |  461 | `	sxu32 n = pRaw->nByte;` |
|   91162 |  462 | `	int base = 10;` |
|       - |  463 | `	sxu32 i, start;` |
|   91162 |  464 | `	if( n < 2 ) return 0;` |
|    8210 |  465 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |  466 | `		base = 16;` |
|    8175 |  467 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |  468 | `		base = 2;` |
|     139 |  469 | `	}` |
|   30566 |  470 | `	for( i = 0; i < n; ++i ){` |
|   22372 |  471 | `		if( z[i] != '_' ) continue;` |
|     814 |  472 | `		if( i > 0 && i + 1 < n` |
|     543 |  473 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |  474 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |  475 | `			continue; /* well-placed separator */` |
|       - |  476 | `		}` |
|       - |  477 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |  478 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |  479 | `		start = i;` |
|      20 |  480 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |  481 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |  482 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |  483 | `		}` |
|      15 |  484 | `		*pBadStart = &z[start];` |
|      15 |  485 | `		*pBadLen = n - start;` |
|      15 |  486 | `		return 1;` |
|     ! 0 |  487 | `	}` |
|    8196 |  488 | `	return 0;` |
|   45582 |  489 |  |
|       - |  490 | `/*` |
|       - |  491 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |  492 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |  493 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |  494 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |  495 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |  496 | ` * so callers can bail from the current construct).` |
|       - |  497 | ` */` |
|   91160 |  498 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |  499 |  |
|   91162 |  500 | `	const char *zBad = 0;` |
|   91162 |  501 | `	sxu32 nBad = 0;` |
|       - |  502 | `	SyString sBad;` |
|       - |  503 | `	sxi32 rc;` |
|   91162 |  504 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   91148 |  505 | `		return SXRET_OK;` |
|       - |  506 | `	}` |
|      15 |  507 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |  508 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  509 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |  510 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  511 | `		return SXERR_ABORT;` |
|       - |  512 | `	}` |
|      15 |  513 | `	return SXERR_SYNTAX;` |
|   45582 |  514 |  |
|       - |  515 | `/*` |
|       - |  516 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |  517 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |  518 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |  519 | ` *` |
|       - |  520 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |  521 | ` * and *pzAlloc is set to NULL.` |
|       - |  522 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |  523 | ` * and *pzAlloc is set to NULL.` |
|       - |  524 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |  525 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |  526 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |  527 | ` *` |
|       - |  528 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |  529 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |  530 | ` */` |
|   91146 |  531 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |  532 | `	SyMemBackend *pAlloc,` |
|       - |  533 | `	const SyString *pToken,` |
|       - |  534 | `	char *zScratch, sxu32 nScratch,` |
|       - |  535 | `	SyString *pOut, char **pzAlloc)` |
|       2 |  536 |  |
|       - |  537 | `	sxu32 i, j;` |
|   91148 |  538 | `	int hasUnderscore = 0;` |
|       - |  539 | `	char *zBuf;` |
|   91148 |  540 | `	*pzAlloc = 0;` |
|  194390 |  541 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  103496 |  542 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   51623 |  543 | `	}` |
|   91148 |  544 | `	if( !hasUnderscore ){` |
|   90896 |  545 | `		SyStringDupPtr(pOut, pToken);` |
|   90896 |  546 | `		return SXRET_OK;` |
|       - |  547 | `	}` |
|     253 |  548 | `	if( pToken->nByte <= nScratch ){` |
|     251 |  549 | `		zBuf = zScratch;` |
|     126 |  550 | `	}else{` |
|       3 |  551 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |  552 | `		if( zBuf == 0 ){` |
|     ! 0 |  553 | `			return SXERR_ABORT;` |
|       - |  554 | `		}` |
|       3 |  555 | `		*pzAlloc = zBuf;` |
|       - |  556 | `	}` |
|     253 |  557 | `	j = 0;` |
|    2895 |  558 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |  559 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |  560 | `	}` |
|     253 |  561 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |  562 | `	return SXRET_OK;` |
|   45575 |  563 |  |
|       - |  564 | `/*` |
|       - |  565 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |  566 | ` * Notes on the integer type.` |
|       - |  567 | ` *  According to the PHP language reference manual` |
|       - |  568 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |  569 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |  570 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |  571 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |  572 | ` * Symisc eXtension to the integer type.` |
|       - |  573 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |  574 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |  575 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |  576 | ` *  [i.e: either 32bit or 64bit].` |
|       - |  577 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |  578 | ` *  documentation.` |
|       - |  579 | ` */` |
|   91132 |  580 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  581 |  |
|   91134 |  582 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   91134 |  583 | `	sxu32 nIdx = 0;` |
|       - |  584 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   91134 |  585 | `	char *zAlloc = 0;` |
|       - |  586 | `	SyString sNum;` |
|       - |  587 | `	sxi32 rc;` |
|   45566 |  588 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   91134 |  589 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   91134 |  590 | `	if( rc != SXRET_OK ){` |
|      11 |  591 | `		return rc;` |
|       - |  592 | `	}` |
|  136685 |  593 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   45561 |  594 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   91124 |  595 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  596 | `		return SXERR_ABORT;` |
|       - |  597 | `	}` |
|   91124 |  598 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  599 | `		ph7_value *pObj;` |
|       - |  600 | `		sxi64 iValue;` |
|   90642 |  601 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   90642 |  602 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   90642 |  603 | `		if( pObj == 0 ){` |
|     ! 0 |  604 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  605 | `			return SXERR_ABORT;` |
|       - |  606 | `		}` |
|   90642 |  607 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   45322 |  608 | `	}else{` |
|       - |  609 | `		/* Real number */` |
|       - |  610 | `		ph7_value *pObj;` |
|       - |  611 | `		/* Reserve a new constant */` |
|     484 |  612 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     484 |  613 | `		if( pObj == 0 ){` |
|     ! 0 |  614 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  615 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  616 | `			return SXERR_ABORT;` |
|       - |  617 | `		}` |
|     484 |  618 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     484 |  619 | `		PH7_MemObjToReal(pObj);` |
|       - |  620 | `	}` |
|   91124 |  621 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  622 | `	/* Emit the load constant instruction */` |
|   91124 |  623 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  624 | `	/* Node successfully compiled */` |
|   91124 |  625 | `	return SXRET_OK;` |
|   45568 |  626 |  |
|       - |  627 | `/*` |
|       - |  628 | ` * Compile a single quoted string.` |
|       - |  629 | ` * According to the PHP language reference manual:` |
|       - |  630 | ` *` |
|       - |  631 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |  632 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |  633 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |  634 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |  635 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |  636 | ` *` |
|       - |  637 | ` */` |
|   59076 |  638 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  639 |  |
|   59078 |  640 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  641 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  642 | `	ph7_value *pObj;` |
|       - |  643 | `	sxu32 nIdx;` |
|   59078 |  644 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  645 | `	/* Delimit the string */` |
|   59078 |  646 | `	zIn  = pStr->zString;` |
|   59078 |  647 | `	zEnd = &zIn[pStr->nByte];` |
|   59078 |  648 | `	if( zIn >= zEnd ){` |
|       - |  649 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  650 | `		 * rather than reserving a new object each time. */` |
|     140 |  651 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     140 |  652 | `		return SXRET_OK;` |
|       - |  653 | `	}` |
|   58940 |  654 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  655 | `		/* Already processed,emit the load constant instruction` |
|       - |  656 | `		 * and return.` |
|       - |  657 | `		 */` |
|   17390 |  658 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17390 |  659 | `		return SXRET_OK;` |
|       - |  660 | `	}` |
|       - |  661 | `	/* Reserve a new constant */` |
|   41552 |  662 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   41552 |  663 | `	if( pObj == 0 ){` |
|     ! 0 |  664 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  665 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  666 | `		return SXERR_ABORT;` |
|       - |  667 | `	}` |
|   41552 |  668 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  669 | `	/* Compile the node */` |
|   41592 |  670 | `	for(;;){` |
|   83186 |  671 | `		if( zIn >= zEnd ){` |
|       - |  672 | `			/* End of input */` |
|   41552 |  673 | `			break;` |
|       - |  674 | `		}` |
|   41636 |  675 | `		zCur = zIn;` |
|  661372 |  676 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  619738 |  677 | `			zIn++;` |
|       2 |  678 | `		}` |
|   41636 |  679 | `		if( zIn > zCur ){` |
|       - |  680 | `			/* Append raw contents*/` |
|   41616 |  681 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20807 |  682 | `		}` |
|   41636 |  683 | `		zIn++;` |
|   41636 |  684 | `		if( zIn < zEnd ){` |
|     105 |  685 | `			if( zIn[0] == '\\' ){` |
|       - |  686 | `				/* A literal backslash */` |
|      23 |  687 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |  688 | `			}else if( zIn[0] == '\'' ){` |
|       - |  689 | `				/* A single quote */` |
|      11 |  690 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |  691 | `			}else{` |
|       - |  692 | `				/* verbatim copy */` |
|      73 |  693 | `				zIn--;` |
|      73 |  694 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |  695 | `				zIn++;` |
|       - |  696 | `			}` |
|      52 |  697 | `		}` |
|       - |  698 | `		/* Advance the stream cursor */` |
|   41636 |  699 | `		zIn++;` |
|       2 |  700 | `	}` |
|       - |  701 | `	/* Emit the load constant instruction */` |
|   41552 |  702 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   41552 |  703 | `	if( pStr->nByte < 1024 ){` |
|       - |  704 | `		/* Install in the literal table */` |
|   41552 |  705 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20775 |  706 | `	}` |
|       - |  707 | `	/* Node successfully compiled */` |
|   41552 |  708 | `	return SXRET_OK;` |
|   29540 |  709 |  |
|       - |  710 | `/*` |
|       - |  711 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |  712 | ` *` |
|       - |  713 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |  714 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |  715 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |  716 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |  717 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |  718 | ` *` |
|       - |  719 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |  720 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |  721 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |  722 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |  723 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |  724 | ` *     whitespace.` |
|       - |  725 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |  726 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |  727 | ` */` |
|     106 |  728 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |  729 |  |
|     108 |  730 | `	SyString *pIn = &pGen->pIn->sData;` |
|     108 |  731 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  732 | `	const char *zPrefix;` |
|       - |  733 | `	const char *z, *zEnd;` |
|       - |  734 | `	char *zBuf, *zDst;` |
|     108 |  735 | `	if( nIndent == 0 ){` |
|       - |  736 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      64 |  737 | `		*pOut = *pIn;` |
|      64 |  738 | `		return SXRET_OK;` |
|       - |  739 | `	}` |
|       - |  740 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |  741 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |  742 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |  743 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |  744 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |  745 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |  746 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |  747 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |  748 | `		zPrefix += 2;` |
|     ! 0 |  749 | `	}else{` |
|      46 |  750 | `		zPrefix += 1;` |
|       - |  751 | `	}` |
|       - |  752 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |  753 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |  754 | `	if( zBuf == 0 ){` |
|     ! 0 |  755 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  756 | `		return SXERR_ABORT;` |
|       - |  757 | `	}` |
|      46 |  758 | `	zDst = zBuf;` |
|      46 |  759 | `	z = pIn->zString;` |
|      46 |  760 | `	zEnd = z + pIn->nByte;` |
|     128 |  761 | `	while( z < zEnd ){` |
|      70 |  762 | `		const char *zLine = z;` |
|       - |  763 | `		sxu32 nLine;` |
|       - |  764 | `		int bEmpty;` |
|     798 |  765 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |  766 | `			z++;` |
|       2 |  767 | `		}` |
|      70 |  768 | `		nLine = (sxu32)(z - zLine);` |
|      70 |  769 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |  770 | `		if( !bEmpty ){` |
|       - |  771 | `			sxu32 i;` |
|      66 |  772 | `			if( nLine < nIndent ){` |
|     ! 0 |  773 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  774 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |  775 | `					nIndent);` |
|     ! 0 |  776 | `				return SXERR_ABORT;` |
|       - |  777 | `			}` |
|     268 |  778 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |  779 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |  780 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |  781 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |  782 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  783 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |  784 | `					}else{` |
|       7 |  785 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  786 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |  787 | `							nIndent);` |
|       - |  788 | `					}` |
|       9 |  789 | `					return SXERR_ABORT;` |
|       - |  790 | `				}` |
|     103 |  791 | `			}` |
|      57 |  792 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |  793 | `			zDst += nLine - nIndent;` |
|      33 |  794 | `		}else if( nLine == 1 ){` |
|       - |  795 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |  796 | `			*zDst++ = '\r';` |
|     ! 0 |  797 | `		}` |
|      61 |  798 | `		if( z < zEnd ){` |
|      25 |  799 | `			*zDst++ = '\n';` |
|      25 |  800 | `			z++;` |
|      12 |  801 | `		}` |
|       1 |  802 | `	}` |
|      37 |  803 | `	pOut->zString = zBuf;` |
|      37 |  804 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |  805 | `	return SXRET_OK;` |
|      55 |  806 |  |
|       - |  807 | `/*` |
|       - |  808 | ` * Compile a nowdoc string.` |
|       - |  809 | ` * According to the PHP language reference manual:` |
|       - |  810 | ` *` |
|       - |  811 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  812 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  813 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  814 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  815 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  816 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  817 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  818 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  819 | ` *  of the closing identifier.` |
|       - |  820 | ` */` |
|      42 |  821 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  822 |  |
|       - |  823 | `	SyString sStripped;` |
|       - |  824 | `	SyString *pStr;` |
|       - |  825 | `	ph7_value *pObj;` |
|       - |  826 | `	sxu32 nIdx;` |
|       - |  827 | `	sxi32 rc;` |
|      44 |  828 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |  829 | `	if( rc != SXRET_OK ){` |
|       5 |  830 | `		return rc;` |
|       - |  831 | `	}` |
|      40 |  832 | `	pStr = &sStripped;` |
|      40 |  833 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |  834 | `	if( pStr->nByte <= 0 ){` |
|       - |  835 | `		/* Empty string,load NULL */` |
|       7 |  836 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  837 | `		return SXRET_OK;` |
|       - |  838 | `	}` |
|       - |  839 | `	/* Reserve a new constant */` |
|      34 |  840 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |  841 | `	if( pObj == 0 ){` |
|     ! 0 |  842 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  843 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  844 | `		return SXERR_ABORT;` |
|       - |  845 | `	}` |
|       - |  846 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |  847 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  848 | `	/* Emit the load constant instruction */` |
|      34 |  849 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  850 | `	/* Node successfully compiled */` |
|      34 |  851 | `	return SXRET_OK;` |
|      23 |  852 |  |
|       - |  853 | `/*` |
|       - |  854 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  855 | ` * According to the PHP language reference manual` |
|       - |  856 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  857 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  858 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  859 | ` *  property in a string with a minimum of effort.` |
|       - |  860 | ` *  Simple syntax` |
|       - |  861 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  862 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  863 | ` *   the end of the name.` |
|       - |  864 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  865 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  866 | ` *   as to simple variables.` |
|       - |  867 | ` *  Complex (curly) syntax` |
|       - |  868 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  869 | ` *   of complex expressions.` |
|       - |  870 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  871 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  872 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  873 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  874 | ` */` |
|    1818 |  875 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  876 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  877 | `	sxu32 nLine,         /* Line number */` |
|       - |  878 | `	const char *zIn,     /* Raw expression */` |
|       - |  879 | `	const char *zEnd     /* End of the expression */` |
|       - |  880 | `	)` |
|       2 |  881 |  |
|       - |  882 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  883 | `	SySet sToken;` |
|       - |  884 | `	sxi32 rc;` |
|       - |  885 | `	/* Initialize the token set */` |
|    1820 |  886 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  887 | `	/* Preallocate some slots */` |
|    1820 |  888 | `	SySetAlloc(&sToken,0x08);` |
|       - |  889 | `	/* Tokenize the text */` |
|    1820 |  890 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  891 | `	/* Swap delimiter */` |
|    1820 |  892 | `	pTmpIn  = pGen->pIn;` |
|    1820 |  893 | `	pTmpEnd = pGen->pEnd;` |
|    1820 |  894 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1820 |  895 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  896 | `	/* Compile the expression */` |
|    1820 |  897 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  898 | `	/* Restore token stream */` |
|    1820 |  899 | `	pGen->pIn  = pTmpIn;` |
|    1820 |  900 | `	pGen->pEnd = pTmpEnd;` |
|       - |  901 | `	/* Release the token set */` |
|    1820 |  902 | `	SySetRelease(&sToken);` |
|       - |  903 | `	/* Compilation result */` |
|    1820 |  904 | `	return rc;` |
|       2 |  905 |  |
|       - |  906 | `/*` |
|       - |  907 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  908 | ` */` |
|   17498 |  909 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  910 |  |
|       - |  911 | `	ph7_value *pConstObj;` |
|   17500 |  912 | `	sxu32 nIdx = 0;` |
|       - |  913 | `	/* Reserve a new constant */` |
|   17500 |  914 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   17500 |  915 | `	if( pConstObj == 0 ){` |
|     ! 0 |  916 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  917 | `		return 0;` |
|       - |  918 | `	}` |
|   17500 |  919 | `	(*pCount)++;` |
|   17500 |  920 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  921 | `	/* Emit the load constant instruction */` |
|   17500 |  922 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17500 |  923 | `	return pConstObj;` |
|    8751 |  924 |  |
|       - |  925 | `/*` |
|       - |  926 | ` * Compile a double quoted/heredoc string.` |
|       - |  927 | ` * According to the PHP language reference manual` |
|       - |  928 | ` * Heredoc` |
|       - |  929 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  930 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  931 | ` *  to close the quotation.` |
|       - |  932 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  933 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  934 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  935 | ` *  Warning` |
|       - |  936 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  937 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  938 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  939 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  940 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  941 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  942 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  943 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  944 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  945 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  946 | ` * Double quoted` |
|       - |  947 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  948 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  949 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  950 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  951 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  952 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  953 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  954 | ` *  \\ backslash` |
|       - |  955 | ` *  \$ dollar sign` |
|       - |  956 | ` *  \" double-quote` |
|       - |  957 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  958 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  959 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  960 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  961 | ` * See string parsing for details.` |
|       - |  962 | ` */` |
|   16206 |  963 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  964 |  |
|   16208 |  965 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  966 | `	const char *zIn,*zCur,*zEnd;` |
|   16208 |  967 | `	ph7_value *pObj = 0;` |
|       - |  968 | `	sxi32 iCons;` |
|       - |  969 | `	sxi32 rc;` |
|       - |  970 | `	/* Delimit the string */` |
|   16208 |  971 | `	zIn  = pStr->zString;` |
|   16208 |  972 | `	zEnd = &zIn[pStr->nByte];` |
|   16208 |  973 | `	if( zIn >= zEnd ){` |
|       - |  974 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  975 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  976 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  977 | `		 */` |
|     232 |  978 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     232 |  979 | `		return SXRET_OK;` |
|       - |  980 | `	}` |
|   15978 |  981 | `	zCur = 0;` |
|       - |  982 | `	/* Compile the node */` |
|   15978 |  983 | `	iCons = 0;` |
|    8897 |  984 | `	for(;;){` |
|   26958 |  985 | `		zCur = zIn;` |
|  141326 |  986 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  116188 |  987 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      51 |  988 | `				break;` |
|  116090 |  989 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1722 |  990 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     861 |  991 | `					break;` |
|       - |  992 | `			}` |
|  114370 |  993 | `			zIn++;` |
|       2 |  994 | `		}` |
|   26958 |  995 | `		if( zIn > zCur ){` |
|   12282 |  996 | `			if( pObj == 0 ){` |
|   12006 |  997 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12006 |  998 | `				if( pObj == 0 ){` |
|     ! 0 |  999 | `					return SXERR_ABORT;` |
|       - | 1000 | `				}` |
|    6002 | 1001 | `			}` |
|   12282 | 1002 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6140 | 1003 | `		}` |
|   26958 | 1004 | `		if( zIn >= zEnd ){` |
|   15978 | 1005 | `			break;` |
|       - | 1006 | `		}` |
|   10982 | 1007 | `		if( zIn[0] == '\\' ){` |
|    9164 | 1008 | `			const char *zPtr = 0;` |
|       - | 1009 | `			sxu32 n;` |
|    9164 | 1010 | `			zIn++;` |
|    9164 | 1011 | `			if( zIn >= zEnd ){` |
|     ! 0 | 1012 | `				break;` |
|       - | 1013 | `			}` |
|    9164 | 1014 | `			if( pObj == 0 ){` |
|    5496 | 1015 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5496 | 1016 | `				if( pObj == 0 ){` |
|     ! 0 | 1017 | `					return SXERR_ABORT;` |
|       - | 1018 | `				}` |
|    2747 | 1019 | `			}` |
|    9164 | 1020 | `			n = sizeof(char); /* size of conversion */` |
|    9164 | 1021 | `			switch( zIn[0] ){` |
|       3 | 1022 | `			case '$':` |
|       - | 1023 | `				/* Dollar sign */` |
|       7 | 1024 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 | 1025 | `				break;` |
|      38 | 1026 | `			case '\\':` |
|       - | 1027 | `				/* A literal backslash */` |
|      78 | 1028 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 | 1029 | `				break;` |
|       2 | 1030 | `			case 'a':` |
|       - | 1031 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 | 1032 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 | 1033 | `				break;` |
|       2 | 1034 | `			case 'b':` |
|       - | 1035 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 | 1036 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 | 1037 | `				break;` |
|       4 | 1038 | `			case 'f':` |
|       - | 1039 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 | 1040 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 | 1041 | `				break;` |
|    4216 | 1042 | `			case 'n':` |
|       - | 1043 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8434 | 1044 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8434 | 1045 | `				break;` |
|      19 | 1046 | `			case 'r':` |
|       - | 1047 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 | 1048 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 | 1049 | `				break;` |
|      24 | 1050 | `			case 't':` |
|       - | 1051 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 | 1052 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 | 1053 | `				break;` |
|       3 | 1054 | `			case 'v':` |
|       - | 1055 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 | 1056 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 | 1057 | `				break;` |
|       1 | 1058 | `			case '\'':` |
|       - | 1059 | `				/* Single quote */` |
|       3 | 1060 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 | 1061 | `				break;` |
|      50 | 1062 | `			case '"':` |
|       - | 1063 | `				/* Double quote */` |
|     102 | 1064 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 | 1065 | `				break;` |
|       5 | 1066 | `			case '0':` |
|       - | 1067 | `				/* NUL byte */` |
|      11 | 1068 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 | 1069 | `				break;` |
|     188 | 1070 | `			case 'x':` |
|     377 | 1071 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - | 1072 | `					int c;` |
|       - | 1073 | `					/* Hex digit */` |
|     363 | 1074 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 | 1075 | `					if( &zIn[2] < zEnd ){` |
|     363 | 1076 | `						c +=  SyHexToint(zIn[2]);` |
|     181 | 1077 | `					}` |
|       - | 1078 | `					/* Output char */` |
|     363 | 1079 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 | 1080 | `					n += sizeof(char) * 2;` |
|     182 | 1081 | `				}else{` |
|       - | 1082 | `					/* Output literal character  */` |
|      15 | 1083 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - | 1084 | `				}` |
|     377 | 1085 | `				break;` |
|      15 | 1086 | `			case 'o':` |
|      31 | 1087 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - | 1088 | `					/* Octal digit stream */` |
|       - | 1089 | `					int c;` |
|      21 | 1090 | `					c = 0;` |
|      21 | 1091 | `					zIn++;` |
|      61 | 1092 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 | 1093 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 | 1094 | `							break;` |
|       - | 1095 | `						}` |
|      41 | 1096 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 | 1097 | `					}` |
|      21 | 1098 | `					if ( c > 0 ){` |
|      15 | 1099 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 | 1100 | `					}` |
|      21 | 1101 | `					n = (sxu32)(zPtr-zIn);` |
|      11 | 1102 | `				}else{` |
|       - | 1103 | `					/* Output literal character  */` |
|      11 | 1104 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - | 1105 | `				}` |
|      31 | 1106 | `				break;` |
|      11 | 1107 | `			default:` |
|       - | 1108 | `				/* Output without a slash */` |
|      23 | 1109 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 | 1110 | `				break;` |
|       - | 1111 | `			}` |
|       - | 1112 | `			/* Advance the stream cursor */` |
|    9164 | 1113 | `			zIn += n;` |
|    9164 | 1114 | `			continue;` |
|       - | 1115 | `		}` |
|    1820 | 1116 | `		if( zIn[0] == '{' ){` |
|       - | 1117 | `			/* Curly syntax */` |
|       - | 1118 | `			const char *zExpr;` |
|     101 | 1119 | `			sxi32 iNest = 1;` |
|     101 | 1120 | `			zIn++;` |
|     101 | 1121 | `			zExpr = zIn;` |
|       - | 1122 | `			/* Synchronize with the next closing curly braces */` |
|    1135 | 1123 | `			while( zIn < zEnd ){` |
|    1135 | 1124 | `				if( zIn[0] == '{' ){` |
|       - | 1125 | `					/* Increment nesting level */` |
|       9 | 1126 | `					iNest++;` |
|    1131 | 1127 | `				}else if(zIn[0] == '}' ){` |
|       - | 1128 | `					/* Decrement nesting level */` |
|     109 | 1129 | `					iNest--;` |
|     109 | 1130 | `					if( iNest <= 0 ){` |
|     101 | 1131 | `						break;` |
|       - | 1132 | `					}` |
|       4 | 1133 | `				}` |
|    1035 | 1134 | `				zIn++;` |
|       1 | 1135 | `			}` |
|       - | 1136 | `			/* Process the expression */` |
|     101 | 1137 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     101 | 1138 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1139 | `				return SXERR_ABORT;` |
|       - | 1140 | `			}` |
|     101 | 1141 | `			if( rc != SXERR_EMPTY ){` |
|     101 | 1142 | `				++iCons;` |
|      50 | 1143 | `			}` |
|     101 | 1144 | `			if( zIn < zEnd ){` |
|       - | 1145 | `				/* Jump the trailing curly */` |
|     101 | 1146 | `				zIn++;` |
|      50 | 1147 | `			}` |
|      51 | 1148 | `		}else{` |
|       - | 1149 | `			/* Simple syntax */` |
|    1720 | 1150 | `			const char *zExpr = zIn;` |
|       - | 1151 | `			/* Assemble variable name */` |
|     865 | 1152 | `			for(;;){` |
|       - | 1153 | `				/* Jump leading dollars */` |
|    3450 | 1154 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1720 | 1155 | `					zIn++;` |
|       2 | 1156 | `				}` |
|     865 | 1157 | `				for(;;){` |
|   10407 | 1158 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7812 | 1159 | `						zIn++;` |
|       2 | 1160 | `					}` |
|    1732 | 1161 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1162 | `						/* UTF-8 stream */` |
|     ! 0 | 1163 | `						zIn++;` |
|     ! 0 | 1164 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1165 | `							zIn++;` |
|     ! 0 | 1166 | `						}` |
|     ! 0 | 1167 | `						continue;` |
|       - | 1168 | `					}` |
|    1732 | 1169 | `					break;` |
|     ! 0 | 1170 | `				}` |
|    1732 | 1171 | `				if( zIn >= zEnd ){` |
|     102 | 1172 | `					break;` |
|       - | 1173 | `				}` |
|    1632 | 1174 | `				if( zIn[0] == '[' ){` |
|       9 | 1175 | `					sxi32 iSquare = 1;` |
|       9 | 1176 | `					zIn++;` |
|      17 | 1177 | `					while( zIn < zEnd ){` |
|      17 | 1178 | `						if( zIn[0] == '[' ){` |
|     ! 0 | 1179 | `							iSquare++;` |
|      17 | 1180 | `						}else if (zIn[0] == ']' ){` |
|       9 | 1181 | `							iSquare--;` |
|       9 | 1182 | `							if( iSquare <= 0 ){` |
|       9 | 1183 | `								break;` |
|       - | 1184 | `							}` |
|     ! 0 | 1185 | `						}` |
|       9 | 1186 | `						zIn++;` |
|       1 | 1187 | `					}` |
|       9 | 1188 | `					if( zIn < zEnd ){` |
|       9 | 1189 | `						zIn++;` |
|       4 | 1190 | `					}` |
|       9 | 1191 | `					break;` |
|    1624 | 1192 | `				}else if(zIn[0] == '{' ){` |
|       6 | 1193 | `					sxi32 iCurly = 1;` |
|       6 | 1194 | `					zIn++;` |
|      18 | 1195 | `					while( zIn < zEnd ){` |
|      16 | 1196 | `						if( zIn[0] == '{' ){` |
|     ! 0 | 1197 | `							iCurly++;` |
|      16 | 1198 | `						}else if (zIn[0] == '}' ){` |
|       3 | 1199 | `							iCurly--;` |
|       3 | 1200 | `							if( iCurly <= 0 ){` |
|       3 | 1201 | `								break;` |
|       - | 1202 | `							}` |
|     ! 0 | 1203 | `						}` |
|      14 | 1204 | `						zIn++;` |
|       2 | 1205 | `					}` |
|       6 | 1206 | `					if( zIn < zEnd ){` |
|       3 | 1207 | `						zIn++;` |
|       1 | 1208 | `					}` |
|       6 | 1209 | `					break;` |
|    1620 | 1210 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - | 1211 | `					/* Member access operator '->' */` |
|      13 | 1212 | `					zIn += 2;` |
|    1614 | 1213 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - | 1214 | `					/* Static member access operator '::' */` |
|     ! 0 | 1215 | `					zIn += 2;` |
|     ! 0 | 1216 | `				}else{` |
|     805 | 1217 | `					break;` |
|       - | 1218 | `				}` |
|       1 | 1219 | `			}` |
|       - | 1220 | `			/* Process the expression */` |
|    1720 | 1221 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1720 | 1222 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1223 | `				return SXERR_ABORT;` |
|       - | 1224 | `			}` |
|    1720 | 1225 | `			if( rc != SXERR_EMPTY ){` |
|    1718 | 1226 | `				++iCons;` |
|     858 | 1227 | `			}` |
|       - | 1228 | `		}` |
|       - | 1229 | `		/* Invalidate the previously used constant */` |
|    1820 | 1230 | `		pObj = 0;` |
|       2 | 1231 | `	}/*for(;;)*/` |
|   15978 | 1232 | `	if( iCons > 1 ){` |
|       - | 1233 | `		/* Concatenate all compiled constants */` |
|    1372 | 1234 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     685 | 1235 | `	}` |
|       - | 1236 | `	/* Node successfully compiled */` |
|   15978 | 1237 | `	return SXRET_OK;` |
|    8105 | 1238 |  |
|       - | 1239 | `/*` |
|       - | 1240 | ` * Compile a double quoted string.` |
|       - | 1241 | ` *  See the block-comment above for more information.` |
|       - | 1242 | ` */` |
|   16146 | 1243 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1244 |  |
|       - | 1245 | `	sxi32 rc;` |
|   16148 | 1246 | `	rc = GenStateCompileString(&(*pGen));` |
|    8073 | 1247 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1248 | `	/* Compilation result */` |
|   16148 | 1249 | `	return rc;` |
|       2 | 1250 |  |
|       - | 1251 | `/*` |
|       - | 1252 | ` * Compile a Heredoc string.` |
|       - | 1253 | ` *  See the block-comment above for more information.` |
|       - | 1254 | ` */` |
|      64 | 1255 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1256 |  |
|       - | 1257 | `	SyString sOrig, sStripped;` |
|       - | 1258 | `	sxi32 rc;` |
|      66 | 1259 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 | 1260 | `	if( rc != SXRET_OK ){` |
|       5 | 1261 | `		return rc;` |
|       - | 1262 | `	}` |
|       - | 1263 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - | 1264 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - | 1265 | `	 * Restore before returning so downstream code that references pIn is` |
|       - | 1266 | `	 * unaffected, including on the error path. */` |
|      62 | 1267 | `	sOrig = pGen->pIn->sData;` |
|      62 | 1268 | `	pGen->pIn->sData = sStripped;` |
|      62 | 1269 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 | 1270 | `	pGen->pIn->sData = sOrig;` |
|      30 | 1271 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 | 1272 | `	return rc;` |
|      34 | 1273 |  |
|       - | 1274 | `/*` |
|       - | 1275 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1276 | ` *  Notes on array entries.` |
|       - | 1277 | ` *  According to the PHP language reference manual` |
|       - | 1278 | ` *  An array can be created by the array() language construct.` |
|       - | 1279 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1280 | ` *  array(  key =>  value` |
|       - | 1281 | ` *    , ...` |
|       - | 1282 | ` *    )` |
|       - | 1283 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1284 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1285 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1286 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1287 | ` *  contain integer and string indices.` |
|       - | 1288 | ` *  A value can be any PHP type.` |
|       - | 1289 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1290 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1291 | ` *  is specified, that value will be overwritten.` |
|       - | 1292 | ` */` |
|   16334 | 1293 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1294 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1295 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1296 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1297 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1298 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1299 | `	)` |
|       2 | 1300 |  |
|       - | 1301 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1302 | `	sxi32 rc;` |
|       - | 1303 | `	/* Swap token stream */` |
|   16336 | 1304 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1305 | `	/* Compile the expression*/` |
|   16336 | 1306 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1307 | `	/* Restore token stream */` |
|   16336 | 1308 | `	RE_SWAP_DELIMITER(pGen);` |
|   16336 | 1309 | `	return rc;` |
|       2 | 1310 |  |
|       - | 1311 | `/*` |
|       - | 1312 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1313 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1314 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1315 | ` * error message.` |
|       - | 1316 | ` * See the routine responible of compiling the array language construct` |
|       - | 1317 | ` * for more inforation.` |
|       - | 1318 | ` */` |
|      30 | 1319 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1320 |  |
|      32 | 1321 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1322 | `	if( pRoot->pOp ){` |
|      19 | 1323 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1324 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1325 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1326 | `			/* Unexpected expression */` |
|      11 | 1327 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1328 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1329 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1330 | `				rc = SXERR_INVALID;` |
|       5 | 1331 | `			}` |
|       7 | 1332 | `		}` |
|      25 | 1333 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1334 | `		/* Unexpected expression */` |
|       3 | 1335 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1336 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1337 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1338 | `			rc = SXERR_INVALID;` |
|       1 | 1339 | `		}` |
|       1 | 1340 | `	}` |
|      32 | 1341 | `	return rc;` |
|       2 | 1342 |  |
|       - | 1343 | `/*` |
|       - | 1344 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1345 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1346 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1347 | ` */` |
|   24018 | 1348 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1349 |  |
|       - | 1350 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1351 | `	SyToken *pKey,*pCur;` |
|   24020 | 1352 | `	sxi32 iEmitRef = 0;` |
|   24020 | 1353 | `	sxi32 nPair = 0;` |
|       - | 1354 | `	sxi32 iNest;` |
|       - | 1355 | `	sxi32 rc;` |
|   24020 | 1356 | `	xValidator = 0;` |
|   19509 | 1357 | `	for(;;){` |
|       - | 1358 | `		/* Jump leading commas */` |
|   44092 | 1359 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5074 | 1360 | `			pGen->pIn++;` |
|       2 | 1361 | `		}` |
|   39020 | 1362 | `		pCur = pGen->pIn;` |
|   39020 | 1363 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1364 | `			/* No more entry to process */` |
|   24008 | 1365 | `			break;` |
|       - | 1366 | `		}` |
|   15014 | 1367 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1368 | `			continue;` |
|       - | 1369 | `		}` |
|       - | 1370 | `		/* Compile the key if available */` |
|   15014 | 1371 | `		pKey = pCur;` |
|   15014 | 1372 | `		iNest = 0;` |
|   41694 | 1373 | `		while( pCur < pGen->pIn ){` |
|   27902 | 1374 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 | 1375 | `				break;` |
|       - | 1376 | `			}` |
|       - | 1377 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - | 1378 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - | 1379 | `			 * separator — it introduces the expression body. Skip past the` |
|       - | 1380 | `			 * signature so the body scan sees no false '=>'.` |
|       - | 1381 | `			 */` |
|   26686 | 1382 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      74 | 1383 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      74 | 1384 | `				SyToken *pFn = pCur;` |
|      72 | 1385 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 | 1386 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 | 1387 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 | 1388 | `					pFn = &pCur[1];` |
|     ! 0 | 1389 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 | 1390 | `				}` |
|      74 | 1391 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 | 1392 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 | 1393 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1394 | `						pCur++;` |
|     ! 0 | 1395 | `					}` |
|       5 | 1396 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 | 1397 | `						pCur++;` |
|       5 | 1398 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - | 1399 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 | 1400 | `						if( pCur < pGen->pIn ){` |
|       5 | 1401 | `							pCur++;` |
|       2 | 1402 | `						}` |
|       2 | 1403 | `					}` |
|       5 | 1404 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1405 | `						pCur++;` |
|     ! 0 | 1406 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 | 1407 | `							&& pCur->sData.nByte == 1` |
|     ! 0 | 1408 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 | 1409 | `							pCur++;` |
|     ! 0 | 1410 | `						}` |
|     ! 0 | 1411 | `						if( pCur < pGen->pIn` |
|     ! 0 | 1412 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1413 | `							pCur++;` |
|     ! 0 | 1414 | `						}` |
|     ! 0 | 1415 | `					}` |
|       - | 1416 | `					/* The rest of the entry is the arrow function body — no` |
|       - | 1417 | `					 * outer key to extract. Stop the scan here. */` |
|       5 | 1418 | `					pCur = pGen->pIn;` |
|       5 | 1419 | `					break;` |
|       - | 1420 | `				}` |
|      34 | 1421 | `			}` |
|   26682 | 1422 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1423 | `				iNest++;` |
|   26644 | 1424 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1425 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1426 | `				 * parser will shortly detect any syntax error.` |
|       - | 1427 | `				 */` |
|      78 | 1428 | `				iNest--;` |
|      38 | 1429 | `			}` |
|   26682 | 1430 | `			pCur++;` |
|       2 | 1431 | `		}` |
|   15014 | 1432 | `		rc = SXERR_EMPTY;` |
|   15014 | 1433 | `		if( pCur < pGen->pIn ){` |
|    1218 | 1434 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1435 | `				/* Missing value */` |
|      11 | 1436 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1437 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1438 | `					return SXERR_ABORT;` |
|       - | 1439 | `				}` |
|      11 | 1440 | `				return SXRET_OK;` |
|       - | 1441 | `			}` |
|       - | 1442 | `			/* Compile the expression holding the key */` |
|    1208 | 1443 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1444 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 | 1445 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1446 | `				return SXERR_ABORT;` |
|       - | 1447 | `			}` |
|    1208 | 1448 | `			pCur++; /* Jump the '=>' operator */` |
|   14401 | 1449 | `		}else if( pKey == pCur ){` |
|       - | 1450 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1451 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1452 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1453 | `		}else{` |
|       - | 1454 | `			/* Reset back the cursor and point to the entry value */` |
|   13798 | 1455 | `			pCur = pKey;` |
|       - | 1456 | `		}` |
|   15004 | 1457 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1458 | `			/* No available key,load NULL */` |
|   13800 | 1459 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6899 | 1460 | `		}` |
|   15004 | 1461 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1462 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1463 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1464 | `			iEmitRef = 1;` |
|      34 | 1465 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1466 | `			if( pCur >= pGen->pIn ){` |
|       - | 1467 | `				/* Missing value */` |
|       3 | 1468 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1469 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1470 | `					return SXERR_ABORT;` |
|       - | 1471 | `				}` |
|       3 | 1472 | `				return SXRET_OK;` |
|       - | 1473 | `			}` |
|      15 | 1474 | `		}` |
|       - | 1475 | `		/* Compile indice value */` |
|   15002 | 1476 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15002 | 1477 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1478 | `			return SXERR_ABORT;` |
|       - | 1479 | `		}` |
|   15002 | 1480 | `		if( iEmitRef ){` |
|       - | 1481 | `			/* Emit the load reference instruction */` |
|      32 | 1482 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1483 | `		}` |
|   15002 | 1484 | `		xValidator = 0;` |
|   15002 | 1485 | `		iEmitRef = 0;` |
|   15002 | 1486 | `		nPair++;` |
|       2 | 1487 | `	}` |
|       - | 1488 | `	/* Emit the load map instruction */` |
|   24008 | 1489 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1490 | `	/* Node successfully compiled */` |
|   24008 | 1491 | `	return SXRET_OK;` |
|   12011 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Compile the 'array' language construct.` |
|       - | 1495 | ` *	 According to the PHP language reference manual` |
|       - | 1496 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1497 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1498 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1499 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1500 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1501 | ` */` |
|   23740 | 1502 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1503 |  |
|       - | 1504 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   23742 | 1505 | `	pGen->pIn += 2;` |
|   23742 | 1506 | `	pGen->pEnd--;` |
|   11870 | 1507 | `	SXUNUSED(iCompileFlag);` |
|   23742 | 1508 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1509 |  |
|       - | 1510 | `/*` |
|       - | 1511 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1512 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1513 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1514 | ` */` |
|     278 | 1515 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1516 |  |
|       - | 1517 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     280 | 1518 | `	pGen->pIn++;` |
|     280 | 1519 | `	pGen->pEnd--;` |
|     139 | 1520 | `	SXUNUSED(iCompileFlag);` |
|     280 | 1521 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1522 |  |
|       - | 1523 | `/*` |
|       - | 1524 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1525 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1526 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1527 | ` * error message.` |
|       - | 1528 | ` * See the routine responible of compiling the list language construct` |
|       - | 1529 | ` * for more inforation.` |
|       - | 1530 | ` */` |
|     128 | 1531 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1532 |  |
|     130 | 1533 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1534 | `	if( pRoot->pOp ){` |
|     ! 0 | 1535 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1536 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1537 | `				/* Unexpected expression */` |
|     ! 0 | 1538 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1539 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1540 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1541 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1542 | `				}` |
|     ! 0 | 1543 | `		}` |
|     130 | 1544 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1545 | `		/* Unexpected expression */` |
|       5 | 1546 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1547 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1548 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1549 | `			rc = SXERR_INVALID;` |
|       2 | 1550 | `		}` |
|       2 | 1551 | `	}` |
|     130 | 1552 | `	return rc;` |
|       2 | 1553 |  |
|       - | 1554 | `/*` |
|       - | 1555 | ` * Compile the 'list' language construct.` |
|       - | 1556 | ` *  According to the PHP language reference` |
|       - | 1557 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1558 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1559 | ` *  Description` |
|       - | 1560 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1561 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1562 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1563 | ` *  Parameters` |
|       - | 1564 | ` *   $varname: A variable.` |
|       - | 1565 | ` *  Return Values` |
|       - | 1566 | ` *   The assigned array.` |
|       - | 1567 | ` */` |
|       - | 1568 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1569 | `struct NestedListEntry {` |
|       - | 1570 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1571 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1572 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1573 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1574 | `};` |
|       - | 1575 | `/*` |
|       - | 1576 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1577 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1578 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1579 | ` */` |
|      74 | 1580 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1581 |  |
|       - | 1582 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1583 | `	SyToken *pNext;` |
|       - | 1584 | `	sxi32 nExpr;` |
|       - | 1585 | `	sxi32 rc;` |
|      76 | 1586 | `	nExpr = 0;` |
|      76 | 1587 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1588 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1589 | `		if( pGen->pIn < pNext ){` |
|       - | 1590 | `			/* Check for nested list() */` |
|     144 | 1591 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1592 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1593 | `				/* Record this nested list for post-processing */` |
|       3 | 1594 | `				SyToken *pListEnd = 0;` |
|       3 | 1595 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1596 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1597 | `				}` |
|       3 | 1598 | `				if( pListEnd ){` |
|       - | 1599 | `					struct NestedListEntry sEntry;` |
|       3 | 1600 | `					sEntry.nIndex = nExpr;` |
|       3 | 1601 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1602 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1603 | `					sEntry.isShort = 0;` |
|       3 | 1604 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1605 | `				}` |
|       - | 1606 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1607 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1608 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1609 | `				/* Nested short destructuring [...] */` |
|      13 | 1610 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1611 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1612 | `				if( pBracketEnd ){` |
|       - | 1613 | `					struct NestedListEntry sEntry;` |
|      13 | 1614 | `					sEntry.nIndex = nExpr;` |
|      13 | 1615 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1616 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1617 | `					sEntry.isShort = 1;` |
|      13 | 1618 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1619 | `				}` |
|       - | 1620 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1621 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1622 | `			}else{` |
|       - | 1623 | `				/* Compile the expression holding the variable */` |
|     130 | 1624 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1625 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1626 | `					SySetRelease(&sNested);` |
|     ! 0 | 1627 | `					return SXRET_OK;` |
|       - | 1628 | `				}` |
|       - | 1629 | `			}` |
|      73 | 1630 | `		}else{` |
|       - | 1631 | `			/* Empty entry,load NULL */` |
|      13 | 1632 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1633 | `		}` |
|     156 | 1634 | `		nExpr++;` |
|       - | 1635 | `		/* Advance the stream cursor */` |
|     156 | 1636 | `		pGen->pIn = &pNext[1];` |
|       2 | 1637 | `	}` |
|       - | 1638 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1639 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1640 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1641 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1642 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1643 | `	 */` |
|      76 | 1644 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1645 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1646 | `		sxu32 i;` |
|      27 | 1647 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1648 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1649 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1650 | `			ph7_value *pIdx;` |
|       - | 1651 | `			sxu32 nConstIdx;` |
|       - | 1652 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1653 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1654 | `			/* Push the integer index for this nested entry */` |
|      15 | 1655 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1656 | `			if( pIdx == 0 ){` |
|     ! 0 | 1657 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1658 | `				SySetRelease(&sNested);` |
|     ! 0 | 1659 | `				return SXERR_ABORT;` |
|       - | 1660 | `			}` |
|      15 | 1661 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1662 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1663 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1664 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1665 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1666 | `			 */` |
|      15 | 1667 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1668 | `			/* Recursively compile the inner list */` |
|      15 | 1669 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1670 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1671 | `			if( apNested[i].isShort ){` |
|      13 | 1672 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1673 | `			}else{` |
|       3 | 1674 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1675 | `			}` |
|      15 | 1676 | `			pGen->pIn = pSavedIn;` |
|      15 | 1677 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1678 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1679 | `				SySetRelease(&sNested);` |
|     ! 0 | 1680 | `				return SXERR_ABORT;` |
|       - | 1681 | `			}` |
|       - | 1682 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1683 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1684 | `		}` |
|       6 | 1685 | `	}` |
|      76 | 1686 | `	SySetRelease(&sNested);` |
|       - | 1687 | `	/* Node successfully compiled */` |
|      76 | 1688 | `	return SXRET_OK;` |
|      39 | 1689 |  |
|      32 | 1690 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1691 |  |
|       - | 1692 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1693 | `	pGen->pIn += 2;` |
|      34 | 1694 | `	pGen->pEnd--;` |
|      16 | 1695 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1696 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1697 |  |
|      42 | 1698 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1699 |  |
|       - | 1700 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1701 | `	pGen->pIn++;` |
|      44 | 1702 | `	pGen->pEnd--;` |
|      21 | 1703 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1704 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1705 |  |
|       - | 1706 | `/* Forward declarations */` |
|       - | 1707 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1708 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1709 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1710 | `/*` |
|       - | 1711 | ` * Compile an annoynmous function or a closure.` |
|       - | 1712 | ` * According to the PHP language reference` |
|       - | 1713 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1714 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1715 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1716 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1717 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1718 | ` *  Example Anonymous function variable assignment example` |
|       - | 1719 | ` * <?php` |
|       - | 1720 | ` * $greet = function($name)` |
|       - | 1721 | ` * {` |
|       - | 1722 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1723 | ` * };` |
|       - | 1724 | ` * $greet('World');` |
|       - | 1725 | ` * $greet('PHP');` |
|       - | 1726 | ` * ?>` |
|       - | 1727 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1728 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1729 | ` */` |
|     168 | 1730 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1731 |  |
|       - | 1732 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1733 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1734 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1735 | `							  * one thread is allowed to compile the script.` |
|       - | 1736 | `						      */` |
|       - | 1737 | `	ph7_value *pObj;` |
|       - | 1738 | `	SyString sName;` |
|       - | 1739 | `	sxu32 nIdx;` |
|       - | 1740 | `	sxu32 nLen;` |
|       - | 1741 | `	sxi32 rc;` |
|       - | 1742 |  |
|     170 | 1743 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     170 | 1744 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1745 | `		pGen->pIn++;` |
|     ! 0 | 1746 | `	}` |
|       - | 1747 | `	/* Reserve a constant for the lambda */` |
|     170 | 1748 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 | 1749 | `	if( pObj == 0 ){` |
|     ! 0 | 1750 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1751 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1752 | `		return SXERR_ABORT;` |
|       - | 1753 | `	}` |
|       - | 1754 | `	/* Generate a unique name */` |
|     170 | 1755 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1756 | `	/* Make sure the generated name is unique */` |
|     170 | 1757 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1758 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1759 | `	}` |
|     170 | 1760 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     170 | 1761 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1762 | `	/* Compile the lambda body */` |
|     170 | 1763 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     170 | 1764 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1765 | `		return SXERR_ABORT;` |
|       - | 1766 | `	}` |
|     170 | 1767 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1768 | `		/* Emit the load closure instruction */` |
|      16 | 1769 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 | 1770 | `	}else{` |
|       - | 1771 | `		/* Emit the load constant instruction */` |
|     156 | 1772 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1773 | `	}` |
|       - | 1774 | `	/* Node successfully compiled */` |
|     170 | 1775 | `	return SXRET_OK;` |
|      86 | 1776 |  |
|       - | 1777 | `/*` |
|       - | 1778 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - | 1779 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - | 1780 | ` * enclosing arrow level, or has already been captured.` |
|       - | 1781 | ` */` |
|     120 | 1782 | `static sxi32 GenStateArrowAddCapture(` |
|       - | 1783 | `	ph7_gen_state *pGen,` |
|       - | 1784 | `	ph7_vm_func *pFunc,` |
|       - | 1785 | `	const char *zName,` |
|       - | 1786 | `	sxu32 nByte,` |
|       - | 1787 | `	SyString *aShadow,` |
|       - | 1788 | `	sxu32 nShadow)` |
|       1 | 1789 |  |
|       - | 1790 | `	ph7_vm_func_closure_env sEnv;` |
|       - | 1791 | `	ph7_vm_func_closure_env *aEnv;` |
|       - | 1792 | `	sxu32 n, nEnv;` |
|       - | 1793 | `	char *zDup;` |
|     121 | 1794 | `	if( nByte == 0 ){` |
|     ! 0 | 1795 | `		return SXRET_OK;` |
|       - | 1796 | `	}` |
|     120 | 1797 | `	if( nByte == sizeof("this")-1` |
|      65 | 1798 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 | 1799 | `		return SXRET_OK;` |
|       - | 1800 | `	}` |
|     145 | 1801 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      92 | 1802 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      88 | 1803 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      67 | 1804 | `			return SXRET_OK;` |
|       - | 1805 | `		}` |
|      14 | 1806 | `	}` |
|      53 | 1807 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 | 1808 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 | 1809 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 | 1810 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 | 1811 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 | 1812 | `			return SXRET_OK;` |
|       - | 1813 | `		}` |
|      15 | 1814 | `	}` |
|      53 | 1815 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 | 1816 | `	if( zDup == 0 ){` |
|     ! 0 | 1817 | `		return SXERR_ABORT;` |
|       - | 1818 | `	}` |
|      53 | 1819 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 | 1820 | `	sEnv.iFlags = 0;` |
|      53 | 1821 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 | 1822 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 | 1823 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 | 1824 | `	return SXRET_OK;` |
|      61 | 1825 |  |
|       - | 1826 | `/*` |
|       - | 1827 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - | 1828 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - | 1829 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - | 1830 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - | 1831 | ` */` |
|      14 | 1832 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - | 1833 | `	ph7_gen_state *pGen,` |
|       - | 1834 | `	ph7_vm_func *pFunc,` |
|       - | 1835 | `	const char *zIn,` |
|       - | 1836 | `	const char *zEnd,` |
|       - | 1837 | `	SyString *aShadow,` |
|       - | 1838 | `	sxu32 nShadow)` |
|       1 | 1839 |  |
|       - | 1840 | `	sxi32 rc;` |
|     159 | 1841 | `	while( zIn < zEnd ){` |
|     145 | 1842 | `		if( zIn[0] == '\\' ){` |
|     ! 0 | 1843 | `			zIn++;` |
|     ! 0 | 1844 | `			if( zIn < zEnd ){` |
|     ! 0 | 1845 | `				zIn++;` |
|     ! 0 | 1846 | `			}` |
|     ! 0 | 1847 | `			continue;` |
|       - | 1848 | `		}` |
|     144 | 1849 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 | 1850 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 | 1851 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - | 1852 | `			const char *zName;` |
|      13 | 1853 | `			zIn++; /* skip '$' */` |
|      13 | 1854 | `			zName = zIn;` |
|      39 | 1855 | `			while( zIn < zEnd ){` |
|      35 | 1856 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 | 1857 | `				if( c >= 0xc0 ){` |
|     ! 0 | 1858 | `					zIn++;` |
|     ! 0 | 1859 | `					while( zIn < zEnd` |
|     ! 0 | 1860 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1861 | `						zIn++;` |
|     ! 0 | 1862 | `					}` |
|     ! 0 | 1863 | `					continue;` |
|       - | 1864 | `				}` |
|      35 | 1865 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 | 1866 | `					break;` |
|       - | 1867 | `				}` |
|      27 | 1868 | `				zIn++;` |
|       1 | 1869 | `			}` |
|      13 | 1870 | `			if( zIn > zName ){` |
|      19 | 1871 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 | 1872 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 | 1873 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1874 | `					return SXERR_ABORT;` |
|       - | 1875 | `				}` |
|       6 | 1876 | `			}` |
|      13 | 1877 | `			continue;` |
|       - | 1878 | `		}` |
|     133 | 1879 | `		zIn++;` |
|       1 | 1880 | `	}` |
|      15 | 1881 | `	return SXRET_OK;` |
|       8 | 1882 |  |
|       - | 1883 | `/*` |
|       - | 1884 | ` * Scan the body token range of an arrow function for free-variable` |
|       - | 1885 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - | 1886 | ` *   - plain $<id> pairs` |
|       - | 1887 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - | 1888 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - | 1889 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - | 1890 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - | 1891 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - | 1892 | ` *     are never mistakenly captured.` |
|       - | 1893 | ` */` |
|     102 | 1894 | `static sxi32 GenStateArrowCaptureScan(` |
|       - | 1895 | `	ph7_gen_state *pGen,` |
|       - | 1896 | `	ph7_vm_func *pFunc,` |
|       - | 1897 | `	SyToken *pStart,` |
|       - | 1898 | `	SyToken *pEnd,` |
|       - | 1899 | `	SyString *aShadow,` |
|       - | 1900 | `	sxu32 nShadow)` |
|       1 | 1901 |  |
|     103 | 1902 | `	SyToken *pScan = pStart;` |
|       - | 1903 | `	sxi32 rc;` |
|     371 | 1904 | `	while( pScan < pEnd ){` |
|     269 | 1905 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 | 1906 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 | 1907 | `				pScan->sData.zString,` |
|      14 | 1908 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 | 1909 | `				aShadow,nShadow);` |
|      15 | 1910 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1911 | `				return SXERR_ABORT;` |
|       - | 1912 | `			}` |
|      15 | 1913 | `			pScan++;` |
|      15 | 1914 | `			continue;` |
|       - | 1915 | `		}` |
|     255 | 1916 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      19 | 1917 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      19 | 1918 | `			SyToken *pFnKw = pScan;` |
|      18 | 1919 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 | 1920 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 | 1921 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 | 1922 | `				pFnKw = &pScan[1];` |
|     ! 0 | 1923 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 | 1924 | `			}` |
|      19 | 1925 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - | 1926 | `				SyToken *pInnerSigStart;` |
|       - | 1927 | `				SyToken *pInnerSigEnd;` |
|       - | 1928 | `				SyToken *pInnerBodyEnd;` |
|       - | 1929 | `				SyString *aInnerShadow;` |
|       - | 1930 | `				sxu32 nInnerShadow;` |
|       - | 1931 | `				sxu32 nInnerParamMax;` |
|       - | 1932 | `				SyToken *p;` |
|       - | 1933 | `				int iNestInner;` |
|      19 | 1934 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 | 1935 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 1936 | `					pScan++;` |
|     ! 0 | 1937 | `				}` |
|      19 | 1938 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 1939 | `					pScan++;` |
|     ! 0 | 1940 | `					continue;` |
|       - | 1941 | `				}` |
|      19 | 1942 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 | 1943 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - | 1944 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 | 1945 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 | 1946 | `					pScan = pEnd;` |
|     ! 0 | 1947 | `					continue;` |
|       - | 1948 | `				}` |
|       - | 1949 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 | 1950 | `				nInnerParamMax = 0;` |
|      57 | 1951 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 | 1952 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 | 1953 | `						nInnerParamMax++;` |
|       6 | 1954 | `					}` |
|      20 | 1955 | `				}` |
|      19 | 1956 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 | 1957 | `					&pGen->pVm->sAllocator,` |
|      18 | 1958 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 | 1959 | `				if( aInnerShadow == 0 ){` |
|     ! 0 | 1960 | `					return SXERR_ABORT;` |
|       - | 1961 | `				}` |
|      19 | 1962 | `				nInnerShadow = 0;` |
|      25 | 1963 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 | 1964 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 | 1965 | `				}` |
|      57 | 1966 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 | 1967 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 | 1968 | `						continue;` |
|       - | 1969 | `					}` |
|      13 | 1970 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 | 1971 | `						break;` |
|       - | 1972 | `					}` |
|      13 | 1973 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 1974 | `						continue;` |
|       - | 1975 | `					}` |
|      13 | 1976 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 | 1977 | `				}` |
|      19 | 1978 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 | 1979 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 | 1980 | `					pScan++;` |
|     ! 0 | 1981 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 | 1982 | `						&& pScan->sData.nByte == 1` |
|     ! 0 | 1983 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 | 1984 | `						pScan++;` |
|     ! 0 | 1985 | `					}` |
|     ! 0 | 1986 | `					if( pScan < pEnd` |
|     ! 0 | 1987 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 | 1988 | `						pScan++;` |
|     ! 0 | 1989 | `					}` |
|     ! 0 | 1990 | `				}` |
|      19 | 1991 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 | 1992 | `					pScan++; /* past '=>' */` |
|       9 | 1993 | `				}` |
|      19 | 1994 | `				pInnerBodyEnd = pScan;` |
|      19 | 1995 | `				iNestInner = 0;` |
|     131 | 1996 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 | 1997 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - | 1998 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - | 1999 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 | 2000 | `						break;` |
|       - | 2001 | `					}` |
|     113 | 2002 | `					if( pInnerBodyEnd->nType &` |
|       - | 2003 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 | 2004 | `						iNestInner++;` |
|     112 | 2005 | `					}else if( pInnerBodyEnd->nType &` |
|       - | 2006 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 | 2007 | `						iNestInner--;` |
|       1 | 2008 | `					}` |
|     113 | 2009 | `					pInnerBodyEnd++;` |
|       1 | 2010 | `				}` |
|       - | 2011 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - | 2012 | `				 * the outer's body: a default value is evaluated at call time` |
|       - | 2013 | `				 * in the outer frame, so any free variable it references is` |
|       - | 2014 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - | 2015 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - | 2016 | `				 * or those names leak into the outer's closure environment.` |
|       - | 2017 | `				 *` |
|       - | 2018 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - | 2019 | `				 * top-level commas, and for each argument scan only the token` |
|       - | 2020 | `				 * range after the '=' sign. */` |
|       - | 2021 | `				{` |
|      19 | 2022 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 | 2023 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 | 2024 | `						SyToken *pArgEnd = pArgStart;` |
|      13 | 2025 | `						SyToken *pEq = 0;` |
|      13 | 2026 | `						int iNestArg = 0;` |
|      49 | 2027 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 | 2028 | `							if( iNestArg == 0` |
|      39 | 2029 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 | 2030 | `								break;` |
|       - | 2031 | `							}` |
|      37 | 2032 | `							if( pArgEnd->nType &` |
|       - | 2033 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 2034 | `								iNestArg++;` |
|      37 | 2035 | `							}else if( pArgEnd->nType &` |
|       - | 2036 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 2037 | `								iNestArg--;` |
|     ! 0 | 2038 | `							}` |
|      36 | 2039 | `							if( pEq == 0 && iNestArg == 0` |
|      31 | 2040 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 | 2041 | `								pEq = pArgEnd;` |
|       3 | 2042 | `							}` |
|      37 | 2043 | `							pArgEnd++;` |
|       1 | 2044 | `						}` |
|      13 | 2045 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 | 2046 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 | 2047 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 | 2048 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 2049 | `								return SXERR_ABORT;` |
|       - | 2050 | `							}` |
|       3 | 2051 | `						}` |
|      13 | 2052 | `						pArgStart = pArgEnd;` |
|      12 | 2053 | `						if( pArgStart < pInnerSigEnd` |
|       8 | 2054 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 | 2055 | `							pArgStart++;` |
|       1 | 2056 | `						}` |
|       1 | 2057 | `					}` |
|       - | 2058 | `				}` |
|      28 | 2059 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 | 2060 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 | 2061 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2062 | `					return SXERR_ABORT;` |
|       - | 2063 | `				}` |
|      19 | 2064 | `				pScan = pInnerBodyEnd;` |
|      19 | 2065 | `				continue;` |
|       - | 2066 | `			}` |
|     ! 0 | 2067 | `		}` |
|     237 | 2068 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     129 | 2069 | `			pScan++;` |
|     129 | 2070 | `			continue;` |
|       - | 2071 | `		}` |
|       - | 2072 | `		{` |
|       - | 2073 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     109 | 2074 | `			SyToken *pDollar = pScan;` |
|     162 | 2075 | `			while( &pDollar[1] < pEnd` |
|     109 | 2076 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 | 2077 | `				pDollar++;` |
|     ! 0 | 2078 | `			}` |
|     109 | 2079 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 | 2080 | `				break;` |
|       - | 2081 | `			}` |
|     109 | 2082 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 2083 | `				pScan = pDollar + 1;` |
|     ! 0 | 2084 | `				continue;` |
|       - | 2085 | `			}` |
|     163 | 2086 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     108 | 2087 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      54 | 2088 | `				aShadow,nShadow);` |
|     109 | 2089 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2090 | `				return SXERR_ABORT;` |
|       - | 2091 | `			}` |
|     109 | 2092 | `			pScan = pDollar + 2;` |
|       - | 2093 | `		}` |
|       1 | 2094 | `	}` |
|     103 | 2095 | `	return SXRET_OK;` |
|      52 | 2096 |  |
|       - | 2097 | `/*` |
|       - | 2098 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - | 2099 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - | 2100 | ` * variables by value. The body is a single expression that acts as an` |
|       - | 2101 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - | 2102 | ` * $this is also made available.` |
|       - | 2103 | ` */` |
|      84 | 2104 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2105 |  |
|       - | 2106 | `	ph7_vm_func *pFunc;` |
|       - | 2107 | `	ph7_vm_func_closure_env sEnv;` |
|       - | 2108 | `	GenBlock *pBlock;` |
|       - | 2109 | `	SySet *pInstrContainer;` |
|       - | 2110 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - | 2111 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - | 2112 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - | 2113 | `	SyToken *pSavedEnd;` |
|       - | 2114 | `	ph7_vm_func_arg *aArgs;` |
|       - | 2115 | `	char zName[512];` |
|       - | 2116 | `	static int iCnt = 1;` |
|       - | 2117 | `	char *zDup;` |
|       - | 2118 | `	sxu32 nLen;` |
|       - | 2119 | `	sxu32 nLine;` |
|      86 | 2120 | `	sxi32 iFlags = 0;` |
|      86 | 2121 | `	int bStatic = 0;` |
|       - | 2122 | `	sxi32 rc;` |
|       - | 2123 | `	sxu32 n;` |
|      42 | 2124 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 2125 |  |
|      86 | 2126 | `	nLine = pGen->pIn->nLine;` |
|       - | 2127 | `	/* Optional 'static' prefix */` |
|      84 | 2128 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      86 | 2129 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 | 2130 | `		bStatic = 1;` |
|       3 | 2131 | `		pGen->pIn++;` |
|       1 | 2132 | `	}` |
|       - | 2133 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      84 | 2134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      86 | 2135 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 | 2136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2137 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 | 2138 | `		return SXERR_SYNTAX;` |
|       - | 2139 | `	}` |
|      86 | 2140 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - | 2141 | `	/* Optional '&' — return by reference */` |
|      86 | 2142 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 | 2143 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 | 2144 | `		pGen->pIn++;` |
|     ! 0 | 2145 | `	}` |
|       - | 2146 | `	/* Expect '(' */` |
|      86 | 2147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 2148 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 | 2149 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 2150 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 | 2151 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 | 2152 | `		}else{` |
|     ! 0 | 2153 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2154 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - | 2155 | `		}` |
|       3 | 2156 | `		return SXERR_SYNTAX;` |
|       - | 2157 | `	}` |
|      84 | 2158 | `	pGen->pIn++; /* Jump '(' */` |
|       - | 2159 | `	/* Delimit the parameter list */` |
|      84 | 2160 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      84 | 2161 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 | 2162 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2163 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 | 2164 | `		return SXERR_SYNTAX;` |
|       - | 2165 | `	}` |
|       - | 2166 | `	/* Allocate the function state */` |
|      82 | 2167 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      82 | 2168 | `	if( pFunc == 0 ){` |
|     ! 0 | 2169 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2170 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2171 | `		return SXERR_ABORT;` |
|       - | 2172 | `	}` |
|       - | 2173 | `	/* Generate a unique lambda name */` |
|      82 | 2174 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     166 | 2175 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 | 2176 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 | 2177 | `	}` |
|      82 | 2178 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      82 | 2179 | `	if( zDup == 0 ){` |
|     ! 0 | 2180 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2181 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2182 | `		return SXERR_ABORT;` |
|       - | 2183 | `	}` |
|      82 | 2184 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - | 2185 | `	/* Collect function arguments */` |
|      82 | 2186 | `	if( pGen->pIn < pSigEnd ){` |
|      52 | 2187 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd);` |
|      52 | 2188 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2189 | `			return SXERR_ABORT;` |
|       - | 2190 | `		}` |
|      25 | 2191 | `	}` |
|       - | 2192 | `	/* Point past ')' and parse optional return type */` |
|      82 | 2193 | `	pGen->pIn = &pSigEnd[1];` |
|      82 | 2194 | `	GenStateParseReturnType(pGen,pFunc);` |
|       - | 2195 | `	/* Expect '=>' */` |
|      82 | 2196 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 | 2197 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 | 2198 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 2199 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 | 2200 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 | 2201 | `		}else{` |
|     ! 0 | 2202 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 2203 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - | 2204 | `		}` |
|       3 | 2205 | `		return SXERR_SYNTAX;` |
|       - | 2206 | `	}` |
|      79 | 2207 | `	pGen->pIn++; /* Jump '=>' */` |
|      79 | 2208 | `	pBodyStart = pGen->pIn;` |
|      79 | 2209 | `	pBodyEnd = pGen->pEnd;` |
|       - | 2210 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - | 2211 | `	 * recursively collect free-variable references from the body. The scan` |
|       - | 2212 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - | 2213 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      79 | 2214 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - | 2215 | `	{` |
|      79 | 2216 | `		SyString *aShadow = 0;` |
|      79 | 2217 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      79 | 2218 | `		if( nShadow > 0 ){` |
|      49 | 2219 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      48 | 2220 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      49 | 2221 | `			if( aShadow == 0 ){` |
|     ! 0 | 2222 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2223 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2224 | `				return SXERR_ABORT;` |
|       - | 2225 | `			}` |
|     103 | 2226 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      55 | 2227 | `				aShadow[n] = aArgs[n].sName;` |
|      28 | 2228 | `			}` |
|      24 | 2229 | `		}` |
|     118 | 2230 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      39 | 2231 | `			aShadow,nShadow);` |
|      79 | 2232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2233 | `			return SXERR_ABORT;` |
|       - | 2234 | `		}` |
|       - | 2235 | `	}` |
|       - | 2236 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - | 2237 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - | 2238 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - | 2239 | `	 * $this. */` |
|      79 | 2240 | `	if( !bStatic ){` |
|       - | 2241 | `		char *zThisDup;` |
|      77 | 2242 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      77 | 2243 | `		if( zThisDup == 0 ){` |
|     ! 0 | 2244 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2245 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2246 | `			return SXERR_ABORT;` |
|       - | 2247 | `		}` |
|      77 | 2248 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      77 | 2249 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      77 | 2250 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      77 | 2251 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      77 | 2252 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      38 | 2253 | `	}` |
|       - | 2254 | `	/* Arrow functions are always closures */` |
|      79 | 2255 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - | 2256 | `	/* Compile the body expression as an implicit return */` |
|     118 | 2257 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      39 | 2258 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      79 | 2259 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2260 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 2261 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 | 2262 | `		return SXERR_ABORT;` |
|       - | 2263 | `	}` |
|      79 | 2264 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      79 | 2265 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      79 | 2266 | `	pSavedEnd = pGen->pEnd;` |
|      79 | 2267 | `	pGen->pIn = pBodyStart;` |
|      79 | 2268 | `	pGen->pEnd = pBodyEnd;` |
|      79 | 2269 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      79 | 2270 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2271 | `		return SXERR_ABORT;` |
|       - | 2272 | `	}` |
|       - | 2273 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack' */` |
|      79 | 2274 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      79 | 2275 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      79 | 2276 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 2277 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      79 | 2278 | `	pGen->pIn = pBodyEnd;` |
|      79 | 2279 | `	pGen->pEnd = pSavedEnd;` |
|       - | 2280 | `	/* Emit the load-closure instruction */` |
|      79 | 2281 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      79 | 2282 | `	return SXRET_OK;` |
|      44 | 2283 |  |
|       - | 2284 | `/*` |
|       - | 2285 | ` * Compile a backtick quoted string.` |
|       - | 2286 | ` */` |
|       4 | 2287 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 2288 |  |
|       - | 2289 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 2290 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 2291 | `	 */` |
|       7 | 2292 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 2293 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 2294 | `		ph7_lib_version()` |
|       - | 2295 | `		);` |
|       - | 2296 | `	/* Load NULL */` |
|       5 | 2297 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 2298 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 2299 | `	/* Node successfully compiled */` |
|       5 | 2300 | `	return SXRET_OK;` |
|       1 | 2301 |  |
|       - | 2302 | `/*` |
|       - | 2303 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 2304 | ` * construct.` |
|       - | 2305 | ` */` |
|      72 | 2306 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2307 |  |
|       - | 2308 | `	SyString *pName;` |
|       - | 2309 | `	sxu32 nKeyID;` |
|       - | 2310 | `	sxi32 rc;` |
|       - | 2311 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 2312 | `	pName = &pGen->pIn->sData;` |
|      74 | 2313 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 2314 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 2315 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 2316 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 2317 | `		/* Compile arguments one after one */` |
|       9 | 2318 | `		pTmp = pGen->pEnd;` |
|       - | 2319 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 2320 | `		 * 'echo' can be used in the context of a function which` |
|       - | 2321 | `		 *  mean that the following expression is valid:` |
|       - | 2322 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 2323 | `		 */` |
|       9 | 2324 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 2325 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 2326 | `			if( pGen->pIn < pNext ){` |
|       9 | 2327 | `				pGen->pEnd = pNext;` |
|       9 | 2328 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 2329 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 2330 | `					return SXERR_ABORT;` |
|       - | 2331 | `				}` |
|       9 | 2332 | `				if( rc != SXERR_EMPTY ){` |
|       - | 2333 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 2334 | `					 * without the overhead of a function call.` |
|       - | 2335 | `					 * This is a very powerful optimization that improve` |
|       - | 2336 | `					 * performance greatly.` |
|       - | 2337 | `					 */` |
|       9 | 2338 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 2339 | `				}` |
|       4 | 2340 | `			}` |
|       - | 2341 | `			/* Jump trailing commas */` |
|       9 | 2342 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 2343 | `				pNext++;` |
|     ! 0 | 2344 | `			}` |
|       9 | 2345 | `			pGen->pIn = pNext;` |
|       1 | 2346 | `		}` |
|       - | 2347 | `		/* Restore token stream */` |
|       9 | 2348 | `		pGen->pEnd = pTmp;` |
|       5 | 2349 | `	}else{` |
|      66 | 2350 | `		sxi32 nArg = 0;` |
|      66 | 2351 | `		sxu32 nIdx = 0;` |
|      66 | 2352 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 2353 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2354 | `			return SXERR_ABORT;` |
|      66 | 2355 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 2356 | `			nArg = 1;` |
|      32 | 2357 | `		}` |
|      66 | 2358 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 2359 | `			ph7_value *pObj;` |
|       - | 2360 | `			/* Emit the call instruction */` |
|      20 | 2361 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 2362 | `			if( pObj == 0 ){` |
|     ! 0 | 2363 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2364 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 2365 | `				return SXERR_ABORT;` |
|       - | 2366 | `			}` |
|      20 | 2367 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 2368 | `			/* Install in the literal table */` |
|      20 | 2369 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 2370 | `		}` |
|       - | 2371 | `		/* Emit the call instruction */` |
|      66 | 2372 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 2373 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 2374 | `	}` |
|       - | 2375 | `	/* Node successfully compiled */` |
|      74 | 2376 | `	return SXRET_OK;` |
|      38 | 2377 |  |
|       - | 2378 | `/*` |
|       - | 2379 | ` * Compile a node holding a variable declaration.` |
|       - | 2380 | ` * According to the PHP language reference` |
|       - | 2381 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 2382 | ` *  The variable name is case-sensitive.` |
|       - | 2383 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 2384 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2385 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 2386 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 2387 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 2388 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 2389 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 2390 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 2391 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 2392 | ` *  the chapter on Expressions.` |
|       - | 2393 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 2394 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 2395 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 2396 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 2397 | ` *  is being assigned (the source variable).` |
|       - | 2398 | ` */` |
|  803384 | 2399 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2400 |  |
|  803386 | 2401 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2402 | `	sxi32 iVv;` |
|       - | 2403 | `	sxi32 iP1;` |
|       - | 2404 | `	void *p3;` |
|       - | 2405 | `	sxi32 rc;` |
|  803386 | 2406 | `	iVv = -1; /* Variable variable counter */` |
| 1606782 | 2407 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  803398 | 2408 | `		pGen->pIn++;` |
|  803398 | 2409 | `		iVv++;` |
|       2 | 2410 | `	}` |
|  803386 | 2411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 2412 | `		/* Invalid variable name */` |
|     ! 0 | 2413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 2414 | `		if( rc == SXERR_ABORT ){` |
|       - | 2415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2416 | `			return SXERR_ABORT;` |
|       - | 2417 | `		}` |
|     ! 0 | 2418 | `		return SXRET_OK;` |
|       - | 2419 | `	}` |
|  803386 | 2420 | `	p3  = 0;` |
|  803386 | 2421 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 2422 | `		/* Dynamic variable creation */` |
|      18 | 2423 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 2424 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 2425 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2426 | `			/* Empty expression */` |
|       3 | 2427 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 2428 | `			return SXRET_OK;` |
|       - | 2429 | `		}` |
|       - | 2430 | `		/* Compile the expression holding the variable name */` |
|      16 | 2431 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 2432 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2433 | `			return SXERR_ABORT;` |
|      16 | 2434 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 2435 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 2436 | `			return SXRET_OK;` |
|       - | 2437 | `		}` |
|       7 | 2438 | `	}else{` |
|       - | 2439 | `		SyHashEntry *pEntry;` |
|       - | 2440 | `		SyString *pName;` |
|  803370 | 2441 | `		char *zName = 0;` |
|       - | 2442 | `		/* Extract variable name */` |
|  803370 | 2443 | `		pName = &pGen->pIn->sData;` |
|       - | 2444 | `		/* Advance the stream cursor */` |
|  803370 | 2445 | `		pGen->pIn++;` |
|  803370 | 2446 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  803370 | 2447 | `		if( pEntry == 0 ){` |
|       - | 2448 | `			/* Duplicate name */` |
|  115478 | 2449 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  115478 | 2450 | `			if( zName == 0 ){` |
|     ! 0 | 2451 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2452 | `				return SXERR_ABORT;` |
|       - | 2453 | `			}` |
|       - | 2454 | `			/* Install in the hashtable */` |
|  115478 | 2455 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   57740 | 2456 | `		}else{` |
|       - | 2457 | `			/* Name already available */` |
|  687894 | 2458 | `			zName = (char *)pEntry->pUserData;` |
|       - | 2459 | `		}` |
|  803370 | 2460 | `		p3 = (void *)zName;` |
|       - | 2461 | `	}` |
|  803382 | 2462 | `	iP1 = 0;` |
|  803382 | 2463 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  308728 | 2464 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 2465 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  302458 | 2466 | `			iP1 = 1;` |
|  151228 | 2467 | `		}` |
|  154363 | 2468 | `	}` |
|       - | 2469 | `	/* Emit the load instruction */` |
|  803382 | 2470 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  803394 | 2471 | `	while( iVv > 0 ){` |
|      13 | 2472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 2473 | `		iVv--;` |
|       1 | 2474 | `	}` |
|       - | 2475 | `	/* Node successfully compiled */` |
|  803382 | 2476 | `	return SXRET_OK;` |
|  401694 | 2477 |  |
|       - | 2478 | `/*` |
|       - | 2479 | ` * Load a literal.` |
|       - | 2480 | ` */` |
|  538708 | 2481 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 2482 |  |
|  538710 | 2483 | `	SyToken *pToken = pGen->pIn;` |
|       - | 2484 | `	ph7_value *pObj;` |
|       - | 2485 | `	SyString *pStr;` |
|       - | 2486 | `	sxu32 nIdx;` |
|       - | 2487 | `	/* Extract token value */` |
|  538710 | 2488 | `	pStr = &pToken->sData;` |
|       - | 2489 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  538710 | 2490 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   97880 | 2491 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 2492 | `			/* NULL constant are always indexed at 0 */` |
|   41626 | 2493 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   41626 | 2494 | `			return SXRET_OK;` |
|   56256 | 2495 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 2496 | `			/* TRUE constant are always indexed at 1 */` |
|     494 | 2497 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     494 | 2498 | `			return SXRET_OK;` |
|       2 | 2499 | `		}` |
|  511197 | 2500 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   84968 | 2501 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 2502 | `			/* FALSE constant are always indexed at 2 */` |
|   36292 | 2503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   36292 | 2504 | `			return SXRET_OK;` |
|  442088 | 2505 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   75092 | 2506 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 2507 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5500 | 2508 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5500 | 2509 | `			if( pObj == 0 ){` |
|     ! 0 | 2510 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2511 | `				return SXERR_ABORT;` |
|       - | 2512 | `			}` |
|    5500 | 2513 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 2514 | `			/* Emit the load constant instruction */` |
|    5500 | 2515 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5500 | 2516 | `			return SXRET_OK;` |
|  412902 | 2517 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   27716 | 2518 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 2519 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 2520 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 2521 | `			if( pObj == 0 ){` |
|     ! 0 | 2522 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2523 | `				return SXERR_ABORT;` |
|       - | 2524 | `			}` |
|       7 | 2525 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 2526 | `				SyString sNs;` |
|       7 | 2527 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 2528 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 2529 | `			}else{` |
|     ! 0 | 2530 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 2531 | `			}` |
|       7 | 2532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 2533 | `			return SXRET_OK;` |
|  412068 | 2534 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11590 | 2535 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  406267 | 2536 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14476 | 2537 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 2538 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 2539 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 2540 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 2541 | `				/* Point to the upper block */` |
|      11 | 2542 | `				pBlock = pBlock->pParent;` |
|       1 | 2543 | `			}` |
|      11 | 2544 | `			if( pBlock == 0 ){` |
|       - | 2545 | `				/* Called in the global scope,load NULL */` |
|       5 | 2546 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 2547 | `			}else{` |
|       - | 2548 | `				/* Extract the target function/method */` |
|       7 | 2549 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 2550 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 2551 | `					/* Not a class method,Load null */` |
|       3 | 2552 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 2553 | `				}else{` |
|       5 | 2554 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 2555 | `					if( pObj == 0 ){` |
|     ! 0 | 2556 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2557 | `						return SXERR_ABORT;` |
|       - | 2558 | `					}` |
|       5 | 2559 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 2560 | `					/* Emit the load constant instruction */` |
|       5 | 2561 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 2562 | `				}` |
|       - | 2563 | `			}` |
|      11 | 2564 | `			return SXRET_OK;` |
|       - | 2565 | `	}` |
|       - | 2566 | `	/* Query literal table */` |
|  454790 | 2567 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 2568 | `		ph7_value *pLitObj;` |
|       - | 2569 | `		/* Unknown literal,install it in the literal table */` |
|  212812 | 2570 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  212812 | 2571 | `		if( pLitObj == 0 ){` |
|     ! 0 | 2572 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2573 | `			return SXERR_ABORT;` |
|       - | 2574 | `		}` |
|  212812 | 2575 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  212812 | 2576 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  106405 | 2577 | `	}` |
|       - | 2578 | `	/* Emit the load constant instruction */` |
|  454790 | 2579 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  454790 | 2580 | `	return SXRET_OK;` |
|  269356 | 2581 |  |
|       - | 2582 | `/*` |
|       - | 2583 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 2584 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 2585 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 2586 | ` * Otherwise, load the simple literal directly.` |
|       - | 2587 | ` */` |
|  538734 | 2588 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 2589 |  |
|       - | 2590 | `	sxi32 rc;` |
|  538736 | 2591 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2592 | `		return SXRET_OK;` |
|       - | 2593 | `	}` |
|       - | 2594 | `	/* Check if this is a multi-token namespace path */` |
|  538736 | 2595 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 2596 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      28 | 2597 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      28 | 2598 | `		int isAbsolute = 0;` |
|      28 | 2599 | `		SyBlobReset(pWorker);` |
|       - | 2600 | `		/* Check for leading backslash (absolute path) */` |
|      28 | 2601 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      26 | 2602 | `			isAbsolute = 1;` |
|      26 | 2603 | `			pGen->pIn++; /* Skip leading backslash */` |
|      12 | 2604 | `		}` |
|       - | 2605 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      28 | 2606 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 2607 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 2608 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 2609 | `		}` |
|       - | 2610 | `		/* Collect all path components */` |
|     108 | 2611 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     108 | 2612 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 | 2613 | `				SyBlobAppend(pWorker,"\\",1);` |
|      22 | 2614 | `			}else{` |
|      68 | 2615 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2616 | `			}` |
|     108 | 2617 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      28 | 2618 | `				pGen->pIn++;` |
|      28 | 2619 | `				break;` |
|       - | 2620 | `			}` |
|      82 | 2621 | `			pGen->pIn++;` |
|       2 | 2622 | `		}` |
|      28 | 2623 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 2624 | `			ph7_value *pObj;` |
|       - | 2625 | `			SyString sPath;` |
|       - | 2626 | `			sxu32 nIdx;` |
|      28 | 2627 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 2628 | `			/* Install in the literal table */` |
|      28 | 2629 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      16 | 2630 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      16 | 2631 | `				if( pObj == 0 ){` |
|     ! 0 | 2632 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2633 | `					return SXERR_ABORT;` |
|       - | 2634 | `				}` |
|      16 | 2635 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      16 | 2636 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       7 | 2637 | `			}` |
|       - | 2638 | `			/* Emit the load constant instruction.` |
|       - | 2639 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      28 | 2640 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      28 | 2641 | `			return SXRET_OK;` |
|       - | 2642 | `		}` |
|     ! 0 | 2643 | `	}` |
|       - | 2644 | `	/* Single-token literal: load directly */` |
|  538710 | 2645 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  538710 | 2646 | `	return rc;` |
|  269369 | 2647 |  |
|       - | 2648 | `/*` |
|       - | 2649 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 2650 | ` */` |
|  538734 | 2651 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2652 |  |
|       - | 2653 | `	sxi32 rc;` |
|  538736 | 2654 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  538736 | 2655 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2656 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 2657 | `		return rc;` |
|       - | 2658 | `	}` |
|       - | 2659 | `	/* Node successfully compiled */` |
|  538736 | 2660 | `	return SXRET_OK;` |
|  269369 | 2661 |  |
|       - | 2662 | `/*` |
|       - | 2663 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 2664 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 2665 | ` */` |
|       8 | 2666 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 2667 |  |
|       - | 2668 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 2669 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 2670 | `		pGen->pIn++;` |
|       1 | 2671 | `	}` |
|       9 | 2672 | `	return SXRET_OK;` |
|       1 | 2673 |  |
|       - | 2674 | `/*` |
|       - | 2675 | ` * Check if the given identifier name is reserved or not.` |
|       - | 2676 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 2677 | ` */` |
|      56 | 2678 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 2679 |  |
|      58 | 2680 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 2681 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 2682 | `			return TRUE;` |
|      24 | 2683 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 2684 | `			return TRUE;` |
|       2 | 2685 | `		}` |
|      43 | 2686 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 2687 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 2688 | `			return TRUE;` |
|       - | 2689 | `		}` |
|     ! 0 | 2690 | `	}` |
|       - | 2691 | `	/* Not a reserved constant */` |
|      50 | 2692 | `	return FALSE;` |
|      30 | 2693 |  |
|       - | 2694 | `/*` |
|       - | 2695 | ` * Compile the 'const' statement.` |
|       - | 2696 | ` * According to the PHP language reference` |
|       - | 2697 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 2698 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 2699 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 2700 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 2701 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2702 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 2703 | ` *  Syntax` |
|       - | 2704 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 2705 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 2706 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 2707 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 2708 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 2709 | ` *  to get a list of all defined constants.` |
|       - | 2710 | ` *` |
|       - | 2711 | ` * Symisc eXtension.` |
|       - | 2712 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 2713 | ` *  would allow only simple scalar value.` |
|       - | 2714 | ` *  Example` |
|       - | 2715 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2716 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2717 | ` */` |
|      32 | 2718 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 2719 |  |
|       - | 2720 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 2721 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2722 | `	SyString *pName;` |
|       - | 2723 | `	sxi32 rc;` |
|      34 | 2724 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 2725 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2726 | `		/* Invalid constant name */` |
|       7 | 2727 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 2728 | `		if( rc == SXERR_ABORT ){` |
|       - | 2729 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2730 | `			return SXERR_ABORT;` |
|       - | 2731 | `		}` |
|       7 | 2732 | `		goto Synchronize;` |
|       - | 2733 | `	}` |
|       - | 2734 | `	/* Peek constant name */` |
|      28 | 2735 | `	pName = &pGen->pIn->sData;` |
|       - | 2736 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 2737 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 2738 | `		/* Reserved constant */` |
|       9 | 2739 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 2740 | `		if( rc == SXERR_ABORT ){` |
|       - | 2741 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2742 | `			return SXERR_ABORT;` |
|       - | 2743 | `		}` |
|       9 | 2744 | `		goto Synchronize;` |
|       - | 2745 | `	}` |
|      20 | 2746 | `	pGen->pIn++;` |
|      20 | 2747 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 2748 | `		/* Invalid statement*/` |
|       5 | 2749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 2750 | `		if( rc == SXERR_ABORT ){` |
|       - | 2751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2752 | `			return SXERR_ABORT;` |
|       - | 2753 | `		}` |
|       5 | 2754 | `		goto Synchronize;` |
|       - | 2755 | `	}` |
|      15 | 2756 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 2757 | `	/* Allocate a new constant value container */` |
|      15 | 2758 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 2759 | `	if( pConsCode == 0 ){` |
|     ! 0 | 2760 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2761 | `		return SXERR_ABORT;` |
|       - | 2762 | `	}` |
|      15 | 2763 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 2764 | `	/* Swap bytecode container */` |
|      15 | 2765 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 2766 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 2767 | `	/* Compile constant value */` |
|      15 | 2768 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2769 | `	/* Emit the done instruction */` |
|      15 | 2770 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 2771 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 2772 | `	if( rc == SXERR_ABORT ){` |
|       - | 2773 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2774 | `		return SXERR_ABORT;` |
|       - | 2775 | `	}` |
|      15 | 2776 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 2777 | `	/* Register the constant with namespace-qualified name */` |
|       - | 2778 | `	{` |
|       - | 2779 | `		SyBlob sFQN;` |
|       - | 2780 | `		SyString sFQNStr;` |
|      15 | 2781 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 2782 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 2783 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 2784 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 2785 | `		SyBlobRelease(&sFQN);` |
|       - | 2786 | `	}` |
|      15 | 2787 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2788 | `		SySetRelease(pConsCode);` |
|     ! 0 | 2789 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 2790 | `	}` |
|      15 | 2791 | `	return SXRET_OK;` |
|       9 | 2792 | `Synchronize:` |
|       - | 2793 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 2794 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 2795 | `		pGen->pIn++;` |
|       1 | 2796 | `	}` |
|      19 | 2797 | `	return SXRET_OK;` |
|      18 | 2798 |  |
|       - | 2799 | `/*` |
|       - | 2800 | ` * Compile the 'continue' statement.` |
|       - | 2801 | ` * According to the PHP language reference` |
|       - | 2802 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 2803 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 2804 | ` *  iteration.` |
|       - | 2805 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 2806 | ` *  the purposes of continue.` |
|       - | 2807 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 2808 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 2809 | ` *  Note:` |
|       - | 2810 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 2811 | ` */` |
|       - | 2812 | `/*` |
|       - | 2813 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 2814 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 2815 | ` * break/continue crosses a try boundary.` |
|       - | 2816 | ` *` |
|       - | 2817 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 2818 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 2819 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 2820 | ` */` |
|    2876 | 2821 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 2822 |  |
|    2878 | 2823 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16802 | 2824 | `	while( pBlock && pBlock != pTarget ){` |
|   13926 | 2825 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 2826 | `			if( pBlock->pUserData ){` |
|       - | 2827 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 2828 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 2829 | `			}else{` |
|       - | 2830 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 2831 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 2832 | `				 * exception context from a sub-execution.` |
|       - | 2833 | `				 */` |
|     ! 0 | 2834 | `				break;` |
|       - | 2835 | `			}` |
|       1 | 2836 | `		}` |
|   13926 | 2837 | `		pBlock = pBlock->pParent;` |
|       2 | 2838 | `	}` |
|    2878 | 2839 |  |
|    2792 | 2840 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2841 |  |
|       - | 2842 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2843 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2844 | `	sxu32 nLineLocal;` |
|       - | 2845 | `	sxi32 rc;` |
|    2794 | 2846 | `	nLineLocal = pGen->pIn->nLine;` |
|    2794 | 2847 | `	iLevel = 0;` |
|       - | 2848 | `	/* Jump the 'continue' keyword */` |
|    2794 | 2849 | `	pGen->pIn++;` |
|    2794 | 2850 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2851 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2852 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2853 | `		 */` |
|       - | 2854 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2855 | `		char *zAlloc = 0;` |
|       - | 2856 | `		SyString sNum;` |
|      16 | 2857 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2858 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2859 | `			return SXERR_ABORT;` |
|       - | 2860 | `		}` |
|      16 | 2861 | `		if( rc == SXRET_OK ){` |
|      20 | 2862 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2863 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2864 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2865 | `				return SXERR_ABORT;` |
|       - | 2866 | `			}` |
|      14 | 2867 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2868 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2869 | `		}` |
|      16 | 2870 | `		if( iLevel < 2 ){` |
|       3 | 2871 | `			iLevel = 0;` |
|       1 | 2872 | `		}` |
|      16 | 2873 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2874 | `	}` |
|       - | 2875 | `	/* Point to the target loop */` |
|    2794 | 2876 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2794 | 2877 | `	if( pLoop == 0 ){` |
|       - | 2878 | `		/* Illegal continue */` |
|      11 | 2879 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2880 | `		if( rc == SXERR_ABORT ){` |
|       - | 2881 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2882 | `			return SXERR_ABORT;` |
|       - | 2883 | `		}` |
|       6 | 2884 | `	}else{` |
|    2784 | 2885 | `		sxu32 nInstrIdx = 0;` |
|       - | 2886 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2784 | 2887 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2784 | 2888 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2889 | `			/* According to the PHP language reference manual` |
|       - | 2890 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2891 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2892 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2893 | `			 */` |
|       5 | 2894 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2895 | `			if( rc == SXRET_OK ){` |
|       5 | 2896 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2897 | `			}` |
|       3 | 2898 | `		}else{` |
|       - | 2899 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2780 | 2900 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2780 | 2901 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2902 | `				JumpFixup sJumpFix;` |
|       - | 2903 | `				/* Post-continue */` |
|      14 | 2904 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 | 2905 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 | 2906 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 | 2907 | `			}` |
|       - | 2908 | `		}` |
|       - | 2909 | `	}` |
|    2794 | 2910 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2911 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2912 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2913 | `	}` |
|       - | 2914 | `	/* Statement successfully compiled */` |
|    2794 | 2915 | `	return SXRET_OK;` |
|    1398 | 2916 |  |
|       - | 2917 | `/*` |
|       - | 2918 | ` * Compile the 'break' statement.` |
|       - | 2919 | ` * According to the PHP language reference` |
|       - | 2920 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2921 | ` *  structure.` |
|       - | 2922 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2923 | ` *  enclosing structures are to be broken out of.` |
|       - | 2924 | ` */` |
|     110 | 2925 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2926 |  |
|       - | 2927 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2928 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2929 | `	sxi32 rc;` |
|     112 | 2930 | `	iLevel = 0;` |
|       - | 2931 | `	/* Jump the 'break' keyword */` |
|     112 | 2932 | `	pGen->pIn++;` |
|     112 | 2933 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2934 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2935 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2936 | `		 */` |
|       - | 2937 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2938 | `		char *zAlloc = 0;` |
|       - | 2939 | `		SyString sNum;` |
|      16 | 2940 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2941 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2942 | `			return SXERR_ABORT;` |
|       - | 2943 | `		}` |
|      16 | 2944 | `		if( rc == SXRET_OK ){` |
|      20 | 2945 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2946 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2947 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2948 | `				return SXERR_ABORT;` |
|       - | 2949 | `			}` |
|      14 | 2950 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2951 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2952 | `		}` |
|      16 | 2953 | `		if( iLevel < 2 ){` |
|       3 | 2954 | `			iLevel = 0;` |
|       1 | 2955 | `		}` |
|      16 | 2956 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2957 | `	}` |
|       - | 2958 | `	/* Extract the target loop */` |
|     112 | 2959 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 | 2960 | `	if( pLoop == 0 ){` |
|       - | 2961 | `		/* Illegal break */` |
|      17 | 2962 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2963 | `		if( rc == SXERR_ABORT ){` |
|       - | 2964 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2965 | `			return SXERR_ABORT;` |
|       - | 2966 | `		}` |
|       9 | 2967 | `	}else{` |
|       - | 2968 | `		sxu32 nInstrIdx;` |
|       - | 2969 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 | 2970 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 | 2971 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 | 2972 | `		if( rc == SXRET_OK ){` |
|       - | 2973 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 | 2974 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 | 2975 | `		}` |
|       - | 2976 | `	}` |
|     112 | 2977 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2978 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2979 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2980 | `	}` |
|       - | 2981 | `	/* Statement successfully compiled */` |
|     112 | 2982 | `	return SXRET_OK;` |
|      57 | 2983 |  |
|       - | 2984 | `/*` |
|       - | 2985 | ` * Compile or record a label.` |
|       - | 2986 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2987 | ` * Example` |
|       - | 2988 | ` *  goto LABEL;` |
|       - | 2989 | ` *   echo 'Foo';` |
|       - | 2990 | ` *  LABEL:` |
|       - | 2991 | ` *   echo 'Bar';` |
|       - | 2992 | ` */` |
|     112 | 2993 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2994 |  |
|       - | 2995 | `	GenBlock *pBlock;` |
|       - | 2996 | `	Label sLabel;` |
|       - | 2997 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2998 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2999 | `	if( pBlock ){` |
|       - | 3000 | `		sxi32 rc;` |
|       7 | 3001 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 3002 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 3003 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3004 | `			return SXERR_ABORT;` |
|       - | 3005 | `		}` |
|       3 | 3006 | `	}else{` |
|     110 | 3007 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 3008 | `		char *zDup;` |
|       - | 3009 | `		/* Initialize label fields */` |
|     110 | 3010 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3011 | `		/* Duplicate label name */` |
|     110 | 3012 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 3013 | `		if( zDup == 0 ){` |
|     ! 0 | 3014 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3015 | `			return SXERR_ABORT;` |
|       - | 3016 | `		}` |
|     110 | 3017 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 3018 | `		sLabel.bRef  = FALSE;` |
|     110 | 3019 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 3020 | `		pBlock = pGen->pCurrent;` |
|     218 | 3021 | `		while( pBlock ){` |
|     130 | 3022 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 3023 | `				break;` |
|       - | 3024 | `			}` |
|       - | 3025 | `			/* Point to the upper block */` |
|     110 | 3026 | `			pBlock = pBlock->pParent;` |
|       2 | 3027 | `		}` |
|     110 | 3028 | `		if( pBlock ){` |
|      22 | 3029 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 3030 | `		}else{` |
|      90 | 3031 | `			sLabel.pFunc = 0;` |
|       - | 3032 | `		}` |
|       - | 3033 | `		/* Insert in label set */` |
|     110 | 3034 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 3035 | `	}` |
|     114 | 3036 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 3037 | `	return SXRET_OK;` |
|      58 | 3038 |  |
|       - | 3039 | `/*` |
|       - | 3040 | ` * Compile the so hated 'goto' statement.` |
|       - | 3041 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 3042 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 3043 | ` * a compiler it has to do this.` |
|       - | 3044 | ` * According to the PHP language reference manual` |
|       - | 3045 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 3046 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 3047 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 3048 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 3049 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 3050 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 3051 | ` *   of a multi-level break` |
|       - | 3052 | ` */` |
|     152 | 3053 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 3054 |  |
|       - | 3055 | `	JumpFixup sJump;` |
|       - | 3056 | `	sxi32 rc;` |
|     154 | 3057 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 3058 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3059 | `		/* Missing label */` |
|     ! 0 | 3060 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 3061 | `		if( rc == SXERR_ABORT ){` |
|       - | 3062 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3063 | `			return SXERR_ABORT;` |
|       - | 3064 | `		}` |
|     ! 0 | 3065 | `		return SXRET_OK;` |
|       - | 3066 | `	}` |
|     154 | 3067 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 3068 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 3069 | `		if( rc == SXERR_ABORT ){` |
|       - | 3070 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3071 | `			return SXERR_ABORT;` |
|       - | 3072 | `		}` |
|       3 | 3073 | `	}else{` |
|     150 | 3074 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 3075 | `		GenBlock *pBlock;` |
|       - | 3076 | `		char *zDup;` |
|       - | 3077 | `		/* Prepare the jump destination */` |
|     150 | 3078 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 3079 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 3080 | `		/* Duplicate label name */` |
|     150 | 3081 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 3082 | `		if( zDup == 0 ){` |
|     ! 0 | 3083 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 3084 | `			return SXERR_ABORT;` |
|       - | 3085 | `		}` |
|     150 | 3086 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 3087 | `		pBlock = pGen->pCurrent;` |
|     312 | 3088 | `		while( pBlock ){` |
|     196 | 3089 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 3090 | `				break;` |
|       - | 3091 | `			}` |
|       - | 3092 | `			/* Point to the upper block */` |
|     164 | 3093 | `			pBlock = pBlock->pParent;` |
|       2 | 3094 | `		}` |
|     150 | 3095 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 3096 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 3097 | `			if( rc == SXERR_ABORT ){` |
|       - | 3098 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3099 | `				return SXERR_ABORT;` |
|       - | 3100 | `			}` |
|       3 | 3101 | `		}` |
|     150 | 3102 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 3103 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 3104 | `		}else{` |
|     124 | 3105 | `			sJump.pFunc = 0;` |
|       - | 3106 | `		}` |
|       - | 3107 | `		/* Emit the unconditional jump */` |
|     150 | 3108 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 3109 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 3110 | `		}` |
|       - | 3111 | `	}` |
|     154 | 3112 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 3113 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 3114 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 3115 | `	}` |
|       - | 3116 | `	/* Statement successfully compiled */` |
|     154 | 3117 | `	return SXRET_OK;` |
|      78 | 3118 |  |
|       - | 3119 | `/*` |
|       - | 3120 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 3121 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 3122 | ` * failure.` |
|       - | 3123 | ` */` |
|      20 | 3124 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 3125 |  |
|       - | 3126 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 3127 | `	sxu32 nRawObj;` |
|      10 | 3128 | `	sxu32 nObjIdx;` |
|       - | 3129 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 3130 | `	 * a PHP block.` |
|       - | 3131 | `	 */` |
|      10 | 3132 | `Consume:` |
|      21 | 3133 | `	nRawObj = nObjIdx = 0;` |
|      21 | 3134 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 3135 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 3136 | `		if( pRawObj == 0 ){` |
|     ! 0 | 3137 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3138 | `			return SXERR_ABORT;` |
|       - | 3139 | `		}` |
|       - | 3140 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 3141 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 3142 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 3143 | `		++nRawObj;` |
|     ! 0 | 3144 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 3145 | `	}` |
|      21 | 3146 | `	if( nRawObj > 0 ){` |
|       - | 3147 | `		/* Emit the consume instruction */` |
|     ! 0 | 3148 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 3149 | `	}` |
|      21 | 3150 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 3151 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 3152 | `		/* Reset the token set */` |
|     ! 0 | 3153 | `		SySetReset(pTokenSet);` |
|       - | 3154 | `		/* Tokenize input */` |
|     ! 0 | 3155 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 3156 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 3157 | `		/* Point to the fresh token stream */` |
|     ! 0 | 3158 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 3159 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 3160 | `		/* Advance the stream cursor */` |
|     ! 0 | 3161 | `		pGen->pRawIn++;` |
|       - | 3162 | `		/* TICKET 1433-011 */` |
|     ! 0 | 3163 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 3164 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 3165 | `			sxi32 rc;` |
|       - | 3166 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 3167 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 3168 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 3169 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 3170 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 3171 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3172 | `				return SXERR_ABORT;` |
|     ! 0 | 3173 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 3174 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3175 | `			}` |
|     ! 0 | 3176 | `			goto Consume;` |
|       - | 3177 | `		}` |
|     ! 0 | 3178 | `	}else{` |
|       - | 3179 | `		/* No more chunks to process */` |
|      21 | 3180 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 3181 | `		return SXERR_EOF;` |
|       - | 3182 | `	}` |
|     ! 0 | 3183 | `	return SXRET_OK;` |
|      11 | 3184 |  |
|       - | 3185 | `/*` |
|       - | 3186 | ` * Compile a PHP block.` |
|       - | 3187 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 3188 | ` * optionally delimited by braces {}.` |
|       - | 3189 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 3190 | ` * and this function takes care of generating the appropriate error` |
|       - | 3191 | ` * message.` |
|       - | 3192 | ` */` |
|  302748 | 3193 | `static sxi32 PH7_CompileBlock(` |
|       - | 3194 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 3195 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 3196 | `	)` |
|       2 | 3197 |  |
|       - | 3198 | `	sxi32 rc;` |
|       - | 3199 | `	sxu32 nLine;` |
|  302750 | 3200 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  301340 | 3201 | `		nLine = pGen->pIn->nLine;` |
|  301340 | 3202 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  301340 | 3203 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3204 | `			return SXERR_ABORT;` |
|       - | 3205 | `		}` |
|  301340 | 3206 | `		pGen->pIn++;` |
|       - | 3207 | `		/* Compile until we hit the closing braces '}' */` |
|  415997 | 3208 | `		for(;;){` |
|  831996 | 3209 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 3210 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 3211 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 3212 | `			 	   return SXERR_ABORT;` |
|       - | 3213 | `				}` |
|      21 | 3214 | `				if( rc == SXERR_EOF ){` |
|       - | 3215 | `					/* No more token to process. Missing closing braces */` |
|      21 | 3216 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 3217 | `					break;` |
|       - | 3218 | `				}` |
|     ! 0 | 3219 | `			}` |
|  831976 | 3220 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 3221 | `				/* Closing braces found,break immediately*/` |
|  301320 | 3222 | `				pGen->pIn++;` |
|  301320 | 3223 | `				break;` |
|       - | 3224 | `			}` |
|       - | 3225 | `			/* Compile a single statement */` |
|  530658 | 3226 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  530658 | 3227 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3228 | `				return SXERR_ABORT;` |
|       - | 3229 | `			}` |
|       2 | 3230 | `		}` |
|  301340 | 3231 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  152081 | 3232 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 3233 | `		pGen->pIn++;` |
|     ! 0 | 3234 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 3235 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3236 | `			return SXERR_ABORT;` |
|       - | 3237 | `		}` |
|       - | 3238 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 3239 | `		for(;;){` |
|     ! 0 | 3240 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3241 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 3242 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 3243 | `			 	   return SXERR_ABORT;` |
|       - | 3244 | `				}` |
|     ! 0 | 3245 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 3246 | `					/* No more token to process */` |
|     ! 0 | 3247 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 3248 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 3249 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 3250 | `					}` |
|     ! 0 | 3251 | `					break;` |
|       - | 3252 | `				}` |
|     ! 0 | 3253 | `			}` |
|     ! 0 | 3254 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 3255 | `				sxi32 nKwrd;` |
|       - | 3256 | `				/* Keyword found */` |
|     ! 0 | 3257 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 3258 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 3259 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 3260 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 3261 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 3262 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 3263 | `						}` |
|     ! 0 | 3264 | `						break;` |
|       - | 3265 | `				}` |
|     ! 0 | 3266 | `			}` |
|       - | 3267 | `			/* Compile a single statement */` |
|     ! 0 | 3268 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 3269 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3270 | `				return SXERR_ABORT;` |
|       - | 3271 | `			}` |
|     ! 0 | 3272 | `		}` |
|     ! 0 | 3273 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 3274 | `	}else{` |
|       - | 3275 | `		/* Compile a single statement */` |
|    1412 | 3276 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1412 | 3277 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3278 | `			return SXERR_ABORT;` |
|       - | 3279 | `		}` |
|       - | 3280 | `	}` |
|       - | 3281 | `	/* Jump trailing semi-colons ';' */` |
|  302750 | 3282 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 3283 | `		pGen->pIn++;` |
|     ! 0 | 3284 | `	}` |
|  302750 | 3285 | `	return SXRET_OK;` |
|  151376 | 3286 |  |
|       - | 3287 | `/*` |
|       - | 3288 | ` * Compile the gentle 'while' statement.` |
|       - | 3289 | ` * According to the PHP language reference` |
|       - | 3290 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 3291 | ` *  The basic form of a while statement is:` |
|       - | 3292 | ` *  while (expr)` |
|       - | 3293 | ` *   statement` |
|       - | 3294 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 3295 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 3296 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 3297 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 3298 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 3299 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 3300 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 3301 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 3302 | ` *  while (expr):` |
|       - | 3303 | ` *    statement` |
|       - | 3304 | ` *   endwhile;` |
|       - | 3305 | ` */` |
|   11094 | 3306 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 3307 |  |
|   11096 | 3308 | `	GenBlock *pWhileBlock = 0;` |
|   11096 | 3309 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 3310 | `	sxu32 nFalseJump;` |
|       - | 3311 | `	sxu32 nLine;` |
|       - | 3312 | `	sxi32 rc;` |
|   11096 | 3313 | `	nLine = pGen->pIn->nLine;` |
|       - | 3314 | `	/* Jump the 'while' keyword */` |
|   11096 | 3315 | `	pGen->pIn++;` |
|   11096 | 3316 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3317 | `		/* Syntax error */` |
|     ! 0 | 3318 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 3319 | `		if( rc == SXERR_ABORT ){` |
|       - | 3320 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3321 | `			return SXERR_ABORT;` |
|       - | 3322 | `		}` |
|     ! 0 | 3323 | `		goto Synchronize;` |
|       - | 3324 | `	}` |
|       - | 3325 | `	/* Jump the left parenthesis '(' */` |
|   11096 | 3326 | `	pGen->pIn++;` |
|       - | 3327 | `	/* Create the loop block */` |
|   11096 | 3328 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11096 | 3329 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3330 | `		return SXERR_ABORT;` |
|       - | 3331 | `	}` |
|       - | 3332 | `	/* Delimit the condition */` |
|   11096 | 3333 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11096 | 3334 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3335 | `		/* Empty expression */` |
|       3 | 3336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 3337 | `		if( rc == SXERR_ABORT ){` |
|       - | 3338 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3339 | `			return SXERR_ABORT;` |
|       - | 3340 | `		}` |
|       1 | 3341 | `	}` |
|       - | 3342 | `	/* Swap token streams */` |
|   11096 | 3343 | `	pTmp = pGen->pEnd;` |
|   11096 | 3344 | `	pGen->pEnd = pEnd;` |
|       - | 3345 | `	/* Compile the expression */` |
|   11096 | 3346 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11096 | 3347 | `	if( rc == SXERR_ABORT ){` |
|       - | 3348 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3349 | `		return SXERR_ABORT;` |
|       - | 3350 | `	}` |
|       - | 3351 | `	/* Update token stream */` |
|   11096 | 3352 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 3353 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3354 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3355 | `			return SXERR_ABORT;` |
|       - | 3356 | `		}` |
|     ! 0 | 3357 | `		pGen->pIn++;` |
|     ! 0 | 3358 | `	}` |
|       - | 3359 | `	/* Synchronize pointers */` |
|   11096 | 3360 | `	pGen->pIn  = &pEnd[1];` |
|   11096 | 3361 | `	pGen->pEnd = pTmp;` |
|       - | 3362 | `	/* Emit the false jump */` |
|   11096 | 3363 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 3364 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11096 | 3365 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 3366 | `	/* Compile the loop body */` |
|   11096 | 3367 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11096 | 3368 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3369 | `		return SXERR_ABORT;` |
|       - | 3370 | `	}` |
|       - | 3371 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11096 | 3372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 3373 | `	/* Fix all jumps now the destination is resolved */` |
|   11096 | 3374 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3375 | `	/* Release the loop block */` |
|   11096 | 3376 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3377 | `	/* Statement successfully compiled */` |
|   11096 | 3378 | `	return SXRET_OK;` |
|     ! 0 | 3379 | `Synchronize:` |
|       - | 3380 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3381 | `	 * compiling this erroneous block.` |
|       - | 3382 | `	 */` |
|     ! 0 | 3383 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3384 | `		pGen->pIn++;` |
|     ! 0 | 3385 | `	}` |
|     ! 0 | 3386 | `	return SXRET_OK;` |
|    5549 | 3387 |  |
|       - | 3388 | `/*` |
|       - | 3389 | ` * Compile the ugly do..while() statement.` |
|       - | 3390 | ` * According to the PHP language reference` |
|       - | 3391 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 3392 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 3393 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 3394 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 3395 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 3396 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 3397 | ` *  would end immediately).` |
|       - | 3398 | ` *  There is just one syntax for do-while loops:` |
|       - | 3399 | ` *  <?php` |
|       - | 3400 | ` *  $i = 0;` |
|       - | 3401 | ` *  do {` |
|       - | 3402 | ` *   echo $i;` |
|       - | 3403 | ` *  } while ($i > 0);` |
|       - | 3404 | ` * ?>` |
|       - | 3405 | ` */` |
|       2 | 3406 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 3407 |  |
|       3 | 3408 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 3409 | `	GenBlock *pDoBlock = 0;` |
|       - | 3410 | `	sxu32 nLine;` |
|       - | 3411 | `	sxi32 rc;` |
|       3 | 3412 | `	nLine = pGen->pIn->nLine;` |
|       - | 3413 | `	/* Jump the 'do' keyword */` |
|       3 | 3414 | `	pGen->pIn++;` |
|       - | 3415 | `	/* Create the loop block */` |
|       3 | 3416 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 3417 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3418 | `		return SXERR_ABORT;` |
|       - | 3419 | `	}` |
|       - | 3420 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 3421 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 3422 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 3423 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3424 | `		return SXERR_ABORT;` |
|       - | 3425 | `	}` |
|       3 | 3426 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3427 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 3428 | `	}` |
|       3 | 3429 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 3430 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 3431 | `			/* Missing 'while' statement */` |
|       3 | 3432 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 3433 | `			if( rc == SXERR_ABORT ){` |
|       - | 3434 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3435 | `				return SXERR_ABORT;` |
|       - | 3436 | `			}` |
|       3 | 3437 | `			goto Synchronize;` |
|       - | 3438 | `	}` |
|       - | 3439 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 3440 | `	pGen->pIn++;` |
|     ! 0 | 3441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3442 | `		/* Syntax error */` |
|     ! 0 | 3443 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 3444 | `		if( rc == SXERR_ABORT ){` |
|       - | 3445 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3446 | `			return SXERR_ABORT;` |
|       - | 3447 | `		}` |
|     ! 0 | 3448 | `		goto Synchronize;` |
|       - | 3449 | `	}` |
|       - | 3450 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 3451 | `	pGen->pIn++;` |
|       - | 3452 | `	/* Delimit the condition */` |
|     ! 0 | 3453 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 3454 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3455 | `		/* Empty expression */` |
|     ! 0 | 3456 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 3457 | `		if( rc == SXERR_ABORT ){` |
|       - | 3458 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3459 | `			return SXERR_ABORT;` |
|       - | 3460 | `		}` |
|     ! 0 | 3461 | `		goto Synchronize;` |
|       - | 3462 | `	}` |
|       - | 3463 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 3464 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 3465 | `		JumpFixup *aPost;` |
|       - | 3466 | `		VmInstr *pInstr;` |
|       - | 3467 | `		sxu32 nJumpDest;` |
|       - | 3468 | `		sxu32 n;` |
|     ! 0 | 3469 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 3470 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 3471 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 3472 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 3473 | `			if( pInstr ){` |
|       - | 3474 | `				/* Fix */` |
|     ! 0 | 3475 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 3476 | `			}` |
|     ! 0 | 3477 | `		}` |
|     ! 0 | 3478 | `	}` |
|       - | 3479 | `	/* Swap token streams */` |
|     ! 0 | 3480 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 3481 | `	pGen->pEnd = pEnd;` |
|       - | 3482 | `	/* Compile the expression */` |
|     ! 0 | 3483 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3484 | `	if( rc == SXERR_ABORT ){` |
|       - | 3485 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3486 | `		return SXERR_ABORT;` |
|       - | 3487 | `	}` |
|       - | 3488 | `	/* Update token stream */` |
|     ! 0 | 3489 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 3490 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3492 | `			return SXERR_ABORT;` |
|       - | 3493 | `		}` |
|     ! 0 | 3494 | `		pGen->pIn++;` |
|     ! 0 | 3495 | `	}` |
|     ! 0 | 3496 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 3497 | `	pGen->pEnd = pTmp;` |
|       - | 3498 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 3499 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 3500 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 3501 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3502 | `	/* Release the loop block */` |
|     ! 0 | 3503 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3504 | `	/* Statement successfully compiled */` |
|     ! 0 | 3505 | `	return SXRET_OK;` |
|       1 | 3506 | `Synchronize:` |
|       - | 3507 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3508 | `	 * compiling this erroneous block.` |
|       - | 3509 | `	 */` |
|       3 | 3510 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3511 | `		pGen->pIn++;` |
|     ! 0 | 3512 | `	}` |
|       3 | 3513 | `	return SXRET_OK;` |
|       2 | 3514 |  |
|       - | 3515 | `/*` |
|       - | 3516 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 3517 | ` * According to the PHP language reference` |
|       - | 3518 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 3519 | ` *  The syntax of a for loop is:` |
|       - | 3520 | ` *  for (expr1; expr2; expr3)` |
|       - | 3521 | ` *   statement` |
|       - | 3522 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 3523 | ` *  the beginning of the loop.` |
|       - | 3524 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 3525 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 3526 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 3527 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 3528 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 3529 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 3530 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 3531 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 3532 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 3533 | ` *  of using the for truth expression.` |
|       - | 3534 | ` */` |
|   11096 | 3535 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 3536 |  |
|   11098 | 3537 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11098 | 3538 | `	GenBlock *pForBlock = 0;` |
|       - | 3539 | `	sxu32 nFalseJump;` |
|       - | 3540 | `	sxu32 nLine;` |
|       - | 3541 | `	sxi32 rc;` |
|   11098 | 3542 | `	nLine = pGen->pIn->nLine;` |
|       - | 3543 | `	/* Jump the 'for' keyword */` |
|   11098 | 3544 | `	pGen->pIn++;` |
|   11098 | 3545 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3546 | `		/* Syntax error */` |
|     ! 0 | 3547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 3548 | `		if( rc == SXERR_ABORT ){` |
|       - | 3549 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3550 | `			return SXERR_ABORT;` |
|       - | 3551 | `		}` |
|     ! 0 | 3552 | `		return SXRET_OK;` |
|       - | 3553 | `	}` |
|       - | 3554 | `	/* Jump the left parenthesis '(' */` |
|   11098 | 3555 | `	pGen->pIn++;` |
|       - | 3556 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11098 | 3557 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11098 | 3558 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3559 | `		/* Empty expression */` |
|     ! 0 | 3560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 3561 | `		if( rc == SXERR_ABORT ){` |
|       - | 3562 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3563 | `			return SXERR_ABORT;` |
|       - | 3564 | `		}` |
|       - | 3565 | `		/* Synchronize */` |
|     ! 0 | 3566 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3567 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3568 | `			pGen->pIn++;` |
|     ! 0 | 3569 | `		}` |
|     ! 0 | 3570 | `		return SXRET_OK;` |
|       - | 3571 | `	}` |
|       - | 3572 | `	/* Swap token streams */` |
|   11098 | 3573 | `	pTmp = pGen->pEnd;` |
|   11098 | 3574 | `	pGen->pEnd = pEnd;` |
|       - | 3575 | `	/* Compile initialization expressions if available */` |
|   11098 | 3576 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3577 | `	/* Pop operand lvalues */` |
|   11098 | 3578 | `	if( rc == SXERR_ABORT ){` |
|       - | 3579 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3580 | `		return SXERR_ABORT;` |
|   11098 | 3581 | `	}else if( rc != SXERR_EMPTY ){` |
|   11096 | 3582 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5547 | 3583 | `	}` |
|   11098 | 3584 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3585 | `		/* Syntax error */` |
|     ! 0 | 3586 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3587 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 3588 | `		if( rc == SXERR_ABORT ){` |
|       - | 3589 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3590 | `			return SXERR_ABORT;` |
|       - | 3591 | `		}` |
|     ! 0 | 3592 | `		return SXRET_OK;` |
|       - | 3593 | `	}` |
|       - | 3594 | `	/* Jump the trailing ';' */` |
|   11098 | 3595 | `	pGen->pIn++;` |
|       - | 3596 | `	/* Create the loop block */` |
|   11098 | 3597 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11098 | 3598 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3599 | `		return SXERR_ABORT;` |
|       - | 3600 | `	}` |
|       - | 3601 | `	/* Deffer continue jumps */` |
|   11098 | 3602 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 3603 | `	/* Compile the condition */` |
|   11098 | 3604 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11098 | 3605 | `	if( rc == SXERR_ABORT ){` |
|       - | 3606 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3607 | `		return SXERR_ABORT;` |
|   11098 | 3608 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 3609 | `		/* Emit the false jump */` |
|   11096 | 3610 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 3611 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11096 | 3612 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5547 | 3613 | `	}` |
|   11098 | 3614 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3615 | `		/* Syntax error */` |
|       5 | 3616 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3617 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 3618 | `		if( rc == SXERR_ABORT ){` |
|       - | 3619 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3620 | `			return SXERR_ABORT;` |
|       - | 3621 | `		}` |
|       5 | 3622 | `		return SXRET_OK;` |
|       - | 3623 | `	}` |
|       - | 3624 | `	/* Jump the trailing ';' */` |
|   11094 | 3625 | `	pGen->pIn++;` |
|       - | 3626 | `	/* Save the post condition stream */` |
|   11094 | 3627 | `	pPostStart = pGen->pIn;` |
|       - | 3628 | `	/* Compile the loop body */` |
|   11094 | 3629 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11094 | 3630 | `	pGen->pEnd = pTmp;` |
|   11094 | 3631 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11094 | 3632 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3633 | `		return SXERR_ABORT;` |
|       - | 3634 | `	}` |
|       - | 3635 | `	/* Fix post-continue jumps */` |
|   11094 | 3636 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 3637 | `		JumpFixup *aPost;` |
|       - | 3638 | `		VmInstr *pInstr;` |
|       - | 3639 | `		sxu32 nJumpDest;` |
|       - | 3640 | `		sxu32 n;` |
|      14 | 3641 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 | 3642 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 | 3643 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 | 3644 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 | 3645 | `			if( pInstr ){` |
|       - | 3646 | `				/* Fix jump */` |
|      14 | 3647 | `				pInstr->iP2 = nJumpDest;` |
|       6 | 3648 | `			}` |
|       8 | 3649 | `		}` |
|       6 | 3650 | `	}` |
|       - | 3651 | `	/* compile the post-expressions if available */` |
|   11094 | 3652 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 3653 | `		pPostStart++;` |
|     ! 0 | 3654 | `	}` |
|   11094 | 3655 | `	if( pPostStart < pEnd ){` |
|       - | 3656 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11094 | 3657 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11094 | 3658 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11094 | 3659 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 3660 | `			/* Syntax error */` |
|     ! 0 | 3661 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 3662 | `			if( rc == SXERR_ABORT ){` |
|       - | 3663 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3664 | `				return SXERR_ABORT;` |
|       - | 3665 | `			}` |
|     ! 0 | 3666 | `			return SXRET_OK;` |
|       - | 3667 | `		}` |
|   11094 | 3668 | `		RE_SWAP_DELIMITER(pGen);` |
|   11094 | 3669 | `		if( rc == SXERR_ABORT ){` |
|       - | 3670 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3671 | `			return SXERR_ABORT;` |
|   11094 | 3672 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 3673 | `			/* Pop operand lvalue */` |
|   11094 | 3674 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5546 | 3675 | `		}` |
|    5546 | 3676 | `	}` |
|       - | 3677 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11094 | 3678 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 3679 | `	/* Fix all jumps now the destination is resolved */` |
|   11094 | 3680 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3681 | `	/* Release the loop block */` |
|   11094 | 3682 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3683 | `	/* Statement successfully compiled */` |
|   11094 | 3684 | `	return SXRET_OK;` |
|    5550 | 3685 |  |
|       - | 3686 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 3687 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 3688 | ` * are allowed.` |
|       - | 3689 | ` */` |
|    5896 | 3690 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 3691 |  |
|    5898 | 3692 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5898 | 3693 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 3694 | `		/* Unexpected expression */` |
|     ! 0 | 3695 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 3696 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 3697 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 3698 | `			rc = SXERR_INVALID;` |
|     ! 0 | 3699 | `		}` |
|     ! 0 | 3700 | `	}` |
|    5898 | 3701 | `	return rc;` |
|       2 | 3702 |  |
|       - | 3703 | `/*` |
|       - | 3704 | ` * Compile the 'foreach' statement.` |
|       - | 3705 | ` * According to the PHP language reference` |
|       - | 3706 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 3707 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 3708 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 3709 | ` *  is a minor but useful extension of the first:` |
|       - | 3710 | ` *  foreach (array_expression as $value)` |
|       - | 3711 | ` *    statement` |
|       - | 3712 | ` *  foreach (array_expression as $key => $value)` |
|       - | 3713 | ` *   statement` |
|       - | 3714 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 3715 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 3716 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 3717 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 3718 | ` *  to the variable $key on each loop.` |
|       - | 3719 | ` *  Note:` |
|       - | 3720 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 3721 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 3722 | ` *  Note:` |
|       - | 3723 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 3724 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 3725 | ` *  or after the foreach without resetting it.` |
|       - | 3726 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 3727 | ` *  of copying the value.` |
|       - | 3728 | ` */` |
|    3000 | 3729 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 3730 |  |
|    3002 | 3731 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3002 | 3732 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3002 | 3733 | `	GenBlock *pForeachBlock = 0;` |
|       - | 3734 | `	ph7_foreach_info *pInfo;` |
|       - | 3735 | `	sxu32 nFalseJump;` |
|       - | 3736 | `	VmInstr *pInstr;` |
|       - | 3737 | `	sxu32 nLine;` |
|       - | 3738 | `	sxi32 rc;` |
|    3002 | 3739 | `	nLine = pGen->pIn->nLine;` |
|       - | 3740 | `	/* Jump the 'foreach' keyword */` |
|    3002 | 3741 | `	pGen->pIn++;` |
|    3002 | 3742 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3743 | `		/* Syntax error */` |
|     ! 0 | 3744 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 3745 | `		if( rc == SXERR_ABORT ){` |
|       - | 3746 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3747 | `			return SXERR_ABORT;` |
|       - | 3748 | `		}` |
|     ! 0 | 3749 | `		goto Synchronize;` |
|       - | 3750 | `	}` |
|       - | 3751 | `	/* Jump the left parenthesis '(' */` |
|    3002 | 3752 | `	pGen->pIn++;` |
|       - | 3753 | `	/* Create the loop block */` |
|    3002 | 3754 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3002 | 3755 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3756 | `		return SXERR_ABORT;` |
|       - | 3757 | `	}` |
|       - | 3758 | `	/* Delimit the expression */` |
|    3002 | 3759 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3002 | 3760 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3761 | `		/* Empty expression */` |
|     ! 0 | 3762 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 3763 | `		if( rc == SXERR_ABORT ){` |
|       - | 3764 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3765 | `			return SXERR_ABORT;` |
|       - | 3766 | `		}` |
|       - | 3767 | `		/* Synchronize */` |
|     ! 0 | 3768 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3769 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3770 | `			pGen->pIn++;` |
|     ! 0 | 3771 | `		}` |
|     ! 0 | 3772 | `		return SXRET_OK;` |
|       - | 3773 | `	}` |
|       - | 3774 | `	/* Compile the array expression */` |
|    3002 | 3775 | `	pCur = pGen->pIn;` |
|   20100 | 3776 | `	while( pCur < pEnd ){` |
|   20100 | 3777 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3012 | 3778 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3012 | 3779 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 3780 | `				/* Break with the first 'as' found */` |
|    3002 | 3781 | `				break;` |
|       - | 3782 | `			}` |
|       5 | 3783 | `		}` |
|       - | 3784 | `		/* Advance the stream cursor */` |
|   17100 | 3785 | `		pCur++;` |
|       2 | 3786 | `	}` |
|    3002 | 3787 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 3788 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 3789 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 3790 | `		if( rc == SXERR_ABORT ){` |
|       - | 3791 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3792 | `			return SXERR_ABORT;` |
|       - | 3793 | `		}` |
|     ! 0 | 3794 | `		goto Synchronize;` |
|       - | 3795 | `	}` |
|       - | 3796 | `	/* Swap token streams */` |
|    3002 | 3797 | `	pTmp = pGen->pEnd;` |
|    3002 | 3798 | `	pGen->pEnd = pCur;` |
|    3002 | 3799 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3002 | 3800 | `	if( rc == SXERR_ABORT ){` |
|       - | 3801 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3802 | `		return SXERR_ABORT;` |
|       - | 3803 | `	}` |
|       - | 3804 | `	/* Update token stream */` |
|    3002 | 3805 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 3806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3807 | `		if( rc == SXERR_ABORT ){` |
|       - | 3808 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3809 | `			return SXERR_ABORT;` |
|       - | 3810 | `		}` |
|     ! 0 | 3811 | `		pGen->pIn++;` |
|     ! 0 | 3812 | `	}` |
|    3002 | 3813 | `	pCur++; /* Jump the 'as' keyword */` |
|    3002 | 3814 | `	pGen->pIn = pCur;` |
|    3002 | 3815 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 3817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3818 | `			return SXERR_ABORT;` |
|       - | 3819 | `		}` |
|     ! 0 | 3820 | `	}` |
|       - | 3821 | `	/* Create the foreach context */` |
|    3002 | 3822 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3002 | 3823 | `	if( pInfo == 0 ){` |
|     ! 0 | 3824 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3825 | `		return SXERR_ABORT;` |
|       - | 3826 | `	}` |
|       - | 3827 | `	/* Zero the structure */` |
|    3002 | 3828 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 3829 | `	/* Initialize structure fields */` |
|    3002 | 3830 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 3831 | `	/* Check if we have a key field */` |
|    9052 | 3832 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6052 | 3833 | `		pCur++;` |
|       2 | 3834 | `	}` |
|    3002 | 3835 | `	if( pCur < pEnd ){` |
|       - | 3836 | `		/* Compile the expression holding the key name */` |
|    2908 | 3837 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 3838 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 3839 | `			if( rc == SXERR_ABORT ){` |
|       - | 3840 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3841 | `				return SXERR_ABORT;` |
|       - | 3842 | `			}` |
|     ! 0 | 3843 | `		}else{` |
|    2908 | 3844 | `			pGen->pEnd = pCur;` |
|    2908 | 3845 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2908 | 3846 | `			if( rc == SXERR_ABORT ){` |
|       - | 3847 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3848 | `				return SXERR_ABORT;` |
|       - | 3849 | `			}` |
|    2908 | 3850 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2908 | 3851 | `			if( pInstr->p3 ){` |
|       - | 3852 | `				/* Record key name */` |
|    2908 | 3853 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1453 | 3854 | `			}` |
|    2908 | 3855 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 3856 | `		}` |
|    2908 | 3857 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1453 | 3858 | `	}` |
|    3002 | 3859 | `	pGen->pEnd = pEnd;` |
|    3002 | 3860 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3861 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 3862 | `		if( rc == SXERR_ABORT ){` |
|       - | 3863 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3864 | `			return SXERR_ABORT;` |
|       - | 3865 | `		}` |
|     ! 0 | 3866 | `		goto Synchronize;` |
|       - | 3867 | `	}` |
|    3002 | 3868 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3869 | `		pGen->pIn++;` |
|       - | 3870 | `		/* Pass by reference  */` |
|      11 | 3871 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3872 | `	}` |
|       - | 3873 | `	/* Check if the value target is list() */` |
|    3002 | 3874 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3875 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3876 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3877 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3878 | `		 */` |
|       - | 3879 | `		static int iForeachListCnt = 0;` |
|       - | 3880 | `		char zTmp[128];` |
|       - | 3881 | `		sxu32 nLen;` |
|       - | 3882 | `		char *zDup;` |
|      10 | 3883 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3884 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3885 | `		if( zDup == 0 ){` |
|     ! 0 | 3886 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3887 | `			return SXERR_ABORT;` |
|       - | 3888 | `		}` |
|      10 | 3889 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3890 | `		/* Save list() token boundaries */` |
|      10 | 3891 | `		pListStart = pGen->pIn;` |
|       - | 3892 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3893 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3894 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3895 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3896 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3897 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3898 | `				return SXERR_ABORT;` |
|       - | 3899 | `			}` |
|       3 | 3900 | `			goto Synchronize;` |
|       - | 3901 | `		}` |
|       7 | 3902 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3903 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3904 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3905 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3906 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3907 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3908 | `				return SXERR_ABORT;` |
|       - | 3909 | `			}` |
|     ! 0 | 3910 | `			goto Synchronize;` |
|       - | 3911 | `		}` |
|       7 | 3912 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3913 | `		pListEnd = pGen->pIn;` |
|       7 | 3914 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2997 | 3915 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3916 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3917 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3918 | `		 */` |
|       - | 3919 | `		static int iForeachShortListCnt = 0;` |
|       - | 3920 | `		char zTmp[128];` |
|       - | 3921 | `		sxu32 nLen;` |
|       - | 3922 | `		char *zDup;` |
|       3 | 3923 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3924 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3925 | `		if( zDup == 0 ){` |
|     ! 0 | 3926 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3927 | `			return SXERR_ABORT;` |
|       - | 3928 | `		}` |
|       3 | 3929 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3930 | `		/* Save [...] token boundaries */` |
|       3 | 3931 | `		pListStart = pGen->pIn;` |
|       - | 3932 | `		/* Advance past [...] */` |
|       3 | 3933 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3934 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3935 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3936 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3937 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3938 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3939 | `				return SXERR_ABORT;` |
|       - | 3940 | `			}` |
|     ! 0 | 3941 | `			goto Synchronize;` |
|       - | 3942 | `		}` |
|       3 | 3943 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3944 | `		pListEnd = pGen->pIn;` |
|       3 | 3945 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3946 | `	}else{` |
|       - | 3947 | `		/* Compile the expression holding the value name */` |
|    2992 | 3948 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2992 | 3949 | `		if( rc == SXERR_ABORT ){` |
|       - | 3950 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3951 | `			return SXERR_ABORT;` |
|       - | 3952 | `		}` |
|    2992 | 3953 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2992 | 3954 | `		if( pInstr->p3 ){` |
|       - | 3955 | `			/* Record value name */` |
|    2992 | 3956 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1495 | 3957 | `		}` |
|       - | 3958 | `	}` |
|       - | 3959 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3000 | 3960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3961 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3000 | 3962 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3963 | `	/* Record the first instruction to execute */` |
|    3000 | 3964 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3965 | `	/* Emit the FOREACH_STEP instruction */` |
|    3000 | 3966 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3967 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3000 | 3968 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3969 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3000 | 3970 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3971 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3972 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3973 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3974 | `		 */` |
|       9 | 3975 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3976 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3977 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3978 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3979 | `		 */` |
|       9 | 3980 | `		pSavedIn = pGen->pIn;` |
|       9 | 3981 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3982 | `		pGen->pIn = pListStart;` |
|       9 | 3983 | `		pGen->pEnd = pListEnd;` |
|       9 | 3984 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3985 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3986 | `		}else{` |
|       7 | 3987 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3988 | `		}` |
|       9 | 3989 | `		pGen->pIn = pSavedIn;` |
|       9 | 3990 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3991 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3992 | `			return SXERR_ABORT;` |
|       - | 3993 | `		}` |
|       - | 3994 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3995 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3996 | `	}` |
|       - | 3997 | `	/* Compile the loop body */` |
|    3000 | 3998 | `	pGen->pIn = &pEnd[1];` |
|    3000 | 3999 | `	pGen->pEnd = pTmp;` |
|    3000 | 4000 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3000 | 4001 | `	if( rc == SXERR_ABORT ){` |
|       - | 4002 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4003 | `		return SXERR_ABORT;` |
|       - | 4004 | `	}` |
|       - | 4005 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3000 | 4006 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 4007 | `	/* Fix all jumps now the destination is resolved */` |
|    3000 | 4008 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4009 | `	/* Release the loop block */` |
|    3000 | 4010 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 4011 | `	/* Statement successfully compiled */` |
|    3000 | 4012 | `	return SXRET_OK;` |
|       1 | 4013 | `Synchronize:` |
|       - | 4014 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 4015 | `	 * compiling this erroneous block.` |
|       - | 4016 | `	 */` |
|       3 | 4017 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4018 | `		pGen->pIn++;` |
|     ! 0 | 4019 | `	}` |
|       3 | 4020 | `	return SXRET_OK;` |
|    1502 | 4021 |  |
|       - | 4022 | `/*` |
|       - | 4023 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 4024 | ` * According to the PHP language reference` |
|       - | 4025 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 4026 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 4027 | ` *  that is similar to that of C:` |
|       - | 4028 | ` *  if (expr)` |
|       - | 4029 | ` *   statement` |
|       - | 4030 | ` *  else construct:` |
|       - | 4031 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 4032 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 4033 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 4034 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 4035 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 4036 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 4037 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 4038 | ` *  elseif` |
|       - | 4039 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 4040 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 4041 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 4042 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 4043 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 4044 | ` *   <?php` |
|       - | 4045 | ` *    if ($a > $b) {` |
|       - | 4046 | ` *     echo "a is bigger than b";` |
|       - | 4047 | ` *    } elseif ($a == $b) {` |
|       - | 4048 | ` *     echo "a is equal to b";` |
|       - | 4049 | ` *    } else {` |
|       - | 4050 | ` *     echo "a is smaller than b";` |
|       - | 4051 | ` *    }` |
|       - | 4052 | ` *    ?>` |
|       - | 4053 | ` */` |
|  110218 | 4054 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 4055 |  |
|  110220 | 4056 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  110220 | 4057 | `	GenBlock *pCondBlock = 0;` |
|       - | 4058 | `	sxu32 nJumpIdx;` |
|       - | 4059 | `	sxu32 nKeyID;` |
|       - | 4060 | `	sxi32 rc;` |
|       - | 4061 | `	/* Jump the 'if' keyword */` |
|  110220 | 4062 | `	pGen->pIn++;` |
|  110220 | 4063 | `	pToken = pGen->pIn;` |
|       - | 4064 | `	/* Create the conditional block */` |
|  110220 | 4065 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  110220 | 4066 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4067 | `		return SXERR_ABORT;` |
|       - | 4068 | `	}` |
|       - | 4069 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   60617 | 4070 | `	for(;;){` |
|  121236 | 4071 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4072 | `			/* Syntax error */` |
|     ! 0 | 4073 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4074 | `				pToken--;` |
|     ! 0 | 4075 | `			}` |
|     ! 0 | 4076 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 4077 | `			if( rc == SXERR_ABORT ){` |
|       - | 4078 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4079 | `				return SXERR_ABORT;` |
|       - | 4080 | `			}` |
|     ! 0 | 4081 | `			goto Synchronize;` |
|       - | 4082 | `		}` |
|       - | 4083 | `		/* Jump the left parenthesis '(' */` |
|  121236 | 4084 | `		pToken++;` |
|       - | 4085 | `		/* Delimit the condition */` |
|  121236 | 4086 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  121236 | 4087 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 4088 | `			/* Syntax error */` |
|     ! 0 | 4089 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 4090 | `				pToken--;` |
|     ! 0 | 4091 | `			}` |
|     ! 0 | 4092 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 4093 | `			if( rc == SXERR_ABORT ){` |
|       - | 4094 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 4095 | `				return SXERR_ABORT;` |
|       - | 4096 | `			}` |
|     ! 0 | 4097 | `			goto Synchronize;` |
|       - | 4098 | `		}` |
|       - | 4099 | `		/* Swap token streams */` |
|  121236 | 4100 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 4101 | `		/* Compile the condition */` |
|  121236 | 4102 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4103 | `		/* Update token stream */` |
|  121236 | 4104 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 4105 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4106 | `			pGen->pIn++;` |
|     ! 0 | 4107 | `		}` |
|  121236 | 4108 | `		pGen->pIn  = &pEnd[1];` |
|  121236 | 4109 | `		pGen->pEnd = pTmp;` |
|  121236 | 4110 | `		if( rc == SXERR_ABORT ){` |
|       - | 4111 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 4112 | `			return SXERR_ABORT;` |
|       - | 4113 | `		}` |
|       - | 4114 | `		/* Emit the false jump */` |
|  121236 | 4115 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 4116 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  121236 | 4117 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 4118 | `		/* Compile the body */` |
|  121236 | 4119 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  121236 | 4120 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4121 | `			return SXERR_ABORT;` |
|       - | 4122 | `		}` |
|  121236 | 4123 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   32624 | 4124 | `			break;` |
|       - | 4125 | `		}` |
|       - | 4126 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   55992 | 4127 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   55992 | 4128 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   36010 | 4129 | `			break;` |
|       - | 4130 | `		}` |
|       - | 4131 | `		/* Emit the unconditional jump */` |
|   19984 | 4132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 4133 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19984 | 4134 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19984 | 4135 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14464 | 4136 | `			pToken = &pGen->pIn[1];` |
|   14464 | 4137 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5526 | 4138 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4485 | 4139 | `					break;` |
|       - | 4140 | `			}` |
|    5498 | 4141 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2748 | 4142 | `		}` |
|   11018 | 4143 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 4144 | `		/* Synchronize cursors */` |
|   11018 | 4145 | `		pToken = pGen->pIn;` |
|       - | 4146 | `		/* Fix the false jump */` |
|   11018 | 4147 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 4148 | `	} /* For(;;) */` |
|       - | 4149 | `	/* Fix the false jump */` |
|  110220 | 4150 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  110220 | 4151 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   44974 | 4152 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 4153 | `			/* Compile the else block */` |
|    8968 | 4154 | `			pGen->pIn++;` |
|    8968 | 4155 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8968 | 4156 | `			if( rc == SXERR_ABORT ){` |
|       - | 4157 |  |
|     ! 0 | 4158 | `				return SXERR_ABORT;` |
|       - | 4159 | `			}` |
|    4483 | 4160 | `	}` |
|  110220 | 4161 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 4162 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  110220 | 4163 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 4164 | `	/* Release the conditional block */` |
|  110220 | 4165 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 4166 | `	/* Statement successfully compiled */` |
|  110220 | 4167 | `	return SXRET_OK;` |
|     ! 0 | 4168 | `Synchronize:` |
|       - | 4169 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 4170 | `	 */` |
|     ! 0 | 4171 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4172 | `		pGen->pIn++;` |
|     ! 0 | 4173 | `	}` |
|     ! 0 | 4174 | `	return SXRET_OK;` |
|   55111 | 4175 |  |
|       - | 4176 | `/*` |
|       - | 4177 | ` * Compile the global construct.` |
|       - | 4178 | ` * According to the PHP language reference` |
|       - | 4179 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 4180 | ` *  to be used in that function.` |
|       - | 4181 | ` *  Example #1 Using global` |
|       - | 4182 | ` *  <?php` |
|       - | 4183 | ` *   $a = 1;` |
|       - | 4184 | ` *   $b = 2;` |
|       - | 4185 | ` *   function Sum()` |
|       - | 4186 | ` *   {` |
|       - | 4187 | ` *    global $a, $b;` |
|       - | 4188 | ` *    $b = $a + $b;` |
|       - | 4189 | ` *   }` |
|       - | 4190 | ` *   Sum();` |
|       - | 4191 | ` *   echo $b;` |
|       - | 4192 | ` *  ?>` |
|       - | 4193 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 4194 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 4195 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 4196 | ` */` |
|      26 | 4197 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 4198 |  |
|      28 | 4199 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 4200 | `	sxi32 nExpr;` |
|       - | 4201 | `	sxi32 rc;` |
|       - | 4202 | `	/* Jump the 'global' keyword */` |
|      28 | 4203 | `	pGen->pIn++;` |
|      28 | 4204 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 4205 | `		/* Nothing to process */` |
|     ! 0 | 4206 | `		return SXRET_OK;` |
|       - | 4207 | `	}` |
|      28 | 4208 | `	pTmp = pGen->pEnd;` |
|      28 | 4209 | `	nExpr = 0;` |
|      56 | 4210 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 4211 | `		if( pGen->pIn < pNext ){` |
|      30 | 4212 | `			pGen->pEnd = pNext;` |
|      30 | 4213 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 4214 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 4215 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 4216 | `					return SXERR_ABORT;` |
|       - | 4217 | `				}` |
|     ! 0 | 4218 | `			}else{` |
|      30 | 4219 | `				pGen->pIn++;` |
|      30 | 4220 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4221 | `					/* Emit a warning */` |
|     ! 0 | 4222 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 4223 | `				}else{` |
|      30 | 4224 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 4225 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4226 | `						return SXERR_ABORT;` |
|      30 | 4227 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 4228 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 4229 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 4230 | `							/* Variable name, not a constant */` |
|      30 | 4231 | `							pLast->iP1 = 0;` |
|      14 | 4232 | `						}` |
|      30 | 4233 | `						nExpr++;` |
|      14 | 4234 | `					}` |
|       - | 4235 | `				}` |
|       - | 4236 | `			}` |
|      14 | 4237 | `		}` |
|       - | 4238 | `		/* Next expression in the stream */` |
|      30 | 4239 | `		pGen->pIn = pNext;` |
|       - | 4240 | `		/* Jump trailing commas */` |
|      32 | 4241 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4242 | `			pGen->pIn++;` |
|       1 | 4243 | `		}` |
|       2 | 4244 | `	}` |
|       - | 4245 | `	/* Restore token stream */` |
|      28 | 4246 | `	pGen->pEnd = pTmp;` |
|      28 | 4247 | `	if( nExpr > 0 ){` |
|       - | 4248 | `		/* Emit the uplink instruction */` |
|      28 | 4249 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 4250 | `	}` |
|      28 | 4251 | `	return SXRET_OK;` |
|      15 | 4252 |  |
|       - | 4253 | `/*` |
|       - | 4254 | ` * Compile the return statement.` |
|       - | 4255 | ` * According to the PHP language reference` |
|       - | 4256 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 4257 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 4258 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 4259 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 4260 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 4261 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 4262 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 4263 | ` *  from within the main script file, then script execution end.` |
|       - | 4264 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 4265 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 4266 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 4267 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 4268 | ` */` |
|  160090 | 4269 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 4270 |  |
|  160092 | 4271 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 4272 | `	sxi32 rc;` |
|       - | 4273 | `	/* Jump the 'return' keyword */` |
|  160092 | 4274 | `	pGen->pIn++;` |
|  160092 | 4275 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 4276 | `		/* Compile the expression */` |
|  160070 | 4277 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  160070 | 4278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4279 | `			return SXERR_ABORT;` |
|  160070 | 4280 | `		}else if(rc != SXERR_EMPTY ){` |
|  160070 | 4281 | `			nRet = 1;` |
|   80034 | 4282 | `		}` |
|   80034 | 4283 | `	}` |
|       - | 4284 | `	/* Emit the done instruction */` |
|  160092 | 4285 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  160092 | 4286 | `	return SXRET_OK;` |
|   80047 | 4287 |  |
|       - | 4288 | `/*` |
|       - | 4289 | ` * Compile a yield expression.` |
|       - | 4290 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 4291 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 4292 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 4293 | ` */` |
|      32 | 4294 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 4295 |  |
|       - | 4296 | `	SyToken *pTmp, *pSplit;` |
|      34 | 4297 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 4298 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 4299 | `	sxi32 rc;` |
|      16 | 4300 | `	(void)iCompileFlag;` |
|       - | 4301 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 4302 | `	pGen->pIn++;` |
|       - | 4303 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 4304 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 4305 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4306 | `		/* Bare yield — no value */` |
|     ! 0 | 4307 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 4308 | `		return SXRET_OK;` |
|       - | 4309 | `	}` |
|       - | 4310 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 4311 | `	pSplit = 0;` |
|       - | 4312 | `	{` |
|      34 | 4313 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 4314 | `		sxi32 nNest = 0;` |
|      78 | 4315 | `		while( pCur < pGen->pEnd ){` |
|      52 | 4316 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 4317 | `				nNest++;` |
|      52 | 4318 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 4319 | `				nNest--;` |
|      52 | 4320 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 4321 | `				pSplit = pCur;` |
|       7 | 4322 | `				break;` |
|       - | 4323 | `			}` |
|      46 | 4324 | `			pCur++;` |
|       2 | 4325 | `		}` |
|       - | 4326 | `	}` |
|      34 | 4327 | `	pTmp = pGen->pEnd;` |
|      34 | 4328 | `	if( pSplit ){` |
|       - | 4329 | `		/* yield $key => $value */` |
|       7 | 4330 | `		pGen->pEnd = pSplit;` |
|       7 | 4331 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 4332 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 4333 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 4334 | `		pGen->pEnd = pTmp;` |
|       7 | 4335 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 4336 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 4337 | `		iP1 = 1;` |
|       7 | 4338 | `		iP2 = 1;` |
|       4 | 4339 | `	}else{` |
|       - | 4340 | `		/* yield $value */` |
|      28 | 4341 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 4342 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 4343 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 4344 | `			iP1 = 1;` |
|      13 | 4345 | `		}` |
|       - | 4346 | `	}` |
|      34 | 4347 | `	pGen->pEnd = pTmp;` |
|      34 | 4348 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 4349 | `	return SXRET_OK;` |
|      18 | 4350 |  |
|       - | 4351 | `/*` |
|       - | 4352 | ` * Compile the die/exit language construct.` |
|       - | 4353 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 4354 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 4355 | ` */` |
|      88 | 4356 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 4357 |  |
|      90 | 4358 | `	sxi32 nExpr = 0;` |
|       - | 4359 | `	sxi32 rc;` |
|       - | 4360 | `	/* Jump the die/exit keyword */` |
|      90 | 4361 | `	pGen->pIn++;` |
|      90 | 4362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 4363 | `		/* Compile the expression */` |
|      90 | 4364 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 4365 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4366 | `			return SXERR_ABORT;` |
|      90 | 4367 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 4368 | `			nExpr = 1;` |
|      44 | 4369 | `		}` |
|      44 | 4370 | `	}` |
|       - | 4371 | `	/* Emit the HALT instruction */` |
|      90 | 4372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 4373 | `	return SXRET_OK;` |
|      46 | 4374 |  |
|       - | 4375 | `/*` |
|       - | 4376 | ` * Compile the 'echo' language construct.` |
|       - | 4377 | ` */` |
|   11356 | 4378 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 4379 |  |
|   11358 | 4380 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 4381 | `	sxi32 rc;` |
|       - | 4382 | `	/* Jump the 'echo' keyword */` |
|   11358 | 4383 | `	pGen->pIn++;` |
|       - | 4384 | `	/* Compile arguments one after one */` |
|   11358 | 4385 | `	pTmp = pGen->pEnd;` |
|   23392 | 4386 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   12036 | 4387 | `		if( pGen->pIn < pNext ){` |
|   12036 | 4388 | `			pGen->pEnd = pNext;` |
|   12036 | 4389 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   12036 | 4390 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4391 | `				return SXERR_ABORT;` |
|   12036 | 4392 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 4393 | `				/* Emit the consume instruction */` |
|   12012 | 4394 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6005 | 4395 | `			}` |
|    6017 | 4396 | `		}` |
|       - | 4397 | `		/* Jump trailing commas */` |
|   12714 | 4398 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     680 | 4399 | `			pNext++;` |
|       2 | 4400 | `		}` |
|   12036 | 4401 | `		pGen->pIn = pNext;` |
|       2 | 4402 | `	}` |
|       - | 4403 | `	/* Restore token stream */` |
|   11358 | 4404 | `	pGen->pEnd = pTmp;` |
|   11358 | 4405 | `	return SXRET_OK;` |
|    5680 | 4406 |  |
|       - | 4407 | `/*` |
|       - | 4408 | ` * Compile the static statement.` |
|       - | 4409 | ` * According to the PHP language reference` |
|       - | 4410 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 4411 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 4412 | ` *  when program execution leaves this scope.` |
|       - | 4413 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 4414 | ` * Symisc eXtension.` |
|       - | 4415 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 4416 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4417 | ` *  Example` |
|       - | 4418 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 4419 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 4420 | ` */` |
|       2 | 4421 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 4422 |  |
|       - | 4423 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 4424 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 4425 | `	GenBlock *pBlock;` |
|       - | 4426 | `	SyString *pName;` |
|       - | 4427 | `	char *zDup;` |
|       - | 4428 | `	sxu32 nLine;` |
|       - | 4429 | `	sxi32 rc;` |
|       - | 4430 | `	/* Jump the static keyword */` |
|       3 | 4431 | `	nLine = pGen->pIn->nLine;` |
|       3 | 4432 | `	pGen->pIn++;` |
|       - | 4433 | `	/* Extract the enclosing function if any */` |
|       3 | 4434 | `	pBlock = pGen->pCurrent;` |
|       5 | 4435 | `	while( pBlock ){` |
|       5 | 4436 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 4437 | `			break;` |
|       - | 4438 | `		}` |
|       - | 4439 | `		/* Point to the upper block */` |
|       3 | 4440 | `		pBlock = pBlock->pParent;` |
|       1 | 4441 | `	}` |
|       3 | 4442 | `	if( pBlock == 0 ){` |
|       - | 4443 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 4444 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 4445 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 4446 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4447 | `				return SXERR_ABORT;` |
|       - | 4448 | `			}` |
|     ! 0 | 4449 | `			goto Synchronize;` |
|       - | 4450 | `		}` |
|       - | 4451 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 4452 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 4453 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4454 | `			return SXERR_ABORT;` |
|     ! 0 | 4455 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 4456 | `			/* Emit the POP instruction */` |
|     ! 0 | 4457 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 4458 | `		}` |
|     ! 0 | 4459 | `		return SXRET_OK;` |
|       - | 4460 | `	}` |
|       3 | 4461 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 4462 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 4463 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 4464 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 4465 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 4466 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4467 | `				return SXERR_ABORT;` |
|       - | 4468 | `			}` |
|       3 | 4469 | `			goto Synchronize;` |
|       - | 4470 | `	}` |
|     ! 0 | 4471 | `	pGen->pIn++;` |
|       - | 4472 | `	/* Extract variable name */` |
|     ! 0 | 4473 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 4474 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 4475 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 4476 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 4477 | `		goto Synchronize;` |
|       - | 4478 | `	}` |
|       - | 4479 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 4480 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 4481 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 4482 | `	/* Duplicate variable name */` |
|     ! 0 | 4483 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 4484 | `	if( zDup == 0 ){` |
|     ! 0 | 4485 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 4486 | `		return SXERR_ABORT;` |
|       - | 4487 | `	}` |
|     ! 0 | 4488 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 4489 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 4490 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 4491 | `		SySet *pInstrContainer;` |
|       - | 4492 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 4493 | `		 * Static variable can take any complex expression including function` |
|       - | 4494 | `		 * call as their initialization value.` |
|       - | 4495 | `		 * Example:` |
|       - | 4496 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 4497 | `		 */` |
|     ! 0 | 4498 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 4499 | `		/* Swap bytecode container */` |
|     ! 0 | 4500 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 4501 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 4502 | `		/* Compile the expression */` |
|     ! 0 | 4503 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4504 | `		/* Emit the done instruction */` |
|     ! 0 | 4505 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 4506 | `		/* Restore default bytecode container */` |
|     ! 0 | 4507 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 4508 | `	}` |
|       - | 4509 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 4510 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 4511 | `	return SXRET_OK;` |
|       1 | 4512 | `Synchronize:` |
|       - | 4513 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 4514 | `	 * statement.` |
|       - | 4515 | `	 */` |
|       5 | 4516 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 4517 | `		pGen->pIn++;` |
|       1 | 4518 | `	}` |
|       3 | 4519 | `	return SXRET_OK;` |
|       2 | 4520 |  |
|       - | 4521 | `/*` |
|       - | 4522 | ` * Compile the var statement.` |
|       - | 4523 | ` * Symisc Extension:` |
|       - | 4524 | ` *      var statement can be used outside of a class definition.` |
|       - | 4525 | ` */` |
|       4 | 4526 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 4527 |  |
|       - | 4528 | `	sxu32 nLine;` |
|       - | 4529 | `	sxi32 rc;` |
|       5 | 4530 | `	nLine = pGen->pIn->nLine;` |
|       - | 4531 | `	/* Jump the 'var' keyword */` |
|       5 | 4532 | `	pGen->pIn++;` |
|       5 | 4533 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 4534 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 4535 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 4536 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 4537 | `			pGen->pIn++;` |
|     ! 0 | 4538 | `		}` |
|     ! 0 | 4539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4540 | `			return SXERR_ABORT;` |
|       - | 4541 | `		}` |
|     ! 0 | 4542 | `	}else{` |
|       - | 4543 | `		/* Compile the expression */` |
|       5 | 4544 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 4545 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4546 | `			return SXERR_ABORT;` |
|       5 | 4547 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 4548 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 4549 | `		}` |
|       - | 4550 | `	}` |
|       5 | 4551 | `	return SXRET_OK;` |
|       3 | 4552 |  |
|       - | 4553 | `/*` |
|       - | 4554 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 4555 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 4556 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 4557 | ` */` |
|       - | 4558 | `/*` |
|       - | 4559 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 4560 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 4561 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 4562 | ` * qualified name and updates the instruction's operand index.` |
|       - | 4563 | ` *` |
|       - | 4564 | ` * Resolution order:` |
|       - | 4565 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 4566 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 4567 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 4568 | ` *` |
|       - | 4569 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 4570 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 4571 | ` * Returns the (possibly new) literal index.` |
|       - | 4572 | ` */` |
|  328550 | 4573 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 4574 |  |
|       - | 4575 | `	ph7_value *pLit;` |
|       - | 4576 | `	const char *zLit;` |
|       - | 4577 | `	SyString sQualified;` |
|       - | 4578 | `	sxu32 nLit;` |
|       - | 4579 | `	sxu32 k;` |
|       - | 4580 | `	sxu32 nNewIdx;` |
|       - | 4581 | `	int hasNsSep;` |
|       - | 4582 | `	SyHashEntry *pImport;` |
|       - | 4583 | `	ph7_value *pNew;` |
|  328552 | 4584 | `	if( pFromImport ){` |
|  314116 | 4585 | `		*pFromImport = 0;` |
|  157057 | 4586 | `	}` |
|  328552 | 4587 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  328552 | 4588 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 4589 | `		return nOrigIdx;` |
|       - | 4590 | `	}` |
|  328552 | 4591 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  328552 | 4592 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 4593 | `	/* Skip if already qualified (contains backslash) */` |
|  328552 | 4594 | `	hasNsSep = 0;` |
| 3533196 | 4595 | `	for( k = 0; k < nLit; k++ ){` |
| 3204682 | 4596 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1602324 | 4597 | `	}` |
|  328552 | 4598 | `	if( hasNsSep ){` |
|      38 | 4599 | `		return nOrigIdx;` |
|       - | 4600 | `	}` |
|       - | 4601 | `	/* Check use imports first (works even outside namespaces) */` |
|  328516 | 4602 | `	SyBlobReset(&pGen->sWorker);` |
|  328516 | 4603 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  328516 | 4604 | `	if( pImport ){` |
|      38 | 4605 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 4606 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 4607 | `		if( pFromImport ){` |
|      18 | 4608 | `			*pFromImport = 1;` |
|       8 | 4609 | `		}` |
|      20 | 4610 | `	}else{` |
|  328480 | 4611 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  328392 | 4612 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 4613 | `		}` |
|       - | 4614 | `		/* Prepend current namespace */` |
|      90 | 4615 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      90 | 4616 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      90 | 4617 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 4618 | `	}` |
|       - | 4619 | `	/* Look up or create a new literal for the qualified name */` |
|     126 | 4620 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     126 | 4621 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 | 4622 | `		return nNewIdx; /* Already interned */` |
|       - | 4623 | `	}` |
|      74 | 4624 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      74 | 4625 | `	if( pNew == 0 ){` |
|     ! 0 | 4626 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 4627 | `	}` |
|      74 | 4628 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      74 | 4629 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      74 | 4630 | `	return nNewIdx;` |
|  164277 | 4631 |  |
|       - | 4632 | `/*` |
|       - | 4633 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 4634 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 4635 | ` */` |
|   27836 | 4636 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4637 |  |
|       - | 4638 | `	SyHashEntry *pImport;` |
|       - | 4639 | `	/* Check use imports first */` |
|   27838 | 4640 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   27838 | 4641 | `	if( pImport ){` |
|      12 | 4642 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 4643 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 4644 | `		return;` |
|       - | 4645 | `	}` |
|       - | 4646 | `	/* Prepend current namespace if active */` |
|   27828 | 4647 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 4648 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 4649 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 4650 | `	}` |
|   27828 | 4651 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13920 | 4652 |  |
|       - | 4653 | `/*` |
|       - | 4654 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 4655 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 4656 | ` * The caller must release pOut when done.` |
|       - | 4657 | ` */` |
|   47392 | 4658 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4659 |  |
|   47394 | 4660 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      54 | 4661 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      54 | 4662 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 | 4663 | `	}` |
|   47394 | 4664 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   47394 | 4665 |  |
|       - | 4666 | `/*` |
|       - | 4667 | ` * Compile a namespace statement` |
|       - | 4668 | ` * According to the PHP language reference manual` |
|       - | 4669 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 4670 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 4671 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 4672 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 4673 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 4674 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 4675 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 4676 | ` *  programming world.` |
|       - | 4677 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 4678 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 4679 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 4680 | ` *  classes/functions/constants.` |
|       - | 4681 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 4682 | ` *  readability of source code.` |
|       - | 4683 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 4684 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 4685 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 4686 | ` *       class MyClass {}` |
|       - | 4687 | ` *       function myfunction() {}` |
|       - | 4688 | ` *       const MYCONST = 1;` |
|       - | 4689 | ` *       $a = new MyClass;` |
|       - | 4690 | ` *       $c = new \my\name\MyClass;` |
|       - | 4691 | ` *       $a = strlen('hi');` |
|       - | 4692 | ` *       $d = namespace\MYCONST;` |
|       - | 4693 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 4694 | ` *       echo constant($d);` |
|       - | 4695 | ` * NOTE` |
|       - | 4696 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4697 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4698 | ` */` |
|       - | 4699 | `/*` |
|       - | 4700 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 4701 | ` */` |
|      14 | 4702 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 4703 |  |
|      15 | 4704 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 | 4705 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 | 4706 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 | 4707 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 | 4708 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 | 4709 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 4710 | `	return "token";` |
|       8 | 4711 |  |
|     100 | 4712 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 4713 |  |
|       - | 4714 | `	sxu32 nLine;` |
|       - | 4715 | `	sxi32 rc;` |
|     102 | 4716 | `	nLine = pGen->pIn->nLine;` |
|     102 | 4717 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 4718 | `	/* Reset namespace and clear previous use imports */` |
|     102 | 4719 | `	SyBlobReset(&pGen->sNamespace);` |
|     102 | 4720 | `	SyHashRelease(&pGen->hUseImports);` |
|     102 | 4721 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     102 | 4722 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     102 | 4723 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     102 | 4724 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     102 | 4725 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     102 | 4726 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4727 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 4728 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4729 | `		return SXRET_OK;` |
|       - | 4730 | `	}` |
|     102 | 4731 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 4732 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 4733 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4734 | `		return SXRET_OK;` |
|       - | 4735 | `	}` |
|     102 | 4736 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 4737 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 4738 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4739 | `		return SXRET_OK;` |
|       - | 4740 | `	}` |
|       - | 4741 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     240 | 4742 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     140 | 4743 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 4744 | `			/* Append backslash separator */` |
|      21 | 4745 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 4746 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 4747 | `			}` |
|      11 | 4748 | `		}else{` |
|       - | 4749 | `			/* Append identifier */` |
|     120 | 4750 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 4751 | `		}` |
|     140 | 4752 | `		pGen->pIn++;` |
|       2 | 4753 | `	}` |
|       - | 4754 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 4755 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 4756 | `	{` |
|     102 | 4757 | `		char *zNsDup = 0;` |
|     102 | 4758 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     149 | 4759 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      98 | 4760 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      49 | 4761 | `		}` |
|     102 | 4762 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 4763 | `	}` |
|     102 | 4764 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 4765 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 4766 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 4767 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 4768 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4769 | `			return SXERR_ABORT;` |
|       - | 4770 | `		}` |
|       2 | 4771 | `	}` |
|     102 | 4772 | `	return SXRET_OK;` |
|      52 | 4773 |  |
|       - | 4774 | `/*` |
|       - | 4775 | ` * Compile the 'use' statement` |
|       - | 4776 | ` * According to the PHP language reference manual` |
|       - | 4777 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 4778 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 4779 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 4780 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 4781 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 4782 | ` *  a function or constant is not supported.` |
|       - | 4783 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 4784 | ` * NOTE` |
|       - | 4785 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4786 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4787 | ` */` |
|      66 | 4788 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 4789 |  |
|       - | 4790 | `	sxu32 nLine;` |
|       - | 4791 | `	sxi32 rc;` |
|       - | 4792 | `	SyBlob sPath;` |
|       - | 4793 | `	SyString sAlias;` |
|       - | 4794 | `	SyToken *pLast;` |
|       - | 4795 | `	char *zDup;` |
|       - | 4796 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 4797 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 4798 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 4799 | `	nLine = pGen->pIn->nLine;` |
|      68 | 4800 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 4801 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 4802 | `	iUseType = 0;` |
|      68 | 4803 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 4804 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 4805 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 4806 | `			iUseType = 1;` |
|      16 | 4807 | `			pGen->pIn++;` |
|      23 | 4808 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 4809 | `			iUseType = 2;` |
|      16 | 4810 | `			pGen->pIn++;` |
|       7 | 4811 | `		}` |
|      14 | 4812 | `	}` |
|       - | 4813 | `	/* Select target hash tables based on import type */` |
|      68 | 4814 | `	switch( iUseType ){` |
|       7 | 4815 | `		case 1:` |
|      16 | 4816 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 4817 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 4818 | `			break;` |
|       7 | 4819 | `		case 2:` |
|      16 | 4820 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 4821 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 4822 | `			break;` |
|      19 | 4823 | `		default:` |
|      40 | 4824 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 4825 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 4826 | `			break;` |
|       - | 4827 | `	}` |
|      68 | 4828 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 4829 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 4830 | `	for(;;){` |
|      70 | 4831 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 4832 | `			break;` |
|       - | 4833 | `		}` |
|      70 | 4834 | `		SyBlobReset(&sPath);` |
|      70 | 4835 | `		pLast = 0;` |
|       - | 4836 | `		/* Collect the full namespace path */` |
|     254 | 4837 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 4838 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 4839 | `				pLast = pGen->pIn;` |
|     126 | 4840 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 4841 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 4842 | `				}` |
|     126 | 4843 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 4844 | `			}` |
|     186 | 4845 | `			pGen->pIn++;` |
|       2 | 4846 | `		}` |
|      70 | 4847 | `		if( pLast == 0 ){` |
|       - | 4848 | `			/* Empty path */` |
|       5 | 4849 | `			break;` |
|       - | 4850 | `		}` |
|       - | 4851 | `		/* Default alias is the last component of the path */` |
|      66 | 4852 | `		sAlias = pLast->sData;` |
|       - | 4853 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 4854 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 4855 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 4856 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 4857 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 4858 | `				sAlias = pGen->pIn->sData;` |
|      18 | 4859 | `				pGen->pIn++;` |
|       8 | 4860 | `			}` |
|       8 | 4861 | `		}` |
|       - | 4862 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4863 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4864 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4865 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4866 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4867 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4868 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4869 | `				return SXERR_ABORT;` |
|       - | 4870 | `			}` |
|       2 | 4871 | `		}` |
|       - | 4872 | `		/* Register the import: alias -> FQN.` |
|       - | 4873 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4874 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4875 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4876 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4877 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4878 | `		if( zDup ){` |
|      66 | 4879 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4880 | `			if( pVmHash ){` |
|       - | 4881 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4882 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4883 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4884 | `				if( zAliasDup ){` |
|      38 | 4885 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4886 | `				}` |
|      18 | 4887 | `			}` |
|      66 | 4888 | `			if( iUseType == 2 ){` |
|       - | 4889 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4890 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4891 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4892 | `				if( zAliasDup ){` |
|       - | 4893 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4894 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4895 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4896 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4897 | `					if( azPair ){` |
|      16 | 4898 | `						azPair[0] = zAliasDup;` |
|      16 | 4899 | `						azPair[1] = zDup;` |
|      16 | 4900 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4901 | `					}` |
|       7 | 4902 | `				}` |
|       7 | 4903 | `			}` |
|      32 | 4904 | `		}` |
|       - | 4905 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4906 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4907 | `			pGen->pIn++;` |
|       2 | 4908 | `		}else{` |
|      33 | 4909 | `			break;` |
|       - | 4910 | `		}` |
|       1 | 4911 | `	}` |
|      68 | 4912 | `	SyBlobRelease(&sPath);` |
|      68 | 4913 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4914 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4915 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4916 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4917 | `			return SXERR_ABORT;` |
|       - | 4918 | `		}` |
|       1 | 4919 | `	}` |
|      68 | 4920 | `	return SXRET_OK;` |
|      35 | 4921 |  |
|       - | 4922 | `/*` |
|       - | 4923 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4924 | ` *` |
|       - | 4925 | ` * According to the PHP language reference manual.` |
|       - | 4926 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4927 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4928 | ` *  declare (directive)` |
|       - | 4929 | ` *   statement` |
|       - | 4930 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4931 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4932 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4933 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4934 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4935 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4936 | ` * <?php` |
|       - | 4937 | ` * // these are the same:` |
|       - | 4938 | ` * // you can use this:` |
|       - | 4939 | ` * declare(ticks=1) {` |
|       - | 4940 | ` *   // entire script here` |
|       - | 4941 | ` * }` |
|       - | 4942 | ` * // or you can use this:` |
|       - | 4943 | ` * declare(ticks=1);` |
|       - | 4944 | ` * // entire script here` |
|       - | 4945 | ` * ?>` |
|       - | 4946 | ` *` |
|       - | 4947 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4948 | ` */` |
|       8 | 4949 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4950 |  |
|       9 | 4951 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4952 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4953 | `	sxi32 rc;` |
|       9 | 4954 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4955 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4956 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4957 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4958 | `			return SXERR_ABORT;` |
|       - | 4959 | `		}` |
|       5 | 4960 | `		goto Synchro;` |
|       - | 4961 | `	}` |
|       5 | 4962 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4963 | `	/* Delimit the directive */` |
|       5 | 4964 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4965 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4966 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4967 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4968 | `			return SXERR_ABORT;` |
|       - | 4969 | `		}` |
|     ! 0 | 4970 | `		return SXRET_OK;` |
|       - | 4971 | `	}` |
|       - | 4972 | `	/* Update the cursor */` |
|       5 | 4973 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4974 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4975 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4976 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4977 | `			return SXERR_ABORT;` |
|       - | 4978 | `		}` |
|     ! 0 | 4979 | `	}` |
|       - | 4980 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4981 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4982 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4983 | `		ph7_lib_version()` |
|       - | 4984 | `		);` |
|       - | 4985 | `	/*All done */` |
|       5 | 4986 | `	return SXRET_OK;` |
|       2 | 4987 | `Synchro:` |
|       - | 4988 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4989 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4990 | `		pGen->pIn++;` |
|       1 | 4991 | `	}` |
|       5 | 4992 | `	return SXRET_OK;` |
|       5 | 4993 |  |
|       - | 4994 | `/*` |
|       - | 4995 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4996 | ` * as follows:` |
|       - | 4997 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4998 | ` * {` |
|       - | 4999 | ` *   return "Making a cup of $type.\n";` |
|       - | 5000 | ` * }` |
|       - | 5001 | ` * Symisc eXtension.` |
|       - | 5002 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 5003 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 5004 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 5005 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 5006 | ` *      {` |
|       - | 5007 | ` *       var_dump($a);` |
|       - | 5008 | ` *      }` |
|       - | 5009 | ` *     //call test without args` |
|       - | 5010 | ` *      test();` |
|       - | 5011 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 5012 | ` *      Example:` |
|       - | 5013 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 5014 | ` * 3 -) Function overloading!!` |
|       - | 5015 | ` *      Example:` |
|       - | 5016 | ` *      function foo($a) {` |
|       - | 5017 | ` *   	  return $a.PHP_EOL;` |
|       - | 5018 | ` *	    }` |
|       - | 5019 | ` *	    function foo($a, $b) {` |
|       - | 5020 | ` *   	  return $a + $b;` |
|       - | 5021 | ` *	    }` |
|       - | 5022 | ` *	    echo foo(5); // Prints "5"` |
|       - | 5023 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 5024 | ` *      // Same arg` |
|       - | 5025 | ` *	   function foo(string $a)` |
|       - | 5026 | ` *	   {` |
|       - | 5027 | ` *	     echo "a is a string\n";` |
|       - | 5028 | ` *	     var_dump($a);` |
|       - | 5029 | ` *	   }` |
|       - | 5030 | ` *	  function foo(int $a)` |
|       - | 5031 | ` *	  {` |
|       - | 5032 | ` *	    echo "a is integer\n";` |
|       - | 5033 | ` *	    var_dump($a);` |
|       - | 5034 | ` *	  }` |
|       - | 5035 | ` *	  function foo(array $a)` |
|       - | 5036 | ` *	  {` |
|       - | 5037 | ` * 	    echo "a is an array\n";` |
|       - | 5038 | ` * 	    var_dump($a);` |
|       - | 5039 | ` *	  }` |
|       - | 5040 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 5041 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 5042 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 5043 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 5044 | ` * introduced by the PH7 engine.` |
|       - | 5045 | ` */` |
|   43988 | 5046 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 5047 |  |
|       - | 5048 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 5049 | `	SySet *pInstrContainer;` |
|       - | 5050 | `	sxi32 rc;` |
|       - | 5051 | `	/* Swap token stream */` |
|   43990 | 5052 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   43990 | 5053 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   43990 | 5054 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 5055 | `	/* Compile the expression holding the argument value */` |
|   43990 | 5056 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 5057 | `	/* Emit the done instruction */` |
|   43990 | 5058 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   43990 | 5059 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   43990 | 5060 | `	RE_SWAP_DELIMITER(pGen);` |
|   43990 | 5061 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5062 | `		return SXERR_ABORT;` |
|       - | 5063 | `	}` |
|   43990 | 5064 | `	return SXRET_OK;` |
|   21996 | 5065 |  |
|       - | 5066 | `/*` |
|       - | 5067 | ` * Collect function arguments one after one.` |
|       - | 5068 | ` * According to the PHP language reference manual.` |
|       - | 5069 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 5070 | ` * list of expressions.` |
|       - | 5071 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 5072 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 5073 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 5074 | ` * for more information.` |
|       - | 5075 | ` * Example #1 Passing arrays to functions` |
|       - | 5076 | ` * <?php` |
|       - | 5077 | ` * function takes_array($input)` |
|       - | 5078 | ` * {` |
|       - | 5079 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 5080 | ` * }` |
|       - | 5081 | ` * ?>` |
|       - | 5082 | ` * Making arguments be passed by reference` |
|       - | 5083 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 5084 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 5085 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 5086 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 5087 | ` * to the argument name in the function definition:` |
|       - | 5088 | ` * Example #2 Passing function parameters by reference` |
|       - | 5089 | ` * <?php` |
|       - | 5090 | ` * function add_some_extra(&$string)` |
|       - | 5091 | ` * {` |
|       - | 5092 | ` *   $string .= 'and something extra.';` |
|       - | 5093 | ` * }` |
|       - | 5094 | ` * $str = 'This is a string, ';` |
|       - | 5095 | ` * add_some_extra($str);` |
|       - | 5096 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 5097 | ` * ?>` |
|       - | 5098 | ` *` |
|       - | 5099 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 5100 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 5101 | ` * on these extension.` |
|       - | 5102 | ` */` |
|   52876 | 5103 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 5104 |  |
|       - | 5105 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 5106 | `	SyToken *pIn;  /* Token stream */` |
|       - | 5107 | `	SyBlob sSig;         /* Function signature */` |
|       - | 5108 | `	char *zDup;          /* Copy of argument name */` |
|       - | 5109 | `	sxi32 rc;` |
|       - | 5110 |  |
|   52878 | 5111 | `	pIn = pGen->pIn;` |
|   52878 | 5112 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 5113 | `	/* Process arguments one after one */` |
|   66881 | 5114 | `	for(;;){` |
|  133764 | 5115 | `		if( pIn >= pEnd ){` |
|       - | 5116 | `			/* No more arguments to process */` |
|   52876 | 5117 | `			break;` |
|       - | 5118 | `		}` |
|   80890 | 5119 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   80890 | 5120 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 5121 | `		/* Detect nullable prefix '?' on type hints */` |
|   80890 | 5122 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 5123 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      16 | 5124 | `			pIn++;` |
|       7 | 5125 | `		}` |
|       - | 5126 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   80890 | 5127 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 5128 | `			pIn++;` |
|       2 | 5129 | `		}` |
|   80890 | 5130 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   55022 | 5131 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   49518 | 5132 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   49518 | 5133 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 5134 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   49518 | 5135 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 5136 | `					sArg.nType = MEMOBJ_BOOL;` |
|   49518 | 5137 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13758 | 5138 | `					sArg.nType = MEMOBJ_INT;` |
|   42640 | 5139 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   35746 | 5140 | `					sArg.nType = MEMOBJ_STRING;` |
|   17890 | 5141 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 5142 | `					sArg.nType = MEMOBJ_REAL;` |
|      18 | 5143 | `				}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      16 | 5144 | `					sArg.nType = MEMOBJ_OBJ;` |
|       9 | 5145 | `				}else{` |
|       4 | 5146 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 5147 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 5148 | `						&pIn->sData);` |
|       - | 5149 | `				}` |
|   24760 | 5150 | `			}else{` |
|    5506 | 5151 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 5152 | `				char *zDupLocal;` |
|       - | 5153 | `				/* Argument must be a class instance,record that*/` |
|    5506 | 5154 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5506 | 5155 | `				if( zDupLocal ){` |
|    5506 | 5156 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5506 | 5157 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2752 | 5158 | `				}` |
|       - | 5159 | `			}` |
|   55022 | 5160 | `			pIn++;` |
|   27510 | 5161 | `		}` |
|   80890 | 5162 | `		if( pIn >= pEnd ){` |
|     ! 0 | 5163 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 5164 | `			return rc;` |
|       - | 5165 | `		}` |
|   80890 | 5166 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 5167 | `			/* Pass by reference,record that */` |
|    2774 | 5168 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2774 | 5169 | `			pIn++;` |
|    1386 | 5170 | `		}` |
|   80890 | 5171 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 5172 | `			/* Variadic parameter: ...$args */` |
|      30 | 5173 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      30 | 5174 | `			pIn++;` |
|      14 | 5175 | `		}` |
|   80890 | 5176 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5177 | `			/* Invalid argument */` |
|     ! 0 | 5178 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 5179 | `			return rc;` |
|       - | 5180 | `		}` |
|   80890 | 5181 | `		pIn++; /* Jump the dollar sign */` |
|       - | 5182 | `		/* Copy argument name */` |
|   80890 | 5183 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   80890 | 5184 | `		if( zDup == 0 ){` |
|     ! 0 | 5185 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 5186 | `			return SXERR_ABORT;` |
|       - | 5187 | `		}` |
|   80890 | 5188 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   80890 | 5189 | `		pIn++;` |
|   80890 | 5190 | `		if( pIn < pEnd ){` |
|   50022 | 5191 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 5192 | `				SyToken *pDefend;` |
|   43992 | 5193 | `				sxi32 iNest = 0;` |
|   43992 | 5194 | `				pIn++; /* Jump the equal sign */` |
|   43992 | 5195 | `				pDefend = pIn;` |
|       - | 5196 | `				/* Process the default value associated with this argument */` |
|   93476 | 5197 | `				while( pDefend < pEnd ){` |
|   71472 | 5198 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   21988 | 5199 | `						break;` |
|       - | 5200 | `					}` |
|   49486 | 5201 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 5202 | `						/* Increment nesting level */` |
|    2750 | 5203 | `						iNest++;` |
|   48112 | 5204 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 5205 | `						/* Decrement nesting level */` |
|    2750 | 5206 | `						iNest--;` |
|    1374 | 5207 | `					}` |
|   49486 | 5208 | `					pDefend++;` |
|       2 | 5209 | `				}` |
|   43992 | 5210 | `				if( pIn >= pDefend ){` |
|       3 | 5211 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 5212 | `					return rc;` |
|       - | 5213 | `				}` |
|       - | 5214 | `				/* Process default value */` |
|   43990 | 5215 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   43990 | 5216 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5217 | `					return rc;` |
|       - | 5218 | `				}` |
|       - | 5219 | `				/* Point beyond the default value */` |
|   43990 | 5220 | `				pIn = pDefend;` |
|   21994 | 5221 | `			}` |
|   50020 | 5222 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 5223 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 5224 | `				return rc;` |
|       - | 5225 | `			}` |
|   50020 | 5226 | `			pIn++; /* Jump the trailing comma */` |
|   25009 | 5227 | `		}` |
|       - | 5228 | `		/* Append argument signature */` |
|   80888 | 5229 | `		if( sArg.nType > 0 ){` |
|   55020 | 5230 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 5231 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5506 | 5232 | `				int marker = 'o';` |
|    5506 | 5233 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5506 | 5234 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2754 | 5235 | `			}else{` |
|       - | 5236 | `				int c;` |
|   49516 | 5237 | `				c = 'n'; /* cc warning */` |
|       - | 5238 | `				/* Type leading character */` |
|   49516 | 5239 | `				switch(sArg.nType){` |
|     ! 0 | 5240 | `				case MEMOBJ_HASHMAP:` |
|       - | 5241 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 5242 | `					c = 'h';` |
|     ! 0 | 5243 | `					break;` |
|    6878 | 5244 | `				case MEMOBJ_INT:` |
|       - | 5245 | `					/* Integer */` |
|   13758 | 5246 | `					c = 'i';` |
|   13758 | 5247 | `					break;` |
|     ! 0 | 5248 | `				case MEMOBJ_BOOL:` |
|       - | 5249 | `					/* Bool */` |
|     ! 0 | 5250 | `					c = 'b';` |
|     ! 0 | 5251 | `					break;` |
|     ! 0 | 5252 | `				case MEMOBJ_REAL:` |
|       - | 5253 | `					/* Float */` |
|     ! 0 | 5254 | `					c = 'f';` |
|     ! 0 | 5255 | `					break;` |
|   17872 | 5256 | `				case MEMOBJ_STRING:` |
|       - | 5257 | `					/* String */` |
|   35746 | 5258 | `					c = 's';` |
|   35746 | 5259 | `					break;` |
|       7 | 5260 | `				case MEMOBJ_OBJ:` |
|       - | 5261 | `					/* Object */` |
|      16 | 5262 | `					c = 'o';` |
|      14 | 5263 | `					break;` |
|     ! 0 | 5264 | `				default:` |
|     ! 0 | 5265 | `					break;` |
|       - | 5266 | `				}` |
|   49516 | 5267 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 5268 | `			}` |
|   27511 | 5269 | `		}else{` |
|       - | 5270 | `			/* No type is associated with this parameter which mean` |
|       - | 5271 | `			 * that this function is not condidate for overloading.` |
|       - | 5272 | `			 */` |
|   25870 | 5273 | `			SyBlobRelease(&sSig);` |
|       - | 5274 | `		}` |
|       - | 5275 | `		/* Save in the argument set */` |
|   80888 | 5276 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 5277 | `	}` |
|   52876 | 5278 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 5279 | `		/* Save function signature */` |
|   33030 | 5280 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16514 | 5281 | `	}` |
|   52876 | 5282 | `	return SXRET_OK;` |
|   26440 | 5283 |  |
|       - | 5284 | `/*` |
|       - | 5285 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 5286 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 5287 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 5288 | ` */` |
|  146798 | 5289 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 5290 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 5291 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 5292 | `	)` |
|       2 | 5293 |  |
|       - | 5294 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 5295 | `	GenBlock *pBlock;` |
|       - | 5296 | `	sxu32 nGotoOfft;` |
|       - | 5297 | `	sxi32 rc;` |
|       - | 5298 | `	/* Attach the new function */` |
|  146800 | 5299 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  146800 | 5300 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5301 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 5302 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5303 | `		return SXERR_ABORT;` |
|       - | 5304 | `	}` |
|  146800 | 5305 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 5306 | `	/* Swap bytecode containers */` |
|  146800 | 5307 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  146800 | 5308 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 5309 | `	/* Compile the body */` |
|  146800 | 5310 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 5311 | `	/* Fix exception jumps now the destination is resolved */` |
|  146800 | 5312 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 5313 | `	/* Emit the final return if not yet done */` |
|  146800 | 5314 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 5315 | `	/* Fix gotos jumps now the destination is resolved */` |
|  146800 | 5316 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 5317 | `		rc = SXERR_ABORT;` |
|     ! 0 | 5318 | `	}` |
|  146800 | 5319 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 5320 | `	/* Restore the default container */` |
|  146800 | 5321 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 5322 | `	/* Leave function block */` |
|  146800 | 5323 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  146800 | 5324 | `	if( rc == SXERR_ABORT ){` |
|       - | 5325 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5326 | `		return SXERR_ABORT;` |
|       - | 5327 | `	}` |
|       - | 5328 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 5329 | `	{` |
|  146800 | 5330 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 5331 | `		sxu32 i;` |
| 3047830 | 5332 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2901048 | 5333 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 5334 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 5335 | `				break;` |
|       - | 5336 | `			}` |
| 1450517 | 5337 | `		}` |
|       - | 5338 | `	}` |
|       - | 5339 | `	/* All done, function body compiled */` |
|  146800 | 5340 | `	return SXRET_OK;` |
|   73401 | 5341 |  |
|       - | 5342 | `/*` |
|       - | 5343 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 5344 | ` * According to the PHP language reference manual.` |
|       - | 5345 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 5346 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 5347 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 5348 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5349 | ` *  Functions need not be defined before they are referenced.` |
|       - | 5350 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 5351 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 5352 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 5353 | ` *  calls with over 32-64 recursion levels.` |
|       - | 5354 | ` *` |
|       - | 5355 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 5356 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 5357 | ` * on these extension.` |
|       - | 5358 | ` */` |
|       - | 5359 | `/*` |
|       - | 5360 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 5361 | ` */` |
|      14 | 5362 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 | 5363 |  |
|       - | 5364 | `	sxu32 i;` |
|      48 | 5365 | `	for( i = 0; i < n; i++ ){` |
|      40 | 5366 | `		int a = zA[i], b = zB[i];` |
|      40 | 5367 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      40 | 5368 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      40 | 5369 | `		if( a != b ) return a - b;` |
|      18 | 5370 | `	}` |
|      10 | 5371 | `	return 0;` |
|       9 | 5372 |  |
|       - | 5373 | `/*` |
|       - | 5374 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 5375 | ` */` |
|       2 | 5376 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 5377 |  |
|       3 | 5378 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 5379 | `	if( zDup ){` |
|       3 | 5380 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 5381 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 5382 | `	}` |
|       3 | 5383 |  |
|       - | 5384 | `/*` |
|       - | 5385 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 5386 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 5387 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 5388 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 5389 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 5390 | ` */` |
|  168910 | 5391 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 5392 |  |
|  168912 | 5393 | `	SyToken *pCur = pGen->pIn;` |
|  168912 | 5394 | `	pFunc->nReturnType = 0;` |
|  168912 | 5395 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  168912 | 5396 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  168834 | 5397 | `		return; /* No return type */` |
|       - | 5398 | `	}` |
|      80 | 5399 | `	pCur++; /* Skip ':' */` |
|      80 | 5400 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 5401 | `		pGen->pIn = pCur;` |
|     ! 0 | 5402 | `		return;` |
|       - | 5403 | `	}` |
|       - | 5404 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      80 | 5405 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|      10 | 5406 | `		pCur++;` |
|      10 | 5407 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 5408 | `			pGen->pIn = pCur;` |
|     ! 0 | 5409 | `			return;` |
|       - | 5410 | `		}` |
|       4 | 5411 | `	}` |
|      80 | 5412 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      72 | 5413 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      72 | 5414 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 5415 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      71 | 5416 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 5417 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      69 | 5418 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      28 | 5419 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      55 | 5420 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      36 | 5421 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      25 | 5422 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 5423 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       7 | 5424 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       3 | 5425 | `			pFunc->nReturnType = MEMOBJ_OBJ;` |
|       4 | 5426 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 5427 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 5428 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 5429 | `		}` |
|      72 | 5430 | `		pCur++;` |
|      45 | 5431 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|      10 | 5432 | `		SyString *pType = &pCur->sData;` |
|      10 | 5433 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|      10 | 5434 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       6 | 5435 | `		}else{` |
|       - | 5436 | `			/* Class/interface name */` |
|     ! 0 | 5437 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 5438 | `		}` |
|      10 | 5439 | `		pCur++;` |
|       4 | 5440 | `	}` |
|      80 | 5441 | `	pGen->pIn = pCur;` |
|   84457 | 5442 |  |
|       - | 5443 |  |
|   36436 | 5444 | `static sxi32 GenStateCompileFunc(` |
|       - | 5445 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5446 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 5447 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 5448 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 5449 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 5450 | `	)` |
|       2 | 5451 |  |
|       - | 5452 | `	ph7_vm_func *pFunc;` |
|       - | 5453 | `	SyToken *pEnd;` |
|       - | 5454 | `	sxu32 nLine;` |
|       - | 5455 | `	char *zName;` |
|       - | 5456 | `	sxi32 rc;` |
|       - | 5457 | `	/* Extract line number */` |
|   36438 | 5458 | `	nLine = pGen->pIn->nLine;` |
|       - | 5459 | `	/* Jump the left parenthesis '(' */` |
|   36438 | 5460 | `	pGen->pIn++;` |
|       - | 5461 | `	/* Delimit the function signature */` |
|   36438 | 5462 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   36438 | 5463 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5464 | `		/* Syntax error */` |
|       7 | 5465 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 5466 | `		if( rc == SXERR_ABORT ){` |
|       - | 5467 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5468 | `			return SXERR_ABORT;` |
|       - | 5469 | `		}` |
|       7 | 5470 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 5471 | `		return SXRET_OK;` |
|       - | 5472 | `	}` |
|       - | 5473 | `	/* Create the function state */` |
|   36432 | 5474 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   36432 | 5475 | `	if( pFunc == 0 ){` |
|     ! 0 | 5476 | `		goto OutOfMem;` |
|       - | 5477 | `	}` |
|       - | 5478 | `	/* Build the function name, prepending namespace if active */` |
|   36439 | 5479 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 5480 | `		SyBlob sFQN;` |
|       - | 5481 | `		sxu32 nLen;` |
|      16 | 5482 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 5483 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 5484 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 5485 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 5486 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 5487 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 5488 | `		SyBlobRelease(&sFQN);` |
|      16 | 5489 | `		if( zName == 0 ){` |
|     ! 0 | 5490 | `			goto OutOfMem;` |
|       - | 5491 | `		}` |
|      16 | 5492 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 5493 | `	}else{` |
|   36418 | 5494 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   36418 | 5495 | `		if( zName == 0 ){` |
|     ! 0 | 5496 | `			goto OutOfMem;` |
|       - | 5497 | `		}` |
|   36418 | 5498 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 5499 | `	}` |
|   36432 | 5500 | `	if( pGen->pIn < pEnd ){` |
|       - | 5501 | `		/* Collect function arguments */` |
|   25254 | 5502 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   25254 | 5503 | `		if( rc == SXERR_ABORT ){` |
|       - | 5504 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5505 | `			return SXERR_ABORT;` |
|       - | 5506 | `		}` |
|   12626 | 5507 | `	}` |
|       - | 5508 | `	/* Point past ')' and parse optional return type ': type' */` |
|   36432 | 5509 | `	pGen->pIn = &pEnd[1];` |
|   36432 | 5510 | `	GenStateParseReturnType(pGen, pFunc);` |
|   36432 | 5511 | `	if( bHandleClosure ){` |
|       - | 5512 | `		ph7_vm_func_closure_env sEnv;` |
|     170 | 5513 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     168 | 5514 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      93 | 5515 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 | 5516 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 5517 | `				/* Closure,record environment variable */` |
|      16 | 5518 | `				pGen->pIn++;` |
|      16 | 5519 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 5520 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 5521 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5522 | `						return SXERR_ABORT;` |
|       - | 5523 | `					}` |
|     ! 0 | 5524 | `				}` |
|      16 | 5525 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 5526 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 | 5527 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 | 5528 | `					int iFlagsLocal = 0;` |
|      34 | 5529 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 | 5530 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 | 5531 | `						break;` |
|       - | 5532 | `					}` |
|      20 | 5533 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 | 5534 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 5535 | `						/* Pass by reference,record that */` |
|     ! 0 | 5536 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 5537 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 5538 | `							);` |
|     ! 0 | 5539 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 5540 | `						pGen->pIn++;` |
|     ! 0 | 5541 | `					}` |
|      18 | 5542 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 | 5543 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 5544 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 5545 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 5546 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 5547 | `								return SXERR_ABORT;` |
|       - | 5548 | `							}` |
|       - | 5549 | `							/* Find the closing parenthesis */` |
|     ! 0 | 5550 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 5551 | `								pGen->pIn++;` |
|     ! 0 | 5552 | `							}` |
|     ! 0 | 5553 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 5554 | `								pGen->pIn++;` |
|     ! 0 | 5555 | `							}` |
|     ! 0 | 5556 | `							break;` |
|       - | 5557 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 5558 | `					}else{` |
|       - | 5559 | `						SyString *pNameLocal;` |
|       - | 5560 | `						char *zDup;` |
|       - | 5561 | `						/* Duplicate variable name */` |
|      20 | 5562 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 | 5563 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 | 5564 | `						if( zDup ){` |
|       - | 5565 | `							/* Zero the structure */` |
|      20 | 5566 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 | 5567 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 | 5568 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 | 5569 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 | 5570 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 5571 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 5572 | `									got_this = 1;` |
|     ! 0 | 5573 | `							}` |
|       - | 5574 | `							/* Save imported variable */` |
|      20 | 5575 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 | 5576 | `						}else{` |
|     ! 0 | 5577 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5578 | `							 return SXERR_ABORT;` |
|       - | 5579 | `						}` |
|       - | 5580 | `					}` |
|      20 | 5581 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 | 5582 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5583 | `						/* Ignore trailing commas */` |
|       7 | 5584 | `						pGen->pIn++;` |
|       1 | 5585 | `					}` |
|       2 | 5586 | `				}` |
|      16 | 5587 | `				if( !got_this ){` |
|       - | 5588 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 5589 | `					 * available to the closure environment.` |
|       - | 5590 | `					 */` |
|      16 | 5591 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 5592 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 | 5593 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 5594 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 | 5595 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 | 5596 | `				}` |
|      16 | 5597 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 5598 | `					/* Mark as closure */` |
|      16 | 5599 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 | 5600 | `				}` |
|       7 | 5601 | `		}` |
|      84 | 5602 | `	}` |
|       - | 5603 | `	/* Compile the body */` |
|   36432 | 5604 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   36432 | 5605 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5606 | `		return SXERR_ABORT;` |
|       - | 5607 | `	}` |
|   36432 | 5608 | `	if( ppFunc ){` |
|     170 | 5609 | `		*ppFunc = pFunc;` |
|      84 | 5610 | `	}` |
|   36432 | 5611 | `	rc = SXRET_OK;` |
|   36432 | 5612 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 5613 | `		/* Finally register the function */` |
|   36418 | 5614 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   18208 | 5615 | `	}` |
|   36432 | 5616 | `	if( rc == SXRET_OK ){` |
|   36432 | 5617 | `		return SXRET_OK;` |
|       - | 5618 | `	}` |
|       - | 5619 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 5620 | `OutOfMem:` |
|       - | 5621 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 5622 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 5623 | `	 */` |
|     ! 0 | 5624 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 5625 | `	return SXERR_ABORT;` |
|   18220 | 5626 |  |
|       - | 5627 | `/*` |
|       - | 5628 | ` * Compile a standard PHP function.` |
|       - | 5629 | ` *  Refer to the block-comment above for more information.` |
|       - | 5630 | ` */` |
|   36274 | 5631 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 5632 |  |
|       - | 5633 | `	SyString *pName;` |
|       - | 5634 | `	sxi32 iFlags;` |
|       - | 5635 | `	sxu32 nLine;` |
|       - | 5636 | `	sxi32 rc;` |
|       - | 5637 |  |
|   36276 | 5638 | `	nLine = pGen->pIn->nLine;` |
|   36276 | 5639 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   36276 | 5640 | `	iFlags = 0;` |
|   36276 | 5641 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5642 | `		/* Return by reference,remember that */` |
|       7 | 5643 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5644 | `		/* Jump the '&' token */` |
|       7 | 5645 | `		pGen->pIn++;` |
|       3 | 5646 | `	}` |
|   36276 | 5647 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5648 | `		/* Invalid function name */` |
|       5 | 5649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 5650 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5651 | `			return SXERR_ABORT;` |
|       - | 5652 | `		}` |
|       - | 5653 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 5654 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 5655 | `			pGen->pIn++;` |
|       1 | 5656 | `		}` |
|       5 | 5657 | `		return SXRET_OK;` |
|       - | 5658 | `	}` |
|   36272 | 5659 | `	pName = &pGen->pIn->sData;` |
|   36272 | 5660 | `	nLine = pGen->pIn->nLine;` |
|       - | 5661 | `	/* Jump the function name */` |
|   36272 | 5662 | `	pGen->pIn++;` |
|   36272 | 5663 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5664 | `		/* Syntax error */` |
|       3 | 5665 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 5666 | `		if( rc == SXERR_ABORT ){` |
|       - | 5667 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5668 | `			return SXERR_ABORT;` |
|       - | 5669 | `		}` |
|       - | 5670 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 5671 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 5672 | `			pGen->pIn++;` |
|     ! 0 | 5673 | `		}` |
|       3 | 5674 | `		return SXRET_OK;` |
|       - | 5675 | `	}` |
|       - | 5676 | `	/* Compile function body */` |
|   36270 | 5677 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   36270 | 5678 | `	return rc;` |
|   18139 | 5679 |  |
|       - | 5680 | `/*` |
|       - | 5681 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 5682 | ` * According to the PHP language reference manual` |
|       - | 5683 | ` *  Visibility:` |
|       - | 5684 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 5685 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 5686 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 5687 | ` *  Members declared protected can be accessed only within the class` |
|       - | 5688 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 5689 | ` *  may only be accessed by the class that defines the member.` |
|       - | 5690 | ` */` |
|  168476 | 5691 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 5692 |  |
|  168478 | 5693 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8314 | 5694 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  160166 | 5695 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   19280 | 5696 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 5697 | `	}` |
|       - | 5698 | `	/* Assume public by default */` |
|  140888 | 5699 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   84240 | 5700 |  |
|       - | 5701 | `/*` |
|       - | 5702 | ` * Compile a class constant.` |
|       - | 5703 | ` * According to the PHP language reference manual` |
|       - | 5704 | ` *  Class Constants` |
|       - | 5705 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 5706 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 5707 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 5708 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 5709 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 5710 | ` *   It's also possible for interfaces to have constants.` |
|       - | 5711 | ` * Symisc eXtension.` |
|       - | 5712 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 5713 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5714 | ` *  Example:` |
|       - | 5715 | ` *   class Test{` |
|       - | 5716 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5717 | ` *   };` |
|       - | 5718 | ` *   var_dump(TEST::MyConst);` |
|       - | 5719 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5720 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5721 | ` */` |
|      30 | 5722 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5723 |  |
|      32 | 5724 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5725 | `	SySet *pInstrContainer;` |
|       - | 5726 | `	ph7_class_attr *pCons;` |
|       - | 5727 | `	SyString *pName;` |
|       - | 5728 | `	sxi32 rc;` |
|       - | 5729 | `	/* Extract visibility level */` |
|      32 | 5730 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 5731 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 5732 | `loop:` |
|       - | 5733 | `	/* Mark as constant */` |
|      32 | 5734 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 5735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5736 | `		/* Invalid constant name */` |
|     ! 0 | 5737 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 5738 | `		if( rc == SXERR_ABORT ){` |
|       - | 5739 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5740 | `			return SXERR_ABORT;` |
|       - | 5741 | `		}` |
|     ! 0 | 5742 | `		goto Synchronize;` |
|       - | 5743 | `	}` |
|       - | 5744 | `	/* Peek constant name */` |
|      32 | 5745 | `	pName = &pGen->pIn->sData;` |
|       - | 5746 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 5747 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 5748 | `		/* Reserved constant name */` |
|     ! 0 | 5749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 5750 | `		if( rc == SXERR_ABORT ){` |
|       - | 5751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5752 | `			return SXERR_ABORT;` |
|       - | 5753 | `		}` |
|     ! 0 | 5754 | `		goto Synchronize;` |
|       - | 5755 | `	}` |
|       - | 5756 | `	/* Advance the stream cursor */` |
|      32 | 5757 | `	pGen->pIn++;` |
|      32 | 5758 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 5759 | `		/* Invalid declaration */` |
|     ! 0 | 5760 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 5761 | `		if( rc == SXERR_ABORT ){` |
|       - | 5762 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5763 | `			return SXERR_ABORT;` |
|       - | 5764 | `		}` |
|     ! 0 | 5765 | `		goto Synchronize;` |
|       - | 5766 | `	}` |
|      32 | 5767 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 5768 | `	/* Allocate a new class attribute */` |
|      32 | 5769 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 5770 | `	if( pCons == 0 ){` |
|     ! 0 | 5771 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5772 | `		return SXERR_ABORT;` |
|       - | 5773 | `	}` |
|       - | 5774 | `	/* Swap bytecode container */` |
|      32 | 5775 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 5776 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 5777 | `	/* Compile constant value.` |
|       - | 5778 | `	 */` |
|      32 | 5779 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 5780 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 5781 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 5782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5783 | `			return SXERR_ABORT;` |
|       - | 5784 | `		}` |
|       1 | 5785 | `	}` |
|       - | 5786 | `	/* Emit the done instruction */` |
|      32 | 5787 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 5788 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 5789 | `	if( rc == SXERR_ABORT ){` |
|       - | 5790 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5791 | `		return SXERR_ABORT;` |
|       - | 5792 | `	}` |
|       - | 5793 | `	/* All done,install the constant */` |
|      32 | 5794 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 5795 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5796 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5797 | `		return SXERR_ABORT;` |
|       - | 5798 | `	}` |
|      32 | 5799 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5800 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 5801 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5802 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5803 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5804 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5805 | `				pTok--;` |
|     ! 0 | 5806 | `			}` |
|     ! 0 | 5807 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5808 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 5809 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5810 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5811 | `				return SXERR_ABORT;` |
|       - | 5812 | `			}` |
|     ! 0 | 5813 | `		}else{` |
|     ! 0 | 5814 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 5815 | `				goto loop;` |
|       - | 5816 | `			}` |
|       - | 5817 | `		}` |
|     ! 0 | 5818 | `	}` |
|      32 | 5819 | `	return SXRET_OK;` |
|     ! 0 | 5820 | `Synchronize:` |
|       - | 5821 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5822 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 5823 | `		pGen->pIn++;` |
|     ! 0 | 5824 | `	}` |
|     ! 0 | 5825 | `	return SXERR_CORRUPT;` |
|      17 | 5826 |  |
|       - | 5827 | `/*` |
|       - | 5828 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 5829 | ` * According to the PHP language reference manual` |
|       - | 5830 | ` *  Properties` |
|       - | 5831 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 5832 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 5833 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 5834 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 5835 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 5836 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 5837 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 5838 | ` * Symisc eXtension.` |
|       - | 5839 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 5840 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5841 | ` *  Example:` |
|       - | 5842 | ` *   class Test{` |
|       - | 5843 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5844 | ` *   };` |
|       - | 5845 | ` *   var_dump(TEST::myVar);` |
|       - | 5846 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5847 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5848 | ` */` |
|       - | 5849 | `/*` |
|       - | 5850 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - | 5851 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - | 5852 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - | 5853 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - | 5854 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - | 5855 | ` */` |
|  110432 | 5856 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 | 5857 |  |
|  110434 | 5858 | `	SyToken *p = pStart;` |
|  110434 | 5859 | `	if( p >= pEnd ) return 0;` |
|  110434 | 5860 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 | 5861 | `		p++;` |
|      16 | 5862 | `		if( p >= pEnd ) return 0;` |
|       7 | 5863 | `	}` |
|  110434 | 5864 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 | 5865 | `		p++;` |
|       3 | 5866 | `		if( p >= pEnd ) return 0;` |
|       1 | 5867 | `	}` |
|  110434 | 5868 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 5869 | `		return 0;` |
|       - | 5870 | `	}` |
|       - | 5871 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - | 5872 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - | 5873 | `	 * but static/final/abstract may still appear here for the initial` |
|       - | 5874 | `	 * dispatch site. */` |
|  110434 | 5875 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  110424 | 5876 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  110463 | 5877 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    2879 | 5878 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  110342 | 5879 | `			return 0;` |
|       - | 5880 | `		}` |
|      41 | 5881 | `	}` |
|      94 | 5882 | `	p++;` |
|       - | 5883 | `	/* Consume optional namespace path */` |
|      96 | 5884 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 | 5885 | `		p += 2;` |
|       1 | 5886 | `	}` |
|      94 | 5887 | `	if( p >= pEnd ) return 0;` |
|      94 | 5888 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   55218 | 5889 |  |
|       - | 5890 |  |
|       - | 5891 | `/*` |
|       - | 5892 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - | 5893 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - | 5894 | ` * if not). Recognized forms:` |
|       - | 5895 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - | 5896 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - | 5897 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - | 5898 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - | 5899 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - | 5900 | ` * on unrecoverable error.` |
|       - | 5901 | ` *` |
|       - | 5902 | ` * When a type is parsed:` |
|       - | 5903 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - | 5904 | ` *   *pClass is set to the class name (for class types)` |
|       - | 5905 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - | 5906 | ` *   *pTypeText is set to the original text span of the type` |
|       - | 5907 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - | 5908 | ` */` |
|      92 | 5909 | `static sxi32 GenStateParsePropertyType(` |
|       - | 5910 | `	ph7_gen_state *pGen,` |
|       - | 5911 | `	sxu32 *pnType,` |
|       - | 5912 | `	SyString *pClass,` |
|       - | 5913 | `	sxi32 *piTypeFlags,` |
|       - | 5914 | `	SyString *pTypeText` |
|       2 | 5915 | `){` |
|      94 | 5916 | `	SyToken *pIn = pGen->pIn;` |
|      94 | 5917 | `	SyToken *pTypeStart = pIn;` |
|      94 | 5918 | `	int bNullable = 0;` |
|      94 | 5919 | `	sxu32 nType = 0;` |
|       - | 5920 | `	SyString sClassName;` |
|      94 | 5921 | `	int bHaveClass = 0;` |
|      94 | 5922 | `	SyStringInitFromBuf(&sClassName,0,0);` |
|      94 | 5923 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 | 5924 | `		return SXRET_OK;` |
|       - | 5925 | `	}` |
|       - | 5926 | `	/* If the first token is '$', there's no type */` |
|      94 | 5927 | `	if( pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5928 | `		return SXRET_OK;` |
|       - | 5929 | `	}` |
|       - | 5930 | `	/* Optional nullable prefix '?' */` |
|      94 | 5931 | `	if( (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 5932 | `		bNullable = 1;` |
|      16 | 5933 | `		pIn++;` |
|      16 | 5934 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 | 5935 | `			return SXERR_SYNTAX;` |
|       - | 5936 | `		}` |
|       7 | 5937 | `	}` |
|       - | 5938 | `	/* Skip leading namespace separator '\' */` |
|      94 | 5939 | `	if( pIn < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       3 | 5940 | `		pIn++;` |
|       1 | 5941 | `	}` |
|      94 | 5942 | `	if( pIn >= pGen->pEnd \|\| (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 5943 | `		return SXERR_SYNTAX;` |
|       - | 5944 | `	}` |
|      94 | 5945 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|      84 | 5946 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|      84 | 5947 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       8 | 5948 | `			nType = MEMOBJ_HASHMAP;` |
|      81 | 5949 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 5950 | `			nType = MEMOBJ_BOOL;` |
|      77 | 5951 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      46 | 5952 | `			nType = MEMOBJ_INT;` |
|      54 | 5953 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      19 | 5954 | `			nType = MEMOBJ_STRING;` |
|      23 | 5955 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       8 | 5956 | `			nType = MEMOBJ_REAL;` |
|      11 | 5957 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       6 | 5958 | `			nType = MEMOBJ_OBJ;` |
|       5 | 5959 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT ){` |
|       - | 5960 | `			/* self/parent — treat as class type with that literal name */` |
|       3 | 5961 | `			nType = SXU32_HIGH;` |
|       3 | 5962 | `			sClassName = pIn->sData;` |
|       3 | 5963 | `			bHaveClass = 1;` |
|       2 | 5964 | `		}else{` |
|       - | 5965 | `			/* Unknown keyword as type — treat as syntax error for properties */` |
|     ! 0 | 5966 | `			return SXERR_SYNTAX;` |
|       - | 5967 | `		}` |
|      84 | 5968 | `		pIn++;` |
|      43 | 5969 | `	}else{` |
|       - | 5970 | `		/* Class / interface name (identifier). Consume namespace path a\b\c` |
|       - | 5971 | `		 * and grow sClassName to span the full qualified name rather than` |
|       - | 5972 | `		 * only the first segment. */` |
|      12 | 5973 | `		SyToken *pFirst = pIn;` |
|      12 | 5974 | `		SyToken *pLast = pIn;` |
|      12 | 5975 | `		sClassName = pIn->sData;` |
|      12 | 5976 | `		nType = SXU32_HIGH;` |
|      12 | 5977 | `		bHaveClass = 1;` |
|      12 | 5978 | `		pIn++;` |
|      14 | 5979 | `		while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP) && (pIn[1].nType & PH7_TK_ID) ){` |
|       3 | 5980 | `			pLast = &pIn[1];` |
|       3 | 5981 | `			pIn += 2;` |
|       1 | 5982 | `		}` |
|      12 | 5983 | `		if( pLast != pFirst ){` |
|       3 | 5984 | `			const char *zFirst = pFirst->sData.zString;` |
|       3 | 5985 | `			const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 | 5986 | `			sClassName.zString = zFirst;` |
|       3 | 5987 | `			sClassName.nByte = (sxu32)(zEnd - zFirst);` |
|       1 | 5988 | `		}` |
|       - | 5989 | `	}` |
|       - | 5990 | `	/* Verify the next token is '$' — otherwise this wasn't a property type */` |
|      94 | 5991 | `	if( pIn >= pGen->pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 5992 | `		return SXERR_SYNTAX;` |
|       - | 5993 | `	}` |
|       - | 5994 | `	/* Commit */` |
|      94 | 5995 | `	*pnType = nType;` |
|      94 | 5996 | `	*piTypeFlags = PH7_CLASS_ATTR_TYPED;` |
|      94 | 5997 | `	if( bNullable ){` |
|      16 | 5998 | `		*piTypeFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       7 | 5999 | `	}` |
|      94 | 6000 | `	if( bHaveClass ){` |
|      14 | 6001 | `		char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sClassName.zString,sClassName.nByte);` |
|      14 | 6002 | `		if( zDup == 0 ){` |
|     ! 0 | 6003 | `			return SXERR_ABORT;` |
|       - | 6004 | `		}` |
|      14 | 6005 | `		SyStringInitFromBuf(pClass,zDup,sClassName.nByte);` |
|       6 | 6006 | `	}` |
|      94 | 6007 | `	if( pTypeText ){` |
|      94 | 6008 | `		const char *zStart = pTypeStart->sData.zString;` |
|      94 | 6009 | `		const char *zEnd = pIn->sData.zString; /* points at '$' */` |
|       - | 6010 | `		sxu32 nLen;` |
|       - | 6011 | `		char *zDupTxt;` |
|      94 | 6012 | `		if( zEnd > zStart ){` |
|      94 | 6013 | `			nLen = (sxu32)(zEnd - zStart);` |
|       - | 6014 | `			/* Strip trailing whitespace that may separate the type from '$' */` |
|     232 | 6015 | `			while( nLen > 0 && (zStart[nLen-1] == ' ' \|\| zStart[nLen-1] == '\t'` |
|      92 | 6016 | `				\|\| zStart[nLen-1] == '\n' \|\| zStart[nLen-1] == '\r') ){` |
|      94 | 6017 | `				nLen--;` |
|       2 | 6018 | `			}` |
|      48 | 6019 | `		}else{` |
|     ! 0 | 6020 | `			nLen = pTypeStart->sData.nByte;` |
|       - | 6021 | `		}` |
|      94 | 6022 | `		zDupTxt = SyMemBackendStrDup(&pGen->pVm->sAllocator,zStart,nLen);` |
|      94 | 6023 | `		if( zDupTxt ){` |
|      94 | 6024 | `			SyStringInitFromBuf(pTypeText,zDupTxt,nLen);` |
|      48 | 6025 | `		}else{` |
|     ! 0 | 6026 | `			SyStringInitFromBuf(pTypeText,0,0);` |
|       - | 6027 | `		}` |
|      46 | 6028 | `	}` |
|      94 | 6029 | `	pGen->pIn = pIn;` |
|      94 | 6030 | `	return SXRET_OK;` |
|      48 | 6031 |  |
|       - | 6032 |  |
|   36044 | 6033 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 6034 |  |
|   36046 | 6035 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6036 | `	ph7_class_attr *pAttr;` |
|       - | 6037 | `	SyString *pName;` |
|       - | 6038 | `	sxi32 rc;` |
|   36046 | 6039 | `	sxu32 nType = 0;` |
|       - | 6040 | `	SyString sTypeClass;` |
|       - | 6041 | `	SyString sTypeText;` |
|   36046 | 6042 | `	sxi32 iTypeFlags = 0;` |
|   36046 | 6043 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   36046 | 6044 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|       - | 6045 | `	/* Extract visibility level */` |
|   36046 | 6046 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - | 6047 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   36092 | 6048 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      94 | 6049 | `		SyToken *pTypeTok = pGen->pIn;` |
|       - | 6050 | `		/* A leading '?' is part of the type, look past it when sniffing the` |
|       - | 6051 | `		 * type keyword for the disallowed list. */` |
|      99 | 6052 | `		if( (pTypeTok->nType & PH7_TK_OP) && pTypeTok->sData.nByte == 1` |
|      16 | 6053 | `		 && pTypeTok->sData.zString[0] == '?' && pTypeTok + 1 < pGen->pEnd ){` |
|      16 | 6054 | `			pTypeTok = pTypeTok + 1;` |
|       7 | 6055 | `		}` |
|       - | 6056 | `		/* Reject disallowed property types up front: void, callable, never,` |
|       - | 6057 | `		 * mixed, and iterable (the last is not yet implemented in PHL — we` |
|       - | 6058 | `		 * reject explicitly rather than silently fall through to a class` |
|       - | 6059 | `		 * name lookup that would resolve to NULL). */` |
|      94 | 6060 | `		if( pTypeTok->nType & PH7_TK_ID ){` |
|      10 | 6061 | `			SyString *pT = &pTypeTok->sData;` |
|       9 | 6062 | `			if( (pT->nByte == 4 && SyMemcmpNoCase(pT->zString,"void",4) == 0)` |
|       8 | 6063 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"never",5) == 0)` |
|       8 | 6064 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"mixed",5) == 0)` |
|       8 | 6065 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"callable",8) == 0)` |
|      10 | 6066 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"iterable",8) == 0) ){` |
|     ! 0 | 6067 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 6068 | `					"Property cannot have type %z",pT);` |
|     ! 0 | 6069 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6070 | `					return SXERR_ABORT;` |
|       - | 6071 | `				}` |
|     ! 0 | 6072 | `				goto Synchronize;` |
|       - | 6073 | `			}` |
|       4 | 6074 | `		}` |
|      94 | 6075 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText);` |
|      94 | 6076 | `		if( rc == SXERR_SYNTAX ){` |
|     ! 0 | 6077 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6078 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 | 6079 | `				&pGen->pIn->sData);` |
|     ! 0 | 6080 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6081 | `				return SXERR_ABORT;` |
|       - | 6082 | `			}` |
|     ! 0 | 6083 | `			goto Synchronize;` |
|      94 | 6084 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 | 6085 | `			return SXERR_ABORT;` |
|       - | 6086 | `		}` |
|      46 | 6087 | `	}` |
|     ! 0 | 6088 | `loop:` |
|   36050 | 6089 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6090 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 | 6091 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6092 | `			return SXERR_ABORT;` |
|       - | 6093 | `		}` |
|     ! 0 | 6094 | `		goto Synchronize;` |
|       - | 6095 | `	}` |
|   36050 | 6096 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   36050 | 6097 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 6098 | `		/* Invalid attribute name */` |
|     ! 0 | 6099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 6100 | `		if( rc == SXERR_ABORT ){` |
|       - | 6101 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6102 | `			return SXERR_ABORT;` |
|       - | 6103 | `		}` |
|     ! 0 | 6104 | `		goto Synchronize;` |
|       - | 6105 | `	}` |
|       - | 6106 | `	/* Peek attribute name */` |
|   36050 | 6107 | `	pName = &pGen->pIn->sData;` |
|       - | 6108 | `	/* Advance the stream cursor */` |
|   36050 | 6109 | `	pGen->pIn++;` |
|   36050 | 6110 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 6111 | `		/* Invalid declaration */` |
|       3 | 6112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 6113 | `		if( rc == SXERR_ABORT ){` |
|       - | 6114 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6115 | `			return SXERR_ABORT;` |
|       - | 6116 | `		}` |
|       3 | 6117 | `		goto Synchronize;` |
|       - | 6118 | `	}` |
|       - | 6119 | `	/* Allocate a new class attribute */` |
|   36048 | 6120 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   36048 | 6121 | `	if( pAttr == 0 ){` |
|     ! 0 | 6122 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 6123 | `		return SXERR_ABORT;` |
|       - | 6124 | `	}` |
|   36048 | 6125 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      98 | 6126 | `		pAttr->nType = nType;` |
|      98 | 6127 | `		pAttr->sClass = sTypeClass;` |
|      98 | 6128 | `		pAttr->sTypeName = sTypeText;` |
|      48 | 6129 | `	}` |
|   36048 | 6130 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 6131 | `		SySet *pInstrContainer;` |
|   11242 | 6132 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 6133 | `		/* Swap bytecode container */` |
|   11242 | 6134 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11242 | 6135 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 6136 | `		/* Compile attribute value.` |
|       - | 6137 | `		 */` |
|   11242 | 6138 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11242 | 6139 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6140 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 6141 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6142 | `				return SXERR_ABORT;` |
|       - | 6143 | `			}` |
|     ! 0 | 6144 | `		}` |
|       - | 6145 | `		/* Emit the done instruction */` |
|   11242 | 6146 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11242 | 6147 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5620 | 6148 | `	}` |
|       - | 6149 | `	/* All done,install the attribute */` |
|   36048 | 6150 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   36048 | 6151 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6152 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6153 | `		return SXERR_ABORT;` |
|       - | 6154 | `	}` |
|   36048 | 6155 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 6156 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 | 6157 | `		pGen->pIn++; /* Jump the comma */` |
|       5 | 6158 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 6159 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 6160 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 6161 | `				pTok--;` |
|     ! 0 | 6162 | `			}` |
|     ! 0 | 6163 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6164 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6165 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 6166 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6167 | `				return SXERR_ABORT;` |
|       - | 6168 | `			}` |
|     ! 0 | 6169 | `		}else{` |
|       5 | 6170 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 | 6171 | `				goto loop;` |
|       - | 6172 | `			}` |
|       - | 6173 | `		}` |
|     ! 0 | 6174 | `	}` |
|   36044 | 6175 | `	return SXRET_OK;` |
|       1 | 6176 | `Synchronize:` |
|       - | 6177 | `	/* Synchronize with the first semi-colon */` |
|       5 | 6178 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 6179 | `		pGen->pIn++;` |
|       1 | 6180 | `	}` |
|       3 | 6181 | `	return SXERR_CORRUPT;` |
|   18024 | 6182 |  |
|       - | 6183 | `/*` |
|       - | 6184 | ` * Compile a class method.` |
|       - | 6185 | ` *` |
|       - | 6186 | ` * Refer to the official documentation for more information` |
|       - | 6187 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 6188 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 6189 | ` * overloading and many more.` |
|       - | 6190 | ` */` |
|  132402 | 6191 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 6192 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 6193 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 6194 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 6195 | `	int doBody,          /* TRUE to process method body */` |
|       - | 6196 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 6197 | `	)` |
|       2 | 6198 |  |
|  132404 | 6199 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6200 | `	ph7_class_method *pMeth;` |
|       - | 6201 | `	sxi32 iFuncFlags;` |
|       - | 6202 | `	SyString *pName;` |
|       - | 6203 | `	SyToken *pEnd;` |
|       - | 6204 | `	sxi32 rc;` |
|       - | 6205 | `	/* Extract visibility level */` |
|  132404 | 6206 | `	iProtection = GetProtectionLevel(iProtection);` |
|  132404 | 6207 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  132404 | 6208 | `	iFuncFlags = 0;` |
|  132404 | 6209 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6210 | `		/* Invalid method name */` |
|     ! 0 | 6211 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 6212 | `		if( rc == SXERR_ABORT ){` |
|       - | 6213 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6214 | `			return SXERR_ABORT;` |
|       - | 6215 | `		}` |
|     ! 0 | 6216 | `		goto Synchronize;` |
|       - | 6217 | `	}` |
|  132404 | 6218 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 6219 | `		/* Return by reference,remember that */` |
|     ! 0 | 6220 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 6221 | `		/* Jump the '&' token */` |
|     ! 0 | 6222 | `		pGen->pIn++;` |
|     ! 0 | 6223 | `	}` |
|  132404 | 6224 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6225 | `		/* Invalid method name */` |
|     ! 0 | 6226 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 6227 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6228 | `			return SXERR_ABORT;` |
|       - | 6229 | `		}` |
|     ! 0 | 6230 | `		goto Synchronize;` |
|       - | 6231 | `	}` |
|       - | 6232 | `	/* Peek method name */` |
|  132404 | 6233 | `	pName = &pGen->pIn->sData;` |
|  132404 | 6234 | `	nLine = pGen->pIn->nLine;` |
|       - | 6235 | `	/* Jump the method name */` |
|  132404 | 6236 | `	pGen->pIn++;` |
|  132404 | 6237 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 6238 | `		/* Abstract method */` |
|   22034 | 6239 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 6240 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6241 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 6242 | `				&pClass->sName,pName);` |
|     ! 0 | 6243 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6244 | `				return SXERR_ABORT;` |
|       - | 6245 | `			}` |
|     ! 0 | 6246 | `		}` |
|       - | 6247 | `		/* Assemble method signature only */` |
|   22034 | 6248 | `		doBody = FALSE;` |
|   11016 | 6249 | `	}` |
|  132404 | 6250 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 6251 | `		/* Syntax error */` |
|     ! 0 | 6252 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 6253 | `		if( rc == SXERR_ABORT ){` |
|       - | 6254 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6255 | `			return SXERR_ABORT;` |
|       - | 6256 | `		}` |
|     ! 0 | 6257 | `		goto Synchronize;` |
|       - | 6258 | `	}` |
|       - | 6259 | `	/* Allocate a new class_method instance */` |
|  132404 | 6260 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  132404 | 6261 | `	if( pMeth == 0 ){` |
|     ! 0 | 6262 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6263 | `		return SXERR_ABORT;` |
|       - | 6264 | `	}` |
|       - | 6265 | `	/* Jump the left parenthesis '(' */` |
|  132404 | 6266 | `	pGen->pIn++;` |
|  132404 | 6267 | `	pEnd = 0; /* cc warning */` |
|       - | 6268 | `	/* Delimit the method signature */` |
|  132404 | 6269 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  132404 | 6270 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6271 | `		/* Syntax error */` |
|       3 | 6272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 6273 | `		if( rc == SXERR_ABORT ){` |
|       - | 6274 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6275 | `			return SXERR_ABORT;` |
|       - | 6276 | `		}` |
|       3 | 6277 | `		goto Synchronize;` |
|       - | 6278 | `	}` |
|  132402 | 6279 | `	if( pGen->pIn < pEnd ){` |
|       - | 6280 | `		/* Collect method arguments */` |
|   27576 | 6281 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   27576 | 6282 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6283 | `			return SXERR_ABORT;` |
|       - | 6284 | `		}` |
|   13787 | 6285 | `	}` |
|       - | 6286 | `	/* Point past ')' and parse optional return type ': type' */` |
|  132402 | 6287 | `	pGen->pIn = &pEnd[1];` |
|  132402 | 6288 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  132402 | 6289 | `	if( doBody ){` |
|       - | 6290 | `		/* Compile method body */` |
|  110370 | 6291 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  110370 | 6292 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6293 | `			return SXERR_ABORT;` |
|       - | 6294 | `		}` |
|   55186 | 6295 | `	}else{` |
|       - | 6296 | `		/* Only method signature is allowed */` |
|   22034 | 6297 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 6298 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6299 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 6300 | `				if( rc == SXERR_ABORT ){` |
|       - | 6301 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6302 | `					return SXERR_ABORT;` |
|       - | 6303 | `				}` |
|     ! 0 | 6304 | `				return SXERR_CORRUPT;` |
|       - | 6305 | `			}` |
|       - | 6306 | `	}` |
|       - | 6307 | `	/* All done,install the method */` |
|  132402 | 6308 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  132402 | 6309 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6310 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6311 | `		return SXERR_ABORT;` |
|       - | 6312 | `	}` |
|  132402 | 6313 | `	return SXRET_OK;` |
|       1 | 6314 | `Synchronize:` |
|       - | 6315 | `	/* Synchronize with the first semi-colon */` |
|       7 | 6316 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 6317 | `		pGen->pIn++;` |
|       1 | 6318 | `	}` |
|       3 | 6319 | `	return SXERR_CORRUPT;` |
|   66203 | 6320 |  |
|       - | 6321 | `/*` |
|       - | 6322 | ` * Compile an object interface.` |
|       - | 6323 | ` *  According to the PHP language reference manual` |
|       - | 6324 | ` *   Object Interfaces:` |
|       - | 6325 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 6326 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 6327 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 6328 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 6329 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 6330 | ` */` |
|    8282 | 6331 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 6332 |  |
|    8284 | 6333 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6334 | `	ph7_class *pClass,*pBase;` |
|       - | 6335 | `	SyToken *pEnd,*pTmp;` |
|       - | 6336 | `	SyString *pName;` |
|       - | 6337 | `	sxi32 nKwrd;` |
|       - | 6338 | `	sxi32 rc;` |
|       - | 6339 | `	/* Jump the 'interface' keyword */` |
|    8284 | 6340 | `	pGen->pIn++;` |
|       - | 6341 | `	/* Extract interface name */` |
|    8284 | 6342 | `	pName = &pGen->pIn->sData;` |
|       - | 6343 | `	/* Advance the stream cursor */` |
|    8284 | 6344 | `	pGen->pIn++;` |
|       - | 6345 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6346 | `		SyBlob sFQN;` |
|       - | 6347 | `		SyString sFQNStr;` |
|    8284 | 6348 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8284 | 6349 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8284 | 6350 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8284 | 6351 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8284 | 6352 | `		SyBlobRelease(&sFQN);` |
|       - | 6353 | `	}` |
|    8284 | 6354 | `	if( pClass == 0 ){` |
|     ! 0 | 6355 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6356 | `		return SXERR_ABORT;` |
|       - | 6357 | `	}` |
|       - | 6358 | `	/* Mark as an interface */` |
|    8284 | 6359 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 6360 | `	/* Assume no base class is given */` |
|    8284 | 6361 | `	pBase = 0;` |
|    8284 | 6362 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6363 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6364 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 6365 | `			SyString *pBaseName;` |
|       - | 6366 | `			/* Extract base interface */` |
|       3 | 6367 | `			pGen->pIn++;` |
|       3 | 6368 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6369 | `				/* Syntax error */` |
|     ! 0 | 6370 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6371 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 6372 | `					pName);` |
|     ! 0 | 6373 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6374 | `				if( rc == SXERR_ABORT ){` |
|       - | 6375 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6376 | `					return SXERR_ABORT;` |
|       - | 6377 | `				}` |
|     ! 0 | 6378 | `				return SXRET_OK;` |
|       - | 6379 | `			}` |
|       3 | 6380 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 6381 | `			{` |
|       - | 6382 | `				SyBlob sResolved;` |
|       3 | 6383 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 6384 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 6385 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 6386 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 6387 | `				SyBlobRelease(&sResolved);` |
|       - | 6388 | `			}` |
|       - | 6389 | `			/* Only interfaces is allowed */` |
|       3 | 6390 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6391 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6392 | `			}` |
|       3 | 6393 | `			if( pBase == 0 ){` |
|       - | 6394 | `				/* Inexistant interface */` |
|     ! 0 | 6395 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 6396 | `				if( rc == SXERR_ABORT ){` |
|       - | 6397 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6398 | `					return SXERR_ABORT;` |
|       - | 6399 | `				}` |
|     ! 0 | 6400 | `			}` |
|       - | 6401 | `			/* Advance the stream cursor */` |
|       3 | 6402 | `			pGen->pIn++;` |
|       1 | 6403 | `		}` |
|       1 | 6404 | `	}` |
|    8284 | 6405 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6406 | `		/* Syntax error */` |
|     ! 0 | 6407 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 6408 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6409 | `		if( rc == SXERR_ABORT ){` |
|       - | 6410 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6411 | `			return SXERR_ABORT;` |
|       - | 6412 | `		}` |
|     ! 0 | 6413 | `		return SXRET_OK;` |
|       - | 6414 | `	}` |
|    8284 | 6415 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8284 | 6416 | `	pEnd = 0; /* cc warning */` |
|       - | 6417 | `	/* Delimit the interface body */` |
|    8284 | 6418 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8284 | 6419 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6420 | `		/* Syntax error */` |
|     ! 0 | 6421 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 6422 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6423 | `		if( rc == SXERR_ABORT ){` |
|       - | 6424 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6425 | `			return SXERR_ABORT;` |
|       - | 6426 | `		}` |
|     ! 0 | 6427 | `		return SXRET_OK;` |
|       - | 6428 | `	}` |
|       - | 6429 | `	/* Swap token stream */` |
|    8284 | 6430 | `	pTmp = pGen->pEnd;` |
|    8284 | 6431 | `	pGen->pEnd = pEnd;` |
|       - | 6432 | `	/* Start the parse process` |
|       - | 6433 | `	 * Note (According to the PHP reference manual):` |
|       - | 6434 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 6435 | `	 *  Only 'public' visibility is allowed.` |
|       - | 6436 | `	 */` |
|   15152 | 6437 | `	for(;;){` |
|       - | 6438 | `		/* Jump leading/trailing semi-colons */` |
|   52328 | 6439 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   22024 | 6440 | `			pGen->pIn++;` |
|       2 | 6441 | `		}` |
|   30306 | 6442 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6443 | `			/* End of interface body */` |
|    8282 | 6444 | `			break;` |
|       - | 6445 | `		}` |
|   22026 | 6446 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6447 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6448 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 6449 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6450 | `			if( rc == SXERR_ABORT ){` |
|       - | 6451 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6452 | `				return SXERR_ABORT;` |
|       - | 6453 | `			}` |
|     ! 0 | 6454 | `			goto done;` |
|       - | 6455 | `		}` |
|       - | 6456 | `		/* Extract the current keyword */` |
|   22026 | 6457 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22026 | 6458 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 6459 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 6460 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 6461 | `			const char *zKind = "member";` |
|       3 | 6462 | `			SyString *pMemberName = 0;` |
|       3 | 6463 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 6464 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 6465 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 6466 | `					zKind = "constant";` |
|       3 | 6467 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 6468 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 6469 | `					}` |
|       1 | 6470 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6471 | `					zKind = "method";` |
|     ! 0 | 6472 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 6473 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 6474 | `					}` |
|     ! 0 | 6475 | `				}` |
|       1 | 6476 | `			}` |
|       3 | 6477 | `			if( pMemberName ){` |
|       4 | 6478 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 6479 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 6480 | `			}else{` |
|     ! 0 | 6481 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6482 | `					"Access type for interface %s must be public",zKind);` |
|       - | 6483 | `			}` |
|       3 | 6484 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6485 | `				return SXERR_ABORT;` |
|       - | 6486 | `			}` |
|       3 | 6487 | `			goto done;` |
|       - | 6488 | `		}` |
|   22024 | 6489 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 6490 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6491 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 6492 | `			if( rc == SXERR_ABORT ){` |
|       - | 6493 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6494 | `				return SXERR_ABORT;` |
|       - | 6495 | `			}` |
|     ! 0 | 6496 | `			goto done;` |
|       - | 6497 | `		}` |
|   22024 | 6498 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 6499 | `			/* Advance the stream cursor */` |
|   22020 | 6500 | `			pGen->pIn++;` |
|   22020 | 6501 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6502 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6503 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 6504 | `				if( rc == SXERR_ABORT ){` |
|       - | 6505 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6506 | `					return SXERR_ABORT;` |
|       - | 6507 | `				}` |
|     ! 0 | 6508 | `				goto done;` |
|       - | 6509 | `			}` |
|   22020 | 6510 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22020 | 6511 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 6512 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6513 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 6514 | `				if( rc == SXERR_ABORT ){` |
|       - | 6515 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6516 | `					return SXERR_ABORT;` |
|       - | 6517 | `				}` |
|     ! 0 | 6518 | `				goto done;` |
|       - | 6519 | `			}` |
|   11009 | 6520 | `		}` |
|   22024 | 6521 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6522 | `			/* Parse constant */` |
|       3 | 6523 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 6524 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6525 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6526 | `					return SXERR_ABORT;` |
|       - | 6527 | `				}` |
|     ! 0 | 6528 | `				goto done;` |
|       - | 6529 | `			}` |
|       2 | 6530 | `		}else{` |
|   22022 | 6531 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   22022 | 6532 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6533 | `				/* Static method,record that */` |
|     ! 0 | 6534 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 6535 | `				/* Advance the stream cursor */` |
|     ! 0 | 6536 | `				pGen->pIn++;` |
|     ! 0 | 6537 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 6538 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6539 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6540 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 6541 | `						if( rc == SXERR_ABORT ){` |
|       - | 6542 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6543 | `							return SXERR_ABORT;` |
|       - | 6544 | `						}` |
|     ! 0 | 6545 | `						goto done;` |
|       - | 6546 | `				}` |
|     ! 0 | 6547 | `			}` |
|       - | 6548 | `			/* Process method signature (no body for interface methods) */` |
|   22022 | 6549 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   22022 | 6550 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6551 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6552 | `					return SXERR_ABORT;` |
|       - | 6553 | `				}` |
|     ! 0 | 6554 | `				goto done;` |
|       - | 6555 | `			}` |
|       - | 6556 | `		}` |
|       2 | 6557 | `	}` |
|       - | 6558 | `	/* Install the interface */` |
|    8282 | 6559 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8282 | 6560 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 6561 | `		/* Inherit from the base interface */` |
|       3 | 6562 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 6563 | `	}` |
|    8282 | 6564 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6565 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6566 | `		return SXERR_ABORT;` |
|       - | 6567 | `	}` |
|    4140 | 6568 | `done:` |
|       - | 6569 | `	/* Point beyond the interface body */` |
|    8284 | 6570 | `	pGen->pIn  = &pEnd[1];` |
|    8284 | 6571 | `	pGen->pEnd = pTmp;` |
|    8284 | 6572 | `	return PH7_OK;` |
|    4143 | 6573 |  |
|       - | 6574 | `/*` |
|       - | 6575 | ` * Compile a user-defined class.` |
|       - | 6576 | ` * According to the PHP language reference manual` |
|       - | 6577 | ` *  class` |
|       - | 6578 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 6579 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 6580 | ` *  of the properties and methods belonging to the class.` |
|       - | 6581 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 6582 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 6583 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 6584 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 6585 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 6586 | ` *  (called "methods").` |
|       - | 6587 | ` */` |
|       - | 6588 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 6589 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 6590 | `struct TraitUseEntry {` |
|       - | 6591 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 6592 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 6593 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 6594 | `};` |
|       - | 6595 | `/*` |
|       - | 6596 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 6597 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 6598 | ` */` |
|   39038 | 6599 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 6600 |  |
|       - | 6601 | `	ph7_class **apIface;` |
|       - | 6602 | `	sxu32 nIface,i;` |
|       - | 6603 | `	sxi32 rc;` |
|   39040 | 6604 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 6605 | `		return SXRET_OK;` |
|       - | 6606 | `	}` |
|   39040 | 6607 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   39040 | 6608 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   41826 | 6609 | `	for(i = 0; i < nIface; i++){` |
|    2788 | 6610 | `		ph7_class *pIface = apIface[i];` |
|       - | 6611 | `		SyHashEntry *pEntry;` |
|    2788 | 6612 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16606 | 6613 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13820 | 6614 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 6615 | `			ph7_class_method *pImplMeth;` |
|   13820 | 6616 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 6617 | `			/* Find the implementing method in the class */` |
|   13820 | 6618 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13820 | 6619 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 6620 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 6621 | `			}` |
|       - | 6622 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13806 | 6623 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 6624 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 6625 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 6626 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 6627 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6628 | `					return SXERR_ABORT;` |
|       - | 6629 | `				}` |
|       1 | 6630 | `			}` |
|       - | 6631 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 6632 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 6633 | `			 */` |
|       - | 6634 | `			{` |
|   13806 | 6635 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13806 | 6636 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13806 | 6637 | `				int sigError = 0;` |
|   13806 | 6638 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 6639 | `					sigError = 1;` |
|   13805 | 6640 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 6641 | `					/* Extra parameters must all have default values */` |
|       5 | 6642 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 6643 | `					sxu32 k;` |
|       7 | 6644 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 6645 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 6646 | `							sigError = 1;` |
|       3 | 6647 | `							break;` |
|       - | 6648 | `						}` |
|       2 | 6649 | `					}` |
|       2 | 6650 | `				}` |
|   13806 | 6651 | `				if( sigError ){` |
|       - | 6652 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 6653 | `					ph7_vm_func_arg *aArgs;` |
|       - | 6654 | `					sxu32 j;` |
|       5 | 6655 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 6656 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 6657 | `					/* Build implementing method signature */` |
|       5 | 6658 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 6659 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 6660 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 6661 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 6662 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 6663 | `					}` |
|       - | 6664 | `					/* Build interface method signature */` |
|       5 | 6665 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 6666 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 6667 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 6668 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 6669 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 6670 | `					}` |
|       7 | 6671 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 6672 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 6673 | `						&pClass->sName,pMName,` |
|       4 | 6674 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 6675 | `						&pIface->sName,pMName,` |
|       4 | 6676 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 6677 | `					SyBlobRelease(&sImplSig);` |
|       5 | 6678 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 6679 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6680 | `						return SXERR_ABORT;` |
|       - | 6681 | `					}` |
|       2 | 6682 | `				}` |
|       - | 6683 | `			}` |
|       2 | 6684 | `		}` |
|    1395 | 6685 | `	}` |
|   39040 | 6686 | `	return SXRET_OK;` |
|   19521 | 6687 |  |
|       - | 6688 | `/*` |
|       - | 6689 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 6690 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 6691 | ` */` |
|   39038 | 6692 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 6693 |  |
|       - | 6694 | `	ph7_class_method *pMeth;` |
|       - | 6695 | `	SyHashEntry *pEntry;` |
|       - | 6696 | `	sxu32 nAbstract;` |
|       - | 6697 | `	SyBlob sMsg;` |
|       - | 6698 | `	sxi32 rc;` |
|       - | 6699 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   39040 | 6700 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 6701 | `		return SXRET_OK;` |
|       - | 6702 | `	}` |
|       - | 6703 | `	/* Count abstract methods */` |
|   39022 | 6704 | `	nAbstract = 0;` |
|   39022 | 6705 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  369658 | 6706 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  330638 | 6707 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  330638 | 6708 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 6709 | `			nAbstract++;` |
|       8 | 6710 | `		}` |
|       2 | 6711 | `	}` |
|   39022 | 6712 | `	if( nAbstract == 0 ){` |
|   39008 | 6713 | `		return SXRET_OK;` |
|       - | 6714 | `	}` |
|       - | 6715 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 6716 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 6717 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 6718 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 6719 | `		&pClass->sName,nAbstract,` |
|       7 | 6720 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 6721 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 6722 | `	/* Second pass: list methods with origins */` |
|       - | 6723 | `	{` |
|      15 | 6724 | `		sxu32 nListed = 0;` |
|      15 | 6725 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 6726 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 6727 | `			ph7_class *pOrigin = 0;` |
|       - | 6728 | `			SyString *pMName;` |
|      19 | 6729 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 6730 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 6731 | `				continue;` |
|       - | 6732 | `			}` |
|      17 | 6733 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 6734 | `			if( nListed > 0 ){` |
|       3 | 6735 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 6736 | `			}` |
|       - | 6737 | `			/* Find the origin of this abstract method.` |
|       - | 6738 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 6739 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 6740 | `			 * methods. Abstract class methods only win when the class` |
|       - | 6741 | `			 * itself declared the abstract method (not inherited from` |
|       - | 6742 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 6743 | `			 * class's namespace.` |
|       - | 6744 | `			 */` |
|       - | 6745 | `			{` |
|       - | 6746 | `				ph7_class **apIface;` |
|       - | 6747 | `				ph7_class **apTrait;` |
|       - | 6748 | `				ph7_class *pWalk;` |
|       - | 6749 | `				sxu32 i;` |
|       - | 6750 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 6751 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 6752 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 6753 | `				 */` |
|      17 | 6754 | `				if( pClass->pBase ){` |
|       9 | 6755 | `					pWalk = pClass->pBase;` |
|      17 | 6756 | `					while( pWalk ){` |
|       - | 6757 | `						ph7_class_method *pParentMeth;` |
|      11 | 6758 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 6759 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 6760 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 6761 | `							 * in this class's ancestor chain.` |
|       - | 6762 | `							 */` |
|      11 | 6763 | `							int fromIface = 0;` |
|      11 | 6764 | `							ph7_class *pAnc = pWalk;` |
|      15 | 6765 | `							while( pAnc ){` |
|       - | 6766 | `								ph7_class **apPI;` |
|       - | 6767 | `								sxu32 j;` |
|      13 | 6768 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 6769 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 6770 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 6771 | `										fromIface = 1;` |
|       9 | 6772 | `										break;` |
|       - | 6773 | `									}` |
|     ! 0 | 6774 | `								}` |
|      13 | 6775 | `								if( fromIface ) break;` |
|       5 | 6776 | `								pAnc = pAnc->pBase;` |
|       1 | 6777 | `							}` |
|      11 | 6778 | `							if( !fromIface ){` |
|       3 | 6779 | `								pOrigin = pWalk;` |
|       3 | 6780 | `								break;` |
|       - | 6781 | `							}` |
|       4 | 6782 | `						}` |
|       9 | 6783 | `						pWalk = pWalk->pBase;` |
|       1 | 6784 | `					}` |
|       4 | 6785 | `				}` |
|       - | 6786 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 6787 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 6788 | `				 */` |
|      17 | 6789 | `				if( !pOrigin ){` |
|      15 | 6790 | `					pWalk = pClass;` |
|      37 | 6791 | `					while( pWalk && !pOrigin ){` |
|      23 | 6792 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 6793 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 6794 | `							ph7_class *pIface = apIface[i];` |
|      13 | 6795 | `							ph7_class *pDeepest = 0;` |
|      25 | 6796 | `							while( pIface ){` |
|      13 | 6797 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 6798 | `									pDeepest = pIface;` |
|       6 | 6799 | `								}` |
|      13 | 6800 | `								pIface = pIface->pBase;` |
|       1 | 6801 | `							}` |
|      13 | 6802 | `							if( pDeepest ){` |
|      13 | 6803 | `								pOrigin = pDeepest;` |
|      13 | 6804 | `								break;` |
|       - | 6805 | `							}` |
|     ! 0 | 6806 | `						}` |
|      23 | 6807 | `						pWalk = pWalk->pBase;` |
|       1 | 6808 | `					}` |
|       7 | 6809 | `				}` |
|       - | 6810 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 6811 | `				if( !pOrigin ){` |
|       3 | 6812 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 6813 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 6814 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 6815 | `							pOrigin = pClass;` |
|       3 | 6816 | `							break;` |
|       - | 6817 | `						}` |
|     ! 0 | 6818 | `					}` |
|       1 | 6819 | `				}` |
|       - | 6820 | `			}` |
|      17 | 6821 | `			if( pOrigin ){` |
|      17 | 6822 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 6823 | `			}else{` |
|       - | 6824 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 6825 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 6826 | `			}` |
|      17 | 6827 | `			nListed++;` |
|       1 | 6828 | `		}` |
|       - | 6829 | `	}` |
|      15 | 6830 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 6831 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 6832 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 6833 | `	SyBlobRelease(&sMsg);` |
|      15 | 6834 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6835 | `		return SXERR_ABORT;` |
|       - | 6836 | `	}` |
|      15 | 6837 | `	return SXRET_OK;` |
|   19521 | 6838 |  |
|   39042 | 6839 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 6840 |  |
|   39044 | 6841 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6842 | `	ph7_class *pClass,*pBase;` |
|       - | 6843 | `	SyToken *pEnd,*pTmp;` |
|       - | 6844 | `	sxi32 iProtection;` |
|       - | 6845 | `	SySet aInterfaces;` |
|       - | 6846 | `	SySet aUseEntries;` |
|       - | 6847 | `	sxi32 iAttrflags;` |
|       - | 6848 | `	SyString *pName;` |
|       - | 6849 | `	sxi32 nKwrd;` |
|       - | 6850 | `	sxi32 rc;` |
|       - | 6851 | `	/* Jump the 'class' keyword */` |
|   39044 | 6852 | `	pGen->pIn++;` |
|   39044 | 6853 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6854 | `		/* Syntax error */` |
|     ! 0 | 6855 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 6856 | `		if( rc == SXERR_ABORT ){` |
|       - | 6857 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6858 | `			return SXERR_ABORT;` |
|       - | 6859 | `		}` |
|       - | 6860 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 6861 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 6862 | `			pGen->pIn++;` |
|     ! 0 | 6863 | `		}` |
|     ! 0 | 6864 | `		return SXRET_OK;` |
|       - | 6865 | `	}` |
|       - | 6866 | `	/* Extract class name */` |
|   39044 | 6867 | `	pName = &pGen->pIn->sData;` |
|       - | 6868 | `	/* Advance the stream cursor */` |
|   39044 | 6869 | `	pGen->pIn++;` |
|       - | 6870 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6871 | `		SyBlob sFQN;` |
|       - | 6872 | `		SyString sFQNStr;` |
|   39044 | 6873 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39044 | 6874 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39044 | 6875 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39044 | 6876 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39044 | 6877 | `		SyBlobRelease(&sFQN);` |
|       - | 6878 | `	}` |
|   39044 | 6879 | `	if( pClass == 0 ){` |
|     ! 0 | 6880 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6881 | `		return SXERR_ABORT;` |
|       - | 6882 | `	}` |
|       - | 6883 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   39044 | 6884 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   39044 | 6885 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 6886 | `	/* Assume a standalone class */` |
|   39044 | 6887 | `	pBase = 0;` |
|   39044 | 6888 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6889 | `		SyString *pBaseName;` |
|   27634 | 6890 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   27634 | 6891 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   24850 | 6892 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   24850 | 6893 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6894 | `				/* Syntax error */` |
|     ! 0 | 6895 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6896 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 6897 | `					pName);` |
|     ! 0 | 6898 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6899 | `				if( rc == SXERR_ABORT ){` |
|       - | 6900 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6901 | `					return SXERR_ABORT;` |
|       - | 6902 | `				}` |
|     ! 0 | 6903 | `				return SXRET_OK;` |
|       - | 6904 | `			}` |
|       - | 6905 | `			/* Extract base class name and resolve through namespace/imports */` |
|   24850 | 6906 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 6907 | `			{` |
|       - | 6908 | `				SyBlob sResolved;` |
|   24850 | 6909 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   24850 | 6910 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   37274 | 6911 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   24848 | 6912 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   24850 | 6913 | `				SyBlobRelease(&sResolved);` |
|       - | 6914 | `			}` |
|       - | 6915 | `			/* Interfaces are not allowed */` |
|   24850 | 6916 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 6917 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6918 | `			}` |
|   24850 | 6919 | `			if( pBase == 0 ){` |
|       - | 6920 | `				/* Inexistant base class */` |
|     ! 0 | 6921 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 6922 | `				if( rc == SXERR_ABORT ){` |
|       - | 6923 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6924 | `					return SXERR_ABORT;` |
|       - | 6925 | `				}` |
|     ! 0 | 6926 | `			}else{` |
|   24850 | 6927 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 6928 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 6929 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 6930 | `					if( rc == SXERR_ABORT ){` |
|       - | 6931 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6932 | `						return SXERR_ABORT;` |
|       - | 6933 | `					}` |
|     ! 0 | 6934 | `				}` |
|       - | 6935 | `			}` |
|       - | 6936 | `			/* Advance the stream cursor */` |
|   24850 | 6937 | `			pGen->pIn++;` |
|   12424 | 6938 | `		}` |
|   27634 | 6939 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 6940 | `			ph7_class *pInterface;` |
|       - | 6941 | `			SyString *pIntName;` |
|       - | 6942 | `			/* Interface implementation */` |
|    2788 | 6943 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1393 | 6944 | `			for(;;){` |
|    2788 | 6945 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6946 | `					/* Syntax error */` |
|     ! 0 | 6947 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6948 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 6949 | `						pName);` |
|     ! 0 | 6950 | `					if( rc == SXERR_ABORT ){` |
|       - | 6951 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6952 | `						return SXERR_ABORT;` |
|       - | 6953 | `					}` |
|     ! 0 | 6954 | `					break;` |
|       - | 6955 | `				}` |
|       - | 6956 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2788 | 6957 | `				pIntName = &pGen->pIn->sData;` |
|       - | 6958 | `				{` |
|       - | 6959 | `					SyBlob sResolved;` |
|    2788 | 6960 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2788 | 6961 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5574 | 6962 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2786 | 6963 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2788 | 6964 | `					SyBlobRelease(&sResolved);` |
|       - | 6965 | `				}` |
|       - | 6966 | `				/* Only interfaces are allowed */` |
|    2788 | 6967 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6968 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 6969 | `				}` |
|    2788 | 6970 | `				if( pInterface == 0 ){` |
|       - | 6971 | `					/* Inexistant interface */` |
|     ! 0 | 6972 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 6973 | `					if( rc == SXERR_ABORT ){` |
|       - | 6974 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6975 | `						return SXERR_ABORT;` |
|       - | 6976 | `					}` |
|     ! 0 | 6977 | `				}else{` |
|       - | 6978 | `					/* Register interface */` |
|    2788 | 6979 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 6980 | `				}` |
|       - | 6981 | `				/* Advance the stream cursor */` |
|    2788 | 6982 | `				pGen->pIn++;` |
|    2788 | 6983 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1395 | 6984 | `					break;` |
|       - | 6985 | `				}` |
|     ! 0 | 6986 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 6987 | `			}` |
|    1393 | 6988 | `		}` |
|   13816 | 6989 | `	}` |
|   39044 | 6990 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6991 | `		/* Syntax error */` |
|     ! 0 | 6992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 6993 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6994 | `		if( rc == SXERR_ABORT ){` |
|       - | 6995 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6996 | `			return SXERR_ABORT;` |
|       - | 6997 | `		}` |
|     ! 0 | 6998 | `		return SXRET_OK;` |
|       - | 6999 | `	}` |
|   39044 | 7000 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39044 | 7001 | `	pEnd = 0; /* cc warning */` |
|       - | 7002 | `	/* Delimit the class body */` |
|   39044 | 7003 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39044 | 7004 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 7005 | `		/* Syntax error */` |
|     ! 0 | 7006 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 7007 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 7008 | `		if( rc == SXERR_ABORT ){` |
|       - | 7009 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7010 | `			return SXERR_ABORT;` |
|       - | 7011 | `		}` |
|     ! 0 | 7012 | `		return SXRET_OK;` |
|       - | 7013 | `	}` |
|       - | 7014 | `	/* Swap token stream */` |
|   39044 | 7015 | `	pTmp = pGen->pEnd;` |
|   39044 | 7016 | `	pGen->pEnd = pEnd;` |
|       - | 7017 | `	/* Set the inherited flags */` |
|   39044 | 7018 | `	pClass->iFlags = iFlags;` |
|       - | 7019 | `	/* Start the parse process */` |
|   74701 | 7020 | `	for(;;){` |
|       - | 7021 | `		/* Jump leading/trailing semi-colons */` |
|  221562 | 7022 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   36098 | 7023 | `			pGen->pIn++;` |
|       2 | 7024 | `		}` |
|  185466 | 7025 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7026 | `			/* End of class body */` |
|   39040 | 7027 | `			break;` |
|       - | 7028 | `		}` |
|  146428 | 7029 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 7030 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7031 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 7032 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 7033 | `			if( rc == SXERR_ABORT ){` |
|       - | 7034 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 7035 | `				return SXERR_ABORT;` |
|       - | 7036 | `			}` |
|     ! 0 | 7037 | `			goto done;` |
|       - | 7038 | `		}` |
|       - | 7039 | `		/* Assume public visibility */` |
|  146428 | 7040 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  146428 | 7041 | `		iAttrflags = 0;` |
|  146428 | 7042 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 7043 | `			/* Extract the current keyword */` |
|  146428 | 7044 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  146428 | 7045 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 7046 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 7047 | `				TraitUseEntry sUse;` |
|      44 | 7048 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 | 7049 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 | 7050 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 | 7051 | `				for(;;){` |
|       - | 7052 | `					ph7_class *pTrait;` |
|       - | 7053 | `					SyString *pTraitName;` |
|      52 | 7054 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 7055 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7056 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 7057 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7058 | `							return SXERR_ABORT;` |
|       - | 7059 | `						}` |
|     ! 0 | 7060 | `						break;` |
|       - | 7061 | `					}` |
|      52 | 7062 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 7063 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 7064 | `						SyBlob sResolved;` |
|      52 | 7065 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 | 7066 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 | 7067 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 | 7068 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 | 7069 | `						SyBlobRelease(&sResolved);` |
|       - | 7070 | `					}` |
|       - | 7071 | `					/* Only traits are allowed */` |
|      52 | 7072 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 7073 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 7074 | `					}` |
|      52 | 7075 | `					if( pTrait == 0 ){` |
|     ! 0 | 7076 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7077 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 7078 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7079 | `							return SXERR_ABORT;` |
|       - | 7080 | `						}` |
|     ! 0 | 7081 | `					}else{` |
|      52 | 7082 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 7083 | `					}` |
|      52 | 7084 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 | 7085 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 | 7086 | `						break;` |
|       - | 7087 | `					}` |
|       9 | 7088 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 7089 | `				}` |
|       - | 7090 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 | 7091 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 7092 | `					SyToken *pBlock;` |
|       9 | 7093 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 7094 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 7095 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 7096 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 7097 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 7098 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 7099 | `					}else{` |
|     ! 0 | 7100 | `						pGen->pIn = pGen->pEnd;` |
|       - | 7101 | `					}` |
|       4 | 7102 | `				}` |
|      44 | 7103 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 7104 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 | 7105 | `				continue;` |
|       - | 7106 | `			}` |
|  146386 | 7107 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  143528 | 7108 | `				iProtection = nKwrd;` |
|  143528 | 7109 | `				pGen->pIn++; /* Jump the visibility token */` |
|  143526 | 7110 | `				if( pGen->pIn >= pGen->pEnd` |
|  143528 | 7111 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 | 7112 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7113 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 7114 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7115 | `					if( rc == SXERR_ABORT ){` |
|       - | 7116 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 7117 | `						return SXERR_ABORT;` |
|       - | 7118 | `					}` |
|     ! 0 | 7119 | `					goto done;` |
|       - | 7120 | `				}` |
|  143528 | 7121 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 7122 | `					/* Attribute declaration (untyped) */` |
|   35932 | 7123 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   35932 | 7124 | `					if( rc != SXRET_OK ){` |
|       3 | 7125 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7126 | `							return SXERR_ABORT;` |
|       - | 7127 | `						}` |
|       3 | 7128 | `						goto done;` |
|       - | 7129 | `					}` |
|   35930 | 7130 | `					continue;` |
|       - | 7131 | `				}` |
|  107598 | 7132 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 7133 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      84 | 7134 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      84 | 7135 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7136 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7137 | `							return SXERR_ABORT;` |
|       - | 7138 | `						}` |
|     ! 0 | 7139 | `						goto done;` |
|       - | 7140 | `					}` |
|      84 | 7141 | `					continue;` |
|       - | 7142 | `				}` |
|       - | 7143 | `				/* Extract the keyword */` |
|  107516 | 7144 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53757 | 7145 | `			}` |
|  110374 | 7146 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 7147 | `				/* Process constant declaration */` |
|      30 | 7148 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 7149 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7150 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7151 | `						return SXERR_ABORT;` |
|       - | 7152 | `					}` |
|     ! 0 | 7153 | `					goto done;` |
|       - | 7154 | `				}` |
|      16 | 7155 | `			}else{` |
|  110346 | 7156 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 7157 | `					/* Static method or attribute,record that */` |
|    2784 | 7158 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2784 | 7159 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2784 | 7160 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 7161 | `						/* Extract the keyword */` |
|    2780 | 7162 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2780 | 7163 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 7164 | `							iProtection = nKwrd;` |
|     ! 0 | 7165 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 7166 | `						}` |
|    1389 | 7167 | `					}` |
|    2782 | 7168 | `					if( pGen->pIn >= pGen->pEnd` |
|    2784 | 7169 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 | 7170 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7171 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 7172 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7173 | `						if( rc == SXERR_ABORT ){` |
|       - | 7174 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 7175 | `							return SXERR_ABORT;` |
|       - | 7176 | `						}` |
|     ! 0 | 7177 | `						goto done;` |
|       - | 7178 | `					}` |
|    2784 | 7179 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 7180 | `						/* Attribute declaration */` |
|       5 | 7181 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 7182 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 7183 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 7184 | `								return SXERR_ABORT;` |
|       - | 7185 | `							}` |
|     ! 0 | 7186 | `							goto done;` |
|       - | 7187 | `						}` |
|       5 | 7188 | `						continue;` |
|       - | 7189 | `					}` |
|    2780 | 7190 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - | 7191 | `						/* Typed static attribute declaration */` |
|       8 | 7192 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 | 7193 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 7194 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 7195 | `								return SXERR_ABORT;` |
|       - | 7196 | `							}` |
|     ! 0 | 7197 | `							goto done;` |
|       - | 7198 | `						}` |
|       8 | 7199 | `						continue;` |
|       - | 7200 | `					}` |
|       - | 7201 | `					/* Extract the keyword */` |
|    2774 | 7202 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  108950 | 7203 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 7204 | `					/* Abstract method,record that */` |
|      10 | 7205 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 7206 | `					/* Mark the whole class as abstract */` |
|      10 | 7207 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 7208 | `					/* Advance the stream cursor */` |
|      10 | 7209 | `					pGen->pIn++;` |
|      10 | 7210 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 7211 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 7212 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 7213 | `							iProtection = nKwrd;` |
|       8 | 7214 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 7215 | `						}` |
|       4 | 7216 | `					}` |
|      10 | 7217 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 7218 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 7219 | `							/* Static method */` |
|     ! 0 | 7220 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 7221 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 7222 | `					}` |
|      10 | 7223 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 7224 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 7225 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7226 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 7227 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 7228 | `							if( rc == SXERR_ABORT ){` |
|       - | 7229 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 7230 | `								return SXERR_ABORT;` |
|       - | 7231 | `							}` |
|     ! 0 | 7232 | `							goto done;` |
|       - | 7233 | `					}` |
|      10 | 7234 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  107560 | 7235 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 7236 | `					/* final method ,record that */` |
|       5 | 7237 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 7238 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 7239 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 7240 | `						/* Extract the keyword */` |
|       5 | 7241 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 7242 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 7243 | `							iProtection = nKwrd;` |
|       5 | 7244 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 7245 | `						}` |
|       2 | 7246 | `					}` |
|       5 | 7247 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 7248 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 7249 | `							/* Static method */` |
|     ! 0 | 7250 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 7251 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 7252 | `					}` |
|       5 | 7253 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 7254 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 7255 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7256 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 7257 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 7258 | `							if( rc == SXERR_ABORT ){` |
|       - | 7259 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 7260 | `								return SXERR_ABORT;` |
|       - | 7261 | `							}` |
|     ! 0 | 7262 | `							goto done;` |
|       - | 7263 | `					}` |
|       5 | 7264 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 7265 | `				}` |
|  110336 | 7266 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 7267 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7268 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 7269 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7270 | `						if( rc == SXERR_ABORT ){` |
|       - | 7271 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 7272 | `							return SXERR_ABORT;` |
|       - | 7273 | `						}` |
|     ! 0 | 7274 | `						goto done;` |
|       - | 7275 | `				}` |
|  110336 | 7276 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 7277 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 7278 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 7279 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7280 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 7281 | `						if( rc == SXERR_ABORT ){` |
|       - | 7282 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 7283 | `							return SXERR_ABORT;` |
|       - | 7284 | `						}` |
|     ! 0 | 7285 | `						goto done;` |
|       - | 7286 | `					}` |
|       - | 7287 | `					/* Attribute declaration */` |
|       7 | 7288 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 7289 | `				}else{` |
|       - | 7290 | `					/* Process method declaration */` |
|  110330 | 7291 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 7292 | `				}` |
|  110336 | 7293 | `				if( rc != SXRET_OK ){` |
|       3 | 7294 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7295 | `						return SXERR_ABORT;` |
|       - | 7296 | `					}` |
|       3 | 7297 | `					goto done;` |
|       - | 7298 | `				}` |
|       - | 7299 | `			}` |
|   55182 | 7300 | `		}else{` |
|       - | 7301 | `			/* Attribute declaration */` |
|     ! 0 | 7302 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7303 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7304 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7305 | `					return SXERR_ABORT;` |
|       - | 7306 | `				}` |
|     ! 0 | 7307 | `				goto done;` |
|       - | 7308 | `			}` |
|       - | 7309 | `		}` |
|       2 | 7310 | `	}` |
|       - | 7311 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 7312 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 7313 | `	 */` |
|       - | 7314 | `	{` |
|       - | 7315 | `		TraitUseEntry *apUse;` |
|       - | 7316 | `		sxu32 nU;` |
|   39040 | 7317 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   39082 | 7318 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 | 7319 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 | 7320 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 | 7321 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 | 7322 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 7323 | `			sxu32 nT;` |
|      44 | 7324 | `			if( !hasResolution ){` |
|       - | 7325 | `				/* No conflict resolution block: use standard trait application */` |
|      76 | 7326 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 | 7327 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 | 7328 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7329 | `						break;` |
|       - | 7330 | `					}` |
|      22 | 7331 | `				}` |
|      19 | 7332 | `			}else{` |
|       - | 7333 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 7334 | `				 * then use the block to resolve method conflicts.` |
|       - | 7335 | `				 */` |
|       - | 7336 | `				SyToken *pR;` |
|      19 | 7337 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 7338 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 7339 | `					ph7_class_attr *pAR;` |
|       - | 7340 | `					SyHashEntry *pER;` |
|       - | 7341 | `					SyString *pNR;` |
|      11 | 7342 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 7343 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 7344 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 7345 | `						pNR = &pAR->sName;` |
|     ! 0 | 7346 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 7347 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 7348 | `						}` |
|     ! 0 | 7349 | `					}` |
|      11 | 7350 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 7351 | `				}` |
|       - | 7352 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 7353 | `				pR = pUse->pResolvStart;` |
|      21 | 7354 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 7355 | `					SyString sTrait,sMethod;` |
|       - | 7356 | `					ph7_class *pSrcTrait;` |
|       - | 7357 | `					ph7_class_method *pMeth;` |
|       - | 7358 | `					sxi32 nRKwrd;` |
|      33 | 7359 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 7360 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 7361 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 7362 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 7363 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 7364 | `					sMethod = pR->sData;` |
|      13 | 7365 | `					pR++;` |
|      13 | 7366 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 7367 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 7368 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 7369 | `							sTrait = sMethod;` |
|       7 | 7370 | `							pR++;` |
|       7 | 7371 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 7372 | `							sMethod = pR->sData;` |
|       7 | 7373 | `							pR++;` |
|       3 | 7374 | `						}` |
|       3 | 7375 | `					}` |
|      13 | 7376 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7377 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 7378 | `						continue;` |
|       - | 7379 | `					}` |
|      13 | 7380 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 7381 | `					pR++;` |
|      13 | 7382 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 7383 | `						pSrcTrait = 0;` |
|       7 | 7384 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 7385 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 7386 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 7387 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 7388 | `								pSrcTrait = apTrait[nT];` |
|       5 | 7389 | `								break;` |
|       - | 7390 | `							}` |
|       2 | 7391 | `						}` |
|       5 | 7392 | `						if( pSrcTrait ){` |
|       5 | 7393 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 7394 | `							if( pMeth ){` |
|       5 | 7395 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 7396 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 7397 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 7398 | `								}` |
|       2 | 7399 | `							}` |
|       2 | 7400 | `						}` |
|       2 | 7401 | `					}` |
|      29 | 7402 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 7403 | `				}` |
|       - | 7404 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 7405 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 7406 | `					ph7_class_method *pMR;` |
|       - | 7407 | `					SyHashEntry *pER;` |
|       - | 7408 | `					SyString *pNR;` |
|      11 | 7409 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 7410 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 7411 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 7412 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 7413 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 7414 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 7415 | `						}` |
|       1 | 7416 | `					}` |
|       6 | 7417 | `				}` |
|       - | 7418 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 7419 | `				pR = pUse->pResolvStart;` |
|      21 | 7420 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 7421 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 7422 | `					ph7_class *pSrcTrait;` |
|       - | 7423 | `					ph7_class_method *pMeth;` |
|      21 | 7424 | `					int hasQual = 0;` |
|       - | 7425 | `					sxi32 nRKwrd;` |
|      33 | 7426 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 7427 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 7428 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 7429 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 7430 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 7431 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 7432 | `					sMethod = pR->sData;` |
|      13 | 7433 | `					pR++;` |
|      13 | 7434 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 7435 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 7436 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 7437 | `							sTrait = sMethod;` |
|       7 | 7438 | `							hasQual = 1;` |
|       7 | 7439 | `							pR++;` |
|       7 | 7440 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 7441 | `							sMethod = pR->sData;` |
|       7 | 7442 | `							pR++;` |
|       3 | 7443 | `						}` |
|       3 | 7444 | `					}` |
|      13 | 7445 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7446 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 7447 | `						continue;` |
|       - | 7448 | `					}` |
|      13 | 7449 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 7450 | `					pR++;` |
|      13 | 7451 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 7452 | `						sxi32 iNewVis = -1;` |
|       9 | 7453 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 7454 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 7455 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 7456 | `								iNewVis = nAK;` |
|       7 | 7457 | `								pR++;` |
|       3 | 7458 | `							}` |
|       3 | 7459 | `						}` |
|       9 | 7460 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 7461 | `							sAlias = pR->sData;` |
|       7 | 7462 | `							pR++;` |
|       3 | 7463 | `						}` |
|       9 | 7464 | `						pMeth = 0;` |
|       9 | 7465 | `						if( hasQual ){` |
|       3 | 7466 | `							pSrcTrait = 0;` |
|       5 | 7467 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 7468 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 7469 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 7470 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 7471 | `									pSrcTrait = apTrait[nT];` |
|       3 | 7472 | `									break;` |
|       - | 7473 | `								}` |
|       2 | 7474 | `							}` |
|       3 | 7475 | `							if( pSrcTrait ){` |
|       3 | 7476 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 7477 | `							}` |
|       2 | 7478 | `						}else{` |
|       7 | 7479 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 7480 | `						}` |
|       9 | 7481 | `						if( pMeth ){` |
|       9 | 7482 | `							if( sAlias.nByte > 0 ){` |
|       - | 7483 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 7484 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 7485 | `								 */` |
|       - | 7486 | `								ph7_class_method *pAlias;` |
|       - | 7487 | `								char *zAliasDup;` |
|       7 | 7488 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 7489 | `								if( pAlias ){` |
|       7 | 7490 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 7491 | `									if( iNewVis >= 0 ){` |
|       5 | 7492 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 7493 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 7494 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 7495 | `									}` |
|       7 | 7496 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 7497 | `									if( zAliasDup ){` |
|       7 | 7498 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 7499 | `									}` |
|       4 | 7500 | `								}` |
|       6 | 7501 | `							}else if( iNewVis >= 0 ){` |
|       - | 7502 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 7503 | `								ph7_class_method *pCopy;` |
|       3 | 7504 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 7505 | `								if( pCopy ){` |
|       3 | 7506 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 7507 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 7508 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 7509 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 7510 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 7511 | `									/* Replace the method in the class hash */` |
|       3 | 7512 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 7513 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 7514 | `								}` |
|       1 | 7515 | `							}` |
|       4 | 7516 | `						}` |
|       4 | 7517 | `						SXUNUSED(hasQual);` |
|       4 | 7518 | `					}` |
|      17 | 7519 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 7520 | `				}` |
|       - | 7521 | `			}` |
|      44 | 7522 | `			SySetRelease(&pUse->aTraits);` |
|      23 | 7523 | `		}` |
|       - | 7524 | `	}` |
|       - | 7525 | `	/* Install the class */` |
|   39040 | 7526 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39040 | 7527 | `	if( rc == SXRET_OK ){` |
|       - | 7528 | `		ph7_class **apInterface;` |
|       - | 7529 | `		sxu32 n;` |
|   39040 | 7530 | `		if( pBase ){` |
|       - | 7531 | `			/* Inherit from base class and mark as a subclass */` |
|   24850 | 7532 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12424 | 7533 | `		}` |
|   39040 | 7534 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   41826 | 7535 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 7536 | `			/* Implements one or more interface */` |
|    2788 | 7537 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2788 | 7538 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7539 | `				break;` |
|       - | 7540 | `			}` |
|    1395 | 7541 | `		}` |
|       - | 7542 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   39040 | 7543 | `		if( rc == SXRET_OK ){` |
|   39040 | 7544 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   39040 | 7545 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 7546 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 7547 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 7548 | `				return SXERR_ABORT;` |
|       - | 7549 | `			}` |
|   19519 | 7550 | `		}` |
|       - | 7551 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   39040 | 7552 | `		if( rc == SXRET_OK ){` |
|   39040 | 7553 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   39040 | 7554 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 7555 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 7556 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 7557 | `				return SXERR_ABORT;` |
|       - | 7558 | `			}` |
|   19519 | 7559 | `		}` |
|   19519 | 7560 | `	}` |
|   39040 | 7561 | `	SySetRelease(&aUseEntries);` |
|   39040 | 7562 | `	SySetRelease(&aInterfaces);` |
|   39040 | 7563 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7564 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7565 | `		return SXERR_ABORT;` |
|       - | 7566 | `	}` |
|   19519 | 7567 | `done:` |
|       - | 7568 | `	/* Point beyond the class body */` |
|   39044 | 7569 | `	pGen->pIn = &pEnd[1];` |
|   39044 | 7570 | `	pGen->pEnd = pTmp;` |
|   39044 | 7571 | `	return PH7_OK;` |
|   19523 | 7572 |  |
|       - | 7573 | `/*` |
|       - | 7574 | ` * Compile a user-defined abstract class.` |
|       - | 7575 | ` *  According to the PHP language reference manual` |
|       - | 7576 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 7577 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 7578 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 7579 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 7580 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 7581 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 7582 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 7583 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 7584 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 7585 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 7586 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 7587 | ` *   could differ.` |
|       - | 7588 | ` */` |
|      16 | 7589 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 7590 |  |
|       - | 7591 | `	sxi32 rc;` |
|      18 | 7592 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 7593 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 7594 | `	return rc;` |
|       2 | 7595 |  |
|       - | 7596 | `/*` |
|       - | 7597 | ` * Compile a user-defined final class.` |
|       - | 7598 | ` *  According to the PHP language reference manual` |
|       - | 7599 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 7600 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 7601 | ` *    final then it cannot be extended.` |
|       - | 7602 | ` */` |
|       2 | 7603 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 7604 |  |
|       - | 7605 | `	sxi32 rc;` |
|       3 | 7606 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 7607 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 7608 | `	return rc;` |
|       1 | 7609 |  |
|       - | 7610 | `/*` |
|       - | 7611 | ` * Compile a user-defined trait.` |
|       - | 7612 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 7613 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 7614 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 7615 | ` */` |
|      54 | 7616 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 7617 |  |
|      56 | 7618 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7619 | `	ph7_class *pClass;` |
|       - | 7620 | `	SyToken *pEnd,*pTmp;` |
|       - | 7621 | `	sxi32 iProtection;` |
|       - | 7622 | `	sxi32 iAttrflags;` |
|       - | 7623 | `	SyString *pName;` |
|       - | 7624 | `	sxi32 nKwrd;` |
|       - | 7625 | `	sxi32 rc;` |
|       - | 7626 | `	/* Jump the 'trait' keyword */` |
|      56 | 7627 | `	pGen->pIn++;` |
|      56 | 7628 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 7629 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 7630 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7631 | `			return SXERR_ABORT;` |
|       - | 7632 | `		}` |
|     ! 0 | 7633 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 7634 | `			pGen->pIn++;` |
|     ! 0 | 7635 | `		}` |
|     ! 0 | 7636 | `		return SXRET_OK;` |
|       - | 7637 | `	}` |
|       - | 7638 | `	/* Extract trait name */` |
|      56 | 7639 | `	pName = &pGen->pIn->sData;` |
|      56 | 7640 | `	pGen->pIn++;` |
|       - | 7641 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 7642 | `		SyBlob sFQN;` |
|       - | 7643 | `		SyString sFQNStr;` |
|      56 | 7644 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 | 7645 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 | 7646 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 | 7647 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 | 7648 | `		SyBlobRelease(&sFQN);` |
|       - | 7649 | `	}` |
|      56 | 7650 | `	if( pClass == 0 ){` |
|     ! 0 | 7651 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7652 | `		return SXERR_ABORT;` |
|       - | 7653 | `	}` |
|       - | 7654 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 | 7655 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 7656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 7657 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 7658 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7659 | `			return SXERR_ABORT;` |
|       - | 7660 | `		}` |
|     ! 0 | 7661 | `		return SXRET_OK;` |
|       - | 7662 | `	}` |
|      56 | 7663 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 | 7664 | `	pEnd = 0;` |
|      56 | 7665 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 | 7666 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 7667 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 7668 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 7669 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7670 | `			return SXERR_ABORT;` |
|       - | 7671 | `		}` |
|     ! 0 | 7672 | `		return SXRET_OK;` |
|       - | 7673 | `	}` |
|       - | 7674 | `	/* Swap token stream */` |
|      56 | 7675 | `	pTmp = pGen->pEnd;` |
|      56 | 7676 | `	pGen->pEnd = pEnd;` |
|       - | 7677 | `	/* Mark as trait */` |
|      56 | 7678 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 7679 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 | 7680 | `	for(;;){` |
|     154 | 7681 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 | 7682 | `			pGen->pIn++;` |
|       2 | 7683 | `		}` |
|     130 | 7684 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 | 7685 | `			break;` |
|       - | 7686 | `		}` |
|      76 | 7687 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 7688 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7689 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 7690 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 7691 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7692 | `				return SXERR_ABORT;` |
|       - | 7693 | `			}` |
|     ! 0 | 7694 | `			goto done;` |
|       - | 7695 | `		}` |
|      76 | 7696 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 | 7697 | `		iAttrflags = 0;` |
|      76 | 7698 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 | 7699 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 | 7700 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 7701 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 7702 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 7703 | `				for(;;){` |
|       - | 7704 | `					ph7_class *pUsedTrait;` |
|       - | 7705 | `					SyString *pUsedName;` |
|       5 | 7706 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 7707 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7708 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 7709 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7710 | `							return SXERR_ABORT;` |
|       - | 7711 | `						}` |
|     ! 0 | 7712 | `						break;` |
|       - | 7713 | `					}` |
|       5 | 7714 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 7715 | `					{` |
|       - | 7716 | `						SyBlob sResolved;` |
|       5 | 7717 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 7718 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 7719 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 7720 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 7721 | `						SyBlobRelease(&sResolved);` |
|       - | 7722 | `					}` |
|       5 | 7723 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 7724 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 7725 | `					}` |
|       5 | 7726 | `					if( pUsedTrait == 0 ){` |
|       4 | 7727 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 7728 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 7729 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7730 | `							return SXERR_ABORT;` |
|       - | 7731 | `						}` |
|       2 | 7732 | `					}else{` |
|       3 | 7733 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 7734 | `					}` |
|       5 | 7735 | `					pGen->pIn++;` |
|       5 | 7736 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 7737 | `						break;` |
|       - | 7738 | `					}` |
|     ! 0 | 7739 | `					pGen->pIn++;` |
|     ! 0 | 7740 | `				}` |
|       5 | 7741 | `				continue;` |
|       - | 7742 | `			}` |
|      72 | 7743 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 | 7744 | `				iProtection = nKwrd;` |
|      68 | 7745 | `				pGen->pIn++;` |
|      66 | 7746 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 | 7747 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 | 7748 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7749 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 7750 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7751 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7752 | `						return SXERR_ABORT;` |
|       - | 7753 | `					}` |
|     ! 0 | 7754 | `					goto done;` |
|       - | 7755 | `				}` |
|      68 | 7756 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 7757 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 7758 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7759 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7760 | `							return SXERR_ABORT;` |
|       - | 7761 | `						}` |
|     ! 0 | 7762 | `						goto done;` |
|       - | 7763 | `					}` |
|      11 | 7764 | `					continue;` |
|       - | 7765 | `				}` |
|      58 | 7766 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 | 7767 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 7768 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 7769 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7770 | `							return SXERR_ABORT;` |
|       - | 7771 | `						}` |
|     ! 0 | 7772 | `						goto done;` |
|       - | 7773 | `					}` |
|       5 | 7774 | `					continue;` |
|       - | 7775 | `				}` |
|      53 | 7776 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 7777 | `			}` |
|      57 | 7778 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 7779 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7780 | `					"Traits cannot have constants");` |
|     ! 0 | 7781 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7782 | `					return SXERR_ABORT;` |
|       - | 7783 | `				}` |
|     ! 0 | 7784 | `				goto done;` |
|     ! 0 | 7785 | `			}else{` |
|      57 | 7786 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 7787 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 7788 | `					pGen->pIn++;` |
|       5 | 7789 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 7790 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 7791 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 7792 | `							iProtection = nKwrd;` |
|     ! 0 | 7793 | `							pGen->pIn++;` |
|     ! 0 | 7794 | `						}` |
|       1 | 7795 | `					}` |
|       4 | 7796 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 | 7797 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 | 7798 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7799 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 7800 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7801 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7802 | `							return SXERR_ABORT;` |
|       - | 7803 | `						}` |
|     ! 0 | 7804 | `						goto done;` |
|       - | 7805 | `					}` |
|       5 | 7806 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 7807 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 7808 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 7809 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 7810 | `								return SXERR_ABORT;` |
|       - | 7811 | `							}` |
|     ! 0 | 7812 | `							goto done;` |
|       - | 7813 | `						}` |
|       3 | 7814 | `						continue;` |
|       - | 7815 | `					}` |
|       3 | 7816 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 | 7817 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7818 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 7819 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 7820 | `								return SXERR_ABORT;` |
|       - | 7821 | `							}` |
|     ! 0 | 7822 | `							goto done;` |
|       - | 7823 | `						}` |
|     ! 0 | 7824 | `						continue;` |
|       - | 7825 | `					}` |
|       3 | 7826 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 7827 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 7828 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 7829 | `					pGen->pIn++;` |
|       5 | 7830 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 7831 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 7832 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 7833 | `							iProtection = nKwrd;` |
|       5 | 7834 | `							pGen->pIn++;` |
|       2 | 7835 | `						}` |
|       2 | 7836 | `					}` |
|       5 | 7837 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 7838 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 7839 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7840 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 7841 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 7842 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7843 | `							return SXERR_ABORT;` |
|       - | 7844 | `						}` |
|     ! 0 | 7845 | `						goto done;` |
|       - | 7846 | `					}` |
|       5 | 7847 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 7848 | `				}` |
|      55 | 7849 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 7850 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7851 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 7852 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7853 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7854 | `						return SXERR_ABORT;` |
|       - | 7855 | `					}` |
|     ! 0 | 7856 | `					goto done;` |
|       - | 7857 | `				}` |
|      55 | 7858 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 7859 | `					pGen->pIn++;` |
|     ! 0 | 7860 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 7861 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7862 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 7863 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7864 | `							return SXERR_ABORT;` |
|       - | 7865 | `						}` |
|     ! 0 | 7866 | `						goto done;` |
|       - | 7867 | `					}` |
|     ! 0 | 7868 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7869 | `				}else{` |
|      55 | 7870 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 7871 | `				}` |
|      55 | 7872 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7873 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7874 | `						return SXERR_ABORT;` |
|       - | 7875 | `					}` |
|     ! 0 | 7876 | `					goto done;` |
|       - | 7877 | `				}` |
|       - | 7878 | `			}` |
|      28 | 7879 | `		}else{` |
|     ! 0 | 7880 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7881 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7882 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7883 | `					return SXERR_ABORT;` |
|       - | 7884 | `				}` |
|     ! 0 | 7885 | `				goto done;` |
|       - | 7886 | `			}` |
|       - | 7887 | `		}` |
|       1 | 7888 | `	}` |
|       - | 7889 | `	/* Install the trait */` |
|      56 | 7890 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 | 7891 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7892 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7893 | `		return SXERR_ABORT;` |
|       - | 7894 | `	}` |
|      27 | 7895 | `done:` |
|       - | 7896 | `	/* Point beyond the trait body */` |
|      56 | 7897 | `	pGen->pIn = &pEnd[1];` |
|      56 | 7898 | `	pGen->pEnd = pTmp;` |
|      56 | 7899 | `	return PH7_OK;` |
|      29 | 7900 |  |
|       - | 7901 | `/*` |
|       - | 7902 | ` * Compile a user-defined class.` |
|       - | 7903 | ` *  According to the PHP language reference manual` |
|       - | 7904 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 7905 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 7906 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 7907 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 7908 | ` *   and functions (called "methods").` |
|       - | 7909 | ` */` |
|   39024 | 7910 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 7911 |  |
|       - | 7912 | `	sxi32 rc;` |
|   39026 | 7913 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   39026 | 7914 | `	return rc;` |
|       2 | 7915 |  |
|       - | 7916 | `/*` |
|       - | 7917 | ` * Exception handling.` |
|       - | 7918 | ` *  According to the PHP language reference manual` |
|       - | 7919 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 7920 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 7921 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 7922 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 7923 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 7924 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 7925 | ` *    (or re-thrown) within a catch block.` |
|       - | 7926 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 7927 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 7928 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 7929 | ` *    been defined with set_exception_handler().` |
|       - | 7930 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 7931 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 7932 | ` */` |
|       - | 7933 | `/*` |
|       - | 7934 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 7935 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 7936 | ` * indicates failure.` |
|       - | 7937 | ` */` |
|    8304 | 7938 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 7939 |  |
|    8306 | 7940 | `	sxi32 rc = SXRET_OK;` |
|    8306 | 7941 | `	if( pRoot->pOp ){` |
|    8300 | 7942 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4152 | 7943 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 7944 | `			/* Unexpected expression */` |
|     ! 0 | 7945 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7946 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 7947 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 7948 | `				rc = SXERR_INVALID;` |
|     ! 0 | 7949 | `			}` |
|       2 | 7950 | `		}` |
|    4155 | 7951 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 7952 | `		/* Unexpected expression */` |
|     ! 0 | 7953 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7954 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 7955 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 7956 | `			rc = SXERR_INVALID;` |
|     ! 0 | 7957 | `		}` |
|     ! 0 | 7958 | `	}` |
|    8306 | 7959 | `	return rc;` |
|       2 | 7960 |  |
|       - | 7961 | `/*` |
|       - | 7962 | ` * Compile a 'throw' statement.` |
|       - | 7963 | ` * throw: This is how you trigger an exception.` |
|       - | 7964 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 7965 | ` */` |
|    8304 | 7966 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 7967 |  |
|    8306 | 7968 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7969 | `	GenBlock *pBlock;` |
|       - | 7970 | `	sxu32 nIdx;` |
|       - | 7971 | `	sxi32 rc;` |
|    8306 | 7972 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 7973 | `	/* Compile the expression */` |
|    8306 | 7974 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8306 | 7975 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 7976 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 7977 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7978 | `			return SXERR_ABORT;` |
|       - | 7979 | `		}` |
|     ! 0 | 7980 | `		return SXRET_OK;` |
|       - | 7981 | `	}` |
|    8306 | 7982 | `	pBlock = pGen->pCurrent;` |
|       - | 7983 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   38594 | 7984 | `	while(pBlock->pParent){` |
|   38590 | 7985 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8302 | 7986 | `			break;` |
|       - | 7987 | `		}` |
|       - | 7988 | `		/* Point to the parent block */` |
|   30290 | 7989 | `		pBlock = pBlock->pParent;` |
|       2 | 7990 | `	}` |
|       - | 7991 | `	/* Emit the throw instruction */` |
|    8306 | 7992 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 7993 | `	/* Emit the jump */` |
|    8306 | 7994 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8306 | 7995 | `	return SXRET_OK;` |
|    4154 | 7996 |  |
|       - | 7997 | `/*` |
|       - | 7998 | ` * Compile a 'catch' block.` |
|       - | 7999 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 8000 | ` * an object containing the exception information.` |
|       - | 8001 | ` */` |
|     132 | 8002 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 8003 |  |
|     134 | 8004 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 8005 | `	ph7_exception_block sCatch;` |
|       - | 8006 | `	SySet *pInstrContainer;` |
|       - | 8007 | `	SyString sClassName;` |
|       - | 8008 | `	GenBlock *pCatch;` |
|       - | 8009 | `	SyToken *pToken;` |
|       - | 8010 | `	SyString *pName;` |
|       - | 8011 | `	char *zDup;` |
|       - | 8012 | `	sxi32 rc;` |
|     134 | 8013 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 8014 | `	/* Zero the structure */` |
|     134 | 8015 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 8016 | `	/* Initialize fields */` |
|     134 | 8017 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     134 | 8018 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     134 | 8019 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 8020 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 8021 | `			pToken = pGen->pIn;` |
|     ! 0 | 8022 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 8023 | `				pToken--;` |
|     ! 0 | 8024 | `			}` |
|     ! 0 | 8025 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 8026 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 8027 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 8028 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8029 | `				return SXERR_ABORT;` |
|       - | 8030 | `			}` |
|     ! 0 | 8031 | `			return SXERR_INVALID;` |
|       - | 8032 | `	}` |
|       - | 8033 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     134 | 8034 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      78 | 8035 | `	for(;;){` |
|     158 | 8036 | `		int isAbsolute = 0;` |
|       - | 8037 | `		SyBlob sName;` |
|     158 | 8038 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 8039 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     158 | 8040 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 8041 | `			isAbsolute = 1;` |
|       7 | 8042 | `			pGen->pIn++;` |
|       3 | 8043 | `		}` |
|     158 | 8044 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     156 | 8045 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 8046 | `			SyBlobRelease(&sName);` |
|       5 | 8047 | `			pToken = pGen->pIn;` |
|       5 | 8048 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 8049 | `				pToken--;` |
|     ! 0 | 8050 | `			}` |
|       7 | 8051 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 8052 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 8053 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 8054 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8055 | `				return SXERR_ABORT;` |
|       - | 8056 | `			}` |
|       5 | 8057 | `			return SXERR_INVALID;` |
|       - | 8058 | `		}` |
|       - | 8059 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     154 | 8060 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     154 | 8061 | `		pGen->pIn++;` |
|     234 | 8062 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      84 | 8063 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 8064 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 8065 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 8066 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 8067 | `			pGen->pIn++;` |
|       1 | 8068 | `		}` |
|       - | 8069 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     154 | 8070 | `		if( !isAbsolute ){` |
|       - | 8071 | `			SyString sRaw;` |
|       - | 8072 | `			SyBlob sResolved;` |
|     148 | 8073 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     148 | 8074 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     148 | 8075 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     221 | 8076 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     146 | 8077 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     148 | 8078 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     148 | 8079 | `			SyBlobRelease(&sResolved);` |
|      75 | 8080 | `		}else{` |
|       - | 8081 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 8082 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 8083 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 8084 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 8085 | `		}` |
|     154 | 8086 | `		SyBlobRelease(&sName);` |
|     154 | 8087 | `		if( zDup == 0 ){` |
|     ! 0 | 8088 | `			goto Mem;` |
|       - | 8089 | `		}` |
|     154 | 8090 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     154 | 8091 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 8092 | `			goto Mem;` |
|       - | 8093 | `		}` |
|       - | 8094 | `		/* Check for '\|' (multi-catch separator) */` |
|     164 | 8095 | `		if( pGen->pIn < pGen->pEnd &&` |
|     152 | 8096 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 8097 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 8098 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 8099 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 8100 | `			continue;` |
|       - | 8101 | `		}` |
|     130 | 8102 | `		break;` |
|     ! 0 | 8103 | `	}` |
|     192 | 8104 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     130 | 8105 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 8106 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 8107 | `			pToken = pGen->pIn;` |
|     ! 0 | 8108 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 8109 | `				pToken--;` |
|     ! 0 | 8110 | `			}` |
|     ! 0 | 8111 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 8112 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 8113 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 8114 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8115 | `				return SXERR_ABORT;` |
|       - | 8116 | `			}` |
|     ! 0 | 8117 | `			return SXERR_INVALID;` |
|       - | 8118 | `	}` |
|     130 | 8119 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 8120 | `	/* Duplicate instance name */` |
|     130 | 8121 | `	pName = &pGen->pIn->sData;` |
|     130 | 8122 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     130 | 8123 | `	if( zDup == 0 ){` |
|     ! 0 | 8124 | `		goto Mem;` |
|       - | 8125 | `	}` |
|     130 | 8126 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     130 | 8127 | `	pGen->pIn++;` |
|     130 | 8128 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 8129 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 8130 | `		pToken = pGen->pIn;` |
|     ! 0 | 8131 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 8132 | `			pToken--;` |
|     ! 0 | 8133 | `		}` |
|     ! 0 | 8134 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 8135 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 8136 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 8137 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8138 | `			return SXERR_ABORT;` |
|       - | 8139 | `		}` |
|     ! 0 | 8140 | `		return SXERR_INVALID;` |
|       - | 8141 | `	}` |
|       - | 8142 | `	/* Compile the block */` |
|     130 | 8143 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 8144 | `	/* Create the catch block */` |
|     130 | 8145 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     130 | 8146 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 8147 | `		return SXERR_ABORT;` |
|       - | 8148 | `	}` |
|       - | 8149 | `	/* Swap bytecode container */` |
|     130 | 8150 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     130 | 8151 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 8152 | `	/* Compile the block */` |
|     130 | 8153 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 8154 | `	/* Fix forward jumps now the destination is resolved  */` |
|     130 | 8155 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8156 | `	/* Emit the DONE instruction */` |
|     130 | 8157 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 8158 | `	/* Leave the block */` |
|     130 | 8159 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 8160 | `	/* Restore the default container */` |
|     130 | 8161 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 8162 | `	/* Install the catch block */` |
|     130 | 8163 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     130 | 8164 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 8165 | `		goto Mem;` |
|       - | 8166 | `	}` |
|     130 | 8167 | `	return SXRET_OK;` |
|     ! 0 | 8168 | `Mem:` |
|     ! 0 | 8169 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 8170 | `	return SXERR_ABORT;` |
|      68 | 8171 |  |
|       - | 8172 | `/*` |
|       - | 8173 | ` * Compile a 'try' block.` |
|       - | 8174 | ` * A function using an exception should be in a "try" block.` |
|       - | 8175 | ` * If the exception does not trigger, the code will continue` |
|       - | 8176 | ` * as normal. However if the exception triggers, an exception` |
|       - | 8177 | ` * is "thrown".` |
|       - | 8178 | ` */` |
|     140 | 8179 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 8180 |  |
|       - | 8181 | `	ph7_exception *pException;` |
|     142 | 8182 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 8183 | `	GenBlock *pTry;` |
|       - | 8184 | `	sxu32 nJmpIdx;` |
|       - | 8185 | `	sxi32 rc;` |
|       - | 8186 | `	/* Create the exception container */` |
|     142 | 8187 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     142 | 8188 | `	if( pException == 0 ){` |
|     ! 0 | 8189 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 8190 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 8191 | `		return SXERR_ABORT;` |
|       - | 8192 | `	}` |
|       - | 8193 | `	/* Zero the structure */` |
|     142 | 8194 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 8195 | `	/* Initialize fields */` |
|     142 | 8196 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     142 | 8197 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     142 | 8198 | `	pException->iHasFinally = 0;` |
|     142 | 8199 | `	pException->iFinallyDone = 0;` |
|     142 | 8200 | `	pException->pVm = pGen->pVm;` |
|       - | 8201 | `	/* Create the try block */` |
|     142 | 8202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     142 | 8203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 8204 | `		return SXERR_ABORT;` |
|       - | 8205 | `	}` |
|       - | 8206 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     142 | 8207 | `	pTry->pUserData = pException;` |
|       - | 8208 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     142 | 8209 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 8210 | `	/* Fix the jump later when the destination is resolved */` |
|     142 | 8211 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     142 | 8212 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 8213 | `	/* Compile the block */` |
|     142 | 8214 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     142 | 8215 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 8216 | `		return SXERR_ABORT;` |
|       - | 8217 | `	}` |
|       - | 8218 | `	/* Fix forward jumps now the destination is resolved */` |
|     142 | 8219 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8220 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     142 | 8221 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 8222 | `	/* Leave the block */` |
|     142 | 8223 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 8224 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     142 | 8225 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     138 | 8226 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 8227 | `		/* Compile one or more catch blocks */` |
|     130 | 8228 | `		for(;;){` |
|     260 | 8229 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     212 | 8230 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      66 | 8231 | `					break;` |
|       - | 8232 | `			}` |
|     134 | 8233 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     134 | 8234 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8235 | `				return SXERR_ABORT;` |
|       - | 8236 | `			}` |
|       2 | 8237 | `		}` |
|      64 | 8238 | `	}` |
|       - | 8239 | `	/* Compile optional finally block */` |
|     142 | 8240 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      76 | 8241 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 8242 | `		SySet *pInstrContainer;` |
|       - | 8243 | `		GenBlock *pFinBlock;` |
|      32 | 8244 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 8245 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 8246 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 8247 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 8248 | `			return SXERR_ABORT;` |
|       - | 8249 | `		}` |
|       - | 8250 | `		/* Swap bytecode container */` |
|      32 | 8251 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 8252 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 8253 | `		/* Compile the finally body */` |
|      32 | 8254 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 8255 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8256 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 8257 | `			return SXERR_ABORT;` |
|       - | 8258 | `		}` |
|       - | 8259 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 8260 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8261 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 8262 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 8263 | `		/* Leave the block */` |
|      32 | 8264 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 8265 | `		/* Restore the default container */` |
|      32 | 8266 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 8267 | `		pException->iHasFinally = 1;` |
|      15 | 8268 | `	}` |
|       - | 8269 | `	/* Must have at least one catch or finally */` |
|     142 | 8270 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 8271 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 8272 | `			"Cannot use try without catch or finally");` |
|       7 | 8273 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8274 | `			return SXERR_ABORT;` |
|       - | 8275 | `		}` |
|       3 | 8276 | `	}` |
|     142 | 8277 | `	return SXRET_OK;` |
|      72 | 8278 |  |
|       - | 8279 | `/*` |
|       - | 8280 | ` * Compile a switch block.` |
|       - | 8281 | ` *  (See block-comment below for more information)` |
|       - | 8282 | ` */` |
|     108 | 8283 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 8284 |  |
|     110 | 8285 | `	sxi32 rc = SXRET_OK;` |
|     110 | 8286 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 8287 | `		/* Unexpected token */` |
|     ! 0 | 8288 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 8289 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8290 | `			return SXERR_ABORT;` |
|       - | 8291 | `		}` |
|     ! 0 | 8292 | `		pGen->pIn++;` |
|     ! 0 | 8293 | `	}` |
|     110 | 8294 | `	pGen->pIn++;` |
|       - | 8295 | `	/* First instruction to execute in this block. */` |
|     110 | 8296 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 8297 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 8298 | `	 * or the '}' token */` |
|     182 | 8299 | `	for(;;){` |
|     366 | 8300 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8301 | `			/* No more input to process */` |
|     ! 0 | 8302 | `			break;` |
|       - | 8303 | `		}` |
|     366 | 8304 | `		rc = SXRET_OK;` |
|     366 | 8305 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 | 8306 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 | 8307 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 8308 | `					/* Unexpected token */` |
|     ! 0 | 8309 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 8310 | `						&pGen->pIn->sData);` |
|     ! 0 | 8311 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8312 | `						return SXERR_ABORT;` |
|       - | 8313 | `					}` |
|       - | 8314 | `					/* FALL THROUGH */` |
|     ! 0 | 8315 | `				}` |
|      28 | 8316 | `				rc = SXERR_EOF;` |
|      28 | 8317 | `				break;` |
|       - | 8318 | `			}` |
|      23 | 8319 | `		}else{` |
|       - | 8320 | `			sxi32 nKwrd;` |
|       - | 8321 | `			/* Extract the keyword */` |
|     298 | 8322 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 | 8323 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 | 8324 | `				break;` |
|       - | 8325 | `			}` |
|     218 | 8326 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 8327 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 8328 | `					/* Unexpected token */` |
|     ! 0 | 8329 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 8330 | `						&pGen->pIn->sData);` |
|     ! 0 | 8331 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8332 | `						return SXERR_ABORT;` |
|       - | 8333 | `					}` |
|       - | 8334 | `					/* FALL THROUGH */` |
|     ! 0 | 8335 | `				}` |
|       - | 8336 | `				/* Block compiled */` |
|       3 | 8337 | `				break;` |
|       - | 8338 | `			}` |
|       - | 8339 | `		}` |
|       - | 8340 | `		/* Compile block */` |
|     258 | 8341 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 | 8342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8343 | `			return SXERR_ABORT;` |
|       - | 8344 | `		}` |
|       2 | 8345 | `	}` |
|     110 | 8346 | `	return rc;` |
|      56 | 8347 |  |
|       - | 8348 | `/*` |
|       - | 8349 | ` * Compile a case eXpression.` |
|       - | 8350 | ` *  (See block-comment below for more information)` |
|       - | 8351 | ` */` |
|      88 | 8352 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 8353 |  |
|       - | 8354 | `	SySet *pInstrContainer;` |
|       - | 8355 | `	SyToken *pEnd,*pTmp;` |
|      90 | 8356 | `	sxi32 iNest = 0;` |
|       - | 8357 | `	sxi32 rc;` |
|       - | 8358 | `	/* Delimit the expression */` |
|      90 | 8359 | `	pEnd = pGen->pIn;` |
|     186 | 8360 | `	while( pEnd < pGen->pEnd ){` |
|     186 | 8361 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 8362 | `			/* Increment nesting level */` |
|       3 | 8363 | `			iNest++;` |
|     185 | 8364 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 8365 | `			/* Decrement nesting level */` |
|       3 | 8366 | `			iNest--;` |
|     183 | 8367 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 | 8368 | `			break;` |
|       - | 8369 | `		}` |
|      98 | 8370 | `		pEnd++;` |
|       2 | 8371 | `	}` |
|      90 | 8372 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 8373 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 8374 | `		if( rc == SXERR_ABORT ){` |
|       - | 8375 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8376 | `			return SXERR_ABORT;` |
|       - | 8377 | `		}` |
|     ! 0 | 8378 | `	}` |
|       - | 8379 | `	/* Swap token stream */` |
|      90 | 8380 | `	pTmp = pGen->pEnd;` |
|      90 | 8381 | `	pGen->pEnd = pEnd;` |
|      90 | 8382 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 | 8383 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 | 8384 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 8385 | `	/* Emit the done instruction */` |
|      90 | 8386 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 | 8387 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 8388 | `	/* Update token stream */` |
|      90 | 8389 | `	pGen->pIn  = pEnd;` |
|      90 | 8390 | `	pGen->pEnd = pTmp;` |
|      90 | 8391 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 8392 | `		return SXERR_ABORT;` |
|       - | 8393 | `	}` |
|      90 | 8394 | `	return SXRET_OK;` |
|      46 | 8395 |  |
|       - | 8396 | `/*` |
|       - | 8397 | ` * Compile the smart switch statement.` |
|       - | 8398 | ` * According to the PHP language reference manual` |
|       - | 8399 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 8400 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 8401 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 8402 | ` *  This is exactly what the switch statement is for.` |
|       - | 8403 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 8404 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 8405 | ` *  of the outer loop, use continue 2.` |
|       - | 8406 | ` *  Note that switch/case does loose comparision.` |
|       - | 8407 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 8408 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 8409 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 8410 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 8411 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 8412 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 8413 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 8414 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 8415 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 8416 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 8417 | ` *  list for the next case.` |
|       - | 8418 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 8419 | ` *  or floating-point numbers and strings.` |
|       - | 8420 | ` */` |
|      28 | 8421 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 8422 |  |
|       - | 8423 | `	GenBlock *pSwitchBlock;` |
|       - | 8424 | `	SyToken *pTmp,*pEnd;` |
|       - | 8425 | `	ph7_switch *pSwitch;` |
|       - | 8426 | `	sxu32 nToken;` |
|       - | 8427 | `	sxu32 nLine;` |
|       - | 8428 | `	sxi32 rc;` |
|      30 | 8429 | `	nLine = pGen->pIn->nLine;` |
|       - | 8430 | `	/* Jump the 'switch' keyword */` |
|      30 | 8431 | `	pGen->pIn++;` |
|      30 | 8432 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 8433 | `		/* Syntax error */` |
|     ! 0 | 8434 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 8435 | `		if( rc == SXERR_ABORT ){` |
|       - | 8436 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8437 | `			return SXERR_ABORT;` |
|       - | 8438 | `		}` |
|     ! 0 | 8439 | `		goto Synchronize;` |
|       - | 8440 | `	}` |
|       - | 8441 | `	/* Jump the left parenthesis '(' */` |
|      30 | 8442 | `	pGen->pIn++;` |
|      30 | 8443 | `	pEnd = 0; /* cc warning */` |
|       - | 8444 | `	/* Create the loop block */` |
|      44 | 8445 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 8446 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 | 8447 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 8448 | `		return SXERR_ABORT;` |
|       - | 8449 | `	}` |
|       - | 8450 | `	/* Delimit the condition */` |
|      30 | 8451 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 | 8452 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 8453 | `		/* Empty expression */` |
|     ! 0 | 8454 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 8455 | `		if( rc == SXERR_ABORT ){` |
|       - | 8456 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 8457 | `			return SXERR_ABORT;` |
|       - | 8458 | `		}` |
|     ! 0 | 8459 | `	}` |
|       - | 8460 | `	/* Swap token streams */` |
|      30 | 8461 | `	pTmp = pGen->pEnd;` |
|      30 | 8462 | `	pGen->pEnd = pEnd;` |
|       - | 8463 | `	/* Compile the expression */` |
|      30 | 8464 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 8465 | `	if( rc == SXERR_ABORT ){` |
|       - | 8466 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 8467 | `		return SXERR_ABORT;` |
|       - | 8468 | `	}` |
|       - | 8469 | `	/* Update token stream */` |
|      30 | 8470 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 8471 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 8472 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 8473 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 8474 | `			return SXERR_ABORT;` |
|       - | 8475 | `		}` |
|     ! 0 | 8476 | `		pGen->pIn++;` |
|     ! 0 | 8477 | `	}` |
|      30 | 8478 | `	pGen->pIn  = &pEnd[1];` |
|      30 | 8479 | `	pGen->pEnd = pTmp;` |
|      30 | 8480 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 8481 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 8482 | `			pTmp = pGen->pIn;` |
|     ! 0 | 8483 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 8484 | `				pTmp--;` |
|     ! 0 | 8485 | `			}` |
|       - | 8486 | `			/* Unexpected token */` |
|     ! 0 | 8487 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 8488 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8489 | `				return SXERR_ABORT;` |
|       - | 8490 | `			}` |
|     ! 0 | 8491 | `			goto Synchronize;` |
|       - | 8492 | `	}` |
|       - | 8493 | `	/* Set the delimiter token */` |
|      30 | 8494 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 8495 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 8496 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 8497 | `	}else{` |
|      28 | 8498 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 8499 | `	}` |
|      30 | 8500 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 8501 | `	/* Create the switch blocks container */` |
|      30 | 8502 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 | 8503 | `	if( pSwitch == 0 ){` |
|       - | 8504 | `		/* Abort compilation */` |
|     ! 0 | 8505 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 8506 | `		return SXERR_ABORT;` |
|       - | 8507 | `	}` |
|       - | 8508 | `	/* Zero the structure */` |
|      30 | 8509 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 8510 | `	/* Initialize fields */` |
|      30 | 8511 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 8512 | `	/* Emit the switch instruction */` |
|      30 | 8513 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 8514 | `	/* Compile case blocks */` |
|      96 | 8515 | `	for(;;){` |
|       - | 8516 | `		sxu32 nKwrd;` |
|     112 | 8517 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8518 | `			/* No more input to process */` |
|     ! 0 | 8519 | `			break;` |
|       - | 8520 | `		}` |
|     112 | 8521 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 8522 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 8523 | `				/* Unexpected token */` |
|     ! 0 | 8524 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8525 | `					&pGen->pIn->sData);` |
|     ! 0 | 8526 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8527 | `					return SXERR_ABORT;` |
|       - | 8528 | `				}` |
|       - | 8529 | `				/* FALL THROUGH */` |
|     ! 0 | 8530 | `			}` |
|       - | 8531 | `			/* Block compiled */` |
|     ! 0 | 8532 | `			break;` |
|       - | 8533 | `		}` |
|       - | 8534 | `		/* Extract the keyword */` |
|     112 | 8535 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 | 8536 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 8537 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 8538 | `				/* Unexpected token */` |
|     ! 0 | 8539 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8540 | `					&pGen->pIn->sData);` |
|     ! 0 | 8541 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8542 | `					return SXERR_ABORT;` |
|       - | 8543 | `				}` |
|       - | 8544 | `				/* FALL THROUGH */` |
|     ! 0 | 8545 | `			}` |
|       - | 8546 | `			/* Block compiled */` |
|       3 | 8547 | `			break;` |
|       - | 8548 | `		}` |
|     110 | 8549 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 8550 | `			/*` |
|       - | 8551 | `			 * Accroding to the PHP language reference manual` |
|       - | 8552 | `			 *  A special case is the default case. This case matches anything` |
|       - | 8553 | `			 *  that wasn't matched by the other cases.` |
|       - | 8554 | `			 */` |
|      22 | 8555 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 8556 | `				/* Default case already compiled */` |
|     ! 0 | 8557 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 8558 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 8559 | `					return SXERR_ABORT;` |
|       - | 8560 | `				}` |
|     ! 0 | 8561 | `			}` |
|      22 | 8562 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 8563 | `			/* Compile the default block */` |
|      22 | 8564 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 | 8565 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 8566 | `				return SXERR_ABORT;` |
|      22 | 8567 | `			}else if( rc == SXERR_EOF ){` |
|      20 | 8568 | `				break;` |
|       1 | 8569 | `			}` |
|      91 | 8570 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 8571 | `			ph7_case_expr sCase;` |
|       - | 8572 | `			/* Standard case block */` |
|      90 | 8573 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 8574 | `			/* initialize the structure */` |
|      90 | 8575 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 8576 | `			/* Compile the case expression */` |
|      90 | 8577 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 | 8578 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8579 | `				return SXERR_ABORT;` |
|       - | 8580 | `			}` |
|       - | 8581 | `			/* Compile the case block */` |
|      90 | 8582 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 8583 | `			/* Insert in the switch container */` |
|      90 | 8584 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 | 8585 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 8586 | `				return SXERR_ABORT;` |
|      90 | 8587 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 8588 | `				break;` |
|       - | 8589 | `			}` |
|      42 | 8590 | `		}else{` |
|       - | 8591 | `			/* Unexpected token */` |
|     ! 0 | 8592 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 8593 | `				&pGen->pIn->sData);` |
|     ! 0 | 8594 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8595 | `				return SXERR_ABORT;` |
|       - | 8596 | `			}` |
|     ! 0 | 8597 | `			break;` |
|       - | 8598 | `		}` |
|       2 | 8599 | `	}` |
|       - | 8600 | `	/* Fix all jumps now the destination is resolved */` |
|      30 | 8601 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 | 8602 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8603 | `	/* Release the loop block */` |
|      30 | 8604 | `	GenStateLeaveBlock(pGen,0);` |
|      30 | 8605 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 8606 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 | 8607 | `		pGen->pIn++;` |
|      14 | 8608 | `	}` |
|       - | 8609 | `	/* Statement successfully compiled */` |
|      30 | 8610 | `	return SXRET_OK;` |
|     ! 0 | 8611 | `Synchronize:` |
|       - | 8612 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 8613 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 8614 | `		pGen->pIn++;` |
|     ! 0 | 8615 | `	}` |
|     ! 0 | 8616 | `	return SXRET_OK;` |
|      16 | 8617 |  |
|       - | 8618 | `/*` |
|       - | 8619 | ` * Generate bytecode for a given expression tree.` |
|       - | 8620 | ` * If something goes wrong while generating bytecode` |
|       - | 8621 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 8622 | ` * this function takes care of generating the appropriate` |
|       - | 8623 | ` * error message.` |
|       - | 8624 | ` */` |
| 2473378 | 8625 | `static sxi32 GenStateEmitExprCode(` |
|       - | 8626 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8627 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 8628 | `	sxi32 iFlags /* Control flags */` |
|       - | 8629 | `	)` |
|       2 | 8630 |  |
|       - | 8631 | `	VmInstr *pInstr;` |
|       - | 8632 | `	sxu32 nJmpIdx;` |
| 2473380 | 8633 | `	sxi32 iP1 = 0;` |
| 2473380 | 8634 | `	sxu32 iP2 = 0;` |
| 2473380 | 8635 | `	void *p3  = 0;` |
|       - | 8636 | `	sxi32 iVmOp;` |
|       - | 8637 | `	sxi32 rc;` |
| 2473380 | 8638 | `	if( pNode->xCode ){` |
|       - | 8639 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 8640 | `		/* Compile node */` |
| 1533010 | 8641 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1533010 | 8642 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1533010 | 8643 | `		RE_SWAP_DELIMITER(pGen);` |
| 1533010 | 8644 | `		return rc;` |
|       - | 8645 | `	}` |
|  940372 | 8646 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 8647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 8648 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 8649 | `		return SXERR_ABORT;` |
|       - | 8650 | `	}` |
|  940372 | 8651 | `	iVmOp = pNode->pOp->iVmOp;` |
|  940372 | 8652 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 | 8653 | `		sxu32 nJmp = 0;` |
|       - | 8654 | `		VmInstr *pInstrFix;` |
|       - | 8655 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 8656 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 8657 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 8658 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 8659 | `		 * stack slot carries a writable nIdx. */` |
|      47 | 8660 | `		if( pNode->pRight ){` |
|      47 | 8661 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 | 8662 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8663 | `				return rc;` |
|       - | 8664 | `			}` |
|       - | 8665 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 8666 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 8667 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 8668 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 8669 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 8670 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 8671 | `			 * cascade for the actual write path stays correct. */` |
|      47 | 8672 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 | 8673 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 | 8674 | `				pInstrFix->iP2 = 3;` |
|       9 | 8675 | `			}` |
|      23 | 8676 | `		}` |
|       - | 8677 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 | 8678 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 8679 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 | 8680 | `		if( pNode->pLeft ){` |
|      47 | 8681 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 | 8682 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8683 | `				return rc;` |
|       - | 8684 | `			}` |
|      23 | 8685 | `		}` |
|       - | 8686 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 | 8687 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 8688 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 | 8689 | `		if( nJmp > 0 ){` |
|      47 | 8690 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 | 8691 | `			if( pInstrFix ){` |
|      47 | 8692 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 | 8693 | `			}` |
|      23 | 8694 | `		}` |
|      47 | 8695 | `		return SXRET_OK;` |
|       - | 8696 | `	}` |
|  940326 | 8697 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 8698 | `		sxu32 nJz,nJmp;` |
|       - | 8699 | `		/* Ternary operator require special handling */` |
|       - | 8700 | `		/* Phase#1: Compile the condition */` |
|    1924 | 8701 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1924 | 8702 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 8703 | `			return rc;` |
|       - | 8704 | `		}` |
|    1924 | 8705 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1924 | 8706 | `		if( pNode->pLeft ){` |
|       - | 8707 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 8708 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1856 | 8709 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 8710 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1856 | 8711 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1856 | 8712 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8713 | `				return rc;` |
|       - | 8714 | `			}` |
|     929 | 8715 | `		}else{` |
|       - | 8716 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 8717 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 8718 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 8719 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 8720 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 8721 | `		}` |
|       - | 8722 | `		/* Phase#4: Emit the unconditional jump */` |
|    1924 | 8723 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 8724 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1924 | 8725 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1924 | 8726 | `		if( pInstr ){` |
|    1924 | 8727 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     961 | 8728 | `		}` |
|    1924 | 8729 | `		if( !pNode->pLeft ){` |
|       - | 8730 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 8731 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 8732 | `		}` |
|       - | 8733 | `		/* Phase#6: Compile the 'else' expression */` |
|    1924 | 8734 | `		if( pNode->pRight ){` |
|    1924 | 8735 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1924 | 8736 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 8737 | `				return rc;` |
|       - | 8738 | `			}` |
|     961 | 8739 | `		}` |
|    1924 | 8740 | `		if( nJmp > 0 ){` |
|       - | 8741 | `			/* Phase#7: Fix the unconditional jump */` |
|    1924 | 8742 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1924 | 8743 | `			if( pInstr ){` |
|    1924 | 8744 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     961 | 8745 | `			}` |
|     961 | 8746 | `		}` |
|       - | 8747 | `		/* All done */` |
|    1924 | 8748 | `		return SXRET_OK;` |
|       - | 8749 | `	}` |
|       - | 8750 | `	/* Generate code for the left tree */` |
|  938404 | 8751 | `	if( pNode->pLeft ){` |
|  938368 | 8752 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 8753 | `			ph7_expr_node **apNode;` |
|  314838 | 8754 | `			int hasSpread = 0;` |
|       - | 8755 | `			sxi32 n;` |
|       - | 8756 | `			/* Recurse and generate bytecodes for function arguments */` |
|  314838 | 8757 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 8758 | `			/* Read-only load */` |
|  314838 | 8759 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  628884 | 8760 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  314048 | 8761 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  314048 | 8762 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 8763 | `					return rc;` |
|       - | 8764 | `				}` |
|  314048 | 8765 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 8766 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 8767 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 8768 | `					hasSpread = 1;` |
|       7 | 8769 | `				}` |
|  157025 | 8770 | `			}` |
|       - | 8771 | `			/* Total number of given arguments */` |
|  314838 | 8772 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  314838 | 8773 | `			iP2 = hasSpread;` |
|       - | 8774 | `			/* Remove stale flags now */` |
|  314838 | 8775 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  157418 | 8776 | `		}` |
|  938368 | 8777 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  938368 | 8778 | `		if( rc != SXRET_OK ){` |
|      27 | 8779 | `			return rc;` |
|       - | 8780 | `		}` |
|  938342 | 8781 | `		if( iVmOp == PH7_OP_CALL ){` |
|  314838 | 8782 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  314838 | 8783 | `			if( pInstr ){` |
|  314838 | 8784 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  314116 | 8785 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 8786 | `					sxu32 nQual;` |
|       - | 8787 | `					/* Prevent constant expansion */` |
|  314116 | 8788 | `					pInstr->iP1 = 0;` |
|       - | 8789 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 8790 | `					 * Only check function imports — class imports must NOT` |
|       - | 8791 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 8792 | `					 * handler fires before NEW; we store the original literal` |
|       - | 8793 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 8794 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 8795 | `					 * class imports. */ {` |
|  314116 | 8796 | `						int fromImport = 0;` |
|  314116 | 8797 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  314116 | 8798 | `						pInstr->iP2 = (sxi32)nQual;` |
|  314116 | 8799 | `						if( nQual != nOrig ){` |
|       - | 8800 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 8801 | `							 * NEW handler can recover the unqualified name. */` |
|      72 | 8802 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      72 | 8803 | `							if( !fromImport ){` |
|      62 | 8804 | `								p3 = (void *)1;` |
|      30 | 8805 | `							}` |
|      37 | 8806 | `						}` |
|       - | 8807 | `					}` |
|  157781 | 8808 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 8809 | `					/* Method call,flag that */` |
|     608 | 8810 | `					pInstr->iP2 = 1;` |
|     303 | 8811 | `				}` |
|  157420 | 8812 | `			}` |
|  780924 | 8813 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 8814 | `			ph7_expr_node **apNode;` |
|       - | 8815 | `			sxi32 n;` |
|       - | 8816 | `			/* Recurse and generate bytecodes for array index */` |
|   70576 | 8817 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  127344 | 8818 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   56770 | 8819 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   56770 | 8820 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 8821 | `					return rc;` |
|       - | 8822 | `				}` |
|   28386 | 8823 | `			}` |
|   70576 | 8824 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   56770 | 8825 | `				iP1 = 1; /* Node have an index associated with it */` |
|   28384 | 8826 | `			}` |
|   70576 | 8827 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 8828 | `				/* Create an empty entry when the desired index is not found */` |
|   27876 | 8829 | `				iP2 = 1;` |
|   13939 | 8830 | `			}` |
|  588219 | 8831 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 8832 | `			/* POP the left node */` |
|      32 | 8833 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 8834 | `		}` |
|  469170 | 8835 | `	}` |
|  938378 | 8836 | `	rc = SXRET_OK;` |
|  938378 | 8837 | `	nJmpIdx = 0;` |
|       - | 8838 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 8839 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 8840 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  938378 | 8841 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     260 | 8842 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     260 | 8843 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     260 | 8844 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     260 | 8845 | `			int isSpecial = 0;` |
|     260 | 8846 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     176 | 8847 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     176 | 8848 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     187 | 8849 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     153 | 8850 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      78 | 8851 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      88 | 8852 | `					isSpecial = 1;` |
|      43 | 8853 | `				}` |
|     108 | 8854 | `			}` |
|     302 | 8855 | `			pInstr->iP1 = 0;` |
|     302 | 8856 | `			if( !isSpecial ){` |
|     132 | 8857 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      65 | 8858 | `			}` |
|       - | 8859 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 8860 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     218 | 8861 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     132 | 8862 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     132 | 8863 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 8864 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 8865 | `					return SXRET_OK;` |
|       - | 8866 | `				}` |
|      44 | 8867 | `			}` |
|      87 | 8868 | `		}` |
|     159 | 8869 | `	}` |
|       - | 8870 | `	/* Generate code for the right tree */` |
|  938302 | 8871 | `	if( pNode->pRight ){` |
|  490358 | 8872 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 8873 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8666 | 8874 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  486026 | 8875 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 8876 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2894 | 8877 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  480248 | 8878 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 8879 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 8880 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 8881 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  478787 | 8882 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  213918 | 8883 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  106958 | 8884 | `		}` |
|  490358 | 8885 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  490358 | 8886 | `		if( iVmOp == PH7_OP_STORE ){` |
|  210988 | 8887 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  210962 | 8888 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 8889 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 8890 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 8891 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 8892 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 8893 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 8894 | `				 */` |
|      54 | 8895 | `				iVmOp = 0;` |
|  210962 | 8896 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  210936 | 8897 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 8898 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   46922 | 8899 | `					iP2 = 1;` |
|   23462 | 8900 | `				}else{` |
|  164016 | 8901 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8902 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   27814 | 8903 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   27814 | 8904 | `						iP1 = pInstr->iP1;` |
|   13908 | 8905 | `					}else{` |
|  136204 | 8906 | `						p3 = pInstr->p3;` |
|       - | 8907 | `					}` |
|       - | 8908 | `					/* POP the last dynamic load instruction */` |
|  164016 | 8909 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 8910 | `				}` |
|  105469 | 8911 | `			}` |
|  384865 | 8912 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 8913 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 8914 | `			if( pInstr ){` |
|      48 | 8915 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8916 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 8917 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 8918 | `					 */` |
|      15 | 8919 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 8920 | `					iP1 = pInstr->iP1;` |
|      15 | 8921 | `					iP2 = pInstr->iP2;` |
|      15 | 8922 | `					p3  = pInstr->p3;` |
|       8 | 8923 | `				}else{` |
|      34 | 8924 | `					p3 = pInstr->p3;` |
|       - | 8925 | `				}` |
|      23 | 8926 | `			}` |
|      23 | 8927 | `		}` |
|  245178 | 8928 | `	}` |
|  938302 | 8929 | `	if( iVmOp > 0 ){` |
|  938190 | 8930 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11248 | 8931 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 8932 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8254 | 8933 | `				iP1 = 1;` |
|    4128 | 8934 | `			}` |
|  932567 | 8935 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 8936 | `			/* Namespace-qualify the class name for NEW */ {` |
|   14274 | 8937 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   14274 | 8938 | `				VmInstr *pCallInstr = 0;` |
|   14274 | 8939 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   14258 | 8940 | `					pCallInstr = pPeek;` |
|   14258 | 8941 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7128 | 8942 | `				}` |
|   14274 | 8943 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 8944 | `					sxu32 nLitForClass;` |
|       - | 8945 | `					/* If the CALL handler already qualified the name using` |
|       - | 8946 | `					 * function imports, recover the original unqualified` |
|       - | 8947 | `					 * literal so we can re-qualify with class imports. */` |
|   14272 | 8948 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 8949 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 8950 | `					}else{` |
|   14240 | 8951 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 8952 | `					}` |
|   14272 | 8953 | `					pPeek->iP1 = 0;` |
|   14272 | 8954 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7135 | 8955 | `				}` |
|       - | 8956 | `			}` |
|   14274 | 8957 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   14274 | 8958 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 8959 | `				VmInstr *pPrev;` |
|   14258 | 8960 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   14258 | 8961 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 8962 | `					/* Pop the call instruction */` |
|   14258 | 8963 | `					iP1 = pInstr->iP1;` |
|   14258 | 8964 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7128 | 8965 | `				}` |
|    7130 | 8966 | `			}` |
|  919808 | 8967 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 8968 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 8969 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 8970 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 8971 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 8972 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 8973 | `				int isSpecialIs = 0;` |
|      50 | 8974 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 8975 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 8976 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 8977 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 8978 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 8979 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 8980 | `						isSpecialIs = 1;` |
|       5 | 8981 | `					}` |
|      23 | 8982 | `				}` |
|      52 | 8983 | `				pInstr->iP1 = 0;` |
|      52 | 8984 | `				if( !isSpecialIs ){` |
|      38 | 8985 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 8986 | `				}` |
|      25 | 8987 | `			}` |
|  912651 | 8988 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 8989 | `			/* Prevent constant expansion for member/property names.` |
|       - | 8990 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 8991 | `			 * should not trigger constant lookup. */` |
|  105762 | 8992 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  105762 | 8993 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  105728 | 8994 | `				pInstr->iP1 = 0;` |
|   52863 | 8995 | `			}` |
|  105762 | 8996 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 8997 | `				/* Static member access,remember that */` |
|     184 | 8998 | `				iP1 = 1;` |
|     184 | 8999 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     184 | 9000 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      28 | 9001 | `					p3 = pInstr->p3;` |
|      28 | 9002 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      13 | 9003 | `				}` |
|      91 | 9004 | `			}` |
|   52880 | 9005 | `		}` |
|       - | 9006 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  938188 | 9007 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  469093 | 9008 | `	}` |
|  938300 | 9009 | `	if( nJmpIdx > 0 ){` |
|       - | 9010 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11588 | 9011 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11588 | 9012 | `		if( pInstr ){` |
|   11588 | 9013 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5793 | 9014 | `		}` |
|    5793 | 9015 | `	}` |
|  938300 | 9016 | `	return rc;` |
| 1236673 | 9017 |  |
|       - | 9018 | `/*` |
|       - | 9019 | ` * Compile a PHP expression.` |
|       - | 9020 | ` * According to the PHP language reference manual:` |
|       - | 9021 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 9022 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 9023 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 9024 | ` *  is "anything that has a value".` |
|       - | 9025 | ` * If something goes wrong while compiling the expression,this` |
|       - | 9026 | ` * function takes care of generating the appropriate error` |
|       - | 9027 | ` * message.` |
|       - | 9028 | ` */` |
|  668196 | 9029 | `static sxi32 PH7_CompileExpr(` |
|       - | 9030 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 9031 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 9032 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 9033 | `	)` |
|       2 | 9034 |  |
|       - | 9035 | `	ph7_expr_node *pRoot;` |
|       - | 9036 | `	SySet sExprNode;` |
|       - | 9037 | `	SyToken *pEnd;` |
|       - | 9038 | `	sxi32 nExpr;` |
|       - | 9039 | `	sxi32 iNest;` |
|       - | 9040 | `	sxi32 rc;` |
|       - | 9041 | `	/* Initialize worker variables */` |
|  668198 | 9042 | `	nExpr = 0;` |
|  668198 | 9043 | `	pRoot = 0;` |
|  668198 | 9044 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  668198 | 9045 | `	SySetAlloc(&sExprNode,0x10);` |
|  668198 | 9046 | `	rc = SXRET_OK;` |
|       - | 9047 | `	/* Delimit the expression */` |
|  668198 | 9048 | `	pEnd = pGen->pIn;` |
|  668198 | 9049 | `	iNest = 0;` |
| 4504120 | 9050 | `	while( pEnd < pGen->pEnd ){` |
| 4271000 | 9051 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 9052 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     236 | 9053 | `			iNest++;` |
| 4270883 | 9054 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     244 | 9055 | `			iNest--;` |
| 4270645 | 9056 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  435284 | 9057 | `			if( iNest <= 0 ){` |
|  435078 | 9058 | `				break;` |
|       - | 9059 | `			}` |
|     103 | 9060 | `		}` |
| 3835924 | 9061 | `		pEnd++;` |
|       2 | 9062 | `	}` |
|  668198 | 9063 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11272 | 9064 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11272 | 9065 | `		iNest = 0;` |
|       - | 9066 | `		/* Stop at the first comma */` |
|   22572 | 9067 | `		while( pEnd2 < pEnd ){` |
|   11306 | 9068 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      12 | 9069 | `				iNest++;` |
|   11301 | 9070 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      12 | 9071 | `				iNest--;` |
|   11291 | 9072 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       9 | 9073 | `				if( iNest <= 0 ){` |
|       5 | 9074 | `					break;` |
|       - | 9075 | `				}` |
|       2 | 9076 | `			}` |
|   11302 | 9077 | `			pEnd2++;` |
|       2 | 9078 | `		}` |
|   11272 | 9079 | `		if( pEnd2 <pEnd ){` |
|       5 | 9080 | `			pEnd = pEnd2;` |
|       2 | 9081 | `		}` |
|    5635 | 9082 | `	}` |
|  668198 | 9083 | `	if( pEnd > pGen->pIn ){` |
|  668188 | 9084 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 9085 | `		/* Swap delimiter */` |
|  668188 | 9086 | `		pGen->pEnd = pEnd;` |
|       - | 9087 | `		/* Try to get an expression tree */` |
|  668188 | 9088 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  668188 | 9089 | `		if( rc == SXRET_OK && pRoot ){` |
|  668018 | 9090 | `			rc = SXRET_OK;` |
|  668018 | 9091 | `			if( xTreeValidator ){` |
|       - | 9092 | `				/* Call the upper layer validator callback */` |
|   14360 | 9093 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7179 | 9094 | `			}` |
|  668018 | 9095 | `			if( rc != SXERR_ABORT ){` |
|       - | 9096 | `				/* Generate code for the given tree */` |
|  668018 | 9097 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  334008 | 9098 | `			}` |
|  668018 | 9099 | `			nExpr = 1;` |
|  334008 | 9100 | `		}` |
|       - | 9101 | `		/* Release the whole tree */` |
|  668188 | 9102 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 9103 | `		/* Synchronize token stream */` |
|  668188 | 9104 | `		pGen->pEnd = pTmp;` |
|  668188 | 9105 | `		pGen->pIn  = pEnd;` |
|  668188 | 9106 | `		if( rc == SXERR_ABORT ){` |
|      11 | 9107 | `			SySetRelease(&sExprNode);` |
|      11 | 9108 | `			return SXERR_ABORT;` |
|       - | 9109 | `		}` |
|  334088 | 9110 | `	}` |
|  668188 | 9111 | `	SySetRelease(&sExprNode);` |
|  668188 | 9112 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  334100 | 9113 |  |
|       - | 9114 | `/*` |
|       - | 9115 | ` * Return a pointer to the node construct handler associated` |
|       - | 9116 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 9117 | ` */` |
|  166548 | 9118 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 9119 |  |
|  166550 | 9120 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 9121 | `		/* Numeric literal: Either real or integer */` |
|   91210 | 9122 | `		return PH7_CompileNumLiteral;` |
|   75342 | 9123 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 9124 | `		/* Double quoted string */` |
|   16154 | 9125 | `		return PH7_CompileString;` |
|   59190 | 9126 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 9127 | `		/* Single quoted string */` |
|   59078 | 9128 | `		return PH7_CompileSimpleString;` |
|     114 | 9129 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 9130 | `		/* Heredoc */` |
|      66 | 9131 | `		return PH7_CompileHereDoc;` |
|      50 | 9132 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 9133 | `		/* Nowdoc */` |
|      44 | 9134 | `		return PH7_CompileNowDoc;` |
|       7 | 9135 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 9136 | `		/* Backtick quoted string */` |
|       5 | 9137 | `		return PH7_CompileBacktic;` |
|       - | 9138 | `	}` |
|       3 | 9139 | `	return 0;` |
|   83276 | 9140 |  |
|       - | 9141 | `/*` |
|       - | 9142 | ` * Compile an unset() statement.` |
|       - | 9143 | ` * unset($var, $arr[$key], ...);` |
|       - | 9144 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 9145 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 9146 | ` * parent array before extracting the element to unset.` |
|       - | 9147 | ` */` |
|    2700 | 9148 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 9149 |  |
|    2702 | 9150 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2702 | 9151 | `	sxu32 nIdx = 0;` |
|       - | 9152 | `	SyString sName;` |
|       - | 9153 | `	sxi32 rc;` |
|       - | 9154 | `	/* Jump the 'unset' keyword */` |
|    2702 | 9155 | `	pGen->pIn++;` |
|       - | 9156 | `	/* Save delimiter */` |
|    2702 | 9157 | `	pTmp = pGen->pEnd;` |
|       - | 9158 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2702 | 9159 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2702 | 9160 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 9161 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 9162 | `		SyToken *pClose;` |
|    2702 | 9163 | `		pGen->pIn++;   /* Skip '(' */` |
|    2702 | 9164 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2702 | 9165 | `		pEnd = pClose; /* Stop at ')' */` |
|    1350 | 9166 | `	}` |
|    2702 | 9167 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 9168 | `	/* Resolve the 'unset' builtin name once */` |
|    2702 | 9169 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     326 | 9170 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     326 | 9171 | `		if( pObj == 0 ){` |
|     ! 0 | 9172 | `			return SXERR_ABORT;` |
|       - | 9173 | `		}` |
|     326 | 9174 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     326 | 9175 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     162 | 9176 | `	}` |
|       - | 9177 | `	/* Compile each comma-separated argument */` |
|    8970 | 9178 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6270 | 9179 | `		if( pGen->pIn < pNext ){` |
|    6270 | 9180 | `			pGen->pEnd = pNext;` |
|    6270 | 9181 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 9182 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6270 | 9183 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 9184 | `				return SXERR_ABORT;` |
|       - | 9185 | `			}` |
|    6270 | 9186 | `			if( rc != SXERR_EMPTY ){` |
|       - | 9187 | `				/* Emit call for this single argument */` |
|    6268 | 9188 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6268 | 9189 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6268 | 9190 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3133 | 9191 | `			}` |
|    3134 | 9192 | `		}` |
|       - | 9193 | `		/* Jump trailing commas */` |
|    9840 | 9194 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3572 | 9195 | `			pNext++;` |
|       2 | 9196 | `		}` |
|    6270 | 9197 | `		pGen->pIn = pNext;` |
|       2 | 9198 | `	}` |
|       - | 9199 | `	/* Skip past the closing ')' if present */` |
|    2702 | 9200 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2702 | 9201 | `		pGen->pIn++;` |
|    1350 | 9202 | `	}` |
|       - | 9203 | `	/* Restore token stream */` |
|    2702 | 9204 | `	pGen->pEnd = pTmp;` |
|    2702 | 9205 | `	return SXRET_OK;` |
|    1352 | 9206 |  |
|       - | 9207 | `/*` |
|       - | 9208 | ` * PHP Language construct table.` |
|       - | 9209 | ` */` |
|       - | 9210 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 9211 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 9212 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 9213 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 9214 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 9215 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 9216 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 9217 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 9218 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 9219 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 9220 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 9221 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 9222 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 9223 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 9224 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 9225 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 9226 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 9227 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 9228 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 9229 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 9230 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 9231 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 9232 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 9233 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 9234 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 9235 | `};` |
|       - | 9236 | `/*` |
|       - | 9237 | ` * Return a pointer to the statement handler routine associated` |
|       - | 9238 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 9239 | ` */` |
|  405194 | 9240 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 9241 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 9242 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 9243 | `	)` |
|       2 | 9244 |  |
|  405196 | 9245 | `	sxu32 n = 0;` |
| 1704220 | 9246 | `	for(;;){` |
| 3408442 | 9247 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   47514 | 9248 | `			break;` |
|       - | 9249 | `		}` |
| 3360930 | 9250 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  357684 | 9251 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 9252 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 9253 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 9254 | `					/* 'static' (class context),return null */` |
|     ! 0 | 9255 | `					return 0;` |
|       - | 9256 | `				}` |
|     ! 0 | 9257 | `			}` |
|  357682 | 9258 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 9259 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 9260 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 9261 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 9262 | `				return 0;` |
|       - | 9263 | `			}` |
|       - | 9264 | `			/* Return a pointer to the handler.` |
|       - | 9265 | `			*/` |
|  357684 | 9266 | `			return aLangConstruct[n].xConstruct;` |
|       - | 9267 | `		}` |
| 3003248 | 9268 | `		n++;` |
|       2 | 9269 | `	}` |
|   47514 | 9270 | `	if( pLookahed ){` |
|   47514 | 9271 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8284 | 9272 | `			return PH7_CompileClassInterface;` |
|   39232 | 9273 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   39026 | 9274 | `			return PH7_CompileClass;` |
|     208 | 9275 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 9276 | `			return PH7_CompileTrait;` |
|     152 | 9277 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 9278 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 9279 | `				return PH7_CompileAbstractClass;` |
|     136 | 9280 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 9281 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 9282 | `				return PH7_CompileFinalClass;` |
|       - | 9283 | `		}` |
|      67 | 9284 | `	}` |
|       - | 9285 | `	/* Not a language construct */` |
|     136 | 9286 | `	return 0;` |
|  202599 | 9287 |  |
|       - | 9288 | `/*` |
|       - | 9289 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 9290 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 9291 | ` */` |
|     134 | 9292 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 9293 |  |
|       - | 9294 | `	int rc;` |
|     136 | 9295 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 9296 | `	if( rc == FALSE ){` |
|      40 | 9297 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 9298 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 9299 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 9300 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 9301 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 9302 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 9303 | `			*/` |
|       - | 9304 | `			){` |
|      34 | 9305 | `				rc = TRUE;` |
|      16 | 9306 | `		}` |
|      20 | 9307 | `	}` |
|     136 | 9308 | `	return rc;` |
|       2 | 9309 |  |
|       - | 9310 | `/*` |
|       - | 9311 | ` * Compile a PHP chunk.` |
|       - | 9312 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 9313 | ` * takes care of generating the appropriate error message.` |
|       - | 9314 | ` */` |
|  543934 | 9315 | `static sxi32 GenStateCompileChunk(` |
|       - | 9316 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 9317 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 9318 | `	)` |
|       2 | 9319 |  |
|       - | 9320 | `	ProcLangConstruct xCons;` |
|       - | 9321 | `	sxi32 rc;` |
|  543936 | 9322 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  325099 | 9323 | `	for(;;){` |
|  650200 | 9324 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 9325 | `			/* No more input to process */` |
|   11860 | 9326 | `			break;` |
|       - | 9327 | `		}` |
|  638342 | 9328 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 9329 | `			/* Compile block */` |
|      12 | 9330 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 9331 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 9332 | `				break;` |
|       - | 9333 | `			}` |
|       7 | 9334 | `		}else{` |
|  638332 | 9335 | `			xCons = 0;` |
|  638332 | 9336 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  405196 | 9337 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 9338 | `				/* Try to extract a language construct handler */` |
|  405196 | 9339 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  405196 | 9340 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 9341 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 9342 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 9343 | `						&pGen->pIn->sData);` |
|       9 | 9344 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 9345 | `						break;` |
|       - | 9346 | `					}` |
|       - | 9347 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 9348 | `					 * this erroneous statement.` |
|       - | 9349 | `					 */` |
|       9 | 9350 | `					xCons = PH7_ErrorRecover;` |
|       4 | 9351 | `				}` |
|  435735 | 9352 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   40750 | 9353 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 9354 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 9355 | `				xCons = PH7_CompileLabel;` |
|      56 | 9356 | `			}` |
|  638332 | 9357 | `			if( xCons == 0 ){` |
|       - | 9358 | `				/* Assume an expression an try to compile it */` |
|  233152 | 9359 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  233152 | 9360 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 9361 | `					/* Pop l-value */` |
|  233014 | 9362 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  116506 | 9363 | `				}` |
|  116577 | 9364 | `			}else{` |
|       - | 9365 | `				/* Go compile the sucker */` |
|  405182 | 9366 | `				rc = xCons(&(*pGen));` |
|       - | 9367 | `			}` |
|  638332 | 9368 | `			if( rc == SXERR_ABORT ){` |
|       - | 9369 | `				/* Request to abort compilation */` |
|      11 | 9370 | `				break;` |
|       - | 9371 | `			}` |
|       - | 9372 | `		}` |
|       - | 9373 | `		/* Ignore trailing semi-colons ';' */` |
| 1057274 | 9374 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  418944 | 9375 | `			pGen->pIn++;` |
|       2 | 9376 | `		}` |
|  638332 | 9377 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 9378 | `			/* Compile a single statement and return */` |
|  532068 | 9379 | `			break;` |
|       - | 9380 | `		}` |
|       - | 9381 | `		/* LOOP ONE */` |
|       - | 9382 | `		/* LOOP TWO */` |
|       - | 9383 | `		/* LOOP THREE */` |
|       - | 9384 | `		/* LOOP FOUR */` |
|       2 | 9385 | `	}` |
|       - | 9386 | `	/* Return compilation status */` |
|  543936 | 9387 | `	return rc;` |
|       2 | 9388 |  |
|       - | 9389 | `/*` |
|       - | 9390 | ` * Compile a Raw PHP chunk.` |
|       - | 9391 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 9392 | ` * takes care of generating the appropriate error message.` |
|       - | 9393 | ` */` |
|   11870 | 9394 | `static sxi32 PH7_CompilePHP(` |
|       - | 9395 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 9396 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 9397 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 9398 | `	)` |
|       2 | 9399 |  |
|   11872 | 9400 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 9401 | `	sxi32 rc;` |
|       - | 9402 | `	/* Reset the token set */` |
|   11872 | 9403 | `	SySetReset(&(*pTokenSet));` |
|       - | 9404 | `	/* Mark as the default token set */` |
|   11872 | 9405 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 9406 | `	/* Advance the stream cursor */` |
|   11872 | 9407 | `	pGen->pRawIn++;` |
|       - | 9408 | `	/* Tokenize the PHP chunk first */` |
|   11872 | 9409 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 9410 | `	/* Point to the head and tail of the token stream. */` |
|   11872 | 9411 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11872 | 9412 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11872 | 9413 | `	if( is_expr ){` |
|     ! 0 | 9414 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 9415 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 9416 | `			/* A simple expression,compile it */` |
|     ! 0 | 9417 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 9418 | `		}` |
|       - | 9419 | `		/* Emit the DONE instruction */` |
|     ! 0 | 9420 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 9421 | `		return SXRET_OK;` |
|       - | 9422 | `	}` |
|   11872 | 9423 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 9424 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 9425 | `		/*` |
|       - | 9426 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 9427 | `		 * According to the PHP reference manual:` |
|       - | 9428 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 9429 | `		 *  immediately follow` |
|       - | 9430 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 9431 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 9432 | `		 * Symisc extension:` |
|       - | 9433 | `		 *   This short syntax works with all PHP opening` |
|       - | 9434 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 9435 | `		 *   only short tag.` |
|       - | 9436 | `		 */` |
|       - | 9437 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 9438 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 9439 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 9440 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 9441 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 9442 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 9443 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 9444 | `		}` |
|       3 | 9445 | `		return SXRET_OK;` |
|       - | 9446 | `	}` |
|       - | 9447 | `	/* Compile the PHP chunk */` |
|   11870 | 9448 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 9449 | `	/* Fix exceptions jumps */` |
|   11870 | 9450 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 9451 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11870 | 9452 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 9453 | `		rc = SXERR_ABORT;` |
|       1 | 9454 | `	}` |
|       - | 9455 | `	/* Reset container */` |
|   11870 | 9456 | `	SySetReset(&pGen->aGoto);` |
|   11870 | 9457 | `	SySetReset(&pGen->aLabel);` |
|       - | 9458 | `	/* Compilation result */` |
|   11870 | 9459 | `	return rc;` |
|    5937 | 9460 |  |
|       - | 9461 | `/*` |
|       - | 9462 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 9463 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 9464 | ` * This is the only compile interface exported from this file.` |
|       - | 9465 | ` */` |
|   14016 | 9466 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 9467 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 9468 | `	SyString *pScript,  /* Script to compile */` |
|       - | 9469 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 9470 | `	)` |
|       2 | 9471 |  |
|       - | 9472 | `	SySet aPhpToken,aRawToken;` |
|       - | 9473 | `	ph7_gen_state *pCodeGen;` |
|       - | 9474 | `	ph7_value *pRawObj;` |
|       - | 9475 | `	sxu32 nObjIdx;` |
|       - | 9476 | `	sxi32 nRawObj;` |
|       - | 9477 | `	int is_expr;` |
|       - | 9478 | `	sxi32 rc;` |
|   14018 | 9479 | `	if( pScript->nByte < 1 ){` |
|       - | 9480 | `		/* Nothing to compile */` |
|     ! 0 | 9481 | `		return PH7_OK;` |
|       - | 9482 | `	}` |
|       - | 9483 | `	/* Initialize the tokens containers */` |
|   14018 | 9484 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14018 | 9485 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14018 | 9486 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14018 | 9487 | `	is_expr = 0;` |
|   14018 | 9488 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 9489 | `		SyToken sTmp;` |
|       - | 9490 | `		/* PHP only: -*/` |
|    2778 | 9491 | `		sTmp.nLine = 1;` |
|    2778 | 9492 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2778 | 9493 | `		sTmp.pUserData = 0;` |
|    2778 | 9494 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2778 | 9495 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2778 | 9496 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 9497 | `			/* A simple PHP expression */` |
|     ! 0 | 9498 | `			is_expr = 1;` |
|     ! 0 | 9499 | `		}` |
|    1390 | 9500 | `	}else{` |
|       - | 9501 | `		/* Tokenize raw text */` |
|   11242 | 9502 | `		SySetAlloc(&aRawToken,32);` |
|   11242 | 9503 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 9504 | `	}` |
|   14018 | 9505 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 9506 | `	/* Process high-level tokens */` |
|   14018 | 9507 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14018 | 9508 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14018 | 9509 | `	rc = PH7_OK;` |
|   14018 | 9510 | `	if( is_expr ){` |
|       - | 9511 | `		/* Compile the expression */` |
|     ! 0 | 9512 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 9513 | `		goto cleanup;` |
|       - | 9514 | `	}` |
|   14018 | 9515 | `	nObjIdx = 0;` |
|       - | 9516 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 9517 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 9518 | `	 * preventing namespace bleeding across include()d files. */` |
|   14018 | 9519 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 9520 | `	/* Start the compilation process */` |
|   12632 | 9521 | `	for(;;){` |
|   37124 | 9522 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14006 | 9523 | `			break; /* No more tokens to process */` |
|       - | 9524 | `		}` |
|   23120 | 9525 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 9526 | `			/* Compile the PHP chunk */` |
|   11872 | 9527 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11872 | 9528 | `			if( rc == SXERR_ABORT ){` |
|      13 | 9529 | `				break;` |
|       - | 9530 | `			}` |
|   11860 | 9531 | `			continue;` |
|       - | 9532 | `		}` |
|       - | 9533 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11250 | 9534 | `		nRawObj = 0;` |
|   22498 | 9535 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 9536 | `			/* Consume the raw chunk without any processing */` |
|   11250 | 9537 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11250 | 9538 | `			if( pRawObj == 0 ){` |
|     ! 0 | 9539 | `				rc = SXERR_MEM;` |
|     ! 0 | 9540 | `				break;` |
|       - | 9541 | `			}` |
|       - | 9542 | `			/* Mark as constant and emit the load constant instruction */` |
|   11250 | 9543 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11250 | 9544 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11250 | 9545 | `			++nRawObj;` |
|   11250 | 9546 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 9547 | `		}` |
|   11250 | 9548 | `		if( nRawObj > 0 ){` |
|       - | 9549 | `			/* Emit the consume instruction */` |
|   11250 | 9550 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5624 | 9551 | `		}` |
|    7010 | 9552 | `	}` |
|    7008 | 9553 | `cleanup:` |
|   14018 | 9554 | `	SySetRelease(&aRawToken);` |
|   14018 | 9555 | `	SySetRelease(&aPhpToken);` |
|   14018 | 9556 | `	return rc;` |
|    7010 | 9557 |  |
|       - | 9558 | `/*` |
|       - | 9559 | ` * Utility routines.Initialize the code generator.` |
|       - | 9560 | ` */` |
|    2748 | 9561 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 9562 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 9563 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 9564 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 9565 | `	)` |
|       2 | 9566 |  |
|    2750 | 9567 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 9568 | `	/* Zero the structure */` |
|    2750 | 9569 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 9570 | `	/* Initial state */` |
|    2750 | 9571 | `	pGen->pVm  = &(*pVm);` |
|    2750 | 9572 | `	pGen->xErr = xErr;` |
|    2750 | 9573 | `	pGen->pErrData = pErrData;` |
|    2750 | 9574 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2750 | 9575 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2750 | 9576 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2750 | 9577 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 9578 | `	/* Error log buffer */` |
|    2750 | 9579 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 9580 | `	/* General purpose working buffer */` |
|    2750 | 9581 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 9582 | `	/* Namespace state */` |
|    2750 | 9583 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2750 | 9584 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2750 | 9585 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2750 | 9586 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 9587 | `	/* Create the global scope */` |
|    2750 | 9588 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 9589 | `	/* Point to the global scope */` |
|    2750 | 9590 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2750 | 9591 | `	return SXRET_OK;` |
|       2 | 9592 |  |
|       - | 9593 | `/*` |
|       - | 9594 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 9595 | ` */` |
|   16490 | 9596 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 9597 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 9598 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 9599 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 9600 | `	)` |
|       2 | 9601 |  |
|   16492 | 9602 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 9603 | `	GenBlock *pBlock,*pParent;` |
|       - | 9604 | `	/* Reset state */` |
|   16492 | 9605 | `	SySetReset(&pGen->aLabel);` |
|   16492 | 9606 | `	SySetReset(&pGen->aGoto);` |
|   16492 | 9607 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16492 | 9608 | `	SyBlobRelease(&pGen->sWorker);` |
|   16492 | 9609 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16492 | 9610 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16492 | 9611 | `	SyHashRelease(&pGen->hUseImports);` |
|   16492 | 9612 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16492 | 9613 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16492 | 9614 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16492 | 9615 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16492 | 9616 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 9617 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 9618 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 9619 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 9620 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 9621 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 9622 | `	 * number of unique names, which is acceptable. */` |
|       - | 9623 | `	/* Point to the global scope */` |
|   16492 | 9624 | `	pBlock = pGen->pCurrent;` |
|   16492 | 9625 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 9626 | `		pParent = pBlock->pParent;` |
|     ! 0 | 9627 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 9628 | `		pBlock = pParent;` |
|     ! 0 | 9629 | `	}` |
|   16492 | 9630 | `	pGen->xErr = xErr;` |
|   16492 | 9631 | `	pGen->pErrData = pErrData;` |
|   16492 | 9632 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16492 | 9633 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16492 | 9634 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16492 | 9635 | `	pGen->nErr = 0;` |
|   16492 | 9636 | `	return SXRET_OK;` |
|       2 | 9637 |  |
|       - | 9638 | `/*` |
|       - | 9639 | ` * Generate a compile-time error message.` |
|       - | 9640 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 9641 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 9642 | ` * abort compilation immediately.` |
|       - | 9643 | ` */` |
|     506 | 9644 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 9645 |  |
|     508 | 9646 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     508 | 9647 | `	const char *zErr = "Error";` |
|       - | 9648 | `	SyString *pFile;` |
|       - | 9649 | `	va_list ap;` |
|       - | 9650 | `	sxi32 rc;` |
|       - | 9651 | `	/* Reset the working buffer */` |
|     508 | 9652 | `	SyBlobReset(pWorker);` |
|       - | 9653 | `	/* Peek the processed file path if available */` |
|     508 | 9654 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     508 | 9655 | `	if( nErrType == E_ERROR ){` |
|       - | 9656 | `		/* Increment the error counter */` |
|     430 | 9657 | `		pGen->nErr++;` |
|     430 | 9658 | `		if( pGen->nErr > 15 ){` |
|       - | 9659 | `			/* Error count limit reached */` |
|       5 | 9660 | `			if( pGen->xErr ){` |
|       5 | 9661 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 9662 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 9663 | `				if( pFile ){` |
|       5 | 9664 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 9665 | `				}` |
|       5 | 9666 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 9667 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 9668 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 9669 | `				}` |
|       2 | 9670 | `			}` |
|       - | 9671 | `			/* Abort immediately */` |
|       5 | 9672 | `			return SXERR_ABORT;` |
|       - | 9673 | `		}` |
|     212 | 9674 | `	}` |
|     504 | 9675 | `	if( pGen->xErr == 0 ){` |
|       - | 9676 | `		/* No available error consumer,return immediately */` |
|       3 | 9677 | `		return SXRET_OK;` |
|       - | 9678 | `	}` |
|     501 | 9679 | `	switch(nErrType){` |
|     423 | 9680 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 9681 | `	case E_WARNING: zErr = "Warning";     break;` |
|      43 | 9682 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 9683 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 9684 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 9685 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 9686 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 9687 | `	default:` |
|     ! 0 | 9688 | `		break;` |
|       - | 9689 | `	}` |
|     501 | 9690 | `	rc = SXRET_OK;` |
|       - | 9691 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     501 | 9692 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     501 | 9693 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     501 | 9694 | `	va_start(ap,zFormat);` |
|     501 | 9695 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     501 | 9696 | `	va_end(ap);` |
|     501 | 9697 | `	if( pFile ){` |
|     501 | 9698 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     250 | 9699 | `	}` |
|       - | 9700 | `	/* Append a new line */` |
|     501 | 9701 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     501 | 9702 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 9703 | `		/* Consume the generated error message */` |
|     501 | 9704 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     250 | 9705 | `	}` |
|     501 | 9706 | `	return rc;` |
|     255 | 9707 |  |
|       - | 9708 |  |
