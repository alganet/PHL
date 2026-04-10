# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3970/5098 lines (77.87%)

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
|    2950 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2952 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8285 |  131 | `	for(;;){` |
|   16572 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2844 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2844 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2818 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      13 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13756 |  140 | `		pBlock = pBlock->pParent;` |
|   13756 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1477 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  572986 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  572988 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  572988 |  162 | `	pBlock->pUserData   = pUserData;` |
|  572988 |  163 | `	pBlock->pGen        = pGen;` |
|  572988 |  164 | `	pBlock->iFlags      = iType;` |
|  572988 |  165 | `	pBlock->pParent     = 0;` |
|  572988 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  572988 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  572988 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  570302 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  570304 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  570304 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  570304 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  570304 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  570304 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  570304 |  200 | `	pGen->pCurrent = pBlock;` |
|  570304 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  275974 |  203 | `		*ppBlock = pBlock;` |
|  137986 |  204 | `	}` |
|  570304 |  205 | `	return SXRET_OK;` |
|  285153 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  570294 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  570296 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  570296 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  570296 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  570294 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  570296 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  570296 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  570296 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  570296 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  570294 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  570296 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  570296 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  570296 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  570296 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  570296 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  570296 |  244 | `	return SXRET_OK;` |
|  285149 |  245 |  |
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
|  173858 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  173860 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  173860 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  173860 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  173860 |  265 | `	return rc;` |
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
|  406024 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  406026 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  744804 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  338780 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  131932 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  206850 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32994 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  173858 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  173858 |  298 | `		if( pInstr ){` |
|  173858 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  173858 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  173858 |  302 | `			aFix[n].nJumpType = -1;` |
|   86928 |  303 | `		}` |
|   86930 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  406026 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  154978 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  154980 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  155126 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  154978 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  155110 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  154978 |  358 | `	return SXRET_OK;` |
|   77491 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  504628 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  504630 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  504630 |  367 | `	if( pEntry == 0 ){` |
|  248844 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  255788 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  255788 |  371 | `	return SXRET_OK;` |
|  252316 |  372 |  |
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
|  248842 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  248844 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  248844 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  124421 |  387 | `	}` |
|  248844 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   88474 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   88476 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   88476 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   88476 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   88476 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   88476 |  408 | `	return pObj;` |
|   44239 |  409 |  |
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
|   88986 |  454 | `static int GenStateFindBadNumericSeparator(` |
|       - |  455 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |  456 |  |
|   88988 |  457 | `	const char *z = pRaw->zString;` |
|   88988 |  458 | `	sxu32 n = pRaw->nByte;` |
|   88988 |  459 | `	int base = 10;` |
|       - |  460 | `	sxu32 i, start;` |
|   88988 |  461 | `	if( n < 2 ) return 0;` |
|    8024 |  462 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |  463 | `		base = 16;` |
|    7989 |  464 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |  465 | `		base = 2;` |
|     139 |  466 | `	}` |
|   29986 |  467 | `	for( i = 0; i < n; ++i ){` |
|   21978 |  468 | `		if( z[i] != '_' ) continue;` |
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
|    8010 |  485 | `	return 0;` |
|   44495 |  486 |  |
|       - |  487 | `/*` |
|       - |  488 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |  489 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |  490 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |  491 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |  492 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |  493 | ` * so callers can bail from the current construct).` |
|       - |  494 | ` */` |
|   88986 |  495 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |  496 |  |
|   88988 |  497 | `	const char *zBad = 0;` |
|   88988 |  498 | `	sxu32 nBad = 0;` |
|       - |  499 | `	SyString sBad;` |
|       - |  500 | `	sxi32 rc;` |
|   88988 |  501 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   88974 |  502 | `		return SXRET_OK;` |
|       - |  503 | `	}` |
|      15 |  504 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |  505 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |  506 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |  507 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  508 | `		return SXERR_ABORT;` |
|       - |  509 | `	}` |
|      15 |  510 | `	return SXERR_SYNTAX;` |
|   44495 |  511 |  |
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
|   88972 |  528 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |  529 | `	SyMemBackend *pAlloc,` |
|       - |  530 | `	const SyString *pToken,` |
|       - |  531 | `	char *zScratch, sxu32 nScratch,` |
|       - |  532 | `	SyString *pOut, char **pzAlloc)` |
|       2 |  533 |  |
|       - |  534 | `	sxu32 i, j;` |
|   88974 |  535 | `	int hasUnderscore = 0;` |
|       - |  536 | `	char *zBuf;` |
|   88974 |  537 | `	*pzAlloc = 0;` |
|  189834 |  538 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  101114 |  539 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   50432 |  540 | `	}` |
|   88974 |  541 | `	if( !hasUnderscore ){` |
|   88722 |  542 | `		SyStringDupPtr(pOut, pToken);` |
|   88722 |  543 | `		return SXRET_OK;` |
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
|   44488 |  560 |  |
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
|   88958 |  577 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  578 |  |
|   88960 |  579 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   88960 |  580 | `	sxu32 nIdx = 0;` |
|       - |  581 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   88960 |  582 | `	char *zAlloc = 0;` |
|       - |  583 | `	SyString sNum;` |
|       - |  584 | `	sxi32 rc;` |
|   44479 |  585 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   88960 |  586 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   88960 |  587 | `	if( rc != SXRET_OK ){` |
|      11 |  588 | `		return rc;` |
|       - |  589 | `	}` |
|  133424 |  590 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   44474 |  591 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   88950 |  592 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  593 | `		return SXERR_ABORT;` |
|       - |  594 | `	}` |
|   88950 |  595 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  596 | `		ph7_value *pObj;` |
|       - |  597 | `		sxi64 iValue;` |
|   88476 |  598 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   88476 |  599 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   88476 |  600 | `		if( pObj == 0 ){` |
|     ! 0 |  601 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |  602 | `			return SXERR_ABORT;` |
|       - |  603 | `		}` |
|   88476 |  604 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   44239 |  605 | `	}else{` |
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
|   88950 |  618 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |  619 | `	/* Emit the load constant instruction */` |
|   88950 |  620 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  621 | `	/* Node successfully compiled */` |
|   88950 |  622 | `	return SXRET_OK;` |
|   44481 |  623 |  |
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
|   57914 |  635 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  636 |  |
|   57916 |  637 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  638 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  639 | `	ph7_value *pObj;` |
|       - |  640 | `	sxu32 nIdx;` |
|   57916 |  641 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  642 | `	/* Delimit the string */` |
|   57916 |  643 | `	zIn  = pStr->zString;` |
|   57916 |  644 | `	zEnd = &zIn[pStr->nByte];` |
|   57916 |  645 | `	if( zIn >= zEnd ){` |
|       - |  646 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  647 | `		 * rather than reserving a new object each time. */` |
|     140 |  648 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     140 |  649 | `		return SXRET_OK;` |
|       - |  650 | `	}` |
|   57778 |  651 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  652 | `		/* Already processed,emit the load constant instruction` |
|       - |  653 | `		 * and return.` |
|       - |  654 | `		 */` |
|   17126 |  655 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17126 |  656 | `		return SXRET_OK;` |
|       - |  657 | `	}` |
|       - |  658 | `	/* Reserve a new constant */` |
|   40654 |  659 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   40654 |  660 | `	if( pObj == 0 ){` |
|     ! 0 |  661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  662 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  663 | `		return SXERR_ABORT;` |
|       - |  664 | `	}` |
|   40654 |  665 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  666 | `	/* Compile the node */` |
|   40694 |  667 | `	for(;;){` |
|   81390 |  668 | `		if( zIn >= zEnd ){` |
|       - |  669 | `			/* End of input */` |
|   40654 |  670 | `			break;` |
|       - |  671 | `		}` |
|   40738 |  672 | `		zCur = zIn;` |
|  646840 |  673 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  606104 |  674 | `			zIn++;` |
|       2 |  675 | `		}` |
|   40738 |  676 | `		if( zIn > zCur ){` |
|       - |  677 | `			/* Append raw contents*/` |
|   40718 |  678 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20358 |  679 | `		}` |
|   40738 |  680 | `		zIn++;` |
|   40738 |  681 | `		if( zIn < zEnd ){` |
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
|   40738 |  696 | `		zIn++;` |
|       2 |  697 | `	}` |
|       - |  698 | `	/* Emit the load constant instruction */` |
|   40654 |  699 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   40654 |  700 | `	if( pStr->nByte < 1024 ){` |
|       - |  701 | `		/* Install in the literal table */` |
|   40654 |  702 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20326 |  703 | `	}` |
|       - |  704 | `	/* Node successfully compiled */` |
|   40654 |  705 | `	return SXRET_OK;` |
|   28959 |  706 |  |
|       - |  707 | `/*` |
|       - |  708 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |  709 | ` *` |
|       - |  710 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |  711 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |  712 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |  713 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |  714 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |  715 | ` *` |
|       - |  716 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |  717 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |  718 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |  719 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |  720 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |  721 | ` *     whitespace.` |
|       - |  722 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |  723 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |  724 | ` */` |
|     104 |  725 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |  726 |  |
|     106 |  727 | `	SyString *pIn = &pGen->pIn->sData;` |
|     106 |  728 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  729 | `	const char *zPrefix;` |
|       - |  730 | `	const char *z, *zEnd;` |
|       - |  731 | `	char *zBuf, *zDst;` |
|     106 |  732 | `	if( nIndent == 0 ){` |
|       - |  733 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      62 |  734 | `		*pOut = *pIn;` |
|      62 |  735 | `		return SXRET_OK;` |
|       - |  736 | `	}` |
|       - |  737 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |  738 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |  739 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |  740 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |  741 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |  742 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |  743 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |  744 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |  745 | `		zPrefix += 2;` |
|     ! 0 |  746 | `	}else{` |
|      46 |  747 | `		zPrefix += 1;` |
|       - |  748 | `	}` |
|       - |  749 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |  750 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |  751 | `	if( zBuf == 0 ){` |
|     ! 0 |  752 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  753 | `		return SXERR_ABORT;` |
|       - |  754 | `	}` |
|      46 |  755 | `	zDst = zBuf;` |
|      46 |  756 | `	z = pIn->zString;` |
|      46 |  757 | `	zEnd = z + pIn->nByte;` |
|     128 |  758 | `	while( z < zEnd ){` |
|      70 |  759 | `		const char *zLine = z;` |
|       - |  760 | `		sxu32 nLine;` |
|       - |  761 | `		int bEmpty;` |
|     798 |  762 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |  763 | `			z++;` |
|       2 |  764 | `		}` |
|      70 |  765 | `		nLine = (sxu32)(z - zLine);` |
|      70 |  766 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |  767 | `		if( !bEmpty ){` |
|       - |  768 | `			sxu32 i;` |
|      66 |  769 | `			if( nLine < nIndent ){` |
|     ! 0 |  770 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  771 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |  772 | `					nIndent);` |
|     ! 0 |  773 | `				return SXERR_ABORT;` |
|       - |  774 | `			}` |
|     268 |  775 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |  776 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |  777 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |  778 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |  779 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  780 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |  781 | `					}else{` |
|       7 |  782 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  783 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |  784 | `							nIndent);` |
|       - |  785 | `					}` |
|       9 |  786 | `					return SXERR_ABORT;` |
|       - |  787 | `				}` |
|     103 |  788 | `			}` |
|      57 |  789 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |  790 | `			zDst += nLine - nIndent;` |
|      33 |  791 | `		}else if( nLine == 1 ){` |
|       - |  792 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |  793 | `			*zDst++ = '\r';` |
|     ! 0 |  794 | `		}` |
|      61 |  795 | `		if( z < zEnd ){` |
|      25 |  796 | `			*zDst++ = '\n';` |
|      25 |  797 | `			z++;` |
|      12 |  798 | `		}` |
|       1 |  799 | `	}` |
|      37 |  800 | `	pOut->zString = zBuf;` |
|      37 |  801 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |  802 | `	return SXRET_OK;` |
|      54 |  803 |  |
|       - |  804 | `/*` |
|       - |  805 | ` * Compile a nowdoc string.` |
|       - |  806 | ` * According to the PHP language reference manual:` |
|       - |  807 | ` *` |
|       - |  808 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |  809 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |  810 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |  811 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |  812 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |  813 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |  814 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |  815 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |  816 | ` *  of the closing identifier.` |
|       - |  817 | ` */` |
|      42 |  818 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  819 |  |
|       - |  820 | `	SyString sStripped;` |
|       - |  821 | `	SyString *pStr;` |
|       - |  822 | `	ph7_value *pObj;` |
|       - |  823 | `	sxu32 nIdx;` |
|       - |  824 | `	sxi32 rc;` |
|      44 |  825 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |  826 | `	if( rc != SXRET_OK ){` |
|       5 |  827 | `		return rc;` |
|       - |  828 | `	}` |
|      40 |  829 | `	pStr = &sStripped;` |
|      40 |  830 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |  831 | `	if( pStr->nByte <= 0 ){` |
|       - |  832 | `		/* Empty string,load NULL */` |
|       7 |  833 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  834 | `		return SXRET_OK;` |
|       - |  835 | `	}` |
|       - |  836 | `	/* Reserve a new constant */` |
|      34 |  837 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |  838 | `	if( pObj == 0 ){` |
|     ! 0 |  839 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  840 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  841 | `		return SXERR_ABORT;` |
|       - |  842 | `	}` |
|       - |  843 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |  844 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |  845 | `	/* Emit the load constant instruction */` |
|      34 |  846 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  847 | `	/* Node successfully compiled */` |
|      34 |  848 | `	return SXRET_OK;` |
|      23 |  849 |  |
|       - |  850 | `/*` |
|       - |  851 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |  852 | ` * According to the PHP language reference manual` |
|       - |  853 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |  854 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |  855 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |  856 | ` *  property in a string with a minimum of effort.` |
|       - |  857 | ` *  Simple syntax` |
|       - |  858 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |  859 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |  860 | ` *   the end of the name.` |
|       - |  861 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |  862 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |  863 | ` *   as to simple variables.` |
|       - |  864 | ` *  Complex (curly) syntax` |
|       - |  865 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |  866 | ` *   of complex expressions.` |
|       - |  867 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |  868 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |  869 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |  870 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |  871 | ` */` |
|    1794 |  872 | `static sxi32 GenStateProcessStringExpression(` |
|       - |  873 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  874 | `	sxu32 nLine,         /* Line number */` |
|       - |  875 | `	const char *zIn,     /* Raw expression */` |
|       - |  876 | `	const char *zEnd     /* End of the expression */` |
|       - |  877 | `	)` |
|       2 |  878 |  |
|       - |  879 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  880 | `	SySet sToken;` |
|       - |  881 | `	sxi32 rc;` |
|       - |  882 | `	/* Initialize the token set */` |
|    1796 |  883 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  884 | `	/* Preallocate some slots */` |
|    1796 |  885 | `	SySetAlloc(&sToken,0x08);` |
|       - |  886 | `	/* Tokenize the text */` |
|    1796 |  887 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  888 | `	/* Swap delimiter */` |
|    1796 |  889 | `	pTmpIn  = pGen->pIn;` |
|    1796 |  890 | `	pTmpEnd = pGen->pEnd;` |
|    1796 |  891 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1796 |  892 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  893 | `	/* Compile the expression */` |
|    1796 |  894 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  895 | `	/* Restore token stream */` |
|    1796 |  896 | `	pGen->pIn  = pTmpIn;` |
|    1796 |  897 | `	pGen->pEnd = pTmpEnd;` |
|       - |  898 | `	/* Release the token set */` |
|    1796 |  899 | `	SySetRelease(&sToken);` |
|       - |  900 | `	/* Compilation result */` |
|    1796 |  901 | `	return rc;` |
|       2 |  902 |  |
|       - |  903 | `/*` |
|       - |  904 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  905 | ` */` |
|   17096 |  906 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  907 |  |
|       - |  908 | `	ph7_value *pConstObj;` |
|   17098 |  909 | `	sxu32 nIdx = 0;` |
|       - |  910 | `	/* Reserve a new constant */` |
|   17098 |  911 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   17098 |  912 | `	if( pConstObj == 0 ){` |
|     ! 0 |  913 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  914 | `		return 0;` |
|       - |  915 | `	}` |
|   17098 |  916 | `	(*pCount)++;` |
|   17098 |  917 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  918 | `	/* Emit the load constant instruction */` |
|   17098 |  919 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17098 |  920 | `	return pConstObj;` |
|    8550 |  921 |  |
|       - |  922 | `/*` |
|       - |  923 | ` * Compile a double quoted/heredoc string.` |
|       - |  924 | ` * According to the PHP language reference manual` |
|       - |  925 | ` * Heredoc` |
|       - |  926 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |  927 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |  928 | ` *  to close the quotation.` |
|       - |  929 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |  930 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |  931 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |  932 | ` *  Warning` |
|       - |  933 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |  934 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |  935 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |  936 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |  937 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |  938 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |  939 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |  940 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |  941 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |  942 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |  943 | ` * Double quoted` |
|       - |  944 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |  945 | ` *  Escaped characters Sequence 	Meaning` |
|       - |  946 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |  947 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |  948 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |  949 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |  950 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |  951 | ` *  \\ backslash` |
|       - |  952 | ` *  \$ dollar sign` |
|       - |  953 | ` *  \" double-quote` |
|       - |  954 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |  955 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  956 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  957 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  958 | ` * See string parsing for details.` |
|       - |  959 | ` */` |
|   15814 |  960 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  961 |  |
|   15816 |  962 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  963 | `	const char *zIn,*zCur,*zEnd;` |
|   15816 |  964 | `	ph7_value *pObj = 0;` |
|       - |  965 | `	sxi32 iCons;` |
|       - |  966 | `	sxi32 rc;` |
|       - |  967 | `	/* Delimit the string */` |
|   15816 |  968 | `	zIn  = pStr->zString;` |
|   15816 |  969 | `	zEnd = &zIn[pStr->nByte];` |
|   15816 |  970 | `	if( zIn >= zEnd ){` |
|       - |  971 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  972 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  973 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  974 | `		 */` |
|     226 |  975 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  976 | `		return SXRET_OK;` |
|       - |  977 | `	}` |
|   15592 |  978 | `	zCur = 0;` |
|       - |  979 | `	/* Compile the node */` |
|   15592 |  980 | `	iCons = 0;` |
|    8692 |  981 | `	for(;;){` |
|   26328 |  982 | `		zCur = zIn;` |
|  139980 |  983 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  115448 |  984 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      49 |  985 | `				break;` |
|  115354 |  986 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1702 |  987 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     851 |  988 | `					break;` |
|       - |  989 | `			}` |
|  113654 |  990 | `			zIn++;` |
|       2 |  991 | `		}` |
|   26328 |  992 | `		if( zIn > zCur ){` |
|   12096 |  993 | `			if( pObj == 0 ){` |
|   11820 |  994 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11820 |  995 | `				if( pObj == 0 ){` |
|     ! 0 |  996 | `					return SXERR_ABORT;` |
|       - |  997 | `				}` |
|    5909 |  998 | `			}` |
|   12096 |  999 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6047 | 1000 | `		}` |
|   26328 | 1001 | `		if( zIn >= zEnd ){` |
|   15592 | 1002 | `			break;` |
|       - | 1003 | `		}` |
|   10738 | 1004 | `		if( zIn[0] == '\\' ){` |
|    8944 | 1005 | `			const char *zPtr = 0;` |
|       - | 1006 | `			sxu32 n;` |
|    8944 | 1007 | `			zIn++;` |
|    8944 | 1008 | `			if( zIn >= zEnd ){` |
|     ! 0 | 1009 | `				break;` |
|       - | 1010 | `			}` |
|    8944 | 1011 | `			if( pObj == 0 ){` |
|    5280 | 1012 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5280 | 1013 | `				if( pObj == 0 ){` |
|     ! 0 | 1014 | `					return SXERR_ABORT;` |
|       - | 1015 | `				}` |
|    2639 | 1016 | `			}` |
|    8944 | 1017 | `			n = sizeof(char); /* size of conversion */` |
|    8944 | 1018 | `			switch( zIn[0] ){` |
|       3 | 1019 | `			case '$':` |
|       - | 1020 | `				/* Dollar sign */` |
|       7 | 1021 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 | 1022 | `				break;` |
|      38 | 1023 | `			case '\\':` |
|       - | 1024 | `				/* A literal backslash */` |
|      78 | 1025 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 | 1026 | `				break;` |
|       2 | 1027 | `			case 'a':` |
|       - | 1028 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 | 1029 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 | 1030 | `				break;` |
|       2 | 1031 | `			case 'b':` |
|       - | 1032 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 | 1033 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 | 1034 | `				break;` |
|       4 | 1035 | `			case 'f':` |
|       - | 1036 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 | 1037 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 | 1038 | `				break;` |
|    4106 | 1039 | `			case 'n':` |
|       - | 1040 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8214 | 1041 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8214 | 1042 | `				break;` |
|      19 | 1043 | `			case 'r':` |
|       - | 1044 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 | 1045 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 | 1046 | `				break;` |
|      24 | 1047 | `			case 't':` |
|       - | 1048 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 | 1049 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 | 1050 | `				break;` |
|       3 | 1051 | `			case 'v':` |
|       - | 1052 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 | 1053 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 | 1054 | `				break;` |
|       1 | 1055 | `			case '\'':` |
|       - | 1056 | `				/* Single quote */` |
|       3 | 1057 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 | 1058 | `				break;` |
|      50 | 1059 | `			case '"':` |
|       - | 1060 | `				/* Double quote */` |
|     102 | 1061 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 | 1062 | `				break;` |
|       5 | 1063 | `			case '0':` |
|       - | 1064 | `				/* NUL byte */` |
|      11 | 1065 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 | 1066 | `				break;` |
|     188 | 1067 | `			case 'x':` |
|     377 | 1068 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - | 1069 | `					int c;` |
|       - | 1070 | `					/* Hex digit */` |
|     363 | 1071 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 | 1072 | `					if( &zIn[2] < zEnd ){` |
|     363 | 1073 | `						c +=  SyHexToint(zIn[2]);` |
|     181 | 1074 | `					}` |
|       - | 1075 | `					/* Output char */` |
|     363 | 1076 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 | 1077 | `					n += sizeof(char) * 2;` |
|     182 | 1078 | `				}else{` |
|       - | 1079 | `					/* Output literal character  */` |
|      15 | 1080 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - | 1081 | `				}` |
|     377 | 1082 | `				break;` |
|      15 | 1083 | `			case 'o':` |
|      31 | 1084 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - | 1085 | `					/* Octal digit stream */` |
|       - | 1086 | `					int c;` |
|      21 | 1087 | `					c = 0;` |
|      21 | 1088 | `					zIn++;` |
|      61 | 1089 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 | 1090 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 | 1091 | `							break;` |
|       - | 1092 | `						}` |
|      41 | 1093 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 | 1094 | `					}` |
|      21 | 1095 | `					if ( c > 0 ){` |
|      15 | 1096 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 | 1097 | `					}` |
|      21 | 1098 | `					n = (sxu32)(zPtr-zIn);` |
|      11 | 1099 | `				}else{` |
|       - | 1100 | `					/* Output literal character  */` |
|      11 | 1101 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - | 1102 | `				}` |
|      31 | 1103 | `				break;` |
|      11 | 1104 | `			default:` |
|       - | 1105 | `				/* Output without a slash */` |
|      23 | 1106 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 | 1107 | `				break;` |
|       - | 1108 | `			}` |
|       - | 1109 | `			/* Advance the stream cursor */` |
|    8944 | 1110 | `			zIn += n;` |
|    8944 | 1111 | `			continue;` |
|       - | 1112 | `		}` |
|    1796 | 1113 | `		if( zIn[0] == '{' ){` |
|       - | 1114 | `			/* Curly syntax */` |
|       - | 1115 | `			const char *zExpr;` |
|      97 | 1116 | `			sxi32 iNest = 1;` |
|      97 | 1117 | `			zIn++;` |
|      97 | 1118 | `			zExpr = zIn;` |
|       - | 1119 | `			/* Synchronize with the next closing curly braces */` |
|    1097 | 1120 | `			while( zIn < zEnd ){` |
|    1097 | 1121 | `				if( zIn[0] == '{' ){` |
|       - | 1122 | `					/* Increment nesting level */` |
|       9 | 1123 | `					iNest++;` |
|    1093 | 1124 | `				}else if(zIn[0] == '}' ){` |
|       - | 1125 | `					/* Decrement nesting level */` |
|     105 | 1126 | `					iNest--;` |
|     105 | 1127 | `					if( iNest <= 0 ){` |
|      97 | 1128 | `						break;` |
|       - | 1129 | `					}` |
|       4 | 1130 | `				}` |
|    1001 | 1131 | `				zIn++;` |
|       1 | 1132 | `			}` |
|       - | 1133 | `			/* Process the expression */` |
|      97 | 1134 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|      97 | 1135 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1136 | `				return SXERR_ABORT;` |
|       - | 1137 | `			}` |
|      97 | 1138 | `			if( rc != SXERR_EMPTY ){` |
|      97 | 1139 | `				++iCons;` |
|      48 | 1140 | `			}` |
|      97 | 1141 | `			if( zIn < zEnd ){` |
|       - | 1142 | `				/* Jump the trailing curly */` |
|      97 | 1143 | `				zIn++;` |
|      48 | 1144 | `			}` |
|      49 | 1145 | `		}else{` |
|       - | 1146 | `			/* Simple syntax */` |
|    1700 | 1147 | `			const char *zExpr = zIn;` |
|       - | 1148 | `			/* Assemble variable name */` |
|     849 | 1149 | `			for(;;){` |
|       - | 1150 | `				/* Jump leading dollars */` |
|    3398 | 1151 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1700 | 1152 | `					zIn++;` |
|       2 | 1153 | `				}` |
|     849 | 1154 | `				for(;;){` |
|   10317 | 1155 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7770 | 1156 | `						zIn++;` |
|       2 | 1157 | `					}` |
|    1700 | 1158 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 1159 | `						/* UTF-8 stream */` |
|     ! 0 | 1160 | `						zIn++;` |
|     ! 0 | 1161 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 | 1162 | `							zIn++;` |
|     ! 0 | 1163 | `						}` |
|     ! 0 | 1164 | `						continue;` |
|       - | 1165 | `					}` |
|    1700 | 1166 | `					break;` |
|     ! 0 | 1167 | `				}` |
|    1700 | 1168 | `				if( zIn >= zEnd ){` |
|      98 | 1169 | `					break;` |
|       - | 1170 | `				}` |
|    1604 | 1171 | `				if( zIn[0] == '[' ){` |
|       9 | 1172 | `					sxi32 iSquare = 1;` |
|       9 | 1173 | `					zIn++;` |
|      17 | 1174 | `					while( zIn < zEnd ){` |
|      17 | 1175 | `						if( zIn[0] == '[' ){` |
|     ! 0 | 1176 | `							iSquare++;` |
|      17 | 1177 | `						}else if (zIn[0] == ']' ){` |
|       9 | 1178 | `							iSquare--;` |
|       9 | 1179 | `							if( iSquare <= 0 ){` |
|       9 | 1180 | `								break;` |
|       - | 1181 | `							}` |
|     ! 0 | 1182 | `						}` |
|       9 | 1183 | `						zIn++;` |
|       1 | 1184 | `					}` |
|       9 | 1185 | `					if( zIn < zEnd ){` |
|       9 | 1186 | `						zIn++;` |
|       4 | 1187 | `					}` |
|       9 | 1188 | `					break;` |
|    1596 | 1189 | `				}else if(zIn[0] == '{' ){` |
|       6 | 1190 | `					sxi32 iCurly = 1;` |
|       6 | 1191 | `					zIn++;` |
|      18 | 1192 | `					while( zIn < zEnd ){` |
|      16 | 1193 | `						if( zIn[0] == '{' ){` |
|     ! 0 | 1194 | `							iCurly++;` |
|      16 | 1195 | `						}else if (zIn[0] == '}' ){` |
|       3 | 1196 | `							iCurly--;` |
|       3 | 1197 | `							if( iCurly <= 0 ){` |
|       3 | 1198 | `								break;` |
|       - | 1199 | `							}` |
|     ! 0 | 1200 | `						}` |
|      14 | 1201 | `						zIn++;` |
|       2 | 1202 | `					}` |
|       6 | 1203 | `					if( zIn < zEnd ){` |
|       3 | 1204 | `						zIn++;` |
|       1 | 1205 | `					}` |
|       6 | 1206 | `					break;` |
|    1592 | 1207 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - | 1208 | `					/* Member access operator '->' */` |
|     ! 0 | 1209 | `					zIn += 2;` |
|    1592 | 1210 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - | 1211 | `					/* Static member access operator '::' */` |
|     ! 0 | 1212 | `					zIn += 2;` |
|     ! 0 | 1213 | `				}else{` |
|     797 | 1214 | `					break;` |
|       - | 1215 | `				}` |
|     ! 0 | 1216 | `			}` |
|       - | 1217 | `			/* Process the expression */` |
|    1700 | 1218 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1700 | 1219 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1220 | `				return SXERR_ABORT;` |
|       - | 1221 | `			}` |
|    1700 | 1222 | `			if( rc != SXERR_EMPTY ){` |
|    1698 | 1223 | `				++iCons;` |
|     848 | 1224 | `			}` |
|       - | 1225 | `		}` |
|       - | 1226 | `		/* Invalidate the previously used constant */` |
|    1796 | 1227 | `		pObj = 0;` |
|       2 | 1228 | `	}/*for(;;)*/` |
|   15592 | 1229 | `	if( iCons > 1 ){` |
|       - | 1230 | `		/* Concatenate all compiled constants */` |
|    1354 | 1231 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     676 | 1232 | `	}` |
|       - | 1233 | `	/* Node successfully compiled */` |
|   15592 | 1234 | `	return SXRET_OK;` |
|    7909 | 1235 |  |
|       - | 1236 | `/*` |
|       - | 1237 | ` * Compile a double quoted string.` |
|       - | 1238 | ` *  See the block-comment above for more information.` |
|       - | 1239 | ` */` |
|   15756 | 1240 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1241 |  |
|       - | 1242 | `	sxi32 rc;` |
|   15758 | 1243 | `	rc = GenStateCompileString(&(*pGen));` |
|    7878 | 1244 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1245 | `	/* Compilation result */` |
|   15758 | 1246 | `	return rc;` |
|       2 | 1247 |  |
|       - | 1248 | `/*` |
|       - | 1249 | ` * Compile a Heredoc string.` |
|       - | 1250 | ` *  See the block-comment above for more information.` |
|       - | 1251 | ` */` |
|      62 | 1252 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1253 |  |
|       - | 1254 | `	SyString sOrig, sStripped;` |
|       - | 1255 | `	sxi32 rc;` |
|      64 | 1256 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      64 | 1257 | `	if( rc != SXRET_OK ){` |
|       5 | 1258 | `		return rc;` |
|       - | 1259 | `	}` |
|       - | 1260 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - | 1261 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - | 1262 | `	 * Restore before returning so downstream code that references pIn is` |
|       - | 1263 | `	 * unaffected, including on the error path. */` |
|      60 | 1264 | `	sOrig = pGen->pIn->sData;` |
|      60 | 1265 | `	pGen->pIn->sData = sStripped;` |
|      60 | 1266 | `	rc = GenStateCompileString(&(*pGen));` |
|      60 | 1267 | `	pGen->pIn->sData = sOrig;` |
|      29 | 1268 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      60 | 1269 | `	return rc;` |
|      33 | 1270 |  |
|       - | 1271 | `/*` |
|       - | 1272 | ` * Compile an array entry whether it is a key or a value.` |
|       - | 1273 | ` *  Notes on array entries.` |
|       - | 1274 | ` *  According to the PHP language reference manual` |
|       - | 1275 | ` *  An array can be created by the array() language construct.` |
|       - | 1276 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - | 1277 | ` *  array(  key =>  value` |
|       - | 1278 | ` *    , ...` |
|       - | 1279 | ` *    )` |
|       - | 1280 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - | 1281 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - | 1282 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - | 1283 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - | 1284 | ` *  contain integer and string indices.` |
|       - | 1285 | ` *  A value can be any PHP type.` |
|       - | 1286 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - | 1287 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - | 1288 | ` *  is specified, that value will be overwritten.` |
|       - | 1289 | ` */` |
|   16052 | 1290 | `static sxi32 GenStateCompileArrayEntry(` |
|       - | 1291 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 1292 | `	SyToken *pIn,        /* Token stream */` |
|       - | 1293 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - | 1294 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - | 1295 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - | 1296 | `	)` |
|       2 | 1297 |  |
|       - | 1298 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 1299 | `	sxi32 rc;` |
|       - | 1300 | `	/* Swap token stream */` |
|   16054 | 1301 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1302 | `	/* Compile the expression*/` |
|   16054 | 1303 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1304 | `	/* Restore token stream */` |
|   16054 | 1305 | `	RE_SWAP_DELIMITER(pGen);` |
|   16054 | 1306 | `	return rc;` |
|       2 | 1307 |  |
|       - | 1308 | `/*` |
|       - | 1309 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - | 1310 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1311 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1312 | ` * error message.` |
|       - | 1313 | ` * See the routine responible of compiling the array language construct` |
|       - | 1314 | ` * for more inforation.` |
|       - | 1315 | ` */` |
|      30 | 1316 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1317 |  |
|      32 | 1318 | `	sxi32 rc = SXRET_OK;` |
|      32 | 1319 | `	if( pRoot->pOp ){` |
|      19 | 1320 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 | 1321 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 | 1322 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 1323 | `			/* Unexpected expression */` |
|      11 | 1324 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1325 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 | 1326 | `			if( rc != SXERR_ABORT ){` |
|      11 | 1327 | `				rc = SXERR_INVALID;` |
|       5 | 1328 | `			}` |
|       7 | 1329 | `		}` |
|      25 | 1330 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1331 | `		/* Unexpected expression */` |
|       3 | 1332 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1333 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 | 1334 | `		if( rc != SXERR_ABORT ){` |
|       3 | 1335 | `			rc = SXERR_INVALID;` |
|       1 | 1336 | `		}` |
|       1 | 1337 | `	}` |
|      32 | 1338 | `	return rc;` |
|       2 | 1339 |  |
|       - | 1340 | `/*` |
|       - | 1341 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - | 1342 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - | 1343 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - | 1344 | ` */` |
|   23488 | 1345 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1346 |  |
|       - | 1347 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1348 | `	SyToken *pKey,*pCur;` |
|   23490 | 1349 | `	sxi32 iEmitRef = 0;` |
|   23490 | 1350 | `	sxi32 nPair = 0;` |
|       - | 1351 | `	sxi32 iNest;` |
|       - | 1352 | `	sxi32 rc;` |
|   23490 | 1353 | `	xValidator = 0;` |
|   19104 | 1354 | `	for(;;){` |
|       - | 1355 | `		/* Jump leading commas */` |
|   43206 | 1356 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4998 | 1357 | `			pGen->pIn++;` |
|       2 | 1358 | `		}` |
|   38210 | 1359 | `		pCur = pGen->pIn;` |
|   38210 | 1360 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1361 | `			/* No more entry to process */` |
|   23478 | 1362 | `			break;` |
|       - | 1363 | `		}` |
|   14734 | 1364 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1365 | `			continue;` |
|       - | 1366 | `		}` |
|       - | 1367 | `		/* Compile the key if available */` |
|   14734 | 1368 | `		pKey = pCur;` |
|   14734 | 1369 | `		iNest = 0;` |
|   40882 | 1370 | `		while( pCur < pGen->pIn ){` |
|   27364 | 1371 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1216 | 1372 | `				break;` |
|       - | 1373 | `			}` |
|   26150 | 1374 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1375 | `				iNest++;` |
|   26112 | 1376 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1377 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1378 | `				 * parser will shortly detect any syntax error.` |
|       - | 1379 | `				 */` |
|      78 | 1380 | `				iNest--;` |
|      38 | 1381 | `			}` |
|   26150 | 1382 | `			pCur++;` |
|       2 | 1383 | `		}` |
|   14734 | 1384 | `		rc = SXERR_EMPTY;` |
|   14734 | 1385 | `		if( pCur < pGen->pIn ){` |
|    1216 | 1386 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - | 1387 | `				/* Missing value */` |
|      11 | 1388 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 | 1389 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1390 | `					return SXERR_ABORT;` |
|       - | 1391 | `				}` |
|      11 | 1392 | `				return SXRET_OK;` |
|       - | 1393 | `			}` |
|       - | 1394 | `			/* Compile the expression holding the key */` |
|    1206 | 1395 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - | 1396 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1206 | 1397 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1398 | `				return SXERR_ABORT;` |
|       - | 1399 | `			}` |
|    1206 | 1400 | `			pCur++; /* Jump the '=>' operator */` |
|   14122 | 1401 | `		}else if( pKey == pCur ){` |
|       - | 1402 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1403 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1404 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1405 | `		}else{` |
|       - | 1406 | `			/* Reset back the cursor and point to the entry value */` |
|   13520 | 1407 | `			pCur = pKey;` |
|       - | 1408 | `		}` |
|   14724 | 1409 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1410 | `			/* No available key,load NULL */` |
|   13522 | 1411 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6760 | 1412 | `		}` |
|   14724 | 1413 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - | 1414 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 | 1415 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 | 1416 | `			iEmitRef = 1;` |
|      34 | 1417 | `			pCur++; /* Jump the '&' token */` |
|      34 | 1418 | `			if( pCur >= pGen->pIn ){` |
|       - | 1419 | `				/* Missing value */` |
|       3 | 1420 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 | 1421 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1422 | `					return SXERR_ABORT;` |
|       - | 1423 | `				}` |
|       3 | 1424 | `				return SXRET_OK;` |
|       - | 1425 | `			}` |
|      15 | 1426 | `		}` |
|       - | 1427 | `		/* Compile indice value */` |
|   14722 | 1428 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14722 | 1429 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1430 | `			return SXERR_ABORT;` |
|       - | 1431 | `		}` |
|   14722 | 1432 | `		if( iEmitRef ){` |
|       - | 1433 | `			/* Emit the load reference instruction */` |
|      32 | 1434 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1435 | `		}` |
|   14722 | 1436 | `		xValidator = 0;` |
|   14722 | 1437 | `		iEmitRef = 0;` |
|   14722 | 1438 | `		nPair++;` |
|       2 | 1439 | `	}` |
|       - | 1440 | `	/* Emit the load map instruction */` |
|   23478 | 1441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1442 | `	/* Node successfully compiled */` |
|   23478 | 1443 | `	return SXRET_OK;` |
|   11746 | 1444 |  |
|       - | 1445 | `/*` |
|       - | 1446 | ` * Compile the 'array' language construct.` |
|       - | 1447 | ` *	 According to the PHP language reference manual` |
|       - | 1448 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1449 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1450 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1451 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1452 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1453 | ` */` |
|   23228 | 1454 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1455 |  |
|       - | 1456 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   23230 | 1457 | `	pGen->pIn += 2;` |
|   23230 | 1458 | `	pGen->pEnd--;` |
|   11614 | 1459 | `	SXUNUSED(iCompileFlag);` |
|   23230 | 1460 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1461 |  |
|       - | 1462 | `/*` |
|       - | 1463 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1464 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1465 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1466 | ` */` |
|     260 | 1467 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1468 |  |
|       - | 1469 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     262 | 1470 | `	pGen->pIn++;` |
|     262 | 1471 | `	pGen->pEnd--;` |
|     130 | 1472 | `	SXUNUSED(iCompileFlag);` |
|     262 | 1473 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1474 |  |
|       - | 1475 | `/*` |
|       - | 1476 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1477 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1478 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1479 | ` * error message.` |
|       - | 1480 | ` * See the routine responible of compiling the list language construct` |
|       - | 1481 | ` * for more inforation.` |
|       - | 1482 | ` */` |
|     128 | 1483 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1484 |  |
|     130 | 1485 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1486 | `	if( pRoot->pOp ){` |
|     ! 0 | 1487 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1488 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1489 | `				/* Unexpected expression */` |
|     ! 0 | 1490 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1491 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1492 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1493 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1494 | `				}` |
|     ! 0 | 1495 | `		}` |
|     130 | 1496 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1497 | `		/* Unexpected expression */` |
|       5 | 1498 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1499 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1500 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1501 | `			rc = SXERR_INVALID;` |
|       2 | 1502 | `		}` |
|       2 | 1503 | `	}` |
|     130 | 1504 | `	return rc;` |
|       2 | 1505 |  |
|       - | 1506 | `/*` |
|       - | 1507 | ` * Compile the 'list' language construct.` |
|       - | 1508 | ` *  According to the PHP language reference` |
|       - | 1509 | ` *  list(): Assign variables as if they were an array.` |
|       - | 1510 | ` *  list() is used to assign a list of variables in one operation.` |
|       - | 1511 | ` *  Description` |
|       - | 1512 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - | 1513 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - | 1514 | ` *   list() is used to assign a list of variables in one operation.` |
|       - | 1515 | ` *  Parameters` |
|       - | 1516 | ` *   $varname: A variable.` |
|       - | 1517 | ` *  Return Values` |
|       - | 1518 | ` *   The assigned array.` |
|       - | 1519 | ` */` |
|       - | 1520 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1521 | `struct NestedListEntry {` |
|       - | 1522 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1523 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1524 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1525 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1526 | `};` |
|       - | 1527 | `/*` |
|       - | 1528 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1529 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1530 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1531 | ` */` |
|      74 | 1532 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1533 |  |
|       - | 1534 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1535 | `	SyToken *pNext;` |
|       - | 1536 | `	sxi32 nExpr;` |
|       - | 1537 | `	sxi32 rc;` |
|      76 | 1538 | `	nExpr = 0;` |
|      76 | 1539 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1540 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1541 | `		if( pGen->pIn < pNext ){` |
|       - | 1542 | `			/* Check for nested list() */` |
|     144 | 1543 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1544 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1545 | `				/* Record this nested list for post-processing */` |
|       3 | 1546 | `				SyToken *pListEnd = 0;` |
|       3 | 1547 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1548 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1549 | `				}` |
|       3 | 1550 | `				if( pListEnd ){` |
|       - | 1551 | `					struct NestedListEntry sEntry;` |
|       3 | 1552 | `					sEntry.nIndex = nExpr;` |
|       3 | 1553 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1554 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1555 | `					sEntry.isShort = 0;` |
|       3 | 1556 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1557 | `				}` |
|       - | 1558 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1559 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1560 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1561 | `				/* Nested short destructuring [...] */` |
|      13 | 1562 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1563 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1564 | `				if( pBracketEnd ){` |
|       - | 1565 | `					struct NestedListEntry sEntry;` |
|      13 | 1566 | `					sEntry.nIndex = nExpr;` |
|      13 | 1567 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1568 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1569 | `					sEntry.isShort = 1;` |
|      13 | 1570 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1571 | `				}` |
|       - | 1572 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1573 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1574 | `			}else{` |
|       - | 1575 | `				/* Compile the expression holding the variable */` |
|     130 | 1576 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1577 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1578 | `					SySetRelease(&sNested);` |
|     ! 0 | 1579 | `					return SXRET_OK;` |
|       - | 1580 | `				}` |
|       - | 1581 | `			}` |
|      73 | 1582 | `		}else{` |
|       - | 1583 | `			/* Empty entry,load NULL */` |
|      13 | 1584 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1585 | `		}` |
|     156 | 1586 | `		nExpr++;` |
|       - | 1587 | `		/* Advance the stream cursor */` |
|     156 | 1588 | `		pGen->pIn = &pNext[1];` |
|       2 | 1589 | `	}` |
|       - | 1590 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1591 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1592 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1593 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1594 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1595 | `	 */` |
|      76 | 1596 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1597 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1598 | `		sxu32 i;` |
|      27 | 1599 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1600 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1601 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1602 | `			ph7_value *pIdx;` |
|       - | 1603 | `			sxu32 nConstIdx;` |
|       - | 1604 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1605 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1606 | `			/* Push the integer index for this nested entry */` |
|      15 | 1607 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1608 | `			if( pIdx == 0 ){` |
|     ! 0 | 1609 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1610 | `				SySetRelease(&sNested);` |
|     ! 0 | 1611 | `				return SXERR_ABORT;` |
|       - | 1612 | `			}` |
|      15 | 1613 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1614 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1615 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1616 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1617 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1618 | `			 */` |
|      15 | 1619 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1620 | `			/* Recursively compile the inner list */` |
|      15 | 1621 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1622 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1623 | `			if( apNested[i].isShort ){` |
|      13 | 1624 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1625 | `			}else{` |
|       3 | 1626 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1627 | `			}` |
|      15 | 1628 | `			pGen->pIn = pSavedIn;` |
|      15 | 1629 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1630 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1631 | `				SySetRelease(&sNested);` |
|     ! 0 | 1632 | `				return SXERR_ABORT;` |
|       - | 1633 | `			}` |
|       - | 1634 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1635 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1636 | `		}` |
|       6 | 1637 | `	}` |
|      76 | 1638 | `	SySetRelease(&sNested);` |
|       - | 1639 | `	/* Node successfully compiled */` |
|      76 | 1640 | `	return SXRET_OK;` |
|      39 | 1641 |  |
|      32 | 1642 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1643 |  |
|       - | 1644 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1645 | `	pGen->pIn += 2;` |
|      34 | 1646 | `	pGen->pEnd--;` |
|      16 | 1647 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1648 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1649 |  |
|      42 | 1650 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1651 |  |
|       - | 1652 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1653 | `	pGen->pIn++;` |
|      44 | 1654 | `	pGen->pEnd--;` |
|      21 | 1655 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1656 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1657 |  |
|       - | 1658 | `/* Forward declarations */` |
|       - | 1659 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1660 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1661 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1662 | `/*` |
|       - | 1663 | ` * Compile an annoynmous function or a closure.` |
|       - | 1664 | ` * According to the PHP language reference` |
|       - | 1665 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1666 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1667 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1668 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1669 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1670 | ` *  Example Anonymous function variable assignment example` |
|       - | 1671 | ` * <?php` |
|       - | 1672 | ` * $greet = function($name)` |
|       - | 1673 | ` * {` |
|       - | 1674 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1675 | ` * };` |
|       - | 1676 | ` * $greet('World');` |
|       - | 1677 | ` * $greet('PHP');` |
|       - | 1678 | ` * ?>` |
|       - | 1679 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1680 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1681 | ` */` |
|     168 | 1682 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1683 |  |
|       - | 1684 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1685 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1686 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1687 | `							  * one thread is allowed to compile the script.` |
|       - | 1688 | `						      */` |
|       - | 1689 | `	ph7_value *pObj;` |
|       - | 1690 | `	SyString sName;` |
|       - | 1691 | `	sxu32 nIdx;` |
|       - | 1692 | `	sxu32 nLen;` |
|       - | 1693 | `	sxi32 rc;` |
|       - | 1694 |  |
|     170 | 1695 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     170 | 1696 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1697 | `		pGen->pIn++;` |
|     ! 0 | 1698 | `	}` |
|       - | 1699 | `	/* Reserve a constant for the lambda */` |
|     170 | 1700 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 | 1701 | `	if( pObj == 0 ){` |
|     ! 0 | 1702 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1703 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1704 | `		return SXERR_ABORT;` |
|       - | 1705 | `	}` |
|       - | 1706 | `	/* Generate a unique name */` |
|     170 | 1707 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1708 | `	/* Make sure the generated name is unique */` |
|     170 | 1709 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1710 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1711 | `	}` |
|     170 | 1712 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     170 | 1713 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1714 | `	/* Compile the lambda body */` |
|     170 | 1715 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     170 | 1716 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1717 | `		return SXERR_ABORT;` |
|       - | 1718 | `	}` |
|     170 | 1719 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1720 | `		/* Emit the load closure instruction */` |
|      16 | 1721 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 | 1722 | `	}else{` |
|       - | 1723 | `		/* Emit the load constant instruction */` |
|     156 | 1724 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1725 | `	}` |
|       - | 1726 | `	/* Node successfully compiled */` |
|     170 | 1727 | `	return SXRET_OK;` |
|      86 | 1728 |  |
|       - | 1729 | `/*` |
|       - | 1730 | ` * Compile a backtick quoted string.` |
|       - | 1731 | ` */` |
|       4 | 1732 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1733 |  |
|       - | 1734 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1735 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1736 | `	 */` |
|       7 | 1737 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1738 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1739 | `		ph7_lib_version()` |
|       - | 1740 | `		);` |
|       - | 1741 | `	/* Load NULL */` |
|       5 | 1742 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1743 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1744 | `	/* Node successfully compiled */` |
|       5 | 1745 | `	return SXRET_OK;` |
|       1 | 1746 |  |
|       - | 1747 | `/*` |
|       - | 1748 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1749 | ` * construct.` |
|       - | 1750 | ` */` |
|      72 | 1751 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1752 |  |
|       - | 1753 | `	SyString *pName;` |
|       - | 1754 | `	sxu32 nKeyID;` |
|       - | 1755 | `	sxi32 rc;` |
|       - | 1756 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1757 | `	pName = &pGen->pIn->sData;` |
|      74 | 1758 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1759 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1760 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1761 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1762 | `		/* Compile arguments one after one */` |
|       9 | 1763 | `		pTmp = pGen->pEnd;` |
|       - | 1764 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1765 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1766 | `		 *  mean that the following expression is valid:` |
|       - | 1767 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1768 | `		 */` |
|       9 | 1769 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1770 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1771 | `			if( pGen->pIn < pNext ){` |
|       9 | 1772 | `				pGen->pEnd = pNext;` |
|       9 | 1773 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1774 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1775 | `					return SXERR_ABORT;` |
|       - | 1776 | `				}` |
|       9 | 1777 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1778 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1779 | `					 * without the overhead of a function call.` |
|       - | 1780 | `					 * This is a very powerful optimization that improve` |
|       - | 1781 | `					 * performance greatly.` |
|       - | 1782 | `					 */` |
|       9 | 1783 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1784 | `				}` |
|       4 | 1785 | `			}` |
|       - | 1786 | `			/* Jump trailing commas */` |
|       9 | 1787 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1788 | `				pNext++;` |
|     ! 0 | 1789 | `			}` |
|       9 | 1790 | `			pGen->pIn = pNext;` |
|       1 | 1791 | `		}` |
|       - | 1792 | `		/* Restore token stream */` |
|       9 | 1793 | `		pGen->pEnd = pTmp;` |
|       5 | 1794 | `	}else{` |
|      66 | 1795 | `		sxi32 nArg = 0;` |
|      66 | 1796 | `		sxu32 nIdx = 0;` |
|      66 | 1797 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1798 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1799 | `			return SXERR_ABORT;` |
|      66 | 1800 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1801 | `			nArg = 1;` |
|      32 | 1802 | `		}` |
|      66 | 1803 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1804 | `			ph7_value *pObj;` |
|       - | 1805 | `			/* Emit the call instruction */` |
|      20 | 1806 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1807 | `			if( pObj == 0 ){` |
|     ! 0 | 1808 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1809 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1810 | `				return SXERR_ABORT;` |
|       - | 1811 | `			}` |
|      20 | 1812 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1813 | `			/* Install in the literal table */` |
|      20 | 1814 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1815 | `		}` |
|       - | 1816 | `		/* Emit the call instruction */` |
|      66 | 1817 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1818 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1819 | `	}` |
|       - | 1820 | `	/* Node successfully compiled */` |
|      74 | 1821 | `	return SXRET_OK;` |
|      38 | 1822 |  |
|       - | 1823 | `/*` |
|       - | 1824 | ` * Compile a node holding a variable declaration.` |
|       - | 1825 | ` * According to the PHP language reference` |
|       - | 1826 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1827 | ` *  The variable name is case-sensitive.` |
|       - | 1828 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1829 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1830 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1831 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1832 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1833 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1834 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1835 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1836 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1837 | ` *  the chapter on Expressions.` |
|       - | 1838 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1839 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1840 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1841 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1842 | ` *  is being assigned (the source variable).` |
|       - | 1843 | ` */` |
|  784444 | 1844 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1845 |  |
|  784446 | 1846 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1847 | `	sxi32 iVv;` |
|       - | 1848 | `	sxi32 iP1;` |
|       - | 1849 | `	void *p3;` |
|       - | 1850 | `	sxi32 rc;` |
|  784446 | 1851 | `	iVv = -1; /* Variable variable counter */` |
| 1568902 | 1852 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  784458 | 1853 | `		pGen->pIn++;` |
|  784458 | 1854 | `		iVv++;` |
|       2 | 1855 | `	}` |
|  784446 | 1856 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1857 | `		/* Invalid variable name */` |
|     ! 0 | 1858 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1859 | `		if( rc == SXERR_ABORT ){` |
|       - | 1860 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1861 | `			return SXERR_ABORT;` |
|       - | 1862 | `		}` |
|     ! 0 | 1863 | `		return SXRET_OK;` |
|       - | 1864 | `	}` |
|  784446 | 1865 | `	p3  = 0;` |
|  784446 | 1866 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1867 | `		/* Dynamic variable creation */` |
|      18 | 1868 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1869 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1870 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1871 | `			/* Empty expression */` |
|       3 | 1872 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1873 | `			return SXRET_OK;` |
|       - | 1874 | `		}` |
|       - | 1875 | `		/* Compile the expression holding the variable name */` |
|      16 | 1876 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1877 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1878 | `			return SXERR_ABORT;` |
|      16 | 1879 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1880 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1881 | `			return SXRET_OK;` |
|       - | 1882 | `		}` |
|       7 | 1883 | `	}else{` |
|       - | 1884 | `		SyHashEntry *pEntry;` |
|       - | 1885 | `		SyString *pName;` |
|  784430 | 1886 | `		char *zName = 0;` |
|       - | 1887 | `		/* Extract variable name */` |
|  784430 | 1888 | `		pName = &pGen->pIn->sData;` |
|       - | 1889 | `		/* Advance the stream cursor */` |
|  784430 | 1890 | `		pGen->pIn++;` |
|  784430 | 1891 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  784430 | 1892 | `		if( pEntry == 0 ){` |
|       - | 1893 | `			/* Duplicate name */` |
|  112754 | 1894 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  112754 | 1895 | `			if( zName == 0 ){` |
|     ! 0 | 1896 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1897 | `				return SXERR_ABORT;` |
|       - | 1898 | `			}` |
|       - | 1899 | `			/* Install in the hashtable */` |
|  112754 | 1900 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   56378 | 1901 | `		}else{` |
|       - | 1902 | `			/* Name already available */` |
|  671678 | 1903 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1904 | `		}` |
|  784430 | 1905 | `		p3 = (void *)zName;` |
|       - | 1906 | `	}` |
|  784442 | 1907 | `	iP1 = 0;` |
|  784442 | 1908 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  301568 | 1909 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1910 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  295458 | 1911 | `			iP1 = 1;` |
|  147728 | 1912 | `		}` |
|  150783 | 1913 | `	}` |
|       - | 1914 | `	/* Emit the load instruction */` |
|  784442 | 1915 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  784454 | 1916 | `	while( iVv > 0 ){` |
|      13 | 1917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1918 | `		iVv--;` |
|       1 | 1919 | `	}` |
|       - | 1920 | `	/* Node successfully compiled */` |
|  784442 | 1921 | `	return SXRET_OK;` |
|  392224 | 1922 |  |
|       - | 1923 | `/*` |
|       - | 1924 | ` * Load a literal.` |
|       - | 1925 | ` */` |
|  526012 | 1926 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1927 |  |
|  526014 | 1928 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1929 | `	ph7_value *pObj;` |
|       - | 1930 | `	SyString *pStr;` |
|       - | 1931 | `	sxu32 nIdx;` |
|       - | 1932 | `	/* Extract token value */` |
|  526014 | 1933 | `	pStr = &pToken->sData;` |
|       - | 1934 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  526014 | 1935 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   95554 | 1936 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1937 | `			/* NULL constant are always indexed at 0 */` |
|   40642 | 1938 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   40642 | 1939 | `			return SXRET_OK;` |
|   54914 | 1940 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1941 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1942 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1943 | `			return SXRET_OK;` |
|       2 | 1944 | `		}` |
|  499166 | 1945 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   82982 | 1946 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1947 | `			/* FALSE constant are always indexed at 2 */` |
|   35454 | 1948 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   35454 | 1949 | `			return SXRET_OK;` |
|  431680 | 1950 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   73340 | 1951 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1952 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5372 | 1953 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5372 | 1954 | `			if( pObj == 0 ){` |
|     ! 0 | 1955 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1956 | `				return SXERR_ABORT;` |
|       - | 1957 | `			}` |
|    5372 | 1958 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1959 | `			/* Emit the load constant instruction */` |
|    5372 | 1960 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5372 | 1961 | `			return SXRET_OK;` |
|  403178 | 1962 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   27076 | 1963 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1964 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1965 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1966 | `			if( pObj == 0 ){` |
|     ! 0 | 1967 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1968 | `				return SXERR_ABORT;` |
|       - | 1969 | `			}` |
|       7 | 1970 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1971 | `				SyString sNs;` |
|       7 | 1972 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1973 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1974 | `			}else{` |
|     ! 0 | 1975 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1976 | `			}` |
|       7 | 1977 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1978 | `			return SXRET_OK;` |
|  402352 | 1979 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11328 | 1980 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  396682 | 1981 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14114 | 1982 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1983 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1984 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1985 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1986 | `				/* Point to the upper block */` |
|      11 | 1987 | `				pBlock = pBlock->pParent;` |
|       1 | 1988 | `			}` |
|      11 | 1989 | `			if( pBlock == 0 ){` |
|       - | 1990 | `				/* Called in the global scope,load NULL */` |
|       5 | 1991 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1992 | `			}else{` |
|       - | 1993 | `				/* Extract the target function/method */` |
|       7 | 1994 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1995 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1996 | `					/* Not a class method,Load null */` |
|       3 | 1997 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1998 | `				}else{` |
|       5 | 1999 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 2000 | `					if( pObj == 0 ){` |
|     ! 0 | 2001 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2002 | `						return SXERR_ABORT;` |
|       - | 2003 | `					}` |
|       5 | 2004 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 2005 | `					/* Emit the load constant instruction */` |
|       5 | 2006 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 2007 | `				}` |
|       - | 2008 | `			}` |
|      11 | 2009 | `			return SXRET_OK;` |
|       - | 2010 | `	}` |
|       - | 2011 | `	/* Query literal table */` |
|  444050 | 2012 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 2013 | `		ph7_value *pLitObj;` |
|       - | 2014 | `		/* Unknown literal,install it in the literal table */` |
|  207792 | 2015 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  207792 | 2016 | `		if( pLitObj == 0 ){` |
|     ! 0 | 2017 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2018 | `			return SXERR_ABORT;` |
|       - | 2019 | `		}` |
|  207792 | 2020 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  207792 | 2021 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  103895 | 2022 | `	}` |
|       - | 2023 | `	/* Emit the load constant instruction */` |
|  444050 | 2024 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  444050 | 2025 | `	return SXRET_OK;` |
|  263008 | 2026 |  |
|       - | 2027 | `/*` |
|       - | 2028 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 2029 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 2030 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 2031 | ` * Otherwise, load the simple literal directly.` |
|       - | 2032 | ` */` |
|  526036 | 2033 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 2034 |  |
|       - | 2035 | `	sxi32 rc;` |
|  526038 | 2036 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2037 | `		return SXRET_OK;` |
|       - | 2038 | `	}` |
|       - | 2039 | `	/* Check if this is a multi-token namespace path */` |
|  526038 | 2040 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 2041 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 2042 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 2043 | `		int isAbsolute = 0;` |
|      26 | 2044 | `		SyBlobReset(pWorker);` |
|       - | 2045 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 2046 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 2047 | `			isAbsolute = 1;` |
|      24 | 2048 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 2049 | `		}` |
|       - | 2050 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 2051 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 2052 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 2053 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 2054 | `		}` |
|       - | 2055 | `		/* Collect all path components */` |
|     102 | 2056 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 2057 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 2058 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 2059 | `			}else{` |
|      64 | 2060 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 2061 | `			}` |
|     102 | 2062 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 2063 | `				pGen->pIn++;` |
|      26 | 2064 | `				break;` |
|       - | 2065 | `			}` |
|      78 | 2066 | `			pGen->pIn++;` |
|       2 | 2067 | `		}` |
|      26 | 2068 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 2069 | `			ph7_value *pObj;` |
|       - | 2070 | `			SyString sPath;` |
|       - | 2071 | `			sxu32 nIdx;` |
|      26 | 2072 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 2073 | `			/* Install in the literal table */` |
|      26 | 2074 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 2075 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 2076 | `				if( pObj == 0 ){` |
|     ! 0 | 2077 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 2078 | `					return SXERR_ABORT;` |
|       - | 2079 | `				}` |
|      13 | 2080 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 2081 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 2082 | `			}` |
|       - | 2083 | `			/* Emit the load constant instruction.` |
|       - | 2084 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 2085 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 2086 | `			return SXRET_OK;` |
|       - | 2087 | `		}` |
|     ! 0 | 2088 | `	}` |
|       - | 2089 | `	/* Single-token literal: load directly */` |
|  526014 | 2090 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  526014 | 2091 | `	return rc;` |
|  263020 | 2092 |  |
|       - | 2093 | `/*` |
|       - | 2094 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 2095 | ` */` |
|  526036 | 2096 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 2097 |  |
|       - | 2098 | `	sxi32 rc;` |
|  526038 | 2099 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  526038 | 2100 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2101 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 2102 | `		return rc;` |
|       - | 2103 | `	}` |
|       - | 2104 | `	/* Node successfully compiled */` |
|  526038 | 2105 | `	return SXRET_OK;` |
|  263020 | 2106 |  |
|       - | 2107 | `/*` |
|       - | 2108 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 2109 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 2110 | ` */` |
|       8 | 2111 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 2112 |  |
|       - | 2113 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 2114 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 2115 | `		pGen->pIn++;` |
|       1 | 2116 | `	}` |
|       9 | 2117 | `	return SXRET_OK;` |
|       1 | 2118 |  |
|       - | 2119 | `/*` |
|       - | 2120 | ` * Check if the given identifier name is reserved or not.` |
|       - | 2121 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 2122 | ` */` |
|      56 | 2123 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 2124 |  |
|      58 | 2125 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 2126 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 2127 | `			return TRUE;` |
|      24 | 2128 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 2129 | `			return TRUE;` |
|       2 | 2130 | `		}` |
|      43 | 2131 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 2132 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 2133 | `			return TRUE;` |
|       - | 2134 | `		}` |
|     ! 0 | 2135 | `	}` |
|       - | 2136 | `	/* Not a reserved constant */` |
|      50 | 2137 | `	return FALSE;` |
|      30 | 2138 |  |
|       - | 2139 | `/*` |
|       - | 2140 | ` * Compile the 'const' statement.` |
|       - | 2141 | ` * According to the PHP language reference` |
|       - | 2142 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 2143 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 2144 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 2145 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 2146 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 2147 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 2148 | ` *  Syntax` |
|       - | 2149 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 2150 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 2151 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 2152 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 2153 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 2154 | ` *  to get a list of all defined constants.` |
|       - | 2155 | ` *` |
|       - | 2156 | ` * Symisc eXtension.` |
|       - | 2157 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 2158 | ` *  would allow only simple scalar value.` |
|       - | 2159 | ` *  Example` |
|       - | 2160 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 2161 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 2162 | ` */` |
|      32 | 2163 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 2164 |  |
|       - | 2165 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 2166 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 2167 | `	SyString *pName;` |
|       - | 2168 | `	sxi32 rc;` |
|      34 | 2169 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 2170 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 2171 | `		/* Invalid constant name */` |
|       7 | 2172 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 2173 | `		if( rc == SXERR_ABORT ){` |
|       - | 2174 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2175 | `			return SXERR_ABORT;` |
|       - | 2176 | `		}` |
|       7 | 2177 | `		goto Synchronize;` |
|       - | 2178 | `	}` |
|       - | 2179 | `	/* Peek constant name */` |
|      28 | 2180 | `	pName = &pGen->pIn->sData;` |
|       - | 2181 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 2182 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 2183 | `		/* Reserved constant */` |
|       9 | 2184 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 2185 | `		if( rc == SXERR_ABORT ){` |
|       - | 2186 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2187 | `			return SXERR_ABORT;` |
|       - | 2188 | `		}` |
|       9 | 2189 | `		goto Synchronize;` |
|       - | 2190 | `	}` |
|      20 | 2191 | `	pGen->pIn++;` |
|      20 | 2192 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 2193 | `		/* Invalid statement*/` |
|       5 | 2194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 2195 | `		if( rc == SXERR_ABORT ){` |
|       - | 2196 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2197 | `			return SXERR_ABORT;` |
|       - | 2198 | `		}` |
|       5 | 2199 | `		goto Synchronize;` |
|       - | 2200 | `	}` |
|      15 | 2201 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 2202 | `	/* Allocate a new constant value container */` |
|      15 | 2203 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 2204 | `	if( pConsCode == 0 ){` |
|     ! 0 | 2205 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2206 | `		return SXERR_ABORT;` |
|       - | 2207 | `	}` |
|      15 | 2208 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 2209 | `	/* Swap bytecode container */` |
|      15 | 2210 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 2211 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 2212 | `	/* Compile constant value */` |
|      15 | 2213 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2214 | `	/* Emit the done instruction */` |
|      15 | 2215 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 2216 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 2217 | `	if( rc == SXERR_ABORT ){` |
|       - | 2218 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2219 | `		return SXERR_ABORT;` |
|       - | 2220 | `	}` |
|      15 | 2221 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 2222 | `	/* Register the constant with namespace-qualified name */` |
|       - | 2223 | `	{` |
|       - | 2224 | `		SyBlob sFQN;` |
|       - | 2225 | `		SyString sFQNStr;` |
|      15 | 2226 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 2227 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 2228 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 2229 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 2230 | `		SyBlobRelease(&sFQN);` |
|       - | 2231 | `	}` |
|      15 | 2232 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2233 | `		SySetRelease(pConsCode);` |
|     ! 0 | 2234 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 2235 | `	}` |
|      15 | 2236 | `	return SXRET_OK;` |
|       9 | 2237 | `Synchronize:` |
|       - | 2238 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 2239 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 2240 | `		pGen->pIn++;` |
|       1 | 2241 | `	}` |
|      19 | 2242 | `	return SXRET_OK;` |
|      18 | 2243 |  |
|       - | 2244 | `/*` |
|       - | 2245 | ` * Compile the 'continue' statement.` |
|       - | 2246 | ` * According to the PHP language reference` |
|       - | 2247 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 2248 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 2249 | ` *  iteration.` |
|       - | 2250 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 2251 | ` *  the purposes of continue.` |
|       - | 2252 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 2253 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 2254 | ` *  Note:` |
|       - | 2255 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 2256 | ` */` |
|       - | 2257 | `/*` |
|       - | 2258 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 2259 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 2260 | ` * break/continue crosses a try boundary.` |
|       - | 2261 | ` *` |
|       - | 2262 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 2263 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 2264 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 2265 | ` */` |
|    2812 | 2266 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 2267 |  |
|    2814 | 2268 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16418 | 2269 | `	while( pBlock && pBlock != pTarget ){` |
|   13606 | 2270 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 2271 | `			if( pBlock->pUserData ){` |
|       - | 2272 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 2273 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 2274 | `			}else{` |
|       - | 2275 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 2276 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 2277 | `				 * exception context from a sub-execution.` |
|       - | 2278 | `				 */` |
|     ! 0 | 2279 | `				break;` |
|       - | 2280 | `			}` |
|       1 | 2281 | `		}` |
|   13606 | 2282 | `		pBlock = pBlock->pParent;` |
|       2 | 2283 | `	}` |
|    2814 | 2284 |  |
|    2728 | 2285 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2286 |  |
|       - | 2287 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2288 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2289 | `	sxu32 nLineLocal;` |
|       - | 2290 | `	sxi32 rc;` |
|    2730 | 2291 | `	nLineLocal = pGen->pIn->nLine;` |
|    2730 | 2292 | `	iLevel = 0;` |
|       - | 2293 | `	/* Jump the 'continue' keyword */` |
|    2730 | 2294 | `	pGen->pIn++;` |
|    2730 | 2295 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2296 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2297 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2298 | `		 */` |
|       - | 2299 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2300 | `		char *zAlloc = 0;` |
|       - | 2301 | `		SyString sNum;` |
|      16 | 2302 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2303 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2304 | `			return SXERR_ABORT;` |
|       - | 2305 | `		}` |
|      16 | 2306 | `		if( rc == SXRET_OK ){` |
|      20 | 2307 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2308 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2309 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2310 | `				return SXERR_ABORT;` |
|       - | 2311 | `			}` |
|      14 | 2312 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2313 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2314 | `		}` |
|      16 | 2315 | `		if( iLevel < 2 ){` |
|       3 | 2316 | `			iLevel = 0;` |
|       1 | 2317 | `		}` |
|      16 | 2318 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2319 | `	}` |
|       - | 2320 | `	/* Point to the target loop */` |
|    2730 | 2321 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2730 | 2322 | `	if( pLoop == 0 ){` |
|       - | 2323 | `		/* Illegal continue */` |
|      11 | 2324 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2325 | `		if( rc == SXERR_ABORT ){` |
|       - | 2326 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2327 | `			return SXERR_ABORT;` |
|       - | 2328 | `		}` |
|       6 | 2329 | `	}else{` |
|    2720 | 2330 | `		sxu32 nInstrIdx = 0;` |
|       - | 2331 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2720 | 2332 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2720 | 2333 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2334 | `			/* According to the PHP language reference manual` |
|       - | 2335 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2336 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2337 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2338 | `			 */` |
|       5 | 2339 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2340 | `			if( rc == SXRET_OK ){` |
|       5 | 2341 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2342 | `			}` |
|       3 | 2343 | `		}else{` |
|       - | 2344 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2716 | 2345 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2716 | 2346 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2347 | `				JumpFixup sJumpFix;` |
|       - | 2348 | `				/* Post-continue */` |
|      14 | 2349 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 | 2350 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 | 2351 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 | 2352 | `			}` |
|       - | 2353 | `		}` |
|       - | 2354 | `	}` |
|    2730 | 2355 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2356 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2357 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2358 | `	}` |
|       - | 2359 | `	/* Statement successfully compiled */` |
|    2730 | 2360 | `	return SXRET_OK;` |
|    1366 | 2361 |  |
|       - | 2362 | `/*` |
|       - | 2363 | ` * Compile the 'break' statement.` |
|       - | 2364 | ` * According to the PHP language reference` |
|       - | 2365 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2366 | ` *  structure.` |
|       - | 2367 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2368 | ` *  enclosing structures are to be broken out of.` |
|       - | 2369 | ` */` |
|     110 | 2370 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2371 |  |
|       - | 2372 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2373 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2374 | `	sxi32 rc;` |
|     112 | 2375 | `	iLevel = 0;` |
|       - | 2376 | `	/* Jump the 'break' keyword */` |
|     112 | 2377 | `	pGen->pIn++;` |
|     112 | 2378 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2379 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2380 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2381 | `		 */` |
|       - | 2382 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 | 2383 | `		char *zAlloc = 0;` |
|       - | 2384 | `		SyString sNum;` |
|      16 | 2385 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 | 2386 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2387 | `			return SXERR_ABORT;` |
|       - | 2388 | `		}` |
|      16 | 2389 | `		if( rc == SXRET_OK ){` |
|      20 | 2390 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 | 2391 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 | 2392 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 2393 | `				return SXERR_ABORT;` |
|       - | 2394 | `			}` |
|      14 | 2395 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 | 2396 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 | 2397 | `		}` |
|      16 | 2398 | `		if( iLevel < 2 ){` |
|       3 | 2399 | `			iLevel = 0;` |
|       1 | 2400 | `		}` |
|      16 | 2401 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 | 2402 | `	}` |
|       - | 2403 | `	/* Extract the target loop */` |
|     112 | 2404 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 | 2405 | `	if( pLoop == 0 ){` |
|       - | 2406 | `		/* Illegal break */` |
|      17 | 2407 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2408 | `		if( rc == SXERR_ABORT ){` |
|       - | 2409 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2410 | `			return SXERR_ABORT;` |
|       - | 2411 | `		}` |
|       9 | 2412 | `	}else{` |
|       - | 2413 | `		sxu32 nInstrIdx;` |
|       - | 2414 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 | 2415 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 | 2416 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 | 2417 | `		if( rc == SXRET_OK ){` |
|       - | 2418 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 | 2419 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 | 2420 | `		}` |
|       - | 2421 | `	}` |
|     112 | 2422 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2423 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2424 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2425 | `	}` |
|       - | 2426 | `	/* Statement successfully compiled */` |
|     112 | 2427 | `	return SXRET_OK;` |
|      57 | 2428 |  |
|       - | 2429 | `/*` |
|       - | 2430 | ` * Compile or record a label.` |
|       - | 2431 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2432 | ` * Example` |
|       - | 2433 | ` *  goto LABEL;` |
|       - | 2434 | ` *   echo 'Foo';` |
|       - | 2435 | ` *  LABEL:` |
|       - | 2436 | ` *   echo 'Bar';` |
|       - | 2437 | ` */` |
|     112 | 2438 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2439 |  |
|       - | 2440 | `	GenBlock *pBlock;` |
|       - | 2441 | `	Label sLabel;` |
|       - | 2442 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2443 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2444 | `	if( pBlock ){` |
|       - | 2445 | `		sxi32 rc;` |
|       7 | 2446 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2447 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2448 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2449 | `			return SXERR_ABORT;` |
|       - | 2450 | `		}` |
|       3 | 2451 | `	}else{` |
|     110 | 2452 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2453 | `		char *zDup;` |
|       - | 2454 | `		/* Initialize label fields */` |
|     110 | 2455 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2456 | `		/* Duplicate label name */` |
|     110 | 2457 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2458 | `		if( zDup == 0 ){` |
|     ! 0 | 2459 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2460 | `			return SXERR_ABORT;` |
|       - | 2461 | `		}` |
|     110 | 2462 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2463 | `		sLabel.bRef  = FALSE;` |
|     110 | 2464 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2465 | `		pBlock = pGen->pCurrent;` |
|     218 | 2466 | `		while( pBlock ){` |
|     130 | 2467 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2468 | `				break;` |
|       - | 2469 | `			}` |
|       - | 2470 | `			/* Point to the upper block */` |
|     110 | 2471 | `			pBlock = pBlock->pParent;` |
|       2 | 2472 | `		}` |
|     110 | 2473 | `		if( pBlock ){` |
|      22 | 2474 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2475 | `		}else{` |
|      90 | 2476 | `			sLabel.pFunc = 0;` |
|       - | 2477 | `		}` |
|       - | 2478 | `		/* Insert in label set */` |
|     110 | 2479 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2480 | `	}` |
|     114 | 2481 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2482 | `	return SXRET_OK;` |
|      58 | 2483 |  |
|       - | 2484 | `/*` |
|       - | 2485 | ` * Compile the so hated 'goto' statement.` |
|       - | 2486 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2487 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2488 | ` * a compiler it has to do this.` |
|       - | 2489 | ` * According to the PHP language reference manual` |
|       - | 2490 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2491 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2492 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2493 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2494 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2495 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2496 | ` *   of a multi-level break` |
|       - | 2497 | ` */` |
|     152 | 2498 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2499 |  |
|       - | 2500 | `	JumpFixup sJump;` |
|       - | 2501 | `	sxi32 rc;` |
|     154 | 2502 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2503 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2504 | `		/* Missing label */` |
|     ! 0 | 2505 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2506 | `		if( rc == SXERR_ABORT ){` |
|       - | 2507 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2508 | `			return SXERR_ABORT;` |
|       - | 2509 | `		}` |
|     ! 0 | 2510 | `		return SXRET_OK;` |
|       - | 2511 | `	}` |
|     154 | 2512 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2513 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2514 | `		if( rc == SXERR_ABORT ){` |
|       - | 2515 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2516 | `			return SXERR_ABORT;` |
|       - | 2517 | `		}` |
|       3 | 2518 | `	}else{` |
|     150 | 2519 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2520 | `		GenBlock *pBlock;` |
|       - | 2521 | `		char *zDup;` |
|       - | 2522 | `		/* Prepare the jump destination */` |
|     150 | 2523 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2524 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2525 | `		/* Duplicate label name */` |
|     150 | 2526 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2527 | `		if( zDup == 0 ){` |
|     ! 0 | 2528 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2529 | `			return SXERR_ABORT;` |
|       - | 2530 | `		}` |
|     150 | 2531 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2532 | `		pBlock = pGen->pCurrent;` |
|     312 | 2533 | `		while( pBlock ){` |
|     196 | 2534 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2535 | `				break;` |
|       - | 2536 | `			}` |
|       - | 2537 | `			/* Point to the upper block */` |
|     164 | 2538 | `			pBlock = pBlock->pParent;` |
|       2 | 2539 | `		}` |
|     150 | 2540 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2541 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2542 | `			if( rc == SXERR_ABORT ){` |
|       - | 2543 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2544 | `				return SXERR_ABORT;` |
|       - | 2545 | `			}` |
|       3 | 2546 | `		}` |
|     150 | 2547 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2548 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2549 | `		}else{` |
|     124 | 2550 | `			sJump.pFunc = 0;` |
|       - | 2551 | `		}` |
|       - | 2552 | `		/* Emit the unconditional jump */` |
|     150 | 2553 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2554 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2555 | `		}` |
|       - | 2556 | `	}` |
|     154 | 2557 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2558 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2559 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2560 | `	}` |
|       - | 2561 | `	/* Statement successfully compiled */` |
|     154 | 2562 | `	return SXRET_OK;` |
|      78 | 2563 |  |
|       - | 2564 | `/*` |
|       - | 2565 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2566 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2567 | ` * failure.` |
|       - | 2568 | ` */` |
|      20 | 2569 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2570 |  |
|       - | 2571 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2572 | `	sxu32 nRawObj;` |
|      10 | 2573 | `	sxu32 nObjIdx;` |
|       - | 2574 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2575 | `	 * a PHP block.` |
|       - | 2576 | `	 */` |
|      10 | 2577 | `Consume:` |
|      21 | 2578 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2579 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2580 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2581 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2582 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2583 | `			return SXERR_ABORT;` |
|       - | 2584 | `		}` |
|       - | 2585 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2586 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2587 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2588 | `		++nRawObj;` |
|     ! 0 | 2589 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2590 | `	}` |
|      21 | 2591 | `	if( nRawObj > 0 ){` |
|       - | 2592 | `		/* Emit the consume instruction */` |
|     ! 0 | 2593 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2594 | `	}` |
|      21 | 2595 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2596 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2597 | `		/* Reset the token set */` |
|     ! 0 | 2598 | `		SySetReset(pTokenSet);` |
|       - | 2599 | `		/* Tokenize input */` |
|     ! 0 | 2600 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2601 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2602 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2603 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2604 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2605 | `		/* Advance the stream cursor */` |
|     ! 0 | 2606 | `		pGen->pRawIn++;` |
|       - | 2607 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2608 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2609 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2610 | `			sxi32 rc;` |
|       - | 2611 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2612 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2613 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2614 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2615 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2616 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2617 | `				return SXERR_ABORT;` |
|     ! 0 | 2618 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2619 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2620 | `			}` |
|     ! 0 | 2621 | `			goto Consume;` |
|       - | 2622 | `		}` |
|     ! 0 | 2623 | `	}else{` |
|       - | 2624 | `		/* No more chunks to process */` |
|      21 | 2625 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2626 | `		return SXERR_EOF;` |
|       - | 2627 | `	}` |
|     ! 0 | 2628 | `	return SXRET_OK;` |
|      11 | 2629 |  |
|       - | 2630 | `/*` |
|       - | 2631 | ` * Compile a PHP block.` |
|       - | 2632 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2633 | ` * optionally delimited by braces {}.` |
|       - | 2634 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2635 | ` * and this function takes care of generating the appropriate error` |
|       - | 2636 | ` * message.` |
|       - | 2637 | ` */` |
|  295738 | 2638 | `static sxi32 PH7_CompileBlock(` |
|       - | 2639 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2640 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2641 | `	)` |
|       2 | 2642 |  |
|       - | 2643 | `	sxi32 rc;` |
|       - | 2644 | `	sxu32 nLine;` |
|  295740 | 2645 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  294332 | 2646 | `		nLine = pGen->pIn->nLine;` |
|  294332 | 2647 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  294332 | 2648 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2649 | `			return SXERR_ABORT;` |
|       - | 2650 | `		}` |
|  294332 | 2651 | `		pGen->pIn++;` |
|       - | 2652 | `		/* Compile until we hit the closing braces '}' */` |
|  406325 | 2653 | `		for(;;){` |
|  812652 | 2654 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2655 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2656 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2657 | `			 	   return SXERR_ABORT;` |
|       - | 2658 | `				}` |
|      21 | 2659 | `				if( rc == SXERR_EOF ){` |
|       - | 2660 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2661 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2662 | `					break;` |
|       - | 2663 | `				}` |
|     ! 0 | 2664 | `			}` |
|  812632 | 2665 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2666 | `				/* Closing braces found,break immediately*/` |
|  294312 | 2667 | `				pGen->pIn++;` |
|  294312 | 2668 | `				break;` |
|       - | 2669 | `			}` |
|       - | 2670 | `			/* Compile a single statement */` |
|  518322 | 2671 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  518322 | 2672 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2673 | `				return SXERR_ABORT;` |
|       - | 2674 | `			}` |
|       2 | 2675 | `		}` |
|  294332 | 2676 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  148575 | 2677 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2678 | `		pGen->pIn++;` |
|     ! 0 | 2679 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2680 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2681 | `			return SXERR_ABORT;` |
|       - | 2682 | `		}` |
|       - | 2683 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2684 | `		for(;;){` |
|     ! 0 | 2685 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2686 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2687 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2688 | `			 	   return SXERR_ABORT;` |
|       - | 2689 | `				}` |
|     ! 0 | 2690 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2691 | `					/* No more token to process */` |
|     ! 0 | 2692 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2693 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2694 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2695 | `					}` |
|     ! 0 | 2696 | `					break;` |
|       - | 2697 | `				}` |
|     ! 0 | 2698 | `			}` |
|     ! 0 | 2699 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2700 | `				sxi32 nKwrd;` |
|       - | 2701 | `				/* Keyword found */` |
|     ! 0 | 2702 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2703 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2704 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2705 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2706 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2707 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2708 | `						}` |
|     ! 0 | 2709 | `						break;` |
|       - | 2710 | `				}` |
|     ! 0 | 2711 | `			}` |
|       - | 2712 | `			/* Compile a single statement */` |
|     ! 0 | 2713 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2714 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2715 | `				return SXERR_ABORT;` |
|       - | 2716 | `			}` |
|     ! 0 | 2717 | `		}` |
|     ! 0 | 2718 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2719 | `	}else{` |
|       - | 2720 | `		/* Compile a single statement */` |
|    1410 | 2721 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 | 2722 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2723 | `			return SXERR_ABORT;` |
|       - | 2724 | `		}` |
|       - | 2725 | `	}` |
|       - | 2726 | `	/* Jump trailing semi-colons ';' */` |
|  295740 | 2727 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2728 | `		pGen->pIn++;` |
|     ! 0 | 2729 | `	}` |
|  295740 | 2730 | `	return SXRET_OK;` |
|  147871 | 2731 |  |
|       - | 2732 | `/*` |
|       - | 2733 | ` * Compile the gentle 'while' statement.` |
|       - | 2734 | ` * According to the PHP language reference` |
|       - | 2735 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2736 | ` *  The basic form of a while statement is:` |
|       - | 2737 | ` *  while (expr)` |
|       - | 2738 | ` *   statement` |
|       - | 2739 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2740 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2741 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2742 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2743 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2744 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2745 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2746 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2747 | ` *  while (expr):` |
|       - | 2748 | ` *    statement` |
|       - | 2749 | ` *   endwhile;` |
|       - | 2750 | ` */` |
|   10838 | 2751 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2752 |  |
|   10840 | 2753 | `	GenBlock *pWhileBlock = 0;` |
|   10840 | 2754 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2755 | `	sxu32 nFalseJump;` |
|       - | 2756 | `	sxu32 nLine;` |
|       - | 2757 | `	sxi32 rc;` |
|   10840 | 2758 | `	nLine = pGen->pIn->nLine;` |
|       - | 2759 | `	/* Jump the 'while' keyword */` |
|   10840 | 2760 | `	pGen->pIn++;` |
|   10840 | 2761 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2762 | `		/* Syntax error */` |
|     ! 0 | 2763 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2764 | `		if( rc == SXERR_ABORT ){` |
|       - | 2765 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2766 | `			return SXERR_ABORT;` |
|       - | 2767 | `		}` |
|     ! 0 | 2768 | `		goto Synchronize;` |
|       - | 2769 | `	}` |
|       - | 2770 | `	/* Jump the left parenthesis '(' */` |
|   10840 | 2771 | `	pGen->pIn++;` |
|       - | 2772 | `	/* Create the loop block */` |
|   10840 | 2773 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10840 | 2774 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2775 | `		return SXERR_ABORT;` |
|       - | 2776 | `	}` |
|       - | 2777 | `	/* Delimit the condition */` |
|   10840 | 2778 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10840 | 2779 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2780 | `		/* Empty expression */` |
|       3 | 2781 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2782 | `		if( rc == SXERR_ABORT ){` |
|       - | 2783 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2784 | `			return SXERR_ABORT;` |
|       - | 2785 | `		}` |
|       1 | 2786 | `	}` |
|       - | 2787 | `	/* Swap token streams */` |
|   10840 | 2788 | `	pTmp = pGen->pEnd;` |
|   10840 | 2789 | `	pGen->pEnd = pEnd;` |
|       - | 2790 | `	/* Compile the expression */` |
|   10840 | 2791 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10840 | 2792 | `	if( rc == SXERR_ABORT ){` |
|       - | 2793 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2794 | `		return SXERR_ABORT;` |
|       - | 2795 | `	}` |
|       - | 2796 | `	/* Update token stream */` |
|   10840 | 2797 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2798 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2799 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2800 | `			return SXERR_ABORT;` |
|       - | 2801 | `		}` |
|     ! 0 | 2802 | `		pGen->pIn++;` |
|     ! 0 | 2803 | `	}` |
|       - | 2804 | `	/* Synchronize pointers */` |
|   10840 | 2805 | `	pGen->pIn  = &pEnd[1];` |
|   10840 | 2806 | `	pGen->pEnd = pTmp;` |
|       - | 2807 | `	/* Emit the false jump */` |
|   10840 | 2808 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2809 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10840 | 2810 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2811 | `	/* Compile the loop body */` |
|   10840 | 2812 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10840 | 2813 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2814 | `		return SXERR_ABORT;` |
|       - | 2815 | `	}` |
|       - | 2816 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10840 | 2817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2818 | `	/* Fix all jumps now the destination is resolved */` |
|   10840 | 2819 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2820 | `	/* Release the loop block */` |
|   10840 | 2821 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2822 | `	/* Statement successfully compiled */` |
|   10840 | 2823 | `	return SXRET_OK;` |
|     ! 0 | 2824 | `Synchronize:` |
|       - | 2825 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2826 | `	 * compiling this erroneous block.` |
|       - | 2827 | `	 */` |
|     ! 0 | 2828 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2829 | `		pGen->pIn++;` |
|     ! 0 | 2830 | `	}` |
|     ! 0 | 2831 | `	return SXRET_OK;` |
|    5421 | 2832 |  |
|       - | 2833 | `/*` |
|       - | 2834 | ` * Compile the ugly do..while() statement.` |
|       - | 2835 | ` * According to the PHP language reference` |
|       - | 2836 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2837 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2838 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2839 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2840 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2841 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2842 | ` *  would end immediately).` |
|       - | 2843 | ` *  There is just one syntax for do-while loops:` |
|       - | 2844 | ` *  <?php` |
|       - | 2845 | ` *  $i = 0;` |
|       - | 2846 | ` *  do {` |
|       - | 2847 | ` *   echo $i;` |
|       - | 2848 | ` *  } while ($i > 0);` |
|       - | 2849 | ` * ?>` |
|       - | 2850 | ` */` |
|       2 | 2851 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2852 |  |
|       3 | 2853 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2854 | `	GenBlock *pDoBlock = 0;` |
|       - | 2855 | `	sxu32 nLine;` |
|       - | 2856 | `	sxi32 rc;` |
|       3 | 2857 | `	nLine = pGen->pIn->nLine;` |
|       - | 2858 | `	/* Jump the 'do' keyword */` |
|       3 | 2859 | `	pGen->pIn++;` |
|       - | 2860 | `	/* Create the loop block */` |
|       3 | 2861 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2862 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2863 | `		return SXERR_ABORT;` |
|       - | 2864 | `	}` |
|       - | 2865 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2866 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2867 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2868 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2869 | `		return SXERR_ABORT;` |
|       - | 2870 | `	}` |
|       3 | 2871 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2872 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2873 | `	}` |
|       3 | 2874 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2875 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2876 | `			/* Missing 'while' statement */` |
|       3 | 2877 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2878 | `			if( rc == SXERR_ABORT ){` |
|       - | 2879 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2880 | `				return SXERR_ABORT;` |
|       - | 2881 | `			}` |
|       3 | 2882 | `			goto Synchronize;` |
|       - | 2883 | `	}` |
|       - | 2884 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2885 | `	pGen->pIn++;` |
|     ! 0 | 2886 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2887 | `		/* Syntax error */` |
|     ! 0 | 2888 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2889 | `		if( rc == SXERR_ABORT ){` |
|       - | 2890 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2891 | `			return SXERR_ABORT;` |
|       - | 2892 | `		}` |
|     ! 0 | 2893 | `		goto Synchronize;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2896 | `	pGen->pIn++;` |
|       - | 2897 | `	/* Delimit the condition */` |
|     ! 0 | 2898 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2899 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2900 | `		/* Empty expression */` |
|     ! 0 | 2901 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2902 | `		if( rc == SXERR_ABORT ){` |
|       - | 2903 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2904 | `			return SXERR_ABORT;` |
|       - | 2905 | `		}` |
|     ! 0 | 2906 | `		goto Synchronize;` |
|       - | 2907 | `	}` |
|       - | 2908 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2909 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2910 | `		JumpFixup *aPost;` |
|       - | 2911 | `		VmInstr *pInstr;` |
|       - | 2912 | `		sxu32 nJumpDest;` |
|       - | 2913 | `		sxu32 n;` |
|     ! 0 | 2914 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2915 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2916 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2917 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2918 | `			if( pInstr ){` |
|       - | 2919 | `				/* Fix */` |
|     ! 0 | 2920 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2921 | `			}` |
|     ! 0 | 2922 | `		}` |
|     ! 0 | 2923 | `	}` |
|       - | 2924 | `	/* Swap token streams */` |
|     ! 0 | 2925 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2926 | `	pGen->pEnd = pEnd;` |
|       - | 2927 | `	/* Compile the expression */` |
|     ! 0 | 2928 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2929 | `	if( rc == SXERR_ABORT ){` |
|       - | 2930 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2931 | `		return SXERR_ABORT;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Update token stream */` |
|     ! 0 | 2934 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2935 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2936 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2937 | `			return SXERR_ABORT;` |
|       - | 2938 | `		}` |
|     ! 0 | 2939 | `		pGen->pIn++;` |
|     ! 0 | 2940 | `	}` |
|     ! 0 | 2941 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2942 | `	pGen->pEnd = pTmp;` |
|       - | 2943 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2944 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2945 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2946 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2947 | `	/* Release the loop block */` |
|     ! 0 | 2948 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2949 | `	/* Statement successfully compiled */` |
|     ! 0 | 2950 | `	return SXRET_OK;` |
|       1 | 2951 | `Synchronize:` |
|       - | 2952 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2953 | `	 * compiling this erroneous block.` |
|       - | 2954 | `	 */` |
|       3 | 2955 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2956 | `		pGen->pIn++;` |
|     ! 0 | 2957 | `	}` |
|       3 | 2958 | `	return SXRET_OK;` |
|       2 | 2959 |  |
|       - | 2960 | `/*` |
|       - | 2961 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2962 | ` * According to the PHP language reference` |
|       - | 2963 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2964 | ` *  The syntax of a for loop is:` |
|       - | 2965 | ` *  for (expr1; expr2; expr3)` |
|       - | 2966 | ` *   statement` |
|       - | 2967 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2968 | ` *  the beginning of the loop.` |
|       - | 2969 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2970 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2971 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2972 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2973 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2974 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2975 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2976 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2977 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2978 | ` *  of using the for truth expression.` |
|       - | 2979 | ` */` |
|   10834 | 2980 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2981 |  |
|   10836 | 2982 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10836 | 2983 | `	GenBlock *pForBlock = 0;` |
|       - | 2984 | `	sxu32 nFalseJump;` |
|       - | 2985 | `	sxu32 nLine;` |
|       - | 2986 | `	sxi32 rc;` |
|   10836 | 2987 | `	nLine = pGen->pIn->nLine;` |
|       - | 2988 | `	/* Jump the 'for' keyword */` |
|   10836 | 2989 | `	pGen->pIn++;` |
|   10836 | 2990 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2991 | `		/* Syntax error */` |
|     ! 0 | 2992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2993 | `		if( rc == SXERR_ABORT ){` |
|       - | 2994 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2995 | `			return SXERR_ABORT;` |
|       - | 2996 | `		}` |
|     ! 0 | 2997 | `		return SXRET_OK;` |
|       - | 2998 | `	}` |
|       - | 2999 | `	/* Jump the left parenthesis '(' */` |
|   10836 | 3000 | `	pGen->pIn++;` |
|       - | 3001 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10836 | 3002 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10836 | 3003 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3004 | `		/* Empty expression */` |
|     ! 0 | 3005 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 3006 | `		if( rc == SXERR_ABORT ){` |
|       - | 3007 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3008 | `			return SXERR_ABORT;` |
|       - | 3009 | `		}` |
|       - | 3010 | `		/* Synchronize */` |
|     ! 0 | 3011 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3012 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3013 | `			pGen->pIn++;` |
|     ! 0 | 3014 | `		}` |
|     ! 0 | 3015 | `		return SXRET_OK;` |
|       - | 3016 | `	}` |
|       - | 3017 | `	/* Swap token streams */` |
|   10836 | 3018 | `	pTmp = pGen->pEnd;` |
|   10836 | 3019 | `	pGen->pEnd = pEnd;` |
|       - | 3020 | `	/* Compile initialization expressions if available */` |
|   10836 | 3021 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3022 | `	/* Pop operand lvalues */` |
|   10836 | 3023 | `	if( rc == SXERR_ABORT ){` |
|       - | 3024 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3025 | `		return SXERR_ABORT;` |
|   10836 | 3026 | `	}else if( rc != SXERR_EMPTY ){` |
|   10834 | 3027 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5416 | 3028 | `	}` |
|   10836 | 3029 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3030 | `		/* Syntax error */` |
|     ! 0 | 3031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3032 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 3033 | `		if( rc == SXERR_ABORT ){` |
|       - | 3034 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3035 | `			return SXERR_ABORT;` |
|       - | 3036 | `		}` |
|     ! 0 | 3037 | `		return SXRET_OK;` |
|       - | 3038 | `	}` |
|       - | 3039 | `	/* Jump the trailing ';' */` |
|   10836 | 3040 | `	pGen->pIn++;` |
|       - | 3041 | `	/* Create the loop block */` |
|   10836 | 3042 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10836 | 3043 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3044 | `		return SXERR_ABORT;` |
|       - | 3045 | `	}` |
|       - | 3046 | `	/* Deffer continue jumps */` |
|   10836 | 3047 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 3048 | `	/* Compile the condition */` |
|   10836 | 3049 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10836 | 3050 | `	if( rc == SXERR_ABORT ){` |
|       - | 3051 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3052 | `		return SXERR_ABORT;` |
|   10836 | 3053 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 3054 | `		/* Emit the false jump */` |
|   10834 | 3055 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 3056 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10834 | 3057 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5416 | 3058 | `	}` |
|   10836 | 3059 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3060 | `		/* Syntax error */` |
|       5 | 3061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 3062 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 3063 | `		if( rc == SXERR_ABORT ){` |
|       - | 3064 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3065 | `			return SXERR_ABORT;` |
|       - | 3066 | `		}` |
|       5 | 3067 | `		return SXRET_OK;` |
|       - | 3068 | `	}` |
|       - | 3069 | `	/* Jump the trailing ';' */` |
|   10832 | 3070 | `	pGen->pIn++;` |
|       - | 3071 | `	/* Save the post condition stream */` |
|   10832 | 3072 | `	pPostStart = pGen->pIn;` |
|       - | 3073 | `	/* Compile the loop body */` |
|   10832 | 3074 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10832 | 3075 | `	pGen->pEnd = pTmp;` |
|   10832 | 3076 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10832 | 3077 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3078 | `		return SXERR_ABORT;` |
|       - | 3079 | `	}` |
|       - | 3080 | `	/* Fix post-continue jumps */` |
|   10832 | 3081 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 3082 | `		JumpFixup *aPost;` |
|       - | 3083 | `		VmInstr *pInstr;` |
|       - | 3084 | `		sxu32 nJumpDest;` |
|       - | 3085 | `		sxu32 n;` |
|      14 | 3086 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 | 3087 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 | 3088 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 | 3089 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 | 3090 | `			if( pInstr ){` |
|       - | 3091 | `				/* Fix jump */` |
|      14 | 3092 | `				pInstr->iP2 = nJumpDest;` |
|       6 | 3093 | `			}` |
|       8 | 3094 | `		}` |
|       6 | 3095 | `	}` |
|       - | 3096 | `	/* compile the post-expressions if available */` |
|   10832 | 3097 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 3098 | `		pPostStart++;` |
|     ! 0 | 3099 | `	}` |
|   10832 | 3100 | `	if( pPostStart < pEnd ){` |
|       - | 3101 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10832 | 3102 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10832 | 3103 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10832 | 3104 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 3105 | `			/* Syntax error */` |
|     ! 0 | 3106 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 3107 | `			if( rc == SXERR_ABORT ){` |
|       - | 3108 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3109 | `				return SXERR_ABORT;` |
|       - | 3110 | `			}` |
|     ! 0 | 3111 | `			return SXRET_OK;` |
|       - | 3112 | `		}` |
|   10832 | 3113 | `		RE_SWAP_DELIMITER(pGen);` |
|   10832 | 3114 | `		if( rc == SXERR_ABORT ){` |
|       - | 3115 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3116 | `			return SXERR_ABORT;` |
|   10832 | 3117 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 3118 | `			/* Pop operand lvalue */` |
|   10832 | 3119 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5415 | 3120 | `		}` |
|    5415 | 3121 | `	}` |
|       - | 3122 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10832 | 3123 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 3124 | `	/* Fix all jumps now the destination is resolved */` |
|   10832 | 3125 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3126 | `	/* Release the loop block */` |
|   10832 | 3127 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3128 | `	/* Statement successfully compiled */` |
|   10832 | 3129 | `	return SXRET_OK;` |
|    5419 | 3130 |  |
|       - | 3131 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 3132 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 3133 | ` * are allowed.` |
|       - | 3134 | ` */` |
|    5766 | 3135 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 3136 |  |
|    5768 | 3137 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5768 | 3138 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 3139 | `		/* Unexpected expression */` |
|     ! 0 | 3140 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 3141 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 3142 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 3143 | `			rc = SXERR_INVALID;` |
|     ! 0 | 3144 | `		}` |
|     ! 0 | 3145 | `	}` |
|    5768 | 3146 | `	return rc;` |
|       2 | 3147 |  |
|       - | 3148 | `/*` |
|       - | 3149 | ` * Compile the 'foreach' statement.` |
|       - | 3150 | ` * According to the PHP language reference` |
|       - | 3151 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 3152 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 3153 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 3154 | ` *  is a minor but useful extension of the first:` |
|       - | 3155 | ` *  foreach (array_expression as $value)` |
|       - | 3156 | ` *    statement` |
|       - | 3157 | ` *  foreach (array_expression as $key => $value)` |
|       - | 3158 | ` *   statement` |
|       - | 3159 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 3160 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 3161 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 3162 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 3163 | ` *  to the variable $key on each loop.` |
|       - | 3164 | ` *  Note:` |
|       - | 3165 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 3166 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 3167 | ` *  Note:` |
|       - | 3168 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 3169 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 3170 | ` *  or after the foreach without resetting it.` |
|       - | 3171 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 3172 | ` *  of copying the value.` |
|       - | 3173 | ` */` |
|    2934 | 3174 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 3175 |  |
|    2936 | 3176 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2936 | 3177 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2936 | 3178 | `	GenBlock *pForeachBlock = 0;` |
|       - | 3179 | `	ph7_foreach_info *pInfo;` |
|       - | 3180 | `	sxu32 nFalseJump;` |
|       - | 3181 | `	VmInstr *pInstr;` |
|       - | 3182 | `	sxu32 nLine;` |
|       - | 3183 | `	sxi32 rc;` |
|    2936 | 3184 | `	nLine = pGen->pIn->nLine;` |
|       - | 3185 | `	/* Jump the 'foreach' keyword */` |
|    2936 | 3186 | `	pGen->pIn++;` |
|    2936 | 3187 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3188 | `		/* Syntax error */` |
|     ! 0 | 3189 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 3190 | `		if( rc == SXERR_ABORT ){` |
|       - | 3191 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3192 | `			return SXERR_ABORT;` |
|       - | 3193 | `		}` |
|     ! 0 | 3194 | `		goto Synchronize;` |
|       - | 3195 | `	}` |
|       - | 3196 | `	/* Jump the left parenthesis '(' */` |
|    2936 | 3197 | `	pGen->pIn++;` |
|       - | 3198 | `	/* Create the loop block */` |
|    2936 | 3199 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2936 | 3200 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3201 | `		return SXERR_ABORT;` |
|       - | 3202 | `	}` |
|       - | 3203 | `	/* Delimit the expression */` |
|    2936 | 3204 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2936 | 3205 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 3206 | `		/* Empty expression */` |
|     ! 0 | 3207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 3208 | `		if( rc == SXERR_ABORT ){` |
|       - | 3209 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 3210 | `			return SXERR_ABORT;` |
|       - | 3211 | `		}` |
|       - | 3212 | `		/* Synchronize */` |
|     ! 0 | 3213 | `		pGen->pIn = pEnd;` |
|     ! 0 | 3214 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 3215 | `			pGen->pIn++;` |
|     ! 0 | 3216 | `		}` |
|     ! 0 | 3217 | `		return SXRET_OK;` |
|       - | 3218 | `	}` |
|       - | 3219 | `	/* Compile the array expression */` |
|    2936 | 3220 | `	pCur = pGen->pIn;` |
|   19646 | 3221 | `	while( pCur < pEnd ){` |
|   19646 | 3222 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2946 | 3223 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2946 | 3224 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 3225 | `				/* Break with the first 'as' found */` |
|    2936 | 3226 | `				break;` |
|       - | 3227 | `			}` |
|       5 | 3228 | `		}` |
|       - | 3229 | `		/* Advance the stream cursor */` |
|   16712 | 3230 | `		pCur++;` |
|       2 | 3231 | `	}` |
|    2936 | 3232 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 3233 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 3234 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 3235 | `		if( rc == SXERR_ABORT ){` |
|       - | 3236 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3237 | `			return SXERR_ABORT;` |
|       - | 3238 | `		}` |
|     ! 0 | 3239 | `		goto Synchronize;` |
|       - | 3240 | `	}` |
|       - | 3241 | `	/* Swap token streams */` |
|    2936 | 3242 | `	pTmp = pGen->pEnd;` |
|    2936 | 3243 | `	pGen->pEnd = pCur;` |
|    2936 | 3244 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2936 | 3245 | `	if( rc == SXERR_ABORT ){` |
|       - | 3246 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3247 | `		return SXERR_ABORT;` |
|       - | 3248 | `	}` |
|       - | 3249 | `	/* Update token stream */` |
|    2936 | 3250 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 3251 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3252 | `		if( rc == SXERR_ABORT ){` |
|       - | 3253 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3254 | `			return SXERR_ABORT;` |
|       - | 3255 | `		}` |
|     ! 0 | 3256 | `		pGen->pIn++;` |
|     ! 0 | 3257 | `	}` |
|    2936 | 3258 | `	pCur++; /* Jump the 'as' keyword */` |
|    2936 | 3259 | `	pGen->pIn = pCur;` |
|    2936 | 3260 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3261 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 3262 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3263 | `			return SXERR_ABORT;` |
|       - | 3264 | `		}` |
|     ! 0 | 3265 | `	}` |
|       - | 3266 | `	/* Create the foreach context */` |
|    2936 | 3267 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2936 | 3268 | `	if( pInfo == 0 ){` |
|     ! 0 | 3269 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 3270 | `		return SXERR_ABORT;` |
|       - | 3271 | `	}` |
|       - | 3272 | `	/* Zero the structure */` |
|    2936 | 3273 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 3274 | `	/* Initialize structure fields */` |
|    2936 | 3275 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 3276 | `	/* Check if we have a key field */` |
|    8854 | 3277 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5920 | 3278 | `		pCur++;` |
|       2 | 3279 | `	}` |
|    2936 | 3280 | `	if( pCur < pEnd ){` |
|       - | 3281 | `		/* Compile the expression holding the key name */` |
|    2844 | 3282 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 3283 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 3284 | `			if( rc == SXERR_ABORT ){` |
|       - | 3285 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3286 | `				return SXERR_ABORT;` |
|       - | 3287 | `			}` |
|     ! 0 | 3288 | `		}else{` |
|    2844 | 3289 | `			pGen->pEnd = pCur;` |
|    2844 | 3290 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2844 | 3291 | `			if( rc == SXERR_ABORT ){` |
|       - | 3292 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3293 | `				return SXERR_ABORT;` |
|       - | 3294 | `			}` |
|    2844 | 3295 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2844 | 3296 | `			if( pInstr->p3 ){` |
|       - | 3297 | `				/* Record key name */` |
|    2844 | 3298 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1421 | 3299 | `			}` |
|    2844 | 3300 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 3301 | `		}` |
|    2844 | 3302 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1421 | 3303 | `	}` |
|    2936 | 3304 | `	pGen->pEnd = pEnd;` |
|    2936 | 3305 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 3306 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 3307 | `		if( rc == SXERR_ABORT ){` |
|       - | 3308 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3309 | `			return SXERR_ABORT;` |
|       - | 3310 | `		}` |
|     ! 0 | 3311 | `		goto Synchronize;` |
|       - | 3312 | `	}` |
|    2936 | 3313 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3314 | `		pGen->pIn++;` |
|       - | 3315 | `		/* Pass by reference  */` |
|      11 | 3316 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3317 | `	}` |
|       - | 3318 | `	/* Check if the value target is list() */` |
|    2936 | 3319 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3320 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3321 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3322 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3323 | `		 */` |
|       - | 3324 | `		static int iForeachListCnt = 0;` |
|       - | 3325 | `		char zTmp[128];` |
|       - | 3326 | `		sxu32 nLen;` |
|       - | 3327 | `		char *zDup;` |
|      10 | 3328 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3329 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3330 | `		if( zDup == 0 ){` |
|     ! 0 | 3331 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3332 | `			return SXERR_ABORT;` |
|       - | 3333 | `		}` |
|      10 | 3334 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3335 | `		/* Save list() token boundaries */` |
|      10 | 3336 | `		pListStart = pGen->pIn;` |
|       - | 3337 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3338 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3339 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3340 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3341 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3342 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3343 | `				return SXERR_ABORT;` |
|       - | 3344 | `			}` |
|       3 | 3345 | `			goto Synchronize;` |
|       - | 3346 | `		}` |
|       7 | 3347 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3348 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3349 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3350 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3351 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3352 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3353 | `				return SXERR_ABORT;` |
|       - | 3354 | `			}` |
|     ! 0 | 3355 | `			goto Synchronize;` |
|       - | 3356 | `		}` |
|       7 | 3357 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3358 | `		pListEnd = pGen->pIn;` |
|       7 | 3359 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2931 | 3360 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3361 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3362 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3363 | `		 */` |
|       - | 3364 | `		static int iForeachShortListCnt = 0;` |
|       - | 3365 | `		char zTmp[128];` |
|       - | 3366 | `		sxu32 nLen;` |
|       - | 3367 | `		char *zDup;` |
|       3 | 3368 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3369 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3370 | `		if( zDup == 0 ){` |
|     ! 0 | 3371 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3372 | `			return SXERR_ABORT;` |
|       - | 3373 | `		}` |
|       3 | 3374 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3375 | `		/* Save [...] token boundaries */` |
|       3 | 3376 | `		pListStart = pGen->pIn;` |
|       - | 3377 | `		/* Advance past [...] */` |
|       3 | 3378 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3379 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3380 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3381 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3382 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3383 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3384 | `				return SXERR_ABORT;` |
|       - | 3385 | `			}` |
|     ! 0 | 3386 | `			goto Synchronize;` |
|       - | 3387 | `		}` |
|       3 | 3388 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3389 | `		pListEnd = pGen->pIn;` |
|       3 | 3390 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3391 | `	}else{` |
|       - | 3392 | `		/* Compile the expression holding the value name */` |
|    2926 | 3393 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2926 | 3394 | `		if( rc == SXERR_ABORT ){` |
|       - | 3395 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3396 | `			return SXERR_ABORT;` |
|       - | 3397 | `		}` |
|    2926 | 3398 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2926 | 3399 | `		if( pInstr->p3 ){` |
|       - | 3400 | `			/* Record value name */` |
|    2926 | 3401 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1462 | 3402 | `		}` |
|       - | 3403 | `	}` |
|       - | 3404 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2934 | 3405 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3406 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2934 | 3407 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3408 | `	/* Record the first instruction to execute */` |
|    2934 | 3409 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3410 | `	/* Emit the FOREACH_STEP instruction */` |
|    2934 | 3411 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3412 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2934 | 3413 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3414 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2934 | 3415 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3416 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3417 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3418 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3419 | `		 */` |
|       9 | 3420 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3421 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3422 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3423 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3424 | `		 */` |
|       9 | 3425 | `		pSavedIn = pGen->pIn;` |
|       9 | 3426 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3427 | `		pGen->pIn = pListStart;` |
|       9 | 3428 | `		pGen->pEnd = pListEnd;` |
|       9 | 3429 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3430 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3431 | `		}else{` |
|       7 | 3432 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3433 | `		}` |
|       9 | 3434 | `		pGen->pIn = pSavedIn;` |
|       9 | 3435 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3436 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3437 | `			return SXERR_ABORT;` |
|       - | 3438 | `		}` |
|       - | 3439 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3440 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3441 | `	}` |
|       - | 3442 | `	/* Compile the loop body */` |
|    2934 | 3443 | `	pGen->pIn = &pEnd[1];` |
|    2934 | 3444 | `	pGen->pEnd = pTmp;` |
|    2934 | 3445 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2934 | 3446 | `	if( rc == SXERR_ABORT ){` |
|       - | 3447 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3448 | `		return SXERR_ABORT;` |
|       - | 3449 | `	}` |
|       - | 3450 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2934 | 3451 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3452 | `	/* Fix all jumps now the destination is resolved */` |
|    2934 | 3453 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3454 | `	/* Release the loop block */` |
|    2934 | 3455 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3456 | `	/* Statement successfully compiled */` |
|    2934 | 3457 | `	return SXRET_OK;` |
|       1 | 3458 | `Synchronize:` |
|       - | 3459 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3460 | `	 * compiling this erroneous block.` |
|       - | 3461 | `	 */` |
|       3 | 3462 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3463 | `		pGen->pIn++;` |
|     ! 0 | 3464 | `	}` |
|       3 | 3465 | `	return SXRET_OK;` |
|    1469 | 3466 |  |
|       - | 3467 | `/*` |
|       - | 3468 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3469 | ` * According to the PHP language reference` |
|       - | 3470 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3471 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3472 | ` *  that is similar to that of C:` |
|       - | 3473 | ` *  if (expr)` |
|       - | 3474 | ` *   statement` |
|       - | 3475 | ` *  else construct:` |
|       - | 3476 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3477 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3478 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3479 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3480 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3481 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3482 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3483 | ` *  elseif` |
|       - | 3484 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3485 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3486 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3487 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3488 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3489 | ` *   <?php` |
|       - | 3490 | ` *    if ($a > $b) {` |
|       - | 3491 | ` *     echo "a is bigger than b";` |
|       - | 3492 | ` *    } elseif ($a == $b) {` |
|       - | 3493 | ` *     echo "a is equal to b";` |
|       - | 3494 | ` *    } else {` |
|       - | 3495 | ` *     echo "a is smaller than b";` |
|       - | 3496 | ` *    }` |
|       - | 3497 | ` *    ?>` |
|       - | 3498 | ` */` |
|  107714 | 3499 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3500 |  |
|  107716 | 3501 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  107716 | 3502 | `	GenBlock *pCondBlock = 0;` |
|       - | 3503 | `	sxu32 nJumpIdx;` |
|       - | 3504 | `	sxu32 nKeyID;` |
|       - | 3505 | `	sxi32 rc;` |
|       - | 3506 | `	/* Jump the 'if' keyword */` |
|  107716 | 3507 | `	pGen->pIn++;` |
|  107716 | 3508 | `	pToken = pGen->pIn;` |
|       - | 3509 | `	/* Create the conditional block */` |
|  107716 | 3510 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  107716 | 3511 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3512 | `		return SXERR_ABORT;` |
|       - | 3513 | `	}` |
|       - | 3514 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   59237 | 3515 | `	for(;;){` |
|  118476 | 3516 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3517 | `			/* Syntax error */` |
|     ! 0 | 3518 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3519 | `				pToken--;` |
|     ! 0 | 3520 | `			}` |
|     ! 0 | 3521 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3522 | `			if( rc == SXERR_ABORT ){` |
|       - | 3523 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3524 | `				return SXERR_ABORT;` |
|       - | 3525 | `			}` |
|     ! 0 | 3526 | `			goto Synchronize;` |
|       - | 3527 | `		}` |
|       - | 3528 | `		/* Jump the left parenthesis '(' */` |
|  118476 | 3529 | `		pToken++;` |
|       - | 3530 | `		/* Delimit the condition */` |
|  118476 | 3531 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  118476 | 3532 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3533 | `			/* Syntax error */` |
|     ! 0 | 3534 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3535 | `				pToken--;` |
|     ! 0 | 3536 | `			}` |
|     ! 0 | 3537 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3538 | `			if( rc == SXERR_ABORT ){` |
|       - | 3539 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3540 | `				return SXERR_ABORT;` |
|       - | 3541 | `			}` |
|     ! 0 | 3542 | `			goto Synchronize;` |
|       - | 3543 | `		}` |
|       - | 3544 | `		/* Swap token streams */` |
|  118476 | 3545 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3546 | `		/* Compile the condition */` |
|  118476 | 3547 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3548 | `		/* Update token stream */` |
|  118476 | 3549 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3550 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3551 | `			pGen->pIn++;` |
|     ! 0 | 3552 | `		}` |
|  118476 | 3553 | `		pGen->pIn  = &pEnd[1];` |
|  118476 | 3554 | `		pGen->pEnd = pTmp;` |
|  118476 | 3555 | `		if( rc == SXERR_ABORT ){` |
|       - | 3556 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3557 | `			return SXERR_ABORT;` |
|       - | 3558 | `		}` |
|       - | 3559 | `		/* Emit the false jump */` |
|  118476 | 3560 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3561 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  118476 | 3562 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3563 | `		/* Compile the body */` |
|  118476 | 3564 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  118476 | 3565 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3566 | `			return SXERR_ABORT;` |
|       - | 3567 | `		}` |
|  118476 | 3568 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   31886 | 3569 | `			break;` |
|       - | 3570 | `		}` |
|       - | 3571 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   54708 | 3572 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54708 | 3573 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   35174 | 3574 | `			break;` |
|       - | 3575 | `		}` |
|       - | 3576 | `		/* Emit the unconditional jump */` |
|   19536 | 3577 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3578 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19536 | 3579 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19536 | 3580 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14144 | 3581 | `			pToken = &pGen->pIn[1];` |
|   14144 | 3582 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5398 | 3583 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4389 | 3584 | `					break;` |
|       - | 3585 | `			}` |
|    5370 | 3586 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2684 | 3587 | `		}` |
|   10762 | 3588 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3589 | `		/* Synchronize cursors */` |
|   10762 | 3590 | `		pToken = pGen->pIn;` |
|       - | 3591 | `		/* Fix the false jump */` |
|   10762 | 3592 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3593 | `	} /* For(;;) */` |
|       - | 3594 | `	/* Fix the false jump */` |
|  107716 | 3595 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  107716 | 3596 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   43946 | 3597 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3598 | `			/* Compile the else block */` |
|    8776 | 3599 | `			pGen->pIn++;` |
|    8776 | 3600 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8776 | 3601 | `			if( rc == SXERR_ABORT ){` |
|       - | 3602 |  |
|     ! 0 | 3603 | `				return SXERR_ABORT;` |
|       - | 3604 | `			}` |
|    4387 | 3605 | `	}` |
|  107716 | 3606 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3607 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  107716 | 3608 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3609 | `	/* Release the conditional block */` |
|  107716 | 3610 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3611 | `	/* Statement successfully compiled */` |
|  107716 | 3612 | `	return SXRET_OK;` |
|     ! 0 | 3613 | `Synchronize:` |
|       - | 3614 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3615 | `	 */` |
|     ! 0 | 3616 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3617 | `		pGen->pIn++;` |
|     ! 0 | 3618 | `	}` |
|     ! 0 | 3619 | `	return SXRET_OK;` |
|   53859 | 3620 |  |
|       - | 3621 | `/*` |
|       - | 3622 | ` * Compile the global construct.` |
|       - | 3623 | ` * According to the PHP language reference` |
|       - | 3624 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3625 | ` *  to be used in that function.` |
|       - | 3626 | ` *  Example #1 Using global` |
|       - | 3627 | ` *  <?php` |
|       - | 3628 | ` *   $a = 1;` |
|       - | 3629 | ` *   $b = 2;` |
|       - | 3630 | ` *   function Sum()` |
|       - | 3631 | ` *   {` |
|       - | 3632 | ` *    global $a, $b;` |
|       - | 3633 | ` *    $b = $a + $b;` |
|       - | 3634 | ` *   }` |
|       - | 3635 | ` *   Sum();` |
|       - | 3636 | ` *   echo $b;` |
|       - | 3637 | ` *  ?>` |
|       - | 3638 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3639 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3640 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3641 | ` */` |
|      26 | 3642 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3643 |  |
|      28 | 3644 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3645 | `	sxi32 nExpr;` |
|       - | 3646 | `	sxi32 rc;` |
|       - | 3647 | `	/* Jump the 'global' keyword */` |
|      28 | 3648 | `	pGen->pIn++;` |
|      28 | 3649 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3650 | `		/* Nothing to process */` |
|     ! 0 | 3651 | `		return SXRET_OK;` |
|       - | 3652 | `	}` |
|      28 | 3653 | `	pTmp = pGen->pEnd;` |
|      28 | 3654 | `	nExpr = 0;` |
|      56 | 3655 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3656 | `		if( pGen->pIn < pNext ){` |
|      30 | 3657 | `			pGen->pEnd = pNext;` |
|      30 | 3658 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3659 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3660 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3661 | `					return SXERR_ABORT;` |
|       - | 3662 | `				}` |
|     ! 0 | 3663 | `			}else{` |
|      30 | 3664 | `				pGen->pIn++;` |
|      30 | 3665 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3666 | `					/* Emit a warning */` |
|     ! 0 | 3667 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3668 | `				}else{` |
|      30 | 3669 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3670 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3671 | `						return SXERR_ABORT;` |
|      30 | 3672 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3673 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3674 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3675 | `							/* Variable name, not a constant */` |
|      30 | 3676 | `							pLast->iP1 = 0;` |
|      14 | 3677 | `						}` |
|      30 | 3678 | `						nExpr++;` |
|      14 | 3679 | `					}` |
|       - | 3680 | `				}` |
|       - | 3681 | `			}` |
|      14 | 3682 | `		}` |
|       - | 3683 | `		/* Next expression in the stream */` |
|      30 | 3684 | `		pGen->pIn = pNext;` |
|       - | 3685 | `		/* Jump trailing commas */` |
|      32 | 3686 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3687 | `			pGen->pIn++;` |
|       1 | 3688 | `		}` |
|       2 | 3689 | `	}` |
|       - | 3690 | `	/* Restore token stream */` |
|      28 | 3691 | `	pGen->pEnd = pTmp;` |
|      28 | 3692 | `	if( nExpr > 0 ){` |
|       - | 3693 | `		/* Emit the uplink instruction */` |
|      28 | 3694 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3695 | `	}` |
|      28 | 3696 | `	return SXRET_OK;` |
|      15 | 3697 |  |
|       - | 3698 | `/*` |
|       - | 3699 | ` * Compile the return statement.` |
|       - | 3700 | ` * According to the PHP language reference` |
|       - | 3701 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3702 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3703 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3704 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3705 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3706 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3707 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3708 | ` *  from within the main script file, then script execution end.` |
|       - | 3709 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3710 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3711 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3712 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3713 | ` */` |
|  156362 | 3714 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3715 |  |
|  156364 | 3716 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3717 | `	sxi32 rc;` |
|       - | 3718 | `	/* Jump the 'return' keyword */` |
|  156364 | 3719 | `	pGen->pIn++;` |
|  156364 | 3720 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3721 | `		/* Compile the expression */` |
|  156342 | 3722 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  156342 | 3723 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3724 | `			return SXERR_ABORT;` |
|  156342 | 3725 | `		}else if(rc != SXERR_EMPTY ){` |
|  156342 | 3726 | `			nRet = 1;` |
|   78170 | 3727 | `		}` |
|   78170 | 3728 | `	}` |
|       - | 3729 | `	/* Emit the done instruction */` |
|  156364 | 3730 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  156364 | 3731 | `	return SXRET_OK;` |
|   78183 | 3732 |  |
|       - | 3733 | `/*` |
|       - | 3734 | ` * Compile a yield expression.` |
|       - | 3735 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3736 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3737 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3738 | ` */` |
|      32 | 3739 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3740 |  |
|       - | 3741 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3742 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3743 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3744 | `	sxi32 rc;` |
|      16 | 3745 | `	(void)iCompileFlag;` |
|       - | 3746 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3747 | `	pGen->pIn++;` |
|       - | 3748 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3749 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3750 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3751 | `		/* Bare yield — no value */` |
|     ! 0 | 3752 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3753 | `		return SXRET_OK;` |
|       - | 3754 | `	}` |
|       - | 3755 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3756 | `	pSplit = 0;` |
|       - | 3757 | `	{` |
|      34 | 3758 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3759 | `		sxi32 nNest = 0;` |
|      78 | 3760 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3761 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3762 | `				nNest++;` |
|      52 | 3763 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3764 | `				nNest--;` |
|      52 | 3765 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3766 | `				pSplit = pCur;` |
|       7 | 3767 | `				break;` |
|       - | 3768 | `			}` |
|      46 | 3769 | `			pCur++;` |
|       2 | 3770 | `		}` |
|       - | 3771 | `	}` |
|      34 | 3772 | `	pTmp = pGen->pEnd;` |
|      34 | 3773 | `	if( pSplit ){` |
|       - | 3774 | `		/* yield $key => $value */` |
|       7 | 3775 | `		pGen->pEnd = pSplit;` |
|       7 | 3776 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3777 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3778 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3779 | `		pGen->pEnd = pTmp;` |
|       7 | 3780 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3781 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3782 | `		iP1 = 1;` |
|       7 | 3783 | `		iP2 = 1;` |
|       4 | 3784 | `	}else{` |
|       - | 3785 | `		/* yield $value */` |
|      28 | 3786 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3787 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3788 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3789 | `			iP1 = 1;` |
|      13 | 3790 | `		}` |
|       - | 3791 | `	}` |
|      34 | 3792 | `	pGen->pEnd = pTmp;` |
|      34 | 3793 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3794 | `	return SXRET_OK;` |
|      18 | 3795 |  |
|       - | 3796 | `/*` |
|       - | 3797 | ` * Compile the die/exit language construct.` |
|       - | 3798 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3799 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3800 | ` */` |
|      88 | 3801 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3802 |  |
|      90 | 3803 | `	sxi32 nExpr = 0;` |
|       - | 3804 | `	sxi32 rc;` |
|       - | 3805 | `	/* Jump the die/exit keyword */` |
|      90 | 3806 | `	pGen->pIn++;` |
|      90 | 3807 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3808 | `		/* Compile the expression */` |
|      90 | 3809 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3810 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3811 | `			return SXERR_ABORT;` |
|      90 | 3812 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3813 | `			nExpr = 1;` |
|      44 | 3814 | `		}` |
|      44 | 3815 | `	}` |
|       - | 3816 | `	/* Emit the HALT instruction */` |
|      90 | 3817 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3818 | `	return SXRET_OK;` |
|      46 | 3819 |  |
|       - | 3820 | `/*` |
|       - | 3821 | ` * Compile the 'echo' language construct.` |
|       - | 3822 | ` */` |
|   11138 | 3823 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3824 |  |
|   11140 | 3825 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3826 | `	sxi32 rc;` |
|       - | 3827 | `	/* Jump the 'echo' keyword */` |
|   11140 | 3828 | `	pGen->pIn++;` |
|       - | 3829 | `	/* Compile arguments one after one */` |
|   11140 | 3830 | `	pTmp = pGen->pEnd;` |
|   22670 | 3831 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11532 | 3832 | `		if( pGen->pIn < pNext ){` |
|   11532 | 3833 | `			pGen->pEnd = pNext;` |
|   11532 | 3834 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11532 | 3835 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3836 | `				return SXERR_ABORT;` |
|   11532 | 3837 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3838 | `				/* Emit the consume instruction */` |
|   11508 | 3839 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5753 | 3840 | `			}` |
|    5765 | 3841 | `		}` |
|       - | 3842 | `		/* Jump trailing commas */` |
|   11924 | 3843 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     394 | 3844 | `			pNext++;` |
|       2 | 3845 | `		}` |
|   11532 | 3846 | `		pGen->pIn = pNext;` |
|       2 | 3847 | `	}` |
|       - | 3848 | `	/* Restore token stream */` |
|   11140 | 3849 | `	pGen->pEnd = pTmp;` |
|   11140 | 3850 | `	return SXRET_OK;` |
|    5571 | 3851 |  |
|       - | 3852 | `/*` |
|       - | 3853 | ` * Compile the static statement.` |
|       - | 3854 | ` * According to the PHP language reference` |
|       - | 3855 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3856 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3857 | ` *  when program execution leaves this scope.` |
|       - | 3858 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3859 | ` * Symisc eXtension.` |
|       - | 3860 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3861 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3862 | ` *  Example` |
|       - | 3863 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3864 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3865 | ` */` |
|       2 | 3866 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3867 |  |
|       - | 3868 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3869 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3870 | `	GenBlock *pBlock;` |
|       - | 3871 | `	SyString *pName;` |
|       - | 3872 | `	char *zDup;` |
|       - | 3873 | `	sxu32 nLine;` |
|       - | 3874 | `	sxi32 rc;` |
|       - | 3875 | `	/* Jump the static keyword */` |
|       3 | 3876 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3877 | `	pGen->pIn++;` |
|       - | 3878 | `	/* Extract the enclosing function if any */` |
|       3 | 3879 | `	pBlock = pGen->pCurrent;` |
|       5 | 3880 | `	while( pBlock ){` |
|       5 | 3881 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3882 | `			break;` |
|       - | 3883 | `		}` |
|       - | 3884 | `		/* Point to the upper block */` |
|       3 | 3885 | `		pBlock = pBlock->pParent;` |
|       1 | 3886 | `	}` |
|       3 | 3887 | `	if( pBlock == 0 ){` |
|       - | 3888 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3889 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3890 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3892 | `				return SXERR_ABORT;` |
|       - | 3893 | `			}` |
|     ! 0 | 3894 | `			goto Synchronize;` |
|       - | 3895 | `		}` |
|       - | 3896 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3897 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3898 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3899 | `			return SXERR_ABORT;` |
|     ! 0 | 3900 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3901 | `			/* Emit the POP instruction */` |
|     ! 0 | 3902 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3903 | `		}` |
|     ! 0 | 3904 | `		return SXRET_OK;` |
|       - | 3905 | `	}` |
|       3 | 3906 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3907 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3908 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3909 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3910 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3911 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3912 | `				return SXERR_ABORT;` |
|       - | 3913 | `			}` |
|       3 | 3914 | `			goto Synchronize;` |
|       - | 3915 | `	}` |
|     ! 0 | 3916 | `	pGen->pIn++;` |
|       - | 3917 | `	/* Extract variable name */` |
|     ! 0 | 3918 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3919 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3920 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3921 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3922 | `		goto Synchronize;` |
|       - | 3923 | `	}` |
|       - | 3924 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3925 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3926 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3927 | `	/* Duplicate variable name */` |
|     ! 0 | 3928 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3929 | `	if( zDup == 0 ){` |
|     ! 0 | 3930 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3931 | `		return SXERR_ABORT;` |
|       - | 3932 | `	}` |
|     ! 0 | 3933 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3934 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3935 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3936 | `		SySet *pInstrContainer;` |
|       - | 3937 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3938 | `		 * Static variable can take any complex expression including function` |
|       - | 3939 | `		 * call as their initialization value.` |
|       - | 3940 | `		 * Example:` |
|       - | 3941 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3942 | `		 */` |
|     ! 0 | 3943 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3944 | `		/* Swap bytecode container */` |
|     ! 0 | 3945 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3946 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3947 | `		/* Compile the expression */` |
|     ! 0 | 3948 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3949 | `		/* Emit the done instruction */` |
|     ! 0 | 3950 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3951 | `		/* Restore default bytecode container */` |
|     ! 0 | 3952 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3953 | `	}` |
|       - | 3954 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3955 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3956 | `	return SXRET_OK;` |
|       1 | 3957 | `Synchronize:` |
|       - | 3958 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3959 | `	 * statement.` |
|       - | 3960 | `	 */` |
|       5 | 3961 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3962 | `		pGen->pIn++;` |
|       1 | 3963 | `	}` |
|       3 | 3964 | `	return SXRET_OK;` |
|       2 | 3965 |  |
|       - | 3966 | `/*` |
|       - | 3967 | ` * Compile the var statement.` |
|       - | 3968 | ` * Symisc Extension:` |
|       - | 3969 | ` *      var statement can be used outside of a class definition.` |
|       - | 3970 | ` */` |
|       4 | 3971 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3972 |  |
|       - | 3973 | `	sxu32 nLine;` |
|       - | 3974 | `	sxi32 rc;` |
|       5 | 3975 | `	nLine = pGen->pIn->nLine;` |
|       - | 3976 | `	/* Jump the 'var' keyword */` |
|       5 | 3977 | `	pGen->pIn++;` |
|       5 | 3978 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3979 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3980 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3981 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3982 | `			pGen->pIn++;` |
|     ! 0 | 3983 | `		}` |
|     ! 0 | 3984 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3985 | `			return SXERR_ABORT;` |
|       - | 3986 | `		}` |
|     ! 0 | 3987 | `	}else{` |
|       - | 3988 | `		/* Compile the expression */` |
|       5 | 3989 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3990 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3991 | `			return SXERR_ABORT;` |
|       5 | 3992 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3993 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3994 | `		}` |
|       - | 3995 | `	}` |
|       5 | 3996 | `	return SXRET_OK;` |
|       3 | 3997 |  |
|       - | 3998 | `/*` |
|       - | 3999 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 4000 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 4001 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 4002 | ` */` |
|       - | 4003 | `/*` |
|       - | 4004 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 4005 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 4006 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 4007 | ` * qualified name and updates the instruction's operand index.` |
|       - | 4008 | ` *` |
|       - | 4009 | ` * Resolution order:` |
|       - | 4010 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 4011 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 4012 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 4013 | ` *` |
|       - | 4014 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 4015 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 4016 | ` * Returns the (possibly new) literal index.` |
|       - | 4017 | ` */` |
|  320948 | 4018 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 4019 |  |
|       - | 4020 | `	ph7_value *pLit;` |
|       - | 4021 | `	const char *zLit;` |
|       - | 4022 | `	SyString sQualified;` |
|       - | 4023 | `	sxu32 nLit;` |
|       - | 4024 | `	sxu32 k;` |
|       - | 4025 | `	sxu32 nNewIdx;` |
|       - | 4026 | `	int hasNsSep;` |
|       - | 4027 | `	SyHashEntry *pImport;` |
|       - | 4028 | `	ph7_value *pNew;` |
|  320950 | 4029 | `	if( pFromImport ){` |
|  306940 | 4030 | `		*pFromImport = 0;` |
|  153469 | 4031 | `	}` |
|  320950 | 4032 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  320950 | 4033 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 4034 | `		return nOrigIdx;` |
|       - | 4035 | `	}` |
|  320950 | 4036 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  320950 | 4037 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 4038 | `	/* Skip if already qualified (contains backslash) */` |
|  320950 | 4039 | `	hasNsSep = 0;` |
| 3452014 | 4040 | `	for( k = 0; k < nLit; k++ ){` |
| 3131098 | 4041 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1565534 | 4042 | `	}` |
|  320950 | 4043 | `	if( hasNsSep ){` |
|      34 | 4044 | `		return nOrigIdx;` |
|       - | 4045 | `	}` |
|       - | 4046 | `	/* Check use imports first (works even outside namespaces) */` |
|  320918 | 4047 | `	SyBlobReset(&pGen->sWorker);` |
|  320918 | 4048 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  320918 | 4049 | `	if( pImport ){` |
|      38 | 4050 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 4051 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 4052 | `		if( pFromImport ){` |
|      18 | 4053 | `			*pFromImport = 1;` |
|       8 | 4054 | `		}` |
|      20 | 4055 | `	}else{` |
|  320882 | 4056 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  320800 | 4057 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 4058 | `		}` |
|       - | 4059 | `		/* Prepend current namespace */` |
|      84 | 4060 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      84 | 4061 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      84 | 4062 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 4063 | `	}` |
|       - | 4064 | `	/* Look up or create a new literal for the qualified name */` |
|     120 | 4065 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     120 | 4066 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      52 | 4067 | `		return nNewIdx; /* Already interned */` |
|       - | 4068 | `	}` |
|      70 | 4069 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      70 | 4070 | `	if( pNew == 0 ){` |
|     ! 0 | 4071 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 4072 | `	}` |
|      70 | 4073 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      70 | 4074 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      70 | 4075 | `	return nNewIdx;` |
|  160476 | 4076 |  |
|       - | 4077 | `/*` |
|       - | 4078 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 4079 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 4080 | ` */` |
|   27156 | 4081 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4082 |  |
|       - | 4083 | `	SyHashEntry *pImport;` |
|       - | 4084 | `	/* Check use imports first */` |
|   27158 | 4085 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   27158 | 4086 | `	if( pImport ){` |
|      12 | 4087 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 4088 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 4089 | `		return;` |
|       - | 4090 | `	}` |
|       - | 4091 | `	/* Prepend current namespace if active */` |
|   27148 | 4092 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 4093 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 4094 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 4095 | `	}` |
|   27148 | 4096 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13580 | 4097 |  |
|       - | 4098 | `/*` |
|       - | 4099 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 4100 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 4101 | ` * The caller must release pOut when done.` |
|       - | 4102 | ` */` |
|   46214 | 4103 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 4104 |  |
|   46216 | 4105 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 4106 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      50 | 4107 | `		SyBlobAppend(pOut,"\\",1);` |
|      24 | 4108 | `	}` |
|   46216 | 4109 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   46216 | 4110 |  |
|       - | 4111 | `/*` |
|       - | 4112 | ` * Compile a namespace statement` |
|       - | 4113 | ` * According to the PHP language reference manual` |
|       - | 4114 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 4115 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 4116 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 4117 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 4118 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 4119 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 4120 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 4121 | ` *  programming world.` |
|       - | 4122 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 4123 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 4124 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 4125 | ` *  classes/functions/constants.` |
|       - | 4126 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 4127 | ` *  readability of source code.` |
|       - | 4128 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 4129 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 4130 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 4131 | ` *       class MyClass {}` |
|       - | 4132 | ` *       function myfunction() {}` |
|       - | 4133 | ` *       const MYCONST = 1;` |
|       - | 4134 | ` *       $a = new MyClass;` |
|       - | 4135 | ` *       $c = new \my\name\MyClass;` |
|       - | 4136 | ` *       $a = strlen('hi');` |
|       - | 4137 | ` *       $d = namespace\MYCONST;` |
|       - | 4138 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 4139 | ` *       echo constant($d);` |
|       - | 4140 | ` * NOTE` |
|       - | 4141 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4142 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4143 | ` */` |
|       - | 4144 | `/*` |
|       - | 4145 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 4146 | ` */` |
|      10 | 4147 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 4148 |  |
|      11 | 4149 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       5 | 4150 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       5 | 4151 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       5 | 4152 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       5 | 4153 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       5 | 4154 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 4155 | `	return "token";` |
|       6 | 4156 |  |
|      96 | 4157 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 4158 |  |
|       - | 4159 | `	sxu32 nLine;` |
|       - | 4160 | `	sxi32 rc;` |
|      98 | 4161 | `	nLine = pGen->pIn->nLine;` |
|      98 | 4162 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 4163 | `	/* Reset namespace and clear previous use imports */` |
|      98 | 4164 | `	SyBlobReset(&pGen->sNamespace);` |
|      98 | 4165 | `	SyHashRelease(&pGen->hUseImports);` |
|      98 | 4166 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4167 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      98 | 4168 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4169 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      98 | 4170 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 4171 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 4172 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 4173 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4174 | `		return SXRET_OK;` |
|       - | 4175 | `	}` |
|      98 | 4176 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 4177 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 4178 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4179 | `		return SXRET_OK;` |
|       - | 4180 | `	}` |
|      98 | 4181 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 4182 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 4183 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 4184 | `		return SXRET_OK;` |
|       - | 4185 | `	}` |
|       - | 4186 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     232 | 4187 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     136 | 4188 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 4189 | `			/* Append backslash separator */` |
|      21 | 4190 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 4191 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 4192 | `			}` |
|      11 | 4193 | `		}else{` |
|       - | 4194 | `			/* Append identifier */` |
|     116 | 4195 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 4196 | `		}` |
|     136 | 4197 | `		pGen->pIn++;` |
|       2 | 4198 | `	}` |
|       - | 4199 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 4200 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 4201 | `	{` |
|      98 | 4202 | `		char *zNsDup = 0;` |
|      98 | 4203 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     143 | 4204 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      94 | 4205 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      47 | 4206 | `		}` |
|      98 | 4207 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 4208 | `	}` |
|      98 | 4209 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 4210 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 4211 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 4212 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 4213 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4214 | `			return SXERR_ABORT;` |
|       - | 4215 | `		}` |
|       2 | 4216 | `	}` |
|      98 | 4217 | `	return SXRET_OK;` |
|      50 | 4218 |  |
|       - | 4219 | `/*` |
|       - | 4220 | ` * Compile the 'use' statement` |
|       - | 4221 | ` * According to the PHP language reference manual` |
|       - | 4222 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 4223 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 4224 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 4225 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 4226 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 4227 | ` *  a function or constant is not supported.` |
|       - | 4228 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 4229 | ` * NOTE` |
|       - | 4230 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 4231 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 4232 | ` */` |
|      66 | 4233 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 4234 |  |
|       - | 4235 | `	sxu32 nLine;` |
|       - | 4236 | `	sxi32 rc;` |
|       - | 4237 | `	SyBlob sPath;` |
|       - | 4238 | `	SyString sAlias;` |
|       - | 4239 | `	SyToken *pLast;` |
|       - | 4240 | `	char *zDup;` |
|       - | 4241 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 4242 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 4243 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 4244 | `	nLine = pGen->pIn->nLine;` |
|      68 | 4245 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 4246 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 4247 | `	iUseType = 0;` |
|      68 | 4248 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 4249 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 4250 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 4251 | `			iUseType = 1;` |
|      16 | 4252 | `			pGen->pIn++;` |
|      23 | 4253 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 4254 | `			iUseType = 2;` |
|      16 | 4255 | `			pGen->pIn++;` |
|       7 | 4256 | `		}` |
|      14 | 4257 | `	}` |
|       - | 4258 | `	/* Select target hash tables based on import type */` |
|      68 | 4259 | `	switch( iUseType ){` |
|       7 | 4260 | `		case 1:` |
|      16 | 4261 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 4262 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 4263 | `			break;` |
|       7 | 4264 | `		case 2:` |
|      16 | 4265 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 4266 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 4267 | `			break;` |
|      19 | 4268 | `		default:` |
|      40 | 4269 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 4270 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 4271 | `			break;` |
|       - | 4272 | `	}` |
|      68 | 4273 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 4274 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 4275 | `	for(;;){` |
|      70 | 4276 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 4277 | `			break;` |
|       - | 4278 | `		}` |
|      70 | 4279 | `		SyBlobReset(&sPath);` |
|      70 | 4280 | `		pLast = 0;` |
|       - | 4281 | `		/* Collect the full namespace path */` |
|     254 | 4282 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 4283 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 4284 | `				pLast = pGen->pIn;` |
|     126 | 4285 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 4286 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 4287 | `				}` |
|     126 | 4288 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 4289 | `			}` |
|     186 | 4290 | `			pGen->pIn++;` |
|       2 | 4291 | `		}` |
|      70 | 4292 | `		if( pLast == 0 ){` |
|       - | 4293 | `			/* Empty path */` |
|       5 | 4294 | `			break;` |
|       - | 4295 | `		}` |
|       - | 4296 | `		/* Default alias is the last component of the path */` |
|      66 | 4297 | `		sAlias = pLast->sData;` |
|       - | 4298 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 4299 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 4300 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 4301 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 4302 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 4303 | `				sAlias = pGen->pIn->sData;` |
|      18 | 4304 | `				pGen->pIn++;` |
|       8 | 4305 | `			}` |
|       8 | 4306 | `		}` |
|       - | 4307 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4308 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4309 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4310 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4311 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4312 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4313 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4314 | `				return SXERR_ABORT;` |
|       - | 4315 | `			}` |
|       2 | 4316 | `		}` |
|       - | 4317 | `		/* Register the import: alias -> FQN.` |
|       - | 4318 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4319 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4320 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4321 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4322 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4323 | `		if( zDup ){` |
|      66 | 4324 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4325 | `			if( pVmHash ){` |
|       - | 4326 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4327 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4328 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4329 | `				if( zAliasDup ){` |
|      38 | 4330 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4331 | `				}` |
|      18 | 4332 | `			}` |
|      66 | 4333 | `			if( iUseType == 2 ){` |
|       - | 4334 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4335 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4336 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4337 | `				if( zAliasDup ){` |
|       - | 4338 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4339 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4340 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4341 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4342 | `					if( azPair ){` |
|      16 | 4343 | `						azPair[0] = zAliasDup;` |
|      16 | 4344 | `						azPair[1] = zDup;` |
|      16 | 4345 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4346 | `					}` |
|       7 | 4347 | `				}` |
|       7 | 4348 | `			}` |
|      32 | 4349 | `		}` |
|       - | 4350 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4351 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4352 | `			pGen->pIn++;` |
|       2 | 4353 | `		}else{` |
|      33 | 4354 | `			break;` |
|       - | 4355 | `		}` |
|       1 | 4356 | `	}` |
|      68 | 4357 | `	SyBlobRelease(&sPath);` |
|      68 | 4358 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4359 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4360 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4361 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4362 | `			return SXERR_ABORT;` |
|       - | 4363 | `		}` |
|       1 | 4364 | `	}` |
|      68 | 4365 | `	return SXRET_OK;` |
|      35 | 4366 |  |
|       - | 4367 | `/*` |
|       - | 4368 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4369 | ` *` |
|       - | 4370 | ` * According to the PHP language reference manual.` |
|       - | 4371 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4372 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4373 | ` *  declare (directive)` |
|       - | 4374 | ` *   statement` |
|       - | 4375 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4376 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4377 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4378 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4379 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4380 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4381 | ` * <?php` |
|       - | 4382 | ` * // these are the same:` |
|       - | 4383 | ` * // you can use this:` |
|       - | 4384 | ` * declare(ticks=1) {` |
|       - | 4385 | ` *   // entire script here` |
|       - | 4386 | ` * }` |
|       - | 4387 | ` * // or you can use this:` |
|       - | 4388 | ` * declare(ticks=1);` |
|       - | 4389 | ` * // entire script here` |
|       - | 4390 | ` * ?>` |
|       - | 4391 | ` *` |
|       - | 4392 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4393 | ` */` |
|       8 | 4394 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4395 |  |
|       9 | 4396 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4397 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4398 | `	sxi32 rc;` |
|       9 | 4399 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4400 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4401 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4403 | `			return SXERR_ABORT;` |
|       - | 4404 | `		}` |
|       5 | 4405 | `		goto Synchro;` |
|       - | 4406 | `	}` |
|       5 | 4407 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4408 | `	/* Delimit the directive */` |
|       5 | 4409 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4410 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4412 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4413 | `			return SXERR_ABORT;` |
|       - | 4414 | `		}` |
|     ! 0 | 4415 | `		return SXRET_OK;` |
|       - | 4416 | `	}` |
|       - | 4417 | `	/* Update the cursor */` |
|       5 | 4418 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4420 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4421 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4422 | `			return SXERR_ABORT;` |
|       - | 4423 | `		}` |
|     ! 0 | 4424 | `	}` |
|       - | 4425 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4426 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4427 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4428 | `		ph7_lib_version()` |
|       - | 4429 | `		);` |
|       - | 4430 | `	/*All done */` |
|       5 | 4431 | `	return SXRET_OK;` |
|       2 | 4432 | `Synchro:` |
|       - | 4433 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4434 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4435 | `		pGen->pIn++;` |
|       1 | 4436 | `	}` |
|       5 | 4437 | `	return SXRET_OK;` |
|       5 | 4438 |  |
|       - | 4439 | `/*` |
|       - | 4440 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4441 | ` * as follows:` |
|       - | 4442 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4443 | ` * {` |
|       - | 4444 | ` *   return "Making a cup of $type.\n";` |
|       - | 4445 | ` * }` |
|       - | 4446 | ` * Symisc eXtension.` |
|       - | 4447 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4448 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4449 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4450 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4451 | ` *      {` |
|       - | 4452 | ` *       var_dump($a);` |
|       - | 4453 | ` *      }` |
|       - | 4454 | ` *     //call test without args` |
|       - | 4455 | ` *      test();` |
|       - | 4456 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4457 | ` *      Example:` |
|       - | 4458 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4459 | ` * 3 -) Function overloading!!` |
|       - | 4460 | ` *      Example:` |
|       - | 4461 | ` *      function foo($a) {` |
|       - | 4462 | ` *   	  return $a.PHP_EOL;` |
|       - | 4463 | ` *	    }` |
|       - | 4464 | ` *	    function foo($a, $b) {` |
|       - | 4465 | ` *   	  return $a + $b;` |
|       - | 4466 | ` *	    }` |
|       - | 4467 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4468 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4469 | ` *      // Same arg` |
|       - | 4470 | ` *	   function foo(string $a)` |
|       - | 4471 | ` *	   {` |
|       - | 4472 | ` *	     echo "a is a string\n";` |
|       - | 4473 | ` *	     var_dump($a);` |
|       - | 4474 | ` *	   }` |
|       - | 4475 | ` *	  function foo(int $a)` |
|       - | 4476 | ` *	  {` |
|       - | 4477 | ` *	    echo "a is integer\n";` |
|       - | 4478 | ` *	    var_dump($a);` |
|       - | 4479 | ` *	  }` |
|       - | 4480 | ` *	  function foo(array $a)` |
|       - | 4481 | ` *	  {` |
|       - | 4482 | ` * 	    echo "a is an array\n";` |
|       - | 4483 | ` * 	    var_dump($a);` |
|       - | 4484 | ` *	  }` |
|       - | 4485 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4486 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4487 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4488 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4489 | ` * introduced by the PH7 engine.` |
|       - | 4490 | ` */` |
|   42956 | 4491 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4492 |  |
|       - | 4493 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4494 | `	SySet *pInstrContainer;` |
|       - | 4495 | `	sxi32 rc;` |
|       - | 4496 | `	/* Swap token stream */` |
|   42958 | 4497 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   42958 | 4498 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   42958 | 4499 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4500 | `	/* Compile the expression holding the argument value */` |
|   42958 | 4501 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4502 | `	/* Emit the done instruction */` |
|   42958 | 4503 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   42958 | 4504 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   42958 | 4505 | `	RE_SWAP_DELIMITER(pGen);` |
|   42958 | 4506 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4507 | `		return SXERR_ABORT;` |
|       - | 4508 | `	}` |
|   42958 | 4509 | `	return SXRET_OK;` |
|   21480 | 4510 |  |
|       - | 4511 | `/*` |
|       - | 4512 | ` * Collect function arguments one after one.` |
|       - | 4513 | ` * According to the PHP language reference manual.` |
|       - | 4514 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4515 | ` * list of expressions.` |
|       - | 4516 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4517 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4518 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4519 | ` * for more information.` |
|       - | 4520 | ` * Example #1 Passing arrays to functions` |
|       - | 4521 | ` * <?php` |
|       - | 4522 | ` * function takes_array($input)` |
|       - | 4523 | ` * {` |
|       - | 4524 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4525 | ` * }` |
|       - | 4526 | ` * ?>` |
|       - | 4527 | ` * Making arguments be passed by reference` |
|       - | 4528 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4529 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4530 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4531 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4532 | ` * to the argument name in the function definition:` |
|       - | 4533 | ` * Example #2 Passing function parameters by reference` |
|       - | 4534 | ` * <?php` |
|       - | 4535 | ` * function add_some_extra(&$string)` |
|       - | 4536 | ` * {` |
|       - | 4537 | ` *   $string .= 'and something extra.';` |
|       - | 4538 | ` * }` |
|       - | 4539 | ` * $str = 'This is a string, ';` |
|       - | 4540 | ` * add_some_extra($str);` |
|       - | 4541 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4542 | ` * ?>` |
|       - | 4543 | ` *` |
|       - | 4544 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4545 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4546 | ` * on these extension.` |
|       - | 4547 | ` */` |
|   51604 | 4548 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4549 |  |
|       - | 4550 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4551 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4552 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4553 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4554 | `	sxi32 rc;` |
|       - | 4555 |  |
|   51606 | 4556 | `	pIn = pGen->pIn;` |
|   51606 | 4557 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4558 | `	/* Process arguments one after one */` |
|   65286 | 4559 | `	for(;;){` |
|  130574 | 4560 | `		if( pIn >= pEnd ){` |
|       - | 4561 | `			/* No more arguments to process */` |
|   51604 | 4562 | `			break;` |
|       - | 4563 | `		}` |
|   78972 | 4564 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   78972 | 4565 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4566 | `		/* Detect nullable prefix '?' on type hints */` |
|   78972 | 4567 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 4568 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      16 | 4569 | `			pIn++;` |
|       7 | 4570 | `		}` |
|       - | 4571 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   78972 | 4572 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4573 | `			pIn++;` |
|       2 | 4574 | `		}` |
|   78972 | 4575 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   53734 | 4576 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   48358 | 4577 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   48358 | 4578 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4579 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   48358 | 4580 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4581 | `					sArg.nType = MEMOBJ_BOOL;` |
|   48358 | 4582 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13432 | 4583 | `					sArg.nType = MEMOBJ_INT;` |
|   41643 | 4584 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   34912 | 4585 | `					sArg.nType = MEMOBJ_STRING;` |
|   17473 | 4586 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4587 | `					sArg.nType = MEMOBJ_REAL;` |
|      18 | 4588 | `				}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      16 | 4589 | `					sArg.nType = MEMOBJ_OBJ;` |
|       9 | 4590 | `				}else{` |
|       4 | 4591 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4592 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4593 | `						&pIn->sData);` |
|       - | 4594 | `				}` |
|   24180 | 4595 | `			}else{` |
|    5378 | 4596 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4597 | `				char *zDupLocal;` |
|       - | 4598 | `				/* Argument must be a class instance,record that*/` |
|    5378 | 4599 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5378 | 4600 | `				if( zDupLocal ){` |
|    5378 | 4601 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5378 | 4602 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2688 | 4603 | `				}` |
|       - | 4604 | `			}` |
|   53734 | 4605 | `			pIn++;` |
|   26866 | 4606 | `		}` |
|   78972 | 4607 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4608 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4609 | `			return rc;` |
|       - | 4610 | `		}` |
|   78972 | 4611 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4612 | `			/* Pass by reference,record that */` |
|    2710 | 4613 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2710 | 4614 | `			pIn++;` |
|    1354 | 4615 | `		}` |
|   78972 | 4616 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4617 | `			/* Variadic parameter: ...$args */` |
|      28 | 4618 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      28 | 4619 | `			pIn++;` |
|      13 | 4620 | `		}` |
|   78972 | 4621 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4622 | `			/* Invalid argument */` |
|     ! 0 | 4623 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4624 | `			return rc;` |
|       - | 4625 | `		}` |
|   78972 | 4626 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4627 | `		/* Copy argument name */` |
|   78972 | 4628 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   78972 | 4629 | `		if( zDup == 0 ){` |
|     ! 0 | 4630 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4631 | `			return SXERR_ABORT;` |
|       - | 4632 | `		}` |
|   78972 | 4633 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   78972 | 4634 | `		pIn++;` |
|   78972 | 4635 | `		if( pIn < pEnd ){` |
|   48858 | 4636 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4637 | `				SyToken *pDefend;` |
|   42960 | 4638 | `				sxi32 iNest = 0;` |
|   42960 | 4639 | `				pIn++; /* Jump the equal sign */` |
|   42960 | 4640 | `				pDefend = pIn;` |
|       - | 4641 | `				/* Process the default value associated with this argument */` |
|   91284 | 4642 | `				while( pDefend < pEnd ){` |
|   69798 | 4643 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   21474 | 4644 | `						break;` |
|       - | 4645 | `					}` |
|   48326 | 4646 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4647 | `						/* Increment nesting level */` |
|    2686 | 4648 | `						iNest++;` |
|   46984 | 4649 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4650 | `						/* Decrement nesting level */` |
|    2686 | 4651 | `						iNest--;` |
|    1342 | 4652 | `					}` |
|   48326 | 4653 | `					pDefend++;` |
|       2 | 4654 | `				}` |
|   42960 | 4655 | `				if( pIn >= pDefend ){` |
|       3 | 4656 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4657 | `					return rc;` |
|       - | 4658 | `				}` |
|       - | 4659 | `				/* Process default value */` |
|   42958 | 4660 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   42958 | 4661 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4662 | `					return rc;` |
|       - | 4663 | `				}` |
|       - | 4664 | `				/* Point beyond the default value */` |
|   42958 | 4665 | `				pIn = pDefend;` |
|   21478 | 4666 | `			}` |
|   48856 | 4667 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4668 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4669 | `				return rc;` |
|       - | 4670 | `			}` |
|   48856 | 4671 | `			pIn++; /* Jump the trailing comma */` |
|   24427 | 4672 | `		}` |
|       - | 4673 | `		/* Append argument signature */` |
|   78970 | 4674 | `		if( sArg.nType > 0 ){` |
|   53732 | 4675 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4676 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5378 | 4677 | `				int marker = 'o';` |
|    5378 | 4678 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5378 | 4679 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2690 | 4680 | `			}else{` |
|       - | 4681 | `				int c;` |
|   48356 | 4682 | `				c = 'n'; /* cc warning */` |
|       - | 4683 | `				/* Type leading character */` |
|   48356 | 4684 | `				switch(sArg.nType){` |
|     ! 0 | 4685 | `				case MEMOBJ_HASHMAP:` |
|       - | 4686 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4687 | `					c = 'h';` |
|     ! 0 | 4688 | `					break;` |
|    6715 | 4689 | `				case MEMOBJ_INT:` |
|       - | 4690 | `					/* Integer */` |
|   13432 | 4691 | `					c = 'i';` |
|   13432 | 4692 | `					break;` |
|     ! 0 | 4693 | `				case MEMOBJ_BOOL:` |
|       - | 4694 | `					/* Bool */` |
|     ! 0 | 4695 | `					c = 'b';` |
|     ! 0 | 4696 | `					break;` |
|     ! 0 | 4697 | `				case MEMOBJ_REAL:` |
|       - | 4698 | `					/* Float */` |
|     ! 0 | 4699 | `					c = 'f';` |
|     ! 0 | 4700 | `					break;` |
|   17455 | 4701 | `				case MEMOBJ_STRING:` |
|       - | 4702 | `					/* String */` |
|   34912 | 4703 | `					c = 's';` |
|   34912 | 4704 | `					break;` |
|       7 | 4705 | `				case MEMOBJ_OBJ:` |
|       - | 4706 | `					/* Object */` |
|      16 | 4707 | `					c = 'o';` |
|      14 | 4708 | `					break;` |
|     ! 0 | 4709 | `				default:` |
|     ! 0 | 4710 | `					break;` |
|       - | 4711 | `				}` |
|   48356 | 4712 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4713 | `			}` |
|   26867 | 4714 | `		}else{` |
|       - | 4715 | `			/* No type is associated with this parameter which mean` |
|       - | 4716 | `			 * that this function is not condidate for overloading.` |
|       - | 4717 | `			 */` |
|   25240 | 4718 | `			SyBlobRelease(&sSig);` |
|       - | 4719 | `		}` |
|       - | 4720 | `		/* Save in the argument set */` |
|   78970 | 4721 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4722 | `	}` |
|   51604 | 4723 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4724 | `		/* Save function signature */` |
|   32254 | 4725 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16126 | 4726 | `	}` |
|   51604 | 4727 | `	return SXRET_OK;` |
|   25804 | 4728 |  |
|       - | 4729 | `/*` |
|       - | 4730 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4731 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4732 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4733 | ` */` |
|  143392 | 4734 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4735 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4736 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4737 | `	)` |
|       2 | 4738 |  |
|       - | 4739 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4740 | `	GenBlock *pBlock;` |
|       - | 4741 | `	sxu32 nGotoOfft;` |
|       - | 4742 | `	sxi32 rc;` |
|       - | 4743 | `	/* Attach the new function */` |
|  143394 | 4744 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  143394 | 4745 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4746 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4747 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4748 | `		return SXERR_ABORT;` |
|       - | 4749 | `	}` |
|  143394 | 4750 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4751 | `	/* Swap bytecode containers */` |
|  143394 | 4752 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  143394 | 4753 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4754 | `	/* Compile the body */` |
|  143394 | 4755 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4756 | `	/* Fix exception jumps now the destination is resolved */` |
|  143394 | 4757 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4758 | `	/* Emit the final return if not yet done */` |
|  143394 | 4759 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4760 | `	/* Fix gotos jumps now the destination is resolved */` |
|  143394 | 4761 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4762 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4763 | `	}` |
|  143394 | 4764 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4765 | `	/* Restore the default container */` |
|  143394 | 4766 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4767 | `	/* Leave function block */` |
|  143394 | 4768 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  143394 | 4769 | `	if( rc == SXERR_ABORT ){` |
|       - | 4770 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4771 | `		return SXERR_ABORT;` |
|       - | 4772 | `	}` |
|       - | 4773 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4774 | `	{` |
|  143394 | 4775 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4776 | `		sxu32 i;` |
| 2976866 | 4777 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2833490 | 4778 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4779 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4780 | `				break;` |
|       - | 4781 | `			}` |
| 1416738 | 4782 | `		}` |
|       - | 4783 | `	}` |
|       - | 4784 | `	/* All done, function body compiled */` |
|  143394 | 4785 | `	return SXRET_OK;` |
|   71698 | 4786 |  |
|       - | 4787 | `/*` |
|       - | 4788 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4789 | ` * According to the PHP language reference manual.` |
|       - | 4790 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4791 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4792 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4793 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4794 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4795 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4796 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4797 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4798 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4799 | ` *` |
|       - | 4800 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4801 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4802 | ` * on these extension.` |
|       - | 4803 | ` */` |
|       - | 4804 | `/*` |
|       - | 4805 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4806 | ` */` |
|       6 | 4807 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4808 |  |
|       - | 4809 | `	sxu32 i;` |
|      31 | 4810 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4811 | `		int a = zA[i], b = zB[i];` |
|      25 | 4812 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4813 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4814 | `		if( a != b ) return a - b;` |
|      13 | 4815 | `	}` |
|       7 | 4816 | `	return 0;` |
|       4 | 4817 |  |
|       - | 4818 | `/*` |
|       - | 4819 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4820 | ` */` |
|       2 | 4821 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4822 |  |
|       3 | 4823 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4824 | `	if( zDup ){` |
|       3 | 4825 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4826 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4827 | `	}` |
|       3 | 4828 |  |
|       - | 4829 | `/*` |
|       - | 4830 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4831 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4832 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4833 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4834 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4835 | ` */` |
|  164912 | 4836 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4837 |  |
|  164914 | 4838 | `	SyToken *pCur = pGen->pIn;` |
|  164914 | 4839 | `	pFunc->nReturnType = 0;` |
|  164914 | 4840 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  164914 | 4841 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  164850 | 4842 | `		return; /* No return type */` |
|       - | 4843 | `	}` |
|      66 | 4844 | `	pCur++; /* Skip ':' */` |
|      66 | 4845 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4846 | `		pGen->pIn = pCur;` |
|     ! 0 | 4847 | `		return;` |
|       - | 4848 | `	}` |
|       - | 4849 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      66 | 4850 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4851 | `		pCur++;` |
|       7 | 4852 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4853 | `			pGen->pIn = pCur;` |
|     ! 0 | 4854 | `			return;` |
|       - | 4855 | `		}` |
|       3 | 4856 | `	}` |
|      66 | 4857 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      60 | 4858 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      60 | 4859 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4860 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      59 | 4861 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4862 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      57 | 4863 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      20 | 4864 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      47 | 4865 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      32 | 4866 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      23 | 4867 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4868 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       7 | 4869 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       3 | 4870 | `			pFunc->nReturnType = MEMOBJ_OBJ;` |
|       4 | 4871 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4872 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4873 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4874 | `		}` |
|      60 | 4875 | `		pCur++;` |
|      36 | 4876 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4877 | `		SyString *pType = &pCur->sData;` |
|       7 | 4878 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4879 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4880 | `		}else{` |
|       - | 4881 | `			/* Class/interface name */` |
|     ! 0 | 4882 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4883 | `		}` |
|       7 | 4884 | `		pCur++;` |
|       3 | 4885 | `	}` |
|      66 | 4886 | `	pGen->pIn = pCur;` |
|   82458 | 4887 |  |
|       - | 4888 |  |
|   35604 | 4889 | `static sxi32 GenStateCompileFunc(` |
|       - | 4890 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4891 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4892 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4893 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4894 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4895 | `	)` |
|       2 | 4896 |  |
|       - | 4897 | `	ph7_vm_func *pFunc;` |
|       - | 4898 | `	SyToken *pEnd;` |
|       - | 4899 | `	sxu32 nLine;` |
|       - | 4900 | `	char *zName;` |
|       - | 4901 | `	sxi32 rc;` |
|       - | 4902 | `	/* Extract line number */` |
|   35606 | 4903 | `	nLine = pGen->pIn->nLine;` |
|       - | 4904 | `	/* Jump the left parenthesis '(' */` |
|   35606 | 4905 | `	pGen->pIn++;` |
|       - | 4906 | `	/* Delimit the function signature */` |
|   35606 | 4907 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   35606 | 4908 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4909 | `		/* Syntax error */` |
|       7 | 4910 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4911 | `		if( rc == SXERR_ABORT ){` |
|       - | 4912 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4913 | `			return SXERR_ABORT;` |
|       - | 4914 | `		}` |
|       7 | 4915 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4916 | `		return SXRET_OK;` |
|       - | 4917 | `	}` |
|       - | 4918 | `	/* Create the function state */` |
|   35600 | 4919 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   35600 | 4920 | `	if( pFunc == 0 ){` |
|     ! 0 | 4921 | `		goto OutOfMem;` |
|       - | 4922 | `	}` |
|       - | 4923 | `	/* Build the function name, prepending namespace if active */` |
|   35607 | 4924 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4925 | `		SyBlob sFQN;` |
|       - | 4926 | `		sxu32 nLen;` |
|      16 | 4927 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4928 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4929 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4930 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4931 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4932 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4933 | `		SyBlobRelease(&sFQN);` |
|      16 | 4934 | `		if( zName == 0 ){` |
|     ! 0 | 4935 | `			goto OutOfMem;` |
|       - | 4936 | `		}` |
|      16 | 4937 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4938 | `	}else{` |
|   35586 | 4939 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   35586 | 4940 | `		if( zName == 0 ){` |
|     ! 0 | 4941 | `			goto OutOfMem;` |
|       - | 4942 | `		}` |
|   35586 | 4943 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4944 | `	}` |
|   35600 | 4945 | `	if( pGen->pIn < pEnd ){` |
|       - | 4946 | `		/* Collect function arguments */` |
|   24678 | 4947 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   24678 | 4948 | `		if( rc == SXERR_ABORT ){` |
|       - | 4949 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4950 | `			return SXERR_ABORT;` |
|       - | 4951 | `		}` |
|   12338 | 4952 | `	}` |
|       - | 4953 | `	/* Point past ')' and parse optional return type ': type' */` |
|   35600 | 4954 | `	pGen->pIn = &pEnd[1];` |
|   35600 | 4955 | `	GenStateParseReturnType(pGen, pFunc);` |
|   35600 | 4956 | `	if( bHandleClosure ){` |
|       - | 4957 | `		ph7_vm_func_closure_env sEnv;` |
|     170 | 4958 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     168 | 4959 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      93 | 4960 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 | 4961 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4962 | `				/* Closure,record environment variable */` |
|      16 | 4963 | `				pGen->pIn++;` |
|      16 | 4964 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4965 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4966 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4967 | `						return SXERR_ABORT;` |
|       - | 4968 | `					}` |
|     ! 0 | 4969 | `				}` |
|      16 | 4970 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4971 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 | 4972 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 | 4973 | `					int iFlagsLocal = 0;` |
|      34 | 4974 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 | 4975 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 | 4976 | `						break;` |
|       - | 4977 | `					}` |
|      20 | 4978 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 | 4979 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4980 | `						/* Pass by reference,record that */` |
|     ! 0 | 4981 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4982 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4983 | `							);` |
|     ! 0 | 4984 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4985 | `						pGen->pIn++;` |
|     ! 0 | 4986 | `					}` |
|      18 | 4987 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 | 4988 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4989 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4990 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4991 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4992 | `								return SXERR_ABORT;` |
|       - | 4993 | `							}` |
|       - | 4994 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4995 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4996 | `								pGen->pIn++;` |
|     ! 0 | 4997 | `							}` |
|     ! 0 | 4998 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4999 | `								pGen->pIn++;` |
|     ! 0 | 5000 | `							}` |
|     ! 0 | 5001 | `							break;` |
|       - | 5002 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 5003 | `					}else{` |
|       - | 5004 | `						SyString *pNameLocal;` |
|       - | 5005 | `						char *zDup;` |
|       - | 5006 | `						/* Duplicate variable name */` |
|      20 | 5007 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 | 5008 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 | 5009 | `						if( zDup ){` |
|       - | 5010 | `							/* Zero the structure */` |
|      20 | 5011 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 | 5012 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 | 5013 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 | 5014 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 | 5015 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 5016 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 5017 | `									got_this = 1;` |
|     ! 0 | 5018 | `							}` |
|       - | 5019 | `							/* Save imported variable */` |
|      20 | 5020 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 | 5021 | `						}else{` |
|     ! 0 | 5022 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5023 | `							 return SXERR_ABORT;` |
|       - | 5024 | `						}` |
|       - | 5025 | `					}` |
|      20 | 5026 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 | 5027 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5028 | `						/* Ignore trailing commas */` |
|       7 | 5029 | `						pGen->pIn++;` |
|       1 | 5030 | `					}` |
|       2 | 5031 | `				}` |
|      16 | 5032 | `				if( !got_this ){` |
|       - | 5033 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 5034 | `					 * available to the closure environment.` |
|       - | 5035 | `					 */` |
|      16 | 5036 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 5037 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 | 5038 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 5039 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 | 5040 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 | 5041 | `				}` |
|      16 | 5042 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 5043 | `					/* Mark as closure */` |
|      16 | 5044 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 | 5045 | `				}` |
|       7 | 5046 | `		}` |
|      84 | 5047 | `	}` |
|       - | 5048 | `	/* Compile the body */` |
|   35600 | 5049 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   35600 | 5050 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5051 | `		return SXERR_ABORT;` |
|       - | 5052 | `	}` |
|   35600 | 5053 | `	if( ppFunc ){` |
|     170 | 5054 | `		*ppFunc = pFunc;` |
|      84 | 5055 | `	}` |
|   35600 | 5056 | `	rc = SXRET_OK;` |
|   35600 | 5057 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 5058 | `		/* Finally register the function */` |
|   35586 | 5059 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17792 | 5060 | `	}` |
|   35600 | 5061 | `	if( rc == SXRET_OK ){` |
|   35600 | 5062 | `		return SXRET_OK;` |
|       - | 5063 | `	}` |
|       - | 5064 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 5065 | `OutOfMem:` |
|       - | 5066 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 5067 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 5068 | `	 */` |
|     ! 0 | 5069 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 5070 | `	return SXERR_ABORT;` |
|   17804 | 5071 |  |
|       - | 5072 | `/*` |
|       - | 5073 | ` * Compile a standard PHP function.` |
|       - | 5074 | ` *  Refer to the block-comment above for more information.` |
|       - | 5075 | ` */` |
|   35442 | 5076 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 5077 |  |
|       - | 5078 | `	SyString *pName;` |
|       - | 5079 | `	sxi32 iFlags;` |
|       - | 5080 | `	sxu32 nLine;` |
|       - | 5081 | `	sxi32 rc;` |
|       - | 5082 |  |
|   35444 | 5083 | `	nLine = pGen->pIn->nLine;` |
|   35444 | 5084 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   35444 | 5085 | `	iFlags = 0;` |
|   35444 | 5086 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5087 | `		/* Return by reference,remember that */` |
|       7 | 5088 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5089 | `		/* Jump the '&' token */` |
|       7 | 5090 | `		pGen->pIn++;` |
|       3 | 5091 | `	}` |
|   35444 | 5092 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5093 | `		/* Invalid function name */` |
|       5 | 5094 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 5095 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5096 | `			return SXERR_ABORT;` |
|       - | 5097 | `		}` |
|       - | 5098 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 5099 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 5100 | `			pGen->pIn++;` |
|       1 | 5101 | `		}` |
|       5 | 5102 | `		return SXRET_OK;` |
|       - | 5103 | `	}` |
|   35440 | 5104 | `	pName = &pGen->pIn->sData;` |
|   35440 | 5105 | `	nLine = pGen->pIn->nLine;` |
|       - | 5106 | `	/* Jump the function name */` |
|   35440 | 5107 | `	pGen->pIn++;` |
|   35440 | 5108 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5109 | `		/* Syntax error */` |
|       3 | 5110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 5111 | `		if( rc == SXERR_ABORT ){` |
|       - | 5112 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5113 | `			return SXERR_ABORT;` |
|       - | 5114 | `		}` |
|       - | 5115 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 5116 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 5117 | `			pGen->pIn++;` |
|     ! 0 | 5118 | `		}` |
|       3 | 5119 | `		return SXRET_OK;` |
|       - | 5120 | `	}` |
|       - | 5121 | `	/* Compile function body */` |
|   35438 | 5122 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   35438 | 5123 | `	return rc;` |
|   17723 | 5124 |  |
|       - | 5125 | `/*` |
|       - | 5126 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 5127 | ` * According to the PHP language reference manual` |
|       - | 5128 | ` *  Visibility:` |
|       - | 5129 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 5130 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 5131 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 5132 | ` *  Members declared protected can be accessed only within the class` |
|       - | 5133 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 5134 | ` *  may only be accessed by the class that defines the member.` |
|       - | 5135 | ` */` |
|  164462 | 5136 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 5137 |  |
|  164464 | 5138 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8118 | 5139 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  156348 | 5140 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18832 | 5141 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 5142 | `	}` |
|       - | 5143 | `	/* Assume public by default */` |
|  137518 | 5144 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   82233 | 5145 |  |
|       - | 5146 | `/*` |
|       - | 5147 | ` * Compile a class constant.` |
|       - | 5148 | ` * According to the PHP language reference manual` |
|       - | 5149 | ` *  Class Constants` |
|       - | 5150 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 5151 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 5152 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 5153 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 5154 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 5155 | ` *   It's also possible for interfaces to have constants.` |
|       - | 5156 | ` * Symisc eXtension.` |
|       - | 5157 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 5158 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5159 | ` *  Example:` |
|       - | 5160 | ` *   class Test{` |
|       - | 5161 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5162 | ` *   };` |
|       - | 5163 | ` *   var_dump(TEST::MyConst);` |
|       - | 5164 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5165 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5166 | ` */` |
|      30 | 5167 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5168 |  |
|      32 | 5169 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5170 | `	SySet *pInstrContainer;` |
|       - | 5171 | `	ph7_class_attr *pCons;` |
|       - | 5172 | `	SyString *pName;` |
|       - | 5173 | `	sxi32 rc;` |
|       - | 5174 | `	/* Extract visibility level */` |
|      32 | 5175 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 5176 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 5177 | `loop:` |
|       - | 5178 | `	/* Mark as constant */` |
|      32 | 5179 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 5180 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5181 | `		/* Invalid constant name */` |
|     ! 0 | 5182 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 5183 | `		if( rc == SXERR_ABORT ){` |
|       - | 5184 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5185 | `			return SXERR_ABORT;` |
|       - | 5186 | `		}` |
|     ! 0 | 5187 | `		goto Synchronize;` |
|       - | 5188 | `	}` |
|       - | 5189 | `	/* Peek constant name */` |
|      32 | 5190 | `	pName = &pGen->pIn->sData;` |
|       - | 5191 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 5192 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 5193 | `		/* Reserved constant name */` |
|     ! 0 | 5194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 5195 | `		if( rc == SXERR_ABORT ){` |
|       - | 5196 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5197 | `			return SXERR_ABORT;` |
|       - | 5198 | `		}` |
|     ! 0 | 5199 | `		goto Synchronize;` |
|       - | 5200 | `	}` |
|       - | 5201 | `	/* Advance the stream cursor */` |
|      32 | 5202 | `	pGen->pIn++;` |
|      32 | 5203 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 5204 | `		/* Invalid declaration */` |
|     ! 0 | 5205 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 5206 | `		if( rc == SXERR_ABORT ){` |
|       - | 5207 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5208 | `			return SXERR_ABORT;` |
|       - | 5209 | `		}` |
|     ! 0 | 5210 | `		goto Synchronize;` |
|       - | 5211 | `	}` |
|      32 | 5212 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 5213 | `	/* Allocate a new class attribute */` |
|      32 | 5214 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 5215 | `	if( pCons == 0 ){` |
|     ! 0 | 5216 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5217 | `		return SXERR_ABORT;` |
|       - | 5218 | `	}` |
|       - | 5219 | `	/* Swap bytecode container */` |
|      32 | 5220 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 5221 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 5222 | `	/* Compile constant value.` |
|       - | 5223 | `	 */` |
|      32 | 5224 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 5225 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 5226 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 5227 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5228 | `			return SXERR_ABORT;` |
|       - | 5229 | `		}` |
|       1 | 5230 | `	}` |
|       - | 5231 | `	/* Emit the done instruction */` |
|      32 | 5232 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 5233 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 5234 | `	if( rc == SXERR_ABORT ){` |
|       - | 5235 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 5236 | `		return SXERR_ABORT;` |
|       - | 5237 | `	}` |
|       - | 5238 | `	/* All done,install the constant */` |
|      32 | 5239 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 5240 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5241 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5242 | `		return SXERR_ABORT;` |
|       - | 5243 | `	}` |
|      32 | 5244 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5245 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 5246 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5247 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5248 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5249 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5250 | `				pTok--;` |
|     ! 0 | 5251 | `			}` |
|     ! 0 | 5252 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5253 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 5254 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5255 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5256 | `				return SXERR_ABORT;` |
|       - | 5257 | `			}` |
|     ! 0 | 5258 | `		}else{` |
|     ! 0 | 5259 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 5260 | `				goto loop;` |
|       - | 5261 | `			}` |
|       - | 5262 | `		}` |
|     ! 0 | 5263 | `	}` |
|      32 | 5264 | `	return SXRET_OK;` |
|     ! 0 | 5265 | `Synchronize:` |
|       - | 5266 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 5267 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 5268 | `		pGen->pIn++;` |
|     ! 0 | 5269 | `	}` |
|     ! 0 | 5270 | `	return SXERR_CORRUPT;` |
|      17 | 5271 |  |
|       - | 5272 | `/*` |
|       - | 5273 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 5274 | ` * According to the PHP language reference manual` |
|       - | 5275 | ` *  Properties` |
|       - | 5276 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 5277 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 5278 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 5279 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 5280 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 5281 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 5282 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 5283 | ` * Symisc eXtension.` |
|       - | 5284 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 5285 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 5286 | ` *  Example:` |
|       - | 5287 | ` *   class Test{` |
|       - | 5288 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 5289 | ` *   };` |
|       - | 5290 | ` *   var_dump(TEST::myVar);` |
|       - | 5291 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 5292 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 5293 | ` */` |
|   35116 | 5294 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 5295 |  |
|   35118 | 5296 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5297 | `	ph7_class_attr *pAttr;` |
|       - | 5298 | `	SyString *pName;` |
|       - | 5299 | `	sxi32 rc;` |
|       - | 5300 | `	/* Extract visibility level */` |
|   35118 | 5301 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17558 | 5302 | `loop:` |
|   35118 | 5303 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   35118 | 5304 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 5305 | `		/* Invalid attribute name */` |
|     ! 0 | 5306 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 5307 | `		if( rc == SXERR_ABORT ){` |
|       - | 5308 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5309 | `			return SXERR_ABORT;` |
|       - | 5310 | `		}` |
|     ! 0 | 5311 | `		goto Synchronize;` |
|       - | 5312 | `	}` |
|       - | 5313 | `	/* Peek attribute name */` |
|   35118 | 5314 | `	pName = &pGen->pIn->sData;` |
|       - | 5315 | `	/* Advance the stream cursor */` |
|   35118 | 5316 | `	pGen->pIn++;` |
|   35118 | 5317 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5318 | `		/* Invalid declaration */` |
|       3 | 5319 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5320 | `		if( rc == SXERR_ABORT ){` |
|       - | 5321 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5322 | `			return SXERR_ABORT;` |
|       - | 5323 | `		}` |
|       3 | 5324 | `		goto Synchronize;` |
|       - | 5325 | `	}` |
|       - | 5326 | `	/* Allocate a new class attribute */` |
|   35116 | 5327 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   35116 | 5328 | `	if( pAttr == 0 ){` |
|     ! 0 | 5329 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5330 | `		return SXERR_ABORT;` |
|       - | 5331 | `	}` |
|   35116 | 5332 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5333 | `		SySet *pInstrContainer;` |
|   10906 | 5334 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5335 | `		/* Swap bytecode container */` |
|   10906 | 5336 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10906 | 5337 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5338 | `		/* Compile attribute value.` |
|       - | 5339 | `		 */` |
|   10906 | 5340 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10906 | 5341 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5342 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5344 | `				return SXERR_ABORT;` |
|       - | 5345 | `			}` |
|     ! 0 | 5346 | `		}` |
|       - | 5347 | `		/* Emit the done instruction */` |
|   10906 | 5348 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10906 | 5349 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5452 | 5350 | `	}` |
|       - | 5351 | `	/* All done,install the attribute */` |
|   35116 | 5352 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   35116 | 5353 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5354 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5355 | `		return SXERR_ABORT;` |
|       - | 5356 | `	}` |
|   35116 | 5357 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5358 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5359 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5360 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5361 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5362 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5363 | `				pTok--;` |
|     ! 0 | 5364 | `			}` |
|     ! 0 | 5365 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5366 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5367 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5368 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5369 | `				return SXERR_ABORT;` |
|       - | 5370 | `			}` |
|     ! 0 | 5371 | `		}else{` |
|     ! 0 | 5372 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5373 | `				goto loop;` |
|       - | 5374 | `			}` |
|       - | 5375 | `		}` |
|     ! 0 | 5376 | `	}` |
|   35116 | 5377 | `	return SXRET_OK;` |
|       1 | 5378 | `Synchronize:` |
|       - | 5379 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5380 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5381 | `		pGen->pIn++;` |
|       1 | 5382 | `	}` |
|       3 | 5383 | `	return SXERR_CORRUPT;` |
|   17560 | 5384 |  |
|       - | 5385 | `/*` |
|       - | 5386 | ` * Compile a class method.` |
|       - | 5387 | ` *` |
|       - | 5388 | ` * Refer to the official documentation for more information` |
|       - | 5389 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5390 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5391 | ` * overloading and many more.` |
|       - | 5392 | ` */` |
|  129316 | 5393 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5394 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5395 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5396 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5397 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5398 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5399 | `	)` |
|       2 | 5400 |  |
|  129318 | 5401 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5402 | `	ph7_class_method *pMeth;` |
|       - | 5403 | `	sxi32 iFuncFlags;` |
|       - | 5404 | `	SyString *pName;` |
|       - | 5405 | `	SyToken *pEnd;` |
|       - | 5406 | `	sxi32 rc;` |
|       - | 5407 | `	/* Extract visibility level */` |
|  129318 | 5408 | `	iProtection = GetProtectionLevel(iProtection);` |
|  129318 | 5409 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  129318 | 5410 | `	iFuncFlags = 0;` |
|  129318 | 5411 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5412 | `		/* Invalid method name */` |
|     ! 0 | 5413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5414 | `		if( rc == SXERR_ABORT ){` |
|       - | 5415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5416 | `			return SXERR_ABORT;` |
|       - | 5417 | `		}` |
|     ! 0 | 5418 | `		goto Synchronize;` |
|       - | 5419 | `	}` |
|  129318 | 5420 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5421 | `		/* Return by reference,remember that */` |
|     ! 0 | 5422 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5423 | `		/* Jump the '&' token */` |
|     ! 0 | 5424 | `		pGen->pIn++;` |
|     ! 0 | 5425 | `	}` |
|  129318 | 5426 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5427 | `		/* Invalid method name */` |
|     ! 0 | 5428 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5429 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5430 | `			return SXERR_ABORT;` |
|       - | 5431 | `		}` |
|     ! 0 | 5432 | `		goto Synchronize;` |
|       - | 5433 | `	}` |
|       - | 5434 | `	/* Peek method name */` |
|  129318 | 5435 | `	pName = &pGen->pIn->sData;` |
|  129318 | 5436 | `	nLine = pGen->pIn->nLine;` |
|       - | 5437 | `	/* Jump the method name */` |
|  129318 | 5438 | `	pGen->pIn++;` |
|  129318 | 5439 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5440 | `		/* Abstract method */` |
|   21522 | 5441 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5442 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5443 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5444 | `				&pClass->sName,pName);` |
|     ! 0 | 5445 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5446 | `				return SXERR_ABORT;` |
|       - | 5447 | `			}` |
|     ! 0 | 5448 | `		}` |
|       - | 5449 | `		/* Assemble method signature only */` |
|   21522 | 5450 | `		doBody = FALSE;` |
|   10760 | 5451 | `	}` |
|  129318 | 5452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5453 | `		/* Syntax error */` |
|     ! 0 | 5454 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5455 | `		if( rc == SXERR_ABORT ){` |
|       - | 5456 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5457 | `			return SXERR_ABORT;` |
|       - | 5458 | `		}` |
|     ! 0 | 5459 | `		goto Synchronize;` |
|       - | 5460 | `	}` |
|       - | 5461 | `	/* Allocate a new class_method instance */` |
|  129318 | 5462 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  129318 | 5463 | `	if( pMeth == 0 ){` |
|     ! 0 | 5464 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5465 | `		return SXERR_ABORT;` |
|       - | 5466 | `	}` |
|       - | 5467 | `	/* Jump the left parenthesis '(' */` |
|  129318 | 5468 | `	pGen->pIn++;` |
|  129318 | 5469 | `	pEnd = 0; /* cc warning */` |
|       - | 5470 | `	/* Delimit the method signature */` |
|  129318 | 5471 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  129318 | 5472 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5473 | `		/* Syntax error */` |
|       3 | 5474 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5475 | `		if( rc == SXERR_ABORT ){` |
|       - | 5476 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5477 | `			return SXERR_ABORT;` |
|       - | 5478 | `		}` |
|       3 | 5479 | `		goto Synchronize;` |
|       - | 5480 | `	}` |
|  129316 | 5481 | `	if( pGen->pIn < pEnd ){` |
|       - | 5482 | `		/* Collect method arguments */` |
|   26930 | 5483 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26930 | 5484 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5485 | `			return SXERR_ABORT;` |
|       - | 5486 | `		}` |
|   13464 | 5487 | `	}` |
|       - | 5488 | `	/* Point past ')' and parse optional return type ': type' */` |
|  129316 | 5489 | `	pGen->pIn = &pEnd[1];` |
|  129316 | 5490 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  129316 | 5491 | `	if( doBody ){` |
|       - | 5492 | `		/* Compile method body */` |
|  107796 | 5493 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  107796 | 5494 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5495 | `			return SXERR_ABORT;` |
|       - | 5496 | `		}` |
|   53899 | 5497 | `	}else{` |
|       - | 5498 | `		/* Only method signature is allowed */` |
|   21522 | 5499 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5500 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5501 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5502 | `				if( rc == SXERR_ABORT ){` |
|       - | 5503 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5504 | `					return SXERR_ABORT;` |
|       - | 5505 | `				}` |
|     ! 0 | 5506 | `				return SXERR_CORRUPT;` |
|       - | 5507 | `			}` |
|       - | 5508 | `	}` |
|       - | 5509 | `	/* All done,install the method */` |
|  129316 | 5510 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  129316 | 5511 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5512 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5513 | `		return SXERR_ABORT;` |
|       - | 5514 | `	}` |
|  129316 | 5515 | `	return SXRET_OK;` |
|       1 | 5516 | `Synchronize:` |
|       - | 5517 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5518 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5519 | `		pGen->pIn++;` |
|       1 | 5520 | `	}` |
|       3 | 5521 | `	return SXERR_CORRUPT;` |
|   64660 | 5522 |  |
|       - | 5523 | `/*` |
|       - | 5524 | ` * Compile an object interface.` |
|       - | 5525 | ` *  According to the PHP language reference manual` |
|       - | 5526 | ` *   Object Interfaces:` |
|       - | 5527 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5528 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5529 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5530 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5531 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5532 | ` */` |
|    8090 | 5533 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5534 |  |
|    8092 | 5535 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5536 | `	ph7_class *pClass,*pBase;` |
|       - | 5537 | `	SyToken *pEnd,*pTmp;` |
|       - | 5538 | `	SyString *pName;` |
|       - | 5539 | `	sxi32 nKwrd;` |
|       - | 5540 | `	sxi32 rc;` |
|       - | 5541 | `	/* Jump the 'interface' keyword */` |
|    8092 | 5542 | `	pGen->pIn++;` |
|       - | 5543 | `	/* Extract interface name */` |
|    8092 | 5544 | `	pName = &pGen->pIn->sData;` |
|       - | 5545 | `	/* Advance the stream cursor */` |
|    8092 | 5546 | `	pGen->pIn++;` |
|       - | 5547 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5548 | `		SyBlob sFQN;` |
|       - | 5549 | `		SyString sFQNStr;` |
|    8092 | 5550 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8092 | 5551 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8092 | 5552 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8092 | 5553 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8092 | 5554 | `		SyBlobRelease(&sFQN);` |
|       - | 5555 | `	}` |
|    8092 | 5556 | `	if( pClass == 0 ){` |
|     ! 0 | 5557 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5558 | `		return SXERR_ABORT;` |
|       - | 5559 | `	}` |
|       - | 5560 | `	/* Mark as an interface */` |
|    8092 | 5561 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5562 | `	/* Assume no base class is given */` |
|    8092 | 5563 | `	pBase = 0;` |
|    8092 | 5564 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5565 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5566 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5567 | `			SyString *pBaseName;` |
|       - | 5568 | `			/* Extract base interface */` |
|       3 | 5569 | `			pGen->pIn++;` |
|       3 | 5570 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5571 | `				/* Syntax error */` |
|     ! 0 | 5572 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5573 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5574 | `					pName);` |
|     ! 0 | 5575 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5576 | `				if( rc == SXERR_ABORT ){` |
|       - | 5577 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5578 | `					return SXERR_ABORT;` |
|       - | 5579 | `				}` |
|     ! 0 | 5580 | `				return SXRET_OK;` |
|       - | 5581 | `			}` |
|       3 | 5582 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5583 | `			{` |
|       - | 5584 | `				SyBlob sResolved;` |
|       3 | 5585 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5586 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5587 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5588 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5589 | `				SyBlobRelease(&sResolved);` |
|       - | 5590 | `			}` |
|       - | 5591 | `			/* Only interfaces is allowed */` |
|       3 | 5592 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5593 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5594 | `			}` |
|       3 | 5595 | `			if( pBase == 0 ){` |
|       - | 5596 | `				/* Inexistant interface */` |
|     ! 0 | 5597 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5598 | `				if( rc == SXERR_ABORT ){` |
|       - | 5599 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5600 | `					return SXERR_ABORT;` |
|       - | 5601 | `				}` |
|     ! 0 | 5602 | `			}` |
|       - | 5603 | `			/* Advance the stream cursor */` |
|       3 | 5604 | `			pGen->pIn++;` |
|       1 | 5605 | `		}` |
|       1 | 5606 | `	}` |
|    8092 | 5607 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5608 | `		/* Syntax error */` |
|     ! 0 | 5609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5610 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5611 | `		if( rc == SXERR_ABORT ){` |
|       - | 5612 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5613 | `			return SXERR_ABORT;` |
|       - | 5614 | `		}` |
|     ! 0 | 5615 | `		return SXRET_OK;` |
|       - | 5616 | `	}` |
|    8092 | 5617 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8092 | 5618 | `	pEnd = 0; /* cc warning */` |
|       - | 5619 | `	/* Delimit the interface body */` |
|    8092 | 5620 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8092 | 5621 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5622 | `		/* Syntax error */` |
|     ! 0 | 5623 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5624 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5625 | `		if( rc == SXERR_ABORT ){` |
|       - | 5626 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5627 | `			return SXERR_ABORT;` |
|       - | 5628 | `		}` |
|     ! 0 | 5629 | `		return SXRET_OK;` |
|       - | 5630 | `	}` |
|       - | 5631 | `	/* Swap token stream */` |
|    8092 | 5632 | `	pTmp = pGen->pEnd;` |
|    8092 | 5633 | `	pGen->pEnd = pEnd;` |
|       - | 5634 | `	/* Start the parse process` |
|       - | 5635 | `	 * Note (According to the PHP reference manual):` |
|       - | 5636 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5637 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5638 | `	 */` |
|   14800 | 5639 | `	for(;;){` |
|       - | 5640 | `		/* Jump leading/trailing semi-colons */` |
|   51112 | 5641 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   21512 | 5642 | `			pGen->pIn++;` |
|       2 | 5643 | `		}` |
|   29602 | 5644 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5645 | `			/* End of interface body */` |
|    8090 | 5646 | `			break;` |
|       - | 5647 | `		}` |
|   21514 | 5648 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5649 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5650 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5651 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5652 | `			if( rc == SXERR_ABORT ){` |
|       - | 5653 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5654 | `				return SXERR_ABORT;` |
|       - | 5655 | `			}` |
|     ! 0 | 5656 | `			goto done;` |
|       - | 5657 | `		}` |
|       - | 5658 | `		/* Extract the current keyword */` |
|   21514 | 5659 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21514 | 5660 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5661 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 5662 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 5663 | `			const char *zKind = "member";` |
|       3 | 5664 | `			SyString *pMemberName = 0;` |
|       3 | 5665 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 5666 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 5667 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 5668 | `					zKind = "constant";` |
|       3 | 5669 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 5670 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 5671 | `					}` |
|       1 | 5672 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5673 | `					zKind = "method";` |
|     ! 0 | 5674 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 5675 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 5676 | `					}` |
|     ! 0 | 5677 | `				}` |
|       1 | 5678 | `			}` |
|       3 | 5679 | `			if( pMemberName ){` |
|       4 | 5680 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 5681 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 5682 | `			}else{` |
|     ! 0 | 5683 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5684 | `					"Access type for interface %s must be public",zKind);` |
|       - | 5685 | `			}` |
|       3 | 5686 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5687 | `				return SXERR_ABORT;` |
|       - | 5688 | `			}` |
|       3 | 5689 | `			goto done;` |
|       - | 5690 | `		}` |
|   21512 | 5691 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5692 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5693 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5694 | `			if( rc == SXERR_ABORT ){` |
|       - | 5695 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5696 | `				return SXERR_ABORT;` |
|       - | 5697 | `			}` |
|     ! 0 | 5698 | `			goto done;` |
|       - | 5699 | `		}` |
|   21512 | 5700 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5701 | `			/* Advance the stream cursor */` |
|   21508 | 5702 | `			pGen->pIn++;` |
|   21508 | 5703 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5704 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5705 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5706 | `				if( rc == SXERR_ABORT ){` |
|       - | 5707 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5708 | `					return SXERR_ABORT;` |
|       - | 5709 | `				}` |
|     ! 0 | 5710 | `				goto done;` |
|       - | 5711 | `			}` |
|   21508 | 5712 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21508 | 5713 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5714 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5715 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5716 | `				if( rc == SXERR_ABORT ){` |
|       - | 5717 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5718 | `					return SXERR_ABORT;` |
|       - | 5719 | `				}` |
|     ! 0 | 5720 | `				goto done;` |
|       - | 5721 | `			}` |
|   10753 | 5722 | `		}` |
|   21512 | 5723 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5724 | `			/* Parse constant */` |
|       3 | 5725 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5726 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5727 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5728 | `					return SXERR_ABORT;` |
|       - | 5729 | `				}` |
|     ! 0 | 5730 | `				goto done;` |
|       - | 5731 | `			}` |
|       2 | 5732 | `		}else{` |
|   21510 | 5733 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   21510 | 5734 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5735 | `				/* Static method,record that */` |
|     ! 0 | 5736 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5737 | `				/* Advance the stream cursor */` |
|     ! 0 | 5738 | `				pGen->pIn++;` |
|     ! 0 | 5739 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5740 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5741 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5742 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5743 | `						if( rc == SXERR_ABORT ){` |
|       - | 5744 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5745 | `							return SXERR_ABORT;` |
|       - | 5746 | `						}` |
|     ! 0 | 5747 | `						goto done;` |
|       - | 5748 | `				}` |
|     ! 0 | 5749 | `			}` |
|       - | 5750 | `			/* Process method signature (no body for interface methods) */` |
|   21510 | 5751 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   21510 | 5752 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5753 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5754 | `					return SXERR_ABORT;` |
|       - | 5755 | `				}` |
|     ! 0 | 5756 | `				goto done;` |
|       - | 5757 | `			}` |
|       - | 5758 | `		}` |
|       2 | 5759 | `	}` |
|       - | 5760 | `	/* Install the interface */` |
|    8090 | 5761 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8090 | 5762 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5763 | `		/* Inherit from the base interface */` |
|       3 | 5764 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5765 | `	}` |
|    8090 | 5766 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5767 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5768 | `		return SXERR_ABORT;` |
|       - | 5769 | `	}` |
|    4044 | 5770 | `done:` |
|       - | 5771 | `	/* Point beyond the interface body */` |
|    8092 | 5772 | `	pGen->pIn  = &pEnd[1];` |
|    8092 | 5773 | `	pGen->pEnd = pTmp;` |
|    8092 | 5774 | `	return PH7_OK;` |
|    4047 | 5775 |  |
|       - | 5776 | `/*` |
|       - | 5777 | ` * Compile a user-defined class.` |
|       - | 5778 | ` * According to the PHP language reference manual` |
|       - | 5779 | ` *  class` |
|       - | 5780 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5781 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5782 | ` *  of the properties and methods belonging to the class.` |
|       - | 5783 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5784 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5785 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5786 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5787 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5788 | ` *  (called "methods").` |
|       - | 5789 | ` */` |
|       - | 5790 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5791 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5792 | `struct TraitUseEntry {` |
|       - | 5793 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5794 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5795 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5796 | `};` |
|       - | 5797 | `/*` |
|       - | 5798 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5799 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5800 | ` */` |
|   38054 | 5801 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5802 |  |
|       - | 5803 | `	ph7_class **apIface;` |
|       - | 5804 | `	sxu32 nIface,i;` |
|       - | 5805 | `	sxi32 rc;` |
|   38056 | 5806 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5807 | `		return SXRET_OK;` |
|       - | 5808 | `	}` |
|   38056 | 5809 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   38056 | 5810 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   40778 | 5811 | `	for(i = 0; i < nIface; i++){` |
|    2724 | 5812 | `		ph7_class *pIface = apIface[i];` |
|       - | 5813 | `		SyHashEntry *pEntry;` |
|    2724 | 5814 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16222 | 5815 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13500 | 5816 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5817 | `			ph7_class_method *pImplMeth;` |
|   13500 | 5818 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5819 | `			/* Find the implementing method in the class */` |
|   13500 | 5820 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13500 | 5821 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5822 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5823 | `			}` |
|       - | 5824 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13486 | 5825 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5826 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5827 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5828 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5829 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5830 | `					return SXERR_ABORT;` |
|       - | 5831 | `				}` |
|       1 | 5832 | `			}` |
|       - | 5833 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5834 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5835 | `			 */` |
|       - | 5836 | `			{` |
|   13486 | 5837 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13486 | 5838 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13486 | 5839 | `				int sigError = 0;` |
|   13486 | 5840 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5841 | `					sigError = 1;` |
|   13485 | 5842 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5843 | `					/* Extra parameters must all have default values */` |
|       5 | 5844 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5845 | `					sxu32 k;` |
|       7 | 5846 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5847 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5848 | `							sigError = 1;` |
|       3 | 5849 | `							break;` |
|       - | 5850 | `						}` |
|       2 | 5851 | `					}` |
|       2 | 5852 | `				}` |
|   13486 | 5853 | `				if( sigError ){` |
|       - | 5854 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5855 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5856 | `					sxu32 j;` |
|       5 | 5857 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5858 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5859 | `					/* Build implementing method signature */` |
|       5 | 5860 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5861 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5862 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5863 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5864 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5865 | `					}` |
|       - | 5866 | `					/* Build interface method signature */` |
|       5 | 5867 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5868 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5869 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5870 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5871 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5872 | `					}` |
|       7 | 5873 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5874 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5875 | `						&pClass->sName,pMName,` |
|       4 | 5876 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5877 | `						&pIface->sName,pMName,` |
|       4 | 5878 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5879 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5880 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5881 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5882 | `						return SXERR_ABORT;` |
|       - | 5883 | `					}` |
|       2 | 5884 | `				}` |
|       - | 5885 | `			}` |
|       2 | 5886 | `		}` |
|    1363 | 5887 | `	}` |
|   38056 | 5888 | `	return SXRET_OK;` |
|   19029 | 5889 |  |
|       - | 5890 | `/*` |
|       - | 5891 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5892 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5893 | ` */` |
|   38054 | 5894 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5895 |  |
|       - | 5896 | `	ph7_class_method *pMeth;` |
|       - | 5897 | `	SyHashEntry *pEntry;` |
|       - | 5898 | `	sxu32 nAbstract;` |
|       - | 5899 | `	SyBlob sMsg;` |
|       - | 5900 | `	sxi32 rc;` |
|       - | 5901 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   38056 | 5902 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5903 | `		return SXRET_OK;` |
|       - | 5904 | `	}` |
|       - | 5905 | `	/* Count abstract methods */` |
|   38038 | 5906 | `	nAbstract = 0;` |
|   38038 | 5907 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  360980 | 5908 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  322944 | 5909 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  322944 | 5910 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5911 | `			nAbstract++;` |
|       8 | 5912 | `		}` |
|       2 | 5913 | `	}` |
|   38038 | 5914 | `	if( nAbstract == 0 ){` |
|   38024 | 5915 | `		return SXRET_OK;` |
|       - | 5916 | `	}` |
|       - | 5917 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5918 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5919 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5920 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5921 | `		&pClass->sName,nAbstract,` |
|       7 | 5922 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5923 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5924 | `	/* Second pass: list methods with origins */` |
|       - | 5925 | `	{` |
|      15 | 5926 | `		sxu32 nListed = 0;` |
|      15 | 5927 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5928 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5929 | `			ph7_class *pOrigin = 0;` |
|       - | 5930 | `			SyString *pMName;` |
|      19 | 5931 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5932 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5933 | `				continue;` |
|       - | 5934 | `			}` |
|      17 | 5935 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5936 | `			if( nListed > 0 ){` |
|       3 | 5937 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5938 | `			}` |
|       - | 5939 | `			/* Find the origin of this abstract method.` |
|       - | 5940 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5941 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5942 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5943 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5944 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5945 | `			 * class's namespace.` |
|       - | 5946 | `			 */` |
|       - | 5947 | `			{` |
|       - | 5948 | `				ph7_class **apIface;` |
|       - | 5949 | `				ph7_class **apTrait;` |
|       - | 5950 | `				ph7_class *pWalk;` |
|       - | 5951 | `				sxu32 i;` |
|       - | 5952 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5953 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5954 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5955 | `				 */` |
|      17 | 5956 | `				if( pClass->pBase ){` |
|       9 | 5957 | `					pWalk = pClass->pBase;` |
|      17 | 5958 | `					while( pWalk ){` |
|       - | 5959 | `						ph7_class_method *pParentMeth;` |
|      11 | 5960 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5961 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5962 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5963 | `							 * in this class's ancestor chain.` |
|       - | 5964 | `							 */` |
|      11 | 5965 | `							int fromIface = 0;` |
|      11 | 5966 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5967 | `							while( pAnc ){` |
|       - | 5968 | `								ph7_class **apPI;` |
|       - | 5969 | `								sxu32 j;` |
|      13 | 5970 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5971 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5972 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5973 | `										fromIface = 1;` |
|       9 | 5974 | `										break;` |
|       - | 5975 | `									}` |
|     ! 0 | 5976 | `								}` |
|      13 | 5977 | `								if( fromIface ) break;` |
|       5 | 5978 | `								pAnc = pAnc->pBase;` |
|       1 | 5979 | `							}` |
|      11 | 5980 | `							if( !fromIface ){` |
|       3 | 5981 | `								pOrigin = pWalk;` |
|       3 | 5982 | `								break;` |
|       - | 5983 | `							}` |
|       4 | 5984 | `						}` |
|       9 | 5985 | `						pWalk = pWalk->pBase;` |
|       1 | 5986 | `					}` |
|       4 | 5987 | `				}` |
|       - | 5988 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5989 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5990 | `				 */` |
|      17 | 5991 | `				if( !pOrigin ){` |
|      15 | 5992 | `					pWalk = pClass;` |
|      37 | 5993 | `					while( pWalk && !pOrigin ){` |
|      23 | 5994 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5995 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5996 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5997 | `							ph7_class *pDeepest = 0;` |
|      25 | 5998 | `							while( pIface ){` |
|      13 | 5999 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 6000 | `									pDeepest = pIface;` |
|       6 | 6001 | `								}` |
|      13 | 6002 | `								pIface = pIface->pBase;` |
|       1 | 6003 | `							}` |
|      13 | 6004 | `							if( pDeepest ){` |
|      13 | 6005 | `								pOrigin = pDeepest;` |
|      13 | 6006 | `								break;` |
|       - | 6007 | `							}` |
|     ! 0 | 6008 | `						}` |
|      23 | 6009 | `						pWalk = pWalk->pBase;` |
|       1 | 6010 | `					}` |
|       7 | 6011 | `				}` |
|       - | 6012 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 6013 | `				if( !pOrigin ){` |
|       3 | 6014 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 6015 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 6016 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 6017 | `							pOrigin = pClass;` |
|       3 | 6018 | `							break;` |
|       - | 6019 | `						}` |
|     ! 0 | 6020 | `					}` |
|       1 | 6021 | `				}` |
|       - | 6022 | `			}` |
|      17 | 6023 | `			if( pOrigin ){` |
|      17 | 6024 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 6025 | `			}else{` |
|       - | 6026 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 6027 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 6028 | `			}` |
|      17 | 6029 | `			nListed++;` |
|       1 | 6030 | `		}` |
|       - | 6031 | `	}` |
|      15 | 6032 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 6033 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 6034 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 6035 | `	SyBlobRelease(&sMsg);` |
|      15 | 6036 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 6037 | `		return SXERR_ABORT;` |
|       - | 6038 | `	}` |
|      15 | 6039 | `	return SXRET_OK;` |
|   19029 | 6040 |  |
|   38058 | 6041 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 6042 |  |
|   38060 | 6043 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6044 | `	ph7_class *pClass,*pBase;` |
|       - | 6045 | `	SyToken *pEnd,*pTmp;` |
|       - | 6046 | `	sxi32 iProtection;` |
|       - | 6047 | `	SySet aInterfaces;` |
|       - | 6048 | `	SySet aUseEntries;` |
|       - | 6049 | `	sxi32 iAttrflags;` |
|       - | 6050 | `	SyString *pName;` |
|       - | 6051 | `	sxi32 nKwrd;` |
|       - | 6052 | `	sxi32 rc;` |
|       - | 6053 | `	/* Jump the 'class' keyword */` |
|   38060 | 6054 | `	pGen->pIn++;` |
|   38060 | 6055 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6056 | `		/* Syntax error */` |
|     ! 0 | 6057 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 6058 | `		if( rc == SXERR_ABORT ){` |
|       - | 6059 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6060 | `			return SXERR_ABORT;` |
|       - | 6061 | `		}` |
|       - | 6062 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 6063 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 6064 | `			pGen->pIn++;` |
|     ! 0 | 6065 | `		}` |
|     ! 0 | 6066 | `		return SXRET_OK;` |
|       - | 6067 | `	}` |
|       - | 6068 | `	/* Extract class name */` |
|   38060 | 6069 | `	pName = &pGen->pIn->sData;` |
|       - | 6070 | `	/* Advance the stream cursor */` |
|   38060 | 6071 | `	pGen->pIn++;` |
|       - | 6072 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6073 | `		SyBlob sFQN;` |
|       - | 6074 | `		SyString sFQNStr;` |
|   38060 | 6075 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   38060 | 6076 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   38060 | 6077 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   38060 | 6078 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   38060 | 6079 | `		SyBlobRelease(&sFQN);` |
|       - | 6080 | `	}` |
|   38060 | 6081 | `	if( pClass == 0 ){` |
|     ! 0 | 6082 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6083 | `		return SXERR_ABORT;` |
|       - | 6084 | `	}` |
|       - | 6085 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   38060 | 6086 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   38060 | 6087 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 6088 | `	/* Assume a standalone class */` |
|   38060 | 6089 | `	pBase = 0;` |
|   38060 | 6090 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6091 | `		SyString *pBaseName;` |
|   26990 | 6092 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26990 | 6093 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   24270 | 6094 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   24270 | 6095 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6096 | `				/* Syntax error */` |
|     ! 0 | 6097 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6098 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 6099 | `					pName);` |
|     ! 0 | 6100 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6101 | `				if( rc == SXERR_ABORT ){` |
|       - | 6102 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6103 | `					return SXERR_ABORT;` |
|       - | 6104 | `				}` |
|     ! 0 | 6105 | `				return SXRET_OK;` |
|       - | 6106 | `			}` |
|       - | 6107 | `			/* Extract base class name and resolve through namespace/imports */` |
|   24270 | 6108 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 6109 | `			{` |
|       - | 6110 | `				SyBlob sResolved;` |
|   24270 | 6111 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   24270 | 6112 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   36404 | 6113 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   24268 | 6114 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   24270 | 6115 | `				SyBlobRelease(&sResolved);` |
|       - | 6116 | `			}` |
|       - | 6117 | `			/* Interfaces are not allowed */` |
|   24270 | 6118 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 6119 | `				pBase = pBase->pNextName;` |
|     ! 0 | 6120 | `			}` |
|   24270 | 6121 | `			if( pBase == 0 ){` |
|       - | 6122 | `				/* Inexistant base class */` |
|     ! 0 | 6123 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 6124 | `				if( rc == SXERR_ABORT ){` |
|       - | 6125 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 6126 | `					return SXERR_ABORT;` |
|       - | 6127 | `				}` |
|     ! 0 | 6128 | `			}else{` |
|   24270 | 6129 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 6130 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 6131 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 6132 | `					if( rc == SXERR_ABORT ){` |
|       - | 6133 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6134 | `						return SXERR_ABORT;` |
|       - | 6135 | `					}` |
|     ! 0 | 6136 | `				}` |
|       - | 6137 | `			}` |
|       - | 6138 | `			/* Advance the stream cursor */` |
|   24270 | 6139 | `			pGen->pIn++;` |
|   12134 | 6140 | `		}` |
|   26990 | 6141 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 6142 | `			ph7_class *pInterface;` |
|       - | 6143 | `			SyString *pIntName;` |
|       - | 6144 | `			/* Interface implementation */` |
|    2724 | 6145 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1361 | 6146 | `			for(;;){` |
|    2724 | 6147 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 6148 | `					/* Syntax error */` |
|     ! 0 | 6149 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 6150 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 6151 | `						pName);` |
|     ! 0 | 6152 | `					if( rc == SXERR_ABORT ){` |
|       - | 6153 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6154 | `						return SXERR_ABORT;` |
|       - | 6155 | `					}` |
|     ! 0 | 6156 | `					break;` |
|       - | 6157 | `				}` |
|       - | 6158 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2724 | 6159 | `				pIntName = &pGen->pIn->sData;` |
|       - | 6160 | `				{` |
|       - | 6161 | `					SyBlob sResolved;` |
|    2724 | 6162 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2724 | 6163 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5446 | 6164 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2722 | 6165 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2724 | 6166 | `					SyBlobRelease(&sResolved);` |
|       - | 6167 | `				}` |
|       - | 6168 | `				/* Only interfaces are allowed */` |
|    2724 | 6169 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 6170 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 6171 | `				}` |
|    2724 | 6172 | `				if( pInterface == 0 ){` |
|       - | 6173 | `					/* Inexistant interface */` |
|     ! 0 | 6174 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 6175 | `					if( rc == SXERR_ABORT ){` |
|       - | 6176 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6177 | `						return SXERR_ABORT;` |
|       - | 6178 | `					}` |
|     ! 0 | 6179 | `				}else{` |
|       - | 6180 | `					/* Register interface */` |
|    2724 | 6181 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 6182 | `				}` |
|       - | 6183 | `				/* Advance the stream cursor */` |
|    2724 | 6184 | `				pGen->pIn++;` |
|    2724 | 6185 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1363 | 6186 | `					break;` |
|       - | 6187 | `				}` |
|     ! 0 | 6188 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 6189 | `			}` |
|    1361 | 6190 | `		}` |
|   13494 | 6191 | `	}` |
|   38060 | 6192 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 6193 | `		/* Syntax error */` |
|     ! 0 | 6194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 6195 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6196 | `		if( rc == SXERR_ABORT ){` |
|       - | 6197 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6198 | `			return SXERR_ABORT;` |
|       - | 6199 | `		}` |
|     ! 0 | 6200 | `		return SXRET_OK;` |
|       - | 6201 | `	}` |
|   38060 | 6202 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   38060 | 6203 | `	pEnd = 0; /* cc warning */` |
|       - | 6204 | `	/* Delimit the class body */` |
|   38060 | 6205 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   38060 | 6206 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 6207 | `		/* Syntax error */` |
|     ! 0 | 6208 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 6209 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6210 | `		if( rc == SXERR_ABORT ){` |
|       - | 6211 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 6212 | `			return SXERR_ABORT;` |
|       - | 6213 | `		}` |
|     ! 0 | 6214 | `		return SXRET_OK;` |
|       - | 6215 | `	}` |
|       - | 6216 | `	/* Swap token stream */` |
|   38060 | 6217 | `	pTmp = pGen->pEnd;` |
|   38060 | 6218 | `	pGen->pEnd = pEnd;` |
|       - | 6219 | `	/* Set the inherited flags */` |
|   38060 | 6220 | `	pClass->iFlags = iFlags;` |
|       - | 6221 | `	/* Start the parse process */` |
|   72922 | 6222 | `	for(;;){` |
|       - | 6223 | `		/* Jump leading/trailing semi-colons */` |
|  216152 | 6224 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   35172 | 6225 | `			pGen->pIn++;` |
|       2 | 6226 | `		}` |
|  180982 | 6227 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 6228 | `			/* End of class body */` |
|   38056 | 6229 | `			break;` |
|       - | 6230 | `		}` |
|  142928 | 6231 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6232 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6233 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6234 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6235 | `			if( rc == SXERR_ABORT ){` |
|       - | 6236 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 6237 | `				return SXERR_ABORT;` |
|       - | 6238 | `			}` |
|     ! 0 | 6239 | `			goto done;` |
|       - | 6240 | `		}` |
|       - | 6241 | `		/* Assume public visibility */` |
|  142928 | 6242 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  142928 | 6243 | `		iAttrflags = 0;` |
|  142928 | 6244 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 6245 | `			/* Extract the current keyword */` |
|  142928 | 6246 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  142928 | 6247 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6248 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 6249 | `				TraitUseEntry sUse;` |
|      41 | 6250 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 6251 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 6252 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 6253 | `				for(;;){` |
|       - | 6254 | `					ph7_class *pTrait;` |
|       - | 6255 | `					SyString *pTraitName;` |
|      49 | 6256 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6257 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6258 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 6259 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6260 | `							return SXERR_ABORT;` |
|       - | 6261 | `						}` |
|     ! 0 | 6262 | `						break;` |
|       - | 6263 | `					}` |
|      49 | 6264 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 6265 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 6266 | `						SyBlob sResolved;` |
|      49 | 6267 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 6268 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 6269 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 6270 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 6271 | `						SyBlobRelease(&sResolved);` |
|       - | 6272 | `					}` |
|       - | 6273 | `					/* Only traits are allowed */` |
|      49 | 6274 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6275 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 6276 | `					}` |
|      49 | 6277 | `					if( pTrait == 0 ){` |
|     ! 0 | 6278 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6279 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 6280 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6281 | `							return SXERR_ABORT;` |
|       - | 6282 | `						}` |
|     ! 0 | 6283 | `					}else{` |
|      49 | 6284 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 6285 | `					}` |
|      49 | 6286 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 6287 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 6288 | `						break;` |
|       - | 6289 | `					}` |
|       9 | 6290 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 6291 | `				}` |
|       - | 6292 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 6293 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 6294 | `					SyToken *pBlock;` |
|       9 | 6295 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 6296 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 6297 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 6298 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 6299 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 6300 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 6301 | `					}else{` |
|     ! 0 | 6302 | `						pGen->pIn = pGen->pEnd;` |
|       - | 6303 | `					}` |
|       4 | 6304 | `				}` |
|      41 | 6305 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 6306 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 6307 | `				continue;` |
|       - | 6308 | `			}` |
|  142888 | 6309 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  140094 | 6310 | `				iProtection = nKwrd;` |
|  140094 | 6311 | `				pGen->pIn++; /* Jump the visibility token */` |
|  140094 | 6312 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6313 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6314 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6315 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6316 | `					if( rc == SXERR_ABORT ){` |
|       - | 6317 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6318 | `						return SXERR_ABORT;` |
|       - | 6319 | `					}` |
|     ! 0 | 6320 | `					goto done;` |
|       - | 6321 | `				}` |
|  140094 | 6322 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6323 | `					/* Attribute declaration */` |
|   35096 | 6324 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   35096 | 6325 | `					if( rc != SXRET_OK ){` |
|       3 | 6326 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6327 | `							return SXERR_ABORT;` |
|       - | 6328 | `						}` |
|       3 | 6329 | `						goto done;` |
|       - | 6330 | `					}` |
|   35094 | 6331 | `					continue;` |
|       - | 6332 | `				}` |
|       - | 6333 | `				/* Extract the keyword */` |
|  105000 | 6334 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   52499 | 6335 | `			}` |
|  107794 | 6336 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6337 | `				/* Process constant declaration */` |
|      30 | 6338 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 6339 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6340 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6341 | `						return SXERR_ABORT;` |
|       - | 6342 | `					}` |
|     ! 0 | 6343 | `					goto done;` |
|       - | 6344 | `				}` |
|      16 | 6345 | `			}else{` |
|  107766 | 6346 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6347 | `					/* Static method or attribute,record that */` |
|    2712 | 6348 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2712 | 6349 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2712 | 6350 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6351 | `						/* Extract the keyword */` |
|    2708 | 6352 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2708 | 6353 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6354 | `							iProtection = nKwrd;` |
|     ! 0 | 6355 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6356 | `						}` |
|    1353 | 6357 | `					}` |
|    2712 | 6358 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6359 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6360 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6361 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6362 | `						if( rc == SXERR_ABORT ){` |
|       - | 6363 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6364 | `							return SXERR_ABORT;` |
|       - | 6365 | `						}` |
|     ! 0 | 6366 | `						goto done;` |
|       - | 6367 | `					}` |
|    2712 | 6368 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6369 | `						/* Attribute declaration */` |
|       5 | 6370 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6371 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6372 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6373 | `								return SXERR_ABORT;` |
|       - | 6374 | `							}` |
|     ! 0 | 6375 | `							goto done;` |
|       - | 6376 | `						}` |
|       5 | 6377 | `						continue;` |
|       - | 6378 | `					}` |
|       - | 6379 | `					/* Extract the keyword */` |
|    2708 | 6380 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  106409 | 6381 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6382 | `					/* Abstract method,record that */` |
|      10 | 6383 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6384 | `					/* Mark the whole class as abstract */` |
|      10 | 6385 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6386 | `					/* Advance the stream cursor */` |
|      10 | 6387 | `					pGen->pIn++;` |
|      10 | 6388 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6389 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6390 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6391 | `							iProtection = nKwrd;` |
|       8 | 6392 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6393 | `						}` |
|       4 | 6394 | `					}` |
|      10 | 6395 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6396 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6397 | `							/* Static method */` |
|     ! 0 | 6398 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6399 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6400 | `					}` |
|      10 | 6401 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6402 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6403 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6404 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6405 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6406 | `							if( rc == SXERR_ABORT ){` |
|       - | 6407 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6408 | `								return SXERR_ABORT;` |
|       - | 6409 | `							}` |
|     ! 0 | 6410 | `							goto done;` |
|       - | 6411 | `					}` |
|      10 | 6412 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  105052 | 6413 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6414 | `					/* final method ,record that */` |
|       5 | 6415 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6416 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6417 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6418 | `						/* Extract the keyword */` |
|       5 | 6419 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6420 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6421 | `							iProtection = nKwrd;` |
|       5 | 6422 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6423 | `						}` |
|       2 | 6424 | `					}` |
|       5 | 6425 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6426 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6427 | `							/* Static method */` |
|     ! 0 | 6428 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6429 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6430 | `					}` |
|       5 | 6431 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6432 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6433 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6434 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6435 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6436 | `							if( rc == SXERR_ABORT ){` |
|       - | 6437 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6438 | `								return SXERR_ABORT;` |
|       - | 6439 | `							}` |
|     ! 0 | 6440 | `							goto done;` |
|       - | 6441 | `					}` |
|       5 | 6442 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6443 | `				}` |
|  107762 | 6444 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6445 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6446 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6447 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6448 | `						if( rc == SXERR_ABORT ){` |
|       - | 6449 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6450 | `							return SXERR_ABORT;` |
|       - | 6451 | `						}` |
|     ! 0 | 6452 | `						goto done;` |
|       - | 6453 | `				}` |
|  107762 | 6454 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6455 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6456 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6457 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6458 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6459 | `						if( rc == SXERR_ABORT ){` |
|       - | 6460 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6461 | `							return SXERR_ABORT;` |
|       - | 6462 | `						}` |
|     ! 0 | 6463 | `						goto done;` |
|       - | 6464 | `					}` |
|       - | 6465 | `					/* Attribute declaration */` |
|       7 | 6466 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6467 | `				}else{` |
|       - | 6468 | `					/* Process method declaration */` |
|  107756 | 6469 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6470 | `				}` |
|  107762 | 6471 | `				if( rc != SXRET_OK ){` |
|       3 | 6472 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6473 | `						return SXERR_ABORT;` |
|       - | 6474 | `					}` |
|       3 | 6475 | `					goto done;` |
|       - | 6476 | `				}` |
|       - | 6477 | `			}` |
|   53895 | 6478 | `		}else{` |
|       - | 6479 | `			/* Attribute declaration */` |
|     ! 0 | 6480 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6481 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6482 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6483 | `					return SXERR_ABORT;` |
|       - | 6484 | `				}` |
|     ! 0 | 6485 | `				goto done;` |
|       - | 6486 | `			}` |
|       - | 6487 | `		}` |
|       2 | 6488 | `	}` |
|       - | 6489 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6490 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6491 | `	 */` |
|       - | 6492 | `	{` |
|       - | 6493 | `		TraitUseEntry *apUse;` |
|       - | 6494 | `		sxu32 nU;` |
|   38056 | 6495 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   38096 | 6496 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6497 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6498 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6499 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6500 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6501 | `			sxu32 nT;` |
|      41 | 6502 | `			if( !hasResolution ){` |
|       - | 6503 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6504 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6505 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6506 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6507 | `						break;` |
|       - | 6508 | `					}` |
|      20 | 6509 | `				}` |
|      17 | 6510 | `			}else{` |
|       - | 6511 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6512 | `				 * then use the block to resolve method conflicts.` |
|       - | 6513 | `				 */` |
|       - | 6514 | `				SyToken *pR;` |
|      19 | 6515 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6516 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6517 | `					ph7_class_attr *pAR;` |
|       - | 6518 | `					SyHashEntry *pER;` |
|       - | 6519 | `					SyString *pNR;` |
|      11 | 6520 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6521 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6522 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6523 | `						pNR = &pAR->sName;` |
|     ! 0 | 6524 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6525 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6526 | `						}` |
|     ! 0 | 6527 | `					}` |
|      11 | 6528 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6529 | `				}` |
|       - | 6530 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6531 | `				pR = pUse->pResolvStart;` |
|      21 | 6532 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6533 | `					SyString sTrait,sMethod;` |
|       - | 6534 | `					ph7_class *pSrcTrait;` |
|       - | 6535 | `					ph7_class_method *pMeth;` |
|       - | 6536 | `					sxi32 nRKwrd;` |
|      33 | 6537 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6538 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6539 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6540 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6541 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6542 | `					sMethod = pR->sData;` |
|      13 | 6543 | `					pR++;` |
|      13 | 6544 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6545 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6546 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6547 | `							sTrait = sMethod;` |
|       7 | 6548 | `							pR++;` |
|       7 | 6549 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6550 | `							sMethod = pR->sData;` |
|       7 | 6551 | `							pR++;` |
|       3 | 6552 | `						}` |
|       3 | 6553 | `					}` |
|      13 | 6554 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6555 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6556 | `						continue;` |
|       - | 6557 | `					}` |
|      13 | 6558 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6559 | `					pR++;` |
|      13 | 6560 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6561 | `						pSrcTrait = 0;` |
|       7 | 6562 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6563 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6564 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6565 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6566 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6567 | `								break;` |
|       - | 6568 | `							}` |
|       2 | 6569 | `						}` |
|       5 | 6570 | `						if( pSrcTrait ){` |
|       5 | 6571 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6572 | `							if( pMeth ){` |
|       5 | 6573 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6574 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6575 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6576 | `								}` |
|       2 | 6577 | `							}` |
|       2 | 6578 | `						}` |
|       2 | 6579 | `					}` |
|      29 | 6580 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6581 | `				}` |
|       - | 6582 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6583 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6584 | `					ph7_class_method *pMR;` |
|       - | 6585 | `					SyHashEntry *pER;` |
|       - | 6586 | `					SyString *pNR;` |
|      11 | 6587 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6588 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6589 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6590 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6591 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6592 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6593 | `						}` |
|       1 | 6594 | `					}` |
|       6 | 6595 | `				}` |
|       - | 6596 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6597 | `				pR = pUse->pResolvStart;` |
|      21 | 6598 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6599 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6600 | `					ph7_class *pSrcTrait;` |
|       - | 6601 | `					ph7_class_method *pMeth;` |
|      21 | 6602 | `					int hasQual = 0;` |
|       - | 6603 | `					sxi32 nRKwrd;` |
|      33 | 6604 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6605 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6606 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6607 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6608 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6609 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6610 | `					sMethod = pR->sData;` |
|      13 | 6611 | `					pR++;` |
|      13 | 6612 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6613 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6614 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6615 | `							sTrait = sMethod;` |
|       7 | 6616 | `							hasQual = 1;` |
|       7 | 6617 | `							pR++;` |
|       7 | 6618 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6619 | `							sMethod = pR->sData;` |
|       7 | 6620 | `							pR++;` |
|       3 | 6621 | `						}` |
|       3 | 6622 | `					}` |
|      13 | 6623 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6624 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6625 | `						continue;` |
|       - | 6626 | `					}` |
|      13 | 6627 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6628 | `					pR++;` |
|      13 | 6629 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6630 | `						sxi32 iNewVis = -1;` |
|       9 | 6631 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6632 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6633 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6634 | `								iNewVis = nAK;` |
|       7 | 6635 | `								pR++;` |
|       3 | 6636 | `							}` |
|       3 | 6637 | `						}` |
|       9 | 6638 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6639 | `							sAlias = pR->sData;` |
|       7 | 6640 | `							pR++;` |
|       3 | 6641 | `						}` |
|       9 | 6642 | `						pMeth = 0;` |
|       9 | 6643 | `						if( hasQual ){` |
|       3 | 6644 | `							pSrcTrait = 0;` |
|       5 | 6645 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6646 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6647 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6648 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6649 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6650 | `									break;` |
|       - | 6651 | `								}` |
|       2 | 6652 | `							}` |
|       3 | 6653 | `							if( pSrcTrait ){` |
|       3 | 6654 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6655 | `							}` |
|       2 | 6656 | `						}else{` |
|       7 | 6657 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6658 | `						}` |
|       9 | 6659 | `						if( pMeth ){` |
|       9 | 6660 | `							if( sAlias.nByte > 0 ){` |
|       - | 6661 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6662 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6663 | `								 */` |
|       - | 6664 | `								ph7_class_method *pAlias;` |
|       - | 6665 | `								char *zAliasDup;` |
|       7 | 6666 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6667 | `								if( pAlias ){` |
|       7 | 6668 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6669 | `									if( iNewVis >= 0 ){` |
|       5 | 6670 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6671 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6672 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6673 | `									}` |
|       7 | 6674 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6675 | `									if( zAliasDup ){` |
|       7 | 6676 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6677 | `									}` |
|       4 | 6678 | `								}` |
|       6 | 6679 | `							}else if( iNewVis >= 0 ){` |
|       - | 6680 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6681 | `								ph7_class_method *pCopy;` |
|       3 | 6682 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6683 | `								if( pCopy ){` |
|       3 | 6684 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6685 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6686 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6687 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6688 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6689 | `									/* Replace the method in the class hash */` |
|       3 | 6690 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6691 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6692 | `								}` |
|       1 | 6693 | `							}` |
|       4 | 6694 | `						}` |
|       4 | 6695 | `						SXUNUSED(hasQual);` |
|       4 | 6696 | `					}` |
|      17 | 6697 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6698 | `				}` |
|       - | 6699 | `			}` |
|      41 | 6700 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6701 | `		}` |
|       - | 6702 | `	}` |
|       - | 6703 | `	/* Install the class */` |
|   38056 | 6704 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   38056 | 6705 | `	if( rc == SXRET_OK ){` |
|       - | 6706 | `		ph7_class **apInterface;` |
|       - | 6707 | `		sxu32 n;` |
|   38056 | 6708 | `		if( pBase ){` |
|       - | 6709 | `			/* Inherit from base class and mark as a subclass */` |
|   24270 | 6710 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12134 | 6711 | `		}` |
|   38056 | 6712 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   40778 | 6713 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6714 | `			/* Implements one or more interface */` |
|    2724 | 6715 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2724 | 6716 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6717 | `				break;` |
|       - | 6718 | `			}` |
|    1363 | 6719 | `		}` |
|       - | 6720 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   38056 | 6721 | `		if( rc == SXRET_OK ){` |
|   38056 | 6722 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   38056 | 6723 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6724 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6725 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6726 | `				return SXERR_ABORT;` |
|       - | 6727 | `			}` |
|   19027 | 6728 | `		}` |
|       - | 6729 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   38056 | 6730 | `		if( rc == SXRET_OK ){` |
|   38056 | 6731 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   38056 | 6732 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6733 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6734 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6735 | `				return SXERR_ABORT;` |
|       - | 6736 | `			}` |
|   19027 | 6737 | `		}` |
|   19027 | 6738 | `	}` |
|   38056 | 6739 | `	SySetRelease(&aUseEntries);` |
|   38056 | 6740 | `	SySetRelease(&aInterfaces);` |
|   38056 | 6741 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6742 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6743 | `		return SXERR_ABORT;` |
|       - | 6744 | `	}` |
|   19027 | 6745 | `done:` |
|       - | 6746 | `	/* Point beyond the class body */` |
|   38060 | 6747 | `	pGen->pIn = &pEnd[1];` |
|   38060 | 6748 | `	pGen->pEnd = pTmp;` |
|   38060 | 6749 | `	return PH7_OK;` |
|   19031 | 6750 |  |
|       - | 6751 | `/*` |
|       - | 6752 | ` * Compile a user-defined abstract class.` |
|       - | 6753 | ` *  According to the PHP language reference manual` |
|       - | 6754 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6755 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6756 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6757 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6758 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6759 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6760 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6761 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6762 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6763 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6764 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6765 | ` *   could differ.` |
|       - | 6766 | ` */` |
|      16 | 6767 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6768 |  |
|       - | 6769 | `	sxi32 rc;` |
|      18 | 6770 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6771 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6772 | `	return rc;` |
|       2 | 6773 |  |
|       - | 6774 | `/*` |
|       - | 6775 | ` * Compile a user-defined final class.` |
|       - | 6776 | ` *  According to the PHP language reference manual` |
|       - | 6777 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6778 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6779 | ` *    final then it cannot be extended.` |
|       - | 6780 | ` */` |
|       2 | 6781 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6782 |  |
|       - | 6783 | `	sxi32 rc;` |
|       3 | 6784 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6785 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6786 | `	return rc;` |
|       1 | 6787 |  |
|       - | 6788 | `/*` |
|       - | 6789 | ` * Compile a user-defined trait.` |
|       - | 6790 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6791 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6792 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6793 | ` */` |
|      52 | 6794 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6795 |  |
|      54 | 6796 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6797 | `	ph7_class *pClass;` |
|       - | 6798 | `	SyToken *pEnd,*pTmp;` |
|       - | 6799 | `	sxi32 iProtection;` |
|       - | 6800 | `	sxi32 iAttrflags;` |
|       - | 6801 | `	SyString *pName;` |
|       - | 6802 | `	sxi32 nKwrd;` |
|       - | 6803 | `	sxi32 rc;` |
|       - | 6804 | `	/* Jump the 'trait' keyword */` |
|      54 | 6805 | `	pGen->pIn++;` |
|      54 | 6806 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6807 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6808 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6809 | `			return SXERR_ABORT;` |
|       - | 6810 | `		}` |
|     ! 0 | 6811 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6812 | `			pGen->pIn++;` |
|     ! 0 | 6813 | `		}` |
|     ! 0 | 6814 | `		return SXRET_OK;` |
|       - | 6815 | `	}` |
|       - | 6816 | `	/* Extract trait name */` |
|      54 | 6817 | `	pName = &pGen->pIn->sData;` |
|      54 | 6818 | `	pGen->pIn++;` |
|       - | 6819 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6820 | `		SyBlob sFQN;` |
|       - | 6821 | `		SyString sFQNStr;` |
|      54 | 6822 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6823 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6824 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6825 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6826 | `		SyBlobRelease(&sFQN);` |
|       - | 6827 | `	}` |
|      54 | 6828 | `	if( pClass == 0 ){` |
|     ! 0 | 6829 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6830 | `		return SXERR_ABORT;` |
|       - | 6831 | `	}` |
|       - | 6832 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6833 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6834 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6835 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6836 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6837 | `			return SXERR_ABORT;` |
|       - | 6838 | `		}` |
|     ! 0 | 6839 | `		return SXRET_OK;` |
|       - | 6840 | `	}` |
|      54 | 6841 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6842 | `	pEnd = 0;` |
|      54 | 6843 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6844 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6845 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6846 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6847 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6848 | `			return SXERR_ABORT;` |
|       - | 6849 | `		}` |
|     ! 0 | 6850 | `		return SXRET_OK;` |
|       - | 6851 | `	}` |
|       - | 6852 | `	/* Swap token stream */` |
|      54 | 6853 | `	pTmp = pGen->pEnd;` |
|      54 | 6854 | `	pGen->pEnd = pEnd;` |
|       - | 6855 | `	/* Mark as trait */` |
|      54 | 6856 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6857 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6858 | `	for(;;){` |
|     144 | 6859 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6860 | `			pGen->pIn++;` |
|       1 | 6861 | `		}` |
|     124 | 6862 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6863 | `			break;` |
|       - | 6864 | `		}` |
|      71 | 6865 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6866 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6867 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6868 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6869 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6870 | `				return SXERR_ABORT;` |
|       - | 6871 | `			}` |
|     ! 0 | 6872 | `			goto done;` |
|       - | 6873 | `		}` |
|      71 | 6874 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6875 | `		iAttrflags = 0;` |
|      71 | 6876 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6877 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6878 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6879 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6880 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6881 | `				for(;;){` |
|       - | 6882 | `					ph7_class *pUsedTrait;` |
|       - | 6883 | `					SyString *pUsedName;` |
|       5 | 6884 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6885 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6886 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6887 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6888 | `							return SXERR_ABORT;` |
|       - | 6889 | `						}` |
|     ! 0 | 6890 | `						break;` |
|       - | 6891 | `					}` |
|       5 | 6892 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6893 | `					{` |
|       - | 6894 | `						SyBlob sResolved;` |
|       5 | 6895 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6896 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6897 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6898 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6899 | `						SyBlobRelease(&sResolved);` |
|       - | 6900 | `					}` |
|       5 | 6901 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6902 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6903 | `					}` |
|       5 | 6904 | `					if( pUsedTrait == 0 ){` |
|       4 | 6905 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6906 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6907 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6908 | `							return SXERR_ABORT;` |
|       - | 6909 | `						}` |
|       2 | 6910 | `					}else{` |
|       3 | 6911 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6912 | `					}` |
|       5 | 6913 | `					pGen->pIn++;` |
|       5 | 6914 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6915 | `						break;` |
|       - | 6916 | `					}` |
|     ! 0 | 6917 | `					pGen->pIn++;` |
|     ! 0 | 6918 | `				}` |
|       5 | 6919 | `				continue;` |
|       - | 6920 | `			}` |
|      67 | 6921 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6922 | `				iProtection = nKwrd;` |
|      63 | 6923 | `				pGen->pIn++;` |
|      63 | 6924 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6925 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6926 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6927 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6928 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6929 | `						return SXERR_ABORT;` |
|       - | 6930 | `					}` |
|     ! 0 | 6931 | `					goto done;` |
|       - | 6932 | `				}` |
|      63 | 6933 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6934 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6935 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6936 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6937 | `							return SXERR_ABORT;` |
|       - | 6938 | `						}` |
|     ! 0 | 6939 | `						goto done;` |
|       - | 6940 | `					}` |
|      11 | 6941 | `					continue;` |
|       - | 6942 | `				}` |
|      53 | 6943 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6944 | `			}` |
|      57 | 6945 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6946 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6947 | `					"Traits cannot have constants");` |
|     ! 0 | 6948 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6949 | `					return SXERR_ABORT;` |
|       - | 6950 | `				}` |
|     ! 0 | 6951 | `				goto done;` |
|     ! 0 | 6952 | `			}else{` |
|      57 | 6953 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6954 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6955 | `					pGen->pIn++;` |
|       5 | 6956 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6957 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6958 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6959 | `							iProtection = nKwrd;` |
|     ! 0 | 6960 | `							pGen->pIn++;` |
|     ! 0 | 6961 | `						}` |
|       1 | 6962 | `					}` |
|       5 | 6963 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6964 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6965 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6966 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6967 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6968 | `							return SXERR_ABORT;` |
|       - | 6969 | `						}` |
|     ! 0 | 6970 | `						goto done;` |
|       - | 6971 | `					}` |
|       5 | 6972 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6973 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6974 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6975 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6976 | `								return SXERR_ABORT;` |
|       - | 6977 | `							}` |
|     ! 0 | 6978 | `							goto done;` |
|       - | 6979 | `						}` |
|       3 | 6980 | `						continue;` |
|       - | 6981 | `					}` |
|       3 | 6982 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6983 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6984 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6985 | `					pGen->pIn++;` |
|       5 | 6986 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6987 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6988 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6989 | `							iProtection = nKwrd;` |
|       5 | 6990 | `							pGen->pIn++;` |
|       2 | 6991 | `						}` |
|       2 | 6992 | `					}` |
|       5 | 6993 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6994 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6995 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6996 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6997 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6998 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6999 | `							return SXERR_ABORT;` |
|       - | 7000 | `						}` |
|     ! 0 | 7001 | `						goto done;` |
|       - | 7002 | `					}` |
|       5 | 7003 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 7004 | `				}` |
|      55 | 7005 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 7006 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7007 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 7008 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 7009 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7010 | `						return SXERR_ABORT;` |
|       - | 7011 | `					}` |
|     ! 0 | 7012 | `					goto done;` |
|       - | 7013 | `				}` |
|      55 | 7014 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 7015 | `					pGen->pIn++;` |
|     ! 0 | 7016 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 7017 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 7018 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 7019 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 7020 | `							return SXERR_ABORT;` |
|       - | 7021 | `						}` |
|     ! 0 | 7022 | `						goto done;` |
|       - | 7023 | `					}` |
|     ! 0 | 7024 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7025 | `				}else{` |
|      55 | 7026 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 7027 | `				}` |
|      55 | 7028 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7029 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7030 | `						return SXERR_ABORT;` |
|       - | 7031 | `					}` |
|     ! 0 | 7032 | `					goto done;` |
|       - | 7033 | `				}` |
|       - | 7034 | `			}` |
|      28 | 7035 | `		}else{` |
|     ! 0 | 7036 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 7037 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7038 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7039 | `					return SXERR_ABORT;` |
|       - | 7040 | `				}` |
|     ! 0 | 7041 | `				goto done;` |
|       - | 7042 | `			}` |
|       - | 7043 | `		}` |
|       1 | 7044 | `	}` |
|       - | 7045 | `	/* Install the trait */` |
|      54 | 7046 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 7047 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7048 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7049 | `		return SXERR_ABORT;` |
|       - | 7050 | `	}` |
|      26 | 7051 | `done:` |
|       - | 7052 | `	/* Point beyond the trait body */` |
|      54 | 7053 | `	pGen->pIn = &pEnd[1];` |
|      54 | 7054 | `	pGen->pEnd = pTmp;` |
|      54 | 7055 | `	return PH7_OK;` |
|      28 | 7056 |  |
|       - | 7057 | `/*` |
|       - | 7058 | ` * Compile a user-defined class.` |
|       - | 7059 | ` *  According to the PHP language reference manual` |
|       - | 7060 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 7061 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 7062 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 7063 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 7064 | ` *   and functions (called "methods").` |
|       - | 7065 | ` */` |
|   38040 | 7066 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 7067 |  |
|       - | 7068 | `	sxi32 rc;` |
|   38042 | 7069 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   38042 | 7070 | `	return rc;` |
|       2 | 7071 |  |
|       - | 7072 | `/*` |
|       - | 7073 | ` * Exception handling.` |
|       - | 7074 | ` *  According to the PHP language reference manual` |
|       - | 7075 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 7076 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 7077 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 7078 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 7079 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 7080 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 7081 | ` *    (or re-thrown) within a catch block.` |
|       - | 7082 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 7083 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 7084 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 7085 | ` *    been defined with set_exception_handler().` |
|       - | 7086 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 7087 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 7088 | ` */` |
|       - | 7089 | `/*` |
|       - | 7090 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 7091 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 7092 | ` * indicates failure.` |
|       - | 7093 | ` */` |
|    8112 | 7094 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 7095 |  |
|    8114 | 7096 | `	sxi32 rc = SXRET_OK;` |
|    8114 | 7097 | `	if( pRoot->pOp ){` |
|    8108 | 7098 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4056 | 7099 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 7100 | `			/* Unexpected expression */` |
|     ! 0 | 7101 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7102 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 7103 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 7104 | `				rc = SXERR_INVALID;` |
|     ! 0 | 7105 | `			}` |
|       2 | 7106 | `		}` |
|    4059 | 7107 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 7108 | `		/* Unexpected expression */` |
|     ! 0 | 7109 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 7110 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 7111 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 7112 | `			rc = SXERR_INVALID;` |
|     ! 0 | 7113 | `		}` |
|     ! 0 | 7114 | `	}` |
|    8114 | 7115 | `	return rc;` |
|       2 | 7116 |  |
|       - | 7117 | `/*` |
|       - | 7118 | ` * Compile a 'throw' statement.` |
|       - | 7119 | ` * throw: This is how you trigger an exception.` |
|       - | 7120 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 7121 | ` */` |
|    8112 | 7122 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 7123 |  |
|    8114 | 7124 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7125 | `	GenBlock *pBlock;` |
|       - | 7126 | `	sxu32 nIdx;` |
|       - | 7127 | `	sxi32 rc;` |
|    8114 | 7128 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 7129 | `	/* Compile the expression */` |
|    8114 | 7130 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8114 | 7131 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 7132 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 7133 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7134 | `			return SXERR_ABORT;` |
|       - | 7135 | `		}` |
|     ! 0 | 7136 | `		return SXRET_OK;` |
|       - | 7137 | `	}` |
|    8114 | 7138 | `	pBlock = pGen->pCurrent;` |
|       - | 7139 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   37698 | 7140 | `	while(pBlock->pParent){` |
|   37694 | 7141 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8110 | 7142 | `			break;` |
|       - | 7143 | `		}` |
|       - | 7144 | `		/* Point to the parent block */` |
|   29586 | 7145 | `		pBlock = pBlock->pParent;` |
|       2 | 7146 | `	}` |
|       - | 7147 | `	/* Emit the throw instruction */` |
|    8114 | 7148 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 7149 | `	/* Emit the jump */` |
|    8114 | 7150 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8114 | 7151 | `	return SXRET_OK;` |
|    4058 | 7152 |  |
|       - | 7153 | `/*` |
|       - | 7154 | ` * Compile a 'catch' block.` |
|       - | 7155 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 7156 | ` * an object containing the exception information.` |
|       - | 7157 | ` */` |
|      98 | 7158 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 7159 |  |
|     100 | 7160 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7161 | `	ph7_exception_block sCatch;` |
|       - | 7162 | `	SySet *pInstrContainer;` |
|       - | 7163 | `	SyString sClassName;` |
|       - | 7164 | `	GenBlock *pCatch;` |
|       - | 7165 | `	SyToken *pToken;` |
|       - | 7166 | `	SyString *pName;` |
|       - | 7167 | `	char *zDup;` |
|       - | 7168 | `	sxi32 rc;` |
|     100 | 7169 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 7170 | `	/* Zero the structure */` |
|     100 | 7171 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 7172 | `	/* Initialize fields */` |
|     100 | 7173 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     100 | 7174 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     100 | 7175 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 7176 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7177 | `			pToken = pGen->pIn;` |
|     ! 0 | 7178 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7179 | `				pToken--;` |
|     ! 0 | 7180 | `			}` |
|     ! 0 | 7181 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7182 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7183 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7184 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7185 | `				return SXERR_ABORT;` |
|       - | 7186 | `			}` |
|     ! 0 | 7187 | `			return SXERR_INVALID;` |
|       - | 7188 | `	}` |
|       - | 7189 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     100 | 7190 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      61 | 7191 | `	for(;;){` |
|     124 | 7192 | `		int isAbsolute = 0;` |
|       - | 7193 | `		SyBlob sName;` |
|     124 | 7194 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 7195 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     124 | 7196 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 7197 | `			isAbsolute = 1;` |
|       7 | 7198 | `			pGen->pIn++;` |
|       3 | 7199 | `		}` |
|     124 | 7200 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     122 | 7201 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 7202 | `			SyBlobRelease(&sName);` |
|       5 | 7203 | `			pToken = pGen->pIn;` |
|       5 | 7204 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7205 | `				pToken--;` |
|     ! 0 | 7206 | `			}` |
|       7 | 7207 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7208 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 7209 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 7210 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7211 | `				return SXERR_ABORT;` |
|       - | 7212 | `			}` |
|       5 | 7213 | `			return SXERR_INVALID;` |
|       - | 7214 | `		}` |
|       - | 7215 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     120 | 7216 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     120 | 7217 | `		pGen->pIn++;` |
|     183 | 7218 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      67 | 7219 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 7220 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 7221 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 7222 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 7223 | `			pGen->pIn++;` |
|       1 | 7224 | `		}` |
|       - | 7225 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     120 | 7226 | `		if( !isAbsolute ){` |
|       - | 7227 | `			SyString sRaw;` |
|       - | 7228 | `			SyBlob sResolved;` |
|     114 | 7229 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     114 | 7230 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     114 | 7231 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     170 | 7232 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     112 | 7233 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     114 | 7234 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     114 | 7235 | `			SyBlobRelease(&sResolved);` |
|      58 | 7236 | `		}else{` |
|       - | 7237 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 7238 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 7239 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 7240 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 7241 | `		}` |
|     120 | 7242 | `		SyBlobRelease(&sName);` |
|     120 | 7243 | `		if( zDup == 0 ){` |
|     ! 0 | 7244 | `			goto Mem;` |
|       - | 7245 | `		}` |
|     120 | 7246 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     120 | 7247 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7248 | `			goto Mem;` |
|       - | 7249 | `		}` |
|       - | 7250 | `		/* Check for '\|' (multi-catch separator) */` |
|     130 | 7251 | `		if( pGen->pIn < pGen->pEnd &&` |
|     118 | 7252 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 7253 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 7254 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 7255 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 7256 | `			continue;` |
|       - | 7257 | `		}` |
|      96 | 7258 | `		break;` |
|     ! 0 | 7259 | `	}` |
|     141 | 7260 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      96 | 7261 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 7262 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 7263 | `			pToken = pGen->pIn;` |
|     ! 0 | 7264 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7265 | `				pToken--;` |
|     ! 0 | 7266 | `			}` |
|     ! 0 | 7267 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7268 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7269 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7270 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7271 | `				return SXERR_ABORT;` |
|       - | 7272 | `			}` |
|     ! 0 | 7273 | `			return SXERR_INVALID;` |
|       - | 7274 | `	}` |
|      96 | 7275 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 7276 | `	/* Duplicate instance name */` |
|      96 | 7277 | `	pName = &pGen->pIn->sData;` |
|      96 | 7278 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      96 | 7279 | `	if( zDup == 0 ){` |
|     ! 0 | 7280 | `		goto Mem;` |
|       - | 7281 | `	}` |
|      96 | 7282 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      96 | 7283 | `	pGen->pIn++;` |
|      96 | 7284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 7285 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 7286 | `		pToken = pGen->pIn;` |
|     ! 0 | 7287 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 7288 | `			pToken--;` |
|     ! 0 | 7289 | `		}` |
|     ! 0 | 7290 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 7291 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 7292 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 7293 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7294 | `			return SXERR_ABORT;` |
|       - | 7295 | `		}` |
|     ! 0 | 7296 | `		return SXERR_INVALID;` |
|       - | 7297 | `	}` |
|       - | 7298 | `	/* Compile the block */` |
|      96 | 7299 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 7300 | `	/* Create the catch block */` |
|      96 | 7301 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      96 | 7302 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7303 | `		return SXERR_ABORT;` |
|       - | 7304 | `	}` |
|       - | 7305 | `	/* Swap bytecode container */` |
|      96 | 7306 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      96 | 7307 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 7308 | `	/* Compile the block */` |
|      96 | 7309 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 7310 | `	/* Fix forward jumps now the destination is resolved  */` |
|      96 | 7311 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7312 | `	/* Emit the DONE instruction */` |
|      96 | 7313 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7314 | `	/* Leave the block */` |
|      96 | 7315 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7316 | `	/* Restore the default container */` |
|      96 | 7317 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7318 | `	/* Install the catch block */` |
|      96 | 7319 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      96 | 7320 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7321 | `		goto Mem;` |
|       - | 7322 | `	}` |
|      96 | 7323 | `	return SXRET_OK;` |
|     ! 0 | 7324 | `Mem:` |
|     ! 0 | 7325 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7326 | `	return SXERR_ABORT;` |
|      51 | 7327 |  |
|       - | 7328 | `/*` |
|       - | 7329 | ` * Compile a 'try' block.` |
|       - | 7330 | ` * A function using an exception should be in a "try" block.` |
|       - | 7331 | ` * If the exception does not trigger, the code will continue` |
|       - | 7332 | ` * as normal. However if the exception triggers, an exception` |
|       - | 7333 | ` * is "thrown".` |
|       - | 7334 | ` */` |
|     106 | 7335 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 7336 |  |
|       - | 7337 | `	ph7_exception *pException;` |
|     108 | 7338 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7339 | `	GenBlock *pTry;` |
|       - | 7340 | `	sxu32 nJmpIdx;` |
|       - | 7341 | `	sxi32 rc;` |
|       - | 7342 | `	/* Create the exception container */` |
|     108 | 7343 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     108 | 7344 | `	if( pException == 0 ){` |
|     ! 0 | 7345 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 7346 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7347 | `		return SXERR_ABORT;` |
|       - | 7348 | `	}` |
|       - | 7349 | `	/* Zero the structure */` |
|     108 | 7350 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 7351 | `	/* Initialize fields */` |
|     108 | 7352 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     108 | 7353 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     108 | 7354 | `	pException->iHasFinally = 0;` |
|     108 | 7355 | `	pException->iFinallyDone = 0;` |
|     108 | 7356 | `	pException->pVm = pGen->pVm;` |
|       - | 7357 | `	/* Create the try block */` |
|     108 | 7358 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     108 | 7359 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7360 | `		return SXERR_ABORT;` |
|       - | 7361 | `	}` |
|       - | 7362 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     108 | 7363 | `	pTry->pUserData = pException;` |
|       - | 7364 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     108 | 7365 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 7366 | `	/* Fix the jump later when the destination is resolved */` |
|     108 | 7367 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     108 | 7368 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 7369 | `	/* Compile the block */` |
|     108 | 7370 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     108 | 7371 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7372 | `		return SXERR_ABORT;` |
|       - | 7373 | `	}` |
|       - | 7374 | `	/* Fix forward jumps now the destination is resolved */` |
|     108 | 7375 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7376 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     108 | 7377 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 7378 | `	/* Leave the block */` |
|     108 | 7379 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7380 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     108 | 7381 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     104 | 7382 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 7383 | `		/* Compile one or more catch blocks */` |
|      96 | 7384 | `		for(;;){` |
|     192 | 7385 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     154 | 7386 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      49 | 7387 | `					break;` |
|       - | 7388 | `			}` |
|     100 | 7389 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     100 | 7390 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7391 | `				return SXERR_ABORT;` |
|       - | 7392 | `			}` |
|       2 | 7393 | `		}` |
|      47 | 7394 | `	}` |
|       - | 7395 | `	/* Compile optional finally block */` |
|     108 | 7396 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      56 | 7397 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 7398 | `		SySet *pInstrContainer;` |
|       - | 7399 | `		GenBlock *pFinBlock;` |
|      32 | 7400 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 7401 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 7402 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 7403 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7404 | `			return SXERR_ABORT;` |
|       - | 7405 | `		}` |
|       - | 7406 | `		/* Swap bytecode container */` |
|      32 | 7407 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 7408 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7409 | `		/* Compile the finally body */` |
|      32 | 7410 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 7411 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7412 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7413 | `			return SXERR_ABORT;` |
|       - | 7414 | `		}` |
|       - | 7415 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 7416 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7417 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 7418 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7419 | `		/* Leave the block */` |
|      32 | 7420 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7421 | `		/* Restore the default container */` |
|      32 | 7422 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 7423 | `		pException->iHasFinally = 1;` |
|      15 | 7424 | `	}` |
|       - | 7425 | `	/* Must have at least one catch or finally */` |
|     108 | 7426 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 7427 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7428 | `			"Cannot use try without catch or finally");` |
|       7 | 7429 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7430 | `			return SXERR_ABORT;` |
|       - | 7431 | `		}` |
|       3 | 7432 | `	}` |
|     108 | 7433 | `	return SXRET_OK;` |
|      55 | 7434 |  |
|       - | 7435 | `/*` |
|       - | 7436 | ` * Compile a switch block.` |
|       - | 7437 | ` *  (See block-comment below for more information)` |
|       - | 7438 | ` */` |
|     108 | 7439 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7440 |  |
|     110 | 7441 | `	sxi32 rc = SXRET_OK;` |
|     110 | 7442 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7443 | `		/* Unexpected token */` |
|     ! 0 | 7444 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7445 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7446 | `			return SXERR_ABORT;` |
|       - | 7447 | `		}` |
|     ! 0 | 7448 | `		pGen->pIn++;` |
|     ! 0 | 7449 | `	}` |
|     110 | 7450 | `	pGen->pIn++;` |
|       - | 7451 | `	/* First instruction to execute in this block. */` |
|     110 | 7452 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 7453 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 7454 | `	 * or the '}' token */` |
|     182 | 7455 | `	for(;;){` |
|     366 | 7456 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7457 | `			/* No more input to process */` |
|     ! 0 | 7458 | `			break;` |
|       - | 7459 | `		}` |
|     366 | 7460 | `		rc = SXRET_OK;` |
|     366 | 7461 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 | 7462 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 | 7463 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 7464 | `					/* Unexpected token */` |
|     ! 0 | 7465 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7466 | `						&pGen->pIn->sData);` |
|     ! 0 | 7467 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7468 | `						return SXERR_ABORT;` |
|       - | 7469 | `					}` |
|       - | 7470 | `					/* FALL THROUGH */` |
|     ! 0 | 7471 | `				}` |
|      28 | 7472 | `				rc = SXERR_EOF;` |
|      28 | 7473 | `				break;` |
|       - | 7474 | `			}` |
|      23 | 7475 | `		}else{` |
|       - | 7476 | `			sxi32 nKwrd;` |
|       - | 7477 | `			/* Extract the keyword */` |
|     298 | 7478 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 | 7479 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 | 7480 | `				break;` |
|       - | 7481 | `			}` |
|     218 | 7482 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7483 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 7484 | `					/* Unexpected token */` |
|     ! 0 | 7485 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7486 | `						&pGen->pIn->sData);` |
|     ! 0 | 7487 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7488 | `						return SXERR_ABORT;` |
|       - | 7489 | `					}` |
|       - | 7490 | `					/* FALL THROUGH */` |
|     ! 0 | 7491 | `				}` |
|       - | 7492 | `				/* Block compiled */` |
|       3 | 7493 | `				break;` |
|       - | 7494 | `			}` |
|       - | 7495 | `		}` |
|       - | 7496 | `		/* Compile block */` |
|     258 | 7497 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 | 7498 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7499 | `			return SXERR_ABORT;` |
|       - | 7500 | `		}` |
|       2 | 7501 | `	}` |
|     110 | 7502 | `	return rc;` |
|      56 | 7503 |  |
|       - | 7504 | `/*` |
|       - | 7505 | ` * Compile a case eXpression.` |
|       - | 7506 | ` *  (See block-comment below for more information)` |
|       - | 7507 | ` */` |
|      88 | 7508 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7509 |  |
|       - | 7510 | `	SySet *pInstrContainer;` |
|       - | 7511 | `	SyToken *pEnd,*pTmp;` |
|      90 | 7512 | `	sxi32 iNest = 0;` |
|       - | 7513 | `	sxi32 rc;` |
|       - | 7514 | `	/* Delimit the expression */` |
|      90 | 7515 | `	pEnd = pGen->pIn;` |
|     186 | 7516 | `	while( pEnd < pGen->pEnd ){` |
|     186 | 7517 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7518 | `			/* Increment nesting level */` |
|       3 | 7519 | `			iNest++;` |
|     185 | 7520 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7521 | `			/* Decrement nesting level */` |
|       3 | 7522 | `			iNest--;` |
|     183 | 7523 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 | 7524 | `			break;` |
|       - | 7525 | `		}` |
|      98 | 7526 | `		pEnd++;` |
|       2 | 7527 | `	}` |
|      90 | 7528 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7529 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7530 | `		if( rc == SXERR_ABORT ){` |
|       - | 7531 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7532 | `			return SXERR_ABORT;` |
|       - | 7533 | `		}` |
|     ! 0 | 7534 | `	}` |
|       - | 7535 | `	/* Swap token stream */` |
|      90 | 7536 | `	pTmp = pGen->pEnd;` |
|      90 | 7537 | `	pGen->pEnd = pEnd;` |
|      90 | 7538 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 | 7539 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 | 7540 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7541 | `	/* Emit the done instruction */` |
|      90 | 7542 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 | 7543 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7544 | `	/* Update token stream */` |
|      90 | 7545 | `	pGen->pIn  = pEnd;` |
|      90 | 7546 | `	pGen->pEnd = pTmp;` |
|      90 | 7547 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7548 | `		return SXERR_ABORT;` |
|       - | 7549 | `	}` |
|      90 | 7550 | `	return SXRET_OK;` |
|      46 | 7551 |  |
|       - | 7552 | `/*` |
|       - | 7553 | ` * Compile the smart switch statement.` |
|       - | 7554 | ` * According to the PHP language reference manual` |
|       - | 7555 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7556 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7557 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7558 | ` *  This is exactly what the switch statement is for.` |
|       - | 7559 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7560 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7561 | ` *  of the outer loop, use continue 2.` |
|       - | 7562 | ` *  Note that switch/case does loose comparision.` |
|       - | 7563 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7564 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7565 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7566 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7567 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7568 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7569 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7570 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7571 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7572 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7573 | ` *  list for the next case.` |
|       - | 7574 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7575 | ` *  or floating-point numbers and strings.` |
|       - | 7576 | ` */` |
|      28 | 7577 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7578 |  |
|       - | 7579 | `	GenBlock *pSwitchBlock;` |
|       - | 7580 | `	SyToken *pTmp,*pEnd;` |
|       - | 7581 | `	ph7_switch *pSwitch;` |
|       - | 7582 | `	sxu32 nToken;` |
|       - | 7583 | `	sxu32 nLine;` |
|       - | 7584 | `	sxi32 rc;` |
|      30 | 7585 | `	nLine = pGen->pIn->nLine;` |
|       - | 7586 | `	/* Jump the 'switch' keyword */` |
|      30 | 7587 | `	pGen->pIn++;` |
|      30 | 7588 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7589 | `		/* Syntax error */` |
|     ! 0 | 7590 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7591 | `		if( rc == SXERR_ABORT ){` |
|       - | 7592 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7593 | `			return SXERR_ABORT;` |
|       - | 7594 | `		}` |
|     ! 0 | 7595 | `		goto Synchronize;` |
|       - | 7596 | `	}` |
|       - | 7597 | `	/* Jump the left parenthesis '(' */` |
|      30 | 7598 | `	pGen->pIn++;` |
|      30 | 7599 | `	pEnd = 0; /* cc warning */` |
|       - | 7600 | `	/* Create the loop block */` |
|      44 | 7601 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 7602 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 | 7603 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7604 | `		return SXERR_ABORT;` |
|       - | 7605 | `	}` |
|       - | 7606 | `	/* Delimit the condition */` |
|      30 | 7607 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 | 7608 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7609 | `		/* Empty expression */` |
|     ! 0 | 7610 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7611 | `		if( rc == SXERR_ABORT ){` |
|       - | 7612 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7613 | `			return SXERR_ABORT;` |
|       - | 7614 | `		}` |
|     ! 0 | 7615 | `	}` |
|       - | 7616 | `	/* Swap token streams */` |
|      30 | 7617 | `	pTmp = pGen->pEnd;` |
|      30 | 7618 | `	pGen->pEnd = pEnd;` |
|       - | 7619 | `	/* Compile the expression */` |
|      30 | 7620 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 7621 | `	if( rc == SXERR_ABORT ){` |
|       - | 7622 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7623 | `		return SXERR_ABORT;` |
|       - | 7624 | `	}` |
|       - | 7625 | `	/* Update token stream */` |
|      30 | 7626 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7627 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7628 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7629 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7630 | `			return SXERR_ABORT;` |
|       - | 7631 | `		}` |
|     ! 0 | 7632 | `		pGen->pIn++;` |
|     ! 0 | 7633 | `	}` |
|      30 | 7634 | `	pGen->pIn  = &pEnd[1];` |
|      30 | 7635 | `	pGen->pEnd = pTmp;` |
|      30 | 7636 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 7637 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7638 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7639 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7640 | `				pTmp--;` |
|     ! 0 | 7641 | `			}` |
|       - | 7642 | `			/* Unexpected token */` |
|     ! 0 | 7643 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7644 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7645 | `				return SXERR_ABORT;` |
|       - | 7646 | `			}` |
|     ! 0 | 7647 | `			goto Synchronize;` |
|       - | 7648 | `	}` |
|       - | 7649 | `	/* Set the delimiter token */` |
|      30 | 7650 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7651 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7652 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7653 | `	}else{` |
|      28 | 7654 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7655 | `	}` |
|      30 | 7656 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7657 | `	/* Create the switch blocks container */` |
|      30 | 7658 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 | 7659 | `	if( pSwitch == 0 ){` |
|       - | 7660 | `		/* Abort compilation */` |
|     ! 0 | 7661 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7662 | `		return SXERR_ABORT;` |
|       - | 7663 | `	}` |
|       - | 7664 | `	/* Zero the structure */` |
|      30 | 7665 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7666 | `	/* Initialize fields */` |
|      30 | 7667 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7668 | `	/* Emit the switch instruction */` |
|      30 | 7669 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7670 | `	/* Compile case blocks */` |
|      96 | 7671 | `	for(;;){` |
|       - | 7672 | `		sxu32 nKwrd;` |
|     112 | 7673 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7674 | `			/* No more input to process */` |
|     ! 0 | 7675 | `			break;` |
|       - | 7676 | `		}` |
|     112 | 7677 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7678 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7679 | `				/* Unexpected token */` |
|     ! 0 | 7680 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7681 | `					&pGen->pIn->sData);` |
|     ! 0 | 7682 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7683 | `					return SXERR_ABORT;` |
|       - | 7684 | `				}` |
|       - | 7685 | `				/* FALL THROUGH */` |
|     ! 0 | 7686 | `			}` |
|       - | 7687 | `			/* Block compiled */` |
|     ! 0 | 7688 | `			break;` |
|       - | 7689 | `		}` |
|       - | 7690 | `		/* Extract the keyword */` |
|     112 | 7691 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 | 7692 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7693 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7694 | `				/* Unexpected token */` |
|     ! 0 | 7695 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7696 | `					&pGen->pIn->sData);` |
|     ! 0 | 7697 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7698 | `					return SXERR_ABORT;` |
|       - | 7699 | `				}` |
|       - | 7700 | `				/* FALL THROUGH */` |
|     ! 0 | 7701 | `			}` |
|       - | 7702 | `			/* Block compiled */` |
|       3 | 7703 | `			break;` |
|       - | 7704 | `		}` |
|     110 | 7705 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7706 | `			/*` |
|       - | 7707 | `			 * Accroding to the PHP language reference manual` |
|       - | 7708 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7709 | `			 *  that wasn't matched by the other cases.` |
|       - | 7710 | `			 */` |
|      22 | 7711 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7712 | `				/* Default case already compiled */` |
|     ! 0 | 7713 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7714 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7715 | `					return SXERR_ABORT;` |
|       - | 7716 | `				}` |
|     ! 0 | 7717 | `			}` |
|      22 | 7718 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7719 | `			/* Compile the default block */` |
|      22 | 7720 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 | 7721 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7722 | `				return SXERR_ABORT;` |
|      22 | 7723 | `			}else if( rc == SXERR_EOF ){` |
|      20 | 7724 | `				break;` |
|       1 | 7725 | `			}` |
|      91 | 7726 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7727 | `			ph7_case_expr sCase;` |
|       - | 7728 | `			/* Standard case block */` |
|      90 | 7729 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7730 | `			/* initialize the structure */` |
|      90 | 7731 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7732 | `			/* Compile the case expression */` |
|      90 | 7733 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 | 7734 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7735 | `				return SXERR_ABORT;` |
|       - | 7736 | `			}` |
|       - | 7737 | `			/* Compile the case block */` |
|      90 | 7738 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7739 | `			/* Insert in the switch container */` |
|      90 | 7740 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 | 7741 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7742 | `				return SXERR_ABORT;` |
|      90 | 7743 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7744 | `				break;` |
|       - | 7745 | `			}` |
|      42 | 7746 | `		}else{` |
|       - | 7747 | `			/* Unexpected token */` |
|     ! 0 | 7748 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7749 | `				&pGen->pIn->sData);` |
|     ! 0 | 7750 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7751 | `				return SXERR_ABORT;` |
|       - | 7752 | `			}` |
|     ! 0 | 7753 | `			break;` |
|       - | 7754 | `		}` |
|       2 | 7755 | `	}` |
|       - | 7756 | `	/* Fix all jumps now the destination is resolved */` |
|      30 | 7757 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 | 7758 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7759 | `	/* Release the loop block */` |
|      30 | 7760 | `	GenStateLeaveBlock(pGen,0);` |
|      30 | 7761 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7762 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 | 7763 | `		pGen->pIn++;` |
|      14 | 7764 | `	}` |
|       - | 7765 | `	/* Statement successfully compiled */` |
|      30 | 7766 | `	return SXRET_OK;` |
|     ! 0 | 7767 | `Synchronize:` |
|       - | 7768 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7769 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7770 | `		pGen->pIn++;` |
|     ! 0 | 7771 | `	}` |
|     ! 0 | 7772 | `	return SXRET_OK;` |
|      16 | 7773 |  |
|       - | 7774 | `/*` |
|       - | 7775 | ` * Generate bytecode for a given expression tree.` |
|       - | 7776 | ` * If something goes wrong while generating bytecode` |
|       - | 7777 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7778 | ` * this function takes care of generating the appropriate` |
|       - | 7779 | ` * error message.` |
|       - | 7780 | ` */` |
| 2415240 | 7781 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7782 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7783 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7784 | `	sxi32 iFlags /* Control flags */` |
|       - | 7785 | `	)` |
|       2 | 7786 |  |
|       - | 7787 | `	VmInstr *pInstr;` |
|       - | 7788 | `	sxu32 nJmpIdx;` |
| 2415242 | 7789 | `	sxi32 iP1 = 0;` |
| 2415242 | 7790 | `	sxu32 iP2 = 0;` |
| 2415242 | 7791 | `	void *p3  = 0;` |
|       - | 7792 | `	sxi32 iVmOp;` |
|       - | 7793 | `	sxi32 rc;` |
| 2415242 | 7794 | `	if( pNode->xCode ){` |
|       - | 7795 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7796 | `		/* Compile node */` |
| 1497030 | 7797 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1497030 | 7798 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1497030 | 7799 | `		RE_SWAP_DELIMITER(pGen);` |
| 1497030 | 7800 | `		return rc;` |
|       - | 7801 | `	}` |
|  918214 | 7802 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7803 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7804 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7805 | `		return SXERR_ABORT;` |
|       - | 7806 | `	}` |
|  918214 | 7807 | `	iVmOp = pNode->pOp->iVmOp;` |
|  918214 | 7808 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 | 7809 | `		sxu32 nJmp = 0;` |
|       - | 7810 | `		VmInstr *pInstrFix;` |
|       - | 7811 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 7812 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 7813 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 7814 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 7815 | `		 * stack slot carries a writable nIdx. */` |
|      47 | 7816 | `		if( pNode->pRight ){` |
|      47 | 7817 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 | 7818 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7819 | `				return rc;` |
|       - | 7820 | `			}` |
|       - | 7821 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 7822 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 7823 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 7824 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 7825 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 7826 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 7827 | `			 * cascade for the actual write path stays correct. */` |
|      47 | 7828 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 | 7829 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 | 7830 | `				pInstrFix->iP2 = 3;` |
|       9 | 7831 | `			}` |
|      23 | 7832 | `		}` |
|       - | 7833 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 | 7834 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 7835 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 | 7836 | `		if( pNode->pLeft ){` |
|      47 | 7837 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 | 7838 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7839 | `				return rc;` |
|       - | 7840 | `			}` |
|      23 | 7841 | `		}` |
|       - | 7842 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 | 7843 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 7844 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 | 7845 | `		if( nJmp > 0 ){` |
|      47 | 7846 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 | 7847 | `			if( pInstrFix ){` |
|      47 | 7848 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 | 7849 | `			}` |
|      23 | 7850 | `		}` |
|      47 | 7851 | `		return SXRET_OK;` |
|       - | 7852 | `	}` |
|  918168 | 7853 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7854 | `		sxu32 nJz,nJmp;` |
|       - | 7855 | `		/* Ternary operator require special handling */` |
|       - | 7856 | `		/* Phase#1: Compile the condition */` |
|    1896 | 7857 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1896 | 7858 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7859 | `			return rc;` |
|       - | 7860 | `		}` |
|    1896 | 7861 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1896 | 7862 | `		if( pNode->pLeft ){` |
|       - | 7863 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7864 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1828 | 7865 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7866 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1828 | 7867 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1828 | 7868 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7869 | `				return rc;` |
|       - | 7870 | `			}` |
|     915 | 7871 | `		}else{` |
|       - | 7872 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7873 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7874 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7875 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7876 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7877 | `		}` |
|       - | 7878 | `		/* Phase#4: Emit the unconditional jump */` |
|    1896 | 7879 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7880 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1896 | 7881 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1896 | 7882 | `		if( pInstr ){` |
|    1896 | 7883 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     947 | 7884 | `		}` |
|    1896 | 7885 | `		if( !pNode->pLeft ){` |
|       - | 7886 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7887 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7888 | `		}` |
|       - | 7889 | `		/* Phase#6: Compile the 'else' expression */` |
|    1896 | 7890 | `		if( pNode->pRight ){` |
|    1896 | 7891 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1896 | 7892 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7893 | `				return rc;` |
|       - | 7894 | `			}` |
|     947 | 7895 | `		}` |
|    1896 | 7896 | `		if( nJmp > 0 ){` |
|       - | 7897 | `			/* Phase#7: Fix the unconditional jump */` |
|    1896 | 7898 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1896 | 7899 | `			if( pInstr ){` |
|    1896 | 7900 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     947 | 7901 | `			}` |
|     947 | 7902 | `		}` |
|       - | 7903 | `		/* All done */` |
|    1896 | 7904 | `		return SXRET_OK;` |
|       - | 7905 | `	}` |
|       - | 7906 | `	/* Generate code for the left tree */` |
|  916274 | 7907 | `	if( pNode->pLeft ){` |
|  916238 | 7908 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7909 | `			ph7_expr_node **apNode;` |
|  307514 | 7910 | `			int hasSpread = 0;` |
|       - | 7911 | `			sxi32 n;` |
|       - | 7912 | `			/* Recurse and generate bytecodes for function arguments */` |
|  307514 | 7913 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7914 | `			/* Read-only load */` |
|  307514 | 7915 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  614438 | 7916 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  306926 | 7917 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  306926 | 7918 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7919 | `					return rc;` |
|       - | 7920 | `				}` |
|  306926 | 7921 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7922 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7923 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7924 | `					hasSpread = 1;` |
|       7 | 7925 | `				}` |
|  153464 | 7926 | `			}` |
|       - | 7927 | `			/* Total number of given arguments */` |
|  307514 | 7928 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  307514 | 7929 | `			iP2 = hasSpread;` |
|       - | 7930 | `			/* Remove stale flags now */` |
|  307514 | 7931 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  153756 | 7932 | `		}` |
|  916238 | 7933 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  916238 | 7934 | `		if( rc != SXRET_OK ){` |
|      21 | 7935 | `			return rc;` |
|       - | 7936 | `		}` |
|  916218 | 7937 | `		if( iVmOp == PH7_OP_CALL ){` |
|  307514 | 7938 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  307514 | 7939 | `			if( pInstr ){` |
|  307514 | 7940 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  306940 | 7941 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7942 | `					sxu32 nQual;` |
|       - | 7943 | `					/* Prevent constant expansion */` |
|  306940 | 7944 | `					pInstr->iP1 = 0;` |
|       - | 7945 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7946 | `					 * Only check function imports — class imports must NOT` |
|       - | 7947 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7948 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7949 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7950 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7951 | `					 * class imports. */ {` |
|  306940 | 7952 | `						int fromImport = 0;` |
|  306940 | 7953 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  306940 | 7954 | `						pInstr->iP2 = (sxi32)nQual;` |
|  306940 | 7955 | `						if( nQual != nOrig ){` |
|       - | 7956 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7957 | `							 * NEW handler can recover the unqualified name. */` |
|      68 | 7958 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      68 | 7959 | `							if( !fromImport ){` |
|      58 | 7960 | `								p3 = (void *)1;` |
|      28 | 7961 | `							}` |
|      35 | 7962 | `						}` |
|       - | 7963 | `					}` |
|  154045 | 7964 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7965 | `					/* Method call,flag that */` |
|     552 | 7966 | `					pInstr->iP2 = 1;` |
|     275 | 7967 | `				}` |
|  153758 | 7968 | `			}` |
|  762462 | 7969 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7970 | `			ph7_expr_node **apNode;` |
|       - | 7971 | `			sxi32 n;` |
|       - | 7972 | `			/* Recurse and generate bytecodes for array index */` |
|   68960 | 7973 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  124438 | 7974 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   55480 | 7975 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   55480 | 7976 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7977 | `					return rc;` |
|       - | 7978 | `				}` |
|   27741 | 7979 | `			}` |
|   68960 | 7980 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   55480 | 7981 | `				iP1 = 1; /* Node have an index associated with it */` |
|   27739 | 7982 | `			}` |
|   68960 | 7983 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7984 | `				/* Create an empty entry when the desired index is not found */` |
|   27230 | 7985 | `				iP2 = 1;` |
|   13616 | 7986 | `			}` |
|  574227 | 7987 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7988 | `			/* POP the left node */` |
|      32 | 7989 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7990 | `		}` |
|  458108 | 7991 | `	}` |
|  916254 | 7992 | `	rc = SXRET_OK;` |
|  916254 | 7993 | `	nJmpIdx = 0;` |
|       - | 7994 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7995 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7996 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  916254 | 7997 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     236 | 7998 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     236 | 7999 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     236 | 8000 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     236 | 8001 | `			int isSpecial = 0;` |
|     236 | 8002 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     152 | 8003 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     152 | 8004 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     163 | 8005 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     131 | 8006 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      67 | 8007 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      86 | 8008 | `					isSpecial = 1;` |
|      42 | 8009 | `				}` |
|      96 | 8010 | `			}` |
|     278 | 8011 | `			pInstr->iP1 = 0;` |
|     278 | 8012 | `			if( !isSpecial ){` |
|     110 | 8013 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      54 | 8014 | `			}` |
|       - | 8015 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 8016 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     194 | 8017 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     110 | 8018 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     110 | 8019 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 8020 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 8021 | `					return SXRET_OK;` |
|       - | 8022 | `				}` |
|      33 | 8023 | `			}` |
|      75 | 8024 | `		}` |
|     147 | 8025 | `	}` |
|       - | 8026 | `	/* Generate code for the right tree */` |
|  916178 | 8027 | `	if( pNode->pRight ){` |
|  478676 | 8028 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 8029 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8474 | 8030 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  474440 | 8031 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 8032 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2830 | 8033 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  468790 | 8034 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 8035 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 8036 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 8037 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  467361 | 8038 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  208818 | 8039 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  104408 | 8040 | `		}` |
|  478676 | 8041 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  478676 | 8042 | `		if( iVmOp == PH7_OP_STORE ){` |
|  205960 | 8043 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  205934 | 8044 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 8045 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 8046 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 8047 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 8048 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 8049 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 8050 | `				 */` |
|      54 | 8051 | `				iVmOp = 0;` |
|  205934 | 8052 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  205908 | 8053 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 8054 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   45720 | 8055 | `					iP2 = 1;` |
|   22861 | 8056 | `				}else{` |
|  160190 | 8057 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8058 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   27168 | 8059 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   27168 | 8060 | `						iP1 = pInstr->iP1;` |
|   13585 | 8061 | `					}else{` |
|  133024 | 8062 | `						p3 = pInstr->p3;` |
|       - | 8063 | `					}` |
|       - | 8064 | `					/* POP the last dynamic load instruction */` |
|  160190 | 8065 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 8066 | `				}` |
|  102955 | 8067 | `			}` |
|  375697 | 8068 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 8069 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 8070 | `			if( pInstr ){` |
|      48 | 8071 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 8072 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 8073 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 8074 | `					 */` |
|      15 | 8075 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 8076 | `					iP1 = pInstr->iP1;` |
|      15 | 8077 | `					iP2 = pInstr->iP2;` |
|      15 | 8078 | `					p3  = pInstr->p3;` |
|       8 | 8079 | `				}else{` |
|      34 | 8080 | `					p3 = pInstr->p3;` |
|       - | 8081 | `				}` |
|      23 | 8082 | `			}` |
|      23 | 8083 | `		}` |
|  239337 | 8084 | `	}` |
|  916178 | 8085 | `	if( iVmOp > 0 ){` |
|  916066 | 8086 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10982 | 8087 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 8088 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8060 | 8089 | `				iP1 = 1;` |
|    4031 | 8090 | `			}` |
|  910576 | 8091 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 8092 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13870 | 8093 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13870 | 8094 | `				VmInstr *pCallInstr = 0;` |
|   13870 | 8095 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13854 | 8096 | `					pCallInstr = pPeek;` |
|   13854 | 8097 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6926 | 8098 | `				}` |
|   13870 | 8099 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 8100 | `					sxu32 nLitForClass;` |
|       - | 8101 | `					/* If the CALL handler already qualified the name using` |
|       - | 8102 | `					 * function imports, recover the original unqualified` |
|       - | 8103 | `					 * literal so we can re-qualify with class imports. */` |
|   13868 | 8104 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      32 | 8105 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      17 | 8106 | `					}else{` |
|   13838 | 8107 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 8108 | `					}` |
|   13868 | 8109 | `					pPeek->iP1 = 0;` |
|   13868 | 8110 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6933 | 8111 | `				}` |
|       - | 8112 | `			}` |
|   13870 | 8113 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13870 | 8114 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 8115 | `				VmInstr *pPrev;` |
|   13854 | 8116 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13854 | 8117 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 8118 | `					/* Pop the call instruction */` |
|   13854 | 8119 | `					iP1 = pInstr->iP1;` |
|   13854 | 8120 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6926 | 8121 | `				}` |
|    6928 | 8122 | `			}` |
|  898152 | 8123 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 8124 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 8125 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 8126 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 8127 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 8128 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 8129 | `				int isSpecialIs = 0;` |
|      50 | 8130 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 8131 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 8132 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 8133 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 8134 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 8135 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 8136 | `						isSpecialIs = 1;` |
|       5 | 8137 | `					}` |
|      23 | 8138 | `				}` |
|      52 | 8139 | `				pInstr->iP1 = 0;` |
|      52 | 8140 | `				if( !isSpecialIs ){` |
|      38 | 8141 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 8142 | `				}` |
|      25 | 8143 | `			}` |
|  891197 | 8144 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 8145 | `			/* Prevent constant expansion for member/property names.` |
|       - | 8146 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 8147 | `			 * should not trigger constant lookup. */` |
|  102970 | 8148 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  102970 | 8149 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  102954 | 8150 | `				pInstr->iP1 = 0;` |
|   51476 | 8151 | `			}` |
|  102970 | 8152 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 8153 | `				/* Static member access,remember that */` |
|     160 | 8154 | `				iP1 = 1;` |
|     160 | 8155 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     160 | 8156 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 8157 | `					p3 = pInstr->p3;` |
|      10 | 8158 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 8159 | `				}` |
|      79 | 8160 | `			}` |
|   51484 | 8161 | `		}` |
|       - | 8162 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  916064 | 8163 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  458031 | 8164 | `	}` |
|  916176 | 8165 | `	if( nJmpIdx > 0 ){` |
|       - | 8166 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11332 | 8167 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11332 | 8168 | `		if( pInstr ){` |
|   11332 | 8169 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5665 | 8170 | `		}` |
|    5665 | 8171 | `	}` |
|  916176 | 8172 | `	return rc;` |
| 1207604 | 8173 |  |
|       - | 8174 | `/*` |
|       - | 8175 | ` * Compile a PHP expression.` |
|       - | 8176 | ` * According to the PHP language reference manual:` |
|       - | 8177 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 8178 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 8179 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 8180 | ` *  is "anything that has a value".` |
|       - | 8181 | ` * If something goes wrong while compiling the expression,this` |
|       - | 8182 | ` * function takes care of generating the appropriate error` |
|       - | 8183 | ` * message.` |
|       - | 8184 | ` */` |
|  652366 | 8185 | `static sxi32 PH7_CompileExpr(` |
|       - | 8186 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8187 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 8188 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 8189 | `	)` |
|       2 | 8190 |  |
|       - | 8191 | `	ph7_expr_node *pRoot;` |
|       - | 8192 | `	SySet sExprNode;` |
|       - | 8193 | `	SyToken *pEnd;` |
|       - | 8194 | `	sxi32 nExpr;` |
|       - | 8195 | `	sxi32 iNest;` |
|       - | 8196 | `	sxi32 rc;` |
|       - | 8197 | `	/* Initialize worker variables */` |
|  652368 | 8198 | `	nExpr = 0;` |
|  652368 | 8199 | `	pRoot = 0;` |
|  652368 | 8200 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  652368 | 8201 | `	SySetAlloc(&sExprNode,0x10);` |
|  652368 | 8202 | `	rc = SXRET_OK;` |
|       - | 8203 | `	/* Delimit the expression */` |
|  652368 | 8204 | `	pEnd = pGen->pIn;` |
|  652368 | 8205 | `	iNest = 0;` |
| 4397786 | 8206 | `	while( pEnd < pGen->pEnd ){` |
| 4170220 | 8207 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8208 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     236 | 8209 | `			iNest++;` |
| 4170103 | 8210 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     244 | 8211 | `			iNest--;` |
| 4169865 | 8212 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  425008 | 8213 | `			if( iNest <= 0 ){` |
|  424802 | 8214 | `				break;` |
|       - | 8215 | `			}` |
|     103 | 8216 | `		}` |
| 3745420 | 8217 | `		pEnd++;` |
|       2 | 8218 | `	}` |
|  652368 | 8219 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10936 | 8220 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10936 | 8221 | `		iNest = 0;` |
|       - | 8222 | `		/* Stop at the first comma */` |
|   21894 | 8223 | `		while( pEnd2 < pEnd ){` |
|   10960 | 8224 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 8225 | `				iNest++;` |
|   10958 | 8226 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 8227 | `				iNest--;` |
|   10954 | 8228 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 8229 | `				if( iNest <= 0 ){` |
|     ! 0 | 8230 | `					break;` |
|       - | 8231 | `				}` |
|       2 | 8232 | `			}` |
|   10960 | 8233 | `			pEnd2++;` |
|       2 | 8234 | `		}` |
|   10936 | 8235 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 8236 | `			pEnd = pEnd2;` |
|     ! 0 | 8237 | `		}` |
|    5467 | 8238 | `	}` |
|  652368 | 8239 | `	if( pEnd > pGen->pIn ){` |
|  652358 | 8240 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 8241 | `		/* Swap delimiter */` |
|  652358 | 8242 | `		pGen->pEnd = pEnd;` |
|       - | 8243 | `		/* Try to get an expression tree */` |
|  652358 | 8244 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  652358 | 8245 | `		if( rc == SXRET_OK && pRoot ){` |
|  652188 | 8246 | `			rc = SXRET_OK;` |
|  652188 | 8247 | `			if( xTreeValidator ){` |
|       - | 8248 | `				/* Call the upper layer validator callback */` |
|   14038 | 8249 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7018 | 8250 | `			}` |
|  652188 | 8251 | `			if( rc != SXERR_ABORT ){` |
|       - | 8252 | `				/* Generate code for the given tree */` |
|  652188 | 8253 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  326093 | 8254 | `			}` |
|  652188 | 8255 | `			nExpr = 1;` |
|  326093 | 8256 | `		}` |
|       - | 8257 | `		/* Release the whole tree */` |
|  652358 | 8258 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 8259 | `		/* Synchronize token stream */` |
|  652358 | 8260 | `		pGen->pEnd = pTmp;` |
|  652358 | 8261 | `		pGen->pIn  = pEnd;` |
|  652358 | 8262 | `		if( rc == SXERR_ABORT ){` |
|      11 | 8263 | `			SySetRelease(&sExprNode);` |
|      11 | 8264 | `			return SXERR_ABORT;` |
|       - | 8265 | `		}` |
|  326173 | 8266 | `	}` |
|  652358 | 8267 | `	SySetRelease(&sExprNode);` |
|  652358 | 8268 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  326185 | 8269 |  |
|       - | 8270 | `/*` |
|       - | 8271 | ` * Return a pointer to the node construct handler associated` |
|       - | 8272 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 8273 | ` */` |
|  162820 | 8274 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 8275 |  |
|  162822 | 8276 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 8277 | `		/* Numeric literal: Either real or integer */` |
|   89036 | 8278 | `		return PH7_CompileNumLiteral;` |
|   73788 | 8279 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 8280 | `		/* Double quoted string */` |
|   15764 | 8281 | `		return PH7_CompileString;` |
|   58026 | 8282 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 8283 | `		/* Single quoted string */` |
|   57916 | 8284 | `		return PH7_CompileSimpleString;` |
|     112 | 8285 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 8286 | `		/* Heredoc */` |
|      64 | 8287 | `		return PH7_CompileHereDoc;` |
|      50 | 8288 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 8289 | `		/* Nowdoc */` |
|      44 | 8290 | `		return PH7_CompileNowDoc;` |
|       7 | 8291 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 8292 | `		/* Backtick quoted string */` |
|       5 | 8293 | `		return PH7_CompileBacktic;` |
|       - | 8294 | `	}` |
|       3 | 8295 | `	return 0;` |
|   81412 | 8296 |  |
|       - | 8297 | `/*` |
|       - | 8298 | ` * Compile an unset() statement.` |
|       - | 8299 | ` * unset($var, $arr[$key], ...);` |
|       - | 8300 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 8301 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 8302 | ` * parent array before extracting the element to unset.` |
|       - | 8303 | ` */` |
|    2598 | 8304 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 8305 |  |
|    2600 | 8306 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2600 | 8307 | `	sxu32 nIdx = 0;` |
|       - | 8308 | `	SyString sName;` |
|       - | 8309 | `	sxi32 rc;` |
|       - | 8310 | `	/* Jump the 'unset' keyword */` |
|    2600 | 8311 | `	pGen->pIn++;` |
|       - | 8312 | `	/* Save delimiter */` |
|    2600 | 8313 | `	pTmp = pGen->pEnd;` |
|       - | 8314 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2600 | 8315 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2600 | 8316 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 8317 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 8318 | `		SyToken *pClose;` |
|    2600 | 8319 | `		pGen->pIn++;   /* Skip '(' */` |
|    2600 | 8320 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2600 | 8321 | `		pEnd = pClose; /* Stop at ')' */` |
|    1299 | 8322 | `	}` |
|    2600 | 8323 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 8324 | `	/* Resolve the 'unset' builtin name once */` |
|    2600 | 8325 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     304 | 8326 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     304 | 8327 | `		if( pObj == 0 ){` |
|     ! 0 | 8328 | `			return SXERR_ABORT;` |
|       - | 8329 | `		}` |
|     304 | 8330 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     304 | 8331 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     151 | 8332 | `	}` |
|       - | 8333 | `	/* Compile each comma-separated argument */` |
|    8708 | 8334 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6110 | 8335 | `		if( pGen->pIn < pNext ){` |
|    6110 | 8336 | `			pGen->pEnd = pNext;` |
|    6110 | 8337 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 8338 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6110 | 8339 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8340 | `				return SXERR_ABORT;` |
|       - | 8341 | `			}` |
|    6110 | 8342 | `			if( rc != SXERR_EMPTY ){` |
|       - | 8343 | `				/* Emit call for this single argument */` |
|    6108 | 8344 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6108 | 8345 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6108 | 8346 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3053 | 8347 | `			}` |
|    3054 | 8348 | `		}` |
|       - | 8349 | `		/* Jump trailing commas */` |
|    9622 | 8350 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3514 | 8351 | `			pNext++;` |
|       2 | 8352 | `		}` |
|    6110 | 8353 | `		pGen->pIn = pNext;` |
|       2 | 8354 | `	}` |
|       - | 8355 | `	/* Skip past the closing ')' if present */` |
|    2600 | 8356 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2600 | 8357 | `		pGen->pIn++;` |
|    1299 | 8358 | `	}` |
|       - | 8359 | `	/* Restore token stream */` |
|    2600 | 8360 | `	pGen->pEnd = pTmp;` |
|    2600 | 8361 | `	return SXRET_OK;` |
|    1301 | 8362 |  |
|       - | 8363 | `/*` |
|       - | 8364 | ` * PHP Language construct table.` |
|       - | 8365 | ` */` |
|       - | 8366 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 8367 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 8368 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 8369 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 8370 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 8371 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 8372 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 8373 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 8374 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 8375 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 8376 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 8377 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 8378 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 8379 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 8380 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 8381 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 8382 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 8383 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 8384 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 8385 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 8386 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 8387 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 8388 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 8389 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 8390 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 8391 | `};` |
|       - | 8392 | `/*` |
|       - | 8393 | ` * Return a pointer to the statement handler routine associated` |
|       - | 8394 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 8395 | ` */` |
|  395754 | 8396 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 8397 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 8398 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 8399 | `	)` |
|       2 | 8400 |  |
|  395756 | 8401 | `	sxu32 n = 0;` |
| 1663146 | 8402 | `	for(;;){` |
| 3326294 | 8403 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   46336 | 8404 | `			break;` |
|       - | 8405 | `		}` |
| 3279960 | 8406 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  349422 | 8407 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 8408 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 8409 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 8410 | `					/* 'static' (class context),return null */` |
|     ! 0 | 8411 | `					return 0;` |
|       - | 8412 | `				}` |
|     ! 0 | 8413 | `			}` |
|       - | 8414 | `			/* Return a pointer to the handler.` |
|       - | 8415 | `			*/` |
|  349422 | 8416 | `			return aLangConstruct[n].xConstruct;` |
|       - | 8417 | `		}` |
| 2930540 | 8418 | `		n++;` |
|       2 | 8419 | `	}` |
|   46336 | 8420 | `	if( pLookahed ){` |
|   46336 | 8421 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8092 | 8422 | `			return PH7_CompileClassInterface;` |
|   38246 | 8423 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   38042 | 8424 | `			return PH7_CompileClass;` |
|     206 | 8425 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 8426 | `			return PH7_CompileTrait;` |
|     152 | 8427 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 8428 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 8429 | `				return PH7_CompileAbstractClass;` |
|     136 | 8430 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 8431 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 8432 | `				return PH7_CompileFinalClass;` |
|       - | 8433 | `		}` |
|      67 | 8434 | `	}` |
|       - | 8435 | `	/* Not a language construct */` |
|     136 | 8436 | `	return 0;` |
|  197879 | 8437 |  |
|       - | 8438 | `/*` |
|       - | 8439 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 8440 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 8441 | ` */` |
|     134 | 8442 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 8443 |  |
|       - | 8444 | `	int rc;` |
|     136 | 8445 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 8446 | `	if( rc == FALSE ){` |
|      40 | 8447 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 8448 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 8449 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 8450 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 8451 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 8452 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 8453 | `			*/` |
|       - | 8454 | `			){` |
|      34 | 8455 | `				rc = TRUE;` |
|      16 | 8456 | `		}` |
|      20 | 8457 | `	}` |
|     136 | 8458 | `	return rc;` |
|       2 | 8459 |  |
|       - | 8460 | `/*` |
|       - | 8461 | ` * Compile a PHP chunk.` |
|       - | 8462 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8463 | ` * takes care of generating the appropriate error message.` |
|       - | 8464 | ` */` |
|  531314 | 8465 | `static sxi32 GenStateCompileChunk(` |
|       - | 8466 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8467 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 8468 | `	)` |
|       2 | 8469 |  |
|       - | 8470 | `	ProcLangConstruct xCons;` |
|       - | 8471 | `	sxi32 rc;` |
|  531316 | 8472 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  317490 | 8473 | `	for(;;){` |
|  634982 | 8474 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8475 | `			/* No more input to process */` |
|   11578 | 8476 | `			break;` |
|       - | 8477 | `		}` |
|  623406 | 8478 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8479 | `			/* Compile block */` |
|      12 | 8480 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 8481 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8482 | `				break;` |
|       - | 8483 | `			}` |
|       7 | 8484 | `		}else{` |
|  623396 | 8485 | `			xCons = 0;` |
|  623396 | 8486 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  395756 | 8487 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 8488 | `				/* Try to extract a language construct handler */` |
|  395756 | 8489 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  395756 | 8490 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 8491 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 8492 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 8493 | `						&pGen->pIn->sData);` |
|       9 | 8494 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8495 | `						break;` |
|       - | 8496 | `					}` |
|       - | 8497 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 8498 | `					 * this erroneous statement.` |
|       - | 8499 | `					 */` |
|       9 | 8500 | `					xCons = PH7_ErrorRecover;` |
|       4 | 8501 | `				}` |
|  425519 | 8502 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   39842 | 8503 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 8504 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 8505 | `				xCons = PH7_CompileLabel;` |
|      56 | 8506 | `			}` |
|  623396 | 8507 | `			if( xCons == 0 ){` |
|       - | 8508 | `				/* Assume an expression an try to compile it */` |
|  227656 | 8509 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  227656 | 8510 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 8511 | `					/* Pop l-value */` |
|  227518 | 8512 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  113758 | 8513 | `				}` |
|  113829 | 8514 | `			}else{` |
|       - | 8515 | `				/* Go compile the sucker */` |
|  395742 | 8516 | `				rc = xCons(&(*pGen));` |
|       - | 8517 | `			}` |
|  623396 | 8518 | `			if( rc == SXERR_ABORT ){` |
|       - | 8519 | `				/* Request to abort compilation */` |
|      11 | 8520 | `				break;` |
|       - | 8521 | `			}` |
|       - | 8522 | `		}` |
|       - | 8523 | `		/* Ignore trailing semi-colons ';' */` |
| 1032534 | 8524 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  409140 | 8525 | `			pGen->pIn++;` |
|       2 | 8526 | `		}` |
|  623396 | 8527 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 8528 | `			/* Compile a single statement and return */` |
|  519730 | 8529 | `			break;` |
|       - | 8530 | `		}` |
|       - | 8531 | `		/* LOOP ONE */` |
|       - | 8532 | `		/* LOOP TWO */` |
|       - | 8533 | `		/* LOOP THREE */` |
|       - | 8534 | `		/* LOOP FOUR */` |
|       2 | 8535 | `	}` |
|       - | 8536 | `	/* Return compilation status */` |
|  531316 | 8537 | `	return rc;` |
|       2 | 8538 |  |
|       - | 8539 | `/*` |
|       - | 8540 | ` * Compile a Raw PHP chunk.` |
|       - | 8541 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8542 | ` * takes care of generating the appropriate error message.` |
|       - | 8543 | ` */` |
|   11588 | 8544 | `static sxi32 PH7_CompilePHP(` |
|       - | 8545 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8546 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8547 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8548 | `	)` |
|       2 | 8549 |  |
|   11590 | 8550 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8551 | `	sxi32 rc;` |
|       - | 8552 | `	/* Reset the token set */` |
|   11590 | 8553 | `	SySetReset(&(*pTokenSet));` |
|       - | 8554 | `	/* Mark as the default token set */` |
|   11590 | 8555 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8556 | `	/* Advance the stream cursor */` |
|   11590 | 8557 | `	pGen->pRawIn++;` |
|       - | 8558 | `	/* Tokenize the PHP chunk first */` |
|   11590 | 8559 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8560 | `	/* Point to the head and tail of the token stream. */` |
|   11590 | 8561 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11590 | 8562 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11590 | 8563 | `	if( is_expr ){` |
|     ! 0 | 8564 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8565 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8566 | `			/* A simple expression,compile it */` |
|     ! 0 | 8567 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8568 | `		}` |
|       - | 8569 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8570 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8571 | `		return SXRET_OK;` |
|       - | 8572 | `	}` |
|   11590 | 8573 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8574 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8575 | `		/*` |
|       - | 8576 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8577 | `		 * According to the PHP reference manual:` |
|       - | 8578 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8579 | `		 *  immediately follow` |
|       - | 8580 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8581 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8582 | `		 * Symisc extension:` |
|       - | 8583 | `		 *   This short syntax works with all PHP opening` |
|       - | 8584 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8585 | `		 *   only short tag.` |
|       - | 8586 | `		 */` |
|       - | 8587 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8588 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8589 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8590 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8591 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8592 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8593 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8594 | `		}` |
|       3 | 8595 | `		return SXRET_OK;` |
|       - | 8596 | `	}` |
|       - | 8597 | `	/* Compile the PHP chunk */` |
|   11588 | 8598 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8599 | `	/* Fix exceptions jumps */` |
|   11588 | 8600 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8601 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11588 | 8602 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8603 | `		rc = SXERR_ABORT;` |
|       1 | 8604 | `	}` |
|       - | 8605 | `	/* Reset container */` |
|   11588 | 8606 | `	SySetReset(&pGen->aGoto);` |
|   11588 | 8607 | `	SySetReset(&pGen->aLabel);` |
|       - | 8608 | `	/* Compilation result */` |
|   11588 | 8609 | `	return rc;` |
|    5796 | 8610 |  |
|       - | 8611 | `/*` |
|       - | 8612 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8613 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8614 | ` * This is the only compile interface exported from this file.` |
|       - | 8615 | ` */` |
|   13720 | 8616 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8617 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8618 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8619 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8620 | `	)` |
|       2 | 8621 |  |
|       - | 8622 | `	SySet aPhpToken,aRawToken;` |
|       - | 8623 | `	ph7_gen_state *pCodeGen;` |
|       - | 8624 | `	ph7_value *pRawObj;` |
|       - | 8625 | `	sxu32 nObjIdx;` |
|       - | 8626 | `	sxi32 nRawObj;` |
|       - | 8627 | `	int is_expr;` |
|       - | 8628 | `	sxi32 rc;` |
|   13722 | 8629 | `	if( pScript->nByte < 1 ){` |
|       - | 8630 | `		/* Nothing to compile */` |
|     ! 0 | 8631 | `		return PH7_OK;` |
|       - | 8632 | `	}` |
|       - | 8633 | `	/* Initialize the tokens containers */` |
|   13722 | 8634 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13722 | 8635 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13722 | 8636 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13722 | 8637 | `	is_expr = 0;` |
|   13722 | 8638 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8639 | `		SyToken sTmp;` |
|       - | 8640 | `		/* PHP only: -*/` |
|    2714 | 8641 | `		sTmp.nLine = 1;` |
|    2714 | 8642 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2714 | 8643 | `		sTmp.pUserData = 0;` |
|    2714 | 8644 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2714 | 8645 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2714 | 8646 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8647 | `			/* A simple PHP expression */` |
|     ! 0 | 8648 | `			is_expr = 1;` |
|     ! 0 | 8649 | `		}` |
|    1358 | 8650 | `	}else{` |
|       - | 8651 | `		/* Tokenize raw text */` |
|   11010 | 8652 | `		SySetAlloc(&aRawToken,32);` |
|   11010 | 8653 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8654 | `	}` |
|   13722 | 8655 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8656 | `	/* Process high-level tokens */` |
|   13722 | 8657 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13722 | 8658 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13722 | 8659 | `	rc = PH7_OK;` |
|   13722 | 8660 | `	if( is_expr ){` |
|       - | 8661 | `		/* Compile the expression */` |
|     ! 0 | 8662 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8663 | `		goto cleanup;` |
|       - | 8664 | `	}` |
|   13722 | 8665 | `	nObjIdx = 0;` |
|       - | 8666 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8667 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8668 | `	 * preventing namespace bleeding across include()d files. */` |
|   13722 | 8669 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8670 | `	/* Start the compilation process */` |
|   12368 | 8671 | `	for(;;){` |
|   36314 | 8672 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13710 | 8673 | `			break; /* No more tokens to process */` |
|       - | 8674 | `		}` |
|   22606 | 8675 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8676 | `			/* Compile the PHP chunk */` |
|   11590 | 8677 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11590 | 8678 | `			if( rc == SXERR_ABORT ){` |
|      13 | 8679 | `				break;` |
|       - | 8680 | `			}` |
|   11578 | 8681 | `			continue;` |
|       - | 8682 | `		}` |
|       - | 8683 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11018 | 8684 | `		nRawObj = 0;` |
|   22034 | 8685 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8686 | `			/* Consume the raw chunk without any processing */` |
|   11018 | 8687 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11018 | 8688 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8689 | `				rc = SXERR_MEM;` |
|     ! 0 | 8690 | `				break;` |
|       - | 8691 | `			}` |
|       - | 8692 | `			/* Mark as constant and emit the load constant instruction */` |
|   11018 | 8693 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11018 | 8694 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11018 | 8695 | `			++nRawObj;` |
|   11018 | 8696 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8697 | `		}` |
|   11018 | 8698 | `		if( nRawObj > 0 ){` |
|       - | 8699 | `			/* Emit the consume instruction */` |
|   11018 | 8700 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5508 | 8701 | `		}` |
|    6862 | 8702 | `	}` |
|    6860 | 8703 | `cleanup:` |
|   13722 | 8704 | `	SySetRelease(&aRawToken);` |
|   13722 | 8705 | `	SySetRelease(&aPhpToken);` |
|   13722 | 8706 | `	return rc;` |
|    6862 | 8707 |  |
|       - | 8708 | `/*` |
|       - | 8709 | ` * Utility routines.Initialize the code generator.` |
|       - | 8710 | ` */` |
|    2684 | 8711 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8712 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8713 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8714 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8715 | `	)` |
|       2 | 8716 |  |
|    2686 | 8717 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8718 | `	/* Zero the structure */` |
|    2686 | 8719 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8720 | `	/* Initial state */` |
|    2686 | 8721 | `	pGen->pVm  = &(*pVm);` |
|    2686 | 8722 | `	pGen->xErr = xErr;` |
|    2686 | 8723 | `	pGen->pErrData = pErrData;` |
|    2686 | 8724 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2686 | 8725 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2686 | 8726 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2686 | 8727 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8728 | `	/* Error log buffer */` |
|    2686 | 8729 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8730 | `	/* General purpose working buffer */` |
|    2686 | 8731 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8732 | `	/* Namespace state */` |
|    2686 | 8733 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2686 | 8734 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2686 | 8735 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2686 | 8736 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8737 | `	/* Create the global scope */` |
|    2686 | 8738 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8739 | `	/* Point to the global scope */` |
|    2686 | 8740 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2686 | 8741 | `	return SXRET_OK;` |
|       2 | 8742 |  |
|       - | 8743 | `/*` |
|       - | 8744 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8745 | ` */` |
|   16130 | 8746 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8747 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8748 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8749 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8750 | `	)` |
|       2 | 8751 |  |
|   16132 | 8752 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8753 | `	GenBlock *pBlock,*pParent;` |
|       - | 8754 | `	/* Reset state */` |
|   16132 | 8755 | `	SySetReset(&pGen->aLabel);` |
|   16132 | 8756 | `	SySetReset(&pGen->aGoto);` |
|   16132 | 8757 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16132 | 8758 | `	SyBlobRelease(&pGen->sWorker);` |
|   16132 | 8759 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16132 | 8760 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16132 | 8761 | `	SyHashRelease(&pGen->hUseImports);` |
|   16132 | 8762 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16132 | 8763 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16132 | 8764 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16132 | 8765 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16132 | 8766 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8767 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8768 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8769 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8770 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8771 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8772 | `	 * number of unique names, which is acceptable. */` |
|       - | 8773 | `	/* Point to the global scope */` |
|   16132 | 8774 | `	pBlock = pGen->pCurrent;` |
|   16132 | 8775 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8776 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8777 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8778 | `		pBlock = pParent;` |
|     ! 0 | 8779 | `	}` |
|   16132 | 8780 | `	pGen->xErr = xErr;` |
|   16132 | 8781 | `	pGen->pErrData = pErrData;` |
|   16132 | 8782 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16132 | 8783 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16132 | 8784 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16132 | 8785 | `	pGen->nErr = 0;` |
|   16132 | 8786 | `	return SXRET_OK;` |
|       2 | 8787 |  |
|       - | 8788 | `/*` |
|       - | 8789 | ` * Generate a compile-time error message.` |
|       - | 8790 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8791 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8792 | ` * abort compilation immediately.` |
|       - | 8793 | ` */` |
|     500 | 8794 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8795 |  |
|     502 | 8796 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     502 | 8797 | `	const char *zErr = "Error";` |
|       - | 8798 | `	SyString *pFile;` |
|       - | 8799 | `	va_list ap;` |
|       - | 8800 | `	sxi32 rc;` |
|       - | 8801 | `	/* Reset the working buffer */` |
|     502 | 8802 | `	SyBlobReset(pWorker);` |
|       - | 8803 | `	/* Peek the processed file path if available */` |
|     502 | 8804 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     502 | 8805 | `	if( nErrType == E_ERROR ){` |
|       - | 8806 | `		/* Increment the error counter */` |
|     430 | 8807 | `		pGen->nErr++;` |
|     430 | 8808 | `		if( pGen->nErr > 15 ){` |
|       - | 8809 | `			/* Error count limit reached */` |
|       5 | 8810 | `			if( pGen->xErr ){` |
|       5 | 8811 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8812 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8813 | `				if( pFile ){` |
|       5 | 8814 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8815 | `				}` |
|       5 | 8816 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8817 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8818 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8819 | `				}` |
|       2 | 8820 | `			}` |
|       - | 8821 | `			/* Abort immediately */` |
|       5 | 8822 | `			return SXERR_ABORT;` |
|       - | 8823 | `		}` |
|     212 | 8824 | `	}` |
|     498 | 8825 | `	if( pGen->xErr == 0 ){` |
|       - | 8826 | `		/* No available error consumer,return immediately */` |
|       3 | 8827 | `		return SXRET_OK;` |
|       - | 8828 | `	}` |
|     495 | 8829 | `	switch(nErrType){` |
|     423 | 8830 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8831 | `	case E_WARNING: zErr = "Warning";     break;` |
|      37 | 8832 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8833 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8834 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8835 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8836 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8837 | `	default:` |
|     ! 0 | 8838 | `		break;` |
|       - | 8839 | `	}` |
|     495 | 8840 | `	rc = SXRET_OK;` |
|       - | 8841 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     495 | 8842 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     495 | 8843 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     495 | 8844 | `	va_start(ap,zFormat);` |
|     495 | 8845 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     495 | 8846 | `	va_end(ap);` |
|     495 | 8847 | `	if( pFile ){` |
|     495 | 8848 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     247 | 8849 | `	}` |
|       - | 8850 | `	/* Append a new line */` |
|     495 | 8851 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     495 | 8852 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8853 | `		/* Consume the generated error message */` |
|     495 | 8854 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     247 | 8855 | `	}` |
|     495 | 8856 | `	return rc;` |
|     252 | 8857 |  |
|       - | 8858 |  |
