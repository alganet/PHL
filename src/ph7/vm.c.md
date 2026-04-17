# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5953/7762 lines (76.69%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `#include <stddef.h>` |
|        - |     8 | `#include <stdlib.h>` |
|        - |     9 | `/*` |
|        - |    10 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    11 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    12 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    13 | ` * statements.` |
|        - |    14 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    15 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    16 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    17 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    18 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    19 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    20 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    21 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    22 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    23 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    24 | ` * and so on.` |
|        - |    25 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    26 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    27 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    28 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    29 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    30 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    31 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    32 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    33 | ` */` |
|        - |    34 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    35 | `/*` |
|        - |    36 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    37 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    38 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    39 | ` * a new memory object.` |
|        - |    40 | ` */` |
|        - |    41 | `typedef struct VmSlot VmSlot;` |
|        - |    42 | `struct VmSlot` |
|        - |    43 |  |
|        - |    44 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    45 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    46 | `};` |
|        - |    47 | `/*` |
|        - |    48 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    49 | ` * follwoing table.` |
|        - |    50 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    51 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    52 | ` * the reference implementation is consistent,solid and it's` |
|        - |    53 | ` * behavior resemble the C++ reference mechanism.` |
|        - |    54 | ` * Refer to the official for more information on this powerful` |
|        - |    55 | ` * extension.` |
|        - |    56 | ` */` |
|        - |    57 | `struct VmRefObj` |
|        - |    58 |  |
|        - |    59 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |    60 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |    61 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |    62 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |    63 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |    64 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |    65 | `};` |
|        - |    66 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |    67 | `/* VmObEntry struct moved to ph7int.h */` |
|        - |    68 | `/*` |
|        - |    69 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |    70 | ` * is stored in an instance of the following structure.` |
|        - |    71 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |    72 | ` */` |
|        - |    73 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |    74 | `struct VmShutdownCB` |
|        - |    75 |  |
|        - |    76 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |    77 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |    78 | `	int nArg;             /* Total number of given arguments */` |
|        - |    79 | `};` |
|        - |    80 | `/*` |
|        - |    81 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |    82 | ` * is stored in an instance of the following structure.` |
|        - |    83 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |    84 | ` */` |
|        - |    85 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|        - |    86 | `struct VmAutoloadCB` |
|        - |    87 |  |
|        - |    88 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|        - |    89 | `};` |
|        - |    90 | `/* Uncaught exception code value */` |
|        - |    91 | `#define PH7_EXCEPTION -255` |
|        - |    92 |  |
|        - |    93 | `/*` |
|        - |    94 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |    95 | ` */` |
|   875756 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   875758 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   875724 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   875714 |   104 | `	return FALSE;` |
|   437902 |   105 |  |
|        - |   106 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   107 | `/*` |
|        - |   108 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   109 | ` * it can be expanded from the target PHP program.` |
|        - |   110 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   111 | ` * simple and work as follows:` |
|        - |   112 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   113 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   114 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   115 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   116 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   117 | ` * (Windows,Linux,...) and so on.` |
|        - |   118 | ` * Please refer to the official documentation for additional information.` |
|        - |   119 | ` */` |
|   569000 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   121 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   122 | `	const SyString *pName,  /* Constant name */` |
|        - |   123 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   124 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   125 | `	)` |
|        2 |   126 |  |
|        - |   127 | `	ph7_constant *pCons;` |
|        - |   128 | `	SyHashEntry *pEntry;` |
|        - |   129 | `	char *zDupName;` |
|        - |   130 | `	sxi32 rc;` |
|   569002 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   569002 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   568998 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   568998 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   568998 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   568998 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   568998 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   568998 |   152 | `	pCons->xExpand = xExpand;` |
|   568998 |   153 | `	pCons->pUserData = pUserData;` |
|   568998 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   568998 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   568998 |   161 | `	return SXRET_OK;` |
|   284502 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1251416 |   170 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   171 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   172 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   173 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   174 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   175 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   176 | `	)` |
|        2 |   177 |  |
|        - |   178 | `	ph7_user_func *pFunc;` |
|        - |   179 | `	char *zDup;` |
|        - |   180 | `	/* Allocate a new user function */` |
|  1251418 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1251418 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1251418 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1251418 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1251418 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1251418 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1251418 |   195 | `	pFunc->pVm   = pVm;` |
|  1251418 |   196 | `	pFunc->xFunc = xFunc;` |
|  1251418 |   197 | `	pFunc->pUserData = pUserData;` |
|  1251418 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1251418 |   200 | `	*ppOut = pFunc;` |
|  1251418 |   201 | `	return SXRET_OK;` |
|   625710 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1254038 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   212 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   213 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   214 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   215 | `	void *pUserData           /* Foreign function private data */` |
|        - |   216 | `	)` |
|        2 |   217 |  |
|        - |   218 | `	ph7_user_func *pFunc;` |
|        - |   219 | `	SyHashEntry *pEntry;` |
|        - |   220 | `	sxi32 rc;` |
|        - |   221 | `	/* Overwrite any previously registered function with the same name */` |
|  1254040 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1254040 |   223 | `	if( pEntry ){` |
|     2624 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2624 |   225 | `		pFunc->pUserData = pUserData;` |
|     2624 |   226 | `		pFunc->xFunc = xFunc;` |
|     2624 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2624 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1251418 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1251418 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1251418 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1251418 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1251418 |   243 | `	return SXRET_OK;` |
|   627021 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   230532 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   230534 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   230534 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   230534 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   230534 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   230534 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   230534 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   230534 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   230534 |   272 | `	pFunc->iFlags = iFlags;` |
|   230534 |   273 | `	pFunc->pUserData = pUserData;` |
|        - |   274 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   275 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   230534 |   276 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   230534 |   277 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   230534 |   278 | `	return SXRET_OK;` |
|        2 |   279 |  |
|        - |   280 | `/*` |
|        - |   281 | ` * Namespace-aware function lookup.` |
|        - |   282 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   283 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   284 | ` */` |
|        - |   285 | `/*` |
|        - |   286 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   287 | ` */` |
|   707100 |   288 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   289 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   290 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   291 | `	SyString *pName     /* Function name */` |
|        - |   292 | `	)` |
|        2 |   293 |  |
|        - |   294 | `	SyHashEntry *pEntry;` |
|        - |   295 | `	sxi32 rc;` |
|   707102 |   296 | `	if( pName == 0 ){` |
|        - |   297 | `		/* Use the built-in name */` |
|    39114 |   298 | `		pName = &pFunc->sName;` |
|    19556 |   299 | `	}` |
|        - |   300 | `	/* Check for duplicates (functions with the same name) first */` |
|   707102 |   301 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   707102 |   302 | `	if( pEntry ){` |
|   523834 |   303 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   523834 |   304 | `		if( pLink != pFunc ){` |
|        - |   305 | `			/* Link */` |
|      188 |   306 | `			pFunc->pNextName = pLink;` |
|      188 |   307 | `			pEntry->pUserData = pFunc;` |
|       93 |   308 | `		}` |
|   523834 |   309 | `		return SXRET_OK;` |
|        - |   310 | `	}` |
|        - |   311 | `	/* First time seen */` |
|   183270 |   312 | `	pFunc->pNextName = 0;` |
|   183270 |   313 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   183270 |   314 | `	return rc;` |
|   353552 |   315 |  |
|        - |   316 | `/*` |
|        - |   317 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   318 | ` */` |
|    53684 |   319 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   320 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   321 | `	ph7_class *pClass /* Target Class */` |
|        - |   322 | `	)` |
|        2 |   323 |  |
|    53686 |   324 | `	SyString *pName = &pClass->sName;` |
|        - |   325 | `	SyHashEntry *pEntry;` |
|        - |   326 | `	sxi32 rc;` |
|        - |   327 | `	/* Check for duplicates */` |
|    53686 |   328 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    53686 |   329 | `	if( pEntry ){` |
|       31 |   330 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   331 | `		/* Link entry with the same name */` |
|       31 |   332 | `		pClass->pNextName = pLink;` |
|       31 |   333 | `		pEntry->pUserData = pClass;` |
|       31 |   334 | `		return SXRET_OK;` |
|        - |   335 | `	}` |
|    53656 |   336 | `	pClass->pNextName = 0;` |
|        - |   337 | `	/* Perform a simple hashtable insertion */` |
|    53656 |   338 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    53656 |   339 | `	return rc;` |
|    26844 |   340 |  |
|        - |   341 | `/*` |
|        - |   342 | ` * Instruction builder interface.` |
|        - |   343 | ` */` |
|  3972514 |   344 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   345 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   346 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   347 | `	sxi32 iP1,    /* First operand */` |
|        - |   348 | `	sxu32 iP2,    /* Second operand */` |
|        - |   349 | `	void *p3,     /* Third operand */` |
|        - |   350 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   351 | `	)` |
|        2 |   352 |  |
|        - |   353 | `	VmInstr sInstr;` |
|        - |   354 | `	sxi32 rc;` |
|        - |   355 | `	/* Fill the VM instruction */` |
|  3972516 |   356 | `	sInstr.iOp = (sxu8)iOp;` |
|  3972516 |   357 | `	sInstr.iP1 = iP1;` |
|  3972516 |   358 | `	sInstr.iP2 = iP2;` |
|  3972516 |   359 | `	sInstr.p3  = p3;` |
|  3972516 |   360 | `	if( pIndex ){` |
|        - |   361 | `		/* Instruction index in the bytecode array */` |
|   215604 |   362 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   107801 |   363 | `	}` |
|        - |   364 | `	/* Finally,record the instruction */` |
|  3972516 |   365 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3972516 |   366 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   367 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   368 | `		/* Fall throw */` |
|      ! 0 |   369 | `	}` |
|  3972516 |   370 | `	return rc;` |
|        2 |   371 |  |
|        - |   372 | `/*` |
|        - |   373 | ` * Swap the current bytecode container with the given one.` |
|        - |   374 | ` */` |
|   515932 |   375 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   376 |  |
|   515934 |   377 | `	if( pContainer == 0 ){` |
|        - |   378 | `		/* Point to the default container */` |
|      ! 0 |   379 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   380 | `	}else{` |
|        - |   381 | `		/* Change container */` |
|   515934 |   382 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   383 | `	}` |
|   515934 |   384 | `	return SXRET_OK;` |
|        2 |   385 |  |
|        - |   386 | `/*` |
|        - |   387 | ` * Return the current bytecode container.` |
|        - |   388 | ` */` |
|   257966 |   389 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   390 |  |
|   257968 |   391 | `	return pVm->pByteContainer;` |
|        2 |   392 |  |
|        - |   393 | `/*` |
|        - |   394 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   395 | ` */` |
|   212590 |   396 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   397 |  |
|        - |   398 | `	VmInstr *pInstr;` |
|   212592 |   399 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   212592 |   400 | `	return pInstr;` |
|        2 |   401 |  |
|        - |   402 | `/*` |
|        - |   403 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   404 | ` */` |
|  1194356 |   405 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   406 |  |
|  1194358 |   407 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   408 |  |
|        - |   409 | `/*` |
|        - |   410 | ` * Pop the last VM instruction.` |
|        - |   411 | ` */` |
|   196886 |   412 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   413 |  |
|   196888 |   414 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   415 |  |
|        - |   416 | `/*` |
|        - |   417 | ` * Peek the last VM instruction.` |
|        - |   418 | ` */` |
|   782854 |   419 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   420 |  |
|   782856 |   421 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   422 |  |
|    30928 |   423 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   424 |  |
|        - |   425 | `	VmInstr *aInstr;` |
|        - |   426 | `	sxu32 n;` |
|    30930 |   427 | `	n = SySetUsed(pVm->pByteContainer);` |
|    30930 |   428 | `	if( n < 2 ){` |
|      ! 0 |   429 | `		return 0;` |
|        - |   430 | `	}` |
|    30930 |   431 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    30930 |   432 | `	return &aInstr[n - 2];` |
|    15466 |   433 |  |
|        - |   434 | `/*` |
|        - |   435 | ` * Allocate a new virtual machine frame.` |
|        - |   436 | ` */` |
|    20078 |   437 | `static VmFrame * VmNewFrame(` |
|        - |   438 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   439 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   440 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   441 | `	)` |
|        2 |   442 |  |
|        - |   443 | `	VmFrame *pFrame;` |
|        - |   444 | `	/* Allocate a new vm frame */` |
|    20080 |   445 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    20080 |   446 | `	if( pFrame == 0 ){` |
|      ! 0 |   447 | `		return 0;` |
|        - |   448 | `	}` |
|        - |   449 | `	/* Zero the structure */` |
|    20080 |   450 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   451 | `	/* Initialize frame fields */` |
|    20080 |   452 | `	pFrame->pUserData = pUserData;` |
|    20080 |   453 | `	pFrame->pThis = pThis;` |
|    20080 |   454 | `	pFrame->pVm = pVm;` |
|    20080 |   455 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    20080 |   456 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    20080 |   457 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    20080 |   458 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    20080 |   459 | `	return pFrame;` |
|    10041 |   460 |  |
|        - |   461 | `/* Forward declaration */` |
|        - |   462 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   463 | `/*` |
|        - |   464 | ` * Enter a VM frame.` |
|        - |   465 | ` */` |
|    20032 |   466 | `static sxi32 VmEnterFrame(` |
|        - |   467 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   468 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   469 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   470 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   471 | `	)` |
|        2 |   472 |  |
|        - |   473 | `	VmFrame *pFrame;` |
|        - |   474 | `	/* Allocate a new frame */` |
|    20034 |   475 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    20034 |   476 | `	if( pFrame == 0 ){` |
|      ! 0 |   477 | `		return SXERR_MEM;` |
|        - |   478 | `	}` |
|        - |   479 | `	/* Link to the list of active VM frame */` |
|    20034 |   480 | `	pFrame->pParent = pVm->pFrame;` |
|    20034 |   481 | `	pVm->pFrame = pFrame;` |
|    20034 |   482 | `	if( ppFrame ){` |
|        - |   483 | `		/* Write a pointer to the new VM frame */` |
|    17098 |   484 | `		*ppFrame = pFrame;` |
|     8548 |   485 | `	}` |
|    20034 |   486 | `	return SXRET_OK;` |
|    10018 |   487 |  |
|        - |   488 | `/*` |
|        - |   489 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   490 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   491 | ` * information.` |
|        - |   492 | ` */` |
|       58 |   493 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   494 |  |
|        - |   495 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   496 | `	SyHashEntry *pEntry = 0;` |
|        - |   497 | `	sxi32 rc;` |
|        - |   498 | `	/* Point to the upper frame */` |
|       60 |   499 | `	pFrame = pVm->pFrame;` |
|       60 |   500 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   501 | `	pTarget = pFrame;` |
|       60 |   502 | `	pFrame = pTarget->pParent;` |
|       60 |   503 | `	while( pFrame ){` |
|       60 |   504 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   505 | `			/* Query the current frame */` |
|       60 |   506 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   507 | `			if( pEntry ){` |
|        - |   508 | `				/* Variable found */` |
|       60 |   509 | `				break;` |
|        - |   510 | `			}` |
|      ! 0 |   511 | `		}` |
|        - |   512 | `		/* Point to the upper frame */` |
|      ! 0 |   513 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   514 | `	}` |
|       60 |   515 | `	if( pEntry == 0 ){` |
|        - |   516 | `		/* Inexistant variable */` |
|      ! 0 |   517 | `		return SXERR_NOTFOUND;` |
|        - |   518 | `	}` |
|        - |   519 | `	/* Link to the current frame */` |
|       60 |   520 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   521 | `	if( rc == SXRET_OK ){` |
|        - |   522 | `		sxu32 nIdx;` |
|       60 |   523 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   524 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   525 | `	}` |
|       60 |   526 | `	return rc;` |
|       31 |   527 |  |
|        - |   528 | `/*` |
|        - |   529 | ` * Leave the top-most active frame.` |
|        - |   530 | ` */` |
|    17086 |   531 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   532 |  |
|    17088 |   533 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    17088 |   534 | `	if( pCurFrame ){` |
|        - |   535 | `		/* Unlink from the list of active VM frame */` |
|    17088 |   536 | `		pVm->pFrame = pCurFrame->pParent;` |
|    17088 |   537 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   538 | `			VmSlot  *aSlot;` |
|        - |   539 | `			sxu32 n;` |
|        - |   540 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    16868 |   541 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   114194 |   542 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   543 | `				/* Unset the local variable */` |
|    97328 |   544 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    48665 |   545 | `			}` |
|        - |   546 | `			/* Remove local reference */` |
|    16868 |   547 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   114256 |   548 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    97390 |   549 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    48696 |   550 | `			}` |
|     8433 |   551 | `		}` |
|        - |   552 | `		/* Release internal containers */` |
|    17088 |   553 | `		SyHashRelease(&pCurFrame->hVar);` |
|    17088 |   554 | `		SySetRelease(&pCurFrame->sArg);` |
|    17088 |   555 | `		SySetRelease(&pCurFrame->sLocal);` |
|    17088 |   556 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   557 | `		/* Release the whole structure */` |
|    17088 |   558 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     8543 |   559 | `	}` |
|    17088 |   560 |  |
|        - |   561 | `/*` |
|        - |   562 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   563 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   564 | ` * should be skipped when looking for the real execution context.` |
|        - |   565 | ` */` |
|  6827354 |   566 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   567 |  |
|  6828584 |   568 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1230 |   569 | `		pFrame = pFrame->pParent;` |
|        2 |   570 | `	}` |
|  6827356 |   571 | `	return pFrame;` |
|        2 |   572 |  |
|        - |   573 | `/*` |
|        - |   574 | ` * Compare two functions signature and return the comparison result.` |
|        - |   575 | ` */` |
|      836 |   576 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   577 |  |
|      837 |   578 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   579 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   580 | `	const char *zSin = pSecond->zString;` |
|      837 |   581 | `	const char *zFin = pFirst->zString;` |
|      837 |   582 | `	const char *zPtr = zFin;` |
|      421 |   583 | `	for(;;){` |
|      843 |   584 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   585 | `			break;` |
|        - |   586 | `		}` |
|       19 |   587 | `		if( zFin[0] != zSin[0] ){` |
|        - |   588 | `			/* mismatch */` |
|       13 |   589 | `			break;` |
|        - |   590 | `		}` |
|        7 |   591 | `		zFin++;` |
|        7 |   592 | `		zSin++;` |
|        1 |   593 | `	}` |
|      837 |   594 | `	return (int)(zFin-zPtr);` |
|        1 |   595 |  |
|        - |   596 | `/*` |
|        - |   597 | ` * Select the appropriate VM function for the current call context.` |
|        - |   598 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   599 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   600 | ` * Refer to the official documentation for more information.` |
|        - |   601 | ` */` |
|      138 |   602 | `static ph7_vm_func * VmOverload(` |
|        - |   603 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   604 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   605 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   606 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   607 | `	)` |
|        2 |   608 |  |
|        - |   609 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   610 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   611 | `	ph7_vm_func *pLink;` |
|        - |   612 | `	SyString sArgSig;` |
|        - |   613 | `	SyBlob sSig;` |
|        - |   614 |  |
|      140 |   615 | `	pLink = pList;` |
|      140 |   616 | `	i = 0;` |
|        - |   617 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   618 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   619 | `		if( pLink == 0 ){` |
|       78 |   620 | `			break;` |
|        - |   621 | `		}` |
|      948 |   622 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   623 | `			/* Candidate for overloading */` |
|      902 |   624 | `			apSet[i++] = pLink;` |
|      450 |   625 | `		}` |
|        - |   626 | `		/* Point to the next entry */` |
|      948 |   627 | `		pLink = pLink->pNextName;` |
|        2 |   628 | `	}` |
|      140 |   629 | `	if( i < 1 ){` |
|        - |   630 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   631 | `		return pList;` |
|        - |   632 | `	}` |
|      140 |   633 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   634 | `		/* Return the only candidate */` |
|       32 |   635 | `		return apSet[0];` |
|        - |   636 | `	}` |
|        - |   637 | `	/* Calculate function signature */` |
|      109 |   638 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   639 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   640 | `		int c = 'n'; /* null */` |
|      259 |   641 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   642 | `			/* Hashmap */` |
|       45 |   643 | `			c = 'h';` |
|      237 |   644 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   645 | `			/* bool */` |
|      ! 0 |   646 | `			c = 'b';` |
|      215 |   647 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   648 | `			/* int */` |
|        7 |   649 | `			c = 'i';` |
|      212 |   650 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   651 | `			/* String */` |
|      107 |   652 | `			c = 's';` |
|      156 |   653 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   654 | `			/* Float */` |
|      ! 0 |   655 | `			c = 'f';` |
|      103 |   656 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   657 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   658 | `			int marker = 'o';` |
|        3 |   659 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   660 | `			SyString *pName = &pClass->sName;` |
|        3 |   661 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   662 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   663 | `			c = -1;` |
|        1 |   664 | `		}` |
|      259 |   665 | `		if( c > 0 ){` |
|      257 |   666 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   667 | `		}` |
|      130 |   668 | `	}` |
|      109 |   669 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   670 | `	iTarget = 0;` |
|      109 |   671 | `	iMax = -1;` |
|        - |   672 | `	/* Select the appropriate function */` |
|      945 |   673 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   674 | `		/* Compare the two signatures */` |
|      837 |   675 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   676 | `		if( iCur > iMax ){` |
|      113 |   677 | `			iMax = iCur;` |
|      113 |   678 | `			iTarget = j;` |
|       56 |   679 | `		}` |
|      419 |   680 | `	}` |
|      109 |   681 | `	SyBlobRelease(&sSig);` |
|        - |   682 | `	/* Appropriate function for the current call context */` |
|      109 |   683 | `	return apSet[iTarget];` |
|       71 |   684 |  |
|        - |   685 | `/* Forward declaration */` |
|        - |   686 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   687 | `/*` |
|        - |   688 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   689 | ` * it can be instanciated from the executed PHP script.` |
|        - |   690 | ` */` |
|   155406 |   691 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   692 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   693 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   694 | `	)` |
|        2 |   695 |  |
|        - |   696 | `	ph7_class_method *pMeth;` |
|        - |   697 | `	ph7_class_attr *pAttr;` |
|        - |   698 | `	SyHashEntry *pEntry;` |
|        - |   699 | `	sxi32 rc;` |
|        - |   700 | `	/* Reset the loop cursor */` |
|   155408 |   701 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   702 | `	/* Process only static and constant attribute */` |
|   608949 |   703 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   704 | `		/* Extract the current attribute */` |
|   375840 |   705 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   375840 |   706 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   707 | `			ph7_value *pMemObj;` |
|        - |   708 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1688 |   709 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1688 |   710 | `			if( pMemObj == 0 ){` |
|      ! 0 |   711 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   712 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   713 | `					&pClass->sName,&pAttr->sName` |
|        - |   714 | `					);` |
|      ! 0 |   715 | `				return SXERR_MEM;` |
|        - |   716 | `			}` |
|     1688 |   717 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   718 | `				/* Initialize attribute default value (any complex expression) */` |
|     1684 |   719 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      841 |   720 | `			}` |
|        - |   721 | `			/* Record attribute index */` |
|     1688 |   722 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   723 | `			/* Install static attribute in the reference table */` |
|     1688 |   724 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   725 | `			/* If this is a typed static property, register the slot so the` |
|        - |   726 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   727 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   728 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1688 |   729 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   730 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   731 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   732 | `					return SXERR_MEM;` |
|        - |   733 | `				}` |
|       10 |   734 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   735 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   736 | `				pVmAttrS->iState = 0;` |
|       10 |   737 | `				pVmAttrS->pOwner = pClass;` |
|        - |   738 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   739 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   740 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   741 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   742 | `				}` |
|       10 |   743 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   744 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   745 | `					return SXERR_MEM;` |
|        - |   746 | `				}` |
|        4 |   747 | `			}` |
|      843 |   748 | `		}` |
|        2 |   749 | `	}` |
|        - |   750 | `	/* Install class methods */` |
|   155408 |   751 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   752 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   753 | `		 */` |
|    76916 |   754 | `		return SXRET_OK;` |
|        - |   755 | `	}` |
|        - |   756 | `	/* Create constructor alias if not yet done */` |
|    78494 |   757 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   758 | `		/* User constructor with the same base class name */` |
|     6106 |   759 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6106 |   760 | `		if( pEntry ){` |
|      ! 0 |   761 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   762 | `			/* Create the alias */` |
|      ! 0 |   763 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   764 | `		}` |
|     3052 |   765 | `	}` |
|        - |   766 | `	/* Install the methods now */` |
|    78494 |   767 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   785736 |   768 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   667998 |   769 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   667998 |   770 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   667990 |   771 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   667990 |   772 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   773 | `				return rc;` |
|        - |   774 | `			}` |
|   333994 |   775 | `		}` |
|        2 |   776 | `	}` |
|        - |   777 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    78494 |   778 | `	pClass->bMounted = TRUE;` |
|    78494 |   779 | `	return SXRET_OK;` |
|    77705 |   780 |  |
|        - |   781 | `/*` |
|        - |   782 | ` * Allocate a private frame for attributes of the given` |
|        - |   783 | ` * class instance (Object in the PHP jargon).` |
|        - |   784 | ` */` |
|     1710 |   785 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   786 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   787 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   788 | `	)` |
|        2 |   789 |  |
|     1712 |   790 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   791 | `	ph7_class_attr *pAttr;` |
|        - |   792 | `	SyHashEntry *pEntry;` |
|        - |   793 | `	sxi32 rc;` |
|        - |   794 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1712 |   795 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7070 |   796 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   797 | `		VmClassAttr *pVmAttr;` |
|        - |   798 | `		/* Extract the current attribute */` |
|     5360 |   799 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5360 |   800 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     5360 |   801 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   802 | `			return SXERR_MEM;` |
|        - |   803 | `		}` |
|     5360 |   804 | `		pVmAttr->pAttr = pAttr;` |
|     5360 |   805 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   806 | `			ph7_value *pMemObj;` |
|        - |   807 | `			/* Reserve a memory object for this attribute */` |
|     5336 |   808 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     5336 |   809 | `			if( pMemObj == 0 ){` |
|      ! 0 |   810 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   811 | `				return SXERR_MEM;` |
|        - |   812 | `			}` |
|     5336 |   813 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     5336 |   814 | `			pVmAttr->iState = 0;` |
|     5336 |   815 | `			pVmAttr->pOwner = pClass;` |
|     5336 |   816 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   817 | `				/* Initialize attribute default value (any complex expression) */` |
|     1826 |   818 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4424 |   819 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   820 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   821 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       64 |   822 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       31 |   823 | `			}` |
|     5336 |   824 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     5336 |   825 | `			if( rc != SXRET_OK ){` |
|        - |   826 | `				VmSlot sSlot;` |
|        - |   827 | `				/* Restore memory object */` |
|      ! 0 |   828 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   829 | `				sSlot.pUserData = 0;` |
|      ! 0 |   830 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   831 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   832 | `				return SXERR_MEM;` |
|        - |   833 | `			}` |
|        - |   834 | `			/* Install attribute in the reference table */` |
|     5336 |   835 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   836 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   837 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   838 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     5336 |   839 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      156 |   840 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      156 |   841 | `				if( rc != SXRET_OK ){` |
|        - |   842 | `					VmSlot sSlot;` |
|      ! 0 |   843 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   844 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   845 | `					sSlot.pUserData = 0;` |
|      ! 0 |   846 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   847 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   848 | `					return SXERR_MEM;` |
|        - |   849 | `				}` |
|       77 |   850 | `			}` |
|     2669 |   851 | `		}else{` |
|        - |   852 | `			/* Install static/constant attribute */` |
|       26 |   853 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   854 | `			pVmAttr->iState = 0;` |
|       26 |   855 | `			pVmAttr->pOwner = pClass;` |
|       26 |   856 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   857 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   858 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   859 | `				return SXERR_MEM;` |
|        - |   860 | `			}` |
|        - |   861 | `		}` |
|        2 |   862 | `	}` |
|     1712 |   863 | `	return SXRET_OK;` |
|      857 |   864 |  |
|        - |   865 | `/* Forward declaration */` |
|        - |   866 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   867 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   868 | `/*` |
|        - |   869 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   870 | ` */` |
|        - |   871 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   872 | `/*` |
|        - |   873 | ` * Reserve a constant memory object.` |
|        - |   874 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   875 | ` */` |
|   423712 |   876 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   877 |  |
|        - |   878 | `	ph7_value *pObj;` |
|        - |   879 | `	sxi32 rc;` |
|   423714 |   880 | `	if( pIndex ){` |
|        - |   881 | `		/* Object index in the object table */` |
|   414906 |   882 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   207452 |   883 | `	}` |
|        - |   884 | `	/* Reserve a slot for the new object */` |
|   423714 |   885 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   423714 |   886 | `	if( rc != SXRET_OK ){` |
|        - |   887 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   888 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   889 | `		 */` |
|      ! 0 |   890 | `		return 0;` |
|        - |   891 | `	}` |
|   423714 |   892 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   423714 |   893 | `	return pObj;` |
|   211858 |   894 |  |
|        - |   895 | `/*` |
|        - |   896 | ` * Reserve a memory object.` |
|        - |   897 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   898 | ` */` |
|  2147976 |   899 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   900 |  |
|        - |   901 | `	ph7_value *pObj;` |
|        - |   902 | `	sxi32 rc;` |
|  2147978 |   903 | `	if( pIndex ){` |
|        - |   904 | `		/* Object index in the object table */` |
|  2147978 |   905 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073988 |   906 | `	}` |
|        - |   907 | `	/* Reserve a slot for the new object */` |
|  2147978 |   908 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2147978 |   909 | `	if( rc != SXRET_OK ){` |
|        - |   910 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   911 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   912 | `		 */` |
|      ! 0 |   913 | `		return 0;` |
|        - |   914 | `	}` |
|  2147978 |   915 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2147978 |   916 | `	return pObj;` |
|  1073990 |   917 |  |
|        - |   918 | `/* Forward declaration */` |
|        - |   919 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   920 | `/* Forward declarations for Fiber C functions */` |
|        - |   921 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   922 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   923 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   924 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   925 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   926 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   927 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   928 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   929 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   930 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   931 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   932 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   933 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   934 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   935 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   936 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |   937 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |   938 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |   939 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   940 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   941 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   942 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   943 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   944 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   945 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   946 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   947 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   948 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   949 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   950 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   951 | `/*` |
|        - |   952 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   953 | ` * directly as foreign functions.` |
|        - |   954 | ` */` |
|        - |   955 | `#define PH7_BUILTIN_LIB \` |
|        - |   956 | `	"interface Throwable {"\` |
|        - |   957 | `	"public function getMessage();"\` |
|        - |   958 | `	"public function getCode();"\` |
|        - |   959 | `	"public function getFile();"\` |
|        - |   960 | `	"public function getLine();"\` |
|        - |   961 | `	"public function getTrace();"\` |
|        - |   962 | `	"public function getTraceAsString();"\` |
|        - |   963 | `	"public function getPrevious();"\` |
|        - |   964 | `	"public function __toString();"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"class Exception implements Throwable { "\` |
|        - |   967 | `    "protected $message = '';"\` |
|        - |   968 | `    "protected $code = 0;"\` |
|        - |   969 | `    "protected $file;"\` |
|        - |   970 | `    "protected $line;"\` |
|        - |   971 | `    "protected $trace;"\` |
|        - |   972 | `    "protected $previous;"\` |
|        - |   973 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |   974 | `	"   if( isset($message) ){"\` |
|        - |   975 | `	"	  $this->message = $message;"\` |
|        - |   976 | `	"   }"\` |
|        - |   977 | `	"   $this->code = $code;"\` |
|        - |   978 | `	"   $this->file = __FILE__;"\` |
|        - |   979 | `	"   $this->line = __LINE__;"\` |
|        - |   980 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   981 | `	"   if( isset($previous) ){"\` |
|        - |   982 | `	"     $this->previous = $previous;"\` |
|        - |   983 | `	"   }"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"public function getMessage(){"\` |
|        - |   986 | `	"   return $this->message;"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	" public function getCode(){"\` |
|        - |   989 | `	"  return $this->code;"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"public function getFile(){"\` |
|        - |   992 | `	"  return $this->file;"\` |
|        - |   993 | `	"}"\` |
|        - |   994 | `	"public function getLine(){"\` |
|        - |   995 | `	"  return $this->line;"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"public function getTrace(){"\` |
|        - |   998 | `	"   return $this->trace;"\` |
|        - |   999 | `	"}"\` |
|        - |  1000 | `	"public function getTraceAsString(){"\` |
|        - |  1001 | `	"  return debug_string_backtrace();"\` |
|        - |  1002 | `	"}"\` |
|        - |  1003 | `	"public function getPrevious(){"\` |
|        - |  1004 | `	"    return $this->previous;"\` |
|        - |  1005 | `	"}"\` |
|        - |  1006 | `	"public function __toString(){"\` |
|        - |  1007 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1008 | `    "}"\` |
|        - |  1009 | `	"}"\` |
|        - |  1010 | `	"class Error implements Throwable { "\` |
|        - |  1011 | `    "protected $message = '';"\` |
|        - |  1012 | `    "protected $code = 0;"\` |
|        - |  1013 | `    "protected $file;"\` |
|        - |  1014 | `    "protected $line;"\` |
|        - |  1015 | `    "protected $trace;"\` |
|        - |  1016 | `    "protected $previous;"\` |
|        - |  1017 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1018 | `	"   if( isset($message) ){"\` |
|        - |  1019 | `	"	  $this->message = $message;"\` |
|        - |  1020 | `	"   }"\` |
|        - |  1021 | `	"   $this->code = $code;"\` |
|        - |  1022 | `	"   $this->file = __FILE__;"\` |
|        - |  1023 | `	"   $this->line = __LINE__;"\` |
|        - |  1024 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1025 | `	"   if( isset($previous) ){"\` |
|        - |  1026 | `	"     $this->previous = $previous;"\` |
|        - |  1027 | `	"   }"\` |
|        - |  1028 | `	"}"\` |
|        - |  1029 | `	"public function getMessage(){"\` |
|        - |  1030 | `	"   return $this->message;"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"public function getCode(){"\` |
|        - |  1033 | `	"  return $this->code;"\` |
|        - |  1034 | `	"}"\` |
|        - |  1035 | `	"public function getFile(){"\` |
|        - |  1036 | `	"  return $this->file;"\` |
|        - |  1037 | `	"}"\` |
|        - |  1038 | `	"public function getLine(){"\` |
|        - |  1039 | `	"  return $this->line;"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"public function getTrace(){"\` |
|        - |  1042 | `	"   return $this->trace;"\` |
|        - |  1043 | `	"}"\` |
|        - |  1044 | `	"public function getTraceAsString(){"\` |
|        - |  1045 | `	"  return debug_string_backtrace();"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"public function getPrevious(){"\` |
|        - |  1048 | `	"    return $this->previous;"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"public function __toString(){"\` |
|        - |  1051 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1052 | `	"}"\` |
|        - |  1053 | `	"}"\` |
|        - |  1054 | `	"class TypeError extends Error { }"\` |
|        - |  1055 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1056 | `	"class ValueError extends Error { }"\` |
|        - |  1057 | `	"class FiberError extends Error { }"\` |
|        - |  1058 | `	"class AssertionError extends Error { }"\` |
|        - |  1059 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1060 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1061 | `	"class ErrorException extends Exception { "\` |
|        - |  1062 | `	"protected $severity;"\` |
|        - |  1063 | `	"public function __construct(string $message = null,"\` |
|        - |  1064 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1065 | `	"   if( isset($message) ){"\` |
|        - |  1066 | `	"	  $this->message = $message;"\` |
|        - |  1067 | `	"   }"\` |
|        - |  1068 | `	"   $this->severity = $severity;"\` |
|        - |  1069 | `	"   $this->code = $code;"\` |
|        - |  1070 | `	"   $this->file = $filename;"\` |
|        - |  1071 | `	"   $this->line = $lineno;"\` |
|        - |  1072 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1073 | `	"   if( isset($previous) ){"\` |
|        - |  1074 | `	"     $this->previous = $previous;"\` |
|        - |  1075 | `	"   }"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"public function getSeverity(){"\` |
|        - |  1078 | `	"   return $this->severity;"\` |
|        - |  1079 | `    "}"\` |
|        - |  1080 | `	"}"\` |
|        - |  1081 | `	"interface Iterator {"\` |
|        - |  1082 | `	"public function current();"\` |
|        - |  1083 | `	"public function key();"\` |
|        - |  1084 | `	"public function next();"\` |
|        - |  1085 | `	"public function rewind();"\` |
|        - |  1086 | `	"public function valid();"\` |
|        - |  1087 | `	"}"\` |
|        - |  1088 | `	"interface IteratorAggregate {"\` |
|        - |  1089 | `	"public function getIterator();"\` |
|        - |  1090 | `	"}"\` |
|        - |  1091 | `	"interface Serializable {"\` |
|        - |  1092 | `	"public function serialize();"\` |
|        - |  1093 | `	"public function unserialize(string $serialized);"\` |
|        - |  1094 | `	"}"\` |
|        - |  1095 | `	"/* Directory releated IO */"\` |
|        - |  1096 | `	"class Directory {"\` |
|        - |  1097 | `	"public $handle = null;"\` |
|        - |  1098 | `	"public $path  = null;"\` |
|        - |  1099 | `	"public function __construct(string $path)"\` |
|        - |  1100 | `	"{"\` |
|        - |  1101 | `	"   $this->handle = opendir($path);"\` |
|        - |  1102 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1103 | `	"      $this->path = $path;"\` |
|        - |  1104 | `	"   }"\` |
|        - |  1105 | `	"}"\` |
|        - |  1106 | `	"public function __destruct()"\` |
|        - |  1107 | `	"{"\` |
|        - |  1108 | `	"  if( $this->handle != null ){"\` |
|        - |  1109 | `	"       closedir($this->handle);"\` |
|        - |  1110 | `	"  }"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"public function read()"\` |
|        - |  1113 | `	"{"\` |
|        - |  1114 | `	"    return readdir($this->handle);"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"public function rewind()"\` |
|        - |  1117 | `	"{"\` |
|        - |  1118 | `	"    rewinddir($this->handle);"\` |
|        - |  1119 | `	"}"\` |
|        - |  1120 | `	"public function close()"\` |
|        - |  1121 | `	"{"\` |
|        - |  1122 | `	"    closedir($this->handle);"\` |
|        - |  1123 | `	"    $this->handle = null;"\` |
|        - |  1124 | `	"}"\` |
|        - |  1125 | `	"}"\` |
|        - |  1126 | `	"class Fiber {"\` |
|        - |  1127 | `	"  private $__ctx;"\` |
|        - |  1128 | `	"  private $__callable;"\` |
|        - |  1129 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1130 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1131 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1132 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1133 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1134 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1135 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1136 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1137 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1138 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1139 | `	"}"\` |
|        - |  1140 | `	"class Generator implements Iterator {"\` |
|        - |  1141 | `	"  private $__ctx;"\` |
|        - |  1142 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1143 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1144 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1145 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1146 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1147 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1148 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1149 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1150 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1151 | `	"}"\` |
|        - |  1152 | `	"class stdClass{"\` |
|        - |  1153 | `	"  public $value;"\` |
|        - |  1154 | `	" /* Magic methods */"\` |
|        - |  1155 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1156 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1157 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1158 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1159 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"function dir(string $path){"\` |
|        - |  1162 | `	"   return new Directory($path);"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"function Dir(string $path){"\` |
|        - |  1165 | `	"   return new Directory($path);"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1168 | `    "{"\` |
|        - |  1169 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1170 | `	"  $aDir = array();"\` |
|        - |  1171 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1172 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1173 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1174 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1175 | `	"   }"\` |
|        - |  1176 | `	"  closedir($pHandle);"\` |
|        - |  1177 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1178 | `	"      rsort($aDir);"\` |
|        - |  1179 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1180 | `	"      sort($aDir);"\` |
|        - |  1181 | `	"  }"\` |
|        - |  1182 | `	"  return $aDir;"\` |
|        - |  1183 | `	"}"\` |
|        - |  1184 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1185 | `	"/* Open the target directory */"\` |
|        - |  1186 | `	"$zDir = dirname($pattern);"\` |
|        - |  1187 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1188 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1189 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1190 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1191 | `	"	return FALSE;"\` |
|        - |  1192 | `	"}"\` |
|        - |  1193 | `	"$pattern = basename($pattern);"\` |
|        - |  1194 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1195 | `	"/* Loop throw available entries */"\` |
|        - |  1196 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1197 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1198 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1199 | `	"	if( $rc ){"\` |
|        - |  1200 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1201 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1202 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1203 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1204 | `	"		  }"\` |
|        - |  1205 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1206 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1207 | `	"		 continue;"\` |
|        - |  1208 | `	"	   }"\` |
|        - |  1209 | `	"	   /* Add the entry */"\` |
|        - |  1210 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1211 | `	"	}"\` |
|        - |  1212 | `	" }"\` |
|        - |  1213 | `	"/* Close the handle */"\` |
|        - |  1214 | `	"closedir($pHandle);"\` |
|        - |  1215 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1216 | `	"  /* Sort the array */"\` |
|        - |  1217 | `	"  sort($pArray);"\` |
|        - |  1218 | `	"}"\` |
|        - |  1219 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1220 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1221 | `	"  $pArray[] = $pattern;"\` |
|        - |  1222 | `	"}"\` |
|        - |  1223 | `	"/* Return the created array */"\` |
|        - |  1224 | `	"return $pArray;"\` |
|        - |  1225 | `   "}"\` |
|        - |  1226 | `   "/* Creates a temporary file */"\` |
|        - |  1227 | `   "function tmpfile(){"\` |
|        - |  1228 | `   "  /* Extract the temp directory */"\` |
|        - |  1229 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1230 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1231 | `   "    /* Use the current dir */"\` |
|        - |  1232 | `   "    $zTempDir = '.';"\` |
|        - |  1233 | `   "  }"\` |
|        - |  1234 | `   "  /* Create the file */"\` |
|        - |  1235 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1236 | `   "  return $pHandle;"\` |
|        - |  1237 | `   "}"\` |
|        - |  1238 | `   "/* Creates a temporary filename */"\` |
|        - |  1239 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1240 | `   "{"\` |
|        - |  1241 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1242 | `   "}"\` |
|        - |  1243 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1244 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1245 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1246 | `   "/* Copy arguments */"\` |
|        - |  1247 | `   "$nArgs = func_num_args();"\` |
|        - |  1248 | `   "$pNew = array();"\` |
|        - |  1249 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1250 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1251 | `    "}"\` |
|        - |  1252 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1253 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1254 | `	"/* Erase */"\` |
|        - |  1255 | `	"array_erase($pArray);"\` |
|        - |  1256 | `	"/* Unshift */"\` |
|        - |  1257 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1258 | `	"return sizeof($pArray);"\` |
|        - |  1259 | `    "}"\` |
|        - |  1260 | `	"function array_merge_recursive(){"\` |
|        - |  1261 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1262 | `    "$arrays = func_get_args();"\` |
|        - |  1263 | `    "$narrays = count($arrays);"\` |
|        - |  1264 | `    "$ret = array();"\` |
|        - |  1265 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1266 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1267 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1268 | `	 " }"\` |
|        - |  1269 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1270 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1271 | `     "  if( $keyIsInt ) {"\` |
|        - |  1272 | `     "   $ret[] = $value;"\` |
|        - |  1273 | `     "  } else {"\` |
|        - |  1274 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1275 | `     "    $cur = $ret[$key];"\` |
|        - |  1276 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1277 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1278 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1279 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1280 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1281 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1282 | `     "    } else {"\` |
|        - |  1283 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1284 | `     "    }"\` |
|        - |  1285 | `     "   } else {"\` |
|        - |  1286 | `     "    $ret[$key] = $value;"\` |
|        - |  1287 | `     "   }"\` |
|        - |  1288 | `     "  }"\` |
|        - |  1289 | `     " }"\` |
|        - |  1290 | `	 " }"\` |
|        - |  1291 | `	 " return $ret;"\` |
|        - |  1292 | `    "}"\` |
|        - |  1293 | `	"function max(){"\` |
|        - |  1294 | `    "  $pArgs = func_get_args();"\` |
|        - |  1295 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1296 | `	"  return null;"\` |
|        - |  1297 | `    " }"\` |
|        - |  1298 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1299 | `    " $pArg = $pArgs[0];"\` |
|        - |  1300 | `	" if( !is_array($pArg) ){"\` |
|        - |  1301 | `	"   return $pArg; "\` |
|        - |  1302 | `	" }"\` |
|        - |  1303 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1304 | `	"   return null;"\` |
|        - |  1305 | `	" }"\` |
|        - |  1306 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1307 | `	" reset($pArg);"\` |
|        - |  1308 | `	" $max = current($pArg);"\` |
|        - |  1309 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1310 | `	"   if( $val > $max ){"\` |
|        - |  1311 | `	"     $max = $val;"\` |
|        - |  1312 | `    " }"\` |
|        - |  1313 | `	" }"\` |
|        - |  1314 | `	" return $max;"\` |
|        - |  1315 | `    " }"\` |
|        - |  1316 | `    " $max = $pArgs[0];"\` |
|        - |  1317 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1318 | `    " $val = $pArgs[$i];"\` |
|        - |  1319 | `	"if( $val > $max ){"\` |
|        - |  1320 | `	" $max = $val;"\` |
|        - |  1321 | `	"}"\` |
|        - |  1322 | `    " }"\` |
|        - |  1323 | `	" return $max;"\` |
|        - |  1324 | `    "}"\` |
|        - |  1325 | `	"function min(){"\` |
|        - |  1326 | `    "  $pArgs = func_get_args();"\` |
|        - |  1327 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1328 | `	"  return null;"\` |
|        - |  1329 | `    " }"\` |
|        - |  1330 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1331 | `    " $pArg = $pArgs[0];"\` |
|        - |  1332 | `	" if( !is_array($pArg) ){"\` |
|        - |  1333 | `	"   return $pArg; "\` |
|        - |  1334 | `	" }"\` |
|        - |  1335 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1336 | `	"   return null;"\` |
|        - |  1337 | `	" }"\` |
|        - |  1338 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1339 | `	" reset($pArg);"\` |
|        - |  1340 | `	" $min = current($pArg);"\` |
|        - |  1341 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1342 | `	"   if( $val < $min ){"\` |
|        - |  1343 | `	"     $min = $val;"\` |
|        - |  1344 | `    " }"\` |
|        - |  1345 | `	" }"\` |
|        - |  1346 | `	" return $min;"\` |
|        - |  1347 | `    " }"\` |
|        - |  1348 | `    " $min = $pArgs[0];"\` |
|        - |  1349 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1350 | `    " $val = $pArgs[$i];"\` |
|        - |  1351 | `	"if( $val < $min ){"\` |
|        - |  1352 | `	" $min = $val;"\` |
|        - |  1353 | `	" }"\` |
|        - |  1354 | `    " }"\` |
|        - |  1355 | `	" return $min;"\` |
|        - |  1356 | `	"}"\` |
|        - |  1357 | `	"function fileowner(string $file){"\` |
|        - |  1358 | `    " $a = stat($file);"\` |
|        - |  1359 | `	" if( !is_array($a) ){"\` |
|        - |  1360 | `	"	return false;"\` |
|        - |  1361 | `	" }"\` |
|        - |  1362 | `	" return $a['uid'];"\` |
|        - |  1363 | `    "}"\` |
|        - |  1364 | `    "function filegroup(string $file){"\` |
|        - |  1365 | `	" $a = stat($file);"\` |
|        - |  1366 | `	" if( !is_array($a) ){"\` |
|        - |  1367 | `	"	return false;"\` |
|        - |  1368 | `	" }"\` |
|        - |  1369 | `	" return $a['gid'];"\` |
|        - |  1370 | `    "}"\` |
|        - |  1371 | `	 "function fileinode(string $file){"\` |
|        - |  1372 | `	" $a = stat($file);"\` |
|        - |  1373 | `	" if( !is_array($a) ){"\` |
|        - |  1374 | `	"	return false;"\` |
|        - |  1375 | `	" }"\` |
|        - |  1376 | `	" return $a['ino'];"\` |
|        - |  1377 | `    "}"` |
|        - |  1378 |  |
|        - |  1379 | `/*` |
|        - |  1380 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1381 | ` * start compiling the target PHP program.` |
|        - |  1382 | ` */` |
|     2936 |  1383 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1384 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1385 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1386 | `	 )` |
|        2 |  1387 |  |
|        - |  1388 | `	SyString sBuiltin;` |
|        - |  1389 | `	ph7_value *pObj;` |
|        - |  1390 | `	sxi32 rc;` |
|        - |  1391 | `	/* Zero the structure */` |
|     2938 |  1392 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1393 | `	/* Initialize VM fields */` |
|     2938 |  1394 | `	pVm->pEngine = &(*pEngine);` |
|     2938 |  1395 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1396 | `	/* Instructions containers */` |
|     2938 |  1397 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2938 |  1398 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2938 |  1399 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1400 | `	/* Object containers */` |
|     2938 |  1401 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2938 |  1402 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1403 | `	/* Virtual machine internal containers */` |
|     2938 |  1404 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2938 |  1405 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2938 |  1406 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2938 |  1407 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2938 |  1408 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2938 |  1409 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2938 |  1410 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2938 |  1411 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2938 |  1412 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2938 |  1413 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2938 |  1414 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2938 |  1415 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2938 |  1416 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2938 |  1417 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2938 |  1418 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2938 |  1419 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2938 |  1420 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2938 |  1421 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2938 |  1422 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2938 |  1423 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2938 |  1424 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2938 |  1425 | `	pVm->pPendingException = 0;` |
|        - |  1426 | `	/* Configuration containers */` |
|     2938 |  1427 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1428 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1429 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2938 |  1430 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2938 |  1431 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2938 |  1432 | `	pVm->iResponseStatus = 200;` |
|     2938 |  1433 | `	pVm->bHeadersSent = 0;` |
|     2938 |  1434 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1435 | `	/* Error callbacks containers */` |
|     2938 |  1436 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2938 |  1437 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2938 |  1438 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2938 |  1439 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2938 |  1440 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1441 | `	/* Set a default recursion limit */` |
|        - |  1442 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2938 |  1443 | `	pVm->nMaxDepth = 32;` |
|        - |  1444 | `#else` |
|        - |  1445 | `	pVm->nMaxDepth = 16;` |
|        - |  1446 | `#endif` |
|        - |  1447 | `	/* Default assertion flags */` |
|     2938 |  1448 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1449 | `	/* JSON return status */` |
|     2938 |  1450 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1451 | `	/* PRNG context */` |
|     2938 |  1452 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1453 | `	/* Install the null constant */` |
|     2938 |  1454 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1455 | `	if( pObj == 0 ){` |
|      ! 0 |  1456 | `		rc = SXERR_MEM;` |
|      ! 0 |  1457 | `		goto Err;` |
|        - |  1458 | `	}` |
|     2938 |  1459 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1460 | `	/* Install the boolean TRUE constant */` |
|     2938 |  1461 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1462 | `	if( pObj == 0 ){` |
|      ! 0 |  1463 | `		rc = SXERR_MEM;` |
|      ! 0 |  1464 | `		goto Err;` |
|        - |  1465 | `	}` |
|     2938 |  1466 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1467 | `	/* Install the boolean FALSE constant */` |
|     2938 |  1468 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2938 |  1469 | `	if( pObj == 0 ){` |
|      ! 0 |  1470 | `		rc = SXERR_MEM;` |
|      ! 0 |  1471 | `		goto Err;` |
|        - |  1472 | `	}` |
|     2938 |  1473 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1474 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1475 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1476 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2938 |  1477 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2938 |  1478 | `	if( pObj == 0 ){` |
|      ! 0 |  1479 | `		rc = SXERR_MEM;` |
|      ! 0 |  1480 | `		goto Err;` |
|        - |  1481 | `	}` |
|     2938 |  1482 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1483 | `	/* Create the global frame */` |
|     2938 |  1484 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2938 |  1485 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1486 | `		goto Err;` |
|        - |  1487 | `	}` |
|        - |  1488 | `	/* Initialize the code generator */` |
|     2938 |  1489 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2938 |  1490 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1491 | `		goto Err;` |
|        - |  1492 | `	}` |
|        - |  1493 | `	/* VM correctly initialized,set the magic number */` |
|     2938 |  1494 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2938 |  1495 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1496 | `	/* Compile the built-in library */` |
|     2938 |  1497 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1498 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2938 |  1499 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1500 | `	/* Register Fiber internal C functions */` |
|     2938 |  1501 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2938 |  1502 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2938 |  1503 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2938 |  1504 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2938 |  1505 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2938 |  1506 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2938 |  1507 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2938 |  1508 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2938 |  1509 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2938 |  1510 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1511 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2938 |  1512 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2938 |  1513 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2938 |  1514 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2938 |  1515 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2938 |  1516 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2938 |  1517 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2938 |  1518 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2938 |  1519 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2938 |  1520 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2938 |  1521 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1522 | `	/* Reset the code generator */` |
|     2938 |  1523 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2938 |  1524 | `	return SXRET_OK;` |
|      ! 0 |  1525 | `Err:` |
|      ! 0 |  1526 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1527 | `	return rc;` |
|     1470 |  1528 |  |
|        - |  1529 | `/*` |
|        - |  1530 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1531 | ` * routine which store the output in an internal blob.` |
|        - |  1532 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1533 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1534 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1535 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1536 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1537 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1538 | ` * to finish executing and extracting the output.` |
|        - |  1539 | ` */` |
|       38 |  1540 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1541 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1542 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1543 | `	void *pUserData     /* User private data */` |
|        - |  1544 | `	)` |
|      ! 0 |  1545 |  |
|        - |  1546 | `	 sxi32 rc;` |
|        - |  1547 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1548 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1549 | `	 return rc;` |
|      ! 0 |  1550 |  |
|        - |  1551 | `/*` |
|        - |  1552 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1553 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1554 | ` */` |
|    17034 |  1555 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1556 |  |
|    17036 |  1557 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    17036 |  1558 | `	if( xCons != VmObConsumer ){` |
|     7198 |  1559 | `		pVm->nOutputLen += nLen;` |
|     7198 |  1560 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      922 |  1561 | `			pVm->bHeadersSent = 1;` |
|      460 |  1562 | `		}` |
|     3598 |  1563 | `	}` |
|    17036 |  1564 |  |
|        - |  1565 | `#define VM_STACK_GUARD 16` |
|        - |  1566 | `/*` |
|        - |  1567 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1568 | ` * our compiled PHP program.` |
|        - |  1569 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1570 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1571 | ` */` |
|    40214 |  1572 | `static ph7_value * VmNewOperandStack(` |
|        - |  1573 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1574 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1575 | `	)` |
|        2 |  1576 |  |
|        - |  1577 | `	ph7_value *pStack;` |
|        - |  1578 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1579 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1580 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1581 | `  ** on the maximum stack depth required.` |
|        - |  1582 | `  **` |
|        - |  1583 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1584 | `  */` |
|    40216 |  1585 | `	nInstr += VM_STACK_GUARD;` |
|    40216 |  1586 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    40216 |  1587 | `	if( pStack == 0 ){` |
|      ! 0 |  1588 | `		return 0;` |
|        - |  1589 | `	}` |
|        - |  1590 | `	/* Initialize the operand stack */` |
|  2826910 |  1591 | `	while( nInstr > 0 ){` |
|  2786696 |  1592 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2786696 |  1593 | `		--nInstr;` |
|        2 |  1594 | `	}` |
|        - |  1595 | `	/* Ready for bytecode execution */` |
|    40216 |  1596 | `	return pStack;` |
|    20109 |  1597 |  |
|        - |  1598 | `/* Forward declaration */` |
|        - |  1599 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1600 | `/*` |
|        - |  1601 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1602 | ` * This routine gets called by the PH7 engine after` |
|        - |  1603 | ` * successful compilation of the target PHP program.` |
|        - |  1604 | ` */` |
|     2622 |  1605 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1606 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1607 | `	)` |
|        2 |  1608 |  |
|        - |  1609 | `	SyHashEntry *pEntry;` |
|        - |  1610 | `	sxi32 rc;` |
|     2624 |  1611 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1612 | `		/* Initialize your VM first */` |
|      ! 0 |  1613 | `		return SXERR_CORRUPT;` |
|        - |  1614 | `	}` |
|        - |  1615 | `	/* Mark the VM ready for byte-code execution */` |
|     2624 |  1616 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1617 | `	/* Release the code generator now we have compiled our program */` |
|     2624 |  1618 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1619 | `	/* Emit the DONE instruction */` |
|     2624 |  1620 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2624 |  1621 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1622 | `		return SXERR_MEM;` |
|        - |  1623 | `	}` |
|        - |  1624 | `	/* Script return value */` |
|     2624 |  1625 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1626 | `	/* Allocate a new operand stack */` |
|     2624 |  1627 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2624 |  1628 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1629 | `		return SXERR_MEM;` |
|        - |  1630 | `	}` |
|        - |  1631 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1632 | `	 * private data. */` |
|     2624 |  1633 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2624 |  1634 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1635 | `	/* Allocate the reference table */` |
|     2624 |  1636 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2624 |  1637 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2624 |  1638 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1639 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1640 | `		return SXERR_MEM;` |
|        - |  1641 | `	}` |
|        - |  1642 | `	/* Zero the reference table */` |
|     2624 |  1643 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1644 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2624 |  1645 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2624 |  1646 | `	if( rc != SXRET_OK ){` |
|        - |  1647 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1648 | `		return rc;` |
|        - |  1649 | `	}` |
|        - |  1650 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2624 |  1651 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2624 |  1652 | `	if( rc != SXRET_OK ){` |
|        - |  1653 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1654 | `		return rc;` |
|        - |  1655 | `	}` |
|        - |  1656 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2624 |  1657 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1658 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2624 |  1659 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1660 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2624 |  1661 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1662 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1663 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2624 |  1664 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2624 |  1665 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1666 | `#endif` |
|        - |  1667 | `	/* Initialize and install static and constants class attributes */` |
|     2624 |  1668 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    50082 |  1669 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    47460 |  1670 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    47460 |  1671 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1672 | `			return rc;` |
|        - |  1673 | `		}` |
|        2 |  1674 | `	}` |
|        - |  1675 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2624 |  1676 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1677 | `	/* VM is ready for bytecode execution */` |
|     2624 |  1678 | `	return SXRET_OK;` |
|     1313 |  1679 |  |
|        - |  1680 | `/*` |
|        - |  1681 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1682 | ` */` |
|      ! 0 |  1683 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1684 |  |
|      ! 0 |  1685 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1686 | `		return SXERR_CORRUPT;` |
|        - |  1687 | `	}` |
|        - |  1688 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1689 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1690 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1691 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1692 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1693 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1694 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1695 | `	pVm->bHttpContext = 0;` |
|        - |  1696 | `	/* Set the ready flag */` |
|      ! 0 |  1697 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1698 | `	return SXRET_OK;` |
|      ! 0 |  1699 |  |
|        - |  1700 | `/*` |
|        - |  1701 | ` * Release a Virtual Machine.` |
|        - |  1702 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1703 | ` */` |
|     2614 |  1704 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1705 |  |
|        - |  1706 | `	/* Set the stale magic number */` |
|     2616 |  1707 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1708 | `	/* Release the private memory subsystem */` |
|     2616 |  1709 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2616 |  1710 | `	return SXRET_OK;` |
|        2 |  1711 |  |
|        - |  1712 | `/*` |
|        - |  1713 | ` * Initialize a foreign function call context.` |
|        - |  1714 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1715 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1716 | ` * functions.` |
|        - |  1717 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1718 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1719 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1720 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1721 | ` */` |
|   650332 |  1722 | `static sxi32 VmInitCallContext(` |
|        - |  1723 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1724 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1725 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1726 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1727 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1728 | `	)` |
|        2 |  1729 |  |
|   650334 |  1730 | `	pOut->pFunc = pFunc;` |
|   650334 |  1731 | `	pOut->pVm   = pVm;` |
|   650334 |  1732 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   650334 |  1733 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1734 | `	/* Assume a null return value */` |
|   650334 |  1735 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   650334 |  1736 | `	pOut->pRet = pRet;` |
|   650334 |  1737 | `	pOut->iFlags = iFlags;` |
|   650334 |  1738 | `	return SXRET_OK;` |
|        2 |  1739 |  |
|        - |  1740 | `/*` |
|        - |  1741 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1742 | ` * left behind.` |
|        - |  1743 | ` */` |
|   650332 |  1744 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1745 |  |
|        - |  1746 | `	sxu32 n;` |
|   650334 |  1747 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7922 |  1748 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    22942 |  1749 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    15022 |  1750 | `			if( apObj[n] == 0 ){` |
|        - |  1751 | `				/* Already released */` |
|      298 |  1752 | `				continue;` |
|        - |  1753 | `			}` |
|    14726 |  1754 | `			PH7_MemObjRelease(apObj[n]);` |
|    14726 |  1755 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7364 |  1756 | `		}` |
|     7922 |  1757 | `		SySetRelease(&pCtx->sVar);` |
|     3960 |  1758 | `	}` |
|   650334 |  1759 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1760 | `		ph7_aux_data *aAux;` |
|        - |  1761 | `		void *pChunk;` |
|        - |  1762 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1763 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1764 | `		 */` |
|        9 |  1765 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1766 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1767 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1768 | `			/* Release the chunk */` |
|       25 |  1769 | `			if( pChunk ){` |
|       25 |  1770 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1771 | `			}` |
|       13 |  1772 | `		}` |
|        9 |  1773 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1774 | `	}` |
|   650334 |  1775 |  |
|        - |  1776 | `/*` |
|        - |  1777 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1778 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1779 | ` */` |
|      296 |  1780 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1781 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1782 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1783 | `	)` |
|        2 |  1784 |  |
|      298 |  1785 | `	if( pValue == 0 ){` |
|        - |  1786 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1787 | `		return;` |
|        - |  1788 | `	}` |
|      298 |  1789 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1790 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1791 | `		sxu32 n;` |
|     1054 |  1792 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1793 | `			if( apObj[n] == pValue ){` |
|      298 |  1794 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1795 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1796 | `				/* Mark as released */` |
|      298 |  1797 | `				apObj[n] = 0;` |
|      298 |  1798 | `				break;` |
|        - |  1799 | `			}` |
|      380 |  1800 | `		}` |
|      148 |  1801 | `	}` |
|      150 |  1802 |  |
|        - |  1803 | `/*` |
|        - |  1804 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1805 | ` */` |
|  3733302 |  1806 | `static void VmPopOperand(` |
|        - |  1807 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1808 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1809 | `	)` |
|        2 |  1810 |  |
|  3733304 |  1811 | `	ph7_value *pTos = *ppTos;` |
|  7943830 |  1812 | `	while( nPop > 0 ){` |
|  4210528 |  1813 | `		PH7_MemObjRelease(pTos);` |
|  4210528 |  1814 | `		pTos--;` |
|  4210528 |  1815 | `		nPop--;` |
|        2 |  1816 | `	}` |
|        - |  1817 | `	/* Top of the stack */` |
|  3733304 |  1818 | `	*ppTos = pTos;` |
|  3733304 |  1819 |  |
|        - |  1820 | `/*` |
|        - |  1821 | ` * Reserve a memory object.` |
|        - |  1822 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1823 | ` */` |
|  3126130 |  1824 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1825 |  |
|  3126132 |  1826 | `	ph7_value *pObj = 0;` |
|        - |  1827 | `	VmSlot *pSlot;` |
|        - |  1828 | `	sxu32 nIdx;` |
|        - |  1829 | `	/* Check for a free slot */` |
|  3126132 |  1830 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3126132 |  1831 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3126132 |  1832 | `	if( pSlot ){` |
|   978156 |  1833 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   978156 |  1834 | `		nIdx = pSlot->nIdx;` |
|   489077 |  1835 | `	}` |
|  3126132 |  1836 | `	if( pObj == 0 ){` |
|        - |  1837 | `		/* Reserve a new memory object */` |
|  2147978 |  1838 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2147978 |  1839 | `		if( pObj == 0 ){` |
|      ! 0 |  1840 | `			return 0;` |
|        - |  1841 | `		}` |
|  1073988 |  1842 | `	}` |
|        - |  1843 | `	/* Set a null default value */` |
|  3126132 |  1844 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3126132 |  1845 | `	pObj->nIdx = nIdx;` |
|  3126132 |  1846 | `	return pObj;` |
|  1563067 |  1847 |  |
|        - |  1848 | `/*` |
|        - |  1849 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1850 | ` */` |
|    33688 |  1851 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1852 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1853 | `	const char *zKey,  /* Entry key */` |
|        - |  1854 | `	sxu32 nByte,       /* Key length */` |
|        - |  1855 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1856 | `	)` |
|        2 |  1857 |  |
|        - |  1858 | `	ph7_value sKey;` |
|        - |  1859 | `	sxi32 rc;` |
|    33690 |  1860 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    33690 |  1861 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1862 | `	/* Perform the insertion */` |
|    33690 |  1863 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    33690 |  1864 | `	PH7_MemObjRelease(&sKey);` |
|    33690 |  1865 | `	return rc;` |
|        2 |  1866 |  |
|        - |  1867 | `/*` |
|        - |  1868 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1869 | ` * Return a pointer to the variable value on success.` |
|        - |  1870 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1871 | ` */` |
|  3478628 |  1872 | `static ph7_value * VmExtractMemObj(` |
|        - |  1873 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1874 | `	const SyString *pName, /* Variable name */` |
|        - |  1875 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1876 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1877 | `	)` |
|        2 |  1878 |  |
|  3478630 |  1879 | `	int bNullify = FALSE;` |
|        - |  1880 | `	SyHashEntry *pEntry;` |
|        - |  1881 | `	VmFrame *pFrame;` |
|        - |  1882 | `	ph7_value *pObj;` |
|        - |  1883 | `	sxu32 nIdx;` |
|        - |  1884 | `	sxi32 rc;` |
|        - |  1885 | `	/* Point to the top active frame */` |
|  3478630 |  1886 | `	pFrame = pVm->pFrame;` |
|  3478630 |  1887 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1888 | `	/* Perform the lookup */` |
|  3478630 |  1889 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1890 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1891 | `		pName = &sAnnon;` |
|        - |  1892 | `		/* Always nullify the object */` |
|      ! 0 |  1893 | `		bNullify = TRUE;` |
|      ! 0 |  1894 | `		bDup = FALSE;` |
|      ! 0 |  1895 | `	}` |
|        - |  1896 | `	/* Check the superglobals table first */` |
|  3478630 |  1897 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3478630 |  1898 | `	if( pEntry == 0 ){` |
|        - |  1899 | `		/* Query the top active frame */` |
|  3478590 |  1900 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3478590 |  1901 | `		if( pEntry == 0 ){` |
|   104932 |  1902 | `			char *zName = (char *)pName->zString;` |
|        - |  1903 | `			VmSlot sLocal;` |
|   104932 |  1904 | `			if( !bCreate ){` |
|        - |  1905 | `				/* Do not create the variable,return NULL instead */` |
|      118 |  1906 | `				return 0;` |
|        - |  1907 | `			}` |
|        - |  1908 | `			/* No such variable,automatically create a new one and install` |
|        - |  1909 | `			 * it in the current frame.` |
|        - |  1910 | `			 */` |
|   104816 |  1911 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   104816 |  1912 | `			if( pObj == 0 ){` |
|      ! 0 |  1913 | `				return 0;` |
|        - |  1914 | `			}` |
|   104816 |  1915 | `			nIdx = pObj->nIdx;` |
|   104816 |  1916 | `			if( bDup ){` |
|        - |  1917 | `				/* Duplicate name */` |
|      172 |  1918 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      172 |  1919 | `				if( zName == 0 ){` |
|      ! 0 |  1920 | `					return 0;` |
|        - |  1921 | `				}` |
|       85 |  1922 | `			}` |
|        - |  1923 | `			/* Link to the top active VM frame */` |
|   104816 |  1924 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   104816 |  1925 | `			if( rc != SXRET_OK ){` |
|        - |  1926 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1927 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1928 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1929 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1930 | `				return 0;` |
|        - |  1931 | `			}` |
|   104816 |  1932 | `			if( pFrame->pParent != 0 ){` |
|        - |  1933 | `				/* Local variable */` |
|    97376 |  1934 | `				sLocal.nIdx = nIdx;` |
|    97376 |  1935 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    48689 |  1936 | `			}else{` |
|        - |  1937 | `				/* Register in the $GLOBALS array */` |
|     7442 |  1938 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1939 | `			}` |
|        - |  1940 | `			/* Install in the reference table */` |
|   104816 |  1941 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1942 | `			/* Save object index */` |
|   104816 |  1943 | `			pObj->nIdx = nIdx;` |
|    52409 |  1944 | `		}else{` |
|        - |  1945 | `			/* Extract variable contents */` |
|  3373660 |  1946 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3373660 |  1947 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3373660 |  1948 | `			if( bNullify && pObj ){` |
|      ! 0 |  1949 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1950 | `			}` |
|        - |  1951 | `		}` |
|  1739348 |  1952 | `	}else{` |
|        - |  1953 | `		/* Superglobal */` |
|       42 |  1954 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1955 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1956 | `	}` |
|  3478514 |  1957 | `	return pObj;` |
|  1739426 |  1958 |  |
|        - |  1959 | `/*` |
|        - |  1960 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1961 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1962 | ` */` |
|     2926 |  1963 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1964 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1965 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1966 | `	sxu32 nByte        /* zName length */` |
|        - |  1967 | `	)` |
|        2 |  1968 |  |
|        - |  1969 | `	SyHashEntry *pEntry;` |
|        - |  1970 | `	ph7_value *pValue;` |
|        - |  1971 | `	sxu32 nIdx;` |
|        - |  1972 | `	/* Query the superglobal table */` |
|     2928 |  1973 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2928 |  1974 | `	if( pEntry == 0 ){` |
|        - |  1975 | `		/* No such entry */` |
|      ! 0 |  1976 | `		return 0;` |
|        - |  1977 | `	}` |
|        - |  1978 | `	/* Extract the superglobal index in the global object pool */` |
|     2928 |  1979 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1980 | `	/* Extract the variable value  */` |
|     2928 |  1981 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2928 |  1982 | `	return pValue;` |
|     1465 |  1983 |  |
|        - |  1984 | `/*` |
|        - |  1985 | ` * Perform a raw hashmap insertion.` |
|        - |  1986 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1987 | ` */` |
|     2956 |  1988 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1989 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1990 | `	const char *zKey,   /* Entry key */` |
|        - |  1991 | `	int nKeylen,        /* zKey length*/` |
|        - |  1992 | `	const char *zData,  /* Entry data */` |
|        - |  1993 | `	int nLen            /* zData length */` |
|        - |  1994 | `	)` |
|        2 |  1995 |  |
|        - |  1996 | `	ph7_value sKey,sValue;` |
|        - |  1997 | `	sxi32 rc;` |
|     2958 |  1998 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2958 |  1999 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2958 |  2000 | `	if( zKey ){` |
|     2936 |  2001 | `		if( nKeylen < 0 ){` |
|     2884 |  2002 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1441 |  2003 | `		}` |
|     2936 |  2004 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1467 |  2005 | `	}` |
|     2958 |  2006 | `	if( zData ){` |
|     2958 |  2007 | `		if( nLen < 0 ){` |
|        - |  2008 | `			/* Compute length automatically */` |
|      144 |  2009 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2010 | `		}` |
|     2958 |  2011 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1478 |  2012 | `	}` |
|        - |  2013 | `	/* Perform the insertion */` |
|     2958 |  2014 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2958 |  2015 | `	PH7_MemObjRelease(&sKey);` |
|     2958 |  2016 | `	PH7_MemObjRelease(&sValue);` |
|     2958 |  2017 | `	return rc;` |
|        2 |  2018 |  |
|        - |  2019 | `/*` |
|        - |  2020 | ` * Configure a working virtual machine instance.` |
|        - |  2021 | ` *` |
|        - |  2022 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2023 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2024 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2025 | ` * The second argument to this function is an integer configuration option` |
|        - |  2026 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2027 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2028 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2029 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2030 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2031 | ` */` |
|    42282 |  2032 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2033 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2034 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2035 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2036 | `	)` |
|        2 |  2037 |  |
|    42284 |  2038 | `	sxi32 rc = SXRET_OK;` |
|    42284 |  2039 | `	switch(nOp){` |
|     1303 |  2040 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2608 |  2041 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2608 |  2042 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2043 | `		/* VM output consumer callback */` |
|        - |  2044 | `#ifdef UNTRUST` |
|        - |  2045 | `		if( xConsumer == 0 ){` |
|        - |  2046 | `			rc = SXERR_CORRUPT;` |
|        - |  2047 | `			break;` |
|        - |  2048 | `		}` |
|        - |  2049 | `#endif` |
|        - |  2050 | `		/* Install the output consumer */` |
|     2608 |  2051 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2608 |  2052 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2608 |  2053 | `		break;` |
|        - |  2054 | `							   }` |
|     1311 |  2055 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2056 | `		/* Import path */` |
|        - |  2057 | `		  const char *zPath;` |
|        - |  2058 | `		  SyString sPath;` |
|     2624 |  2059 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2060 | `#if defined(UNTRUST)` |
|        - |  2061 | `		  if( zPath == 0 ){` |
|        - |  2062 | `			  rc = SXERR_EMPTY;` |
|        - |  2063 | `			  break;` |
|        - |  2064 | `		  }` |
|        - |  2065 | `#endif` |
|     2624 |  2066 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2067 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2068 | `#ifdef __WINNT__` |
|        2 |  2069 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2070 | `#endif` |
|     5246 |  2071 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2072 | `		  /* Remove leading and trailing white spaces */` |
|     2624 |  2073 | `		  SyStringFullTrim(&sPath);` |
|     2624 |  2074 | `		  if( sPath.nByte > 0 ){` |
|        - |  2075 | `			  /* Store the path in the corresponding conatiner */` |
|     2624 |  2076 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1311 |  2077 | `		  }` |
|     2624 |  2078 | `		  break;` |
|        - |  2079 | `									 }` |
|     1311 |  2080 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2081 | `		/* Run-Time Error report */` |
|     2624 |  2082 | `		pVm->bErrReport = 1;` |
|     2624 |  2083 | `		break;` |
|      ! 0 |  2084 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2085 | `		/* Recursion depth */` |
|      ! 0 |  2086 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2087 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2088 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2089 | `		}` |
|      ! 0 |  2090 | `		break;` |
|        - |  2091 | `									   }` |
|      ! 0 |  2092 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2093 | `		/* VM output length in bytes */` |
|      ! 0 |  2094 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2095 | `#ifdef UNTRUST` |
|        - |  2096 | `		if( pOut == 0 ){` |
|        - |  2097 | `			rc = SXERR_CORRUPT;` |
|        - |  2098 | `			break;` |
|        - |  2099 | `		}` |
|        - |  2100 | `#endif` |
|      ! 0 |  2101 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2102 | `		break;` |
|        - |  2103 | `							   }` |
|        - |  2104 |  |
|    13110 |  2105 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2106 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2107 | `		/* Create a new superglobal/global variable */` |
|    26222 |  2108 | `		const char *zName = va_arg(ap,const char *);` |
|    26222 |  2109 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2110 | `		SyHashEntry *pEntry;` |
|        - |  2111 | `		ph7_value *pObj;` |
|        - |  2112 | `		sxu32 nByte;` |
|        - |  2113 | `		sxu32 nIdx;` |
|        - |  2114 | `#ifdef UNTRUST` |
|        - |  2115 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2116 | `			rc = SXERR_CORRUPT;` |
|        - |  2117 | `			break;` |
|        - |  2118 | `		}` |
|        - |  2119 | `#endif` |
|    26222 |  2120 | `		nByte = SyStrlen(zName);` |
|    26222 |  2121 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2122 | `			/* Check if the superglobal is already installed */` |
|    26222 |  2123 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13112 |  2124 | `		}else{` |
|        - |  2125 | `			/* Query the top active VM frame */` |
|      ! 0 |  2126 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2127 | `		}` |
|    26222 |  2128 | `		if( pEntry ){` |
|        - |  2129 | `			/* Variable already installed */` |
|      ! 0 |  2130 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2131 | `			/* Extract contents */` |
|      ! 0 |  2132 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2133 | `			if( pObj ){` |
|        - |  2134 | `				/* Overwrite old contents */` |
|      ! 0 |  2135 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2136 | `			}` |
|      ! 0 |  2137 | `		}else{` |
|        - |  2138 | `			/* Install a new variable */` |
|    26222 |  2139 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26222 |  2140 | `			if( pObj == 0 ){` |
|      ! 0 |  2141 | `				rc = SXERR_MEM;` |
|      ! 0 |  2142 | `				break;` |
|        - |  2143 | `			}` |
|    26222 |  2144 | `			nIdx = pObj->nIdx;` |
|        - |  2145 | `			/* Copy value */` |
|    26222 |  2146 | `			PH7_MemObjStore(pValue,pObj);` |
|    26222 |  2147 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2148 | `				/* Install the superglobal */` |
|    26222 |  2149 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13112 |  2150 | `			}else{` |
|        - |  2151 | `				/* Install in the current frame */` |
|      ! 0 |  2152 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2153 | `			}` |
|    26222 |  2154 | `			if( rc == SXRET_OK ){` |
|        - |  2155 | `				SyHashEntry *pRef;` |
|    26222 |  2156 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26222 |  2157 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13112 |  2158 | `				}else{` |
|      ! 0 |  2159 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2160 | `				}` |
|        - |  2161 | `				/* Install in the reference table */` |
|    26222 |  2162 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26222 |  2163 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2164 | `					/* Register in the $GLOBALS array */` |
|    26222 |  2165 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13110 |  2166 | `				}` |
|    13110 |  2167 | `			}` |
|        - |  2168 | `		}` |
|    26222 |  2169 | `		break;` |
|        - |  2170 | `									}` |
|     1441 |  2171 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2172 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2173 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2174 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2175 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2176 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2177 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2884 |  2178 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2884 |  2179 | `		const char *zValue = va_arg(ap,const char *);` |
|     2884 |  2180 | `		int nLen = va_arg(ap,int);` |
|        - |  2181 | `		ph7_hashmap *pMap;` |
|        - |  2182 | `		ph7_value *pValue;` |
|     2884 |  2183 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2184 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2185 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2883 |  2186 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2187 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2188 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2882 |  2189 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2190 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2191 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2882 |  2192 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2193 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2194 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2882 |  2195 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2196 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2197 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2882 |  2198 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2199 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2200 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2201 | `		}else{` |
|        - |  2202 | `			/* Extract the $_SERVER superglobal */` |
|     2882 |  2203 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2204 | `		}` |
|     2884 |  2205 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2206 | `			/* No such entry */` |
|      ! 0 |  2207 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2208 | `			break;` |
|        - |  2209 | `		}` |
|        - |  2210 | `		/* Point to the hashmap */` |
|     2884 |  2211 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2212 | `		/* Perform the insertion */` |
|     2884 |  2213 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2884 |  2214 | `		break;` |
|        - |  2215 | `								   }` |
|       11 |  2216 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2217 | `		/* Script arguments */` |
|       24 |  2218 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2219 | `		ph7_hashmap *pMap;` |
|        - |  2220 | `		ph7_value *pValue;` |
|        - |  2221 | `		sxu32 n;` |
|       24 |  2222 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2223 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2224 | `			break;` |
|        - |  2225 | `		}` |
|        - |  2226 | `		/* Extract the $argv array */` |
|       24 |  2227 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2228 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2229 | `			/* No such entry */` |
|      ! 0 |  2230 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2231 | `			break;` |
|        - |  2232 | `		}` |
|        - |  2233 | `		/* Point to the hashmap */` |
|       24 |  2234 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2235 | `		/* Perform the insertion */` |
|       24 |  2236 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2237 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2238 | `		if( rc == SXRET_OK ){` |
|       24 |  2239 | `			if( pMap->nEntry > 1 ){` |
|        - |  2240 | `				/* Append space separator first */` |
|       18 |  2241 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2242 | `			}` |
|       24 |  2243 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2244 | `		}` |
|       24 |  2245 | `		break;` |
|        - |  2246 | `								  }` |
|      ! 0 |  2247 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2248 | `		/* error_log() consumer */` |
|      ! 0 |  2249 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2250 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2251 | `		break;` |
|        - |  2252 | `										}` |
|      ! 0 |  2253 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2254 | `		/* Script return value */` |
|      ! 0 |  2255 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2256 | `#ifdef UNTRUST` |
|        - |  2257 | `		if( ppValue == 0 ){` |
|        - |  2258 | `			rc = SXERR_CORRUPT;` |
|        - |  2259 | `			break;` |
|        - |  2260 | `		}` |
|        - |  2261 | `#endif` |
|      ! 0 |  2262 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2263 | `		break;` |
|        - |  2264 | `								   }` |
|     2622 |  2265 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2266 | `		/* Register an IO stream device */` |
|     5246 |  2267 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2268 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7866 |  2269 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5246 |  2270 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2271 | `				/* Invalid stream */` |
|      ! 0 |  2272 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2273 | `				break;` |
|        - |  2274 | `		}` |
|     5246 |  2275 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2276 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2624 |  2277 | `			pVm->pDefStream = pStream;` |
|     1311 |  2278 | `		}` |
|        - |  2279 | `		/* Insert in the appropriate container */` |
|     5246 |  2280 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5246 |  2281 | `		break;` |
|        - |  2282 | `								  }` |
|        8 |  2283 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2284 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2285 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2286 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2287 | `#ifdef UNTRUST` |
|        - |  2288 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2289 | `			rc = SXERR_CORRUPT;` |
|        - |  2290 | `			break;` |
|        - |  2291 | `		}` |
|        - |  2292 | `#endif` |
|       16 |  2293 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2294 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2295 | `		break;` |
|        - |  2296 | `									   }` |
|        8 |  2297 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2298 | `		/* Raw HTTP request*/` |
|       16 |  2299 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2300 | `		int nByte = va_arg(ap,int);` |
|       16 |  2301 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2302 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2303 | `			break;` |
|        - |  2304 | `		}` |
|       16 |  2305 | `		if( nByte < 0 ){` |
|        - |  2306 | `			/* Compute length automatically */` |
|      ! 0 |  2307 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2308 | `		}` |
|        - |  2309 | `		/* Process the request */` |
|       16 |  2310 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2311 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2312 | `		if( rc == SXRET_OK ){` |
|       16 |  2313 | `			pVm->bHttpContext = 1;` |
|        8 |  2314 | `		}` |
|       16 |  2315 | `		break;` |
|        - |  2316 | `									}` |
|        8 |  2317 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2318 | `		/* Extract HTTP response status code */` |
|       16 |  2319 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2320 | `		if( pStatus ){` |
|       16 |  2321 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2322 | `		}` |
|       16 |  2323 | `		break;` |
|        - |  2324 | `										}` |
|        8 |  2325 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2326 | `		/* Iterate response headers via callback */` |
|        - |  2327 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2328 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2329 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2330 | `		if( xCallback ){` |
|       16 |  2331 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2332 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2333 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2334 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2335 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2336 | `							   pUserData);` |
|       12 |  2337 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2338 | `					break;` |
|        - |  2339 | `				}` |
|        6 |  2340 | `			}` |
|        8 |  2341 | `		}` |
|       16 |  2342 | `		break;` |
|        - |  2343 | `										 }` |
|      ! 0 |  2344 | `	default:` |
|        - |  2345 | `		/* Unknown configuration option */` |
|      ! 0 |  2346 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2347 | `		break;` |
|        - |  2348 | `	}` |
|    42284 |  2349 | `	return rc;` |
|        2 |  2350 |  |
|        - |  2351 | `/* Forward declaration */` |
|        - |  2352 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2353 | `/*` |
|        - |  2354 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2355 | ` * format.` |
|        - |  2356 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2357 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2358 | ` * (STDOUT).` |
|        - |  2359 | ` */` |
|        2 |  2360 | `static sxi32 VmByteCodeDump(` |
|        - |  2361 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2362 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2363 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2364 | `	)` |
|        1 |  2365 |  |
|        - |  2366 | `	static const char zDump[] = {` |
|        - |  2367 | `		"====================================================\n"` |
|        - |  2368 | `		"PH7 VM Dump\n"` |
|        - |  2369 | `		"====================================================\n"` |
|        - |  2370 | `	};` |
|        - |  2371 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2372 | `	sxi32 rc = SXRET_OK;` |
|        - |  2373 | `	sxu32 n;` |
|        - |  2374 | `	/* Point to the PH7 instructions */` |
|        3 |  2375 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2376 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2377 | `	n = 0;` |
|        3 |  2378 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2379 | `	/* Dump instructions */` |
|        7 |  2380 | `	for(;;){` |
|       15 |  2381 | `		if( pInstr >= pEnd ){` |
|        - |  2382 | `			/* No more instructions */` |
|        3 |  2383 | `			break;` |
|        - |  2384 | `		}` |
|        - |  2385 | `		/* Format and call the consumer callback */` |
|       19 |  2386 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2387 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2388 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2389 | `		if( rc != SXRET_OK ){` |
|        - |  2390 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2391 | `			return rc;` |
|        - |  2392 | `		}` |
|       13 |  2393 | `		++n;` |
|       13 |  2394 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2395 | `	}` |
|        3 |  2396 | `	return rc;` |
|        2 |  2397 |  |
|        - |  2398 | `/* Forward declaration */` |
|        - |  2399 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2400 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2401 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2402 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2403 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2404 | `/*` |
|        - |  2405 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2406 | ` * consumer callback.` |
|        - |  2407 | ` */` |
|      580 |  2408 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2409 |  |
|      581 |  2410 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      581 |  2411 | `	sxi32 rc = SXRET_OK;` |
|        - |  2412 | `	/* Append a new line */` |
|        - |  2413 | `#ifdef __WINNT__` |
|        1 |  2414 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2415 | `#else` |
|      580 |  2416 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2417 | `#endif` |
|        - |  2418 | `	/* Invoke the output consumer callback */` |
|      581 |  2419 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      581 |  2420 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      581 |  2421 | `	return rc;` |
|        1 |  2422 |  |
|        - |  2423 | `/*` |
|        - |  2424 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2425 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2426 | ` * information.` |
|        - |  2427 | ` */` |
|      136 |  2428 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2429 |  |
|      138 |  2430 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2431 | `		ph7_value apArg[4];` |
|        - |  2432 | `		ph7_value *apArgPtr[4];` |
|        - |  2433 | `		ph7_value sResult;` |
|        - |  2434 | `		SyString sErr;` |
|        - |  2435 | `		/* Prepare arguments */` |
|       64 |  2436 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2437 | `			/* use explicit message length to avoid reading past buffer */` |
|       64 |  2438 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       64 |  2439 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       64 |  2440 | `		if( pFile ){` |
|       64 |  2441 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       64 |  2442 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       33 |  2443 | `		}else{` |
|      ! 0 |  2444 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2445 | `		}` |
|       64 |  2446 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       64 |  2447 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2448 | `		/* Set up pointer array */` |
|       64 |  2449 | `		apArgPtr[0] = &apArg[0];` |
|       64 |  2450 | `		apArgPtr[1] = &apArg[1];` |
|       64 |  2451 | `		apArgPtr[2] = &apArg[2];` |
|       64 |  2452 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2453 | `		/* Call the handler */` |
|       64 |  2454 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2455 | `		/* Check return value */` |
|       64 |  2456 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2457 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2458 | `		}` |
|        - |  2459 | `		/* Release */` |
|       64 |  2460 | `		PH7_MemObjRelease(&apArg[0]);` |
|       64 |  2461 | `		PH7_MemObjRelease(&apArg[1]);` |
|       64 |  2462 | `		PH7_MemObjRelease(&apArg[2]);` |
|       64 |  2463 | `		PH7_MemObjRelease(&apArg[3]);` |
|       64 |  2464 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2465 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2466 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       64 |  2467 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2468 | `	}` |
|        - |  2469 | `	/* No handler, always call error handler */` |
|       75 |  2470 | `	return TRUE;` |
|       70 |  2471 |  |
|       98 |  2472 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2473 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2474 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2475 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2476 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2477 | `	)` |
|        2 |  2478 |  |
|      100 |  2479 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2480 | `	SyString *pFile;` |
|        - |  2481 | `	char *zErr;` |
|      100 |  2482 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2483 | `	if( !pVm->bErrReport ){` |
|        - |  2484 | `		/* Don't bother reporting errors */` |
|        3 |  2485 | `		return SXRET_OK;` |
|        - |  2486 | `	}` |
|        - |  2487 | `	/* Reset the working buffer */` |
|       98 |  2488 | `	SyBlobReset(pWorker);` |
|        - |  2489 | `	/* Peek the processed file if available */` |
|       98 |  2490 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2491 | `	if( pFile ){` |
|        - |  2492 | `		/* Append file name */` |
|       98 |  2493 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2494 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2495 | `	}` |
|        - |  2496 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2497 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2498 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2499 | `	 * E_DEPRECATED). */` |
|       98 |  2500 | `	zErr = "Error:  ";` |
|       98 |  2501 | `	switch(iErr){` |
|       19 |  2502 | `	case PH7_CTX_WARNING:` |
|       40 |  2503 | `		zErr = "Warning:  ";` |
|       40 |  2504 | `		break;` |
|        6 |  2505 | `	case PH7_CTX_NOTICE:` |
|       14 |  2506 | `		zErr = "Notice:  ";` |
|       12 |  2507 | `		break;` |
|       23 |  2508 | `	default:` |
|        - |  2509 | `		/* keep iErr unchanged */` |
|       46 |  2510 | `		break;` |
|        - |  2511 | `	}` |
|       98 |  2512 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2513 | `	if( pFuncName ){` |
|        - |  2514 | `		/* Append function name first */` |
|       23 |  2515 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2516 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2517 | `	}` |
|       98 |  2518 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2519 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2520 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2521 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2522 | `	}` |
|       98 |  2523 | `	return rc;` |
|       51 |  2524 |  |
|        - |  2525 | `/*` |
|        - |  2526 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2527 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2528 | ` * information.` |
|        - |  2529 | ` */` |
|       40 |  2530 | `static sxi32 VmThrowErrorAp(` |
|        - |  2531 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2532 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2533 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2534 | `	const char *zFormat, /* Format message */` |
|        - |  2535 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2536 | `	)` |
|        2 |  2537 |  |
|       42 |  2538 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2539 | `	SyBlob sMsg;` |
|        - |  2540 | `	SyString *pFile;` |
|        - |  2541 | `	char *zErr;` |
|       42 |  2542 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2543 | `	if( !pVm->bErrReport ){` |
|        - |  2544 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2545 | `		return SXRET_OK;` |
|        - |  2546 | `	}` |
|        - |  2547 | `	/* Reset the working buffer */` |
|       42 |  2548 | `	SyBlobReset(pWorker);` |
|        - |  2549 | `	/* Peek the processed file if available */` |
|       42 |  2550 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2551 | `	if( pFile ){` |
|        - |  2552 | `		/* Append file name */` |
|       42 |  2553 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2554 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2555 | `	}` |
|        - |  2556 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2557 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2558 | `	 * the correct errno value. */` |
|       42 |  2559 | `	zErr = "Error:  ";` |
|       42 |  2560 | `	switch(iErr){` |
|        4 |  2561 | `	case PH7_CTX_WARNING:` |
|        9 |  2562 | `		zErr = "Warning:  ";` |
|        9 |  2563 | `		break;` |
|        3 |  2564 | `	case PH7_CTX_NOTICE:` |
|        7 |  2565 | `		zErr = "Notice:  ";` |
|        6 |  2566 | `		break;` |
|       13 |  2567 | `	default:` |
|        - |  2568 | `		/* do not change iErr */` |
|       26 |  2569 | `		break;` |
|        - |  2570 | `	}` |
|       42 |  2571 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2572 | `	if( pFuncName ){` |
|        - |  2573 | `		/* Append function name first */` |
|       26 |  2574 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2575 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2576 | `	}` |
|        - |  2577 | `	/* Format the raw message */` |
|       42 |  2578 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2579 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2580 | `	/* Check if a user error handler is installed */` |
|       42 |  2581 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2582 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2583 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2584 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2585 | `	}` |
|       42 |  2586 | `	SyBlobRelease(&sMsg);` |
|       42 |  2587 | `	return rc;` |
|       22 |  2588 |  |
|        - |  2589 | `/*` |
|        - |  2590 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2591 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2592 | ` * possible.` |
|        - |  2593 | ` */` |
|       38 |  2594 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2595 |  |
|        - |  2596 | `	ph7_class *pClass;` |
|       39 |  2597 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2598 | `	ph7_class_instance *pThis;` |
|        - |  2599 | `	ph7_class_method *pCons;` |
|        - |  2600 | `	ph7_value sArg;` |
|        - |  2601 | `	ph7_value *apArg[1];` |
|        - |  2602 | `	SyBlob sMsg;` |
|        - |  2603 | `	SyString sMsgStr;` |
|        - |  2604 | `	VmFrame *pFrame;` |
|        - |  2605 | `	sxi32 rc;` |
|       39 |  2606 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2607 | `	if( pClass == 0 ){` |
|      ! 0 |  2608 | `		return PH7_ABORT;` |
|        - |  2609 | `	}` |
|       39 |  2610 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2611 | `	if( pThis == 0 ){` |
|      ! 0 |  2612 | `		return PH7_ABORT;` |
|        - |  2613 | `	}` |
|       39 |  2614 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2615 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2616 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2617 | `	{` |
|       39 |  2618 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2619 | `		if( pOwner ){` |
|       39 |  2620 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2621 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2622 | `		}else{` |
|      ! 0 |  2623 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2624 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2625 | `		}` |
|        - |  2626 | `	}` |
|       39 |  2627 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2628 | `	if( pCons ){` |
|       39 |  2629 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2630 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2631 | `		apArg[0] = &sArg;` |
|       39 |  2632 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2633 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2634 | `	}` |
|       39 |  2635 | `	SyBlobRelease(&sMsg);` |
|       39 |  2636 | `	pFrame = pVm->pFrame;` |
|       39 |  2637 | `	if( pFrame ){` |
|       39 |  2638 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2639 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2640 | `	}` |
|       39 |  2641 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2642 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2643 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2644 | `		return PH7_ABORT;` |
|        - |  2645 | `	}` |
|       39 |  2646 | `	return PH7_EXCEPTION;` |
|       20 |  2647 |  |
|        - |  2648 |  |
|        - |  2649 | `/*` |
|        - |  2650 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2651 | ` */` |
|        4 |  2652 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2653 |  |
|        - |  2654 | `	ph7_class *pErrClass;` |
|        - |  2655 | `	ph7_class_instance *pThis;` |
|        - |  2656 | `	ph7_class_method *pCons;` |
|        - |  2657 | `	ph7_value sArg;` |
|        - |  2658 | `	ph7_value *apArg[1];` |
|        - |  2659 | `	SyBlob sMsg;` |
|        - |  2660 | `	SyString sMsgStr;` |
|        - |  2661 | `	VmFrame *pFrame;` |
|        - |  2662 | `	sxi32 rc;` |
|        5 |  2663 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2664 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2665 | `		return PH7_ABORT;` |
|        - |  2666 | `	}` |
|        5 |  2667 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2668 | `	if( pThis == 0 ){` |
|      ! 0 |  2669 | `		return PH7_ABORT;` |
|        - |  2670 | `	}` |
|        5 |  2671 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2672 | `	{` |
|        5 |  2673 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2674 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2675 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2676 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2677 | `	}` |
|        5 |  2678 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2679 | `	if( pCons ){` |
|        5 |  2680 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2681 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2682 | `		apArg[0] = &sArg;` |
|        5 |  2683 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2684 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2685 | `	}` |
|        5 |  2686 | `	SyBlobRelease(&sMsg);` |
|        5 |  2687 | `	pFrame = pVm->pFrame;` |
|        5 |  2688 | `	if( pFrame ){` |
|        5 |  2689 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2690 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2691 | `	}` |
|        5 |  2692 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2693 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2694 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2695 | `		return PH7_ABORT;` |
|        - |  2696 | `	}` |
|        5 |  2697 | `	return PH7_EXCEPTION;` |
|        3 |  2698 |  |
|        - |  2699 |  |
|        - |  2700 | `/*` |
|        - |  2701 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2702 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2703 | ` * For class types, instanceof is verified.` |
|        - |  2704 | ` *` |
|        - |  2705 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2706 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2707 | ` */` |
|        - |  2708 | `/*` |
|        - |  2709 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2710 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2711 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2712 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2713 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2714 | ` */` |
|       20 |  2715 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2716 |  |
|        - |  2717 | `	const char *z, *zEnd, *zTail;` |
|        - |  2718 | `	sxu32 n;` |
|        - |  2719 | `	sxu8 bReal;` |
|        - |  2720 | `	sxi32 rc;` |
|       22 |  2721 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2722 | `		return 0;` |
|        - |  2723 | `	}` |
|       22 |  2724 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2725 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2726 | `	zEnd = z + n;` |
|       22 |  2727 | `	if( n == 0 ){` |
|      ! 0 |  2728 | `		return 0;` |
|        - |  2729 | `	}` |
|       22 |  2730 | `	zTail = 0;` |
|       22 |  2731 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2732 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2733 | `		return 0;` |
|        - |  2734 | `	}` |
|        - |  2735 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2736 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2737 | `		zTail++;` |
|      ! 0 |  2738 | `	}` |
|       16 |  2739 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2740 |  |
|        - |  2741 |  |
|        - |  2742 | `/*` |
|        - |  2743 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2744 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2745 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2746 | ` *   0 if it's not strictly numeric.` |
|        - |  2747 | ` */` |
|       16 |  2748 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2749 |  |
|        - |  2750 | `	const char *z, *zEnd, *zTail;` |
|        - |  2751 | `	sxu32 n;` |
|       18 |  2752 | `	sxu8 bReal = 0;` |
|        - |  2753 | `	sxi32 rc;` |
|       18 |  2754 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2755 | `		return 0;` |
|        - |  2756 | `	}` |
|       18 |  2757 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2758 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2759 | `	zEnd = z + n;` |
|       18 |  2760 | `	if( n == 0 ) return 0;` |
|       18 |  2761 | `	zTail = 0;` |
|       18 |  2762 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2763 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2764 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2765 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2766 | `	return bReal ? 2 : 1;` |
|       10 |  2767 |  |
|        - |  2768 |  |
|        - |  2769 | `/*` |
|        - |  2770 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2771 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2772 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2773 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2774 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2775 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2776 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2777 | ` * throw.` |
|        - |  2778 | ` *` |
|        - |  2779 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2780 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2781 | ` */` |
|       98 |  2782 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2783 |  |
|        - |  2784 | `	sxu32 i;` |
|        - |  2785 | `	ph7_type_alt *aAlts;` |
|        - |  2786 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2787 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2788 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2789 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2790 | `	}` |
|       88 |  2791 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2792 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2793 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2794 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2795 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2796 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2797 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2798 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2799 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2800 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2801 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2802 | `	}` |
|        - |  2803 | `	/* Object handling */` |
|       88 |  2804 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2805 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2806 | `		if( bHasClassAlt ){` |
|       14 |  2807 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2808 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2809 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2810 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2811 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2812 | `			}` |
|       26 |  2813 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2814 | `				ph7_class *pExpected;` |
|        - |  2815 | `				SyString *pCN;` |
|       22 |  2816 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2817 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2818 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2819 | `					pExpected = pSelfNow;` |
|       22 |  2820 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2821 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2822 | `				}else{` |
|       22 |  2823 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2824 | `				}` |
|       22 |  2825 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2826 | `					return SXRET_OK;` |
|        - |  2827 | `				}` |
|        8 |  2828 | `			}` |
|        2 |  2829 | `		}` |
|        9 |  2830 | `		return SXERR_INVALID;` |
|        - |  2831 | `	}` |
|        - |  2832 | `	/* Array handling */` |
|       72 |  2833 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2834 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2835 | `	}` |
|        - |  2836 | `	/* Scalar handling — exact match first */` |
|       66 |  2837 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2838 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2839 | `	}` |
|       42 |  2840 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2841 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2842 | `	}` |
|       38 |  2843 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2844 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2845 | `	}` |
|       18 |  2846 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2847 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2848 | `	}` |
|       18 |  2849 | `	if( bStrict ){` |
|        - |  2850 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2851 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2852 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2853 | `			return SXRET_OK;` |
|        - |  2854 | `		}` |
|      ! 0 |  2855 | `		return SXERR_INVALID;` |
|        - |  2856 | `	}` |
|        - |  2857 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2858 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2859 | `	 * to match PHP's union RFC. */` |
|        - |  2860 | `	{` |
|       18 |  2861 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2862 | `		if( bHasInt ){` |
|        - |  2863 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2864 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2865 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2866 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2867 | `				return SXRET_OK;` |
|        - |  2868 | `			}` |
|       18 |  2869 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2870 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2871 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2872 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2873 | `					return SXRET_OK;` |
|        - |  2874 | `				}` |
|      ! 0 |  2875 | `			}` |
|       18 |  2876 | `			if( kind == 1 ){` |
|        9 |  2877 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2878 | `				return SXRET_OK;` |
|        - |  2879 | `			}` |
|        4 |  2880 | `		}` |
|       10 |  2881 | `		if( bHasFloat ){` |
|       10 |  2882 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2883 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2884 | `				return SXRET_OK;` |
|        - |  2885 | `			}` |
|       10 |  2886 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2887 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2888 | `				return SXRET_OK;` |
|        - |  2889 | `			}` |
|        1 |  2890 | `		}` |
|        3 |  2891 | `		if( bHasString ){` |
|      ! 0 |  2892 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2893 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2894 | `				return SXRET_OK;` |
|        - |  2895 | `			}` |
|      ! 0 |  2896 | `		}` |
|        3 |  2897 | `		if( bHasBool ){` |
|      ! 0 |  2898 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2899 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2900 | `				return SXRET_OK;` |
|        - |  2901 | `			}` |
|      ! 0 |  2902 | `		}` |
|        - |  2903 | `	}` |
|        3 |  2904 | `	return SXERR_INVALID;` |
|       51 |  2905 |  |
|        - |  2906 |  |
|        - |  2907 | `/*` |
|        - |  2908 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  2909 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  2910 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  2911 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  2912 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  2913 | ` */` |
|       34 |  2914 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  2915 |  |
|       36 |  2916 | `	if( bStrict ){` |
|        - |  2917 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  2918 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  2919 | `			PH7_MemObjToReal(pVal);` |
|        3 |  2920 | `			return SXRET_OK;` |
|        - |  2921 | `		}` |
|        7 |  2922 | `		return SXERR_INVALID;` |
|        - |  2923 | `	}` |
|        - |  2924 | `	{` |
|       28 |  2925 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  2926 | `		if( xCast ) xCast(pVal);` |
|        - |  2927 | `	}` |
|       28 |  2928 | `	return SXRET_OK;` |
|       19 |  2929 |  |
|        - |  2930 |  |
|        - |  2931 | `/*` |
|        - |  2932 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  2933 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  2934 | ` *` |
|        - |  2935 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  2936 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  2937 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  2938 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  2939 | ` */` |
|        8 |  2940 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  2941 |  |
|        9 |  2942 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  2943 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  2944 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  2945 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  2946 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  2947 | `		}` |
|        9 |  2948 | `		zBuf[nCopy] = 0;` |
|        9 |  2949 | `		return zBuf;` |
|        - |  2950 | `	}` |
|      ! 0 |  2951 | `	switch( nType ){` |
|      ! 0 |  2952 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  2953 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  2954 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  2955 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  2956 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  2957 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  2958 | `		default:             return "scalar";` |
|        - |  2959 | `	}` |
|        5 |  2960 |  |
|        - |  2961 |  |
|        - |  2962 | `/*` |
|        - |  2963 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2964 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2965 | ` */` |
|       18 |  2966 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2967 |  |
|       19 |  2968 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  2969 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  2970 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  2971 | `	return zBuf;` |
|        1 |  2972 |  |
|        - |  2973 |  |
|    13016 |  2974 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2975 |  |
|        - |  2976 | `	SyHashEntry *pSlot;` |
|        - |  2977 | `	VmClassAttr *pVmAttr;` |
|        - |  2978 | `	ph7_class_attr *pAttr;` |
|    13018 |  2979 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    13018 |  2980 | `	if( pSlot == 0 ){` |
|    12822 |  2981 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2982 | `	}` |
|      198 |  2983 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      198 |  2984 | `	pAttr = pVmAttr->pAttr;` |
|      198 |  2985 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2986 | `		return SXRET_OK;` |
|        - |  2987 | `	}` |
|        - |  2988 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  2989 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  2990 | `	 * matching PHP's documented behavior. */` |
|      198 |  2991 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  2992 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  2993 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  2994 |  |
|       16 |  2995 | `		if( rc == SXRET_OK ){` |
|        9 |  2996 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  2997 | `			return SXRET_OK;` |
|        - |  2998 | `		}` |
|        7 |  2999 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3000 | `			char zBuf[128];` |
|        4 |  3001 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3002 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3003 | `		}` |
|        5 |  3004 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3005 | `	}` |
|        - |  3006 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      184 |  3007 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3008 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3009 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3010 | `			return SXRET_OK;` |
|        - |  3011 | `		}` |
|        3 |  3012 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3013 | `	}` |
|        - |  3014 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3015 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3016 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      172 |  3017 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3018 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3019 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3020 | `			return SXRET_OK;` |
|        - |  3021 | `		}` |
|        7 |  3022 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3023 | `	}` |
|      162 |  3024 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3025 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3026 | `		 * currently active on the self-stack. */` |
|       26 |  3027 | `		ph7_class *pExpected = 0;` |
|       26 |  3028 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3029 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3030 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3031 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3032 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3033 | `		}` |
|       26 |  3034 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3035 | `			pExpected = pSelfNow;` |
|       24 |  3036 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3037 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3038 | `		}else{` |
|       22 |  3039 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3040 | `		}` |
|       26 |  3041 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3042 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3043 | `		}` |
|       26 |  3044 | `		if( pExpected ){` |
|       22 |  3045 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3046 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3047 | `				char zBuf[128];` |
|        7 |  3048 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3049 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3050 | `			}` |
|        8 |  3051 | `		}` |
|       22 |  3052 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3053 | `		return SXRET_OK;` |
|        - |  3054 | `	}` |
|        - |  3055 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3056 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      138 |  3057 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3058 | `		char zBuf[128];` |
|       10 |  3059 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3060 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3061 | `	}` |
|      132 |  3062 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3063 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3064 | `		if( xCast ){` |
|        - |  3065 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3066 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3067 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3068 | `			}` |
|       24 |  3069 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3070 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3071 | `			}` |
|        - |  3072 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3073 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3074 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3075 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3076 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3077 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3078 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3079 | `			}` |
|       12 |  3080 | `			xCast(pValue);` |
|        5 |  3081 | `		}` |
|        5 |  3082 | `	}` |
|      118 |  3083 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      118 |  3084 | `	return SXRET_OK;` |
|     6510 |  3085 |  |
|        - |  3086 |  |
|        - |  3087 | `/*` |
|        - |  3088 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3089 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3090 | ` * information.` |
|        - |  3091 | ` * ------------------------------------` |
|        - |  3092 | ` * Simple boring wrapper function.` |
|        - |  3093 | ` * ------------------------------------` |
|        - |  3094 | ` */` |
|       16 |  3095 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3096 |  |
|        - |  3097 | `	va_list ap;` |
|        - |  3098 | `	sxi32 rc;` |
|       17 |  3099 | `	va_start(ap,zFormat);` |
|       17 |  3100 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3101 | `	va_end(ap);` |
|       17 |  3102 | `	return rc;` |
|        1 |  3103 |  |
|        - |  3104 | `/*` |
|        - |  3105 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3106 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3107 | ` */` |
|       34 |  3108 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3109 |  |
|        - |  3110 | `	ph7_class *pClass;` |
|        - |  3111 | `	ph7_class_instance *pThis;` |
|        - |  3112 | `	ph7_class_method *pCons;` |
|        - |  3113 | `	ph7_value sArg;` |
|        - |  3114 | `	ph7_value *apArg[1];` |
|        - |  3115 | `	SyBlob sMsg;` |
|        - |  3116 | `	SyString sMsgStr;` |
|        - |  3117 | `	VmFrame *pFrame;` |
|        - |  3118 | `	sxi32 rc;` |
|       35 |  3119 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3120 | `	if( pClass == 0 ){` |
|      ! 0 |  3121 | `		return PH7_ABORT;` |
|        - |  3122 | `	}` |
|       35 |  3123 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3124 | `	if( pThis == 0 ){` |
|      ! 0 |  3125 | `		return PH7_ABORT;` |
|        - |  3126 | `	}` |
|       35 |  3127 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3128 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3129 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3130 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3131 | `	if( pCons ){` |
|       35 |  3132 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3133 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3134 | `		apArg[0] = &sArg;` |
|       35 |  3135 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3136 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3137 | `	}` |
|       35 |  3138 | `	SyBlobRelease(&sMsg);` |
|       35 |  3139 | `	pFrame = pVm->pFrame;` |
|       35 |  3140 | `	if( pFrame ){` |
|       35 |  3141 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3142 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3143 | `	}` |
|       35 |  3144 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3145 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3146 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3147 | `		return PH7_ABORT;` |
|        - |  3148 | `	}` |
|       31 |  3149 | `	return PH7_EXCEPTION;` |
|       18 |  3150 |  |
|        - |  3151 | `/*` |
|        - |  3152 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3153 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3154 | ` */` |
|        6 |  3155 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3156 |  |
|        - |  3157 | `	ph7_class *pClass;` |
|        - |  3158 | `	ph7_class_instance *pThis;` |
|        - |  3159 | `	ph7_class_method *pCons;` |
|        - |  3160 | `	ph7_value sArg;` |
|        - |  3161 | `	ph7_value *apArg[1];` |
|        - |  3162 | `	SyBlob sMsg;` |
|        - |  3163 | `	SyString sMsgStr;` |
|        - |  3164 | `	VmFrame *pFrame;` |
|        - |  3165 | `	sxi32 rc;` |
|        7 |  3166 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3167 | `	if( pClass == 0 ){` |
|      ! 0 |  3168 | `		return PH7_ABORT;` |
|        - |  3169 | `	}` |
|        7 |  3170 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3171 | `	if( pThis == 0 ){` |
|      ! 0 |  3172 | `		return PH7_ABORT;` |
|        - |  3173 | `	}` |
|        7 |  3174 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3175 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3176 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3177 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3178 | `	if( pCons ){` |
|        7 |  3179 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3180 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3181 | `		apArg[0] = &sArg;` |
|        7 |  3182 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3183 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3184 | `	}` |
|        7 |  3185 | `	SyBlobRelease(&sMsg);` |
|        7 |  3186 | `	pFrame = pVm->pFrame;` |
|        7 |  3187 | `	if( pFrame ){` |
|        7 |  3188 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3189 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3190 | `	}` |
|        7 |  3191 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3192 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3193 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3194 | `		return PH7_ABORT;` |
|        - |  3195 | `	}` |
|      ! 0 |  3196 | `	return PH7_EXCEPTION;` |
|        4 |  3197 |  |
|        - |  3198 | `/*` |
|        - |  3199 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3200 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3201 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3202 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3203 | ` */` |
|        - |  3204 | `/*` |
|        - |  3205 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3206 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3207 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3208 | ` */` |
|       24 |  3209 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3210 |  |
|        - |  3211 | `	sxu32 nCopy;` |
|       26 |  3212 | `	if( nBuf == 0 ) return "";` |
|       26 |  3213 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3214 | `		zBuf[0] = 0;` |
|      ! 0 |  3215 | `		return zBuf;` |
|        - |  3216 | `	}` |
|       26 |  3217 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3218 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3219 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3220 | `	zBuf[nCopy] = 0;` |
|       26 |  3221 | `	return zBuf;` |
|       14 |  3222 |  |
|        - |  3223 |  |
|      152 |  3224 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3225 |  |
|      154 |  3226 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3227 | `	const char *zGiven;` |
|        - |  3228 | `	char zBuf[128];` |
|        - |  3229 | `	char zTypeBuf[128];` |
|        - |  3230 | `	/* Untyped function: no enforcement. */` |
|      154 |  3231 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3232 | `		return SXRET_OK;` |
|        - |  3233 | `	}` |
|        - |  3234 | `	/* void return type: the function must not produce a value. */` |
|      154 |  3235 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|       20 |  3236 | `		if( pValue == 0 ){` |
|       18 |  3237 | `			return SXRET_OK;` |
|        - |  3238 | `		}` |
|        - |  3239 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3240 | `		 * still counts as "returned a value" here. */` |
|        3 |  3241 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3242 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3243 | `	}` |
|        - |  3244 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3245 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      136 |  3246 | `	if( pValue == 0 ){` |
|      ! 0 |  3247 | `		const char *zExpected = "value";` |
|      ! 0 |  3248 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3249 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3250 | `		}` |
|      ! 0 |  3251 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3252 | `	}` |
|        - |  3253 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3254 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3255 | `	 * bNullable=0 here. */` |
|      136 |  3256 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3257 | `		sxi32 rcU;` |
|      ! 0 |  3258 | `		int bNullable = 0;` |
|      ! 0 |  3259 | `		const char *zExpected = "union";` |
|        - |  3260 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3261 | `		{` |
|        - |  3262 | `			sxu32 i;` |
|      ! 0 |  3263 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3264 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3265 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3266 | `			}` |
|        - |  3267 | `		}` |
|      ! 0 |  3268 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3269 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3270 | `			return SXRET_OK;` |
|        - |  3271 | `		}` |
|      ! 0 |  3272 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3273 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3274 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3275 | `			zGiven = "null";` |
|      ! 0 |  3276 | `		}else{` |
|      ! 0 |  3277 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3278 | `		}` |
|      ! 0 |  3279 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3280 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3281 | `		}` |
|      ! 0 |  3282 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3283 | `	}` |
|        - |  3284 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3285 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3286 | `	 * it into the TypeError message. */` |
|      136 |  3287 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3288 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3289 | `		const char *zExpected;` |
|        - |  3290 | `		ph7_class *pExpected;` |
|        6 |  3291 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3292 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3293 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3294 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3295 | `		}` |
|        6 |  3296 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3297 | `			pExpected = pSelfNow;` |
|        4 |  3298 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3299 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3300 | `		}else{` |
|        3 |  3301 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3302 | `		}` |
|        6 |  3303 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3304 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3305 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3306 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3307 | `		}` |
|        6 |  3308 | `		if( pExpected ){` |
|        6 |  3309 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3310 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3311 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3312 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3313 | `			}` |
|        2 |  3314 | `		}` |
|        6 |  3315 | `		return SXRET_OK;` |
|        - |  3316 | `	}` |
|        - |  3317 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3318 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3319 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3320 | `	 * via the type-text leading '?'. */` |
|      132 |  3321 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3322 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3323 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3324 | `			return SXRET_OK;` |
|        - |  3325 | `		}` |
|      ! 0 |  3326 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3327 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3328 | `			"null");` |
|        - |  3329 | `	}` |
|        - |  3330 | `	/* Exact match? Done. */` |
|      126 |  3331 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      120 |  3332 | `		return SXRET_OK;` |
|        - |  3333 | `	}` |
|        - |  3334 | `	/* Object->scalar is never compatible. */` |
|        8 |  3335 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3336 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3337 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3338 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3339 | `			zGiven);` |
|        - |  3340 | `	}` |
|        - |  3341 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3342 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3343 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3344 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3345 | `			ph7_type_name(pValue));` |
|        - |  3346 | `	}` |
|        - |  3347 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3348 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3349 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3350 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3351 | `	if( !bStrict` |
|        5 |  3352 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3353 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3354 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3355 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3356 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3357 | `			"string");` |
|        - |  3358 | `	}` |
|        6 |  3359 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3360 | `		return SXRET_OK;` |
|        - |  3361 | `	}` |
|        4 |  3362 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3363 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3364 | `		ph7_type_name(pValue));` |
|       78 |  3365 |  |
|        - |  3366 | `/*` |
|        - |  3367 | ` * Report a fatal named-argument error.` |
|        - |  3368 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3369 | ` */` |
|        6 |  3370 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3371 |  |
|        7 |  3372 | `	const char *zFunc = 0;` |
|        7 |  3373 | `	int nFunc = 0;` |
|        7 |  3374 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3375 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3376 |  |
|        - |  3377 | `/*` |
|        - |  3378 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3379 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3380 | ` * information.` |
|        - |  3381 | ` * ------------------------------------` |
|        - |  3382 | ` * Simple boring wrapper function.` |
|        - |  3383 | ` * ------------------------------------` |
|        - |  3384 | ` */` |
|       24 |  3385 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3386 |  |
|        - |  3387 | `	sxi32 rc;` |
|       26 |  3388 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3389 | `	return rc;` |
|        2 |  3390 |  |
|        - |  3391 | `/*` |
|        - |  3392 | ` * Resolve function context from the current frame.` |
|        - |  3393 | ` */` |
|      978 |  3394 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3395 |  |
|        - |  3396 | `	VmFrame *pFrame;` |
|        - |  3397 | `	ph7_vm_func *pFunc;` |
|      979 |  3398 | `	*pzFuncName = 0;` |
|      979 |  3399 | `	*pnFuncLen = 0;` |
|      979 |  3400 | `	pFrame = pVm->pFrame;` |
|      979 |  3401 | `	if( pFrame == 0 ){` |
|      ! 0 |  3402 | `		return;` |
|        - |  3403 | `	}` |
|      979 |  3404 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      979 |  3405 | `	if( pFrame->pParent == 0 ){` |
|      955 |  3406 | `		return;` |
|        - |  3407 | `	}` |
|       25 |  3408 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3409 | `	if( pFunc == 0 ){` |
|      ! 0 |  3410 | `		return;` |
|        - |  3411 | `	}` |
|       25 |  3412 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3413 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      490 |  3414 |  |
|        - |  3415 | `/*` |
|        - |  3416 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3417 | ` */` |
|      504 |  3418 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3419 |  |
|        - |  3420 | `	SyBlob sOut;` |
|        - |  3421 | `	SyString *pFile;` |
|      505 |  3422 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3423 | `		return PH7_OK;` |
|        - |  3424 | `	}` |
|      505 |  3425 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3426 | `		zClass = "Exception";` |
|      ! 0 |  3427 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3428 | `	}` |
|      505 |  3429 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      483 |  3430 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      241 |  3431 | `	}` |
|      505 |  3432 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      505 |  3433 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      505 |  3434 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      505 |  3435 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      505 |  3436 | `	if( zMsg && nMsg > 0 ){` |
|      505 |  3437 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      505 |  3438 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      252 |  3439 | `	}` |
|      505 |  3440 | `	if( pFile ){` |
|      505 |  3441 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      505 |  3442 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3443 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      252 |  3444 | `	}` |
|      505 |  3445 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      505 |  3446 | `	if( pFile ){` |
|      505 |  3447 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      505 |  3448 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3449 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3450 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3451 | `		}else{` |
|      481 |  3452 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3453 | `		}` |
|      252 |  3454 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3455 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3456 | `	}else{` |
|      ! 0 |  3457 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3458 | `	}` |
|      505 |  3459 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      505 |  3460 | `	if( pFile ){` |
|      505 |  3461 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      505 |  3462 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      505 |  3463 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      505 |  3464 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      252 |  3465 | `	}` |
|      505 |  3466 | `	VmCallErrorHandler(pVm,&sOut);` |
|      505 |  3467 | `	SyBlobRelease(&sOut);` |
|      505 |  3468 | `	return PH7_ABORT;` |
|      253 |  3469 |  |
|        - |  3470 | `/*` |
|        - |  3471 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3472 | ` */` |
|      482 |  3473 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3474 |  |
|        - |  3475 | `	ph7_vm *pVm;` |
|        - |  3476 | `	ph7_class *pClass;` |
|        - |  3477 | `	ph7_class_instance *pThis;` |
|        - |  3478 | `	ph7_class_method *pCons;` |
|        - |  3479 | `	ph7_value sArg;` |
|        - |  3480 | `	ph7_value *apArg[1];` |
|        - |  3481 | `	SyBlob sMsg;` |
|        - |  3482 | `	SyString sMsgStr;` |
|        - |  3483 | `	VmFrame *pFrame;` |
|        - |  3484 | `	va_list ap;` |
|        - |  3485 | `	sxi32 rc;` |
|        - |  3486 |  |
|      484 |  3487 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3488 | `		return PH7_ABORT;` |
|        - |  3489 | `	}` |
|      484 |  3490 | `	pVm = pCtx->pVm;` |
|      484 |  3491 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3492 | `		zClass = "Error";` |
|      ! 0 |  3493 | `	}` |
|      484 |  3494 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      484 |  3495 | `	if( pClass == 0 ){` |
|      ! 0 |  3496 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3497 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3498 | `			zClass` |
|        - |  3499 | `			);` |
|        - |  3500 | `	}` |
|      484 |  3501 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      484 |  3502 | `	if( pThis == 0 ){` |
|      ! 0 |  3503 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3504 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3505 | `			);` |
|        - |  3506 | `	}` |
|        - |  3507 |  |
|      484 |  3508 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      484 |  3509 | `	va_start(ap,zFormat);` |
|      484 |  3510 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      484 |  3511 | `	va_end(ap);` |
|        - |  3512 |  |
|      484 |  3513 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      484 |  3514 | `	if( pCons ){` |
|      484 |  3515 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      484 |  3516 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      484 |  3517 | `		apArg[0] = &sArg;` |
|      484 |  3518 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      484 |  3519 | `		PH7_MemObjRelease(&sArg);` |
|      241 |  3520 | `	}` |
|      484 |  3521 | `	SyBlobRelease(&sMsg);` |
|        - |  3522 |  |
|      484 |  3523 | `	pFrame = pVm->pFrame;` |
|      484 |  3524 | `	if( pFrame ){` |
|      484 |  3525 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      484 |  3526 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      241 |  3527 | `	}` |
|      484 |  3528 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      484 |  3529 | `	PH7_ClassInstanceUnref(pThis);` |
|      484 |  3530 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3531 | `		return PH7_ABORT;` |
|        - |  3532 | `	}` |
|       14 |  3533 | `	return PH7_EXCEPTION;` |
|      243 |  3534 |  |
|        - |  3535 | `/*` |
|        - |  3536 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3537 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3538 | ` */` |
|      ! 0 |  3539 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3540 |  |
|        - |  3541 | `	ph7_vm *pVm;` |
|        - |  3542 | `	SyBlob sMsg;` |
|      ! 0 |  3543 | `	const char *zFuncName = 0;` |
|      ! 0 |  3544 | `	int nFuncLen = 0;` |
|        - |  3545 | `	va_list ap;` |
|        - |  3546 | `	sxi32 rc;` |
|        - |  3547 |  |
|      ! 0 |  3548 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3549 | `		return PH7_OK;` |
|        - |  3550 | `	}` |
|      ! 0 |  3551 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3552 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3553 | `		zClass = "Error";` |
|      ! 0 |  3554 | `	}` |
|        - |  3555 |  |
|      ! 0 |  3556 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3557 |  |
|      ! 0 |  3558 | `	va_start(ap,zFormat);` |
|      ! 0 |  3559 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3560 | `	va_end(ap);` |
|        - |  3561 |  |
|      ! 0 |  3562 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3563 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3564 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3565 | `	}` |
|      ! 0 |  3566 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3567 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3568 | `	}` |
|      ! 0 |  3569 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3570 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3571 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3572 | `	return rc;` |
|      ! 0 |  3573 |  |
|        - |  3574 | `/*` |
|        - |  3575 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3576 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3577 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3578 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3579 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3580 | ` * when VmByteCodeExec returns.` |
|        - |  3581 | ` */` |
|      144 |  3582 | `static sxi32 VmSuspendCtx(` |
|        - |  3583 | `	ph7_vm *pVm,` |
|        - |  3584 | `	ph7_exec_ctx *pCtx,` |
|        - |  3585 | `	sxi32 pc,` |
|        - |  3586 | `	sxi32 nTos` |
|        - |  3587 | `	)` |
|        2 |  3588 |  |
|       72 |  3589 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3590 | `	pCtx->pc = pc;` |
|      146 |  3591 | `	pCtx->nTos = nTos;` |
|      146 |  3592 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3593 | `	return PH7_SUSPEND;` |
|        2 |  3594 |  |
|        - |  3595 | `/*` |
|        - |  3596 | ` * Resolve named-argument mapping.` |
|        - |  3597 | ` *` |
|        - |  3598 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3599 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3600 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3601 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3602 | ` * every formal parameter that received a value.` |
|        - |  3603 | ` *` |
|        - |  3604 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3605 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3606 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3607 | ` */` |
|       92 |  3608 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3609 | `	ph7_vm *pVm,` |
|        - |  3610 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3611 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3612 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3613 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3614 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3615 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3616 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3617 |  |
|        2 |  3618 |  |
|       94 |  3619 | `	sxi32 posIdx = 0;` |
|        - |  3620 | `	sxu32 i;` |
|        - |  3621 | `	char zErrMsg[256];` |
|       94 |  3622 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      278 |  3623 | `	for( i = 0; i < nActual; i++ ){` |
|      186 |  3624 | `		aSlot[i] = -2;` |
|       94 |  3625 | `	}` |
|      272 |  3626 | `	for( i = 0; i < nActual; i++ ){` |
|      269 |  3627 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3628 | `			/* Named argument — find formal by name */` |
|      174 |  3629 | `			int found = 0;` |
|        - |  3630 | `			sxu32 k;` |
|      288 |  3631 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      274 |  3632 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      265 |  3633 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      252 |  3634 | `						pMap->aNames[i].zString,` |
|      378 |  3635 | `						pMap->aNames[i].nByte) == 0 ){` |
|      162 |  3636 | `					if( aUsed[k] ){` |
|        7 |  3637 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3638 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3639 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3640 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3641 | `						return PH7_ABORT;` |
|        - |  3642 | `					}` |
|      158 |  3643 | `					aSlot[i] = (sxi32)k;` |
|      158 |  3644 | `					aUsed[k] = 1;` |
|      158 |  3645 | `					found = 1;` |
|      158 |  3646 | `					break;` |
|        - |  3647 | `				}` |
|       59 |  3648 | `			}` |
|      170 |  3649 | `			if( !found ){` |
|       14 |  3650 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3651 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3652 | `				}else{` |
|        4 |  3653 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3654 | `						"Unknown named parameter $%.*s",` |
|        2 |  3655 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3656 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3657 | `					return PH7_ABORT;` |
|        - |  3658 | `				}` |
|        5 |  3659 | `			}` |
|       85 |  3660 | `		}else{` |
|        - |  3661 | `			/* Positional argument */` |
|       14 |  3662 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       14 |  3663 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3664 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3665 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3666 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3667 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3668 | `					return PH7_ABORT;` |
|        - |  3669 | `				}` |
|       14 |  3670 | `				aSlot[i] = posIdx;` |
|       14 |  3671 | `				aUsed[posIdx] = 1;` |
|        6 |  3672 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3673 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3674 | `			}` |
|       14 |  3675 | `			posIdx++;` |
|        - |  3676 | `		}` |
|       91 |  3677 | `	}` |
|       87 |  3678 | `	return SXRET_OK;` |
|       48 |  3679 |  |
|        - |  3680 | `/*` |
|        - |  3681 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3682 | ` *` |
|        - |  3683 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3684 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3685 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3686 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3687 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3688 | ` * then the program execution is halted.` |
|        - |  3689 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3690 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3691 | ` * or to reset the VM to it's initial state.` |
|        - |  3692 | ` */` |
|    40312 |  3693 | `static sxi32 VmByteCodeExec(` |
|        - |  3694 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3695 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3696 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3697 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3698 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3699 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3700 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3701 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3702 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3703 | `	)` |
|        2 |  3704 |  |
|        - |  3705 | `	VmInstr *pInstr;` |
|        - |  3706 | `	ph7_value *pTos;` |
|        - |  3707 | `	SySet aArg;` |
|        - |  3708 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3709 | `	sxi32 pc;` |
|        - |  3710 | `	sxi32 rc;` |
|        - |  3711 | `	/* Argument container */` |
|    40314 |  3712 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    40314 |  3713 | `	if( nTos < 0 ){` |
|    37896 |  3714 | `		pTos = &pStack[-1];` |
|    18949 |  3715 | `	}else{` |
|     2420 |  3716 | `		pTos = &pStack[nTos];` |
|        - |  3717 | `	}` |
|    40314 |  3718 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    40314 |  3719 | `	pc = nPc;` |
|        - |  3720 | `/*` |
|        - |  3721 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3722 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3723 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3724 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3725 | ` */` |
|        - |  3726 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3727 | `	{ \` |
|        - |  3728 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3729 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3730 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3731 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3732 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3733 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3734 | `				break; \` |
|        - |  3735 | `			} \` |
|        - |  3736 | `			goto Exception; \` |
|        - |  3737 | `		} \` |
|        - |  3738 | `	}` |
|        - |  3739 | `	/* Execute as much as we can */` |
|  5584156 |  3740 | `	for(;;){` |
|        - |  3741 | `		/* Fetch the instruction to execute */` |
| 11167610 |  3742 | `		pInstr = &aInstr[pc];` |
| 11167610 |  3743 | `		rc = SXRET_OK;` |
|        - |  3744 | `/*` |
|        - |  3745 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3746 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3747 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3748 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3749 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3750 | ` */` |
| 11167610 |  3751 | `		switch(pInstr->iOp){` |
|        - |  3752 | `/*` |
|        - |  3753 | ` * DONE: P1 * *` |
|        - |  3754 | ` *` |
|        - |  3755 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3756 | ` * and return immediately.` |
|        - |  3757 | ` */` |
|    19821 |  3758 | `case PH7_OP_DONE:` |
|        - |  3759 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  3760 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  3761 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  3762 | `	 * callback trampolines, and the main script. */` |
|    39644 |  3763 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      154 |  3764 | `		ph7_value *pRetVal = 0;` |
|      154 |  3765 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      138 |  3766 | `			pRetVal = pTos;` |
|       68 |  3767 | `		}` |
|      154 |  3768 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      154 |  3769 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      148 |  3770 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  3771 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  3772 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3773 | `				pTos--;` |
|      ! 0 |  3774 | `			}` |
|      ! 0 |  3775 | `			goto Exception;` |
|        - |  3776 | `		}` |
|        - |  3777 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  3778 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  3779 | `		 * defensively we clear the pointer after a successful check). */` |
|      148 |  3780 | `		pEnforceRetFunc = 0;` |
|       73 |  3781 | `	}` |
|    39638 |  3782 | `	if( pInstr->iP1 ){` |
|        - |  3783 | `#ifdef UNTRUST` |
|        - |  3784 | `		if( pTos < pStack ){` |
|        - |  3785 | `			goto Abort;` |
|        - |  3786 | `		}` |
|        - |  3787 | `#endif` |
|    23866 |  3788 | `		if( pLastRef ){` |
|    15058 |  3789 | `			*pLastRef = pTos->nIdx;` |
|     7528 |  3790 | `		}` |
|    23866 |  3791 | `		if( pResult ){` |
|        - |  3792 | `			/* Execution result */` |
|    22622 |  3793 | `			PH7_MemObjStore(pTos,pResult);` |
|    11310 |  3794 | `		}` |
|    23866 |  3795 | `		VmPopOperand(&pTos,1);` |
|    27706 |  3796 | `	}else if( pLastRef ){` |
|        - |  3797 | `		/* Nothing referenced */` |
|     1538 |  3798 | `		*pLastRef = SXU32_HIGH;` |
|      768 |  3799 | `	}` |
|        - |  3800 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3801 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3802 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3803 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3804 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3805 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3806 | `	 * block can override it.` |
|        - |  3807 | `	 */` |
|    39640 |  3808 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3809 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3810 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3811 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3812 | `		pExc->pFrame = 0;` |
|        3 |  3813 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3814 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3815 | `			pExc->iFinallyDone = 1;` |
|        - |  3816 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3817 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3818 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3819 | `				goto Abort;` |
|        - |  3820 | `			}` |
|        1 |  3821 | `		}` |
|        1 |  3822 | `	}` |
|    39638 |  3823 | `	goto Done;` |
|        - |  3824 | `/*` |
|        - |  3825 | ` * HALT: P1 * *` |
|        - |  3826 | ` *` |
|        - |  3827 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3828 | ` * and abort immediately.` |
|        - |  3829 | ` */` |
|        4 |  3830 | `case PH7_OP_HALT:` |
|        9 |  3831 | `	if( pInstr->iP1 ){` |
|        - |  3832 | `#ifdef UNTRUST` |
|        - |  3833 | `		if( pTos < pStack ){` |
|        - |  3834 | `			goto Abort;` |
|        - |  3835 | `		}` |
|        - |  3836 | `#endif` |
|        9 |  3837 | `		if( pLastRef ){` |
|      ! 0 |  3838 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3839 | `		}` |
|        9 |  3840 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3841 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3842 | `				/* Output the exit message */` |
|        7 |  3843 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3844 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3845 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3846 | `			}` |
|        7 |  3847 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3848 | `			/* Record exit status */` |
|        5 |  3849 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3850 | `		}` |
|        9 |  3851 | `		VmPopOperand(&pTos,1);` |
|        4 |  3852 | `	}else if( pLastRef ){` |
|        - |  3853 | `		/* Nothing referenced */` |
|      ! 0 |  3854 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3855 | `	}` |
|        - |  3856 | `	/* Check if we're in an included file context */` |
|        9 |  3857 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3858 | `		/* Terminate the entire process */` |
|        9 |  3859 | `		exit(pVm->iExitStatus);` |
|        - |  3860 | `	}` |
|      ! 0 |  3861 | `	goto Abort;` |
|        - |  3862 | `/*` |
|        - |  3863 | ` * JMP: * P2 *` |
|        - |  3864 | ` *` |
|        - |  3865 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3866 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3867 | ` */` |
|   238427 |  3868 | `case PH7_OP_JMP:` |
|   476900 |  3869 | `	pc = pInstr->iP2 - 1;` |
|   476900 |  3870 | `	break;` |
|        - |  3871 | `/*` |
|        - |  3872 | ` * JZ: P1 P2 *` |
|        - |  3873 | ` *` |
|        - |  3874 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3875 | ` * entry in the stack if P1 is zero.` |
|        - |  3876 | ` */` |
|   564689 |  3877 | `case PH7_OP_JZ:` |
|        - |  3878 | `#ifdef UNTRUST` |
|        - |  3879 | `	if( pTos < pStack ){` |
|        - |  3880 | `		goto Abort;` |
|        - |  3881 | `	}` |
|        - |  3882 | `#endif` |
|        - |  3883 | `	/* Get a boolean value */` |
|  1129468 |  3884 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3885 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3886 | `	}` |
|  1129468 |  3887 | `	if( !pTos->x.iVal ){` |
|        - |  3888 | `		/* Take the jump */` |
|   578716 |  3889 | `		pc = pInstr->iP2 - 1;` |
|   289357 |  3890 | `	}` |
|  1129468 |  3891 | `	if( !pInstr->iP1 ){` |
|   898030 |  3892 | `		VmPopOperand(&pTos,1);` |
|   449036 |  3893 | `	}` |
|  1129468 |  3894 | `	break;` |
|        - |  3895 | `/*` |
|        - |  3896 | ` * JNZ: P1 P2 *` |
|        - |  3897 | ` *` |
|        - |  3898 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3899 | ` * entry in the stack if P1 is zero.` |
|        - |  3900 | ` */` |
|    59389 |  3901 | `case PH7_OP_JNZ:` |
|        - |  3902 | `#ifdef UNTRUST` |
|        - |  3903 | `	if( pTos < pStack ){` |
|        - |  3904 | `		goto Abort;` |
|        - |  3905 | `	}` |
|        - |  3906 | `#endif` |
|        - |  3907 | `	/* Get a boolean value */` |
|   118780 |  3908 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3909 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3910 | `	}` |
|   118780 |  3911 | `	if( pTos->x.iVal ){` |
|        - |  3912 | `		/* Take the jump */` |
|     5198 |  3913 | `		pc = pInstr->iP2 - 1;` |
|     2598 |  3914 | `	}` |
|   118780 |  3915 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3916 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3917 | `	}` |
|   118780 |  3918 | `	break;` |
|        - |  3919 | `/*` |
|        - |  3920 | ` * NOOP: * * *` |
|        - |  3921 | ` *` |
|        - |  3922 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3923 | ` * destination.` |
|        - |  3924 | ` */` |
|      ! 0 |  3925 | `case PH7_OP_NOOP:` |
|      ! 0 |  3926 | `	break;` |
|        - |  3927 | `/*` |
|        - |  3928 | ` * POP: P1 * *` |
|        - |  3929 | ` *` |
|        - |  3930 | ` * Pop P1 elements from the operand stack.` |
|        - |  3931 | ` */` |
|   437004 |  3932 | `case PH7_OP_POP: {` |
|   874054 |  3933 | `	sxi32 n = pInstr->iP1;` |
|   874054 |  3934 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3935 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3936 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3937 | `	}` |
|   874054 |  3938 | `	VmPopOperand(&pTos,n);` |
|   874054 |  3939 | `	break;` |
|        - |  3940 | `				 }` |
|        - |  3941 | `/*` |
|        - |  3942 | ` * DUP: * * *` |
|        - |  3943 | ` *` |
|        - |  3944 | ` * Duplicate the top of the stack.` |
|        - |  3945 | ` */` |
|       41 |  3946 | `case PH7_OP_DUP:` |
|        - |  3947 | `#ifdef UNTRUST` |
|        - |  3948 | `	if( pTos < pStack ){` |
|        - |  3949 | `		goto Abort;` |
|        - |  3950 | `	}` |
|        - |  3951 | `#endif` |
|       84 |  3952 | `	pTos++;` |
|       84 |  3953 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3954 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3955 | `	break;` |
|        - |  3956 | `/*` |
|        - |  3957 | ` * NSSWITCH: * * P3` |
|        - |  3958 | ` *` |
|        - |  3959 | ` * Switch the active namespace at runtime.` |
|        - |  3960 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3961 | ` */` |
|     7302 |  3962 | `case PH7_OP_NSSWITCH:` |
|    14606 |  3963 | `	SyBlobReset(&pVm->sNamespace);` |
|    14606 |  3964 | `	if( pInstr->p3 ){` |
|       98 |  3965 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  3966 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  3967 | `	}` |
|        - |  3968 | `	/* Clear namespace-scoped use-const imports */` |
|    14606 |  3969 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14606 |  3970 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14606 |  3971 | `	break;` |
|        - |  3972 | `/* OP_USECONST P1 * P3` |
|        - |  3973 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3974 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3975 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3976 | ` */` |
|        7 |  3977 | `case PH7_OP_USECONST: {` |
|       16 |  3978 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3979 | `	if( azPair ){` |
|       16 |  3980 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3981 | `	}` |
|       16 |  3982 | `	break;` |
|        - |  3983 | `				}` |
|        - |  3984 | `/*` |
|        - |  3985 | ` * CVT_INT: * * *` |
|        - |  3986 | ` *` |
|        - |  3987 | ` * Force the top of the stack to be an integer.` |
|        - |  3988 | ` */` |
|       77 |  3989 | `case PH7_OP_CVT_INT:` |
|        - |  3990 | `#ifdef UNTRUST` |
|        - |  3991 | `	if( pTos < pStack ){` |
|        - |  3992 | `		goto Abort;` |
|        - |  3993 | `	}` |
|        - |  3994 | `#endif` |
|      156 |  3995 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3996 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3997 | `	}` |
|        - |  3998 | `	/* Invalidate any prior representation */` |
|      156 |  3999 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  4000 | `	break;` |
|        - |  4001 | `/*` |
|        - |  4002 | ` * CVT_REAL: * * *` |
|        - |  4003 | ` *` |
|        - |  4004 | ` * Force the top of the stack to be a real.` |
|        - |  4005 | ` */` |
|        4 |  4006 | `case PH7_OP_CVT_REAL:` |
|        - |  4007 | `#ifdef UNTRUST` |
|        - |  4008 | `	if( pTos < pStack ){` |
|        - |  4009 | `		goto Abort;` |
|        - |  4010 | `	}` |
|        - |  4011 | `#endif` |
|        9 |  4012 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4013 | `		PH7_MemObjToReal(pTos);` |
|        2 |  4014 | `	}` |
|        - |  4015 | `	/* Invalidate any prior representation */` |
|        9 |  4016 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  4017 | `	break;` |
|        - |  4018 | `/*` |
|        - |  4019 | ` * CVT_STR: * * *` |
|        - |  4020 | ` *` |
|        - |  4021 | ` * Force the top of the stack to be a string.` |
|        - |  4022 | ` */` |
|      146 |  4023 | `case PH7_OP_CVT_STR:` |
|        - |  4024 | `#ifdef UNTRUST` |
|        - |  4025 | `	if( pTos < pStack ){` |
|        - |  4026 | `		goto Abort;` |
|        - |  4027 | `	}` |
|        - |  4028 | `#endif` |
|      294 |  4029 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  4030 | `		PH7_MemObjToString(pTos);` |
|      146 |  4031 | `	}` |
|      294 |  4032 | `	break;` |
|        - |  4033 | `/*` |
|        - |  4034 | ` * CVT_BOOL: * * *` |
|        - |  4035 | ` *` |
|        - |  4036 | ` * Force the top of the stack to be a boolean.` |
|        - |  4037 | ` */` |
|        5 |  4038 | `case PH7_OP_CVT_BOOL:` |
|        - |  4039 | `#ifdef UNTRUST` |
|        - |  4040 | `	if( pTos < pStack ){` |
|        - |  4041 | `		goto Abort;` |
|        - |  4042 | `	}` |
|        - |  4043 | `#endif` |
|       11 |  4044 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4045 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4046 | `	}` |
|       11 |  4047 | `	break;` |
|        - |  4048 | `/*` |
|        - |  4049 | ` * CVT_NULL: * * *` |
|        - |  4050 | ` *` |
|        - |  4051 | ` * Nullify the top of the stack.` |
|        - |  4052 | ` */` |
|        3 |  4053 | `case PH7_OP_CVT_NULL:` |
|        - |  4054 | `#ifdef UNTRUST` |
|        - |  4055 | `	if( pTos < pStack ){` |
|        - |  4056 | `		goto Abort;` |
|        - |  4057 | `	}` |
|        - |  4058 | `#endif` |
|        7 |  4059 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4060 | `	break;` |
|        - |  4061 | `/*` |
|        - |  4062 | ` * CVT_NUMC: * * *` |
|        - |  4063 | ` *` |
|        - |  4064 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4065 | ` */` |
|      ! 0 |  4066 | `case PH7_OP_CVT_NUMC:` |
|        - |  4067 | `#ifdef UNTRUST` |
|        - |  4068 | `	if( pTos < pStack ){` |
|        - |  4069 | `		goto Abort;` |
|        - |  4070 | `	}` |
|        - |  4071 | `#endif` |
|        - |  4072 | `	/* Force a numeric cast */` |
|      ! 0 |  4073 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4074 | `	break;` |
|        - |  4075 | `/*` |
|        - |  4076 | ` * CVT_ARRAY: * * *` |
|        - |  4077 | ` *` |
|        - |  4078 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4079 | ` */` |
|       10 |  4080 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4081 | `#ifdef UNTRUST` |
|        - |  4082 | `	if( pTos < pStack ){` |
|        - |  4083 | `		goto Abort;` |
|        - |  4084 | `	}` |
|        - |  4085 | `#endif` |
|        - |  4086 | `	/* Force a hashmap cast */` |
|       21 |  4087 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4088 | `	if( rc != SXRET_OK ){` |
|        - |  4089 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4090 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4091 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4092 | `	}` |
|       21 |  4093 | `	break;` |
|        - |  4094 | `/*` |
|        - |  4095 | ` * CVT_OBJ: * * *` |
|        - |  4096 | ` *` |
|        - |  4097 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4098 | ` */` |
|        8 |  4099 | `case PH7_OP_CVT_OBJ:` |
|        - |  4100 | `#ifdef UNTRUST` |
|        - |  4101 | `	if( pTos < pStack ){` |
|        - |  4102 | `		goto Abort;` |
|        - |  4103 | `	}` |
|        - |  4104 | `#endif` |
|       17 |  4105 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4106 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4107 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4108 | `	}` |
|       17 |  4109 | `	break;` |
|        - |  4110 | `/*` |
|        - |  4111 | ` * ERR_CTRL * * *` |
|        - |  4112 | ` *` |
|        - |  4113 | ` * Error control operator.` |
|        - |  4114 | ` */` |
|    14915 |  4115 | `case PH7_OP_ERR_CTRL:` |
|        - |  4116 | `	/*` |
|        - |  4117 | `	 * TICKET 1433-038:` |
|        - |  4118 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4119 | `	 * use the public API,to control error output.` |
|        - |  4120 | `	 */` |
|    29830 |  4121 | `	break;` |
|        - |  4122 | `/*` |
|        - |  4123 | ` * IS_A * * *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4126 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4127 | ` * holding a class name or an object).` |
|        - |  4128 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4129 | ` */` |
|       42 |  4130 | `case PH7_OP_IS_A:{` |
|       86 |  4131 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  4132 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4133 | `#ifdef UNTRUST` |
|        - |  4134 | `	if( pNos < pStack ){` |
|        - |  4135 | `		goto Abort;` |
|        - |  4136 | `	}` |
|        - |  4137 | `#endif` |
|       86 |  4138 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  4139 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  4140 | `		ph7_class *pClass = 0;` |
|        - |  4141 | `		/* Extract the target class */` |
|       84 |  4142 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4143 | `			/* Instance already loaded */` |
|      ! 0 |  4144 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  4145 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  4146 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  4147 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4148 | `			/* Handle self/static/parent keywords */` |
|       84 |  4149 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4150 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  4151 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4152 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  4153 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4154 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4155 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4156 | `					pClass = pSelf->pBase;` |
|        2 |  4157 | `				}` |
|        3 |  4158 | `			}else{` |
|       74 |  4159 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4160 | `			}` |
|       41 |  4161 | `		}` |
|       84 |  4162 | `		if( pClass ){` |
|        - |  4163 | `			/* Perform the query */` |
|       84 |  4164 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  4165 | `		}` |
|       41 |  4166 | `	}` |
|        - |  4167 | `	/* Push result */` |
|       86 |  4168 | `	VmPopOperand(&pTos,1);` |
|       86 |  4169 | `	PH7_MemObjRelease(pTos);` |
|       86 |  4170 | `	pTos->x.iVal = iRes;` |
|       86 |  4171 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  4172 | `	break;` |
|        - |  4173 | `				 }` |
|        - |  4174 |  |
|        - |  4175 | `/*` |
|        - |  4176 | ` * LOADC P1 P2 *` |
|        - |  4177 | ` *` |
|        - |  4178 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4179 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4180 | ` */` |
|   948440 |  4181 | `case PH7_OP_LOADC: {` |
|        - |  4182 | `	ph7_value *pObj;` |
|        - |  4183 | `	/* Reserve a room */` |
|  1896926 |  4184 | `	pTos++;` |
|  2836224 |  4185 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1896926 |  4186 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4187 | `			SyHashEntry *pEntry;` |
|        - |  4188 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4189 | `			{` |
|        - |  4190 | `				SyHashEntry *pConstImport;` |
|    27563 |  4191 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    18374 |  4192 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18376 |  4193 | `				if( pConstImport ){` |
|       11 |  4194 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4195 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4196 | `					if( pEntry ){` |
|       11 |  4197 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4198 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4199 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4200 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4201 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4202 | `						break;` |
|        - |  4203 | `					}` |
|        - |  4204 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4205 | `				}` |
|        - |  4206 | `			}` |
|        - |  4207 | `			/* Candidate for expansion via user defined callbacks */` |
|    18366 |  4208 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18366 |  4209 | `			if( pEntry ){` |
|    18362 |  4210 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4211 | `				/* Set a NULL default value */` |
|    18362 |  4212 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    18362 |  4213 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4214 | `				/* Invoke the callback and deal with the expanded value */` |
|    18362 |  4215 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4216 | `				/* Mark as constant */` |
|    18362 |  4217 | `				pTos->nIdx = SXU32_HIGH;` |
|    18362 |  4218 | `				break;` |
|        - |  4219 | `			}` |
|        - |  4220 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4221 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4222 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4223 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4224 | `			{` |
|        6 |  4225 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  4226 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4227 | `				sxu32 j;` |
|        6 |  4228 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  4229 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  4230 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  4231 | `				}` |
|        6 |  4232 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4233 | `					/* Try current_namespace\name */` |
|      ! 0 |  4234 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4235 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4236 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4237 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4238 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4239 | `					if( pEntry ){` |
|      ! 0 |  4240 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4241 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4242 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4243 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4244 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4245 | `						break;` |
|        - |  4246 | `					}` |
|        - |  4247 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4248 | `				}` |
|        6 |  4249 | `				if( isQualified ){` |
|        - |  4250 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4251 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4252 | `					SyBlob sErr;` |
|        3 |  4253 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4254 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4255 | `					if( pErrFile ){` |
|        3 |  4256 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4257 | `					}` |
|        3 |  4258 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4259 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4260 | `					SyBlobRelease(&sErr);` |
|        3 |  4261 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4262 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4263 | `					goto LoadC_Done;` |
|        - |  4264 | `				}` |
|        - |  4265 | `			}` |
|        1 |  4266 | `		}` |
|  1878554 |  4267 | `		PH7_MemObjLoad(pObj,pTos);` |
|   939300 |  4268 | `	}else{` |
|        - |  4269 | `		/* Set a NULL value */` |
|      ! 0 |  4270 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4271 | `	}` |
|   939255 |  4272 | `LoadC_Done:` |
|        - |  4273 | `	/* Mark as constant */` |
|  1878556 |  4274 | `	pTos->nIdx = SXU32_HIGH;` |
|  1878556 |  4275 | `	break;` |
|        - |  4276 | `				  }` |
|        - |  4277 | `/*` |
|        - |  4278 | ` * LOAD: P1 * P3` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4281 | ` * from the P3 operand.` |
|        - |  4282 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4283 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4284 | ` */` |
|  1503111 |  4285 | `case PH7_OP_LOAD:{` |
|        - |  4286 | `	ph7_value *pObj;` |
|        - |  4287 | `	SyString sName;` |
|  3006444 |  4288 | `	if( pInstr->p3 == 0 ){` |
|        - |  4289 | `		/* Take the variable name from the top of the stack */` |
|        - |  4290 | `#ifdef UNTRUST` |
|        - |  4291 | `		if( pTos < pStack ){` |
|        - |  4292 | `			goto Abort;` |
|        - |  4293 | `		}` |
|        - |  4294 | `#endif` |
|        - |  4295 | `		/* Force a string cast */` |
|       19 |  4296 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4297 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4298 | `		}` |
|       19 |  4299 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4300 | `	}else{` |
|  3006426 |  4301 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4302 | `		/* Reserve a room for the target object */` |
|  3006426 |  4303 | `		pTos++;` |
|        - |  4304 | `	}` |
|        - |  4305 | `	/* Extract the requested memory object */` |
|  3006444 |  4306 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3006444 |  4307 | `	if( pObj == 0 ){` |
|       28 |  4308 | `		if( pInstr->iP1 ){` |
|        - |  4309 | `			/* Variable not found,load NULL */` |
|       28 |  4310 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4311 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4312 | `			}else{` |
|       28 |  4313 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4314 | `			}` |
|       28 |  4315 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1503126 |  4316 | `			break;` |
|      ! 0 |  4317 | `		}else{` |
|        - |  4318 | `			/* Fatal error */` |
|      ! 0 |  4319 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4320 | `			goto Abort;` |
|        - |  4321 | `		}` |
|        - |  4322 | `	}` |
|        - |  4323 | `	/* Load variable contents */` |
|  3006418 |  4324 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3006418 |  4325 | `	pTos->nIdx = pObj->nIdx;` |
|  3006418 |  4326 | `	break;` |
|        - |  4327 | `				   }` |
|        - |  4328 | `/*` |
|        - |  4329 | ` * LOAD_MAP P1 * *` |
|        - |  4330 | ` *` |
|        - |  4331 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4332 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4333 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4334 | ` */` |
|    21108 |  4335 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4336 | `	ph7_hashmap *pMap;` |
|        - |  4337 | `	/* Allocate a new hashmap instance */` |
|    42218 |  4338 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    42218 |  4339 | `	if( pMap == 0 ){` |
|      ! 0 |  4340 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4341 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4342 | `		goto Abort;` |
|        - |  4343 | `	}` |
|    42218 |  4344 | `	if( pInstr->iP1 > 0 ){` |
|     2376 |  4345 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  4346 | `		/* Perform the insertion */` |
|     7288 |  4347 | `		while( pEntry < pTos ){` |
|     4914 |  4348 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4349 | `				/* Insertion by reference */` |
|      142 |  4350 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4351 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4352 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4353 | `					);` |
|       48 |  4354 | `			}else{` |
|        - |  4355 | `				/* Standard insertion */` |
|     7229 |  4356 | `				PH7_HashmapInsert(pMap,` |
|     4818 |  4357 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2409 |  4358 | `					&pEntry[1]` |
|        - |  4359 | `				);` |
|        - |  4360 | `			}` |
|        - |  4361 | `			/* Next pair on the stack */` |
|     4914 |  4362 | `			pEntry += 2;` |
|        2 |  4363 | `		}` |
|        - |  4364 | `		/* Pop P1 elements */` |
|     2376 |  4365 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1187 |  4366 | `	}` |
|        - |  4367 | `	/* Push the hashmap */` |
|    42218 |  4368 | `	pTos++;` |
|    42218 |  4369 | `	pTos->nIdx = SXU32_HIGH;` |
|    42218 |  4370 | `	pTos->x.pOther = pMap;` |
|    42218 |  4371 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    42218 |  4372 | `	break;` |
|        - |  4373 | `					  }` |
|        - |  4374 | `/*` |
|        - |  4375 | ` * LOAD_LIST: P1 * *` |
|        - |  4376 | ` *` |
|        - |  4377 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4378 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4379 | ` * Caveats:` |
|        - |  4380 | ` *  This implementation support only a single nesting level.` |
|        - |  4381 | ` */` |
|       48 |  4382 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4383 | `	ph7_value *pEntry;` |
|       98 |  4384 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4385 | `		/* Empty list,break immediately */` |
|      ! 0 |  4386 | `		break;` |
|        - |  4387 | `	}` |
|       98 |  4388 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4389 | `#ifdef UNTRUST` |
|        - |  4390 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4391 | `		goto Abort;` |
|        - |  4392 | `	}` |
|        - |  4393 | `#endif` |
|       98 |  4394 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4395 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4396 | `		ph7_hashmap_node *pNode;` |
|        - |  4397 | `		ph7_value sKey,*pObj;` |
|        - |  4398 | `		/* Start Copying */` |
|       91 |  4399 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4400 | `		while( pEntry <= pTos ){` |
|      193 |  4401 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4402 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4403 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4404 | `					if( rc == SXRET_OK ){` |
|        - |  4405 | `						/* Store node value */` |
|      165 |  4406 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4407 | `					}else{` |
|        - |  4408 | `						/* Undefined array key */` |
|        - |  4409 | `						char zMsg[128];` |
|      ! 0 |  4410 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4411 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4412 | `						PH7_MemObjRelease(pObj);` |
|        - |  4413 | `					}` |
|       82 |  4414 | `				}` |
|       82 |  4415 | `			}` |
|      193 |  4416 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4417 | `			pEntry++;` |
|        1 |  4418 | `		}` |
|       46 |  4419 | `	}else{` |
|        - |  4420 | `		/* Source is not an array */` |
|        - |  4421 | `		ph7_value *pObj;` |
|       18 |  4422 | `		while( pEntry <= pTos ){` |
|       12 |  4423 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4424 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4425 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4426 | `				}` |
|        5 |  4427 | `			}` |
|       12 |  4428 | `			pEntry++;` |
|        2 |  4429 | `		}` |
|        8 |  4430 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4431 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4432 | `			const char *zType = "unknown";` |
|        3 |  4433 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4434 | `			char zMsg[256];` |
|        3 |  4435 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4436 | `				zType = "string";` |
|        1 |  4437 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4438 | `				zType = "int";` |
|      ! 0 |  4439 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4440 | `				zType = "float";` |
|      ! 0 |  4441 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4442 | `				zType = "object";` |
|      ! 0 |  4443 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4444 | `				zType = "resource";` |
|      ! 0 |  4445 | `			}` |
|        3 |  4446 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4447 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4448 | `		}` |
|        - |  4449 | `	}` |
|       98 |  4450 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4451 | `	break;` |
|        - |  4452 | `					   }` |
|        - |  4453 | `/*` |
|        - |  4454 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4455 | ` *` |
|        - |  4456 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4457 | ` * from the stack.` |
|        - |  4458 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4459 | ` * instead.` |
|        - |  4460 | ` */` |
|   241675 |  4461 | `case PH7_OP_LOAD_IDX: {` |
|   483396 |  4462 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   483396 |  4463 | `	ph7_hashmap *pMap = 0;` |
|        - |  4464 | `	ph7_value *pIdx;` |
|   483396 |  4465 | `	pIdx = 0;` |
|   483396 |  4466 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4467 | `		if( !pInstr->iP2){` |
|        - |  4468 | `			/* No available index,load NULL */` |
|      ! 0 |  4469 | `			if( pTos >= pStack ){` |
|      ! 0 |  4470 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4471 | `			}else{` |
|        - |  4472 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4473 | `				pTos++;` |
|      ! 0 |  4474 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4475 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4476 | `			}` |
|        - |  4477 | `			/* Emit a notice */` |
|      ! 0 |  4478 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4479 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4480 | `			break;` |
|        - |  4481 | `		}` |
|      ! 0 |  4482 | `	}else{` |
|   483396 |  4483 | `		pIdx = pTos;` |
|   483396 |  4484 | `		pTos--;` |
|        - |  4485 | `	}` |
|   483396 |  4486 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4487 | `		/* String access */` |
|   377664 |  4488 | `		if( pIdx ){` |
|        - |  4489 | `			sxu32 nOfft;` |
|   377664 |  4490 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4491 | `				/* Force an int cast */` |
|      ! 0 |  4492 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4493 | `			}` |
|   377664 |  4494 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   377664 |  4495 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4496 | `				/* Invalid offset,load null */` |
|      ! 0 |  4497 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4498 | `			}else{` |
|   377664 |  4499 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   377664 |  4500 | `				int c = zData[nOfft];` |
|   377664 |  4501 | `				PH7_MemObjRelease(pTos);` |
|   377664 |  4502 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   377664 |  4503 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4504 | `			}` |
|   188855 |  4505 | `		}else{` |
|        - |  4506 | `			/* No available index,load NULL */` |
|      ! 0 |  4507 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4508 | `		}` |
|   377664 |  4509 | `		break;` |
|        - |  4510 | `	}` |
|   105734 |  4511 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4512 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4513 | `			ph7_value *pObj;` |
|        3 |  4514 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4515 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4516 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4517 | `			}` |
|        1 |  4518 | `		}` |
|        1 |  4519 | `	}` |
|   105734 |  4520 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   105734 |  4521 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   105734 |  4522 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4523 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4524 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4525 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4526 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4527 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4528 | `		}` |
|        - |  4529 | `		/* Point to the hashmap */` |
|   105734 |  4530 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   105734 |  4531 | `		if( pIdx ){` |
|        - |  4532 | `			/* Load the desired entry */` |
|   105734 |  4533 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    52866 |  4534 | `		}` |
|   105734 |  4535 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4536 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4537 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4538 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4539 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4540 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4541 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4542 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4543 | `			 * correct for the outermost write. */` |
|       19 |  4544 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4545 | `			if( !needWrite && pNode ){` |
|       13 |  4546 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4547 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4548 | `					needWrite = 1;` |
|        3 |  4549 | `				}` |
|        6 |  4550 | `			}` |
|       19 |  4551 | `			if( needWrite ){` |
|       13 |  4552 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4553 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4554 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4555 | `					 * into the new map's storage. */` |
|        7 |  4556 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4557 | `					if( pIdx ){` |
|        7 |  4558 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4559 | `					}` |
|        3 |  4560 | `				}` |
|        6 |  4561 | `			}` |
|        9 |  4562 | `		}` |
|   105734 |  4563 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4564 | `			/* Create a new empty entry */` |
|      273 |  4565 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4566 | `			if( rc == SXRET_OK ){` |
|        - |  4567 | `				/* Point to the last inserted entry */` |
|      273 |  4568 | `				pNode = pMap->pLast;` |
|      136 |  4569 | `			}` |
|      136 |  4570 | `		}` |
|    52866 |  4571 | `	}` |
|   105734 |  4572 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4573 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4574 | `		char zMsg[128];` |
|      ! 0 |  4575 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4576 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4577 | `		}` |
|      ! 0 |  4578 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4579 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4580 | `	}` |
|   105734 |  4581 | `	if( pIdx ){` |
|   105734 |  4582 | `		PH7_MemObjRelease(pIdx);` |
|    52866 |  4583 | `	}` |
|   105734 |  4584 | `	if( rc == SXRET_OK ){` |
|        - |  4585 | `		/* Load entry contents */` |
|    47146 |  4586 | `		if( pMap->iRef < 2 ){` |
|        - |  4587 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4588 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4589 | `			 */` |
|       24 |  4590 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4591 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4592 | `		}else{` |
|    47124 |  4593 | `			pTos->nIdx = pNode->nValIdx;` |
|    47124 |  4594 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    47124 |  4595 | `			PH7_HashmapUnref(pMap);` |
|        - |  4596 | `		}` |
|    23574 |  4597 | `	}else{` |
|        - |  4598 | `		/* No such entry,load NULL */` |
|    58590 |  4599 | `		PH7_MemObjRelease(pTos);` |
|    58590 |  4600 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4601 | `	}` |
|   105734 |  4602 | `	break;` |
|        - |  4603 | `					  }` |
|        - |  4604 | `/*` |
|        - |  4605 | ` * LOAD_CLOSURE * * P3` |
|        - |  4606 | ` *` |
|        - |  4607 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4608 | ` * name in the stack.` |
|        - |  4609 | ` */` |
|       45 |  4610 | `case PH7_OP_LOAD_CLOSURE:{` |
|       91 |  4611 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       91 |  4612 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4613 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4614 | `		ph7_vm_func *pClosure;` |
|        - |  4615 | `		char *zName;` |
|        - |  4616 | `		sxu32 mLen;` |
|        - |  4617 | `		sxu32 n;` |
|        - |  4618 | `		/* Create a new VM function */` |
|       91 |  4619 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4620 | `		/* Generate an unique closure name */` |
|       91 |  4621 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       91 |  4622 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4623 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4624 | `			goto Abort;` |
|        - |  4625 | `		}` |
|       91 |  4626 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       91 |  4627 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4628 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4629 | `		}` |
|        - |  4630 | `		/* Zero the stucture */` |
|       91 |  4631 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4632 | `		/* Perform a structure assignment on read-only items */` |
|       91 |  4633 | `		pClosure->aArgs = pFunc->aArgs;` |
|       91 |  4634 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       91 |  4635 | `		pClosure->aStatic = pFunc->aStatic;` |
|       91 |  4636 | `		pClosure->iFlags = pFunc->iFlags;` |
|       91 |  4637 | `		pClosure->pUserData = pFunc->pUserData;` |
|       91 |  4638 | `		pClosure->sSignature = pFunc->sSignature;` |
|       91 |  4639 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       91 |  4640 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       91 |  4641 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       91 |  4642 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       91 |  4643 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4644 | `		/* Register the closure */` |
|       91 |  4645 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4646 | `		/* Set up closure environment */` |
|       91 |  4647 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       91 |  4648 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      245 |  4649 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4650 | `			ph7_value *pValue;` |
|      155 |  4651 | `			pEnv = &aEnv[n];` |
|      155 |  4652 | `			sEnv.sName  = pEnv->sName;` |
|      155 |  4653 | `			sEnv.iFlags = pEnv->iFlags;` |
|      155 |  4654 | `			sEnv.nIdx = SXU32_HIGH;` |
|      155 |  4655 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      155 |  4656 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4657 | `				/* Pass by reference */` |
|      ! 0 |  4658 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4659 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4660 | `					);` |
|      ! 0 |  4661 | `			}` |
|        - |  4662 | `			/* Standard pass by value */` |
|      155 |  4663 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      155 |  4664 | `			if( pValue ){` |
|        - |  4665 | `				/* Copy imported value */` |
|       69 |  4666 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4667 | `			}` |
|        - |  4668 | `			/* Insert the imported variable */` |
|      155 |  4669 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       78 |  4670 | `		}` |
|        - |  4671 | `		/* Finally,load the closure name on the stack */` |
|       91 |  4672 | `		pTos++;` |
|       91 |  4673 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       45 |  4674 | `	}` |
|       91 |  4675 | `	break;` |
|        - |  4676 | `						 }` |
|        - |  4677 | `/*` |
|        - |  4678 | ` * STORE * P2 P3` |
|        - |  4679 | ` *` |
|        - |  4680 | ` * Perform a store (Assignment) operation.` |
|        - |  4681 | ` */` |
|   133375 |  4682 | `case PH7_OP_STORE: {` |
|        - |  4683 | `	ph7_value *pObj;` |
|        - |  4684 | `	SyString sName;` |
|        - |  4685 | `#ifdef UNTRUST` |
|        - |  4686 | `	if( pTos < pStack ){` |
|        - |  4687 | `		goto Abort;` |
|        - |  4688 | `	}` |
|        - |  4689 | `#endif` |
|   266752 |  4690 | `	if( pInstr->iP2 ){` |
|        - |  4691 | `		sxu32 nIdx;` |
|        - |  4692 | `		sxi32 rcT;` |
|        - |  4693 | `		/* Member store operation */` |
|     4292 |  4694 | `		nIdx = pTos->nIdx;` |
|     4292 |  4695 | `		VmPopOperand(&pTos,1);` |
|     4292 |  4696 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4697 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4698 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4699 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4700 | `		}else{` |
|        - |  4701 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4702 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4288 |  4703 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4288 |  4704 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4705 | `				goto Abort;` |
|        - |  4706 | `			}` |
|     4288 |  4707 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4708 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4709 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4710 | `				 * propagate out of the VM loop. */` |
|       37 |  4711 | `				VmPopOperand(&pTos,1);` |
|        - |  4712 | `				{` |
|       37 |  4713 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4714 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4715 | `						pc = pFrm2->iExceptionJump - 1;` |
|   133394 |  4716 | `						break;` |
|        - |  4717 | `					}` |
|        - |  4718 | `				}` |
|      ! 0 |  4719 | `				goto Exception;` |
|        - |  4720 | `			}` |
|        - |  4721 | `			/* Point to the desired memory object */` |
|     4252 |  4722 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4252 |  4723 | `			if( pObj ){` |
|        - |  4724 | `				/* Perform the store operation */` |
|     4252 |  4725 | `				PH7_MemObjStore(pTos,pObj);` |
|     2125 |  4726 | `			}` |
|        - |  4727 | `		}` |
|     4256 |  4728 | `		break;` |
|   262462 |  4729 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4730 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4731 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4732 | `			/* Force a string cast */` |
|      ! 0 |  4733 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4734 | `		}` |
|        7 |  4735 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4736 | `		pTos--;` |
|        - |  4737 | `#ifdef UNTRUST` |
|        - |  4738 | `		if( pTos < pStack  ){` |
|        - |  4739 | `			goto Abort;` |
|        - |  4740 | `		}` |
|        - |  4741 | `#endif` |
|        4 |  4742 | `	}else{` |
|   262456 |  4743 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4744 | `	}` |
|        - |  4745 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   262462 |  4746 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   262462 |  4747 | `	if( pObj == 0 ){` |
|      ! 0 |  4748 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4749 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4750 | `		goto Abort;` |
|        - |  4751 | `	}` |
|   262462 |  4752 | `	if( !pInstr->p3 ){` |
|        7 |  4753 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4754 | `	}` |
|        - |  4755 | `	/* Perform the store operation */` |
|   262462 |  4756 | `	PH7_MemObjStore(pTos,pObj);` |
|   262462 |  4757 | `	break;` |
|        - |  4758 | `				   }` |
|        - |  4759 | `/*` |
|        - |  4760 | ` * STORE_IDX:   P1 * P3` |
|        - |  4761 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4762 | ` *` |
|        - |  4763 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4764 | ` */` |
|    91473 |  4765 | `case PH7_OP_STORE_IDX:` |
|        - |  4766 | `case PH7_OP_STORE_IDX_REF: {` |
|   182948 |  4767 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4768 | `	ph7_value *pKey;` |
|        - |  4769 | `	sxu32 nIdx;` |
|   182948 |  4770 | `	if( pInstr->iP1 ){` |
|        - |  4771 | `		/* Key is next on stack */` |
|    61216 |  4772 | `		pKey = pTos;` |
|    61216 |  4773 | `		pTos--;` |
|    30609 |  4774 | `	}else{` |
|   121734 |  4775 | `		pKey = 0;` |
|        - |  4776 | `	}` |
|   182948 |  4777 | `	nIdx = pTos->nIdx;` |
|   182948 |  4778 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4779 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4780 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4781 | `		 * checking true sharing count, then re-add after separation. */` |
|   182896 |  4782 | `		if( nIdx != SXU32_HIGH ){` |
|   182896 |  4783 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   274343 |  4784 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   182896 |  4785 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4786 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4787 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4788 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4789 | `				 * refcounts if the backing array was already separated. */` |
|   182896 |  4790 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   182896 |  4791 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   182896 |  4792 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   182896 |  4793 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   182896 |  4794 | `					pTos->x.pOther = pMap;` |
|    91449 |  4795 | `				}else{` |
|        - |  4796 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4797 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4798 | `					pMap = pCur;` |
|        - |  4799 | `				}` |
|    91449 |  4800 | `			}else{` |
|      ! 0 |  4801 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4802 | `			}` |
|    91449 |  4803 | `		}else{` |
|      ! 0 |  4804 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4805 | `		}` |
|   182896 |  4806 | `		if( pMap->iRef < 2 ){` |
|        - |  4807 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4808 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4809 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4810 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4811 | `			pMap->iRef = 2;` |
|      ! 0 |  4812 | `		}` |
|    91449 |  4813 | `	}else{` |
|        - |  4814 | `		ph7_value *pObj;` |
|       53 |  4815 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4816 | `		if( pObj == 0 ){` |
|      ! 0 |  4817 | `			if( pKey ){` |
|      ! 0 |  4818 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4819 | `			}` |
|      ! 0 |  4820 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4821 | `			break;` |
|        - |  4822 | `		}` |
|        - |  4823 | `		/* Phase#1: Load the array */` |
|       53 |  4824 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4825 | `			VmPopOperand(&pTos,1);` |
|       53 |  4826 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4827 | `				/* Force a string cast */` |
|      ! 0 |  4828 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4829 | `			}` |
|       53 |  4830 | `			if( pKey == 0 ){` |
|        - |  4831 | `				/* Append string */` |
|        3 |  4832 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4833 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4834 | `				}` |
|        2 |  4835 | `			}else{` |
|        - |  4836 | `				sxu32 nOfft;` |
|       51 |  4837 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4838 | `					/* Force an int cast */` |
|       51 |  4839 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4840 | `				}` |
|       51 |  4841 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4842 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4843 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4844 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4845 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4846 | `				}else{` |
|      ! 0 |  4847 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4848 | `						/* Perform an append operation */` |
|      ! 0 |  4849 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4850 | `					}` |
|        - |  4851 | `				}` |
|        - |  4852 | `			}` |
|       53 |  4853 | `			if( pKey ){` |
|       51 |  4854 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4855 | `			}` |
|       53 |  4856 | `			break;` |
|      ! 0 |  4857 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4858 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4859 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4860 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4861 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4862 | `				goto Abort;` |
|        - |  4863 | `			}` |
|      ! 0 |  4864 | `		}` |
|        - |  4865 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4866 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4867 | `	}` |
|   182896 |  4868 | `	VmPopOperand(&pTos,1);` |
|        - |  4869 | `	/* Phase#2: Perform the insertion */` |
|   182896 |  4870 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4871 | `		/* Insertion by reference */` |
|       15 |  4872 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4873 | `	}else{` |
|   182882 |  4874 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4875 | `	}` |
|   182896 |  4876 | `	if( pKey ){` |
|    61166 |  4877 | `		PH7_MemObjRelease(pKey);` |
|    30582 |  4878 | `	}` |
|   182896 |  4879 | `	break;` |
|        - |  4880 | `					   }` |
|        - |  4881 | `/*` |
|        - |  4882 | ` * INCR: P1 * *` |
|        - |  4883 | ` *` |
|        - |  4884 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4885 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4886 | ` * the stack and increment after that.` |
|        - |  4887 | ` */` |
|   163526 |  4888 | `case PH7_OP_INCR:` |
|        - |  4889 | `#ifdef UNTRUST` |
|        - |  4890 | `	if( pTos < pStack ){` |
|        - |  4891 | `		goto Abort;` |
|        - |  4892 | `	}` |
|        - |  4893 | `#endif` |
|   327098 |  4894 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   327098 |  4895 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4896 | `			ph7_value *pObj;` |
|   327098 |  4897 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4898 | `				/* Force a numeric cast */` |
|   327098 |  4899 | `				PH7_MemObjToNumeric(pObj);` |
|   327098 |  4900 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4901 | `					pObj->rVal++;` |
|        - |  4902 | `					/* Try to get an integer representation */` |
|      ! 0 |  4903 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4904 | `				}else{` |
|   327098 |  4905 | `					pObj->x.iVal++;` |
|   327098 |  4906 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4907 | `				}` |
|   327098 |  4908 | `				if( pInstr->iP1 ){` |
|        - |  4909 | `					/* Pre-icrement */` |
|       77 |  4910 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4911 | `				}` |
|   163570 |  4912 | `			}` |
|   163572 |  4913 | `		}else{` |
|      ! 0 |  4914 | `			if( pInstr->iP1 ){` |
|        - |  4915 | `				/* Force a numeric cast */` |
|      ! 0 |  4916 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4917 | `				/* Pre-increment */` |
|      ! 0 |  4918 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4919 | `					pTos->rVal++;` |
|        - |  4920 | `					/* Try to get an integer representation */` |
|      ! 0 |  4921 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4922 | `				}else{` |
|      ! 0 |  4923 | `					pTos->x.iVal++;` |
|      ! 0 |  4924 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4925 | `				}` |
|      ! 0 |  4926 | `			}` |
|        - |  4927 | `		}` |
|   163570 |  4928 | `	}` |
|   327098 |  4929 | `	break;` |
|        - |  4930 | `/*` |
|        - |  4931 | ` * DECR: P1 * *` |
|        - |  4932 | ` *` |
|        - |  4933 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4934 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4935 | ` * and decrement after that.` |
|        - |  4936 | ` */` |
|        2 |  4937 | `case PH7_OP_DECR:` |
|        - |  4938 | `#ifdef UNTRUST` |
|        - |  4939 | `	if( pTos < pStack ){` |
|        - |  4940 | `		goto Abort;` |
|        - |  4941 | `	}` |
|        - |  4942 | `#endif` |
|        5 |  4943 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4944 | `		/* Force a numeric cast */` |
|        5 |  4945 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4946 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4947 | `			ph7_value *pObj;` |
|        5 |  4948 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4949 | `				/* Force a numeric cast */` |
|        5 |  4950 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4951 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4952 | `					pObj->rVal--;` |
|        - |  4953 | `					/* Try to get an integer representation */` |
|      ! 0 |  4954 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4955 | `				}else{` |
|        5 |  4956 | `					pObj->x.iVal--;` |
|        5 |  4957 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4958 | `				}` |
|        5 |  4959 | `				if( pInstr->iP1 ){` |
|        - |  4960 | `					/* Pre-icrement */` |
|      ! 0 |  4961 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4962 | `				}` |
|        2 |  4963 | `			}` |
|        3 |  4964 | `		}else{` |
|      ! 0 |  4965 | `			if( pInstr->iP1 ){` |
|        - |  4966 | `				/* Pre-increment */` |
|      ! 0 |  4967 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4968 | `					pTos->rVal--;` |
|        - |  4969 | `					/* Try to get an integer representation */` |
|      ! 0 |  4970 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4971 | `				}else{` |
|      ! 0 |  4972 | `					pTos->x.iVal--;` |
|      ! 0 |  4973 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4974 | `				}` |
|      ! 0 |  4975 | `			}` |
|        - |  4976 | `		}` |
|        2 |  4977 | `	}` |
|        5 |  4978 | `	break;` |
|        - |  4979 | `/*` |
|        - |  4980 | ` * UMINUS: * * *` |
|        - |  4981 | ` *` |
|        - |  4982 | ` * Perform a unary minus operation.` |
|        - |  4983 | ` */` |
|    27526 |  4984 | `case PH7_OP_UMINUS:` |
|        - |  4985 | `#ifdef UNTRUST` |
|        - |  4986 | `	if( pTos < pStack ){` |
|        - |  4987 | `		goto Abort;` |
|        - |  4988 | `	}` |
|        - |  4989 | `#endif` |
|        - |  4990 | `	/* Force a numeric (integer,real or both) cast */` |
|    55054 |  4991 | `	PH7_MemObjToNumeric(pTos);` |
|    55054 |  4992 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4993 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4994 | `	}` |
|    55054 |  4995 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    55024 |  4996 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    27511 |  4997 | `	}` |
|    55054 |  4998 | `	break;` |
|        - |  4999 | `/*` |
|        - |  5000 | ` * UPLUS: * * *` |
|        - |  5001 | ` *` |
|        - |  5002 | ` * Perform a unary plus operation.` |
|        - |  5003 | ` */` |
|       18 |  5004 | `case PH7_OP_UPLUS:` |
|        - |  5005 | `#ifdef UNTRUST` |
|        - |  5006 | `	if( pTos < pStack ){` |
|        - |  5007 | `		goto Abort;` |
|        - |  5008 | `	}` |
|        - |  5009 | `#endif` |
|        - |  5010 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5011 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5012 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5013 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5014 | `	}` |
|       37 |  5015 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5016 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5017 | `	}` |
|       37 |  5018 | `	break;` |
|        - |  5019 | `/*` |
|        - |  5020 | ` * OP_LNOT: * * *` |
|        - |  5021 | ` *` |
|        - |  5022 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5023 | ` * with its complement.` |
|        - |  5024 | ` */` |
|    42770 |  5025 | `case PH7_OP_LNOT:` |
|        - |  5026 | `#ifdef UNTRUST` |
|        - |  5027 | `	if( pTos < pStack ){` |
|        - |  5028 | `		goto Abort;` |
|        - |  5029 | `	}` |
|        - |  5030 | `#endif` |
|        - |  5031 | `	/* Force a boolean cast */` |
|    85586 |  5032 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  5033 | `		PH7_MemObjToBool(pTos);` |
|       10 |  5034 | `	}` |
|    85586 |  5035 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    85586 |  5036 | `	break;` |
|        - |  5037 | `/*` |
|        - |  5038 | ` * OP_BITNOT: * * *` |
|        - |  5039 | ` *` |
|        - |  5040 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5041 | ` * with its ones-complement.` |
|        - |  5042 | ` */` |
|       13 |  5043 | `case PH7_OP_BITNOT:` |
|        - |  5044 | `#ifdef UNTRUST` |
|        - |  5045 | `	if( pTos < pStack ){` |
|        - |  5046 | `		goto Abort;` |
|        - |  5047 | `	}` |
|        - |  5048 | `#endif` |
|        - |  5049 | `	/* Force an integer cast */` |
|       28 |  5050 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5051 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5052 | `	}` |
|       28 |  5053 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  5054 | `	break;` |
|        - |  5055 | `/* OP_MUL * * *` |
|        - |  5056 | ` * OP_MUL_STORE * * *` |
|        - |  5057 | ` *` |
|        - |  5058 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5059 | ` * and push the result back onto the stack.` |
|        - |  5060 | ` */` |
|     1278 |  5061 | `case PH7_OP_MUL:` |
|        - |  5062 | `case PH7_OP_MUL_STORE: {` |
|     2558 |  5063 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5064 | `	/* Force the operand to be numeric */` |
|        - |  5065 | `#ifdef UNTRUST` |
|        - |  5066 | `	if( pNos < pStack ){` |
|        - |  5067 | `		goto Abort;` |
|        - |  5068 | `	}` |
|        - |  5069 | `#endif` |
|     2558 |  5070 | `	PH7_MemObjToNumeric(pTos);` |
|     2558 |  5071 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5072 | `	/* Perform the requested operation */` |
|     2558 |  5073 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5074 | `		/* Floating point arithemic */` |
|        - |  5075 | `		ph7_real a,b,r;` |
|       19 |  5076 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5077 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5078 | `		}` |
|       19 |  5079 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5080 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5081 | `		}` |
|       19 |  5082 | `		a = pNos->rVal;` |
|       19 |  5083 | `		b = pTos->rVal;` |
|       19 |  5084 | `		r = a * b;` |
|        - |  5085 | `		/* Push the result */` |
|       19 |  5086 | `		pNos->rVal = r;` |
|       19 |  5087 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5088 | `		/* Try to get an integer representation */` |
|       19 |  5089 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5090 | `	}else{` |
|        - |  5091 | `		/* Integer arithmetic */` |
|        - |  5092 | `		sxi64 a,b,r;` |
|     2540 |  5093 | `		a = pNos->x.iVal;` |
|     2540 |  5094 | `		b = pTos->x.iVal;` |
|     2540 |  5095 | `		r = a * b;` |
|        - |  5096 | `		/* Push the result */` |
|     2540 |  5097 | `		pNos->x.iVal = r;` |
|     2540 |  5098 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5099 | `	}` |
|     2558 |  5100 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5101 | `		ph7_value *pObj;` |
|       32 |  5102 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5103 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5104 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5105 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5106 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5107 | `		}` |
|       15 |  5108 | `	}` |
|     2558 |  5109 | `	VmPopOperand(&pTos,1);` |
|     2558 |  5110 | `	break;` |
|        - |  5111 | `				 }` |
|        - |  5112 | `/* OP_ADD * * *` |
|        - |  5113 | ` *` |
|        - |  5114 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5115 | ` * and push the result back onto the stack.` |
|        - |  5116 | ` */` |
|      492 |  5117 | `case PH7_OP_ADD:{` |
|      986 |  5118 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5119 | `#ifdef UNTRUST` |
|        - |  5120 | `	if( pNos < pStack ){` |
|        - |  5121 | `		goto Abort;` |
|        - |  5122 | `	}` |
|        - |  5123 | `#endif` |
|        - |  5124 | `	/* Perform the addition */` |
|      986 |  5125 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      986 |  5126 | `	VmPopOperand(&pTos,1);` |
|      986 |  5127 | `	break;` |
|        - |  5128 | `				}` |
|        - |  5129 | `/*` |
|        - |  5130 | ` * OP_ADD_STORE * * *` |
|        - |  5131 | ` *` |
|        - |  5132 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5133 | ` * and push the result back onto the stack.` |
|        - |  5134 | ` */` |
|      502 |  5135 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5136 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5137 | `	ph7_value *pObj;` |
|        - |  5138 | `	sxu32 nIdx;` |
|        - |  5139 | `#ifdef UNTRUST` |
|        - |  5140 | `	if( pNos < pStack ){` |
|        - |  5141 | `		goto Abort;` |
|        - |  5142 | `	}` |
|        - |  5143 | `#endif` |
|        - |  5144 | `	/* Perform the addition */` |
|     1006 |  5145 | `	nIdx = pTos->nIdx;` |
|     1006 |  5146 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5147 | `	/* Peform the store operation */` |
|     1006 |  5148 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5149 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5150 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5151 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5152 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5153 | `	}` |
|        - |  5154 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5155 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5156 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5157 | `	break;` |
|        - |  5158 | `				}` |
|        - |  5159 | `/* OP_SUB * * *` |
|        - |  5160 | ` *` |
|        - |  5161 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5162 | ` * first (what was next on the stack) from the second (the` |
|        - |  5163 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5164 | ` */` |
|      302 |  5165 | `case PH7_OP_SUB: {` |
|      606 |  5166 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5167 | `#ifdef UNTRUST` |
|        - |  5168 | `	if( pNos < pStack ){` |
|        - |  5169 | `		goto Abort;` |
|        - |  5170 | `	}` |
|        - |  5171 | `#endif` |
|      606 |  5172 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5173 | `		/* Floating point arithemic */` |
|        - |  5174 | `		ph7_real a,b,r;` |
|       95 |  5175 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5176 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5177 | `		}` |
|       95 |  5178 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5179 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5180 | `		}` |
|       95 |  5181 | `		a = pNos->rVal;` |
|       95 |  5182 | `		b = pTos->rVal;` |
|       95 |  5183 | `		r = a - b;` |
|        - |  5184 | `		/* Push the result */` |
|       95 |  5185 | `		pNos->rVal = r;` |
|       95 |  5186 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5187 | `		/* Try to get an integer representation */` |
|       95 |  5188 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5189 | `	}else{` |
|        - |  5190 | `		/* Integer arithmetic */` |
|        - |  5191 | `		sxi64 a,b,r;` |
|      512 |  5192 | `		a = pNos->x.iVal;` |
|      512 |  5193 | `		b = pTos->x.iVal;` |
|      512 |  5194 | `		r = a - b;` |
|        - |  5195 | `		/* Push the result */` |
|      512 |  5196 | `		pNos->x.iVal = r;` |
|      512 |  5197 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5198 | `	}` |
|      606 |  5199 | `	VmPopOperand(&pTos,1);` |
|      606 |  5200 | `	break;` |
|        - |  5201 | `				 }` |
|        - |  5202 | `/* OP_SUB_STORE * * *` |
|        - |  5203 | ` *` |
|        - |  5204 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5205 | ` * first (what was next on the stack) from the second (the` |
|        - |  5206 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5207 | ` */` |
|        4 |  5208 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5209 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5210 | `	ph7_value *pObj;` |
|        - |  5211 | `#ifdef UNTRUST` |
|        - |  5212 | `	if( pNos < pStack ){` |
|        - |  5213 | `		goto Abort;` |
|        - |  5214 | `	}` |
|        - |  5215 | `#endif` |
|       10 |  5216 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5217 | `		/* Floating point arithemic */` |
|        - |  5218 | `		ph7_real a,b,r;` |
|      ! 0 |  5219 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5220 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5221 | `		}` |
|      ! 0 |  5222 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5223 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5224 | `		}` |
|      ! 0 |  5225 | `		a = pTos->rVal;` |
|      ! 0 |  5226 | `		b = pNos->rVal;` |
|      ! 0 |  5227 | `		r = a - b;` |
|        - |  5228 | `		/* Push the result */` |
|      ! 0 |  5229 | `		pNos->rVal = r;` |
|      ! 0 |  5230 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5231 | `		/* Try to get an integer representation */` |
|      ! 0 |  5232 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5233 | `	}else{` |
|        - |  5234 | `		/* Integer arithmetic */` |
|        - |  5235 | `		sxi64 a,b,r;` |
|       10 |  5236 | `		a = pTos->x.iVal;` |
|       10 |  5237 | `		b = pNos->x.iVal;` |
|       10 |  5238 | `		r = a - b;` |
|        - |  5239 | `		/* Push the result */` |
|       10 |  5240 | `		pNos->x.iVal = r;` |
|       10 |  5241 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5242 | `	}` |
|       10 |  5243 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5244 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5245 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5246 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5247 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5248 | `	}` |
|       10 |  5249 | `	VmPopOperand(&pTos,1);` |
|       10 |  5250 | `	break;` |
|        - |  5251 | `				 }` |
|        - |  5252 |  |
|        - |  5253 | `/*` |
|        - |  5254 | ` * OP_MOD * * *` |
|        - |  5255 | ` *` |
|        - |  5256 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5257 | ` * first (what was next on the stack) from the second (the` |
|        - |  5258 | ` * top of the stack) and push the remainder after division` |
|        - |  5259 | ` * onto the stack.` |
|        - |  5260 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5261 | ` */` |
|      307 |  5262 | `case PH7_OP_MOD:{` |
|      616 |  5263 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5264 | `	sxi64 a,b,r;` |
|        - |  5265 | `#ifdef UNTRUST` |
|        - |  5266 | `	if( pNos < pStack ){` |
|        - |  5267 | `		goto Abort;` |
|        - |  5268 | `	}` |
|        - |  5269 | `#endif` |
|        - |  5270 | `	/* Force the operands to be integer */` |
|      616 |  5271 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5272 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5273 | `	}` |
|      616 |  5274 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5275 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5276 | `	}` |
|        - |  5277 | `	/* Perform the requested operation */` |
|      616 |  5278 | `	a = pNos->x.iVal;` |
|      616 |  5279 | `	b = pTos->x.iVal;` |
|      616 |  5280 | `	if( b == 0 ){` |
|        3 |  5281 | `		r = 0;` |
|        3 |  5282 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5283 | `		/* goto Abort; */` |
|        2 |  5284 | `	}else{` |
|      613 |  5285 | `		r = a%b;` |
|        - |  5286 | `	}` |
|        - |  5287 | `	/* Push the result */` |
|      616 |  5288 | `	pNos->x.iVal = r;` |
|      616 |  5289 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  5290 | `	VmPopOperand(&pTos,1);` |
|      616 |  5291 | `	break;` |
|        - |  5292 | `				}` |
|        - |  5293 | `/*` |
|        - |  5294 | ` * OP_MOD_STORE * * *` |
|        - |  5295 | ` *` |
|        - |  5296 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5297 | ` * first (what was next on the stack) from the second (the` |
|        - |  5298 | ` * top of the stack) and push the remainder after division` |
|        - |  5299 | ` * onto the stack.` |
|        - |  5300 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5301 | ` */` |
|        1 |  5302 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5303 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5304 | `	ph7_value *pObj;` |
|        - |  5305 | `	sxi64 a,b,r;` |
|        - |  5306 | `#ifdef UNTRUST` |
|        - |  5307 | `	if( pNos < pStack ){` |
|        - |  5308 | `		goto Abort;` |
|        - |  5309 | `	}` |
|        - |  5310 | `#endif` |
|        - |  5311 | `	/* Force the operands to be integer */` |
|        3 |  5312 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5313 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5314 | `	}` |
|        3 |  5315 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5316 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5317 | `	}` |
|        - |  5318 | `	/* Perform the requested operation */` |
|        3 |  5319 | `	a = pTos->x.iVal;` |
|        3 |  5320 | `	b = pNos->x.iVal;` |
|        3 |  5321 | `	if( b == 0 ){` |
|      ! 0 |  5322 | `		r = 0;` |
|      ! 0 |  5323 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5324 | `		/* goto Abort; */` |
|      ! 0 |  5325 | `	}else{` |
|        3 |  5326 | `		r = a%b;` |
|        - |  5327 | `	}` |
|        - |  5328 | `	/* Push the result */` |
|        3 |  5329 | `	pNos->x.iVal = r;` |
|        3 |  5330 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5331 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5332 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5333 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5334 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5335 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5336 | `	}` |
|        3 |  5337 | `	VmPopOperand(&pTos,1);` |
|        3 |  5338 | `	break;` |
|        - |  5339 | `				}` |
|        - |  5340 | `/*` |
|        - |  5341 | ` * OP_DIV * * *` |
|        - |  5342 | ` *` |
|        - |  5343 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5344 | ` * first (what was next on the stack) from the second (the` |
|        - |  5345 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5346 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5347 | ` */` |
|       30 |  5348 | `case PH7_OP_DIV:{` |
|       62 |  5349 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5350 | `	ph7_real a,b,r;` |
|        - |  5351 | `#ifdef UNTRUST` |
|        - |  5352 | `	if( pNos < pStack ){` |
|        - |  5353 | `		goto Abort;` |
|        - |  5354 | `	}` |
|        - |  5355 | `#endif` |
|        - |  5356 | `	/* Force the operands to be real */` |
|       62 |  5357 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  5358 | `		PH7_MemObjToReal(pTos);` |
|       28 |  5359 | `	}` |
|       62 |  5360 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  5361 | `		PH7_MemObjToReal(pNos);` |
|       11 |  5362 | `	}` |
|        - |  5363 | `	/* Perform the requested operation */` |
|       62 |  5364 | `	a = pNos->rVal;` |
|       62 |  5365 | `	b = pTos->rVal;` |
|       62 |  5366 | `	if( b == 0 ){` |
|        - |  5367 | `		/* Division by zero */` |
|        3 |  5368 | `		pNos->rVal = 0;` |
|        3 |  5369 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5370 | `		/* goto Abort; */` |
|        2 |  5371 | `	}else{` |
|       59 |  5372 | `		r = a/b;` |
|        - |  5373 | `		/* Push the result */` |
|       59 |  5374 | `		pNos->rVal = r;` |
|       59 |  5375 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5376 | `		/* Try to get an integer representation */` |
|       59 |  5377 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5378 | `	}` |
|       62 |  5379 | `	VmPopOperand(&pTos,1);` |
|       62 |  5380 | `	break;` |
|        - |  5381 | `				}` |
|        - |  5382 | `/*` |
|        - |  5383 | ` * OP_DIV_STORE * * *` |
|        - |  5384 | ` *` |
|        - |  5385 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5386 | ` * first (what was next on the stack) from the second (the` |
|        - |  5387 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5388 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5389 | ` */` |
|        2 |  5390 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5391 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5392 | `	ph7_value *pObj;` |
|        - |  5393 | `	ph7_real a,b,r;` |
|        - |  5394 | `#ifdef UNTRUST` |
|        - |  5395 | `	if( pNos < pStack ){` |
|        - |  5396 | `		goto Abort;` |
|        - |  5397 | `	}` |
|        - |  5398 | `#endif` |
|        - |  5399 | `	/* Force the operands to be real */` |
|        5 |  5400 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5401 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5402 | `	}` |
|        5 |  5403 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5404 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5405 | `	}` |
|        - |  5406 | `	/* Perform the requested operation */` |
|        5 |  5407 | `	a = pTos->rVal;` |
|        5 |  5408 | `	b = pNos->rVal;` |
|        5 |  5409 | `	if( b == 0 ){` |
|        - |  5410 | `		/* Division by zero */` |
|      ! 0 |  5411 | `		r = 0;` |
|      ! 0 |  5412 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5413 | `		/* goto Abort; */` |
|      ! 0 |  5414 | `	}else{` |
|        5 |  5415 | `		r = a/b;` |
|        - |  5416 | `		/* Push the result */` |
|        5 |  5417 | `		pNos->rVal = r;` |
|        5 |  5418 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5419 | `		/* Try to get an integer representation */` |
|        5 |  5420 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5421 | `	}` |
|        5 |  5422 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5423 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5424 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5425 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5426 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5427 | `	}` |
|        5 |  5428 | `	VmPopOperand(&pTos,1);` |
|        5 |  5429 | `	break;` |
|        - |  5430 | `				}` |
|        - |  5431 | `/* OP_BAND * * *` |
|        - |  5432 | ` *` |
|        - |  5433 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5434 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5435 | ` * two elements.` |
|        - |  5436 | `*/` |
|        - |  5437 | `/* OP_BOR * * *` |
|        - |  5438 | ` *` |
|        - |  5439 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5440 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5441 | ` * two elements.` |
|        - |  5442 | ` */` |
|        - |  5443 | `/* OP_BXOR * * *` |
|        - |  5444 | ` *` |
|        - |  5445 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5446 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5447 | ` * two elements.` |
|        - |  5448 | ` */` |
|       44 |  5449 | `case PH7_OP_BAND:` |
|        - |  5450 | `case PH7_OP_BOR:` |
|        - |  5451 | `case PH7_OP_BXOR:{` |
|       90 |  5452 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5453 | `	sxi64 a,b,r;` |
|        - |  5454 | `#ifdef UNTRUST` |
|        - |  5455 | `	if( pNos < pStack ){` |
|        - |  5456 | `		goto Abort;` |
|        - |  5457 | `	}` |
|        - |  5458 | `#endif` |
|        - |  5459 | `	/* Force the operands to be integer */` |
|       90 |  5460 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5461 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5462 | `	}` |
|       90 |  5463 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5464 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5465 | `	}` |
|        - |  5466 | `	/* Perform the requested operation */` |
|       90 |  5467 | `	a = pNos->x.iVal;` |
|       90 |  5468 | `	b = pTos->x.iVal;` |
|       90 |  5469 | `	switch(pInstr->iOp){` |
|        7 |  5470 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5471 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5472 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5473 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5474 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5475 | `	case PH7_OP_BAND:` |
|       62 |  5476 | `	default:          r = a&b; break;` |
|        - |  5477 | `	}` |
|        - |  5478 | `	/* Push the result */` |
|       90 |  5479 | `	pNos->x.iVal = r;` |
|       90 |  5480 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5481 | `	VmPopOperand(&pTos,1);` |
|       90 |  5482 | `	break;` |
|        - |  5483 | `				 }` |
|        - |  5484 | `/* OP_BAND_STORE * * *` |
|        - |  5485 | ` *` |
|        - |  5486 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5487 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5488 | ` * two elements.` |
|        - |  5489 | `*/` |
|        - |  5490 | `/* OP_BOR_STORE * * *` |
|        - |  5491 | ` *` |
|        - |  5492 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5493 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5494 | ` * two elements.` |
|        - |  5495 | ` */` |
|        - |  5496 | `/* OP_BXOR_STORE * * *` |
|        - |  5497 | ` *` |
|        - |  5498 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5499 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5500 | ` * two elements.` |
|        - |  5501 | ` */` |
|       10 |  5502 | `case PH7_OP_BAND_STORE:` |
|        - |  5503 | `case PH7_OP_BOR_STORE:` |
|        - |  5504 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5505 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5506 | `	ph7_value *pObj;` |
|        - |  5507 | `	sxi64 a,b,r;` |
|        - |  5508 | `#ifdef UNTRUST` |
|        - |  5509 | `	if( pNos < pStack ){` |
|        - |  5510 | `		goto Abort;` |
|        - |  5511 | `	}` |
|        - |  5512 | `#endif` |
|        - |  5513 | `	/* Force the operands to be integer */` |
|       21 |  5514 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5515 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5516 | `	}` |
|       21 |  5517 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5518 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5519 | `	}` |
|        - |  5520 | `	/* Perform the requested operation */` |
|       21 |  5521 | `	a = pTos->x.iVal;` |
|       21 |  5522 | `	b = pNos->x.iVal;` |
|       21 |  5523 | `	switch(pInstr->iOp){` |
|        3 |  5524 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5525 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5526 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5527 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5528 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5529 | `	case PH7_OP_BAND:` |
|        7 |  5530 | `	default:          r = a&b; break;` |
|        - |  5531 | `	}` |
|        - |  5532 | `	/* Push the result */` |
|       21 |  5533 | `	pNos->x.iVal = r;` |
|       21 |  5534 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5535 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5536 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5537 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5538 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5539 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5540 | `	}` |
|       21 |  5541 | `	VmPopOperand(&pTos,1);` |
|       21 |  5542 | `	break;` |
|        - |  5543 | `				 }` |
|        - |  5544 | `/* OP_SHL * * *` |
|        - |  5545 | ` *` |
|        - |  5546 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5547 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5548 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5549 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5550 | ` */` |
|        - |  5551 | `/* OP_SHR * * *` |
|        - |  5552 | ` *` |
|        - |  5553 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5554 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5555 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5556 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5557 | ` */` |
|       12 |  5558 | `case PH7_OP_SHL:` |
|        - |  5559 | `case PH7_OP_SHR: {` |
|       25 |  5560 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5561 | `	sxi64 a,r;` |
|        - |  5562 | `	sxi32 b;` |
|        - |  5563 | `#ifdef UNTRUST` |
|        - |  5564 | `	if( pNos < pStack ){` |
|        - |  5565 | `		goto Abort;` |
|        - |  5566 | `	}` |
|        - |  5567 | `#endif` |
|        - |  5568 | `	/* Force the operands to be integer */` |
|       25 |  5569 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5570 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5571 | `	}` |
|       25 |  5572 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5573 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5574 | `	}` |
|        - |  5575 | `	/* Perform the requested operation */` |
|       25 |  5576 | `	a = pNos->x.iVal;` |
|       25 |  5577 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5578 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5579 | `		r = a << b;` |
|        8 |  5580 | `	}else{` |
|       11 |  5581 | `		r = a >> b;` |
|        - |  5582 | `	}` |
|        - |  5583 | `	/* Push the result */` |
|       25 |  5584 | `	pNos->x.iVal = r;` |
|       25 |  5585 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5586 | `	VmPopOperand(&pTos,1);` |
|       25 |  5587 | `	break;` |
|        - |  5588 | `				 }` |
|        - |  5589 | `/*  OP_SHL_STORE * * *` |
|        - |  5590 | ` *` |
|        - |  5591 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5592 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5593 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5594 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5595 | ` */` |
|        - |  5596 | `/* OP_SHR_STORE * * *` |
|        - |  5597 | ` *` |
|        - |  5598 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5599 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5600 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5601 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5602 | ` */` |
|        9 |  5603 | `case PH7_OP_SHL_STORE:` |
|        - |  5604 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5605 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5606 | `	ph7_value *pObj;` |
|        - |  5607 | `	sxi64 a,r;` |
|        - |  5608 | `	sxi32 b;` |
|        - |  5609 | `#ifdef UNTRUST` |
|        - |  5610 | `	if( pNos < pStack ){` |
|        - |  5611 | `		goto Abort;` |
|        - |  5612 | `	}` |
|        - |  5613 | `#endif` |
|        - |  5614 | `	/* Force the operands to be integer */` |
|       19 |  5615 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5616 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5617 | `	}` |
|       19 |  5618 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5619 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5620 | `	}` |
|        - |  5621 | `	/* Perform the requested operation */` |
|       19 |  5622 | `	a = pTos->x.iVal;` |
|       19 |  5623 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5624 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5625 | `		r = a << b;` |
|        5 |  5626 | `	}else{` |
|       11 |  5627 | `		r = a >> b;` |
|        - |  5628 | `	}` |
|        - |  5629 | `	/* Push the result */` |
|       19 |  5630 | `	pNos->x.iVal = r;` |
|       19 |  5631 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5632 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5633 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5634 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5635 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5636 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5637 | `	}` |
|       19 |  5638 | `	VmPopOperand(&pTos,1);` |
|       19 |  5639 | `	break;` |
|        - |  5640 | `				 }` |
|        - |  5641 | `/* CAT:  P1 * *` |
|        - |  5642 | ` *` |
|        - |  5643 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5644 | ` * back.` |
|        - |  5645 | ` */` |
|    68518 |  5646 | `case PH7_OP_CAT:{` |
|        - |  5647 | `	ph7_value *pNos,*pCur;` |
|   137038 |  5648 | `	if( pInstr->iP1 < 1 ){` |
|   109760 |  5649 | `		pNos = &pTos[-1];` |
|    54881 |  5650 | `	}else{` |
|    27280 |  5651 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5652 | `	}` |
|        - |  5653 | `#ifdef UNTRUST` |
|        - |  5654 | `	if( pNos < pStack ){` |
|        - |  5655 | `		goto Abort;` |
|        - |  5656 | `	}` |
|        - |  5657 | `#endif` |
|        - |  5658 | `	/* Force a string cast */` |
|   137038 |  5659 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5660 | `		PH7_MemObjToString(pNos);` |
|      817 |  5661 | `	}` |
|   137038 |  5662 | `	pCur = &pNos[1];` |
|   276616 |  5663 | `	while( pCur <= pTos ){` |
|   139580 |  5664 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50878 |  5665 | `			PH7_MemObjToString(pCur);` |
|    25438 |  5666 | `		}` |
|        - |  5667 | `		/* Perform the concatenation */` |
|   139580 |  5668 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   139538 |  5669 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    69768 |  5670 | `		}` |
|   139580 |  5671 | `		SyBlobRelease(&pCur->sBlob);` |
|   139580 |  5672 | `		pCur++;` |
|        2 |  5673 | `	}` |
|   137038 |  5674 | `	pTos = pNos;` |
|   137038 |  5675 | `	break;` |
|        - |  5676 | `				}` |
|        - |  5677 | `/*  CAT_STORE: * * *` |
|        - |  5678 | ` *` |
|        - |  5679 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5680 | ` * back.` |
|        - |  5681 | ` */` |
|     3808 |  5682 | `case PH7_OP_CAT_STORE:{` |
|     7618 |  5683 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5684 | `	ph7_value *pObj;` |
|        - |  5685 | `#ifdef UNTRUST` |
|        - |  5686 | `	if( pNos < pStack ){` |
|        - |  5687 | `		goto Abort;` |
|        - |  5688 | `	}` |
|        - |  5689 | `#endif` |
|     7618 |  5690 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5691 | `		/* Force a string cast */` |
|        3 |  5692 | `		PH7_MemObjToString(pTos);` |
|        1 |  5693 | `	}` |
|     7618 |  5694 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5695 | `		/* Force a string cast */` |
|      ! 0 |  5696 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5697 | `	}` |
|        - |  5698 | `	/* Perform the concatenation (Reverse order) */` |
|     7618 |  5699 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7618 |  5700 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3808 |  5701 | `	}` |
|        - |  5702 | `	/* Perform the store operation */` |
|     7618 |  5703 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5704 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7618 |  5705 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7618 |  5706 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7616 |  5707 | `		PH7_MemObjStore(pTos,pObj);` |
|     3807 |  5708 | `	}` |
|     7616 |  5709 | `	PH7_MemObjStore(pTos,pNos);` |
|     7616 |  5710 | `	VmPopOperand(&pTos,1);` |
|     7616 |  5711 | `	break;` |
|        - |  5712 | `				}` |
|        - |  5713 | `/* OP_AND: * * *` |
|        - |  5714 | ` *` |
|        - |  5715 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5716 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5717 | ` * stack.` |
|        - |  5718 | ` */` |
|        - |  5719 | `/* OP_OR: * * *` |
|        - |  5720 | ` *` |
|        - |  5721 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5722 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5723 | ` * stack.` |
|        - |  5724 | ` */` |
|   104076 |  5725 | `case PH7_OP_LAND:` |
|        - |  5726 | `case PH7_OP_LOR: {` |
|   208198 |  5727 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5728 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5729 | `#ifdef UNTRUST` |
|        - |  5730 | `	if( pNos < pStack ){` |
|        - |  5731 | `		goto Abort;` |
|        - |  5732 | `	}` |
|        - |  5733 | `#endif` |
|        - |  5734 | `	/* Force a boolean cast */` |
|   208198 |  5735 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5736 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5737 | `	}` |
|   208198 |  5738 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5739 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5740 | `	}` |
|   208198 |  5741 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   208198 |  5742 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   208198 |  5743 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5744 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    94618 |  5745 | `		v1 = and_logic[v1*3+v2];` |
|    47332 |  5746 | `	}else{` |
|        - |  5747 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   113582 |  5748 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5749 | `	}` |
|   208198 |  5750 | `	if( v1 == 2 ){` |
|      ! 0 |  5751 | `		v1 = 1;` |
|      ! 0 |  5752 | `	}` |
|   208198 |  5753 | `	VmPopOperand(&pTos,1);` |
|   208198 |  5754 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   208198 |  5755 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   208198 |  5756 | `	break;` |
|        - |  5757 | `				 }` |
|        - |  5758 | `/*` |
|        - |  5759 | ` * OP_NULLC: * * *` |
|        - |  5760 | ` * Null coalescing operator '??'.` |
|        - |  5761 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5762 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5763 | ` */` |
|        - |  5764 | `/*` |
|        - |  5765 | ` * OP_NULLC: * P2 *` |
|        - |  5766 | ` * Short-circuit null coalescing '??'.` |
|        - |  5767 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5768 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5769 | ` */` |
|       52 |  5770 | `case PH7_OP_NULLC: {` |
|        - |  5771 | `#ifdef UNTRUST` |
|        - |  5772 | `	if( pTos < pStack ){` |
|        - |  5773 | `		goto Abort;` |
|        - |  5774 | `	}` |
|        - |  5775 | `#endif` |
|      106 |  5776 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5777 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  5778 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  5779 | `	}else{` |
|        - |  5780 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  5781 | `		VmPopOperand(&pTos, 1);` |
|        - |  5782 | `	}` |
|      106 |  5783 | `	break;` |
|        - |  5784 |  |
|        - |  5785 | `/*` |
|        - |  5786 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5787 | ` * Null coalescing assignment short-circuit.` |
|        - |  5788 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5789 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5790 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5791 | ` */` |
|       23 |  5792 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5793 | `#ifdef UNTRUST` |
|        - |  5794 | `	if( pTos < pStack ){` |
|        - |  5795 | `		goto Abort;` |
|        - |  5796 | `	}` |
|        - |  5797 | `#endif` |
|       47 |  5798 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5799 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5800 | `	}` |
|       47 |  5801 | `	break;` |
|        - |  5802 |  |
|        - |  5803 | `/*` |
|        - |  5804 | ` * OP_NULLC_STORE: * * *` |
|        - |  5805 | ` * Null coalescing assignment store.` |
|        - |  5806 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5807 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5808 | ` * expression result.` |
|        - |  5809 | ` */` |
|        - |  5810 | `/*` |
|        - |  5811 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  5812 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  5813 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  5814 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  5815 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  5816 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  5817 | ` */` |
|       51 |  5818 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  5819 | `#ifdef UNTRUST` |
|        - |  5820 | `	if( pTos < pStack ){` |
|        - |  5821 | `		goto Abort;` |
|        - |  5822 | `	}` |
|        - |  5823 | `#endif` |
|      104 |  5824 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  5825 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  5826 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  5827 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  5828 | `	}` |
|      104 |  5829 | `	break;` |
|        - |  5830 |  |
|       14 |  5831 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5832 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5833 | `	ph7_value *pObj;` |
|        - |  5834 | `	sxu32 nIdx;` |
|        - |  5835 | `#ifdef UNTRUST` |
|        - |  5836 | `	if( pNos < pStack ){` |
|        - |  5837 | `		goto Abort;` |
|        - |  5838 | `	}` |
|        - |  5839 | `#endif` |
|       29 |  5840 | `	nIdx = pNos->nIdx;` |
|       29 |  5841 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5842 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5843 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5844 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5845 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5846 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5847 | `	}` |
|       29 |  5848 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5849 | `	VmPopOperand(&pTos,1);` |
|       29 |  5850 | `	break;` |
|        - |  5851 |  |
|        - |  5852 | `/*` |
|        - |  5853 | ` * OP_SPREAD: * * *` |
|        - |  5854 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5855 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5856 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5857 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5858 | ` */` |
|        9 |  5859 | `case PH7_OP_SPREAD: {` |
|        - |  5860 | `#ifdef UNTRUST` |
|        - |  5861 | `	if( pTos < pStack ){` |
|        - |  5862 | `		goto Abort;` |
|        - |  5863 | `	}` |
|        - |  5864 | `#endif` |
|       20 |  5865 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  5866 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  5867 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  5868 | `		if( nEntry == 0 ){` |
|        - |  5869 | `			/* Empty array — remove from stack */` |
|        3 |  5870 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5871 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  5872 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5873 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5874 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5875 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5876 | `				VM_STACK_GUARD);` |
|      ! 0 |  5877 | `		}else{` |
|        - |  5878 | `			ph7_hashmap_node *pNode2;` |
|        - |  5879 | `			ph7_value *pElem;` |
|        - |  5880 | `			sxu32 i;` |
|        - |  5881 | `			/* Overwrite TOS with first element */` |
|       18 |  5882 | `			pNode2 = pMap->pFirst;` |
|       18 |  5883 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  5884 | `			PH7_MemObjRelease(pTos);` |
|       18 |  5885 | `			if( pElem ){` |
|       18 |  5886 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  5887 | `			}` |
|       18 |  5888 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5889 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5890 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  5891 | `			pNode2 = pNode2->pPrev;` |
|        - |  5892 | `			/* Push remaining elements */` |
|       44 |  5893 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  5894 | `				pTos++;` |
|       28 |  5895 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  5896 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  5897 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  5898 | `				if( pElem ){` |
|       28 |  5899 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  5900 | `				}` |
|       28 |  5901 | `				pNode2 = pNode2->pPrev;` |
|       15 |  5902 | `			}` |
|       18 |  5903 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5904 | `		}` |
|        9 |  5905 | `	}` |
|        - |  5906 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  5907 | `	break;` |
|        - |  5908 |  |
|        - |  5909 | `/* OP_LXOR: * * *` |
|        - |  5910 | ` *` |
|        - |  5911 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5912 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5913 | ` * stack.` |
|        - |  5914 | ` * According to the PHP language reference manual:` |
|        - |  5915 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5916 | ` *  TRUE,but not both.` |
|        - |  5917 | ` */` |
|        5 |  5918 | `case PH7_OP_LXOR:{` |
|       11 |  5919 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5920 | `	sxi32 v = 0;` |
|        - |  5921 | `#ifdef UNTRUST` |
|        - |  5922 | `	if( pNos < pStack ){` |
|        - |  5923 | `		goto Abort;` |
|        - |  5924 | `	}` |
|        - |  5925 | `#endif` |
|        - |  5926 | `	/* Force a boolean cast */` |
|       11 |  5927 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5928 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5929 | `	}` |
|       11 |  5930 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5931 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5932 | `	}` |
|       11 |  5933 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5934 | `		v = 1;` |
|        3 |  5935 | `	}` |
|       11 |  5936 | `	VmPopOperand(&pTos,1);` |
|       11 |  5937 | `	pTos->x.iVal = v;` |
|       11 |  5938 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5939 | `	break;` |
|        - |  5940 | `				 }` |
|        - |  5941 | `/* OP_EQ P1 P2 P3` |
|        - |  5942 | ` *` |
|        - |  5943 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5944 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5945 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5946 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5947 | ` */` |
|        - |  5948 | `/* OP_NEQ P1 P2 P3` |
|        - |  5949 | ` *` |
|        - |  5950 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5951 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5952 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5953 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5954 | ` */` |
|     4328 |  5955 | `case PH7_OP_EQ:` |
|        - |  5956 | `case PH7_OP_NEQ: {` |
|     8658 |  5957 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5958 | `	/* Perform the comparison and act accordingly */` |
|        - |  5959 | `#ifdef UNTRUST` |
|        - |  5960 | `	if( pNos < pStack ){` |
|        - |  5961 | `		goto Abort;` |
|        - |  5962 | `	}` |
|        - |  5963 | `#endif` |
|     8658 |  5964 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8658 |  5965 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5966 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8649 |  5967 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8614 |  5968 | `		rc = rc == 0;` |
|     4308 |  5969 | `	}else{` |
|       28 |  5970 | `		rc = rc != 0;` |
|        - |  5971 | `	}` |
|     8658 |  5972 | `	VmPopOperand(&pTos,1);` |
|     8658 |  5973 | `	if( !pInstr->iP2 ){` |
|        - |  5974 | `		/* Push comparison result without taking the jump */` |
|     8658 |  5975 | `		PH7_MemObjRelease(pTos);` |
|     8658 |  5976 | `		pTos->x.iVal = rc;` |
|        - |  5977 | `		/* Invalidate any prior representation */` |
|     8658 |  5978 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4330 |  5979 | `	}else{` |
|      ! 0 |  5980 | `		if( rc ){` |
|        - |  5981 | `			/* Jump to the desired location */` |
|      ! 0 |  5982 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5983 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5984 | `		}` |
|        - |  5985 | `	}` |
|     8658 |  5986 | `	break;` |
|        - |  5987 | `				 }` |
|        - |  5988 | `/* OP_TEQ P1 P2 *` |
|        - |  5989 | ` *` |
|        - |  5990 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5991 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5992 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5993 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5994 | ` */` |
|   152111 |  5995 | `case PH7_OP_TEQ: {` |
|   304224 |  5996 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5997 | `	/* Perform the comparison and act accordingly */` |
|        - |  5998 | `#ifdef UNTRUST` |
|        - |  5999 | `	if( pNos < pStack ){` |
|        - |  6000 | `		goto Abort;` |
|        - |  6001 | `	}` |
|        - |  6002 | `#endif` |
|   304224 |  6003 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   304224 |  6004 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6005 | `		rc = 0;` |
|        2 |  6006 | `	}else{` |
|   304222 |  6007 | `		rc = rc == 0;` |
|        - |  6008 | `	}` |
|   304224 |  6009 | `	VmPopOperand(&pTos,1);` |
|   304224 |  6010 | `	if( !pInstr->iP2 ){` |
|        - |  6011 | `		/* Push comparison result without taking the jump */` |
|   304224 |  6012 | `		PH7_MemObjRelease(pTos);` |
|   304224 |  6013 | `		pTos->x.iVal = rc;` |
|        - |  6014 | `		/* Invalidate any prior representation */` |
|   304224 |  6015 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   152113 |  6016 | `	}else{` |
|      ! 0 |  6017 | `		if( rc ){` |
|        - |  6018 | `			/* Jump to the desired location */` |
|      ! 0 |  6019 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6020 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6021 | `		}` |
|        - |  6022 | `	}` |
|   304224 |  6023 | `	break;` |
|        - |  6024 | `				 }` |
|        - |  6025 | `/* OP_TNE P1 P2 *` |
|        - |  6026 | ` *` |
|        - |  6027 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6028 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6029 | ` * instruction.` |
|        - |  6030 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6031 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6032 | ` *` |
|        - |  6033 | ` */` |
|   117397 |  6034 | `case PH7_OP_TNE: {` |
|   234796 |  6035 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6036 | `	/* Perform the comparison and act accordingly */` |
|        - |  6037 | `#ifdef UNTRUST` |
|        - |  6038 | `	if( pNos < pStack ){` |
|        - |  6039 | `		goto Abort;` |
|        - |  6040 | `	}` |
|        - |  6041 | `#endif` |
|   234796 |  6042 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   234796 |  6043 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6044 | `		rc = 1;` |
|        2 |  6045 | `	}else{` |
|   234794 |  6046 | `		rc = rc != 0;` |
|        - |  6047 | `	}` |
|   234796 |  6048 | `	VmPopOperand(&pTos,1);` |
|   234796 |  6049 | `	if( !pInstr->iP2 ){` |
|        - |  6050 | `		/* Push comparison result without taking the jump */` |
|   234796 |  6051 | `		PH7_MemObjRelease(pTos);` |
|   234796 |  6052 | `		pTos->x.iVal = rc;` |
|        - |  6053 | `		/* Invalidate any prior representation */` |
|   234796 |  6054 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   117399 |  6055 | `	}else{` |
|      ! 0 |  6056 | `		if( rc ){` |
|        - |  6057 | `			/* Jump to the desired location */` |
|      ! 0 |  6058 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6059 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6060 | `		}` |
|        - |  6061 | `	}` |
|   234796 |  6062 | `	break;` |
|        - |  6063 | `				 }` |
|        - |  6064 | `/* OP_LT P1 P2 P3` |
|        - |  6065 | ` *` |
|        - |  6066 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6067 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6068 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6069 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6070 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6071 | ` *` |
|        - |  6072 | ` */` |
|        - |  6073 | `/* OP_LE P1 P2 P3` |
|        - |  6074 | ` *` |
|        - |  6075 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6076 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6077 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6078 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6079 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6080 | ` *` |
|        - |  6081 | ` */` |
|   109839 |  6082 | `case PH7_OP_LT:` |
|        - |  6083 | `case PH7_OP_LE: {` |
|   219724 |  6084 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6085 | `	/* Perform the comparison and act accordingly */` |
|        - |  6086 | `#ifdef UNTRUST` |
|        - |  6087 | `	if( pNos < pStack ){` |
|        - |  6088 | `		goto Abort;` |
|        - |  6089 | `	}` |
|        - |  6090 | `#endif` |
|   219724 |  6091 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   219724 |  6092 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6093 | `		rc = 0;` |
|   219720 |  6094 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1562 |  6095 | `		rc = rc < 1;` |
|      782 |  6096 | `	}else{` |
|   218156 |  6097 | `		rc = rc < 0;` |
|        - |  6098 | `	}` |
|   219724 |  6099 | `	VmPopOperand(&pTos,1);` |
|   219724 |  6100 | `	if( !pInstr->iP2 ){` |
|        - |  6101 | `		/* Push comparison result without taking the jump */` |
|   219724 |  6102 | `		PH7_MemObjRelease(pTos);` |
|   219724 |  6103 | `		pTos->x.iVal = rc;` |
|        - |  6104 | `		/* Invalidate any prior representation */` |
|   219724 |  6105 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   109885 |  6106 | `	}else{` |
|      ! 0 |  6107 | `		if( rc ){` |
|        - |  6108 | `			/* Jump to the desired location */` |
|      ! 0 |  6109 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6110 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6111 | `		}` |
|        - |  6112 | `	}` |
|   219724 |  6113 | `	break;` |
|        - |  6114 | `				}` |
|        - |  6115 | `/* OP_GT P1 P2 P3` |
|        - |  6116 | ` *` |
|        - |  6117 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6118 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6119 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6120 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6121 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6122 | ` *` |
|        - |  6123 | ` */` |
|        - |  6124 | `/* OP_GE P1 P2 P3` |
|        - |  6125 | ` *` |
|        - |  6126 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6127 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6128 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6129 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6130 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6131 | ` *` |
|        - |  6132 | ` */` |
|    54156 |  6133 | `case PH7_OP_GT:` |
|        - |  6134 | `case PH7_OP_GE: {` |
|   108314 |  6135 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6136 | `	/* Perform the comparison and act accordingly */` |
|        - |  6137 | `#ifdef UNTRUST` |
|        - |  6138 | `	if( pNos < pStack ){` |
|        - |  6139 | `		goto Abort;` |
|        - |  6140 | `	}` |
|        - |  6141 | `#endif` |
|   108314 |  6142 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   108314 |  6143 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6144 | `		rc = 0;` |
|   108310 |  6145 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   108142 |  6146 | `		rc = rc >= 0;` |
|    54072 |  6147 | `	}else{` |
|      166 |  6148 | `		rc = rc > 0;` |
|        - |  6149 | `	}` |
|   108314 |  6150 | `	VmPopOperand(&pTos,1);` |
|   108314 |  6151 | `	if( !pInstr->iP2 ){` |
|        - |  6152 | `		/* Push comparison result without taking the jump */` |
|   108314 |  6153 | `		PH7_MemObjRelease(pTos);` |
|   108314 |  6154 | `		pTos->x.iVal = rc;` |
|        - |  6155 | `		/* Invalidate any prior representation */` |
|   108314 |  6156 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    54158 |  6157 | `	}else{` |
|      ! 0 |  6158 | `		if( rc ){` |
|        - |  6159 | `			/* Jump to the desired location */` |
|      ! 0 |  6160 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6161 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6162 | `		}` |
|        - |  6163 | `	}` |
|   108314 |  6164 | `	break;` |
|        - |  6165 | `				}` |
|        - |  6166 | `/* OP_SPACESHIP * * *` |
|        - |  6167 | ` *` |
|        - |  6168 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6169 | ` *   -1 if left < right` |
|        - |  6170 | ` *    0 if left == right` |
|        - |  6171 | ` *    1 if left > right` |
|        - |  6172 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6173 | ` */` |
|       25 |  6174 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6175 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6176 | `#ifdef UNTRUST` |
|        - |  6177 | `	if( pNos < pStack ){` |
|        - |  6178 | `		goto Abort;` |
|        - |  6179 | `	}` |
|        - |  6180 | `#endif` |
|       51 |  6181 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6182 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6183 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6184 | `		rc = 1;` |
|        4 |  6185 | `	}else{` |
|        - |  6186 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6187 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6188 | `	}` |
|       51 |  6189 | `	VmPopOperand(&pTos,1);` |
|       51 |  6190 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6191 | `	pTos->x.iVal = rc;` |
|       51 |  6192 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6193 | `	break;` |
|        - |  6194 | `				}` |
|        - |  6195 | `/* OP_SEQ P1 P2 *` |
|        - |  6196 | ` * Strict string comparison.` |
|        - |  6197 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6198 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6199 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6200 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6201 | ` * use PH7_OP_EQ.` |
|        - |  6202 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6203 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6204 | ` */` |
|        - |  6205 | `/* OP_SNE P1 P2 *` |
|        - |  6206 | ` * Strict string comparison.` |
|        - |  6207 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6208 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6209 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6210 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6211 | ` * use PH7_OP_EQ.` |
|        - |  6212 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6213 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6214 | ` */` |
|       18 |  6215 | `case PH7_OP_SEQ:` |
|        - |  6216 | `case PH7_OP_SNE: {` |
|       38 |  6217 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6218 | `	SyString s1,s2;` |
|        - |  6219 | `	/* Perform the comparison and act accordingly */` |
|        - |  6220 | `#ifdef UNTRUST` |
|        - |  6221 | `	if( pNos < pStack ){` |
|        - |  6222 | `		goto Abort;` |
|        - |  6223 | `	}` |
|        - |  6224 | `#endif` |
|        - |  6225 | `	/* Force a string cast */` |
|       38 |  6226 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6227 | `		PH7_MemObjToString(pTos);` |
|        2 |  6228 | `	}` |
|       38 |  6229 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6230 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6231 | `	}` |
|       38 |  6232 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6233 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6234 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6235 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6236 | `		rc = rc != 0;` |
|      ! 0 |  6237 | `	}else{` |
|       38 |  6238 | `		rc = rc == 0;` |
|        - |  6239 | `	}` |
|       38 |  6240 | `	VmPopOperand(&pTos,1);` |
|       38 |  6241 | `	if( !pInstr->iP2 ){` |
|        - |  6242 | `		/* Push comparison result without taking the jump */` |
|       38 |  6243 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6244 | `		pTos->x.iVal = rc;` |
|        - |  6245 | `		/* Invalidate any prior representation */` |
|       38 |  6246 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6247 | `	}else{` |
|      ! 0 |  6248 | `		if( rc ){` |
|        - |  6249 | `			/* Jump to the desired location */` |
|      ! 0 |  6250 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6251 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6252 | `		}` |
|        - |  6253 | `	}` |
|       38 |  6254 | `	break;` |
|        - |  6255 | `				 }` |
|        - |  6256 | `/*` |
|        - |  6257 | ` * OP_LOAD_REF * * *` |
|        - |  6258 | ` * Push the index of a referenced object on the stack.` |
|        - |  6259 | ` */` |
|       57 |  6260 | `case PH7_OP_LOAD_REF: {` |
|        - |  6261 | `	sxu32 nIdx;` |
|        - |  6262 | `#ifdef UNTRUST` |
|        - |  6263 | `	if( pTos < pStack ){` |
|        - |  6264 | `		goto Abort;` |
|        - |  6265 | `	}` |
|        - |  6266 | `#endif` |
|        - |  6267 | `	/* Extract memory object index */` |
|      115 |  6268 | `	nIdx = pTos->nIdx;` |
|      115 |  6269 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6270 | `		/* Nullify the object */` |
|       95 |  6271 | `		PH7_MemObjRelease(pTos);` |
|        - |  6272 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6273 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6274 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6275 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6276 | `	}` |
|      115 |  6277 | `	break;` |
|        - |  6278 | `					  }` |
|        - |  6279 | `/*` |
|        - |  6280 | ` * OP_STORE_REF * * P3` |
|        - |  6281 | ` * Perform an assignment operation by reference.` |
|        - |  6282 | ` */` |
|       16 |  6283 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6284 | `	 SyString sName = { 0 , 0 };` |
|        - |  6285 | `	 VmFrame *pFrameLocal;` |
|        - |  6286 | `	SyHashEntry *pEntry;` |
|        - |  6287 | `	sxu32 nIdx;` |
|        - |  6288 | `#ifdef UNTRUST` |
|        - |  6289 | `	if( pTos < pStack ){` |
|        - |  6290 | `		goto Abort;` |
|        - |  6291 | `	}` |
|        - |  6292 | `#endif` |
|       34 |  6293 | `	if( pInstr->p3 == 0 ){` |
|        - |  6294 | `		char *zName;` |
|        - |  6295 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  6296 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6297 | `			/* Force a string cast */` |
|      ! 0 |  6298 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6299 | `		}` |
|      ! 0 |  6300 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6301 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  6302 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6303 | `			if( zName ){` |
|      ! 0 |  6304 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6305 | `			}` |
|      ! 0 |  6306 | `		}` |
|      ! 0 |  6307 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6308 | `		pTos--;` |
|      ! 0 |  6309 | `	}else{` |
|       34 |  6310 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6311 | `	}` |
|       34 |  6312 | `	nIdx = pTos->nIdx;` |
|       34 |  6313 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6314 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6315 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6316 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6317 | `		}else{` |
|        - |  6318 | `			ph7_value *pObj;` |
|        - |  6319 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6320 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6321 | `			if( pObj == 0 ){` |
|      ! 0 |  6322 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6323 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6324 | `				goto Abort;` |
|        - |  6325 | `			}` |
|        - |  6326 | `			/* Perform the store operation */` |
|      ! 0 |  6327 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6328 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6329 | `		}` |
|       34 |  6330 | `	}else if( sName.nByte > 0){` |
|       34 |  6331 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6332 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6333 | `		}else{` |
|       34 |  6334 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6335 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6336 | `			/* Query the local frame */` |
|       34 |  6337 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6338 | `			if( pEntry ){` |
|      ! 0 |  6339 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6340 | `			}else{` |
|       34 |  6341 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6342 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6343 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6344 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6345 | `				}` |
|       34 |  6346 | `				if( rc == SXRET_OK ){` |
|       34 |  6347 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6348 | `				}` |
|        - |  6349 | `			}` |
|        - |  6350 | `		}` |
|       16 |  6351 | `	}` |
|       34 |  6352 | `	break;` |
|        - |  6353 | `				 }` |
|        - |  6354 | `/*` |
|        - |  6355 | ` * OP_UPLINK P1 * *` |
|        - |  6356 | ` * Link a variable to the top active VM frame.` |
|        - |  6357 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6358 | ` */` |
|       28 |  6359 | `case PH7_OP_UPLINK: {` |
|       58 |  6360 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6361 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6362 | `		SyString sName;` |
|        - |  6363 | `		/* Perform the link */` |
|      116 |  6364 | `		while( pLink <= pTos ){` |
|       60 |  6365 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6366 | `				/* Force a string cast */` |
|      ! 0 |  6367 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6368 | `			}` |
|       60 |  6369 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6370 | `			if( sName.nByte > 0 ){` |
|       60 |  6371 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6372 | `			}` |
|       60 |  6373 | `			pLink++;` |
|        2 |  6374 | `		}` |
|       28 |  6375 | `	}` |
|       58 |  6376 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6377 | `	break;` |
|        - |  6378 | `					}` |
|        - |  6379 | `/*` |
|        - |  6380 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6381 | ` * Push an exception in the corresponding container so that` |
|        - |  6382 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6383 | ` */` |
|      110 |  6384 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      222 |  6385 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6386 | `	VmFrame *pFrameLocal;` |
|        - |  6387 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      222 |  6388 | `	pException->iFinallyDone = 0;` |
|      222 |  6389 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6390 | `	/* Create the exception frame */` |
|      222 |  6391 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      222 |  6392 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6393 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6394 | `		goto Abort;` |
|        - |  6395 | `	}` |
|        - |  6396 | `	/* Mark the special frame */` |
|      222 |  6397 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      222 |  6398 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6399 | `	/* Point to the frame that trigger the exception */` |
|      222 |  6400 | `	pFrameLocal = pFrameLocal->pParent;` |
|      222 |  6401 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      222 |  6402 | `	pException->pFrame = pFrameLocal;` |
|      222 |  6403 | `	break;` |
|        - |  6404 | `							}` |
|        - |  6405 | `/*` |
|        - |  6406 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6407 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6408 | ` */` |
|      109 |  6409 | `case PH7_OP_POP_EXCEPTION: {` |
|      220 |  6410 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      220 |  6411 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6412 | `		ph7_exception **apException;` |
|        - |  6413 | `		/* Pop the loaded exception */` |
|       32 |  6414 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6415 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6416 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6417 | `		}` |
|       15 |  6418 | `	}` |
|      220 |  6419 | `	pException->pFrame = 0;` |
|        - |  6420 | `	/* Leave the exception frame */` |
|      220 |  6421 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6422 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      220 |  6423 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6424 | `		sxi32 rcFinally;` |
|       20 |  6425 | `		pException->iFinallyDone = 1;` |
|       20 |  6426 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6427 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6428 | `			goto Abort;` |
|        - |  6429 | `		}` |
|        9 |  6430 | `	}` |
|      220 |  6431 | `	break;` |
|        - |  6432 | `							}` |
|        - |  6433 |  |
|        - |  6434 | `/*` |
|        - |  6435 | ` * OP_THROW * P2 *` |
|        - |  6436 | ` * Throw an user exception.` |
|        - |  6437 | ` */` |
|       58 |  6438 | `case PH7_OP_THROW: {` |
|      118 |  6439 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6440 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6441 | `#ifdef UNTRUST` |
|        - |  6442 | `	if( pTos < pStack ){` |
|        - |  6443 | `		goto Abort;` |
|        - |  6444 | `	}` |
|        - |  6445 | `#endif` |
|      118 |  6446 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6447 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6448 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6449 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6450 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6451 | `		ph7_class *pThrowable;` |
|        - |  6452 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6453 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6454 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6455 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6456 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6457 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6458 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6459 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6460 | `			if( pErrorClass ){` |
|        3 |  6461 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6462 | `			}` |
|        3 |  6463 | `			if( pErrInst ){` |
|        - |  6464 | `				ph7_class_method *pCons;` |
|        3 |  6465 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6466 | `				if( pCons ){` |
|        - |  6467 | `					ph7_value sArg;` |
|        - |  6468 | `					ph7_value *apArg[1];` |
|        - |  6469 | `					SyString sMsgStr;` |
|        - |  6470 | `					static const char zErrMsg[] =` |
|        - |  6471 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6472 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6473 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6474 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6475 | `					apArg[0] = &sArg;` |
|        3 |  6476 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6477 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6478 | `				}` |
|        3 |  6479 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6480 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6481 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6482 | `					goto Abort;` |
|        - |  6483 | `				}` |
|        2 |  6484 | `			}else{` |
|        - |  6485 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6486 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6487 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6488 | `					goto Abort;` |
|        - |  6489 | `				}` |
|        - |  6490 | `			}` |
|        2 |  6491 | `		}else{` |
|        - |  6492 | `			/* Throw the exception */` |
|      116 |  6493 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6494 | `			if( rc == SXERR_ABORT ){` |
|        - |  6495 | `				/* Abort processing immediately */` |
|       11 |  6496 | `				goto Abort;` |
|        - |  6497 | `			}` |
|        - |  6498 | `		}` |
|       55 |  6499 | `	}else{` |
|        - |  6500 | `		/* Expecting a class instance */` |
|      ! 0 |  6501 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6502 | `		if( rc == SXERR_ABORT ){` |
|        - |  6503 | `			/* Abort processing immediately */` |
|      ! 0 |  6504 | `			goto Abort;` |
|        - |  6505 | `		}` |
|        - |  6506 | `	}` |
|        - |  6507 | `	/* Pop the top entry */` |
|      108 |  6508 | `	VmPopOperand(&pTos,1);` |
|        - |  6509 | `	/* Perform an unconditional jump */` |
|      108 |  6510 | `	pc = nJump - 1;` |
|      108 |  6511 | `	break;` |
|        - |  6512 | `				   }` |
|        - |  6513 | `/*` |
|        - |  6514 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6515 | ` * Prepare a foreach step.` |
|        - |  6516 | ` */` |
|     5735 |  6517 | `case PH7_OP_FOREACH_INIT: {` |
|    11472 |  6518 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6519 | `	void *pName;` |
|        - |  6520 | `#ifdef UNTRUST` |
|        - |  6521 | `	if( pTos < pStack ){` |
|        - |  6522 | `		goto Abort;` |
|        - |  6523 | `	}` |
|        - |  6524 | `#endif` |
|    11472 |  6525 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6526 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6527 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6528 | `			/* Force a string cast */` |
|      ! 0 |  6529 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6530 | `		}` |
|        - |  6531 | `		/* Duplicate name */` |
|      ! 0 |  6532 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6533 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6534 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6535 | `		}` |
|      ! 0 |  6536 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6537 | `	}` |
|    11472 |  6538 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6539 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6540 | `			/* Force a string cast */` |
|      ! 0 |  6541 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6542 | `		}` |
|        - |  6543 | `		/* Duplicate name */` |
|      ! 0 |  6544 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6545 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6546 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6547 | `		}` |
|      ! 0 |  6548 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6549 | `	}` |
|        - |  6550 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11472 |  6551 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6552 | `		/* Jump out of the loop */` |
|      ! 0 |  6553 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6554 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6555 | `		}` |
|      ! 0 |  6556 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6557 | `	}else{` |
|        - |  6558 | `		ph7_foreach_step *pStep;` |
|    11472 |  6559 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11472 |  6560 | `		if( pStep == 0 ){` |
|      ! 0 |  6561 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6562 | `			/* Jump out of the loop */` |
|      ! 0 |  6563 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6564 | `		}else{` |
|        - |  6565 | `			/* Zero the structure */` |
|    11472 |  6566 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6567 | `			/* Prepare the step */` |
|    11472 |  6568 | `			pStep->iFlags = pInfo->iFlags;` |
|    11472 |  6569 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6570 | `				ph7_hashmap *pMap;` |
|        - |  6571 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6572 | `				 * source array so mutations don't affect other sharers. */` |
|    11440 |  6573 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6574 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6575 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6576 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6577 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6578 | `						 * variable still points at the same hashmap as` |
|        - |  6579 | `						 * the stack value. */` |
|        9 |  6580 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6581 | `							pCur->iRef--;` |
|        9 |  6582 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6583 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6584 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6585 | `						}` |
|        4 |  6586 | `					}` |
|        4 |  6587 | `				}` |
|    11440 |  6588 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6589 | `				/* Reset the internal loop cursor */` |
|    11440 |  6590 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6591 | `				/* Mark the step */` |
|    11440 |  6592 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11440 |  6593 | `				pStep->xIter.pMap = pMap;` |
|    11440 |  6594 | `				pMap->iRef++;` |
|     5721 |  6595 | `			}else{` |
|       34 |  6596 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6597 | `				ph7_class *pIteratorClass;` |
|        - |  6598 | `				/* Check if the object implements Iterator */` |
|       34 |  6599 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6600 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6601 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6602 | `					ph7_class_method *pRewind;` |
|       24 |  6603 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6604 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6605 | `					pThis->iRef++;` |
|       24 |  6606 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6607 | `					if( pRewind ){` |
|       24 |  6608 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6609 | `					}` |
|       13 |  6610 | `				}else{` |
|        - |  6611 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6612 | `					ph7_class *pIterAggClass;` |
|       12 |  6613 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6614 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6615 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6616 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6617 | `						ph7_class_method *pGetIter;` |
|        3 |  6618 | `						int iterAggOk = 0;` |
|        3 |  6619 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6620 | `						if( pGetIter ){` |
|        - |  6621 | `							ph7_value sResult;` |
|        3 |  6622 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6623 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6624 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6625 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6626 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6627 | `									ph7_class_method *pRewind;` |
|        3 |  6628 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6629 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6630 | `									pIterObj->iRef++;` |
|        - |  6631 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6632 | `									pStep->pOwner = pThis;` |
|        3 |  6633 | `									pThis->iRef++;` |
|        3 |  6634 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6635 | `									if( pRewind ){` |
|        3 |  6636 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6637 | `									}` |
|        3 |  6638 | `									iterAggOk = 1;` |
|        1 |  6639 | `								}` |
|        1 |  6640 | `							}` |
|        3 |  6641 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6642 | `						}` |
|        3 |  6643 | `						if( !iterAggOk ){` |
|        - |  6644 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6645 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6646 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6647 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6648 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6649 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6650 | `						}` |
|        2 |  6651 | `					}else{` |
|        - |  6652 | `						/* Plain object iteration via hAttr */` |
|        9 |  6653 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6654 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6655 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6656 | `						pThis->iRef++;` |
|        - |  6657 | `					}` |
|        - |  6658 | `				}` |
|        - |  6659 | `			}` |
|        - |  6660 | `		}` |
|    11472 |  6661 | `		if( pStep ){` |
|    11472 |  6662 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6663 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6664 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6665 | `				/* Jump out of the loop */` |
|      ! 0 |  6666 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6667 | `			}` |
|     5735 |  6668 | `		}` |
|        - |  6669 | `	}` |
|    11472 |  6670 | `	VmPopOperand(&pTos,1);` |
|    11472 |  6671 | `	break;` |
|        - |  6672 | `						  }` |
|        - |  6673 | `/*` |
|        - |  6674 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6675 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6676 | ` */` |
|    93590 |  6677 | `case PH7_OP_FOREACH_STEP: {` |
|   187182 |  6678 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6679 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6680 | `	ph7_value *pValue;` |
|        - |  6681 | `	VmFrame *pFrameLocal;` |
|        - |  6682 | `	/* Peek the last step */` |
|   187182 |  6683 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   187182 |  6684 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   187182 |  6685 | `	pFrameLocal = pVm->pFrame;` |
|   187182 |  6686 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   187182 |  6687 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   187054 |  6688 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6689 | `		ph7_hashmap_node *pNode;` |
|        - |  6690 | `		/* Extract the current node value */` |
|   187054 |  6691 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   187054 |  6692 | `		if( pNode == 0 ){` |
|        - |  6693 | `			/* No more entry to process */` |
|    11438 |  6694 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11438 |  6695 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6696 | `				/* Break the reference with the last element */` |
|        7 |  6697 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6698 | `			}` |
|        - |  6699 | `			/* Automatically reset the loop cursor */` |
|    11438 |  6700 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6701 | `			/* Cleanup the mess left behind */` |
|    11438 |  6702 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11438 |  6703 | `			SySetPop(&pInfo->aStep);` |
|    11438 |  6704 | `			PH7_HashmapUnref(pMap);` |
|     5720 |  6705 | `		}else{` |
|   175618 |  6706 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6707 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6708 | `				if( pKey ){` |
|      426 |  6709 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6710 | `				}` |
|      212 |  6711 | `			}` |
|   175618 |  6712 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6713 | `				SyHashEntry *pEntry;` |
|        - |  6714 | `				/* Pass by reference */` |
|       23 |  6715 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6716 | `				if( pEntry ){` |
|       21 |  6717 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6718 | `				}else{` |
|        4 |  6719 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6720 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6721 | `				}` |
|       12 |  6722 | `			}else{` |
|        - |  6723 | `				/* Make a copy of the entry value */` |
|   175596 |  6724 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   175596 |  6725 | `				if( pValue ){` |
|   175596 |  6726 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    87797 |  6727 | `				}` |
|        - |  6728 | `			}` |
|        2 |  6729 | `		}` |
|    93656 |  6730 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6731 | `		/* Iterator-based iteration.` |
|        - |  6732 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6733 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6734 | `		 */` |
|      106 |  6735 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6736 | `		ph7_class_method *pMethod;` |
|        - |  6737 | `		ph7_value sResult;` |
|      106 |  6738 | `		int isValid = 0;` |
|        - |  6739 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6740 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6741 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6742 | `		}else{` |
|       82 |  6743 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6744 | `			if( pMethod ){` |
|       82 |  6745 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6746 | `			}` |
|        - |  6747 | `		}` |
|        - |  6748 | `		/* Call valid() */` |
|      106 |  6749 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6750 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6751 | `		if( pMethod ){` |
|      106 |  6752 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6753 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6754 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6755 | `		}` |
|      106 |  6756 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6757 | `		if( !isValid ){` |
|        - |  6758 | `			/* Iterator exhausted */` |
|       24 |  6759 | `			pc = pInstr->iP2 - 1;` |
|        - |  6760 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6761 | `			if( pStep->pOwner ){` |
|        3 |  6762 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6763 | `			}` |
|       24 |  6764 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6765 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6766 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6767 | `		}else{` |
|        - |  6768 | `			/* Call current() to get value */` |
|       84 |  6769 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6770 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6771 | `			if( pMethod ){` |
|       84 |  6772 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6773 | `			}` |
|       84 |  6774 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6775 | `			if( pValue ){` |
|       84 |  6776 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6777 | `			}` |
|       84 |  6778 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6779 | `			/* Call key() if needed */` |
|       84 |  6780 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6781 | `				ph7_value sKey;` |
|       35 |  6782 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6783 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6784 | `				if( pMethod ){` |
|       35 |  6785 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6786 | `				}` |
|       35 |  6787 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6788 | `				if( pValue ){` |
|       35 |  6789 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6790 | `				}` |
|       35 |  6791 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6792 | `			}` |
|        - |  6793 | `		}` |
|       54 |  6794 | `	}else{` |
|       25 |  6795 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6796 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6797 | `		SyHashEntry *pEntry;` |
|        - |  6798 | `		/* Point to the next attribute */` |
|       29 |  6799 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6800 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6801 | `			/* Check access permission */` |
|       31 |  6802 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6803 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6804 | `					break; /* Access is granted */` |
|        - |  6805 | `			}` |
|        1 |  6806 | `		}` |
|       25 |  6807 | `		if( pEntry == 0 ){` |
|        - |  6808 | `			/* Clean up the mess left behind */` |
|        9 |  6809 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6810 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6811 | `				/* Break the reference with the last element */` |
|        3 |  6812 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6813 | `			}` |
|        9 |  6814 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6815 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6816 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6817 | `		}else{` |
|       17 |  6818 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6819 | `			ph7_value *pAttrValue;` |
|       17 |  6820 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6821 | `				/* Fill with the current attribute name */` |
|       17 |  6822 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6823 | `				if( pKey ){` |
|       17 |  6824 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6825 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6826 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6827 | `				}` |
|        8 |  6828 | `			}` |
|        - |  6829 | `			/* Extract attribute value */` |
|       17 |  6830 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6831 | `			if( pAttrValue ){` |
|       17 |  6832 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6833 | `					/* Pass by reference */` |
|        3 |  6834 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6835 | `					if( pEntry ){` |
|        3 |  6836 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6837 | `					}else{` |
|      ! 0 |  6838 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6839 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6840 | `					}` |
|        2 |  6841 | `				}else{` |
|        - |  6842 | `					/* Make a copy of the attribute value */` |
|       15 |  6843 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6844 | `					if( pValue ){` |
|       15 |  6845 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6846 | `					}` |
|        - |  6847 | `				}` |
|        8 |  6848 | `			}` |
|        - |  6849 | `		}` |
|        - |  6850 | `	}` |
|   187182 |  6851 | `	break;` |
|        - |  6852 | `						  }` |
|        - |  6853 | `/*` |
|        - |  6854 | ` * OP_MEMBER P1 P2` |
|        - |  6855 | ` * Load class attribute/method on the stack.` |
|        - |  6856 | ` */` |
|     3307 |  6857 | `case PH7_OP_MEMBER: {` |
|        - |  6858 | `	ph7_class_instance *pThis;` |
|        - |  6859 | `	ph7_value *pNos;` |
|        - |  6860 | `	SyString sName;` |
|     6616 |  6861 | `	if( !pInstr->iP1 ){` |
|     6390 |  6862 | `		pNos = &pTos[-1];` |
|        - |  6863 | `#ifdef UNTRUST` |
|        - |  6864 | `		if( pNos < pStack ){` |
|        - |  6865 | `			goto Abort;` |
|        - |  6866 | `		}` |
|        - |  6867 | `#endif` |
|     6390 |  6868 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6869 | `			ph7_class *pClass;` |
|        - |  6870 | `			/* Class already instantiated */` |
|     6388 |  6871 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6872 | `			/* Point to the instantiated class */` |
|     6388 |  6873 | `			pClass = pThis->pClass;` |
|        - |  6874 | `			/* Extract attribute name first */` |
|     6388 |  6875 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     6388 |  6876 | `			if( pInstr->iP2 ){` |
|        - |  6877 | `				/* Method call */` |
|      666 |  6878 | `				ph7_class_method *pMeth = 0;` |
|      666 |  6879 | `				if( sName.nByte > 0 ){` |
|        - |  6880 | `					/* Extract the target method */` |
|      666 |  6881 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      332 |  6882 | `				}` |
|      666 |  6883 | `				if( pMeth == 0 ){` |
|      ! 0 |  6884 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6885 | `						&pClass->sName,&sName` |
|        - |  6886 | `						);` |
|        - |  6887 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6888 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6889 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6890 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6891 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6892 | `				}else{` |
|        - |  6893 | `					/* Push method name on the stack */` |
|      666 |  6894 | `					PH7_MemObjRelease(pTos);` |
|      666 |  6895 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      666 |  6896 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6897 | `				}` |
|      666 |  6898 | `				pTos->nIdx = SXU32_HIGH;` |
|      334 |  6899 | `			}else{` |
|        - |  6900 | `				/* Attribute access */` |
|     5724 |  6901 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6902 | `				SyHashEntry *pEntry;` |
|        - |  6903 | `				/* Extract the target attribute */` |
|     5724 |  6904 | `				if( sName.nByte > 0 ){` |
|     5724 |  6905 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     5724 |  6906 | `					if( pEntry ){` |
|        - |  6907 | `						/* Point to the attribute value */` |
|     5722 |  6908 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2860 |  6909 | `					}` |
|     2861 |  6910 | `				}` |
|     5724 |  6911 | `				if( pObjAttr == 0 ){` |
|        - |  6912 | `					/* No such attribute,load null */` |
|        4 |  6913 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6914 | `						&pClass->sName,&sName);` |
|        - |  6915 | `					/* Call the __get magic method if available */` |
|        3 |  6916 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6917 | `				}` |
|     5724 |  6918 | `				VmPopOperand(&pTos,1);` |
|        - |  6919 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6920 | `				 * This is due to the following case:` |
|        - |  6921 | `				 *     (new TestClass())->foo;` |
|        - |  6922 | `				 */` |
|     5724 |  6923 | `				pThis->iRef++;` |
|     5724 |  6924 | `				PH7_MemObjRelease(pTos);` |
|     5724 |  6925 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     5724 |  6926 | `				if( pObjAttr ){` |
|     5722 |  6927 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6928 | `					/* Check attribute access */` |
|     5722 |  6929 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6930 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6931 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6932 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6933 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6934 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     5720 |  6935 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2897 |  6936 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       72 |  6937 | `							VmInstr *pNext = pInstr + 1;` |
|       72 |  6938 | `							int bIsLhs = 0;` |
|       72 |  6939 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       70 |  6940 | `								bIsLhs = 1;` |
|       34 |  6941 | `							}` |
|       72 |  6942 | `							if( !bIsLhs ){` |
|        3 |  6943 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6944 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6945 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6946 | `									goto Abort;` |
|        - |  6947 | `								}` |
|        - |  6948 | `								{` |
|        3 |  6949 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6950 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6951 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3307 |  6952 | `										break;` |
|        - |  6953 | `									}` |
|        - |  6954 | `								}` |
|      ! 0 |  6955 | `								goto Exception;` |
|        - |  6956 | `							}` |
|       34 |  6957 | `						}` |
|        - |  6958 | `						/* Load attribute */` |
|     5720 |  6959 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     5720 |  6960 | `						if( pValue ){` |
|     5720 |  6961 | `							if( pThis->iRef < 2 ){` |
|        - |  6962 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6963 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6964 | `								 */` |
|        7 |  6965 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6966 | `							}else{` |
|        - |  6967 | `								/* Simple load */` |
|     5714 |  6968 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6969 | `							}` |
|     5720 |  6970 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     5718 |  6971 | `								if( pThis->iRef > 1 ){` |
|        - |  6972 | `									/* Load attribute index */` |
|     5712 |  6973 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2855 |  6974 | `								}` |
|     2858 |  6975 | `							}` |
|     2859 |  6976 | `						}` |
|     2861 |  6977 | `					}else{` |
|        - |  6978 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6979 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6980 | `						char zMsg[256];` |
|      ! 0 |  6981 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6982 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6983 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6984 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6985 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6986 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6987 | `						goto Abort;` |
|        - |  6988 | `					}` |
|     2859 |  6989 | `				}` |
|        - |  6990 | `				/* Safely unreference the object */` |
|     5722 |  6991 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6992 | `			}` |
|     3194 |  6993 | `		}else{` |
|        3 |  6994 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  6995 | `			VmPopOperand(&pTos,1);` |
|        3 |  6996 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6997 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6998 | `		}` |
|     3195 |  6999 | `	}else{` |
|        - |  7000 | `		/* Static member access using class name */` |
|      228 |  7001 | `		pNos = pTos;` |
|      228 |  7002 | `		pThis = 0;` |
|      228 |  7003 | `		if( !pInstr->p3 ){` |
|      190 |  7004 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7005 | `			pNos--;` |
|        - |  7006 | `#ifdef UNTRUST` |
|        - |  7007 | `			if( pNos < pStack ){` |
|        - |  7008 | `				goto Abort;` |
|        - |  7009 | `			}` |
|        - |  7010 | `#endif` |
|       96 |  7011 | `		}else{` |
|        - |  7012 | `			/* Attribute name already computed */` |
|       40 |  7013 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7014 | `		}` |
|      228 |  7015 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7016 | `			ph7_class *pClass = 0;` |
|      228 |  7017 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7018 | `				/* Class already instantiated */` |
|        5 |  7019 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7020 | `				pClass = pThis->pClass;` |
|        5 |  7021 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7022 | `			}else{` |
|        - |  7023 | `				/* Try to extract the target class */` |
|      224 |  7024 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7025 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7026 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7027 | `					/* Handle self/static/parent keywords */` |
|      224 |  7028 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7029 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7030 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7031 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7032 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7033 | `						}` |
|      194 |  7034 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7035 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7036 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7037 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7038 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7039 | `							pClass = pSelf->pBase;` |
|       13 |  7040 | `						}` |
|       15 |  7041 | `					}else{` |
|      112 |  7042 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7043 | `					}` |
|      111 |  7044 | `				}` |
|        - |  7045 | `			}` |
|      228 |  7046 | `			if( pClass == 0 ){` |
|        - |  7047 | `				/* Undefined class */` |
|      ! 0 |  7048 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7049 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7050 | `					);` |
|      ! 0 |  7051 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7052 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7053 | `				}` |
|      ! 0 |  7054 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7055 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7056 | `			}else{` |
|      228 |  7057 | `				if( pInstr->iP2 ){` |
|        - |  7058 | `					/* Method call */` |
|       86 |  7059 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7060 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7061 | `						/* Extract the target method */` |
|       86 |  7062 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7063 | `					}` |
|       86 |  7064 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7065 | `						if( pMeth ){` |
|      ! 0 |  7066 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7067 | `								&pClass->sName,&sName` |
|        - |  7068 | `								);` |
|      ! 0 |  7069 | `						}else{` |
|      ! 0 |  7070 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7071 | `								&pClass->sName,&sName` |
|        - |  7072 | `								);` |
|        - |  7073 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7074 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7075 | `						}` |
|        - |  7076 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7077 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7078 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7079 | `						}` |
|      ! 0 |  7080 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7081 | `					}else{` |
|        - |  7082 | `						/* Push method name on the stack */` |
|       86 |  7083 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7084 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7085 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7086 | `					}` |
|       86 |  7087 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7088 | `				}else{` |
|        - |  7089 | `					/* Attribute access */` |
|      144 |  7090 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7091 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7092 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7093 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7094 | `						/* ::class returns the fully qualified class name */` |
|        - |  7095 | `						/* Pop the attribute name from the stack */` |
|       60 |  7096 | `						if( !pInstr->p3 ){` |
|       60 |  7097 | `							VmPopOperand(&pTos,1);` |
|       29 |  7098 | `						}` |
|       60 |  7099 | `						PH7_MemObjRelease(pTos);` |
|        - |  7100 | `						/* Load the class name */` |
|       60 |  7101 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7102 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7103 | `					}else{` |
|        - |  7104 | `						/* Extract the target attribute */` |
|       86 |  7105 | `						if( sName.nByte > 0 ){` |
|       86 |  7106 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7107 | `						}` |
|       86 |  7108 | `						if( pAttr == 0 ){` |
|        - |  7109 | `							/* No such attribute,load null */` |
|      ! 0 |  7110 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7111 | `								&pClass->sName,&sName);` |
|        - |  7112 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7113 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7114 | `						}` |
|        - |  7115 | `						/* Pop the attribute name from the stack */` |
|       86 |  7116 | `						if( !pInstr->p3 ){` |
|       48 |  7117 | `							VmPopOperand(&pTos,1);` |
|       23 |  7118 | `						}` |
|       86 |  7119 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7120 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7121 | `						if( pAttr ){` |
|       86 |  7122 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7123 | `								/* Access to a non static attribute */` |
|      ! 0 |  7124 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7125 | `									&pClass->sName,&pAttr->sName` |
|        - |  7126 | `									);` |
|      ! 0 |  7127 | `							}else{` |
|        - |  7128 | `								ph7_value *pValue;` |
|        - |  7129 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7130 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7131 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7132 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7133 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7134 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7135 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7136 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7137 | `										if( pS ){` |
|       28 |  7138 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7139 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7140 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7141 | `												int bIsLhs = 0;` |
|        8 |  7142 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7143 | `													bIsLhs = 1;` |
|        2 |  7144 | `												}` |
|        8 |  7145 | `												if( !bIsLhs ){` |
|        3 |  7146 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7147 | `													if( pThis ){` |
|      ! 0 |  7148 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7149 | `													}` |
|        3 |  7150 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7151 | `														goto Abort;` |
|        - |  7152 | `													}` |
|        - |  7153 | `													{` |
|        3 |  7154 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7155 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7156 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7157 | `															break;` |
|        - |  7158 | `														}` |
|        - |  7159 | `													}` |
|      ! 0 |  7160 | `													goto Exception;` |
|        - |  7161 | `												}` |
|        2 |  7162 | `											}` |
|       12 |  7163 | `										}` |
|       12 |  7164 | `									}` |
|        - |  7165 | `									/* Load the desired attribute */` |
|       80 |  7166 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7167 | `									if( pValue ){` |
|       80 |  7168 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7169 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7170 | `											/* Load index number */` |
|       38 |  7171 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7172 | `										}` |
|       39 |  7173 | `									}` |
|       41 |  7174 | `								}else{` |
|        - |  7175 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7176 | `									char zMsg[256];` |
|        5 |  7177 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7178 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7179 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7180 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7181 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7182 | `									}else{` |
|      ! 0 |  7183 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7184 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7185 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7186 | `									}` |
|        5 |  7187 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7188 | `									goto Abort;` |
|        - |  7189 | `								}` |
|        - |  7190 | `							}` |
|       39 |  7191 | `						}` |
|        - |  7192 | `					}` |
|        - |  7193 | `				}` |
|      222 |  7194 | `				if( pThis ){` |
|        - |  7195 | `					/* Safely unreference the object */` |
|        5 |  7196 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7197 | `				}` |
|        - |  7198 | `			}` |
|      112 |  7199 | `		}else{` |
|        - |  7200 | `			/* Pop operands */` |
|      ! 0 |  7201 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7202 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7203 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7204 | `			}` |
|      ! 0 |  7205 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7206 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7207 | `		}` |
|        - |  7208 | `	}` |
|     6608 |  7209 | `	break;` |
|        - |  7210 | `					}` |
|        - |  7211 | `/*` |
|        - |  7212 | ` * OP_NEW P1 * * *` |
|        - |  7213 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7214 | ` */` |
|      531 |  7215 | `case PH7_OP_NEW: {` |
|     1064 |  7216 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1064 |  7217 | `	ph7_class *pClass = 0;` |
|        - |  7218 | `	ph7_class_instance *pNew;` |
|     1064 |  7219 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7220 | `		/* Try to extract the desired class */` |
|     1595 |  7221 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1062 |  7222 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      531 |  7223 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7224 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7225 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7226 | `	}` |
|     1064 |  7227 | `	if( pClass == 0 ){` |
|        - |  7228 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7229 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7230 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7231 | `			);` |
|        - |  7232 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7233 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7234 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7235 | `			/* Pop given arguments */` |
|      ! 0 |  7236 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7237 | `		}` |
|      ! 0 |  7238 | `		goto Abort;` |
|      ! 0 |  7239 | `	}else{` |
|        - |  7240 | `		ph7_class_method *pCons;` |
|        - |  7241 | `		/* Create a new class instance */` |
|     1064 |  7242 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1064 |  7243 | `		if( pNew == 0 ){` |
|      ! 0 |  7244 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7245 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7246 | `				&pClass->sName` |
|        - |  7247 | `			);` |
|      ! 0 |  7248 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7249 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7250 | `				/* Pop given arguments */` |
|      ! 0 |  7251 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7252 | `			}` |
|      ! 0 |  7253 | `			break;` |
|        - |  7254 | `		}` |
|        - |  7255 | `		/* Check if a constructor is available */` |
|     1064 |  7256 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1064 |  7257 | `		if( pCons == 0 ){` |
|      760 |  7258 | `			SyString *pName = &pClass->sName;` |
|        - |  7259 | `			/* Check for a constructor with the same base class name */` |
|      760 |  7260 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      379 |  7261 | `		}` |
|     1064 |  7262 | `		if( pCons ){` |
|        - |  7263 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7264 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7265 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7266 | `			 * (including variadic string-key packing). */` |
|      306 |  7267 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      306 |  7268 | `			SySetReset(&aArg);` |
|      600 |  7269 | `			while( pArg < pTos ){` |
|      296 |  7270 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      296 |  7271 | `				pArg++;` |
|        2 |  7272 | `			}` |
|      306 |  7273 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7274 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7275 | `				sxu32 n;` |
|       61 |  7276 | `				n = SySetUsed(&aArg);` |
|        - |  7277 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7278 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7279 | `				 * after resolution). */` |
|      109 |  7280 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  7281 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  7282 | `					if( pFuncArg ){` |
|       49 |  7283 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7284 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7285 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7286 | `						}` |
|       24 |  7287 | `					}` |
|       49 |  7288 | `					n++;` |
|        1 |  7289 | `				}` |
|       30 |  7290 | `			}` |
|      306 |  7291 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  7292 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      306 |  7293 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  7294 | `				pNew->iRef = 1;` |
|      ! 0 |  7295 | `			}` |
|      152 |  7296 | `		}` |
|     1064 |  7297 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7298 | `			/* Pop given arguments */` |
|      242 |  7299 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      120 |  7300 | `		}` |
|     1064 |  7301 | `		PH7_MemObjRelease(pTos);` |
|     1064 |  7302 | `		pTos->x.pOther = pNew;` |
|     1064 |  7303 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7304 | `	}` |
|     1064 |  7305 | `	break;` |
|        - |  7306 | `				 }` |
|        - |  7307 | `/*` |
|        - |  7308 | ` * OP_CLONE * * *` |
|        - |  7309 | ` * Perfome a clone operation.` |
|        - |  7310 | ` */` |
|       24 |  7311 | `case PH7_OP_CLONE: {` |
|        - |  7312 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7313 | `#ifdef UNTRUST` |
|        - |  7314 | `	if( pTos < pStack ){` |
|        - |  7315 | `		goto Abort;` |
|        - |  7316 | `	}` |
|        - |  7317 | `#endif` |
|        - |  7318 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7319 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7320 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7321 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7322 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7323 | `		break;` |
|        - |  7324 | `	}` |
|        - |  7325 | `	/* Point to the source */` |
|       46 |  7326 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7327 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7328 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7329 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7330 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7331 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7332 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7333 | `		break;` |
|        - |  7334 | `	}` |
|        - |  7335 | `	/* Perform the clone operation */` |
|       46 |  7336 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7337 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7338 | `	if( pClone == 0 ){` |
|      ! 0 |  7339 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7340 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7341 | `	}else{` |
|        - |  7342 | `		/* Load the cloned object */` |
|       46 |  7343 | `		pTos->x.pOther = pClone;` |
|       46 |  7344 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7345 | `	}` |
|       46 |  7346 | `	break;` |
|        - |  7347 | `				   }` |
|        - |  7348 | `/*` |
|        - |  7349 | ` * OP_SWITCH * * P3` |
|        - |  7350 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7351 | ` */` |
|       26 |  7352 | `case PH7_OP_SWITCH: {` |
|       54 |  7353 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7354 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7355 | `	ph7_value sValue,sCaseValue;` |
|        - |  7356 | `	sxu32 n,nEntry;` |
|        - |  7357 | `#ifdef UNTRUST` |
|        - |  7358 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7359 | `		goto Abort;` |
|        - |  7360 | `	}` |
|        - |  7361 | `#endif` |
|        - |  7362 | `	/* Point to the case table  */` |
|       54 |  7363 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7364 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7365 | `	/* Select the appropriate case block to execute */` |
|       54 |  7366 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7367 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7368 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7369 | `		pCase = &aCase[n];` |
|      130 |  7370 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7371 | `		/* Execute the case expression first */` |
|      130 |  7372 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7373 | `		/* Compare the two expression */` |
|      130 |  7374 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7375 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7376 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7377 | `		if( rc == 0 ){` |
|        - |  7378 | `			/* Value match,jump to this block */` |
|       52 |  7379 | `			pc = pCase->nStart - 1;` |
|       52 |  7380 | `			break;` |
|        - |  7381 | `		}` |
|       41 |  7382 | `	}` |
|       54 |  7383 | `	VmPopOperand(&pTos,1);` |
|       54 |  7384 | `	if( n >= nEntry ){` |
|        - |  7385 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7386 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7387 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7388 | `		}else{` |
|        - |  7389 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7390 | `			pc = pSwitch->nOut - 1;` |
|        - |  7391 | `		}` |
|        1 |  7392 | `	}` |
|       54 |  7393 | `	break;` |
|        - |  7394 | `					}` |
|        - |  7395 | `/*` |
|        - |  7396 | ` * OP_MATCH * * P3` |
|        - |  7397 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7398 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7399 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7400 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7401 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7402 | ` */` |
|       54 |  7403 | `case PH7_OP_MATCH: {` |
|      110 |  7404 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7405 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7406 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7407 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7408 | `	int matched = 0;` |
|        - |  7409 | `#ifdef UNTRUST` |
|        - |  7410 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7411 | `		goto Abort;` |
|        - |  7412 | `	}` |
|        - |  7413 | `#endif` |
|      110 |  7414 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7415 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7416 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7417 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7418 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7419 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7420 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7421 | `		pArm = &aArm[i];` |
|      240 |  7422 | `		if( pArm->bDefault ){` |
|       13 |  7423 | `			pDefault = pArm;` |
|       13 |  7424 | `			continue;` |
|        - |  7425 | `		}` |
|      228 |  7426 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7427 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7428 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7429 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7430 | `				continue;` |
|        - |  7431 | `			}` |
|      260 |  7432 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7433 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7434 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7435 | `			if( rc == 0 ){` |
|       93 |  7436 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7437 | `				matched = 1;` |
|       93 |  7438 | `				break;` |
|        - |  7439 | `			}` |
|       85 |  7440 | `		}` |
|      115 |  7441 | `	}` |
|      110 |  7442 | `	if( !matched && pDefault ){` |
|       13 |  7443 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7444 | `		matched = 1;` |
|        6 |  7445 | `	}` |
|      110 |  7446 | `	if( !matched ){` |
|        5 |  7447 | `		const char *zType = "unknown";` |
|        - |  7448 | `		char zMsg[128];` |
|        - |  7449 | `		sxu32 nMsg;` |
|        5 |  7450 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7451 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7452 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7453 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7454 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7455 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7456 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7457 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7458 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7459 | `		default: break;` |
|        - |  7460 | `		}` |
|        7 |  7461 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7462 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7463 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7464 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7465 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7466 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7467 | `		goto Abort;` |
|        - |  7468 | `	}` |
|      105 |  7469 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7470 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7471 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7472 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7473 | `	break;` |
|        - |  7474 | `					}` |
|        - |  7475 | `/*` |
|        - |  7476 | ` * OP_YIELD P1 P2 *` |
|        - |  7477 | ` *  Yield a value from a generator function.` |
|        - |  7478 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7479 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7480 | ` */` |
|       34 |  7481 | `case PH7_OP_YIELD: {` |
|        - |  7482 | `	ph7_generator *pGen;` |
|       70 |  7483 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7484 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7485 | `		goto Abort;` |
|        - |  7486 | `	}` |
|       70 |  7487 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7488 | `	if( pInstr->iP2 ){` |
|        - |  7489 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7490 | `#ifdef UNTRUST` |
|        - |  7491 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7492 | `#endif` |
|        7 |  7493 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7494 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7495 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7496 | `		VmPopOperand(&pTos, 1);` |
|        - |  7497 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7498 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7499 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7500 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7501 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7502 | `			}` |
|        1 |  7503 | `		}` |
|       67 |  7504 | `	}else if( pInstr->iP1 ){` |
|        - |  7505 | `		/* yield $value */` |
|        - |  7506 | `#ifdef UNTRUST` |
|        - |  7507 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7508 | `#endif` |
|       64 |  7509 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7510 | `		VmPopOperand(&pTos, 1);` |
|        - |  7511 | `		/* Auto-increment key */` |
|       64 |  7512 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7513 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7514 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7515 | `	}else{` |
|        - |  7516 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7517 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7518 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7519 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7520 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7521 | `	}` |
|        - |  7522 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7523 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7524 | `	goto Suspend;` |
|        - |  7525 |  |
|        - |  7526 | `/*` |
|        - |  7527 | ` * OP_CALL P1 * *` |
|        - |  7528 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7529 | ` *  function on the stack.` |
|        - |  7530 | ` */` |
|   333503 |  7531 | `case PH7_OP_CALL: {` |
|   667052 |  7532 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7533 | `	ph7_value *pArg;` |
|   667052 |  7534 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   667052 |  7535 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7536 | `	SyHashEntry *pEntry;` |
|        - |  7537 | `	SyString sName;` |
|        - |  7538 | `	/* Extract function name */` |
|   667052 |  7539 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7540 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7541 | `			ph7_value sResult;` |
|      ! 0 |  7542 | `			SySetReset(&aArg);` |
|      ! 0 |  7543 | `			while( pArg < pTos ){` |
|      ! 0 |  7544 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7545 | `				pArg++;` |
|      ! 0 |  7546 | `			}` |
|      ! 0 |  7547 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7548 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7549 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7550 | `			SySetReset(&aArg);` |
|        - |  7551 | `			/* Pop given arguments */` |
|      ! 0 |  7552 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7553 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7554 | `			}` |
|        - |  7555 | `			/* Copy result */` |
|      ! 0 |  7556 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7557 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7558 | `		}else{` |
|        3 |  7559 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7560 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7561 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7562 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7563 | `			}else{` |
|        - |  7564 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7565 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7566 | `			}` |
|        - |  7567 | `			/* Pop given arguments */` |
|        3 |  7568 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7569 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7570 | `			}` |
|        - |  7571 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7572 | `			PH7_MemObjRelease(pTos);` |
|        - |  7573 | `		}` |
|   333217 |  7574 | `		break;` |
|        - |  7575 | `	}` |
|   667050 |  7576 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7577 | `	/* Check for a compiled function first.` |
|        - |  7578 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7579 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   667050 |  7580 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7581 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7582 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7583 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7584 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7585 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7586 | `	{` |
|   667050 |  7587 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   667050 |  7588 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7589 | `		const char *zFunc;` |
|        - |  7590 | `		const char *zEnd;` |
|        - |  7591 | `		const char *z;` |
|        - |  7592 | `		SyString sGlobal;` |
|       22 |  7593 | `		zFunc = sName.zString;` |
|       22 |  7594 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7595 | `		z = zEnd;` |
|        - |  7596 | `		/* Find last namespace separator */` |
|      194 |  7597 | `		while( z > zFunc ){` |
|      194 |  7598 | `			if( z[-1] == '\\' ){` |
|       22 |  7599 | `				break;` |
|        - |  7600 | `			}` |
|      174 |  7601 | `			z--;` |
|        2 |  7602 | `		}` |
|       22 |  7603 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7604 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  7605 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  7606 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  7607 | `		}` |
|       10 |  7608 | `	}` |
|        - |  7609 | `	} /* end VmCallArgMap namespace scope */` |
|   667050 |  7610 | `	if( pEntry ){` |
|        - |  7611 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7612 | `		ph7_class_instance *pThis;` |
|        - |  7613 | `		ph7_value *pFrameStack;` |
|        - |  7614 | `		ph7_vm_func *pVmFunc;` |
|        - |  7615 | `		ph7_class *pSelf;` |
|        - |  7616 | `		VmFrame *pFrame;` |
|        - |  7617 | `		ph7_value *pObj;` |
|        - |  7618 | `		VmSlot sArg;` |
|        - |  7619 | `		sxu32 n;` |
|        - |  7620 | `		/* initialize fields */` |
|    16714 |  7621 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    16714 |  7622 | `		pThis = 0;` |
|    16714 |  7623 | `		pSelf = 0;` |
|    16714 |  7624 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7625 | `			ph7_class_method *pMeth;` |
|        - |  7626 | `			/* Class method call */` |
|     2590 |  7627 | `			ph7_value *pTarget = &pTos[-1];` |
|     2590 |  7628 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7629 | `				/* Extract the 'this' pointer */` |
|     2590 |  7630 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7631 | `					/* Instance already loaded */` |
|     2500 |  7632 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2500 |  7633 | `					pThis->iRef++;` |
|     2500 |  7634 | `					pSelf = pThis->pClass;` |
|     1249 |  7635 | `				}` |
|     2590 |  7636 | `				if( pSelf == 0 ){` |
|       92 |  7637 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7638 | `						/* "Late Static Binding" class name */` |
|      128 |  7639 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  7640 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  7641 | `					}` |
|       92 |  7642 | `					if( pSelf == 0 ){` |
|       21 |  7643 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  7644 | `					}` |
|       45 |  7645 | `				}` |
|     2590 |  7646 | `				if( pThis == 0  ){` |
|       92 |  7647 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  7648 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  7649 | `					if( pFrameLocal->pParent ){` |
|        - |  7650 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  7651 | `						pThis = pFrameLocal->pThis;` |
|       66 |  7652 | `						if( pThis ){` |
|       21 |  7653 | `							pThis->iRef++;` |
|       10 |  7654 | `						}` |
|       32 |  7655 | `					}` |
|       45 |  7656 | `				}` |
|     2590 |  7657 | `				VmPopOperand(&pTos,1);` |
|     2590 |  7658 | `				PH7_MemObjRelease(pTos);` |
|        - |  7659 | `				/* Synchronize pointers */` |
|     2590 |  7660 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7661 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7662 | `				 * user have already computed the random generated unique class method name` |
|        - |  7663 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7664 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7665 | `				 */` |
|     2590 |  7666 | `				while( pArg < pStack ){` |
|      ! 0 |  7667 | `					pArg++;` |
|      ! 0 |  7668 | `				}` |
|     2590 |  7669 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7670 | `					/* Check if the call is allowed */` |
|     2590 |  7671 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2590 |  7672 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7673 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7674 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7675 | `							char zMsg[256];` |
|      ! 0 |  7676 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7677 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7678 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7679 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7680 | `							/* Pop given arguments */` |
|      ! 0 |  7681 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7682 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7683 | `							}` |
|      ! 0 |  7684 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7685 | `							goto Abort;` |
|        - |  7686 | `						}` |
|        6 |  7687 | `					}` |
|     1294 |  7688 | `				}` |
|     1294 |  7689 | `			}` |
|     1294 |  7690 | `		}` |
|        - |  7691 | `		/* Check The recursion limit */` |
|    16714 |  7692 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7693 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7694 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7695 | `				&pVmFunc->sName);` |
|        - |  7696 | `			/* Pop given arguments */` |
|        3 |  7697 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7698 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7699 | `			}` |
|        - |  7700 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7701 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7702 | `			break;` |
|        - |  7703 | `		}` |
|    16712 |  7704 | `		if( pVmFunc->pNextName ){` |
|        - |  7705 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7706 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7707 | `		}` |
|    16712 |  7708 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7709 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7710 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7711 | `			ph7_generator *pGenerator;` |
|        - |  7712 | `			ph7_class_instance *pGenObj;` |
|        - |  7713 | `			ph7_value *pCtxAttr;` |
|        - |  7714 | `			SyString sAttrName;` |
|        - |  7715 | `			ph7_value **apCallArgs;` |
|        - |  7716 | `			int nGenArgs, iArg;` |
|        - |  7717 | `			/* Collect arguments from the operand stack */` |
|       24 |  7718 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7719 | `			apCallArgs = 0;` |
|       24 |  7720 | `			if( nGenArgs > 0 ){` |
|       14 |  7721 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7722 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7723 | `				if( apCallArgs == 0 ){` |
|        - |  7724 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7725 | `					nGenArgs = 0;` |
|      ! 0 |  7726 | `				}else{` |
|       10 |  7727 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7728 | `					int didReorder = 0;` |
|       10 |  7729 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7730 | `						/* Named-argument reordering for generator */` |
|        5 |  7731 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7732 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7733 | `						sxu32 nNV = nF;` |
|        5 |  7734 | `						sxi32 iVIdx = -1;` |
|        - |  7735 | `						sxi32 *aGSlot;` |
|        - |  7736 | `						sxu8 *aGUsed;` |
|        - |  7737 | `						sxu32 gi;` |
|       13 |  7738 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7739 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7740 | `						}` |
|        7 |  7741 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7742 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7743 | `						if( aGSlot ){` |
|        5 |  7744 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7745 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7746 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7747 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7748 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7749 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7750 | `								goto Abort;` |
|        - |  7751 | `							}` |
|        - |  7752 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7753 | `							 * append overflow (variadic / positional beyond` |
|        - |  7754 | `							 * formals) so downstream sees every argument. */` |
|        - |  7755 | `							{` |
|        5 |  7756 | `								int nOut = 0;` |
|       13 |  7757 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7758 | `									sxu32 gj;` |
|       13 |  7759 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7760 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7761 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7762 | `											break;` |
|        - |  7763 | `										}` |
|        3 |  7764 | `									}` |
|        5 |  7765 | `								}` |
|       13 |  7766 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7767 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7768 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7769 | `									}` |
|        5 |  7770 | `								}` |
|        5 |  7771 | `								nGenArgs = nOut;` |
|        - |  7772 | `							}` |
|        5 |  7773 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7774 | `							didReorder = 1;` |
|        2 |  7775 | `						}` |
|        - |  7776 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7777 | `						 * positional fill below — preserves arg order rather` |
|        - |  7778 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7779 | `					}` |
|       10 |  7780 | `					if( !didReorder ){` |
|       12 |  7781 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7782 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7783 | `						}` |
|        2 |  7784 | `					}` |
|        - |  7785 | `				}` |
|        4 |  7786 | `			}` |
|        - |  7787 | `			/* Create execution context and generator wrapper */` |
|       24 |  7788 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7789 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7790 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7791 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7792 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7793 | `				break;` |
|        - |  7794 | `			}` |
|       24 |  7795 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7796 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7797 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7798 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7799 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7800 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7801 | `				break;` |
|        - |  7802 | `			}` |
|        - |  7803 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7804 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7805 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7806 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7807 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7808 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7809 | `			if( apCallArgs ){` |
|       10 |  7810 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7811 | `			}` |
|       24 |  7812 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7813 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7814 | `				if( pThis ){` |
|      ! 0 |  7815 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7816 | `				}` |
|      ! 0 |  7817 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7818 | `					goto Abort;` |
|        - |  7819 | `				}` |
|      ! 0 |  7820 | `				break;` |
|        - |  7821 | `			}` |
|        - |  7822 | `			/* Create Generator class instance */` |
|       24 |  7823 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7824 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7825 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7826 | `				break;` |
|        - |  7827 | `			}` |
|        - |  7828 | `			/* Store generator in __ctx attribute */` |
|       24 |  7829 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7830 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7831 | `			if( pCtxAttr ){` |
|       24 |  7832 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  7833 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  7834 | `			}` |
|        - |  7835 | `			/* Pop args and function name, push Generator object */` |
|       24 |  7836 | `			PH7_MemObjRelease(pTos);` |
|       24 |  7837 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  7838 | `			pTos->x.pOther = pGenObj;` |
|       24 |  7839 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  7840 | `			pGenObj->iRef++;` |
|       24 |  7841 | `			if( pThis ){` |
|      ! 0 |  7842 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7843 | `			}` |
|       24 |  7844 | `			break;` |
|        - |  7845 | `		}` |
|        - |  7846 | `		/* Extract the formal argument set */` |
|    16690 |  7847 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7848 | `		/* Create a new VM frame  */` |
|    16690 |  7849 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    16690 |  7850 | `		if( rc != SXRET_OK ){` |
|        - |  7851 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7852 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7853 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7854 | `				&pVmFunc->sName);` |
|        - |  7855 | `			/* Pop given arguments */` |
|      ! 0 |  7856 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7857 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7858 | `			}` |
|        - |  7859 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7860 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7861 | `			break;` |
|        - |  7862 | `		}` |
|    16690 |  7863 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7864 | `			/* Install the '$this' variable */` |
|        - |  7865 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2518 |  7866 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2518 |  7867 | `			if( pObj ){` |
|        - |  7868 | `				/* Reflect the change */` |
|     2518 |  7869 | `				pObj->x.pOther = pThis;` |
|     2518 |  7870 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1258 |  7871 | `			}` |
|     1258 |  7872 | `		}` |
|    16690 |  7873 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7874 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7875 | `			/* Install static variables */` |
|      ! 0 |  7876 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7877 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7878 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7879 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7880 | `					/* Initialize the static variables */` |
|      ! 0 |  7881 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7882 | `					if( pObj ){` |
|        - |  7883 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7884 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7885 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7886 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7887 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7888 | `						}` |
|      ! 0 |  7889 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7890 | `					}else{` |
|      ! 0 |  7891 | `						continue;` |
|        - |  7892 | `					}` |
|      ! 0 |  7893 | `				}` |
|        - |  7894 | `				/* Install in the current frame */` |
|      ! 0 |  7895 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7896 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7897 | `			}` |
|      ! 0 |  7898 | `		}` |
|        - |  7899 | `		/* Push arguments in the local frame */` |
|        - |  7900 | `		{` |
|    16690 |  7901 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  7902 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  7903 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    16690 |  7904 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    16690 |  7905 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  7906 | `			/* ============================================================` |
|        - |  7907 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  7908 | `			 *` |
|        - |  7909 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  7910 | `			 * or position, then install them in the frame.` |
|        - |  7911 | `			 * ============================================================ */` |
|       90 |  7912 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  7913 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  7914 | `			sxi32 iVariadicIdx = -1;` |
|        - |  7915 | `			sxu32 nNonVariadic;` |
|        - |  7916 | `			sxi32 *aSlot;` |
|        - |  7917 | `			sxu8  *aUsed;` |
|        - |  7918 | `			sxu32 i;` |
|        - |  7919 | `			/* Find variadic parameter index */` |
|      274 |  7920 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  7921 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  7922 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  7923 | `					break;` |
|        - |  7924 | `				}` |
|       94 |  7925 | `			}` |
|       90 |  7926 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  7927 | `			/* Allocate mapping arrays */` |
|      134 |  7928 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  7929 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  7930 | `			if( aSlot == 0 ){` |
|      ! 0 |  7931 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  7932 | `				goto Abort;` |
|        - |  7933 | `			}` |
|       90 |  7934 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  7935 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  7936 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  7937 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  7938 | `			if( rc == PH7_ABORT ){` |
|        7 |  7939 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  7940 | `				goto Abort;` |
|        - |  7941 | `			}` |
|        - |  7942 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  7943 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  7944 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  7945 | `				sxi32 iSrc = -1;` |
|      291 |  7946 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  7947 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  7948 | `						iSrc = (sxi32)i;` |
|      157 |  7949 | `						break;` |
|        - |  7950 | `					}` |
|       59 |  7951 | `				}` |
|      175 |  7952 | `				if( iSrc >= 0 ){` |
|        - |  7953 | `					/* Argument was provided — install with type checking */` |
|      157 |  7954 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  7955 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  7956 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  7957 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  7958 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  7959 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7960 | `					}` |
|        - |  7961 | `					/* Type checking: union types */` |
|      157 |  7962 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  7963 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  7964 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  7965 | `							bCallIsStrict);` |
|       13 |  7966 | `						if( rcU != SXRET_OK ){` |
|        - |  7967 | `							const char *zGiven;` |
|      ! 0 |  7968 | `							const char *zExpected = "union";` |
|        - |  7969 | `							char zBuf[128];` |
|        - |  7970 | `							char zTypeBuf[128];` |
|      ! 0 |  7971 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7972 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  7973 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7974 | `								zGiven = "null";` |
|      ! 0 |  7975 | `							}else{` |
|      ! 0 |  7976 | `								zGiven = ph7_type_name(pVal);` |
|        - |  7977 | `							}` |
|      ! 0 |  7978 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  7979 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  7980 | `							}` |
|      ! 0 |  7981 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7982 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  7983 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7984 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7985 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  7986 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7987 | `							pFrameStack = 0;` |
|      ! 0 |  7988 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  7989 | `							goto SkipFuncBody;` |
|        - |  7990 | `						}` |
|      159 |  7991 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  7992 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  7993 | `						/* Scalar/class type checking */` |
|       17 |  7994 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  7995 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  7996 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  7997 | `							if( pClass ){` |
|      ! 0 |  7998 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7999 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8000 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8001 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8002 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8003 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8004 | `									}` |
|      ! 0 |  8005 | `								}else{` |
|      ! 0 |  8006 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8007 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8008 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8009 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8010 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8011 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8012 | `									}` |
|        - |  8013 | `								}` |
|      ! 0 |  8014 | `							}` |
|       17 |  8015 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8016 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8017 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8018 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8019 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8020 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8021 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8022 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8023 | `								pFrameStack = 0;` |
|      ! 0 |  8024 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8025 | `								goto SkipFuncBody;` |
|        7 |  8026 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8027 | `								char zTypeBuf[128];` |
|      ! 0 |  8028 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8029 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8030 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8031 | `									ph7_type_name(pVal));` |
|      ! 0 |  8032 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8033 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8034 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8035 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8036 | `								pFrameStack = 0;` |
|      ! 0 |  8037 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8038 | `								goto SkipFuncBody;` |
|        - |  8039 | `							}` |
|        3 |  8040 | `						}` |
|        8 |  8041 | `					}` |
|        - |  8042 | `					/* Install: by reference or by value */` |
|      157 |  8043 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8044 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8045 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8046 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8047 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8048 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8049 | `							}` |
|      ! 0 |  8050 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8051 | `						}else{` |
|        7 |  8052 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8053 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8054 | `							if( pRefEntry == 0 ){` |
|        7 |  8055 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8056 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8057 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8058 | `								sArg.pUserData = 0;` |
|        5 |  8059 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8060 | `							}` |
|        5 |  8061 | `							pObj = 0;` |
|        - |  8062 | `						}` |
|        3 |  8063 | `					}else{` |
|      153 |  8064 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8065 | `					}` |
|      157 |  8066 | `					if( pObj ){` |
|      153 |  8067 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  8068 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  8069 | `						sArg.pUserData = 0;` |
|      153 |  8070 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  8071 | `					}` |
|       79 |  8072 | `				}else{` |
|        - |  8073 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8074 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8075 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8076 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8077 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8078 | `						if( pObj ){` |
|       19 |  8079 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8080 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8081 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8082 | `							sArg.pUserData = 0;` |
|       19 |  8083 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8084 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8085 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8086 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8087 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8088 | `							}` |
|        9 |  8089 | `						}` |
|        9 |  8090 | `					}` |
|        - |  8091 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8092 | `				}` |
|       88 |  8093 | `			}` |
|        - |  8094 | `			/* Handle variadic parameter */` |
|       83 |  8095 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8096 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8097 | `				if( pObj ){` |
|        9 |  8098 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8099 | `					{` |
|        9 |  8100 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8101 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8102 | `							if( aSlot[i] == -1 ){` |
|       16 |  8103 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8104 | `									/* Named variadic entry: insert with string key */` |
|        - |  8105 | `									ph7_value sKey;` |
|       11 |  8106 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8107 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8108 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8109 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8110 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8111 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8112 | `								}else{` |
|        - |  8113 | `									/* Positional variadic entry */` |
|      ! 0 |  8114 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8115 | `								}` |
|        5 |  8116 | `							}` |
|       12 |  8117 | `						}` |
|        - |  8118 | `					}` |
|        9 |  8119 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8120 | `					sArg.pUserData = 0;` |
|        9 |  8121 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8122 | `				}` |
|        5 |  8123 | `			}else{` |
|        - |  8124 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8125 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8126 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8127 | `				 * the positional-only path's behavior. */` |
|       75 |  8128 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  8129 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  8130 | `					if( aSlot[i] == -2 ){` |
|        - |  8131 | `						char zAnonBuf[32];` |
|        - |  8132 | `						SyString sAnonName;` |
|      ! 0 |  8133 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8134 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8135 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8136 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8137 | `						if( pObj ){` |
|      ! 0 |  8138 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8139 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8140 | `							sArg.pUserData = 0;` |
|      ! 0 |  8141 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8142 | `						}` |
|      ! 0 |  8143 | `						nAnon++;` |
|      ! 0 |  8144 | `					}` |
|       73 |  8145 | `				}` |
|        - |  8146 | `			}` |
|        - |  8147 | `			/* Release all stack arguments */` |
|      249 |  8148 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  8149 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  8150 | `			}` |
|       83 |  8151 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8152 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  8153 | `			n = nFormal;` |
|       42 |  8154 | `		}else{` |
|        - |  8155 | `		/* ============================================================` |
|        - |  8156 | `		 * Positional-only matching path (original)` |
|        - |  8157 | `		 * ============================================================ */` |
|    16602 |  8158 | `		n = 0;` |
|    44552 |  8159 | `		while( pArg < pTos ){` |
|    28018 |  8160 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8161 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  8162 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  8163 | `				if( pObj ){` |
|        - |  8164 | `					/* Initialize as empty array */` |
|       36 |  8165 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8166 | `					{` |
|       36 |  8167 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  8168 | `						while( pArg < pTos ){` |
|        - |  8169 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8170 | `							 *` |
|        - |  8171 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8172 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8173 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8174 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8175 | `							 * fixing both wants a separate counter for elements` |
|        - |  8176 | `							 * already packed into the variadic array. */` |
|      104 |  8177 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8178 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8179 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8180 | `									bCallIsStrict);` |
|       16 |  8181 | `								if( rcU != SXRET_OK ){` |
|        - |  8182 | `									const char *zGiven;` |
|        3 |  8183 | `									const char *zExpected = "union";` |
|        - |  8184 | `									char zBuf[128];` |
|        - |  8185 | `									char zTypeBuf[128];` |
|        3 |  8186 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8187 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8188 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8189 | `										zGiven = "null";` |
|      ! 0 |  8190 | `									}else{` |
|        3 |  8191 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8192 | `									}` |
|        3 |  8193 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8194 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8195 | `									}` |
|        4 |  8196 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8197 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8198 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8199 | `										goto Abort;` |
|        - |  8200 | `									}` |
|        3 |  8201 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8202 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8203 | `									pFrameStack = 0;` |
|        3 |  8204 | `									rc = PH7_EXCEPTION;` |
|        3 |  8205 | `									goto SkipFuncBody;` |
|        - |  8206 | `								}` |
|       14 |  8207 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8208 | `								pArg++;` |
|       14 |  8209 | `								continue;` |
|        - |  8210 | `							}` |
|        - |  8211 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8212 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  8213 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8214 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8215 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8216 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8217 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8218 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8219 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8220 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8221 | `										goto Abort;` |
|        - |  8222 | `									}` |
|        - |  8223 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8224 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8225 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8226 | `									pFrameStack = 0;` |
|      ! 0 |  8227 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8228 | `									goto SkipFuncBody;` |
|       13 |  8229 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8230 | `									char zTypeBuf[128];` |
|      ! 0 |  8231 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8232 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8233 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8234 | `										ph7_type_name(pArg));` |
|      ! 0 |  8235 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8236 | `										goto Abort;` |
|        - |  8237 | `									}` |
|      ! 0 |  8238 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8239 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8240 | `									pFrameStack = 0;` |
|      ! 0 |  8241 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8242 | `									goto SkipFuncBody;` |
|        - |  8243 | `								}` |
|        6 |  8244 | `							}` |
|       90 |  8245 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  8246 | `							pArg++;` |
|        2 |  8247 | `						}` |
|        - |  8248 | `					}` |
|       34 |  8249 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  8250 | `					sArg.pUserData = 0;` |
|       34 |  8251 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  8252 | `				}` |
|       34 |  8253 | `				break; /* All remaining args consumed */` |
|        - |  8254 | `			}` |
|    27984 |  8255 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    27824 |  8256 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       33 |  8257 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  8258 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  8259 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  8260 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8261 | `						goto Abort;` |
|        - |  8262 | `					}` |
|      ! 0 |  8263 | `				}` |
|        - |  8264 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    27826 |  8265 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  8266 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  8267 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  8268 | `						bCallIsStrict);` |
|       60 |  8269 | `					if( rcU != SXRET_OK ){` |
|        - |  8270 | `						const char *zGiven;` |
|       19 |  8271 | `						const char *zExpected = "union";` |
|        - |  8272 | `						char zBuf[128];` |
|        - |  8273 | `						char zTypeBuf[128];` |
|       19 |  8274 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  8275 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  8276 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  8277 | `							zGiven = "null";` |
|        5 |  8278 | `						}else{` |
|        5 |  8279 | `							zGiven = ph7_type_name(pArg);` |
|        - |  8280 | `						}` |
|       19 |  8281 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  8282 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  8283 | `						}` |
|       28 |  8284 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  8285 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  8286 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  8287 | `							goto Abort;` |
|        - |  8288 | `						}` |
|       19 |  8289 | `						PH7_MemObjRelease(pTos);` |
|       19 |  8290 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  8291 | `						pFrameStack = 0;` |
|       19 |  8292 | `						rc = PH7_EXCEPTION;` |
|       19 |  8293 | `						goto SkipFuncBody;` |
|        - |  8294 | `					}` |
|       21 |  8295 | `				}else` |
|        - |  8296 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  8297 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    27792 |  8298 | `				if( aFormalArg[n].nType > 0` |
|    14539 |  8299 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1284 |  8300 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  8301 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  8302 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  8303 | `						ph7_class *pClass;` |
|        - |  8304 | `						/* Try to extract the desired class */` |
|       26 |  8305 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  8306 | `						if( pClass ){` |
|       22 |  8307 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8308 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8309 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8310 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8311 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8312 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8313 | `								}` |
|      ! 0 |  8314 | `							}else{` |
|        - |  8315 | `								/* reuse pThis declared in outer scope */` |
|       22 |  8316 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  8317 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  8318 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  8319 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8320 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8321 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8322 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8323 | `								}` |
|        - |  8324 | `							}` |
|       12 |  8325 | `						}` |
|     1272 |  8326 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  8327 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8328 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  8329 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  8330 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  8331 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8332 | `								goto Abort;` |
|        - |  8333 | `							}` |
|        - |  8334 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  8335 | `							PH7_MemObjRelease(pTos);` |
|       11 |  8336 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  8337 | `							pFrameStack = 0;` |
|       11 |  8338 | `							rc = PH7_EXCEPTION;` |
|       11 |  8339 | `							goto SkipFuncBody;` |
|       14 |  8340 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8341 | `							char zTypeBuf[128];` |
|        7 |  8342 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  8343 | `								&aFormalArg[n].sName,` |
|        4 |  8344 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  8345 | `								ph7_type_name(pArg));` |
|        5 |  8346 | `							if( rc == PH7_ABORT ){` |
|        5 |  8347 | `								goto Abort;` |
|        - |  8348 | `							}` |
|      ! 0 |  8349 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8350 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8351 | `							pFrameStack = 0;` |
|      ! 0 |  8352 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8353 | `							goto SkipFuncBody;` |
|        - |  8354 | `						}` |
|        4 |  8355 | `					}` |
|      634 |  8356 | `				}` |
|    27794 |  8357 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8358 | `					/* Pass by reference */` |
|       54 |  8359 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8360 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8361 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8362 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8363 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8364 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8365 | `						}` |
|        - |  8366 | `						/* Switch to pass by value */` |
|      ! 0 |  8367 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8368 | `					}else{` |
|        - |  8369 | `						SyHashEntry *pRefEntry;` |
|        - |  8370 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  8371 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  8372 | `						if( pRefEntry == 0 ){` |
|       80 |  8373 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  8374 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  8375 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  8376 | `							sArg.pUserData = 0;` |
|       54 |  8377 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  8378 | `						}` |
|       54 |  8379 | `						pObj = 0;` |
|        - |  8380 | `					}` |
|       28 |  8381 | `				}else{` |
|        - |  8382 | `					/* Pass by value,make a copy of the given argument */` |
|    27742 |  8383 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8384 | `				}` |
|    13898 |  8385 | `			}else{` |
|        - |  8386 | `				char zName[32];` |
|        - |  8387 | `				SyString sArgName;` |
|        - |  8388 | `				/* Set a dummy name */` |
|      160 |  8389 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      160 |  8390 | `				sArgName.zString = zName;` |
|        - |  8391 | `				/* Annonymous argument */` |
|      160 |  8392 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8393 | `			}` |
|    27952 |  8394 | `			if( pObj ){` |
|    27900 |  8395 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8396 | `				/* Insert argument index  */` |
|    27900 |  8397 | `				sArg.nIdx = pObj->nIdx;` |
|    27900 |  8398 | `				sArg.pUserData = 0;` |
|    27900 |  8399 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    13949 |  8400 | `			}` |
|    27952 |  8401 | `			PH7_MemObjRelease(pArg);` |
|    27952 |  8402 | `			pArg++;` |
|    27952 |  8403 | `			++n;` |
|        2 |  8404 | `		}` |
|        - |  8405 | `		} /* end named vs positional branch */` |
|        - |  8406 | `		/* Set up closure environment */` |
|    16650 |  8407 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8408 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8409 | `			ph7_value *pValue;` |
|        - |  8410 | `			sxu32 iEnv;` |
|      115 |  8411 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      295 |  8412 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      181 |  8413 | `				pEnv = &aEnv[iEnv];` |
|      181 |  8414 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8415 | `					/* Do not install null value */` |
|      109 |  8416 | `					continue;` |
|        - |  8417 | `				}` |
|       73 |  8418 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  8419 | `				if( pValue == 0 ){` |
|      ! 0 |  8420 | `					continue;` |
|        - |  8421 | `				}` |
|        - |  8422 | `				/* Invalidate any prior representation */` |
|       73 |  8423 | `				PH7_MemObjRelease(pValue);` |
|        - |  8424 | `				/* Duplicate bound variable value */` |
|       73 |  8425 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  8426 | `			}` |
|       57 |  8427 | `		}` |
|        - |  8428 | `		/* Process default values for remaining formal parameters */` |
|    19098 |  8429 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2490 |  8430 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8431 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  8432 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  8433 | `				if( pObj ){` |
|       42 |  8434 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  8435 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  8436 | `					sArg.pUserData = 0;` |
|       42 |  8437 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  8438 | `				}` |
|       42 |  8439 | `				n++;` |
|       42 |  8440 | `				break; /* Variadic is always last */` |
|        - |  8441 | `			}` |
|     2450 |  8442 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2444 |  8443 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2444 |  8444 | `				if( pObj ){` |
|        - |  8445 | `					/* Evaluate the default value and extract it's result */` |
|     2444 |  8446 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2444 |  8447 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8448 | `						goto Abort;` |
|        - |  8449 | `					}` |
|        - |  8450 | `					/* Insert argument index */` |
|     2444 |  8451 | `					sArg.nIdx = pObj->nIdx;` |
|     2444 |  8452 | `					sArg.pUserData = 0;` |
|     2444 |  8453 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8454 | `					/* Make sure the default argument is of the correct type */` |
|     2442 |  8455 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1636 |  8456 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8457 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8458 | `						/* Cast to the desired type */` |
|        3 |  8459 | `						xCast(pObj);` |
|        1 |  8460 | `					}` |
|     1221 |  8461 | `				}` |
|     1221 |  8462 | `			}` |
|     2450 |  8463 | `			++n;` |
|        2 |  8464 | `		}` |
|        - |  8465 | `		} /* end VmCallArgMap scope */` |
|        - |  8466 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8467 | `		 * does not return anything.` |
|        - |  8468 | `		 */` |
|    16650 |  8469 | `		PH7_MemObjRelease(pTos);` |
|    16650 |  8470 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8471 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    16650 |  8472 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    16650 |  8473 | `		if( pFrameStack == 0 ){` |
|        - |  8474 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8475 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8476 | `				&pVmFunc->sName);` |
|      ! 0 |  8477 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8478 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8479 | `			}` |
|      ! 0 |  8480 | `			break;` |
|        - |  8481 | `		}` |
|     8324 |  8482 | `SkipFuncBody:` |
|    16680 |  8483 | `		if( pSelf ){` |
|        - |  8484 | `			/* Push class name */` |
|     2588 |  8485 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1293 |  8486 | `		}` |
|        - |  8487 | `		/* Increment nesting level */` |
|    16680 |  8488 | `		pVm->nRecursionDepth++;` |
|    16680 |  8489 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8490 | `			/* Execute function body */` |
|    24974 |  8491 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    16648 |  8492 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8324 |  8493 | `		}` |
|        - |  8494 | `		/* Decrement nesting level */` |
|    16680 |  8495 | `		pVm->nRecursionDepth--;` |
|    16680 |  8496 | `		if( pSelf ){` |
|        - |  8497 | `			/* Pop class name */` |
|     2588 |  8498 | `			(void)SySetPop(&pVm->aSelf);` |
|     1293 |  8499 | `		}` |
|        - |  8500 | `		/* Cleanup the mess left behind */` |
|    16680 |  8501 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8502 | `			/* Return by reference,reflect that */` |
|        9 |  8503 | `			if( n != SXU32_HIGH ){` |
|        9 |  8504 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8505 | `				sxu32 i;` |
|        - |  8506 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8507 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8508 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8509 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8510 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8511 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8512 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8513 | `								&pVmFunc->sName);` |
|      ! 0 |  8514 | `						}` |
|      ! 0 |  8515 | `						n = SXU32_HIGH;` |
|      ! 0 |  8516 | `						break;` |
|        - |  8517 | `					}` |
|        3 |  8518 | `				}` |
|        5 |  8519 | `			}else{` |
|      ! 0 |  8520 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8521 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8522 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8523 | `						&pVmFunc->sName);` |
|      ! 0 |  8524 | `				}` |
|        - |  8525 | `			}` |
|        9 |  8526 | `			pTos->nIdx = n;` |
|        4 |  8527 | `		}` |
|        - |  8528 | `		/* Cleanup the mess left behind */` |
|    16680 |  8529 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8530 | `			/* An exception was throw in this frame */` |
|       48 |  8531 | `			pFrame = pFrame->pParent;` |
|       48 |  8532 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8533 | `				/* Pop the resutlt */` |
|       46 |  8534 | `				VmPopOperand(&pTos,1);` |
|        - |  8535 | `				/* Jump to this destination */` |
|       46 |  8536 | `				pc = pFrame->iExceptionJump - 1;` |
|       46 |  8537 | `				rc = PH7_OK;` |
|       24 |  8538 | `			}else{` |
|        3 |  8539 | `				if( pFrame->pParent ){` |
|        3 |  8540 | `					rc = PH7_EXCEPTION;` |
|        2 |  8541 | `				}else{` |
|        - |  8542 | `					/* Continue normal execution */` |
|      ! 0 |  8543 | `					rc = PH7_OK;` |
|        - |  8544 | `				}` |
|        - |  8545 | `			}` |
|       23 |  8546 | `		}` |
|        - |  8547 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    16680 |  8548 | `		if( pFrameStack ){` |
|    16650 |  8549 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8324 |  8550 | `		}` |
|        - |  8551 | `		/* Leave the frame */` |
|    16680 |  8552 | `		VmLeaveFrame(&(*pVm));` |
|    16680 |  8553 | `		if( rc == PH7_ABORT ){` |
|        - |  8554 | `			/* Abort processing immeditaley */` |
|       15 |  8555 | `			goto Abort;` |
|    16666 |  8556 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8557 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8558 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8559 | `			 * overwriting the state saved by the inner level.` |
|        - |  8560 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8561 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8562 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8563 | `			goto Suspend;` |
|    16628 |  8564 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8565 | `			goto Exception;` |
|        - |  8566 | `		}` |
|     8314 |  8567 | `	}else{` |
|        - |  8568 | `		ph7_user_func *pFunc;` |
|        - |  8569 | `		ph7_context sCtx;` |
|        - |  8570 | `		ph7_value sRet;` |
|        - |  8571 | `		/* Look for an installed foreign function.` |
|        - |  8572 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8573 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8574 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8575 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   650338 |  8576 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8577 | `		{` |
|   650338 |  8578 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   650338 |  8579 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8580 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8581 | `			const char *zShort = sName.zString;` |
|        - |  8582 | `			sxu32 i;` |
|      334 |  8583 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8584 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8585 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8586 | `				}` |
|      158 |  8587 | `			}` |
|       22 |  8588 | `			if( zShort != sName.zString ){` |
|       22 |  8589 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8590 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8591 | `			}` |
|       10 |  8592 | `		}` |
|        - |  8593 | `		} /* end VmCallArgMap namespace scope */` |
|   650338 |  8594 | `		if( pEntry == 0 ){` |
|        - |  8595 | `			/* Call to undefined function */` |
|        5 |  8596 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8597 | `			/* Pop given arguments */` |
|        5 |  8598 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8599 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8600 | `			}` |
|        - |  8601 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8602 | `			PH7_MemObjRelease(pTos);` |
|        9 |  8603 | `			break;` |
|        - |  8604 | `		}` |
|   650334 |  8605 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8606 | `		/* Start collecting function arguments */` |
|   650334 |  8607 | `		SySetReset(&aArg);` |
|  1750052 |  8608 | `		while( pArg < pTos ){` |
|  1099720 |  8609 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1099720 |  8610 | `			pArg++;` |
|        2 |  8611 | `		}` |
|        - |  8612 | `		/* Assume a null return value */` |
|   650334 |  8613 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8614 | `		/* Init the call context */` |
|   650334 |  8615 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8616 | `		/* Call the foreign function */` |
|   650334 |  8617 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8618 | `		/* Release the call context */` |
|   650334 |  8619 | `		VmReleaseCallContext(&sCtx);` |
|   650334 |  8620 | `		if( rc == PH7_ABORT ){` |
|      471 |  8621 | `			goto Abort;` |
|   649864 |  8622 | `		}else if( rc == PH7_EXCEPTION ){` |
|       14 |  8623 | `			VmFrame *pFrm = pVm->pFrame;` |
|       14 |  8624 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       14 |  8625 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8626 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8627 | `				goto Exception;` |
|        - |  8628 | `			}` |
|        - |  8629 | `			/* Exception was caught: pop args and the result slot */` |
|        9 |  8630 | `			PH7_MemObjRelease(&sRet);` |
|        9 |  8631 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8632 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8633 | `			}` |
|        - |  8634 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        9 |  8635 | `			VmPopOperand(&pTos,1);` |
|        - |  8636 | `			/* Jump past the try/catch block via the exception frame */` |
|        9 |  8637 | `			pFrm = pVm->pFrame;` |
|        9 |  8638 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        9 |  8639 | `				pc = pFrm->iExceptionJump - 1;` |
|        4 |  8640 | `			}` |
|        9 |  8641 | `			break;` |
|        - |  8642 | `		}` |
|   649852 |  8643 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8644 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8645 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8646 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8647 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8648 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8649 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8650 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8651 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8652 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8653 | `			}` |
|        - |  8654 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8655 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8656 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8657 | `			goto Suspend;` |
|        - |  8658 | `		}` |
|   649814 |  8659 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8660 | `			/* Pop function name and arguments */` |
|   629328 |  8661 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   314685 |  8662 | `		}` |
|        - |  8663 | `		/* Save foreign function return value */` |
|   649814 |  8664 | `		PH7_MemObjStore(&sRet,pTos);` |
|   649814 |  8665 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8666 | `	}` |
|   666438 |  8667 | `	break;` |
|        - |  8668 | `				  }` |
|        - |  8669 | `/*` |
|        - |  8670 | ` * OP_CONSUME: P1 * *` |
|        - |  8671 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8672 | ` */` |
|    13823 |  8673 | `case PH7_OP_CONSUME: {` |
|    27648 |  8674 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    27648 |  8675 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8676 |  |
|    27648 |  8677 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    27648 |  8678 | `	pCur = pOut;` |
|        - |  8679 | `	/* Start the consume process  */` |
|    55294 |  8680 | `	while( pOut <= pTos ){` |
|        - |  8681 | `		/* Force a string cast */` |
|    27648 |  8682 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      556 |  8683 | `			PH7_MemObjToString(pOut);` |
|      277 |  8684 | `		}` |
|    27648 |  8685 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8686 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8687 | `			/* Invoke the output consumer callback */` |
|    16078 |  8688 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    16078 |  8689 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    16078 |  8690 | `			SyBlobRelease(&pOut->sBlob);` |
|    16078 |  8691 | `			if( rc == SXERR_ABORT ){` |
|        - |  8692 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8693 | `				goto Abort;` |
|        - |  8694 | `			}` |
|     8038 |  8695 | `		}` |
|    27648 |  8696 | `		pOut++;` |
|        2 |  8697 | `	}` |
|    27648 |  8698 | `	pTos = &pCur[-1];` |
|    27646 |  8699 | `	break;` |
|        - |  8700 | `					 }` |
|        - |  8701 |  |
|        - |  8702 | `		} /* Switch() */` |
| 11127298 |  8703 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8704 | `	} /* For(;;) */` |
|    19818 |  8705 | `Done:` |
|    39638 |  8706 | `	SySetRelease(&aArg);` |
|    39638 |  8707 | `	return SXRET_OK;` |
|       72 |  8708 | `Suspend:` |
|      146 |  8709 | `	SySetRelease(&aArg);` |
|      146 |  8710 | `	return PH7_SUSPEND;` |
|      259 |  8711 | `Abort:` |
|      519 |  8712 | `	SySetRelease(&aArg);` |
|     1767 |  8713 | `	while( pTos >= pStack ){` |
|     1249 |  8714 | `		PH7_MemObjRelease(pTos);` |
|     1249 |  8715 | `		pTos--;` |
|        1 |  8716 | `	}` |
|      519 |  8717 | `	return PH7_ABORT;` |
|        3 |  8718 | `Exception:` |
|        8 |  8719 | `	SySetRelease(&aArg);` |
|       22 |  8720 | `	while( pTos >= pStack ){` |
|       16 |  8721 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8722 | `		pTos--;` |
|        2 |  8723 | `	}` |
|        8 |  8724 | `	return PH7_EXCEPTION;` |
|    20154 |  8725 |  |
|        - |  8726 | `/*` |
|        - |  8727 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8728 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8729 | ` * See block-comment on that function for additional information.` |
|        - |  8730 | ` */` |
|    18578 |  8731 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8732 |  |
|        - |  8733 | `	ph7_value *pStack;` |
|        - |  8734 | `	sxi32 rc;` |
|        - |  8735 | `	/* Allocate a new operand stack */` |
|    18580 |  8736 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    18580 |  8737 | `	if( pStack == 0 ){` |
|      ! 0 |  8738 | `		return SXERR_MEM;` |
|        - |  8739 | `	}` |
|        - |  8740 | `	/* Execute the program */` |
|    18580 |  8741 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  8742 | `	/* Free the operand stack */` |
|    18580 |  8743 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8744 | `	/* Execution result */` |
|    18580 |  8745 | `	return rc;` |
|     9291 |  8746 |  |
|        - |  8747 | `/*` |
|        - |  8748 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8749 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8750 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8751 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8752 | ` * execution ends.` |
|        - |  8753 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8754 | ` * additional information.` |
|        - |  8755 | ` */` |
|     2614 |  8756 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8757 |  |
|        - |  8758 | `	VmShutdownCB *pEntry;` |
|        - |  8759 | `	ph7_value *apArg[10];` |
|        - |  8760 | `	sxu32 n,nEntry;` |
|        - |  8761 | `	int i;` |
|        - |  8762 | `	/* Point to the stack of registered callbacks */` |
|     2616 |  8763 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28756 |  8764 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26142 |  8765 | `		apArg[i] = 0;` |
|    13072 |  8766 | `	}` |
|     2618 |  8767 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8768 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8769 | `		if( pEntry ){` |
|        - |  8770 | `			/* Prepare callback arguments if any */` |
|        3 |  8771 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8772 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8773 | `					break;` |
|        - |  8774 | `				}` |
|      ! 0 |  8775 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8776 | `			}` |
|        - |  8777 | `			/* Invoke the callback */` |
|        3 |  8778 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8779 | `			/*` |
|        - |  8780 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8781 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8782 | `			 */` |
|        3 |  8783 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8784 | `			if( pEntry ){` |
|        3 |  8785 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8786 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8787 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8788 | `				}` |
|        1 |  8789 | `			}` |
|        1 |  8790 | `		}` |
|        2 |  8791 | `	}` |
|     2616 |  8792 | `	SySetReset(&pVm->aShutdown);` |
|     2616 |  8793 |  |
|        - |  8794 | `/*` |
|        - |  8795 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8796 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8797 | ` * See block-comment on that function for additional information.` |
|        - |  8798 | ` */` |
|     2622 |  8799 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8800 |  |
|        - |  8801 | `	/* Make sure we are ready to execute this program */` |
|     2624 |  8802 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8803 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8804 | `	}` |
|        - |  8805 | `	/* Set the execution magic number  */` |
|     2624 |  8806 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8807 | `	/* Execute the program */` |
|     2624 |  8808 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  8809 | `	/* Invoke any shutdown callbacks */` |
|     2620 |  8810 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8811 | `	/*` |
|        - |  8812 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8813 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8814 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8815 | `	 */` |
|     2620 |  8816 | `	return SXRET_OK;` |
|     1313 |  8817 |  |
|        - |  8818 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8819 | `/*` |
|        - |  8820 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8821 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8822 | ` */` |
|       46 |  8823 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8824 |  |
|        - |  8825 | `	ph7_exec_ctx *pCtx;` |
|        - |  8826 | `	ph7_value *pStack;` |
|        - |  8827 | `	VmFrame *pFrame;` |
|       48 |  8828 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8829 | `	if( pCtx == 0 ){` |
|      ! 0 |  8830 | `		return 0;` |
|        - |  8831 | `	}` |
|       48 |  8832 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  8833 | `	pCtx->pVm = pVm;` |
|       48 |  8834 | `	pCtx->pFunc = pFunc;` |
|       48 |  8835 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  8836 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  8837 | `	pCtx->pc = 0;` |
|       48 |  8838 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  8839 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  8840 | `	/* Allocate a private operand stack */` |
|       48 |  8841 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  8842 | `	if( pStack == 0 ){` |
|      ! 0 |  8843 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8844 | `		return 0;` |
|        - |  8845 | `	}` |
|       48 |  8846 | `	pCtx->pStack = pStack;` |
|        - |  8847 | `	/* Create a detached frame for the fiber */` |
|       48 |  8848 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  8849 | `	if( pFrame == 0 ){` |
|      ! 0 |  8850 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  8851 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8852 | `		return 0;` |
|        - |  8853 | `	}` |
|       48 |  8854 | `	pCtx->pFrame = pFrame;` |
|       48 |  8855 | `	return pCtx;` |
|       25 |  8856 |  |
|        - |  8857 | `/*` |
|        - |  8858 | ` * Start executing a fiber context for the first time.` |
|        - |  8859 | ` */` |
|       46 |  8860 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  8861 |  |
|        - |  8862 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8863 | `	sxi32 rc;` |
|       48 |  8864 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8865 | `		return SXERR_INVALID;` |
|        - |  8866 | `	}` |
|        - |  8867 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  8868 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  8869 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8870 | `	/* Save and set the active context */` |
|       48 |  8871 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  8872 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  8873 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  8874 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  8875 | `	pVm->nRecursionDepth++;` |
|        - |  8876 | `	/* Execute from the beginning */` |
|       48 |  8877 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  8878 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  8879 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  8880 | `	pVm->nRecursionDepth--;` |
|        - |  8881 | `	/* Restore the previous context */` |
|       48 |  8882 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  8883 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8884 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  8885 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  8886 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  8887 | `		if( pResult ){` |
|       24 |  8888 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  8889 | `		}` |
|       46 |  8890 | `		return SXRET_OK;` |
|        - |  8891 | `	}` |
|        - |  8892 | `	/* Detach frame */` |
|        3 |  8893 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  8894 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  8895 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  8896 | `	}` |
|        3 |  8897 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8898 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8899 | `		return PH7_ABORT;` |
|        - |  8900 | `	}` |
|        3 |  8901 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8902 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8903 | `		return PH7_EXCEPTION;` |
|        - |  8904 | `	}` |
|        - |  8905 | `	/* Normal completion */` |
|        3 |  8906 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  8907 | `	if( pResult ){` |
|        3 |  8908 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  8909 | `	}` |
|        3 |  8910 | `	return SXRET_OK;` |
|       25 |  8911 |  |
|        - |  8912 | `/*` |
|        - |  8913 | ` * Resume a suspended fiber context.` |
|        - |  8914 | ` */` |
|       98 |  8915 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  8916 |  |
|        - |  8917 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8918 | `	sxi32 rc;` |
|      100 |  8919 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  8920 | `		return SXERR_INVALID;` |
|        - |  8921 | `	}` |
|        - |  8922 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  8923 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  8924 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  8925 | `	if( pResumeValue ){` |
|       40 |  8926 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  8927 | `	}else{` |
|       62 |  8928 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  8929 | `	}` |
|      100 |  8930 | `	pCtx->nTos++;` |
|        - |  8931 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  8932 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  8933 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8934 | `	/* Save and set the active context */` |
|      100 |  8935 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  8936 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  8937 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  8938 | `	pVm->nRecursionDepth++;` |
|        - |  8939 | `	/* Resume execution from saved PC */` |
|      100 |  8940 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  8941 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  8942 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  8943 | `	pVm->nRecursionDepth--;` |
|        - |  8944 | `	/* Restore the previous context */` |
|      100 |  8945 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  8946 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8947 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  8948 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  8949 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  8950 | `		if( pResult ){` |
|       18 |  8951 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8952 | `		}` |
|       64 |  8953 | `		return SXRET_OK;` |
|        - |  8954 | `	}` |
|        - |  8955 | `	/* Detach frame */` |
|       38 |  8956 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  8957 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  8958 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  8959 | `	}` |
|       38 |  8960 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8961 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8962 | `		return PH7_ABORT;` |
|        - |  8963 | `	}` |
|       38 |  8964 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8965 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8966 | `		return PH7_EXCEPTION;` |
|        - |  8967 | `	}` |
|        - |  8968 | `	/* Normal completion */` |
|       38 |  8969 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  8970 | `	if( pResult ){` |
|       20 |  8971 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8972 | `	}` |
|       38 |  8973 | `	return SXRET_OK;` |
|       51 |  8974 |  |
|        - |  8975 | `/*` |
|        - |  8976 | ` * Release an execution context and all its resources.` |
|        - |  8977 | ` */` |
|        4 |  8978 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8979 |  |
|        5 |  8980 | `	if( pCtx == 0 ){` |
|      ! 0 |  8981 | `		return;` |
|        - |  8982 | `	}` |
|        5 |  8983 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8984 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8985 | `		return;` |
|        - |  8986 | `	}` |
|        5 |  8987 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8988 | `	/* Release values */` |
|        5 |  8989 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8990 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8991 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8992 | `	if( pCtx->pFrame ){` |
|        - |  8993 | `		VmSlot *aSlot;` |
|        - |  8994 | `		sxu32 n;` |
|        - |  8995 | `		/* Free local variables */` |
|        5 |  8996 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8997 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8998 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8999 | `		}` |
|        - |  9000 | `		/* Remove local references */` |
|        5 |  9001 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9002 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9003 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9004 | `		}` |
|        5 |  9005 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9006 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9007 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9008 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9009 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9010 | `		pCtx->pFrame = 0;` |
|        2 |  9011 | `	}` |
|        - |  9012 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9013 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9014 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9015 | `	if( pCtx->pStack ){` |
|        5 |  9016 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9017 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9018 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9019 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9020 | `				pTos--;` |
|        1 |  9021 | `			}` |
|        2 |  9022 | `		}` |
|        5 |  9023 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9024 | `		pCtx->pStack = 0;` |
|        2 |  9025 | `	}` |
|        - |  9026 | `	/* Free the context itself */` |
|        5 |  9027 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9028 |  |
|        - |  9029 | `/*` |
|        - |  9030 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9031 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9032 | ` */` |
|       90 |  9033 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9034 |  |
|        - |  9035 | `	ph7_class_instance *pThis;` |
|        - |  9036 | `	SyString sAttr;` |
|        - |  9037 | `	ph7_value *pAttr;` |
|       92 |  9038 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9039 | `		return 0;` |
|        - |  9040 | `	}` |
|       92 |  9041 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9042 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9043 | `		return 0;` |
|        - |  9044 | `	}` |
|       92 |  9045 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9046 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9047 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9048 | `		return 0;` |
|        - |  9049 | `	}` |
|       62 |  9050 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9051 |  |
|        - |  9052 | `/*` |
|        - |  9053 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9054 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9055 | ` */` |
|       38 |  9056 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9057 |  |
|       40 |  9058 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9059 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9060 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9061 | `			"Cannot suspend outside of a fiber");` |
|        - |  9062 | `	}` |
|       40 |  9063 | `	if( nArg > 0 ){` |
|       40 |  9064 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9065 | `	}else{` |
|      ! 0 |  9066 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9067 | `	}` |
|       40 |  9068 | `	return PH7_SUSPEND;` |
|       21 |  9069 |  |
|        - |  9070 | `/*` |
|        - |  9071 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9072 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9073 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9074 | ` */` |
|       24 |  9075 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9076 |  |
|        - |  9077 | `	ph7_class_instance *pThis;` |
|        - |  9078 | `	ph7_value *pAttr;` |
|        - |  9079 | `	SyString sAttrName;` |
|       26 |  9080 | `	if( nArg < 2 ){` |
|      ! 0 |  9081 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9082 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9083 | `	}` |
|       26 |  9084 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9085 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9086 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9087 | `	}` |
|       26 |  9088 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9089 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9090 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9091 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9092 | `	}` |
|        - |  9093 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9094 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9095 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9096 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9097 | `	}` |
|        - |  9098 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9099 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9100 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9101 | `	if( pAttr ){` |
|       26 |  9102 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9103 | `	}` |
|       26 |  9104 | `	return PH7_OK;` |
|       14 |  9105 |  |
|        - |  9106 | `/*` |
|        - |  9107 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9108 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9109 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9110 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9111 | ` */` |
|       24 |  9112 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9113 | `	ph7_class_instance **ppThis)` |
|        2 |  9114 |  |
|       26 |  9115 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9116 | `	ph7_value *pCallable;` |
|        - |  9117 | `	SyString sAttrName;` |
|       26 |  9118 | `	*ppThis = 0;` |
|       26 |  9119 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9120 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9121 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9122 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9123 | `		return 0;` |
|        - |  9124 | `	}` |
|       26 |  9125 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9126 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9127 | `		SyString sName;` |
|        - |  9128 | `		SyHashEntry *pEntry;` |
|        - |  9129 | `		ph7_vm_func *pFunc;` |
|       26 |  9130 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9131 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9132 | `		if( pEntry == 0 ){` |
|      ! 0 |  9133 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9134 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9135 | `			return 0;` |
|        - |  9136 | `		}` |
|       26 |  9137 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9138 | `		return pFunc;` |
|      ! 0 |  9139 | `	}else{` |
|        - |  9140 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9141 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9142 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9143 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9144 | `		if( pMethod == 0 ){` |
|      ! 0 |  9145 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9146 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9147 | `			return 0;` |
|        - |  9148 | `		}` |
|      ! 0 |  9149 | `		*ppThis = pClosure;` |
|      ! 0 |  9150 | `		return &pMethod->sFunc;` |
|        - |  9151 | `	}` |
|       14 |  9152 |  |
|        - |  9153 | `/*` |
|        - |  9154 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9155 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9156 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9157 | ` */` |
|       46 |  9158 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9159 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9160 |  |
|       48 |  9161 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9162 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9163 | `	sxu32 nFormal, n;` |
|        - |  9164 | `	VmSlot sSlot;` |
|        - |  9165 | `	sxi32 rc;` |
|        - |  9166 | `	/* Install $this for closure/method callables */` |
|       48 |  9167 | `	if( pClosureThis ){` |
|        - |  9168 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9169 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9170 | `		if( pObj ){` |
|      ! 0 |  9171 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9172 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9173 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9174 | `		}` |
|      ! 0 |  9175 | `	}` |
|        - |  9176 | `	/* Install static variables */` |
|       48 |  9177 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9178 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9179 | `		ph7_value *pVal;` |
|      ! 0 |  9180 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9181 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9182 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9183 | `			if( pVal ){` |
|      ! 0 |  9184 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9185 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9186 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9187 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9188 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9189 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9190 | `				}` |
|      ! 0 |  9191 | `			}` |
|      ! 0 |  9192 | `		}` |
|      ! 0 |  9193 | `	}` |
|        - |  9194 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9195 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9196 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9197 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9198 | `		ph7_value *pObj;` |
|       20 |  9199 | `		if( n < (sxu32)nArg ){` |
|        - |  9200 | `			/* Argument provided — install with type casting */` |
|       20 |  9201 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9202 | `			if( pObj ){` |
|       20 |  9203 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9204 | `				/* Type casting */` |
|       20 |  9205 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9206 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9207 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9208 | `						if( xCast ){` |
|      ! 0 |  9209 | `							xCast(pObj);` |
|      ! 0 |  9210 | `						}` |
|      ! 0 |  9211 | `					}` |
|      ! 0 |  9212 | `				}` |
|       20 |  9213 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9214 | `				sSlot.pUserData = 0;` |
|       20 |  9215 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9216 | `			}` |
|        9 |  9217 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9218 | `			/* Default value */` |
|      ! 0 |  9219 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9220 | `			if( pObj ){` |
|      ! 0 |  9221 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9222 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9223 | `					return rc;` |
|        - |  9224 | `				}` |
|      ! 0 |  9225 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9226 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9227 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9228 | `						if( xCast ){` |
|      ! 0 |  9229 | `							xCast(pObj);` |
|      ! 0 |  9230 | `						}` |
|      ! 0 |  9231 | `					}` |
|      ! 0 |  9232 | `				}` |
|      ! 0 |  9233 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9234 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9235 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9236 | `			}` |
|      ! 0 |  9237 | `		}` |
|       11 |  9238 | `	}` |
|        - |  9239 | `	/* Install closure environment (captured variables) */` |
|       48 |  9240 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9241 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9242 | `		ph7_value *pValue;` |
|        - |  9243 | `		sxu32 iEnv;` |
|        3 |  9244 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9245 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9246 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9247 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9248 | `				continue;` |
|        - |  9249 | `			}` |
|        5 |  9250 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9251 | `			if( pValue == 0 ){` |
|      ! 0 |  9252 | `				continue;` |
|        - |  9253 | `			}` |
|        5 |  9254 | `			PH7_MemObjRelease(pValue);` |
|        5 |  9255 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  9256 | `		}` |
|        1 |  9257 | `	}` |
|       48 |  9258 | `	return SXRET_OK;` |
|       25 |  9259 |  |
|        - |  9260 | `/*` |
|        - |  9261 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  9262 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  9263 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  9264 | ` */` |
|       26 |  9265 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9266 |  |
|       28 |  9267 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9268 | `	ph7_class_instance *pThis;` |
|        - |  9269 | `	ph7_class_instance *pClosureThis;` |
|        - |  9270 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9271 | `	ph7_vm_func *pFunc;` |
|        - |  9272 | `	ph7_value sResult;` |
|        - |  9273 | `	ph7_value *pCtxAttr;` |
|        - |  9274 | `	SyString sAttrName;` |
|        - |  9275 | `	sxi32 rc;` |
|       28 |  9276 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9277 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  9278 | `	}` |
|       28 |  9279 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9280 | `	/* Check if already started (has a __ctx) */` |
|       28 |  9281 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  9282 | `	if( pExecCtx != 0 ){` |
|        3 |  9283 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9284 | `			"Cannot start a fiber that has already been started");` |
|        - |  9285 | `	}` |
|        - |  9286 | `	/* Resolve callable */` |
|       26 |  9287 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  9288 | `	if( pFunc == 0 ){` |
|      ! 0 |  9289 | `		return PH7_EXCEPTION;` |
|        - |  9290 | `	}` |
|        - |  9291 | `	/* Create execution context now that we know the function */` |
|       26 |  9292 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  9293 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9294 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9295 | `			"Fiber::start(): out of memory");` |
|        - |  9296 | `	}` |
|        - |  9297 | `	/* Store context in $this->__ctx */` |
|       26 |  9298 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  9299 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9300 | `	if( pCtxAttr ){` |
|       26 |  9301 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  9302 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  9303 | `	}` |
|        - |  9304 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  9305 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  9306 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  9307 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  9308 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  9309 | `	/* Unpack the args array and install into the frame */` |
|        - |  9310 | `	{` |
|       26 |  9311 | `		ph7_value **apValues = 0;` |
|       26 |  9312 | `		int nActual = 0;` |
|       26 |  9313 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  9314 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  9315 | `			ph7_hashmap_node *pNode;` |
|       26 |  9316 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  9317 | `			if( nCount > 0 ){` |
|        3 |  9318 | `				sxu32 idx = 0;` |
|        4 |  9319 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  9320 | `					nCount * sizeof(ph7_value *));` |
|        3 |  9321 | `				if( apValues ){` |
|        3 |  9322 | `					pNode = pMap->pFirst;` |
|        7 |  9323 | `					while( pNode && idx < nCount ){` |
|        5 |  9324 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  9325 | `						idx++;` |
|        5 |  9326 | `						pNode = pNode->pPrev;` |
|        1 |  9327 | `					}` |
|        3 |  9328 | `					nActual = (int)idx;` |
|        1 |  9329 | `				}` |
|        1 |  9330 | `			}` |
|       12 |  9331 | `		}` |
|       26 |  9332 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  9333 | `		if( apValues ){` |
|        3 |  9334 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  9335 | `		}` |
|        - |  9336 | `	}` |
|        - |  9337 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  9338 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  9339 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  9340 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9341 | `		return PH7_ABORT;` |
|        - |  9342 | `	}` |
|       26 |  9343 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  9344 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  9345 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9346 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9347 | `		return PH7_ABORT;` |
|        - |  9348 | `	}` |
|       26 |  9349 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9350 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9351 | `		return PH7_EXCEPTION;` |
|        - |  9352 | `	}` |
|       26 |  9353 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  9354 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  9355 | `	return PH7_OK;` |
|       15 |  9356 |  |
|        - |  9357 | `/*` |
|        - |  9358 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9359 | ` */` |
|       36 |  9360 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9361 |  |
|       38 |  9362 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9363 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9364 | `	ph7_value sResult;` |
|        - |  9365 | `	ph7_value *pResumeVal;` |
|        - |  9366 | `	sxi32 rc;` |
|       38 |  9367 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9368 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9369 | `		return PH7_OK;` |
|        - |  9370 | `	}` |
|       38 |  9371 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9372 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9373 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9374 | `		return PH7_OK;` |
|        - |  9375 | `	}` |
|       38 |  9376 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9377 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9378 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9379 | `	}` |
|       36 |  9380 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9381 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9382 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9383 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9384 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9385 | `		return PH7_ABORT;` |
|        - |  9386 | `	}` |
|       36 |  9387 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9388 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9389 | `		return PH7_EXCEPTION;` |
|        - |  9390 | `	}` |
|       36 |  9391 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9392 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9393 | `	return PH7_OK;` |
|       20 |  9394 |  |
|        - |  9395 | `/*` |
|        - |  9396 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9397 | ` */` |
|        6 |  9398 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9399 |  |
|        8 |  9400 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9401 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9402 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9403 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9404 | `		return PH7_OK;` |
|        - |  9405 | `	}` |
|        8 |  9406 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9407 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9408 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9409 | `		return PH7_OK;` |
|        - |  9410 | `	}` |
|        8 |  9411 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9412 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9413 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9414 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9415 | `		}` |
|      ! 0 |  9416 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9417 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9418 | `	}` |
|        8 |  9419 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9420 | `	return PH7_OK;` |
|        5 |  9421 |  |
|        - |  9422 | `/*` |
|        - |  9423 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9424 | ` */` |
|        6 |  9425 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9426 |  |
|        - |  9427 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9428 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9429 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9430 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9431 | `	return PH7_OK;` |
|        4 |  9432 |  |
|      ! 0 |  9433 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9434 |  |
|        - |  9435 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9436 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9437 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9438 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9439 | `	return PH7_OK;` |
|      ! 0 |  9440 |  |
|        6 |  9441 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9442 |  |
|        - |  9443 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9444 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9445 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9446 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9447 | `	return PH7_OK;` |
|        4 |  9448 |  |
|        6 |  9449 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9450 |  |
|        - |  9451 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9452 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9453 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9454 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9455 | `	return PH7_OK;` |
|        4 |  9456 |  |
|        - |  9457 | `/*` |
|        - |  9458 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9459 | ` */` |
|        4 |  9460 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9461 |  |
|        5 |  9462 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9463 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9464 | `	if( nArg < 1 ){` |
|      ! 0 |  9465 | `		return PH7_OK;` |
|        - |  9466 | `	}` |
|        5 |  9467 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9468 | `	if( pExecCtx ){` |
|        5 |  9469 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9470 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9471 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9472 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9473 | `			SyString sAttrName;` |
|        - |  9474 | `			ph7_value *pAttr;` |
|        5 |  9475 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9476 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9477 | `			if( pAttr ){` |
|        5 |  9478 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9479 | `			}` |
|        2 |  9480 | `		}` |
|        2 |  9481 | `	}` |
|        5 |  9482 | `	return PH7_OK;` |
|        3 |  9483 |  |
|        - |  9484 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9485 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9486 |  |
|        - |  9487 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9488 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9489 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9490 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9491 |  |
|      ! 0 |  9492 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9493 |  |
|        - |  9494 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9495 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9496 | `	ph7_exec_ctx *pCtx;` |
|        - |  9497 | `	ph7_vm_func *pFunc;` |
|        - |  9498 | `	ph7_value *pCallable;` |
|        - |  9499 | `	ph7_value *pCtxAttr;` |
|        - |  9500 | `	SyString sAttrName;` |
|        - |  9501 | `	/* Must not already be started */` |
|      ! 0 |  9502 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9503 | `	if( pCtx != 0 ){` |
|      ! 0 |  9504 | `		return SXERR_INVALID;` |
|        - |  9505 | `	}` |
|      ! 0 |  9506 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9507 | `		return SXERR_INVALID;` |
|        - |  9508 | `	}` |
|      ! 0 |  9509 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9510 | `	/* Get the callable */` |
|      ! 0 |  9511 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9512 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9513 | `	if( pCallable == 0 ){` |
|      ! 0 |  9514 | `		return SXERR_INVALID;` |
|        - |  9515 | `	}` |
|        - |  9516 | `	/* Resolve callable */` |
|      ! 0 |  9517 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9518 | `		SyString sName;` |
|        - |  9519 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9520 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9521 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9522 | `		if( pEntry == 0 ){` |
|      ! 0 |  9523 | `			return SXERR_NOTFOUND;` |
|        - |  9524 | `		}` |
|      ! 0 |  9525 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9526 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9527 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9528 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9529 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9530 | `		if( pMethod == 0 ){` |
|      ! 0 |  9531 | `			return SXERR_INVALID;` |
|        - |  9532 | `		}` |
|      ! 0 |  9533 | `		pClosureThis = pClosure;` |
|      ! 0 |  9534 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9535 | `	}else{` |
|      ! 0 |  9536 | `		return SXERR_INVALID;` |
|        - |  9537 | `	}` |
|        - |  9538 | `	/* Create context */` |
|      ! 0 |  9539 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9540 | `	if( pCtx == 0 ){` |
|      ! 0 |  9541 | `		return SXERR_MEM;` |
|        - |  9542 | `	}` |
|        - |  9543 | `	/* Store in __ctx */` |
|      ! 0 |  9544 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9545 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9546 | `	if( pCtxAttr ){` |
|      ! 0 |  9547 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9548 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9549 | `	}` |
|        - |  9550 | `	/* Set up frame with args */` |
|      ! 0 |  9551 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9552 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9553 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9554 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9555 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9556 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9557 |  |
|      ! 0 |  9558 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9559 |  |
|      ! 0 |  9560 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9561 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9562 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9563 |  |
|      ! 0 |  9564 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9565 |  |
|      ! 0 |  9566 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9567 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9568 |  |
|      ! 0 |  9569 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9570 |  |
|      ! 0 |  9571 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9572 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9573 |  |
|      ! 0 |  9574 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9575 |  |
|      ! 0 |  9576 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9577 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9578 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9579 |  |
|        - |  9580 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9581 | `/*` |
|        - |  9582 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9583 | ` */` |
|       22 |  9584 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9585 |  |
|        - |  9586 | `	ph7_generator *pGen;` |
|       24 |  9587 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9588 | `	if( pGen == 0 ){` |
|      ! 0 |  9589 | `		return 0;` |
|        - |  9590 | `	}` |
|       24 |  9591 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9592 | `	pGen->pCtx = pCtx;` |
|       24 |  9593 | `	pGen->iImplicitKey = 0;` |
|       24 |  9594 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9595 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9596 | `	/* Link the generator back to the exec context */` |
|       24 |  9597 | `	pCtx->pPrivate = pGen;` |
|       24 |  9598 | `	return pGen;` |
|       13 |  9599 |  |
|        - |  9600 | `/*` |
|        - |  9601 | ` * Release a generator and its execution context.` |
|        - |  9602 | ` */` |
|      ! 0 |  9603 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9604 |  |
|      ! 0 |  9605 | `	if( pGen == 0 ){` |
|      ! 0 |  9606 | `		return;` |
|        - |  9607 | `	}` |
|      ! 0 |  9608 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9609 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9610 | `	if( pGen->pCtx ){` |
|      ! 0 |  9611 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9612 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9613 | `		pGen->pCtx = 0;` |
|      ! 0 |  9614 | `	}` |
|      ! 0 |  9615 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9616 |  |
|        - |  9617 | `/*` |
|        - |  9618 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9619 | ` */` |
|      236 |  9620 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9621 |  |
|        - |  9622 | `	ph7_class_instance *pThis;` |
|        - |  9623 | `	SyString sAttr;` |
|        - |  9624 | `	ph7_value *pAttr;` |
|      238 |  9625 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9626 | `		return 0;` |
|        - |  9627 | `	}` |
|      238 |  9628 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9629 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9630 | `		return 0;` |
|        - |  9631 | `	}` |
|      238 |  9632 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9633 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9634 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9635 | `		return 0;` |
|        - |  9636 | `	}` |
|      238 |  9637 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9638 |  |
|        - |  9639 | `/*` |
|        - |  9640 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9641 | ` */` |
|       22 |  9642 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9643 |  |
|        - |  9644 | `	ph7_generator *pGen;` |
|        - |  9645 | `	sxi32 rc;` |
|       24 |  9646 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9647 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9648 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9649 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9650 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9651 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9652 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9653 | `	}` |
|       24 |  9654 | `	return PH7_OK;` |
|       13 |  9655 |  |
|        - |  9656 | `/*` |
|        - |  9657 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9658 | ` */` |
|       68 |  9659 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9660 |  |
|        - |  9661 | `	ph7_generator *pGen;` |
|       70 |  9662 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9663 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9664 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9665 | `	return PH7_OK;` |
|       36 |  9666 |  |
|        - |  9667 | `/*` |
|        - |  9668 | ` * Generator::current() — return the last yielded value.` |
|        - |  9669 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9670 | ` */` |
|       68 |  9671 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9672 |  |
|        - |  9673 | `	ph7_generator *pGen;` |
|        - |  9674 | `	sxi32 rc;` |
|       70 |  9675 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9676 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9677 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9678 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9679 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9680 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9681 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9682 | `	}` |
|       70 |  9683 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9684 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9685 | `	}else{` |
|      ! 0 |  9686 | `		ph7_result_null(pCtx);` |
|        - |  9687 | `	}` |
|       70 |  9688 | `	return PH7_OK;` |
|       36 |  9689 |  |
|        - |  9690 | `/*` |
|        - |  9691 | ` * Generator::key() — return the last yielded key.` |
|        - |  9692 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9693 | ` */` |
|       12 |  9694 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9695 |  |
|        - |  9696 | `	ph7_generator *pGen;` |
|        - |  9697 | `	sxi32 rc;` |
|       13 |  9698 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9699 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9700 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9701 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9702 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9703 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9704 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9705 | `	}` |
|       13 |  9706 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9707 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9708 | `	}else{` |
|      ! 0 |  9709 | `		ph7_result_null(pCtx);` |
|        - |  9710 | `	}` |
|       13 |  9711 | `	return PH7_OK;` |
|        7 |  9712 |  |
|        - |  9713 | `/*` |
|        - |  9714 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9715 | ` */` |
|       60 |  9716 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9717 |  |
|        - |  9718 | `	ph7_generator *pGen;` |
|        - |  9719 | `	sxi32 rc;` |
|       62 |  9720 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9721 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9722 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9723 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9724 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9725 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9726 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9727 | `	}else{` |
|      ! 0 |  9728 | `		return PH7_OK;` |
|        - |  9729 | `	}` |
|       62 |  9730 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9731 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9732 | `	return PH7_OK;` |
|       32 |  9733 |  |
|        - |  9734 | `/*` |
|        - |  9735 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9736 | ` */` |
|        4 |  9737 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9738 |  |
|        - |  9739 | `	ph7_generator *pGen;` |
|        - |  9740 | `	ph7_value *pSendVal;` |
|        - |  9741 | `	sxi32 rc;` |
|        5 |  9742 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9743 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9744 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9745 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9746 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9747 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9748 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9749 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9750 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9751 | `	}else{` |
|      ! 0 |  9752 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9753 | `		return PH7_OK;` |
|        - |  9754 | `	}` |
|        5 |  9755 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9756 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9757 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9758 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9759 | `	}else{` |
|        3 |  9760 | `		ph7_result_null(pCtx);` |
|        - |  9761 | `	}` |
|        5 |  9762 | `	return PH7_OK;` |
|        3 |  9763 |  |
|        - |  9764 | `/*` |
|        - |  9765 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9766 | ` *` |
|        - |  9767 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9768 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9769 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9770 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9771 | ` * the exception to the caller.` |
|        - |  9772 | ` */` |
|      ! 0 |  9773 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9774 |  |
|        - |  9775 | `	ph7_generator *pGen;` |
|        - |  9776 | `	const char *zMsg;` |
|        - |  9777 | `	int nLen;` |
|      ! 0 |  9778 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9779 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9780 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9781 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9782 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9783 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9784 | `			"Cannot throw into a closed generator");` |
|        - |  9785 | `	}` |
|        - |  9786 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9787 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9788 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9789 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9790 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9791 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9792 | `	nLen = 0;` |
|      ! 0 |  9793 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9794 | `		/* Try to get the exception's message */` |
|        - |  9795 | `		SyString sAttr;` |
|        - |  9796 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9797 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9798 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9799 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9800 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9801 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9802 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9803 | `		}` |
|      ! 0 |  9804 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9805 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9806 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9807 | `	}` |
|      ! 0 |  9808 | `	(void)nLen;` |
|      ! 0 |  9809 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9810 |  |
|        - |  9811 | `/*` |
|        - |  9812 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9813 | ` */` |
|        2 |  9814 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9815 |  |
|        - |  9816 | `	ph7_generator *pGen;` |
|        3 |  9817 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9818 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9819 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9820 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9821 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9822 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9823 | `	}` |
|        3 |  9824 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9825 | `	return PH7_OK;` |
|        2 |  9826 |  |
|        - |  9827 | `/*` |
|        - |  9828 | ` * Generator::__destruct() — clean up.` |
|        - |  9829 | ` */` |
|      ! 0 |  9830 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9831 |  |
|        - |  9832 | `	ph7_generator *pGen;` |
|      ! 0 |  9833 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  9834 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9835 | `	if( pGen ){` |
|      ! 0 |  9836 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  9837 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9838 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9839 | `			SyString sAttrName;` |
|        - |  9840 | `			ph7_value *pAttr;` |
|      ! 0 |  9841 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9842 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9843 | `			if( pAttr ){` |
|      ! 0 |  9844 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  9845 | `			}` |
|      ! 0 |  9846 | `		}` |
|      ! 0 |  9847 | `	}` |
|      ! 0 |  9848 | `	return PH7_OK;` |
|      ! 0 |  9849 |  |
|        - |  9850 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  9851 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  9852 | `/*` |
|        - |  9853 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  9854 | ` * the desired message.` |
|        - |  9855 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  9856 | ` * in 'api.c' for additional information.` |
|        - |  9857 | ` */` |
|      370 |  9858 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  9859 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  9860 | `	SyString *pString /* Message to output */` |
|        - |  9861 | `	)` |
|        2 |  9862 |  |
|      372 |  9863 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  9864 | `	sxi32 rc = SXRET_OK;` |
|        - |  9865 | `	/* Call the output consumer */` |
|      372 |  9866 | `	if( pString->nByte > 0 ){` |
|      372 |  9867 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  9868 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  9869 | `	}` |
|      372 |  9870 | `	return rc;` |
|        2 |  9871 |  |
|        - |  9872 | `/*` |
|        - |  9873 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  9874 | ` * callback to consume the formatted message.` |
|        - |  9875 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  9876 | ` * in 'api.c' for additional information.` |
|        - |  9877 | ` */` |
|        2 |  9878 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  9879 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  9880 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  9881 | `	va_list ap           /* Variable list of arguments */` |
|        - |  9882 | `	)` |
|        1 |  9883 |  |
|        3 |  9884 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  9885 | `	sxi32 rc = SXRET_OK;` |
|        - |  9886 | `	SyBlob sWorker;` |
|        - |  9887 | `	/* Format the message and call the output consumer */` |
|        3 |  9888 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  9889 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  9890 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  9891 | `		/* Consume the formatted message */` |
|        3 |  9892 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  9893 | `	}` |
|        3 |  9894 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  9895 | `	/* Release the working buffer */` |
|        3 |  9896 | `	SyBlobRelease(&sWorker);` |
|        3 |  9897 | `	return rc;` |
|        1 |  9898 |  |
|        - |  9899 | `/*` |
|        - |  9900 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  9901 | ` * This function never fail and always return a pointer` |
|        - |  9902 | ` * to a null terminated string.` |
|        - |  9903 | ` */` |
|       12 |  9904 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  9905 |  |
|       13 |  9906 | `	const char *zOp = "Unknown     ";` |
|       13 |  9907 | `	switch(nOp){` |
|        3 |  9908 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  9909 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  9910 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  9911 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  9912 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  9913 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  9914 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  9915 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  9916 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  9917 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  9918 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  9919 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  9920 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  9921 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  9922 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  9923 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  9924 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  9925 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  9926 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  9927 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  9928 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  9929 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  9930 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  9931 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  9932 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  9933 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  9934 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  9935 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  9936 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  9937 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  9938 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  9939 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  9940 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  9941 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  9942 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  9943 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  9944 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  9945 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9946 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9947 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9948 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9949 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9950 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9951 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9952 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9953 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9954 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9955 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9956 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9957 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9958 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9959 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9960 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9961 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9962 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9963 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9964 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9965 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9966 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 |  9967 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9968 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9969 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9970 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9971 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9972 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9973 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9974 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9975 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9976 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9977 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9978 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9979 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9980 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9981 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9982 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9983 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9984 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9985 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9986 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9987 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9988 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9989 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9990 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9991 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9992 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9993 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9994 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9995 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9996 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9997 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9998 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 |  9999 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10000 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10001 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10002 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10003 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10004 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10005 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10006 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10007 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10008 | `	default:` |
|      ! 0 | 10009 | `		break;` |
|        - | 10010 | `	}` |
|       13 | 10011 | `	return zOp;` |
|        1 | 10012 |  |
|        - | 10013 | `/*` |
|        - | 10014 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10015 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10016 | ` * is responsible of consuming the generated dump.` |
|        - | 10017 | ` */` |
|        2 | 10018 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10019 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10020 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10021 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10022 | `	)` |
|        1 | 10023 |  |
|        - | 10024 | `	sxi32 rc;` |
|        3 | 10025 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10026 | `	return rc;` |
|        1 | 10027 |  |
|        - | 10028 | `/*` |
|        - | 10029 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10030 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10031 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10032 | ` * in 'compile.c' for additional information.` |
|        - | 10033 | ` */` |
|       14 | 10034 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10035 |  |
|       15 | 10036 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10037 | `	/* Evaluate and expand constant value */` |
|       15 | 10038 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10039 |  |
|        - | 10040 | `/*` |
|        - | 10041 | ` * Section:` |
|        - | 10042 | ` *  Function handling functions.` |
|        - | 10043 | ` * Status:` |
|        - | 10044 | ` *    Stable.` |
|        - | 10045 | ` */` |
|        - | 10046 | `/*` |
|        - | 10047 | ` * int func_num_args(void)` |
|        - | 10048 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10049 | ` * Parameters` |
|        - | 10050 | ` *   None.` |
|        - | 10051 | ` * Return` |
|        - | 10052 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10053 | ` *  or -1 if called from the globe scope.` |
|        - | 10054 | ` */` |
|      944 | 10055 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10056 |  |
|        - | 10057 | `	VmFrame *pFrame;` |
|        - | 10058 | `	ph7_vm *pVm;` |
|        - | 10059 | `	/* Point to the target VM */` |
|      946 | 10060 | `	pVm = pCtx->pVm;` |
|        - | 10061 | `	/* Current frame */` |
|      946 | 10062 | `	pFrame = pVm->pFrame;` |
|      946 | 10063 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 | 10064 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10065 | `		SXUNUSED(nArg);` |
|      ! 0 | 10066 | `		SXUNUSED(apArg);` |
|        - | 10067 | `		/* Global frame,return -1 */` |
|      ! 0 | 10068 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10069 | `		return SXRET_OK;` |
|        - | 10070 | `	}` |
|        - | 10071 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 | 10072 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 | 10073 | `	ph7_result_int(pCtx,nArg);` |
|      946 | 10074 | `	return SXRET_OK;` |
|      474 | 10075 |  |
|        - | 10076 | `/*` |
|        - | 10077 | ` * value func_get_arg(int $arg_num)` |
|        - | 10078 | ` *   Return an item from the argument list.` |
|        - | 10079 | ` * Parameters` |
|        - | 10080 | ` *  Argument number(index start from zero).` |
|        - | 10081 | ` * Return` |
|        - | 10082 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10083 | ` */` |
|       22 | 10084 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10085 |  |
|       24 | 10086 | `	ph7_value *pObj = 0;` |
|       24 | 10087 | `	VmSlot *pSlot = 0;` |
|        - | 10088 | `	VmFrame *pFrame;` |
|        - | 10089 | `	ph7_vm *pVm;` |
|        - | 10090 | `	/* Point to the target VM */` |
|       24 | 10091 | `	pVm = pCtx->pVm;` |
|        - | 10092 | `	/* Current frame */` |
|       24 | 10093 | `	pFrame = pVm->pFrame;` |
|       24 | 10094 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10095 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10096 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10097 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10098 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10099 | `		return SXRET_OK;` |
|        - | 10100 | `	}` |
|        - | 10101 | `	/* Extract the desired index */` |
|       21 | 10102 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10103 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10104 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10105 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10106 | `		return SXRET_OK;` |
|        - | 10107 | `	}` |
|        - | 10108 | `	/* Extract the desired argument */` |
|       21 | 10109 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10110 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10111 | `			/* Return the desired argument */` |
|       21 | 10112 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10113 | `		}else{` |
|        - | 10114 | `			/* No such argument,return false */` |
|      ! 0 | 10115 | `			ph7_result_bool(pCtx,0);` |
|        - | 10116 | `		}` |
|       11 | 10117 | `	}else{` |
|        - | 10118 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10119 | `		ph7_result_bool(pCtx,0);` |
|        - | 10120 | `	}` |
|       21 | 10121 | `	return SXRET_OK;` |
|       13 | 10122 |  |
|        - | 10123 | `/*` |
|        - | 10124 | ` * array func_get_args_byref(void)` |
|        - | 10125 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10126 | ` * Parameters` |
|        - | 10127 | ` *  None.` |
|        - | 10128 | ` * Return` |
|        - | 10129 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10130 | ` *  member of the current user-defined function's argument list.` |
|        - | 10131 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10132 | ` * NOTE:` |
|        - | 10133 | ` *  Arguments are returned to the array by reference.` |
|        - | 10134 | ` */` |
|        2 | 10135 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10136 |  |
|        - | 10137 | `	ph7_value *pArray;` |
|        - | 10138 | `	VmFrame *pFrame;` |
|        - | 10139 | `	VmSlot *aSlot;` |
|        - | 10140 | `	sxu32 n;` |
|        - | 10141 | `	/* Point to the current frame */` |
|        3 | 10142 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10143 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10144 | `	if( pFrame->pParent == 0 ){` |
|        - | 10145 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10146 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10147 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10148 | `		return SXRET_OK;` |
|        - | 10149 | `	}` |
|        - | 10150 | `	/* Create a new array */` |
|        3 | 10151 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10152 | `	if( pArray == 0 ){` |
|      ! 0 | 10153 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10154 | `		SXUNUSED(apArg);` |
|      ! 0 | 10155 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10156 | `		return SXRET_OK;` |
|        - | 10157 | `	}` |
|        - | 10158 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10159 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10160 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10161 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10162 | `	}` |
|        - | 10163 | `	/* Return the freshly created array */` |
|        3 | 10164 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10165 | `	return SXRET_OK;` |
|        2 | 10166 |  |
|        - | 10167 | `/*` |
|        - | 10168 | ` * array func_get_args(void)` |
|        - | 10169 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10170 | ` * Parameters` |
|        - | 10171 | ` *  None.` |
|        - | 10172 | ` * Return` |
|        - | 10173 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10174 | ` *  member of the current user-defined function's argument list.` |
|        - | 10175 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10176 | ` */` |
|       88 | 10177 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10178 |  |
|       90 | 10179 | `	ph7_value *pObj = 0;` |
|        - | 10180 | `	ph7_value *pArray;` |
|        - | 10181 | `	VmFrame *pFrame;` |
|        - | 10182 | `	VmSlot *aSlot;` |
|        - | 10183 | `	sxu32 n;` |
|        - | 10184 | `	/* Point to the current frame */` |
|       90 | 10185 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10186 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10187 | `	if( pFrame->pParent == 0 ){` |
|        - | 10188 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10189 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10190 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10191 | `		return SXRET_OK;` |
|        - | 10192 | `	}` |
|        - | 10193 | `	/* Create a new array */` |
|       90 | 10194 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10195 | `	if( pArray == 0 ){` |
|      ! 0 | 10196 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10197 | `		SXUNUSED(apArg);` |
|      ! 0 | 10198 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10199 | `		return SXRET_OK;` |
|        - | 10200 | `	}` |
|        - | 10201 | `	/* Start filling the array with the given arguments */` |
|       90 | 10202 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10203 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10204 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10205 | `		if( pObj ){` |
|      134 | 10206 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10207 | `		}` |
|       68 | 10208 | `	}` |
|        - | 10209 | `	/* Return the freshly created array */` |
|       90 | 10210 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10211 | `	return SXRET_OK;` |
|       46 | 10212 |  |
|        - | 10213 | `/*` |
|        - | 10214 | ` * bool function_exists(string $name)` |
|        - | 10215 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10216 | ` * Parameters` |
|        - | 10217 | ` *  The name of the desired function.` |
|        - | 10218 | ` * Return` |
|        - | 10219 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10220 | ` */` |
|     1680 | 10221 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10222 |  |
|        - | 10223 | `	const char *zName;` |
|        - | 10224 | `	ph7_vm *pVm;` |
|        - | 10225 | `	int nLen;` |
|        - | 10226 | `	int res;` |
|     1682 | 10227 | `	if( nArg < 1 ){` |
|        - | 10228 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10229 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10230 | `		return SXRET_OK;` |
|        - | 10231 | `	}` |
|        - | 10232 | `	/* Point to the target VM */` |
|     1682 | 10233 | `	pVm = pCtx->pVm;` |
|        - | 10234 | `	/* Extract the function name */` |
|     1682 | 10235 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10236 | `	/* Assume the function is not defined */` |
|     1682 | 10237 | `	res = 0;` |
|        - | 10238 | `	/* Perform the lookup */` |
|     2520 | 10239 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 | 10240 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10241 | `			/* Function is defined */` |
|      206 | 10242 | `			res = 1;` |
|      102 | 10243 | `	}` |
|     1682 | 10244 | `	ph7_result_bool(pCtx,res);` |
|     1682 | 10245 | `	return SXRET_OK;` |
|      842 | 10246 |  |
|        - | 10247 | `/*` |
|        - | 10248 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10249 | ` * [i.e: Whether it is callable or not].` |
|        - | 10250 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10251 | ` */` |
|    20516 | 10252 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 10253 |  |
|    20518 | 10254 | `	int res = 0;` |
|    20518 | 10255 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10256 | `		/* Call the magic method __invoke if available */` |
|      ! 0 | 10257 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 10258 | `		ph7_class_method *pMethod;` |
|      ! 0 | 10259 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 | 10260 | `		if( pMethod && CallInvoke ){` |
|        - | 10261 | `			ph7_value sResult;` |
|        - | 10262 | `			sxi32 rc;` |
|        - | 10263 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 | 10264 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 | 10265 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 | 10266 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 | 10267 | `				res = sResult.x.iVal != 0;` |
|      ! 0 | 10268 | `			}` |
|      ! 0 | 10269 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10270 | `		}` |
|    20518 | 10271 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 10272 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 10273 | `		if( pMap->nEntry == 2 ){` |
|        - | 10274 | `			ph7_class *pClass;` |
|        - | 10275 | `			ph7_value *pV;` |
|        - | 10276 | `			/* Extract the target class */` |
|       12 | 10277 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 10278 | `			if( pV ){` |
|       12 | 10279 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 10280 | `				if( pClass ){` |
|        - | 10281 | `					ph7_class_method *pMethod;` |
|        - | 10282 | `					/* Extract the target method */` |
|       10 | 10283 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 10284 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 10285 | `						/* Perform the lookup */` |
|       10 | 10286 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 10287 | `						if( pMethod ){` |
|        - | 10288 | `							/* Method is callable */` |
|        5 | 10289 | `							res = 1;` |
|        2 | 10290 | `						}` |
|        4 | 10291 | `					}` |
|        4 | 10292 | `				}` |
|        5 | 10293 | `			}` |
|        7 | 10294 | `		}` |
|    20505 | 10295 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 10296 | `		const char *zName;` |
|        - | 10297 | `		int nLen;` |
|        - | 10298 | `		/* Extract the name */` |
|     5444 | 10299 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 10300 | `		/* Perform the lookup */` |
|     5459 | 10301 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 10302 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10303 | `				/* Function is callable */` |
|     5426 | 10304 | `				res = 1;` |
|     2712 | 10305 | `		}` |
|     2721 | 10306 | `	}` |
|    20518 | 10307 | `	return res;` |
|        2 | 10308 |  |
|        - | 10309 | `/*` |
|        - | 10310 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 10311 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10312 | ` * Parameters` |
|        - | 10313 | ` * $name` |
|        - | 10314 | ` *    The callback function to check` |
|        - | 10315 | ` * $syntax_only` |
|        - | 10316 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 10317 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 10318 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 10319 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 10320 | ` *    a string.` |
|        - | 10321 | ` * Return` |
|        - | 10322 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 10323 | ` */` |
|       14 | 10324 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10325 |  |
|        - | 10326 | `	ph7_vm *pVm;` |
|        - | 10327 | `	int res;` |
|       15 | 10328 | `	if( nArg < 1 ){` |
|        - | 10329 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 10330 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10331 | `		return SXRET_OK;` |
|        - | 10332 | `	}` |
|        - | 10333 | `	/* Point to the target VM */` |
|       15 | 10334 | `	pVm = pCtx->pVm;` |
|        - | 10335 | `	/* Perform the requested operation */` |
|       15 | 10336 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 | 10337 | `	ph7_result_bool(pCtx,res);` |
|       15 | 10338 | `	return SXRET_OK;` |
|        8 | 10339 |  |
|        - | 10340 | `/*` |
|        - | 10341 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 10342 | ` * defined below.` |
|        - | 10343 | ` */` |
|     1218 | 10344 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10345 |  |
|     1219 | 10346 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10347 | `	ph7_value sName;` |
|        - | 10348 | `	sxi32 rc;` |
|        - | 10349 | `	/* Prepare the function name for insertion */` |
|     1219 | 10350 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1219 | 10351 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10352 | `	/* Perform the insertion */` |
|     1219 | 10353 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1219 | 10354 | `	PH7_MemObjRelease(&sName);` |
|     1219 | 10355 | `	return rc;` |
|        1 | 10356 |  |
|        - | 10357 | `/*` |
|        - | 10358 | ` * array get_defined_functions(void)` |
|        - | 10359 | ` *  Returns an array of all defined functions.` |
|        - | 10360 | ` * Parameter` |
|        - | 10361 | ` *  None.` |
|        - | 10362 | ` * Return` |
|        - | 10363 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10364 | ` *  both built-in (internal) and user-defined.` |
|        - | 10365 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10366 | ` *  defined ones using $arr["user"].` |
|        - | 10367 | ` * Note:` |
|        - | 10368 | ` *  NULL is returned on failure.` |
|        - | 10369 | ` */` |
|        2 | 10370 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10371 |  |
|        - | 10372 | `	ph7_value *pArray,*pEntry;` |
|        - | 10373 | `	/* NOTE:` |
|        - | 10374 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10375 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10376 | `	 */` |
|        3 | 10377 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10378 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10379 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10380 | `		SXUNUSED(apArg);` |
|        - | 10381 | `		/* Return NULL */` |
|      ! 0 | 10382 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10383 | `		return SXRET_OK;` |
|        - | 10384 | `	}` |
|        3 | 10385 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10386 | `	if( pEntry == 0 ){` |
|        - | 10387 | `		/* Return NULL */` |
|      ! 0 | 10388 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10389 | `		return SXRET_OK;` |
|        - | 10390 | `	}` |
|        - | 10391 | `	/* Fill with the appropriate information */` |
|        3 | 10392 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10393 | `	/* Create the 'internal' index */` |
|        3 | 10394 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10395 | `	/* Create the user-func array */` |
|        3 | 10396 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10397 | `	if( pEntry == 0 ){` |
|        - | 10398 | `		/* Return NULL */` |
|      ! 0 | 10399 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10400 | `		return SXRET_OK;` |
|        - | 10401 | `	}` |
|        - | 10402 | `	/* Fill with the appropriate information */` |
|        3 | 10403 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10404 | `	/* Create the 'user' index */` |
|        3 | 10405 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10406 | `	/* Return the multi-dimensional array */` |
|        3 | 10407 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10408 | `	return SXRET_OK;` |
|        2 | 10409 |  |
|        - | 10410 | `/*` |
|        - | 10411 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10412 | ` *  Register a function for execution on shutdown.` |
|        - | 10413 | ` * Note` |
|        - | 10414 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10415 | ` *  be called in the same order as they were registered.` |
|        - | 10416 | ` * Parameters` |
|        - | 10417 | ` *  $callback` |
|        - | 10418 | ` *   The shutdown callback to register.` |
|        - | 10419 | ` * $param` |
|        - | 10420 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10421 | ` * Return` |
|        - | 10422 | ` *  Nothing.` |
|        - | 10423 | ` */` |
|        2 | 10424 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10425 |  |
|        - | 10426 | `	VmShutdownCB sEntry;` |
|        - | 10427 | `	int i,j;` |
|        3 | 10428 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10429 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10430 | `		return PH7_OK;` |
|        - | 10431 | `	}` |
|        - | 10432 | `	/* Zero the Entry */` |
|        3 | 10433 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10434 | `	/* Initialize fields */` |
|        3 | 10435 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10436 | `	/* Save the callback name for later invocation name */` |
|        3 | 10437 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10438 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10439 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10440 | `	}` |
|        - | 10441 | `	/* Copy arguments */` |
|        3 | 10442 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10443 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10444 | `			/* Limit reached */` |
|      ! 0 | 10445 | `			break;` |
|        - | 10446 | `		}` |
|      ! 0 | 10447 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10448 | `	}` |
|        3 | 10449 | `	sEntry.nArg = j;` |
|        - | 10450 | `	/* Install the callback */` |
|        3 | 10451 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10452 | `	return PH7_OK;` |
|        2 | 10453 |  |
|        - | 10454 | `/*` |
|        - | 10455 | ` * Section:` |
|        - | 10456 | ` *  Class handling functions.` |
|        - | 10457 | ` * Status:` |
|        - | 10458 | ` *    Stable.` |
|        - | 10459 | ` */` |
|        - | 10460 | `/*` |
|        - | 10461 | ` * Extract the top active class. NULL is returned` |
|        - | 10462 | ` * if the class stack is empty.` |
|        - | 10463 | ` */` |
|      792 | 10464 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10465 |  |
|      794 | 10466 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10467 | `	ph7_class **apClass;` |
|      794 | 10468 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10469 | `		/* Empty stack,return NULL */` |
|       15 | 10470 | `		return 0;` |
|        - | 10471 | `	}` |
|        - | 10472 | `	/* Peek the last entry */` |
|      780 | 10473 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      780 | 10474 | `	return apClass[pSet->nUsed - 1];` |
|      398 | 10475 |  |
|        - | 10476 | `/*` |
|        - | 10477 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10478 | ` *   Get the class that declared the currently executing method.` |
|        - | 10479 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10480 | ` *` |
|        - | 10481 | ` * Parameters` |
|        - | 10482 | ` *   pVm: Target VM` |
|        - | 10483 | ` *` |
|        - | 10484 | ` * Return` |
|        - | 10485 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10486 | ` *   - Not executing within a class method` |
|        - | 10487 | ` *` |
|        - | 10488 | ` * Note` |
|        - | 10489 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10490 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10491 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10492 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10493 | ` *   declaring class.` |
|        - | 10494 | ` */` |
|       98 | 10495 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10496 |  |
|      100 | 10497 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10498 | `	ph7_vm_func *pVmFunc;` |
|        - | 10499 |  |
|        - | 10500 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10501 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10502 |  |
|        - | 10503 | `	/* Check if we're in a method context */` |
|      100 | 10504 | `	if( pFrame->pParent ){` |
|       96 | 10505 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10506 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10507 | `			/* Return the declaring class */` |
|       96 | 10508 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10509 | `		}` |
|      ! 0 | 10510 | `	}` |
|        - | 10511 |  |
|        5 | 10512 | `	return 0;` |
|       51 | 10513 |  |
|        - | 10514 |  |
|        - | 10515 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10516 | `/*` |
|        - | 10517 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10518 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10519 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10520 | ` * return value indicates failure.` |
|        - | 10521 | ` */` |
|        - | 10522 | `/*` |
|        - | 10523 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10524 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10525 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10526 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10527 | ` */` |
|     1840 | 10528 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10529 | `	ph7_vm *pVm,` |
|        - | 10530 | `	ph7_class_instance *pThis,` |
|        - | 10531 | `	ph7_class_method *pMethod,` |
|        - | 10532 | `	ph7_value *pResult,` |
|        - | 10533 | `	int nArg,` |
|        - | 10534 | `	ph7_value **apArg,` |
|        - | 10535 | `	VmCallArgMap *pMap` |
|        - | 10536 | `	)` |
|        2 | 10537 |  |
|        - | 10538 | `	ph7_value *aStack;` |
|        - | 10539 | `	VmInstr aInstr[2];` |
|        - | 10540 | `	int iCursor;` |
|        - | 10541 | `	int i;` |
|     1842 | 10542 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1842 | 10543 | `	if( aStack == 0 ){` |
|      ! 0 | 10544 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10545 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10546 | `		return SXERR_MEM;` |
|        - | 10547 | `	}` |
|     2732 | 10548 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      892 | 10549 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      892 | 10550 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      447 | 10551 | `	}` |
|     1842 | 10552 | `	iCursor = nArg + 1;` |
|     1842 | 10553 | `	if( pThis ){` |
|     1836 | 10554 | `		pThis->iRef++;` |
|     1836 | 10555 | `		aStack[i].x.pOther = pThis;` |
|     1836 | 10556 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      917 | 10557 | `	}` |
|     1842 | 10558 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1842 | 10559 | `	i++;` |
|     1842 | 10560 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1842 | 10561 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1842 | 10562 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1842 | 10563 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1842 | 10564 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1842 | 10565 | `	aInstr[0].iP1 = nArg;` |
|     1842 | 10566 | `	aInstr[0].iP2 = 0;` |
|     1842 | 10567 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1842 | 10568 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1842 | 10569 | `	aInstr[1].iP1 = 1;` |
|     1842 | 10570 | `	aInstr[1].iP2 = 0;` |
|     1842 | 10571 | `	aInstr[1].p3  = 0;` |
|     1842 | 10572 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     1842 | 10573 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1842 | 10574 | `	return PH7_OK;` |
|      922 | 10575 |  |
|     1536 | 10576 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10577 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10578 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10579 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10580 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10581 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10582 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10583 | `	)` |
|        2 | 10584 |  |
|     1538 | 10585 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10586 |  |
|        - | 10587 | `/*` |
|        - | 10588 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10589 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10590 | ` * in the apArg[] array.` |
|        - | 10591 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10592 | ` * return value indicates failure.` |
|        - | 10593 | ` */` |
|      980 | 10594 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10595 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10596 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10597 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10598 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10599 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10600 | `	)` |
|        2 | 10601 |  |
|        - | 10602 | `	ph7_value *aStack;` |
|        - | 10603 | `	VmInstr aInstr[2];` |
|        - | 10604 | `	int i;` |
|      982 | 10605 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10606 | `		/* Don't bother processing,it's invalid anyway */` |
|      491 | 10607 | `		if( pResult ){` |
|        - | 10608 | `			/* Assume a null return value */` |
|      ! 0 | 10609 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10610 | `		}` |
|      491 | 10611 | `		return SXERR_INVALID;` |
|        - | 10612 | `	}` |
|      492 | 10613 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10614 | `		/* Class method */` |
|       11 | 10615 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10616 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10617 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10618 | `		ph7_class *pClass = 0;` |
|        - | 10619 | `		ph7_value *pValue;` |
|        - | 10620 | `		sxi32 rc;` |
|       11 | 10621 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10622 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10623 | `			if( pResult ){` |
|        - | 10624 | `				/* Assume a null return value */` |
|      ! 0 | 10625 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10626 | `			}` |
|      ! 0 | 10627 | `			return SXRET_OK;` |
|        - | 10628 | `		}` |
|        - | 10629 | `		/* Extract the class name or an instance of it */` |
|       11 | 10630 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10631 | `		if( pValue ){` |
|       11 | 10632 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10633 | `		}` |
|       11 | 10634 | `		if( pClass == 0 ){` |
|        - | 10635 | `			/* No such class,return NULL */` |
|      ! 0 | 10636 | `			if( pResult ){` |
|      ! 0 | 10637 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10638 | `			}` |
|      ! 0 | 10639 | `			return SXRET_OK;` |
|        - | 10640 | `		}` |
|       11 | 10641 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10642 | `			/* Point to the class instance */` |
|        5 | 10643 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10644 | `		}` |
|        - | 10645 | `		/* Try to extract the method */` |
|       11 | 10646 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10647 | `		if( pValue ){` |
|       11 | 10648 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10649 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10650 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10651 | `			}` |
|        5 | 10652 | `		}` |
|       11 | 10653 | `		if( pMethod == 0 ){` |
|        - | 10654 | `			/* No such method,return NULL */` |
|      ! 0 | 10655 | `			if( pResult ){` |
|      ! 0 | 10656 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10657 | `			}` |
|      ! 0 | 10658 | `			return SXRET_OK;` |
|        - | 10659 | `		}` |
|        - | 10660 | `		/* Call the class method */` |
|       11 | 10661 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10662 | `		return rc;` |
|        - | 10663 | `	}` |
|        - | 10664 | `	/* Create a new operand stack */` |
|      482 | 10665 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      482 | 10666 | `	if( aStack == 0 ){` |
|      ! 0 | 10667 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10668 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10669 | `		if( pResult ){` |
|        - | 10670 | `			/* Assume a null return value */` |
|      ! 0 | 10671 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10672 | `		}` |
|      ! 0 | 10673 | `		return SXERR_MEM;` |
|        - | 10674 | `	}` |
|        - | 10675 | `	/* Fill the operand stack with the given arguments */` |
|     1544 | 10676 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1064 | 10677 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10678 | `		/*` |
|        - | 10679 | `		 * Symisc eXtension:` |
|        - | 10680 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10681 | `		 */` |
|     1064 | 10682 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      533 | 10683 | `	}` |
|        - | 10684 | `	/* Push the function name */` |
|      482 | 10685 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      482 | 10686 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10687 | `	/* Emit the CALL istruction */` |
|      482 | 10688 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      482 | 10689 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      482 | 10690 | `	aInstr[0].iP2 = 0;` |
|      482 | 10691 | `	aInstr[0].p3  = 0;` |
|        - | 10692 | `	/* Emit the DONE instruction */` |
|      482 | 10693 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      482 | 10694 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      482 | 10695 | `	aInstr[1].iP2 = 0;` |
|      482 | 10696 | `	aInstr[1].p3  = 0;` |
|        - | 10697 | `	/* Execute the function body (if available) */` |
|      482 | 10698 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 10699 | `	/* Clean up the mess left behind */` |
|      482 | 10700 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      482 | 10701 | `	return PH7_OK;` |
|      492 | 10702 |  |
|        - | 10703 | `/*` |
|        - | 10704 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10705 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10706 | ` * parameter.` |
|        - | 10707 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10708 | ` * return value indicates failure.` |
|        - | 10709 | ` */` |
|      236 | 10710 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10711 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10712 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10713 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10714 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10715 | `	)` |
|        1 | 10716 |  |
|        - | 10717 | `	ph7_value *pArg;` |
|        - | 10718 | `	SySet aArg;` |
|        - | 10719 | `	va_list ap;` |
|        - | 10720 | `	sxi32 rc;` |
|      237 | 10721 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10722 | `	/* Copy arguments one after one */` |
|      237 | 10723 | `	va_start(ap,pResult);` |
|      393 | 10724 | `	for(;;){` |
|      787 | 10725 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10726 | `		if( pArg == 0 ){` |
|      237 | 10727 | `			break;` |
|        - | 10728 | `		}` |
|      551 | 10729 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10730 | `	}` |
|        - | 10731 | `	/* Call the core routine */` |
|      237 | 10732 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10733 | `	/* Cleanup */` |
|      237 | 10734 | `	SySetRelease(&aArg);` |
|      237 | 10735 | `	return rc;` |
|        1 | 10736 |  |
|        - | 10737 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10738 | `/*` |
|        - | 10739 | ` * bool defined(string $name)` |
|        - | 10740 | ` *  Checks whether a given named constant exists.` |
|        - | 10741 | ` * Parameter:` |
|        - | 10742 | ` *  Name of the desired constant.` |
|        - | 10743 | ` * Return` |
|        - | 10744 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10745 | ` */` |
|       14 | 10746 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10747 |  |
|        - | 10748 | `	const char *zName;` |
|       16 | 10749 | `	int nLen = 0;` |
|       16 | 10750 | `	int res = 0;` |
|       16 | 10751 | `	if( nArg < 1 ){` |
|        - | 10752 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10753 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10754 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10755 | `		return SXRET_OK;` |
|        - | 10756 | `	}` |
|        - | 10757 | `	/* Extract constant name */` |
|       16 | 10758 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10759 | `	/* Perform the lookup */` |
|       16 | 10760 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10761 | `		/* Already defined */` |
|       10 | 10762 | `		res = 1;` |
|        4 | 10763 | `	}` |
|       16 | 10764 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10765 | `	return SXRET_OK;` |
|        9 | 10766 |  |
|        - | 10767 | `/*` |
|        - | 10768 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10769 | ` * below.` |
|        - | 10770 | ` */` |
|       10 | 10771 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10772 |  |
|       12 | 10773 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10774 | `	/* Expand constant value */` |
|       12 | 10775 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10776 |  |
|        - | 10777 | `/*` |
|        - | 10778 | ` * bool define(string $constant_name,expression value)` |
|        - | 10779 | ` *  Defines a named constant at runtime.` |
|        - | 10780 | ` * Parameter:` |
|        - | 10781 | ` *  $constant_name` |
|        - | 10782 | ` *   The name of the constant` |
|        - | 10783 | ` *  $value` |
|        - | 10784 | ` *   Constant value` |
|        - | 10785 | ` * Return:` |
|        - | 10786 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10787 | ` */` |
|       12 | 10788 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10789 |  |
|        - | 10790 | `	const char *zName;  /* Constant name */` |
|        - | 10791 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10792 | `	int nLen = 0;       /* Name length */` |
|        - | 10793 | `	sxi32 rc;` |
|       14 | 10794 | `	if( nArg < 2 ){` |
|        - | 10795 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10796 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10797 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10798 | `		return SXRET_OK;` |
|        - | 10799 | `	}` |
|       14 | 10800 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10801 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10802 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10803 | `		return SXRET_OK;` |
|        - | 10804 | `	}` |
|        - | 10805 | `	/* Extract constant name */` |
|       14 | 10806 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10807 | `	if( nLen < 1 ){` |
|      ! 0 | 10808 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10809 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10810 | `		return SXRET_OK;` |
|        - | 10811 | `	}` |
|        - | 10812 | `	/* Duplicate constant value */` |
|       14 | 10813 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10814 | `	if( pValue == 0 ){` |
|      ! 0 | 10815 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10816 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10817 | `		return SXRET_OK;` |
|        - | 10818 | `	}` |
|        - | 10819 | `	/* Initialize the memory object */` |
|       14 | 10820 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10821 | `	/* Register the constant */` |
|       14 | 10822 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10823 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10824 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10825 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10826 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10827 | `		return SXRET_OK;` |
|        - | 10828 | `	}` |
|        - | 10829 | `	/* Duplicate constant value */` |
|       14 | 10830 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10831 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 10832 | `		/* Lower case the constant name */` |
|      ! 0 | 10833 | `		char *zCur = (char *)zName;` |
|      ! 0 | 10834 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 10835 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 10836 | `				/* UTF-8 stream */` |
|      ! 0 | 10837 | `				zCur++;` |
|      ! 0 | 10838 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 10839 | `					zCur++;` |
|      ! 0 | 10840 | `				}` |
|      ! 0 | 10841 | `				continue;` |
|        - | 10842 | `			}` |
|      ! 0 | 10843 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 10844 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 10845 | `				zCur[0] = (char)c;` |
|      ! 0 | 10846 | `			}` |
|      ! 0 | 10847 | `			zCur++;` |
|      ! 0 | 10848 | `		}` |
|        - | 10849 | `		/* Finally,register the constant */` |
|      ! 0 | 10850 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 10851 | `	}` |
|        - | 10852 | `	/* All done,return TRUE */` |
|       14 | 10853 | `	ph7_result_bool(pCtx,1);` |
|       14 | 10854 | `	return SXRET_OK;` |
|        8 | 10855 |  |
|        - | 10856 | `/*` |
|        - | 10857 | ` * value constant(string $name)` |
|        - | 10858 | ` *  Returns the value of a constant` |
|        - | 10859 | ` * Parameter` |
|        - | 10860 | ` *  $name` |
|        - | 10861 | ` *    Name of the constant.` |
|        - | 10862 | ` * Return` |
|        - | 10863 | ` *  Constant value or NULL if not defined.` |
|        - | 10864 | ` */` |
|        8 | 10865 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10866 |  |
|        - | 10867 | `	SyHashEntry *pEntry;` |
|        - | 10868 | `	ph7_constant *pCons;` |
|        - | 10869 | `	const char *zName; /* Constant name */` |
|        - | 10870 | `	ph7_value sVal;    /* Constant value */` |
|        - | 10871 | `	int nLen;` |
|       10 | 10872 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10873 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 10874 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 10875 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10876 | `		return SXRET_OK;` |
|        - | 10877 | `	}` |
|        - | 10878 | `	/* Extract the constant name */` |
|       10 | 10879 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10880 | `	/* Perform the query */` |
|       10 | 10881 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 10882 | `	if( pEntry == 0 ){` |
|        3 | 10883 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 10884 | `		ph7_result_null(pCtx);` |
|        3 | 10885 | `		return SXRET_OK;` |
|        - | 10886 | `	}` |
|        8 | 10887 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 10888 | `	/* Point to the structure that describe the constant */` |
|        8 | 10889 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 10890 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 10891 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 10892 | `	/* Return that value */` |
|        8 | 10893 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 10894 | `	/* Cleanup */` |
|        8 | 10895 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 10896 | `	return SXRET_OK;` |
|        6 | 10897 |  |
|        - | 10898 | `/*` |
|        - | 10899 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 10900 | ` * defined below.` |
|        - | 10901 | ` */` |
|      452 | 10902 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10903 |  |
|      453 | 10904 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10905 | `	ph7_value sName;` |
|        - | 10906 | `	sxi32 rc;` |
|        - | 10907 | `	/* Prepare the constant name for insertion */` |
|      453 | 10908 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 10909 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10910 | `	/* Perform the insertion */` |
|      453 | 10911 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 10912 | `	PH7_MemObjRelease(&sName);` |
|      453 | 10913 | `	return rc;` |
|        1 | 10914 |  |
|        - | 10915 | `/*` |
|        - | 10916 | ` * array get_defined_constants(void)` |
|        - | 10917 | ` *  Returns an associative array with the names of all defined` |
|        - | 10918 | ` *  constants.` |
|        - | 10919 | ` * Parameters` |
|        - | 10920 | ` *  NONE.` |
|        - | 10921 | ` * Returns` |
|        - | 10922 | ` *  Returns the names of all the constants currently defined.` |
|        - | 10923 | ` */` |
|        2 | 10924 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10925 |  |
|        - | 10926 | `	ph7_value *pArray;` |
|        - | 10927 | `	/* Create the array first*/` |
|        3 | 10928 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10929 | `	if( pArray == 0 ){` |
|      ! 0 | 10930 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10931 | `		SXUNUSED(apArg);` |
|        - | 10932 | `		/* Return NULL */` |
|      ! 0 | 10933 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10934 | `		return SXRET_OK;` |
|        - | 10935 | `	}` |
|        - | 10936 | `	/* Fill the array with the defined constants */` |
|        3 | 10937 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 10938 | `	/* Return the created array */` |
|        3 | 10939 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10940 | `	return SXRET_OK;` |
|        2 | 10941 |  |
|        - | 10942 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 10943 | `/*` |
|        - | 10944 | ` * Section:` |
|        - | 10945 | ` *  Random numbers/string generators.` |
|        - | 10946 | ` * Status:` |
|        - | 10947 | ` *    Stable.` |
|        - | 10948 | ` */` |
|        - | 10949 | `/*` |
|        - | 10950 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 10951 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10952 | ` * used by te SQLite3 library.` |
|        - | 10953 | ` */` |
|     2696 | 10954 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10955 |  |
|        - | 10956 | `	sxu32 iNum;` |
|     2698 | 10957 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2698 | 10958 | `	return iNum;` |
|        2 | 10959 |  |
|        - | 10960 | `/*` |
|        - | 10961 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10962 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10963 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10964 | ` * by te SQLite3 library.` |
|        - | 10965 | ` */` |
|   191552 | 10966 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10967 |  |
|        - | 10968 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10969 | `	int i;` |
|        - | 10970 | `	/* Generate a binary string first */` |
|   191554 | 10971 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10972 | `	/* Turn the binary string into english based alphabet */` |
|  2107242 | 10973 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1915690 | 10974 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   957846 | 10975 | `	 }` |
|   191554 | 10976 |  |
|        - | 10977 | `/*` |
|        - | 10978 | ` * int rand()` |
|        - | 10979 | ` * int mt_rand()` |
|        - | 10980 | ` * int rand(int $min,int $max)` |
|        - | 10981 | ` * int mt_rand(int $min,int $max)` |
|        - | 10982 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10983 | ` * Parameter` |
|        - | 10984 | ` *  $min` |
|        - | 10985 | ` *    The lowest value to return (default: 0)` |
|        - | 10986 | ` *  $max` |
|        - | 10987 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10988 | ` * Return` |
|        - | 10989 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10990 | ` * Note:` |
|        - | 10991 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10992 | ` *  by te SQLite3 library.` |
|        - | 10993 | ` */` |
|       20 | 10994 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10995 |  |
|        - | 10996 | `	sxu32 iNum;` |
|        - | 10997 | `	/* Generate the random number */` |
|       21 | 10998 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10999 | `	if( nArg > 1 ){` |
|        - | 11000 | `		sxu32 iMin,iMax;` |
|        3 | 11001 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11002 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11003 | `		if( iMin < iMax ){` |
|        3 | 11004 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11005 | `			if( iDiv > 0 ){` |
|        3 | 11006 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11007 | `			}` |
|        1 | 11008 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11009 | `			iNum %= iMax;` |
|      ! 0 | 11010 | `		}` |
|        1 | 11011 | `	}` |
|        - | 11012 | `	/* Return the number */` |
|       21 | 11013 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11014 | `	return SXRET_OK;` |
|        1 | 11015 |  |
|        - | 11016 | `/*` |
|        - | 11017 | ` * int getrandmax(void)` |
|        - | 11018 | ` * int mt_getrandmax(void)` |
|        - | 11019 | ` * int rc4_getrandmax(void)` |
|        - | 11020 | ` *   Show largest possible random value` |
|        - | 11021 | ` * Return` |
|        - | 11022 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11023 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11024 | ` * Note:` |
|        - | 11025 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11026 | ` *  by te SQLite3 library.` |
|        - | 11027 | ` */` |
|        4 | 11028 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11029 |  |
|        2 | 11030 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11031 | `	SXUNUSED(apArg);` |
|        5 | 11032 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11033 | `	return SXRET_OK;` |
|        1 | 11034 |  |
|        - | 11035 | `/*` |
|        - | 11036 | ` * string rand_str()` |
|        - | 11037 | ` * string rand_str(int $len)` |
|        - | 11038 | ` *  Generate a random string (English alphabet).` |
|        - | 11039 | ` * Parameter` |
|        - | 11040 | ` *  $len` |
|        - | 11041 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11042 | ` * Return` |
|        - | 11043 | ` *   A pseudo random string.` |
|        - | 11044 | ` * Note:` |
|        - | 11045 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11046 | ` *  by te SQLite3 library.` |
|        - | 11047 | ` *  This function is a symisc extension.` |
|        - | 11048 | ` */` |
|      120 | 11049 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11050 |  |
|        - | 11051 | `	char zString[1024];` |
|      122 | 11052 | `	int iLen = 0x10;` |
|      122 | 11053 | `	if( nArg > 0 ){` |
|        - | 11054 | `		/* Get the desired length */` |
|      122 | 11055 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11056 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11057 | `			/* Default length */` |
|        3 | 11058 | `			iLen = 0x10;` |
|        1 | 11059 | `		}` |
|       60 | 11060 | `	}` |
|        - | 11061 | `	/* Generate the random string */` |
|      122 | 11062 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11063 | `	/* Return the generated string */` |
|      122 | 11064 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11065 | `	return SXRET_OK;` |
|        2 | 11066 |  |
|        - | 11067 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11068 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11069 | `/* Unique ID private data */` |
|        - | 11070 | `struct unique_id_data` |
|        - | 11071 |  |
|        - | 11072 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11073 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 11074 | `};` |
|        - | 11075 | `/*` |
|        - | 11076 | ` * Binary to hex consumer callback.` |
|        - | 11077 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 11078 | ` * defined below.` |
|        - | 11079 | ` */` |
|      192 | 11080 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 11081 |  |
|      193 | 11082 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 11083 | `	sxu32 nBuflen;` |
|        - | 11084 | `	/* Extract result buffer length */` |
|      193 | 11085 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 11086 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 11087 | `			/*` |
|        - | 11088 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 11089 | `			 * string will be 13 characters long` |
|        - | 11090 | `			 */` |
|       25 | 11091 | `		return SXERR_ABORT;` |
|        - | 11092 | `	}` |
|      169 | 11093 | `	if( nBuflen > 22 ){` |
|      ! 0 | 11094 | `		return SXERR_ABORT;` |
|        - | 11095 | `	}` |
|        - | 11096 | `	/* Safely Consume the hex stream */` |
|      169 | 11097 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 11098 | `	return SXRET_OK;` |
|       97 | 11099 |  |
|        - | 11100 | `/*` |
|        - | 11101 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 11102 | ` *  Generate a unique ID` |
|        - | 11103 | ` * Parameter` |
|        - | 11104 | ` * $prefix` |
|        - | 11105 | ` *  Append this prefix to the generated unique ID.` |
|        - | 11106 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 11107 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 11108 | ` * $more_entropy` |
|        - | 11109 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 11110 | ` *  that the result will be unique.` |
|        - | 11111 | ` * Return` |
|        - | 11112 | ` *  Returns the unique identifier, as a string.` |
|        - | 11113 | ` */` |
|       24 | 11114 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11115 |  |
|        - | 11116 | `	struct unique_id_data sUniq;` |
|        - | 11117 | `	unsigned char zDigest[20];` |
|       25 | 11118 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11119 | `	const char *zPrefix;` |
|        - | 11120 | `	SHA1Context sCtx;` |
|        - | 11121 | `	char zRandom[7];` |
|        - | 11122 | `	int nPrefix;` |
|        - | 11123 | `	int entropy;` |
|        - | 11124 | `	/* Generate a random string first */` |
|       25 | 11125 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 11126 | `	/* Initialize fields */` |
|       25 | 11127 | `	zPrefix = 0;` |
|       25 | 11128 | `	nPrefix = 0;` |
|       25 | 11129 | `	entropy = 0;` |
|       25 | 11130 | `	if( nArg > 0 ){` |
|        - | 11131 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 11132 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 11133 | `		if( nArg > 1 ){` |
|      ! 0 | 11134 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11135 | `		}` |
|      ! 0 | 11136 | `	}` |
|       25 | 11137 | `	SHA1Init(&sCtx);` |
|        - | 11138 | `	/* Generate the random ID */` |
|       25 | 11139 | `	if( nPrefix > 0 ){` |
|      ! 0 | 11140 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 11141 | `	}` |
|        - | 11142 | `	/* Append the random ID */` |
|       25 | 11143 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 11144 | `	/* Append the random string */` |
|       25 | 11145 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 11146 | `	/* Increment the number */` |
|       25 | 11147 | `	pVm->unique_id++;` |
|       25 | 11148 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 11149 | `	/* Hexify the digest */` |
|       25 | 11150 | `	sUniq.pCtx = pCtx;` |
|       25 | 11151 | `	sUniq.entropy = entropy;` |
|       25 | 11152 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 11153 | `	/* All done */` |
|       25 | 11154 | `	return PH7_OK;` |
|        1 | 11155 |  |
|        - | 11156 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11157 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11158 | `/*` |
|        - | 11159 | ` * Section:` |
|        - | 11160 | ` *  Language construct implementation as foreign functions.` |
|        - | 11161 | ` * Status:` |
|        - | 11162 | ` *    Stable.` |
|        - | 11163 | ` */` |
|        - | 11164 | `/*` |
|        - | 11165 | ` * void echo($string...)` |
|        - | 11166 | ` *  Output one or more messages.` |
|        - | 11167 | ` * Parameters` |
|        - | 11168 | ` *  $string` |
|        - | 11169 | ` *   Message to output.` |
|        - | 11170 | ` * Return` |
|        - | 11171 | ` *  NULL.` |
|        - | 11172 | ` */` |
|      ! 0 | 11173 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11174 |  |
|        - | 11175 | `	const char *zData;` |
|      ! 0 | 11176 | `	int nDataLen = 0;` |
|        - | 11177 | `	ph7_vm *pVm;` |
|        - | 11178 | `	int i,rc;` |
|        - | 11179 | `	/* Point to the target VM */` |
|      ! 0 | 11180 | `	pVm = pCtx->pVm;` |
|        - | 11181 | `	/* Output */` |
|      ! 0 | 11182 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 11183 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 11184 | `		if( nDataLen > 0 ){` |
|      ! 0 | 11185 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 11186 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 11187 | `			if( rc == SXERR_ABORT ){` |
|        - | 11188 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11189 | `				return PH7_ABORT;` |
|        - | 11190 | `			}` |
|      ! 0 | 11191 | `		}` |
|      ! 0 | 11192 | `	}` |
|      ! 0 | 11193 | `	return SXRET_OK;` |
|      ! 0 | 11194 |  |
|        - | 11195 | `/*` |
|        - | 11196 | ` * int print($string...)` |
|        - | 11197 | ` *  Output one or more messages.` |
|        - | 11198 | ` * Parameters` |
|        - | 11199 | ` *  $string` |
|        - | 11200 | ` *   Message to output.` |
|        - | 11201 | ` * Return` |
|        - | 11202 | ` *  1 always.` |
|        - | 11203 | ` */` |
|        2 | 11204 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11205 |  |
|        - | 11206 | `	const char *zData;` |
|        3 | 11207 | `	int nDataLen = 0;` |
|        - | 11208 | `	ph7_vm *pVm;` |
|        - | 11209 | `	int i,rc;` |
|        - | 11210 | `	/* Point to the target VM */` |
|        3 | 11211 | `	pVm = pCtx->pVm;` |
|        - | 11212 | `	/* Output */` |
|        5 | 11213 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 11214 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 11215 | `		if( nDataLen > 0 ){` |
|        3 | 11216 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 11217 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 11218 | `			if( rc == SXERR_ABORT ){` |
|        - | 11219 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11220 | `				return PH7_ABORT;` |
|        - | 11221 | `			}` |
|        1 | 11222 | `		}` |
|        2 | 11223 | `	}` |
|        - | 11224 | `	/* Return 1 */` |
|        3 | 11225 | `	ph7_result_int(pCtx,1);` |
|        3 | 11226 | `	return SXRET_OK;` |
|        2 | 11227 |  |
|        - | 11228 | `/*` |
|        - | 11229 | ` * void exit(string $msg)` |
|        - | 11230 | ` * void exit(int $status)` |
|        - | 11231 | ` * void die(string $ms)` |
|        - | 11232 | ` * void die(int $status)` |
|        - | 11233 | ` *   Output a message and terminate program execution.` |
|        - | 11234 | ` * Parameter` |
|        - | 11235 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 11236 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 11237 | ` *  and not printed` |
|        - | 11238 | ` * Return` |
|        - | 11239 | ` *  NULL` |
|        - | 11240 | ` */` |
|      ! 0 | 11241 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11242 |  |
|      ! 0 | 11243 | `	if( nArg > 0 ){` |
|      ! 0 | 11244 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 11245 | `			const char *zData;` |
|      ! 0 | 11246 | `			int iLen = 0;` |
|        - | 11247 | `			/* Print exit message */` |
|      ! 0 | 11248 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 11249 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 11250 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 11251 | `			sxi32 iExitStatus;` |
|        - | 11252 | `			/* Record exit status code */` |
|      ! 0 | 11253 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 11254 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 11255 | `		}` |
|      ! 0 | 11256 | `	}` |
|        - | 11257 | `	/* Check if we are in an included file */` |
|      ! 0 | 11258 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 11259 | `		/* Exit the entire process */` |
|      ! 0 | 11260 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 11261 | `	}` |
|        - | 11262 | `	/* Abort processing immediately */` |
|      ! 0 | 11263 | `	return PH7_ABORT;` |
|      ! 0 | 11264 |  |
|        - | 11265 | `/*` |
|        - | 11266 | ` * bool isset($var,...)` |
|        - | 11267 | ` *  Finds out whether a variable is set.` |
|        - | 11268 | ` * Parameters` |
|        - | 11269 | ` *  One or more variable to check.` |
|        - | 11270 | ` * Return` |
|        - | 11271 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 11272 | ` */` |
|    85934 | 11273 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11274 |  |
|        - | 11275 | `	ph7_value *pObj;` |
|    85936 | 11276 | `	int res = 0;` |
|        - | 11277 | `	int i;` |
|    85936 | 11278 | `	if( nArg < 1 ){` |
|        - | 11279 | `		/* Missing arguments,return false */` |
|      ! 0 | 11280 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 11281 | `		return SXRET_OK;` |
|        - | 11282 | `	}` |
|        - | 11283 | `	/* Iterate over available arguments */` |
|   112514 | 11284 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    85936 | 11285 | `		pObj = apArg[i];` |
|    85936 | 11286 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    58584 | 11287 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11288 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 11289 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 11290 | `			}` |
|    29291 | 11291 | `		}` |
|    85936 | 11292 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    85936 | 11293 | `		if( !res ){` |
|        - | 11294 | `			/* Variable not set,return FALSE */` |
|    59358 | 11295 | `			ph7_result_bool(pCtx,0);` |
|    59358 | 11296 | `			return SXRET_OK;` |
|        - | 11297 | `		}` |
|    13291 | 11298 | `	}` |
|        - | 11299 | `	/* All given variable are set,return TRUE */` |
|    26580 | 11300 | `	ph7_result_bool(pCtx,1);` |
|    26580 | 11301 | `	return SXRET_OK;` |
|    42969 | 11302 |  |
|        - | 11303 | `/*` |
|        - | 11304 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 11305 | ` * frame,the reference table and discard it's contents.` |
|        - | 11306 | ` * This function never fail and always return SXRET_OK.` |
|        - | 11307 | ` */` |
|  3085378 | 11308 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 11309 |  |
|        - | 11310 | `	ph7_value *pObj;` |
|        - | 11311 | `	VmRefObj *pRef;` |
|  3085380 | 11312 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3085380 | 11313 | `	if( pObj ){` |
|        - | 11314 | `		/* Release the object */` |
|  3085380 | 11315 | `		PH7_MemObjRelease(pObj);` |
|  1542689 | 11316 | `	}` |
|        - | 11317 | `	/* Remove old reference links */` |
|  3085380 | 11318 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3085380 | 11319 | `	if( pRef ){` |
|  3085374 | 11320 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 11321 | `		/* Unlink from the reference table */` |
|  3085374 | 11322 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3085374 | 11323 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 11324 | `			VmSlot sFree;` |
|        - | 11325 | `			/* Restore to the free list */` |
|  3085366 | 11326 | `			sFree.nIdx = nObjIdx;` |
|  3085366 | 11327 | `			sFree.pUserData = 0;` |
|  3085366 | 11328 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1542682 | 11329 | `		}` |
|  1542686 | 11330 | `	}` |
|  3085380 | 11331 | `	return SXRET_OK;` |
|        2 | 11332 |  |
|        - | 11333 | `/*` |
|        - | 11334 | ` * void unset($var,...)` |
|        - | 11335 | ` *   Unset one or more given variable.` |
|        - | 11336 | ` * Parameters` |
|        - | 11337 | ` *  One or more variable to unset.` |
|        - | 11338 | ` * Return` |
|        - | 11339 | ` *  Nothing.` |
|        - | 11340 | ` */` |
|     7290 | 11341 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11342 |  |
|        - | 11343 | `	ph7_value *pObj;` |
|        - | 11344 | `	ph7_vm *pVm;` |
|        - | 11345 | `	int i;` |
|        - | 11346 | `	/* Point to the target VM */` |
|     7292 | 11347 | `	pVm = pCtx->pVm;` |
|        - | 11348 | `	/* Iterate and unset */` |
|    14582 | 11349 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7292 | 11350 | `		pObj = apArg[i];` |
|     7292 | 11351 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 11352 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11353 | `				/* Throw an error */` |
|      ! 0 | 11354 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 11355 | `			}` |
|      ! 0 | 11356 | `		}else{` |
|     7292 | 11357 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 11358 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7292 | 11359 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7286 | 11360 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3642 | 11361 | `			}` |
|        - | 11362 | `		}` |
|     3647 | 11363 | `	}` |
|     7292 | 11364 | `	return SXRET_OK;` |
|        2 | 11365 |  |
|        - | 11366 | `/*` |
|        - | 11367 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 11368 | ` */` |
|      110 | 11369 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11370 |  |
|      111 | 11371 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 11372 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11373 | `	ph7_value *pObj;` |
|        - | 11374 | `	sxu32 nIdx;` |
|        - | 11375 | `	/* Extract the memory object */` |
|      111 | 11376 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 11377 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 11378 | `	if( pObj ){` |
|      111 | 11379 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 11380 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 11381 | `				SyString sName;` |
|        - | 11382 | `				ph7_value sKey;` |
|        - | 11383 | `				/* Perform the insertion */` |
|      109 | 11384 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 11385 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 11386 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 11387 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 11388 | `			}` |
|       54 | 11389 | `		}` |
|       55 | 11390 | `	}` |
|      111 | 11391 | `	return SXRET_OK;` |
|        1 | 11392 |  |
|        - | 11393 | `/*` |
|        - | 11394 | ` * array get_defined_vars(void)` |
|        - | 11395 | ` *  Returns an array of all defined variables.` |
|        - | 11396 | ` * Parameter` |
|        - | 11397 | ` *  None` |
|        - | 11398 | ` * Return` |
|        - | 11399 | ` *  An array with all the variables defined in the current scope.` |
|        - | 11400 | ` */` |
|        2 | 11401 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11402 |  |
|        3 | 11403 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11404 | `	ph7_value *pArray;` |
|        - | 11405 | `	/* Create a new array */` |
|        3 | 11406 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11407 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11408 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11409 | `		SXUNUSED(apArg);` |
|        - | 11410 | `		/* Return NULL */` |
|      ! 0 | 11411 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11412 | `		return SXRET_OK;` |
|        - | 11413 | `	}` |
|        - | 11414 | `	/* Superglobals first */` |
|        3 | 11415 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 11416 | `	/* Then variable defined in the current frame */` |
|        3 | 11417 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 11418 | `	/* Finally,return the created array */` |
|        3 | 11419 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11420 | `	return SXRET_OK;` |
|        2 | 11421 |  |
|        - | 11422 | `/*` |
|        - | 11423 | ` * bool gettype($var)` |
|        - | 11424 | ` *  Get the type of a variable` |
|        - | 11425 | ` * Parameters` |
|        - | 11426 | ` *   $var` |
|        - | 11427 | ` *    The variable being type checked.` |
|        - | 11428 | ` * Return` |
|        - | 11429 | ` *   String representation of the given variable type.` |
|        - | 11430 | ` */` |
|       32 | 11431 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11432 |  |
|       34 | 11433 | `	const char *zType = "Empty";` |
|       34 | 11434 | `	if( nArg > 0 ){` |
|       34 | 11435 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 11436 | `	}` |
|        - | 11437 | `	/* Return the variable type */` |
|       34 | 11438 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11439 | `	return SXRET_OK;` |
|        2 | 11440 |  |
|        - | 11441 | `/*` |
|        - | 11442 | ` * string get_resource_type(resource $handle)` |
|        - | 11443 | ` *  This function gets the type of the given resource.` |
|        - | 11444 | ` * Parameters` |
|        - | 11445 | ` *  $handle` |
|        - | 11446 | ` *  The evaluated resource handle.` |
|        - | 11447 | ` * Return` |
|        - | 11448 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11449 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11450 | ` *  the return value will be the string Unknown.` |
|        - | 11451 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11452 | ` *  is not a resource.` |
|        - | 11453 | ` */` |
|        2 | 11454 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11455 |  |
|        3 | 11456 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11457 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11458 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11459 | `		return PH7_OK;` |
|        - | 11460 | `	}` |
|        3 | 11461 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11462 | `	return SXRET_OK;` |
|        2 | 11463 |  |
|        - | 11464 | `/*` |
|        - | 11465 | ` * void var_dump(expression,....)` |
|        - | 11466 | ` *   var_dump � Dumps information about a variable` |
|        - | 11467 | ` * Parameters` |
|        - | 11468 | ` *   One or more expression to dump.` |
|        - | 11469 | ` * Returns` |
|        - | 11470 | ` *  Nothing.` |
|        - | 11471 | ` */` |
|      218 | 11472 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11473 |  |
|        - | 11474 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11475 | `	int i;` |
|      220 | 11476 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11477 | `	/* Dump one or more expressions */` |
|      444 | 11478 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11479 | `		ph7_value *pObj = apArg[i];` |
|        - | 11480 | `		/* Reset the working buffer */` |
|      226 | 11481 | `		SyBlobReset(&sDump);` |
|        - | 11482 | `		/* Dump the given expression */` |
|      226 | 11483 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11484 | `		/* Output */` |
|      226 | 11485 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11486 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11487 | `		}` |
|      114 | 11488 | `	}` |
|        - | 11489 | `	/* Release the working buffer */` |
|      220 | 11490 | `	SyBlobRelease(&sDump);` |
|      220 | 11491 | `	return SXRET_OK;` |
|        2 | 11492 |  |
|        - | 11493 | `/*` |
|        - | 11494 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11495 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11496 | ` * Parameters` |
|        - | 11497 | ` *   expression: Expression to dump` |
|        - | 11498 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11499 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11500 | ` *            print_r() will return the information rather than print it.` |
|        - | 11501 | ` * Return` |
|        - | 11502 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11503 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11504 | ` */` |
|       16 | 11505 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11506 |  |
|       17 | 11507 | `	int ret_string = 0;` |
|        - | 11508 | `	SyBlob sDump;` |
|       17 | 11509 | `	if( nArg < 1 ){` |
|        - | 11510 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11511 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11512 | `		return SXRET_OK;` |
|        - | 11513 | `	}` |
|       17 | 11514 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11515 | `	if ( nArg > 1 ){` |
|        - | 11516 | `		/* Where to redirect output */` |
|       11 | 11517 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11518 | `	}` |
|        - | 11519 | `	/* Generate dump */` |
|       17 | 11520 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11521 | `	if( !ret_string ){` |
|        - | 11522 | `		/* Output dump */` |
|        7 | 11523 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11524 | `		/* Return true */` |
|        7 | 11525 | `		ph7_result_bool(pCtx,1);` |
|        4 | 11526 | `	}else{` |
|        - | 11527 | `		/* Generated dump as return value */` |
|       11 | 11528 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11529 | `	}` |
|        - | 11530 | `	/* Release the working buffer */` |
|       17 | 11531 | `	SyBlobRelease(&sDump);` |
|       17 | 11532 | `	return SXRET_OK;` |
|        9 | 11533 |  |
|        - | 11534 | `/*` |
|        - | 11535 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 11536 | ` * Same job as print_r. (see coment above)` |
|        - | 11537 | ` */` |
|        2 | 11538 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11539 |  |
|        3 | 11540 | `	int ret_string = 0;` |
|        - | 11541 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11542 | `	if( nArg < 1 ){` |
|        - | 11543 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11544 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11545 | `		return SXRET_OK;` |
|        - | 11546 | `	}` |
|        3 | 11547 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11548 | `	if ( nArg > 1 ){` |
|        - | 11549 | `		/* Where to redirect output */` |
|        3 | 11550 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11551 | `	}` |
|        - | 11552 | `	/* Generate dump */` |
|        3 | 11553 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11554 | `	if( !ret_string ){` |
|        - | 11555 | `		/* Output dump */` |
|      ! 0 | 11556 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11557 | `		/* Return NULL */` |
|      ! 0 | 11558 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11559 | `	}else{` |
|        - | 11560 | `		/* Generated dump as return value */` |
|        3 | 11561 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11562 | `	}` |
|        - | 11563 | `	/* Release the working buffer */` |
|        3 | 11564 | `	SyBlobRelease(&sDump);` |
|        3 | 11565 | `	return SXRET_OK;` |
|        2 | 11566 |  |
|        - | 11567 | `/*` |
|        - | 11568 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11569 | ` *  Set/get the various assert flags.` |
|        - | 11570 | ` * Parameter` |
|        - | 11571 | ` * $what` |
|        - | 11572 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11573 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11574 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11575 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11576 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11577 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11578 | ` * $value` |
|        - | 11579 | ` *   An optional new value for the option.` |
|        - | 11580 | ` * Return` |
|        - | 11581 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11582 | ` */` |
|       28 | 11583 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11584 |  |
|       30 | 11585 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11586 | `	int iOption;` |
|        - | 11587 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11588 | `	if( nArg < 1 ){` |
|        3 | 11589 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11590 | `			"ArgumentCountError",` |
|        - | 11591 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11592 | `			);` |
|        - | 11593 | `	}` |
|        - | 11594 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11595 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11596 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11597 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11598 | `			"TypeError",` |
|        - | 11599 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11600 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11601 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11602 | `			);` |
|        - | 11603 | `	}` |
|       28 | 11604 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11605 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11606 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11607 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11608 | `	switch( iOption ){` |
|        5 | 11609 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11610 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11611 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11612 | `		if( nArg > 1 ){` |
|        5 | 11613 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11614 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11615 | `			}else{` |
|        3 | 11616 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11617 | `			}` |
|        2 | 11618 | `		}` |
|       12 | 11619 | `		break;` |
|        1 | 11620 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11621 | `		/* Return old callback or null */` |
|        3 | 11622 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11623 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11624 | `		}else{` |
|        3 | 11625 | `			ph7_result_null(pCtx);` |
|        - | 11626 | `		}` |
|        3 | 11627 | `		if( nArg > 1 ){` |
|      ! 0 | 11628 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11629 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11630 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11631 | `			}else{` |
|      ! 0 | 11632 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11633 | `			}` |
|      ! 0 | 11634 | `		}` |
|        3 | 11635 | `		break;` |
|        5 | 11636 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11637 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11638 | `		if( nArg > 1 ){` |
|        5 | 11639 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11640 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11641 | `			}else{` |
|        3 | 11642 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11643 | `			}` |
|        2 | 11644 | `		}` |
|       11 | 11645 | `		break;` |
|      ! 0 | 11646 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11647 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11648 | `		break;` |
|        1 | 11649 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11650 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11651 | `		break;` |
|      ! 0 | 11652 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11653 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11654 | `		break;` |
|        1 | 11655 | `	default:` |
|        - | 11656 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11657 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11658 | `			"ValueError",` |
|        - | 11659 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11660 | `			);` |
|        - | 11661 | `	}` |
|       26 | 11662 | `	return PH7_OK;` |
|       16 | 11663 |  |
|        - | 11664 | `/*` |
|        - | 11665 | ` * bool assert(mixed $assertion)` |
|        - | 11666 | ` *  Checks if assertion is FALSE.` |
|        - | 11667 | ` * Parameter` |
|        - | 11668 | ` *  $assertion` |
|        - | 11669 | ` *    The assertion to test.` |
|        - | 11670 | ` * Return` |
|        - | 11671 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11672 | ` */` |
|       24 | 11673 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11674 |  |
|       26 | 11675 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11676 | `	int iFlags,iResult;` |
|        - | 11677 | `	const char *zDesc;` |
|        - | 11678 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11679 | `	if( nArg < 1 ){` |
|        3 | 11680 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11681 | `			"ArgumentCountError",` |
|        - | 11682 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11683 | `			);` |
|        - | 11684 | `	}` |
|       24 | 11685 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11686 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11687 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11688 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11689 | `		return PH7_OK;` |
|        - | 11690 | `	}` |
|        - | 11691 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11692 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11693 | `	if( !iResult ){` |
|        - | 11694 | `		/* Assertion failed */` |
|        - | 11695 | `		/* Extract optional description */` |
|       13 | 11696 | `		zDesc = 0;` |
|       13 | 11697 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11698 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11699 | `		}` |
|       13 | 11700 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11701 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11702 | `			ph7_value sFile,sLine;` |
|        - | 11703 | `			ph7_value *apCbArg[3];` |
|        - | 11704 | `			SyString *pFile;` |
|        - | 11705 | `			/* Extract the processed script */` |
|      ! 0 | 11706 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11707 | `			if( pFile == 0 ){` |
|      ! 0 | 11708 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11709 | `			}` |
|        - | 11710 | `			/* Invoke the callback */` |
|      ! 0 | 11711 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11712 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11713 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11714 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11715 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11716 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11717 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11718 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11719 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11720 | `		}` |
|       13 | 11721 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11722 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11723 | `			return PH7_ABORT;` |
|        - | 11724 | `		}` |
|        - | 11725 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11726 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11727 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11728 | `				"AssertionError",` |
|        - | 11729 | `				"%s",` |
|        1 | 11730 | `				zDesc` |
|        - | 11731 | `				);` |
|      ! 0 | 11732 | `		}else{` |
|       11 | 11733 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11734 | `				"AssertionError",` |
|        - | 11735 | `				"assert(false)"` |
|        - | 11736 | `				);` |
|        - | 11737 | `		}` |
|        - | 11738 | `	}` |
|        - | 11739 | `	/* Assertion passed */` |
|       11 | 11740 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11741 | `	return PH7_OK;` |
|       14 | 11742 |  |
|        - | 11743 | `/*` |
|        - | 11744 | ` * Section:` |
|        - | 11745 | ` *  Error reporting functions.` |
|        - | 11746 | ` * Status:` |
|        - | 11747 | ` *    Stable.` |
|        - | 11748 | ` */` |
|        - | 11749 | `/*` |
|        - | 11750 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11751 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11752 | ` * Parameters` |
|        - | 11753 | ` *  $error_msg` |
|        - | 11754 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11755 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11756 | ` * $error_type` |
|        - | 11757 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11758 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11759 | ` * Return` |
|        - | 11760 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11761 | ` */` |
|       12 | 11762 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11763 |  |
|       14 | 11764 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11765 | `	int rc = PH7_OK;` |
|       14 | 11766 | `	if( nArg > 0 ){` |
|        - | 11767 | `		const char *zErr;` |
|        - | 11768 | `		int nLen;` |
|        - | 11769 | `		/* Extract the error message */` |
|       12 | 11770 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11771 | `		if( nArg > 1 ){` |
|        - | 11772 | `			/* Extract the error type */` |
|       12 | 11773 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11774 | `			switch( nErr ){` |
|        1 | 11775 | `			case 1:   /* E_ERROR */` |
|        - | 11776 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11777 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11778 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11779 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11780 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11781 | `				break;` |
|        1 | 11782 | `			case 2:   /* E_WARNING */` |
|        - | 11783 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11784 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11785 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11786 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11787 | `				break;` |
|        3 | 11788 | `			default:` |
|        8 | 11789 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11790 | `				break;` |
|        - | 11791 | `			}` |
|        5 | 11792 | `		}` |
|        - | 11793 | `		/* Report error */` |
|       12 | 11794 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11795 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11796 | `			return rc;` |
|        - | 11797 | `		}` |
|        - | 11798 | `		/* Return true */` |
|       12 | 11799 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11800 | `	}else{` |
|        - | 11801 | `		/* Missing arguments,return FALSE */` |
|        3 | 11802 | `		ph7_result_bool(pCtx,0);` |
|        - | 11803 | `	}` |
|       14 | 11804 | `	return rc;` |
|        8 | 11805 |  |
|        - | 11806 | `/*` |
|        - | 11807 | ` * int error_reporting([int $level])` |
|        - | 11808 | ` *  Sets which PHP errors are reported.` |
|        - | 11809 | ` * Parameters` |
|        - | 11810 | ` *  $level` |
|        - | 11811 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11812 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11813 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11814 | ` *   levels will not always behave as expected.` |
|        - | 11815 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11816 | ` *   in the predefined constants.` |
|        - | 11817 | ` * Return` |
|        - | 11818 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11819 | ` *   parameter is given.` |
|        - | 11820 | ` */` |
|       38 | 11821 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11822 |  |
|       40 | 11823 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11824 | `	int nOld;` |
|        - | 11825 | `	/* Extract the old reporting level */` |
|       40 | 11826 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11827 | `	if( nArg > 0 ){` |
|        - | 11828 | `		int nNew;` |
|        - | 11829 | `		/* Extract the desired error reporting level */` |
|       32 | 11830 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11831 | `		if( !nNew ){` |
|        - | 11832 | `			/* Do not report errors at all */` |
|        5 | 11833 | `			pVm->bErrReport = 0;` |
|        3 | 11834 | `		}else{` |
|        - | 11835 | `			/* Report all errors */` |
|       28 | 11836 | `			pVm->bErrReport = 1;` |
|        - | 11837 | `		}` |
|       15 | 11838 | `	}` |
|        - | 11839 | `	/* Return the old level */` |
|       40 | 11840 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 11841 | `	return PH7_OK;` |
|        2 | 11842 |  |
|        - | 11843 | `/*` |
|        - | 11844 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 11845 | ` *  Send an error message somewhere.` |
|        - | 11846 | ` * Parameter` |
|        - | 11847 | ` *  $message` |
|        - | 11848 | ` *   The error message that should be logged.` |
|        - | 11849 | ` *  $message_type` |
|        - | 11850 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 11851 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 11852 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 11853 | ` *       This is the default option.` |
|        - | 11854 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 11855 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 11856 | ` *    2  No longer an option.` |
|        - | 11857 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 11858 | ` *       to the end of the message string.` |
|        - | 11859 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 11860 | ` *  $destination` |
|        - | 11861 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 11862 | ` *  $extra_headers` |
|        - | 11863 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 11864 | ` * Return` |
|        - | 11865 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11866 | ` * NOTE:` |
|        - | 11867 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 11868 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 11869 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 11870 | ` *  Otherwise this function is no-op.` |
|        - | 11871 | ` */` |
|        4 | 11872 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11873 |  |
|        - | 11874 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 11875 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 11876 | `	int iType = 0;` |
|        5 | 11877 | `	if( nArg < 1 ){` |
|        - | 11878 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 11879 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11880 | `		return PH7_OK;` |
|        - | 11881 | `	}` |
|        5 | 11882 | `	if( pVm->xErrLog  ){` |
|        - | 11883 | `		/* Invoke the user callback */` |
|      ! 0 | 11884 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 11885 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 11886 | `		if( nArg > 1 ){` |
|      ! 0 | 11887 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11888 | `			if( nArg > 2 ){` |
|      ! 0 | 11889 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 11890 | `				if( nArg > 3 ){` |
|      ! 0 | 11891 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 11892 | `				}` |
|      ! 0 | 11893 | `			}` |
|      ! 0 | 11894 | `		}` |
|      ! 0 | 11895 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 11896 | `	}` |
|        - | 11897 | `	/* Retun TRUE */` |
|        5 | 11898 | `	ph7_result_bool(pCtx,1);` |
|        5 | 11899 | `	return PH7_OK;` |
|        3 | 11900 |  |
|        - | 11901 | `/*` |
|        - | 11902 | ` * bool restore_exception_handler(void)` |
|        - | 11903 | ` *  Restores the previously defined exception handler function.` |
|        - | 11904 | ` * Parameter` |
|        - | 11905 | ` *  None` |
|        - | 11906 | ` * Return` |
|        - | 11907 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 11908 | ` */` |
|        4 | 11909 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11910 |  |
|        5 | 11911 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11912 | `	ph7_value *pOld,*pNew;` |
|        - | 11913 | `	/* Point to the old and the new handler */` |
|        5 | 11914 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 11915 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 11916 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11917 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11918 | `		SXUNUSED(apArg);` |
|        - | 11919 | `		/* No installed handler,return FALSE */` |
|        5 | 11920 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11921 | `		return PH7_OK;` |
|        - | 11922 | `	}` |
|        - | 11923 | `	/* Copy the old handler */` |
|      ! 0 | 11924 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11925 | `	PH7_MemObjRelease(pOld);` |
|        - | 11926 | `	/* Return TRUE */` |
|      ! 0 | 11927 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11928 | `	return PH7_OK;` |
|        3 | 11929 |  |
|        - | 11930 | `/*` |
|        - | 11931 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 11932 | ` *  Sets a user-defined exception handler function.` |
|        - | 11933 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 11934 | ` * NOTE` |
|        - | 11935 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 11936 | ` *  the satndard PHP engine.` |
|        - | 11937 | ` * Parameters` |
|        - | 11938 | ` *  $exception_handler` |
|        - | 11939 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 11940 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 11941 | ` *   that was thrown.` |
|        - | 11942 | ` *  Note:` |
|        - | 11943 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11944 | ` * Return` |
|        - | 11945 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 11946 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11947 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11948 | ` */` |
|        4 | 11949 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11950 |  |
|        6 | 11951 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11952 | `	ph7_value *pOld,*pNew;` |
|        - | 11953 | `	/* Point to the old and the new handler */` |
|        6 | 11954 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11955 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11956 | `	/* Return the old handler */` |
|        6 | 11957 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11958 | `	if( nArg > 0 ){` |
|        6 | 11959 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11960 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11961 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11962 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11963 | `		}else{` |
|        6 | 11964 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11965 | `			/* Install the new handler */` |
|        6 | 11966 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11967 | `		}` |
|        2 | 11968 | `	}` |
|        6 | 11969 | `	return PH7_OK;` |
|        2 | 11970 |  |
|        - | 11971 | `/*` |
|        - | 11972 | ` * bool restore_error_handler(void)` |
|        - | 11973 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11974 | ` * Parameters:` |
|        - | 11975 | ` *  None.` |
|        - | 11976 | ` * Return` |
|        - | 11977 | ` *  Always TRUE.` |
|        - | 11978 | ` */` |
|        6 | 11979 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11980 |  |
|        7 | 11981 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11982 | `	ph7_value *pOld,*pNew;` |
|        - | 11983 | `	/* Point to the old and the new handler */` |
|        7 | 11984 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 11985 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 11986 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 11987 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 11988 | `		SXUNUSED(apArg);` |
|        - | 11989 | `		/* No installed callback,return FALSE */` |
|        7 | 11990 | `		ph7_result_bool(pCtx,0);` |
|        7 | 11991 | `		return PH7_OK;` |
|        - | 11992 | `	}` |
|        - | 11993 | `	/* Copy the old callback */` |
|      ! 0 | 11994 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11995 | `	PH7_MemObjRelease(pOld);` |
|        - | 11996 | `	/* Return TRUE */` |
|      ! 0 | 11997 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11998 | `	return PH7_OK;` |
|        4 | 11999 |  |
|        - | 12000 | `/*` |
|        - | 12001 | ` * value set_error_handler(callable $error_handler)` |
|        - | 12002 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12003 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12004 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12005 | ` *  Sets a user-defined error handler function.` |
|        - | 12006 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 12007 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 12008 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 12009 | ` *  conditions (using trigger_error()).` |
|        - | 12010 | ` * Parameters` |
|        - | 12011 | ` *  $error_handler` |
|        - | 12012 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 12013 | ` *   describing the error.` |
|        - | 12014 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 12015 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 12016 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 12017 | ` *   The function can be shown as:` |
|        - | 12018 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 12019 | ` *     errno` |
|        - | 12020 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 12021 | ` *   errstr` |
|        - | 12022 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 12023 | ` *   errfile` |
|        - | 12024 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 12025 | ` *     was raised in, as a string.` |
|        - | 12026 | ` *  Note:` |
|        - | 12027 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12028 | ` * Return` |
|        - | 12029 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 12030 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12031 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12032 | ` */` |
|    10144 | 12033 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12034 |  |
|    10146 | 12035 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12036 | `	ph7_value *pOld,*pNew;` |
|        - | 12037 | `	/* Point to the old and the new handler */` |
|    10146 | 12038 | `	pOld = &pVm->aErrCB[0];` |
|    10146 | 12039 | `	pNew = &pVm->aErrCB[1];` |
|        - | 12040 | `	/* Return the old handler */` |
|    10146 | 12041 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10146 | 12042 | `	if( nArg > 0 ){` |
|    10146 | 12043 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12044 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5071 | 12045 | `			PH7_MemObjRelease(pNew);` |
|     5071 | 12046 | `			ph7_result_bool(pCtx,1);` |
|     2536 | 12047 | `		}else{` |
|     5076 | 12048 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12049 | `			/* Install the new handler */` |
|     5076 | 12050 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12051 | `		}` |
|     5072 | 12052 | `	}` |
|    10146 | 12053 | `	return PH7_OK;` |
|        2 | 12054 |  |
|        - | 12055 | `/*` |
|        - | 12056 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 12057 | ` *  Generates a backtrace.` |
|        - | 12058 | ` * Paramaeter` |
|        - | 12059 | ` *  $options` |
|        - | 12060 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 12061 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 12062 | ` *   all the function/method arguments, to save memory.` |
|        - | 12063 | ` * $limit` |
|        - | 12064 | ` *   (Not Used)` |
|        - | 12065 | ` * Return` |
|        - | 12066 | ` *  An array.The possible returned elements are as follows:` |
|        - | 12067 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 12068 | ` *          Name        Type      Description` |
|        - | 12069 | ` *          ------      ------     -----------` |
|        - | 12070 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 12071 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 12072 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 12073 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 12074 | ` *          object      object    The current object.` |
|        - | 12075 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 12076 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 12077 | ` */` |
|      734 | 12078 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12079 |  |
|      736 | 12080 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12081 | `	ph7_value *pArray;` |
|        - | 12082 | `	ph7_class *pClass;` |
|        - | 12083 | `	ph7_value *pValue;` |
|        - | 12084 | `	SyString *pFile;` |
|        - | 12085 | `	/* Create a new array */` |
|      736 | 12086 | `	pArray = ph7_context_new_array(pCtx);` |
|      736 | 12087 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      736 | 12088 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12089 | `		/* Out of memory,return NULL */` |
|      ! 0 | 12090 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12091 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12092 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12093 | `		SXUNUSED(apArg);` |
|      ! 0 | 12094 | `		return PH7_OK;` |
|        - | 12095 | `	}` |
|        - | 12096 | `	/* Dump running function name and it's arguments  */` |
|      736 | 12097 | `	if( pVm->pFrame->pParent ){` |
|      736 | 12098 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 12099 | `		ph7_vm_func *pFunc;` |
|        - | 12100 | `		ph7_value *pArg;` |
|      736 | 12101 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      736 | 12102 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      736 | 12103 | `		if( pFrame->pParent && pFunc ){` |
|      736 | 12104 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      736 | 12105 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      736 | 12106 | `			ph7_value_reset_string_cursor(pValue);` |
|      367 | 12107 | `		}` |
|        - | 12108 | `		/* Function arguments */` |
|      736 | 12109 | `		pArg = ph7_context_new_array(pCtx);` |
|      736 | 12110 | `		if( pArg  ){` |
|        - | 12111 | `			ph7_value *pObj;` |
|        - | 12112 | `			VmSlot *aSlot;` |
|        - | 12113 | `			sxu32 n;` |
|        - | 12114 | `			/* Start filling the array with the given arguments */` |
|      736 | 12115 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2942 | 12116 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2208 | 12117 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2208 | 12118 | `				if( pObj ){` |
|     2208 | 12119 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1103 | 12120 | `				}` |
|     1105 | 12121 | `			}` |
|        - | 12122 | `			/* Save the array */` |
|      736 | 12123 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      367 | 12124 | `		}` |
|      367 | 12125 | `	}` |
|      736 | 12126 | `	ph7_value_int(pValue,1);` |
|        - | 12127 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 12128 | `	 * line numbers at run-time. )` |
|        - | 12129 | `	 */` |
|      736 | 12130 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 12131 | `	/* Current processed script */` |
|      736 | 12132 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      736 | 12133 | `	if( pFile ){` |
|      736 | 12134 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      736 | 12135 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      736 | 12136 | `		ph7_value_reset_string_cursor(pValue);` |
|      367 | 12137 | `	}` |
|        - | 12138 | `	/* Top class */` |
|      736 | 12139 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      736 | 12140 | `	if( pClass ){` |
|      732 | 12141 | `		ph7_value_reset_string_cursor(pValue);` |
|      732 | 12142 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      732 | 12143 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      365 | 12144 | `	}` |
|        - | 12145 | `	/* Return the freshly created array */` |
|      736 | 12146 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12147 | `	/*` |
|        - | 12148 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 12149 | `	 * as soon we return from this function.` |
|        - | 12150 | `	 */` |
|      736 | 12151 | `	return PH7_OK;` |
|      369 | 12152 |  |
|        - | 12153 | `/*` |
|        - | 12154 | ` * Generate a small backtrace.` |
|        - | 12155 | ` * Store the generated dump in the given BLOB` |
|        - | 12156 | ` */` |
|        4 | 12157 | `static int VmMiniBacktrace(` |
|        - | 12158 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12159 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 12160 | `	)` |
|        1 | 12161 |  |
|        5 | 12162 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12163 | `	ph7_vm_func *pFunc;` |
|        - | 12164 | `	ph7_class *pClass;` |
|        - | 12165 | `	SyString *pFile;` |
|        - | 12166 | `	/* Called function */` |
|        5 | 12167 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 12168 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 12169 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12170 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 12171 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 12172 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 12173 | `	}else{` |
|      ! 0 | 12174 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 12175 | `	}` |
|        5 | 12176 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 12177 | `	/* Current processed script */` |
|        5 | 12178 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 12179 | `	if( pFile ){` |
|        5 | 12180 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12181 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 12182 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 12183 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 12184 | `	}` |
|        - | 12185 | `	/* Top class */` |
|        5 | 12186 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 12187 | `	if( pClass ){` |
|      ! 0 | 12188 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 12189 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 12190 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 12191 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 12192 | `	}` |
|        5 | 12193 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 12194 | `	/* All done */` |
|        5 | 12195 | `	return SXRET_OK;` |
|        1 | 12196 |  |
|        - | 12197 | `/*` |
|        - | 12198 | ` * void debug_print_backtrace()` |
|        - | 12199 | ` *  Prints a backtrace` |
|        - | 12200 | ` * Parameters` |
|        - | 12201 | ` * None` |
|        - | 12202 | ` * Return` |
|        - | 12203 | ` * NULL` |
|        - | 12204 | ` */` |
|        2 | 12205 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12206 |  |
|        3 | 12207 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12208 | `	SyBlob sDump;` |
|        3 | 12209 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12210 | `	/* Generate the backtrace */` |
|        3 | 12211 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12212 | `	/* Output backtrace */` |
|        3 | 12213 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12214 | `	/* All done,cleanup */` |
|        3 | 12215 | `	SyBlobRelease(&sDump);` |
|        1 | 12216 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12217 | `	SXUNUSED(apArg);` |
|        3 | 12218 | `	return PH7_OK;` |
|        1 | 12219 |  |
|        - | 12220 | `/*` |
|        - | 12221 | ` * string debug_string_backtrace()` |
|        - | 12222 | ` *  Generate a backtrace` |
|        - | 12223 | ` * Parameters` |
|        - | 12224 | ` * None` |
|        - | 12225 | ` * Return` |
|        - | 12226 | ` *  A mini backtrace().` |
|        - | 12227 | ` * Note that this is a symisc extension.` |
|        - | 12228 | ` */` |
|        2 | 12229 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12230 |  |
|        3 | 12231 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12232 | `	SyBlob sDump;` |
|        3 | 12233 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12234 | `	/* Generate the backtrace */` |
|        3 | 12235 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12236 | `	/* Return the backtrace */` |
|        3 | 12237 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 12238 | `	/* All done,cleanup */` |
|        3 | 12239 | `	SyBlobRelease(&sDump);` |
|        1 | 12240 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12241 | `	SXUNUSED(apArg);` |
|        3 | 12242 | `	return PH7_OK;` |
|        1 | 12243 |  |
|        - | 12244 | `/*` |
|        - | 12245 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 12246 | ` * exception is triggered.` |
|        - | 12247 | ` */` |
|      492 | 12248 | `static sxi32 VmUncaughtException(` |
|        - | 12249 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12250 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12251 | `	)` |
|        1 | 12252 |  |
|        - | 12253 | `	ph7_value *apArg[2],sArg;` |
|      493 | 12254 | `	int nArg = 1;` |
|        - | 12255 | `	sxi32 rc;` |
|      493 | 12256 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 12257 | `		/* Nesting limit reached */` |
|      ! 0 | 12258 | `		return SXRET_OK;` |
|        - | 12259 | `	}` |
|        - | 12260 | `	/* Call any exception handler if available */` |
|      493 | 12261 | `	PH7_MemObjInit(pVm,&sArg);` |
|      493 | 12262 | `	if( pThis ){` |
|        - | 12263 | `		/* Load the exception instance */` |
|      493 | 12264 | `		sArg.x.pOther = pThis;` |
|      493 | 12265 | `		pThis->iRef++;` |
|      493 | 12266 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      247 | 12267 | `	}else{` |
|      ! 0 | 12268 | `		nArg = 0;` |
|        - | 12269 | `	}` |
|      493 | 12270 | `	apArg[0] = &sArg;` |
|        - | 12271 | `	/* Call the exception handler if available */` |
|      493 | 12272 | `	pVm->nExceptDepth++;` |
|      493 | 12273 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      493 | 12274 | `	pVm->nExceptDepth--;` |
|      493 | 12275 | `	if( rc != SXRET_OK ){` |
|        - | 12276 | `		SyBlob sMsgBuf;` |
|      491 | 12277 | `		const char *zClass = "Exception";` |
|      491 | 12278 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 12279 | `		const char *zMsg;` |
|        - | 12280 | `		sxu32 nMsg;` |
|        - | 12281 | `		const char *zFuncName;` |
|        - | 12282 | `		int nFuncLen;` |
|      491 | 12283 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      491 | 12284 | `		if( pThis ){` |
|        - | 12285 | `			ph7_class_method *pGetMessage;` |
|        - | 12286 | `			ph7_value sMsg;` |
|        - | 12287 | `			const char *zTmp;` |
|        - | 12288 | `			int nTmp;` |
|      491 | 12289 | `			zClass = pThis->pClass->sName.zString;` |
|      491 | 12290 | `			nClass = pThis->pClass->sName.nByte;` |
|      491 | 12291 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      491 | 12292 | `			if( pGetMessage ){` |
|      491 | 12293 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      491 | 12294 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      491 | 12295 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      491 | 12296 | `					if( zTmp && nTmp > 0 ){` |
|      491 | 12297 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      245 | 12298 | `					}` |
|      245 | 12299 | `				}` |
|      491 | 12300 | `				PH7_MemObjRelease(&sMsg);` |
|      245 | 12301 | `			}` |
|      245 | 12302 | `		}` |
|      491 | 12303 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      491 | 12304 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      491 | 12305 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      491 | 12306 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      491 | 12307 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 12308 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      491 | 12309 | `		rc = SXERR_ABORT;` |
|      245 | 12310 | `	}` |
|      493 | 12311 | `	PH7_MemObjRelease(&sArg);` |
|      493 | 12312 | `	return rc;` |
|      247 | 12313 |  |
|        - | 12314 | `/*` |
|        - | 12315 | ` * Throw a user exception.` |
|        - | 12316 | ` *` |
|        - | 12317 | ` * Exception dispatch follows this sequence:` |
|        - | 12318 | ` *` |
|        - | 12319 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 12320 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 12321 | ` *` |
|        - | 12322 | ` * 2. If NO catch matches:` |
|        - | 12323 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 12324 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 12325 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 12326 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 12327 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 12328 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 12329 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 12330 | ` *` |
|        - | 12331 | ` * 3. If a catch DOES match:` |
|        - | 12332 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 12333 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 12334 | ` *       inside the catch body from immediately propagating past our` |
|        - | 12335 | ` *       finally block.` |
|        - | 12336 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 12337 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 12338 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 12339 | ` *       in pPendingException (step 2c).` |
|        - | 12340 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 12341 | ` *    d. Run finally (if present).` |
|        - | 12342 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 12343 | ` *       that handlers are restored and finally has run.` |
|        - | 12344 | ` */` |
|      690 | 12345 | `static sxi32 VmThrowException(` |
|        - | 12346 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 12347 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12348 | `	)` |
|        2 | 12349 |  |
|        - | 12350 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 12351 | `	ph7_exception **apException;` |
|        - | 12352 | `	ph7_exception *pException;` |
|        - | 12353 | `	/* Point to the stack of loaded exceptions */` |
|      692 | 12354 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      692 | 12355 | `	pException = 0;` |
|      692 | 12356 | `	pCatch = 0;` |
|      692 | 12357 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12358 | `		ph7_exception_block *aCatch;` |
|        - | 12359 | `		ph7_class *pClass;` |
|        - | 12360 | `		SyString *aNames;` |
|        - | 12361 | `		sxu32 nNames;` |
|        - | 12362 | `		int matched;` |
|        - | 12363 | `		sxu32 j,k;` |
|        - | 12364 | `		/* Locate the appropriate block to execute */` |
|      192 | 12365 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      192 | 12366 | `		(void)SySetPop(&pVm->aException);` |
|      192 | 12367 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      200 | 12368 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 12369 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      198 | 12370 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      198 | 12371 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      198 | 12372 | `			matched = 0;` |
|      224 | 12373 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 12374 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 12375 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 12376 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      216 | 12377 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      216 | 12378 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 12379 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 12380 | `					continue;` |
|        - | 12381 | `				}` |
|      216 | 12382 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      190 | 12383 | `					matched = 1;` |
|      190 | 12384 | `					break;` |
|        - | 12385 | `				}` |
|       14 | 12386 | `			}` |
|      198 | 12387 | `			if( matched ){` |
|        - | 12388 | `				/* Catch block found,break immediately */` |
|      190 | 12389 | `				pCatch = &aCatch[j];` |
|      190 | 12390 | `				break;` |
|        - | 12391 | `			}` |
|        5 | 12392 | `		}` |
|       95 | 12393 | `	}` |
|        - | 12394 | `	/* Execute the cached block if available */` |
|      692 | 12395 | `	if( pCatch == 0 ){` |
|        - | 12396 | `		sxi32 rc;` |
|        - | 12397 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      504 | 12398 | `		if( pException && pException->iHasFinally ){` |
|        3 | 12399 | `			pException->iFinallyDone = 1;` |
|        3 | 12400 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 12401 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12402 | `				return SXERR_ABORT;` |
|        - | 12403 | `			}` |
|        1 | 12404 | `		}` |
|        - | 12405 | `		/* Check if there is an outer exception handler on the stack */` |
|      504 | 12406 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12407 | `			/* Re-throw to the outer handler */` |
|        3 | 12408 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 12409 | `		}` |
|        - | 12410 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 12411 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 12412 | `		 * exception instead of reporting it uncaught.` |
|        - | 12413 | `		 */` |
|      502 | 12414 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 12415 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 12416 | `			 * by looking for a catch frame on the stack.` |
|        - | 12417 | `			 */` |
|      502 | 12418 | `			VmFrame *pF = pVm->pFrame;` |
|      502 | 12419 | `			int inCatch = 0;` |
|     1010 | 12420 | `			while( pF ){` |
|      518 | 12421 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 12422 | `					inCatch = 1;` |
|        9 | 12423 | `					break;` |
|        - | 12424 | `				}` |
|      509 | 12425 | `				pF = pF->pParent;` |
|        1 | 12426 | `			}` |
|      502 | 12427 | `			if( inCatch ){` |
|        - | 12428 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 12429 | `				pThis->iRef++;` |
|        9 | 12430 | `				pVm->pPendingException = pThis;` |
|        9 | 12431 | `				return SXRET_OK;` |
|        - | 12432 | `			}` |
|      246 | 12433 | `		}` |
|        - | 12434 | `		/* Truly uncaught */` |
|      493 | 12435 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      493 | 12436 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12437 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12438 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12439 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12440 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12441 | `			}` |
|      ! 0 | 12442 | `		}` |
|      493 | 12443 | `		return rc;` |
|      ! 0 | 12444 | `	}else{` |
|      190 | 12445 | `		VmFrame *pFrame = pVm->pFrame;` |
|      190 | 12446 | `		ph7_exception **apSaved = 0;` |
|        - | 12447 | `		sxu32 nSavedCount;` |
|        - | 12448 | `		sxi32 rc;` |
|      190 | 12449 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      190 | 12450 | `		if( pException->pFrame == pFrame ){` |
|      140 | 12451 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       69 | 12452 | `		}` |
|        - | 12453 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12454 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12455 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12456 | `		 */` |
|      190 | 12457 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      190 | 12458 | `		if( nSavedCount > 0 ){` |
|       16 | 12459 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 12460 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12461 | `			if( apSaved ){` |
|       16 | 12462 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 12463 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12464 | `				SySetReset(&pVm->aException);` |
|        5 | 12465 | `			}` |
|        5 | 12466 | `		}` |
|        - | 12467 | `		/* Create a private frame first */` |
|      190 | 12468 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      190 | 12469 | `		if( rc == SXRET_OK ){` |
|      190 | 12470 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      190 | 12471 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      190 | 12472 | `			if( pObj ){` |
|      190 | 12473 | `				pThis->iRef++;` |
|      190 | 12474 | `				pObj->x.pOther = pThis;` |
|      190 | 12475 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       94 | 12476 | `			}` |
|        - | 12477 | `			/* Execute the catch block */` |
|      190 | 12478 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12479 | `			/* Leave the frame */` |
|      190 | 12480 | `			VmLeaveFrame(&(*pVm));` |
|       94 | 12481 | `		}` |
|        - | 12482 | `		/* Restore the outer exception handlers */` |
|      190 | 12483 | `		if( apSaved ){` |
|        - | 12484 | `			sxu32 k;` |
|        - | 12485 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12486 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12487 | `			 * Restore the original outer entries.` |
|        - | 12488 | `			 */` |
|       11 | 12489 | `			SySetReset(&pVm->aException);` |
|       21 | 12490 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 12491 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 12492 | `			}` |
|       11 | 12493 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 12494 | `		}` |
|        - | 12495 | `		/* Execute the finally block after catch */` |
|      190 | 12496 | `		if( pException->iHasFinally ){` |
|       16 | 12497 | `			pException->iFinallyDone = 1;` |
|        - | 12498 | `			{` |
|       16 | 12499 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12500 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12501 | `					return SXERR_ABORT;` |
|        - | 12502 | `				}` |
|        - | 12503 | `			}` |
|        7 | 12504 | `		}` |
|      190 | 12505 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12506 | `			return SXERR_ABORT;` |
|        - | 12507 | `		}` |
|        - | 12508 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12509 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12510 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12511 | `		 */` |
|      190 | 12512 | `		if( pVm->pPendingException ){` |
|        9 | 12513 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 12514 | `			pVm->pPendingException = 0;` |
|        9 | 12515 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12516 | `		}` |
|        - | 12517 | `	}` |
|        - | 12518 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12519 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12520 | `	 */` |
|      182 | 12521 | `	return SXRET_OK;` |
|      347 | 12522 |  |
|        - | 12523 | `/*` |
|        - | 12524 | ` * Section:` |
|        - | 12525 | ` *  Version,Credits and Copyright related functions.` |
|        - | 12526 | ` * Status:` |
|        - | 12527 | ` *    Stable.` |
|        - | 12528 | ` */` |
|        - | 12529 | `/*` |
|        - | 12530 | ` * string ph7version(void)` |
|        - | 12531 | ` *  Returns the running version of the PH7 version.` |
|        - | 12532 | ` * Parameters` |
|        - | 12533 | ` *  None` |
|        - | 12534 | ` * Return` |
|        - | 12535 | ` * Current PH7 version.` |
|        - | 12536 | ` */` |
|        2 | 12537 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12538 |  |
|        1 | 12539 | `	SXUNUSED(nArg);` |
|        1 | 12540 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12541 | `	/* Current engine version */` |
|        3 | 12542 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12543 | `	return PH7_OK;` |
|        1 | 12544 |  |
|        - | 12545 | `/*` |
|        - | 12546 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12547 | ` */` |
|        - | 12548 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12549 | ` "<html><head>"\` |
|        - | 12550 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12551 | ` "<style type=\"text/css\">"\` |
|        - | 12552 | ` "div {"\` |
|        - | 12553 | `     "border: 1px solid #cccccc;"\` |
|        - | 12554 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12555 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12556 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12557 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12558 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12559 | `     "-o-border-radius: 10px;"\` |
|        - | 12560 | `     "border-radius: 10px;"\` |
|        - | 12561 | `     "padding-left: 2em;"\` |
|        - | 12562 | `     "background-color: white;"\` |
|        - | 12563 | `     "margin-left: auto;"\` |
|        - | 12564 | `     "font-family: verdana;"\` |
|        - | 12565 | `     "padding-right: 2em;"\` |
|        - | 12566 | `     "margin-right: auto;"\` |
|        - | 12567 | `     "}"\` |
|        - | 12568 | `     "body {"\` |
|        - | 12569 | `     "padding: 0.2em;"\` |
|        - | 12570 | `     "font-style: normal;"\` |
|        - | 12571 | `     "font-size: medium;"\` |
|        - | 12572 | `     "background-color: #f2f2f2;"\` |
|        - | 12573 | `     "}"\` |
|        - | 12574 | `     "hr {"\` |
|        - | 12575 | `     "border-style: solid none none;"\` |
|        - | 12576 | `     "border-width: 1px medium medium;"\` |
|        - | 12577 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12578 | `     "height: 1px;"\` |
|        - | 12579 | `     "}"\` |
|        - | 12580 | `     "a {"\` |
|        - | 12581 | `     "color: #3366cc;"\` |
|        - | 12582 | `     "text-decoration: none;"\` |
|        - | 12583 | `     "}"\` |
|        - | 12584 | `     "a:hover {"\` |
|        - | 12585 | `     "color: #999999;"\` |
|        - | 12586 | `     "}"\` |
|        - | 12587 | `     "a:active {"\` |
|        - | 12588 | `     "color: #663399;"\` |
|        - | 12589 | `     "}"\` |
|        - | 12590 | `     "h1 {"\` |
|        - | 12591 | `     "margin: 0;"\` |
|        - | 12592 | `     "padding: 0;"\` |
|        - | 12593 | `     "font-family: Verdana;"\` |
|        - | 12594 | `     "font-weight: bold;"\` |
|        - | 12595 | `     "font-style: normal;"\` |
|        - | 12596 | `     "font-size: medium;"\` |
|        - | 12597 | `     "text-transform: capitalize;"\` |
|        - | 12598 | `     "color: #0a328c;"\` |
|        - | 12599 | `     "}"\` |
|        - | 12600 | `     "p {"\` |
|        - | 12601 | `     "margin: 0 auto;"\` |
|        - | 12602 | `     "font-size: medium;"\` |
|        - | 12603 | `     "font-style: normal;"\` |
|        - | 12604 | `     "font-family: verdana;"\` |
|        - | 12605 | `     "}"\` |
|        - | 12606 | `"</style></head><body>"\` |
|        - | 12607 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12608 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12609 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12610 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12611 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12612 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12613 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12614 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12615 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12616 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12617 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12618 |  |
|        - | 12619 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12620 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12621 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12622 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12623 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12624 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12625 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12626 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12627 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12628 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12629 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12630 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12631 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12632 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12633 |  |
|        - | 12634 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12635 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12636 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12637 | `"&nbsp;*<br>"\` |
|        - | 12638 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12639 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12640 | `"&nbsp;* are met:<br>"\` |
|        - | 12641 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12642 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12643 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12644 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12645 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12646 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12647 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12648 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12649 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12650 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12651 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12652 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12653 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12654 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12655 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12656 | `"&nbsp;*<br>"\` |
|        - | 12657 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12658 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12659 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12660 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12661 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12662 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12663 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12664 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12665 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12666 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12667 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12668 | `"&nbsp;*/<br>"\` |
|        - | 12669 | `"</span></small></small></p>"\` |
|        - | 12670 | `"</div></body></html>"` |
|        - | 12671 | `/*` |
|        - | 12672 | ` * bool ph7credits(void)` |
|        - | 12673 | ` * bool ph7info(void)` |
|        - | 12674 | ` * bool ph7copyright(void)` |
|        - | 12675 | ` *  Prints out the credits for PH7 engine` |
|        - | 12676 | ` * Parameters` |
|        - | 12677 | ` *  None` |
|        - | 12678 | ` * Return` |
|        - | 12679 | ` *  Always TRUE` |
|        - | 12680 | ` */` |
|        2 | 12681 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12682 |  |
|        3 | 12683 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12684 | `	/* Expand the HTML page above*/` |
|        3 | 12685 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12686 | `	ph7_context_output_format(` |
|        1 | 12687 | `		pCtx,` |
|        - | 12688 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12689 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12690 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12691 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12692 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12693 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12694 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12695 | `#ifdef __WINNT__` |
|        - | 12696 | `		"Windows NT"` |
|        - | 12697 | `#elif defined(__UNIXES__)` |
|        - | 12698 | `		"UNIX-Like"` |
|        - | 12699 | `#else` |
|        - | 12700 | `		"Other OS"` |
|        - | 12701 | `#endif` |
|        - | 12702 | `		);` |
|        3 | 12703 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12704 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12705 | `	SXUNUSED(apArg);` |
|        - | 12706 | `	/* Return TRUE */` |
|        - | 12707 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12708 | `	return PH7_OK;` |
|        1 | 12709 |  |
|        - | 12710 | `/*` |
|        - | 12711 | ` * Section:` |
|        - | 12712 | ` *    URL related routines.` |
|        - | 12713 | ` * Status:` |
|        - | 12714 | ` *    Stable.` |
|        - | 12715 | ` */` |
|        - | 12716 | `/*` |
|        - | 12717 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12718 | ` *  Parse a URL and return its fields.` |
|        - | 12719 | ` * Parameters` |
|        - | 12720 | ` *  $url` |
|        - | 12721 | ` *   The URL to parse.` |
|        - | 12722 | ` * $component` |
|        - | 12723 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12724 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12725 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12726 | ` *  in which case the return value will be an integer).` |
|        - | 12727 | ` * Return` |
|        - | 12728 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12729 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12730 | ` *  this array are:` |
|        - | 12731 | ` *   scheme - e.g. http` |
|        - | 12732 | ` *   host` |
|        - | 12733 | ` *   port` |
|        - | 12734 | ` *   user` |
|        - | 12735 | ` *   pass` |
|        - | 12736 | ` *   path` |
|        - | 12737 | ` *   query - after the question mark ?` |
|        - | 12738 | ` *   fragment - after the hashmark #` |
|        - | 12739 | ` * Note:` |
|        - | 12740 | ` *  FALSE is returned on failure.` |
|        - | 12741 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12742 | ` *  with the standard PHP engine.` |
|        - | 12743 | ` */` |
|       28 | 12744 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12745 |  |
|        - | 12746 | `	const char *zStr; /* Input string */` |
|        - | 12747 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12748 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12749 | `	int nLen;` |
|        - | 12750 | `	sxi32 rc;` |
|       29 | 12751 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12752 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12753 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12754 | `		return PH7_OK;` |
|        - | 12755 | `	}` |
|        - | 12756 | `	/* Extract the given URI */` |
|       29 | 12757 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12758 | `	if( nLen < 1 ){` |
|        - | 12759 | `		/* Nothing to process,return FALSE */` |
|        3 | 12760 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12761 | `		return PH7_OK;` |
|        - | 12762 | `	}` |
|        - | 12763 | `	/* Get a parse */` |
|       27 | 12764 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12765 | `	if( rc != SXRET_OK ){` |
|        - | 12766 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12767 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12768 | `		return PH7_OK;` |
|        - | 12769 | `	}` |
|       27 | 12770 | `	if( nArg > 1 ){` |
|      ! 0 | 12771 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12772 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12773 | `		switch(nComponent){` |
|      ! 0 | 12774 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12775 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12776 | `			if( pComp->nByte < 1 ){` |
|        - | 12777 | `				/* No available value,return NULL */` |
|      ! 0 | 12778 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12779 | `			}else{` |
|      ! 0 | 12780 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12781 | `			}` |
|      ! 0 | 12782 | `			break;` |
|      ! 0 | 12783 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12784 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12785 | `			if( pComp->nByte < 1 ){` |
|        - | 12786 | `				/* No available value,return NULL */` |
|      ! 0 | 12787 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12788 | `			}else{` |
|      ! 0 | 12789 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12790 | `			}` |
|      ! 0 | 12791 | `			break;` |
|      ! 0 | 12792 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12793 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12794 | `			if( pComp->nByte < 1 ){` |
|        - | 12795 | `				/* No available value,return NULL */` |
|      ! 0 | 12796 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12797 | `			}else{` |
|      ! 0 | 12798 | `				int iPort = 0;` |
|        - | 12799 | `				/* Cast the value to integer */` |
|      ! 0 | 12800 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12801 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12802 | `			}` |
|      ! 0 | 12803 | `			break;` |
|      ! 0 | 12804 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12805 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12806 | `			if( pComp->nByte < 1 ){` |
|        - | 12807 | `				/* No available value,return NULL */` |
|      ! 0 | 12808 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12809 | `			}else{` |
|      ! 0 | 12810 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12811 | `			}` |
|      ! 0 | 12812 | `			break;` |
|      ! 0 | 12813 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12814 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12815 | `			if( pComp->nByte < 1 ){` |
|        - | 12816 | `				/* No available value,return NULL */` |
|      ! 0 | 12817 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12818 | `			}else{` |
|      ! 0 | 12819 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12820 | `			}` |
|      ! 0 | 12821 | `			break;` |
|      ! 0 | 12822 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12823 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12824 | `			if( pComp->nByte < 1 ){` |
|        - | 12825 | `				/* No available value,return NULL */` |
|      ! 0 | 12826 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12827 | `			}else{` |
|      ! 0 | 12828 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12829 | `			}` |
|      ! 0 | 12830 | `			break;` |
|      ! 0 | 12831 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 12832 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 12833 | `			if( pComp->nByte < 1 ){` |
|        - | 12834 | `				/* No available value,return NULL */` |
|      ! 0 | 12835 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12836 | `			}else{` |
|      ! 0 | 12837 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12838 | `			}` |
|      ! 0 | 12839 | `			break;` |
|      ! 0 | 12840 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 12841 | `			pComp = &sURI.sPath;` |
|      ! 0 | 12842 | `			if( pComp->nByte < 1 ){` |
|        - | 12843 | `				/* No available value,return NULL */` |
|      ! 0 | 12844 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12845 | `			}else{` |
|      ! 0 | 12846 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12847 | `			}` |
|      ! 0 | 12848 | `			break;` |
|      ! 0 | 12849 | `		default:` |
|        - | 12850 | `			/* No such entry,return NULL */` |
|      ! 0 | 12851 | `			ph7_result_null(pCtx);` |
|      ! 0 | 12852 | `			break;` |
|        - | 12853 | `		}` |
|      ! 0 | 12854 | `	}else{` |
|        - | 12855 | `		ph7_value *pArray,*pValue;` |
|        - | 12856 | `		/* Return an associative array */` |
|       27 | 12857 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 12858 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 12859 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12860 | `			/* Out of memory */` |
|      ! 0 | 12861 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12862 | `			/* Return false */` |
|      ! 0 | 12863 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 12864 | `			return PH7_OK;` |
|        - | 12865 | `		}` |
|        - | 12866 | `		/* Fill the array */` |
|       27 | 12867 | `		pComp = &sURI.sScheme;` |
|       27 | 12868 | `		if( pComp->nByte > 0 ){` |
|       19 | 12869 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 12870 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 12871 | `		}` |
|        - | 12872 | `		/* Reset the string cursor */` |
|       27 | 12873 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12874 | `		pComp = &sURI.sHost;` |
|       27 | 12875 | `		if( pComp->nByte > 0 ){` |
|       25 | 12876 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 12877 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 12878 | `		}` |
|        - | 12879 | `		/* Reset the string cursor */` |
|       27 | 12880 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12881 | `		pComp = &sURI.sPort;` |
|       27 | 12882 | `		if( pComp->nByte > 0 ){` |
|       11 | 12883 | `			int iPort = 0;/* cc warning */` |
|        - | 12884 | `			/* Convert to integer */` |
|       11 | 12885 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 12886 | `			ph7_value_int(pValue,iPort);` |
|       11 | 12887 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 12888 | `		}` |
|        - | 12889 | `		/* Reset the string cursor */` |
|       27 | 12890 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12891 | `		pComp = &sURI.sUser;` |
|       27 | 12892 | `		if( pComp->nByte > 0 ){` |
|        7 | 12893 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12894 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 12895 | `		}` |
|        - | 12896 | `		/* Reset the string cursor */` |
|       27 | 12897 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12898 | `		pComp = &sURI.sPass;` |
|       27 | 12899 | `		if( pComp->nByte > 0 ){` |
|        7 | 12900 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12901 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 12902 | `		}` |
|        - | 12903 | `		/* Reset the string cursor */` |
|       27 | 12904 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12905 | `		pComp = &sURI.sPath;` |
|       27 | 12906 | `		if( pComp->nByte > 0 ){` |
|       17 | 12907 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 12908 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 12909 | `		}` |
|        - | 12910 | `		/* Reset the string cursor */` |
|       27 | 12911 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12912 | `		pComp = &sURI.sQuery;` |
|       27 | 12913 | `		if( pComp->nByte > 0 ){` |
|        5 | 12914 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12915 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 12916 | `		}` |
|        - | 12917 | `		/* Reset the string cursor */` |
|       27 | 12918 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12919 | `		pComp = &sURI.sFragment;` |
|       27 | 12920 | `		if( pComp->nByte > 0 ){` |
|        5 | 12921 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12922 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 12923 | `		}` |
|        - | 12924 | `		/* Return the created array */` |
|       27 | 12925 | `		ph7_result_value(pCtx,pArray);` |
|        - | 12926 | `		/* NOTE:` |
|        - | 12927 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 12928 | `		 * automatically as soon we return from this function.` |
|        - | 12929 | `		 */` |
|        - | 12930 | `	}` |
|        - | 12931 | `	/* All done */` |
|       27 | 12932 | `	return PH7_OK;` |
|       15 | 12933 |  |
|        - | 12934 | `/*` |
|        - | 12935 | ` * Section:` |
|        - | 12936 | ` *   Array related routines.` |
|        - | 12937 | ` * Status:` |
|        - | 12938 | ` *    Stable.` |
|        - | 12939 | ` * Note 2012-5-21 01:04:15:` |
|        - | 12940 | ` *  Array related functions that need access to the underlying` |
|        - | 12941 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 12942 | ` */` |
|        - | 12943 | `/*` |
|        - | 12944 | ` * The [compact()] function store it's state information in an instance` |
|        - | 12945 | ` * of the following structure.` |
|        - | 12946 | ` */` |
|        - | 12947 | `struct compact_data` |
|        - | 12948 |  |
|        - | 12949 | `	ph7_value *pArray;  /* Target array */` |
|        - | 12950 | `	int nRecCount;      /* Recursion count */` |
|        - | 12951 | `};` |
|        - | 12952 | `/*` |
|        - | 12953 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12954 | ` */` |
|      ! 0 | 12955 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12956 |  |
|      ! 0 | 12957 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12958 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12959 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12960 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12961 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12962 | `		SyString sVar;` |
|      ! 0 | 12963 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12964 | `		if( sVar.nByte > 0 ){` |
|        - | 12965 | `			/* Query the current frame */` |
|      ! 0 | 12966 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12967 | `			/* ^` |
|        - | 12968 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12969 | `			 */` |
|      ! 0 | 12970 | `			if( pKey ){` |
|        - | 12971 | `				/* Perform the insertion */` |
|      ! 0 | 12972 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12973 | `			}` |
|      ! 0 | 12974 | `		}` |
|      ! 0 | 12975 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12976 | `		int rc;` |
|        - | 12977 | `		/* Recursively traverse this array */` |
|      ! 0 | 12978 | `		pData->nRecCount++;` |
|      ! 0 | 12979 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12980 | `		pData->nRecCount--;` |
|      ! 0 | 12981 | `		return rc;` |
|        - | 12982 | `	}` |
|      ! 0 | 12983 | `	return SXRET_OK;` |
|      ! 0 | 12984 |  |
|        - | 12985 | `/*` |
|        - | 12986 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12987 | ` *  Create array containing variables and their values.` |
|        - | 12988 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12989 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12990 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12991 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12992 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12993 | ` * Parameters` |
|        - | 12994 | ` *  $varname` |
|        - | 12995 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12996 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12997 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12998 | ` *   it recursively.` |
|        - | 12999 | ` * Return` |
|        - | 13000 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 13001 | ` */` |
|        2 | 13002 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13003 |  |
|        - | 13004 | `	ph7_value *pArray,*pObj;` |
|        3 | 13005 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13006 | `	const char *zName;` |
|        - | 13007 | `	SyString sVar;` |
|        - | 13008 | `	int i,nLen;` |
|        3 | 13009 | `	if( nArg < 1 ){` |
|        - | 13010 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 13011 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13012 | `		return PH7_OK;` |
|        - | 13013 | `	}` |
|        - | 13014 | `	/* Create the array */` |
|        3 | 13015 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13016 | `	if( pArray == 0 ){` |
|        - | 13017 | `		/* Out of memory */` |
|      ! 0 | 13018 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13019 | `		/* Return NULL */` |
|      ! 0 | 13020 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13021 | `		return PH7_OK;` |
|        - | 13022 | `	}` |
|        - | 13023 | `	/* Perform the requested operation */` |
|        7 | 13024 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 13025 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 13026 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 13027 | `				struct compact_data sData;` |
|      ! 0 | 13028 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 13029 | `				/* Recursively walk the array */` |
|      ! 0 | 13030 | `				sData.nRecCount = 0;` |
|      ! 0 | 13031 | `				sData.pArray = pArray;` |
|      ! 0 | 13032 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 13033 | `			}` |
|      ! 0 | 13034 | `		}else{` |
|        - | 13035 | `			/* Extract variable name */` |
|        5 | 13036 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 13037 | `			if( nLen > 0 ){` |
|        5 | 13038 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 13039 | `				/* Check if the variable is available in the current frame */` |
|        5 | 13040 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 13041 | `				if( pObj ){` |
|        5 | 13042 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 13043 | `				}` |
|        2 | 13044 | `			}` |
|        - | 13045 | `		}` |
|        3 | 13046 | `	}` |
|        - | 13047 | `	/* Return the array */` |
|        3 | 13048 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13049 | `	return PH7_OK;` |
|        2 | 13050 |  |
|        - | 13051 | `/*` |
|        - | 13052 | ` * The [extract()] function store it's state information in an instance` |
|        - | 13053 | ` * of the following structure.` |
|        - | 13054 | ` */` |
|        - | 13055 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 13056 | `struct extract_aux_data` |
|        - | 13057 |  |
|        - | 13058 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 13059 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 13060 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 13061 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 13062 | `	int iFlags;           /* Control flags */` |
|        - | 13063 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 13064 | `};` |
|        - | 13065 | `/* Forward declaration */` |
|        - | 13066 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 13067 | `/*` |
|        - | 13068 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 13069 | ` *   Import variables into the current symbol table from an array.` |
|        - | 13070 | ` * Parameters` |
|        - | 13071 | ` * $var_array` |
|        - | 13072 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 13073 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 13074 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 13075 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 13076 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 13077 | ` * $extract_type` |
|        - | 13078 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 13079 | ` *  It can be one of the following values:` |
|        - | 13080 | ` *   EXTR_OVERWRITE` |
|        - | 13081 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 13082 | ` *   EXTR_SKIP` |
|        - | 13083 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 13084 | ` *   EXTR_PREFIX_SAME` |
|        - | 13085 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 13086 | ` *   EXTR_PREFIX_ALL` |
|        - | 13087 | ` *       Prefix all variable names with prefix.` |
|        - | 13088 | ` *   EXTR_PREFIX_INVALID` |
|        - | 13089 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 13090 | ` *   EXTR_IF_EXISTS` |
|        - | 13091 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 13092 | ` *       otherwise do nothing.` |
|        - | 13093 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 13094 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 13095 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 13096 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 13097 | ` *      the current symbol table.` |
|        - | 13098 | ` * $prefix` |
|        - | 13099 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 13100 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 13101 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 13102 | ` *  underscore character.` |
|        - | 13103 | ` * Return` |
|        - | 13104 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 13105 | ` */` |
|        4 | 13106 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13107 |  |
|        - | 13108 | `	extract_aux_data sAux;` |
|        - | 13109 | `	ph7_hashmap *pMap;` |
|        5 | 13110 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 13111 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 13112 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13113 | `		return PH7_OK;` |
|        - | 13114 | `	}` |
|        - | 13115 | `	/* Point to the target hashmap */` |
|        5 | 13116 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 13117 | `	if( pMap->nEntry < 1 ){` |
|        - | 13118 | `		/* Empty map,return  0 */` |
|      ! 0 | 13119 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13120 | `		return PH7_OK;` |
|        - | 13121 | `	}` |
|        - | 13122 | `	/* Prepare the aux data */` |
|        5 | 13123 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 13124 | `	if( nArg > 1 ){` |
|        3 | 13125 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 13126 | `		if( nArg > 2 ){` |
|      ! 0 | 13127 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 13128 | `		}` |
|        1 | 13129 | `	}` |
|        5 | 13130 | `	sAux.pVm = pCtx->pVm;` |
|        - | 13131 | `	/* Invoke the worker callback */` |
|        5 | 13132 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 13133 | `	/* Number of variables successfully imported */` |
|        5 | 13134 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 13135 | `	return PH7_OK;` |
|        3 | 13136 |  |
|        - | 13137 | `/*` |
|        - | 13138 | ` * Worker callback for the [extract()] function defined` |
|        - | 13139 | ` * below.` |
|        - | 13140 | ` */` |
|        8 | 13141 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13142 |  |
|        9 | 13143 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 13144 | `	int iFlags = pAux->iFlags;` |
|        9 | 13145 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13146 | `	ph7_value *pObj;` |
|        - | 13147 | `	SyString sVar;` |
|        9 | 13148 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 13149 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 13150 | `	}` |
|        - | 13151 | `	/* Perform a string cast */` |
|        9 | 13152 | `	PH7_MemObjToString(pKey);` |
|        9 | 13153 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13154 | `		/* Unavailable variable name */` |
|      ! 0 | 13155 | `		return SXRET_OK;` |
|        - | 13156 | `	}` |
|        9 | 13157 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 13158 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 13159 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13160 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13161 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13162 | `			);` |
|      ! 0 | 13163 | `	}else{` |
|       13 | 13164 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 13165 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13166 | `	}` |
|        9 | 13167 | `	sVar.zString = pAux->zWorker;` |
|        - | 13168 | `	/* Try to extract the variable */` |
|        9 | 13169 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 13170 | `	if( pObj ){` |
|        - | 13171 | `		/* Collision */` |
|        5 | 13172 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 13173 | `			return SXRET_OK;` |
|        - | 13174 | `		}` |
|        5 | 13175 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 13176 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 13177 | `				/* Already prefixed */` |
|      ! 0 | 13178 | `				return SXRET_OK;` |
|        - | 13179 | `			}` |
|      ! 0 | 13180 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13181 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13182 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13183 | `				);` |
|      ! 0 | 13184 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 13185 | `		}` |
|        3 | 13186 | `	}else{` |
|        - | 13187 | `		/* Create the variable */` |
|        5 | 13188 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 13189 | `	}` |
|        9 | 13190 | `	if( pObj ){` |
|        - | 13191 | `		/* Overwrite the old value */` |
|        9 | 13192 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 13193 | `		/* Increment counter */` |
|        9 | 13194 | `		pAux->iCount++;` |
|        4 | 13195 | `	}` |
|        9 | 13196 | `	return SXRET_OK;` |
|        5 | 13197 |  |
|        - | 13198 | `/*` |
|        - | 13199 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 13200 | ` * defined below.` |
|        - | 13201 | ` */` |
|        2 | 13202 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13203 |  |
|        3 | 13204 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 13205 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13206 | `	ph7_value *pObj;` |
|        - | 13207 | `	SyString sVar;` |
|        - | 13208 | `	/* Perform a string cast */` |
|        3 | 13209 | `	PH7_MemObjToString(pKey);` |
|        3 | 13210 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13211 | `		/* Unavailable variable name */` |
|      ! 0 | 13212 | `		return SXRET_OK;` |
|        - | 13213 | `	}` |
|        3 | 13214 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 13215 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 13216 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 13217 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 13218 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13219 | `			);` |
|        2 | 13220 | `	}else{` |
|      ! 0 | 13221 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 13222 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13223 | `	}` |
|        3 | 13224 | `	sVar.zString = pAux->zWorker;` |
|        - | 13225 | `	/* Extract the variable */` |
|        3 | 13226 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 13227 | `	if( pObj ){` |
|        3 | 13228 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 13229 | `	}` |
|        3 | 13230 | `	return SXRET_OK;` |
|        2 | 13231 |  |
|        - | 13232 | `/*` |
|        - | 13233 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 13234 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 13235 | ` * Parameters` |
|        - | 13236 | ` * $types` |
|        - | 13237 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 13238 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 13239 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 13240 | ` *  POST includes the POST uploaded file information.` |
|        - | 13241 | ` *  Note:` |
|        - | 13242 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 13243 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 13244 | ` * $prefix` |
|        - | 13245 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 13246 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 13247 | ` *  variable named $pref_userid.` |
|        - | 13248 | ` * Return` |
|        - | 13249 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13250 | ` */` |
|        2 | 13251 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13252 |  |
|        - | 13253 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 13254 | `	extract_aux_data sAux;` |
|        - | 13255 | `	int nLen,nPrefixLen;` |
|        - | 13256 | `	ph7_value *pSuper;` |
|        - | 13257 | `	ph7_vm *pVm;` |
|        - | 13258 | `	/* By default import only $_GET variables  */` |
|        3 | 13259 | `	zImport = "G";` |
|        3 | 13260 | `	nLen = (int)sizeof(char);` |
|        3 | 13261 | `	zPrefix = 0;` |
|        3 | 13262 | `	nPrefixLen = 0;` |
|        3 | 13263 | `	if( nArg > 0 ){` |
|        3 | 13264 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 13265 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 13266 | `		}` |
|        3 | 13267 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13268 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 13269 | `		}` |
|        1 | 13270 | `	}` |
|        - | 13271 | `	/* Point to the underlying VM */` |
|        3 | 13272 | `	pVm = pCtx->pVm;` |
|        - | 13273 | `	/* Initialize the aux data */` |
|        3 | 13274 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 13275 | `	sAux.zPrefix = zPrefix;` |
|        3 | 13276 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 13277 | `	sAux.pVm = pVm;` |
|        - | 13278 | `	/* Extract */` |
|        3 | 13279 | `	zEnd = &zImport[nLen];` |
|        5 | 13280 | `	while( zImport < zEnd ){` |
|        3 | 13281 | `		int c = zImport[0];` |
|        3 | 13282 | `		pSuper = 0;` |
|        3 | 13283 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 13284 | `			/* Import $_GET variables */` |
|        3 | 13285 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 13286 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 13287 | `			/* Import $_POST variables */` |
|      ! 0 | 13288 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 13289 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 13290 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 13291 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 13292 | `		}` |
|        3 | 13293 | `		if( pSuper ){` |
|        - | 13294 | `			/* Iterate throw array entries */` |
|        3 | 13295 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 13296 | `		}` |
|        - | 13297 | `		/* Advance the cursor */` |
|        3 | 13298 | `		zImport++;` |
|        1 | 13299 | `	}` |
|        - | 13300 | `	/* All done,return TRUE*/` |
|        3 | 13301 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13302 | `	return PH7_OK;` |
|        1 | 13303 |  |
|        - | 13304 | `/*` |
|        - | 13305 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 13306 | ` * Refer to the eval() language construct implementation for more` |
|        - | 13307 | ` * information.` |
|        - | 13308 | ` */` |
|    11886 | 13309 | `static sxi32 VmEvalChunk(` |
|        - | 13310 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 13311 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 13312 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 13313 | `	int iFlags,         /* Compile flag */` |
|        - | 13314 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 13315 | `	)` |
|        2 | 13316 |  |
|        - | 13317 | `	SySet *pByteCode,aByteCode;` |
|        - | 13318 | `	SyBlob sSavedNs;` |
|    11888 | 13319 | `	ProcConsumer xErr = 0;` |
|    11888 | 13320 | `	void *pErrData = 0;` |
|        - | 13321 | `	/* Initialize bytecode container */` |
|    11888 | 13322 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11888 | 13323 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 13324 | `	/* Reset the code generator */` |
|    11888 | 13325 | `	if( bTrueReturn ){` |
|        - | 13326 | `		/* Included file,log compile-time errors */` |
|     8932 | 13327 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8932 | 13328 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4465 | 13329 | `	}` |
|    11888 | 13330 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 13331 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 13332 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 13333 | `	 * the caller's namespace is restored. */` |
|    11888 | 13334 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11888 | 13335 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11888 | 13336 | `	if( bTrueReturn ){` |
|        - | 13337 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8932 | 13338 | `		SyBlobReset(&pVm->sNamespace);` |
|     4465 | 13339 | `	}` |
|        - | 13340 | `	/* Swap bytecode container */` |
|    11888 | 13341 | `	pByteCode = pVm->pByteContainer;` |
|    11888 | 13342 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 13343 | `	/* Compile the chunk */` |
|    11888 | 13344 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17831 | 13345 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 13346 | `		/* Compilation error,return false */` |
|        3 | 13347 | `		if( pCtx ){` |
|        3 | 13348 | `			ph7_result_bool(pCtx,0);` |
|        1 | 13349 | `		}` |
|        2 | 13350 | `	}else{` |
|        - | 13351 | `		/* Mount any newly defined classes */` |
|        - | 13352 | `		SyHashEntry *pEntry;` |
|        - | 13353 | `		ph7_class *pClass;` |
|        - | 13354 | `		ph7_value sResult; /* Return value */` |
|        - | 13355 | `		sxi32 rc;` |
|    11886 | 13356 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   562890 | 13357 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   545064 | 13358 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 13359 | `			/* Only mount classes that haven't been mounted yet */` |
|   545064 | 13360 | `			if( !pClass->bMounted ){` |
|   107950 | 13361 | `				rc = VmMountUserClass(pVm,pClass);` |
|   107950 | 13362 | `				if( rc != SXRET_OK ){` |
|        - | 13363 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 13364 | `					if( pCtx ){` |
|      ! 0 | 13365 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 13366 | `					}` |
|      ! 0 | 13367 | `					goto Cleanup;` |
|        - | 13368 | `				}` |
|    53974 | 13369 | `			}` |
|        2 | 13370 | `		}` |
|    11886 | 13371 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 13372 | `			/* Out of memory */` |
|      ! 0 | 13373 | `			if( pCtx ){` |
|      ! 0 | 13374 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 13375 | `			}` |
|      ! 0 | 13376 | `			goto Cleanup;` |
|        - | 13377 | `		}` |
|    11886 | 13378 | `		if( bTrueReturn ){` |
|        - | 13379 | `			/* Assume a boolean true return value */` |
|     8932 | 13380 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4467 | 13381 | `		}else{` |
|        - | 13382 | `			/* Assume a null return value */` |
|     2956 | 13383 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 13384 | `		}` |
|        - | 13385 | `		/* Execute the compiled chunk */` |
|    11886 | 13386 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11886 | 13387 | `		if( pCtx ){` |
|        - | 13388 | `			/* Set the execution result */` |
|     8950 | 13389 | `			ph7_result_value(pCtx,&sResult);` |
|     4474 | 13390 | `		}` |
|    11886 | 13391 | `		PH7_MemObjRelease(&sResult);` |
|        - | 13392 | `	}` |
|     5943 | 13393 | `Cleanup:` |
|        - | 13394 | `	/* Cleanup the mess left behind */` |
|    11888 | 13395 | `	pVm->pByteContainer = pByteCode;` |
|    11888 | 13396 | `	SySetRelease(&aByteCode);` |
|        - | 13397 | `	/* Restore caller's namespace state */` |
|    11888 | 13398 | `	SyBlobReset(&pVm->sNamespace);` |
|    11888 | 13399 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11888 | 13400 | `	SyBlobRelease(&sSavedNs);` |
|    11888 | 13401 | `	return SXRET_OK;` |
|        2 | 13402 |  |
|        - | 13403 | `/*` |
|        - | 13404 | ` * value eval(string $code)` |
|        - | 13405 | ` *   Evaluate a string as PHP code.` |
|        - | 13406 | ` * Parameter` |
|        - | 13407 | ` *  code: PHP code to evaluate.` |
|        - | 13408 | ` * Return` |
|        - | 13409 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 13410 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 13411 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 13412 | ` */` |
|       22 | 13413 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13414 |  |
|        - | 13415 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 13416 | `	if( nArg < 1 ){` |
|        - | 13417 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13418 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13419 | `		return SXRET_OK;` |
|        - | 13420 | `	}` |
|        - | 13421 | `	/* Chunk to evaluate */` |
|       24 | 13422 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 13423 | `	if( sChunk.nByte < 1 ){` |
|        - | 13424 | `		/* Empty string,return NULL */` |
|        3 | 13425 | `		ph7_result_null(pCtx);` |
|        3 | 13426 | `		return SXRET_OK;` |
|        - | 13427 | `	}` |
|        - | 13428 | `	/* Eval the chunk */` |
|       22 | 13429 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 13430 | `	return SXRET_OK;` |
|       13 | 13431 |  |
|        - | 13432 | `/*` |
|        - | 13433 | ` * Check if a file path is already included.` |
|        - | 13434 | ` */` |
|    17856 | 13435 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13436 |  |
|        - | 13437 | `	SyString *aEntries;` |
|        - | 13438 | `	sxu32 n;` |
|    17858 | 13439 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13440 | `	/* Perform a linear search */` |
| 79657284 | 13441 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 79639434 | 13442 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13443 | `			/* Already included */` |
|        7 | 13444 | `			return TRUE;` |
|        - | 13445 | `		}` |
| 39819715 | 13446 | `	}` |
|    17852 | 13447 | `	return FALSE;` |
|     8930 | 13448 |  |
|        - | 13449 | `/*` |
|        - | 13450 | ` * Push a file path in the appropriate VM container.` |
|        - | 13451 | ` */` |
|    20784 | 13452 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13453 |  |
|        - | 13454 | `	SyString sPath;` |
|        - | 13455 | `	char *zDup;` |
|        - | 13456 | `#ifdef __WINNT__` |
|        - | 13457 | `	char *zCur;` |
|        - | 13458 | `#endif` |
|        - | 13459 | `	sxi32 rc;` |
|    20786 | 13460 | `	if( nLen < 0 ){` |
|     2930 | 13461 | `		nLen = SyStrlen(zPath);` |
|     1464 | 13462 | `	}` |
|        - | 13463 | `	/* Duplicate the file path first */` |
|    20786 | 13464 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20786 | 13465 | `	if( zDup == 0 ){` |
|      ! 0 | 13466 | `		return SXERR_MEM;` |
|        - | 13467 | `	}` |
|        - | 13468 | `#ifdef __WINNT__` |
|        - | 13469 | `	/* Normalize path on windows` |
|        - | 13470 | `	 * Example:` |
|        - | 13471 | `	 *    Path/To/File.php` |
|        - | 13472 | `	 * becomes` |
|        - | 13473 | `	 *   path\to\file.php` |
|        - | 13474 | `	 */` |
|        2 | 13475 | `	zCur = zDup;` |
|        2 | 13476 | `	while( zCur[0] != 0 ){` |
|        2 | 13477 | `		if( zCur[0] == '/' ){` |
|        2 | 13478 | `			zCur[0] = '\\';` |
|        2 | 13479 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13480 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13481 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13482 | `		}` |
|        2 | 13483 | `		zCur++;` |
|        2 | 13484 | `	}` |
|        - | 13485 | `#endif` |
|        - | 13486 | `	/* Install the file path */` |
|    20786 | 13487 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20786 | 13488 | `	if( !bMain ){` |
|    17858 | 13489 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13490 | `			/* Already included */` |
|        7 | 13491 | `			*pNew = 0;` |
|        4 | 13492 | `		}else{` |
|        - | 13493 | `			/* Insert in the corresponding container */` |
|    17852 | 13494 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17852 | 13495 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13496 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13497 | `				return rc;` |
|        - | 13498 | `			}` |
|    17852 | 13499 | `			*pNew = 1;` |
|        - | 13500 | `		}` |
|     8928 | 13501 | `	}` |
|    20786 | 13502 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20786 | 13503 | `	return SXRET_OK;` |
|    10394 | 13504 |  |
|        - | 13505 | `/*` |
|        - | 13506 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13507 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13508 | ` * indicates failure.` |
|        - | 13509 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13510 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13511 | ` * operations.` |
|        - | 13512 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13513 | ` * this function is a no-op.` |
|        - | 13514 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13515 | ` * constructs for more information.` |
|        - | 13516 | ` */` |
|     8940 | 13517 | `static sxi32 VmExecIncludedFile(` |
|        - | 13518 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13519 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13520 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13521 | `	 )` |
|        2 | 13522 |  |
|        - | 13523 | `	sxi32 rc;` |
|        - | 13524 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13525 | `	const ph7_io_stream *pStream;` |
|        - | 13526 | `	SyBlob sContents;` |
|        - | 13527 | `	void *pHandle;` |
|        - | 13528 | `	ph7_vm *pVm;` |
|        - | 13529 | `	int isNew;` |
|        - | 13530 | `	/* Initialize fields */` |
|     8942 | 13531 | `	pVm = pCtx->pVm;` |
|     8942 | 13532 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8942 | 13533 | `	isNew = 0;` |
|        - | 13534 | `	/* Extract the associated stream */` |
|     8942 | 13535 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 13536 | `	/*` |
|        - | 13537 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 13538 | `	 * in a read-only mode.` |
|        - | 13539 | `	 */` |
|     8942 | 13540 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8942 | 13541 | `	if( pHandle == 0 ){` |
|        8 | 13542 | `		return SXERR_IO;` |
|        - | 13543 | `	}` |
|     8936 | 13544 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8936 | 13545 | `	if( IncludeOnce && !isNew ){` |
|        - | 13546 | `		/* Already included */` |
|        5 | 13547 | `		rc = SXERR_EXISTS;` |
|        3 | 13548 | `	}else{` |
|        - | 13549 | `		/* Read the whole file contents */` |
|     8932 | 13550 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8932 | 13551 | `		if( rc == SXRET_OK ){` |
|        - | 13552 | `			SyString sScript;` |
|        - | 13553 | `			/* Compile and execute the script */` |
|     8932 | 13554 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8932 | 13555 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4465 | 13556 | `		}` |
|        - | 13557 | `	}` |
|        - | 13558 | `	/* Pop from the set of included file */` |
|     8936 | 13559 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13560 | `	/* Close the handle */` |
|     8936 | 13561 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13562 | `	/* Release the working buffer */` |
|     8936 | 13563 | `	SyBlobRelease(&sContents);` |
|        - | 13564 | `#else` |
|        - | 13565 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13566 | `	SXUNUSED(pPath);` |
|        - | 13567 | `	SXUNUSED(IncludeOnce);` |
|        - | 13568 | `	rc = SXERR_IO;` |
|        - | 13569 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8936 | 13570 | `	return rc;` |
|     4472 | 13571 |  |
|        - | 13572 | `/*` |
|        - | 13573 | ` * string get_include_path(void)` |
|        - | 13574 | ` *  Gets the current include_path configuration option.` |
|        - | 13575 | ` * Parameter` |
|        - | 13576 | ` *  None` |
|        - | 13577 | ` * Return` |
|        - | 13578 | ` *  Included paths as a string` |
|        - | 13579 | ` */` |
|        2 | 13580 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13581 |  |
|        3 | 13582 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13583 | `	SyString *aEntry;` |
|        - | 13584 | `	int dir_sep;` |
|        - | 13585 | `	sxu32 n;` |
|        - | 13586 | `#ifdef __WINNT__` |
|        1 | 13587 | `	dir_sep = ';';` |
|        - | 13588 | `#else` |
|        - | 13589 | `	/* Assume UNIX path separator */` |
|        2 | 13590 | `	dir_sep = ':';` |
|        - | 13591 | `#endif` |
|        1 | 13592 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13593 | `	SXUNUSED(apArg);` |
|        - | 13594 | `	/* Point to the list of import paths */` |
|        3 | 13595 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13596 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13597 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13598 | `		if( n > 0 ){` |
|        - | 13599 | `			/* Append dir seprator */` |
|      ! 0 | 13600 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13601 | `		}` |
|        - | 13602 | `		/* Append path */` |
|        3 | 13603 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13604 | `	}` |
|        3 | 13605 | `	return PH7_OK;` |
|        1 | 13606 |  |
|        - | 13607 | `/*` |
|        - | 13608 | ` * string get_get_included_files(void)` |
|        - | 13609 | ` *  Gets the current include_path configuration option.` |
|        - | 13610 | ` * Parameter` |
|        - | 13611 | ` *  None` |
|        - | 13612 | ` * Return` |
|        - | 13613 | ` *  Included paths as a string` |
|        - | 13614 | ` */` |
|        2 | 13615 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13616 |  |
|        3 | 13617 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13618 | `	ph7_value *pArray,*pWorker;` |
|        - | 13619 | `	SyString *pEntry;` |
|        - | 13620 | `	int c,d;` |
|        - | 13621 | `	/* Create an array and a working value */` |
|        3 | 13622 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13623 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13624 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13625 | `		/* Out of memory,return null */` |
|      ! 0 | 13626 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13627 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13628 | `		SXUNUSED(apArg);` |
|      ! 0 | 13629 | `		return PH7_OK;` |
|        - | 13630 | `	}` |
|        3 | 13631 | `	c = d = '/';` |
|        - | 13632 | `#ifdef __WINNT__` |
|        1 | 13633 | `	d = '\\';` |
|        - | 13634 | `#endif` |
|        - | 13635 | `	/* Iterate throw entries */` |
|        3 | 13636 | `	SySetResetCursor(pFiles);` |
|     3839 | 13637 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13638 | `		const char *zBase,*zEnd;` |
|        - | 13639 | `		int iLen;` |
|        - | 13640 | `		/* reset the string cursor */` |
|     3837 | 13641 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13642 | `		/* Extract base name */` |
|     3837 | 13643 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13644 | `		/* Ignore trailing '/' */` |
|     5755 | 13645 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13646 | `			zEnd--;` |
|      ! 0 | 13647 | `		}` |
|     3837 | 13648 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13649 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13650 | `			zEnd--;` |
|        1 | 13651 | `		}` |
|     3837 | 13652 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13653 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13654 | `		/* Copy entry name */` |
|     3837 | 13655 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13656 | `		/* Perform the insertion */` |
|     3837 | 13657 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13658 | `	}` |
|        - | 13659 | `	/* All done,return the created array */` |
|        3 | 13660 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13661 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13662 | `	 * by the engine as soon we return from this foreign` |
|        - | 13663 | `	 * function.` |
|        - | 13664 | `	 */` |
|        3 | 13665 | `	return PH7_OK;` |
|        2 | 13666 |  |
|        - | 13667 | `/*` |
|        - | 13668 | ` * include:` |
|        - | 13669 | ` * According to the PHP reference manual.` |
|        - | 13670 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13671 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13672 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13673 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13674 | ` *  and the current working directory before failing. The include()` |
|        - | 13675 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13676 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13677 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13678 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13679 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13680 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13681 | ` *  directory to find the requested file.` |
|        - | 13682 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13683 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13684 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13685 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13686 | ` */` |
|     8922 | 13687 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13688 |  |
|        - | 13689 | `	SyString sFile;` |
|        - | 13690 | `	sxi32 rc;` |
|     8924 | 13691 | `	if( nArg < 1 ){` |
|        - | 13692 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13693 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13694 | `		return SXRET_OK;` |
|        - | 13695 | `	}` |
|        - | 13696 | `	/* File to include */` |
|     8924 | 13697 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8924 | 13698 | `	if( sFile.nByte < 1 ){` |
|        - | 13699 | `		/* Empty string,return NULL */` |
|      ! 0 | 13700 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13701 | `		return SXRET_OK;` |
|        - | 13702 | `	}` |
|        - | 13703 | `	/* Open,compile and execute the desired script */` |
|     8924 | 13704 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8924 | 13705 | `	if( rc != SXRET_OK ){` |
|        - | 13706 | `		/* Emit a warning and return false */` |
|        3 | 13707 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13708 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13709 | `	}` |
|     8924 | 13710 | `	return SXRET_OK;` |
|     4463 | 13711 |  |
|        - | 13712 | `/*` |
|        - | 13713 | ` * include_once:` |
|        - | 13714 | ` *  According to the PHP reference manual.` |
|        - | 13715 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13716 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13717 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13718 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13719 | ` *   just once.` |
|        - | 13720 | ` */` |
|        4 | 13721 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13722 |  |
|        - | 13723 | `	SyString sFile;` |
|        - | 13724 | `	sxi32 rc;` |
|        5 | 13725 | `	if( nArg < 1 ){` |
|        - | 13726 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13727 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13728 | `		return SXRET_OK;` |
|        - | 13729 | `	}` |
|        - | 13730 | `	/* File to include */` |
|        5 | 13731 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13732 | `	if( sFile.nByte < 1 ){` |
|        - | 13733 | `		/* Empty string,return NULL */` |
|      ! 0 | 13734 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13735 | `		return SXRET_OK;` |
|        - | 13736 | `	}` |
|        - | 13737 | `	/* Open,compile and execute the desired script */` |
|        5 | 13738 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13739 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13740 | `		/* File already included,return TRUE */` |
|        3 | 13741 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13742 | `		return SXRET_OK;` |
|        - | 13743 | `	}` |
|        3 | 13744 | `	if( rc != SXRET_OK ){` |
|        - | 13745 | `		/* Emit a warning and return false */` |
|      ! 0 | 13746 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13747 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13748 | ` 	}` |
|        3 | 13749 | `	return SXRET_OK;` |
|        3 | 13750 |  |
|        - | 13751 | `/*` |
|        - | 13752 | ` * require.` |
|        - | 13753 | ` *  According to the PHP reference manual.` |
|        - | 13754 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13755 | ` *   also produce a fatal level error.` |
|        - | 13756 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13757 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13758 | ` */` |
|        6 | 13759 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13760 |  |
|        - | 13761 | `	SyString sFile;` |
|        - | 13762 | `	sxi32 rc;` |
|        8 | 13763 | `	if( nArg < 1 ){` |
|        - | 13764 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13765 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13766 | `		return SXRET_OK;` |
|        - | 13767 | `	}` |
|        - | 13768 | `	/* File to include */` |
|        8 | 13769 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13770 | `	if( sFile.nByte < 1 ){` |
|        - | 13771 | `		/* Empty string,return NULL */` |
|      ! 0 | 13772 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13773 | `		return SXRET_OK;` |
|        - | 13774 | `	}` |
|        - | 13775 | `	/* Open,compile and execute the desired script */` |
|        8 | 13776 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13777 | `	if( rc != SXRET_OK ){` |
|        - | 13778 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13779 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13780 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13781 | `		return PH7_ABORT;` |
|        - | 13782 | `	}` |
|        8 | 13783 | `	return SXRET_OK;` |
|        5 | 13784 |  |
|        - | 13785 | `/*` |
|        - | 13786 | ` * require_once:` |
|        - | 13787 | ` *  According to the PHP reference manual.` |
|        - | 13788 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13789 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13790 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13791 | ` *   and how it differs from its non _once siblings.` |
|        - | 13792 | ` */` |
|        4 | 13793 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13794 |  |
|        - | 13795 | `	SyString sFile;` |
|        - | 13796 | `	sxi32 rc;` |
|        5 | 13797 | `	if( nArg < 1 ){` |
|        - | 13798 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13799 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13800 | `		return SXRET_OK;` |
|        - | 13801 | `	}` |
|        - | 13802 | `	/* File to include */` |
|        5 | 13803 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13804 | `	if( sFile.nByte < 1 ){` |
|        - | 13805 | `		/* Empty string,return NULL */` |
|      ! 0 | 13806 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13807 | `		return SXRET_OK;` |
|        - | 13808 | `	}` |
|        - | 13809 | `	/* Open,compile and execute the desired script */` |
|        5 | 13810 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13811 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13812 | `		/* File already included,return TRUE */` |
|        3 | 13813 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13814 | `		return SXRET_OK;` |
|        - | 13815 | `	}` |
|        3 | 13816 | `	if( rc != SXRET_OK ){` |
|        - | 13817 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13818 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13819 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13820 | `		return PH7_ABORT;` |
|        - | 13821 | `	}` |
|        3 | 13822 | `	return SXRET_OK;` |
|        3 | 13823 |  |
|        - | 13824 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13825 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13826 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13827 | `/*` |
|        - | 13828 | ` * Section:` |
|        - | 13829 | ` *  SPL Autoloading functions.` |
|        - | 13830 | ` * Status:` |
|        - | 13831 | ` *  Stable.` |
|        - | 13832 | ` */` |
|        - | 13833 | `/*` |
|        - | 13834 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 13835 | ` *  Register given function as __autoload() implementation.` |
|        - | 13836 | ` * Parameters` |
|        - | 13837 | ` *  callback` |
|        - | 13838 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 13839 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 13840 | ` *  throw` |
|        - | 13841 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 13842 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 13843 | ` *  prepend` |
|        - | 13844 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 13845 | ` *   autoload stack instead of appending it.` |
|        - | 13846 | ` * Return` |
|        - | 13847 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13848 | ` */` |
|       34 | 13849 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13850 |  |
|        - | 13851 | `	VmAutoloadCB sEntry;` |
|       36 | 13852 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 13853 | `	int iPrepend = 0;` |
|        - | 13854 | `	sxu32 n;` |
|       36 | 13855 | `	if( nArg < 1 ){` |
|        - | 13856 | `		/* No callback provided — register default spl_autoload.` |
|        - | 13857 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 13858 | `		/* Check for duplicates first */` |
|        9 | 13859 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 13860 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 13861 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 13862 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 13863 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 13864 | `				ph7_result_bool(pCtx,1);` |
|        5 | 13865 | `				return SXRET_OK;` |
|        - | 13866 | `			}` |
|      ! 0 | 13867 | `		}` |
|        5 | 13868 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 13869 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 13870 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 13871 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 13872 | `		ph7_result_bool(pCtx,1);` |
|        5 | 13873 | `		return SXRET_OK;` |
|        - | 13874 | `	}` |
|        - | 13875 | `	/* Validate that the callback is callable */` |
|       28 | 13876 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 13877 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 13878 | `		if( nArg >= 2 ){` |
|      ! 0 | 13879 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13880 | `		}` |
|      ! 0 | 13881 | `		if( iThrow ){` |
|      ! 0 | 13882 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 13883 | `				"Argument is not callable");` |
|      ! 0 | 13884 | `		}` |
|      ! 0 | 13885 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13886 | `		return SXRET_OK;` |
|        - | 13887 | `	}` |
|        - | 13888 | `	/* Check for duplicates */` |
|       46 | 13889 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 13890 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 13891 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13892 | `			/* Already registered */` |
|      ! 0 | 13893 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13894 | `			return SXRET_OK;` |
|        - | 13895 | `		}` |
|       11 | 13896 | `	}` |
|        - | 13897 | `	/* Check prepend flag */` |
|       28 | 13898 | `	if( nArg >= 3 ){` |
|        3 | 13899 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 13900 | `	}` |
|        - | 13901 | `	/* Store the callback */` |
|       28 | 13902 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 13903 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 13904 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 13905 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 13906 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 13907 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 13908 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 13909 | `		VmAutoloadCB *aBase;` |
|        3 | 13910 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13911 | `		/* Rotate: move last entry to front */` |
|        3 | 13912 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 13913 | `		if( aBase ){` |
|        - | 13914 | `			VmAutoloadCB sTemp;` |
|        - | 13915 | `			sxu32 i;` |
|        3 | 13916 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 13917 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 13918 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 13919 | `			}` |
|        3 | 13920 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 13921 | `		}` |
|        2 | 13922 | `	}else{` |
|       26 | 13923 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13924 | `	}` |
|       28 | 13925 | `	ph7_result_bool(pCtx,1);` |
|       28 | 13926 | `	return SXRET_OK;` |
|       19 | 13927 |  |
|        - | 13928 | `/*` |
|        - | 13929 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 13930 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 13931 | ` * Parameters` |
|        - | 13932 | ` *  callback` |
|        - | 13933 | ` *   The autoload function being unregistered.` |
|        - | 13934 | ` * Return` |
|        - | 13935 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13936 | ` */` |
|       32 | 13937 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13938 |  |
|       34 | 13939 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13940 | `	sxu32 n,nEntry;` |
|       34 | 13941 | `	if( nArg < 1 ){` |
|      ! 0 | 13942 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13943 | `		return SXRET_OK;` |
|        - | 13944 | `	}` |
|       34 | 13945 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13946 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 13947 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 13948 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13949 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 13950 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13951 | `			sxu32 i;` |
|       32 | 13952 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13953 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13954 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13955 | `			}` |
|        - | 13956 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13957 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13958 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13959 | `			return SXRET_OK;` |
|        - | 13960 | `		}` |
|        3 | 13961 | `	}` |
|        3 | 13962 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13963 | `	return SXRET_OK;` |
|       18 | 13964 |  |
|        - | 13965 | `/*` |
|        - | 13966 | ` * array spl_autoload_functions(void)` |
|        - | 13967 | ` *  Return all registered __autoload() functions.` |
|        - | 13968 | ` * Return` |
|        - | 13969 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13970 | ` *  an empty array is returned.` |
|        - | 13971 | ` */` |
|       20 | 13972 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13973 |  |
|       21 | 13974 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13975 | `	ph7_value *pArray;` |
|        - | 13976 | `	sxu32 n,nEntry;` |
|       10 | 13977 | `	SXUNUSED(nArg);` |
|       10 | 13978 | `	SXUNUSED(apArg);` |
|       21 | 13979 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13980 | `	if( pArray == 0 ){` |
|      ! 0 | 13981 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13982 | `		return SXRET_OK;` |
|        - | 13983 | `	}` |
|       21 | 13984 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13985 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13986 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13987 | `		if( pEntry ){` |
|       15 | 13988 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13989 | `		}` |
|        8 | 13990 | `	}` |
|       21 | 13991 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13992 | `	return SXRET_OK;` |
|       11 | 13993 |  |
|        - | 13994 | `/*` |
|        - | 13995 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13996 | ` *  Default implementation of __autoload().` |
|        - | 13997 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13998 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13999 | ` * Parameters` |
|        - | 14000 | ` *  class` |
|        - | 14001 | ` *   The class name being searched.` |
|        - | 14002 | ` *  file_extensions` |
|        - | 14003 | ` *   Comma-separated list of file extensions to try.` |
|        - | 14004 | ` */` |
|        2 | 14005 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14006 |  |
|        - | 14007 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 14008 | `	SyBlob sPath;` |
|        - | 14009 | `	int nClass;` |
|        - | 14010 | `	sxi32 rc;` |
|        3 | 14011 | `	if( nArg < 1 ){` |
|      ! 0 | 14012 | `		return SXRET_OK;` |
|        - | 14013 | `	}` |
|        3 | 14014 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 14015 | `	if( nClass < 1 ){` |
|      ! 0 | 14016 | `		return SXRET_OK;` |
|        - | 14017 | `	}` |
|        - | 14018 | `	/* Default extensions */` |
|        3 | 14019 | `	zExt = ".php,.inc";` |
|        3 | 14020 | `	if( nArg >= 2 ){` |
|        - | 14021 | `		int nExt;` |
|      ! 0 | 14022 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 14023 | `		if( nExt < 1 ){` |
|      ! 0 | 14024 | `			zExt = ".php,.inc";` |
|      ! 0 | 14025 | `		}` |
|      ! 0 | 14026 | `	}` |
|        3 | 14027 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 14028 | `	/* Iterate over comma-separated extensions */` |
|        3 | 14029 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 14030 | `	zCur = zExt;` |
|        7 | 14031 | `	while( zCur < zEnd ){` |
|        - | 14032 | `		const char *zComma;` |
|        - | 14033 | `		SyString sFile;` |
|        - | 14034 | `		int i;` |
|        - | 14035 | `		/* Find next comma or end */` |
|        5 | 14036 | `		zComma = zCur;` |
|       21 | 14037 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 14038 | `			zComma++;` |
|        1 | 14039 | `		}` |
|        - | 14040 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 14041 | `		SyBlobReset(&sPath);` |
|       69 | 14042 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 14043 | `			char c = zClass[i];` |
|       65 | 14044 | `			if( c == '\\' ){` |
|      ! 0 | 14045 | `				c = '/';` |
|       65 | 14046 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 14047 | `				c = c + ('a' - 'A');` |
|        6 | 14048 | `			}` |
|       65 | 14049 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 14050 | `		}` |
|        - | 14051 | `		/* Append extension */` |
|        5 | 14052 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 14053 | `		/* Try to include the file */` |
|        5 | 14054 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 14055 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 14056 | `		if( rc == SXRET_OK ){` |
|        - | 14057 | `			/* File included successfully */` |
|      ! 0 | 14058 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 14059 | `			return SXRET_OK;` |
|        - | 14060 | `		}` |
|        - | 14061 | `		/* Move past the comma */` |
|        5 | 14062 | `		zCur = zComma;` |
|        5 | 14063 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 14064 | `			zCur++;` |
|        1 | 14065 | `		}` |
|        1 | 14066 | `	}` |
|        3 | 14067 | `	SyBlobRelease(&sPath);` |
|        3 | 14068 | `	return SXRET_OK;` |
|        2 | 14069 |  |
|        - | 14070 | `/* Table of built-in VM functions. */` |
|        - | 14071 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 14072 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 14073 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 14074 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 14075 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 14076 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 14077 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 14078 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 14079 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 14080 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 14081 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 14082 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 14083 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 14084 | `	    /* Constants management */` |
|        - | 14085 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 14086 | `	{ "define",   vm_builtin_define               },` |
|        - | 14087 | `	{ "constant", vm_builtin_constant             },` |
|        - | 14088 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 14089 | `	   /* Class/Object functions */` |
|        - | 14090 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 14091 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 14092 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 14093 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 14094 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 14095 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 14096 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 14097 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 14098 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 14099 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 14100 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 14101 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 14102 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 14103 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 14104 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 14105 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 14106 | `	   /* SPL Autoloading */` |
|        - | 14107 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 14108 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 14109 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 14110 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 14111 | `	   /* Random numbers/strings generators */` |
|        - | 14112 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 14113 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 14114 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 14115 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 14116 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 14117 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14118 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 14119 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 14120 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 14121 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14122 | `	   /* Language constructs functions */` |
|        - | 14123 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 14124 | `	{ "print", vm_builtin_print                   },` |
|        - | 14125 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 14126 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 14127 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 14128 | `	  /* Variable handling functions */` |
|        - | 14129 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 14130 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 14131 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 14132 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 14133 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 14134 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 14135 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 14136 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 14137 | `	  /* Ouput control functions */` |
|        - | 14138 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 14139 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 14140 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 14141 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 14142 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 14143 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 14144 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 14145 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 14146 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 14147 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 14148 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 14149 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 14150 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 14151 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 14152 | `	  /* Assertion functions */` |
|        - | 14153 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 14154 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 14155 | `	  /* Error reporting functions */` |
|        - | 14156 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 14157 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 14158 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 14159 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 14160 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 14161 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 14162 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 14163 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 14164 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 14165 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 14166 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 14167 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 14168 | `	  /* Release info */` |
|        - | 14169 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14170 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14171 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14172 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14173 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14174 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14175 | `	  /* hashmap */` |
|        - | 14176 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14177 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14178 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14179 | `	  /* URL related function */` |
|        - | 14180 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14181 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14182 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14183 | `	   /* XML processing functions */` |
|        - | 14184 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14185 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14186 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14187 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14188 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14189 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14190 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14191 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14192 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14193 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14194 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14195 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14196 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14197 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14198 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14199 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14200 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14201 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14202 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14203 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14204 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14205 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14206 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14207 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14208 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14209 | `	   /* Command line processing */` |
|        - | 14210 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14211 | `	   /* JSON encoding/decoding */` |
|        - | 14212 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14213 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14214 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14215 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14216 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14217 | `	   /* Files/URI inclusion facility */` |
|        - | 14218 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14219 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14220 | `	{ "include",      vm_builtin_include          },` |
|        - | 14221 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14222 | `	{ "require",      vm_builtin_require          },` |
|        - | 14223 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14224 | `};` |
|        - | 14225 | `/*` |
|        - | 14226 | ` * Register the built-in VM functions defined above.` |
|        - | 14227 | ` */` |
|     2622 | 14228 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14229 |  |
|        - | 14230 | `	sxi32 rc;` |
|        - | 14231 | `	sxu32 n;` |
|   338240 | 14232 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14233 | `		/* Note that these special functions have access` |
|        - | 14234 | `		 * to the underlying virtual machine as their` |
|        - | 14235 | `		 * private data.` |
|        - | 14236 | `		 */` |
|   335618 | 14237 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   335618 | 14238 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14239 | `			return rc;` |
|        - | 14240 | `		}` |
|   167810 | 14241 | `	}` |
|     2624 | 14242 | `	return SXRET_OK;` |
|     1313 | 14243 |  |
|        - | 14244 | `/*` |
|        - | 14245 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 14246 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 14247 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 14248 | ` */` |
|    40748 | 14249 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 14250 |  |
|    40750 | 14251 | `	if( !iLoadable ){` |
|    39056 | 14252 | `		return pClass;` |
|        - | 14253 | `	}` |
|     1700 | 14254 | `	while(pClass){` |
|     1696 | 14255 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1692 | 14256 | `			return pClass;` |
|        - | 14257 | `		}` |
|        5 | 14258 | `		pClass = pClass->pNextName;` |
|        1 | 14259 | `	}` |
|        5 | 14260 | `	return 0;` |
|    20376 | 14261 |  |
|        - | 14262 | `/*` |
|        - | 14263 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 14264 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 14265 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 14266 | ` * registered in the VM's class table.` |
|        - | 14267 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 14268 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 14269 | ` */` |
|       38 | 14270 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14271 |  |
|        - | 14272 | `	VmAutoloadCB *pEntry;` |
|        - | 14273 | `	ph7_value sArg,sResult;` |
|        - | 14274 | `	SyHashEntry *pHashEntry;` |
|        - | 14275 | `	ph7_class *pClass;` |
|        - | 14276 | `	sxu32 n,nEntry;` |
|       40 | 14277 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 14278 | `	if( nEntry < 1 ){` |
|       26 | 14279 | `		return 0;` |
|        - | 14280 | `	}` |
|        - | 14281 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 14282 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 14283 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 14284 | `	}` |
|        - | 14285 | `	/* Mark this class as being autoloaded */` |
|       14 | 14286 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 14287 | `	/* Prepare the class name argument */` |
|       14 | 14288 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 14289 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 14290 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 14291 | `	pClass = 0;` |
|       28 | 14292 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 14293 | `		ph7_value *apArg[1];` |
|       24 | 14294 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 14295 | `		if( pEntry == 0 ){` |
|      ! 0 | 14296 | `			continue;` |
|        - | 14297 | `		}` |
|       24 | 14298 | `		apArg[0] = &sArg;` |
|       24 | 14299 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 14300 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 14301 | `			continue;` |
|        - | 14302 | `		}` |
|        - | 14303 | `		/* Check if the class is now available */` |
|       24 | 14304 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 14305 | `		if( pHashEntry ){` |
|       10 | 14306 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 14307 | `			if( pClass ){` |
|       10 | 14308 | `				break;` |
|        - | 14309 | `			}` |
|      ! 0 | 14310 | `		}` |
|        9 | 14311 | `	}` |
|       14 | 14312 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 14313 | `	PH7_MemObjRelease(&sResult);` |
|        - | 14314 | `	/* Remove reentrancy guard */` |
|       14 | 14315 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 14316 | `	return pClass;` |
|       21 | 14317 |  |
|        - | 14318 | `/*` |
|        - | 14319 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 14320 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 14321 | ` */` |
|       18 | 14322 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14323 |  |
|       20 | 14324 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 14325 |  |
|        - | 14326 | `/*` |
|        - | 14327 | ` * Check if the given name refer to an installed class.` |
|        - | 14328 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14329 | ` */` |
|    40760 | 14330 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14331 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14332 | `	const char *zName,  /* Name of the target class */` |
|        - | 14333 | `	sxu32 nByte,        /* zName length */` |
|        - | 14334 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14335 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14336 | `						 */` |
|        - | 14337 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14338 | `	)` |
|        2 | 14339 |  |
|        - | 14340 | `	SyHashEntry *pEntry;` |
|        - | 14341 | `	ph7_class *pClass;` |
|    20380 | 14342 | `	SXUNUSED(iNest);` |
|        - | 14343 | `	/* Exact class lookup.` |
|        - | 14344 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 14345 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    40762 | 14346 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    40762 | 14347 | `	if( pEntry == 0 ){` |
|        - | 14348 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 14349 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 14350 | `	}` |
|    40742 | 14351 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    40742 | 14352 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20382 | 14353 |  |
|        - | 14354 | `/*` |
|        - | 14355 | ` * Reference Table Implementation` |
|        - | 14356 | ` * Status: stable <chm@symisc.net>` |
|        - | 14357 | ` * Intro` |
|        - | 14358 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14359 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14360 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14361 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14362 | ` *  Refer to the official for more information on this powerful` |
|        - | 14363 | ` *  extension.` |
|        - | 14364 | ` */` |
|        - | 14365 | `/*` |
|        - | 14366 | ` * Allocate a new reference entry.` |
|        - | 14367 | ` */` |
|  3123508 | 14368 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14369 |  |
|        - | 14370 | `	VmRefObj *pRef;` |
|        - | 14371 | `	/* Allocate a new instance */` |
|  3123510 | 14372 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3123510 | 14373 | `	if( pRef == 0 ){` |
|      ! 0 | 14374 | `		return 0;` |
|        - | 14375 | `	}` |
|        - | 14376 | `	/* Zero the structure */` |
|  3123510 | 14377 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14378 | `	/* Initialize fields */` |
|  3123510 | 14379 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3123510 | 14380 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3123510 | 14381 | `	pRef->nIdx = nIdx;` |
|  3123510 | 14382 | `	return pRef;` |
|  1561756 | 14383 |  |
|        - | 14384 | `/*` |
|        - | 14385 | ` * Default hash function used by the reference table` |
|        - | 14386 | ` * for lookup/insertion operations.` |
|        - | 14387 | ` */` |
| 17195243 | 14388 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14389 |  |
|        - | 14390 | `	/* Calculate the hash based on the memory object index */` |
| 17195245 | 14391 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14392 |  |
|        - | 14393 | `/*` |
|        - | 14394 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14395 | ` * in the reference table.` |
|        - | 14396 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14397 | ` * otherwise.` |
|        - | 14398 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14399 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14400 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14401 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14402 | ` * Refer to the official for more information on this powerful` |
|        - | 14403 | ` * extension.` |
|        - | 14404 | ` */` |
|  9316118 | 14405 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14406 |  |
|        - | 14407 | `	VmRefObj *pRef;` |
|        - | 14408 | `	sxu32 nBucket;` |
|        - | 14409 | `	/* Point to the appropriate bucket */` |
|  9316120 | 14410 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14411 | `	/* Perform the lookup */` |
|  9316120 | 14412 | `	pRef = pVm->apRefObj[nBucket];` |
| 20247383 | 14413 | `	for(;;){` |
| 40477876 | 14414 | `		if( pRef == 0 ){` |
|  3220850 | 14415 | `			break;` |
|        - | 14416 | `		}` |
| 37257028 | 14417 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14418 | `			/* Entry found */` |
|  6095272 | 14419 | `			return pRef;` |
|        - | 14420 | `		}` |
|        - | 14421 | `		/* Point to the next entry */` |
| 31161758 | 14422 | `		pRef = pRef->pNextCollide;` |
|        2 | 14423 | `	}` |
|        - | 14424 | `	/* No such entry,return NULL */` |
|  3220850 | 14425 | `	return 0;` |
|  4658061 | 14426 |  |
|        - | 14427 | `/*` |
|        - | 14428 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14429 | ` *` |
|        - | 14430 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14431 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14432 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14433 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14434 | ` * Refer to the official for more information on this powerful` |
|        - | 14435 | ` * extension.` |
|        - | 14436 | ` */` |
|  3123508 | 14437 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14438 |  |
|        - | 14439 | `	sxu32 nBucket;` |
|  3123510 | 14440 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14441 | `		VmRefObj **apNew;` |
|        - | 14442 | `		sxu32 nNew;` |
|        - | 14443 | `		/* Allocate a larger table */` |
|     4462 | 14444 | `		nNew = pVm->nRefSize << 1;` |
|     4462 | 14445 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4462 | 14446 | `		if( apNew ){` |
|     4462 | 14447 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14448 | `			sxu32 n;` |
|        - | 14449 | `			/* Zero the structure */` |
|     4462 | 14450 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14451 | `			/* Rehash all referenced entries */` |
|  2845672 | 14452 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14453 | `				/* Remove old collision links */` |
|  2841212 | 14454 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14455 | `				/* Point to the appropriate bucket */` |
|  2841212 | 14456 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14457 | `				/* Insert the entry  */` |
|  2841212 | 14458 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2841212 | 14459 | `				if( apNew[nBucket] ){` |
|  2298896 | 14460 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14461 | `				}` |
|  2841212 | 14462 | `				apNew[nBucket] = pEntry;` |
|        - | 14463 | `				/* Point to the next entry */` |
|  2841212 | 14464 | `				pEntry = pEntry->pNext;` |
|  1420607 | 14465 | `			}` |
|        - | 14466 | `			/* Release the old table */` |
|     4462 | 14467 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14468 | `			/* Install the new one */` |
|     4462 | 14469 | `			pVm->apRefObj = apNew;` |
|     4462 | 14470 | `			pVm->nRefSize = nNew;` |
|     2230 | 14471 | `		}` |
|     2230 | 14472 | `	}` |
|        - | 14473 | `	/* Point to the appropriate bucket */` |
|  3123510 | 14474 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14475 | `	/* Insert the entry */` |
|  3123510 | 14476 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3123510 | 14477 | `	if( pVm->apRefObj[nBucket] ){` |
|  2560958 | 14478 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1280505 | 14479 | `	}` |
|  3123510 | 14480 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3123510 | 14481 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3123510 | 14482 | `	pVm->nRefUsed++;` |
|  3123510 | 14483 | `	return SXRET_OK;` |
|        2 | 14484 |  |
|        - | 14485 | `/*` |
|        - | 14486 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14487 | ` * the reference table.` |
|        - | 14488 | ` * This function is invoked when the user perform an unset` |
|        - | 14489 | ` * call [i.e: unset($var); ].` |
|        - | 14490 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14491 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14492 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14493 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14494 | ` * Refer to the official for more information on this powerful` |
|        - | 14495 | ` * extension.` |
|        - | 14496 | ` */` |
|  3085372 | 14497 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14498 |  |
|        - | 14499 | `	ph7_hashmap_node **apNode;` |
|        - | 14500 | `	SyHashEntry **apEntry;` |
|        - | 14501 | `	sxu32 n;` |
|        - | 14502 | `	/* Point to the reference table */` |
|  3085374 | 14503 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3085374 | 14504 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14505 | `	/* Unlink the entry from the reference table */` |
|  3189192 | 14506 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   103820 | 14507 | `		if( apEntry[n] ){` |
|   103770 | 14508 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    51884 | 14509 | `		}` |
|    51911 | 14510 | `	}` |
|  6068722 | 14511 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2983350 | 14512 | `		if( apNode[n] ){` |
|     7406 | 14513 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3702 | 14514 | `		}` |
|  1491676 | 14515 | `	}` |
|  3085374 | 14516 | `	if( pRef->pPrevCollide ){` |
|  1170967 | 14517 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   585656 | 14518 | `	}else{` |
|  1914409 | 14519 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14520 | `	}` |
|  3085374 | 14521 | `	if( pRef->pNextCollide ){` |
|  1747943 | 14522 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   873971 | 14523 | `	}` |
|  3085374 | 14524 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14525 | `	/* Release the node */` |
|  3085374 | 14526 | `	SySetRelease(&pRef->aReference);` |
|  3085374 | 14527 | `	SySetRelease(&pRef->aArrEntries);` |
|  3085374 | 14528 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3085374 | 14529 | `	pVm->nRefUsed--;` |
|  3085374 | 14530 | `	return SXRET_OK;` |
|        2 | 14531 |  |
|        - | 14532 | `/*` |
|        - | 14533 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14534 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14535 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14536 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14537 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14538 | ` * Refer to the official for more information on this powerful` |
|        - | 14539 | ` * extension.` |
|        - | 14540 | ` */` |
|  3157396 | 14541 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14542 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14543 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14544 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14545 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14546 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14547 | `	)` |
|        2 | 14548 |  |
|  3157398 | 14549 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14550 | `	VmRefObj *pRef;` |
|        - | 14551 | `	/* Check if the referenced object already exists */` |
|  3157398 | 14552 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3157398 | 14553 | `	if( pRef == 0 ){` |
|        - | 14554 | `		/* Create a new entry */` |
|  3123510 | 14555 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3123510 | 14556 | `		if( pRef == 0 ){` |
|      ! 0 | 14557 | `			return SXERR_MEM;` |
|        - | 14558 | `		}` |
|  3123510 | 14559 | `		pRef->iFlags = iFlags;` |
|        - | 14560 | `		/* Install the entry */` |
|  3123510 | 14561 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1561754 | 14562 | `	}` |
|  3157398 | 14563 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3157398 | 14564 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14565 | `		VmSlot sRef;` |
|        - | 14566 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14567 | `		 * be deleted when we leave this frame.` |
|        - | 14568 | `		 */` |
|    97438 | 14569 | `		sRef.nIdx = nIdx;` |
|    97438 | 14570 | `		sRef.pUserData = pEntry;` |
|    97438 | 14571 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14572 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14573 | `		}` |
|    48718 | 14574 | `	}` |
|  3157398 | 14575 | `	if( pEntry ){` |
|        - | 14576 | `		/* Address of the hash-entry */` |
|   131126 | 14577 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    65562 | 14578 | `	}` |
|  3157398 | 14579 | `	if( pMapEntry ){` |
|        - | 14580 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3019254 | 14581 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1509626 | 14582 | `	}` |
|  3157398 | 14583 | `	return SXRET_OK;` |
|  1578700 | 14584 |  |
|        - | 14585 | `/*` |
|        - | 14586 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14587 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14588 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14589 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14590 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14591 | ` * Refer to the official for more information on this powerful` |
|        - | 14592 | ` * extension.` |
|        - | 14593 | ` */` |
|  3073344 | 14594 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14595 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14596 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14597 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14598 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14599 | `	)` |
|        2 | 14600 |  |
|        - | 14601 | `	VmRefObj *pRef;` |
|        - | 14602 | `	sxu32 n;` |
|        - | 14603 | `	/* Check if the referenced object already exists */` |
|  3073346 | 14604 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3073346 | 14605 | `	if( pRef == 0 ){` |
|        - | 14606 | `		/* Not such entry */` |
|    97336 | 14607 | `		return SXERR_NOTFOUND;` |
|        - | 14608 | `	}` |
|        - | 14609 | `	/* Remove the desired entry */` |
|  2976012 | 14610 | `	if( pEntry ){` |
|        - | 14611 | `		SyHashEntry **apEntry;` |
|       62 | 14612 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 14613 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 14614 | `			if( apEntry[n] == pEntry ){` |
|        - | 14615 | `				/* Nullify the entry */` |
|       62 | 14616 | `				apEntry[n] = 0;` |
|        - | 14617 | `				/*` |
|        - | 14618 | `				 * NOTE:` |
|        - | 14619 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14620 | `				 * we avoid wasting spaces.` |
|        - | 14621 | `				 */` |
|       30 | 14622 | `			}` |
|       85 | 14623 | `		}` |
|       30 | 14624 | `	}` |
|  2976012 | 14625 | `	if( pMapEntry ){` |
|        - | 14626 | `		ph7_hashmap_node **apNode;` |
|  2975952 | 14627 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5951996 | 14628 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2976046 | 14629 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14630 | `				/* nullify the entry */` |
|  2975952 | 14631 | `				apNode[n] = 0;` |
|  1487975 | 14632 | `			}` |
|  1488024 | 14633 | `		}` |
|  1487975 | 14634 | `	}` |
|  2976012 | 14635 | `	return SXRET_OK;` |
|  1536674 | 14636 |  |
|        - | 14637 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14638 | `/*` |
|        - | 14639 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14640 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14641 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14642 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14643 | ` * For more information on how to register IO stream devices,please` |
|        - | 14644 | ` * refer to the official documentation.` |
|        - | 14645 | ` */` |
|    27186 | 14646 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14647 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14648 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14649 | `	int nByte              /* *pzDevice length*/` |
|        - | 14650 | `	)` |
|        2 | 14651 |  |
|        - | 14652 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14653 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14654 | `	SyString sDev,sCur;` |
|        - | 14655 | `	sxu32 n,nEntry;` |
|        - | 14656 | `	int rc;` |
|        - | 14657 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    27188 | 14658 | `	zNext = zCur = zIn = *pzDevice;` |
|    27188 | 14659 | `	zEnd = &zIn[nByte];` |
|  1725866 | 14660 | `	while( zIn < zEnd ){` |
|  1698682 | 14661 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14662 | `			/* Got one */` |
|        3 | 14663 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14664 | `			break;` |
|        - | 14665 | `		}` |
|        - | 14666 | `		/* Advance the cursor */` |
|  1698680 | 14667 | `		zIn++;` |
|        2 | 14668 | `	}` |
|    27188 | 14669 | `	if( zIn >= zEnd ){` |
|        - | 14670 | `		/* No such scheme,return the default stream */` |
|    27186 | 14671 | `		return pVm->pDefStream;` |
|        - | 14672 | `	}` |
|        3 | 14673 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14674 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14675 | `	SyStringFullTrim(&sDev);` |
|        - | 14676 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14677 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14678 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14679 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14680 | `		pStream = apStream[n];` |
|        3 | 14681 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14682 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14683 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14684 | `		if( rc == 0 ){` |
|        - | 14685 | `			/* Stream device found */` |
|        3 | 14686 | `			*pzDevice = zNext;` |
|        3 | 14687 | `			return pStream;` |
|        - | 14688 | `		}` |
|      ! 0 | 14689 | `	}` |
|        - | 14690 | `	/* No such stream,return NULL */` |
|      ! 0 | 14691 | `	return 0;` |
|    13595 | 14692 |  |
|        - | 14693 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14694 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14695 |  |
