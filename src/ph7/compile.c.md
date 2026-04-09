# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3791/4906 lines (77.27%)

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
|    2884 |  128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |  129 |  |
|    2886 |  130 | `	GenBlock *pBlock = pCurrent;` |
|    8095 |  131 | `	for(;;){` |
|   16192 |  132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2774 |  133 | `			iCount--; /* Decrement nesting level */` |
|    2774 |  134 | `			if( iCount < 1 ){` |
|       - |  135 | `				/* Block meet with the desired criteria */` |
|    2752 |  136 | `				return pBlock;` |
|       - |  137 | `			}` |
|      11 |  138 | `		}` |
|       - |  139 | `		/* Point to the upper block */` |
|   13442 |  140 | `		pBlock = pBlock->pParent;` |
|   13442 |  141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |  142 | `			/* Forbidden */` |
|      69 |  143 | `			break;` |
|       - |  144 | `		}` |
|       2 |  145 | `	}` |
|       - |  146 | `	/* No such block */` |
|     136 |  147 | `	return 0;` |
|    1444 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Initialize a freshly allocated block instance.` |
|       - |  151 | ` */` |
|  560708 |  152 | `static void GenStateInitBlock(` |
|       - |  153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |  155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |  157 | `	void *pUserData      /* Upper layer private data */` |
|       - |  158 | `	)` |
|       2 |  159 |  |
|       - |  160 | `	/* Initialize block fields */` |
|  560710 |  161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  560710 |  162 | `	pBlock->pUserData   = pUserData;` |
|  560710 |  163 | `	pBlock->pGen        = pGen;` |
|  560710 |  164 | `	pBlock->iFlags      = iType;` |
|  560710 |  165 | `	pBlock->pParent     = 0;` |
|  560710 |  166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  560710 |  167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  560710 |  168 |  |
|       - |  169 | `/*` |
|       - |  170 | ` * Allocate a new block instance.` |
|       - |  171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |  172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |  173 | ` * processing on failure.` |
|       - |  174 | ` */` |
|  558082 |  175 | `static sxi32 GenStateEnterBlock(` |
|       - |  176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |  178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |  179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |  180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |  181 | `	)` |
|       2 |  182 |  |
|       - |  183 | `	GenBlock *pBlock;` |
|       - |  184 | `	/* Allocate a new block instance */` |
|  558084 |  185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  558084 |  186 | `	if( pBlock == 0 ){` |
|       - |  187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  189 | `		 */` |
|     ! 0 |  190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |  191 | `		/* Abort processing immediately */` |
|     ! 0 |  192 | `		return SXERR_ABORT;` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Zero the structure */` |
|  558084 |  195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  558084 |  196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |  197 | `	/* Link to the parent block */` |
|  558084 |  198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |  199 | `	/* Mark as the current block */` |
|  558084 |  200 | `	pGen->pCurrent = pBlock;` |
|  558084 |  201 | `	if( ppBlock ){` |
|       - |  202 | `		/* Write a pointer to the new instance */` |
|  270068 |  203 | `		*ppBlock = pBlock;` |
|  135033 |  204 | `	}` |
|  558084 |  205 | `	return SXRET_OK;` |
|  279043 |  206 |  |
|       - |  207 | `/*` |
|       - |  208 | ` * Release block fields without freeing the whole instance.` |
|       - |  209 | ` */` |
|  558074 |  210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |  211 |  |
|  558076 |  212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  558076 |  213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  558076 |  214 |  |
|       - |  215 | `/*` |
|       - |  216 | ` * Release a block.` |
|       - |  217 | ` */` |
|  558074 |  218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |  219 |  |
|  558076 |  220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  558076 |  221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |  222 | `	/* Free the instance */` |
|  558076 |  223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  558076 |  224 |  |
|       - |  225 | `/*` |
|       - |  226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |  227 | ` */` |
|  558074 |  228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |  229 |  |
|  558076 |  230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  558076 |  231 | `	if( pBlock == 0 ){` |
|       - |  232 | `		/* No more block to pop */` |
|     ! 0 |  233 | `		return SXERR_EMPTY;` |
|       - |  234 | `	}` |
|       - |  235 | `	/* Point to the upper block */` |
|  558076 |  236 | `	pGen->pCurrent = pBlock->pParent;` |
|  558076 |  237 | `	if( ppBlock ){` |
|       - |  238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |  239 | `		*ppBlock = pBlock;` |
|     ! 0 |  240 | `	}else{` |
|       - |  241 | `		/* Safely release the block */` |
|  558076 |  242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |  243 | `	}` |
|  558076 |  244 | `	return SXRET_OK;` |
|  279039 |  245 |  |
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
|  170188 |  256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |  257 |  |
|       - |  258 | `	JumpFixup sJumpFix;` |
|       - |  259 | `	sxi32 rc;` |
|       - |  260 | `	/* Init the JumpFixup structure */` |
|  170190 |  261 | `	sJumpFix.nJumpType = nJumpType;` |
|  170190 |  262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |  263 | `	/* Insert in the jump fixup table */` |
|  170190 |  264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  170190 |  265 | `	return rc;` |
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
|  397446 |  278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |  279 |  |
|       - |  280 | `	JumpFixup *aFix;` |
|       - |  281 | `	VmInstr *pInstr;` |
|       - |  282 | `	sxu32 nFixed;` |
|       - |  283 | `	sxu32 n;` |
|       - |  284 | `	/* Point to the jump fixup table */` |
|  397448 |  285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |  286 | `	/* Fix the desired jumps */` |
|  729076 |  287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  331630 |  288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |  289 | `			/* Already fixed */` |
|  129148 |  290 | `			continue;` |
|       - |  291 | `		}` |
|  202484 |  292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |  293 | `			/* Not of our interest */` |
|   32298 |  294 | `			continue;` |
|       - |  295 | `		}` |
|       - |  296 | `		/* Point to the instruction to fix */` |
|  170188 |  297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  170188 |  298 | `		if( pInstr ){` |
|  170188 |  299 | `			pInstr->iP2 = nJumpDest;` |
|  170188 |  300 | `			nFixed++;` |
|       - |  301 | `			/* Mark as fixed */` |
|  170188 |  302 | `			aFix[n].nJumpType = -1;` |
|   85093 |  303 | `		}` |
|   85095 |  304 | `	}` |
|       - |  305 | `	/* Total number of fixed jumps */` |
|  397448 |  306 | `	return nFixed;` |
|       2 |  307 |  |
|       - |  308 | `/*` |
|       - |  309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |  310 | ` * The goto statement can be used to jump to another section` |
|       - |  311 | ` * in the program.` |
|       - |  312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |  313 | ` * statement for more information.` |
|       - |  314 | ` */` |
|  151692 |  315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |  316 |  |
|       - |  317 | `	JumpFixup *pJump,*aJumps;` |
|       - |  318 | `	Label *pLabel,*aLabel;` |
|       - |  319 | `	VmInstr *pInstr;` |
|       - |  320 | `	sxi32 rc;` |
|       - |  321 | `	sxu32 n;` |
|       - |  322 | `	/* Point to the goto table */` |
|  151694 |  323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |  324 | `	/* Fix */` |
|  151840 |  325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  151692 |  350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  151824 |  351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |  352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |  353 | `			/* Emit a warning */` |
|      37 |  354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |  355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |  356 | `		}` |
|      68 |  357 | `	}` |
|  151692 |  358 | `	return SXRET_OK;` |
|   75848 |  359 |  |
|       - |  360 | `/*` |
|       - |  361 | ` * Check if a given token value is installed in the literal table.` |
|       - |  362 | ` */` |
|  494024 |  363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |  364 |  |
|       - |  365 | `	SyHashEntry *pEntry;` |
|  494026 |  366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  494026 |  367 | `	if( pEntry == 0 ){` |
|  243566 |  368 | `		return SXERR_NOTFOUND;` |
|       - |  369 | `	}` |
|  250462 |  370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  250462 |  371 | `	return SXRET_OK;` |
|  247014 |  372 |  |
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
|  243564 |  383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |  384 |  |
|  243566 |  385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  243566 |  386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  121782 |  387 | `	}` |
|  243566 |  388 | `	return SXRET_OK;` |
|       2 |  389 |  |
|       - |  390 | `/*` |
|       - |  391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |  392 | ` * in the constant table.` |
|       - |  393 | ` */` |
|   86372 |  394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |  395 |  |
|       - |  396 | `	ph7_value *pObj;` |
|   86374 |  397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |  398 | `	/* Reserve a new constant */` |
|   86374 |  399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   86374 |  400 | `	if( pObj == 0 ){` |
|     ! 0 |  401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  402 | `		return 0;` |
|       - |  403 | `	}` |
|   86374 |  404 | `	*pIdx = nIdx;` |
|       - |  405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |  406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |  407 | `	 */` |
|   86374 |  408 | `	return pObj;` |
|   43188 |  409 |  |
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
|   86786 |  431 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  432 |  |
|   86788 |  433 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   86788 |  434 | `	sxu32 nIdx = 0;` |
|   86788 |  435 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |  436 | `		ph7_value *pObj;` |
|       - |  437 | `		sxi64 iValue;` |
|   86374 |  438 | `		iValue = PH7_TokenValueToInt64(&pToken->sData);` |
|   86374 |  439 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   86374 |  440 | `		if( pObj == 0 ){` |
|     ! 0 |  441 | `			SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  442 | `			return SXERR_ABORT;` |
|       - |  443 | `		}` |
|   86374 |  444 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   43188 |  445 | `	}else{` |
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
|   86788 |  458 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  459 | `	/* Node successfully compiled */` |
|   86788 |  460 | `	return SXRET_OK;` |
|   43395 |  461 |  |
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
|   56716 |  473 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  474 |  |
|   56718 |  475 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |  476 | `	const char *zIn,*zCur,*zEnd;` |
|       - |  477 | `	ph7_value *pObj;` |
|       - |  478 | `	sxu32 nIdx;` |
|   56718 |  479 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |  480 | `	/* Delimit the string */` |
|   56718 |  481 | `	zIn  = pStr->zString;` |
|   56718 |  482 | `	zEnd = &zIn[pStr->nByte];` |
|   56718 |  483 | `	if( zIn >= zEnd ){` |
|       - |  484 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |  485 | `		 * rather than reserving a new object each time. */` |
|     138 |  486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     138 |  487 | `		return SXRET_OK;` |
|       - |  488 | `	}` |
|   56582 |  489 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |  490 | `		/* Already processed,emit the load constant instruction` |
|       - |  491 | `		 * and return.` |
|       - |  492 | `		 */` |
|   16764 |  493 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16764 |  494 | `		return SXRET_OK;` |
|       - |  495 | `	}` |
|       - |  496 | `	/* Reserve a new constant */` |
|   39820 |  497 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   39820 |  498 | `	if( pObj == 0 ){` |
|     ! 0 |  499 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  500 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|   39820 |  503 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  504 | `	/* Compile the node */` |
|   39860 |  505 | `	for(;;){` |
|   79722 |  506 | `		if( zIn >= zEnd ){` |
|       - |  507 | `			/* End of input */` |
|   39820 |  508 | `			break;` |
|       - |  509 | `		}` |
|   39904 |  510 | `		zCur = zIn;` |
|  633558 |  511 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  593656 |  512 | `			zIn++;` |
|       2 |  513 | `		}` |
|   39904 |  514 | `		if( zIn > zCur ){` |
|       - |  515 | `			/* Append raw contents*/` |
|   39884 |  516 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   19941 |  517 | `		}` |
|   39904 |  518 | `		zIn++;` |
|   39904 |  519 | `		if( zIn < zEnd ){` |
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
|   39904 |  534 | `		zIn++;` |
|       2 |  535 | `	}` |
|       - |  536 | `	/* Emit the load constant instruction */` |
|   39820 |  537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   39820 |  538 | `	if( pStr->nByte < 1024 ){` |
|       - |  539 | `		/* Install in the literal table */` |
|   39820 |  540 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   19909 |  541 | `	}` |
|       - |  542 | `	/* Node successfully compiled */` |
|   39820 |  543 | `	return SXRET_OK;` |
|   28360 |  544 |  |
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
|    1710 |  606 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1712 |  617 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |  618 | `	/* Preallocate some slots */` |
|    1712 |  619 | `	SySetAlloc(&sToken,0x08);` |
|       - |  620 | `	/* Tokenize the text */` |
|    1712 |  621 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |  622 | `	/* Swap delimiter */` |
|    1712 |  623 | `	pTmpIn  = pGen->pIn;` |
|    1712 |  624 | `	pTmpEnd = pGen->pEnd;` |
|    1712 |  625 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1712 |  626 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |  627 | `	/* Compile the expression */` |
|    1712 |  628 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  629 | `	/* Restore token stream */` |
|    1712 |  630 | `	pGen->pIn  = pTmpIn;` |
|    1712 |  631 | `	pGen->pEnd = pTmpEnd;` |
|       - |  632 | `	/* Release the token set */` |
|    1712 |  633 | `	SySetRelease(&sToken);` |
|       - |  634 | `	/* Compilation result */` |
|    1712 |  635 | `	return rc;` |
|       2 |  636 |  |
|       - |  637 | `/*` |
|       - |  638 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |  639 | ` */` |
|   16564 |  640 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |  641 |  |
|       - |  642 | `	ph7_value *pConstObj;` |
|   16566 |  643 | `	sxu32 nIdx = 0;` |
|       - |  644 | `	/* Reserve a new constant */` |
|   16566 |  645 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   16566 |  646 | `	if( pConstObj == 0 ){` |
|     ! 0 |  647 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  648 | `		return 0;` |
|       - |  649 | `	}` |
|   16566 |  650 | `	(*pCount)++;` |
|   16566 |  651 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |  652 | `	/* Emit the load constant instruction */` |
|   16566 |  653 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   16566 |  654 | `	return pConstObj;` |
|    8284 |  655 |  |
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
|   15360 |  694 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  695 |  |
|   15362 |  696 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  697 | `	const char *zIn,*zCur,*zEnd;` |
|   15362 |  698 | `	ph7_value *pObj = 0;` |
|       - |  699 | `	sxi32 iCons;` |
|       - |  700 | `	sxi32 rc;` |
|       - |  701 | `	/* Delimit the string */` |
|   15362 |  702 | `	zIn  = pStr->zString;` |
|   15362 |  703 | `	zEnd = &zIn[pStr->nByte];` |
|   15362 |  704 | `	if( zIn >= zEnd ){` |
|       - |  705 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  706 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  707 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  708 | `		 */` |
|     226 |  709 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     226 |  710 | `		return SXRET_OK;` |
|       - |  711 | `	}` |
|   15138 |  712 | `	zCur = 0;` |
|       - |  713 | `	/* Compile the node */` |
|   15138 |  714 | `	iCons = 0;` |
|    8423 |  715 | `	for(;;){` |
|   25436 |  716 | `		zCur = zIn;` |
|  138286 |  717 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  114562 |  718 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      44 |  719 | `				break;` |
|  114478 |  720 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1628 |  721 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     814 |  722 | `					break;` |
|       - |  723 | `			}` |
|  112852 |  724 | `			zIn++;` |
|       2 |  725 | `		}` |
|   25436 |  726 | `		if( zIn > zCur ){` |
|   11880 |  727 | `			if( pObj == 0 ){` |
|   11604 |  728 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   11604 |  729 | `				if( pObj == 0 ){` |
|     ! 0 |  730 | `					return SXERR_ABORT;` |
|       - |  731 | `				}` |
|    5801 |  732 | `			}` |
|   11880 |  733 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    5939 |  734 | `		}` |
|   25436 |  735 | `		if( zIn >= zEnd ){` |
|   15138 |  736 | `			break;` |
|       - |  737 | `		}` |
|   10300 |  738 | `		if( zIn[0] == '\\' ){` |
|    8590 |  739 | `			const char *zPtr = 0;` |
|       - |  740 | `			sxu32 n;` |
|    8590 |  741 | `			zIn++;` |
|    8590 |  742 | `			if( zIn >= zEnd ){` |
|     ! 0 |  743 | `				break;` |
|       - |  744 | `			}` |
|    8590 |  745 | `			if( pObj == 0 ){` |
|    4964 |  746 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    4964 |  747 | `				if( pObj == 0 ){` |
|     ! 0 |  748 | `					return SXERR_ABORT;` |
|       - |  749 | `				}` |
|    2481 |  750 | `			}` |
|    8590 |  751 | `			n = sizeof(char); /* size of conversion */` |
|    8590 |  752 | `			switch( zIn[0] ){` |
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
|    3929 |  773 | `			case 'n':` |
|       - |  774 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    7860 |  775 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    7860 |  776 | `				break;` |
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
|    8590 |  844 | `			zIn += n;` |
|    8590 |  845 | `			continue;` |
|       - |  846 | `		}` |
|    1712 |  847 | `		if( zIn[0] == '{' ){` |
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
|    1626 |  881 | `			const char *zExpr = zIn;` |
|       - |  882 | `			/* Assemble variable name */` |
|     812 |  883 | `			for(;;){` |
|       - |  884 | `				/* Jump leading dollars */` |
|    3250 |  885 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1626 |  886 | `					zIn++;` |
|       2 |  887 | `				}` |
|     812 |  888 | `				for(;;){` |
|   10066 |  889 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7630 |  890 | `						zIn++;` |
|       2 |  891 | `					}` |
|    1626 |  892 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  893 | `						/* UTF-8 stream */` |
|     ! 0 |  894 | `						zIn++;` |
|     ! 0 |  895 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  896 | `							zIn++;` |
|     ! 0 |  897 | `						}` |
|     ! 0 |  898 | `						continue;` |
|       - |  899 | `					}` |
|    1626 |  900 | `					break;` |
|     ! 0 |  901 | `				}` |
|    1626 |  902 | `				if( zIn >= zEnd ){` |
|      96 |  903 | `					break;` |
|       - |  904 | `				}` |
|    1532 |  905 | `				if( zIn[0] == '[' ){` |
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
|    1524 |  923 | `				}else if(zIn[0] == '{' ){` |
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
|    1520 |  941 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  942 | `					/* Member access operator '->' */` |
|     ! 0 |  943 | `					zIn += 2;` |
|    1520 |  944 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  945 | `					/* Static member access operator '::' */` |
|     ! 0 |  946 | `					zIn += 2;` |
|     ! 0 |  947 | `				}else{` |
|     761 |  948 | `					break;` |
|       - |  949 | `				}` |
|     ! 0 |  950 | `			}` |
|       - |  951 | `			/* Process the expression */` |
|    1626 |  952 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1626 |  953 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  954 | `				return SXERR_ABORT;` |
|       - |  955 | `			}` |
|    1626 |  956 | `			if( rc != SXERR_EMPTY ){` |
|    1624 |  957 | `				++iCons;` |
|     811 |  958 | `			}` |
|       - |  959 | `		}` |
|       - |  960 | `		/* Invalidate the previously used constant */` |
|    1712 |  961 | `		pObj = 0;` |
|       2 |  962 | `	}/*for(;;)*/` |
|   15138 |  963 | `	if( iCons > 1 ){` |
|       - |  964 | `		/* Concatenate all compiled constants */` |
|    1286 |  965 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     642 |  966 | `	}` |
|       - |  967 | `	/* Node successfully compiled */` |
|   15138 |  968 | `	return SXRET_OK;` |
|    7682 |  969 |  |
|       - |  970 | `/*` |
|       - |  971 | ` * Compile a double quoted string.` |
|       - |  972 | ` *  See the block-comment above for more information.` |
|       - |  973 | ` */` |
|   15334 |  974 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  975 |  |
|       - |  976 | `	sxi32 rc;` |
|   15336 |  977 | `	rc = GenStateCompileString(&(*pGen));` |
|    7667 |  978 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  979 | `	/* Compilation result */` |
|   15336 |  980 | `	return rc;` |
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
|   15766 | 1012 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   15768 | 1023 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - | 1024 | `	/* Compile the expression*/` |
|   15768 | 1025 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - | 1026 | `	/* Restore token stream */` |
|   15768 | 1027 | `	RE_SWAP_DELIMITER(pGen);` |
|   15768 | 1028 | `	return rc;` |
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
|   23000 | 1067 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 | 1068 |  |
|       - | 1069 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - | 1070 | `	SyToken *pKey,*pCur;` |
|   23002 | 1071 | `	sxi32 iEmitRef = 0;` |
|   23002 | 1072 | `	sxi32 nPair = 0;` |
|       - | 1073 | `	sxi32 iNest;` |
|       - | 1074 | `	sxi32 rc;` |
|   23002 | 1075 | `	xValidator = 0;` |
|   18729 | 1076 | `	for(;;){` |
|       - | 1077 | `		/* Jump leading commas */` |
|   42386 | 1078 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    4928 | 1079 | `			pGen->pIn++;` |
|       2 | 1080 | `		}` |
|   37460 | 1081 | `		pCur = pGen->pIn;` |
|   37460 | 1082 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - | 1083 | `			/* No more entry to process */` |
|   22990 | 1084 | `			break;` |
|       - | 1085 | `		}` |
|   14472 | 1086 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 | 1087 | `			continue;` |
|       - | 1088 | `		}` |
|       - | 1089 | `		/* Compile the key if available */` |
|   14472 | 1090 | `		pKey = pCur;` |
|   14472 | 1091 | `		iNest = 0;` |
|   40126 | 1092 | `		while( pCur < pGen->pIn ){` |
|   26846 | 1093 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1192 | 1094 | `				break;` |
|       - | 1095 | `			}` |
|   25656 | 1096 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 | 1097 | `				iNest++;` |
|   25618 | 1098 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - | 1099 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - | 1100 | `				 * parser will shortly detect any syntax error.` |
|       - | 1101 | `				 */` |
|      78 | 1102 | `				iNest--;` |
|      38 | 1103 | `			}` |
|   25656 | 1104 | `			pCur++;` |
|       2 | 1105 | `		}` |
|   14472 | 1106 | `		rc = SXERR_EMPTY;` |
|   14472 | 1107 | `		if( pCur < pGen->pIn ){` |
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
|   13872 | 1123 | `		}else if( pKey == pCur ){` |
|       - | 1124 | `			/* Key is omitted,emit a warning */` |
|     ! 0 | 1125 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 | 1126 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 | 1127 | `		}else{` |
|       - | 1128 | `			/* Reset back the cursor and point to the entry value */` |
|   13282 | 1129 | `			pCur = pKey;` |
|       - | 1130 | `		}` |
|   14462 | 1131 | `		if( rc == SXERR_EMPTY ){` |
|       - | 1132 | `			/* No available key,load NULL */` |
|   13284 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    6641 | 1134 | `		}` |
|   14462 | 1135 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   14460 | 1150 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   14460 | 1151 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1152 | `			return SXERR_ABORT;` |
|       - | 1153 | `		}` |
|   14460 | 1154 | `		if( iEmitRef ){` |
|       - | 1155 | `			/* Emit the load reference instruction */` |
|      32 | 1156 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 | 1157 | `		}` |
|   14460 | 1158 | `		xValidator = 0;` |
|   14460 | 1159 | `		iEmitRef = 0;` |
|   14460 | 1160 | `		nPair++;` |
|       2 | 1161 | `	}` |
|       - | 1162 | `	/* Emit the load map instruction */` |
|   22990 | 1163 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - | 1164 | `	/* Node successfully compiled */` |
|   22990 | 1165 | `	return SXRET_OK;` |
|   11502 | 1166 |  |
|       - | 1167 | `/*` |
|       - | 1168 | ` * Compile the 'array' language construct.` |
|       - | 1169 | ` *	 According to the PHP language reference manual` |
|       - | 1170 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - | 1171 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - | 1172 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - | 1173 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - | 1174 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - | 1175 | ` */` |
|   22764 | 1176 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1177 |  |
|       - | 1178 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   22766 | 1179 | `	pGen->pIn += 2;` |
|   22766 | 1180 | `	pGen->pEnd--;` |
|   11382 | 1181 | `	SXUNUSED(iCompileFlag);` |
|   22766 | 1182 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1183 |  |
|       - | 1184 | `/*` |
|       - | 1185 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - | 1186 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - | 1187 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - | 1188 | ` */` |
|     236 | 1189 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1190 |  |
|       - | 1191 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     238 | 1192 | `	pGen->pIn++;` |
|     238 | 1193 | `	pGen->pEnd--;` |
|     118 | 1194 | `	SXUNUSED(iCompileFlag);` |
|     238 | 1195 | `	return GenStateCompileArrayBody(pGen);` |
|       2 | 1196 |  |
|       - | 1197 | `/*` |
|       - | 1198 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - | 1199 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - | 1200 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - | 1201 | ` * error message.` |
|       - | 1202 | ` * See the routine responible of compiling the list language construct` |
|       - | 1203 | ` * for more inforation.` |
|       - | 1204 | ` */` |
|     128 | 1205 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 1206 |  |
|     130 | 1207 | `	sxi32 rc = SXRET_OK;` |
|     130 | 1208 | `	if( pRoot->pOp ){` |
|     ! 0 | 1209 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 | 1210 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - | 1211 | `				/* Unexpected expression */` |
|     ! 0 | 1212 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1213 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 | 1214 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 | 1215 | `					rc = SXERR_INVALID;` |
|     ! 0 | 1216 | `				}` |
|     ! 0 | 1217 | `		}` |
|     130 | 1218 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 1219 | `		/* Unexpected expression */` |
|       5 | 1220 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 1221 | `			"list(): Expecting a variable not an expression");` |
|       5 | 1222 | `		if( rc != SXERR_ABORT ){` |
|       5 | 1223 | `			rc = SXERR_INVALID;` |
|       2 | 1224 | `		}` |
|       2 | 1225 | `	}` |
|     130 | 1226 | `	return rc;` |
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
|       - | 1242 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - | 1243 | `struct NestedListEntry {` |
|       - | 1244 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - | 1245 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - | 1246 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - | 1247 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - | 1248 | `};` |
|       - | 1249 | `/*` |
|       - | 1250 | ` * Shared body for list() and short list [...] compilation.` |
|       - | 1251 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - | 1252 | ` * the opening delimiter and before the closing delimiter.` |
|       - | 1253 | ` */` |
|      74 | 1254 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 | 1255 |  |
|       - | 1256 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - | 1257 | `	SyToken *pNext;` |
|       - | 1258 | `	sxi32 nExpr;` |
|       - | 1259 | `	sxi32 rc;` |
|      76 | 1260 | `	nExpr = 0;` |
|      76 | 1261 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 | 1262 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 | 1263 | `		if( pGen->pIn < pNext ){` |
|       - | 1264 | `			/* Check for nested list() */` |
|     144 | 1265 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 | 1266 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 1267 | `				/* Record this nested list for post-processing */` |
|       3 | 1268 | `				SyToken *pListEnd = 0;` |
|       3 | 1269 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 | 1270 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 | 1271 | `				}` |
|       3 | 1272 | `				if( pListEnd ){` |
|       - | 1273 | `					struct NestedListEntry sEntry;` |
|       3 | 1274 | `					sEntry.nIndex = nExpr;` |
|       3 | 1275 | `					sEntry.pStart = pGen->pIn;` |
|       3 | 1276 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 | 1277 | `					sEntry.isShort = 0;` |
|       3 | 1278 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 | 1279 | `				}` |
|       - | 1280 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 | 1281 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 | 1282 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 1283 | `				/* Nested short destructuring [...] */` |
|      13 | 1284 | `				SyToken *pBracketEnd = 0;` |
|      13 | 1285 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 | 1286 | `				if( pBracketEnd ){` |
|       - | 1287 | `					struct NestedListEntry sEntry;` |
|      13 | 1288 | `					sEntry.nIndex = nExpr;` |
|      13 | 1289 | `					sEntry.pStart = pGen->pIn;` |
|      13 | 1290 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 | 1291 | `					sEntry.isShort = 1;` |
|      13 | 1292 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 | 1293 | `				}` |
|       - | 1294 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 | 1295 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 | 1296 | `			}else{` |
|       - | 1297 | `				/* Compile the expression holding the variable */` |
|     130 | 1298 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 | 1299 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 1300 | `					SySetRelease(&sNested);` |
|     ! 0 | 1301 | `					return SXRET_OK;` |
|       - | 1302 | `				}` |
|       - | 1303 | `			}` |
|      73 | 1304 | `		}else{` |
|       - | 1305 | `			/* Empty entry,load NULL */` |
|      13 | 1306 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - | 1307 | `		}` |
|     156 | 1308 | `		nExpr++;` |
|       - | 1309 | `		/* Advance the stream cursor */` |
|     156 | 1310 | `		pGen->pIn = &pNext[1];` |
|       2 | 1311 | `	}` |
|       - | 1312 | `	/* Emit the LOAD_LIST instruction */` |
|      76 | 1313 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - | 1314 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - | 1315 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - | 1316 | `	 * at the corresponding index and recursively destructure it.` |
|       - | 1317 | `	 */` |
|      76 | 1318 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 | 1319 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - | 1320 | `		sxu32 i;` |
|      27 | 1321 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 | 1322 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 | 1323 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - | 1324 | `			ph7_value *pIdx;` |
|       - | 1325 | `			sxu32 nConstIdx;` |
|       - | 1326 | `			/* DUP the source array (it's on stack top) */` |
|      15 | 1327 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - | 1328 | `			/* Push the integer index for this nested entry */` |
|      15 | 1329 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 | 1330 | `			if( pIdx == 0 ){` |
|     ! 0 | 1331 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1332 | `				SySetRelease(&sNested);` |
|     ! 0 | 1333 | `				return SXERR_ABORT;` |
|       - | 1334 | `			}` |
|      15 | 1335 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 | 1336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - | 1337 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - | 1338 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - | 1339 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - | 1340 | `			 */` |
|      15 | 1341 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - | 1342 | `			/* Recursively compile the inner list */` |
|      15 | 1343 | `			pGen->pIn = apNested[i].pStart;` |
|      15 | 1344 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 | 1345 | `			if( apNested[i].isShort ){` |
|      13 | 1346 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 | 1347 | `			}else{` |
|       3 | 1348 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - | 1349 | `			}` |
|      15 | 1350 | `			pGen->pIn = pSavedIn;` |
|      15 | 1351 | `			pGen->pEnd = pSavedEnd;` |
|      15 | 1352 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 1353 | `				SySetRelease(&sNested);` |
|     ! 0 | 1354 | `				return SXERR_ABORT;` |
|       - | 1355 | `			}` |
|       - | 1356 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 | 1357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 | 1358 | `		}` |
|       6 | 1359 | `	}` |
|      76 | 1360 | `	SySetRelease(&sNested);` |
|       - | 1361 | `	/* Node successfully compiled */` |
|      76 | 1362 | `	return SXRET_OK;` |
|      39 | 1363 |  |
|      32 | 1364 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1365 |  |
|       - | 1366 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 | 1367 | `	pGen->pIn += 2;` |
|      34 | 1368 | `	pGen->pEnd--;` |
|      16 | 1369 | `	SXUNUSED(iCompileFlag);` |
|      34 | 1370 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1371 |  |
|      42 | 1372 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1373 |  |
|       - | 1374 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 | 1375 | `	pGen->pIn++;` |
|      44 | 1376 | `	pGen->pEnd--;` |
|      21 | 1377 | `	SXUNUSED(iCompileFlag);` |
|      44 | 1378 | `	return GenStateCompileListBody(pGen);` |
|       2 | 1379 |  |
|       - | 1380 | `/* Forward declarations */` |
|       - | 1381 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - | 1382 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - | 1383 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - | 1384 | `/*` |
|       - | 1385 | ` * Compile an annoynmous function or a closure.` |
|       - | 1386 | ` * According to the PHP language reference` |
|       - | 1387 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - | 1388 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - | 1389 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - | 1390 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - | 1391 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - | 1392 | ` *  Example Anonymous function variable assignment example` |
|       - | 1393 | ` * <?php` |
|       - | 1394 | ` * $greet = function($name)` |
|       - | 1395 | ` * {` |
|       - | 1396 | ` *    printf("Hello %s\r\n", $name);` |
|       - | 1397 | ` * };` |
|       - | 1398 | ` * $greet('World');` |
|       - | 1399 | ` * $greet('PHP');` |
|       - | 1400 | ` * ?>` |
|       - | 1401 | ` * Note that the implementation of annoynmous function and closure under` |
|       - | 1402 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - | 1403 | ` */` |
|     166 | 1404 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1405 |  |
|       - | 1406 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - | 1407 | `	char zName[512];         /* Unique lambda name */` |
|       - | 1408 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - | 1409 | `							  * one thread is allowed to compile the script.` |
|       - | 1410 | `						      */` |
|       - | 1411 | `	ph7_value *pObj;` |
|       - | 1412 | `	SyString sName;` |
|       - | 1413 | `	sxu32 nIdx;` |
|       - | 1414 | `	sxu32 nLen;` |
|       - | 1415 | `	sxi32 rc;` |
|       - | 1416 |  |
|     168 | 1417 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     168 | 1418 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 | 1419 | `		pGen->pIn++;` |
|     ! 0 | 1420 | `	}` |
|       - | 1421 | `	/* Reserve a constant for the lambda */` |
|     168 | 1422 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     168 | 1423 | `	if( pObj == 0 ){` |
|     ! 0 | 1424 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1425 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1426 | `		return SXERR_ABORT;` |
|       - | 1427 | `	}` |
|       - | 1428 | `	/* Generate a unique name */` |
|     168 | 1429 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - | 1430 | `	/* Make sure the generated name is unique */` |
|     168 | 1431 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 | 1432 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 | 1433 | `	}` |
|     168 | 1434 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     168 | 1435 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - | 1436 | `	/* Compile the lambda body */` |
|     168 | 1437 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     168 | 1438 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 1439 | `		return SXERR_ABORT;` |
|       - | 1440 | `	}` |
|     168 | 1441 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 1442 | `		/* Emit the load closure instruction */` |
|      14 | 1443 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       8 | 1444 | `	}else{` |
|       - | 1445 | `		/* Emit the load constant instruction */` |
|     156 | 1446 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1447 | `	}` |
|       - | 1448 | `	/* Node successfully compiled */` |
|     168 | 1449 | `	return SXRET_OK;` |
|      85 | 1450 |  |
|       - | 1451 | `/*` |
|       - | 1452 | ` * Compile a backtick quoted string.` |
|       - | 1453 | ` */` |
|       4 | 1454 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1455 |  |
|       - | 1456 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - | 1457 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - | 1458 | `	 */` |
|       7 | 1459 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - | 1460 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 | 1461 | `		ph7_lib_version()` |
|       - | 1462 | `		);` |
|       - | 1463 | `	/* Load NULL */` |
|       5 | 1464 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1465 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - | 1466 | `	/* Node successfully compiled */` |
|       5 | 1467 | `	return SXRET_OK;` |
|       1 | 1468 |  |
|       - | 1469 | `/*` |
|       - | 1470 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1471 | ` * construct.` |
|       - | 1472 | ` */` |
|      72 | 1473 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1474 |  |
|       - | 1475 | `	SyString *pName;` |
|       - | 1476 | `	sxu32 nKeyID;` |
|       - | 1477 | `	sxi32 rc;` |
|       - | 1478 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 | 1479 | `	pName = &pGen->pIn->sData;` |
|      74 | 1480 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 | 1481 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 | 1482 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 | 1483 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1484 | `		/* Compile arguments one after one */` |
|       9 | 1485 | `		pTmp = pGen->pEnd;` |
|       - | 1486 | `		/* Symisc eXtension to the PHP programming language:` |
|       - | 1487 | `		 * 'echo' can be used in the context of a function which` |
|       - | 1488 | `		 *  mean that the following expression is valid:` |
|       - | 1489 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - | 1490 | `		 */` |
|       9 | 1491 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 | 1492 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 | 1493 | `			if( pGen->pIn < pNext ){` |
|       9 | 1494 | `				pGen->pEnd = pNext;` |
|       9 | 1495 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 | 1496 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1497 | `					return SXERR_ABORT;` |
|       - | 1498 | `				}` |
|       9 | 1499 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1500 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1501 | `					 * without the overhead of a function call.` |
|       - | 1502 | `					 * This is a very powerful optimization that improve` |
|       - | 1503 | `					 * performance greatly.` |
|       - | 1504 | `					 */` |
|       9 | 1505 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 | 1506 | `				}` |
|       4 | 1507 | `			}` |
|       - | 1508 | `			/* Jump trailing commas */` |
|       9 | 1509 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1510 | `				pNext++;` |
|     ! 0 | 1511 | `			}` |
|       9 | 1512 | `			pGen->pIn = pNext;` |
|       1 | 1513 | `		}` |
|       - | 1514 | `		/* Restore token stream */` |
|       9 | 1515 | `		pGen->pEnd = pTmp;` |
|       5 | 1516 | `	}else{` |
|      66 | 1517 | `		sxi32 nArg = 0;` |
|      66 | 1518 | `		sxu32 nIdx = 0;` |
|      66 | 1519 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 | 1520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1521 | `			return SXERR_ABORT;` |
|      66 | 1522 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 | 1523 | `			nArg = 1;` |
|      32 | 1524 | `		}` |
|      66 | 1525 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1526 | `			ph7_value *pObj;` |
|       - | 1527 | `			/* Emit the call instruction */` |
|      20 | 1528 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1529 | `			if( pObj == 0 ){` |
|     ! 0 | 1530 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1531 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1532 | `				return SXERR_ABORT;` |
|       - | 1533 | `			}` |
|      20 | 1534 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1535 | `			/* Install in the literal table */` |
|      20 | 1536 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 | 1537 | `		}` |
|       - | 1538 | `		/* Emit the call instruction */` |
|      66 | 1539 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 | 1540 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - | 1541 | `	}` |
|       - | 1542 | `	/* Node successfully compiled */` |
|      74 | 1543 | `	return SXRET_OK;` |
|      38 | 1544 |  |
|       - | 1545 | `/*` |
|       - | 1546 | ` * Compile a node holding a variable declaration.` |
|       - | 1547 | ` * According to the PHP language reference` |
|       - | 1548 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1549 | ` *  The variable name is case-sensitive.` |
|       - | 1550 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1551 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1552 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1553 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1554 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1555 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1556 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1557 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1558 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1559 | ` *  the chapter on Expressions.` |
|       - | 1560 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1561 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1562 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1563 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1564 | ` *  is being assigned (the source variable).` |
|       - | 1565 | ` */` |
|  767534 | 1566 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1567 |  |
|  767536 | 1568 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1569 | `	sxi32 iVv;` |
|       - | 1570 | `	sxi32 iP1;` |
|       - | 1571 | `	void *p3;` |
|       - | 1572 | `	sxi32 rc;` |
|  767536 | 1573 | `	iVv = -1; /* Variable variable counter */` |
| 1535082 | 1574 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  767548 | 1575 | `		pGen->pIn++;` |
|  767548 | 1576 | `		iVv++;` |
|       2 | 1577 | `	}` |
|  767536 | 1578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1579 | `		/* Invalid variable name */` |
|     ! 0 | 1580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 | 1581 | `		if( rc == SXERR_ABORT ){` |
|       - | 1582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1583 | `			return SXERR_ABORT;` |
|       - | 1584 | `		}` |
|     ! 0 | 1585 | `		return SXRET_OK;` |
|       - | 1586 | `	}` |
|  767536 | 1587 | `	p3  = 0;` |
|  767536 | 1588 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1589 | `		/* Dynamic variable creation */` |
|      18 | 1590 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 | 1591 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 | 1592 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1593 | `			/* Empty expression */` |
|       3 | 1594 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 | 1595 | `			return SXRET_OK;` |
|       - | 1596 | `		}` |
|       - | 1597 | `		/* Compile the expression holding the variable name */` |
|      16 | 1598 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 | 1599 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1600 | `			return SXERR_ABORT;` |
|      16 | 1601 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1602 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 | 1603 | `			return SXRET_OK;` |
|       - | 1604 | `		}` |
|       7 | 1605 | `	}else{` |
|       - | 1606 | `		SyHashEntry *pEntry;` |
|       - | 1607 | `		SyString *pName;` |
|  767520 | 1608 | `		char *zName = 0;` |
|       - | 1609 | `		/* Extract variable name */` |
|  767520 | 1610 | `		pName = &pGen->pIn->sData;` |
|       - | 1611 | `		/* Advance the stream cursor */` |
|  767520 | 1612 | `		pGen->pIn++;` |
|  767520 | 1613 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  767520 | 1614 | `		if( pEntry == 0 ){` |
|       - | 1615 | `			/* Duplicate name */` |
|  110348 | 1616 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  110348 | 1617 | `			if( zName == 0 ){` |
|     ! 0 | 1618 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1619 | `				return SXERR_ABORT;` |
|       - | 1620 | `			}` |
|       - | 1621 | `			/* Install in the hashtable */` |
|  110348 | 1622 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   55175 | 1623 | `		}else{` |
|       - | 1624 | `			/* Name already available */` |
|  657174 | 1625 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1626 | `		}` |
|  767520 | 1627 | `		p3 = (void *)zName;` |
|       - | 1628 | `	}` |
|  767532 | 1629 | `	iP1 = 0;` |
|  767532 | 1630 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  295176 | 1631 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1632 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  289162 | 1633 | `			iP1 = 1;` |
|  144580 | 1634 | `		}` |
|  147587 | 1635 | `	}` |
|       - | 1636 | `	/* Emit the load instruction */` |
|  767532 | 1637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  767544 | 1638 | `	while( iVv > 0 ){` |
|      13 | 1639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 | 1640 | `		iVv--;` |
|       1 | 1641 | `	}` |
|       - | 1642 | `	/* Node successfully compiled */` |
|  767532 | 1643 | `	return SXRET_OK;` |
|  383769 | 1644 |  |
|       - | 1645 | `/*` |
|       - | 1646 | ` * Load a literal.` |
|       - | 1647 | ` */` |
|  514860 | 1648 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 | 1649 |  |
|  514862 | 1650 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1651 | `	ph7_value *pObj;` |
|       - | 1652 | `	SyString *pStr;` |
|       - | 1653 | `	sxu32 nIdx;` |
|       - | 1654 | `	/* Extract token value */` |
|  514862 | 1655 | `	pStr = &pToken->sData;` |
|       - | 1656 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  514862 | 1657 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   93498 | 1658 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1659 | `			/* NULL constant are always indexed at 0 */` |
|   39748 | 1660 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   39748 | 1661 | `			return SXRET_OK;` |
|   53752 | 1662 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1663 | `			/* TRUE constant are always indexed at 1 */` |
|     488 | 1664 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     488 | 1665 | `			return SXRET_OK;` |
|       2 | 1666 | `		}` |
|  488611 | 1667 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   81226 | 1668 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1669 | `			/* FALSE constant are always indexed at 2 */` |
|   34696 | 1670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   34696 | 1671 | `			return SXRET_OK;` |
|  422549 | 1672 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   71754 | 1673 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1674 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5256 | 1675 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5256 | 1676 | `			if( pObj == 0 ){` |
|     ! 0 | 1677 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1678 | `				return SXERR_ABORT;` |
|       - | 1679 | `			}` |
|    5256 | 1680 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1681 | `			/* Emit the load constant instruction */` |
|    5256 | 1682 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5256 | 1683 | `			return SXRET_OK;` |
|  394666 | 1684 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   26496 | 1685 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1686 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 | 1687 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 | 1688 | `			if( pObj == 0 ){` |
|     ! 0 | 1689 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1690 | `				return SXERR_ABORT;` |
|       - | 1691 | `			}` |
|       7 | 1692 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1693 | `				SyString sNs;` |
|       7 | 1694 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1695 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1696 | `			}else{` |
|     ! 0 | 1697 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1698 | `			}` |
|       7 | 1699 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 | 1700 | `			return SXRET_OK;` |
|  393862 | 1701 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11092 | 1702 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  388310 | 1703 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   13814 | 1704 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 | 1705 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1706 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 | 1707 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - | 1708 | `				/* Point to the upper block */` |
|      11 | 1709 | `				pBlock = pBlock->pParent;` |
|       1 | 1710 | `			}` |
|      11 | 1711 | `			if( pBlock == 0 ){` |
|       - | 1712 | `				/* Called in the global scope,load NULL */` |
|       5 | 1713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1714 | `			}else{` |
|       - | 1715 | `				/* Extract the target function/method */` |
|       7 | 1716 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 | 1717 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - | 1718 | `					/* Not a class method,Load null */` |
|       3 | 1719 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 | 1720 | `				}else{` |
|       5 | 1721 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 | 1722 | `					if( pObj == 0 ){` |
|     ! 0 | 1723 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1724 | `						return SXERR_ABORT;` |
|       - | 1725 | `					}` |
|       5 | 1726 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - | 1727 | `					/* Emit the load constant instruction */` |
|       5 | 1728 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1729 | `				}` |
|       - | 1730 | `			}` |
|      11 | 1731 | `			return SXRET_OK;` |
|       - | 1732 | `	}` |
|       - | 1733 | `	/* Query literal table */` |
|  434666 | 1734 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - | 1735 | `		ph7_value *pLitObj;` |
|       - | 1736 | `		/* Unknown literal,install it in the literal table */` |
|  203348 | 1737 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  203348 | 1738 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1739 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1740 | `			return SXERR_ABORT;` |
|       - | 1741 | `		}` |
|  203348 | 1742 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  203348 | 1743 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  101673 | 1744 | `	}` |
|       - | 1745 | `	/* Emit the load constant instruction */` |
|  434666 | 1746 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  434666 | 1747 | `	return SXRET_OK;` |
|  257432 | 1748 |  |
|       - | 1749 | `/*` |
|       - | 1750 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1751 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1752 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1753 | ` * Otherwise, load the simple literal directly.` |
|       - | 1754 | ` */` |
|  514884 | 1755 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 | 1756 |  |
|       - | 1757 | `	sxi32 rc;` |
|  514886 | 1758 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1759 | `		return SXRET_OK;` |
|       - | 1760 | `	}` |
|       - | 1761 | `	/* Check if this is a multi-token namespace path */` |
|  514886 | 1762 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1763 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      26 | 1764 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      26 | 1765 | `		int isAbsolute = 0;` |
|      26 | 1766 | `		SyBlobReset(pWorker);` |
|       - | 1767 | `		/* Check for leading backslash (absolute path) */` |
|      26 | 1768 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      24 | 1769 | `			isAbsolute = 1;` |
|      24 | 1770 | `			pGen->pIn++; /* Skip leading backslash */` |
|      11 | 1771 | `		}` |
|       - | 1772 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      26 | 1773 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 | 1774 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 | 1775 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 | 1776 | `		}` |
|       - | 1777 | `		/* Collect all path components */` |
|     102 | 1778 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     102 | 1779 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      40 | 1780 | `				SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1781 | `			}else{` |
|      64 | 1782 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1783 | `			}` |
|     102 | 1784 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      26 | 1785 | `				pGen->pIn++;` |
|      26 | 1786 | `				break;` |
|       - | 1787 | `			}` |
|      78 | 1788 | `			pGen->pIn++;` |
|       2 | 1789 | `		}` |
|      26 | 1790 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1791 | `			ph7_value *pObj;` |
|       - | 1792 | `			SyString sPath;` |
|       - | 1793 | `			sxu32 nIdx;` |
|      26 | 1794 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1795 | `			/* Install in the literal table */` |
|      26 | 1796 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      13 | 1797 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1798 | `				if( pObj == 0 ){` |
|     ! 0 | 1799 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1800 | `					return SXERR_ABORT;` |
|       - | 1801 | `				}` |
|      13 | 1802 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      13 | 1803 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       6 | 1804 | `			}` |
|       - | 1805 | `			/* Emit the load constant instruction.` |
|       - | 1806 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      26 | 1807 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      26 | 1808 | `			return SXRET_OK;` |
|       - | 1809 | `		}` |
|     ! 0 | 1810 | `	}` |
|       - | 1811 | `	/* Single-token literal: load directly */` |
|  514862 | 1812 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  514862 | 1813 | `	return rc;` |
|  257444 | 1814 |  |
|       - | 1815 | `/*` |
|       - | 1816 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1817 | ` */` |
|  514884 | 1818 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 | 1819 |  |
|       - | 1820 | `	sxi32 rc;` |
|  514886 | 1821 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  514886 | 1822 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1823 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1824 | `		return rc;` |
|       - | 1825 | `	}` |
|       - | 1826 | `	/* Node successfully compiled */` |
|  514886 | 1827 | `	return SXRET_OK;` |
|  257444 | 1828 |  |
|       - | 1829 | `/*` |
|       - | 1830 | ` * Recover from a compile-time error. In other words synchronize` |
|       - | 1831 | ` * the token stream cursor with the first semi-colon seen.` |
|       - | 1832 | ` */` |
|       8 | 1833 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 | 1834 |  |
|       - | 1835 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 | 1836 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 | 1837 | `		pGen->pIn++;` |
|       1 | 1838 | `	}` |
|       9 | 1839 | `	return SXRET_OK;` |
|       1 | 1840 |  |
|       - | 1841 | `/*` |
|       - | 1842 | ` * Check if the given identifier name is reserved or not.` |
|       - | 1843 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - | 1844 | ` */` |
|      56 | 1845 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 | 1846 |  |
|      58 | 1847 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 | 1848 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1849 | `			return TRUE;` |
|      24 | 1850 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 | 1851 | `			return TRUE;` |
|       2 | 1852 | `		}` |
|      43 | 1853 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 | 1854 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 | 1855 | `			return TRUE;` |
|       - | 1856 | `		}` |
|     ! 0 | 1857 | `	}` |
|       - | 1858 | `	/* Not a reserved constant */` |
|      50 | 1859 | `	return FALSE;` |
|      30 | 1860 |  |
|       - | 1861 | `/*` |
|       - | 1862 | ` * Compile the 'const' statement.` |
|       - | 1863 | ` * According to the PHP language reference` |
|       - | 1864 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - | 1865 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - | 1866 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - | 1867 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - | 1868 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1869 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - | 1870 | ` *  Syntax` |
|       - | 1871 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - | 1872 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - | 1873 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - | 1874 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - | 1875 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - | 1876 | ` *  to get a list of all defined constants.` |
|       - | 1877 | ` *` |
|       - | 1878 | ` * Symisc eXtension.` |
|       - | 1879 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - | 1880 | ` *  would allow only simple scalar value.` |
|       - | 1881 | ` *  Example` |
|       - | 1882 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 1883 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 1884 | ` */` |
|      32 | 1885 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 | 1886 |  |
|       - | 1887 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 | 1888 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1889 | `	SyString *pName;` |
|       - | 1890 | `	sxi32 rc;` |
|      34 | 1891 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 | 1892 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 1893 | `		/* Invalid constant name */` |
|       7 | 1894 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 | 1895 | `		if( rc == SXERR_ABORT ){` |
|       - | 1896 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1897 | `			return SXERR_ABORT;` |
|       - | 1898 | `		}` |
|       7 | 1899 | `		goto Synchronize;` |
|       - | 1900 | `	}` |
|       - | 1901 | `	/* Peek constant name */` |
|      28 | 1902 | `	pName = &pGen->pIn->sData;` |
|       - | 1903 | `	/* Make sure the constant name isn't reserved */` |
|      28 | 1904 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 1905 | `		/* Reserved constant */` |
|       9 | 1906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 | 1907 | `		if( rc == SXERR_ABORT ){` |
|       - | 1908 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1909 | `			return SXERR_ABORT;` |
|       - | 1910 | `		}` |
|       9 | 1911 | `		goto Synchronize;` |
|       - | 1912 | `	}` |
|      20 | 1913 | `	pGen->pIn++;` |
|      20 | 1914 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 1915 | `		/* Invalid statement*/` |
|       5 | 1916 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 | 1917 | `		if( rc == SXERR_ABORT ){` |
|       - | 1918 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1919 | `			return SXERR_ABORT;` |
|       - | 1920 | `		}` |
|       5 | 1921 | `		goto Synchronize;` |
|       - | 1922 | `	}` |
|      15 | 1923 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - | 1924 | `	/* Allocate a new constant value container */` |
|      15 | 1925 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 | 1926 | `	if( pConsCode == 0 ){` |
|     ! 0 | 1927 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1928 | `		return SXERR_ABORT;` |
|       - | 1929 | `	}` |
|      15 | 1930 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 1931 | `	/* Swap bytecode container */` |
|      15 | 1932 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 | 1933 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - | 1934 | `	/* Compile constant value */` |
|      15 | 1935 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 1936 | `	/* Emit the done instruction */` |
|      15 | 1937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 | 1938 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 | 1939 | `	if( rc == SXERR_ABORT ){` |
|       - | 1940 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 1941 | `		return SXERR_ABORT;` |
|       - | 1942 | `	}` |
|      15 | 1943 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - | 1944 | `	/* Register the constant with namespace-qualified name */` |
|       - | 1945 | `	{` |
|       - | 1946 | `		SyBlob sFQN;` |
|       - | 1947 | `		SyString sFQNStr;` |
|      15 | 1948 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 | 1949 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 | 1950 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 | 1951 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 | 1952 | `		SyBlobRelease(&sFQN);` |
|       - | 1953 | `	}` |
|      15 | 1954 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1955 | `		SySetRelease(pConsCode);` |
|     ! 0 | 1956 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 | 1957 | `	}` |
|      15 | 1958 | `	return SXRET_OK;` |
|       9 | 1959 | `Synchronize:` |
|       - | 1960 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 | 1961 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 | 1962 | `		pGen->pIn++;` |
|       1 | 1963 | `	}` |
|      19 | 1964 | `	return SXRET_OK;` |
|      18 | 1965 |  |
|       - | 1966 | `/*` |
|       - | 1967 | ` * Compile the 'continue' statement.` |
|       - | 1968 | ` * According to the PHP language reference` |
|       - | 1969 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - | 1970 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - | 1971 | ` *  iteration.` |
|       - | 1972 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - | 1973 | ` *  the purposes of continue.` |
|       - | 1974 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - | 1975 | ` *  of enclosing loops it should skip to the end of.` |
|       - | 1976 | ` *  Note:` |
|       - | 1977 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - | 1978 | ` */` |
|       - | 1979 | `/*` |
|       - | 1980 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - | 1981 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - | 1982 | ` * break/continue crosses a try boundary.` |
|       - | 1983 | ` *` |
|       - | 1984 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - | 1985 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - | 1986 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - | 1987 | ` */` |
|    2746 | 1988 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 | 1989 |  |
|    2748 | 1990 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   16038 | 1991 | `	while( pBlock && pBlock != pTarget ){` |
|   13292 | 1992 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 | 1993 | `			if( pBlock->pUserData ){` |
|       - | 1994 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 | 1995 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 | 1996 | `			}else{` |
|       - | 1997 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - | 1998 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - | 1999 | `				 * exception context from a sub-execution.` |
|       - | 2000 | `				 */` |
|     ! 0 | 2001 | `				break;` |
|       - | 2002 | `			}` |
|       1 | 2003 | `		}` |
|   13292 | 2004 | `		pBlock = pBlock->pParent;` |
|       2 | 2005 | `	}` |
|    2748 | 2006 |  |
|    2666 | 2007 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 | 2008 |  |
|       - | 2009 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2010 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2011 | `	sxu32 nLineLocal;` |
|       - | 2012 | `	sxi32 rc;` |
|    2668 | 2013 | `	nLineLocal = pGen->pIn->nLine;` |
|    2668 | 2014 | `	iLevel = 0;` |
|       - | 2015 | `	/* Jump the 'continue' keyword */` |
|    2668 | 2016 | `	pGen->pIn++;` |
|    2668 | 2017 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2018 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2019 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2020 | `		 */` |
|      12 | 2021 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2022 | `		if( iLevel < 2 ){` |
|     ! 0 | 2023 | `			iLevel = 0;` |
|     ! 0 | 2024 | `		}` |
|      12 | 2025 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2026 | `	}` |
|       - | 2027 | `	/* Point to the target loop */` |
|    2668 | 2028 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2668 | 2029 | `	if( pLoop == 0 ){` |
|       - | 2030 | `		/* Illegal continue */` |
|      11 | 2031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 | 2032 | `		if( rc == SXERR_ABORT ){` |
|       - | 2033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2034 | `			return SXERR_ABORT;` |
|       - | 2035 | `		}` |
|       6 | 2036 | `	}else{` |
|    2658 | 2037 | `		sxu32 nInstrIdx = 0;` |
|       - | 2038 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2658 | 2039 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2658 | 2040 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - | 2041 | `			/* According to the PHP language reference manual` |
|       - | 2042 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - | 2043 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - | 2044 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - | 2045 | `			 */` |
|       5 | 2046 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 | 2047 | `			if( rc == SXRET_OK ){` |
|       5 | 2048 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 | 2049 | `			}` |
|       3 | 2050 | `		}else{` |
|       - | 2051 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2654 | 2052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2654 | 2053 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - | 2054 | `				JumpFixup sJumpFix;` |
|       - | 2055 | `				/* Post-continue */` |
|       9 | 2056 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|       9 | 2057 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|       9 | 2058 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       4 | 2059 | `			}` |
|       - | 2060 | `		}` |
|       - | 2061 | `	}` |
|    2668 | 2062 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2063 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2064 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 | 2065 | `	}` |
|       - | 2066 | `	/* Statement successfully compiled */` |
|    2668 | 2067 | `	return SXRET_OK;` |
|    1335 | 2068 |  |
|       - | 2069 | `/*` |
|       - | 2070 | ` * Compile the 'break' statement.` |
|       - | 2071 | ` * According to the PHP language reference` |
|       - | 2072 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - | 2073 | ` *  structure.` |
|       - | 2074 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - | 2075 | ` *  enclosing structures are to be broken out of.` |
|       - | 2076 | ` */` |
|     106 | 2077 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 | 2078 |  |
|       - | 2079 | `	GenBlock *pLoop; /* Target loop */` |
|       - | 2080 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - | 2081 | `	sxi32 rc;` |
|     108 | 2082 | `	iLevel = 0;` |
|       - | 2083 | `	/* Jump the 'break' keyword */` |
|     108 | 2084 | `	pGen->pIn++;` |
|     108 | 2085 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - | 2086 | `		/* optional numeric argument which tells us how many levels` |
|       - | 2087 | `		 * of enclosing loops we should skip to the end of.` |
|       - | 2088 | `		 */` |
|      12 | 2089 | `		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);` |
|      12 | 2090 | `		if( iLevel < 2 ){` |
|     ! 0 | 2091 | `			iLevel = 0;` |
|     ! 0 | 2092 | `		}` |
|      12 | 2093 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       5 | 2094 | `	}` |
|       - | 2095 | `	/* Extract the target loop */` |
|     108 | 2096 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     108 | 2097 | `	if( pLoop == 0 ){` |
|       - | 2098 | `		/* Illegal break */` |
|      17 | 2099 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 | 2100 | `		if( rc == SXERR_ABORT ){` |
|       - | 2101 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2102 | `			return SXERR_ABORT;` |
|       - | 2103 | `		}` |
|       9 | 2104 | `	}else{` |
|       - | 2105 | `		sxu32 nInstrIdx;` |
|       - | 2106 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      92 | 2107 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      92 | 2108 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      92 | 2109 | `		if( rc == SXRET_OK ){` |
|       - | 2110 | `			/* Fix the jump later when the jump destination is resolved */` |
|      92 | 2111 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      45 | 2112 | `		}` |
|       - | 2113 | `	}` |
|     108 | 2114 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2115 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 | 2116 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 | 2117 | `	}` |
|       - | 2118 | `	/* Statement successfully compiled */` |
|     108 | 2119 | `	return SXRET_OK;` |
|      55 | 2120 |  |
|       - | 2121 | `/*` |
|       - | 2122 | ` * Compile or record a label.` |
|       - | 2123 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - | 2124 | ` * Example` |
|       - | 2125 | ` *  goto LABEL;` |
|       - | 2126 | ` *   echo 'Foo';` |
|       - | 2127 | ` *  LABEL:` |
|       - | 2128 | ` *   echo 'Bar';` |
|       - | 2129 | ` */` |
|     112 | 2130 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 | 2131 |  |
|       - | 2132 | `	GenBlock *pBlock;` |
|       - | 2133 | `	Label sLabel;` |
|       - | 2134 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 | 2135 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 | 2136 | `	if( pBlock ){` |
|       - | 2137 | `		sxi32 rc;` |
|       7 | 2138 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 | 2139 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 | 2140 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2141 | `			return SXERR_ABORT;` |
|       - | 2142 | `		}` |
|       3 | 2143 | `	}else{` |
|     110 | 2144 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2145 | `		char *zDup;` |
|       - | 2146 | `		/* Initialize label fields */` |
|     110 | 2147 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - | 2148 | `		/* Duplicate label name */` |
|     110 | 2149 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 | 2150 | `		if( zDup == 0 ){` |
|     ! 0 | 2151 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2152 | `			return SXERR_ABORT;` |
|       - | 2153 | `		}` |
|     110 | 2154 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 | 2155 | `		sLabel.bRef  = FALSE;` |
|     110 | 2156 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 | 2157 | `		pBlock = pGen->pCurrent;` |
|     218 | 2158 | `		while( pBlock ){` |
|     130 | 2159 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 | 2160 | `				break;` |
|       - | 2161 | `			}` |
|       - | 2162 | `			/* Point to the upper block */` |
|     110 | 2163 | `			pBlock = pBlock->pParent;` |
|       2 | 2164 | `		}` |
|     110 | 2165 | `		if( pBlock ){` |
|      22 | 2166 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 | 2167 | `		}else{` |
|      90 | 2168 | `			sLabel.pFunc = 0;` |
|       - | 2169 | `		}` |
|       - | 2170 | `		/* Insert in label set */` |
|     110 | 2171 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - | 2172 | `	}` |
|     114 | 2173 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 | 2174 | `	return SXRET_OK;` |
|      58 | 2175 |  |
|       - | 2176 | `/*` |
|       - | 2177 | ` * Compile the so hated 'goto' statement.` |
|       - | 2178 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - | 2179 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - | 2180 | ` * a compiler it has to do this.` |
|       - | 2181 | ` * According to the PHP language reference manual` |
|       - | 2182 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - | 2183 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - | 2184 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - | 2185 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - | 2186 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - | 2187 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - | 2188 | ` *   of a multi-level break` |
|       - | 2189 | ` */` |
|     152 | 2190 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 | 2191 |  |
|       - | 2192 | `	JumpFixup sJump;` |
|       - | 2193 | `	sxi32 rc;` |
|     154 | 2194 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 | 2195 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 2196 | `		/* Missing label */` |
|     ! 0 | 2197 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 | 2198 | `		if( rc == SXERR_ABORT ){` |
|       - | 2199 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2200 | `			return SXERR_ABORT;` |
|       - | 2201 | `		}` |
|     ! 0 | 2202 | `		return SXRET_OK;` |
|       - | 2203 | `	}` |
|     154 | 2204 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 | 2205 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 | 2206 | `		if( rc == SXERR_ABORT ){` |
|       - | 2207 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2208 | `			return SXERR_ABORT;` |
|       - | 2209 | `		}` |
|       3 | 2210 | `	}else{` |
|     150 | 2211 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - | 2212 | `		GenBlock *pBlock;` |
|       - | 2213 | `		char *zDup;` |
|       - | 2214 | `		/* Prepare the jump destination */` |
|     150 | 2215 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 | 2216 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - | 2217 | `		/* Duplicate label name */` |
|     150 | 2218 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 | 2219 | `		if( zDup == 0 ){` |
|     ! 0 | 2220 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 2221 | `			return SXERR_ABORT;` |
|       - | 2222 | `		}` |
|     150 | 2223 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 | 2224 | `		pBlock = pGen->pCurrent;` |
|     312 | 2225 | `		while( pBlock ){` |
|     196 | 2226 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 | 2227 | `				break;` |
|       - | 2228 | `			}` |
|       - | 2229 | `			/* Point to the upper block */` |
|     164 | 2230 | `			pBlock = pBlock->pParent;` |
|       2 | 2231 | `		}` |
|     150 | 2232 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 | 2233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 | 2234 | `			if( rc == SXERR_ABORT ){` |
|       - | 2235 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2236 | `				return SXERR_ABORT;` |
|       - | 2237 | `			}` |
|       3 | 2238 | `		}` |
|     150 | 2239 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 | 2240 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 | 2241 | `		}else{` |
|     124 | 2242 | `			sJump.pFunc = 0;` |
|       - | 2243 | `		}` |
|       - | 2244 | `		/* Emit the unconditional jump */` |
|     150 | 2245 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 | 2246 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 | 2247 | `		}` |
|       - | 2248 | `	}` |
|     154 | 2249 | `	pGen->pIn++; /* Jump the label name */` |
|     154 | 2250 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 | 2251 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 | 2252 | `	}` |
|       - | 2253 | `	/* Statement successfully compiled */` |
|     154 | 2254 | `	return SXRET_OK;` |
|      78 | 2255 |  |
|       - | 2256 | `/*` |
|       - | 2257 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - | 2258 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - | 2259 | ` * failure.` |
|       - | 2260 | ` */` |
|      20 | 2261 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 | 2262 |  |
|       - | 2263 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - | 2264 | `	sxu32 nRawObj;` |
|      10 | 2265 | `	sxu32 nObjIdx;` |
|       - | 2266 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - | 2267 | `	 * a PHP block.` |
|       - | 2268 | `	 */` |
|      10 | 2269 | `Consume:` |
|      21 | 2270 | `	nRawObj = nObjIdx = 0;` |
|      21 | 2271 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 | 2272 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 | 2273 | `		if( pRawObj == 0 ){` |
|     ! 0 | 2274 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 2275 | `			return SXERR_ABORT;` |
|       - | 2276 | `		}` |
|       - | 2277 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 | 2278 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 | 2279 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 | 2280 | `		++nRawObj;` |
|     ! 0 | 2281 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 | 2282 | `	}` |
|      21 | 2283 | `	if( nRawObj > 0 ){` |
|       - | 2284 | `		/* Emit the consume instruction */` |
|     ! 0 | 2285 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 | 2286 | `	}` |
|      21 | 2287 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 | 2288 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - | 2289 | `		/* Reset the token set */` |
|     ! 0 | 2290 | `		SySetReset(pTokenSet);` |
|       - | 2291 | `		/* Tokenize input */` |
|     ! 0 | 2292 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 | 2293 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - | 2294 | `		/* Point to the fresh token stream */` |
|     ! 0 | 2295 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 | 2296 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - | 2297 | `		/* Advance the stream cursor */` |
|     ! 0 | 2298 | `		pGen->pRawIn++;` |
|       - | 2299 | `		/* TICKET 1433-011 */` |
|     ! 0 | 2300 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 2301 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 2302 | `			sxi32 rc;` |
|       - | 2303 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 | 2304 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 | 2305 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 | 2306 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 | 2307 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 2308 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2309 | `				return SXERR_ABORT;` |
|     ! 0 | 2310 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 | 2311 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 2312 | `			}` |
|     ! 0 | 2313 | `			goto Consume;` |
|       - | 2314 | `		}` |
|     ! 0 | 2315 | `	}else{` |
|       - | 2316 | `		/* No more chunks to process */` |
|      21 | 2317 | `		pGen->pIn = pGen->pEnd;` |
|      21 | 2318 | `		return SXERR_EOF;` |
|       - | 2319 | `	}` |
|     ! 0 | 2320 | `	return SXRET_OK;` |
|      11 | 2321 |  |
|       - | 2322 | `/*` |
|       - | 2323 | ` * Compile a PHP block.` |
|       - | 2324 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - | 2325 | ` * optionally delimited by braces {}.` |
|       - | 2326 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 2327 | ` * and this function takes care of generating the appropriate error` |
|       - | 2328 | ` * message.` |
|       - | 2329 | ` */` |
|  289418 | 2330 | `static sxi32 PH7_CompileBlock(` |
|       - | 2331 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 2332 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - | 2333 | `	)` |
|       2 | 2334 |  |
|       - | 2335 | `	sxi32 rc;` |
|       - | 2336 | `	sxu32 nLine;` |
|  289420 | 2337 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  288018 | 2338 | `		nLine = pGen->pIn->nLine;` |
|  288018 | 2339 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  288018 | 2340 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2341 | `			return SXERR_ABORT;` |
|       - | 2342 | `		}` |
|  288018 | 2343 | `		pGen->pIn++;` |
|       - | 2344 | `		/* Compile until we hit the closing braces '}' */` |
|  397601 | 2345 | `		for(;;){` |
|  795204 | 2346 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 | 2347 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 | 2348 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2349 | `			 	   return SXERR_ABORT;` |
|       - | 2350 | `				}` |
|      21 | 2351 | `				if( rc == SXERR_EOF ){` |
|       - | 2352 | `					/* No more token to process. Missing closing braces */` |
|      21 | 2353 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 | 2354 | `					break;` |
|       - | 2355 | `				}` |
|     ! 0 | 2356 | `			}` |
|  795184 | 2357 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - | 2358 | `				/* Closing braces found,break immediately*/` |
|  287998 | 2359 | `				pGen->pIn++;` |
|  287998 | 2360 | `				break;` |
|       - | 2361 | `			}` |
|       - | 2362 | `			/* Compile a single statement */` |
|  507188 | 2363 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  507188 | 2364 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2365 | `				return SXERR_ABORT;` |
|       - | 2366 | `			}` |
|       2 | 2367 | `		}` |
|  288018 | 2368 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  145412 | 2369 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 | 2370 | `		pGen->pIn++;` |
|     ! 0 | 2371 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 | 2372 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2373 | `			return SXERR_ABORT;` |
|       - | 2374 | `		}` |
|       - | 2375 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 | 2376 | `		for(;;){` |
|     ! 0 | 2377 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 2378 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 | 2379 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 | 2380 | `			 	   return SXERR_ABORT;` |
|       - | 2381 | `				}` |
|     ! 0 | 2382 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - | 2383 | `					/* No more token to process */` |
|     ! 0 | 2384 | `					if( rc == SXERR_EOF ){` |
|     ! 0 | 2385 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - | 2386 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 | 2387 | `					}` |
|     ! 0 | 2388 | `					break;` |
|       - | 2389 | `				}` |
|     ! 0 | 2390 | `			}` |
|     ! 0 | 2391 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 2392 | `				sxi32 nKwrd;` |
|       - | 2393 | `				/* Keyword found */` |
|     ! 0 | 2394 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 | 2395 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 | 2396 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - | 2397 | `						/* Delimiter keyword found,break */` |
|     ! 0 | 2398 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 | 2399 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 | 2400 | `						}` |
|     ! 0 | 2401 | `						break;` |
|       - | 2402 | `				}` |
|     ! 0 | 2403 | `			}` |
|       - | 2404 | `			/* Compile a single statement */` |
|     ! 0 | 2405 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 | 2406 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 2407 | `				return SXERR_ABORT;` |
|       - | 2408 | `			}` |
|     ! 0 | 2409 | `		}` |
|     ! 0 | 2410 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 | 2411 | `	}else{` |
|       - | 2412 | `		/* Compile a single statement */` |
|    1404 | 2413 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1404 | 2414 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2415 | `			return SXERR_ABORT;` |
|       - | 2416 | `		}` |
|       - | 2417 | `	}` |
|       - | 2418 | `	/* Jump trailing semi-colons ';' */` |
|  289420 | 2419 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2420 | `		pGen->pIn++;` |
|     ! 0 | 2421 | `	}` |
|  289420 | 2422 | `	return SXRET_OK;` |
|  144711 | 2423 |  |
|       - | 2424 | `/*` |
|       - | 2425 | ` * Compile the gentle 'while' statement.` |
|       - | 2426 | ` * According to the PHP language reference` |
|       - | 2427 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - | 2428 | ` *  The basic form of a while statement is:` |
|       - | 2429 | ` *  while (expr)` |
|       - | 2430 | ` *   statement` |
|       - | 2431 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - | 2432 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - | 2433 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - | 2434 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - | 2435 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - | 2436 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - | 2437 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - | 2438 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - | 2439 | ` *  while (expr):` |
|       - | 2440 | ` *    statement` |
|       - | 2441 | ` *   endwhile;` |
|       - | 2442 | ` */` |
|   10606 | 2443 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 | 2444 |  |
|   10608 | 2445 | `	GenBlock *pWhileBlock = 0;` |
|   10608 | 2446 | `	SyToken *pTmp,*pEnd = 0;` |
|       - | 2447 | `	sxu32 nFalseJump;` |
|       - | 2448 | `	sxu32 nLine;` |
|       - | 2449 | `	sxi32 rc;` |
|   10608 | 2450 | `	nLine = pGen->pIn->nLine;` |
|       - | 2451 | `	/* Jump the 'while' keyword */` |
|   10608 | 2452 | `	pGen->pIn++;` |
|   10608 | 2453 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2454 | `		/* Syntax error */` |
|     ! 0 | 2455 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2456 | `		if( rc == SXERR_ABORT ){` |
|       - | 2457 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2458 | `			return SXERR_ABORT;` |
|       - | 2459 | `		}` |
|     ! 0 | 2460 | `		goto Synchronize;` |
|       - | 2461 | `	}` |
|       - | 2462 | `	/* Jump the left parenthesis '(' */` |
|   10608 | 2463 | `	pGen->pIn++;` |
|       - | 2464 | `	/* Create the loop block */` |
|   10608 | 2465 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   10608 | 2466 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2467 | `		return SXERR_ABORT;` |
|       - | 2468 | `	}` |
|       - | 2469 | `	/* Delimit the condition */` |
|   10608 | 2470 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10608 | 2471 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2472 | `		/* Empty expression */` |
|       3 | 2473 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 | 2474 | `		if( rc == SXERR_ABORT ){` |
|       - | 2475 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2476 | `			return SXERR_ABORT;` |
|       - | 2477 | `		}` |
|       1 | 2478 | `	}` |
|       - | 2479 | `	/* Swap token streams */` |
|   10608 | 2480 | `	pTmp = pGen->pEnd;` |
|   10608 | 2481 | `	pGen->pEnd = pEnd;` |
|       - | 2482 | `	/* Compile the expression */` |
|   10608 | 2483 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10608 | 2484 | `	if( rc == SXERR_ABORT ){` |
|       - | 2485 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2486 | `		return SXERR_ABORT;` |
|       - | 2487 | `	}` |
|       - | 2488 | `	/* Update token stream */` |
|   10608 | 2489 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2491 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2492 | `			return SXERR_ABORT;` |
|       - | 2493 | `		}` |
|     ! 0 | 2494 | `		pGen->pIn++;` |
|     ! 0 | 2495 | `	}` |
|       - | 2496 | `	/* Synchronize pointers */` |
|   10608 | 2497 | `	pGen->pIn  = &pEnd[1];` |
|   10608 | 2498 | `	pGen->pEnd = pTmp;` |
|       - | 2499 | `	/* Emit the false jump */` |
|   10608 | 2500 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2501 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10608 | 2502 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - | 2503 | `	/* Compile the loop body */` |
|   10608 | 2504 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   10608 | 2505 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2506 | `		return SXERR_ABORT;` |
|       - | 2507 | `	}` |
|       - | 2508 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10608 | 2509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - | 2510 | `	/* Fix all jumps now the destination is resolved */` |
|   10608 | 2511 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2512 | `	/* Release the loop block */` |
|   10608 | 2513 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2514 | `	/* Statement successfully compiled */` |
|   10608 | 2515 | `	return SXRET_OK;` |
|     ! 0 | 2516 | `Synchronize:` |
|       - | 2517 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2518 | `	 * compiling this erroneous block.` |
|       - | 2519 | `	 */` |
|     ! 0 | 2520 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2521 | `		pGen->pIn++;` |
|     ! 0 | 2522 | `	}` |
|     ! 0 | 2523 | `	return SXRET_OK;` |
|    5305 | 2524 |  |
|       - | 2525 | `/*` |
|       - | 2526 | ` * Compile the ugly do..while() statement.` |
|       - | 2527 | ` * According to the PHP language reference` |
|       - | 2528 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - | 2529 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - | 2530 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - | 2531 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - | 2532 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - | 2533 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - | 2534 | ` *  would end immediately).` |
|       - | 2535 | ` *  There is just one syntax for do-while loops:` |
|       - | 2536 | ` *  <?php` |
|       - | 2537 | ` *  $i = 0;` |
|       - | 2538 | ` *  do {` |
|       - | 2539 | ` *   echo $i;` |
|       - | 2540 | ` *  } while ($i > 0);` |
|       - | 2541 | ` * ?>` |
|       - | 2542 | ` */` |
|       2 | 2543 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 | 2544 |  |
|       3 | 2545 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 | 2546 | `	GenBlock *pDoBlock = 0;` |
|       - | 2547 | `	sxu32 nLine;` |
|       - | 2548 | `	sxi32 rc;` |
|       3 | 2549 | `	nLine = pGen->pIn->nLine;` |
|       - | 2550 | `	/* Jump the 'do' keyword */` |
|       3 | 2551 | `	pGen->pIn++;` |
|       - | 2552 | `	/* Create the loop block */` |
|       3 | 2553 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 | 2554 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2555 | `		return SXERR_ABORT;` |
|       - | 2556 | `	}` |
|       - | 2557 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 | 2558 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 | 2559 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 | 2560 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2561 | `		return SXERR_ABORT;` |
|       - | 2562 | `	}` |
|       3 | 2563 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2564 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 | 2565 | `	}` |
|       3 | 2566 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 | 2567 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - | 2568 | `			/* Missing 'while' statement */` |
|       3 | 2569 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 | 2570 | `			if( rc == SXERR_ABORT ){` |
|       - | 2571 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2572 | `				return SXERR_ABORT;` |
|       - | 2573 | `			}` |
|       3 | 2574 | `			goto Synchronize;` |
|       - | 2575 | `	}` |
|       - | 2576 | `	/* Jump the 'while' keyword */` |
|     ! 0 | 2577 | `	pGen->pIn++;` |
|     ! 0 | 2578 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2579 | `		/* Syntax error */` |
|     ! 0 | 2580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 | 2581 | `		if( rc == SXERR_ABORT ){` |
|       - | 2582 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2583 | `			return SXERR_ABORT;` |
|       - | 2584 | `		}` |
|     ! 0 | 2585 | `		goto Synchronize;` |
|       - | 2586 | `	}` |
|       - | 2587 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 | 2588 | `	pGen->pIn++;` |
|       - | 2589 | `	/* Delimit the condition */` |
|     ! 0 | 2590 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 | 2591 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2592 | `		/* Empty expression */` |
|     ! 0 | 2593 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 | 2594 | `		if( rc == SXERR_ABORT ){` |
|       - | 2595 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2596 | `			return SXERR_ABORT;` |
|       - | 2597 | `		}` |
|     ! 0 | 2598 | `		goto Synchronize;` |
|       - | 2599 | `	}` |
|       - | 2600 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 | 2601 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - | 2602 | `		JumpFixup *aPost;` |
|       - | 2603 | `		VmInstr *pInstr;` |
|       - | 2604 | `		sxu32 nJumpDest;` |
|       - | 2605 | `		sxu32 n;` |
|     ! 0 | 2606 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 | 2607 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 | 2608 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 | 2609 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 | 2610 | `			if( pInstr ){` |
|       - | 2611 | `				/* Fix */` |
|     ! 0 | 2612 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 | 2613 | `			}` |
|     ! 0 | 2614 | `		}` |
|     ! 0 | 2615 | `	}` |
|       - | 2616 | `	/* Swap token streams */` |
|     ! 0 | 2617 | `	pTmp = pGen->pEnd;` |
|     ! 0 | 2618 | `	pGen->pEnd = pEnd;` |
|       - | 2619 | `	/* Compile the expression */` |
|     ! 0 | 2620 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 2621 | `	if( rc == SXERR_ABORT ){` |
|       - | 2622 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2623 | `		return SXERR_ABORT;` |
|       - | 2624 | `	}` |
|       - | 2625 | `	/* Update token stream */` |
|     ! 0 | 2626 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 2627 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2628 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2629 | `			return SXERR_ABORT;` |
|       - | 2630 | `		}` |
|     ! 0 | 2631 | `		pGen->pIn++;` |
|     ! 0 | 2632 | `	}` |
|     ! 0 | 2633 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 | 2634 | `	pGen->pEnd = pTmp;` |
|       - | 2635 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 | 2636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - | 2637 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 | 2638 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2639 | `	/* Release the loop block */` |
|     ! 0 | 2640 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2641 | `	/* Statement successfully compiled */` |
|     ! 0 | 2642 | `	return SXRET_OK;` |
|       1 | 2643 | `Synchronize:` |
|       - | 2644 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 2645 | `	 * compiling this erroneous block.` |
|       - | 2646 | `	 */` |
|       3 | 2647 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 2648 | `		pGen->pIn++;` |
|     ! 0 | 2649 | `	}` |
|       3 | 2650 | `	return SXRET_OK;` |
|       2 | 2651 |  |
|       - | 2652 | `/*` |
|       - | 2653 | ` * Compile the complex and powerful 'for' statement.` |
|       - | 2654 | ` * According to the PHP language reference` |
|       - | 2655 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - | 2656 | ` *  The syntax of a for loop is:` |
|       - | 2657 | ` *  for (expr1; expr2; expr3)` |
|       - | 2658 | ` *   statement` |
|       - | 2659 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - | 2660 | ` *  the beginning of the loop.` |
|       - | 2661 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - | 2662 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - | 2663 | ` *  to FALSE, the execution of the loop ends.` |
|       - | 2664 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - | 2665 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - | 2666 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - | 2667 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - | 2668 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - | 2669 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - | 2670 | ` *  of using the for truth expression.` |
|       - | 2671 | ` */` |
|   10590 | 2672 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 | 2673 |  |
|   10592 | 2674 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   10592 | 2675 | `	GenBlock *pForBlock = 0;` |
|       - | 2676 | `	sxu32 nFalseJump;` |
|       - | 2677 | `	sxu32 nLine;` |
|       - | 2678 | `	sxi32 rc;` |
|   10592 | 2679 | `	nLine = pGen->pIn->nLine;` |
|       - | 2680 | `	/* Jump the 'for' keyword */` |
|   10592 | 2681 | `	pGen->pIn++;` |
|   10592 | 2682 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2683 | `		/* Syntax error */` |
|     ! 0 | 2684 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 | 2685 | `		if( rc == SXERR_ABORT ){` |
|       - | 2686 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2687 | `			return SXERR_ABORT;` |
|       - | 2688 | `		}` |
|     ! 0 | 2689 | `		return SXRET_OK;` |
|       - | 2690 | `	}` |
|       - | 2691 | `	/* Jump the left parenthesis '(' */` |
|   10592 | 2692 | `	pGen->pIn++;` |
|       - | 2693 | `	/* Delimit the init-expr;condition;post-expr */` |
|   10592 | 2694 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   10592 | 2695 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2696 | `		/* Empty expression */` |
|     ! 0 | 2697 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 | 2698 | `		if( rc == SXERR_ABORT ){` |
|       - | 2699 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2700 | `			return SXERR_ABORT;` |
|       - | 2701 | `		}` |
|       - | 2702 | `		/* Synchronize */` |
|     ! 0 | 2703 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2704 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2705 | `			pGen->pIn++;` |
|     ! 0 | 2706 | `		}` |
|     ! 0 | 2707 | `		return SXRET_OK;` |
|       - | 2708 | `	}` |
|       - | 2709 | `	/* Swap token streams */` |
|   10592 | 2710 | `	pTmp = pGen->pEnd;` |
|   10592 | 2711 | `	pGen->pEnd = pEnd;` |
|       - | 2712 | `	/* Compile initialization expressions if available */` |
|   10592 | 2713 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 2714 | `	/* Pop operand lvalues */` |
|   10592 | 2715 | `	if( rc == SXERR_ABORT ){` |
|       - | 2716 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2717 | `		return SXERR_ABORT;` |
|   10592 | 2718 | `	}else if( rc != SXERR_EMPTY ){` |
|   10590 | 2719 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5294 | 2720 | `	}` |
|   10592 | 2721 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2722 | `		/* Syntax error */` |
|     ! 0 | 2723 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2724 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 | 2725 | `		if( rc == SXERR_ABORT ){` |
|       - | 2726 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2727 | `			return SXERR_ABORT;` |
|       - | 2728 | `		}` |
|     ! 0 | 2729 | `		return SXRET_OK;` |
|       - | 2730 | `	}` |
|       - | 2731 | `	/* Jump the trailing ';' */` |
|   10592 | 2732 | `	pGen->pIn++;` |
|       - | 2733 | `	/* Create the loop block */` |
|   10592 | 2734 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   10592 | 2735 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2736 | `		return SXERR_ABORT;` |
|       - | 2737 | `	}` |
|       - | 2738 | `	/* Deffer continue jumps */` |
|   10592 | 2739 | `	pForBlock->bPostContinue = TRUE;` |
|       - | 2740 | `	/* Compile the condition */` |
|   10592 | 2741 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10592 | 2742 | `	if( rc == SXERR_ABORT ){` |
|       - | 2743 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2744 | `		return SXERR_ABORT;` |
|   10592 | 2745 | `	}else if( rc != SXERR_EMPTY ){` |
|       - | 2746 | `		/* Emit the false jump */` |
|   10590 | 2747 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - | 2748 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   10590 | 2749 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5294 | 2750 | `	}` |
|   10592 | 2751 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 2752 | `		/* Syntax error */` |
|       5 | 2753 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 2754 | `			"for: Expected ';' after conditionals expressions");` |
|       5 | 2755 | `		if( rc == SXERR_ABORT ){` |
|       - | 2756 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2757 | `			return SXERR_ABORT;` |
|       - | 2758 | `		}` |
|       5 | 2759 | `		return SXRET_OK;` |
|       - | 2760 | `	}` |
|       - | 2761 | `	/* Jump the trailing ';' */` |
|   10588 | 2762 | `	pGen->pIn++;` |
|       - | 2763 | `	/* Save the post condition stream */` |
|   10588 | 2764 | `	pPostStart = pGen->pIn;` |
|       - | 2765 | `	/* Compile the loop body */` |
|   10588 | 2766 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   10588 | 2767 | `	pGen->pEnd = pTmp;` |
|   10588 | 2768 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   10588 | 2769 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 2770 | `		return SXERR_ABORT;` |
|       - | 2771 | `	}` |
|       - | 2772 | `	/* Fix post-continue jumps */` |
|   10588 | 2773 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - | 2774 | `		JumpFixup *aPost;` |
|       - | 2775 | `		VmInstr *pInstr;` |
|       - | 2776 | `		sxu32 nJumpDest;` |
|       - | 2777 | `		sxu32 n;` |
|       9 | 2778 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|       9 | 2779 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      17 | 2780 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|       9 | 2781 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|       9 | 2782 | `			if( pInstr ){` |
|       - | 2783 | `				/* Fix jump */` |
|       9 | 2784 | `				pInstr->iP2 = nJumpDest;` |
|       4 | 2785 | `			}` |
|       5 | 2786 | `		}` |
|       4 | 2787 | `	}` |
|       - | 2788 | `	/* compile the post-expressions if available */` |
|   10588 | 2789 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 | 2790 | `		pPostStart++;` |
|     ! 0 | 2791 | `	}` |
|   10588 | 2792 | `	if( pPostStart < pEnd ){` |
|       - | 2793 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   10588 | 2794 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   10588 | 2795 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   10588 | 2796 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 2797 | `			/* Syntax error */` |
|     ! 0 | 2798 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 | 2799 | `			if( rc == SXERR_ABORT ){` |
|       - | 2800 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 2801 | `				return SXERR_ABORT;` |
|       - | 2802 | `			}` |
|     ! 0 | 2803 | `			return SXRET_OK;` |
|       - | 2804 | `		}` |
|   10588 | 2805 | `		RE_SWAP_DELIMITER(pGen);` |
|   10588 | 2806 | `		if( rc == SXERR_ABORT ){` |
|       - | 2807 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2808 | `			return SXERR_ABORT;` |
|   10588 | 2809 | `		}else if( rc != SXERR_EMPTY){` |
|       - | 2810 | `			/* Pop operand lvalue */` |
|   10588 | 2811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5293 | 2812 | `		}` |
|    5293 | 2813 | `	}` |
|       - | 2814 | `	/* Emit the unconditional jump to the start of the loop */` |
|   10588 | 2815 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - | 2816 | `	/* Fix all jumps now the destination is resolved */` |
|   10588 | 2817 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 2818 | `	/* Release the loop block */` |
|   10588 | 2819 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 2820 | `	/* Statement successfully compiled */` |
|   10588 | 2821 | `	return SXRET_OK;` |
|    5297 | 2822 |  |
|       - | 2823 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - | 2824 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - | 2825 | ` * are allowed.` |
|       - | 2826 | ` */` |
|    5650 | 2827 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 2828 |  |
|    5652 | 2829 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    5652 | 2830 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 2831 | `		/* Unexpected expression */` |
|     ! 0 | 2832 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 2833 | `			"foreach: Expecting a variable name");` |
|     ! 0 | 2834 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 2835 | `			rc = SXERR_INVALID;` |
|     ! 0 | 2836 | `		}` |
|     ! 0 | 2837 | `	}` |
|    5652 | 2838 | `	return rc;` |
|       2 | 2839 |  |
|       - | 2840 | `/*` |
|       - | 2841 | ` * Compile the 'foreach' statement.` |
|       - | 2842 | ` * According to the PHP language reference` |
|       - | 2843 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - | 2844 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - | 2845 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - | 2846 | ` *  is a minor but useful extension of the first:` |
|       - | 2847 | ` *  foreach (array_expression as $value)` |
|       - | 2848 | ` *    statement` |
|       - | 2849 | ` *  foreach (array_expression as $key => $value)` |
|       - | 2850 | ` *   statement` |
|       - | 2851 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - | 2852 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - | 2853 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - | 2854 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - | 2855 | ` *  to the variable $key on each loop.` |
|       - | 2856 | ` *  Note:` |
|       - | 2857 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - | 2858 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - | 2859 | ` *  Note:` |
|       - | 2860 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - | 2861 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - | 2862 | ` *  or after the foreach without resetting it.` |
|       - | 2863 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - | 2864 | ` *  of copying the value.` |
|       - | 2865 | ` */` |
|    2876 | 2866 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 | 2867 |  |
|    2878 | 2868 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    2878 | 2869 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    2878 | 2870 | `	GenBlock *pForeachBlock = 0;` |
|       - | 2871 | `	ph7_foreach_info *pInfo;` |
|       - | 2872 | `	sxu32 nFalseJump;` |
|       - | 2873 | `	VmInstr *pInstr;` |
|       - | 2874 | `	sxu32 nLine;` |
|       - | 2875 | `	sxi32 rc;` |
|    2878 | 2876 | `	nLine = pGen->pIn->nLine;` |
|       - | 2877 | `	/* Jump the 'foreach' keyword */` |
|    2878 | 2878 | `	pGen->pIn++;` |
|    2878 | 2879 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 2880 | `		/* Syntax error */` |
|     ! 0 | 2881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 | 2882 | `		if( rc == SXERR_ABORT ){` |
|       - | 2883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2884 | `			return SXERR_ABORT;` |
|       - | 2885 | `		}` |
|     ! 0 | 2886 | `		goto Synchronize;` |
|       - | 2887 | `	}` |
|       - | 2888 | `	/* Jump the left parenthesis '(' */` |
|    2878 | 2889 | `	pGen->pIn++;` |
|       - | 2890 | `	/* Create the loop block */` |
|    2878 | 2891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    2878 | 2892 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2893 | `		return SXERR_ABORT;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Delimit the expression */` |
|    2878 | 2896 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    2878 | 2897 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 2898 | `		/* Empty expression */` |
|     ! 0 | 2899 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 | 2900 | `		if( rc == SXERR_ABORT ){` |
|       - | 2901 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 2902 | `			return SXERR_ABORT;` |
|       - | 2903 | `		}` |
|       - | 2904 | `		/* Synchronize */` |
|     ! 0 | 2905 | `		pGen->pIn = pEnd;` |
|     ! 0 | 2906 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 | 2907 | `			pGen->pIn++;` |
|     ! 0 | 2908 | `		}` |
|     ! 0 | 2909 | `		return SXRET_OK;` |
|       - | 2910 | `	}` |
|       - | 2911 | `	/* Compile the array expression */` |
|    2878 | 2912 | `	pCur = pGen->pIn;` |
|   19240 | 2913 | `	while( pCur < pEnd ){` |
|   19240 | 2914 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    2888 | 2915 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    2888 | 2916 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - | 2917 | `				/* Break with the first 'as' found */` |
|    2878 | 2918 | `				break;` |
|       - | 2919 | `			}` |
|       5 | 2920 | `		}` |
|       - | 2921 | `		/* Advance the stream cursor */` |
|   16364 | 2922 | `		pCur++;` |
|       2 | 2923 | `	}` |
|    2878 | 2924 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 | 2925 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - | 2926 | `			"foreach: Missing array/object expression");` |
|     ! 0 | 2927 | `		if( rc == SXERR_ABORT ){` |
|       - | 2928 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2929 | `			return SXERR_ABORT;` |
|       - | 2930 | `		}` |
|     ! 0 | 2931 | `		goto Synchronize;` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Swap token streams */` |
|    2878 | 2934 | `	pTmp = pGen->pEnd;` |
|    2878 | 2935 | `	pGen->pEnd = pCur;` |
|    2878 | 2936 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    2878 | 2937 | `	if( rc == SXERR_ABORT ){` |
|       - | 2938 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 2939 | `		return SXERR_ABORT;` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Update token stream */` |
|    2878 | 2942 | `	while(pGen->pIn < pCur ){` |
|     ! 0 | 2943 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 2944 | `		if( rc == SXERR_ABORT ){` |
|       - | 2945 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2946 | `			return SXERR_ABORT;` |
|       - | 2947 | `		}` |
|     ! 0 | 2948 | `		pGen->pIn++;` |
|     ! 0 | 2949 | `	}` |
|    2878 | 2950 | `	pCur++; /* Jump the 'as' keyword */` |
|    2878 | 2951 | `	pGen->pIn = pCur;` |
|    2878 | 2952 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2953 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 | 2954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 2955 | `			return SXERR_ABORT;` |
|       - | 2956 | `		}` |
|     ! 0 | 2957 | `	}` |
|       - | 2958 | `	/* Create the foreach context */` |
|    2878 | 2959 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    2878 | 2960 | `	if( pInfo == 0 ){` |
|     ! 0 | 2961 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 2962 | `		return SXERR_ABORT;` |
|       - | 2963 | `	}` |
|       - | 2964 | `	/* Zero the structure */` |
|    2878 | 2965 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - | 2966 | `	/* Initialize structure fields */` |
|    2878 | 2967 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - | 2968 | `	/* Check if we have a key field */` |
|    8680 | 2969 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    5804 | 2970 | `		pCur++;` |
|       2 | 2971 | `	}` |
|    2878 | 2972 | `	if( pCur < pEnd ){` |
|       - | 2973 | `		/* Compile the expression holding the key name */` |
|    2786 | 2974 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 | 2975 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 | 2976 | `			if( rc == SXERR_ABORT ){` |
|       - | 2977 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2978 | `				return SXERR_ABORT;` |
|       - | 2979 | `			}` |
|     ! 0 | 2980 | `		}else{` |
|    2786 | 2981 | `			pGen->pEnd = pCur;` |
|    2786 | 2982 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2786 | 2983 | `			if( rc == SXERR_ABORT ){` |
|       - | 2984 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 2985 | `				return SXERR_ABORT;` |
|       - | 2986 | `			}` |
|    2786 | 2987 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2786 | 2988 | `			if( pInstr->p3 ){` |
|       - | 2989 | `				/* Record key name */` |
|    2786 | 2990 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1392 | 2991 | `			}` |
|    2786 | 2992 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - | 2993 | `		}` |
|    2786 | 2994 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1392 | 2995 | `	}` |
|    2878 | 2996 | `	pGen->pEnd = pEnd;` |
|    2878 | 2997 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 2998 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 | 2999 | `		if( rc == SXERR_ABORT ){` |
|       - | 3000 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3001 | `			return SXERR_ABORT;` |
|       - | 3002 | `		}` |
|     ! 0 | 3003 | `		goto Synchronize;` |
|       - | 3004 | `	}` |
|    2878 | 3005 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 | 3006 | `		pGen->pIn++;` |
|       - | 3007 | `		/* Pass by reference  */` |
|      11 | 3008 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 | 3009 | `	}` |
|       - | 3010 | `	/* Check if the value target is list() */` |
|    2878 | 3011 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 3012 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - | 3013 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - | 3014 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - | 3015 | `		 */` |
|       - | 3016 | `		static int iForeachListCnt = 0;` |
|       - | 3017 | `		char zTmp[128];` |
|       - | 3018 | `		sxu32 nLen;` |
|       - | 3019 | `		char *zDup;` |
|      10 | 3020 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 | 3021 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 | 3022 | `		if( zDup == 0 ){` |
|     ! 0 | 3023 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3024 | `			return SXERR_ABORT;` |
|       - | 3025 | `		}` |
|      10 | 3026 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3027 | `		/* Save list() token boundaries */` |
|      10 | 3028 | `		pListStart = pGen->pIn;` |
|       - | 3029 | `		/* Advance past list(...) — validate parentheses */` |
|      10 | 3030 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 | 3031 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 | 3032 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - | 3033 | `				"foreach: Expected '(' after 'list'");` |
|       3 | 3034 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3035 | `				return SXERR_ABORT;` |
|       - | 3036 | `			}` |
|       3 | 3037 | `			goto Synchronize;` |
|       - | 3038 | `		}` |
|       7 | 3039 | `		pGen->pIn++; /* Jump '(' */` |
|       7 | 3040 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 | 3041 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3042 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3043 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 | 3044 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3045 | `				return SXERR_ABORT;` |
|       - | 3046 | `			}` |
|     ! 0 | 3047 | `			goto Synchronize;` |
|       - | 3048 | `		}` |
|       7 | 3049 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 | 3050 | `		pListEnd = pGen->pIn;` |
|       7 | 3051 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    2873 | 3052 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - | 3053 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - | 3054 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - | 3055 | `		 */` |
|       - | 3056 | `		static int iForeachShortListCnt = 0;` |
|       - | 3057 | `		char zTmp[128];` |
|       - | 3058 | `		sxu32 nLen;` |
|       - | 3059 | `		char *zDup;` |
|       3 | 3060 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 | 3061 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 | 3062 | `		if( zDup == 0 ){` |
|     ! 0 | 3063 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3064 | `			return SXERR_ABORT;` |
|       - | 3065 | `		}` |
|       3 | 3066 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - | 3067 | `		/* Save [...] token boundaries */` |
|       3 | 3068 | `		pListStart = pGen->pIn;` |
|       - | 3069 | `		/* Advance past [...] */` |
|       3 | 3070 | `		pGen->pIn++; /* Jump '[' */` |
|       3 | 3071 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 | 3072 | `		if( pListEnd >= pEnd ){` |
|     ! 0 | 3073 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 3074 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 | 3075 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3076 | `				return SXERR_ABORT;` |
|       - | 3077 | `			}` |
|     ! 0 | 3078 | `			goto Synchronize;` |
|       - | 3079 | `		}` |
|       3 | 3080 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 | 3081 | `		pListEnd = pGen->pIn;` |
|       3 | 3082 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 | 3083 | `	}else{` |
|       - | 3084 | `		/* Compile the expression holding the value name */` |
|    2868 | 3085 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2868 | 3086 | `		if( rc == SXERR_ABORT ){` |
|       - | 3087 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3088 | `			return SXERR_ABORT;` |
|       - | 3089 | `		}` |
|    2868 | 3090 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2868 | 3091 | `		if( pInstr->p3 ){` |
|       - | 3092 | `			/* Record value name */` |
|    2868 | 3093 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1433 | 3094 | `		}` |
|       - | 3095 | `	}` |
|       - | 3096 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    2876 | 3097 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - | 3098 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2876 | 3099 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - | 3100 | `	/* Record the first instruction to execute */` |
|    2876 | 3101 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3102 | `	/* Emit the FOREACH_STEP instruction */` |
|    2876 | 3103 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - | 3104 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    2876 | 3105 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - | 3106 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    2876 | 3107 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - | 3108 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - | 3109 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - | 3110 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - | 3111 | `		 */` |
|       9 | 3112 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - | 3113 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - | 3114 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - | 3115 | `		 * picks up the delimiter and the variable names inside.` |
|       - | 3116 | `		 */` |
|       9 | 3117 | `		pSavedIn = pGen->pIn;` |
|       9 | 3118 | `		pSavedEnd = pGen->pEnd;` |
|       9 | 3119 | `		pGen->pIn = pListStart;` |
|       9 | 3120 | `		pGen->pEnd = pListEnd;` |
|       9 | 3121 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 | 3122 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 | 3123 | `		}else{` |
|       7 | 3124 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - | 3125 | `		}` |
|       9 | 3126 | `		pGen->pIn = pSavedIn;` |
|       9 | 3127 | `		pGen->pEnd = pSavedEnd;` |
|       9 | 3128 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3129 | `			return SXERR_ABORT;` |
|       - | 3130 | `		}` |
|       - | 3131 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 | 3132 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 | 3133 | `	}` |
|       - | 3134 | `	/* Compile the loop body */` |
|    2876 | 3135 | `	pGen->pIn = &pEnd[1];` |
|    2876 | 3136 | `	pGen->pEnd = pTmp;` |
|    2876 | 3137 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    2876 | 3138 | `	if( rc == SXERR_ABORT ){` |
|       - | 3139 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 3140 | `		return SXERR_ABORT;` |
|       - | 3141 | `	}` |
|       - | 3142 | `	/* Emit the unconditional jump to the start of the loop */` |
|    2876 | 3143 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - | 3144 | `	/* Fix all jumps now the destination is resolved */` |
|    2876 | 3145 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 3146 | `	/* Release the loop block */` |
|    2876 | 3147 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3148 | `	/* Statement successfully compiled */` |
|    2876 | 3149 | `	return SXRET_OK;` |
|       1 | 3150 | `Synchronize:` |
|       - | 3151 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - | 3152 | `	 * compiling this erroneous block.` |
|       - | 3153 | `	 */` |
|       3 | 3154 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3155 | `		pGen->pIn++;` |
|     ! 0 | 3156 | `	}` |
|       3 | 3157 | `	return SXRET_OK;` |
|    1440 | 3158 |  |
|       - | 3159 | `/*` |
|       - | 3160 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - | 3161 | ` * According to the PHP language reference` |
|       - | 3162 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - | 3163 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - | 3164 | ` *  that is similar to that of C:` |
|       - | 3165 | ` *  if (expr)` |
|       - | 3166 | ` *   statement` |
|       - | 3167 | ` *  else construct:` |
|       - | 3168 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - | 3169 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - | 3170 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - | 3171 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - | 3172 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - | 3173 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - | 3174 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - | 3175 | ` *  elseif` |
|       - | 3176 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - | 3177 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - | 3178 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - | 3179 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - | 3180 | ` *   than b, a equal to b or a is smaller than b:` |
|       - | 3181 | ` *   <?php` |
|       - | 3182 | ` *    if ($a > $b) {` |
|       - | 3183 | ` *     echo "a is bigger than b";` |
|       - | 3184 | ` *    } elseif ($a == $b) {` |
|       - | 3185 | ` *     echo "a is equal to b";` |
|       - | 3186 | ` *    } else {` |
|       - | 3187 | ` *     echo "a is smaller than b";` |
|       - | 3188 | ` *    }` |
|       - | 3189 | ` *    ?>` |
|       - | 3190 | ` */` |
|  105452 | 3191 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 | 3192 |  |
|  105454 | 3193 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  105454 | 3194 | `	GenBlock *pCondBlock = 0;` |
|       - | 3195 | `	sxu32 nJumpIdx;` |
|       - | 3196 | `	sxu32 nKeyID;` |
|       - | 3197 | `	sxi32 rc;` |
|       - | 3198 | `	/* Jump the 'if' keyword */` |
|  105454 | 3199 | `	pGen->pIn++;` |
|  105454 | 3200 | `	pToken = pGen->pIn;` |
|       - | 3201 | `	/* Create the conditional block */` |
|  105454 | 3202 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  105454 | 3203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3204 | `		return SXERR_ABORT;` |
|       - | 3205 | `	}` |
|       - | 3206 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   57990 | 3207 | `	for(;;){` |
|  115982 | 3208 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 3209 | `			/* Syntax error */` |
|     ! 0 | 3210 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3211 | `				pToken--;` |
|     ! 0 | 3212 | `			}` |
|     ! 0 | 3213 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 | 3214 | `			if( rc == SXERR_ABORT ){` |
|       - | 3215 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3216 | `				return SXERR_ABORT;` |
|       - | 3217 | `			}` |
|     ! 0 | 3218 | `			goto Synchronize;` |
|       - | 3219 | `		}` |
|       - | 3220 | `		/* Jump the left parenthesis '(' */` |
|  115982 | 3221 | `		pToken++;` |
|       - | 3222 | `		/* Delimit the condition */` |
|  115982 | 3223 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  115982 | 3224 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - | 3225 | `			/* Syntax error */` |
|     ! 0 | 3226 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 3227 | `				pToken--;` |
|     ! 0 | 3228 | `			}` |
|     ! 0 | 3229 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 | 3230 | `			if( rc == SXERR_ABORT ){` |
|       - | 3231 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 3232 | `				return SXERR_ABORT;` |
|       - | 3233 | `			}` |
|     ! 0 | 3234 | `			goto Synchronize;` |
|       - | 3235 | `		}` |
|       - | 3236 | `		/* Swap token streams */` |
|  115982 | 3237 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - | 3238 | `		/* Compile the condition */` |
|  115982 | 3239 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3240 | `		/* Update token stream */` |
|  115982 | 3241 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 | 3242 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3243 | `			pGen->pIn++;` |
|     ! 0 | 3244 | `		}` |
|  115982 | 3245 | `		pGen->pIn  = &pEnd[1];` |
|  115982 | 3246 | `		pGen->pEnd = pTmp;` |
|  115982 | 3247 | `		if( rc == SXERR_ABORT ){` |
|       - | 3248 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 3249 | `			return SXERR_ABORT;` |
|       - | 3250 | `		}` |
|       - | 3251 | `		/* Emit the false jump */` |
|  115982 | 3252 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - | 3253 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  115982 | 3254 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - | 3255 | `		/* Compile the body */` |
|  115982 | 3256 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  115982 | 3257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3258 | `			return SXERR_ABORT;` |
|       - | 3259 | `		}` |
|  115982 | 3260 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   31221 | 3261 | `			break;` |
|       - | 3262 | `		}` |
|       - | 3263 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   53544 | 3264 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   53544 | 3265 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   34416 | 3266 | `			break;` |
|       - | 3267 | `		}` |
|       - | 3268 | `		/* Emit the unconditional jump */` |
|   19130 | 3269 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - | 3270 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   19130 | 3271 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   19130 | 3272 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   13854 | 3273 | `			pToken = &pGen->pIn[1];` |
|   13854 | 3274 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5282 | 3275 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4302 | 3276 | `					break;` |
|       - | 3277 | `			}` |
|    5254 | 3278 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2626 | 3279 | `		}` |
|   10530 | 3280 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - | 3281 | `		/* Synchronize cursors */` |
|   10530 | 3282 | `		pToken = pGen->pIn;` |
|       - | 3283 | `		/* Fix the false jump */` |
|   10530 | 3284 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 | 3285 | `	} /* For(;;) */` |
|       - | 3286 | `	/* Fix the false jump */` |
|  105454 | 3287 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  105454 | 3288 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   43014 | 3289 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - | 3290 | `			/* Compile the else block */` |
|    8602 | 3291 | `			pGen->pIn++;` |
|    8602 | 3292 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    8602 | 3293 | `			if( rc == SXERR_ABORT ){` |
|       - | 3294 |  |
|     ! 0 | 3295 | `				return SXERR_ABORT;` |
|       - | 3296 | `			}` |
|    4300 | 3297 | `	}` |
|  105454 | 3298 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - | 3299 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  105454 | 3300 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - | 3301 | `	/* Release the conditional block */` |
|  105454 | 3302 | `	GenStateLeaveBlock(pGen,0);` |
|       - | 3303 | `	/* Statement successfully compiled */` |
|  105454 | 3304 | `	return SXRET_OK;` |
|     ! 0 | 3305 | `Synchronize:` |
|       - | 3306 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - | 3307 | `	 */` |
|     ! 0 | 3308 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 3309 | `		pGen->pIn++;` |
|     ! 0 | 3310 | `	}` |
|     ! 0 | 3311 | `	return SXRET_OK;` |
|   52728 | 3312 |  |
|       - | 3313 | `/*` |
|       - | 3314 | ` * Compile the global construct.` |
|       - | 3315 | ` * According to the PHP language reference` |
|       - | 3316 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - | 3317 | ` *  to be used in that function.` |
|       - | 3318 | ` *  Example #1 Using global` |
|       - | 3319 | ` *  <?php` |
|       - | 3320 | ` *   $a = 1;` |
|       - | 3321 | ` *   $b = 2;` |
|       - | 3322 | ` *   function Sum()` |
|       - | 3323 | ` *   {` |
|       - | 3324 | ` *    global $a, $b;` |
|       - | 3325 | ` *    $b = $a + $b;` |
|       - | 3326 | ` *   }` |
|       - | 3327 | ` *   Sum();` |
|       - | 3328 | ` *   echo $b;` |
|       - | 3329 | ` *  ?>` |
|       - | 3330 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - | 3331 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - | 3332 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - | 3333 | ` */` |
|      26 | 3334 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 | 3335 |  |
|      28 | 3336 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3337 | `	sxi32 nExpr;` |
|       - | 3338 | `	sxi32 rc;` |
|       - | 3339 | `	/* Jump the 'global' keyword */` |
|      28 | 3340 | `	pGen->pIn++;` |
|      28 | 3341 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - | 3342 | `		/* Nothing to process */` |
|     ! 0 | 3343 | `		return SXRET_OK;` |
|       - | 3344 | `	}` |
|      28 | 3345 | `	pTmp = pGen->pEnd;` |
|      28 | 3346 | `	nExpr = 0;` |
|      56 | 3347 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 | 3348 | `		if( pGen->pIn < pNext ){` |
|      30 | 3349 | `			pGen->pEnd = pNext;` |
|      30 | 3350 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3351 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 | 3352 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3353 | `					return SXERR_ABORT;` |
|       - | 3354 | `				}` |
|     ! 0 | 3355 | `			}else{` |
|      30 | 3356 | `				pGen->pIn++;` |
|      30 | 3357 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3358 | `					/* Emit a warning */` |
|     ! 0 | 3359 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 | 3360 | `				}else{` |
|      30 | 3361 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 | 3362 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 3363 | `						return SXERR_ABORT;` |
|      30 | 3364 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 | 3365 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 | 3366 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - | 3367 | `							/* Variable name, not a constant */` |
|      30 | 3368 | `							pLast->iP1 = 0;` |
|      14 | 3369 | `						}` |
|      30 | 3370 | `						nExpr++;` |
|      14 | 3371 | `					}` |
|       - | 3372 | `				}` |
|       - | 3373 | `			}` |
|      14 | 3374 | `		}` |
|       - | 3375 | `		/* Next expression in the stream */` |
|      30 | 3376 | `		pGen->pIn = pNext;` |
|       - | 3377 | `		/* Jump trailing commas */` |
|      32 | 3378 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 3379 | `			pGen->pIn++;` |
|       1 | 3380 | `		}` |
|       2 | 3381 | `	}` |
|       - | 3382 | `	/* Restore token stream */` |
|      28 | 3383 | `	pGen->pEnd = pTmp;` |
|      28 | 3384 | `	if( nExpr > 0 ){` |
|       - | 3385 | `		/* Emit the uplink instruction */` |
|      28 | 3386 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 | 3387 | `	}` |
|      28 | 3388 | `	return SXRET_OK;` |
|      15 | 3389 |  |
|       - | 3390 | `/*` |
|       - | 3391 | ` * Compile the return statement.` |
|       - | 3392 | ` * According to the PHP language reference` |
|       - | 3393 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - | 3394 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - | 3395 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - | 3396 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - | 3397 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - | 3398 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - | 3399 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - | 3400 | ` *  from within the main script file, then script execution end.` |
|       - | 3401 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - | 3402 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - | 3403 | ` *  should do so as PHP has less work to do in this case.` |
|       - | 3404 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - | 3405 | ` */` |
|  152964 | 3406 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 | 3407 |  |
|  152966 | 3408 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - | 3409 | `	sxi32 rc;` |
|       - | 3410 | `	/* Jump the 'return' keyword */` |
|  152966 | 3411 | `	pGen->pIn++;` |
|  152966 | 3412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3413 | `		/* Compile the expression */` |
|  152944 | 3414 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  152944 | 3415 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3416 | `			return SXERR_ABORT;` |
|  152944 | 3417 | `		}else if(rc != SXERR_EMPTY ){` |
|  152944 | 3418 | `			nRet = 1;` |
|   76471 | 3419 | `		}` |
|   76471 | 3420 | `	}` |
|       - | 3421 | `	/* Emit the done instruction */` |
|  152966 | 3422 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  152966 | 3423 | `	return SXRET_OK;` |
|   76484 | 3424 |  |
|       - | 3425 | `/*` |
|       - | 3426 | ` * Compile a yield expression.` |
|       - | 3427 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - | 3428 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - | 3429 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - | 3430 | ` */` |
|      32 | 3431 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 | 3432 |  |
|       - | 3433 | `	SyToken *pTmp, *pSplit;` |
|      34 | 3434 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 | 3435 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - | 3436 | `	sxi32 rc;` |
|      16 | 3437 | `	(void)iCompileFlag;` |
|       - | 3438 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 | 3439 | `	pGen->pIn++;` |
|       - | 3440 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - | 3441 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 | 3442 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3443 | `		/* Bare yield — no value */` |
|     ! 0 | 3444 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 | 3445 | `		return SXRET_OK;` |
|       - | 3446 | `	}` |
|       - | 3447 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 | 3448 | `	pSplit = 0;` |
|       - | 3449 | `	{` |
|      34 | 3450 | `		SyToken *pCur = pGen->pIn;` |
|      34 | 3451 | `		sxi32 nNest = 0;` |
|      78 | 3452 | `		while( pCur < pGen->pEnd ){` |
|      52 | 3453 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 | 3454 | `				nNest++;` |
|      52 | 3455 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 | 3456 | `				nNest--;` |
|      52 | 3457 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 | 3458 | `				pSplit = pCur;` |
|       7 | 3459 | `				break;` |
|       - | 3460 | `			}` |
|      46 | 3461 | `			pCur++;` |
|       2 | 3462 | `		}` |
|       - | 3463 | `	}` |
|      34 | 3464 | `	pTmp = pGen->pEnd;` |
|      34 | 3465 | `	if( pSplit ){` |
|       - | 3466 | `		/* yield $key => $value */` |
|       7 | 3467 | `		pGen->pEnd = pSplit;` |
|       7 | 3468 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3469 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3470 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 | 3471 | `		pGen->pEnd = pTmp;` |
|       7 | 3472 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 | 3473 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 | 3474 | `		iP1 = 1;` |
|       7 | 3475 | `		iP2 = 1;` |
|       4 | 3476 | `	}else{` |
|       - | 3477 | `		/* yield $value */` |
|      28 | 3478 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 | 3479 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 | 3480 | `		if( rc != SXERR_EMPTY ){` |
|      28 | 3481 | `			iP1 = 1;` |
|      13 | 3482 | `		}` |
|       - | 3483 | `	}` |
|      34 | 3484 | `	pGen->pEnd = pTmp;` |
|      34 | 3485 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 | 3486 | `	return SXRET_OK;` |
|      18 | 3487 |  |
|       - | 3488 | `/*` |
|       - | 3489 | ` * Compile the die/exit language construct.` |
|       - | 3490 | ` * The role of these constructs is to terminate execution of the script.` |
|       - | 3491 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - | 3492 | ` */` |
|      88 | 3493 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 | 3494 |  |
|      90 | 3495 | `	sxi32 nExpr = 0;` |
|       - | 3496 | `	sxi32 rc;` |
|       - | 3497 | `	/* Jump the die/exit keyword */` |
|      90 | 3498 | `	pGen->pIn++;` |
|      90 | 3499 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - | 3500 | `		/* Compile the expression */` |
|      90 | 3501 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 | 3502 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3503 | `			return SXERR_ABORT;` |
|      90 | 3504 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 | 3505 | `			nExpr = 1;` |
|      44 | 3506 | `		}` |
|      44 | 3507 | `	}` |
|       - | 3508 | `	/* Emit the HALT instruction */` |
|      90 | 3509 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 | 3510 | `	return SXRET_OK;` |
|      46 | 3511 |  |
|       - | 3512 | `/*` |
|       - | 3513 | ` * Compile the 'echo' language construct.` |
|       - | 3514 | ` */` |
|   10780 | 3515 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 | 3516 |  |
|   10782 | 3517 | `	SyToken *pTmp,*pNext = 0;` |
|       - | 3518 | `	sxi32 rc;` |
|       - | 3519 | `	/* Jump the 'echo' keyword */` |
|   10782 | 3520 | `	pGen->pIn++;` |
|       - | 3521 | `	/* Compile arguments one after one */` |
|   10782 | 3522 | `	pTmp = pGen->pEnd;` |
|   21950 | 3523 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   11170 | 3524 | `		if( pGen->pIn < pNext ){` |
|   11170 | 3525 | `			pGen->pEnd = pNext;` |
|   11170 | 3526 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   11170 | 3527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3528 | `				return SXERR_ABORT;` |
|   11170 | 3529 | `			}else if( rc != SXERR_EMPTY ){` |
|       - | 3530 | `				/* Emit the consume instruction */` |
|   11146 | 3531 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    5572 | 3532 | `			}` |
|    5584 | 3533 | `		}` |
|       - | 3534 | `		/* Jump trailing commas */` |
|   11558 | 3535 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     390 | 3536 | `			pNext++;` |
|       2 | 3537 | `		}` |
|   11170 | 3538 | `		pGen->pIn = pNext;` |
|       2 | 3539 | `	}` |
|       - | 3540 | `	/* Restore token stream */` |
|   10782 | 3541 | `	pGen->pEnd = pTmp;` |
|   10782 | 3542 | `	return SXRET_OK;` |
|    5392 | 3543 |  |
|       - | 3544 | `/*` |
|       - | 3545 | ` * Compile the static statement.` |
|       - | 3546 | ` * According to the PHP language reference` |
|       - | 3547 | ` *  Another important feature of variable scoping is the static variable.` |
|       - | 3548 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - | 3549 | ` *  when program execution leaves this scope.` |
|       - | 3550 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - | 3551 | ` * Symisc eXtension.` |
|       - | 3552 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - | 3553 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 3554 | ` *  Example` |
|       - | 3555 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - | 3556 | ` *    Refer to the official documentation for more information on this feature.` |
|       - | 3557 | ` */` |
|       2 | 3558 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 | 3559 |  |
|       - | 3560 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - | 3561 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - | 3562 | `	GenBlock *pBlock;` |
|       - | 3563 | `	SyString *pName;` |
|       - | 3564 | `	char *zDup;` |
|       - | 3565 | `	sxu32 nLine;` |
|       - | 3566 | `	sxi32 rc;` |
|       - | 3567 | `	/* Jump the static keyword */` |
|       3 | 3568 | `	nLine = pGen->pIn->nLine;` |
|       3 | 3569 | `	pGen->pIn++;` |
|       - | 3570 | `	/* Extract the enclosing function if any */` |
|       3 | 3571 | `	pBlock = pGen->pCurrent;` |
|       5 | 3572 | `	while( pBlock ){` |
|       5 | 3573 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 | 3574 | `			break;` |
|       - | 3575 | `		}` |
|       - | 3576 | `		/* Point to the upper block */` |
|       3 | 3577 | `		pBlock = pBlock->pParent;` |
|       1 | 3578 | `	}` |
|       3 | 3579 | `	if( pBlock == 0 ){` |
|       - | 3580 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 | 3581 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 3582 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 | 3583 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3584 | `				return SXERR_ABORT;` |
|       - | 3585 | `			}` |
|     ! 0 | 3586 | `			goto Synchronize;` |
|       - | 3587 | `		}` |
|       - | 3588 | `		/* Compile the expression holding the variable */` |
|     ! 0 | 3589 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 | 3590 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3591 | `			return SXERR_ABORT;` |
|     ! 0 | 3592 | `		}else if( rc != SXERR_EMPTY ){` |
|       - | 3593 | `			/* Emit the POP instruction */` |
|     ! 0 | 3594 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 | 3595 | `		}` |
|     ! 0 | 3596 | `		return SXRET_OK;` |
|       - | 3597 | `	}` |
|       3 | 3598 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - | 3599 | `	/* Make sure we are dealing with a valid statement */` |
|       3 | 3600 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 | 3601 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 | 3602 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 | 3603 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 3604 | `				return SXERR_ABORT;` |
|       - | 3605 | `			}` |
|       3 | 3606 | `			goto Synchronize;` |
|       - | 3607 | `	}` |
|     ! 0 | 3608 | `	pGen->pIn++;` |
|       - | 3609 | `	/* Extract variable name */` |
|     ! 0 | 3610 | `	pName = &pGen->pIn->sData;` |
|     ! 0 | 3611 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 | 3612 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 | 3613 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 3614 | `		goto Synchronize;` |
|       - | 3615 | `	}` |
|       - | 3616 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 | 3617 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 | 3618 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - | 3619 | `	/* Duplicate variable name */` |
|     ! 0 | 3620 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 | 3621 | `	if( zDup == 0 ){` |
|     ! 0 | 3622 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 3623 | `		return SXERR_ABORT;` |
|       - | 3624 | `	}` |
|     ! 0 | 3625 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - | 3626 | `	/* Check if we have an expression to compile */` |
|     ! 0 | 3627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - | 3628 | `		SySet *pInstrContainer;` |
|       - | 3629 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - | 3630 | `		 * Static variable can take any complex expression including function` |
|       - | 3631 | `		 * call as their initialization value.` |
|       - | 3632 | `		 * Example:` |
|       - | 3633 | `		 *		static $var = foo(1,4+5,bar());` |
|       - | 3634 | `		 */` |
|     ! 0 | 3635 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - | 3636 | `		/* Swap bytecode container */` |
|     ! 0 | 3637 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 | 3638 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - | 3639 | `		/* Compile the expression */` |
|     ! 0 | 3640 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 3641 | `		/* Emit the done instruction */` |
|     ! 0 | 3642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - | 3643 | `		/* Restore default bytecode container */` |
|     ! 0 | 3644 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 3645 | `	}` |
|       - | 3646 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 | 3647 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 | 3648 | `	return SXRET_OK;` |
|       1 | 3649 | `Synchronize:` |
|       - | 3650 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - | 3651 | `	 * statement.` |
|       - | 3652 | `	 */` |
|       5 | 3653 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 | 3654 | `		pGen->pIn++;` |
|       1 | 3655 | `	}` |
|       3 | 3656 | `	return SXRET_OK;` |
|       2 | 3657 |  |
|       - | 3658 | `/*` |
|       - | 3659 | ` * Compile the var statement.` |
|       - | 3660 | ` * Symisc Extension:` |
|       - | 3661 | ` *      var statement can be used outside of a class definition.` |
|       - | 3662 | ` */` |
|       4 | 3663 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 | 3664 |  |
|       - | 3665 | `	sxu32 nLine;` |
|       - | 3666 | `	sxi32 rc;` |
|       5 | 3667 | `	nLine = pGen->pIn->nLine;` |
|       - | 3668 | `	/* Jump the 'var' keyword */` |
|       5 | 3669 | `	pGen->pIn++;` |
|       5 | 3670 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 3671 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - | 3672 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 | 3673 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 | 3674 | `			pGen->pIn++;` |
|     ! 0 | 3675 | `		}` |
|     ! 0 | 3676 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3677 | `			return SXERR_ABORT;` |
|       - | 3678 | `		}` |
|     ! 0 | 3679 | `	}else{` |
|       - | 3680 | `		/* Compile the expression */` |
|       5 | 3681 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 | 3682 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3683 | `			return SXERR_ABORT;` |
|       5 | 3684 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 | 3685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 | 3686 | `		}` |
|       - | 3687 | `	}` |
|       5 | 3688 | `	return SXRET_OK;` |
|       3 | 3689 |  |
|       - | 3690 | `/*` |
|       - | 3691 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - | 3692 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - | 3693 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - | 3694 | ` */` |
|       - | 3695 | `/*` |
|       - | 3696 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - | 3697 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - | 3698 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - | 3699 | ` * qualified name and updates the instruction's operand index.` |
|       - | 3700 | ` *` |
|       - | 3701 | ` * Resolution order:` |
|       - | 3702 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - | 3703 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - | 3704 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - | 3705 | ` *` |
|       - | 3706 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - | 3707 | ` * came from an import (step 1) and 0 otherwise.` |
|       - | 3708 | ` * Returns the (possibly new) literal index.` |
|       - | 3709 | ` */` |
|  314186 | 3710 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 | 3711 |  |
|       - | 3712 | `	ph7_value *pLit;` |
|       - | 3713 | `	const char *zLit;` |
|       - | 3714 | `	SyString sQualified;` |
|       - | 3715 | `	sxu32 nLit;` |
|       - | 3716 | `	sxu32 k;` |
|       - | 3717 | `	sxu32 nNewIdx;` |
|       - | 3718 | `	int hasNsSep;` |
|       - | 3719 | `	SyHashEntry *pImport;` |
|       - | 3720 | `	ph7_value *pNew;` |
|  314188 | 3721 | `	if( pFromImport ){` |
|  300476 | 3722 | `		*pFromImport = 0;` |
|  150237 | 3723 | `	}` |
|  314188 | 3724 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  314188 | 3725 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 | 3726 | `		return nOrigIdx;` |
|       - | 3727 | `	}` |
|  314188 | 3728 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  314188 | 3729 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - | 3730 | `	/* Skip if already qualified (contains backslash) */` |
|  314188 | 3731 | `	hasNsSep = 0;` |
| 3379576 | 3732 | `	for( k = 0; k < nLit; k++ ){` |
| 3065422 | 3733 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1532696 | 3734 | `	}` |
|  314188 | 3735 | `	if( hasNsSep ){` |
|      34 | 3736 | `		return nOrigIdx;` |
|       - | 3737 | `	}` |
|       - | 3738 | `	/* Check use imports first (works even outside namespaces) */` |
|  314156 | 3739 | `	SyBlobReset(&pGen->sWorker);` |
|  314156 | 3740 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  314156 | 3741 | `	if( pImport ){` |
|      38 | 3742 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 | 3743 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 | 3744 | `		if( pFromImport ){` |
|      18 | 3745 | `			*pFromImport = 1;` |
|       8 | 3746 | `		}` |
|      20 | 3747 | `	}else{` |
|  314120 | 3748 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  314038 | 3749 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - | 3750 | `		}` |
|       - | 3751 | `		/* Prepend current namespace */` |
|      84 | 3752 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      84 | 3753 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      84 | 3754 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - | 3755 | `	}` |
|       - | 3756 | `	/* Look up or create a new literal for the qualified name */` |
|     120 | 3757 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     120 | 3758 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      52 | 3759 | `		return nNewIdx; /* Already interned */` |
|       - | 3760 | `	}` |
|      70 | 3761 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      70 | 3762 | `	if( pNew == 0 ){` |
|     ! 0 | 3763 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - | 3764 | `	}` |
|      70 | 3765 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      70 | 3766 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      70 | 3767 | `	return nNewIdx;` |
|  157095 | 3768 |  |
|       - | 3769 | `/*` |
|       - | 3770 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - | 3771 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - | 3772 | ` */` |
|   26576 | 3773 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3774 |  |
|       - | 3775 | `	SyHashEntry *pImport;` |
|       - | 3776 | `	/* Check use imports first */` |
|   26578 | 3777 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   26578 | 3778 | `	if( pImport ){` |
|      12 | 3779 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 | 3780 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 | 3781 | `		return;` |
|       - | 3782 | `	}` |
|       - | 3783 | `	/* Prepend current namespace if active */` |
|   26568 | 3784 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 | 3785 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 3786 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 | 3787 | `	}` |
|   26568 | 3788 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   13290 | 3789 |  |
|       - | 3790 | `/*` |
|       - | 3791 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - | 3792 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - | 3793 | ` * The caller must release pOut when done.` |
|       - | 3794 | ` */` |
|   45222 | 3795 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 | 3796 |  |
|   45224 | 3797 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      50 | 3798 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      50 | 3799 | `		SyBlobAppend(pOut,"\\",1);` |
|      24 | 3800 | `	}` |
|   45224 | 3801 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   45224 | 3802 |  |
|       - | 3803 | `/*` |
|       - | 3804 | ` * Compile a namespace statement` |
|       - | 3805 | ` * According to the PHP language reference manual` |
|       - | 3806 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - | 3807 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - | 3808 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - | 3809 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - | 3810 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - | 3811 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - | 3812 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - | 3813 | ` *  programming world.` |
|       - | 3814 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - | 3815 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - | 3816 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - | 3817 | ` *  classes/functions/constants.` |
|       - | 3818 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - | 3819 | ` *  readability of source code.` |
|       - | 3820 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - | 3821 | ` *  Here is an example of namespace syntax in PHP:` |
|       - | 3822 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - | 3823 | ` *       class MyClass {}` |
|       - | 3824 | ` *       function myfunction() {}` |
|       - | 3825 | ` *       const MYCONST = 1;` |
|       - | 3826 | ` *       $a = new MyClass;` |
|       - | 3827 | ` *       $c = new \my\name\MyClass;` |
|       - | 3828 | ` *       $a = strlen('hi');` |
|       - | 3829 | ` *       $d = namespace\MYCONST;` |
|       - | 3830 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - | 3831 | ` *       echo constant($d);` |
|       - | 3832 | ` * NOTE` |
|       - | 3833 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3834 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3835 | ` */` |
|       - | 3836 | `/*` |
|       - | 3837 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - | 3838 | ` */` |
|      10 | 3839 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 | 3840 |  |
|      11 | 3841 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       5 | 3842 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       5 | 3843 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       5 | 3844 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       5 | 3845 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       5 | 3846 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 | 3847 | `	return "token";` |
|       6 | 3848 |  |
|      96 | 3849 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 | 3850 |  |
|       - | 3851 | `	sxu32 nLine;` |
|       - | 3852 | `	sxi32 rc;` |
|      98 | 3853 | `	nLine = pGen->pIn->nLine;` |
|      98 | 3854 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - | 3855 | `	/* Reset namespace and clear previous use imports */` |
|      98 | 3856 | `	SyBlobReset(&pGen->sNamespace);` |
|      98 | 3857 | `	SyHashRelease(&pGen->hUseImports);` |
|      98 | 3858 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3859 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|      98 | 3860 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3861 | `	SyHashRelease(&pGen->hUseConstImports);` |
|      98 | 3862 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|      98 | 3863 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 3864 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 | 3865 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3866 | `		return SXRET_OK;` |
|       - | 3867 | `	}` |
|      98 | 3868 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - | 3869 | `		/* namespace; — switch to global namespace */` |
|     ! 0 | 3870 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3871 | `		return SXRET_OK;` |
|       - | 3872 | `	}` |
|      98 | 3873 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - | 3874 | `		/* namespace { } — global namespace block */` |
|     ! 0 | 3875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 | 3876 | `		return SXRET_OK;` |
|       - | 3877 | `	}` |
|       - | 3878 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     232 | 3879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     136 | 3880 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - | 3881 | `			/* Append backslash separator */` |
|      21 | 3882 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 3883 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 | 3884 | `			}` |
|      11 | 3885 | `		}else{` |
|       - | 3886 | `			/* Append identifier */` |
|     116 | 3887 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 3888 | `		}` |
|     136 | 3889 | `		pGen->pIn++;` |
|       2 | 3890 | `	}` |
|       - | 3891 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - | 3892 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - | 3893 | `	{` |
|      98 | 3894 | `		char *zNsDup = 0;` |
|      98 | 3895 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     143 | 3896 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      94 | 3897 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      47 | 3898 | `		}` |
|      98 | 3899 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - | 3900 | `	}` |
|      98 | 3901 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 | 3902 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - | 3903 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 | 3904 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 | 3905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 3906 | `			return SXERR_ABORT;` |
|       - | 3907 | `		}` |
|       2 | 3908 | `	}` |
|      98 | 3909 | `	return SXRET_OK;` |
|      50 | 3910 |  |
|       - | 3911 | `/*` |
|       - | 3912 | ` * Compile the 'use' statement` |
|       - | 3913 | ` * According to the PHP language reference manual` |
|       - | 3914 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - | 3915 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - | 3916 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - | 3917 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - | 3918 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - | 3919 | ` *  a function or constant is not supported.` |
|       - | 3920 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - | 3921 | ` * NOTE` |
|       - | 3922 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - | 3923 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - | 3924 | ` */` |
|      66 | 3925 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 | 3926 |  |
|       - | 3927 | `	sxu32 nLine;` |
|       - | 3928 | `	sxi32 rc;` |
|       - | 3929 | `	SyBlob sPath;` |
|       - | 3930 | `	SyString sAlias;` |
|       - | 3931 | `	SyToken *pLast;` |
|       - | 3932 | `	char *zDup;` |
|       - | 3933 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - | 3934 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - | 3935 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 | 3936 | `	nLine = pGen->pIn->nLine;` |
|      68 | 3937 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - | 3938 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 | 3939 | `	iUseType = 0;` |
|      68 | 3940 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 | 3941 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 | 3942 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 | 3943 | `			iUseType = 1;` |
|      16 | 3944 | `			pGen->pIn++;` |
|      23 | 3945 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 | 3946 | `			iUseType = 2;` |
|      16 | 3947 | `			pGen->pIn++;` |
|       7 | 3948 | `		}` |
|      14 | 3949 | `	}` |
|       - | 3950 | `	/* Select target hash tables based on import type */` |
|      68 | 3951 | `	switch( iUseType ){` |
|       7 | 3952 | `		case 1:` |
|      16 | 3953 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 | 3954 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 | 3955 | `			break;` |
|       7 | 3956 | `		case 2:` |
|      16 | 3957 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 | 3958 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 | 3959 | `			break;` |
|      19 | 3960 | `		default:` |
|      40 | 3961 | `			pGenHash = &pGen->hUseImports;` |
|      40 | 3962 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 | 3963 | `			break;` |
|       - | 3964 | `	}` |
|      68 | 3965 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - | 3966 | `	/* Process one or more use declarations separated by commas */` |
|      34 | 3967 | `	for(;;){` |
|      70 | 3968 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 3969 | `			break;` |
|       - | 3970 | `		}` |
|      70 | 3971 | `		SyBlobReset(&sPath);` |
|      70 | 3972 | `		pLast = 0;` |
|       - | 3973 | `		/* Collect the full namespace path */` |
|     254 | 3974 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 | 3975 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 | 3976 | `				pLast = pGen->pIn;` |
|     126 | 3977 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 | 3978 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 | 3979 | `				}` |
|     126 | 3980 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 | 3981 | `			}` |
|     186 | 3982 | `			pGen->pIn++;` |
|       2 | 3983 | `		}` |
|      70 | 3984 | `		if( pLast == 0 ){` |
|       - | 3985 | `			/* Empty path */` |
|       5 | 3986 | `			break;` |
|       - | 3987 | `		}` |
|       - | 3988 | `		/* Default alias is the last component of the path */` |
|      66 | 3989 | `		sAlias = pLast->sData;` |
|       - | 3990 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 | 3991 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 | 3992 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 | 3993 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 | 3994 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 | 3995 | `				sAlias = pGen->pIn->sData;` |
|      18 | 3996 | `				pGen->pIn++;` |
|       8 | 3997 | `			}` |
|       8 | 3998 | `		}` |
|       - | 3999 | `		/* Check for duplicate import alias (per-type) */` |
|      66 | 4000 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 | 4001 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 4002 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 | 4003 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 | 4004 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4005 | `				SyBlobRelease(&sPath);` |
|     ! 0 | 4006 | `				return SXERR_ABORT;` |
|       - | 4007 | `			}` |
|       2 | 4008 | `		}` |
|       - | 4009 | `		/* Register the import: alias -> FQN.` |
|       - | 4010 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - | 4011 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - | 4012 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 | 4013 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 | 4014 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 | 4015 | `		if( zDup ){` |
|      66 | 4016 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 | 4017 | `			if( pVmHash ){` |
|       - | 4018 | `				/* Class imports: populate VM table directly (class resolution` |
|       - | 4019 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 | 4020 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 | 4021 | `				if( zAliasDup ){` |
|      38 | 4022 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 | 4023 | `				}` |
|      18 | 4024 | `			}` |
|      66 | 4025 | `			if( iUseType == 2 ){` |
|       - | 4026 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - | 4027 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 | 4028 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 | 4029 | `				if( zAliasDup ){` |
|       - | 4030 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - | 4031 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - | 4032 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 | 4033 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 | 4034 | `					if( azPair ){` |
|      16 | 4035 | `						azPair[0] = zAliasDup;` |
|      16 | 4036 | `						azPair[1] = zDup;` |
|      16 | 4037 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 | 4038 | `					}` |
|       7 | 4039 | `				}` |
|       7 | 4040 | `			}` |
|      32 | 4041 | `		}` |
|       - | 4042 | `		/* Check for comma (multiple use declarations) */` |
|      66 | 4043 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 | 4044 | `			pGen->pIn++;` |
|       2 | 4045 | `		}else{` |
|      33 | 4046 | `			break;` |
|       - | 4047 | `		}` |
|       1 | 4048 | `	}` |
|      68 | 4049 | `	SyBlobRelease(&sPath);` |
|      68 | 4050 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 | 4051 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 | 4052 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 | 4053 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4054 | `			return SXERR_ABORT;` |
|       - | 4055 | `		}` |
|       1 | 4056 | `	}` |
|      68 | 4057 | `	return SXRET_OK;` |
|      35 | 4058 |  |
|       - | 4059 | `/*` |
|       - | 4060 | ` * Compile the stupid 'declare' language construct.` |
|       - | 4061 | ` *` |
|       - | 4062 | ` * According to the PHP language reference manual.` |
|       - | 4063 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - | 4064 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - | 4065 | ` *  declare (directive)` |
|       - | 4066 | ` *   statement` |
|       - | 4067 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - | 4068 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - | 4069 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - | 4070 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - | 4071 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - | 4072 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - | 4073 | ` * <?php` |
|       - | 4074 | ` * // these are the same:` |
|       - | 4075 | ` * // you can use this:` |
|       - | 4076 | ` * declare(ticks=1) {` |
|       - | 4077 | ` *   // entire script here` |
|       - | 4078 | ` * }` |
|       - | 4079 | ` * // or you can use this:` |
|       - | 4080 | ` * declare(ticks=1);` |
|       - | 4081 | ` * // entire script here` |
|       - | 4082 | ` * ?>` |
|       - | 4083 | ` *` |
|       - | 4084 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - | 4085 | ` */` |
|       8 | 4086 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 | 4087 |  |
|       9 | 4088 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 | 4089 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - | 4090 | `	sxi32 rc;` |
|       9 | 4091 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 | 4092 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 | 4093 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 | 4094 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4095 | `			return SXERR_ABORT;` |
|       - | 4096 | `		}` |
|       5 | 4097 | `		goto Synchro;` |
|       - | 4098 | `	}` |
|       5 | 4099 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - | 4100 | `	/* Delimit the directive */` |
|       5 | 4101 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 | 4102 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 4103 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 | 4104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4105 | `			return SXERR_ABORT;` |
|       - | 4106 | `		}` |
|     ! 0 | 4107 | `		return SXRET_OK;` |
|       - | 4108 | `	}` |
|       - | 4109 | `	/* Update the cursor */` |
|       5 | 4110 | `	pGen->pIn = &pEnd[1];` |
|       5 | 4111 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 | 4112 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 | 4113 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4114 | `			return SXERR_ABORT;` |
|       - | 4115 | `		}` |
|     ! 0 | 4116 | `	}` |
|       - | 4117 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 | 4118 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - | 4119 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 | 4120 | `		ph7_lib_version()` |
|       - | 4121 | `		);` |
|       - | 4122 | `	/*All done */` |
|       5 | 4123 | `	return SXRET_OK;` |
|       2 | 4124 | `Synchro:` |
|       - | 4125 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 | 4126 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 | 4127 | `		pGen->pIn++;` |
|       1 | 4128 | `	}` |
|       5 | 4129 | `	return SXRET_OK;` |
|       5 | 4130 |  |
|       - | 4131 | `/*` |
|       - | 4132 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - | 4133 | ` * as follows:` |
|       - | 4134 | ` * function makecoffee($type = "cappuccino")` |
|       - | 4135 | ` * {` |
|       - | 4136 | ` *   return "Making a cup of $type.\n";` |
|       - | 4137 | ` * }` |
|       - | 4138 | ` * Symisc eXtension.` |
|       - | 4139 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - | 4140 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - | 4141 | ` *      Example: Work only with PH7,generate error under zend` |
|       - | 4142 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - | 4143 | ` *      {` |
|       - | 4144 | ` *       var_dump($a);` |
|       - | 4145 | ` *      }` |
|       - | 4146 | ` *     //call test without args` |
|       - | 4147 | ` *      test();` |
|       - | 4148 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - | 4149 | ` *      Example:` |
|       - | 4150 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - | 4151 | ` * 3 -) Function overloading!!` |
|       - | 4152 | ` *      Example:` |
|       - | 4153 | ` *      function foo($a) {` |
|       - | 4154 | ` *   	  return $a.PHP_EOL;` |
|       - | 4155 | ` *	    }` |
|       - | 4156 | ` *	    function foo($a, $b) {` |
|       - | 4157 | ` *   	  return $a + $b;` |
|       - | 4158 | ` *	    }` |
|       - | 4159 | ` *	    echo foo(5); // Prints "5"` |
|       - | 4160 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - | 4161 | ` *      // Same arg` |
|       - | 4162 | ` *	   function foo(string $a)` |
|       - | 4163 | ` *	   {` |
|       - | 4164 | ` *	     echo "a is a string\n";` |
|       - | 4165 | ` *	     var_dump($a);` |
|       - | 4166 | ` *	   }` |
|       - | 4167 | ` *	  function foo(int $a)` |
|       - | 4168 | ` *	  {` |
|       - | 4169 | ` *	    echo "a is integer\n";` |
|       - | 4170 | ` *	    var_dump($a);` |
|       - | 4171 | ` *	  }` |
|       - | 4172 | ` *	  function foo(array $a)` |
|       - | 4173 | ` *	  {` |
|       - | 4174 | ` * 	    echo "a is an array\n";` |
|       - | 4175 | ` * 	    var_dump($a);` |
|       - | 4176 | ` *	  }` |
|       - | 4177 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - | 4178 | ` *	  foo(52); // a is integer [second foo]` |
|       - | 4179 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - | 4180 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - | 4181 | ` * introduced by the PH7 engine.` |
|       - | 4182 | ` */` |
|   42026 | 4183 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 | 4184 |  |
|       - | 4185 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - | 4186 | `	SySet *pInstrContainer;` |
|       - | 4187 | `	sxi32 rc;` |
|       - | 4188 | `	/* Swap token stream */` |
|   42028 | 4189 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   42028 | 4190 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   42028 | 4191 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - | 4192 | `	/* Compile the expression holding the argument value */` |
|   42028 | 4193 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 4194 | `	/* Emit the done instruction */` |
|   42028 | 4195 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   42028 | 4196 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   42028 | 4197 | `	RE_SWAP_DELIMITER(pGen);` |
|   42028 | 4198 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4199 | `		return SXERR_ABORT;` |
|       - | 4200 | `	}` |
|   42028 | 4201 | `	return SXRET_OK;` |
|   21015 | 4202 |  |
|       - | 4203 | `/*` |
|       - | 4204 | ` * Collect function arguments one after one.` |
|       - | 4205 | ` * According to the PHP language reference manual.` |
|       - | 4206 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - | 4207 | ` * list of expressions.` |
|       - | 4208 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - | 4209 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - | 4210 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - | 4211 | ` * for more information.` |
|       - | 4212 | ` * Example #1 Passing arrays to functions` |
|       - | 4213 | ` * <?php` |
|       - | 4214 | ` * function takes_array($input)` |
|       - | 4215 | ` * {` |
|       - | 4216 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - | 4217 | ` * }` |
|       - | 4218 | ` * ?>` |
|       - | 4219 | ` * Making arguments be passed by reference` |
|       - | 4220 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - | 4221 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - | 4222 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - | 4223 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - | 4224 | ` * to the argument name in the function definition:` |
|       - | 4225 | ` * Example #2 Passing function parameters by reference` |
|       - | 4226 | ` * <?php` |
|       - | 4227 | ` * function add_some_extra(&$string)` |
|       - | 4228 | ` * {` |
|       - | 4229 | ` *   $string .= 'and something extra.';` |
|       - | 4230 | ` * }` |
|       - | 4231 | ` * $str = 'This is a string, ';` |
|       - | 4232 | ` * add_some_extra($str);` |
|       - | 4233 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - | 4234 | ` * ?>` |
|       - | 4235 | ` *` |
|       - | 4236 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - | 4237 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - | 4238 | ` * on these extension.` |
|       - | 4239 | ` */` |
|   50474 | 4240 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 | 4241 |  |
|       - | 4242 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - | 4243 | `	SyToken *pIn;  /* Token stream */` |
|       - | 4244 | `	SyBlob sSig;         /* Function signature */` |
|       - | 4245 | `	char *zDup;          /* Copy of argument name */` |
|       - | 4246 | `	sxi32 rc;` |
|       - | 4247 |  |
|   50476 | 4248 | `	pIn = pGen->pIn;` |
|   50476 | 4249 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - | 4250 | `	/* Process arguments one after one */` |
|   63854 | 4251 | `	for(;;){` |
|  127710 | 4252 | `		if( pIn >= pEnd ){` |
|       - | 4253 | `			/* No more arguments to process */` |
|   50474 | 4254 | `			break;` |
|       - | 4255 | `		}` |
|   77238 | 4256 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   77238 | 4257 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 4258 | `		/* Detect nullable prefix '?' on type hints */` |
|   77238 | 4259 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_OP) && pIn->sData.nByte == 1 && pIn->sData.zString[0] == '?' ){` |
|      16 | 4260 | `			sArg.iFlags \|= VM_FUNC_ARG_NULLABLE;` |
|      16 | 4261 | `			pIn++;` |
|       7 | 4262 | `		}` |
|       - | 4263 | `		/* Skip leading namespace separator '\' on FQN type hints like \Throwable */` |
|   77238 | 4264 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       5 | 4265 | `			pIn++;` |
|       2 | 4266 | `		}` |
|   77238 | 4267 | `		if( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|   52574 | 4268 | `			if( pIn->nType & PH7_TK_KEYWORD ){` |
|   47314 | 4269 | `				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   47314 | 4270 | `				if( nKey & PH7_TKWRD_ARRAY ){` |
|     ! 0 | 4271 | `					sArg.nType = MEMOBJ_HASHMAP;` |
|   47314 | 4272 | `				}else if( nKey & PH7_TKWRD_BOOL ){` |
|     ! 0 | 4273 | `					sArg.nType = MEMOBJ_BOOL;` |
|   47314 | 4274 | `				}else if( nKey & PH7_TKWRD_INT ){` |
|   13142 | 4275 | `					sArg.nType = MEMOBJ_INT;` |
|   40744 | 4276 | `				}else if( nKey & PH7_TKWRD_STRING ){` |
|   34158 | 4277 | `					sArg.nType = MEMOBJ_STRING;` |
|   17096 | 4278 | `				}else if( nKey & PH7_TKWRD_FLOAT ){` |
|     ! 0 | 4279 | `					sArg.nType = MEMOBJ_REAL;` |
|      18 | 4280 | `				}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      16 | 4281 | `					sArg.nType = MEMOBJ_OBJ;` |
|       9 | 4282 | `				}else{` |
|       4 | 4283 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,` |
|       - | 4284 | `						"Invalid argument type '%z',Automatic cast will not be performed",` |
|       1 | 4285 | `						&pIn->sData);` |
|       - | 4286 | `				}` |
|   23658 | 4287 | `			}else{` |
|    5262 | 4288 | `				SyString *pName = &pIn->sData; /* Class name */` |
|       - | 4289 | `				char *zDupLocal;` |
|       - | 4290 | `				/* Argument must be a class instance,record that*/` |
|    5262 | 4291 | `				zDupLocal = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    5262 | 4292 | `				if( zDupLocal ){` |
|    5262 | 4293 | `					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */` |
|    5262 | 4294 | `					SyStringInitFromBuf(&sArg.sClass,zDupLocal,pName->nByte);` |
|    2630 | 4295 | `				}` |
|       - | 4296 | `			}` |
|   52574 | 4297 | `			pIn++;` |
|   26286 | 4298 | `		}` |
|   77238 | 4299 | `		if( pIn >= pEnd ){` |
|     ! 0 | 4300 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 | 4301 | `			return rc;` |
|       - | 4302 | `		}` |
|   77238 | 4303 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - | 4304 | `			/* Pass by reference,record that */` |
|    2652 | 4305 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2652 | 4306 | `			pIn++;` |
|    1325 | 4307 | `		}` |
|   77238 | 4308 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - | 4309 | `			/* Variadic parameter: ...$args */` |
|      28 | 4310 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      28 | 4311 | `			pIn++;` |
|      13 | 4312 | `		}` |
|   77238 | 4313 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4314 | `			/* Invalid argument */` |
|     ! 0 | 4315 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 | 4316 | `			return rc;` |
|       - | 4317 | `		}` |
|   77238 | 4318 | `		pIn++; /* Jump the dollar sign */` |
|       - | 4319 | `		/* Copy argument name */` |
|   77238 | 4320 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   77238 | 4321 | `		if( zDup == 0 ){` |
|     ! 0 | 4322 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 | 4323 | `			return SXERR_ABORT;` |
|       - | 4324 | `		}` |
|   77238 | 4325 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   77238 | 4326 | `		pIn++;` |
|   77238 | 4327 | `		if( pIn < pEnd ){` |
|   47786 | 4328 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - | 4329 | `				SyToken *pDefend;` |
|   42030 | 4330 | `				sxi32 iNest = 0;` |
|   42030 | 4331 | `				pIn++; /* Jump the equal sign */` |
|   42030 | 4332 | `				pDefend = pIn;` |
|       - | 4333 | `				/* Process the default value associated with this argument */` |
|   89308 | 4334 | `				while( pDefend < pEnd ){` |
|   68288 | 4335 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   21010 | 4336 | `						break;` |
|       - | 4337 | `					}` |
|   47280 | 4338 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - | 4339 | `						/* Increment nesting level */` |
|    2628 | 4340 | `						iNest++;` |
|   45967 | 4341 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - | 4342 | `						/* Decrement nesting level */` |
|    2628 | 4343 | `						iNest--;` |
|    1313 | 4344 | `					}` |
|   47280 | 4345 | `					pDefend++;` |
|       2 | 4346 | `				}` |
|   42030 | 4347 | `				if( pIn >= pDefend ){` |
|       3 | 4348 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 | 4349 | `					return rc;` |
|       - | 4350 | `				}` |
|       - | 4351 | `				/* Process default value */` |
|   42028 | 4352 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   42028 | 4353 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 4354 | `					return rc;` |
|       - | 4355 | `				}` |
|       - | 4356 | `				/* Point beyond the default value */` |
|   42028 | 4357 | `				pIn = pDefend;` |
|   21013 | 4358 | `			}` |
|   47784 | 4359 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 | 4360 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 | 4361 | `				return rc;` |
|       - | 4362 | `			}` |
|   47784 | 4363 | `			pIn++; /* Jump the trailing comma */` |
|   23891 | 4364 | `		}` |
|       - | 4365 | `		/* Append argument signature */` |
|   77236 | 4366 | `		if( sArg.nType > 0 ){` |
|   52572 | 4367 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - | 4368 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5262 | 4369 | `				int marker = 'o';` |
|    5262 | 4370 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5262 | 4371 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2632 | 4372 | `			}else{` |
|       - | 4373 | `				int c;` |
|   47312 | 4374 | `				c = 'n'; /* cc warning */` |
|       - | 4375 | `				/* Type leading character */` |
|   47312 | 4376 | `				switch(sArg.nType){` |
|     ! 0 | 4377 | `				case MEMOBJ_HASHMAP:` |
|       - | 4378 | `					/* Hashmap aka 'array' */` |
|     ! 0 | 4379 | `					c = 'h';` |
|     ! 0 | 4380 | `					break;` |
|    6570 | 4381 | `				case MEMOBJ_INT:` |
|       - | 4382 | `					/* Integer */` |
|   13142 | 4383 | `					c = 'i';` |
|   13142 | 4384 | `					break;` |
|     ! 0 | 4385 | `				case MEMOBJ_BOOL:` |
|       - | 4386 | `					/* Bool */` |
|     ! 0 | 4387 | `					c = 'b';` |
|     ! 0 | 4388 | `					break;` |
|     ! 0 | 4389 | `				case MEMOBJ_REAL:` |
|       - | 4390 | `					/* Float */` |
|     ! 0 | 4391 | `					c = 'f';` |
|     ! 0 | 4392 | `					break;` |
|   17078 | 4393 | `				case MEMOBJ_STRING:` |
|       - | 4394 | `					/* String */` |
|   34158 | 4395 | `					c = 's';` |
|   34158 | 4396 | `					break;` |
|       7 | 4397 | `				case MEMOBJ_OBJ:` |
|       - | 4398 | `					/* Object */` |
|      16 | 4399 | `					c = 'o';` |
|      14 | 4400 | `					break;` |
|     ! 0 | 4401 | `				default:` |
|     ! 0 | 4402 | `					break;` |
|       - | 4403 | `				}` |
|   47312 | 4404 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - | 4405 | `			}` |
|   26287 | 4406 | `		}else{` |
|       - | 4407 | `			/* No type is associated with this parameter which mean` |
|       - | 4408 | `			 * that this function is not condidate for overloading.` |
|       - | 4409 | `			 */` |
|   24666 | 4410 | `			SyBlobRelease(&sSig);` |
|       - | 4411 | `		}` |
|       - | 4412 | `		/* Save in the argument set */` |
|   77236 | 4413 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 | 4414 | `	}` |
|   50474 | 4415 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - | 4416 | `		/* Save function signature */` |
|   31558 | 4417 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   15778 | 4418 | `	}` |
|   50474 | 4419 | `	return SXRET_OK;` |
|   25239 | 4420 |  |
|       - | 4421 | `/*` |
|       - | 4422 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - | 4423 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - | 4424 | ` * and this routine takes care of generating the appropriate error message.` |
|       - | 4425 | ` */` |
|  140284 | 4426 | `static sxi32 GenStateCompileFuncBody(` |
|       - | 4427 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 4428 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - | 4429 | `	)` |
|       2 | 4430 |  |
|       - | 4431 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - | 4432 | `	GenBlock *pBlock;` |
|       - | 4433 | `	sxu32 nGotoOfft;` |
|       - | 4434 | `	sxi32 rc;` |
|       - | 4435 | `	/* Attach the new function */` |
|  140286 | 4436 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  140286 | 4437 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4438 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - | 4439 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4440 | `		return SXERR_ABORT;` |
|       - | 4441 | `	}` |
|  140286 | 4442 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - | 4443 | `	/* Swap bytecode containers */` |
|  140286 | 4444 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  140286 | 4445 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - | 4446 | `	/* Compile the body */` |
|  140286 | 4447 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 4448 | `	/* Fix exception jumps now the destination is resolved */` |
|  140286 | 4449 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 4450 | `	/* Emit the final return if not yet done */` |
|  140286 | 4451 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 4452 | `	/* Fix gotos jumps now the destination is resolved */` |
|  140286 | 4453 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 | 4454 | `		rc = SXERR_ABORT;` |
|     ! 0 | 4455 | `	}` |
|  140286 | 4456 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - | 4457 | `	/* Restore the default container */` |
|  140286 | 4458 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 4459 | `	/* Leave function block */` |
|  140286 | 4460 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  140286 | 4461 | `	if( rc == SXERR_ABORT ){` |
|       - | 4462 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4463 | `		return SXERR_ABORT;` |
|       - | 4464 | `	}` |
|       - | 4465 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - | 4466 | `	{` |
|  140286 | 4467 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - | 4468 | `		sxu32 i;` |
| 2912530 | 4469 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2772262 | 4470 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 | 4471 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 | 4472 | `				break;` |
|       - | 4473 | `			}` |
| 1386124 | 4474 | `		}` |
|       - | 4475 | `	}` |
|       - | 4476 | `	/* All done, function body compiled */` |
|  140286 | 4477 | `	return SXRET_OK;` |
|   70144 | 4478 |  |
|       - | 4479 | `/*` |
|       - | 4480 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - | 4481 | ` * According to the PHP language reference manual.` |
|       - | 4482 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - | 4483 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - | 4484 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - | 4485 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 4486 | ` *  Functions need not be defined before they are referenced.` |
|       - | 4487 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - | 4488 | ` *  a function even if they were defined inside and vice versa.` |
|       - | 4489 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - | 4490 | ` *  calls with over 32-64 recursion levels.` |
|       - | 4491 | ` *` |
|       - | 4492 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - | 4493 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - | 4494 | ` * on these extension.` |
|       - | 4495 | ` */` |
|       - | 4496 | `/*` |
|       - | 4497 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - | 4498 | ` */` |
|       6 | 4499 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       1 | 4500 |  |
|       - | 4501 | `	sxu32 i;` |
|      31 | 4502 | `	for( i = 0; i < n; i++ ){` |
|      25 | 4503 | `		int a = zA[i], b = zB[i];` |
|      25 | 4504 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|      25 | 4505 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|      25 | 4506 | `		if( a != b ) return a - b;` |
|      13 | 4507 | `	}` |
|       7 | 4508 | `	return 0;` |
|       4 | 4509 |  |
|       - | 4510 | `/*` |
|       - | 4511 | ` * Helper: set the return type to a class/self/parent/static sentinel.` |
|       - | 4512 | ` */` |
|       2 | 4513 | `static void GenStateSetReturnClass(ph7_gen_state *pGen, ph7_vm_func *pFunc, const char *zName, sxu32 nByte)` |
|       1 | 4514 |  |
|       3 | 4515 | `	char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator, zName, nByte);` |
|       3 | 4516 | `	if( zDup ){` |
|       3 | 4517 | `		pFunc->nReturnType = SXU32_HIGH;` |
|       3 | 4518 | `		SyStringInitFromBuf(&pFunc->sReturnClass, zDup, nByte);` |
|       1 | 4519 | `	}` |
|       3 | 4520 |  |
|       - | 4521 | `/*` |
|       - | 4522 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - | 4523 | `` * pGen->pIn should point to the token after `)`.`` |
|       - | 4524 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - | 4525 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - | 4526 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, and nullable `: ?type`.`` |
|       - | 4527 | ` */` |
|  161340 | 4528 | `static void GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 | 4529 |  |
|  161342 | 4530 | `	SyToken *pCur = pGen->pIn;` |
|  161342 | 4531 | `	pFunc->nReturnType = 0;` |
|  161342 | 4532 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  161342 | 4533 | `	if( pCur >= pGen->pEnd \|\| (pCur->nType & PH7_TK_COLON) == 0 ){` |
|  161278 | 4534 | `		return; /* No return type */` |
|       - | 4535 | `	}` |
|      66 | 4536 | `	pCur++; /* Skip ':' */` |
|      66 | 4537 | `	if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4538 | `		pGen->pIn = pCur;` |
|     ! 0 | 4539 | `		return;` |
|       - | 4540 | `	}` |
|       - | 4541 | `	/* Handle nullable prefix '?' (tokenized as PH7_TK_OP with '?' operator) */` |
|      66 | 4542 | `	if( (pCur->nType & PH7_TK_OP) && pCur->sData.nByte == 1 && pCur->sData.zString[0] == '?' ){` |
|       7 | 4543 | `		pCur++;` |
|       7 | 4544 | `		if( pCur >= pGen->pEnd ){` |
|     ! 0 | 4545 | `			pGen->pIn = pCur;` |
|     ! 0 | 4546 | `			return;` |
|       - | 4547 | `		}` |
|       3 | 4548 | `	}` |
|      66 | 4549 | `	if( pCur->nType & PH7_TK_KEYWORD ){` |
|      60 | 4550 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pCur->pUserData));` |
|      60 | 4551 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|       3 | 4552 | `			pFunc->nReturnType = MEMOBJ_HASHMAP;` |
|      59 | 4553 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       3 | 4554 | `			pFunc->nReturnType = MEMOBJ_BOOL;` |
|      57 | 4555 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|      20 | 4556 | `			pFunc->nReturnType = MEMOBJ_INT;` |
|      47 | 4557 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|      32 | 4558 | `			pFunc->nReturnType = MEMOBJ_STRING;` |
|      23 | 4559 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|       3 | 4560 | `			pFunc->nReturnType = MEMOBJ_REAL;` |
|       7 | 4561 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|       3 | 4562 | `			pFunc->nReturnType = MEMOBJ_OBJ;` |
|       4 | 4563 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT \|\| nKey == PH7_TKWRD_STATIC ){` |
|       - | 4564 | `			/* self/parent/static — store as class sentinel */` |
|       3 | 4565 | `			GenStateSetReturnClass(pGen, pFunc, pCur->sData.zString, pCur->sData.nByte);` |
|       1 | 4566 | `		}` |
|      60 | 4567 | `		pCur++;` |
|      36 | 4568 | `	}else if( pCur->nType & PH7_TK_ID ){` |
|       7 | 4569 | `		SyString *pType = &pCur->sData;` |
|       7 | 4570 | `		if( pType->nByte == 4 && SyMemcmpNoCase(pType->zString, "void", 4) == 0 ){` |
|       7 | 4571 | `			pFunc->nReturnType = MEMOBJ_VOID;` |
|       4 | 4572 | `		}else{` |
|       - | 4573 | `			/* Class/interface name */` |
|     ! 0 | 4574 | `			GenStateSetReturnClass(pGen, pFunc, pType->zString, pType->nByte);` |
|       - | 4575 | `		}` |
|       7 | 4576 | `		pCur++;` |
|       3 | 4577 | `	}` |
|      66 | 4578 | `	pGen->pIn = pCur;` |
|   80672 | 4579 |  |
|       - | 4580 |  |
|   34822 | 4581 | `static sxi32 GenStateCompileFunc(` |
|       - | 4582 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 4583 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - | 4584 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 4585 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - | 4586 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - | 4587 | `	)` |
|       2 | 4588 |  |
|       - | 4589 | `	ph7_vm_func *pFunc;` |
|       - | 4590 | `	SyToken *pEnd;` |
|       - | 4591 | `	sxu32 nLine;` |
|       - | 4592 | `	char *zName;` |
|       - | 4593 | `	sxi32 rc;` |
|       - | 4594 | `	/* Extract line number */` |
|   34824 | 4595 | `	nLine = pGen->pIn->nLine;` |
|       - | 4596 | `	/* Jump the left parenthesis '(' */` |
|   34824 | 4597 | `	pGen->pIn++;` |
|       - | 4598 | `	/* Delimit the function signature */` |
|   34824 | 4599 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   34824 | 4600 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 4601 | `		/* Syntax error */` |
|       7 | 4602 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 | 4603 | `		if( rc == SXERR_ABORT ){` |
|       - | 4604 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4605 | `			return SXERR_ABORT;` |
|       - | 4606 | `		}` |
|       7 | 4607 | `		pGen->pIn = pGen->pEnd;` |
|       7 | 4608 | `		return SXRET_OK;` |
|       - | 4609 | `	}` |
|       - | 4610 | `	/* Create the function state */` |
|   34818 | 4611 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   34818 | 4612 | `	if( pFunc == 0 ){` |
|     ! 0 | 4613 | `		goto OutOfMem;` |
|       - | 4614 | `	}` |
|       - | 4615 | `	/* Build the function name, prepending namespace if active */` |
|   34825 | 4616 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - | 4617 | `		SyBlob sFQN;` |
|       - | 4618 | `		sxu32 nLen;` |
|      16 | 4619 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 | 4620 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 | 4621 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 | 4622 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 | 4623 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 | 4624 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 | 4625 | `		SyBlobRelease(&sFQN);` |
|      16 | 4626 | `		if( zName == 0 ){` |
|     ! 0 | 4627 | `			goto OutOfMem;` |
|       - | 4628 | `		}` |
|      16 | 4629 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 | 4630 | `	}else{` |
|   34804 | 4631 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   34804 | 4632 | `		if( zName == 0 ){` |
|     ! 0 | 4633 | `			goto OutOfMem;` |
|       - | 4634 | `		}` |
|   34804 | 4635 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - | 4636 | `	}` |
|   34818 | 4637 | `	if( pGen->pIn < pEnd ){` |
|       - | 4638 | `		/* Collect function arguments */` |
|   24134 | 4639 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   24134 | 4640 | `		if( rc == SXERR_ABORT ){` |
|       - | 4641 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4642 | `			return SXERR_ABORT;` |
|       - | 4643 | `		}` |
|   12066 | 4644 | `	}` |
|       - | 4645 | `	/* Point past ')' and parse optional return type ': type' */` |
|   34818 | 4646 | `	pGen->pIn = &pEnd[1];` |
|   34818 | 4647 | `	GenStateParseReturnType(pGen, pFunc);` |
|   34818 | 4648 | `	if( bHandleClosure ){` |
|       - | 4649 | `		ph7_vm_func_closure_env sEnv;` |
|     168 | 4650 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     166 | 4651 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      91 | 4652 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      14 | 4653 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 4654 | `				/* Closure,record environment variable */` |
|      14 | 4655 | `				pGen->pIn++;` |
|      14 | 4656 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 | 4657 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 | 4658 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 4659 | `						return SXERR_ABORT;` |
|       - | 4660 | `					}` |
|     ! 0 | 4661 | `				}` |
|      14 | 4662 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - | 4663 | `				/* Compile until we hit the first closing parenthesis */` |
|      28 | 4664 | `				while( pGen->pIn < pGen->pEnd ){` |
|      28 | 4665 | `					int iFlagsLocal = 0;` |
|      28 | 4666 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      14 | 4667 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      14 | 4668 | `						break;` |
|       - | 4669 | `					}` |
|      16 | 4670 | `					nLineLocal = pGen->pIn->nLine;` |
|      16 | 4671 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - | 4672 | `						/* Pass by reference,record that */` |
|     ! 0 | 4673 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - | 4674 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - | 4675 | `							);` |
|     ! 0 | 4676 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 | 4677 | `						pGen->pIn++;` |
|     ! 0 | 4678 | `					}` |
|      14 | 4679 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      16 | 4680 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 | 4681 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - | 4682 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 | 4683 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 4684 | `								return SXERR_ABORT;` |
|       - | 4685 | `							}` |
|       - | 4686 | `							/* Find the closing parenthesis */` |
|     ! 0 | 4687 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 | 4688 | `								pGen->pIn++;` |
|     ! 0 | 4689 | `							}` |
|     ! 0 | 4690 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 | 4691 | `								pGen->pIn++;` |
|     ! 0 | 4692 | `							}` |
|     ! 0 | 4693 | `							break;` |
|       - | 4694 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 | 4695 | `					}else{` |
|       - | 4696 | `						SyString *pNameLocal;` |
|       - | 4697 | `						char *zDup;` |
|       - | 4698 | `						/* Duplicate variable name */` |
|      16 | 4699 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      16 | 4700 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      16 | 4701 | `						if( zDup ){` |
|       - | 4702 | `							/* Zero the structure */` |
|      16 | 4703 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 | 4704 | `							sEnv.iFlags = iFlagsLocal;` |
|      16 | 4705 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 | 4706 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      16 | 4707 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 | 4708 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 | 4709 | `									got_this = 1;` |
|     ! 0 | 4710 | `							}` |
|       - | 4711 | `							/* Save imported variable */` |
|      16 | 4712 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 | 4713 | `						}else{` |
|     ! 0 | 4714 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4715 | `							 return SXERR_ABORT;` |
|       - | 4716 | `						}` |
|       - | 4717 | `					}` |
|      16 | 4718 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      18 | 4719 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4720 | `						/* Ignore trailing commas */` |
|       3 | 4721 | `						pGen->pIn++;` |
|       1 | 4722 | `					}` |
|       2 | 4723 | `				}` |
|      14 | 4724 | `				if( !got_this ){` |
|       - | 4725 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - | 4726 | `					 * available to the closure environment.` |
|       - | 4727 | `					 */` |
|      14 | 4728 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      14 | 4729 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      14 | 4730 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      14 | 4731 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      14 | 4732 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       6 | 4733 | `				}` |
|      14 | 4734 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - | 4735 | `					/* Mark as closure */` |
|      14 | 4736 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       6 | 4737 | `				}` |
|       6 | 4738 | `		}` |
|      83 | 4739 | `	}` |
|       - | 4740 | `	/* Compile the body */` |
|   34818 | 4741 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   34818 | 4742 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 4743 | `		return SXERR_ABORT;` |
|       - | 4744 | `	}` |
|   34818 | 4745 | `	if( ppFunc ){` |
|     168 | 4746 | `		*ppFunc = pFunc;` |
|      83 | 4747 | `	}` |
|   34818 | 4748 | `	rc = SXRET_OK;` |
|   34818 | 4749 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - | 4750 | `		/* Finally register the function */` |
|   34806 | 4751 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   17402 | 4752 | `	}` |
|   34818 | 4753 | `	if( rc == SXRET_OK ){` |
|   34818 | 4754 | `		return SXRET_OK;` |
|       - | 4755 | `	}` |
|       - | 4756 | `	/* Fall through if something goes wrong */` |
|     ! 0 | 4757 | `OutOfMem:` |
|       - | 4758 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - | 4759 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - | 4760 | `	 */` |
|     ! 0 | 4761 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 | 4762 | `	return SXERR_ABORT;` |
|   17413 | 4763 |  |
|       - | 4764 | `/*` |
|       - | 4765 | ` * Compile a standard PHP function.` |
|       - | 4766 | ` *  Refer to the block-comment above for more information.` |
|       - | 4767 | ` */` |
|   34662 | 4768 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 | 4769 |  |
|       - | 4770 | `	SyString *pName;` |
|       - | 4771 | `	sxi32 iFlags;` |
|       - | 4772 | `	sxu32 nLine;` |
|       - | 4773 | `	sxi32 rc;` |
|       - | 4774 |  |
|   34664 | 4775 | `	nLine = pGen->pIn->nLine;` |
|   34664 | 4776 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   34664 | 4777 | `	iFlags = 0;` |
|   34664 | 4778 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 4779 | `		/* Return by reference,remember that */` |
|       7 | 4780 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 4781 | `		/* Jump the '&' token */` |
|       7 | 4782 | `		pGen->pIn++;` |
|       3 | 4783 | `	}` |
|   34664 | 4784 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 4785 | `		/* Invalid function name */` |
|       5 | 4786 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 | 4787 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4788 | `			return SXERR_ABORT;` |
|       - | 4789 | `		}` |
|       - | 4790 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 | 4791 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 | 4792 | `			pGen->pIn++;` |
|       1 | 4793 | `		}` |
|       5 | 4794 | `		return SXRET_OK;` |
|       - | 4795 | `	}` |
|   34660 | 4796 | `	pName = &pGen->pIn->sData;` |
|   34660 | 4797 | `	nLine = pGen->pIn->nLine;` |
|       - | 4798 | `	/* Jump the function name */` |
|   34660 | 4799 | `	pGen->pIn++;` |
|   34660 | 4800 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 4801 | `		/* Syntax error */` |
|       3 | 4802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 | 4803 | `		if( rc == SXERR_ABORT ){` |
|       - | 4804 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4805 | `			return SXERR_ABORT;` |
|       - | 4806 | `		}` |
|       - | 4807 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 | 4808 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 | 4809 | `			pGen->pIn++;` |
|     ! 0 | 4810 | `		}` |
|       3 | 4811 | `		return SXRET_OK;` |
|       - | 4812 | `	}` |
|       - | 4813 | `	/* Compile function body */` |
|   34658 | 4814 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   34658 | 4815 | `	return rc;` |
|   17333 | 4816 |  |
|       - | 4817 | `/*` |
|       - | 4818 | ` * Extract the visibility level associated with a given keyword.` |
|       - | 4819 | ` * According to the PHP language reference manual` |
|       - | 4820 | ` *  Visibility:` |
|       - | 4821 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - | 4822 | ` *  the declaration with the keywords public, protected or private.` |
|       - | 4823 | ` *  Class members declared public can be accessed everywhere.` |
|       - | 4824 | ` *  Members declared protected can be accessed only within the class` |
|       - | 4825 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - | 4826 | ` *  may only be accessed by the class that defines the member.` |
|       - | 4827 | ` */` |
|  160912 | 4828 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 | 4829 |  |
|  160914 | 4830 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    7944 | 4831 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  152972 | 4832 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   18426 | 4833 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - | 4834 | `	}` |
|       - | 4835 | `	/* Assume public by default */` |
|  134548 | 4836 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   80458 | 4837 |  |
|       - | 4838 | `/*` |
|       - | 4839 | ` * Compile a class constant.` |
|       - | 4840 | ` * According to the PHP language reference manual` |
|       - | 4841 | ` *  Class Constants` |
|       - | 4842 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - | 4843 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - | 4844 | ` *   you don't use the $ symbol to declare or use them.` |
|       - | 4845 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - | 4846 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - | 4847 | ` *   It's also possible for interfaces to have constants.` |
|       - | 4848 | ` * Symisc eXtension.` |
|       - | 4849 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - | 4850 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4851 | ` *  Example:` |
|       - | 4852 | ` *   class Test{` |
|       - | 4853 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4854 | ` *   };` |
|       - | 4855 | ` *   var_dump(TEST::MyConst);` |
|       - | 4856 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4857 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4858 | ` */` |
|      30 | 4859 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4860 |  |
|      32 | 4861 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4862 | `	SySet *pInstrContainer;` |
|       - | 4863 | `	ph7_class_attr *pCons;` |
|       - | 4864 | `	SyString *pName;` |
|       - | 4865 | `	sxi32 rc;` |
|       - | 4866 | `	/* Extract visibility level */` |
|      32 | 4867 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 | 4868 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 | 4869 | `loop:` |
|       - | 4870 | `	/* Mark as constant */` |
|      32 | 4871 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 | 4872 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 4873 | `		/* Invalid constant name */` |
|     ! 0 | 4874 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 | 4875 | `		if( rc == SXERR_ABORT ){` |
|       - | 4876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4877 | `			return SXERR_ABORT;` |
|       - | 4878 | `		}` |
|     ! 0 | 4879 | `		goto Synchronize;` |
|       - | 4880 | `	}` |
|       - | 4881 | `	/* Peek constant name */` |
|      32 | 4882 | `	pName = &pGen->pIn->sData;` |
|       - | 4883 | `	/* Make sure the constant name isn't reserved */` |
|      32 | 4884 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - | 4885 | `		/* Reserved constant name */` |
|     ! 0 | 4886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 | 4887 | `		if( rc == SXERR_ABORT ){` |
|       - | 4888 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4889 | `			return SXERR_ABORT;` |
|       - | 4890 | `		}` |
|     ! 0 | 4891 | `		goto Synchronize;` |
|       - | 4892 | `	}` |
|       - | 4893 | `	/* Advance the stream cursor */` |
|      32 | 4894 | `	pGen->pIn++;` |
|      32 | 4895 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - | 4896 | `		/* Invalid declaration */` |
|     ! 0 | 4897 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 | 4898 | `		if( rc == SXERR_ABORT ){` |
|       - | 4899 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 4900 | `			return SXERR_ABORT;` |
|       - | 4901 | `		}` |
|     ! 0 | 4902 | `		goto Synchronize;` |
|       - | 4903 | `	}` |
|      32 | 4904 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - | 4905 | `	/* Allocate a new class attribute */` |
|      32 | 4906 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 | 4907 | `	if( pCons == 0 ){` |
|     ! 0 | 4908 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4909 | `		return SXERR_ABORT;` |
|       - | 4910 | `	}` |
|       - | 4911 | `	/* Swap bytecode container */` |
|      32 | 4912 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 4913 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - | 4914 | `	/* Compile constant value.` |
|       - | 4915 | `	 */` |
|      32 | 4916 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 | 4917 | `	if( rc == SXERR_EMPTY ){` |
|       3 | 4918 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 | 4919 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 4920 | `			return SXERR_ABORT;` |
|       - | 4921 | `		}` |
|       1 | 4922 | `	}` |
|       - | 4923 | `	/* Emit the done instruction */` |
|      32 | 4924 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 | 4925 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 4926 | `	if( rc == SXERR_ABORT ){` |
|       - | 4927 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 | 4928 | `		return SXERR_ABORT;` |
|       - | 4929 | `	}` |
|       - | 4930 | `	/* All done,install the constant */` |
|      32 | 4931 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 | 4932 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 4933 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 4934 | `		return SXERR_ABORT;` |
|       - | 4935 | `	}` |
|      32 | 4936 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 4937 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 | 4938 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 4939 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 4940 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 4941 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 4942 | `				pTok--;` |
|     ! 0 | 4943 | `			}` |
|     ! 0 | 4944 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 4945 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 | 4946 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 4947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 4948 | `				return SXERR_ABORT;` |
|       - | 4949 | `			}` |
|     ! 0 | 4950 | `		}else{` |
|     ! 0 | 4951 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 | 4952 | `				goto loop;` |
|       - | 4953 | `			}` |
|       - | 4954 | `		}` |
|     ! 0 | 4955 | `	}` |
|      32 | 4956 | `	return SXRET_OK;` |
|     ! 0 | 4957 | `Synchronize:` |
|       - | 4958 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 4959 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 | 4960 | `		pGen->pIn++;` |
|     ! 0 | 4961 | `	}` |
|     ! 0 | 4962 | `	return SXERR_CORRUPT;` |
|      17 | 4963 |  |
|       - | 4964 | `/*` |
|       - | 4965 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - | 4966 | ` * According to the PHP language reference manual` |
|       - | 4967 | ` *  Properties` |
|       - | 4968 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - | 4969 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - | 4970 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - | 4971 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - | 4972 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - | 4973 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - | 4974 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - | 4975 | ` * Symisc eXtension.` |
|       - | 4976 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - | 4977 | ` *  the zend engine would allow only simple scalar value.` |
|       - | 4978 | ` *  Example:` |
|       - | 4979 | ` *   class Test{` |
|       - | 4980 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - | 4981 | ` *   };` |
|       - | 4982 | ` *   var_dump(TEST::myVar);` |
|       - | 4983 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - | 4984 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - | 4985 | ` */` |
|   34356 | 4986 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 | 4987 |  |
|   34358 | 4988 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 4989 | `	ph7_class_attr *pAttr;` |
|       - | 4990 | `	SyString *pName;` |
|       - | 4991 | `	sxi32 rc;` |
|       - | 4992 | `	/* Extract visibility level */` |
|   34358 | 4993 | `	iProtection = GetProtectionLevel(iProtection);` |
|   17178 | 4994 | `loop:` |
|   34358 | 4995 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   34358 | 4996 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - | 4997 | `		/* Invalid attribute name */` |
|     ! 0 | 4998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 | 4999 | `		if( rc == SXERR_ABORT ){` |
|       - | 5000 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5001 | `			return SXERR_ABORT;` |
|       - | 5002 | `		}` |
|     ! 0 | 5003 | `		goto Synchronize;` |
|       - | 5004 | `	}` |
|       - | 5005 | `	/* Peek attribute name */` |
|   34358 | 5006 | `	pName = &pGen->pIn->sData;` |
|       - | 5007 | `	/* Advance the stream cursor */` |
|   34358 | 5008 | `	pGen->pIn++;` |
|   34358 | 5009 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - | 5010 | `		/* Invalid declaration */` |
|       3 | 5011 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 | 5012 | `		if( rc == SXERR_ABORT ){` |
|       - | 5013 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5014 | `			return SXERR_ABORT;` |
|       - | 5015 | `		}` |
|       3 | 5016 | `		goto Synchronize;` |
|       - | 5017 | `	}` |
|       - | 5018 | `	/* Allocate a new class attribute */` |
|   34356 | 5019 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|   34356 | 5020 | `	if( pAttr == 0 ){` |
|     ! 0 | 5021 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 5022 | `		return SXERR_ABORT;` |
|       - | 5023 | `	}` |
|   34356 | 5024 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - | 5025 | `		SySet *pInstrContainer;` |
|   10672 | 5026 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - | 5027 | `		/* Swap bytecode container */` |
|   10672 | 5028 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10672 | 5029 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - | 5030 | `		/* Compile attribute value.` |
|       - | 5031 | `		 */` |
|   10672 | 5032 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   10672 | 5033 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 | 5034 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 | 5035 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5036 | `				return SXERR_ABORT;` |
|       - | 5037 | `			}` |
|     ! 0 | 5038 | `		}` |
|       - | 5039 | `		/* Emit the done instruction */` |
|   10672 | 5040 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   10672 | 5041 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5335 | 5042 | `	}` |
|       - | 5043 | `	/* All done,install the attribute */` |
|   34356 | 5044 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   34356 | 5045 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5046 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5047 | `		return SXERR_ABORT;` |
|       - | 5048 | `	}` |
|   34356 | 5049 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - | 5050 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|     ! 0 | 5051 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 | 5052 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 | 5053 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 | 5054 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 | 5055 | `				pTok--;` |
|     ! 0 | 5056 | `			}` |
|     ! 0 | 5057 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5058 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5059 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 | 5060 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5061 | `				return SXERR_ABORT;` |
|       - | 5062 | `			}` |
|     ! 0 | 5063 | `		}else{` |
|     ! 0 | 5064 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 | 5065 | `				goto loop;` |
|       - | 5066 | `			}` |
|       - | 5067 | `		}` |
|     ! 0 | 5068 | `	}` |
|   34356 | 5069 | `	return SXRET_OK;` |
|       1 | 5070 | `Synchronize:` |
|       - | 5071 | `	/* Synchronize with the first semi-colon */` |
|       5 | 5072 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 | 5073 | `		pGen->pIn++;` |
|       1 | 5074 | `	}` |
|       3 | 5075 | `	return SXERR_CORRUPT;` |
|   17180 | 5076 |  |
|       - | 5077 | `/*` |
|       - | 5078 | ` * Compile a class method.` |
|       - | 5079 | ` *` |
|       - | 5080 | ` * Refer to the official documentation for more information` |
|       - | 5081 | ` * on the powerful extension introduced by the PH7 engine` |
|       - | 5082 | ` * to the OO subsystem such as full type hinting,method` |
|       - | 5083 | ` * overloading and many more.` |
|       - | 5084 | ` */` |
|  126526 | 5085 | `static sxi32 GenStateCompileClassMethod(` |
|       - | 5086 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 5087 | `	sxi32 iProtection,   /* Visibility level */` |
|       - | 5088 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - | 5089 | `	int doBody,          /* TRUE to process method body */` |
|       - | 5090 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - | 5091 | `	)` |
|       2 | 5092 |  |
|  126528 | 5093 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5094 | `	ph7_class_method *pMeth;` |
|       - | 5095 | `	sxi32 iFuncFlags;` |
|       - | 5096 | `	SyString *pName;` |
|       - | 5097 | `	SyToken *pEnd;` |
|       - | 5098 | `	sxi32 rc;` |
|       - | 5099 | `	/* Extract visibility level */` |
|  126528 | 5100 | `	iProtection = GetProtectionLevel(iProtection);` |
|  126528 | 5101 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  126528 | 5102 | `	iFuncFlags = 0;` |
|  126528 | 5103 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5104 | `		/* Invalid method name */` |
|     ! 0 | 5105 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5106 | `		if( rc == SXERR_ABORT ){` |
|       - | 5107 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5108 | `			return SXERR_ABORT;` |
|       - | 5109 | `		}` |
|     ! 0 | 5110 | `		goto Synchronize;` |
|       - | 5111 | `	}` |
|  126528 | 5112 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - | 5113 | `		/* Return by reference,remember that */` |
|     ! 0 | 5114 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - | 5115 | `		/* Jump the '&' token */` |
|     ! 0 | 5116 | `		pGen->pIn++;` |
|     ! 0 | 5117 | `	}` |
|  126528 | 5118 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 5119 | `		/* Invalid method name */` |
|     ! 0 | 5120 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 | 5121 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5122 | `			return SXERR_ABORT;` |
|       - | 5123 | `		}` |
|     ! 0 | 5124 | `		goto Synchronize;` |
|       - | 5125 | `	}` |
|       - | 5126 | `	/* Peek method name */` |
|  126528 | 5127 | `	pName = &pGen->pIn->sData;` |
|  126528 | 5128 | `	nLine = pGen->pIn->nLine;` |
|       - | 5129 | `	/* Jump the method name */` |
|  126528 | 5130 | `	pGen->pIn++;` |
|  126528 | 5131 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - | 5132 | `		/* Abstract method */` |
|   21058 | 5133 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 | 5134 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5135 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 | 5136 | `				&pClass->sName,pName);` |
|     ! 0 | 5137 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5138 | `				return SXERR_ABORT;` |
|       - | 5139 | `			}` |
|     ! 0 | 5140 | `		}` |
|       - | 5141 | `		/* Assemble method signature only */` |
|   21058 | 5142 | `		doBody = FALSE;` |
|   10528 | 5143 | `	}` |
|  126528 | 5144 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 5145 | `		/* Syntax error */` |
|     ! 0 | 5146 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 | 5147 | `		if( rc == SXERR_ABORT ){` |
|       - | 5148 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5149 | `			return SXERR_ABORT;` |
|       - | 5150 | `		}` |
|     ! 0 | 5151 | `		goto Synchronize;` |
|       - | 5152 | `	}` |
|       - | 5153 | `	/* Allocate a new class_method instance */` |
|  126528 | 5154 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  126528 | 5155 | `	if( pMeth == 0 ){` |
|     ! 0 | 5156 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5157 | `		return SXERR_ABORT;` |
|       - | 5158 | `	}` |
|       - | 5159 | `	/* Jump the left parenthesis '(' */` |
|  126528 | 5160 | `	pGen->pIn++;` |
|  126528 | 5161 | `	pEnd = 0; /* cc warning */` |
|       - | 5162 | `	/* Delimit the method signature */` |
|  126528 | 5163 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  126528 | 5164 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5165 | `		/* Syntax error */` |
|       3 | 5166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 | 5167 | `		if( rc == SXERR_ABORT ){` |
|       - | 5168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5169 | `			return SXERR_ABORT;` |
|       - | 5170 | `		}` |
|       3 | 5171 | `		goto Synchronize;` |
|       - | 5172 | `	}` |
|  126526 | 5173 | `	if( pGen->pIn < pEnd ){` |
|       - | 5174 | `		/* Collect method arguments */` |
|   26344 | 5175 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   26344 | 5176 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5177 | `			return SXERR_ABORT;` |
|       - | 5178 | `		}` |
|   13171 | 5179 | `	}` |
|       - | 5180 | `	/* Point past ')' and parse optional return type ': type' */` |
|  126526 | 5181 | `	pGen->pIn = &pEnd[1];` |
|  126526 | 5182 | `	GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  126526 | 5183 | `	if( doBody ){` |
|       - | 5184 | `		/* Compile method body */` |
|  105470 | 5185 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  105470 | 5186 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 5187 | `			return SXERR_ABORT;` |
|       - | 5188 | `		}` |
|   52736 | 5189 | `	}else{` |
|       - | 5190 | `		/* Only method signature is allowed */` |
|   21058 | 5191 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 | 5192 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5193 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 | 5194 | `				if( rc == SXERR_ABORT ){` |
|       - | 5195 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5196 | `					return SXERR_ABORT;` |
|       - | 5197 | `				}` |
|     ! 0 | 5198 | `				return SXERR_CORRUPT;` |
|       - | 5199 | `			}` |
|       - | 5200 | `	}` |
|       - | 5201 | `	/* All done,install the method */` |
|  126526 | 5202 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  126526 | 5203 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5204 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5205 | `		return SXERR_ABORT;` |
|       - | 5206 | `	}` |
|  126526 | 5207 | `	return SXRET_OK;` |
|       1 | 5208 | `Synchronize:` |
|       - | 5209 | `	/* Synchronize with the first semi-colon */` |
|       7 | 5210 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 | 5211 | `		pGen->pIn++;` |
|       1 | 5212 | `	}` |
|       3 | 5213 | `	return SXERR_CORRUPT;` |
|   63265 | 5214 |  |
|       - | 5215 | `/*` |
|       - | 5216 | ` * Compile an object interface.` |
|       - | 5217 | ` *  According to the PHP language reference manual` |
|       - | 5218 | ` *   Object Interfaces:` |
|       - | 5219 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - | 5220 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - | 5221 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - | 5222 | ` *   class, but without any of the methods having their contents defined.` |
|       - | 5223 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - | 5224 | ` */` |
|    7916 | 5225 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 | 5226 |  |
|    7918 | 5227 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5228 | `	ph7_class *pClass,*pBase;` |
|       - | 5229 | `	SyToken *pEnd,*pTmp;` |
|       - | 5230 | `	SyString *pName;` |
|       - | 5231 | `	sxi32 nKwrd;` |
|       - | 5232 | `	sxi32 rc;` |
|       - | 5233 | `	/* Jump the 'interface' keyword */` |
|    7918 | 5234 | `	pGen->pIn++;` |
|       - | 5235 | `	/* Extract interface name */` |
|    7918 | 5236 | `	pName = &pGen->pIn->sData;` |
|       - | 5237 | `	/* Advance the stream cursor */` |
|    7918 | 5238 | `	pGen->pIn++;` |
|       - | 5239 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5240 | `		SyBlob sFQN;` |
|       - | 5241 | `		SyString sFQNStr;` |
|    7918 | 5242 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    7918 | 5243 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    7918 | 5244 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    7918 | 5245 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    7918 | 5246 | `		SyBlobRelease(&sFQN);` |
|       - | 5247 | `	}` |
|    7918 | 5248 | `	if( pClass == 0 ){` |
|     ! 0 | 5249 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5250 | `		return SXERR_ABORT;` |
|       - | 5251 | `	}` |
|       - | 5252 | `	/* Mark as an interface */` |
|    7918 | 5253 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - | 5254 | `	/* Assume no base class is given */` |
|    7918 | 5255 | `	pBase = 0;` |
|    7918 | 5256 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 5257 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 5258 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - | 5259 | `			SyString *pBaseName;` |
|       - | 5260 | `			/* Extract base interface */` |
|       3 | 5261 | `			pGen->pIn++;` |
|       3 | 5262 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5263 | `				/* Syntax error */` |
|     ! 0 | 5264 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5265 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 | 5266 | `					pName);` |
|     ! 0 | 5267 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5268 | `				if( rc == SXERR_ABORT ){` |
|       - | 5269 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5270 | `					return SXERR_ABORT;` |
|       - | 5271 | `				}` |
|     ! 0 | 5272 | `				return SXRET_OK;` |
|       - | 5273 | `			}` |
|       3 | 5274 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5275 | `			{` |
|       - | 5276 | `				SyBlob sResolved;` |
|       3 | 5277 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 | 5278 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 | 5279 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 | 5280 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 | 5281 | `				SyBlobRelease(&sResolved);` |
|       - | 5282 | `			}` |
|       - | 5283 | `			/* Only interfaces is allowed */` |
|       3 | 5284 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5285 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5286 | `			}` |
|       3 | 5287 | `			if( pBase == 0 ){` |
|       - | 5288 | `				/* Inexistant interface */` |
|     ! 0 | 5289 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 | 5290 | `				if( rc == SXERR_ABORT ){` |
|       - | 5291 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5292 | `					return SXERR_ABORT;` |
|       - | 5293 | `				}` |
|     ! 0 | 5294 | `			}` |
|       - | 5295 | `			/* Advance the stream cursor */` |
|       3 | 5296 | `			pGen->pIn++;` |
|       1 | 5297 | `		}` |
|       1 | 5298 | `	}` |
|    7918 | 5299 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5300 | `		/* Syntax error */` |
|     ! 0 | 5301 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 | 5302 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5303 | `		if( rc == SXERR_ABORT ){` |
|       - | 5304 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5305 | `			return SXERR_ABORT;` |
|       - | 5306 | `		}` |
|     ! 0 | 5307 | `		return SXRET_OK;` |
|       - | 5308 | `	}` |
|    7918 | 5309 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    7918 | 5310 | `	pEnd = 0; /* cc warning */` |
|       - | 5311 | `	/* Delimit the interface body */` |
|    7918 | 5312 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    7918 | 5313 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5314 | `		/* Syntax error */` |
|     ! 0 | 5315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 | 5316 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5317 | `		if( rc == SXERR_ABORT ){` |
|       - | 5318 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5319 | `			return SXERR_ABORT;` |
|       - | 5320 | `		}` |
|     ! 0 | 5321 | `		return SXRET_OK;` |
|       - | 5322 | `	}` |
|       - | 5323 | `	/* Swap token stream */` |
|    7918 | 5324 | `	pTmp = pGen->pEnd;` |
|    7918 | 5325 | `	pGen->pEnd = pEnd;` |
|       - | 5326 | `	/* Start the parse process` |
|       - | 5327 | `	 * Note (According to the PHP reference manual):` |
|       - | 5328 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - | 5329 | `	 *  Only 'public' visibility is allowed.` |
|       - | 5330 | `	 */` |
|   14481 | 5331 | `	for(;;){` |
|       - | 5332 | `		/* Jump leading/trailing semi-colons */` |
|   50010 | 5333 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   21048 | 5334 | `			pGen->pIn++;` |
|       2 | 5335 | `		}` |
|   28964 | 5336 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5337 | `			/* End of interface body */` |
|    7916 | 5338 | `			break;` |
|       - | 5339 | `		}` |
|   21050 | 5340 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5341 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5342 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 | 5343 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5344 | `			if( rc == SXERR_ABORT ){` |
|       - | 5345 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5346 | `				return SXERR_ABORT;` |
|       - | 5347 | `			}` |
|     ! 0 | 5348 | `			goto done;` |
|       - | 5349 | `		}` |
|       - | 5350 | `		/* Extract the current keyword */` |
|   21050 | 5351 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21050 | 5352 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - | 5353 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - | 5354 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 | 5355 | `			const char *zKind = "member";` |
|       3 | 5356 | `			SyString *pMemberName = 0;` |
|       3 | 5357 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 | 5358 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 | 5359 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 | 5360 | `					zKind = "constant";` |
|       3 | 5361 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 | 5362 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 | 5363 | `					}` |
|       1 | 5364 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5365 | `					zKind = "method";` |
|     ! 0 | 5366 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 | 5367 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 | 5368 | `					}` |
|     ! 0 | 5369 | `				}` |
|       1 | 5370 | `			}` |
|       3 | 5371 | `			if( pMemberName ){` |
|       4 | 5372 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 | 5373 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 | 5374 | `			}else{` |
|     ! 0 | 5375 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5376 | `					"Access type for interface %s must be public",zKind);` |
|       - | 5377 | `			}` |
|       3 | 5378 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 5379 | `				return SXERR_ABORT;` |
|       - | 5380 | `			}` |
|       3 | 5381 | `			goto done;` |
|       - | 5382 | `		}` |
|   21048 | 5383 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5384 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5385 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5386 | `			if( rc == SXERR_ABORT ){` |
|       - | 5387 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5388 | `				return SXERR_ABORT;` |
|       - | 5389 | `			}` |
|     ! 0 | 5390 | `			goto done;` |
|       - | 5391 | `		}` |
|   21048 | 5392 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - | 5393 | `			/* Advance the stream cursor */` |
|   21044 | 5394 | `			pGen->pIn++;` |
|   21044 | 5395 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 5396 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5397 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5398 | `				if( rc == SXERR_ABORT ){` |
|       - | 5399 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5400 | `					return SXERR_ABORT;` |
|       - | 5401 | `				}` |
|     ! 0 | 5402 | `				goto done;` |
|       - | 5403 | `			}` |
|   21044 | 5404 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   21044 | 5405 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 | 5406 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5407 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 | 5408 | `				if( rc == SXERR_ABORT ){` |
|       - | 5409 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5410 | `					return SXERR_ABORT;` |
|       - | 5411 | `				}` |
|     ! 0 | 5412 | `				goto done;` |
|       - | 5413 | `			}` |
|   10521 | 5414 | `		}` |
|   21048 | 5415 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 5416 | `			/* Parse constant */` |
|       3 | 5417 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 | 5418 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5419 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5420 | `					return SXERR_ABORT;` |
|       - | 5421 | `				}` |
|     ! 0 | 5422 | `				goto done;` |
|       - | 5423 | `			}` |
|       2 | 5424 | `		}else{` |
|   21046 | 5425 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   21046 | 5426 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 5427 | `				/* Static method,record that */` |
|     ! 0 | 5428 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - | 5429 | `				/* Advance the stream cursor */` |
|     ! 0 | 5430 | `				pGen->pIn++;` |
|     ! 0 | 5431 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 | 5432 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 5433 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5434 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 | 5435 | `						if( rc == SXERR_ABORT ){` |
|       - | 5436 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 5437 | `							return SXERR_ABORT;` |
|       - | 5438 | `						}` |
|     ! 0 | 5439 | `						goto done;` |
|       - | 5440 | `				}` |
|     ! 0 | 5441 | `			}` |
|       - | 5442 | `			/* Process method signature (no body for interface methods) */` |
|   21046 | 5443 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   21046 | 5444 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 5445 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5446 | `					return SXERR_ABORT;` |
|       - | 5447 | `				}` |
|     ! 0 | 5448 | `				goto done;` |
|       - | 5449 | `			}` |
|       - | 5450 | `		}` |
|       2 | 5451 | `	}` |
|       - | 5452 | `	/* Install the interface */` |
|    7916 | 5453 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    7916 | 5454 | `	if( rc == SXRET_OK && pBase ){` |
|       - | 5455 | `		/* Inherit from the base interface */` |
|       3 | 5456 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 | 5457 | `	}` |
|    7916 | 5458 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5459 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5460 | `		return SXERR_ABORT;` |
|       - | 5461 | `	}` |
|    3957 | 5462 | `done:` |
|       - | 5463 | `	/* Point beyond the interface body */` |
|    7918 | 5464 | `	pGen->pIn  = &pEnd[1];` |
|    7918 | 5465 | `	pGen->pEnd = pTmp;` |
|    7918 | 5466 | `	return PH7_OK;` |
|    3960 | 5467 |  |
|       - | 5468 | `/*` |
|       - | 5469 | ` * Compile a user-defined class.` |
|       - | 5470 | ` * According to the PHP language reference manual` |
|       - | 5471 | ` *  class` |
|       - | 5472 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - | 5473 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - | 5474 | ` *  of the properties and methods belonging to the class.` |
|       - | 5475 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - | 5476 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - | 5477 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - | 5478 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - | 5479 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - | 5480 | ` *  (called "methods").` |
|       - | 5481 | ` */` |
|       - | 5482 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - | 5483 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - | 5484 | `struct TraitUseEntry {` |
|       - | 5485 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - | 5486 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - | 5487 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - | 5488 | `};` |
|       - | 5489 | `/*` |
|       - | 5490 | ` * Validate that methods implementing interface contracts have compatible` |
|       - | 5491 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - | 5492 | ` */` |
|   37236 | 5493 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5494 |  |
|       - | 5495 | `	ph7_class **apIface;` |
|       - | 5496 | `	sxu32 nIface,i;` |
|       - | 5497 | `	sxi32 rc;` |
|   37238 | 5498 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 | 5499 | `		return SXRET_OK;` |
|       - | 5500 | `	}` |
|   37238 | 5501 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   37238 | 5502 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   39902 | 5503 | `	for(i = 0; i < nIface; i++){` |
|    2666 | 5504 | `		ph7_class *pIface = apIface[i];` |
|       - | 5505 | `		SyHashEntry *pEntry;` |
|    2666 | 5506 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   15874 | 5507 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   13210 | 5508 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - | 5509 | `			ph7_class_method *pImplMeth;` |
|   13210 | 5510 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - | 5511 | `			/* Find the implementing method in the class */` |
|   13210 | 5512 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   13210 | 5513 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 5514 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - | 5515 | `			}` |
|       - | 5516 | `			/* Check visibility: interface methods must be implemented as public */` |
|   13196 | 5517 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 | 5518 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5519 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 | 5520 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 | 5521 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 5522 | `					return SXERR_ABORT;` |
|       - | 5523 | `				}` |
|       1 | 5524 | `			}` |
|       - | 5525 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - | 5526 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - | 5527 | `			 */` |
|       - | 5528 | `			{` |
|   13196 | 5529 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   13196 | 5530 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   13196 | 5531 | `				int sigError = 0;` |
|   13196 | 5532 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 | 5533 | `					sigError = 1;` |
|   13195 | 5534 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - | 5535 | `					/* Extra parameters must all have default values */` |
|       5 | 5536 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - | 5537 | `					sxu32 k;` |
|       7 | 5538 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 | 5539 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 | 5540 | `							sigError = 1;` |
|       3 | 5541 | `							break;` |
|       - | 5542 | `						}` |
|       2 | 5543 | `					}` |
|       2 | 5544 | `				}` |
|   13196 | 5545 | `				if( sigError ){` |
|       - | 5546 | `					SyBlob sImplSig, sIfaceSig;` |
|       - | 5547 | `					ph7_vm_func_arg *aArgs;` |
|       - | 5548 | `					sxu32 j;` |
|       5 | 5549 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 | 5550 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - | 5551 | `					/* Build implementing method signature */` |
|       5 | 5552 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 | 5553 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 | 5554 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 | 5555 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 | 5556 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5557 | `					}` |
|       - | 5558 | `					/* Build interface method signature */` |
|       5 | 5559 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 | 5560 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 | 5561 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 | 5562 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 | 5563 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 | 5564 | `					}` |
|       7 | 5565 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - | 5566 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 | 5567 | `						&pClass->sName,pMName,` |
|       4 | 5568 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 | 5569 | `						&pIface->sName,pMName,` |
|       4 | 5570 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 | 5571 | `					SyBlobRelease(&sImplSig);` |
|       5 | 5572 | `					SyBlobRelease(&sIfaceSig);` |
|       5 | 5573 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 5574 | `						return SXERR_ABORT;` |
|       - | 5575 | `					}` |
|       2 | 5576 | `				}` |
|       - | 5577 | `			}` |
|       2 | 5578 | `		}` |
|    1334 | 5579 | `	}` |
|   37238 | 5580 | `	return SXRET_OK;` |
|   18620 | 5581 |  |
|       - | 5582 | `/*` |
|       - | 5583 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - | 5584 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - | 5585 | ` */` |
|   37236 | 5586 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 | 5587 |  |
|       - | 5588 | `	ph7_class_method *pMeth;` |
|       - | 5589 | `	SyHashEntry *pEntry;` |
|       - | 5590 | `	sxu32 nAbstract;` |
|       - | 5591 | `	SyBlob sMsg;` |
|       - | 5592 | `	sxi32 rc;` |
|       - | 5593 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   37238 | 5594 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 | 5595 | `		return SXRET_OK;` |
|       - | 5596 | `	}` |
|       - | 5597 | `	/* Count abstract methods */` |
|   37220 | 5598 | `	nAbstract = 0;` |
|   37220 | 5599 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  353196 | 5600 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  315978 | 5601 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  315978 | 5602 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 | 5603 | `			nAbstract++;` |
|       8 | 5604 | `		}` |
|       2 | 5605 | `	}` |
|   37220 | 5606 | `	if( nAbstract == 0 ){` |
|   37206 | 5607 | `		return SXRET_OK;` |
|       - | 5608 | `	}` |
|       - | 5609 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 | 5610 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 | 5611 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - | 5612 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 | 5613 | `		&pClass->sName,nAbstract,` |
|       7 | 5614 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 | 5615 | `		(nAbstract > 1 ? "s" : ""));` |
|       - | 5616 | `	/* Second pass: list methods with origins */` |
|       - | 5617 | `	{` |
|      15 | 5618 | `		sxu32 nListed = 0;` |
|      15 | 5619 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 | 5620 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 | 5621 | `			ph7_class *pOrigin = 0;` |
|       - | 5622 | `			SyString *pMName;` |
|      19 | 5623 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 | 5624 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 | 5625 | `				continue;` |
|       - | 5626 | `			}` |
|      17 | 5627 | `			pMName = &pMeth->sFunc.sName;` |
|      17 | 5628 | `			if( nListed > 0 ){` |
|       3 | 5629 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 | 5630 | `			}` |
|       - | 5631 | `			/* Find the origin of this abstract method.` |
|       - | 5632 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - | 5633 | `			 * inheritance chains) take precedence for interface-declared` |
|       - | 5634 | `			 * methods. Abstract class methods only win when the class` |
|       - | 5635 | `			 * itself declared the abstract method (not inherited from` |
|       - | 5636 | `			 * an interface). Trait methods are adopted into the using` |
|       - | 5637 | `			 * class's namespace.` |
|       - | 5638 | `			 */` |
|       - | 5639 | `			{` |
|       - | 5640 | `				ph7_class **apIface;` |
|       - | 5641 | `				ph7_class **apTrait;` |
|       - | 5642 | `				ph7_class *pWalk;` |
|       - | 5643 | `				sxu32 i;` |
|       - | 5644 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - | 5645 | `				 * (one that was written in the class body, not inherited from an` |
|       - | 5646 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - | 5647 | `				 */` |
|      17 | 5648 | `				if( pClass->pBase ){` |
|       9 | 5649 | `					pWalk = pClass->pBase;` |
|      17 | 5650 | `					while( pWalk ){` |
|       - | 5651 | `						ph7_class_method *pParentMeth;` |
|      11 | 5652 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 | 5653 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - | 5654 | `							/* Exclude methods that came from an interface anywhere` |
|       - | 5655 | `							 * in this class's ancestor chain.` |
|       - | 5656 | `							 */` |
|      11 | 5657 | `							int fromIface = 0;` |
|      11 | 5658 | `							ph7_class *pAnc = pWalk;` |
|      15 | 5659 | `							while( pAnc ){` |
|       - | 5660 | `								ph7_class **apPI;` |
|       - | 5661 | `								sxu32 j;` |
|      13 | 5662 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 | 5663 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 | 5664 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 | 5665 | `										fromIface = 1;` |
|       9 | 5666 | `										break;` |
|       - | 5667 | `									}` |
|     ! 0 | 5668 | `								}` |
|      13 | 5669 | `								if( fromIface ) break;` |
|       5 | 5670 | `								pAnc = pAnc->pBase;` |
|       1 | 5671 | `							}` |
|      11 | 5672 | `							if( !fromIface ){` |
|       3 | 5673 | `								pOrigin = pWalk;` |
|       3 | 5674 | `								break;` |
|       - | 5675 | `							}` |
|       4 | 5676 | `						}` |
|       9 | 5677 | `						pWalk = pWalk->pBase;` |
|       1 | 5678 | `					}` |
|       4 | 5679 | `				}` |
|       - | 5680 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - | 5681 | `				 * each interface's own parent chain for the deepest origin.` |
|       - | 5682 | `				 */` |
|      17 | 5683 | `				if( !pOrigin ){` |
|      15 | 5684 | `					pWalk = pClass;` |
|      37 | 5685 | `					while( pWalk && !pOrigin ){` |
|      23 | 5686 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 | 5687 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 | 5688 | `							ph7_class *pIface = apIface[i];` |
|      13 | 5689 | `							ph7_class *pDeepest = 0;` |
|      25 | 5690 | `							while( pIface ){` |
|      13 | 5691 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 | 5692 | `									pDeepest = pIface;` |
|       6 | 5693 | `								}` |
|      13 | 5694 | `								pIface = pIface->pBase;` |
|       1 | 5695 | `							}` |
|      13 | 5696 | `							if( pDeepest ){` |
|      13 | 5697 | `								pOrigin = pDeepest;` |
|      13 | 5698 | `								break;` |
|       - | 5699 | `							}` |
|     ! 0 | 5700 | `						}` |
|      23 | 5701 | `						pWalk = pWalk->pBase;` |
|       1 | 5702 | `					}` |
|       7 | 5703 | `				}` |
|       - | 5704 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 | 5705 | `				if( !pOrigin ){` |
|       3 | 5706 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 | 5707 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 | 5708 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 | 5709 | `							pOrigin = pClass;` |
|       3 | 5710 | `							break;` |
|       - | 5711 | `						}` |
|     ! 0 | 5712 | `					}` |
|       1 | 5713 | `				}` |
|       - | 5714 | `			}` |
|      17 | 5715 | `			if( pOrigin ){` |
|      17 | 5716 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 | 5717 | `			}else{` |
|       - | 5718 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 | 5719 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - | 5720 | `			}` |
|      17 | 5721 | `			nListed++;` |
|       1 | 5722 | `		}` |
|       - | 5723 | `	}` |
|      15 | 5724 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 | 5725 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 | 5726 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 | 5727 | `	SyBlobRelease(&sMsg);` |
|      15 | 5728 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 5729 | `		return SXERR_ABORT;` |
|       - | 5730 | `	}` |
|      15 | 5731 | `	return SXRET_OK;` |
|   18620 | 5732 |  |
|   37240 | 5733 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 | 5734 |  |
|   37242 | 5735 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 5736 | `	ph7_class *pClass,*pBase;` |
|       - | 5737 | `	SyToken *pEnd,*pTmp;` |
|       - | 5738 | `	sxi32 iProtection;` |
|       - | 5739 | `	SySet aInterfaces;` |
|       - | 5740 | `	SySet aUseEntries;` |
|       - | 5741 | `	sxi32 iAttrflags;` |
|       - | 5742 | `	SyString *pName;` |
|       - | 5743 | `	sxi32 nKwrd;` |
|       - | 5744 | `	sxi32 rc;` |
|       - | 5745 | `	/* Jump the 'class' keyword */` |
|   37242 | 5746 | `	pGen->pIn++;` |
|   37242 | 5747 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5748 | `		/* Syntax error */` |
|     ! 0 | 5749 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 | 5750 | `		if( rc == SXERR_ABORT ){` |
|       - | 5751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5752 | `			return SXERR_ABORT;` |
|       - | 5753 | `		}` |
|       - | 5754 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 | 5755 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 | 5756 | `			pGen->pIn++;` |
|     ! 0 | 5757 | `		}` |
|     ! 0 | 5758 | `		return SXRET_OK;` |
|       - | 5759 | `	}` |
|       - | 5760 | `	/* Extract class name */` |
|   37242 | 5761 | `	pName = &pGen->pIn->sData;` |
|       - | 5762 | `	/* Advance the stream cursor */` |
|   37242 | 5763 | `	pGen->pIn++;` |
|       - | 5764 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 5765 | `		SyBlob sFQN;` |
|       - | 5766 | `		SyString sFQNStr;` |
|   37242 | 5767 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   37242 | 5768 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   37242 | 5769 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   37242 | 5770 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   37242 | 5771 | `		SyBlobRelease(&sFQN);` |
|       - | 5772 | `	}` |
|   37242 | 5773 | `	if( pClass == 0 ){` |
|     ! 0 | 5774 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 5775 | `		return SXERR_ABORT;` |
|       - | 5776 | `	}` |
|       - | 5777 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   37242 | 5778 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   37242 | 5779 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - | 5780 | `	/* Assume a standalone class */` |
|   37242 | 5781 | `	pBase = 0;` |
|   37242 | 5782 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 5783 | `		SyString *pBaseName;` |
|   26410 | 5784 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   26410 | 5785 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   23748 | 5786 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   23748 | 5787 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5788 | `				/* Syntax error */` |
|     ! 0 | 5789 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5790 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 | 5791 | `					pName);` |
|     ! 0 | 5792 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5793 | `				if( rc == SXERR_ABORT ){` |
|       - | 5794 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5795 | `					return SXERR_ABORT;` |
|       - | 5796 | `				}` |
|     ! 0 | 5797 | `				return SXRET_OK;` |
|       - | 5798 | `			}` |
|       - | 5799 | `			/* Extract base class name and resolve through namespace/imports */` |
|   23748 | 5800 | `			pBaseName = &pGen->pIn->sData;` |
|       - | 5801 | `			{` |
|       - | 5802 | `				SyBlob sResolved;` |
|   23748 | 5803 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23748 | 5804 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   35621 | 5805 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23746 | 5806 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23748 | 5807 | `				SyBlobRelease(&sResolved);` |
|       - | 5808 | `			}` |
|       - | 5809 | `			/* Interfaces are not allowed */` |
|   23748 | 5810 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 | 5811 | `				pBase = pBase->pNextName;` |
|     ! 0 | 5812 | `			}` |
|   23748 | 5813 | `			if( pBase == 0 ){` |
|       - | 5814 | `				/* Inexistant base class */` |
|     ! 0 | 5815 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 | 5816 | `				if( rc == SXERR_ABORT ){` |
|       - | 5817 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 | 5818 | `					return SXERR_ABORT;` |
|       - | 5819 | `				}` |
|     ! 0 | 5820 | `			}else{` |
|   23748 | 5821 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 | 5822 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 | 5823 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 | 5824 | `					if( rc == SXERR_ABORT ){` |
|       - | 5825 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5826 | `						return SXERR_ABORT;` |
|       - | 5827 | `					}` |
|     ! 0 | 5828 | `				}` |
|       - | 5829 | `			}` |
|       - | 5830 | `			/* Advance the stream cursor */` |
|   23748 | 5831 | `			pGen->pIn++;` |
|   11873 | 5832 | `		}` |
|   26410 | 5833 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - | 5834 | `			ph7_class *pInterface;` |
|       - | 5835 | `			SyString *pIntName;` |
|       - | 5836 | `			/* Interface implementation */` |
|    2666 | 5837 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1332 | 5838 | `			for(;;){` |
|    2666 | 5839 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - | 5840 | `					/* Syntax error */` |
|     ! 0 | 5841 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - | 5842 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 | 5843 | `						pName);` |
|     ! 0 | 5844 | `					if( rc == SXERR_ABORT ){` |
|       - | 5845 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5846 | `						return SXERR_ABORT;` |
|       - | 5847 | `					}` |
|     ! 0 | 5848 | `					break;` |
|       - | 5849 | `				}` |
|       - | 5850 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2666 | 5851 | `				pIntName = &pGen->pIn->sData;` |
|       - | 5852 | `				{` |
|       - | 5853 | `					SyBlob sResolved;` |
|    2666 | 5854 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2666 | 5855 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5330 | 5856 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2664 | 5857 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2666 | 5858 | `					SyBlobRelease(&sResolved);` |
|       - | 5859 | `				}` |
|       - | 5860 | `				/* Only interfaces are allowed */` |
|    2666 | 5861 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 | 5862 | `					pInterface = pInterface->pNextName;` |
|     ! 0 | 5863 | `				}` |
|    2666 | 5864 | `				if( pInterface == 0 ){` |
|       - | 5865 | `					/* Inexistant interface */` |
|     ! 0 | 5866 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 | 5867 | `					if( rc == SXERR_ABORT ){` |
|       - | 5868 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 5869 | `						return SXERR_ABORT;` |
|       - | 5870 | `					}` |
|     ! 0 | 5871 | `				}else{` |
|       - | 5872 | `					/* Register interface */` |
|    2666 | 5873 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - | 5874 | `				}` |
|       - | 5875 | `				/* Advance the stream cursor */` |
|    2666 | 5876 | `				pGen->pIn++;` |
|    2666 | 5877 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1334 | 5878 | `					break;` |
|       - | 5879 | `				}` |
|     ! 0 | 5880 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 | 5881 | `			}` |
|    1332 | 5882 | `		}` |
|   13204 | 5883 | `	}` |
|   37242 | 5884 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - | 5885 | `		/* Syntax error */` |
|     ! 0 | 5886 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 | 5887 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5888 | `		if( rc == SXERR_ABORT ){` |
|       - | 5889 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5890 | `			return SXERR_ABORT;` |
|       - | 5891 | `		}` |
|     ! 0 | 5892 | `		return SXRET_OK;` |
|       - | 5893 | `	}` |
|   37242 | 5894 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   37242 | 5895 | `	pEnd = 0; /* cc warning */` |
|       - | 5896 | `	/* Delimit the class body */` |
|   37242 | 5897 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   37242 | 5898 | `	if( pEnd >= pGen->pEnd ){` |
|       - | 5899 | `		/* Syntax error */` |
|     ! 0 | 5900 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 | 5901 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 5902 | `		if( rc == SXERR_ABORT ){` |
|       - | 5903 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 5904 | `			return SXERR_ABORT;` |
|       - | 5905 | `		}` |
|     ! 0 | 5906 | `		return SXRET_OK;` |
|       - | 5907 | `	}` |
|       - | 5908 | `	/* Swap token stream */` |
|   37242 | 5909 | `	pTmp = pGen->pEnd;` |
|   37242 | 5910 | `	pGen->pEnd = pEnd;` |
|       - | 5911 | `	/* Set the inherited flags */` |
|   37242 | 5912 | `	pClass->iFlags = iFlags;` |
|       - | 5913 | `	/* Start the parse process */` |
|   71350 | 5914 | `	for(;;){` |
|       - | 5915 | `		/* Jump leading/trailing semi-colons */` |
|  211488 | 5916 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   34412 | 5917 | `			pGen->pIn++;` |
|       2 | 5918 | `		}` |
|  177078 | 5919 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 5920 | `			/* End of class body */` |
|   37238 | 5921 | `			break;` |
|       - | 5922 | `		}` |
|  139842 | 5923 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 5924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 5925 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 5926 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 5927 | `			if( rc == SXERR_ABORT ){` |
|       - | 5928 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 | 5929 | `				return SXERR_ABORT;` |
|       - | 5930 | `			}` |
|     ! 0 | 5931 | `			goto done;` |
|       - | 5932 | `		}` |
|       - | 5933 | `		/* Assume public visibility */` |
|  139842 | 5934 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  139842 | 5935 | `		iAttrflags = 0;` |
|  139842 | 5936 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - | 5937 | `			/* Extract the current keyword */` |
|  139842 | 5938 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  139842 | 5939 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 5940 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - | 5941 | `				TraitUseEntry sUse;` |
|      41 | 5942 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      41 | 5943 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      41 | 5944 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      28 | 5945 | `				for(;;){` |
|       - | 5946 | `					ph7_class *pTrait;` |
|       - | 5947 | `					SyString *pTraitName;` |
|      49 | 5948 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 5949 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5950 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 | 5951 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5952 | `							return SXERR_ABORT;` |
|       - | 5953 | `						}` |
|     ! 0 | 5954 | `						break;` |
|       - | 5955 | `					}` |
|      49 | 5956 | `					pTraitName = &pGen->pIn->sData;` |
|       - | 5957 | `					/* Resolve trait name through namespace/imports */ {` |
|       - | 5958 | `						SyBlob sResolved;` |
|      49 | 5959 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      49 | 5960 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|      97 | 5961 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      48 | 5962 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      49 | 5963 | `						SyBlobRelease(&sResolved);` |
|       - | 5964 | `					}` |
|       - | 5965 | `					/* Only traits are allowed */` |
|      49 | 5966 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 5967 | `						pTrait = pTrait->pNextName;` |
|     ! 0 | 5968 | `					}` |
|      49 | 5969 | `					if( pTrait == 0 ){` |
|     ! 0 | 5970 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 5971 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 | 5972 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 5973 | `							return SXERR_ABORT;` |
|       - | 5974 | `						}` |
|     ! 0 | 5975 | `					}else{` |
|      49 | 5976 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - | 5977 | `					}` |
|      49 | 5978 | `					pGen->pIn++; /* Advance past trait name */` |
|      49 | 5979 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      21 | 5980 | `						break;` |
|       - | 5981 | `					}` |
|       9 | 5982 | `					pGen->pIn++; /* Jump the comma */` |
|       1 | 5983 | `				}` |
|       - | 5984 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      41 | 5985 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - | 5986 | `					SyToken *pBlock;` |
|       9 | 5987 | `					pGen->pIn++; /* Jump '{' */` |
|       9 | 5988 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 | 5989 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 | 5990 | `					sUse.pResolvEnd = pBlock;` |
|       9 | 5991 | `					if( pBlock < pGen->pEnd ){` |
|       9 | 5992 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 | 5993 | `					}else{` |
|     ! 0 | 5994 | `						pGen->pIn = pGen->pEnd;` |
|       - | 5995 | `					}` |
|       4 | 5996 | `				}` |
|      41 | 5997 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - | 5998 | `				/* The semicolon will be consumed by the outer loop */` |
|      41 | 5999 | `				continue;` |
|       - | 6000 | `			}` |
|  139802 | 6001 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  137068 | 6002 | `				iProtection = nKwrd;` |
|  137068 | 6003 | `				pGen->pIn++; /* Jump the visibility token */` |
|  137068 | 6004 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6005 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6006 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 | 6007 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6008 | `					if( rc == SXERR_ABORT ){` |
|       - | 6009 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 | 6010 | `						return SXERR_ABORT;` |
|       - | 6011 | `					}` |
|     ! 0 | 6012 | `					goto done;` |
|       - | 6013 | `				}` |
|  137068 | 6014 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6015 | `					/* Attribute declaration */` |
|   34336 | 6016 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   34336 | 6017 | `					if( rc != SXRET_OK ){` |
|       3 | 6018 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6019 | `							return SXERR_ABORT;` |
|       - | 6020 | `						}` |
|       3 | 6021 | `						goto done;` |
|       - | 6022 | `					}` |
|   34334 | 6023 | `					continue;` |
|       - | 6024 | `				}` |
|       - | 6025 | `				/* Extract the keyword */` |
|  102734 | 6026 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   51366 | 6027 | `			}` |
|  105468 | 6028 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - | 6029 | `				/* Process constant declaration */` |
|      30 | 6030 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 | 6031 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6032 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6033 | `						return SXERR_ABORT;` |
|       - | 6034 | `					}` |
|     ! 0 | 6035 | `					goto done;` |
|       - | 6036 | `				}` |
|      16 | 6037 | `			}else{` |
|  105440 | 6038 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - | 6039 | `					/* Static method or attribute,record that */` |
|    2652 | 6040 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2652 | 6041 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2652 | 6042 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6043 | `						/* Extract the keyword */` |
|    2648 | 6044 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2648 | 6045 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6046 | `							iProtection = nKwrd;` |
|     ! 0 | 6047 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 | 6048 | `						}` |
|    1323 | 6049 | `					}` |
|    2652 | 6050 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6051 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6052 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 | 6053 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6054 | `						if( rc == SXERR_ABORT ){` |
|       - | 6055 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6056 | `							return SXERR_ABORT;` |
|       - | 6057 | `						}` |
|     ! 0 | 6058 | `						goto done;` |
|       - | 6059 | `					}` |
|    2652 | 6060 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - | 6061 | `						/* Attribute declaration */` |
|       5 | 6062 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 | 6063 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6064 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6065 | `								return SXERR_ABORT;` |
|       - | 6066 | `							}` |
|     ! 0 | 6067 | `							goto done;` |
|       - | 6068 | `						}` |
|       5 | 6069 | `						continue;` |
|       - | 6070 | `					}` |
|       - | 6071 | `					/* Extract the keyword */` |
|    2648 | 6072 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  104113 | 6073 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - | 6074 | `					/* Abstract method,record that */` |
|      10 | 6075 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - | 6076 | `					/* Mark the whole class as abstract */` |
|      10 | 6077 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - | 6078 | `					/* Advance the stream cursor */` |
|      10 | 6079 | `					pGen->pIn++;` |
|      10 | 6080 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 | 6081 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 | 6082 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 | 6083 | `							iProtection = nKwrd;` |
|       8 | 6084 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 | 6085 | `						}` |
|       4 | 6086 | `					}` |
|      10 | 6087 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 | 6088 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6089 | `							/* Static method */` |
|     ! 0 | 6090 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6091 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6092 | `					}` |
|      10 | 6093 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 | 6094 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6095 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6096 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 | 6097 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6098 | `							if( rc == SXERR_ABORT ){` |
|       - | 6099 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6100 | `								return SXERR_ABORT;` |
|       - | 6101 | `							}` |
|     ! 0 | 6102 | `							goto done;` |
|       - | 6103 | `					}` |
|      10 | 6104 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  102786 | 6105 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - | 6106 | `					/* final method ,record that */` |
|       5 | 6107 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 | 6108 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 | 6109 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - | 6110 | `						/* Extract the keyword */` |
|       5 | 6111 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6112 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6113 | `							iProtection = nKwrd;` |
|       5 | 6114 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 | 6115 | `						}` |
|       2 | 6116 | `					}` |
|       5 | 6117 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 | 6118 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - | 6119 | `							/* Static method */` |
|     ! 0 | 6120 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 | 6121 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 | 6122 | `					}` |
|       5 | 6123 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6124 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6125 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6126 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 | 6127 | `								&pGen->pIn->sData,pName);` |
|     ! 0 | 6128 | `							if( rc == SXERR_ABORT ){` |
|       - | 6129 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 | 6130 | `								return SXERR_ABORT;` |
|       - | 6131 | `							}` |
|     ! 0 | 6132 | `							goto done;` |
|       - | 6133 | `					}` |
|       5 | 6134 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6135 | `				}` |
|  105436 | 6136 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6137 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6138 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 | 6139 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6140 | `						if( rc == SXERR_ABORT ){` |
|       - | 6141 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6142 | `							return SXERR_ABORT;` |
|       - | 6143 | `						}` |
|     ! 0 | 6144 | `						goto done;` |
|       - | 6145 | `				}` |
|  105436 | 6146 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 | 6147 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 | 6148 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 | 6149 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6150 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6151 | `						if( rc == SXERR_ABORT ){` |
|       - | 6152 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 | 6153 | `							return SXERR_ABORT;` |
|       - | 6154 | `						}` |
|     ! 0 | 6155 | `						goto done;` |
|       - | 6156 | `					}` |
|       - | 6157 | `					/* Attribute declaration */` |
|       7 | 6158 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 | 6159 | `				}else{` |
|       - | 6160 | `					/* Process method declaration */` |
|  105430 | 6161 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6162 | `				}` |
|  105436 | 6163 | `				if( rc != SXRET_OK ){` |
|       3 | 6164 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6165 | `						return SXERR_ABORT;` |
|       - | 6166 | `					}` |
|       3 | 6167 | `					goto done;` |
|       - | 6168 | `				}` |
|       - | 6169 | `			}` |
|   52732 | 6170 | `		}else{` |
|       - | 6171 | `			/* Attribute declaration */` |
|     ! 0 | 6172 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6173 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6174 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6175 | `					return SXERR_ABORT;` |
|       - | 6176 | `				}` |
|     ! 0 | 6177 | `				goto done;` |
|       - | 6178 | `			}` |
|       - | 6179 | `		}` |
|       2 | 6180 | `	}` |
|       - | 6181 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - | 6182 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - | 6183 | `	 */` |
|       - | 6184 | `	{` |
|       - | 6185 | `		TraitUseEntry *apUse;` |
|       - | 6186 | `		sxu32 nU;` |
|   37238 | 6187 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   37278 | 6188 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      41 | 6189 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      41 | 6190 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      41 | 6191 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      41 | 6192 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - | 6193 | `			sxu32 nT;` |
|      41 | 6194 | `			if( !hasResolution ){` |
|       - | 6195 | `				/* No conflict resolution block: use standard trait application */` |
|      71 | 6196 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      39 | 6197 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      39 | 6198 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6199 | `						break;` |
|       - | 6200 | `					}` |
|      20 | 6201 | `				}` |
|      17 | 6202 | `			}else{` |
|       - | 6203 | `				/* With resolution block: copy attributes, record traits,` |
|       - | 6204 | `				 * then use the block to resolve method conflicts.` |
|       - | 6205 | `				 */` |
|       - | 6206 | `				SyToken *pR;` |
|      19 | 6207 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 | 6208 | `					ph7_class *pTR = apTrait[nT];` |
|       - | 6209 | `					ph7_class_attr *pAR;` |
|       - | 6210 | `					SyHashEntry *pER;` |
|       - | 6211 | `					SyString *pNR;` |
|      11 | 6212 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 | 6213 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 | 6214 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 | 6215 | `						pNR = &pAR->sName;` |
|     ! 0 | 6216 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 | 6217 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 | 6218 | `						}` |
|     ! 0 | 6219 | `					}` |
|      11 | 6220 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 | 6221 | `				}` |
|       - | 6222 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 | 6223 | `				pR = pUse->pResolvStart;` |
|      21 | 6224 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6225 | `					SyString sTrait,sMethod;` |
|       - | 6226 | `					ph7_class *pSrcTrait;` |
|       - | 6227 | `					ph7_class_method *pMeth;` |
|       - | 6228 | `					sxi32 nRKwrd;` |
|      33 | 6229 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6230 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6231 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6232 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6233 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6234 | `					sMethod = pR->sData;` |
|      13 | 6235 | `					pR++;` |
|      13 | 6236 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6237 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6238 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6239 | `							sTrait = sMethod;` |
|       7 | 6240 | `							pR++;` |
|       7 | 6241 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6242 | `							sMethod = pR->sData;` |
|       7 | 6243 | `							pR++;` |
|       3 | 6244 | `						}` |
|       3 | 6245 | `					}` |
|      13 | 6246 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6247 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6248 | `						continue;` |
|       - | 6249 | `					}` |
|      13 | 6250 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6251 | `					pR++;` |
|      13 | 6252 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 | 6253 | `						pSrcTrait = 0;` |
|       7 | 6254 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 | 6255 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 | 6256 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 | 6257 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 | 6258 | `								pSrcTrait = apTrait[nT];` |
|       5 | 6259 | `								break;` |
|       - | 6260 | `							}` |
|       2 | 6261 | `						}` |
|       5 | 6262 | `						if( pSrcTrait ){` |
|       5 | 6263 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 | 6264 | `							if( pMeth ){` |
|       5 | 6265 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 | 6266 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 | 6267 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 | 6268 | `								}` |
|       2 | 6269 | `							}` |
|       2 | 6270 | `						}` |
|       2 | 6271 | `					}` |
|      29 | 6272 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6273 | `				}` |
|       - | 6274 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 | 6275 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - | 6276 | `					ph7_class_method *pMR;` |
|       - | 6277 | `					SyHashEntry *pER;` |
|       - | 6278 | `					SyString *pNR;` |
|      11 | 6279 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 | 6280 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 | 6281 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 | 6282 | `						pNR = &pMR->sFunc.sName;` |
|      19 | 6283 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 | 6284 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 | 6285 | `						}` |
|       1 | 6286 | `					}` |
|       6 | 6287 | `				}` |
|       - | 6288 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 | 6289 | `				pR = pUse->pResolvStart;` |
|      21 | 6290 | `				while( pR < pUse->pResolvEnd ){` |
|       - | 6291 | `					SyString sTrait,sMethod,sAlias;` |
|       - | 6292 | `					ph7_class *pSrcTrait;` |
|       - | 6293 | `					ph7_class_method *pMeth;` |
|      21 | 6294 | `					int hasQual = 0;` |
|       - | 6295 | `					sxi32 nRKwrd;` |
|      33 | 6296 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 | 6297 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 | 6298 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 | 6299 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 | 6300 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 | 6301 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 | 6302 | `					sMethod = pR->sData;` |
|      13 | 6303 | `					pR++;` |
|      13 | 6304 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 | 6305 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 | 6306 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 | 6307 | `							sTrait = sMethod;` |
|       7 | 6308 | `							hasQual = 1;` |
|       7 | 6309 | `							pR++;` |
|       7 | 6310 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 | 6311 | `							sMethod = pR->sData;` |
|       7 | 6312 | `							pR++;` |
|       3 | 6313 | `						}` |
|       3 | 6314 | `					}` |
|      13 | 6315 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 6316 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 | 6317 | `						continue;` |
|       - | 6318 | `					}` |
|      13 | 6319 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 | 6320 | `					pR++;` |
|      13 | 6321 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 | 6322 | `						sxi32 iNewVis = -1;` |
|       9 | 6323 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 | 6324 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 | 6325 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 | 6326 | `								iNewVis = nAK;` |
|       7 | 6327 | `								pR++;` |
|       3 | 6328 | `							}` |
|       3 | 6329 | `						}` |
|       9 | 6330 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 | 6331 | `							sAlias = pR->sData;` |
|       7 | 6332 | `							pR++;` |
|       3 | 6333 | `						}` |
|       9 | 6334 | `						pMeth = 0;` |
|       9 | 6335 | `						if( hasQual ){` |
|       3 | 6336 | `							pSrcTrait = 0;` |
|       5 | 6337 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 | 6338 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 | 6339 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 | 6340 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 | 6341 | `									pSrcTrait = apTrait[nT];` |
|       3 | 6342 | `									break;` |
|       - | 6343 | `								}` |
|       2 | 6344 | `							}` |
|       3 | 6345 | `							if( pSrcTrait ){` |
|       3 | 6346 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 | 6347 | `							}` |
|       2 | 6348 | `						}else{` |
|       7 | 6349 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - | 6350 | `						}` |
|       9 | 6351 | `						if( pMeth ){` |
|       9 | 6352 | `							if( sAlias.nByte > 0 ){` |
|       - | 6353 | `								/* Create a shallow copy of the method struct for the alias` |
|       - | 6354 | `								 * so it can carry its own visibility without affecting the original.` |
|       - | 6355 | `								 */` |
|       - | 6356 | `								ph7_class_method *pAlias;` |
|       - | 6357 | `								char *zAliasDup;` |
|       7 | 6358 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 | 6359 | `								if( pAlias ){` |
|       7 | 6360 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 | 6361 | `									if( iNewVis >= 0 ){` |
|       5 | 6362 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6363 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6364 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 | 6365 | `									}` |
|       7 | 6366 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 | 6367 | `									if( zAliasDup ){` |
|       7 | 6368 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 | 6369 | `									}` |
|       4 | 6370 | `								}` |
|       6 | 6371 | `							}else if( iNewVis >= 0 ){` |
|       - | 6372 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - | 6373 | `								ph7_class_method *pCopy;` |
|       3 | 6374 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 | 6375 | `								if( pCopy ){` |
|       3 | 6376 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 | 6377 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 | 6378 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 | 6379 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 | 6380 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - | 6381 | `									/* Replace the method in the class hash */` |
|       3 | 6382 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 | 6383 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 | 6384 | `								}` |
|       1 | 6385 | `							}` |
|       4 | 6386 | `						}` |
|       4 | 6387 | `						SXUNUSED(hasQual);` |
|       4 | 6388 | `					}` |
|      17 | 6389 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 | 6390 | `				}` |
|       - | 6391 | `			}` |
|      41 | 6392 | `			SySetRelease(&pUse->aTraits);` |
|      21 | 6393 | `		}` |
|       - | 6394 | `	}` |
|       - | 6395 | `	/* Install the class */` |
|   37238 | 6396 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   37238 | 6397 | `	if( rc == SXRET_OK ){` |
|       - | 6398 | `		ph7_class **apInterface;` |
|       - | 6399 | `		sxu32 n;` |
|   37238 | 6400 | `		if( pBase ){` |
|       - | 6401 | `			/* Inherit from base class and mark as a subclass */` |
|   23748 | 6402 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11873 | 6403 | `		}` |
|   37238 | 6404 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   39902 | 6405 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - | 6406 | `			/* Implements one or more interface */` |
|    2666 | 6407 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2666 | 6408 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6409 | `				break;` |
|       - | 6410 | `			}` |
|    1334 | 6411 | `		}` |
|       - | 6412 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   37238 | 6413 | `		if( rc == SXRET_OK ){` |
|   37238 | 6414 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   37238 | 6415 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6416 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6417 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6418 | `				return SXERR_ABORT;` |
|       - | 6419 | `			}` |
|   18618 | 6420 | `		}` |
|       - | 6421 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   37238 | 6422 | `		if( rc == SXRET_OK ){` |
|   37238 | 6423 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   37238 | 6424 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 | 6425 | `				SySetRelease(&aUseEntries);` |
|     ! 0 | 6426 | `				SySetRelease(&aInterfaces);` |
|     ! 0 | 6427 | `				return SXERR_ABORT;` |
|       - | 6428 | `			}` |
|   18618 | 6429 | `		}` |
|   18618 | 6430 | `	}` |
|   37238 | 6431 | `	SySetRelease(&aUseEntries);` |
|   37238 | 6432 | `	SySetRelease(&aInterfaces);` |
|   37238 | 6433 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6434 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6435 | `		return SXERR_ABORT;` |
|       - | 6436 | `	}` |
|   18618 | 6437 | `done:` |
|       - | 6438 | `	/* Point beyond the class body */` |
|   37242 | 6439 | `	pGen->pIn = &pEnd[1];` |
|   37242 | 6440 | `	pGen->pEnd = pTmp;` |
|   37242 | 6441 | `	return PH7_OK;` |
|   18622 | 6442 |  |
|       - | 6443 | `/*` |
|       - | 6444 | ` * Compile a user-defined abstract class.` |
|       - | 6445 | ` *  According to the PHP language reference manual` |
|       - | 6446 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - | 6447 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - | 6448 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - | 6449 | ` *   the method's signature - they cannot define the implementation.` |
|       - | 6450 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - | 6451 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - | 6452 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - | 6453 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - | 6454 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - | 6455 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - | 6456 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - | 6457 | ` *   could differ.` |
|       - | 6458 | ` */` |
|      16 | 6459 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 | 6460 |  |
|       - | 6461 | `	sxi32 rc;` |
|      18 | 6462 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 | 6463 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 | 6464 | `	return rc;` |
|       2 | 6465 |  |
|       - | 6466 | `/*` |
|       - | 6467 | ` * Compile a user-defined final class.` |
|       - | 6468 | ` *  According to the PHP language reference manual` |
|       - | 6469 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - | 6470 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - | 6471 | ` *    final then it cannot be extended.` |
|       - | 6472 | ` */` |
|       2 | 6473 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 | 6474 |  |
|       - | 6475 | `	sxi32 rc;` |
|       3 | 6476 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 | 6477 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 | 6478 | `	return rc;` |
|       1 | 6479 |  |
|       - | 6480 | `/*` |
|       - | 6481 | ` * Compile a user-defined trait.` |
|       - | 6482 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - | 6483 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - | 6484 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - | 6485 | ` */` |
|      52 | 6486 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 | 6487 |  |
|      54 | 6488 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6489 | `	ph7_class *pClass;` |
|       - | 6490 | `	SyToken *pEnd,*pTmp;` |
|       - | 6491 | `	sxi32 iProtection;` |
|       - | 6492 | `	sxi32 iAttrflags;` |
|       - | 6493 | `	SyString *pName;` |
|       - | 6494 | `	sxi32 nKwrd;` |
|       - | 6495 | `	sxi32 rc;` |
|       - | 6496 | `	/* Jump the 'trait' keyword */` |
|      54 | 6497 | `	pGen->pIn++;` |
|      54 | 6498 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6499 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 | 6500 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6501 | `			return SXERR_ABORT;` |
|       - | 6502 | `		}` |
|     ! 0 | 6503 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 | 6504 | `			pGen->pIn++;` |
|     ! 0 | 6505 | `		}` |
|     ! 0 | 6506 | `		return SXRET_OK;` |
|       - | 6507 | `	}` |
|       - | 6508 | `	/* Extract trait name */` |
|      54 | 6509 | `	pName = &pGen->pIn->sData;` |
|      54 | 6510 | `	pGen->pIn++;` |
|       - | 6511 | `	/* Build FQN and obtain a raw class */ {` |
|       - | 6512 | `		SyBlob sFQN;` |
|       - | 6513 | `		SyString sFQNStr;` |
|      54 | 6514 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      54 | 6515 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      54 | 6516 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      54 | 6517 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      54 | 6518 | `		SyBlobRelease(&sFQN);` |
|       - | 6519 | `	}` |
|      54 | 6520 | `	if( pClass == 0 ){` |
|     ! 0 | 6521 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6522 | `		return SXERR_ABORT;` |
|       - | 6523 | `	}` |
|       - | 6524 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      54 | 6525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 | 6526 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 | 6527 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6528 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6529 | `			return SXERR_ABORT;` |
|       - | 6530 | `		}` |
|     ! 0 | 6531 | `		return SXRET_OK;` |
|       - | 6532 | `	}` |
|      54 | 6533 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      54 | 6534 | `	pEnd = 0;` |
|      54 | 6535 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      54 | 6536 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 | 6537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 | 6538 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 | 6539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6540 | `			return SXERR_ABORT;` |
|       - | 6541 | `		}` |
|     ! 0 | 6542 | `		return SXRET_OK;` |
|       - | 6543 | `	}` |
|       - | 6544 | `	/* Swap token stream */` |
|      54 | 6545 | `	pTmp = pGen->pEnd;` |
|      54 | 6546 | `	pGen->pEnd = pEnd;` |
|       - | 6547 | `	/* Mark as trait */` |
|      54 | 6548 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - | 6549 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      53 | 6550 | `	for(;;){` |
|     144 | 6551 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      21 | 6552 | `			pGen->pIn++;` |
|       1 | 6553 | `		}` |
|     124 | 6554 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      54 | 6555 | `			break;` |
|       - | 6556 | `		}` |
|      71 | 6557 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6558 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6559 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6560 | `				&pGen->pIn->sData,pName);` |
|     ! 0 | 6561 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6562 | `				return SXERR_ABORT;` |
|       - | 6563 | `			}` |
|     ! 0 | 6564 | `			goto done;` |
|       - | 6565 | `		}` |
|      71 | 6566 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      71 | 6567 | `		iAttrflags = 0;` |
|      71 | 6568 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      71 | 6569 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      71 | 6570 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - | 6571 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 | 6572 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 | 6573 | `				for(;;){` |
|       - | 6574 | `					ph7_class *pUsedTrait;` |
|       - | 6575 | `					SyString *pUsedName;` |
|       5 | 6576 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 | 6577 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 6578 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 | 6579 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6580 | `							return SXERR_ABORT;` |
|       - | 6581 | `						}` |
|     ! 0 | 6582 | `						break;` |
|       - | 6583 | `					}` |
|       5 | 6584 | `					pUsedName = &pGen->pIn->sData;` |
|       - | 6585 | `					{` |
|       - | 6586 | `						SyBlob sResolved;` |
|       5 | 6587 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 | 6588 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 | 6589 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 | 6590 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 | 6591 | `						SyBlobRelease(&sResolved);` |
|       - | 6592 | `					}` |
|       5 | 6593 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 | 6594 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 | 6595 | `					}` |
|       5 | 6596 | `					if( pUsedTrait == 0 ){` |
|       4 | 6597 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 | 6598 | `							"'%z' is not a trait",pUsedName);` |
|       3 | 6599 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6600 | `							return SXERR_ABORT;` |
|       - | 6601 | `						}` |
|       2 | 6602 | `					}else{` |
|       3 | 6603 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - | 6604 | `					}` |
|       5 | 6605 | `					pGen->pIn++;` |
|       5 | 6606 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 | 6607 | `						break;` |
|       - | 6608 | `					}` |
|     ! 0 | 6609 | `					pGen->pIn++;` |
|     ! 0 | 6610 | `				}` |
|       5 | 6611 | `				continue;` |
|       - | 6612 | `			}` |
|      67 | 6613 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      63 | 6614 | `				iProtection = nKwrd;` |
|      63 | 6615 | `				pGen->pIn++;` |
|      63 | 6616 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6617 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6618 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 | 6619 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6620 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6621 | `						return SXERR_ABORT;` |
|       - | 6622 | `					}` |
|     ! 0 | 6623 | `					goto done;` |
|       - | 6624 | `				}` |
|      63 | 6625 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 | 6626 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 | 6627 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 6628 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6629 | `							return SXERR_ABORT;` |
|       - | 6630 | `						}` |
|     ! 0 | 6631 | `						goto done;` |
|       - | 6632 | `					}` |
|      11 | 6633 | `					continue;` |
|       - | 6634 | `				}` |
|      53 | 6635 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 | 6636 | `			}` |
|      57 | 6637 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 | 6638 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6639 | `					"Traits cannot have constants");` |
|     ! 0 | 6640 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6641 | `					return SXERR_ABORT;` |
|       - | 6642 | `				}` |
|     ! 0 | 6643 | `				goto done;` |
|     ! 0 | 6644 | `			}else{` |
|      57 | 6645 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 | 6646 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 | 6647 | `					pGen->pIn++;` |
|       5 | 6648 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 | 6649 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 | 6650 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 | 6651 | `							iProtection = nKwrd;` |
|     ! 0 | 6652 | `							pGen->pIn++;` |
|     ! 0 | 6653 | `						}` |
|       1 | 6654 | `					}` |
|       5 | 6655 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 | 6656 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6657 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 | 6658 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6659 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6660 | `							return SXERR_ABORT;` |
|       - | 6661 | `						}` |
|     ! 0 | 6662 | `						goto done;` |
|       - | 6663 | `					}` |
|       5 | 6664 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 | 6665 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 | 6666 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 6667 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 | 6668 | `								return SXERR_ABORT;` |
|       - | 6669 | `							}` |
|     ! 0 | 6670 | `							goto done;` |
|       - | 6671 | `						}` |
|       3 | 6672 | `						continue;` |
|       - | 6673 | `					}` |
|       3 | 6674 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 | 6675 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 | 6676 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 | 6677 | `					pGen->pIn++;` |
|       5 | 6678 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 | 6679 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 | 6680 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 | 6681 | `							iProtection = nKwrd;` |
|       5 | 6682 | `							pGen->pIn++;` |
|       2 | 6683 | `						}` |
|       2 | 6684 | `					}` |
|       5 | 6685 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 | 6686 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 | 6687 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6688 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 | 6689 | `							&pGen->pIn->sData,pName);` |
|     ! 0 | 6690 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6691 | `							return SXERR_ABORT;` |
|       - | 6692 | `						}` |
|     ! 0 | 6693 | `						goto done;` |
|       - | 6694 | `					}` |
|       5 | 6695 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 | 6696 | `				}` |
|      55 | 6697 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 | 6698 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6699 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 | 6700 | `						&pGen->pIn->sData,pName);` |
|     ! 0 | 6701 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6702 | `						return SXERR_ABORT;` |
|       - | 6703 | `					}` |
|     ! 0 | 6704 | `					goto done;` |
|       - | 6705 | `				}` |
|      55 | 6706 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 | 6707 | `					pGen->pIn++;` |
|     ! 0 | 6708 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 | 6709 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 6710 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 | 6711 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 | 6712 | `							return SXERR_ABORT;` |
|       - | 6713 | `						}` |
|     ! 0 | 6714 | `						goto done;` |
|       - | 6715 | `					}` |
|     ! 0 | 6716 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6717 | `				}else{` |
|      55 | 6718 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - | 6719 | `				}` |
|      55 | 6720 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6721 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 6722 | `						return SXERR_ABORT;` |
|       - | 6723 | `					}` |
|     ! 0 | 6724 | `					goto done;` |
|       - | 6725 | `				}` |
|       - | 6726 | `			}` |
|      28 | 6727 | `		}else{` |
|     ! 0 | 6728 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 | 6729 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6730 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 6731 | `					return SXERR_ABORT;` |
|       - | 6732 | `				}` |
|     ! 0 | 6733 | `				goto done;` |
|       - | 6734 | `			}` |
|       - | 6735 | `		}` |
|       1 | 6736 | `	}` |
|       - | 6737 | `	/* Install the trait */` |
|      54 | 6738 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      54 | 6739 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6740 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 6741 | `		return SXERR_ABORT;` |
|       - | 6742 | `	}` |
|      26 | 6743 | `done:` |
|       - | 6744 | `	/* Point beyond the trait body */` |
|      54 | 6745 | `	pGen->pIn = &pEnd[1];` |
|      54 | 6746 | `	pGen->pEnd = pTmp;` |
|      54 | 6747 | `	return PH7_OK;` |
|      28 | 6748 |  |
|       - | 6749 | `/*` |
|       - | 6750 | ` * Compile a user-defined class.` |
|       - | 6751 | ` *  According to the PHP language reference manual` |
|       - | 6752 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - | 6753 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - | 6754 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - | 6755 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - | 6756 | ` *   and functions (called "methods").` |
|       - | 6757 | ` */` |
|   37222 | 6758 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 | 6759 |  |
|       - | 6760 | `	sxi32 rc;` |
|   37224 | 6761 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   37224 | 6762 | `	return rc;` |
|       2 | 6763 |  |
|       - | 6764 | `/*` |
|       - | 6765 | ` * Exception handling.` |
|       - | 6766 | ` *  According to the PHP language reference manual` |
|       - | 6767 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - | 6768 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - | 6769 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - | 6770 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - | 6771 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - | 6772 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - | 6773 | ` *    (or re-thrown) within a catch block.` |
|       - | 6774 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - | 6775 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - | 6776 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - | 6777 | ` *    been defined with set_exception_handler().` |
|       - | 6778 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - | 6779 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - | 6780 | ` */` |
|       - | 6781 | `/*` |
|       - | 6782 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - | 6783 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - | 6784 | ` * indicates failure.` |
|       - | 6785 | ` */` |
|    7938 | 6786 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 | 6787 |  |
|    7940 | 6788 | `	sxi32 rc = SXRET_OK;` |
|    7940 | 6789 | `	if( pRoot->pOp ){` |
|    7934 | 6790 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    3969 | 6791 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - | 6792 | `			/* Unexpected expression */` |
|     ! 0 | 6793 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6794 | `				"throw: Expecting an exception class instance");` |
|     ! 0 | 6795 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 | 6796 | `				rc = SXERR_INVALID;` |
|     ! 0 | 6797 | `			}` |
|       2 | 6798 | `		}` |
|    3972 | 6799 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - | 6800 | `		/* Unexpected expression */` |
|     ! 0 | 6801 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - | 6802 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 6803 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 | 6804 | `			rc = SXERR_INVALID;` |
|     ! 0 | 6805 | `		}` |
|     ! 0 | 6806 | `	}` |
|    7940 | 6807 | `	return rc;` |
|       2 | 6808 |  |
|       - | 6809 | `/*` |
|       - | 6810 | ` * Compile a 'throw' statement.` |
|       - | 6811 | ` * throw: This is how you trigger an exception.` |
|       - | 6812 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - | 6813 | ` */` |
|    7938 | 6814 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 | 6815 |  |
|    7940 | 6816 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6817 | `	GenBlock *pBlock;` |
|       - | 6818 | `	sxu32 nIdx;` |
|       - | 6819 | `	sxi32 rc;` |
|    7940 | 6820 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - | 6821 | `	/* Compile the expression */` |
|    7940 | 6822 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    7940 | 6823 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 | 6824 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 | 6825 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6826 | `			return SXERR_ABORT;` |
|       - | 6827 | `		}` |
|     ! 0 | 6828 | `		return SXRET_OK;` |
|       - | 6829 | `	}` |
|    7940 | 6830 | `	pBlock = pGen->pCurrent;` |
|       - | 6831 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   36886 | 6832 | `	while(pBlock->pParent){` |
|   36882 | 6833 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    7936 | 6834 | `			break;` |
|       - | 6835 | `		}` |
|       - | 6836 | `		/* Point to the parent block */` |
|   28948 | 6837 | `		pBlock = pBlock->pParent;` |
|       2 | 6838 | `	}` |
|       - | 6839 | `	/* Emit the throw instruction */` |
|    7940 | 6840 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - | 6841 | `	/* Emit the jump */` |
|    7940 | 6842 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    7940 | 6843 | `	return SXRET_OK;` |
|    3971 | 6844 |  |
|       - | 6845 | `/*` |
|       - | 6846 | ` * Compile a 'catch' block.` |
|       - | 6847 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 6848 | ` * an object containing the exception information.` |
|       - | 6849 | ` */` |
|      98 | 6850 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 | 6851 |  |
|     100 | 6852 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 6853 | `	ph7_exception_block sCatch;` |
|       - | 6854 | `	SySet *pInstrContainer;` |
|       - | 6855 | `	SyString sClassName;` |
|       - | 6856 | `	GenBlock *pCatch;` |
|       - | 6857 | `	SyToken *pToken;` |
|       - | 6858 | `	SyString *pName;` |
|       - | 6859 | `	char *zDup;` |
|       - | 6860 | `	sxi32 rc;` |
|     100 | 6861 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 6862 | `	/* Zero the structure */` |
|     100 | 6863 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 6864 | `	/* Initialize fields */` |
|     100 | 6865 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     100 | 6866 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     100 | 6867 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 6868 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6869 | `			pToken = pGen->pIn;` |
|     ! 0 | 6870 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6871 | `				pToken--;` |
|     ! 0 | 6872 | `			}` |
|     ! 0 | 6873 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6874 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6875 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6876 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6877 | `				return SXERR_ABORT;` |
|       - | 6878 | `			}` |
|     ! 0 | 6879 | `			return SXERR_INVALID;` |
|       - | 6880 | `	}` |
|       - | 6881 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     100 | 6882 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      61 | 6883 | `	for(;;){` |
|     124 | 6884 | `		int isAbsolute = 0;` |
|       - | 6885 | `		SyBlob sName;` |
|     124 | 6886 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - | 6887 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     124 | 6888 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 | 6889 | `			isAbsolute = 1;` |
|       7 | 6890 | `			pGen->pIn++;` |
|       3 | 6891 | `		}` |
|     124 | 6892 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     122 | 6893 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 | 6894 | `			SyBlobRelease(&sName);` |
|       5 | 6895 | `			pToken = pGen->pIn;` |
|       5 | 6896 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6897 | `				pToken--;` |
|     ! 0 | 6898 | `			}` |
|       7 | 6899 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6900 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 6901 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 | 6902 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6903 | `				return SXERR_ABORT;` |
|       - | 6904 | `			}` |
|       5 | 6905 | `			return SXERR_INVALID;` |
|       - | 6906 | `		}` |
|       - | 6907 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     120 | 6908 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     120 | 6909 | `		pGen->pIn++;` |
|     183 | 6910 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      67 | 6911 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 | 6912 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 | 6913 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 | 6914 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 | 6915 | `			pGen->pIn++;` |
|       1 | 6916 | `		}` |
|       - | 6917 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     120 | 6918 | `		if( !isAbsolute ){` |
|       - | 6919 | `			SyString sRaw;` |
|       - | 6920 | `			SyBlob sResolved;` |
|     114 | 6921 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     114 | 6922 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     114 | 6923 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     170 | 6924 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     112 | 6925 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     114 | 6926 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     114 | 6927 | `			SyBlobRelease(&sResolved);` |
|      58 | 6928 | `		}else{` |
|       - | 6929 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 | 6930 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 | 6931 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 | 6932 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - | 6933 | `		}` |
|     120 | 6934 | `		SyBlobRelease(&sName);` |
|     120 | 6935 | `		if( zDup == 0 ){` |
|     ! 0 | 6936 | `			goto Mem;` |
|       - | 6937 | `		}` |
|     120 | 6938 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     120 | 6939 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 6940 | `			goto Mem;` |
|       - | 6941 | `		}` |
|       - | 6942 | `		/* Check for '\|' (multi-catch separator) */` |
|     130 | 6943 | `		if( pGen->pIn < pGen->pEnd &&` |
|     118 | 6944 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 | 6945 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 | 6946 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 | 6947 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 | 6948 | `			continue;` |
|       - | 6949 | `		}` |
|      96 | 6950 | `		break;` |
|     ! 0 | 6951 | `	}` |
|     141 | 6952 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|      96 | 6953 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 6954 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 6955 | `			pToken = pGen->pIn;` |
|     ! 0 | 6956 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6957 | `				pToken--;` |
|     ! 0 | 6958 | `			}` |
|     ! 0 | 6959 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6960 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6961 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6962 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 6963 | `				return SXERR_ABORT;` |
|       - | 6964 | `			}` |
|     ! 0 | 6965 | `			return SXERR_INVALID;` |
|       - | 6966 | `	}` |
|      96 | 6967 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 6968 | `	/* Duplicate instance name */` |
|      96 | 6969 | `	pName = &pGen->pIn->sData;` |
|      96 | 6970 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|      96 | 6971 | `	if( zDup == 0 ){` |
|     ! 0 | 6972 | `		goto Mem;` |
|       - | 6973 | `	}` |
|      96 | 6974 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|      96 | 6975 | `	pGen->pIn++;` |
|      96 | 6976 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 6977 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 6978 | `		pToken = pGen->pIn;` |
|     ! 0 | 6979 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 6980 | `			pToken--;` |
|     ! 0 | 6981 | `		}` |
|     ! 0 | 6982 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 6983 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 6984 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 6985 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 6986 | `			return SXERR_ABORT;` |
|       - | 6987 | `		}` |
|     ! 0 | 6988 | `		return SXERR_INVALID;` |
|       - | 6989 | `	}` |
|       - | 6990 | `	/* Compile the block */` |
|      96 | 6991 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 6992 | `	/* Create the catch block */` |
|      96 | 6993 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|      96 | 6994 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6995 | `		return SXERR_ABORT;` |
|       - | 6996 | `	}` |
|       - | 6997 | `	/* Swap bytecode container */` |
|      96 | 6998 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      96 | 6999 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 7000 | `	/* Compile the block */` |
|      96 | 7001 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 7002 | `	/* Fix forward jumps now the destination is resolved  */` |
|      96 | 7003 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7004 | `	/* Emit the DONE instruction */` |
|      96 | 7005 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7006 | `	/* Leave the block */` |
|      96 | 7007 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7008 | `	/* Restore the default container */` |
|      96 | 7009 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7010 | `	/* Install the catch block */` |
|      96 | 7011 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|      96 | 7012 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7013 | `		goto Mem;` |
|       - | 7014 | `	}` |
|      96 | 7015 | `	return SXRET_OK;` |
|     ! 0 | 7016 | `Mem:` |
|     ! 0 | 7017 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7018 | `	return SXERR_ABORT;` |
|      51 | 7019 |  |
|       - | 7020 | `/*` |
|       - | 7021 | ` * Compile a 'try' block.` |
|       - | 7022 | ` * A function using an exception should be in a "try" block.` |
|       - | 7023 | ` * If the exception does not trigger, the code will continue` |
|       - | 7024 | ` * as normal. However if the exception triggers, an exception` |
|       - | 7025 | ` * is "thrown".` |
|       - | 7026 | ` */` |
|     106 | 7027 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 | 7028 |  |
|       - | 7029 | `	ph7_exception *pException;` |
|     108 | 7030 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 7031 | `	GenBlock *pTry;` |
|       - | 7032 | `	sxu32 nJmpIdx;` |
|       - | 7033 | `	sxi32 rc;` |
|       - | 7034 | `	/* Create the exception container */` |
|     108 | 7035 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     108 | 7036 | `	if( pException == 0 ){` |
|     ! 0 | 7037 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 7038 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 7039 | `		return SXERR_ABORT;` |
|       - | 7040 | `	}` |
|       - | 7041 | `	/* Zero the structure */` |
|     108 | 7042 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 7043 | `	/* Initialize fields */` |
|     108 | 7044 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     108 | 7045 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     108 | 7046 | `	pException->iHasFinally = 0;` |
|     108 | 7047 | `	pException->iFinallyDone = 0;` |
|     108 | 7048 | `	pException->pVm = pGen->pVm;` |
|       - | 7049 | `	/* Create the try block */` |
|     108 | 7050 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     108 | 7051 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7052 | `		return SXERR_ABORT;` |
|       - | 7053 | `	}` |
|       - | 7054 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     108 | 7055 | `	pTry->pUserData = pException;` |
|       - | 7056 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     108 | 7057 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 7058 | `	/* Fix the jump later when the destination is resolved */` |
|     108 | 7059 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     108 | 7060 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 7061 | `	/* Compile the block */` |
|     108 | 7062 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     108 | 7063 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7064 | `		return SXERR_ABORT;` |
|       - | 7065 | `	}` |
|       - | 7066 | `	/* Fix forward jumps now the destination is resolved */` |
|     108 | 7067 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7068 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     108 | 7069 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 7070 | `	/* Leave the block */` |
|     108 | 7071 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7072 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     108 | 7073 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     104 | 7074 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 7075 | `		/* Compile one or more catch blocks */` |
|      96 | 7076 | `		for(;;){` |
|     192 | 7077 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     154 | 7078 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      49 | 7079 | `					break;` |
|       - | 7080 | `			}` |
|     100 | 7081 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     100 | 7082 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7083 | `				return SXERR_ABORT;` |
|       - | 7084 | `			}` |
|       2 | 7085 | `		}` |
|      47 | 7086 | `	}` |
|       - | 7087 | `	/* Compile optional finally block */` |
|     108 | 7088 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      56 | 7089 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 7090 | `		SySet *pInstrContainer;` |
|       - | 7091 | `		GenBlock *pFinBlock;` |
|      32 | 7092 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 7093 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 | 7094 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 | 7095 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7096 | `			return SXERR_ABORT;` |
|       - | 7097 | `		}` |
|       - | 7098 | `		/* Swap bytecode container */` |
|      32 | 7099 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 | 7100 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 7101 | `		/* Compile the finally body */` |
|      32 | 7102 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 | 7103 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7104 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 7105 | `			return SXERR_ABORT;` |
|       - | 7106 | `		}` |
|       - | 7107 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 | 7108 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7109 | `		/* Emit DONE to terminate the finally block */` |
|      32 | 7110 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 7111 | `		/* Leave the block */` |
|      32 | 7112 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 7113 | `		/* Restore the default container */` |
|      32 | 7114 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 | 7115 | `		pException->iHasFinally = 1;` |
|      15 | 7116 | `	}` |
|       - | 7117 | `	/* Must have at least one catch or finally */` |
|     108 | 7118 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 | 7119 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 7120 | `			"Cannot use try without catch or finally");` |
|       7 | 7121 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7122 | `			return SXERR_ABORT;` |
|       - | 7123 | `		}` |
|       3 | 7124 | `	}` |
|     108 | 7125 | `	return SXRET_OK;` |
|      55 | 7126 |  |
|       - | 7127 | `/*` |
|       - | 7128 | ` * Compile a switch block.` |
|       - | 7129 | ` *  (See block-comment below for more information)` |
|       - | 7130 | ` */` |
|      98 | 7131 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 | 7132 |  |
|     100 | 7133 | `	sxi32 rc = SXRET_OK;` |
|     100 | 7134 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 7135 | `		/* Unexpected token */` |
|     ! 0 | 7136 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7137 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7138 | `			return SXERR_ABORT;` |
|       - | 7139 | `		}` |
|     ! 0 | 7140 | `		pGen->pIn++;` |
|     ! 0 | 7141 | `	}` |
|     100 | 7142 | `	pGen->pIn++;` |
|       - | 7143 | `	/* First instruction to execute in this block. */` |
|     100 | 7144 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 7145 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 7146 | `	 * or the '}' token */` |
|     172 | 7147 | `	for(;;){` |
|     346 | 7148 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7149 | `			/* No more input to process */` |
|     ! 0 | 7150 | `			break;` |
|       - | 7151 | `		}` |
|     346 | 7152 | `		rc = SXRET_OK;` |
|     346 | 7153 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      68 | 7154 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      26 | 7155 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 7156 | `					/* Unexpected token */` |
|     ! 0 | 7157 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7158 | `						&pGen->pIn->sData);` |
|     ! 0 | 7159 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7160 | `						return SXERR_ABORT;` |
|       - | 7161 | `					}` |
|       - | 7162 | `					/* FALL THROUGH */` |
|     ! 0 | 7163 | `				}` |
|      26 | 7164 | `				rc = SXERR_EOF;` |
|      26 | 7165 | `				break;` |
|       - | 7166 | `			}` |
|      23 | 7167 | `		}else{` |
|       - | 7168 | `			sxi32 nKwrd;` |
|       - | 7169 | `			/* Extract the keyword */` |
|     280 | 7170 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     280 | 7171 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      38 | 7172 | `				break;` |
|       - | 7173 | `			}` |
|     208 | 7174 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7175 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 7176 | `					/* Unexpected token */` |
|     ! 0 | 7177 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 7178 | `						&pGen->pIn->sData);` |
|     ! 0 | 7179 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 7180 | `						return SXERR_ABORT;` |
|       - | 7181 | `					}` |
|       - | 7182 | `					/* FALL THROUGH */` |
|     ! 0 | 7183 | `				}` |
|       - | 7184 | `				/* Block compiled */` |
|       3 | 7185 | `				break;` |
|       - | 7186 | `			}` |
|       - | 7187 | `		}` |
|       - | 7188 | `		/* Compile block */` |
|     248 | 7189 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     248 | 7190 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7191 | `			return SXERR_ABORT;` |
|       - | 7192 | `		}` |
|       2 | 7193 | `	}` |
|     100 | 7194 | `	return rc;` |
|      51 | 7195 |  |
|       - | 7196 | `/*` |
|       - | 7197 | ` * Compile a case eXpression.` |
|       - | 7198 | ` *  (See block-comment below for more information)` |
|       - | 7199 | ` */` |
|      80 | 7200 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 | 7201 |  |
|       - | 7202 | `	SySet *pInstrContainer;` |
|       - | 7203 | `	SyToken *pEnd,*pTmp;` |
|      82 | 7204 | `	sxi32 iNest = 0;` |
|       - | 7205 | `	sxi32 rc;` |
|       - | 7206 | `	/* Delimit the expression */` |
|      82 | 7207 | `	pEnd = pGen->pIn;` |
|     170 | 7208 | `	while( pEnd < pGen->pEnd ){` |
|     170 | 7209 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 7210 | `			/* Increment nesting level */` |
|       3 | 7211 | `			iNest++;` |
|     169 | 7212 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 7213 | `			/* Decrement nesting level */` |
|       3 | 7214 | `			iNest--;` |
|     167 | 7215 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      82 | 7216 | `			break;` |
|       - | 7217 | `		}` |
|      90 | 7218 | `		pEnd++;` |
|       2 | 7219 | `	}` |
|      82 | 7220 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 7221 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 7222 | `		if( rc == SXERR_ABORT ){` |
|       - | 7223 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7224 | `			return SXERR_ABORT;` |
|       - | 7225 | `		}` |
|     ! 0 | 7226 | `	}` |
|       - | 7227 | `	/* Swap token stream */` |
|      82 | 7228 | `	pTmp = pGen->pEnd;` |
|      82 | 7229 | `	pGen->pEnd = pEnd;` |
|      82 | 7230 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      82 | 7231 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      82 | 7232 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 7233 | `	/* Emit the done instruction */` |
|      82 | 7234 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      82 | 7235 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 7236 | `	/* Update token stream */` |
|      82 | 7237 | `	pGen->pIn  = pEnd;` |
|      82 | 7238 | `	pGen->pEnd = pTmp;` |
|      82 | 7239 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 7240 | `		return SXERR_ABORT;` |
|       - | 7241 | `	}` |
|      82 | 7242 | `	return SXRET_OK;` |
|      42 | 7243 |  |
|       - | 7244 | `/*` |
|       - | 7245 | ` * Compile the smart switch statement.` |
|       - | 7246 | ` * According to the PHP language reference manual` |
|       - | 7247 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 7248 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 7249 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 7250 | ` *  This is exactly what the switch statement is for.` |
|       - | 7251 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 7252 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 7253 | ` *  of the outer loop, use continue 2.` |
|       - | 7254 | ` *  Note that switch/case does loose comparision.` |
|       - | 7255 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 7256 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 7257 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 7258 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 7259 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 7260 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 7261 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 7262 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 7263 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 7264 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 7265 | ` *  list for the next case.` |
|       - | 7266 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 7267 | ` *  or floating-point numbers and strings.` |
|       - | 7268 | ` */` |
|      26 | 7269 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 | 7270 |  |
|       - | 7271 | `	GenBlock *pSwitchBlock;` |
|       - | 7272 | `	SyToken *pTmp,*pEnd;` |
|       - | 7273 | `	ph7_switch *pSwitch;` |
|       - | 7274 | `	sxu32 nToken;` |
|       - | 7275 | `	sxu32 nLine;` |
|       - | 7276 | `	sxi32 rc;` |
|      28 | 7277 | `	nLine = pGen->pIn->nLine;` |
|       - | 7278 | `	/* Jump the 'switch' keyword */` |
|      28 | 7279 | `	pGen->pIn++;` |
|      28 | 7280 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 7281 | `		/* Syntax error */` |
|     ! 0 | 7282 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 7283 | `		if( rc == SXERR_ABORT ){` |
|       - | 7284 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7285 | `			return SXERR_ABORT;` |
|       - | 7286 | `		}` |
|     ! 0 | 7287 | `		goto Synchronize;` |
|       - | 7288 | `	}` |
|       - | 7289 | `	/* Jump the left parenthesis '(' */` |
|      28 | 7290 | `	pGen->pIn++;` |
|      28 | 7291 | `	pEnd = 0; /* cc warning */` |
|       - | 7292 | `	/* Create the loop block */` |
|      41 | 7293 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      13 | 7294 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      28 | 7295 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 7296 | `		return SXERR_ABORT;` |
|       - | 7297 | `	}` |
|       - | 7298 | `	/* Delimit the condition */` |
|      28 | 7299 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      28 | 7300 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 7301 | `		/* Empty expression */` |
|     ! 0 | 7302 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 7303 | `		if( rc == SXERR_ABORT ){` |
|       - | 7304 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 7305 | `			return SXERR_ABORT;` |
|       - | 7306 | `		}` |
|     ! 0 | 7307 | `	}` |
|       - | 7308 | `	/* Swap token streams */` |
|      28 | 7309 | `	pTmp = pGen->pEnd;` |
|      28 | 7310 | `	pGen->pEnd = pEnd;` |
|       - | 7311 | `	/* Compile the expression */` |
|      28 | 7312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      28 | 7313 | `	if( rc == SXERR_ABORT ){` |
|       - | 7314 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 7315 | `		return SXERR_ABORT;` |
|       - | 7316 | `	}` |
|       - | 7317 | `	/* Update token stream */` |
|      28 | 7318 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 7319 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 7320 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 7321 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 7322 | `			return SXERR_ABORT;` |
|       - | 7323 | `		}` |
|     ! 0 | 7324 | `		pGen->pIn++;` |
|     ! 0 | 7325 | `	}` |
|      28 | 7326 | `	pGen->pIn  = &pEnd[1];` |
|      28 | 7327 | `	pGen->pEnd = pTmp;` |
|      28 | 7328 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      26 | 7329 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 7330 | `			pTmp = pGen->pIn;` |
|     ! 0 | 7331 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 7332 | `				pTmp--;` |
|     ! 0 | 7333 | `			}` |
|       - | 7334 | `			/* Unexpected token */` |
|     ! 0 | 7335 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 7336 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7337 | `				return SXERR_ABORT;` |
|       - | 7338 | `			}` |
|     ! 0 | 7339 | `			goto Synchronize;` |
|       - | 7340 | `	}` |
|       - | 7341 | `	/* Set the delimiter token */` |
|      28 | 7342 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 7343 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 7344 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 7345 | `	}else{` |
|      26 | 7346 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 7347 | `	}` |
|      28 | 7348 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 7349 | `	/* Create the switch blocks container */` |
|      28 | 7350 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      28 | 7351 | `	if( pSwitch == 0 ){` |
|       - | 7352 | `		/* Abort compilation */` |
|     ! 0 | 7353 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 7354 | `		return SXERR_ABORT;` |
|       - | 7355 | `	}` |
|       - | 7356 | `	/* Zero the structure */` |
|      28 | 7357 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 7358 | `	/* Initialize fields */` |
|      28 | 7359 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 7360 | `	/* Emit the switch instruction */` |
|      28 | 7361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 7362 | `	/* Compile case blocks */` |
|      87 | 7363 | `	for(;;){` |
|       - | 7364 | `		sxu32 nKwrd;` |
|     102 | 7365 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 7366 | `			/* No more input to process */` |
|     ! 0 | 7367 | `			break;` |
|       - | 7368 | `		}` |
|     102 | 7369 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 7370 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 7371 | `				/* Unexpected token */` |
|     ! 0 | 7372 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7373 | `					&pGen->pIn->sData);` |
|     ! 0 | 7374 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7375 | `					return SXERR_ABORT;` |
|       - | 7376 | `				}` |
|       - | 7377 | `				/* FALL THROUGH */` |
|     ! 0 | 7378 | `			}` |
|       - | 7379 | `			/* Block compiled */` |
|     ! 0 | 7380 | `			break;` |
|       - | 7381 | `		}` |
|       - | 7382 | `		/* Extract the keyword */` |
|     102 | 7383 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     102 | 7384 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 7385 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 7386 | `				/* Unexpected token */` |
|     ! 0 | 7387 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7388 | `					&pGen->pIn->sData);` |
|     ! 0 | 7389 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7390 | `					return SXERR_ABORT;` |
|       - | 7391 | `				}` |
|       - | 7392 | `				/* FALL THROUGH */` |
|     ! 0 | 7393 | `			}` |
|       - | 7394 | `			/* Block compiled */` |
|       3 | 7395 | `			break;` |
|       - | 7396 | `		}` |
|     100 | 7397 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 7398 | `			/*` |
|       - | 7399 | `			 * Accroding to the PHP language reference manual` |
|       - | 7400 | `			 *  A special case is the default case. This case matches anything` |
|       - | 7401 | `			 *  that wasn't matched by the other cases.` |
|       - | 7402 | `			 */` |
|      20 | 7403 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 7404 | `				/* Default case already compiled */` |
|     ! 0 | 7405 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 7406 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 7407 | `					return SXERR_ABORT;` |
|       - | 7408 | `				}` |
|     ! 0 | 7409 | `			}` |
|      20 | 7410 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 7411 | `			/* Compile the default block */` |
|      20 | 7412 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      20 | 7413 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7414 | `				return SXERR_ABORT;` |
|      20 | 7415 | `			}else if( rc == SXERR_EOF ){` |
|      18 | 7416 | `				break;` |
|       1 | 7417 | `			}` |
|      83 | 7418 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 7419 | `			ph7_case_expr sCase;` |
|       - | 7420 | `			/* Standard case block */` |
|      82 | 7421 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 7422 | `			/* initialize the structure */` |
|      82 | 7423 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 7424 | `			/* Compile the case expression */` |
|      82 | 7425 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      82 | 7426 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7427 | `				return SXERR_ABORT;` |
|       - | 7428 | `			}` |
|       - | 7429 | `			/* Compile the case block */` |
|      82 | 7430 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 7431 | `			/* Insert in the switch container */` |
|      82 | 7432 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      82 | 7433 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 7434 | `				return SXERR_ABORT;` |
|      82 | 7435 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 7436 | `				break;` |
|       - | 7437 | `			}` |
|      38 | 7438 | `		}else{` |
|       - | 7439 | `			/* Unexpected token */` |
|     ! 0 | 7440 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 7441 | `				&pGen->pIn->sData);` |
|     ! 0 | 7442 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7443 | `				return SXERR_ABORT;` |
|       - | 7444 | `			}` |
|     ! 0 | 7445 | `			break;` |
|       - | 7446 | `		}` |
|       2 | 7447 | `	}` |
|       - | 7448 | `	/* Fix all jumps now the destination is resolved */` |
|      28 | 7449 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 7450 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 7451 | `	/* Release the loop block */` |
|      28 | 7452 | `	GenStateLeaveBlock(pGen,0);` |
|      28 | 7453 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 7454 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      28 | 7455 | `		pGen->pIn++;` |
|      13 | 7456 | `	}` |
|       - | 7457 | `	/* Statement successfully compiled */` |
|      28 | 7458 | `	return SXRET_OK;` |
|     ! 0 | 7459 | `Synchronize:` |
|       - | 7460 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 7461 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 7462 | `		pGen->pIn++;` |
|     ! 0 | 7463 | `	}` |
|     ! 0 | 7464 | `	return SXRET_OK;` |
|      15 | 7465 |  |
|       - | 7466 | `/*` |
|       - | 7467 | ` * Generate bytecode for a given expression tree.` |
|       - | 7468 | ` * If something goes wrong while generating bytecode` |
|       - | 7469 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 7470 | ` * this function takes care of generating the appropriate` |
|       - | 7471 | ` * error message.` |
|       - | 7472 | ` */` |
| 2362836 | 7473 | `static sxi32 GenStateEmitExprCode(` |
|       - | 7474 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 7475 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 7476 | `	sxi32 iFlags /* Control flags */` |
|       - | 7477 | `	)` |
|       2 | 7478 |  |
|       - | 7479 | `	VmInstr *pInstr;` |
|       - | 7480 | `	sxu32 nJmpIdx;` |
| 2362838 | 7481 | `	sxi32 iP1 = 0;` |
| 2362838 | 7482 | `	sxu32 iP2 = 0;` |
| 2362838 | 7483 | `	void *p3  = 0;` |
|       - | 7484 | `	sxi32 iVmOp;` |
|       - | 7485 | `	sxi32 rc;` |
| 2362838 | 7486 | `	if( pNode->xCode ){` |
|       - | 7487 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 7488 | `		/* Compile node */` |
| 1464636 | 7489 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1464636 | 7490 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1464636 | 7491 | `		RE_SWAP_DELIMITER(pGen);` |
| 1464636 | 7492 | `		return rc;` |
|       - | 7493 | `	}` |
|  898204 | 7494 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 7495 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 7496 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 7497 | `		return SXERR_ABORT;` |
|       - | 7498 | `	}` |
|  898204 | 7499 | `	iVmOp = pNode->pOp->iVmOp;` |
|  898204 | 7500 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 7501 | `		sxu32 nJz,nJmp;` |
|       - | 7502 | `		/* Ternary operator require special handling */` |
|       - | 7503 | `		/* Phase#1: Compile the condition */` |
|    1884 | 7504 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1884 | 7505 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7506 | `			return rc;` |
|       - | 7507 | `		}` |
|    1884 | 7508 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1884 | 7509 | `		if( pNode->pLeft ){` |
|       - | 7510 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 7511 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1816 | 7512 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7513 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1816 | 7514 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1816 | 7515 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7516 | `				return rc;` |
|       - | 7517 | `			}` |
|     909 | 7518 | `		}else{` |
|       - | 7519 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 7520 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 7521 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 7522 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 7523 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 7524 | `		}` |
|       - | 7525 | `		/* Phase#4: Emit the unconditional jump */` |
|    1884 | 7526 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 7527 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1884 | 7528 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1884 | 7529 | `		if( pInstr ){` |
|    1884 | 7530 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     941 | 7531 | `		}` |
|    1884 | 7532 | `		if( !pNode->pLeft ){` |
|       - | 7533 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 7534 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 7535 | `		}` |
|       - | 7536 | `		/* Phase#6: Compile the 'else' expression */` |
|    1884 | 7537 | `		if( pNode->pRight ){` |
|    1884 | 7538 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1884 | 7539 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 7540 | `				return rc;` |
|       - | 7541 | `			}` |
|     941 | 7542 | `		}` |
|    1884 | 7543 | `		if( nJmp > 0 ){` |
|       - | 7544 | `			/* Phase#7: Fix the unconditional jump */` |
|    1884 | 7545 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1884 | 7546 | `			if( pInstr ){` |
|    1884 | 7547 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     941 | 7548 | `			}` |
|     941 | 7549 | `		}` |
|       - | 7550 | `		/* All done */` |
|    1884 | 7551 | `		return SXRET_OK;` |
|       - | 7552 | `	}` |
|       - | 7553 | `	/* Generate code for the left tree */` |
|  896322 | 7554 | `	if( pNode->pLeft ){` |
|  896286 | 7555 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 7556 | `			ph7_expr_node **apNode;` |
|  301044 | 7557 | `			int hasSpread = 0;` |
|       - | 7558 | `			sxi32 n;` |
|       - | 7559 | `			/* Recurse and generate bytecodes for function arguments */` |
|  301044 | 7560 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - | 7561 | `			/* Read-only load */` |
|  301044 | 7562 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  601538 | 7563 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  300496 | 7564 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  300496 | 7565 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7566 | `					return rc;` |
|       - | 7567 | `				}` |
|  300496 | 7568 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 7569 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 | 7570 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 | 7571 | `					hasSpread = 1;` |
|       7 | 7572 | `				}` |
|  150249 | 7573 | `			}` |
|       - | 7574 | `			/* Total number of given arguments */` |
|  301044 | 7575 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  301044 | 7576 | `			iP2 = hasSpread;` |
|       - | 7577 | `			/* Remove stale flags now */` |
|  301044 | 7578 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  150521 | 7579 | `		}` |
|  896286 | 7580 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  896286 | 7581 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 7582 | `			return rc;` |
|       - | 7583 | `		}` |
|  896286 | 7584 | `		if( iVmOp == PH7_OP_CALL ){` |
|  301044 | 7585 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  301044 | 7586 | `			if( pInstr ){` |
|  301044 | 7587 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  300476 | 7588 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 7589 | `					sxu32 nQual;` |
|       - | 7590 | `					/* Prevent constant expansion */` |
|  300476 | 7591 | `					pInstr->iP1 = 0;` |
|       - | 7592 | `					/* Namespace-qualify the function name for CALL.` |
|       - | 7593 | `					 * Only check function imports — class imports must NOT` |
|       - | 7594 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - | 7595 | `					 * handler fires before NEW; we store the original literal` |
|       - | 7596 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - | 7597 | `					 * can recover the unqualified name and re-qualify with` |
|       - | 7598 | `					 * class imports. */ {` |
|  300476 | 7599 | `						int fromImport = 0;` |
|  300476 | 7600 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  300476 | 7601 | `						pInstr->iP2 = (sxi32)nQual;` |
|  300476 | 7602 | `						if( nQual != nOrig ){` |
|       - | 7603 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 7604 | `							 * NEW handler can recover the unqualified name. */` |
|      68 | 7605 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      68 | 7606 | `							if( !fromImport ){` |
|      58 | 7607 | `								p3 = (void *)1;` |
|      28 | 7608 | `							}` |
|      35 | 7609 | `						}` |
|       - | 7610 | `					}` |
|  150807 | 7611 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 7612 | `					/* Method call,flag that */` |
|     548 | 7613 | `					pInstr->iP2 = 1;` |
|     273 | 7614 | `				}` |
|  150523 | 7615 | `			}` |
|  745765 | 7616 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 7617 | `			ph7_expr_node **apNode;` |
|       - | 7618 | `			sxi32 n;` |
|       - | 7619 | `			/* Recurse and generate bytecodes for array index */` |
|   67412 | 7620 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  121632 | 7621 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   54222 | 7622 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   54222 | 7623 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 7624 | `					return rc;` |
|       - | 7625 | `				}` |
|   27112 | 7626 | `			}` |
|   67412 | 7627 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   54222 | 7628 | `				iP1 = 1; /* Node have an index associated with it */` |
|   27110 | 7629 | `			}` |
|   67412 | 7630 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 7631 | `				/* Create an empty entry when the desired index is not found */` |
|   26622 | 7632 | `				iP2 = 1;` |
|   13312 | 7633 | `			}` |
|  561539 | 7634 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 7635 | `			/* POP the left node */` |
|      32 | 7636 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 7637 | `		}` |
|  448142 | 7638 | `	}` |
|  896322 | 7639 | `	rc = SXRET_OK;` |
|  896322 | 7640 | `	nJmpIdx = 0;` |
|       - | 7641 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 7642 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 7643 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  896322 | 7644 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     234 | 7645 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     234 | 7646 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     234 | 7647 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     234 | 7648 | `			int isSpecial = 0;` |
|     234 | 7649 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     150 | 7650 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     150 | 7651 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     161 | 7652 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     129 | 7653 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      66 | 7654 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      86 | 7655 | `					isSpecial = 1;` |
|      42 | 7656 | `				}` |
|      95 | 7657 | `			}` |
|     276 | 7658 | `			pInstr->iP1 = 0;` |
|     276 | 7659 | `			if( !isSpecial ){` |
|     108 | 7660 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      53 | 7661 | `			}` |
|       - | 7662 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 7663 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     192 | 7664 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     108 | 7665 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     108 | 7666 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 7667 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 7668 | `					return SXRET_OK;` |
|       - | 7669 | `				}` |
|      32 | 7670 | `			}` |
|      74 | 7671 | `		}` |
|     146 | 7672 | `	}` |
|       - | 7673 | `	/* Generate code for the right tree */` |
|  896246 | 7674 | `	if( pNode->pRight ){` |
|  468144 | 7675 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 7676 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8298 | 7677 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  463996 | 7678 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 7679 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2772 | 7680 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  458463 | 7681 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 7682 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 | 7683 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 | 7684 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  457063 | 7685 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  204320 | 7686 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  102159 | 7687 | `		}` |
|  468144 | 7688 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  468144 | 7689 | `		if( iVmOp == PH7_OP_STORE ){` |
|  201528 | 7690 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  201502 | 7691 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 7692 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 7693 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 7694 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 7695 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 7696 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 7697 | `				 */` |
|      54 | 7698 | `				iVmOp = 0;` |
|  201502 | 7699 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  201476 | 7700 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 7701 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   44730 | 7702 | `					iP2 = 1;` |
|   22366 | 7703 | `				}else{` |
|  156748 | 7704 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7705 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   26584 | 7706 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   26584 | 7707 | `						iP1 = pInstr->iP1;` |
|   13293 | 7708 | `					}else{` |
|  130166 | 7709 | `						p3 = pInstr->p3;` |
|       - | 7710 | `					}` |
|       - | 7711 | `					/* POP the last dynamic load instruction */` |
|  156748 | 7712 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 7713 | `				}` |
|  100739 | 7714 | `			}` |
|  367381 | 7715 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      46 | 7716 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      46 | 7717 | `			if( pInstr ){` |
|      46 | 7718 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 7719 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 7720 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 7721 | `					 */` |
|      15 | 7722 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 7723 | `					iP1 = pInstr->iP1;` |
|      15 | 7724 | `					iP2 = pInstr->iP2;` |
|      15 | 7725 | `					p3  = pInstr->p3;` |
|       8 | 7726 | `				}else{` |
|      32 | 7727 | `					p3 = pInstr->p3;` |
|       - | 7728 | `				}` |
|      22 | 7729 | `			}` |
|      22 | 7730 | `		}` |
|  234071 | 7731 | `	}` |
|  896246 | 7732 | `	if( iVmOp > 0 ){` |
|  896134 | 7733 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   10738 | 7734 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 7735 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    7886 | 7736 | `				iP1 = 1;` |
|    3944 | 7737 | `			}` |
|  890766 | 7738 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 7739 | `			/* Namespace-qualify the class name for NEW */ {` |
|   13574 | 7740 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   13574 | 7741 | `				VmInstr *pCallInstr = 0;` |
|   13574 | 7742 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   13558 | 7743 | `					pCallInstr = pPeek;` |
|   13558 | 7744 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    6778 | 7745 | `				}` |
|   13574 | 7746 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 7747 | `					sxu32 nLitForClass;` |
|       - | 7748 | `					/* If the CALL handler already qualified the name using` |
|       - | 7749 | `					 * function imports, recover the original unqualified` |
|       - | 7750 | `					 * literal so we can re-qualify with class imports. */` |
|   13572 | 7751 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      32 | 7752 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      17 | 7753 | `					}else{` |
|   13542 | 7754 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 7755 | `					}` |
|   13572 | 7756 | `					pPeek->iP1 = 0;` |
|   13572 | 7757 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    6785 | 7758 | `				}` |
|       - | 7759 | `			}` |
|   13574 | 7760 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   13574 | 7761 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 7762 | `				VmInstr *pPrev;` |
|   13558 | 7763 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   13558 | 7764 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 7765 | `					/* Pop the call instruction */` |
|   13558 | 7766 | `					iP1 = pInstr->iP1;` |
|   13558 | 7767 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    6778 | 7768 | `				}` |
|    6780 | 7769 | `			}` |
|  878612 | 7770 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 7771 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 7772 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 7773 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 7774 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 7775 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 7776 | `				int isSpecialIs = 0;` |
|      50 | 7777 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 7778 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 7779 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 7780 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 7781 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 7782 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 7783 | `						isSpecialIs = 1;` |
|       5 | 7784 | `					}` |
|      23 | 7785 | `				}` |
|      52 | 7786 | `				pInstr->iP1 = 0;` |
|      52 | 7787 | `				if( !isSpecialIs ){` |
|      38 | 7788 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 7789 | `				}` |
|      25 | 7790 | `			}` |
|  871805 | 7791 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 7792 | `			/* Prevent constant expansion for member/property names.` |
|       - | 7793 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 7794 | `			 * should not trigger constant lookup. */` |
|  100752 | 7795 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  100752 | 7796 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  100736 | 7797 | `				pInstr->iP1 = 0;` |
|   50367 | 7798 | `			}` |
|  100752 | 7799 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 7800 | `				/* Static member access,remember that */` |
|     158 | 7801 | `				iP1 = 1;` |
|     158 | 7802 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     158 | 7803 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      10 | 7804 | `					p3 = pInstr->p3;` |
|      10 | 7805 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       4 | 7806 | `				}` |
|      78 | 7807 | `			}` |
|   50375 | 7808 | `		}` |
|       - | 7809 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  896132 | 7810 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  448065 | 7811 | `	}` |
|  896244 | 7812 | `	if( nJmpIdx > 0 ){` |
|       - | 7813 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11098 | 7814 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11098 | 7815 | `		if( pInstr ){` |
|   11098 | 7816 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5548 | 7817 | `		}` |
|    5548 | 7818 | `	}` |
|  896244 | 7819 | `	return rc;` |
| 1181402 | 7820 |  |
|       - | 7821 | `/*` |
|       - | 7822 | ` * Compile a PHP expression.` |
|       - | 7823 | ` * According to the PHP language reference manual:` |
|       - | 7824 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 7825 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 7826 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 7827 | ` *  is "anything that has a value".` |
|       - | 7828 | ` * If something goes wrong while compiling the expression,this` |
|       - | 7829 | ` * function takes care of generating the appropriate error` |
|       - | 7830 | ` * message.` |
|       - | 7831 | ` */` |
|  638252 | 7832 | `static sxi32 PH7_CompileExpr(` |
|       - | 7833 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 7834 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 7835 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 7836 | `	)` |
|       2 | 7837 |  |
|       - | 7838 | `	ph7_expr_node *pRoot;` |
|       - | 7839 | `	SySet sExprNode;` |
|       - | 7840 | `	SyToken *pEnd;` |
|       - | 7841 | `	sxi32 nExpr;` |
|       - | 7842 | `	sxi32 iNest;` |
|       - | 7843 | `	sxi32 rc;` |
|       - | 7844 | `	/* Initialize worker variables */` |
|  638254 | 7845 | `	nExpr = 0;` |
|  638254 | 7846 | `	pRoot = 0;` |
|  638254 | 7847 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  638254 | 7848 | `	SySetAlloc(&sExprNode,0x10);` |
|  638254 | 7849 | `	rc = SXRET_OK;` |
|       - | 7850 | `	/* Delimit the expression */` |
|  638254 | 7851 | `	pEnd = pGen->pIn;` |
|  638254 | 7852 | `	iNest = 0;` |
| 4302800 | 7853 | `	while( pEnd < pGen->pEnd ){` |
| 4080140 | 7854 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 7855 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     234 | 7856 | `			iNest++;` |
| 4080024 | 7857 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     242 | 7858 | `			iNest--;` |
| 4079788 | 7859 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  415798 | 7860 | `			if( iNest <= 0 ){` |
|  415594 | 7861 | `				break;` |
|       - | 7862 | `			}` |
|     102 | 7863 | `		}` |
| 3664548 | 7864 | `		pEnd++;` |
|       2 | 7865 | `	}` |
|  638254 | 7866 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   10702 | 7867 | `		SyToken *pEnd2 = pGen->pIn;` |
|   10702 | 7868 | `		iNest = 0;` |
|       - | 7869 | `		/* Stop at the first comma */` |
|   21426 | 7870 | `		while( pEnd2 < pEnd ){` |
|   10726 | 7871 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       6 | 7872 | `				iNest++;` |
|   10724 | 7873 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       6 | 7874 | `				iNest--;` |
|   10720 | 7875 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       5 | 7876 | `				if( iNest <= 0 ){` |
|     ! 0 | 7877 | `					break;` |
|       - | 7878 | `				}` |
|       2 | 7879 | `			}` |
|   10726 | 7880 | `			pEnd2++;` |
|       2 | 7881 | `		}` |
|   10702 | 7882 | `		if( pEnd2 <pEnd ){` |
|     ! 0 | 7883 | `			pEnd = pEnd2;` |
|     ! 0 | 7884 | `		}` |
|    5350 | 7885 | `	}` |
|  638254 | 7886 | `	if( pEnd > pGen->pIn ){` |
|  638244 | 7887 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 7888 | `		/* Swap delimiter */` |
|  638244 | 7889 | `		pGen->pEnd = pEnd;` |
|       - | 7890 | `		/* Try to get an expression tree */` |
|  638244 | 7891 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  638244 | 7892 | `		if( rc == SXRET_OK && pRoot ){` |
|  638084 | 7893 | `			rc = SXRET_OK;` |
|  638084 | 7894 | `			if( xTreeValidator ){` |
|       - | 7895 | `				/* Call the upper layer validator callback */` |
|   13748 | 7896 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    6873 | 7897 | `			}` |
|  638084 | 7898 | `			if( rc != SXERR_ABORT ){` |
|       - | 7899 | `				/* Generate code for the given tree */` |
|  638084 | 7900 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  319041 | 7901 | `			}` |
|  638084 | 7902 | `			nExpr = 1;` |
|  319041 | 7903 | `		}` |
|       - | 7904 | `		/* Release the whole tree */` |
|  638244 | 7905 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 7906 | `		/* Synchronize token stream */` |
|  638244 | 7907 | `		pGen->pEnd = pTmp;` |
|  638244 | 7908 | `		pGen->pIn  = pEnd;` |
|  638244 | 7909 | `		if( rc == SXERR_ABORT ){` |
|       3 | 7910 | `			SySetRelease(&sExprNode);` |
|       3 | 7911 | `			return SXERR_ABORT;` |
|       - | 7912 | `		}` |
|  319120 | 7913 | `	}` |
|  638252 | 7914 | `	SySetRelease(&sExprNode);` |
|  638252 | 7915 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  319128 | 7916 |  |
|       - | 7917 | `/*` |
|       - | 7918 | ` * Return a pointer to the node construct handler associated` |
|       - | 7919 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 7920 | ` */` |
|  158968 | 7921 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 7922 |  |
|  158970 | 7923 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 7924 | `		/* Numeric literal: Either real or integer */` |
|   86854 | 7925 | `		return PH7_CompileNumLiteral;` |
|   72118 | 7926 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 7927 | `		/* Double quoted string */` |
|   15342 | 7928 | `		return PH7_CompileString;` |
|   56778 | 7929 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 7930 | `		/* Single quoted string */` |
|   56718 | 7931 | `		return PH7_CompileSimpleString;` |
|      62 | 7932 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 7933 | `		/* Heredoc */` |
|      28 | 7934 | `		return PH7_CompileHereDoc;` |
|      36 | 7935 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 7936 | `		/* Nowdoc */` |
|      29 | 7937 | `		return PH7_CompileNowDoc;` |
|       7 | 7938 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 7939 | `		/* Backtick quoted string */` |
|       5 | 7940 | `		return PH7_CompileBacktic;` |
|       - | 7941 | `	}` |
|       3 | 7942 | `	return 0;` |
|   79486 | 7943 |  |
|       - | 7944 | `/*` |
|       - | 7945 | ` * Compile an unset() statement.` |
|       - | 7946 | ` * unset($var, $arr[$key], ...);` |
|       - | 7947 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 7948 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 7949 | ` * parent array before extracting the element to unset.` |
|       - | 7950 | ` */` |
|    2574 | 7951 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 7952 |  |
|    2576 | 7953 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2576 | 7954 | `	sxu32 nIdx = 0;` |
|       - | 7955 | `	SyString sName;` |
|       - | 7956 | `	sxi32 rc;` |
|       - | 7957 | `	/* Jump the 'unset' keyword */` |
|    2576 | 7958 | `	pGen->pIn++;` |
|       - | 7959 | `	/* Save delimiter */` |
|    2576 | 7960 | `	pTmp = pGen->pEnd;` |
|       - | 7961 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2576 | 7962 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2576 | 7963 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 7964 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 7965 | `		SyToken *pClose;` |
|    2576 | 7966 | `		pGen->pIn++;   /* Skip '(' */` |
|    2576 | 7967 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2576 | 7968 | `		pEnd = pClose; /* Stop at ')' */` |
|    1287 | 7969 | `	}` |
|    2576 | 7970 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 7971 | `	/* Resolve the 'unset' builtin name once */` |
|    2576 | 7972 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     304 | 7973 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     304 | 7974 | `		if( pObj == 0 ){` |
|     ! 0 | 7975 | `			return SXERR_ABORT;` |
|       - | 7976 | `		}` |
|     304 | 7977 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     304 | 7978 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     151 | 7979 | `	}` |
|       - | 7980 | `	/* Compile each comma-separated argument */` |
|    8588 | 7981 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6014 | 7982 | `		if( pGen->pIn < pNext ){` |
|    6014 | 7983 | `			pGen->pEnd = pNext;` |
|    6014 | 7984 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 7985 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6014 | 7986 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 7987 | `				return SXERR_ABORT;` |
|       - | 7988 | `			}` |
|    6014 | 7989 | `			if( rc != SXERR_EMPTY ){` |
|       - | 7990 | `				/* Emit call for this single argument */` |
|    6012 | 7991 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6012 | 7992 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6012 | 7993 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3005 | 7994 | `			}` |
|    3006 | 7995 | `		}` |
|       - | 7996 | `		/* Jump trailing commas */` |
|    9452 | 7997 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3440 | 7998 | `			pNext++;` |
|       2 | 7999 | `		}` |
|    6014 | 8000 | `		pGen->pIn = pNext;` |
|       2 | 8001 | `	}` |
|       - | 8002 | `	/* Skip past the closing ')' if present */` |
|    2576 | 8003 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2576 | 8004 | `		pGen->pIn++;` |
|    1287 | 8005 | `	}` |
|       - | 8006 | `	/* Restore token stream */` |
|    2576 | 8007 | `	pGen->pEnd = pTmp;` |
|    2576 | 8008 | `	return SXRET_OK;` |
|    1289 | 8009 |  |
|       - | 8010 | `/*` |
|       - | 8011 | ` * PHP Language construct table.` |
|       - | 8012 | ` */` |
|       - | 8013 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 8014 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 8015 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 8016 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 8017 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 8018 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 8019 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 8020 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 8021 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 8022 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 8023 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 8024 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 8025 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 8026 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 8027 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 8028 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 8029 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 8030 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 8031 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 8032 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 8033 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 8034 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 8035 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 8036 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 8037 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 8038 | `};` |
|       - | 8039 | `/*` |
|       - | 8040 | ` * Return a pointer to the statement handler routine associated` |
|       - | 8041 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 8042 | ` */` |
|  387164 | 8043 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 8044 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 8045 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 8046 | `	)` |
|       2 | 8047 |  |
|  387166 | 8048 | `	sxu32 n = 0;` |
| 1627689 | 8049 | `	for(;;){` |
| 3255380 | 8050 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   45344 | 8051 | `			break;` |
|       - | 8052 | `		}` |
| 3210038 | 8053 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  341824 | 8054 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 8055 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 8056 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 8057 | `					/* 'static' (class context),return null */` |
|     ! 0 | 8058 | `					return 0;` |
|       - | 8059 | `				}` |
|     ! 0 | 8060 | `			}` |
|       - | 8061 | `			/* Return a pointer to the handler.` |
|       - | 8062 | `			*/` |
|  341824 | 8063 | `			return aLangConstruct[n].xConstruct;` |
|       - | 8064 | `		}` |
| 2868216 | 8065 | `		n++;` |
|       2 | 8066 | `	}` |
|   45344 | 8067 | `	if( pLookahed ){` |
|   45344 | 8068 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    7918 | 8069 | `			return PH7_CompileClassInterface;` |
|   37428 | 8070 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   37224 | 8071 | `			return PH7_CompileClass;` |
|     206 | 8072 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      54 | 8073 | `			return PH7_CompileTrait;` |
|     152 | 8074 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 | 8075 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 | 8076 | `				return PH7_CompileAbstractClass;` |
|     136 | 8077 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 8078 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 8079 | `				return PH7_CompileFinalClass;` |
|       - | 8080 | `		}` |
|      67 | 8081 | `	}` |
|       - | 8082 | `	/* Not a language construct */` |
|     136 | 8083 | `	return 0;` |
|  193584 | 8084 |  |
|       - | 8085 | `/*` |
|       - | 8086 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 8087 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 8088 | ` */` |
|     134 | 8089 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 8090 |  |
|       - | 8091 | `	int rc;` |
|     136 | 8092 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 | 8093 | `	if( rc == FALSE ){` |
|      40 | 8094 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 | 8095 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 8096 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 8097 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 8098 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 8099 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 8100 | `			*/` |
|       - | 8101 | `			){` |
|      34 | 8102 | `				rc = TRUE;` |
|      16 | 8103 | `		}` |
|      20 | 8104 | `	}` |
|     136 | 8105 | `	return rc;` |
|       2 | 8106 |  |
|       - | 8107 | `/*` |
|       - | 8108 | ` * Compile a PHP chunk.` |
|       - | 8109 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8110 | ` * takes care of generating the appropriate error message.` |
|       - | 8111 | ` */` |
|  519996 | 8112 | `static sxi32 GenStateCompileChunk(` |
|       - | 8113 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 8114 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 8115 | `	)` |
|       2 | 8116 |  |
|       - | 8117 | `	ProcLangConstruct xCons;` |
|       - | 8118 | `	sxi32 rc;` |
|  519998 | 8119 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  310656 | 8120 | `	for(;;){` |
|  621314 | 8121 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 8122 | `			/* No more input to process */` |
|   11408 | 8123 | `			break;` |
|       - | 8124 | `		}` |
|  609908 | 8125 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 8126 | `			/* Compile block */` |
|      12 | 8127 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      12 | 8128 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 8129 | `				break;` |
|       - | 8130 | `			}` |
|       7 | 8131 | `		}else{` |
|  609898 | 8132 | `			xCons = 0;` |
|  609898 | 8133 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  387166 | 8134 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 8135 | `				/* Try to extract a language construct handler */` |
|  387166 | 8136 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  387166 | 8137 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 8138 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 8139 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 8140 | `						&pGen->pIn->sData);` |
|       9 | 8141 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 8142 | `						break;` |
|       - | 8143 | `					}` |
|       - | 8144 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 8145 | `					 * this erroneous statement.` |
|       - | 8146 | `					 */` |
|       9 | 8147 | `					xCons = PH7_ErrorRecover;` |
|       4 | 8148 | `				}` |
|  416316 | 8149 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   39022 | 8150 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 8151 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 8152 | `				xCons = PH7_CompileLabel;` |
|      56 | 8153 | `			}` |
|  609898 | 8154 | `			if( xCons == 0 ){` |
|       - | 8155 | `				/* Assume an expression an try to compile it */` |
|  222748 | 8156 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  222748 | 8157 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 8158 | `					/* Pop l-value */` |
|  222620 | 8159 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  111309 | 8160 | `				}` |
|  111375 | 8161 | `			}else{` |
|       - | 8162 | `				/* Go compile the sucker */` |
|  387152 | 8163 | `				rc = xCons(&(*pGen));` |
|       - | 8164 | `			}` |
|  609898 | 8165 | `			if( rc == SXERR_ABORT ){` |
|       - | 8166 | `				/* Request to abort compilation */` |
|       3 | 8167 | `				break;` |
|       - | 8168 | `			}` |
|       - | 8169 | `		}` |
|       - | 8170 | `		/* Ignore trailing semi-colons ';' */` |
| 1010118 | 8171 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  400214 | 8172 | `			pGen->pIn++;` |
|       2 | 8173 | `		}` |
|  609906 | 8174 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 8175 | `			/* Compile a single statement and return */` |
|  508590 | 8176 | `			break;` |
|       - | 8177 | `		}` |
|       - | 8178 | `		/* LOOP ONE */` |
|       - | 8179 | `		/* LOOP TWO */` |
|       - | 8180 | `		/* LOOP THREE */` |
|       - | 8181 | `		/* LOOP FOUR */` |
|       2 | 8182 | `	}` |
|       - | 8183 | `	/* Return compilation status */` |
|  519998 | 8184 | `	return rc;` |
|       2 | 8185 |  |
|       - | 8186 | `/*` |
|       - | 8187 | ` * Compile a Raw PHP chunk.` |
|       - | 8188 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 8189 | ` * takes care of generating the appropriate error message.` |
|       - | 8190 | ` */` |
|   11410 | 8191 | `static sxi32 PH7_CompilePHP(` |
|       - | 8192 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 8193 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 8194 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 8195 | `	)` |
|       2 | 8196 |  |
|   11412 | 8197 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 8198 | `	sxi32 rc;` |
|       - | 8199 | `	/* Reset the token set */` |
|   11412 | 8200 | `	SySetReset(&(*pTokenSet));` |
|       - | 8201 | `	/* Mark as the default token set */` |
|   11412 | 8202 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 8203 | `	/* Advance the stream cursor */` |
|   11412 | 8204 | `	pGen->pRawIn++;` |
|       - | 8205 | `	/* Tokenize the PHP chunk first */` |
|   11412 | 8206 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 8207 | `	/* Point to the head and tail of the token stream. */` |
|   11412 | 8208 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11412 | 8209 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11412 | 8210 | `	if( is_expr ){` |
|     ! 0 | 8211 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 8212 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 8213 | `			/* A simple expression,compile it */` |
|     ! 0 | 8214 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 8215 | `		}` |
|       - | 8216 | `		/* Emit the DONE instruction */` |
|     ! 0 | 8217 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 8218 | `		return SXRET_OK;` |
|       - | 8219 | `	}` |
|   11412 | 8220 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 8221 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 8222 | `		/*` |
|       - | 8223 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 8224 | `		 * According to the PHP reference manual:` |
|       - | 8225 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 8226 | `		 *  immediately follow` |
|       - | 8227 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 8228 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 8229 | `		 * Symisc extension:` |
|       - | 8230 | `		 *   This short syntax works with all PHP opening` |
|       - | 8231 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 8232 | `		 *   only short tag.` |
|       - | 8233 | `		 */` |
|       - | 8234 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 8235 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 8236 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 8237 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 8238 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 8239 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 8240 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 8241 | `		}` |
|       3 | 8242 | `		return SXRET_OK;` |
|       - | 8243 | `	}` |
|       - | 8244 | `	/* Compile the PHP chunk */` |
|   11410 | 8245 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 8246 | `	/* Fix exceptions jumps */` |
|   11410 | 8247 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 8248 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11410 | 8249 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 8250 | `		rc = SXERR_ABORT;` |
|       1 | 8251 | `	}` |
|       - | 8252 | `	/* Reset container */` |
|   11410 | 8253 | `	SySetReset(&pGen->aGoto);` |
|   11410 | 8254 | `	SySetReset(&pGen->aLabel);` |
|       - | 8255 | `	/* Compilation result */` |
|   11410 | 8256 | `	return rc;` |
|    5707 | 8257 |  |
|       - | 8258 | `/*` |
|       - | 8259 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 8260 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 8261 | ` * This is the only compile interface exported from this file.` |
|       - | 8262 | ` */` |
|   13462 | 8263 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 8264 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 8265 | `	SyString *pScript,  /* Script to compile */` |
|       - | 8266 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 8267 | `	)` |
|       2 | 8268 |  |
|       - | 8269 | `	SySet aPhpToken,aRawToken;` |
|       - | 8270 | `	ph7_gen_state *pCodeGen;` |
|       - | 8271 | `	ph7_value *pRawObj;` |
|       - | 8272 | `	sxu32 nObjIdx;` |
|       - | 8273 | `	sxi32 nRawObj;` |
|       - | 8274 | `	int is_expr;` |
|       - | 8275 | `	sxi32 rc;` |
|   13464 | 8276 | `	if( pScript->nByte < 1 ){` |
|       - | 8277 | `		/* Nothing to compile */` |
|     ! 0 | 8278 | `		return PH7_OK;` |
|       - | 8279 | `	}` |
|       - | 8280 | `	/* Initialize the tokens containers */` |
|   13464 | 8281 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13464 | 8282 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   13464 | 8283 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   13464 | 8284 | `	is_expr = 0;` |
|   13464 | 8285 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 8286 | `		SyToken sTmp;` |
|       - | 8287 | `		/* PHP only: -*/` |
|    2656 | 8288 | `		sTmp.nLine = 1;` |
|    2656 | 8289 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2656 | 8290 | `		sTmp.pUserData = 0;` |
|    2656 | 8291 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2656 | 8292 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2656 | 8293 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 8294 | `			/* A simple PHP expression */` |
|     ! 0 | 8295 | `			is_expr = 1;` |
|     ! 0 | 8296 | `		}` |
|    1329 | 8297 | `	}else{` |
|       - | 8298 | `		/* Tokenize raw text */` |
|   10810 | 8299 | `		SySetAlloc(&aRawToken,32);` |
|   10810 | 8300 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 8301 | `	}` |
|   13464 | 8302 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 8303 | `	/* Process high-level tokens */` |
|   13464 | 8304 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   13464 | 8305 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   13464 | 8306 | `	rc = PH7_OK;` |
|   13464 | 8307 | `	if( is_expr ){` |
|       - | 8308 | `		/* Compile the expression */` |
|     ! 0 | 8309 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 8310 | `		goto cleanup;` |
|       - | 8311 | `	}` |
|   13464 | 8312 | `	nObjIdx = 0;` |
|       - | 8313 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 8314 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 8315 | `	 * preventing namespace bleeding across include()d files. */` |
|   13464 | 8316 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 8317 | `	/* Start the compilation process */` |
|   12139 | 8318 | `	for(;;){` |
|   35686 | 8319 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   13460 | 8320 | `			break; /* No more tokens to process */` |
|       - | 8321 | `		}` |
|   22228 | 8322 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 8323 | `			/* Compile the PHP chunk */` |
|   11412 | 8324 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11412 | 8325 | `			if( rc == SXERR_ABORT ){` |
|       5 | 8326 | `				break;` |
|       - | 8327 | `			}` |
|   11408 | 8328 | `			continue;` |
|       - | 8329 | `		}` |
|       - | 8330 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   10818 | 8331 | `		nRawObj = 0;` |
|   21634 | 8332 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 8333 | `			/* Consume the raw chunk without any processing */` |
|   10818 | 8334 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   10818 | 8335 | `			if( pRawObj == 0 ){` |
|     ! 0 | 8336 | `				rc = SXERR_MEM;` |
|     ! 0 | 8337 | `				break;` |
|       - | 8338 | `			}` |
|       - | 8339 | `			/* Mark as constant and emit the load constant instruction */` |
|   10818 | 8340 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   10818 | 8341 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   10818 | 8342 | `			++nRawObj;` |
|   10818 | 8343 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 8344 | `		}` |
|   10818 | 8345 | `		if( nRawObj > 0 ){` |
|       - | 8346 | `			/* Emit the consume instruction */` |
|   10818 | 8347 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5408 | 8348 | `		}` |
|    6733 | 8349 | `	}` |
|    6731 | 8350 | `cleanup:` |
|   13464 | 8351 | `	SySetRelease(&aRawToken);` |
|   13464 | 8352 | `	SySetRelease(&aPhpToken);` |
|   13464 | 8353 | `	return rc;` |
|    6733 | 8354 |  |
|       - | 8355 | `/*` |
|       - | 8356 | ` * Utility routines.Initialize the code generator.` |
|       - | 8357 | ` */` |
|    2626 | 8358 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 8359 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8360 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8361 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8362 | `	)` |
|       2 | 8363 |  |
|    2628 | 8364 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8365 | `	/* Zero the structure */` |
|    2628 | 8366 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 8367 | `	/* Initial state */` |
|    2628 | 8368 | `	pGen->pVm  = &(*pVm);` |
|    2628 | 8369 | `	pGen->xErr = xErr;` |
|    2628 | 8370 | `	pGen->pErrData = pErrData;` |
|    2628 | 8371 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2628 | 8372 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2628 | 8373 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2628 | 8374 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 8375 | `	/* Error log buffer */` |
|    2628 | 8376 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 8377 | `	/* General purpose working buffer */` |
|    2628 | 8378 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 8379 | `	/* Namespace state */` |
|    2628 | 8380 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2628 | 8381 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2628 | 8382 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2628 | 8383 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8384 | `	/* Create the global scope */` |
|    2628 | 8385 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 8386 | `	/* Point to the global scope */` |
|    2628 | 8387 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2628 | 8388 | `	return SXRET_OK;` |
|       2 | 8389 |  |
|       - | 8390 | `/*` |
|       - | 8391 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 8392 | ` */` |
|   15820 | 8393 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 8394 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 8395 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 8396 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 8397 | `	)` |
|       2 | 8398 |  |
|   15822 | 8399 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 8400 | `	GenBlock *pBlock,*pParent;` |
|       - | 8401 | `	/* Reset state */` |
|   15822 | 8402 | `	SySetReset(&pGen->aLabel);` |
|   15822 | 8403 | `	SySetReset(&pGen->aGoto);` |
|   15822 | 8404 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   15822 | 8405 | `	SyBlobRelease(&pGen->sWorker);` |
|   15822 | 8406 | `	SyBlobRelease(&pGen->sNamespace);` |
|   15822 | 8407 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   15822 | 8408 | `	SyHashRelease(&pGen->hUseImports);` |
|   15822 | 8409 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   15822 | 8410 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   15822 | 8411 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   15822 | 8412 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   15822 | 8413 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 8414 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 8415 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 8416 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 8417 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 8418 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 8419 | `	 * number of unique names, which is acceptable. */` |
|       - | 8420 | `	/* Point to the global scope */` |
|   15822 | 8421 | `	pBlock = pGen->pCurrent;` |
|   15822 | 8422 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 8423 | `		pParent = pBlock->pParent;` |
|     ! 0 | 8424 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 8425 | `		pBlock = pParent;` |
|     ! 0 | 8426 | `	}` |
|   15822 | 8427 | `	pGen->xErr = xErr;` |
|   15822 | 8428 | `	pGen->pErrData = pErrData;` |
|   15822 | 8429 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   15822 | 8430 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   15822 | 8431 | `	pGen->pIn = pGen->pEnd = 0;` |
|   15822 | 8432 | `	pGen->nErr = 0;` |
|   15822 | 8433 | `	return SXRET_OK;` |
|       2 | 8434 |  |
|       - | 8435 | `/*` |
|       - | 8436 | ` * Generate a compile-time error message.` |
|       - | 8437 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 8438 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 8439 | ` * abort compilation immediately.` |
|       - | 8440 | ` */` |
|     468 | 8441 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 8442 |  |
|     470 | 8443 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     470 | 8444 | `	const char *zErr = "Error";` |
|       - | 8445 | `	SyString *pFile;` |
|       - | 8446 | `	va_list ap;` |
|       - | 8447 | `	sxi32 rc;` |
|       - | 8448 | `	/* Reset the working buffer */` |
|     470 | 8449 | `	SyBlobReset(pWorker);` |
|       - | 8450 | `	/* Peek the processed file path if available */` |
|     470 | 8451 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     470 | 8452 | `	if( nErrType == E_ERROR ){` |
|       - | 8453 | `		/* Increment the error counter */` |
|     424 | 8454 | `		pGen->nErr++;` |
|     424 | 8455 | `		if( pGen->nErr > 15 ){` |
|       - | 8456 | `			/* Error count limit reached */` |
|       5 | 8457 | `			if( pGen->xErr ){` |
|       5 | 8458 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 8459 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 8460 | `				if( pFile ){` |
|       5 | 8461 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 8462 | `				}` |
|       5 | 8463 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 8464 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 8465 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 8466 | `				}` |
|       2 | 8467 | `			}` |
|       - | 8468 | `			/* Abort immediately */` |
|       5 | 8469 | `			return SXERR_ABORT;` |
|       - | 8470 | `		}` |
|     209 | 8471 | `	}` |
|     466 | 8472 | `	if( pGen->xErr == 0 ){` |
|       - | 8473 | `		/* No available error consumer,return immediately */` |
|       3 | 8474 | `		return SXRET_OK;` |
|       - | 8475 | `	}` |
|     463 | 8476 | `	switch(nErrType){` |
|     417 | 8477 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      29 | 8478 | `	case E_WARNING: zErr = "Warning";     break;` |
|      11 | 8479 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 8480 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 8481 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 8482 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 8483 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 8484 | `	default:` |
|     ! 0 | 8485 | `		break;` |
|       - | 8486 | `	}` |
|     463 | 8487 | `	rc = SXRET_OK;` |
|       - | 8488 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     463 | 8489 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     463 | 8490 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     463 | 8491 | `	va_start(ap,zFormat);` |
|     463 | 8492 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     463 | 8493 | `	va_end(ap);` |
|     463 | 8494 | `	if( pFile ){` |
|     463 | 8495 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     231 | 8496 | `	}` |
|       - | 8497 | `	/* Append a new line */` |
|     463 | 8498 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     463 | 8499 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 8500 | `		/* Consume the generated error message */` |
|     463 | 8501 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     231 | 8502 | `	}` |
|     463 | 8503 | `	return rc;` |
|     236 | 8504 |  |
|       - | 8505 |  |
