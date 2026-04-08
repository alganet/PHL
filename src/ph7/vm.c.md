# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5038/6598 lines (76.36%)

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
|   793030 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   793032 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   792998 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   792988 |   104 | `	return FALSE;` |
|   396539 |   105 |  |
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
|   506070 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   506072 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   506072 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   506068 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   506068 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   506068 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   506068 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   506068 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   506068 |   152 | `	pCons->xExpand = xExpand;` |
|   506068 |   153 | `	pCons->pUserData = pUserData;` |
|   506068 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   506068 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   506068 |   161 | `	return SXRET_OK;` |
|   253037 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1112640 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1112642 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1112642 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1112642 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1112642 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1112642 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1112642 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1112642 |   195 | `	pFunc->pVm   = pVm;` |
|  1112642 |   196 | `	pFunc->xFunc = xFunc;` |
|  1112642 |   197 | `	pFunc->pUserData = pUserData;` |
|  1112642 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1112642 |   200 | `	*ppOut = pFunc;` |
|  1112642 |   201 | `	return SXRET_OK;` |
|   556322 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1114972 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1114974 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1114974 |   223 | `	if( pEntry ){` |
|     2334 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2334 |   225 | `		pFunc->pUserData = pUserData;` |
|     2334 |   226 | `		pFunc->xFunc = xFunc;` |
|     2334 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2334 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1112642 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1112642 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1112642 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1112642 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1112642 |   243 | `	return SXRET_OK;` |
|   557488 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   159226 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   159228 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   159228 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   159228 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   159228 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   159228 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   159228 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   159228 |   270 | `	pFunc->iFlags = iFlags;` |
|   159228 |   271 | `	pFunc->pUserData = pUserData;` |
|   159228 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   159228 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   625696 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   625698 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    34352 |   293 | `		pName = &pFunc->sName;` |
|    17175 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   625698 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   625698 |   297 | `	if( pEntry ){` |
|   487472 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   487472 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      184 |   301 | `			pFunc->pNextName = pLink;` |
|      184 |   302 | `			pEntry->pUserData = pFunc;` |
|       91 |   303 | `		}` |
|   487472 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   138228 |   307 | `	pFunc->pNextName = 0;` |
|   138228 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   138228 |   309 | `	return rc;` |
|   312850 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    44562 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    44564 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    44564 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    44564 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    44534 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    44534 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    44534 |   334 | `	return rc;` |
|    22283 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3214468 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   340 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   341 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   342 | `	sxi32 iP1,    /* First operand */` |
|        - |   343 | `	sxu32 iP2,    /* Second operand */` |
|        - |   344 | `	void *p3,     /* Third operand */` |
|        - |   345 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   346 | `	)` |
|        2 |   347 |  |
|        - |   348 | `	VmInstr sInstr;` |
|        - |   349 | `	sxi32 rc;` |
|        - |   350 | `	/* Fill the VM instruction */` |
|  3214470 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3214470 |   352 | `	sInstr.iP1 = iP1;` |
|  3214470 |   353 | `	sInstr.iP2 = iP2;` |
|  3214470 |   354 | `	sInstr.p3  = p3;` |
|  3214470 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   185446 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    92722 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3214470 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3214470 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3214470 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   381272 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   381274 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   381274 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   381274 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   190636 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   190638 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   182776 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   182778 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   182778 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   963386 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   963388 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   173738 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   173740 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   622462 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   622464 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    26656 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    26658 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26658 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    26658 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26658 |   427 | `	return &aInstr[n - 2];` |
|    13330 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16130 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16132 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16132 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16132 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16132 |   447 | `	pFrame->pUserData = pUserData;` |
|    16132 |   448 | `	pFrame->pThis = pThis;` |
|    16132 |   449 | `	pFrame->pVm = pVm;` |
|    16132 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16132 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16132 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16132 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16132 |   454 | `	return pFrame;` |
|     8067 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    16088 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    16090 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16090 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    16090 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    16090 |   476 | `	pVm->pFrame = pFrame;` |
|    16090 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13498 |   479 | `		*ppFrame = pFrame;` |
|     6748 |   480 | `	}` |
|    16090 |   481 | `	return SXRET_OK;` |
|     8046 |   482 |  |
|        - |   483 | `/*` |
|        - |   484 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   485 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   486 | ` * information.` |
|        - |   487 | ` */` |
|       52 |   488 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   489 |  |
|        - |   490 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   491 | `	SyHashEntry *pEntry = 0;` |
|        - |   492 | `	sxi32 rc;` |
|        - |   493 | `	/* Point to the upper frame */` |
|       54 |   494 | `	pFrame = pVm->pFrame;` |
|       54 |   495 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       54 |   496 | `	pTarget = pFrame;` |
|       54 |   497 | `	pFrame = pTarget->pParent;` |
|       54 |   498 | `	while( pFrame ){` |
|       54 |   499 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   500 | `			/* Query the current frame */` |
|       54 |   501 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   502 | `			if( pEntry ){` |
|        - |   503 | `				/* Variable found */` |
|       54 |   504 | `				break;` |
|        - |   505 | `			}` |
|      ! 0 |   506 | `		}` |
|        - |   507 | `		/* Point to the upper frame */` |
|      ! 0 |   508 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   509 | `	}` |
|       54 |   510 | `	if( pEntry == 0 ){` |
|        - |   511 | `		/* Inexistant variable */` |
|      ! 0 |   512 | `		return SXERR_NOTFOUND;` |
|        - |   513 | `	}` |
|        - |   514 | `	/* Link to the current frame */` |
|       54 |   515 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   516 | `	if( rc == SXRET_OK ){` |
|        - |   517 | `		sxu32 nIdx;` |
|       54 |   518 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   519 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   520 | `	}` |
|       54 |   521 | `	return rc;` |
|       28 |   522 |  |
|        - |   523 | `/*` |
|        - |   524 | ` * Leave the top-most active frame.` |
|        - |   525 | ` */` |
|    13496 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13498 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13498 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13498 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13498 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13434 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    93420 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    79988 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39995 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13434 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    93476 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    80044 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    40023 |   545 | `			}` |
|     6716 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13498 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13498 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13498 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13498 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13498 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6748 |   554 | `	}` |
|    13498 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6410418 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6410696 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6410420 |   566 | `	return pFrame;` |
|        2 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Compare two functions signature and return the comparison result.` |
|        - |   570 | ` */` |
|      818 |   571 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   572 |  |
|      819 |   573 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   574 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   575 | `	const char *zSin = pSecond->zString;` |
|      819 |   576 | `	const char *zFin = pFirst->zString;` |
|      819 |   577 | `	const char *zPtr = zFin;` |
|      409 |   578 | `	for(;;){` |
|      819 |   579 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   580 | `			break;` |
|        - |   581 | `		}` |
|      ! 0 |   582 | `		if( zFin[0] != zSin[0] ){` |
|        - |   583 | `			/* mismatch */` |
|      ! 0 |   584 | `			break;` |
|        - |   585 | `		}` |
|      ! 0 |   586 | `		zFin++;` |
|      ! 0 |   587 | `		zSin++;` |
|      ! 0 |   588 | `	}` |
|      819 |   589 | `	return (int)(zFin-zPtr);` |
|        1 |   590 |  |
|        - |   591 | `/*` |
|        - |   592 | ` * Select the appropriate VM function for the current call context.` |
|        - |   593 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   594 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   595 | ` * Refer to the official documentation for more information.` |
|        - |   596 | ` */` |
|      132 |   597 | `static ph7_vm_func * VmOverload(` |
|        - |   598 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   599 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   600 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   601 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   602 | `	)` |
|        2 |   603 |  |
|        - |   604 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   605 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   606 | `	ph7_vm_func *pLink;` |
|        - |   607 | `	SyString sArgSig;` |
|        - |   608 | `	SyBlob sSig;` |
|        - |   609 |  |
|      134 |   610 | `	pLink = pList;` |
|      134 |   611 | `	i = 0;` |
|        - |   612 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   613 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   614 | `		if( pLink == 0 ){` |
|       72 |   615 | `			break;` |
|        - |   616 | `		}` |
|      930 |   617 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   618 | `			/* Candidate for overloading */` |
|      884 |   619 | `			apSet[i++] = pLink;` |
|      441 |   620 | `		}` |
|        - |   621 | `		/* Point to the next entry */` |
|      930 |   622 | `		pLink = pLink->pNextName;` |
|        2 |   623 | `	}` |
|      134 |   624 | `	if( i < 1 ){` |
|        - |   625 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   626 | `		return pList;` |
|        - |   627 | `	}` |
|      134 |   628 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   629 | `		/* Return the only candidate */` |
|       32 |   630 | `		return apSet[0];` |
|        - |   631 | `	}` |
|        - |   632 | `	/* Calculate function signature */` |
|      103 |   633 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   634 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   635 | `		int c = 'n'; /* null */` |
|      253 |   636 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   637 | `			/* Hashmap */` |
|       45 |   638 | `			c = 'h';` |
|      231 |   639 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   640 | `			/* bool */` |
|      ! 0 |   641 | `			c = 'b';` |
|      209 |   642 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   643 | `			/* int */` |
|        5 |   644 | `			c = 'i';` |
|      207 |   645 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   646 | `			/* String */` |
|      105 |   647 | `			c = 's';` |
|      153 |   648 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   649 | `			/* Float */` |
|      ! 0 |   650 | `			c = 'f';` |
|      101 |   651 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   652 | `			/* Class instance */` |
|      ! 0 |   653 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   654 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   655 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   656 | `			c = -1;` |
|      ! 0 |   657 | `		}` |
|      253 |   658 | `		if( c > 0 ){` |
|      253 |   659 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   660 | `		}` |
|      127 |   661 | `	}` |
|      103 |   662 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   663 | `	iTarget = 0;` |
|      103 |   664 | `	iMax = -1;` |
|        - |   665 | `	/* Select the appropriate function */` |
|      921 |   666 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   667 | `		/* Compare the two signatures */` |
|      819 |   668 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   669 | `		if( iCur > iMax ){` |
|      103 |   670 | `			iMax = iCur;` |
|      103 |   671 | `			iTarget = j;` |
|       51 |   672 | `		}` |
|      410 |   673 | `	}` |
|      103 |   674 | `	SyBlobRelease(&sSig);` |
|        - |   675 | `	/* Appropriate function for the current call context */` |
|      103 |   676 | `	return apSet[iTarget];` |
|       68 |   677 |  |
|        - |   678 | `/* Forward declaration */` |
|        - |   679 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   680 | `/*` |
|        - |   681 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   682 | ` * it can be instanciated from the executed PHP script.` |
|        - |   683 | ` */` |
|   122020 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   122022 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   514468 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   697 | `		/* Extract the current attribute */` |
|   331438 |   698 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   331438 |   699 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   700 | `			ph7_value *pMemObj;` |
|        - |   701 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   702 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   703 | `			if( pMemObj == 0 ){` |
|      ! 0 |   704 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   705 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   706 | `					&pClass->sName,&pAttr->sName` |
|        - |   707 | `					);` |
|      ! 0 |   708 | `				return SXERR_MEM;` |
|        - |   709 | `			}` |
|     1294 |   710 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   711 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   712 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   713 | `			}` |
|        - |   714 | `			/* Record attribute index */` |
|     1294 |   715 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   716 | `			/* Install static attribute in the reference table */` |
|     1294 |   717 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   718 | `		}` |
|        2 |   719 | `	}` |
|        - |   720 | `	/* Install class methods */` |
|   122022 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    52726 |   724 | `		return SXRET_OK;` |
|        - |   725 | `	}` |
|        - |   726 | `	/* Create constructor alias if not yet done */` |
|    69298 |   727 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   728 | `		/* User constructor with the same base class name */` |
|     5242 |   729 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5242 |   730 | `		if( pEntry ){` |
|      ! 0 |   731 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   732 | `			/* Create the alias */` |
|      ! 0 |   733 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   734 | `		}` |
|     2620 |   735 | `	}` |
|        - |   736 | `	/* Install the methods now */` |
|    69298 |   737 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   695300 |   738 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   591356 |   739 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   591356 |   740 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   591348 |   741 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   591348 |   742 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   743 | `				return rc;` |
|        - |   744 | `			}` |
|   295673 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    69298 |   748 | `	pClass->bMounted = TRUE;` |
|    69298 |   749 | `	return SXRET_OK;` |
|    61012 |   750 |  |
|        - |   751 | `/*` |
|        - |   752 | ` * Allocate a private frame for attributes of the given` |
|        - |   753 | ` * class instance (Object in the PHP jargon).` |
|        - |   754 | ` */` |
|     1214 |   755 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   756 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   757 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   758 | `	)` |
|        2 |   759 |  |
|     1216 |   760 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   761 | `	ph7_class_attr *pAttr;` |
|        - |   762 | `	SyHashEntry *pEntry;` |
|        - |   763 | `	sxi32 rc;` |
|        - |   764 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1216 |   765 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4976 |   766 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   767 | `		VmClassAttr *pVmAttr;` |
|        - |   768 | `		/* Extract the current attribute */` |
|     3762 |   769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3762 |   770 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3762 |   771 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   772 | `			return SXERR_MEM;` |
|        - |   773 | `		}` |
|     3762 |   774 | `		pVmAttr->pAttr = pAttr;` |
|     3762 |   775 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   776 | `			ph7_value *pMemObj;` |
|        - |   777 | `			/* Reserve a memory object for this attribute */` |
|     3756 |   778 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3756 |   779 | `			if( pMemObj == 0 ){` |
|      ! 0 |   780 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   781 | `				return SXERR_MEM;` |
|        - |   782 | `			}` |
|     3756 |   783 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3756 |   784 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   785 | `				/* Initialize attribute default value (any complex expression) */` |
|     1212 |   786 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      605 |   787 | `			}` |
|     3756 |   788 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3756 |   789 | `			if( rc != SXRET_OK ){` |
|        - |   790 | `				VmSlot sSlot;` |
|        - |   791 | `				/* Restore memory object */` |
|      ! 0 |   792 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   793 | `				sSlot.pUserData = 0;` |
|      ! 0 |   794 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `			/* Install attribute in the reference table */` |
|     3756 |   799 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1879 |   800 | `		}else{` |
|        - |   801 | `			/* Install static/constant attribute */` |
|        8 |   802 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   803 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   804 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|        - |   808 | `		}` |
|        2 |   809 | `	}` |
|     1216 |   810 | `	return SXRET_OK;` |
|      609 |   811 |  |
|        - |   812 | `/* Forward declaration */` |
|        - |   813 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   814 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   815 | `/*` |
|        - |   816 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   817 | ` */` |
|        - |   818 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   819 | `/*` |
|        - |   820 | ` * Reserve a constant memory object.` |
|        - |   821 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   822 | ` */` |
|   368786 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   368788 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   361012 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   180505 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   368788 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   368788 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   368788 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   368788 |   840 | `	return pObj;` |
|   184395 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2142388 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2142390 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2142390 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071194 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2142390 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2142390 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2142390 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2142390 |   863 | `	return pObj;` |
|  1071196 |   864 |  |
|        - |   865 | `/* Forward declaration */` |
|        - |   866 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   867 | `/* Forward declarations for Fiber C functions */` |
|        - |   868 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   869 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   870 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   871 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   872 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   873 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   874 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   875 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   876 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   879 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   880 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   881 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   882 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   883 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   884 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   885 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   886 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   887 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   888 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   889 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   890 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   891 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   892 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   893 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   894 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   895 | `/*` |
|        - |   896 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   897 | ` * directly as foreign functions.` |
|        - |   898 | ` */` |
|        - |   899 | `#define PH7_BUILTIN_LIB \` |
|        - |   900 | `	"class Exception { "\` |
|        - |   901 | `    "protected $message = 'Unknown exception';"\` |
|        - |   902 | `    "protected $code = 0;"\` |
|        - |   903 | `    "protected $file;"\` |
|        - |   904 | `    "protected $line;"\` |
|        - |   905 | `    "protected $trace;"\` |
|        - |   906 | `    "protected $previous;"\` |
|        - |   907 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   908 | `	"   if( isset($message) ){"\` |
|        - |   909 | `	"	  $this->message = $message;"\` |
|        - |   910 | `	"   }"\` |
|        - |   911 | `	"   $this->code = $code;"\` |
|        - |   912 | `	"   $this->file = __FILE__;"\` |
|        - |   913 | `	"   $this->line = __LINE__;"\` |
|        - |   914 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   915 | `	"   if( isset($previous) ){"\` |
|        - |   916 | `	"     $this->previous = $previous;"\` |
|        - |   917 | `	"   }"\` |
|        - |   918 | `	"}"\` |
|        - |   919 | `	"public function getMessage(){"\` |
|        - |   920 | `	"   return $this->message;"\` |
|        - |   921 | `	"}"\` |
|        - |   922 | `	" public function getCode(){"\` |
|        - |   923 | `	"  return $this->code;"\` |
|        - |   924 | `	"}"\` |
|        - |   925 | `	"public function getFile(){"\` |
|        - |   926 | `	"  return $this->file;"\` |
|        - |   927 | `	"}"\` |
|        - |   928 | `	"public function getLine(){"\` |
|        - |   929 | `	"  return $this->line;"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"public function getTrace(){"\` |
|        - |   932 | `	"   return $this->trace;"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"public function getTraceAsString(){"\` |
|        - |   935 | `	"  return debug_string_backtrace();"\` |
|        - |   936 | `	"}"\` |
|        - |   937 | `	"public function getPrevious(){"\` |
|        - |   938 | `	"    return $this->previous;"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function __toString(){"\` |
|        - |   941 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   942 | `    "}"\` |
|        - |   943 | `	"}"\` |
|        - |   944 | `	"class Error extends Exception { }"\` |
|        - |   945 | `	"class TypeError extends Error { }"\` |
|        - |   946 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   947 | `	"class ValueError extends Error { }"\` |
|        - |   948 | `	"class FiberError extends Error { }"\` |
|        - |   949 | `	"class AssertionError extends Error { }"\` |
|        - |   950 | `	"class ArithmeticError extends Error { }"\` |
|        - |   951 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |   952 | `	"class ErrorException extends Exception { "\` |
|        - |   953 | `	"protected $severity;"\` |
|        - |   954 | `	"public function __construct(string $message = null,"\` |
|        - |   955 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   956 | `	"   if( isset($message) ){"\` |
|        - |   957 | `	"	  $this->message = $message;"\` |
|        - |   958 | `	"   }"\` |
|        - |   959 | `	"   $this->severity = $severity;"\` |
|        - |   960 | `	"   $this->code = $code;"\` |
|        - |   961 | `	"   $this->file = $filename;"\` |
|        - |   962 | `	"   $this->line = $lineno;"\` |
|        - |   963 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   964 | `	"   if( isset($previous) ){"\` |
|        - |   965 | `	"     $this->previous = $previous;"\` |
|        - |   966 | `	"   }"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"public function getSeverity(){"\` |
|        - |   969 | `	"   return $this->severity;"\` |
|        - |   970 | `    "}"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	"interface Iterator {"\` |
|        - |   973 | `	"public function current();"\` |
|        - |   974 | `	"public function key();"\` |
|        - |   975 | `	"public function next();"\` |
|        - |   976 | `	"public function rewind();"\` |
|        - |   977 | `	"public function valid();"\` |
|        - |   978 | `	"}"\` |
|        - |   979 | `	"interface IteratorAggregate {"\` |
|        - |   980 | `	"public function getIterator();"\` |
|        - |   981 | `	"}"\` |
|        - |   982 | `	"interface Serializable {"\` |
|        - |   983 | `	"public function serialize();"\` |
|        - |   984 | `	"public function unserialize(string $serialized);"\` |
|        - |   985 | `	"}"\` |
|        - |   986 | `	"/* Directory releated IO */"\` |
|        - |   987 | `	"class Directory {"\` |
|        - |   988 | `	"public $handle = null;"\` |
|        - |   989 | `	"public $path  = null;"\` |
|        - |   990 | `	"public function __construct(string $path)"\` |
|        - |   991 | `	"{"\` |
|        - |   992 | `	"   $this->handle = opendir($path);"\` |
|        - |   993 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   994 | `	"      $this->path = $path;"\` |
|        - |   995 | `	"   }"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"public function __destruct()"\` |
|        - |   998 | `	"{"\` |
|        - |   999 | `	"  if( $this->handle != null ){"\` |
|        - |  1000 | `	"       closedir($this->handle);"\` |
|        - |  1001 | `	"  }"\` |
|        - |  1002 | `	"}"\` |
|        - |  1003 | `	"public function read()"\` |
|        - |  1004 | `	"{"\` |
|        - |  1005 | `	"    return readdir($this->handle);"\` |
|        - |  1006 | `	"}"\` |
|        - |  1007 | `	"public function rewind()"\` |
|        - |  1008 | `	"{"\` |
|        - |  1009 | `	"    rewinddir($this->handle);"\` |
|        - |  1010 | `	"}"\` |
|        - |  1011 | `	"public function close()"\` |
|        - |  1012 | `	"{"\` |
|        - |  1013 | `	"    closedir($this->handle);"\` |
|        - |  1014 | `	"    $this->handle = null;"\` |
|        - |  1015 | `	"}"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"class Fiber {"\` |
|        - |  1018 | `	"  private $__ctx;"\` |
|        - |  1019 | `	"  private $__callable;"\` |
|        - |  1020 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1021 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1022 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1023 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1024 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1025 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1026 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1027 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1028 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1029 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"class Generator implements Iterator {"\` |
|        - |  1032 | `	"  private $__ctx;"\` |
|        - |  1033 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1034 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1035 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1036 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1037 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1038 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1039 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1040 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1041 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"class stdClass{"\` |
|        - |  1044 | `	"  public $value;"\` |
|        - |  1045 | `	" /* Magic methods */"\` |
|        - |  1046 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1047 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1048 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1049 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1050 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1051 | `	"}"\` |
|        - |  1052 | `	"function dir(string $path){"\` |
|        - |  1053 | `	"   return new Directory($path);"\` |
|        - |  1054 | `	"}"\` |
|        - |  1055 | `	"function Dir(string $path){"\` |
|        - |  1056 | `	"   return new Directory($path);"\` |
|        - |  1057 | `	"}"\` |
|        - |  1058 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1059 | `    "{"\` |
|        - |  1060 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1061 | `	"  $aDir = array();"\` |
|        - |  1062 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1063 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1064 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1065 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1066 | `	"   }"\` |
|        - |  1067 | `	"  closedir($pHandle);"\` |
|        - |  1068 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1069 | `	"      rsort($aDir);"\` |
|        - |  1070 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1071 | `	"      sort($aDir);"\` |
|        - |  1072 | `	"  }"\` |
|        - |  1073 | `	"  return $aDir;"\` |
|        - |  1074 | `	"}"\` |
|        - |  1075 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1076 | `	"/* Open the target directory */"\` |
|        - |  1077 | `	"$zDir = dirname($pattern);"\` |
|        - |  1078 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1079 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1080 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1081 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1082 | `	"	return FALSE;"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"$pattern = basename($pattern);"\` |
|        - |  1085 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1086 | `	"/* Loop throw available entries */"\` |
|        - |  1087 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1088 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1089 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1090 | `	"	if( $rc ){"\` |
|        - |  1091 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1092 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1093 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1094 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1095 | `	"		  }"\` |
|        - |  1096 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1097 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1098 | `	"		 continue;"\` |
|        - |  1099 | `	"	   }"\` |
|        - |  1100 | `	"	   /* Add the entry */"\` |
|        - |  1101 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1102 | `	"	}"\` |
|        - |  1103 | `	" }"\` |
|        - |  1104 | `	"/* Close the handle */"\` |
|        - |  1105 | `	"closedir($pHandle);"\` |
|        - |  1106 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1107 | `	"  /* Sort the array */"\` |
|        - |  1108 | `	"  sort($pArray);"\` |
|        - |  1109 | `	"}"\` |
|        - |  1110 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1111 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1112 | `	"  $pArray[] = $pattern;"\` |
|        - |  1113 | `	"}"\` |
|        - |  1114 | `	"/* Return the created array */"\` |
|        - |  1115 | `	"return $pArray;"\` |
|        - |  1116 | `   "}"\` |
|        - |  1117 | `   "/* Creates a temporary file */"\` |
|        - |  1118 | `   "function tmpfile(){"\` |
|        - |  1119 | `   "  /* Extract the temp directory */"\` |
|        - |  1120 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1121 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1122 | `   "    /* Use the current dir */"\` |
|        - |  1123 | `   "    $zTempDir = '.';"\` |
|        - |  1124 | `   "  }"\` |
|        - |  1125 | `   "  /* Create the file */"\` |
|        - |  1126 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1127 | `   "  return $pHandle;"\` |
|        - |  1128 | `   "}"\` |
|        - |  1129 | `   "/* Creates a temporary filename */"\` |
|        - |  1130 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1131 | `   "{"\` |
|        - |  1132 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1133 | `   "}"\` |
|        - |  1134 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1135 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1136 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1137 | `   "/* Copy arguments */"\` |
|        - |  1138 | `   "$nArgs = func_num_args();"\` |
|        - |  1139 | `   "$pNew = array();"\` |
|        - |  1140 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1141 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1142 | `    "}"\` |
|        - |  1143 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1144 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1145 | `	"/* Erase */"\` |
|        - |  1146 | `	"array_erase($pArray);"\` |
|        - |  1147 | `	"/* Unshift */"\` |
|        - |  1148 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1149 | `	"return sizeof($pArray);"\` |
|        - |  1150 | `    "}"\` |
|        - |  1151 | `	"function array_merge_recursive(){"\` |
|        - |  1152 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1153 | `    "$arrays = func_get_args();"\` |
|        - |  1154 | `    "$narrays = count($arrays);"\` |
|        - |  1155 | `    "$ret = array();"\` |
|        - |  1156 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1157 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1158 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1159 | `	 " }"\` |
|        - |  1160 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1161 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1162 | `     "  if( $keyIsInt ) {"\` |
|        - |  1163 | `     "   $ret[] = $value;"\` |
|        - |  1164 | `     "  } else {"\` |
|        - |  1165 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1166 | `     "    $cur = $ret[$key];"\` |
|        - |  1167 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1168 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1169 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1171 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1172 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1173 | `     "    } else {"\` |
|        - |  1174 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1175 | `     "    }"\` |
|        - |  1176 | `     "   } else {"\` |
|        - |  1177 | `     "    $ret[$key] = $value;"\` |
|        - |  1178 | `     "   }"\` |
|        - |  1179 | `     "  }"\` |
|        - |  1180 | `     " }"\` |
|        - |  1181 | `	 " }"\` |
|        - |  1182 | `	 " return $ret;"\` |
|        - |  1183 | `    "}"\` |
|        - |  1184 | `	"function max(){"\` |
|        - |  1185 | `    "  $pArgs = func_get_args();"\` |
|        - |  1186 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1187 | `	"  return null;"\` |
|        - |  1188 | `    " }"\` |
|        - |  1189 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1190 | `    " $pArg = $pArgs[0];"\` |
|        - |  1191 | `	" if( !is_array($pArg) ){"\` |
|        - |  1192 | `	"   return $pArg; "\` |
|        - |  1193 | `	" }"\` |
|        - |  1194 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1195 | `	"   return null;"\` |
|        - |  1196 | `	" }"\` |
|        - |  1197 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1198 | `	" reset($pArg);"\` |
|        - |  1199 | `	" $max = current($pArg);"\` |
|        - |  1200 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1201 | `	"   if( $val > $max ){"\` |
|        - |  1202 | `	"     $max = $val;"\` |
|        - |  1203 | `    " }"\` |
|        - |  1204 | `	" }"\` |
|        - |  1205 | `	" return $max;"\` |
|        - |  1206 | `    " }"\` |
|        - |  1207 | `    " $max = $pArgs[0];"\` |
|        - |  1208 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1209 | `    " $val = $pArgs[$i];"\` |
|        - |  1210 | `	"if( $val > $max ){"\` |
|        - |  1211 | `	" $max = $val;"\` |
|        - |  1212 | `	"}"\` |
|        - |  1213 | `    " }"\` |
|        - |  1214 | `	" return $max;"\` |
|        - |  1215 | `    "}"\` |
|        - |  1216 | `	"function min(){"\` |
|        - |  1217 | `    "  $pArgs = func_get_args();"\` |
|        - |  1218 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1219 | `	"  return null;"\` |
|        - |  1220 | `    " }"\` |
|        - |  1221 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1222 | `    " $pArg = $pArgs[0];"\` |
|        - |  1223 | `	" if( !is_array($pArg) ){"\` |
|        - |  1224 | `	"   return $pArg; "\` |
|        - |  1225 | `	" }"\` |
|        - |  1226 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1227 | `	"   return null;"\` |
|        - |  1228 | `	" }"\` |
|        - |  1229 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1230 | `	" reset($pArg);"\` |
|        - |  1231 | `	" $min = current($pArg);"\` |
|        - |  1232 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1233 | `	"   if( $val < $min ){"\` |
|        - |  1234 | `	"     $min = $val;"\` |
|        - |  1235 | `    " }"\` |
|        - |  1236 | `	" }"\` |
|        - |  1237 | `	" return $min;"\` |
|        - |  1238 | `    " }"\` |
|        - |  1239 | `    " $min = $pArgs[0];"\` |
|        - |  1240 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1241 | `    " $val = $pArgs[$i];"\` |
|        - |  1242 | `	"if( $val < $min ){"\` |
|        - |  1243 | `	" $min = $val;"\` |
|        - |  1244 | `	" }"\` |
|        - |  1245 | `    " }"\` |
|        - |  1246 | `	" return $min;"\` |
|        - |  1247 | `	"}"\` |
|        - |  1248 | `	"function fileowner(string $file){"\` |
|        - |  1249 | `    " $a = stat($file);"\` |
|        - |  1250 | `	" if( !is_array($a) ){"\` |
|        - |  1251 | `	"	return false;"\` |
|        - |  1252 | `	" }"\` |
|        - |  1253 | `	" return $a['uid'];"\` |
|        - |  1254 | `    "}"\` |
|        - |  1255 | `    "function filegroup(string $file){"\` |
|        - |  1256 | `	" $a = stat($file);"\` |
|        - |  1257 | `	" if( !is_array($a) ){"\` |
|        - |  1258 | `	"	return false;"\` |
|        - |  1259 | `	" }"\` |
|        - |  1260 | `	" return $a['gid'];"\` |
|        - |  1261 | `    "}"\` |
|        - |  1262 | `	 "function fileinode(string $file){"\` |
|        - |  1263 | `	" $a = stat($file);"\` |
|        - |  1264 | `	" if( !is_array($a) ){"\` |
|        - |  1265 | `	"	return false;"\` |
|        - |  1266 | `	" }"\` |
|        - |  1267 | `	" return $a['ino'];"\` |
|        - |  1268 | `    "}"` |
|        - |  1269 |  |
|        - |  1270 | `/*` |
|        - |  1271 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1272 | ` * start compiling the target PHP program.` |
|        - |  1273 | ` */` |
|     2592 |  1274 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1275 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1276 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1277 | `	 )` |
|        2 |  1278 |  |
|        - |  1279 | `	SyString sBuiltin;` |
|        - |  1280 | `	ph7_value *pObj;` |
|        - |  1281 | `	sxi32 rc;` |
|        - |  1282 | `	/* Zero the structure */` |
|     2594 |  1283 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1284 | `	/* Initialize VM fields */` |
|     2594 |  1285 | `	pVm->pEngine = &(*pEngine);` |
|     2594 |  1286 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1287 | `	/* Instructions containers */` |
|     2594 |  1288 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2594 |  1289 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2594 |  1290 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1291 | `	/* Object containers */` |
|     2594 |  1292 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2594 |  1293 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1294 | `	/* Virtual machine internal containers */` |
|     2594 |  1295 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2594 |  1296 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2594 |  1297 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2594 |  1298 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2594 |  1299 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2594 |  1300 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2594 |  1301 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2594 |  1302 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2594 |  1303 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2594 |  1304 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2594 |  1305 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2594 |  1306 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2594 |  1307 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2594 |  1308 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2594 |  1309 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2594 |  1310 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2594 |  1311 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2594 |  1312 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2594 |  1313 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2594 |  1314 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2594 |  1315 | `	pVm->pPendingException = 0;` |
|        - |  1316 | `	/* Configuration containers */` |
|     2594 |  1317 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2594 |  1318 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2594 |  1319 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2594 |  1320 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2594 |  1321 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2594 |  1322 | `	pVm->iResponseStatus = 200;` |
|     2594 |  1323 | `	pVm->bHeadersSent = 0;` |
|     2594 |  1324 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1325 | `	/* Error callbacks containers */` |
|     2594 |  1326 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2594 |  1327 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2594 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2594 |  1329 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2594 |  1330 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1331 | `	/* Set a default recursion limit */` |
|        - |  1332 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2594 |  1333 | `	pVm->nMaxDepth = 32;` |
|        - |  1334 | `#else` |
|        - |  1335 | `	pVm->nMaxDepth = 16;` |
|        - |  1336 | `#endif` |
|        - |  1337 | `	/* Default assertion flags */` |
|     2594 |  1338 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1339 | `	/* JSON return status */` |
|     2594 |  1340 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1341 | `	/* PRNG context */` |
|     2594 |  1342 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1343 | `	/* Install the null constant */` |
|     2594 |  1344 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2594 |  1345 | `	if( pObj == 0 ){` |
|      ! 0 |  1346 | `		rc = SXERR_MEM;` |
|      ! 0 |  1347 | `		goto Err;` |
|        - |  1348 | `	}` |
|     2594 |  1349 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1350 | `	/* Install the boolean TRUE constant */` |
|     2594 |  1351 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2594 |  1352 | `	if( pObj == 0 ){` |
|      ! 0 |  1353 | `		rc = SXERR_MEM;` |
|      ! 0 |  1354 | `		goto Err;` |
|        - |  1355 | `	}` |
|     2594 |  1356 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1357 | `	/* Install the boolean FALSE constant */` |
|     2594 |  1358 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2594 |  1359 | `	if( pObj == 0 ){` |
|      ! 0 |  1360 | `		rc = SXERR_MEM;` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|     2594 |  1363 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1364 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1365 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1366 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2594 |  1367 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2594 |  1368 | `	if( pObj == 0 ){` |
|      ! 0 |  1369 | `		rc = SXERR_MEM;` |
|      ! 0 |  1370 | `		goto Err;` |
|        - |  1371 | `	}` |
|     2594 |  1372 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1373 | `	/* Create the global frame */` |
|     2594 |  1374 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2594 |  1375 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1376 | `		goto Err;` |
|        - |  1377 | `	}` |
|        - |  1378 | `	/* Initialize the code generator */` |
|     2594 |  1379 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2594 |  1380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1381 | `		goto Err;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* VM correctly initialized,set the magic number */` |
|     2594 |  1384 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2594 |  1385 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1386 | `	/* Compile the built-in library */` |
|     2594 |  1387 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1388 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2594 |  1389 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1390 | `	/* Register Fiber internal C functions */` |
|     2594 |  1391 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2594 |  1392 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2594 |  1393 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2594 |  1394 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2594 |  1395 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2594 |  1396 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2594 |  1397 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2594 |  1398 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2594 |  1399 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2594 |  1400 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1401 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2594 |  1402 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2594 |  1403 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2594 |  1404 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2594 |  1405 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2594 |  1406 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2594 |  1407 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2594 |  1408 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2594 |  1409 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2594 |  1410 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2594 |  1411 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1412 | `	/* Reset the code generator */` |
|     2594 |  1413 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2594 |  1414 | `	return SXRET_OK;` |
|      ! 0 |  1415 | `Err:` |
|      ! 0 |  1416 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1417 | `	return rc;` |
|     1298 |  1418 |  |
|        - |  1419 | `/*` |
|        - |  1420 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1421 | ` * routine which store the output in an internal blob.` |
|        - |  1422 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1423 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1424 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1425 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1426 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1427 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1428 | ` * to finish executing and extracting the output.` |
|        - |  1429 | ` */` |
|       38 |  1430 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1431 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1432 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1433 | `	void *pUserData     /* User private data */` |
|        - |  1434 | `	)` |
|      ! 0 |  1435 |  |
|        - |  1436 | `	 sxi32 rc;` |
|        - |  1437 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1438 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1439 | `	 return rc;` |
|      ! 0 |  1440 |  |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1443 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1444 | ` */` |
|    14176 |  1445 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1446 |  |
|    14178 |  1447 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14178 |  1448 | `	if( xCons != VmObConsumer ){` |
|     6302 |  1449 | `		pVm->nOutputLen += nLen;` |
|     6302 |  1450 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      828 |  1451 | `			pVm->bHeadersSent = 1;` |
|      413 |  1452 | `		}` |
|     3150 |  1453 | `	}` |
|    14178 |  1454 |  |
|        - |  1455 | `#define VM_STACK_GUARD 16` |
|        - |  1456 | `/*` |
|        - |  1457 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1458 | ` * our compiled PHP program.` |
|        - |  1459 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1460 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1461 | ` */` |
|    33120 |  1462 | `static ph7_value * VmNewOperandStack(` |
|        - |  1463 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1464 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1465 | `	)` |
|        2 |  1466 |  |
|        - |  1467 | `	ph7_value *pStack;` |
|        - |  1468 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1469 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1470 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1471 | `  ** on the maximum stack depth required.` |
|        - |  1472 | `  **` |
|        - |  1473 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1474 | `  */` |
|    33122 |  1475 | `	nInstr += VM_STACK_GUARD;` |
|    33122 |  1476 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    33122 |  1477 | `	if( pStack == 0 ){` |
|      ! 0 |  1478 | `		return 0;` |
|        - |  1479 | `	}` |
|        - |  1480 | `	/* Initialize the operand stack */` |
|  2072246 |  1481 | `	while( nInstr > 0 ){` |
|  2039126 |  1482 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2039126 |  1483 | `		--nInstr;` |
|        2 |  1484 | `	}` |
|        - |  1485 | `	/* Ready for bytecode execution */` |
|    33122 |  1486 | `	return pStack;` |
|    16562 |  1487 |  |
|        - |  1488 | `/* Forward declaration */` |
|        - |  1489 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1490 | `/*` |
|        - |  1491 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1492 | ` * This routine gets called by the PH7 engine after` |
|        - |  1493 | ` * successful compilation of the target PHP program.` |
|        - |  1494 | ` */` |
|     2332 |  1495 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1496 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1497 | `	)` |
|        2 |  1498 |  |
|        - |  1499 | `	SyHashEntry *pEntry;` |
|        - |  1500 | `	sxi32 rc;` |
|     2334 |  1501 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1502 | `		/* Initialize your VM first */` |
|      ! 0 |  1503 | `		return SXERR_CORRUPT;` |
|        - |  1504 | `	}` |
|        - |  1505 | `	/* Mark the VM ready for byte-code execution */` |
|     2334 |  1506 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1507 | `	/* Release the code generator now we have compiled our program */` |
|     2334 |  1508 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1509 | `	/* Emit the DONE instruction */` |
|     2334 |  1510 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2334 |  1511 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1512 | `		return SXERR_MEM;` |
|        - |  1513 | `	}` |
|        - |  1514 | `	/* Script return value */` |
|     2334 |  1515 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1516 | `	/* Allocate a new operand stack */` |
|     2334 |  1517 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2334 |  1518 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1519 | `		return SXERR_MEM;` |
|        - |  1520 | `	}` |
|        - |  1521 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1522 | `	 * private data. */` |
|     2334 |  1523 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2334 |  1524 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1525 | `	/* Allocate the reference table */` |
|     2334 |  1526 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2334 |  1527 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2334 |  1528 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1529 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1530 | `		return SXERR_MEM;` |
|        - |  1531 | `	}` |
|        - |  1532 | `	/* Zero the reference table */` |
|     2334 |  1533 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1534 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2334 |  1535 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2334 |  1536 | `	if( rc != SXRET_OK ){` |
|        - |  1537 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1538 | `		return rc;` |
|        - |  1539 | `	}` |
|        - |  1540 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2334 |  1541 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2334 |  1542 | `	if( rc != SXRET_OK ){` |
|        - |  1543 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1544 | `		return rc;` |
|        - |  1545 | `	}` |
|        - |  1546 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2334 |  1547 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1548 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2334 |  1549 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1550 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2334 |  1551 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1552 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1553 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2334 |  1554 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2334 |  1555 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1556 | `#endif` |
|        - |  1557 | `	/* Initialize and install static and constants class attributes */` |
|     2334 |  1558 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    42154 |  1559 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    39822 |  1560 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    39822 |  1561 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1562 | `			return rc;` |
|        - |  1563 | `		}` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2334 |  1566 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1567 | `	/* VM is ready for bytecode execution */` |
|     2334 |  1568 | `	return SXRET_OK;` |
|     1168 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1572 | ` */` |
|      ! 0 |  1573 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1574 |  |
|      ! 0 |  1575 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1576 | `		return SXERR_CORRUPT;` |
|        - |  1577 | `	}` |
|        - |  1578 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1579 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1580 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1581 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1582 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1583 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1584 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1585 | `	pVm->bHttpContext = 0;` |
|        - |  1586 | `	/* Set the ready flag */` |
|      ! 0 |  1587 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1588 | `	return SXRET_OK;` |
|      ! 0 |  1589 |  |
|        - |  1590 | `/*` |
|        - |  1591 | ` * Release a Virtual Machine.` |
|        - |  1592 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1593 | ` */` |
|     2324 |  1594 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1595 |  |
|        - |  1596 | `	/* Set the stale magic number */` |
|     2326 |  1597 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1598 | `	/* Release the private memory subsystem */` |
|     2326 |  1599 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2326 |  1600 | `	return SXRET_OK;` |
|        2 |  1601 |  |
|        - |  1602 | `/*` |
|        - |  1603 | ` * Initialize a foreign function call context.` |
|        - |  1604 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1605 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1606 | ` * functions.` |
|        - |  1607 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1608 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1609 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1610 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1611 | ` */` |
|   583954 |  1612 | `static sxi32 VmInitCallContext(` |
|        - |  1613 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1614 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1615 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1616 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1617 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1618 | `	)` |
|        2 |  1619 |  |
|   583956 |  1620 | `	pOut->pFunc = pFunc;` |
|   583956 |  1621 | `	pOut->pVm   = pVm;` |
|   583956 |  1622 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   583956 |  1623 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1624 | `	/* Assume a null return value */` |
|   583956 |  1625 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   583956 |  1626 | `	pOut->pRet = pRet;` |
|   583956 |  1627 | `	pOut->iFlags = iFlags;` |
|   583956 |  1628 | `	return SXRET_OK;` |
|        2 |  1629 |  |
|        - |  1630 | `/*` |
|        - |  1631 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1632 | ` * left behind.` |
|        - |  1633 | ` */` |
|   583954 |  1634 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1635 |  |
|        - |  1636 | `	sxu32 n;` |
|   583956 |  1637 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7106 |  1638 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20284 |  1639 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13180 |  1640 | `			if( apObj[n] == 0 ){` |
|        - |  1641 | `				/* Already released */` |
|      298 |  1642 | `				continue;` |
|        - |  1643 | `			}` |
|    12884 |  1644 | `			PH7_MemObjRelease(apObj[n]);` |
|    12884 |  1645 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6443 |  1646 | `		}` |
|     7106 |  1647 | `		SySetRelease(&pCtx->sVar);` |
|     3552 |  1648 | `	}` |
|   583956 |  1649 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1650 | `		ph7_aux_data *aAux;` |
|        - |  1651 | `		void *pChunk;` |
|        - |  1652 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1653 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1654 | `		 */` |
|        9 |  1655 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1656 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1657 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1658 | `			/* Release the chunk */` |
|       25 |  1659 | `			if( pChunk ){` |
|       25 |  1660 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1661 | `			}` |
|       13 |  1662 | `		}` |
|        9 |  1663 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1664 | `	}` |
|   583956 |  1665 |  |
|        - |  1666 | `/*` |
|        - |  1667 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1668 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1669 | ` */` |
|      296 |  1670 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1671 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1672 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1673 | `	)` |
|        2 |  1674 |  |
|      298 |  1675 | `	if( pValue == 0 ){` |
|        - |  1676 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1677 | `		return;` |
|        - |  1678 | `	}` |
|      298 |  1679 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1680 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1681 | `		sxu32 n;` |
|     1054 |  1682 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1683 | `			if( apObj[n] == pValue ){` |
|      298 |  1684 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1685 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1686 | `				/* Mark as released */` |
|      298 |  1687 | `				apObj[n] = 0;` |
|      298 |  1688 | `				break;` |
|        - |  1689 | `			}` |
|      380 |  1690 | `		}` |
|      148 |  1691 | `	}` |
|      150 |  1692 |  |
|        - |  1693 | `/*` |
|        - |  1694 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1695 | ` */` |
|  3375064 |  1696 | `static void VmPopOperand(` |
|        - |  1697 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1698 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1699 | `	)` |
|        2 |  1700 |  |
|  3375066 |  1701 | `	ph7_value *pTos = *ppTos;` |
|  7175566 |  1702 | `	while( nPop > 0 ){` |
|  3800502 |  1703 | `		PH7_MemObjRelease(pTos);` |
|  3800502 |  1704 | `		pTos--;` |
|  3800502 |  1705 | `		nPop--;` |
|        2 |  1706 | `	}` |
|        - |  1707 | `	/* Top of the stack */` |
|  3375066 |  1708 | `	*ppTos = pTos;` |
|  3375066 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Reserve a memory object.` |
|        - |  1712 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1713 | ` */` |
|  3069542 |  1714 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1715 |  |
|  3069544 |  1716 | `	ph7_value *pObj = 0;` |
|        - |  1717 | `	VmSlot *pSlot;` |
|        - |  1718 | `	sxu32 nIdx;` |
|        - |  1719 | `	/* Check for a free slot */` |
|  3069544 |  1720 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3069544 |  1721 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3069544 |  1722 | `	if( pSlot ){` |
|   927156 |  1723 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   927156 |  1724 | `		nIdx = pSlot->nIdx;` |
|   463577 |  1725 | `	}` |
|  3069544 |  1726 | `	if( pObj == 0 ){` |
|        - |  1727 | `		/* Reserve a new memory object */` |
|  2142390 |  1728 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2142390 |  1729 | `		if( pObj == 0 ){` |
|      ! 0 |  1730 | `			return 0;` |
|        - |  1731 | `		}` |
|  1071194 |  1732 | `	}` |
|        - |  1733 | `	/* Set a null default value */` |
|  3069544 |  1734 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3069544 |  1735 | `	pObj->nIdx = nIdx;` |
|  3069544 |  1736 | `	return pObj;` |
|  1534773 |  1737 |  |
|        - |  1738 | `/*` |
|        - |  1739 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1740 | ` */` |
|    30172 |  1741 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1742 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1743 | `	const char *zKey,  /* Entry key */` |
|        - |  1744 | `	sxu32 nByte,       /* Key length */` |
|        - |  1745 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1746 | `	)` |
|        2 |  1747 |  |
|        - |  1748 | `	ph7_value sKey;` |
|        - |  1749 | `	sxi32 rc;` |
|    30174 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30174 |  1751 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1752 | `	/* Perform the insertion */` |
|    30174 |  1753 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30174 |  1754 | `	PH7_MemObjRelease(&sKey);` |
|    30174 |  1755 | `	return rc;` |
|        2 |  1756 |  |
|        - |  1757 | `/*` |
|        - |  1758 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1759 | ` * Return a pointer to the variable value on success.` |
|        - |  1760 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1761 | ` */` |
|  3143748 |  1762 | `static ph7_value * VmExtractMemObj(` |
|        - |  1763 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1764 | `	const SyString *pName, /* Variable name */` |
|        - |  1765 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1766 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1767 | `	)` |
|        2 |  1768 |  |
|  3143750 |  1769 | `	int bNullify = FALSE;` |
|        - |  1770 | `	SyHashEntry *pEntry;` |
|        - |  1771 | `	VmFrame *pFrame;` |
|        - |  1772 | `	ph7_value *pObj;` |
|        - |  1773 | `	sxu32 nIdx;` |
|        - |  1774 | `	sxi32 rc;` |
|        - |  1775 | `	/* Point to the top active frame */` |
|  3143750 |  1776 | `	pFrame = pVm->pFrame;` |
|  3143750 |  1777 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1778 | `	/* Perform the lookup */` |
|  3143750 |  1779 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1780 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1781 | `		pName = &sAnnon;` |
|        - |  1782 | `		/* Always nullify the object */` |
|      ! 0 |  1783 | `		bNullify = TRUE;` |
|      ! 0 |  1784 | `		bDup = FALSE;` |
|      ! 0 |  1785 | `	}` |
|        - |  1786 | `	/* Check the superglobals table first */` |
|  3143750 |  1787 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3143750 |  1788 | `	if( pEntry == 0 ){` |
|        - |  1789 | `		/* Query the top active frame */` |
|  3143710 |  1790 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3143710 |  1791 | `		if( pEntry == 0 ){` |
|    86886 |  1792 | `			char *zName = (char *)pName->zString;` |
|        - |  1793 | `			VmSlot sLocal;` |
|    86886 |  1794 | `			if( !bCreate ){` |
|        - |  1795 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1796 | `				return 0;` |
|        - |  1797 | `			}` |
|        - |  1798 | `			/* No such variable,automatically create a new one and install` |
|        - |  1799 | `			 * it in the current frame.` |
|        - |  1800 | `			 */` |
|    86850 |  1801 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86850 |  1802 | `			if( pObj == 0 ){` |
|      ! 0 |  1803 | `				return 0;` |
|        - |  1804 | `			}` |
|    86850 |  1805 | `			nIdx = pObj->nIdx;` |
|    86850 |  1806 | `			if( bDup ){` |
|        - |  1807 | `				/* Duplicate name */` |
|      168 |  1808 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1809 | `				if( zName == 0 ){` |
|      ! 0 |  1810 | `					return 0;` |
|        - |  1811 | `				}` |
|       83 |  1812 | `			}` |
|        - |  1813 | `			/* Link to the top active VM frame */` |
|    86850 |  1814 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86850 |  1815 | `			if( rc != SXRET_OK ){` |
|        - |  1816 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1817 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1818 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1819 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1820 | `				return 0;` |
|        - |  1821 | `			}` |
|    86850 |  1822 | `			if( pFrame->pParent != 0 ){` |
|        - |  1823 | `				/* Local variable */` |
|    80024 |  1824 | `				sLocal.nIdx = nIdx;` |
|    80024 |  1825 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    40013 |  1826 | `			}else{` |
|        - |  1827 | `				/* Register in the $GLOBALS array */` |
|     6828 |  1828 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1829 | `			}` |
|        - |  1830 | `			/* Install in the reference table */` |
|    86850 |  1831 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1832 | `			/* Save object index */` |
|    86850 |  1833 | `			pObj->nIdx = nIdx;` |
|    43426 |  1834 | `		}else{` |
|        - |  1835 | `			/* Extract variable contents */` |
|  3056826 |  1836 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3056826 |  1837 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3056826 |  1838 | `			if( bNullify && pObj ){` |
|      ! 0 |  1839 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1840 | `			}` |
|        - |  1841 | `		}` |
|  1571948 |  1842 | `	}else{` |
|        - |  1843 | `		/* Superglobal */` |
|       42 |  1844 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1845 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1846 | `	}` |
|  3143714 |  1847 | `	return pObj;` |
|  1571986 |  1848 |  |
|        - |  1849 | `/*` |
|        - |  1850 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1851 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1852 | ` */` |
|     2636 |  1853 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1854 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1855 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1856 | `	sxu32 nByte        /* zName length */` |
|        - |  1857 | `	)` |
|        2 |  1858 |  |
|        - |  1859 | `	SyHashEntry *pEntry;` |
|        - |  1860 | `	ph7_value *pValue;` |
|        - |  1861 | `	sxu32 nIdx;` |
|        - |  1862 | `	/* Query the superglobal table */` |
|     2638 |  1863 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2638 |  1864 | `	if( pEntry == 0 ){` |
|        - |  1865 | `		/* No such entry */` |
|      ! 0 |  1866 | `		return 0;` |
|        - |  1867 | `	}` |
|        - |  1868 | `	/* Extract the superglobal index in the global object pool */` |
|     2638 |  1869 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1870 | `	/* Extract the variable value  */` |
|     2638 |  1871 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2638 |  1872 | `	return pValue;` |
|     1320 |  1873 |  |
|        - |  1874 | `/*` |
|        - |  1875 | ` * Perform a raw hashmap insertion.` |
|        - |  1876 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1877 | ` */` |
|     2666 |  1878 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1879 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1880 | `	const char *zKey,   /* Entry key */` |
|        - |  1881 | `	int nKeylen,        /* zKey length*/` |
|        - |  1882 | `	const char *zData,  /* Entry data */` |
|        - |  1883 | `	int nLen            /* zData length */` |
|        - |  1884 | `	)` |
|        2 |  1885 |  |
|        - |  1886 | `	ph7_value sKey,sValue;` |
|        - |  1887 | `	sxi32 rc;` |
|     2668 |  1888 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2668 |  1889 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2668 |  1890 | `	if( zKey ){` |
|     2646 |  1891 | `		if( nKeylen < 0 ){` |
|     2594 |  1892 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1296 |  1893 | `		}` |
|     2646 |  1894 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1322 |  1895 | `	}` |
|     2668 |  1896 | `	if( zData ){` |
|     2668 |  1897 | `		if( nLen < 0 ){` |
|        - |  1898 | `			/* Compute length automatically */` |
|      144 |  1899 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1900 | `		}` |
|     2668 |  1901 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1333 |  1902 | `	}` |
|        - |  1903 | `	/* Perform the insertion */` |
|     2668 |  1904 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2668 |  1905 | `	PH7_MemObjRelease(&sKey);` |
|     2668 |  1906 | `	PH7_MemObjRelease(&sValue);` |
|     2668 |  1907 | `	return rc;` |
|        2 |  1908 |  |
|        - |  1909 | `/*` |
|        - |  1910 | ` * Configure a working virtual machine instance.` |
|        - |  1911 | ` *` |
|        - |  1912 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1913 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1914 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1915 | ` * The second argument to this function is an integer configuration option` |
|        - |  1916 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1917 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1918 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1919 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1920 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1921 | ` */` |
|    37642 |  1922 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1923 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1924 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1925 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1926 | `	)` |
|        2 |  1927 |  |
|    37644 |  1928 | `	sxi32 rc = SXRET_OK;` |
|    37644 |  1929 | `	switch(nOp){` |
|     1158 |  1930 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2318 |  1931 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2318 |  1932 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1933 | `		/* VM output consumer callback */` |
|        - |  1934 | `#ifdef UNTRUST` |
|        - |  1935 | `		if( xConsumer == 0 ){` |
|        - |  1936 | `			rc = SXERR_CORRUPT;` |
|        - |  1937 | `			break;` |
|        - |  1938 | `		}` |
|        - |  1939 | `#endif` |
|        - |  1940 | `		/* Install the output consumer */` |
|     2318 |  1941 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2318 |  1942 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2318 |  1943 | `		break;` |
|        - |  1944 | `							   }` |
|     1166 |  1945 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1946 | `		/* Import path */` |
|        - |  1947 | `		  const char *zPath;` |
|        - |  1948 | `		  SyString sPath;` |
|     2334 |  1949 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1950 | `#if defined(UNTRUST)` |
|        - |  1951 | `		  if( zPath == 0 ){` |
|        - |  1952 | `			  rc = SXERR_EMPTY;` |
|        - |  1953 | `			  break;` |
|        - |  1954 | `		  }` |
|        - |  1955 | `#endif` |
|     2334 |  1956 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1957 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1958 | `#ifdef __WINNT__` |
|        2 |  1959 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1960 | `#endif` |
|     4666 |  1961 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1962 | `		  /* Remove leading and trailing white spaces */` |
|     2334 |  1963 | `		  SyStringFullTrim(&sPath);` |
|     2334 |  1964 | `		  if( sPath.nByte > 0 ){` |
|        - |  1965 | `			  /* Store the path in the corresponding conatiner */` |
|     2334 |  1966 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1166 |  1967 | `		  }` |
|     2334 |  1968 | `		  break;` |
|        - |  1969 | `									 }` |
|     1166 |  1970 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1971 | `		/* Run-Time Error report */` |
|     2334 |  1972 | `		pVm->bErrReport = 1;` |
|     2334 |  1973 | `		break;` |
|      ! 0 |  1974 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1975 | `		/* Recursion depth */` |
|      ! 0 |  1976 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1977 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1978 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1979 | `		}` |
|      ! 0 |  1980 | `		break;` |
|        - |  1981 | `									   }` |
|      ! 0 |  1982 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1983 | `		/* VM output length in bytes */` |
|      ! 0 |  1984 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1985 | `#ifdef UNTRUST` |
|        - |  1986 | `		if( pOut == 0 ){` |
|        - |  1987 | `			rc = SXERR_CORRUPT;` |
|        - |  1988 | `			break;` |
|        - |  1989 | `		}` |
|        - |  1990 | `#endif` |
|      ! 0 |  1991 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1992 | `		break;` |
|        - |  1993 | `							   }` |
|        - |  1994 |  |
|    11660 |  1995 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1996 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1997 | `		/* Create a new superglobal/global variable */` |
|    23322 |  1998 | `		const char *zName = va_arg(ap,const char *);` |
|    23322 |  1999 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2000 | `		SyHashEntry *pEntry;` |
|        - |  2001 | `		ph7_value *pObj;` |
|        - |  2002 | `		sxu32 nByte;` |
|        - |  2003 | `		sxu32 nIdx;` |
|        - |  2004 | `#ifdef UNTRUST` |
|        - |  2005 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2006 | `			rc = SXERR_CORRUPT;` |
|        - |  2007 | `			break;` |
|        - |  2008 | `		}` |
|        - |  2009 | `#endif` |
|    23322 |  2010 | `		nByte = SyStrlen(zName);` |
|    23322 |  2011 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2012 | `			/* Check if the superglobal is already installed */` |
|    23322 |  2013 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11662 |  2014 | `		}else{` |
|        - |  2015 | `			/* Query the top active VM frame */` |
|      ! 0 |  2016 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2017 | `		}` |
|    23322 |  2018 | `		if( pEntry ){` |
|        - |  2019 | `			/* Variable already installed */` |
|      ! 0 |  2020 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2021 | `			/* Extract contents */` |
|      ! 0 |  2022 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2023 | `			if( pObj ){` |
|        - |  2024 | `				/* Overwrite old contents */` |
|      ! 0 |  2025 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2026 | `			}` |
|      ! 0 |  2027 | `		}else{` |
|        - |  2028 | `			/* Install a new variable */` |
|    23322 |  2029 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23322 |  2030 | `			if( pObj == 0 ){` |
|      ! 0 |  2031 | `				rc = SXERR_MEM;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `			}` |
|    23322 |  2034 | `			nIdx = pObj->nIdx;` |
|        - |  2035 | `			/* Copy value */` |
|    23322 |  2036 | `			PH7_MemObjStore(pValue,pObj);` |
|    23322 |  2037 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2038 | `				/* Install the superglobal */` |
|    23322 |  2039 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11662 |  2040 | `			}else{` |
|        - |  2041 | `				/* Install in the current frame */` |
|      ! 0 |  2042 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2043 | `			}` |
|    23322 |  2044 | `			if( rc == SXRET_OK ){` |
|        - |  2045 | `				SyHashEntry *pRef;` |
|    23322 |  2046 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23322 |  2047 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11662 |  2048 | `				}else{` |
|      ! 0 |  2049 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2050 | `				}` |
|        - |  2051 | `				/* Install in the reference table */` |
|    23322 |  2052 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23322 |  2053 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2054 | `					/* Register in the $GLOBALS array */` |
|    23322 |  2055 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11660 |  2056 | `				}` |
|    11660 |  2057 | `			}` |
|        - |  2058 | `		}` |
|    23322 |  2059 | `		break;` |
|        - |  2060 | `									}` |
|     1296 |  2061 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2062 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2063 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2066 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2067 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2594 |  2068 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2594 |  2069 | `		const char *zValue = va_arg(ap,const char *);` |
|     2594 |  2070 | `		int nLen = va_arg(ap,int);` |
|        - |  2071 | `		ph7_hashmap *pMap;` |
|        - |  2072 | `		ph7_value *pValue;` |
|     2594 |  2073 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2074 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2593 |  2076 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2077 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2592 |  2079 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2080 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2081 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2592 |  2082 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2083 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2084 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2592 |  2085 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2086 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2087 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2592 |  2088 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2089 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2090 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2091 | `		}else{` |
|        - |  2092 | `			/* Extract the $_SERVER superglobal */` |
|     2592 |  2093 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2094 | `		}` |
|     2594 |  2095 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2096 | `			/* No such entry */` |
|      ! 0 |  2097 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2098 | `			break;` |
|        - |  2099 | `		}` |
|        - |  2100 | `		/* Point to the hashmap */` |
|     2594 |  2101 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2102 | `		/* Perform the insertion */` |
|     2594 |  2103 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2594 |  2104 | `		break;` |
|        - |  2105 | `								   }` |
|       11 |  2106 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2107 | `		/* Script arguments */` |
|       24 |  2108 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2109 | `		ph7_hashmap *pMap;` |
|        - |  2110 | `		ph7_value *pValue;` |
|        - |  2111 | `		sxu32 n;` |
|       24 |  2112 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2113 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2114 | `			break;` |
|        - |  2115 | `		}` |
|        - |  2116 | `		/* Extract the $argv array */` |
|       24 |  2117 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2118 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2119 | `			/* No such entry */` |
|      ! 0 |  2120 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2121 | `			break;` |
|        - |  2122 | `		}` |
|        - |  2123 | `		/* Point to the hashmap */` |
|       24 |  2124 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2125 | `		/* Perform the insertion */` |
|       24 |  2126 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2127 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2128 | `		if( rc == SXRET_OK ){` |
|       24 |  2129 | `			if( pMap->nEntry > 1 ){` |
|        - |  2130 | `				/* Append space separator first */` |
|       18 |  2131 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2132 | `			}` |
|       24 |  2133 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2134 | `		}` |
|       24 |  2135 | `		break;` |
|        - |  2136 | `								  }` |
|      ! 0 |  2137 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2138 | `		/* error_log() consumer */` |
|      ! 0 |  2139 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2140 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2141 | `		break;` |
|        - |  2142 | `										}` |
|      ! 0 |  2143 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2144 | `		/* Script return value */` |
|      ! 0 |  2145 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2146 | `#ifdef UNTRUST` |
|        - |  2147 | `		if( ppValue == 0 ){` |
|        - |  2148 | `			rc = SXERR_CORRUPT;` |
|        - |  2149 | `			break;` |
|        - |  2150 | `		}` |
|        - |  2151 | `#endif` |
|      ! 0 |  2152 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2153 | `		break;` |
|        - |  2154 | `								   }` |
|     2332 |  2155 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2156 | `		/* Register an IO stream device */` |
|     4666 |  2157 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2158 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6996 |  2159 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4666 |  2160 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2161 | `				/* Invalid stream */` |
|      ! 0 |  2162 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2163 | `				break;` |
|        - |  2164 | `		}` |
|     4666 |  2165 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2166 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2334 |  2167 | `			pVm->pDefStream = pStream;` |
|     1166 |  2168 | `		}` |
|        - |  2169 | `		/* Insert in the appropriate container */` |
|     4666 |  2170 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4666 |  2171 | `		break;` |
|        - |  2172 | `								  }` |
|        8 |  2173 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2174 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2175 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2176 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2177 | `#ifdef UNTRUST` |
|        - |  2178 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2179 | `			rc = SXERR_CORRUPT;` |
|        - |  2180 | `			break;` |
|        - |  2181 | `		}` |
|        - |  2182 | `#endif` |
|       16 |  2183 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2184 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2185 | `		break;` |
|        - |  2186 | `									   }` |
|        8 |  2187 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2188 | `		/* Raw HTTP request*/` |
|       16 |  2189 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2190 | `		int nByte = va_arg(ap,int);` |
|       16 |  2191 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2192 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2193 | `			break;` |
|        - |  2194 | `		}` |
|       16 |  2195 | `		if( nByte < 0 ){` |
|        - |  2196 | `			/* Compute length automatically */` |
|      ! 0 |  2197 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2198 | `		}` |
|        - |  2199 | `		/* Process the request */` |
|       16 |  2200 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2201 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2202 | `		if( rc == SXRET_OK ){` |
|       16 |  2203 | `			pVm->bHttpContext = 1;` |
|        8 |  2204 | `		}` |
|       16 |  2205 | `		break;` |
|        - |  2206 | `									}` |
|        8 |  2207 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2208 | `		/* Extract HTTP response status code */` |
|       16 |  2209 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2210 | `		if( pStatus ){` |
|       16 |  2211 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2212 | `		}` |
|       16 |  2213 | `		break;` |
|        - |  2214 | `										}` |
|        8 |  2215 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2216 | `		/* Iterate response headers via callback */` |
|        - |  2217 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2218 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2219 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2220 | `		if( xCallback ){` |
|       16 |  2221 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2222 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2223 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2224 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2225 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2226 | `							   pUserData);` |
|       12 |  2227 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2228 | `					break;` |
|        - |  2229 | `				}` |
|        6 |  2230 | `			}` |
|        8 |  2231 | `		}` |
|       16 |  2232 | `		break;` |
|        - |  2233 | `										 }` |
|      ! 0 |  2234 | `	default:` |
|        - |  2235 | `		/* Unknown configuration option */` |
|      ! 0 |  2236 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2237 | `		break;` |
|        - |  2238 | `	}` |
|    37644 |  2239 | `	return rc;` |
|        2 |  2240 |  |
|        - |  2241 | `/* Forward declaration */` |
|        - |  2242 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2243 | `/*` |
|        - |  2244 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2245 | ` * format.` |
|        - |  2246 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2247 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2248 | ` * (STDOUT).` |
|        - |  2249 | ` */` |
|        2 |  2250 | `static sxi32 VmByteCodeDump(` |
|        - |  2251 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2252 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2253 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2254 | `	)` |
|        1 |  2255 |  |
|        - |  2256 | `	static const char zDump[] = {` |
|        - |  2257 | `		"====================================================\n"` |
|        - |  2258 | `		"PH7 VM Dump\n"` |
|        - |  2259 | `		"====================================================\n"` |
|        - |  2260 | `	};` |
|        - |  2261 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2262 | `	sxi32 rc = SXRET_OK;` |
|        - |  2263 | `	sxu32 n;` |
|        - |  2264 | `	/* Point to the PH7 instructions */` |
|        3 |  2265 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2266 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2267 | `	n = 0;` |
|        3 |  2268 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2269 | `	/* Dump instructions */` |
|        7 |  2270 | `	for(;;){` |
|       15 |  2271 | `		if( pInstr >= pEnd ){` |
|        - |  2272 | `			/* No more instructions */` |
|        3 |  2273 | `			break;` |
|        - |  2274 | `		}` |
|        - |  2275 | `		/* Format and call the consumer callback */` |
|       19 |  2276 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2277 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2278 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2279 | `		if( rc != SXRET_OK ){` |
|        - |  2280 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2281 | `			return rc;` |
|        - |  2282 | `		}` |
|       13 |  2283 | `		++n;` |
|       13 |  2284 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2285 | `	}` |
|        3 |  2286 | `	return rc;` |
|        2 |  2287 |  |
|        - |  2288 | `/* Forward declaration */` |
|        - |  2289 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2290 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2291 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2292 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2293 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2294 | `/*` |
|        - |  2295 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2296 | ` * consumer callback.` |
|        - |  2297 | ` */` |
|      552 |  2298 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2299 |  |
|      553 |  2300 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      553 |  2301 | `	sxi32 rc = SXRET_OK;` |
|        - |  2302 | `	/* Append a new line */` |
|        - |  2303 | `#ifdef __WINNT__` |
|        1 |  2304 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2305 | `#else` |
|      552 |  2306 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2307 | `#endif` |
|        - |  2308 | `	/* Invoke the output consumer callback */` |
|      553 |  2309 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      553 |  2310 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      553 |  2311 | `	return rc;` |
|        1 |  2312 |  |
|        - |  2313 | `/*` |
|        - |  2314 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2315 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2316 | ` * information.` |
|        - |  2317 | ` */` |
|      132 |  2318 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2319 |  |
|      134 |  2320 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2321 | `		ph7_value apArg[4];` |
|        - |  2322 | `		ph7_value *apArgPtr[4];` |
|        - |  2323 | `		ph7_value sResult;` |
|        - |  2324 | `		SyString sErr;` |
|        - |  2325 | `		/* Prepare arguments */` |
|       61 |  2326 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2327 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2328 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2329 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2330 | `		if( pFile ){` |
|       61 |  2331 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2332 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2333 | `		}else{` |
|      ! 0 |  2334 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2335 | `		}` |
|       61 |  2336 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2337 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2338 | `		/* Set up pointer array */` |
|       61 |  2339 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2340 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2341 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2342 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2343 | `		/* Call the handler */` |
|       61 |  2344 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2345 | `		/* Check return value */` |
|       61 |  2346 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2347 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2348 | `		}` |
|        - |  2349 | `		/* Release */` |
|       61 |  2350 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2351 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2352 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2353 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2354 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2355 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2356 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2357 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2358 | `	}` |
|        - |  2359 | `	/* No handler, always call error handler */` |
|       73 |  2360 | `	return TRUE;` |
|       68 |  2361 |  |
|       96 |  2362 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2363 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2364 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2365 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2366 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2367 | `	)` |
|        2 |  2368 |  |
|       98 |  2369 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2370 | `	SyString *pFile;` |
|        - |  2371 | `	char *zErr;` |
|       98 |  2372 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2373 | `	if( !pVm->bErrReport ){` |
|        - |  2374 | `		/* Don't bother reporting errors */` |
|        3 |  2375 | `		return SXRET_OK;` |
|        - |  2376 | `	}` |
|        - |  2377 | `	/* Reset the working buffer */` |
|       96 |  2378 | `	SyBlobReset(pWorker);` |
|        - |  2379 | `	/* Peek the processed file if available */` |
|       96 |  2380 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2381 | `	if( pFile ){` |
|        - |  2382 | `		/* Append file name */` |
|       96 |  2383 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2384 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2385 | `	}` |
|        - |  2386 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2387 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2388 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2389 | `	 * E_DEPRECATED). */` |
|       96 |  2390 | `	zErr = "Error:  ";` |
|       96 |  2391 | `	switch(iErr){` |
|       18 |  2392 | `	case PH7_CTX_WARNING:` |
|       38 |  2393 | `		zErr = "Warning:  ";` |
|       38 |  2394 | `		break;` |
|        6 |  2395 | `	case PH7_CTX_NOTICE:` |
|       14 |  2396 | `		zErr = "Notice:  ";` |
|       12 |  2397 | `		break;` |
|       23 |  2398 | `	default:` |
|        - |  2399 | `		/* keep iErr unchanged */` |
|       46 |  2400 | `		break;` |
|        - |  2401 | `	}` |
|       96 |  2402 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2403 | `	if( pFuncName ){` |
|        - |  2404 | `		/* Append function name first */` |
|       23 |  2405 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2406 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2407 | `	}` |
|       96 |  2408 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2409 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2410 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2411 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2412 | `	}` |
|       96 |  2413 | `	return rc;` |
|       50 |  2414 |  |
|        - |  2415 | `/*` |
|        - |  2416 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2417 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2418 | ` * information.` |
|        - |  2419 | ` */` |
|       38 |  2420 | `static sxi32 VmThrowErrorAp(` |
|        - |  2421 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2422 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2423 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2424 | `	const char *zFormat, /* Format message */` |
|        - |  2425 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2426 | `	)` |
|        2 |  2427 |  |
|       40 |  2428 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2429 | `	SyBlob sMsg;` |
|        - |  2430 | `	SyString *pFile;` |
|        - |  2431 | `	char *zErr;` |
|       40 |  2432 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2433 | `	if( !pVm->bErrReport ){` |
|        - |  2434 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2435 | `		return SXRET_OK;` |
|        - |  2436 | `	}` |
|        - |  2437 | `	/* Reset the working buffer */` |
|       40 |  2438 | `	SyBlobReset(pWorker);` |
|        - |  2439 | `	/* Peek the processed file if available */` |
|       40 |  2440 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2441 | `	if( pFile ){` |
|        - |  2442 | `		/* Append file name */` |
|       40 |  2443 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2444 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2445 | `	}` |
|        - |  2446 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2447 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2448 | `	 * the correct errno value. */` |
|       40 |  2449 | `	zErr = "Error:  ";` |
|       40 |  2450 | `	switch(iErr){` |
|        4 |  2451 | `	case PH7_CTX_WARNING:` |
|        9 |  2452 | `		zErr = "Warning:  ";` |
|        9 |  2453 | `		break;` |
|        3 |  2454 | `	case PH7_CTX_NOTICE:` |
|        7 |  2455 | `		zErr = "Notice:  ";` |
|        6 |  2456 | `		break;` |
|       12 |  2457 | `	default:` |
|        - |  2458 | `		/* do not change iErr */` |
|       24 |  2459 | `		break;` |
|        - |  2460 | `	}` |
|       40 |  2461 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2462 | `	if( pFuncName ){` |
|        - |  2463 | `		/* Append function name first */` |
|       26 |  2464 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2465 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2466 | `	}` |
|        - |  2467 | `	/* Format the raw message */` |
|       40 |  2468 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2469 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2470 | `	/* Check if a user error handler is installed */` |
|       40 |  2471 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2472 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2473 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2474 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2475 | `	}` |
|       40 |  2476 | `	SyBlobRelease(&sMsg);` |
|       40 |  2477 | `	return rc;` |
|       21 |  2478 |  |
|        - |  2479 | `/*` |
|        - |  2480 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2481 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2482 | ` * information.` |
|        - |  2483 | ` * ------------------------------------` |
|        - |  2484 | ` * Simple boring wrapper function.` |
|        - |  2485 | ` * ------------------------------------` |
|        - |  2486 | ` */` |
|       14 |  2487 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2488 |  |
|        - |  2489 | `	va_list ap;` |
|        - |  2490 | `	sxi32 rc;` |
|       15 |  2491 | `	va_start(ap,zFormat);` |
|       15 |  2492 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2493 | `	va_end(ap);` |
|       15 |  2494 | `	return rc;` |
|        1 |  2495 |  |
|        - |  2496 | `/*` |
|        - |  2497 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2498 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2499 | ` * information.` |
|        - |  2500 | ` * ------------------------------------` |
|        - |  2501 | ` * Simple boring wrapper function.` |
|        - |  2502 | ` * ------------------------------------` |
|        - |  2503 | ` */` |
|       24 |  2504 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2505 |  |
|        - |  2506 | `	sxi32 rc;` |
|       26 |  2507 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2508 | `	return rc;` |
|        2 |  2509 |  |
|        - |  2510 | `/*` |
|        - |  2511 | ` * Resolve function context from the current frame.` |
|        - |  2512 | ` */` |
|      950 |  2513 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2514 |  |
|        - |  2515 | `	VmFrame *pFrame;` |
|        - |  2516 | `	ph7_vm_func *pFunc;` |
|      951 |  2517 | `	*pzFuncName = 0;` |
|      951 |  2518 | `	*pnFuncLen = 0;` |
|      951 |  2519 | `	pFrame = pVm->pFrame;` |
|      951 |  2520 | `	if( pFrame == 0 ){` |
|      ! 0 |  2521 | `		return;` |
|        - |  2522 | `	}` |
|      951 |  2523 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      951 |  2524 | `	if( pFrame->pParent == 0 ){` |
|      945 |  2525 | `		return;` |
|        - |  2526 | `	}` |
|        7 |  2527 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2528 | `	if( pFunc == 0 ){` |
|      ! 0 |  2529 | `		return;` |
|        - |  2530 | `	}` |
|        7 |  2531 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2532 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      476 |  2533 |  |
|        - |  2534 | `/*` |
|        - |  2535 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2536 | ` */` |
|      478 |  2537 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2538 |  |
|        - |  2539 | `	SyBlob sOut;` |
|        - |  2540 | `	SyString *pFile;` |
|      479 |  2541 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2542 | `		return PH7_OK;` |
|        - |  2543 | `	}` |
|      479 |  2544 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2545 | `		zClass = "Exception";` |
|      ! 0 |  2546 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2547 | `	}` |
|      479 |  2548 | `	if( zMsg == 0 ){` |
|      ! 0 |  2549 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2550 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2551 | `	}` |
|      479 |  2552 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      473 |  2553 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      236 |  2554 | `	}` |
|      479 |  2555 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      479 |  2556 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      479 |  2557 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      479 |  2558 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      479 |  2559 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      479 |  2560 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      479 |  2561 | `	if( pFile ){` |
|      479 |  2562 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      479 |  2563 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2564 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      239 |  2565 | `	}` |
|      479 |  2566 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      479 |  2567 | `	if( pFile ){` |
|      479 |  2568 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      479 |  2569 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2570 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2571 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2572 | `		}else{` |
|      473 |  2573 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2574 | `		}` |
|      239 |  2575 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2576 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2577 | `	}else{` |
|      ! 0 |  2578 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2579 | `	}` |
|      479 |  2580 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      479 |  2581 | `	if( pFile ){` |
|      479 |  2582 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      479 |  2583 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      479 |  2584 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2585 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      239 |  2586 | `	}` |
|      479 |  2587 | `	VmCallErrorHandler(pVm,&sOut);` |
|      479 |  2588 | `	SyBlobRelease(&sOut);` |
|      479 |  2589 | `	return PH7_ABORT;` |
|      240 |  2590 |  |
|        - |  2591 | `/*` |
|        - |  2592 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2593 | ` */` |
|      480 |  2594 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2595 |  |
|        - |  2596 | `	ph7_vm *pVm;` |
|        - |  2597 | `	ph7_class *pClass;` |
|        - |  2598 | `	ph7_class_instance *pThis;` |
|        - |  2599 | `	ph7_class_method *pCons;` |
|        - |  2600 | `	ph7_value sArg;` |
|        - |  2601 | `	ph7_value *apArg[1];` |
|        - |  2602 | `	SyBlob sMsg;` |
|        - |  2603 | `	SyString sMsgStr;` |
|        - |  2604 | `	VmFrame *pFrame;` |
|        - |  2605 | `	va_list ap;` |
|        - |  2606 | `	sxi32 rc;` |
|        - |  2607 |  |
|      482 |  2608 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2609 | `		return PH7_ABORT;` |
|        - |  2610 | `	}` |
|      482 |  2611 | `	pVm = pCtx->pVm;` |
|      482 |  2612 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2613 | `		zClass = "Error";` |
|      ! 0 |  2614 | `	}` |
|      482 |  2615 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  2616 | `	if( pClass == 0 ){` |
|      ! 0 |  2617 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2618 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2619 | `			zClass` |
|        - |  2620 | `			);` |
|        - |  2621 | `	}` |
|      482 |  2622 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  2623 | `	if( pThis == 0 ){` |
|      ! 0 |  2624 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2625 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2626 | `			);` |
|        - |  2627 | `	}` |
|        - |  2628 |  |
|      482 |  2629 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  2630 | `	va_start(ap,zFormat);` |
|      482 |  2631 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  2632 | `	va_end(ap);` |
|        - |  2633 |  |
|      482 |  2634 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  2635 | `	if( pCons ){` |
|      482 |  2636 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  2637 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  2638 | `		apArg[0] = &sArg;` |
|      482 |  2639 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  2640 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  2641 | `	}` |
|      482 |  2642 | `	SyBlobRelease(&sMsg);` |
|        - |  2643 |  |
|      482 |  2644 | `	pFrame = pVm->pFrame;` |
|      482 |  2645 | `	if( pFrame ){` |
|      482 |  2646 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  2647 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  2648 | `	}` |
|      482 |  2649 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  2650 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  2651 | `	if( rc == SXERR_ABORT ){` |
|      471 |  2652 | `		return PH7_ABORT;` |
|        - |  2653 | `	}` |
|       12 |  2654 | `	return PH7_EXCEPTION;` |
|      242 |  2655 |  |
|        - |  2656 | `/*` |
|        - |  2657 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2658 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2659 | ` */` |
|      ! 0 |  2660 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2661 |  |
|        - |  2662 | `	ph7_vm *pVm;` |
|        - |  2663 | `	SyBlob sMsg;` |
|      ! 0 |  2664 | `	const char *zFuncName = 0;` |
|      ! 0 |  2665 | `	int nFuncLen = 0;` |
|        - |  2666 | `	va_list ap;` |
|        - |  2667 | `	sxi32 rc;` |
|        - |  2668 |  |
|      ! 0 |  2669 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2670 | `		return PH7_OK;` |
|        - |  2671 | `	}` |
|      ! 0 |  2672 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2673 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2674 | `		zClass = "Error";` |
|      ! 0 |  2675 | `	}` |
|        - |  2676 |  |
|      ! 0 |  2677 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2678 |  |
|      ! 0 |  2679 | `	va_start(ap,zFormat);` |
|      ! 0 |  2680 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2681 | `	va_end(ap);` |
|        - |  2682 |  |
|      ! 0 |  2683 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2684 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2685 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2686 | `	}` |
|      ! 0 |  2687 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2688 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2689 | `	}` |
|      ! 0 |  2690 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2691 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2692 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2693 | `	return rc;` |
|      ! 0 |  2694 |  |
|        - |  2695 | `/*` |
|        - |  2696 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2697 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2698 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2699 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2700 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2701 | ` * when VmByteCodeExec returns.` |
|        - |  2702 | ` */` |
|      132 |  2703 | `static sxi32 VmSuspendCtx(` |
|        - |  2704 | `	ph7_vm *pVm,` |
|        - |  2705 | `	ph7_exec_ctx *pCtx,` |
|        - |  2706 | `	sxi32 pc,` |
|        - |  2707 | `	sxi32 nTos` |
|        - |  2708 | `	)` |
|        2 |  2709 |  |
|       66 |  2710 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2711 | `	pCtx->pc = pc;` |
|      134 |  2712 | `	pCtx->nTos = nTos;` |
|      134 |  2713 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2714 | `	return PH7_SUSPEND;` |
|        2 |  2715 |  |
|        - |  2716 | `/*` |
|        - |  2717 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2718 | ` *` |
|        - |  2719 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2720 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2721 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2722 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2723 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2724 | ` * then the program execution is halted.` |
|        - |  2725 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2726 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2727 | ` * or to reset the VM to it's initial state.` |
|        - |  2728 | ` */` |
|    33206 |  2729 | `static sxi32 VmByteCodeExec(` |
|        - |  2730 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2731 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2732 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2733 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2734 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2735 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2736 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2737 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2738 | `	)` |
|        2 |  2739 |  |
|        - |  2740 | `	VmInstr *pInstr;` |
|        - |  2741 | `	ph7_value *pTos;` |
|        - |  2742 | `	SySet aArg;` |
|        - |  2743 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2744 | `	sxi32 pc;` |
|        - |  2745 | `	sxi32 rc;` |
|        - |  2746 | `	/* Argument container */` |
|    33208 |  2747 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    33208 |  2748 | `	if( nTos < 0 ){` |
|    31142 |  2749 | `		pTos = &pStack[-1];` |
|    15572 |  2750 | `	}else{` |
|     2068 |  2751 | `		pTos = &pStack[nTos];` |
|        - |  2752 | `	}` |
|    33208 |  2753 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    33208 |  2754 | `	pc = nPc;` |
|        - |  2755 | `	/* Execute as much as we can */` |
|  5050323 |  2756 | `	for(;;){` |
|        - |  2757 | `		/* Fetch the instruction to execute */` |
| 10099944 |  2758 | `		pInstr = &aInstr[pc];` |
| 10099944 |  2759 | `		rc = SXRET_OK;` |
|        - |  2760 | `/*` |
|        - |  2761 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2762 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2763 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2764 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2765 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2766 | ` */` |
| 10099944 |  2767 | `		switch(pInstr->iOp){` |
|        - |  2768 | `/*` |
|        - |  2769 | ` * DONE: P1 * *` |
|        - |  2770 | ` *` |
|        - |  2771 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2772 | ` * and return immediately.` |
|        - |  2773 | ` */` |
|    16288 |  2774 | `case PH7_OP_DONE:` |
|    32578 |  2775 | `	if( pInstr->iP1 ){` |
|        - |  2776 | `#ifdef UNTRUST` |
|        - |  2777 | `		if( pTos < pStack ){` |
|        - |  2778 | `			goto Abort;` |
|        - |  2779 | `		}` |
|        - |  2780 | `#endif` |
|    18900 |  2781 | `		if( pLastRef ){` |
|    12316 |  2782 | `			*pLastRef = pTos->nIdx;` |
|     6157 |  2783 | `		}` |
|    18900 |  2784 | `		if( pResult ){` |
|        - |  2785 | `			/* Execution result */` |
|    17948 |  2786 | `			PH7_MemObjStore(pTos,pResult);` |
|     8973 |  2787 | `		}` |
|    18900 |  2788 | `		VmPopOperand(&pTos,1);` |
|    23129 |  2789 | `	}else if( pLastRef ){` |
|        - |  2790 | `		/* Nothing referenced */` |
|     1036 |  2791 | `		*pLastRef = SXU32_HIGH;` |
|      517 |  2792 | `	}` |
|        - |  2793 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2794 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2795 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2796 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2797 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2798 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2799 | `	 * block can override it.` |
|        - |  2800 | `	 */` |
|    32580 |  2801 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2802 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2803 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2804 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2805 | `		pExc->pFrame = 0;` |
|        3 |  2806 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2807 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2808 | `			pExc->iFinallyDone = 1;` |
|        - |  2809 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2810 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2811 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2812 | `				goto Abort;` |
|        - |  2813 | `			}` |
|        1 |  2814 | `		}` |
|        1 |  2815 | `	}` |
|    32578 |  2816 | `	goto Done;` |
|        - |  2817 | `/*` |
|        - |  2818 | ` * HALT: P1 * *` |
|        - |  2819 | ` *` |
|        - |  2820 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2821 | ` * and abort immediately.` |
|        - |  2822 | ` */` |
|        4 |  2823 | `case PH7_OP_HALT:` |
|        9 |  2824 | `	if( pInstr->iP1 ){` |
|        - |  2825 | `#ifdef UNTRUST` |
|        - |  2826 | `		if( pTos < pStack ){` |
|        - |  2827 | `			goto Abort;` |
|        - |  2828 | `		}` |
|        - |  2829 | `#endif` |
|        9 |  2830 | `		if( pLastRef ){` |
|      ! 0 |  2831 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2832 | `		}` |
|        9 |  2833 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2834 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2835 | `				/* Output the exit message */` |
|        7 |  2836 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2837 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2838 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2839 | `			}` |
|        7 |  2840 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2841 | `			/* Record exit status */` |
|        5 |  2842 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2843 | `		}` |
|        9 |  2844 | `		VmPopOperand(&pTos,1);` |
|        4 |  2845 | `	}else if( pLastRef ){` |
|        - |  2846 | `		/* Nothing referenced */` |
|      ! 0 |  2847 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2848 | `	}` |
|        - |  2849 | `	/* Check if we're in an included file context */` |
|        9 |  2850 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2851 | `		/* Terminate the entire process */` |
|        9 |  2852 | `		exit(pVm->iExitStatus);` |
|        - |  2853 | `	}` |
|      ! 0 |  2854 | `	goto Abort;` |
|        - |  2855 | `/*` |
|        - |  2856 | ` * JMP: * P2 *` |
|        - |  2857 | ` *` |
|        - |  2858 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2859 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2860 | ` */` |
|   217801 |  2861 | `case PH7_OP_JMP:` |
|   435648 |  2862 | `	pc = pInstr->iP2 - 1;` |
|   435648 |  2863 | `	break;` |
|        - |  2864 | `/*` |
|        - |  2865 | ` * JZ: P1 P2 *` |
|        - |  2866 | ` *` |
|        - |  2867 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2868 | ` * entry in the stack if P1 is zero.` |
|        - |  2869 | ` */` |
|   509398 |  2870 | `case PH7_OP_JZ:` |
|        - |  2871 | `#ifdef UNTRUST` |
|        - |  2872 | `	if( pTos < pStack ){` |
|        - |  2873 | `		goto Abort;` |
|        - |  2874 | `	}` |
|        - |  2875 | `#endif` |
|        - |  2876 | `	/* Get a boolean value */` |
|  1018886 |  2877 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2878 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2879 | `	}` |
|  1018886 |  2880 | `	if( !pTos->x.iVal ){` |
|        - |  2881 | `		/* Take the jump */` |
|   515274 |  2882 | `		pc = pInstr->iP2 - 1;` |
|   257636 |  2883 | `	}` |
|  1018886 |  2884 | `	if( !pInstr->iP1 ){` |
|   809784 |  2885 | `		VmPopOperand(&pTos,1);` |
|   404913 |  2886 | `	}` |
|  1018886 |  2887 | `	break;` |
|        - |  2888 | `/*` |
|        - |  2889 | ` * JNZ: P1 P2 *` |
|        - |  2890 | ` *` |
|        - |  2891 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2892 | ` * entry in the stack if P1 is zero.` |
|        - |  2893 | ` */` |
|    53927 |  2894 | `case PH7_OP_JNZ:` |
|        - |  2895 | `#ifdef UNTRUST` |
|        - |  2896 | `	if( pTos < pStack ){` |
|        - |  2897 | `		goto Abort;` |
|        - |  2898 | `	}` |
|        - |  2899 | `#endif` |
|        - |  2900 | `	/* Get a boolean value */` |
|   107856 |  2901 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2902 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2903 | `	}` |
|   107856 |  2904 | `	if( pTos->x.iVal ){` |
|        - |  2905 | `		/* Take the jump */` |
|     4594 |  2906 | `		pc = pInstr->iP2 - 1;` |
|     2296 |  2907 | `	}` |
|   107856 |  2908 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2909 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2910 | `	}` |
|   107856 |  2911 | `	break;` |
|        - |  2912 | `/*` |
|        - |  2913 | ` * NOOP: * * *` |
|        - |  2914 | ` *` |
|        - |  2915 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2916 | ` * destination.` |
|        - |  2917 | ` */` |
|      ! 0 |  2918 | `case PH7_OP_NOOP:` |
|      ! 0 |  2919 | `	break;` |
|        - |  2920 | `/*` |
|        - |  2921 | ` * POP: P1 * *` |
|        - |  2922 | ` *` |
|        - |  2923 | ` * Pop P1 elements from the operand stack.` |
|        - |  2924 | ` */` |
|   397056 |  2925 | `case PH7_OP_POP: {` |
|   794158 |  2926 | `	sxi32 n = pInstr->iP1;` |
|   794158 |  2927 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2928 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2929 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2930 | `	}` |
|   794158 |  2931 | `	VmPopOperand(&pTos,n);` |
|   794158 |  2932 | `	break;` |
|        - |  2933 | `				 }` |
|        - |  2934 | `/*` |
|        - |  2935 | ` * DUP: * * *` |
|        - |  2936 | ` *` |
|        - |  2937 | ` * Duplicate the top of the stack.` |
|        - |  2938 | ` */` |
|       35 |  2939 | `case PH7_OP_DUP:` |
|        - |  2940 | `#ifdef UNTRUST` |
|        - |  2941 | `	if( pTos < pStack ){` |
|        - |  2942 | `		goto Abort;` |
|        - |  2943 | `	}` |
|        - |  2944 | `#endif` |
|       72 |  2945 | `	pTos++;` |
|       72 |  2946 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2947 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2948 | `	break;` |
|        - |  2949 | `/*` |
|        - |  2950 | ` * NSSWITCH: * * P3` |
|        - |  2951 | ` *` |
|        - |  2952 | ` * Switch the active namespace at runtime.` |
|        - |  2953 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2954 | ` */` |
|     6570 |  2955 | `case PH7_OP_NSSWITCH:` |
|    13142 |  2956 | `	SyBlobReset(&pVm->sNamespace);` |
|    13142 |  2957 | `	if( pInstr->p3 ){` |
|       90 |  2958 | `		const char *zNs = (const char *)pInstr->p3;` |
|       90 |  2959 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       44 |  2960 | `	}` |
|        - |  2961 | `	/* Clear namespace-scoped use-const imports */` |
|    13142 |  2962 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13142 |  2963 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13142 |  2964 | `	break;` |
|        - |  2965 | `/* OP_USECONST P1 * P3` |
|        - |  2966 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  2967 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  2968 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  2969 | ` */` |
|        7 |  2970 | `case PH7_OP_USECONST: {` |
|       16 |  2971 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  2972 | `	if( azPair ){` |
|       16 |  2973 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  2974 | `	}` |
|       16 |  2975 | `	break;` |
|        - |  2976 | `				}` |
|        - |  2977 | `/*` |
|        - |  2978 | ` * CVT_INT: * * *` |
|        - |  2979 | ` *` |
|        - |  2980 | ` * Force the top of the stack to be an integer.` |
|        - |  2981 | ` */` |
|       35 |  2982 | `case PH7_OP_CVT_INT:` |
|        - |  2983 | `#ifdef UNTRUST` |
|        - |  2984 | `	if( pTos < pStack ){` |
|        - |  2985 | `		goto Abort;` |
|        - |  2986 | `	}` |
|        - |  2987 | `#endif` |
|       72 |  2988 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2989 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2990 | `	}` |
|        - |  2991 | `	/* Invalidate any prior representation */` |
|       72 |  2992 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2993 | `	break;` |
|        - |  2994 | `/*` |
|        - |  2995 | ` * CVT_REAL: * * *` |
|        - |  2996 | ` *` |
|        - |  2997 | ` * Force the top of the stack to be a real.` |
|        - |  2998 | ` */` |
|        4 |  2999 | `case PH7_OP_CVT_REAL:` |
|        - |  3000 | `#ifdef UNTRUST` |
|        - |  3001 | `	if( pTos < pStack ){` |
|        - |  3002 | `		goto Abort;` |
|        - |  3003 | `	}` |
|        - |  3004 | `#endif` |
|        9 |  3005 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3006 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3007 | `	}` |
|        - |  3008 | `	/* Invalidate any prior representation */` |
|        9 |  3009 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3010 | `	break;` |
|        - |  3011 | `/*` |
|        - |  3012 | ` * CVT_STR: * * *` |
|        - |  3013 | ` *` |
|        - |  3014 | ` * Force the top of the stack to be a string.` |
|        - |  3015 | ` */` |
|      146 |  3016 | `case PH7_OP_CVT_STR:` |
|        - |  3017 | `#ifdef UNTRUST` |
|        - |  3018 | `	if( pTos < pStack ){` |
|        - |  3019 | `		goto Abort;` |
|        - |  3020 | `	}` |
|        - |  3021 | `#endif` |
|      294 |  3022 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3023 | `		PH7_MemObjToString(pTos);` |
|      146 |  3024 | `	}` |
|      294 |  3025 | `	break;` |
|        - |  3026 | `/*` |
|        - |  3027 | ` * CVT_BOOL: * * *` |
|        - |  3028 | ` *` |
|        - |  3029 | ` * Force the top of the stack to be a boolean.` |
|        - |  3030 | ` */` |
|        5 |  3031 | `case PH7_OP_CVT_BOOL:` |
|        - |  3032 | `#ifdef UNTRUST` |
|        - |  3033 | `	if( pTos < pStack ){` |
|        - |  3034 | `		goto Abort;` |
|        - |  3035 | `	}` |
|        - |  3036 | `#endif` |
|       11 |  3037 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3038 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3039 | `	}` |
|       11 |  3040 | `	break;` |
|        - |  3041 | `/*` |
|        - |  3042 | ` * CVT_NULL: * * *` |
|        - |  3043 | ` *` |
|        - |  3044 | ` * Nullify the top of the stack.` |
|        - |  3045 | ` */` |
|        3 |  3046 | `case PH7_OP_CVT_NULL:` |
|        - |  3047 | `#ifdef UNTRUST` |
|        - |  3048 | `	if( pTos < pStack ){` |
|        - |  3049 | `		goto Abort;` |
|        - |  3050 | `	}` |
|        - |  3051 | `#endif` |
|        7 |  3052 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3053 | `	break;` |
|        - |  3054 | `/*` |
|        - |  3055 | ` * CVT_NUMC: * * *` |
|        - |  3056 | ` *` |
|        - |  3057 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3058 | ` */` |
|      ! 0 |  3059 | `case PH7_OP_CVT_NUMC:` |
|        - |  3060 | `#ifdef UNTRUST` |
|        - |  3061 | `	if( pTos < pStack ){` |
|        - |  3062 | `		goto Abort;` |
|        - |  3063 | `	}` |
|        - |  3064 | `#endif` |
|        - |  3065 | `	/* Force a numeric cast */` |
|      ! 0 |  3066 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3067 | `	break;` |
|        - |  3068 | `/*` |
|        - |  3069 | ` * CVT_ARRAY: * * *` |
|        - |  3070 | ` *` |
|        - |  3071 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3072 | ` */` |
|       10 |  3073 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3074 | `#ifdef UNTRUST` |
|        - |  3075 | `	if( pTos < pStack ){` |
|        - |  3076 | `		goto Abort;` |
|        - |  3077 | `	}` |
|        - |  3078 | `#endif` |
|        - |  3079 | `	/* Force a hashmap cast */` |
|       21 |  3080 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3081 | `	if( rc != SXRET_OK ){` |
|        - |  3082 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3083 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3084 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3085 | `	}` |
|       21 |  3086 | `	break;` |
|        - |  3087 | `/*` |
|        - |  3088 | ` * CVT_OBJ: * * *` |
|        - |  3089 | ` *` |
|        - |  3090 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3091 | ` */` |
|        8 |  3092 | `case PH7_OP_CVT_OBJ:` |
|        - |  3093 | `#ifdef UNTRUST` |
|        - |  3094 | `	if( pTos < pStack ){` |
|        - |  3095 | `		goto Abort;` |
|        - |  3096 | `	}` |
|        - |  3097 | `#endif` |
|       17 |  3098 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3099 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3100 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3101 | `	}` |
|       17 |  3102 | `	break;` |
|        - |  3103 | `/*` |
|        - |  3104 | ` * ERR_CTRL * * *` |
|        - |  3105 | ` *` |
|        - |  3106 | ` * Error control operator.` |
|        - |  3107 | ` */` |
|    13200 |  3108 | `case PH7_OP_ERR_CTRL:` |
|        - |  3109 | `	/*` |
|        - |  3110 | `	 * TICKET 1433-038:` |
|        - |  3111 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3112 | `	 * use the public API,to control error output.` |
|        - |  3113 | `	 */` |
|    26400 |  3114 | `	break;` |
|        - |  3115 | `/*` |
|        - |  3116 | ` * IS_A * * *` |
|        - |  3117 | ` *` |
|        - |  3118 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3119 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3120 | ` * holding a class name or an object).` |
|        - |  3121 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3122 | ` */` |
|       23 |  3123 | `case PH7_OP_IS_A:{` |
|       48 |  3124 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3125 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3126 | `#ifdef UNTRUST` |
|        - |  3127 | `	if( pNos < pStack ){` |
|        - |  3128 | `		goto Abort;` |
|        - |  3129 | `	}` |
|        - |  3130 | `#endif` |
|       48 |  3131 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3132 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3133 | `		ph7_class *pClass = 0;` |
|        - |  3134 | `		/* Extract the target class */` |
|       46 |  3135 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3136 | `			/* Instance already loaded */` |
|      ! 0 |  3137 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3138 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3139 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3140 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3141 | `			/* Handle self/static/parent keywords */` |
|       46 |  3142 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3143 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3144 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3145 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3146 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3147 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3148 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3149 | `					pClass = pSelf->pBase;` |
|        2 |  3150 | `				}` |
|        3 |  3151 | `			}else{` |
|       36 |  3152 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3153 | `			}` |
|       22 |  3154 | `		}` |
|       46 |  3155 | `		if( pClass ){` |
|        - |  3156 | `			/* Perform the query */` |
|       46 |  3157 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3158 | `		}` |
|       22 |  3159 | `	}` |
|        - |  3160 | `	/* Push result */` |
|       48 |  3161 | `	VmPopOperand(&pTos,1);` |
|       48 |  3162 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3163 | `	pTos->x.iVal = iRes;` |
|       48 |  3164 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3165 | `	break;` |
|        - |  3166 | `				 }` |
|        - |  3167 |  |
|        - |  3168 | `/*` |
|        - |  3169 | ` * LOADC P1 P2 *` |
|        - |  3170 | ` *` |
|        - |  3171 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3172 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3173 | ` */` |
|   850101 |  3174 | `case PH7_OP_LOADC: {` |
|        - |  3175 | `	ph7_value *pObj;` |
|        - |  3176 | `	/* Reserve a room */` |
|  1700248 |  3177 | `	pTos++;` |
|  2542091 |  3178 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1700248 |  3179 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3180 | `			SyHashEntry *pEntry;` |
|        - |  3181 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3182 | `			{` |
|        - |  3183 | `				SyHashEntry *pConstImport;` |
|    24911 |  3184 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16606 |  3185 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16608 |  3186 | `				if( pConstImport ){` |
|       11 |  3187 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3188 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3189 | `					if( pEntry ){` |
|       11 |  3190 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3191 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3192 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3193 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3194 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3195 | `						break;` |
|        - |  3196 | `					}` |
|        - |  3197 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3198 | `				}` |
|        - |  3199 | `			}` |
|        - |  3200 | `			/* Candidate for expansion via user defined callbacks */` |
|    16598 |  3201 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16598 |  3202 | `			if( pEntry ){` |
|    16594 |  3203 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3204 | `				/* Set a NULL default value */` |
|    16594 |  3205 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16594 |  3206 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3207 | `				/* Invoke the callback and deal with the expanded value */` |
|    16594 |  3208 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3209 | `				/* Mark as constant */` |
|    16594 |  3210 | `				pTos->nIdx = SXU32_HIGH;` |
|    16594 |  3211 | `				break;` |
|        - |  3212 | `			}` |
|        - |  3213 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3214 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3215 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3216 | `			{` |
|        6 |  3217 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3218 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3219 | `				sxu32 j;` |
|        6 |  3220 | `				int isQualified = 0;` |
|       32 |  3221 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3222 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3223 | `				}` |
|        6 |  3224 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3225 | `					/* Try current_namespace\name */` |
|      ! 0 |  3226 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3227 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3228 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3229 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3230 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3231 | `					if( pEntry ){` |
|      ! 0 |  3232 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3233 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3234 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3235 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3236 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3237 | `						break;` |
|        - |  3238 | `					}` |
|        - |  3239 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3240 | `				}` |
|        6 |  3241 | `				if( isQualified ){` |
|        - |  3242 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3243 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3244 | `					SyBlob sErr;` |
|        3 |  3245 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3246 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3247 | `					if( pErrFile ){` |
|        3 |  3248 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3249 | `					}` |
|        3 |  3250 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3251 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3252 | `					SyBlobRelease(&sErr);` |
|        3 |  3253 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3254 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3255 | `					goto LoadC_Done;` |
|        - |  3256 | `				}` |
|        - |  3257 | `			}` |
|        1 |  3258 | `		}` |
|  1683644 |  3259 | `		PH7_MemObjLoad(pObj,pTos);` |
|   841845 |  3260 | `	}else{` |
|        - |  3261 | `		/* Set a NULL value */` |
|      ! 0 |  3262 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3263 | `	}` |
|   841800 |  3264 | `LoadC_Done:` |
|        - |  3265 | `	/* Mark as constant */` |
|  1683646 |  3266 | `	pTos->nIdx = SXU32_HIGH;` |
|  1683646 |  3267 | `	break;` |
|        - |  3268 | `				  }` |
|        - |  3269 | `/*` |
|        - |  3270 | ` * LOAD: P1 * P3` |
|        - |  3271 | ` *` |
|        - |  3272 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3273 | ` * from the P3 operand.` |
|        - |  3274 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3275 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3276 | ` */` |
|  1365196 |  3277 | `case PH7_OP_LOAD:{` |
|        - |  3278 | `	ph7_value *pObj;` |
|        - |  3279 | `	SyString sName;` |
|  2730614 |  3280 | `	if( pInstr->p3 == 0 ){` |
|        - |  3281 | `		/* Take the variable name from the top of the stack */` |
|        - |  3282 | `#ifdef UNTRUST` |
|        - |  3283 | `		if( pTos < pStack ){` |
|        - |  3284 | `			goto Abort;` |
|        - |  3285 | `		}` |
|        - |  3286 | `#endif` |
|        - |  3287 | `		/* Force a string cast */` |
|       19 |  3288 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3289 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3290 | `		}` |
|       19 |  3291 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3292 | `	}else{` |
|  2730596 |  3293 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3294 | `		/* Reserve a room for the target object */` |
|  2730596 |  3295 | `		pTos++;` |
|        - |  3296 | `	}` |
|        - |  3297 | `	/* Extract the requested memory object */` |
|  2730614 |  3298 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2730614 |  3299 | `	if( pObj == 0 ){` |
|       26 |  3300 | `		if( pInstr->iP1 ){` |
|        - |  3301 | `			/* Variable not found,load NULL */` |
|       26 |  3302 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3303 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3304 | `			}else{` |
|       26 |  3305 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3306 | `			}` |
|       26 |  3307 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1365210 |  3308 | `			break;` |
|      ! 0 |  3309 | `		}else{` |
|        - |  3310 | `			/* Fatal error */` |
|      ! 0 |  3311 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3312 | `			goto Abort;` |
|        - |  3313 | `		}` |
|        - |  3314 | `	}` |
|        - |  3315 | `	/* Load variable contents */` |
|  2730590 |  3316 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2730590 |  3317 | `	pTos->nIdx = pObj->nIdx;` |
|  2730590 |  3318 | `	break;` |
|        - |  3319 | `				   }` |
|        - |  3320 | `/*` |
|        - |  3321 | ` * LOAD_MAP P1 * *` |
|        - |  3322 | ` *` |
|        - |  3323 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3324 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3325 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3326 | ` */` |
|    18960 |  3327 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3328 | `	ph7_hashmap *pMap;` |
|        - |  3329 | `	/* Allocate a new hashmap instance */` |
|    37922 |  3330 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    37922 |  3331 | `	if( pMap == 0 ){` |
|      ! 0 |  3332 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3333 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3334 | `		goto Abort;` |
|        - |  3335 | `	}` |
|    37922 |  3336 | `	if( pInstr->iP1 > 0 ){` |
|     2276 |  3337 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3338 | `		/* Perform the insertion */` |
|     6960 |  3339 | `		while( pEntry < pTos ){` |
|     4686 |  3340 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3341 | `				/* Insertion by reference */` |
|      142 |  3342 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3343 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3344 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3345 | `					);` |
|       48 |  3346 | `			}else{` |
|        - |  3347 | `				/* Standard insertion */` |
|     6887 |  3348 | `				PH7_HashmapInsert(pMap,` |
|     4590 |  3349 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2295 |  3350 | `					&pEntry[1]` |
|        - |  3351 | `				);` |
|        - |  3352 | `			}` |
|        - |  3353 | `			/* Next pair on the stack */` |
|     4686 |  3354 | `			pEntry += 2;` |
|        2 |  3355 | `		}` |
|        - |  3356 | `		/* Pop P1 elements */` |
|     2276 |  3357 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1137 |  3358 | `	}` |
|        - |  3359 | `	/* Push the hashmap */` |
|    37922 |  3360 | `	pTos++;` |
|    37922 |  3361 | `	pTos->nIdx = SXU32_HIGH;` |
|    37922 |  3362 | `	pTos->x.pOther = pMap;` |
|    37922 |  3363 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    37922 |  3364 | `	break;` |
|        - |  3365 | `					  }` |
|        - |  3366 | `/*` |
|        - |  3367 | ` * LOAD_LIST: P1 * *` |
|        - |  3368 | ` *` |
|        - |  3369 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3370 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3371 | ` * Caveats:` |
|        - |  3372 | ` *  This implementation support only a single nesting level.` |
|        - |  3373 | ` */` |
|       26 |  3374 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3375 | `	ph7_value *pEntry;` |
|       53 |  3376 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3377 | `		/* Empty list,break immediately */` |
|      ! 0 |  3378 | `		break;` |
|        - |  3379 | `	}` |
|       53 |  3380 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3381 | `#ifdef UNTRUST` |
|        - |  3382 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3383 | `		goto Abort;` |
|        - |  3384 | `	}` |
|        - |  3385 | `#endif` |
|       53 |  3386 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       49 |  3387 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3388 | `		ph7_hashmap_node *pNode;` |
|        - |  3389 | `		ph7_value sKey,*pObj;` |
|        - |  3390 | `		/* Start Copying */` |
|       49 |  3391 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      153 |  3392 | `		while( pEntry <= pTos ){` |
|      105 |  3393 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       97 |  3394 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       97 |  3395 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       97 |  3396 | `					if( rc == SXRET_OK ){` |
|        - |  3397 | `						/* Store node value */` |
|       97 |  3398 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       49 |  3399 | `					}else{` |
|        - |  3400 | `						/* Nullify the variable */` |
|      ! 0 |  3401 | `						PH7_MemObjRelease(pObj);` |
|        - |  3402 | `					}` |
|       48 |  3403 | `				}` |
|       48 |  3404 | `			}` |
|      105 |  3405 | `			sKey.x.iVal++; /* Next numeric index */` |
|      105 |  3406 | `			pEntry++;` |
|        1 |  3407 | `		}` |
|       24 |  3408 | `	}` |
|       53 |  3409 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       53 |  3410 | `	break;` |
|        - |  3411 | `					   }` |
|        - |  3412 | `/*` |
|        - |  3413 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3414 | ` *` |
|        - |  3415 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3416 | ` * from the stack.` |
|        - |  3417 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3418 | ` * instead.` |
|        - |  3419 | ` */` |
|   218803 |  3420 | `case PH7_OP_LOAD_IDX: {` |
|   437652 |  3421 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   437652 |  3422 | `	ph7_hashmap *pMap = 0;` |
|        - |  3423 | `	ph7_value *pIdx;` |
|   437652 |  3424 | `	pIdx = 0;` |
|   437652 |  3425 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3426 | `		if( !pInstr->iP2){` |
|        - |  3427 | `			/* No available index,load NULL */` |
|      ! 0 |  3428 | `			if( pTos >= pStack ){` |
|      ! 0 |  3429 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3430 | `			}else{` |
|        - |  3431 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3432 | `				pTos++;` |
|      ! 0 |  3433 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3434 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3435 | `			}` |
|        - |  3436 | `			/* Emit a notice */` |
|      ! 0 |  3437 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3438 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3439 | `			break;` |
|        - |  3440 | `		}` |
|      ! 0 |  3441 | `	}else{` |
|   437652 |  3442 | `		pIdx = pTos;` |
|   437652 |  3443 | `		pTos--;` |
|        - |  3444 | `	}` |
|   437652 |  3445 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3446 | `		/* String access */` |
|   343430 |  3447 | `		if( pIdx ){` |
|        - |  3448 | `			sxu32 nOfft;` |
|   343430 |  3449 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3450 | `				/* Force an int cast */` |
|      ! 0 |  3451 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3452 | `			}` |
|   343430 |  3453 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   343430 |  3454 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3455 | `				/* Invalid offset,load null */` |
|      ! 0 |  3456 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3457 | `			}else{` |
|   343430 |  3458 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   343430 |  3459 | `				int c = zData[nOfft];` |
|   343430 |  3460 | `				PH7_MemObjRelease(pTos);` |
|   343430 |  3461 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   343430 |  3462 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3463 | `			}` |
|   171738 |  3464 | `		}else{` |
|        - |  3465 | `			/* No available index,load NULL */` |
|      ! 0 |  3466 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3467 | `		}` |
|   343430 |  3468 | `		break;` |
|        - |  3469 | `	}` |
|    94224 |  3470 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3471 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3472 | `			ph7_value *pObj;` |
|      ! 0 |  3473 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3474 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3475 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3476 | `			}` |
|      ! 0 |  3477 | `		}` |
|      ! 0 |  3478 | `	}` |
|    94224 |  3479 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    94224 |  3480 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    94224 |  3481 | `		if( pInstr->iP2 ){` |
|        - |  3482 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3483 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3484 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3485 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3486 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3487 | `		}` |
|        - |  3488 | `		/* Point to the hashmap */` |
|    94224 |  3489 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    94224 |  3490 | `		if( pIdx ){` |
|        - |  3491 | `			/* Load the desired entry */` |
|    94224 |  3492 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    47111 |  3493 | `		}` |
|    94224 |  3494 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3495 | `			/* Create a new empty entry */` |
|      265 |  3496 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3497 | `			if( rc == SXRET_OK ){` |
|        - |  3498 | `				/* Point to the last inserted entry */` |
|      265 |  3499 | `				pNode = pMap->pLast;` |
|      132 |  3500 | `			}` |
|      132 |  3501 | `		}` |
|    47111 |  3502 | `	}` |
|    94224 |  3503 | `	if( pIdx ){` |
|    94224 |  3504 | `		PH7_MemObjRelease(pIdx);` |
|    47111 |  3505 | `	}` |
|    94224 |  3506 | `	if( rc == SXRET_OK ){` |
|        - |  3507 | `		/* Load entry contents */` |
|    42942 |  3508 | `		if( pMap->iRef < 2 ){` |
|        - |  3509 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3510 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3511 | `			 */` |
|       24 |  3512 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3513 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3514 | `		}else{` |
|    42920 |  3515 | `			pTos->nIdx = pNode->nValIdx;` |
|    42920 |  3516 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    42920 |  3517 | `			PH7_HashmapUnref(pMap);` |
|        - |  3518 | `		}` |
|    21472 |  3519 | `	}else{` |
|        - |  3520 | `		/* No such entry,load NULL */` |
|    51284 |  3521 | `		PH7_MemObjRelease(pTos);` |
|    51284 |  3522 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3523 | `	}` |
|    94224 |  3524 | `	break;` |
|        - |  3525 | `					  }` |
|        - |  3526 | `/*` |
|        - |  3527 | ` * LOAD_CLOSURE * * P3` |
|        - |  3528 | ` *` |
|        - |  3529 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3530 | ` * name in the stack.` |
|        - |  3531 | ` */` |
|        4 |  3532 | `case PH7_OP_LOAD_CLOSURE:{` |
|        9 |  3533 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        9 |  3534 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3535 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3536 | `		ph7_vm_func *pClosure;` |
|        - |  3537 | `		char *zName;` |
|        - |  3538 | `		sxu32 mLen;` |
|        - |  3539 | `		sxu32 n;` |
|        - |  3540 | `		/* Create a new VM function */` |
|        9 |  3541 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3542 | `		/* Generate an unique closure name */` |
|        9 |  3543 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        9 |  3544 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3545 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3546 | `			goto Abort;` |
|        - |  3547 | `		}` |
|        9 |  3548 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        9 |  3549 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3550 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3551 | `		}` |
|        - |  3552 | `		/* Zero the stucture */` |
|        9 |  3553 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3554 | `		/* Perform a structure assignment on read-only items */` |
|        9 |  3555 | `		pClosure->aArgs = pFunc->aArgs;` |
|        9 |  3556 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        9 |  3557 | `		pClosure->aStatic = pFunc->aStatic;` |
|        9 |  3558 | `		pClosure->iFlags = pFunc->iFlags;` |
|        9 |  3559 | `		pClosure->pUserData = pFunc->pUserData;` |
|        9 |  3560 | `		pClosure->sSignature = pFunc->sSignature;` |
|        9 |  3561 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|        9 |  3562 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|        9 |  3563 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3564 | `		/* Register the closure */` |
|        9 |  3565 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3566 | `		/* Set up closure environment */` |
|        9 |  3567 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        9 |  3568 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       27 |  3569 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3570 | `			ph7_value *pValue;` |
|       19 |  3571 | `			pEnv = &aEnv[n];` |
|       19 |  3572 | `			sEnv.sName  = pEnv->sName;` |
|       19 |  3573 | `			sEnv.iFlags = pEnv->iFlags;` |
|       19 |  3574 | `			sEnv.nIdx = SXU32_HIGH;` |
|       19 |  3575 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       19 |  3576 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3577 | `				/* Pass by reference */` |
|      ! 0 |  3578 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3579 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3580 | `					);` |
|      ! 0 |  3581 | `			}` |
|        - |  3582 | `			/* Standard pass by value */` |
|       19 |  3583 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       19 |  3584 | `			if( pValue ){` |
|        - |  3585 | `				/* Copy imported value */` |
|       11 |  3586 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3587 | `			}` |
|        - |  3588 | `			/* Insert the imported variable */` |
|       19 |  3589 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       10 |  3590 | `		}` |
|        - |  3591 | `		/* Finally,load the closure name on the stack */` |
|        9 |  3592 | `		pTos++;` |
|        9 |  3593 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3594 | `	}` |
|        9 |  3595 | `	break;` |
|        - |  3596 | `						 }` |
|        - |  3597 | `/*` |
|        - |  3598 | ` * STORE * P2 P3` |
|        - |  3599 | ` *` |
|        - |  3600 | ` * Perform a store (Assignment) operation.` |
|        - |  3601 | ` */` |
|   116658 |  3602 | `case PH7_OP_STORE: {` |
|        - |  3603 | `	ph7_value *pObj;` |
|        - |  3604 | `	SyString sName;` |
|        - |  3605 | `#ifdef UNTRUST` |
|        - |  3606 | `	if( pTos < pStack ){` |
|        - |  3607 | `		goto Abort;` |
|        - |  3608 | `	}` |
|        - |  3609 | `#endif` |
|   233318 |  3610 | `	if( pInstr->iP2 ){` |
|        - |  3611 | `		sxu32 nIdx;` |
|        - |  3612 | `		/* Member store operation */` |
|     3014 |  3613 | `		nIdx = pTos->nIdx;` |
|     3014 |  3614 | `		VmPopOperand(&pTos,1);` |
|     3014 |  3615 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3616 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3617 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3618 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3619 | `		}else{` |
|        - |  3620 | `			/* Point to the desired memory object */` |
|     3010 |  3621 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3010 |  3622 | `			if( pObj ){` |
|        - |  3623 | `				/* Perform the store operation */` |
|     3010 |  3624 | `				PH7_MemObjStore(pTos,pObj);` |
|     1504 |  3625 | `			}` |
|        - |  3626 | `		}` |
|   118166 |  3627 | `		break;` |
|   230306 |  3628 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3629 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3630 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3631 | `			/* Force a string cast */` |
|      ! 0 |  3632 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3633 | `		}` |
|        7 |  3634 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3635 | `		pTos--;` |
|        - |  3636 | `#ifdef UNTRUST` |
|        - |  3637 | `		if( pTos < pStack  ){` |
|        - |  3638 | `			goto Abort;` |
|        - |  3639 | `		}` |
|        - |  3640 | `#endif` |
|        4 |  3641 | `	}else{` |
|   230300 |  3642 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3643 | `	}` |
|        - |  3644 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   230306 |  3645 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   230306 |  3646 | `	if( pObj == 0 ){` |
|      ! 0 |  3647 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3648 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3649 | `		goto Abort;` |
|        - |  3650 | `	}` |
|   230306 |  3651 | `	if( !pInstr->p3 ){` |
|        7 |  3652 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3653 | `	}` |
|        - |  3654 | `	/* Perform the store operation */` |
|   230306 |  3655 | `	PH7_MemObjStore(pTos,pObj);` |
|   230306 |  3656 | `	break;` |
|        - |  3657 | `				   }` |
|        - |  3658 | `/*` |
|        - |  3659 | ` * STORE_IDX:   P1 * P3` |
|        - |  3660 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3661 | ` *` |
|        - |  3662 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3663 | ` */` |
|    84090 |  3664 | `case PH7_OP_STORE_IDX:` |
|        - |  3665 | `case PH7_OP_STORE_IDX_REF: {` |
|   168182 |  3666 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3667 | `	ph7_value *pKey;` |
|        - |  3668 | `	sxu32 nIdx;` |
|   168182 |  3669 | `	if( pInstr->iP1 ){` |
|        - |  3670 | `		/* Key is next on stack */` |
|    58240 |  3671 | `		pKey = pTos;` |
|    58240 |  3672 | `		pTos--;` |
|    29121 |  3673 | `	}else{` |
|   109944 |  3674 | `		pKey = 0;` |
|        - |  3675 | `	}` |
|   168182 |  3676 | `	nIdx = pTos->nIdx;` |
|   168182 |  3677 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3678 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3679 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3680 | `		 * checking true sharing count, then re-add after separation. */` |
|   168130 |  3681 | `		if( nIdx != SXU32_HIGH ){` |
|   168130 |  3682 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   252194 |  3683 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   168130 |  3684 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3685 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3686 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3687 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3688 | `				 * refcounts if the backing array was already separated. */` |
|   168130 |  3689 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   168130 |  3690 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   168130 |  3691 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   168130 |  3692 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   168130 |  3693 | `					pTos->x.pOther = pMap;` |
|    84066 |  3694 | `				}else{` |
|        - |  3695 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3696 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3697 | `					pMap = pCur;` |
|        - |  3698 | `				}` |
|    84066 |  3699 | `			}else{` |
|      ! 0 |  3700 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3701 | `			}` |
|    84066 |  3702 | `		}else{` |
|      ! 0 |  3703 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3704 | `		}` |
|   168130 |  3705 | `		if( pMap->iRef < 2 ){` |
|        - |  3706 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3707 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3708 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3709 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3710 | `			pMap->iRef = 2;` |
|      ! 0 |  3711 | `		}` |
|    84066 |  3712 | `	}else{` |
|        - |  3713 | `		ph7_value *pObj;` |
|       53 |  3714 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3715 | `		if( pObj == 0 ){` |
|      ! 0 |  3716 | `			if( pKey ){` |
|      ! 0 |  3717 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3718 | `			}` |
|      ! 0 |  3719 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3720 | `			break;` |
|        - |  3721 | `		}` |
|        - |  3722 | `		/* Phase#1: Load the array */` |
|       53 |  3723 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3724 | `			VmPopOperand(&pTos,1);` |
|       53 |  3725 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3726 | `				/* Force a string cast */` |
|      ! 0 |  3727 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3728 | `			}` |
|       53 |  3729 | `			if( pKey == 0 ){` |
|        - |  3730 | `				/* Append string */` |
|        3 |  3731 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3732 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3733 | `				}` |
|        2 |  3734 | `			}else{` |
|        - |  3735 | `				sxu32 nOfft;` |
|       51 |  3736 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3737 | `					/* Force an int cast */` |
|       51 |  3738 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3739 | `				}` |
|       51 |  3740 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3741 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3742 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3743 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3744 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3745 | `				}else{` |
|      ! 0 |  3746 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3747 | `						/* Perform an append operation */` |
|      ! 0 |  3748 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3749 | `					}` |
|        - |  3750 | `				}` |
|        - |  3751 | `			}` |
|       53 |  3752 | `			if( pKey ){` |
|       51 |  3753 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3754 | `			}` |
|       53 |  3755 | `			break;` |
|      ! 0 |  3756 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3757 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3758 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3759 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3760 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3761 | `				goto Abort;` |
|        - |  3762 | `			}` |
|      ! 0 |  3763 | `		}` |
|        - |  3764 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3765 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3766 | `	}` |
|   168130 |  3767 | `	VmPopOperand(&pTos,1);` |
|        - |  3768 | `	/* Phase#2: Perform the insertion */` |
|   168130 |  3769 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3770 | `		/* Insertion by reference */` |
|       15 |  3771 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3772 | `	}else{` |
|   168116 |  3773 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3774 | `	}` |
|   168130 |  3775 | `	if( pKey ){` |
|    58190 |  3776 | `		PH7_MemObjRelease(pKey);` |
|    29094 |  3777 | `	}` |
|   168130 |  3778 | `	break;` |
|        - |  3779 | `					   }` |
|        - |  3780 | `/*` |
|        - |  3781 | ` * INCR: P1 * *` |
|        - |  3782 | ` *` |
|        - |  3783 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3784 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3785 | ` * the stack and increment after that.` |
|        - |  3786 | ` */` |
|   152479 |  3787 | `case PH7_OP_INCR:` |
|        - |  3788 | `#ifdef UNTRUST` |
|        - |  3789 | `	if( pTos < pStack ){` |
|        - |  3790 | `		goto Abort;` |
|        - |  3791 | `	}` |
|        - |  3792 | `#endif` |
|   305004 |  3793 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   305004 |  3794 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3795 | `			ph7_value *pObj;` |
|   305004 |  3796 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3797 | `				/* Force a numeric cast */` |
|   305004 |  3798 | `				PH7_MemObjToNumeric(pObj);` |
|   305004 |  3799 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3800 | `					pObj->rVal++;` |
|        - |  3801 | `					/* Try to get an integer representation */` |
|      ! 0 |  3802 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3803 | `				}else{` |
|   305004 |  3804 | `					pObj->x.iVal++;` |
|   305004 |  3805 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3806 | `				}` |
|   305004 |  3807 | `				if( pInstr->iP1 ){` |
|        - |  3808 | `					/* Pre-icrement */` |
|       71 |  3809 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3810 | `				}` |
|   152523 |  3811 | `			}` |
|   152525 |  3812 | `		}else{` |
|      ! 0 |  3813 | `			if( pInstr->iP1 ){` |
|        - |  3814 | `				/* Force a numeric cast */` |
|      ! 0 |  3815 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3816 | `				/* Pre-increment */` |
|      ! 0 |  3817 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3818 | `					pTos->rVal++;` |
|        - |  3819 | `					/* Try to get an integer representation */` |
|      ! 0 |  3820 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3821 | `				}else{` |
|      ! 0 |  3822 | `					pTos->x.iVal++;` |
|      ! 0 |  3823 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3824 | `				}` |
|      ! 0 |  3825 | `			}` |
|        - |  3826 | `		}` |
|   152523 |  3827 | `	}` |
|   305004 |  3828 | `	break;` |
|        - |  3829 | `/*` |
|        - |  3830 | ` * DECR: P1 * *` |
|        - |  3831 | ` *` |
|        - |  3832 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3833 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3834 | ` * and decrement after that.` |
|        - |  3835 | ` */` |
|        2 |  3836 | `case PH7_OP_DECR:` |
|        - |  3837 | `#ifdef UNTRUST` |
|        - |  3838 | `	if( pTos < pStack ){` |
|        - |  3839 | `		goto Abort;` |
|        - |  3840 | `	}` |
|        - |  3841 | `#endif` |
|        5 |  3842 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3843 | `		/* Force a numeric cast */` |
|        5 |  3844 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3845 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3846 | `			ph7_value *pObj;` |
|        5 |  3847 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3848 | `				/* Force a numeric cast */` |
|        5 |  3849 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3850 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3851 | `					pObj->rVal--;` |
|        - |  3852 | `					/* Try to get an integer representation */` |
|      ! 0 |  3853 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3854 | `				}else{` |
|        5 |  3855 | `					pObj->x.iVal--;` |
|        5 |  3856 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3857 | `				}` |
|        5 |  3858 | `				if( pInstr->iP1 ){` |
|        - |  3859 | `					/* Pre-icrement */` |
|      ! 0 |  3860 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3861 | `				}` |
|        2 |  3862 | `			}` |
|        3 |  3863 | `		}else{` |
|      ! 0 |  3864 | `			if( pInstr->iP1 ){` |
|        - |  3865 | `				/* Pre-increment */` |
|      ! 0 |  3866 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3867 | `					pTos->rVal--;` |
|        - |  3868 | `					/* Try to get an integer representation */` |
|      ! 0 |  3869 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3870 | `				}else{` |
|      ! 0 |  3871 | `					pTos->x.iVal--;` |
|      ! 0 |  3872 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3873 | `				}` |
|      ! 0 |  3874 | `			}` |
|        - |  3875 | `		}` |
|        2 |  3876 | `	}` |
|        5 |  3877 | `	break;` |
|        - |  3878 | `/*` |
|        - |  3879 | ` * UMINUS: * * *` |
|        - |  3880 | ` *` |
|        - |  3881 | ` * Perform a unary minus operation.` |
|        - |  3882 | ` */` |
|    24550 |  3883 | `case PH7_OP_UMINUS:` |
|        - |  3884 | `#ifdef UNTRUST` |
|        - |  3885 | `	if( pTos < pStack ){` |
|        - |  3886 | `		goto Abort;` |
|        - |  3887 | `	}` |
|        - |  3888 | `#endif` |
|        - |  3889 | `	/* Force a numeric (integer,real or both) cast */` |
|    49102 |  3890 | `	PH7_MemObjToNumeric(pTos);` |
|    49102 |  3891 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3892 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3893 | `	}` |
|    49102 |  3894 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    49072 |  3895 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24535 |  3896 | `	}` |
|    49102 |  3897 | `	break;` |
|        - |  3898 | `/*` |
|        - |  3899 | ` * UPLUS: * * *` |
|        - |  3900 | ` *` |
|        - |  3901 | ` * Perform a unary plus operation.` |
|        - |  3902 | ` */` |
|       17 |  3903 | `case PH7_OP_UPLUS:` |
|        - |  3904 | `#ifdef UNTRUST` |
|        - |  3905 | `	if( pTos < pStack ){` |
|        - |  3906 | `		goto Abort;` |
|        - |  3907 | `	}` |
|        - |  3908 | `#endif` |
|        - |  3909 | `	/* Force a numeric (integer,real or both) cast */` |
|       35 |  3910 | `	PH7_MemObjToNumeric(pTos);` |
|       35 |  3911 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3912 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3913 | `	}` |
|       35 |  3914 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       35 |  3915 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       17 |  3916 | `	}` |
|       35 |  3917 | `	break;` |
|        - |  3918 | `/*` |
|        - |  3919 | ` * OP_LNOT: * * *` |
|        - |  3920 | ` *` |
|        - |  3921 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3922 | ` * with its complement.` |
|        - |  3923 | ` */` |
|    40417 |  3924 | `case PH7_OP_LNOT:` |
|        - |  3925 | `#ifdef UNTRUST` |
|        - |  3926 | `	if( pTos < pStack ){` |
|        - |  3927 | `		goto Abort;` |
|        - |  3928 | `	}` |
|        - |  3929 | `#endif` |
|        - |  3930 | `	/* Force a boolean cast */` |
|    80880 |  3931 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3932 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3933 | `	}` |
|    80880 |  3934 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80880 |  3935 | `	break;` |
|        - |  3936 | `/*` |
|        - |  3937 | ` * OP_BITNOT: * * *` |
|        - |  3938 | ` *` |
|        - |  3939 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3940 | ` * with its ones-complement.` |
|        - |  3941 | ` */` |
|       13 |  3942 | `case PH7_OP_BITNOT:` |
|        - |  3943 | `#ifdef UNTRUST` |
|        - |  3944 | `	if( pTos < pStack ){` |
|        - |  3945 | `		goto Abort;` |
|        - |  3946 | `	}` |
|        - |  3947 | `#endif` |
|        - |  3948 | `	/* Force an integer cast */` |
|       28 |  3949 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3950 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3951 | `	}` |
|       28 |  3952 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  3953 | `	break;` |
|        - |  3954 | `/* OP_MUL * * *` |
|        - |  3955 | ` * OP_MUL_STORE * * *` |
|        - |  3956 | ` *` |
|        - |  3957 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3958 | ` * and push the result back onto the stack.` |
|        - |  3959 | ` */` |
|     1249 |  3960 | `case PH7_OP_MUL:` |
|        - |  3961 | `case PH7_OP_MUL_STORE: {` |
|     2500 |  3962 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3963 | `	/* Force the operand to be numeric */` |
|        - |  3964 | `#ifdef UNTRUST` |
|        - |  3965 | `	if( pNos < pStack ){` |
|        - |  3966 | `		goto Abort;` |
|        - |  3967 | `	}` |
|        - |  3968 | `#endif` |
|     2500 |  3969 | `	PH7_MemObjToNumeric(pTos);` |
|     2500 |  3970 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3971 | `	/* Perform the requested operation */` |
|     2500 |  3972 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3973 | `		/* Floating point arithemic */` |
|        - |  3974 | `		ph7_real a,b,r;` |
|       17 |  3975 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3976 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3977 | `		}` |
|       17 |  3978 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3979 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3980 | `		}` |
|       17 |  3981 | `		a = pNos->rVal;` |
|       17 |  3982 | `		b = pTos->rVal;` |
|       17 |  3983 | `		r = a * b;` |
|        - |  3984 | `		/* Push the result */` |
|       17 |  3985 | `		pNos->rVal = r;` |
|       17 |  3986 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3987 | `		/* Try to get an integer representation */` |
|       17 |  3988 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3989 | `	}else{` |
|        - |  3990 | `		/* Integer arithmetic */` |
|        - |  3991 | `		sxi64 a,b,r;` |
|     2484 |  3992 | `		a = pNos->x.iVal;` |
|     2484 |  3993 | `		b = pTos->x.iVal;` |
|     2484 |  3994 | `		r = a * b;` |
|        - |  3995 | `		/* Push the result */` |
|     2484 |  3996 | `		pNos->x.iVal = r;` |
|     2484 |  3997 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3998 | `	}` |
|     2500 |  3999 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4000 | `		ph7_value *pObj;` |
|       27 |  4001 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4002 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       27 |  4003 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  4004 | `			PH7_MemObjStore(pNos,pObj);` |
|       13 |  4005 | `		}` |
|       13 |  4006 | `	}` |
|     2500 |  4007 | `	VmPopOperand(&pTos,1);` |
|     2500 |  4008 | `	break;` |
|        - |  4009 | `				 }` |
|        - |  4010 | `/* OP_ADD * * *` |
|        - |  4011 | ` *` |
|        - |  4012 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4013 | ` * and push the result back onto the stack.` |
|        - |  4014 | ` */` |
|      452 |  4015 | `case PH7_OP_ADD:{` |
|      906 |  4016 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4017 | `#ifdef UNTRUST` |
|        - |  4018 | `	if( pNos < pStack ){` |
|        - |  4019 | `		goto Abort;` |
|        - |  4020 | `	}` |
|        - |  4021 | `#endif` |
|        - |  4022 | `	/* Perform the addition */` |
|      906 |  4023 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      906 |  4024 | `	VmPopOperand(&pTos,1);` |
|      906 |  4025 | `	break;` |
|        - |  4026 | `				}` |
|        - |  4027 | `/*` |
|        - |  4028 | ` * OP_ADD_STORE * * *` |
|        - |  4029 | ` *` |
|        - |  4030 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4031 | ` * and push the result back onto the stack.` |
|        - |  4032 | ` */` |
|      495 |  4033 | `case PH7_OP_ADD_STORE:{` |
|      992 |  4034 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4035 | `	ph7_value *pObj;` |
|        - |  4036 | `	sxu32 nIdx;` |
|        - |  4037 | `#ifdef UNTRUST` |
|        - |  4038 | `	if( pNos < pStack ){` |
|        - |  4039 | `		goto Abort;` |
|        - |  4040 | `	}` |
|        - |  4041 | `#endif` |
|        - |  4042 | `	/* Perform the addition */` |
|      992 |  4043 | `	nIdx = pTos->nIdx;` |
|      992 |  4044 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4045 | `	/* Peform the store operation */` |
|      992 |  4046 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4047 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      992 |  4048 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      992 |  4049 | `		PH7_MemObjStore(pTos,pObj);` |
|      495 |  4050 | `	}` |
|        - |  4051 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      992 |  4052 | `	PH7_MemObjStore(pTos,pNos);` |
|      992 |  4053 | `	VmPopOperand(&pTos,1);` |
|      992 |  4054 | `	break;` |
|        - |  4055 | `				}` |
|        - |  4056 | `/* OP_SUB * * *` |
|        - |  4057 | ` *` |
|        - |  4058 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4059 | ` * first (what was next on the stack) from the second (the` |
|        - |  4060 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4061 | ` */` |
|      301 |  4062 | `case PH7_OP_SUB: {` |
|      604 |  4063 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4064 | `#ifdef UNTRUST` |
|        - |  4065 | `	if( pNos < pStack ){` |
|        - |  4066 | `		goto Abort;` |
|        - |  4067 | `	}` |
|        - |  4068 | `#endif` |
|      604 |  4069 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4070 | `		/* Floating point arithemic */` |
|        - |  4071 | `		ph7_real a,b,r;` |
|       95 |  4072 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4073 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4074 | `		}` |
|       95 |  4075 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4076 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4077 | `		}` |
|       95 |  4078 | `		a = pNos->rVal;` |
|       95 |  4079 | `		b = pTos->rVal;` |
|       95 |  4080 | `		r = a - b;` |
|        - |  4081 | `		/* Push the result */` |
|       95 |  4082 | `		pNos->rVal = r;` |
|       95 |  4083 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4084 | `		/* Try to get an integer representation */` |
|       95 |  4085 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4086 | `	}else{` |
|        - |  4087 | `		/* Integer arithmetic */` |
|        - |  4088 | `		sxi64 a,b,r;` |
|      510 |  4089 | `		a = pNos->x.iVal;` |
|      510 |  4090 | `		b = pTos->x.iVal;` |
|      510 |  4091 | `		r = a - b;` |
|        - |  4092 | `		/* Push the result */` |
|      510 |  4093 | `		pNos->x.iVal = r;` |
|      510 |  4094 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4095 | `	}` |
|      604 |  4096 | `	VmPopOperand(&pTos,1);` |
|      604 |  4097 | `	break;` |
|        - |  4098 | `				 }` |
|        - |  4099 | `/* OP_SUB_STORE * * *` |
|        - |  4100 | ` *` |
|        - |  4101 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4102 | ` * first (what was next on the stack) from the second (the` |
|        - |  4103 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4104 | ` */` |
|        2 |  4105 | `case PH7_OP_SUB_STORE: {` |
|        5 |  4106 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4107 | `	ph7_value *pObj;` |
|        - |  4108 | `#ifdef UNTRUST` |
|        - |  4109 | `	if( pNos < pStack ){` |
|        - |  4110 | `		goto Abort;` |
|        - |  4111 | `	}` |
|        - |  4112 | `#endif` |
|        5 |  4113 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4114 | `		/* Floating point arithemic */` |
|        - |  4115 | `		ph7_real a,b,r;` |
|      ! 0 |  4116 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4117 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4118 | `		}` |
|      ! 0 |  4119 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4120 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4121 | `		}` |
|      ! 0 |  4122 | `		a = pTos->rVal;` |
|      ! 0 |  4123 | `		b = pNos->rVal;` |
|      ! 0 |  4124 | `		r = a - b;` |
|        - |  4125 | `		/* Push the result */` |
|      ! 0 |  4126 | `		pNos->rVal = r;` |
|      ! 0 |  4127 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4128 | `		/* Try to get an integer representation */` |
|      ! 0 |  4129 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4130 | `	}else{` |
|        - |  4131 | `		/* Integer arithmetic */` |
|        - |  4132 | `		sxi64 a,b,r;` |
|        5 |  4133 | `		a = pTos->x.iVal;` |
|        5 |  4134 | `		b = pNos->x.iVal;` |
|        5 |  4135 | `		r = a - b;` |
|        - |  4136 | `		/* Push the result */` |
|        5 |  4137 | `		pNos->x.iVal = r;` |
|        5 |  4138 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4139 | `	}` |
|        5 |  4140 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4141 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4142 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4143 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4144 | `	}` |
|        5 |  4145 | `	VmPopOperand(&pTos,1);` |
|        5 |  4146 | `	break;` |
|        - |  4147 | `				 }` |
|        - |  4148 |  |
|        - |  4149 | `/*` |
|        - |  4150 | ` * OP_MOD * * *` |
|        - |  4151 | ` *` |
|        - |  4152 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4153 | ` * first (what was next on the stack) from the second (the` |
|        - |  4154 | ` * top of the stack) and push the remainder after division` |
|        - |  4155 | ` * onto the stack.` |
|        - |  4156 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4157 | ` */` |
|      306 |  4158 | `case PH7_OP_MOD:{` |
|      614 |  4159 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4160 | `	sxi64 a,b,r;` |
|        - |  4161 | `#ifdef UNTRUST` |
|        - |  4162 | `	if( pNos < pStack ){` |
|        - |  4163 | `		goto Abort;` |
|        - |  4164 | `	}` |
|        - |  4165 | `#endif` |
|        - |  4166 | `	/* Force the operands to be integer */` |
|      614 |  4167 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4168 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4169 | `	}` |
|      614 |  4170 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4171 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4172 | `	}` |
|        - |  4173 | `	/* Perform the requested operation */` |
|      614 |  4174 | `	a = pNos->x.iVal;` |
|      614 |  4175 | `	b = pTos->x.iVal;` |
|      614 |  4176 | `	if( b == 0 ){` |
|        3 |  4177 | `		r = 0;` |
|        3 |  4178 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4179 | `		/* goto Abort; */` |
|        2 |  4180 | `	}else{` |
|      611 |  4181 | `		r = a%b;` |
|        - |  4182 | `	}` |
|        - |  4183 | `	/* Push the result */` |
|      614 |  4184 | `	pNos->x.iVal = r;` |
|      614 |  4185 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      614 |  4186 | `	VmPopOperand(&pTos,1);` |
|      614 |  4187 | `	break;` |
|        - |  4188 | `				}` |
|        - |  4189 | `/*` |
|        - |  4190 | ` * OP_MOD_STORE * * *` |
|        - |  4191 | ` *` |
|        - |  4192 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4193 | ` * first (what was next on the stack) from the second (the` |
|        - |  4194 | ` * top of the stack) and push the remainder after division` |
|        - |  4195 | ` * onto the stack.` |
|        - |  4196 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4197 | ` */` |
|        1 |  4198 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4199 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4200 | `	ph7_value *pObj;` |
|        - |  4201 | `	sxi64 a,b,r;` |
|        - |  4202 | `#ifdef UNTRUST` |
|        - |  4203 | `	if( pNos < pStack ){` |
|        - |  4204 | `		goto Abort;` |
|        - |  4205 | `	}` |
|        - |  4206 | `#endif` |
|        - |  4207 | `	/* Force the operands to be integer */` |
|        3 |  4208 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4209 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4210 | `	}` |
|        3 |  4211 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4212 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4213 | `	}` |
|        - |  4214 | `	/* Perform the requested operation */` |
|        3 |  4215 | `	a = pTos->x.iVal;` |
|        3 |  4216 | `	b = pNos->x.iVal;` |
|        3 |  4217 | `	if( b == 0 ){` |
|      ! 0 |  4218 | `		r = 0;` |
|      ! 0 |  4219 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4220 | `		/* goto Abort; */` |
|      ! 0 |  4221 | `	}else{` |
|        3 |  4222 | `		r = a%b;` |
|        - |  4223 | `	}` |
|        - |  4224 | `	/* Push the result */` |
|        3 |  4225 | `	pNos->x.iVal = r;` |
|        3 |  4226 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4227 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4228 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4229 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4230 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4231 | `	}` |
|        3 |  4232 | `	VmPopOperand(&pTos,1);` |
|        3 |  4233 | `	break;` |
|        - |  4234 | `				}` |
|        - |  4235 | `/*` |
|        - |  4236 | ` * OP_DIV * * *` |
|        - |  4237 | ` *` |
|        - |  4238 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4239 | ` * first (what was next on the stack) from the second (the` |
|        - |  4240 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4241 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4242 | ` */` |
|       29 |  4243 | `case PH7_OP_DIV:{` |
|       60 |  4244 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4245 | `	ph7_real a,b,r;` |
|        - |  4246 | `#ifdef UNTRUST` |
|        - |  4247 | `	if( pNos < pStack ){` |
|        - |  4248 | `		goto Abort;` |
|        - |  4249 | `	}` |
|        - |  4250 | `#endif` |
|        - |  4251 | `	/* Force the operands to be real */` |
|       60 |  4252 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 |  4253 | `		PH7_MemObjToReal(pTos);` |
|       27 |  4254 | `	}` |
|       60 |  4255 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       22 |  4256 | `		PH7_MemObjToReal(pNos);` |
|       10 |  4257 | `	}` |
|        - |  4258 | `	/* Perform the requested operation */` |
|       60 |  4259 | `	a = pNos->rVal;` |
|       60 |  4260 | `	b = pTos->rVal;` |
|       60 |  4261 | `	if( b == 0 ){` |
|        - |  4262 | `		/* Division by zero */` |
|        3 |  4263 | `		pNos->rVal = 0;` |
|        3 |  4264 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4265 | `		/* goto Abort; */` |
|        2 |  4266 | `	}else{` |
|       57 |  4267 | `		r = a/b;` |
|        - |  4268 | `		/* Push the result */` |
|       57 |  4269 | `		pNos->rVal = r;` |
|       57 |  4270 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4271 | `		/* Try to get an integer representation */` |
|       57 |  4272 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4273 | `	}` |
|       60 |  4274 | `	VmPopOperand(&pTos,1);` |
|       60 |  4275 | `	break;` |
|        - |  4276 | `				}` |
|        - |  4277 | `/*` |
|        - |  4278 | ` * OP_DIV_STORE * * *` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4281 | ` * first (what was next on the stack) from the second (the` |
|        - |  4282 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4283 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4284 | ` */` |
|        1 |  4285 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4286 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4287 | `	ph7_value *pObj;` |
|        - |  4288 | `	ph7_real a,b,r;` |
|        - |  4289 | `#ifdef UNTRUST` |
|        - |  4290 | `	if( pNos < pStack ){` |
|        - |  4291 | `		goto Abort;` |
|        - |  4292 | `	}` |
|        - |  4293 | `#endif` |
|        - |  4294 | `	/* Force the operands to be real */` |
|        3 |  4295 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4296 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4297 | `	}` |
|        3 |  4298 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4299 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4300 | `	}` |
|        - |  4301 | `	/* Perform the requested operation */` |
|        3 |  4302 | `	a = pTos->rVal;` |
|        3 |  4303 | `	b = pNos->rVal;` |
|        3 |  4304 | `	if( b == 0 ){` |
|        - |  4305 | `		/* Division by zero */` |
|      ! 0 |  4306 | `		r = 0;` |
|      ! 0 |  4307 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4308 | `		/* goto Abort; */` |
|      ! 0 |  4309 | `	}else{` |
|        3 |  4310 | `		r = a/b;` |
|        - |  4311 | `		/* Push the result */` |
|        3 |  4312 | `		pNos->rVal = r;` |
|        3 |  4313 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4314 | `		/* Try to get an integer representation */` |
|        3 |  4315 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4316 | `	}` |
|        3 |  4317 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4318 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4319 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4320 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4321 | `	}` |
|        3 |  4322 | `	VmPopOperand(&pTos,1);` |
|        3 |  4323 | `	break;` |
|        - |  4324 | `				}` |
|        - |  4325 | `/* OP_BAND * * *` |
|        - |  4326 | ` *` |
|        - |  4327 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4328 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4329 | ` * two elements.` |
|        - |  4330 | `*/` |
|        - |  4331 | `/* OP_BOR * * *` |
|        - |  4332 | ` *` |
|        - |  4333 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4334 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4335 | ` * two elements.` |
|        - |  4336 | ` */` |
|        - |  4337 | `/* OP_BXOR * * *` |
|        - |  4338 | ` *` |
|        - |  4339 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4340 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4341 | ` * two elements.` |
|        - |  4342 | ` */` |
|       44 |  4343 | `case PH7_OP_BAND:` |
|        - |  4344 | `case PH7_OP_BOR:` |
|        - |  4345 | `case PH7_OP_BXOR:{` |
|       90 |  4346 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4347 | `	sxi64 a,b,r;` |
|        - |  4348 | `#ifdef UNTRUST` |
|        - |  4349 | `	if( pNos < pStack ){` |
|        - |  4350 | `		goto Abort;` |
|        - |  4351 | `	}` |
|        - |  4352 | `#endif` |
|        - |  4353 | `	/* Force the operands to be integer */` |
|       90 |  4354 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4355 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4356 | `	}` |
|       90 |  4357 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4358 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4359 | `	}` |
|        - |  4360 | `	/* Perform the requested operation */` |
|       90 |  4361 | `	a = pNos->x.iVal;` |
|       90 |  4362 | `	b = pTos->x.iVal;` |
|       90 |  4363 | `	switch(pInstr->iOp){` |
|        7 |  4364 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4365 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4366 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4367 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4368 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4369 | `	case PH7_OP_BAND:` |
|       62 |  4370 | `	default:          r = a&b; break;` |
|        - |  4371 | `	}` |
|        - |  4372 | `	/* Push the result */` |
|       90 |  4373 | `	pNos->x.iVal = r;` |
|       90 |  4374 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4375 | `	VmPopOperand(&pTos,1);` |
|       90 |  4376 | `	break;` |
|        - |  4377 | `				 }` |
|        - |  4378 | `/* OP_BAND_STORE * * *` |
|        - |  4379 | ` *` |
|        - |  4380 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4381 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4382 | ` * two elements.` |
|        - |  4383 | `*/` |
|        - |  4384 | `/* OP_BOR_STORE * * *` |
|        - |  4385 | ` *` |
|        - |  4386 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4387 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4388 | ` * two elements.` |
|        - |  4389 | ` */` |
|        - |  4390 | `/* OP_BXOR_STORE * * *` |
|        - |  4391 | ` *` |
|        - |  4392 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4393 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4394 | ` * two elements.` |
|        - |  4395 | ` */` |
|       10 |  4396 | `case PH7_OP_BAND_STORE:` |
|        - |  4397 | `case PH7_OP_BOR_STORE:` |
|        - |  4398 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4399 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4400 | `	ph7_value *pObj;` |
|        - |  4401 | `	sxi64 a,b,r;` |
|        - |  4402 | `#ifdef UNTRUST` |
|        - |  4403 | `	if( pNos < pStack ){` |
|        - |  4404 | `		goto Abort;` |
|        - |  4405 | `	}` |
|        - |  4406 | `#endif` |
|        - |  4407 | `	/* Force the operands to be integer */` |
|       21 |  4408 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4409 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4410 | `	}` |
|       21 |  4411 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4412 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4413 | `	}` |
|        - |  4414 | `	/* Perform the requested operation */` |
|       21 |  4415 | `	a = pTos->x.iVal;` |
|       21 |  4416 | `	b = pNos->x.iVal;` |
|       21 |  4417 | `	switch(pInstr->iOp){` |
|        3 |  4418 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4419 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4420 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4421 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4422 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4423 | `	case PH7_OP_BAND:` |
|        7 |  4424 | `	default:          r = a&b; break;` |
|        - |  4425 | `	}` |
|        - |  4426 | `	/* Push the result */` |
|       21 |  4427 | `	pNos->x.iVal = r;` |
|       21 |  4428 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4429 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4430 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4431 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4432 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4433 | `	}` |
|       21 |  4434 | `	VmPopOperand(&pTos,1);` |
|       21 |  4435 | `	break;` |
|        - |  4436 | `				 }` |
|        - |  4437 | `/* OP_SHL * * *` |
|        - |  4438 | ` *` |
|        - |  4439 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4440 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4441 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4442 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4443 | ` */` |
|        - |  4444 | `/* OP_SHR * * *` |
|        - |  4445 | ` *` |
|        - |  4446 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4447 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4448 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4449 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4450 | ` */` |
|       12 |  4451 | `case PH7_OP_SHL:` |
|        - |  4452 | `case PH7_OP_SHR: {` |
|       25 |  4453 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4454 | `	sxi64 a,r;` |
|        - |  4455 | `	sxi32 b;` |
|        - |  4456 | `#ifdef UNTRUST` |
|        - |  4457 | `	if( pNos < pStack ){` |
|        - |  4458 | `		goto Abort;` |
|        - |  4459 | `	}` |
|        - |  4460 | `#endif` |
|        - |  4461 | `	/* Force the operands to be integer */` |
|       25 |  4462 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4463 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4464 | `	}` |
|       25 |  4465 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4466 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4467 | `	}` |
|        - |  4468 | `	/* Perform the requested operation */` |
|       25 |  4469 | `	a = pNos->x.iVal;` |
|       25 |  4470 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4471 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4472 | `		r = a << b;` |
|        8 |  4473 | `	}else{` |
|       11 |  4474 | `		r = a >> b;` |
|        - |  4475 | `	}` |
|        - |  4476 | `	/* Push the result */` |
|       25 |  4477 | `	pNos->x.iVal = r;` |
|       25 |  4478 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4479 | `	VmPopOperand(&pTos,1);` |
|       25 |  4480 | `	break;` |
|        - |  4481 | `				 }` |
|        - |  4482 | `/*  OP_SHL_STORE * * *` |
|        - |  4483 | ` *` |
|        - |  4484 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4485 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4486 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4487 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4488 | ` */` |
|        - |  4489 | `/* OP_SHR_STORE * * *` |
|        - |  4490 | ` *` |
|        - |  4491 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4492 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4493 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4494 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4495 | ` */` |
|        9 |  4496 | `case PH7_OP_SHL_STORE:` |
|        - |  4497 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4498 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4499 | `	ph7_value *pObj;` |
|        - |  4500 | `	sxi64 a,r;` |
|        - |  4501 | `	sxi32 b;` |
|        - |  4502 | `#ifdef UNTRUST` |
|        - |  4503 | `	if( pNos < pStack ){` |
|        - |  4504 | `		goto Abort;` |
|        - |  4505 | `	}` |
|        - |  4506 | `#endif` |
|        - |  4507 | `	/* Force the operands to be integer */` |
|       19 |  4508 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4509 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4510 | `	}` |
|       19 |  4511 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4512 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4513 | `	}` |
|        - |  4514 | `	/* Perform the requested operation */` |
|       19 |  4515 | `	a = pTos->x.iVal;` |
|       19 |  4516 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4517 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4518 | `		r = a << b;` |
|        5 |  4519 | `	}else{` |
|       11 |  4520 | `		r = a >> b;` |
|        - |  4521 | `	}` |
|        - |  4522 | `	/* Push the result */` |
|       19 |  4523 | `	pNos->x.iVal = r;` |
|       19 |  4524 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4525 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4526 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4527 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4528 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4529 | `	}` |
|       19 |  4530 | `	VmPopOperand(&pTos,1);` |
|       19 |  4531 | `	break;` |
|        - |  4532 | `				 }` |
|        - |  4533 | `/* CAT:  P1 * *` |
|        - |  4534 | ` *` |
|        - |  4535 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4536 | ` * back.` |
|        - |  4537 | ` */` |
|    63848 |  4538 | `case PH7_OP_CAT:{` |
|        - |  4539 | `	ph7_value *pNos,*pCur;` |
|   127698 |  4540 | `	if( pInstr->iP1 < 1 ){` |
|   100646 |  4541 | `		pNos = &pTos[-1];` |
|    50324 |  4542 | `	}else{` |
|    27054 |  4543 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4544 | `	}` |
|        - |  4545 | `#ifdef UNTRUST` |
|        - |  4546 | `	if( pNos < pStack ){` |
|        - |  4547 | `		goto Abort;` |
|        - |  4548 | `	}` |
|        - |  4549 | `#endif` |
|        - |  4550 | `	/* Force a string cast */` |
|   127698 |  4551 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1362 |  4552 | `		PH7_MemObjToString(pNos);` |
|      680 |  4553 | `	}` |
|   127698 |  4554 | `	pCur = &pNos[1];` |
|   257446 |  4555 | `	while( pCur <= pTos ){` |
|   129750 |  4556 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50686 |  4557 | `			PH7_MemObjToString(pCur);` |
|    25342 |  4558 | `		}` |
|        - |  4559 | `		/* Perform the concatenation */` |
|   129750 |  4560 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   129712 |  4561 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64855 |  4562 | `		}` |
|   129750 |  4563 | `		SyBlobRelease(&pCur->sBlob);` |
|   129750 |  4564 | `		pCur++;` |
|        2 |  4565 | `	}` |
|   127698 |  4566 | `	pTos = pNos;` |
|   127698 |  4567 | `	break;` |
|        - |  4568 | `				}` |
|        - |  4569 | `/*  CAT_STORE: * * *` |
|        - |  4570 | ` *` |
|        - |  4571 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4572 | ` * back.` |
|        - |  4573 | ` */` |
|     3414 |  4574 | `case PH7_OP_CAT_STORE:{` |
|     6830 |  4575 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4576 | `	ph7_value *pObj;` |
|        - |  4577 | `#ifdef UNTRUST` |
|        - |  4578 | `	if( pNos < pStack ){` |
|        - |  4579 | `		goto Abort;` |
|        - |  4580 | `	}` |
|        - |  4581 | `#endif` |
|     6830 |  4582 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4583 | `		/* Force a string cast */` |
|      ! 0 |  4584 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4585 | `	}` |
|     6830 |  4586 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4587 | `		/* Force a string cast */` |
|      ! 0 |  4588 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4589 | `	}` |
|        - |  4590 | `	/* Perform the concatenation (Reverse order) */` |
|     6830 |  4591 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6830 |  4592 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3414 |  4593 | `	}` |
|        - |  4594 | `	/* Perform the store operation */` |
|     6830 |  4595 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4596 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6830 |  4597 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6830 |  4598 | `		PH7_MemObjStore(pTos,pObj);` |
|     3414 |  4599 | `	}` |
|     6830 |  4600 | `	PH7_MemObjStore(pTos,pNos);` |
|     6830 |  4601 | `	VmPopOperand(&pTos,1);` |
|     6830 |  4602 | `	break;` |
|        - |  4603 | `				}` |
|        - |  4604 | `/* OP_AND: * * *` |
|        - |  4605 | ` *` |
|        - |  4606 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4607 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4608 | ` * stack.` |
|        - |  4609 | ` */` |
|        - |  4610 | `/* OP_OR: * * *` |
|        - |  4611 | ` *` |
|        - |  4612 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4613 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4614 | ` * stack.` |
|        - |  4615 | ` */` |
|    95760 |  4616 | `case PH7_OP_LAND:` |
|        - |  4617 | `case PH7_OP_LOR: {` |
|   191566 |  4618 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4619 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4620 | `#ifdef UNTRUST` |
|        - |  4621 | `	if( pNos < pStack ){` |
|        - |  4622 | `		goto Abort;` |
|        - |  4623 | `	}` |
|        - |  4624 | `#endif` |
|        - |  4625 | `	/* Force a boolean cast */` |
|   191566 |  4626 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4627 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4628 | `	}` |
|   191566 |  4629 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4630 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4631 | `	}` |
|   191566 |  4632 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   191566 |  4633 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   191566 |  4634 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4635 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    88304 |  4636 | `		v1 = and_logic[v1*3+v2];` |
|    44175 |  4637 | `	}else{` |
|        - |  4638 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   103264 |  4639 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4640 | `	}` |
|   191566 |  4641 | `	if( v1 == 2 ){` |
|      ! 0 |  4642 | `		v1 = 1;` |
|      ! 0 |  4643 | `	}` |
|   191566 |  4644 | `	VmPopOperand(&pTos,1);` |
|   191566 |  4645 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   191566 |  4646 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   191566 |  4647 | `	break;` |
|        - |  4648 | `				 }` |
|        - |  4649 | `/*` |
|        - |  4650 | ` * OP_NULLC: * * *` |
|        - |  4651 | ` * Null coalescing operator '??'.` |
|        - |  4652 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4653 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4654 | ` */` |
|        - |  4655 | `/*` |
|        - |  4656 | ` * OP_NULLC: * P2 *` |
|        - |  4657 | ` * Short-circuit null coalescing '??'.` |
|        - |  4658 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4659 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4660 | ` */` |
|       19 |  4661 | `case PH7_OP_NULLC: {` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pTos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|       40 |  4667 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4668 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4669 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4670 | `	}else{` |
|        - |  4671 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4672 | `		VmPopOperand(&pTos, 1);` |
|        - |  4673 | `	}` |
|       40 |  4674 | `	break;` |
|        - |  4675 |  |
|        - |  4676 | `/*` |
|        - |  4677 | ` * OP_SPREAD: * * *` |
|        - |  4678 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4679 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4680 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4681 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4682 | ` */` |
|        7 |  4683 | `case PH7_OP_SPREAD: {` |
|        - |  4684 | `#ifdef UNTRUST` |
|        - |  4685 | `	if( pTos < pStack ){` |
|        - |  4686 | `		goto Abort;` |
|        - |  4687 | `	}` |
|        - |  4688 | `#endif` |
|       15 |  4689 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4690 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4691 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4692 | `		if( nEntry == 0 ){` |
|        - |  4693 | `			/* Empty array — remove from stack */` |
|        3 |  4694 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4695 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4696 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4697 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4698 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4699 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4700 | `				VM_STACK_GUARD);` |
|      ! 0 |  4701 | `		}else{` |
|        - |  4702 | `			ph7_hashmap_node *pNode2;` |
|        - |  4703 | `			ph7_value *pElem;` |
|        - |  4704 | `			sxu32 i;` |
|        - |  4705 | `			/* Overwrite TOS with first element */` |
|       13 |  4706 | `			pNode2 = pMap->pFirst;` |
|       13 |  4707 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4708 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4709 | `			if( pElem ){` |
|       13 |  4710 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4711 | `			}` |
|       13 |  4712 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4713 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4714 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4715 | `			pNode2 = pNode2->pPrev;` |
|        - |  4716 | `			/* Push remaining elements */` |
|       33 |  4717 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4718 | `				pTos++;` |
|       21 |  4719 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4720 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4721 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4722 | `				if( pElem ){` |
|       21 |  4723 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4724 | `				}` |
|       21 |  4725 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4726 | `			}` |
|       13 |  4727 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4728 | `		}` |
|        7 |  4729 | `	}` |
|        - |  4730 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4731 | `	break;` |
|        - |  4732 |  |
|        - |  4733 | `/* OP_LXOR: * * *` |
|        - |  4734 | ` *` |
|        - |  4735 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4736 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4737 | ` * stack.` |
|        - |  4738 | ` * According to the PHP language reference manual:` |
|        - |  4739 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4740 | ` *  TRUE,but not both.` |
|        - |  4741 | ` */` |
|        5 |  4742 | `case PH7_OP_LXOR:{` |
|       11 |  4743 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4744 | `	sxi32 v = 0;` |
|        - |  4745 | `#ifdef UNTRUST` |
|        - |  4746 | `	if( pNos < pStack ){` |
|        - |  4747 | `		goto Abort;` |
|        - |  4748 | `	}` |
|        - |  4749 | `#endif` |
|        - |  4750 | `	/* Force a boolean cast */` |
|       11 |  4751 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4752 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4753 | `	}` |
|       11 |  4754 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4755 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4756 | `	}` |
|       11 |  4757 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4758 | `		v = 1;` |
|        3 |  4759 | `	}` |
|       11 |  4760 | `	VmPopOperand(&pTos,1);` |
|       11 |  4761 | `	pTos->x.iVal = v;` |
|       11 |  4762 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4763 | `	break;` |
|        - |  4764 | `				 }` |
|        - |  4765 | `/* OP_EQ P1 P2 P3` |
|        - |  4766 | ` *` |
|        - |  4767 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4768 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4769 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4770 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4771 | ` */` |
|        - |  4772 | `/* OP_NEQ P1 P2 P3` |
|        - |  4773 | ` *` |
|        - |  4774 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4775 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4776 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4777 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4778 | ` */` |
|     4025 |  4779 | `case PH7_OP_EQ:` |
|        - |  4780 | `case PH7_OP_NEQ: {` |
|     8052 |  4781 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4782 | `	/* Perform the comparison and act accordingly */` |
|        - |  4783 | `#ifdef UNTRUST` |
|        - |  4784 | `	if( pNos < pStack ){` |
|        - |  4785 | `		goto Abort;` |
|        - |  4786 | `	}` |
|        - |  4787 | `#endif` |
|     8052 |  4788 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8052 |  4789 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4790 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8043 |  4791 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8008 |  4792 | `		rc = rc == 0;` |
|     4005 |  4793 | `	}else{` |
|       28 |  4794 | `		rc = rc != 0;` |
|        - |  4795 | `	}` |
|     8052 |  4796 | `	VmPopOperand(&pTos,1);` |
|     8052 |  4797 | `	if( !pInstr->iP2 ){` |
|        - |  4798 | `		/* Push comparison result without taking the jump */` |
|     8052 |  4799 | `		PH7_MemObjRelease(pTos);` |
|     8052 |  4800 | `		pTos->x.iVal = rc;` |
|        - |  4801 | `		/* Invalidate any prior representation */` |
|     8052 |  4802 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4027 |  4803 | `	}else{` |
|      ! 0 |  4804 | `		if( rc ){` |
|        - |  4805 | `			/* Jump to the desired location */` |
|      ! 0 |  4806 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4807 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4808 | `		}` |
|        - |  4809 | `	}` |
|     8052 |  4810 | `	break;` |
|        - |  4811 | `				 }` |
|        - |  4812 | `/* OP_TEQ P1 P2 *` |
|        - |  4813 | ` *` |
|        - |  4814 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4815 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4816 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4817 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4818 | ` */` |
|   134939 |  4819 | `case PH7_OP_TEQ: {` |
|   269880 |  4820 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4821 | `	/* Perform the comparison and act accordingly */` |
|        - |  4822 | `#ifdef UNTRUST` |
|        - |  4823 | `	if( pNos < pStack ){` |
|        - |  4824 | `		goto Abort;` |
|        - |  4825 | `	}` |
|        - |  4826 | `#endif` |
|   269880 |  4827 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   269880 |  4828 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4829 | `		rc = 0;` |
|        2 |  4830 | `	}else{` |
|   269878 |  4831 | `		rc = rc == 0;` |
|        - |  4832 | `	}` |
|   269880 |  4833 | `	VmPopOperand(&pTos,1);` |
|   269880 |  4834 | `	if( !pInstr->iP2 ){` |
|        - |  4835 | `		/* Push comparison result without taking the jump */` |
|   269880 |  4836 | `		PH7_MemObjRelease(pTos);` |
|   269880 |  4837 | `		pTos->x.iVal = rc;` |
|        - |  4838 | `		/* Invalidate any prior representation */` |
|   269880 |  4839 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   134941 |  4840 | `	}else{` |
|      ! 0 |  4841 | `		if( rc ){` |
|        - |  4842 | `			/* Jump to the desired location */` |
|      ! 0 |  4843 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4844 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4845 | `		}` |
|        - |  4846 | `	}` |
|   269880 |  4847 | `	break;` |
|        - |  4848 | `				 }` |
|        - |  4849 | `/* OP_TNE P1 P2 *` |
|        - |  4850 | ` *` |
|        - |  4851 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4852 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4853 | ` * instruction.` |
|        - |  4854 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4855 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4856 | ` *` |
|        - |  4857 | ` */` |
|   105241 |  4858 | `case PH7_OP_TNE: {` |
|   210484 |  4859 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4860 | `	/* Perform the comparison and act accordingly */` |
|        - |  4861 | `#ifdef UNTRUST` |
|        - |  4862 | `	if( pNos < pStack ){` |
|        - |  4863 | `		goto Abort;` |
|        - |  4864 | `	}` |
|        - |  4865 | `#endif` |
|   210484 |  4866 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   210484 |  4867 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4868 | `		rc = 1;` |
|        2 |  4869 | `	}else{` |
|   210482 |  4870 | `		rc = rc != 0;` |
|        - |  4871 | `	}` |
|   210484 |  4872 | `	VmPopOperand(&pTos,1);` |
|   210484 |  4873 | `	if( !pInstr->iP2 ){` |
|        - |  4874 | `		/* Push comparison result without taking the jump */` |
|   210484 |  4875 | `		PH7_MemObjRelease(pTos);` |
|   210484 |  4876 | `		pTos->x.iVal = rc;` |
|        - |  4877 | `		/* Invalidate any prior representation */` |
|   210484 |  4878 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   105243 |  4879 | `	}else{` |
|      ! 0 |  4880 | `		if( rc ){` |
|        - |  4881 | `			/* Jump to the desired location */` |
|      ! 0 |  4882 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4883 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4884 | `		}` |
|        - |  4885 | `	}` |
|   210484 |  4886 | `	break;` |
|        - |  4887 | `				 }` |
|        - |  4888 | `/* OP_LT P1 P2 P3` |
|        - |  4889 | ` *` |
|        - |  4890 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4891 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4892 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4893 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4894 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4895 | ` *` |
|        - |  4896 | ` */` |
|        - |  4897 | `/* OP_LE P1 P2 P3` |
|        - |  4898 | ` *` |
|        - |  4899 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4900 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4901 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4902 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4903 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4904 | ` *` |
|        - |  4905 | ` */` |
|   103130 |  4906 | `case PH7_OP_LT:` |
|        - |  4907 | `case PH7_OP_LE: {` |
|   206306 |  4908 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4909 | `	/* Perform the comparison and act accordingly */` |
|        - |  4910 | `#ifdef UNTRUST` |
|        - |  4911 | `	if( pNos < pStack ){` |
|        - |  4912 | `		goto Abort;` |
|        - |  4913 | `	}` |
|        - |  4914 | `#endif` |
|   206306 |  4915 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   206306 |  4916 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4917 | `		rc = 0;` |
|   206302 |  4918 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      432 |  4919 | `		rc = rc < 1;` |
|      217 |  4920 | `	}else{` |
|   205868 |  4921 | `		rc = rc < 0;` |
|        - |  4922 | `	}` |
|   206306 |  4923 | `	VmPopOperand(&pTos,1);` |
|   206306 |  4924 | `	if( !pInstr->iP2 ){` |
|        - |  4925 | `		/* Push comparison result without taking the jump */` |
|   206306 |  4926 | `		PH7_MemObjRelease(pTos);` |
|   206306 |  4927 | `		pTos->x.iVal = rc;` |
|        - |  4928 | `		/* Invalidate any prior representation */` |
|   206306 |  4929 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103176 |  4930 | `	}else{` |
|      ! 0 |  4931 | `		if( rc ){` |
|        - |  4932 | `			/* Jump to the desired location */` |
|      ! 0 |  4933 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4934 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4935 | `		}` |
|        - |  4936 | `	}` |
|   206306 |  4937 | `	break;` |
|        - |  4938 | `				}` |
|        - |  4939 | `/* OP_GT P1 P2 P3` |
|        - |  4940 | ` *` |
|        - |  4941 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4942 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4943 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4944 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4945 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4946 | ` *` |
|        - |  4947 | ` */` |
|        - |  4948 | `/* OP_GE P1 P2 P3` |
|        - |  4949 | ` *` |
|        - |  4950 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4951 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4952 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4953 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4954 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4955 | ` *` |
|        - |  4956 | ` */` |
|    49133 |  4957 | `case PH7_OP_GT:` |
|        - |  4958 | `case PH7_OP_GE: {` |
|    98268 |  4959 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4960 | `	/* Perform the comparison and act accordingly */` |
|        - |  4961 | `#ifdef UNTRUST` |
|        - |  4962 | `	if( pNos < pStack ){` |
|        - |  4963 | `		goto Abort;` |
|        - |  4964 | `	}` |
|        - |  4965 | `#endif` |
|    98268 |  4966 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    98268 |  4967 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4968 | `		rc = 0;` |
|    98264 |  4969 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    98110 |  4970 | `		rc = rc >= 0;` |
|    49056 |  4971 | `	}else{` |
|      152 |  4972 | `		rc = rc > 0;` |
|        - |  4973 | `	}` |
|    98268 |  4974 | `	VmPopOperand(&pTos,1);` |
|    98268 |  4975 | `	if( !pInstr->iP2 ){` |
|        - |  4976 | `		/* Push comparison result without taking the jump */` |
|    98268 |  4977 | `		PH7_MemObjRelease(pTos);` |
|    98268 |  4978 | `		pTos->x.iVal = rc;` |
|        - |  4979 | `		/* Invalidate any prior representation */` |
|    98268 |  4980 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    49135 |  4981 | `	}else{` |
|      ! 0 |  4982 | `		if( rc ){` |
|        - |  4983 | `			/* Jump to the desired location */` |
|      ! 0 |  4984 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4985 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4986 | `		}` |
|        - |  4987 | `	}` |
|    98268 |  4988 | `	break;` |
|        - |  4989 | `				}` |
|        - |  4990 | `/* OP_SPACESHIP * * *` |
|        - |  4991 | ` *` |
|        - |  4992 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  4993 | ` *   -1 if left < right` |
|        - |  4994 | ` *    0 if left == right` |
|        - |  4995 | ` *    1 if left > right` |
|        - |  4996 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  4997 | ` */` |
|       25 |  4998 | `case PH7_OP_SPACESHIP: {` |
|       51 |  4999 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5000 | `#ifdef UNTRUST` |
|        - |  5001 | `	if( pNos < pStack ){` |
|        - |  5002 | `		goto Abort;` |
|        - |  5003 | `	}` |
|        - |  5004 | `#endif` |
|       51 |  5005 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5006 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5007 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5008 | `		rc = 1;` |
|        4 |  5009 | `	}else{` |
|        - |  5010 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5011 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5012 | `	}` |
|       51 |  5013 | `	VmPopOperand(&pTos,1);` |
|       51 |  5014 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5015 | `	pTos->x.iVal = rc;` |
|       51 |  5016 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5017 | `	break;` |
|        - |  5018 | `				}` |
|        - |  5019 | `/* OP_SEQ P1 P2 *` |
|        - |  5020 | ` * Strict string comparison.` |
|        - |  5021 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5022 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5023 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5024 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5025 | ` * use PH7_OP_EQ.` |
|        - |  5026 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5027 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5028 | ` */` |
|        - |  5029 | `/* OP_SNE P1 P2 *` |
|        - |  5030 | ` * Strict string comparison.` |
|        - |  5031 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5032 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5033 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5034 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5035 | ` * use PH7_OP_EQ.` |
|        - |  5036 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5037 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5038 | ` */` |
|       18 |  5039 | `case PH7_OP_SEQ:` |
|        - |  5040 | `case PH7_OP_SNE: {` |
|       38 |  5041 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5042 | `	SyString s1,s2;` |
|        - |  5043 | `	/* Perform the comparison and act accordingly */` |
|        - |  5044 | `#ifdef UNTRUST` |
|        - |  5045 | `	if( pNos < pStack ){` |
|        - |  5046 | `		goto Abort;` |
|        - |  5047 | `	}` |
|        - |  5048 | `#endif` |
|        - |  5049 | `	/* Force a string cast */` |
|       38 |  5050 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5051 | `		PH7_MemObjToString(pTos);` |
|        2 |  5052 | `	}` |
|       38 |  5053 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5054 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5055 | `	}` |
|       38 |  5056 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5057 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5058 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5059 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5060 | `		rc = rc != 0;` |
|      ! 0 |  5061 | `	}else{` |
|       38 |  5062 | `		rc = rc == 0;` |
|        - |  5063 | `	}` |
|       38 |  5064 | `	VmPopOperand(&pTos,1);` |
|       38 |  5065 | `	if( !pInstr->iP2 ){` |
|        - |  5066 | `		/* Push comparison result without taking the jump */` |
|       38 |  5067 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5068 | `		pTos->x.iVal = rc;` |
|        - |  5069 | `		/* Invalidate any prior representation */` |
|       38 |  5070 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5071 | `	}else{` |
|      ! 0 |  5072 | `		if( rc ){` |
|        - |  5073 | `			/* Jump to the desired location */` |
|      ! 0 |  5074 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5075 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5076 | `		}` |
|        - |  5077 | `	}` |
|       38 |  5078 | `	break;` |
|        - |  5079 | `				 }` |
|        - |  5080 | `/*` |
|        - |  5081 | ` * OP_LOAD_REF * * *` |
|        - |  5082 | ` * Push the index of a referenced object on the stack.` |
|        - |  5083 | ` */` |
|       57 |  5084 | `case PH7_OP_LOAD_REF: {` |
|        - |  5085 | `	sxu32 nIdx;` |
|        - |  5086 | `#ifdef UNTRUST` |
|        - |  5087 | `	if( pTos < pStack ){` |
|        - |  5088 | `		goto Abort;` |
|        - |  5089 | `	}` |
|        - |  5090 | `#endif` |
|        - |  5091 | `	/* Extract memory object index */` |
|      115 |  5092 | `	nIdx = pTos->nIdx;` |
|      115 |  5093 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5094 | `		/* Nullify the object */` |
|       95 |  5095 | `		PH7_MemObjRelease(pTos);` |
|        - |  5096 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5097 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5098 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5099 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5100 | `	}` |
|      115 |  5101 | `	break;` |
|        - |  5102 | `					  }` |
|        - |  5103 | `/*` |
|        - |  5104 | ` * OP_STORE_REF * * P3` |
|        - |  5105 | ` * Perform an assignment operation by reference.` |
|        - |  5106 | ` */` |
|       15 |  5107 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5108 | `	 SyString sName = { 0 , 0 };` |
|        - |  5109 | `	 VmFrame *pFrameLocal;` |
|        - |  5110 | `	SyHashEntry *pEntry;` |
|        - |  5111 | `	sxu32 nIdx;` |
|        - |  5112 | `#ifdef UNTRUST` |
|        - |  5113 | `	if( pTos < pStack ){` |
|        - |  5114 | `		goto Abort;` |
|        - |  5115 | `	}` |
|        - |  5116 | `#endif` |
|       32 |  5117 | `	if( pInstr->p3 == 0 ){` |
|        - |  5118 | `		char *zName;` |
|        - |  5119 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5120 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5121 | `			/* Force a string cast */` |
|      ! 0 |  5122 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5123 | `		}` |
|      ! 0 |  5124 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5125 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5126 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5127 | `			if( zName ){` |
|      ! 0 |  5128 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5129 | `			}` |
|      ! 0 |  5130 | `		}` |
|      ! 0 |  5131 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5132 | `		pTos--;` |
|      ! 0 |  5133 | `	}else{` |
|       32 |  5134 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5135 | `	}` |
|       32 |  5136 | `	nIdx = pTos->nIdx;` |
|       32 |  5137 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5138 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5139 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5140 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5141 | `		}else{` |
|        - |  5142 | `			ph7_value *pObj;` |
|        - |  5143 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5144 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5145 | `			if( pObj == 0 ){` |
|      ! 0 |  5146 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5147 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5148 | `				goto Abort;` |
|        - |  5149 | `			}` |
|        - |  5150 | `			/* Perform the store operation */` |
|      ! 0 |  5151 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5152 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5153 | `		}` |
|       32 |  5154 | `	}else if( sName.nByte > 0){` |
|       32 |  5155 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5156 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5157 | `		}else{` |
|       32 |  5158 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5159 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5160 | `			/* Query the local frame */` |
|       32 |  5161 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5162 | `			if( pEntry ){` |
|      ! 0 |  5163 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5164 | `			}else{` |
|       32 |  5165 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5166 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5167 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5168 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5169 | `				}` |
|       32 |  5170 | `				if( rc == SXRET_OK ){` |
|       32 |  5171 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5172 | `				}` |
|        - |  5173 | `			}` |
|        - |  5174 | `		}` |
|       15 |  5175 | `	}` |
|       32 |  5176 | `	break;` |
|        - |  5177 | `				 }` |
|        - |  5178 | `/*` |
|        - |  5179 | ` * OP_UPLINK P1 * *` |
|        - |  5180 | ` * Link a variable to the top active VM frame.` |
|        - |  5181 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5182 | ` */` |
|       25 |  5183 | `case PH7_OP_UPLINK: {` |
|       52 |  5184 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5185 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5186 | `		SyString sName;` |
|        - |  5187 | `		/* Perform the link */` |
|      104 |  5188 | `		while( pLink <= pTos ){` |
|       54 |  5189 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5190 | `				/* Force a string cast */` |
|      ! 0 |  5191 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5192 | `			}` |
|       54 |  5193 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5194 | `			if( sName.nByte > 0 ){` |
|       54 |  5195 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5196 | `			}` |
|       54 |  5197 | `			pLink++;` |
|        2 |  5198 | `		}` |
|       25 |  5199 | `	}` |
|       52 |  5200 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5201 | `	break;` |
|        - |  5202 | `					}` |
|        - |  5203 | `/*` |
|        - |  5204 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5205 | ` * Push an exception in the corresponding container so that` |
|        - |  5206 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5207 | ` */` |
|       32 |  5208 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5209 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5210 | `	VmFrame *pFrameLocal;` |
|        - |  5211 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5212 | `	pException->iFinallyDone = 0;` |
|       66 |  5213 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5214 | `	/* Create the exception frame */` |
|       66 |  5215 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5216 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5217 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5218 | `		goto Abort;` |
|        - |  5219 | `	}` |
|        - |  5220 | `	/* Mark the special frame */` |
|       66 |  5221 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5222 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5223 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5224 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5225 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5226 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5227 | `	break;` |
|        - |  5228 | `							}` |
|        - |  5229 | `/*` |
|        - |  5230 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5231 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5232 | ` */` |
|       31 |  5233 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5234 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5235 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5236 | `		ph7_exception **apException;` |
|        - |  5237 | `		/* Pop the loaded exception */` |
|       28 |  5238 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5239 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5240 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5241 | `		}` |
|       13 |  5242 | `	}` |
|       64 |  5243 | `	pException->pFrame = 0;` |
|        - |  5244 | `	/* Leave the exception frame */` |
|       64 |  5245 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5246 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5247 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5248 | `		sxi32 rcFinally;` |
|       20 |  5249 | `		pException->iFinallyDone = 1;` |
|       20 |  5250 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5251 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5252 | `			goto Abort;` |
|        - |  5253 | `		}` |
|        9 |  5254 | `	}` |
|       64 |  5255 | `	break;` |
|        - |  5256 | `							}` |
|        - |  5257 |  |
|        - |  5258 | `/*` |
|        - |  5259 | ` * OP_THROW * P2 *` |
|        - |  5260 | ` * Throw an user exception.` |
|        - |  5261 | ` */` |
|       18 |  5262 | `case PH7_OP_THROW: {` |
|       38 |  5263 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5264 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5265 | `#ifdef UNTRUST` |
|        - |  5266 | `	if( pTos < pStack ){` |
|        - |  5267 | `		goto Abort;` |
|        - |  5268 | `	}` |
|        - |  5269 | `#endif` |
|       38 |  5270 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5271 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5272 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5273 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5274 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5275 | `		ph7_class *pException;` |
|        - |  5276 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5277 | `		 */` |
|       38 |  5278 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5279 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5280 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5281 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5282 | `			if( rc == SXERR_ABORT ){` |
|        - |  5283 | `				/* Abort processing immediately */` |
|      ! 0 |  5284 | `				goto Abort;` |
|        - |  5285 | `			}` |
|      ! 0 |  5286 | `		}else{` |
|        - |  5287 | `			/* Throw the exception */` |
|       38 |  5288 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5289 | `			if( rc == SXERR_ABORT ){` |
|        - |  5290 | `				/* Abort processing immediately */` |
|        9 |  5291 | `				goto Abort;` |
|        - |  5292 | `			}` |
|        - |  5293 | `		}` |
|       16 |  5294 | `	}else{` |
|        - |  5295 | `		/* Expecting a class instance */` |
|      ! 0 |  5296 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5297 | `		if( rc == SXERR_ABORT ){` |
|        - |  5298 | `			/* Abort processing immediately */` |
|      ! 0 |  5299 | `			goto Abort;` |
|        - |  5300 | `		}` |
|        - |  5301 | `	}` |
|        - |  5302 | `	/* Pop the top entry */` |
|       30 |  5303 | `	VmPopOperand(&pTos,1);` |
|        - |  5304 | `	/* Perform an unconditional jump */` |
|       30 |  5305 | `	pc = nJump - 1;` |
|       30 |  5306 | `	break;` |
|        - |  5307 | `				   }` |
|        - |  5308 | `/*` |
|        - |  5309 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5310 | ` * Prepare a foreach step.` |
|        - |  5311 | ` */` |
|     5121 |  5312 | `case PH7_OP_FOREACH_INIT: {` |
|    10244 |  5313 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5314 | `	void *pName;` |
|        - |  5315 | `#ifdef UNTRUST` |
|        - |  5316 | `	if( pTos < pStack ){` |
|        - |  5317 | `		goto Abort;` |
|        - |  5318 | `	}` |
|        - |  5319 | `#endif` |
|    10244 |  5320 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5321 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5322 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5323 | `			/* Force a string cast */` |
|      ! 0 |  5324 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5325 | `		}` |
|        - |  5326 | `		/* Duplicate name */` |
|      ! 0 |  5327 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5328 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5329 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5330 | `		}` |
|      ! 0 |  5331 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5332 | `	}` |
|    10244 |  5333 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5334 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5335 | `			/* Force a string cast */` |
|      ! 0 |  5336 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5337 | `		}` |
|        - |  5338 | `		/* Duplicate name */` |
|      ! 0 |  5339 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5340 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5341 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5342 | `		}` |
|      ! 0 |  5343 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5344 | `	}` |
|        - |  5345 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10244 |  5346 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5347 | `		/* Jump out of the loop */` |
|      ! 0 |  5348 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5349 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5350 | `		}` |
|      ! 0 |  5351 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5352 | `	}else{` |
|        - |  5353 | `		ph7_foreach_step *pStep;` |
|    10244 |  5354 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10244 |  5355 | `		if( pStep == 0 ){` |
|      ! 0 |  5356 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5357 | `			/* Jump out of the loop */` |
|      ! 0 |  5358 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5359 | `		}else{` |
|        - |  5360 | `			/* Zero the structure */` |
|    10244 |  5361 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5362 | `			/* Prepare the step */` |
|    10244 |  5363 | `			pStep->iFlags = pInfo->iFlags;` |
|    10244 |  5364 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5365 | `				ph7_hashmap *pMap;` |
|        - |  5366 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5367 | `				 * source array so mutations don't affect other sharers. */` |
|    10216 |  5368 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5369 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5370 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5371 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5372 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5373 | `						 * variable still points at the same hashmap as` |
|        - |  5374 | `						 * the stack value. */` |
|        9 |  5375 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5376 | `							pCur->iRef--;` |
|        9 |  5377 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5378 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5379 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5380 | `						}` |
|        4 |  5381 | `					}` |
|        4 |  5382 | `				}` |
|    10216 |  5383 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5384 | `				/* Reset the internal loop cursor */` |
|    10216 |  5385 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5386 | `				/* Mark the step */` |
|    10216 |  5387 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10216 |  5388 | `				pStep->xIter.pMap = pMap;` |
|    10216 |  5389 | `				pMap->iRef++;` |
|     5109 |  5390 | `			}else{` |
|       30 |  5391 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5392 | `				ph7_class *pIteratorClass;` |
|        - |  5393 | `				/* Check if the object implements Iterator */` |
|       30 |  5394 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5395 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5396 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5397 | `					ph7_class_method *pRewind;` |
|       20 |  5398 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5399 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5400 | `					pThis->iRef++;` |
|       20 |  5401 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5402 | `					if( pRewind ){` |
|       20 |  5403 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5404 | `					}` |
|       11 |  5405 | `				}else{` |
|        - |  5406 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5407 | `					ph7_class *pIterAggClass;` |
|       12 |  5408 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5409 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5410 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5411 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5412 | `						ph7_class_method *pGetIter;` |
|        3 |  5413 | `						int iterAggOk = 0;` |
|        3 |  5414 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5415 | `						if( pGetIter ){` |
|        - |  5416 | `							ph7_value sResult;` |
|        3 |  5417 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5418 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5419 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5420 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5421 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5422 | `									ph7_class_method *pRewind;` |
|        3 |  5423 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5424 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5425 | `									pIterObj->iRef++;` |
|        - |  5426 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5427 | `									pStep->pOwner = pThis;` |
|        3 |  5428 | `									pThis->iRef++;` |
|        3 |  5429 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5430 | `									if( pRewind ){` |
|        3 |  5431 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5432 | `									}` |
|        3 |  5433 | `									iterAggOk = 1;` |
|        1 |  5434 | `								}` |
|        1 |  5435 | `							}` |
|        3 |  5436 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5437 | `						}` |
|        3 |  5438 | `						if( !iterAggOk ){` |
|        - |  5439 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5440 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5441 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5442 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5443 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5444 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5445 | `						}` |
|        2 |  5446 | `					}else{` |
|        - |  5447 | `						/* Plain object iteration via hAttr */` |
|        9 |  5448 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5449 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5450 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5451 | `						pThis->iRef++;` |
|        - |  5452 | `					}` |
|        - |  5453 | `				}` |
|        - |  5454 | `			}` |
|        - |  5455 | `		}` |
|    10244 |  5456 | `		if( pStep ){` |
|    10244 |  5457 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5458 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5459 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5460 | `				/* Jump out of the loop */` |
|      ! 0 |  5461 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5462 | `			}` |
|     5121 |  5463 | `		}` |
|        - |  5464 | `	}` |
|    10244 |  5465 | `	VmPopOperand(&pTos,1);` |
|    10244 |  5466 | `	break;` |
|        - |  5467 | `						  }` |
|        - |  5468 | `/*` |
|        - |  5469 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5470 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5471 | ` */` |
|    82862 |  5472 | `case PH7_OP_FOREACH_STEP: {` |
|   165726 |  5473 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5474 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5475 | `	ph7_value *pValue;` |
|        - |  5476 | `	VmFrame *pFrameLocal;` |
|        - |  5477 | `	/* Peek the last step */` |
|   165726 |  5478 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   165726 |  5479 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   165726 |  5480 | `	pFrameLocal = pVm->pFrame;` |
|   165726 |  5481 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   165726 |  5482 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   165614 |  5483 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5484 | `		ph7_hashmap_node *pNode;` |
|        - |  5485 | `		/* Extract the current node value */` |
|   165614 |  5486 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   165614 |  5487 | `		if( pNode == 0 ){` |
|        - |  5488 | `			/* No more entry to process */` |
|    10214 |  5489 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10214 |  5490 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5491 | `				/* Break the reference with the last element */` |
|        7 |  5492 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5493 | `			}` |
|        - |  5494 | `			/* Automatically reset the loop cursor */` |
|    10214 |  5495 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5496 | `			/* Cleanup the mess left behind */` |
|    10214 |  5497 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10214 |  5498 | `			SySetPop(&pInfo->aStep);` |
|    10214 |  5499 | `			PH7_HashmapUnref(pMap);` |
|     5108 |  5500 | `		}else{` |
|   155402 |  5501 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5502 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5503 | `				if( pKey ){` |
|      416 |  5504 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5505 | `				}` |
|      207 |  5506 | `			}` |
|   155402 |  5507 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5508 | `				SyHashEntry *pEntry;` |
|        - |  5509 | `				/* Pass by reference */` |
|       23 |  5510 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5511 | `				if( pEntry ){` |
|       23 |  5512 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5513 | `				}else{` |
|      ! 0 |  5514 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5515 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5516 | `				}` |
|       12 |  5517 | `			}else{` |
|        - |  5518 | `				/* Make a copy of the entry value */` |
|   155380 |  5519 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   155380 |  5520 | `				if( pValue ){` |
|   155380 |  5521 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    77689 |  5522 | `				}` |
|        - |  5523 | `			}` |
|        2 |  5524 | `		}` |
|    82920 |  5525 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5526 | `		/* Iterator-based iteration.` |
|        - |  5527 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5528 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5529 | `		 */` |
|       90 |  5530 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5531 | `		ph7_class_method *pMethod;` |
|        - |  5532 | `		ph7_value sResult;` |
|       90 |  5533 | `		int isValid = 0;` |
|        - |  5534 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5535 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5536 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5537 | `		}else{` |
|       70 |  5538 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5539 | `			if( pMethod ){` |
|       70 |  5540 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5541 | `			}` |
|        - |  5542 | `		}` |
|        - |  5543 | `		/* Call valid() */` |
|       90 |  5544 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5545 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5546 | `		if( pMethod ){` |
|       90 |  5547 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5548 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5549 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5550 | `		}` |
|       90 |  5551 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5552 | `		if( !isValid ){` |
|        - |  5553 | `			/* Iterator exhausted */` |
|       20 |  5554 | `			pc = pInstr->iP2 - 1;` |
|        - |  5555 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5556 | `			if( pStep->pOwner ){` |
|        3 |  5557 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5558 | `			}` |
|       20 |  5559 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5560 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5561 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5562 | `		}else{` |
|        - |  5563 | `			/* Call current() to get value */` |
|       72 |  5564 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5565 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5566 | `			if( pMethod ){` |
|       72 |  5567 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5568 | `			}` |
|       72 |  5569 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5570 | `			if( pValue ){` |
|       72 |  5571 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5572 | `			}` |
|       72 |  5573 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5574 | `			/* Call key() if needed */` |
|       72 |  5575 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5576 | `				ph7_value sKey;` |
|       35 |  5577 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5578 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5579 | `				if( pMethod ){` |
|       35 |  5580 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5581 | `				}` |
|       35 |  5582 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5583 | `				if( pValue ){` |
|       35 |  5584 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5585 | `				}` |
|       35 |  5586 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5587 | `			}` |
|        - |  5588 | `		}` |
|       46 |  5589 | `	}else{` |
|       25 |  5590 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5591 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5592 | `		SyHashEntry *pEntry;` |
|        - |  5593 | `		/* Point to the next attribute */` |
|       29 |  5594 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5595 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5596 | `			/* Check access permission */` |
|       31 |  5597 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5598 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5599 | `					break; /* Access is granted */` |
|        - |  5600 | `			}` |
|        1 |  5601 | `		}` |
|       25 |  5602 | `		if( pEntry == 0 ){` |
|        - |  5603 | `			/* Clean up the mess left behind */` |
|        9 |  5604 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5605 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5606 | `				/* Break the reference with the last element */` |
|        3 |  5607 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5608 | `			}` |
|        9 |  5609 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5610 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5611 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5612 | `		}else{` |
|       17 |  5613 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5614 | `			ph7_value *pAttrValue;` |
|       17 |  5615 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5616 | `				/* Fill with the current attribute name */` |
|       17 |  5617 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5618 | `				if( pKey ){` |
|       17 |  5619 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5620 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5621 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5622 | `				}` |
|        8 |  5623 | `			}` |
|        - |  5624 | `			/* Extract attribute value */` |
|       17 |  5625 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5626 | `			if( pAttrValue ){` |
|       17 |  5627 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5628 | `					/* Pass by reference */` |
|        3 |  5629 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5630 | `					if( pEntry ){` |
|        3 |  5631 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5632 | `					}else{` |
|      ! 0 |  5633 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5634 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5635 | `					}` |
|        2 |  5636 | `				}else{` |
|        - |  5637 | `					/* Make a copy of the attribute value */` |
|       15 |  5638 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5639 | `					if( pValue ){` |
|       15 |  5640 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5641 | `					}` |
|        - |  5642 | `				}` |
|        8 |  5643 | `			}` |
|        - |  5644 | `		}` |
|        - |  5645 | `	}` |
|   165726 |  5646 | `	break;` |
|        - |  5647 | `						  }` |
|        - |  5648 | `/*` |
|        - |  5649 | ` * OP_MEMBER P1 P2` |
|        - |  5650 | ` * Load class attribute/method on the stack.` |
|        - |  5651 | ` */` |
|     2234 |  5652 | `case PH7_OP_MEMBER: {` |
|        - |  5653 | `	ph7_class_instance *pThis;` |
|        - |  5654 | `	ph7_value *pNos;` |
|        - |  5655 | `	SyString sName;` |
|     4470 |  5656 | `	if( !pInstr->iP1 ){` |
|     4328 |  5657 | `		pNos = &pTos[-1];` |
|        - |  5658 | `#ifdef UNTRUST` |
|        - |  5659 | `		if( pNos < pStack ){` |
|        - |  5660 | `			goto Abort;` |
|        - |  5661 | `		}` |
|        - |  5662 | `#endif` |
|     4328 |  5663 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5664 | `			ph7_class *pClass;` |
|        - |  5665 | `			/* Class already instantiated */` |
|     4328 |  5666 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5667 | `			/* Point to the instantiated class */` |
|     4328 |  5668 | `			pClass = pThis->pClass;` |
|        - |  5669 | `			/* Extract attribute name first */` |
|     4328 |  5670 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4328 |  5671 | `			if( pInstr->iP2 ){` |
|        - |  5672 | `				/* Method call */` |
|      436 |  5673 | `				ph7_class_method *pMeth = 0;` |
|      436 |  5674 | `				if( sName.nByte > 0 ){` |
|        - |  5675 | `					/* Extract the target method */` |
|      436 |  5676 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      217 |  5677 | `				}` |
|      436 |  5678 | `				if( pMeth == 0 ){` |
|      ! 0 |  5679 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5680 | `						&pClass->sName,&sName` |
|        - |  5681 | `						);` |
|        - |  5682 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5683 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5684 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5685 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5686 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5687 | `				}else{` |
|        - |  5688 | `					/* Push method name on the stack */` |
|      436 |  5689 | `					PH7_MemObjRelease(pTos);` |
|      436 |  5690 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      436 |  5691 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5692 | `				}` |
|      436 |  5693 | `				pTos->nIdx = SXU32_HIGH;` |
|      219 |  5694 | `			}else{` |
|        - |  5695 | `				/* Attribute access */` |
|     3894 |  5696 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5697 | `				SyHashEntry *pEntry;` |
|        - |  5698 | `				/* Extract the target attribute */` |
|     3894 |  5699 | `				if( sName.nByte > 0 ){` |
|     3894 |  5700 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3894 |  5701 | `					if( pEntry ){` |
|        - |  5702 | `						/* Point to the attribute value */` |
|     3892 |  5703 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1945 |  5704 | `					}` |
|     1946 |  5705 | `				}` |
|     3894 |  5706 | `				if( pObjAttr == 0 ){` |
|        - |  5707 | `					/* No such attribute,load null */` |
|        4 |  5708 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5709 | `						&pClass->sName,&sName);` |
|        - |  5710 | `					/* Call the __get magic method if available */` |
|        3 |  5711 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5712 | `				}` |
|     3894 |  5713 | `				VmPopOperand(&pTos,1);` |
|        - |  5714 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5715 | `				 * This is due to the following case:` |
|        - |  5716 | `				 *     (new TestClass())->foo;` |
|        - |  5717 | `				 */` |
|     3894 |  5718 | `				pThis->iRef++;` |
|     3894 |  5719 | `				PH7_MemObjRelease(pTos);` |
|     3894 |  5720 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3894 |  5721 | `				if( pObjAttr ){` |
|     3892 |  5722 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5723 | `					/* Check attribute access */` |
|     3892 |  5724 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5725 | `						/* Load attribute */` |
|     3892 |  5726 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3892 |  5727 | `						if( pValue ){` |
|     3892 |  5728 | `							if( pThis->iRef < 2 ){` |
|        - |  5729 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5730 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5731 | `								 */` |
|        3 |  5732 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5733 | `							}else{` |
|        - |  5734 | `								/* Simple load */` |
|     3890 |  5735 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5736 | `							}` |
|     3892 |  5737 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3890 |  5738 | `								if( pThis->iRef > 1 ){` |
|        - |  5739 | `									/* Load attribute index */` |
|     3888 |  5740 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1943 |  5741 | `								}` |
|     1944 |  5742 | `							}` |
|     1945 |  5743 | `						}` |
|     1945 |  5744 | `					}` |
|     1945 |  5745 | `				}` |
|        - |  5746 | `				/* Safely unreference the object */` |
|     3894 |  5747 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5748 | `			}` |
|     2165 |  5749 | `		}else{` |
|      ! 0 |  5750 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5751 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5752 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5753 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5754 | `		}` |
|     2165 |  5755 | `	}else{` |
|        - |  5756 | `		/* Static member access using class name */` |
|      144 |  5757 | `		pNos = pTos;` |
|      144 |  5758 | `		pThis = 0;` |
|      144 |  5759 | `		if( !pInstr->p3 ){` |
|      132 |  5760 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      132 |  5761 | `			pNos--;` |
|        - |  5762 | `#ifdef UNTRUST` |
|        - |  5763 | `			if( pNos < pStack ){` |
|        - |  5764 | `				goto Abort;` |
|        - |  5765 | `			}` |
|        - |  5766 | `#endif` |
|       67 |  5767 | `		}else{` |
|        - |  5768 | `			/* Attribute name already computed */` |
|       14 |  5769 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5770 | `		}` |
|      144 |  5771 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      144 |  5772 | `			ph7_class *pClass = 0;` |
|      144 |  5773 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5774 | `				/* Class already instantiated */` |
|        5 |  5775 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5776 | `				pClass = pThis->pClass;` |
|        5 |  5777 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5778 | `			}else{` |
|        - |  5779 | `				/* Try to extract the target class */` |
|      140 |  5780 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      140 |  5781 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      140 |  5782 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5783 | `					/* Handle self/static/parent keywords */` |
|      140 |  5784 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       36 |  5785 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       36 |  5786 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5787 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5788 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5789 | `						}` |
|      123 |  5790 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       22 |  5791 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      103 |  5792 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       16 |  5793 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       16 |  5794 | `						if( pSelf && pSelf->pBase ){` |
|       16 |  5795 | `							pClass = pSelf->pBase;` |
|        7 |  5796 | `						}` |
|        9 |  5797 | `					}else{` |
|       72 |  5798 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5799 | `					}` |
|       69 |  5800 | `				}` |
|        - |  5801 | `			}` |
|      144 |  5802 | `			if( pClass == 0 ){` |
|        - |  5803 | `				/* Undefined class */` |
|      ! 0 |  5804 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5805 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5806 | `					);` |
|      ! 0 |  5807 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5808 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5809 | `				}` |
|      ! 0 |  5810 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5811 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5812 | `			}else{` |
|      144 |  5813 | `				if( pInstr->iP2 ){` |
|        - |  5814 | `					/* Method call */` |
|       68 |  5815 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5816 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5817 | `						/* Extract the target method */` |
|       68 |  5818 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5819 | `					}` |
|       68 |  5820 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5821 | `						if( pMeth ){` |
|      ! 0 |  5822 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5823 | `								&pClass->sName,&sName` |
|        - |  5824 | `								);` |
|      ! 0 |  5825 | `						}else{` |
|      ! 0 |  5826 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5827 | `								&pClass->sName,&sName` |
|        - |  5828 | `								);` |
|        - |  5829 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5830 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5831 | `						}` |
|        - |  5832 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5833 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5834 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5835 | `						}` |
|      ! 0 |  5836 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5837 | `					}else{` |
|        - |  5838 | `						/* Push method name on the stack */` |
|       68 |  5839 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5840 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5841 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5842 | `					}` |
|       68 |  5843 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5844 | `				}else{` |
|        - |  5845 | `					/* Attribute access */` |
|       78 |  5846 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5847 | `					/* Check for special ::class pseudo-constant */` |
|      113 |  5848 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5849 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5850 | `						/* ::class returns the fully qualified class name */` |
|        - |  5851 | `						/* Pop the attribute name from the stack */` |
|       60 |  5852 | `						if( !pInstr->p3 ){` |
|       60 |  5853 | `							VmPopOperand(&pTos,1);` |
|       29 |  5854 | `						}` |
|       60 |  5855 | `						PH7_MemObjRelease(pTos);` |
|        - |  5856 | `						/* Load the class name */` |
|       60 |  5857 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5858 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5859 | `					}else{` |
|        - |  5860 | `						/* Extract the target attribute */` |
|       20 |  5861 | `						if( sName.nByte > 0 ){` |
|       20 |  5862 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5863 | `						}` |
|       20 |  5864 | `						if( pAttr == 0 ){` |
|        - |  5865 | `							/* No such attribute,load null */` |
|      ! 0 |  5866 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5867 | `								&pClass->sName,&sName);` |
|        - |  5868 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5869 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5870 | `						}` |
|        - |  5871 | `						/* Pop the attribute name from the stack */` |
|       20 |  5872 | `						if( !pInstr->p3 ){` |
|        7 |  5873 | `							VmPopOperand(&pTos,1);` |
|        3 |  5874 | `						}` |
|       20 |  5875 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5876 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5877 | `						if( pAttr ){` |
|       20 |  5878 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5879 | `								/* Access to a non static attribute */` |
|      ! 0 |  5880 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5881 | `									&pClass->sName,&pAttr->sName` |
|        - |  5882 | `									);` |
|      ! 0 |  5883 | `							}else{` |
|        - |  5884 | `								ph7_value *pValue;` |
|        - |  5885 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5886 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5887 | `									/* Load the desired attribute */` |
|       20 |  5888 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5889 | `									if( pValue ){` |
|       20 |  5890 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5891 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5892 | `											/* Load index number */` |
|       14 |  5893 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5894 | `										}` |
|        9 |  5895 | `									}` |
|        9 |  5896 | `								}` |
|        - |  5897 | `							}` |
|        9 |  5898 | `						}` |
|        - |  5899 | `					}` |
|        - |  5900 | `				}` |
|      144 |  5901 | `				if( pThis ){` |
|        - |  5902 | `					/* Safely unreference the object */` |
|        5 |  5903 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5904 | `				}` |
|        - |  5905 | `			}` |
|       73 |  5906 | `		}else{` |
|        - |  5907 | `			/* Pop operands */` |
|      ! 0 |  5908 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5909 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5910 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5911 | `			}` |
|      ! 0 |  5912 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5913 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5914 | `		}` |
|        - |  5915 | `	}` |
|     4470 |  5916 | `	break;` |
|        - |  5917 | `					}` |
|        - |  5918 | `/*` |
|        - |  5919 | ` * OP_NEW P1 * * *` |
|        - |  5920 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5921 | ` */` |
|      329 |  5922 | `case PH7_OP_NEW: {` |
|      660 |  5923 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      660 |  5924 | `	ph7_class *pClass = 0;` |
|        - |  5925 | `	ph7_class_instance *pNew;` |
|      660 |  5926 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5927 | `		/* Try to extract the desired class */` |
|      989 |  5928 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      658 |  5929 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      329 |  5930 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5931 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5932 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5933 | `	}` |
|      660 |  5934 | `	if( pClass == 0 ){` |
|        - |  5935 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5936 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5937 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5938 | `			);` |
|        - |  5939 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5940 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5941 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5942 | `			/* Pop given arguments */` |
|      ! 0 |  5943 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5944 | `		}` |
|      ! 0 |  5945 | `		goto Abort;` |
|      ! 0 |  5946 | `	}else{` |
|        - |  5947 | `		ph7_class_method *pCons;` |
|        - |  5948 | `		/* Create a new class instance */` |
|      660 |  5949 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      660 |  5950 | `		if( pNew == 0 ){` |
|      ! 0 |  5951 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5952 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5953 | `				&pClass->sName` |
|        - |  5954 | `			);` |
|      ! 0 |  5955 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5956 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5957 | `				/* Pop given arguments */` |
|      ! 0 |  5958 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5959 | `			}` |
|      ! 0 |  5960 | `			break;` |
|        - |  5961 | `		}` |
|        - |  5962 | `		/* Check if a constructor is available */` |
|      660 |  5963 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      660 |  5964 | `		if( pCons == 0 ){` |
|      546 |  5965 | `			SyString *pName = &pClass->sName;` |
|        - |  5966 | `			/* Check for a constructor with the same base class name */` |
|      546 |  5967 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      272 |  5968 | `		}` |
|      660 |  5969 | `		if( pCons ){` |
|        - |  5970 | `			/* Call the class constructor */` |
|      116 |  5971 | `			SySetReset(&aArg);` |
|      220 |  5972 | `			while( pArg < pTos ){` |
|      106 |  5973 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  5974 | `				pArg++;` |
|        2 |  5975 | `			}` |
|      116 |  5976 | `			if( pVm->bErrReport ){` |
|        - |  5977 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5978 | `				sxu32 n;` |
|       57 |  5979 | `				n = SySetUsed(&aArg);` |
|        - |  5980 | `				/* Emit a notice for missing arguments */` |
|      101 |  5981 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  5982 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  5983 | `					if( pFuncArg ){` |
|       45 |  5984 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5985 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5986 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5987 | `						}` |
|       22 |  5988 | `					}` |
|       45 |  5989 | `					n++;` |
|        1 |  5990 | `				}` |
|       28 |  5991 | `			}` |
|      116 |  5992 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5993 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  5994 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5995 | `				pNew->iRef = 1;` |
|      ! 0 |  5996 | `			}` |
|       57 |  5997 | `		}` |
|      660 |  5998 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5999 | `			/* Pop given arguments */` |
|       98 |  6000 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  6001 | `		}` |
|      660 |  6002 | `		PH7_MemObjRelease(pTos);` |
|      660 |  6003 | `		pTos->x.pOther = pNew;` |
|      660 |  6004 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6005 | `	}` |
|      660 |  6006 | `	break;` |
|        - |  6007 | `				 }` |
|        - |  6008 | `/*` |
|        - |  6009 | ` * OP_CLONE * * *` |
|        - |  6010 | ` * Perfome a clone operation.` |
|        - |  6011 | ` */` |
|       23 |  6012 | `case PH7_OP_CLONE: {` |
|        - |  6013 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6014 | `#ifdef UNTRUST` |
|        - |  6015 | `	if( pTos < pStack ){` |
|        - |  6016 | `		goto Abort;` |
|        - |  6017 | `	}` |
|        - |  6018 | `#endif` |
|        - |  6019 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6020 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6021 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6022 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6023 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6024 | `		break;` |
|        - |  6025 | `	}` |
|        - |  6026 | `	/* Point to the source */` |
|       44 |  6027 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6028 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6029 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6030 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6031 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6032 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6033 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6034 | `		break;` |
|        - |  6035 | `	}` |
|        - |  6036 | `	/* Perform the clone operation */` |
|       44 |  6037 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6038 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6039 | `	if( pClone == 0 ){` |
|      ! 0 |  6040 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6041 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6042 | `	}else{` |
|        - |  6043 | `		/* Load the cloned object */` |
|       44 |  6044 | `		pTos->x.pOther = pClone;` |
|       44 |  6045 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6046 | `	}` |
|       44 |  6047 | `	break;` |
|        - |  6048 | `				   }` |
|        - |  6049 | `/*` |
|        - |  6050 | ` * OP_SWITCH * * P3` |
|        - |  6051 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6052 | ` */` |
|       21 |  6053 | `case PH7_OP_SWITCH: {` |
|       44 |  6054 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6055 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6056 | `	ph7_value sValue,sCaseValue;` |
|        - |  6057 | `	sxu32 n,nEntry;` |
|        - |  6058 | `#ifdef UNTRUST` |
|        - |  6059 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6060 | `		goto Abort;` |
|        - |  6061 | `	}` |
|        - |  6062 | `#endif` |
|        - |  6063 | `	/* Point to the case table  */` |
|       44 |  6064 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  6065 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6066 | `	/* Select the appropriate case block to execute */` |
|       44 |  6067 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  6068 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  6069 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  6070 | `		pCase = &aCase[n];` |
|      102 |  6071 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6072 | `		/* Execute the case expression first */` |
|      102 |  6073 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6074 | `		/* Compare the two expression */` |
|      102 |  6075 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  6076 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  6077 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  6078 | `		if( rc == 0 ){` |
|        - |  6079 | `			/* Value match,jump to this block */` |
|       44 |  6080 | `			pc = pCase->nStart - 1;` |
|       44 |  6081 | `			break;` |
|        - |  6082 | `		}` |
|       31 |  6083 | `	}` |
|       44 |  6084 | `	VmPopOperand(&pTos,1);` |
|       44 |  6085 | `	if( n >= nEntry ){` |
|        - |  6086 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6087 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6088 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6089 | `		}else{` |
|        - |  6090 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6091 | `			pc = pSwitch->nOut - 1;` |
|        - |  6092 | `		}` |
|      ! 0 |  6093 | `	}` |
|       44 |  6094 | `	break;` |
|        - |  6095 | `					}` |
|        - |  6096 | `/*` |
|        - |  6097 | ` * OP_YIELD P1 P2 *` |
|        - |  6098 | ` *  Yield a value from a generator function.` |
|        - |  6099 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6100 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6101 | ` */` |
|       28 |  6102 | `case PH7_OP_YIELD: {` |
|        - |  6103 | `	ph7_generator *pGen;` |
|       58 |  6104 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6105 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6106 | `		goto Abort;` |
|        - |  6107 | `	}` |
|       58 |  6108 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6109 | `	if( pInstr->iP2 ){` |
|        - |  6110 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6111 | `#ifdef UNTRUST` |
|        - |  6112 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6113 | `#endif` |
|        7 |  6114 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6115 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6116 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6117 | `		VmPopOperand(&pTos, 1);` |
|        - |  6118 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6119 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6120 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6121 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6122 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6123 | `			}` |
|        1 |  6124 | `		}` |
|       55 |  6125 | `	}else if( pInstr->iP1 ){` |
|        - |  6126 | `		/* yield $value */` |
|        - |  6127 | `#ifdef UNTRUST` |
|        - |  6128 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6129 | `#endif` |
|       52 |  6130 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6131 | `		VmPopOperand(&pTos, 1);` |
|        - |  6132 | `		/* Auto-increment key */` |
|       52 |  6133 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6134 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6135 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6136 | `	}else{` |
|        - |  6137 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6138 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6139 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6140 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6141 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6142 | `	}` |
|        - |  6143 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6144 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6145 | `	goto Suspend;` |
|        - |  6146 |  |
|        - |  6147 | `/*` |
|        - |  6148 | ` * OP_CALL P1 * *` |
|        - |  6149 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6150 | ` *  function on the stack.` |
|        - |  6151 | ` */` |
|   298666 |  6152 | `case PH7_OP_CALL: {` |
|   597378 |  6153 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6154 | `	ph7_value *pArg;` |
|   597378 |  6155 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   597378 |  6156 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6157 | `	SyHashEntry *pEntry;` |
|        - |  6158 | `	SyString sName;` |
|        - |  6159 | `	/* Extract function name */` |
|   597378 |  6160 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6161 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6162 | `			ph7_value sResult;` |
|      ! 0 |  6163 | `			SySetReset(&aArg);` |
|      ! 0 |  6164 | `			while( pArg < pTos ){` |
|      ! 0 |  6165 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6166 | `				pArg++;` |
|      ! 0 |  6167 | `			}` |
|      ! 0 |  6168 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6169 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6170 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6171 | `			SySetReset(&aArg);` |
|        - |  6172 | `			/* Pop given arguments */` |
|      ! 0 |  6173 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6174 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6175 | `			}` |
|        - |  6176 | `			/* Copy result */` |
|      ! 0 |  6177 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6178 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6179 | `		}else{` |
|        3 |  6180 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6181 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6182 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6183 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6184 | `			}else{` |
|        - |  6185 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6186 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6187 | `			}` |
|        - |  6188 | `			/* Pop given arguments */` |
|        3 |  6189 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6190 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6191 | `			}` |
|        - |  6192 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6193 | `			PH7_MemObjRelease(pTos);` |
|        - |  6194 | `		}` |
|   298389 |  6195 | `		break;` |
|        - |  6196 | `	}` |
|   597376 |  6197 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6198 | `	/* Check for a compiled function first.` |
|        - |  6199 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6200 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   597376 |  6201 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6202 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6203 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6204 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6205 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6206 | `	 * function calls inside namespaces. */` |
|   597376 |  6207 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6208 | `		const char *zFunc;` |
|        - |  6209 | `		const char *zEnd;` |
|        - |  6210 | `		const char *z;` |
|        - |  6211 | `		SyString sGlobal;` |
|       18 |  6212 | `		zFunc = sName.zString;` |
|       18 |  6213 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6214 | `		z = zEnd;` |
|        - |  6215 | `		/* Find last namespace separator */` |
|      154 |  6216 | `		while( z > zFunc ){` |
|      154 |  6217 | `			if( z[-1] == '\\' ){` |
|       18 |  6218 | `				break;` |
|        - |  6219 | `			}` |
|      138 |  6220 | `			z--;` |
|        2 |  6221 | `		}` |
|       18 |  6222 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6223 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6224 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6225 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6226 | `		}` |
|        8 |  6227 | `	}` |
|   597376 |  6228 | `	if( pEntry ){` |
|        - |  6229 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6230 | `		ph7_class_instance *pThis;` |
|        - |  6231 | `		ph7_value *pFrameStack;` |
|        - |  6232 | `		ph7_vm_func *pVmFunc;` |
|        - |  6233 | `		ph7_class *pSelf;` |
|        - |  6234 | `		VmFrame *pFrame;` |
|        - |  6235 | `		ph7_value *pObj;` |
|        - |  6236 | `		VmSlot sArg;` |
|        - |  6237 | `		sxu32 n;` |
|        - |  6238 | `		/* initialize fields */` |
|    13418 |  6239 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13418 |  6240 | `		pThis = 0;` |
|    13418 |  6241 | `		pSelf = 0;` |
|    13418 |  6242 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6243 | `			ph7_class_method *pMeth;` |
|        - |  6244 | `			/* Class method call */` |
|     2010 |  6245 | `			ph7_value *pTarget = &pTos[-1];` |
|     2010 |  6246 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6247 | `				/* Extract the 'this' pointer */` |
|     2010 |  6248 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6249 | `					/* Instance already loaded */` |
|     1938 |  6250 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1938 |  6251 | `					pThis->iRef++;` |
|     1938 |  6252 | `					pSelf = pThis->pClass;` |
|      968 |  6253 | `				}` |
|     2010 |  6254 | `				if( pSelf == 0 ){` |
|       74 |  6255 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6256 | `						/* "Late Static Binding" class name */` |
|      101 |  6257 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6258 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6259 | `					}` |
|       74 |  6260 | `					if( pSelf == 0 ){` |
|       13 |  6261 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6262 | `					}` |
|       36 |  6263 | `				}` |
|     2010 |  6264 | `				if( pThis == 0  ){` |
|       74 |  6265 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6266 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6267 | `					if( pFrameLocal->pParent ){` |
|        - |  6268 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6269 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6270 | `						if( pThis ){` |
|       13 |  6271 | `							pThis->iRef++;` |
|        6 |  6272 | `						}` |
|       28 |  6273 | `					}` |
|       36 |  6274 | `				}` |
|     2010 |  6275 | `				VmPopOperand(&pTos,1);` |
|     2010 |  6276 | `				PH7_MemObjRelease(pTos);` |
|        - |  6277 | `				/* Synchronize pointers */` |
|     2010 |  6278 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6279 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6280 | `				 * user have already computed the random generated unique class method name` |
|        - |  6281 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6282 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6283 | `				 */` |
|     2010 |  6284 | `				while( pArg < pStack ){` |
|      ! 0 |  6285 | `					pArg++;` |
|      ! 0 |  6286 | `				}` |
|     2010 |  6287 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6288 | `					/* Check if the call is allowed */` |
|     2010 |  6289 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2010 |  6290 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6291 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6292 | `							/* Pop given arguments */` |
|      ! 0 |  6293 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6294 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6295 | `							}` |
|        - |  6296 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6297 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6298 | `							break;` |
|        - |  6299 | `						}` |
|        3 |  6300 | `					}` |
|     1004 |  6301 | `				}` |
|     1004 |  6302 | `			}` |
|     1004 |  6303 | `		}` |
|        - |  6304 | `		/* Check The recursion limit */` |
|    13418 |  6305 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6306 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6307 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6308 | `				&pVmFunc->sName);` |
|        - |  6309 | `			/* Pop given arguments */` |
|        3 |  6310 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6311 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6312 | `			}` |
|        - |  6313 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6314 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6315 | `			break;` |
|        - |  6316 | `		}` |
|    13416 |  6317 | `		if( pVmFunc->pNextName ){` |
|        - |  6318 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6319 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6320 | `		}` |
|    13416 |  6321 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6322 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6323 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6324 | `			ph7_generator *pGenerator;` |
|        - |  6325 | `			ph7_class_instance *pGenObj;` |
|        - |  6326 | `			ph7_value *pCtxAttr;` |
|        - |  6327 | `			SyString sAttrName;` |
|        - |  6328 | `			ph7_value **apCallArgs;` |
|        - |  6329 | `			int nGenArgs, iArg;` |
|        - |  6330 | `			/* Collect arguments from the operand stack */` |
|       20 |  6331 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6332 | `			apCallArgs = 0;` |
|       20 |  6333 | `			if( nGenArgs > 0 ){` |
|        8 |  6334 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6335 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6336 | `				if( apCallArgs == 0 ){` |
|        - |  6337 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6338 | `					nGenArgs = 0;` |
|      ! 0 |  6339 | `				}else{` |
|       12 |  6340 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6341 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6342 | `					}` |
|        - |  6343 | `				}` |
|        2 |  6344 | `			}` |
|        - |  6345 | `			/* Create execution context and generator wrapper */` |
|       20 |  6346 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6347 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6348 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6349 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6350 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6351 | `				break;` |
|        - |  6352 | `			}` |
|       20 |  6353 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6354 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6355 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6356 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6357 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6358 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6359 | `				break;` |
|        - |  6360 | `			}` |
|        - |  6361 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6362 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6363 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6364 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6365 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6366 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6367 | `			if( apCallArgs ){` |
|        6 |  6368 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6369 | `			}` |
|       20 |  6370 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6371 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6372 | `				if( pThis ){` |
|      ! 0 |  6373 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6374 | `				}` |
|      ! 0 |  6375 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6376 | `					goto Abort;` |
|        - |  6377 | `				}` |
|      ! 0 |  6378 | `				break;` |
|        - |  6379 | `			}` |
|        - |  6380 | `			/* Create Generator class instance */` |
|       20 |  6381 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6382 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6383 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6384 | `				break;` |
|        - |  6385 | `			}` |
|        - |  6386 | `			/* Store generator in __ctx attribute */` |
|       20 |  6387 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6388 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6389 | `			if( pCtxAttr ){` |
|       20 |  6390 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6391 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6392 | `			}` |
|        - |  6393 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6394 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6395 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6396 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6397 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6398 | `			pGenObj->iRef++;` |
|       20 |  6399 | `			if( pThis ){` |
|      ! 0 |  6400 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6401 | `			}` |
|       20 |  6402 | `			break;` |
|        - |  6403 | `		}` |
|        - |  6404 | `		/* Extract the formal argument set */` |
|    13398 |  6405 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6406 | `		/* Create a new VM frame  */` |
|    13398 |  6407 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13398 |  6408 | `		if( rc != SXRET_OK ){` |
|        - |  6409 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6410 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6411 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6412 | `				&pVmFunc->sName);` |
|        - |  6413 | `			/* Pop given arguments */` |
|      ! 0 |  6414 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6415 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6416 | `			}` |
|        - |  6417 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6418 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6419 | `			break;` |
|        - |  6420 | `		}` |
|    13398 |  6421 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6422 | `			/* Install the '$this' variable */` |
|        - |  6423 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1948 |  6424 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1948 |  6425 | `			if( pObj ){` |
|        - |  6426 | `				/* Reflect the change */` |
|     1948 |  6427 | `				pObj->x.pOther = pThis;` |
|     1948 |  6428 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      973 |  6429 | `			}` |
|      973 |  6430 | `		}` |
|    13398 |  6431 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6432 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6433 | `			/* Install static variables */` |
|      ! 0 |  6434 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6435 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6436 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6437 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6438 | `					/* Initialize the static variables */` |
|      ! 0 |  6439 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6440 | `					if( pObj ){` |
|        - |  6441 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6442 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6443 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6444 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6445 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6446 | `						}` |
|      ! 0 |  6447 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6448 | `					}else{` |
|      ! 0 |  6449 | `						continue;` |
|        - |  6450 | `					}` |
|      ! 0 |  6451 | `				}` |
|        - |  6452 | `				/* Install in the current frame */` |
|      ! 0 |  6453 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6454 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6455 | `			}` |
|      ! 0 |  6456 | `		}` |
|        - |  6457 | `		/* Push arguments in the local frame */` |
|    13398 |  6458 | `		n = 0;` |
|    36300 |  6459 | `		while( pArg < pTos ){` |
|    22924 |  6460 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6461 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6462 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6463 | `				if( pObj ){` |
|        - |  6464 | `					/* Initialize as empty array */` |
|       21 |  6465 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6466 | `					{` |
|       21 |  6467 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6468 | `						while( pArg < pTos ){` |
|        - |  6469 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6470 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6471 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6472 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6473 | `								if( xCast ){` |
|       13 |  6474 | `									xCast(pArg);` |
|        6 |  6475 | `								}` |
|        6 |  6476 | `							}` |
|       63 |  6477 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6478 | `							pArg++;` |
|        1 |  6479 | `						}` |
|        - |  6480 | `					}` |
|       21 |  6481 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6482 | `					sArg.pUserData = 0;` |
|       21 |  6483 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6484 | `				}` |
|       21 |  6485 | `				break; /* All remaining args consumed */` |
|        - |  6486 | `			}` |
|    22904 |  6487 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22748 |  6488 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6489 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6490 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6491 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6492 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6493 | `						goto Abort;` |
|        - |  6494 | `					}` |
|      ! 0 |  6495 | `				}` |
|        - |  6496 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6497 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22760 |  6498 | `				if( aFormalArg[n].nType > 0` |
|    11955 |  6499 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1148 |  6500 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6501 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6502 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6503 | `						ph7_class *pClass;` |
|        - |  6504 | `						/* Try to extract the desired class */` |
|        5 |  6505 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6506 | `						if( pClass ){` |
|        5 |  6507 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6508 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6509 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6510 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6511 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6512 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6513 | `								}` |
|      ! 0 |  6514 | `							}else{` |
|        - |  6515 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6516 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6517 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6518 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6519 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6520 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6521 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6522 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6523 | `								}` |
|        - |  6524 | `							}` |
|        3 |  6525 | `						}` |
|     1146 |  6526 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6527 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6528 | `						/* Cast to the desired type */` |
|      ! 0 |  6529 | `						xCast(pArg);` |
|      ! 0 |  6530 | `					}` |
|      573 |  6531 | `				}` |
|    22750 |  6532 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6533 | `					/* Pass by reference */` |
|       54 |  6534 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6535 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6536 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6537 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6538 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6539 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6540 | `						}` |
|        - |  6541 | `						/* Switch to pass by value */` |
|      ! 0 |  6542 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6543 | `					}else{` |
|        - |  6544 | `						SyHashEntry *pRefEntry;` |
|        - |  6545 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6546 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6547 | `						if( pRefEntry == 0 ){` |
|       80 |  6548 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6549 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6550 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6551 | `							sArg.pUserData = 0;` |
|       54 |  6552 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6553 | `						}` |
|       54 |  6554 | `						pObj = 0;` |
|        - |  6555 | `					}` |
|       28 |  6556 | `				}else{` |
|        - |  6557 | `					/* Pass by value,make a copy of the given argument */` |
|    22698 |  6558 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6559 | `				}` |
|    11376 |  6560 | `			}else{` |
|        - |  6561 | `				char zName[32];` |
|        - |  6562 | `				SyString sArgName;` |
|        - |  6563 | `				/* Set a dummy name */` |
|      156 |  6564 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6565 | `				sArgName.zString = zName;` |
|        - |  6566 | `				/* Annonymous argument */` |
|      156 |  6567 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6568 | `			}` |
|    22904 |  6569 | `			if( pObj ){` |
|    22852 |  6570 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6571 | `				/* Insert argument index  */` |
|    22852 |  6572 | `				sArg.nIdx = pObj->nIdx;` |
|    22852 |  6573 | `				sArg.pUserData = 0;` |
|    22852 |  6574 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11425 |  6575 | `			}` |
|    22904 |  6576 | `			PH7_MemObjRelease(pArg);` |
|    22904 |  6577 | `			pArg++;` |
|    22904 |  6578 | `			++n;` |
|        2 |  6579 | `		}` |
|        - |  6580 | `		/* Set up closure environment */` |
|    13398 |  6581 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6582 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6583 | `			ph7_value *pValue;` |
|        - |  6584 | `			sxu32 iEnv;` |
|       11 |  6585 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6586 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6587 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6588 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6589 | `					/* Do not install null value */` |
|       11 |  6590 | `					continue;` |
|        - |  6591 | `				}` |
|       11 |  6592 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6593 | `				if( pValue == 0 ){` |
|      ! 0 |  6594 | `					continue;` |
|        - |  6595 | `				}` |
|        - |  6596 | `				/* Invalidate any prior representation */` |
|       11 |  6597 | `				PH7_MemObjRelease(pValue);` |
|        - |  6598 | `				/* Duplicate bound variable value */` |
|       11 |  6599 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6600 | `			}` |
|        5 |  6601 | `		}` |
|        - |  6602 | `		/* Process default values for remaining formal parameters */` |
|    15372 |  6603 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2002 |  6604 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6605 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6606 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6607 | `				if( pObj ){` |
|       27 |  6608 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6609 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6610 | `					sArg.pUserData = 0;` |
|       27 |  6611 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6612 | `				}` |
|       27 |  6613 | `				n++;` |
|       27 |  6614 | `				break; /* Variadic is always last */` |
|        - |  6615 | `			}` |
|     1976 |  6616 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1970 |  6617 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1970 |  6618 | `				if( pObj ){` |
|        - |  6619 | `					/* Evaluate the default value and extract it's result */` |
|     1970 |  6620 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1970 |  6621 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6622 | `						goto Abort;` |
|        - |  6623 | `					}` |
|        - |  6624 | `					/* Insert argument index */` |
|     1970 |  6625 | `					sArg.nIdx = pObj->nIdx;` |
|     1970 |  6626 | `					sArg.pUserData = 0;` |
|     1970 |  6627 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6628 | `					/* Make sure the default argument is of the correct type */` |
|     1970 |  6629 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6630 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6631 | `						/* Cast to the desired type */` |
|      ! 0 |  6632 | `						xCast(pObj);` |
|      ! 0 |  6633 | `					}` |
|      984 |  6634 | `				}` |
|      984 |  6635 | `			}` |
|     1976 |  6636 | `			++n;` |
|        2 |  6637 | `		}` |
|        - |  6638 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6639 | `		 * does not return anything.` |
|        - |  6640 | `		 */` |
|    13398 |  6641 | `		PH7_MemObjRelease(pTos);` |
|    13398 |  6642 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6643 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13398 |  6644 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13398 |  6645 | `		if( pFrameStack == 0 ){` |
|        - |  6646 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6647 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6648 | `				&pVmFunc->sName);` |
|      ! 0 |  6649 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6650 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6651 | `			}` |
|      ! 0 |  6652 | `			break;` |
|        - |  6653 | `		}` |
|    13398 |  6654 | `		if( pSelf ){` |
|        - |  6655 | `			/* Push class name */` |
|     2008 |  6656 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1003 |  6657 | `		}` |
|        - |  6658 | `		/* Increment nesting level */` |
|    13398 |  6659 | `		pVm->nRecursionDepth++;` |
|        - |  6660 | `		/* Execute function body */` |
|    13398 |  6661 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6662 | `		/* Decrement nesting level */` |
|    13398 |  6663 | `		pVm->nRecursionDepth--;` |
|    13398 |  6664 | `		if( pSelf ){` |
|        - |  6665 | `			/* Pop class name */` |
|     2008 |  6666 | `			(void)SySetPop(&pVm->aSelf);` |
|     1003 |  6667 | `		}` |
|        - |  6668 | `		/* Cleanup the mess left behind */` |
|    13398 |  6669 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6670 | `			/* Return by reference,reflect that */` |
|        9 |  6671 | `			if( n != SXU32_HIGH ){` |
|        9 |  6672 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6673 | `				sxu32 i;` |
|        - |  6674 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6675 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6676 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6677 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6678 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6679 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6680 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6681 | `								&pVmFunc->sName);` |
|      ! 0 |  6682 | `						}` |
|      ! 0 |  6683 | `						n = SXU32_HIGH;` |
|      ! 0 |  6684 | `						break;` |
|        - |  6685 | `					}` |
|        3 |  6686 | `				}` |
|        5 |  6687 | `			}else{` |
|      ! 0 |  6688 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6689 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6690 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6691 | `						&pVmFunc->sName);` |
|      ! 0 |  6692 | `				}` |
|        - |  6693 | `			}` |
|        9 |  6694 | `			pTos->nIdx = n;` |
|        4 |  6695 | `		}` |
|        - |  6696 | `		/* Cleanup the mess left behind */` |
|    13398 |  6697 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6698 | `			/* An exception was throw in this frame */` |
|       12 |  6699 | `			pFrame = pFrame->pParent;` |
|       12 |  6700 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6701 | `				/* Pop the resutlt */` |
|       10 |  6702 | `				VmPopOperand(&pTos,1);` |
|        - |  6703 | `				/* Jump to this destination */` |
|       10 |  6704 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6705 | `				rc = PH7_OK;` |
|        6 |  6706 | `			}else{` |
|        3 |  6707 | `				if( pFrame->pParent ){` |
|        3 |  6708 | `					rc = PH7_EXCEPTION;` |
|        2 |  6709 | `				}else{` |
|        - |  6710 | `					/* Continue normal execution */` |
|      ! 0 |  6711 | `					rc = PH7_OK;` |
|        - |  6712 | `				}` |
|        - |  6713 | `			}` |
|        5 |  6714 | `		}` |
|        - |  6715 | `		/* Free the operand stack */` |
|    13398 |  6716 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6717 | `		/* Leave the frame */` |
|    13398 |  6718 | `		VmLeaveFrame(&(*pVm));` |
|    13398 |  6719 | `		if( rc == PH7_ABORT ){` |
|        - |  6720 | `			/* Abort processing immeditaley */` |
|        7 |  6721 | `			goto Abort;` |
|    13392 |  6722 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6723 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6724 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6725 | `			 * overwriting the state saved by the inner level.` |
|        - |  6726 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6727 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6728 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6729 | `			goto Suspend;` |
|    13354 |  6730 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6731 | `			goto Exception;` |
|        - |  6732 | `		}` |
|     6677 |  6733 | `	}else{` |
|        - |  6734 | `		ph7_user_func *pFunc;` |
|        - |  6735 | `		ph7_context sCtx;` |
|        - |  6736 | `		ph7_value sRet;` |
|        - |  6737 | `		/* Look for an installed foreign function.` |
|        - |  6738 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6739 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6740 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6741 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6742 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6743 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   583960 |  6744 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   583960 |  6745 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6746 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6747 | `			const char *zShort = sName.zString;` |
|        - |  6748 | `			sxu32 i;` |
|      262 |  6749 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6750 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6751 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6752 | `				}` |
|      124 |  6753 | `			}` |
|       18 |  6754 | `			if( zShort != sName.zString ){` |
|       18 |  6755 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6756 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6757 | `			}` |
|        8 |  6758 | `		}` |
|   583960 |  6759 | `		if( pEntry == 0 ){` |
|        - |  6760 | `			/* Call to undefined function */` |
|        5 |  6761 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6762 | `			/* Pop given arguments */` |
|        5 |  6763 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6764 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6765 | `			}` |
|        - |  6766 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6767 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6768 | `			break;` |
|        - |  6769 | `		}` |
|   583956 |  6770 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6771 | `		/* Start collecting function arguments */` |
|   583956 |  6772 | `		SySetReset(&aArg);` |
|  1567796 |  6773 | `		while( pArg < pTos ){` |
|   983842 |  6774 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   983842 |  6775 | `			pArg++;` |
|        2 |  6776 | `		}` |
|        - |  6777 | `		/* Assume a null return value */` |
|   583956 |  6778 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6779 | `		/* Init the call context */` |
|   583956 |  6780 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6781 | `		/* Call the foreign function */` |
|   583956 |  6782 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6783 | `		/* Release the call context */` |
|   583956 |  6784 | `		VmReleaseCallContext(&sCtx);` |
|   583956 |  6785 | `		if( rc == PH7_ABORT ){` |
|      471 |  6786 | `			goto Abort;` |
|   583486 |  6787 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6788 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6789 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6790 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6791 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6792 | `				goto Exception;` |
|        - |  6793 | `			}` |
|        - |  6794 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6795 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6796 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6797 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6798 | `			}` |
|        - |  6799 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6800 | `			VmPopOperand(&pTos,1);` |
|        - |  6801 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6802 | `			pFrm = pVm->pFrame;` |
|        7 |  6803 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6804 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6805 | `			}` |
|        7 |  6806 | `			break;` |
|        - |  6807 | `		}` |
|   583476 |  6808 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6809 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6810 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6811 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6812 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6813 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6814 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6815 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6816 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6817 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6818 | `			}` |
|        - |  6819 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6820 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6821 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6822 | `			goto Suspend;` |
|        - |  6823 | `		}` |
|   583438 |  6824 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6825 | `			/* Pop function name and arguments */` |
|   564806 |  6826 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   282424 |  6827 | `		}` |
|        - |  6828 | `		/* Save foreign function return value */` |
|   583438 |  6829 | `		PH7_MemObjStore(&sRet,pTos);` |
|   583438 |  6830 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6831 | `	}` |
|   596788 |  6832 | `	break;` |
|        - |  6833 | `				  }` |
|        - |  6834 | `/*` |
|        - |  6835 | ` * OP_CONSUME: P1 * *` |
|        - |  6836 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6837 | ` */` |
|    11846 |  6838 | `case PH7_OP_CONSUME: {` |
|    23694 |  6839 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23694 |  6840 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6841 |  |
|    23694 |  6842 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23694 |  6843 | `	pCur = pOut;` |
|        - |  6844 | `	/* Start the consume process  */` |
|    47386 |  6845 | `	while( pOut <= pTos ){` |
|        - |  6846 | `		/* Force a string cast */` |
|    23694 |  6847 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6848 | `			PH7_MemObjToString(pOut);` |
|      149 |  6849 | `		}` |
|    23694 |  6850 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6851 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6852 | `			/* Invoke the output consumer callback */` |
|    13248 |  6853 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13248 |  6854 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13248 |  6855 | `			SyBlobRelease(&pOut->sBlob);` |
|    13248 |  6856 | `			if( rc == SXERR_ABORT ){` |
|        - |  6857 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6858 | `				goto Abort;` |
|        - |  6859 | `			}` |
|     6623 |  6860 | `		}` |
|    23694 |  6861 | `		pOut++;` |
|        2 |  6862 | `	}` |
|    23694 |  6863 | `	pTos = &pCur[-1];` |
|    23692 |  6864 | `	break;` |
|        - |  6865 | `					 }` |
|        - |  6866 |  |
|        - |  6867 | `		} /* Switch() */` |
| 10066738 |  6868 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6869 | `	} /* For(;;) */` |
|    16288 |  6870 | `Done:` |
|    32578 |  6871 | `	SySetRelease(&aArg);` |
|    32578 |  6872 | `	return SXRET_OK;` |
|       66 |  6873 | `Suspend:` |
|      134 |  6874 | `	SySetRelease(&aArg);` |
|      134 |  6875 | `	return PH7_SUSPEND;` |
|      242 |  6876 | `Abort:` |
|      485 |  6877 | `	SySetRelease(&aArg);` |
|     1685 |  6878 | `	while( pTos >= pStack ){` |
|     1201 |  6879 | `		PH7_MemObjRelease(pTos);` |
|     1201 |  6880 | `		pTos--;` |
|        1 |  6881 | `	}` |
|      485 |  6882 | `	return PH7_ABORT;` |
|        3 |  6883 | `Exception:` |
|        8 |  6884 | `	SySetRelease(&aArg);` |
|       22 |  6885 | `	while( pTos >= pStack ){` |
|       16 |  6886 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6887 | `		pTos--;` |
|        2 |  6888 | `	}` |
|        8 |  6889 | `	return PH7_EXCEPTION;` |
|    16601 |  6890 |  |
|        - |  6891 | `/*` |
|        - |  6892 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6893 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6894 | ` * See block-comment on that function for additional information.` |
|        - |  6895 | ` */` |
|    15370 |  6896 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6897 |  |
|        - |  6898 | `	ph7_value *pStack;` |
|        - |  6899 | `	sxi32 rc;` |
|        - |  6900 | `	/* Allocate a new operand stack */` |
|    15372 |  6901 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15372 |  6902 | `	if( pStack == 0 ){` |
|      ! 0 |  6903 | `		return SXERR_MEM;` |
|        - |  6904 | `	}` |
|        - |  6905 | `	/* Execute the program */` |
|    15372 |  6906 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6907 | `	/* Free the operand stack */` |
|    15372 |  6908 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6909 | `	/* Execution result */` |
|    15372 |  6910 | `	return rc;` |
|     7687 |  6911 |  |
|        - |  6912 | `/*` |
|        - |  6913 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6914 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6915 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6916 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6917 | ` * execution ends.` |
|        - |  6918 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6919 | ` * additional information.` |
|        - |  6920 | ` */` |
|     2324 |  6921 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6922 |  |
|        - |  6923 | `	VmShutdownCB *pEntry;` |
|        - |  6924 | `	ph7_value *apArg[10];` |
|        - |  6925 | `	sxu32 n,nEntry;` |
|        - |  6926 | `	int i;` |
|        - |  6927 | `	/* Point to the stack of registered callbacks */` |
|     2326 |  6928 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25566 |  6929 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23242 |  6930 | `		apArg[i] = 0;` |
|    11622 |  6931 | `	}` |
|     2328 |  6932 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6933 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6934 | `		if( pEntry ){` |
|        - |  6935 | `			/* Prepare callback arguments if any */` |
|        3 |  6936 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6937 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6938 | `					break;` |
|        - |  6939 | `				}` |
|      ! 0 |  6940 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6941 | `			}` |
|        - |  6942 | `			/* Invoke the callback */` |
|        3 |  6943 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6944 | `			/*` |
|        - |  6945 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6946 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6947 | `			 */` |
|        3 |  6948 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6949 | `			if( pEntry ){` |
|        3 |  6950 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6951 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6952 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6953 | `				}` |
|        1 |  6954 | `			}` |
|        1 |  6955 | `		}` |
|        2 |  6956 | `	}` |
|     2326 |  6957 | `	SySetReset(&pVm->aShutdown);` |
|     2326 |  6958 |  |
|        - |  6959 | `/*` |
|        - |  6960 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6961 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6962 | ` * See block-comment on that function for additional information.` |
|        - |  6963 | ` */` |
|     2332 |  6964 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6965 |  |
|        - |  6966 | `	/* Make sure we are ready to execute this program */` |
|     2334 |  6967 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6968 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6969 | `	}` |
|        - |  6970 | `	/* Set the execution magic number  */` |
|     2334 |  6971 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6972 | `	/* Execute the program */` |
|     2334 |  6973 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6974 | `	/* Invoke any shutdown callbacks */` |
|     2330 |  6975 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6976 | `	/*` |
|        - |  6977 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6978 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6979 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6980 | `	 */` |
|     2330 |  6981 | `	return SXRET_OK;` |
|     1168 |  6982 |  |
|        - |  6983 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6984 | `/*` |
|        - |  6985 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6986 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6987 | ` */` |
|       42 |  6988 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  6989 |  |
|        - |  6990 | `	ph7_exec_ctx *pCtx;` |
|        - |  6991 | `	ph7_value *pStack;` |
|        - |  6992 | `	VmFrame *pFrame;` |
|       44 |  6993 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  6994 | `	if( pCtx == 0 ){` |
|      ! 0 |  6995 | `		return 0;` |
|        - |  6996 | `	}` |
|       44 |  6997 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  6998 | `	pCtx->pVm = pVm;` |
|       44 |  6999 | `	pCtx->pFunc = pFunc;` |
|       44 |  7000 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7001 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7002 | `	pCtx->pc = 0;` |
|       44 |  7003 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7004 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7005 | `	/* Allocate a private operand stack */` |
|       44 |  7006 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7007 | `	if( pStack == 0 ){` |
|      ! 0 |  7008 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7009 | `		return 0;` |
|        - |  7010 | `	}` |
|       44 |  7011 | `	pCtx->pStack = pStack;` |
|        - |  7012 | `	/* Create a detached frame for the fiber */` |
|       44 |  7013 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7014 | `	if( pFrame == 0 ){` |
|      ! 0 |  7015 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7016 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7017 | `		return 0;` |
|        - |  7018 | `	}` |
|       44 |  7019 | `	pCtx->pFrame = pFrame;` |
|       44 |  7020 | `	return pCtx;` |
|       23 |  7021 |  |
|        - |  7022 | `/*` |
|        - |  7023 | ` * Start executing a fiber context for the first time.` |
|        - |  7024 | ` */` |
|       42 |  7025 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7026 |  |
|        - |  7027 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7028 | `	sxi32 rc;` |
|       44 |  7029 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7030 | `		return SXERR_INVALID;` |
|        - |  7031 | `	}` |
|        - |  7032 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7033 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7034 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7035 | `	/* Save and set the active context */` |
|       44 |  7036 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7037 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7038 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7039 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7040 | `	pVm->nRecursionDepth++;` |
|        - |  7041 | `	/* Execute from the beginning */` |
|       65 |  7042 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7043 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7044 | `	pVm->nRecursionDepth--;` |
|        - |  7045 | `	/* Restore the previous context */` |
|       44 |  7046 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7047 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7048 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7049 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7050 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7051 | `		if( pResult ){` |
|       24 |  7052 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7053 | `		}` |
|       42 |  7054 | `		return SXRET_OK;` |
|        - |  7055 | `	}` |
|        - |  7056 | `	/* Detach frame */` |
|        3 |  7057 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7058 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7059 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7060 | `	}` |
|        3 |  7061 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7062 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7063 | `		return PH7_ABORT;` |
|        - |  7064 | `	}` |
|        3 |  7065 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7066 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7067 | `		return PH7_EXCEPTION;` |
|        - |  7068 | `	}` |
|        - |  7069 | `	/* Normal completion */` |
|        3 |  7070 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7071 | `	if( pResult ){` |
|        3 |  7072 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7073 | `	}` |
|        3 |  7074 | `	return SXRET_OK;` |
|       23 |  7075 |  |
|        - |  7076 | `/*` |
|        - |  7077 | ` * Resume a suspended fiber context.` |
|        - |  7078 | ` */` |
|       86 |  7079 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7080 |  |
|        - |  7081 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7082 | `	sxi32 rc;` |
|       88 |  7083 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7084 | `		return SXERR_INVALID;` |
|        - |  7085 | `	}` |
|        - |  7086 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7087 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7088 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7089 | `	if( pResumeValue ){` |
|       40 |  7090 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7091 | `	}else{` |
|       50 |  7092 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7093 | `	}` |
|       88 |  7094 | `	pCtx->nTos++;` |
|        - |  7095 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7096 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7097 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7098 | `	/* Save and set the active context */` |
|       88 |  7099 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7100 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7101 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7102 | `	pVm->nRecursionDepth++;` |
|        - |  7103 | `	/* Resume execution from saved PC */` |
|      131 |  7104 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7105 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7106 | `	pVm->nRecursionDepth--;` |
|        - |  7107 | `	/* Restore the previous context */` |
|       88 |  7108 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7109 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7110 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7111 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7112 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7113 | `		if( pResult ){` |
|       18 |  7114 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7115 | `		}` |
|       56 |  7116 | `		return SXRET_OK;` |
|        - |  7117 | `	}` |
|        - |  7118 | `	/* Detach frame */` |
|       34 |  7119 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7120 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7121 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7122 | `	}` |
|       34 |  7123 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7124 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7125 | `		return PH7_ABORT;` |
|        - |  7126 | `	}` |
|       34 |  7127 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7128 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7129 | `		return PH7_EXCEPTION;` |
|        - |  7130 | `	}` |
|        - |  7131 | `	/* Normal completion */` |
|       34 |  7132 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7133 | `	if( pResult ){` |
|       20 |  7134 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7135 | `	}` |
|       34 |  7136 | `	return SXRET_OK;` |
|       45 |  7137 |  |
|        - |  7138 | `/*` |
|        - |  7139 | ` * Release an execution context and all its resources.` |
|        - |  7140 | ` */` |
|        4 |  7141 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7142 |  |
|        5 |  7143 | `	if( pCtx == 0 ){` |
|      ! 0 |  7144 | `		return;` |
|        - |  7145 | `	}` |
|        5 |  7146 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7147 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7148 | `		return;` |
|        - |  7149 | `	}` |
|        5 |  7150 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7151 | `	/* Release values */` |
|        5 |  7152 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7153 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7154 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7155 | `	if( pCtx->pFrame ){` |
|        - |  7156 | `		VmSlot *aSlot;` |
|        - |  7157 | `		sxu32 n;` |
|        - |  7158 | `		/* Free local variables */` |
|        5 |  7159 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7160 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7161 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7162 | `		}` |
|        - |  7163 | `		/* Remove local references */` |
|        5 |  7164 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7165 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7166 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7167 | `		}` |
|        5 |  7168 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7169 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7170 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7171 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7172 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7173 | `		pCtx->pFrame = 0;` |
|        2 |  7174 | `	}` |
|        - |  7175 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7176 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7177 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7178 | `	if( pCtx->pStack ){` |
|        5 |  7179 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7180 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7181 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7182 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7183 | `				pTos--;` |
|        1 |  7184 | `			}` |
|        2 |  7185 | `		}` |
|        5 |  7186 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7187 | `		pCtx->pStack = 0;` |
|        2 |  7188 | `	}` |
|        - |  7189 | `	/* Free the context itself */` |
|        5 |  7190 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7191 |  |
|        - |  7192 | `/*` |
|        - |  7193 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7194 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7195 | ` */` |
|       90 |  7196 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7197 |  |
|        - |  7198 | `	ph7_class_instance *pThis;` |
|        - |  7199 | `	SyString sAttr;` |
|        - |  7200 | `	ph7_value *pAttr;` |
|       92 |  7201 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7202 | `		return 0;` |
|        - |  7203 | `	}` |
|       92 |  7204 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7205 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7206 | `		return 0;` |
|        - |  7207 | `	}` |
|       92 |  7208 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7209 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7210 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7211 | `		return 0;` |
|        - |  7212 | `	}` |
|       62 |  7213 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7214 |  |
|        - |  7215 | `/*` |
|        - |  7216 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7217 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7218 | ` */` |
|       38 |  7219 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7220 |  |
|       40 |  7221 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7222 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7223 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7224 | `			"Cannot suspend outside of a fiber");` |
|        - |  7225 | `	}` |
|       40 |  7226 | `	if( nArg > 0 ){` |
|       40 |  7227 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7228 | `	}else{` |
|      ! 0 |  7229 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7230 | `	}` |
|       40 |  7231 | `	return PH7_SUSPEND;` |
|       21 |  7232 |  |
|        - |  7233 | `/*` |
|        - |  7234 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7235 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7236 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7237 | ` */` |
|       24 |  7238 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7239 |  |
|        - |  7240 | `	ph7_class_instance *pThis;` |
|        - |  7241 | `	ph7_value *pAttr;` |
|        - |  7242 | `	SyString sAttrName;` |
|       26 |  7243 | `	if( nArg < 2 ){` |
|      ! 0 |  7244 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7245 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7246 | `	}` |
|       26 |  7247 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7248 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7249 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7250 | `	}` |
|       26 |  7251 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7252 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7253 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7254 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7255 | `	}` |
|        - |  7256 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7257 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7258 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7259 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7260 | `	}` |
|        - |  7261 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7262 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7263 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7264 | `	if( pAttr ){` |
|       26 |  7265 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7266 | `	}` |
|       26 |  7267 | `	return PH7_OK;` |
|       14 |  7268 |  |
|        - |  7269 | `/*` |
|        - |  7270 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7271 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7272 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7273 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7274 | ` */` |
|       24 |  7275 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7276 | `	ph7_class_instance **ppThis)` |
|        2 |  7277 |  |
|       26 |  7278 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7279 | `	ph7_value *pCallable;` |
|        - |  7280 | `	SyString sAttrName;` |
|       26 |  7281 | `	*ppThis = 0;` |
|       26 |  7282 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7283 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7284 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7285 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7286 | `		return 0;` |
|        - |  7287 | `	}` |
|       26 |  7288 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7289 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7290 | `		SyString sName;` |
|        - |  7291 | `		SyHashEntry *pEntry;` |
|        - |  7292 | `		ph7_vm_func *pFunc;` |
|       26 |  7293 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7294 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7295 | `		if( pEntry == 0 ){` |
|      ! 0 |  7296 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7297 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7298 | `			return 0;` |
|        - |  7299 | `		}` |
|       26 |  7300 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7301 | `		return pFunc;` |
|      ! 0 |  7302 | `	}else{` |
|        - |  7303 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7304 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7305 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7306 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7307 | `		if( pMethod == 0 ){` |
|      ! 0 |  7308 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7309 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7310 | `			return 0;` |
|        - |  7311 | `		}` |
|      ! 0 |  7312 | `		*ppThis = pClosure;` |
|      ! 0 |  7313 | `		return &pMethod->sFunc;` |
|        - |  7314 | `	}` |
|       14 |  7315 |  |
|        - |  7316 | `/*` |
|        - |  7317 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7318 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7319 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7320 | ` */` |
|       42 |  7321 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7322 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7323 |  |
|       44 |  7324 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7325 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7326 | `	sxu32 nFormal, n;` |
|        - |  7327 | `	VmSlot sSlot;` |
|        - |  7328 | `	sxi32 rc;` |
|        - |  7329 | `	/* Install $this for closure/method callables */` |
|       44 |  7330 | `	if( pClosureThis ){` |
|        - |  7331 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7332 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7333 | `		if( pObj ){` |
|      ! 0 |  7334 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7335 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7336 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7337 | `		}` |
|      ! 0 |  7338 | `	}` |
|        - |  7339 | `	/* Install static variables */` |
|       44 |  7340 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7341 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7342 | `		ph7_value *pVal;` |
|      ! 0 |  7343 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7344 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7345 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7346 | `			if( pVal ){` |
|      ! 0 |  7347 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7348 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7349 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7350 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7351 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7352 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7353 | `				}` |
|      ! 0 |  7354 | `			}` |
|      ! 0 |  7355 | `		}` |
|      ! 0 |  7356 | `	}` |
|        - |  7357 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7358 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7359 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7360 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7361 | `		ph7_value *pObj;` |
|       12 |  7362 | `		if( n < (sxu32)nArg ){` |
|        - |  7363 | `			/* Argument provided — install with type casting */` |
|       12 |  7364 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7365 | `			if( pObj ){` |
|       12 |  7366 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7367 | `				/* Type casting */` |
|       12 |  7368 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7369 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7370 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7371 | `						if( xCast ){` |
|      ! 0 |  7372 | `							xCast(pObj);` |
|      ! 0 |  7373 | `						}` |
|      ! 0 |  7374 | `					}` |
|      ! 0 |  7375 | `				}` |
|       12 |  7376 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7377 | `				sSlot.pUserData = 0;` |
|       12 |  7378 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7379 | `			}` |
|        5 |  7380 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7381 | `			/* Default value */` |
|      ! 0 |  7382 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7383 | `			if( pObj ){` |
|      ! 0 |  7384 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7385 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7386 | `					return rc;` |
|        - |  7387 | `				}` |
|      ! 0 |  7388 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7389 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7390 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7391 | `						if( xCast ){` |
|      ! 0 |  7392 | `							xCast(pObj);` |
|      ! 0 |  7393 | `						}` |
|      ! 0 |  7394 | `					}` |
|      ! 0 |  7395 | `				}` |
|      ! 0 |  7396 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7397 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7398 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7399 | `			}` |
|      ! 0 |  7400 | `		}` |
|        7 |  7401 | `	}` |
|        - |  7402 | `	/* Install closure environment (captured variables) */` |
|       44 |  7403 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7404 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7405 | `		ph7_value *pValue;` |
|        - |  7406 | `		sxu32 iEnv;` |
|        3 |  7407 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7408 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7409 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7410 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7411 | `				continue;` |
|        - |  7412 | `			}` |
|        5 |  7413 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7414 | `			if( pValue == 0 ){` |
|      ! 0 |  7415 | `				continue;` |
|        - |  7416 | `			}` |
|        5 |  7417 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7418 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7419 | `		}` |
|        1 |  7420 | `	}` |
|       44 |  7421 | `	return SXRET_OK;` |
|       23 |  7422 |  |
|        - |  7423 | `/*` |
|        - |  7424 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7425 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7426 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7427 | ` */` |
|       26 |  7428 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7429 |  |
|       28 |  7430 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7431 | `	ph7_class_instance *pThis;` |
|        - |  7432 | `	ph7_class_instance *pClosureThis;` |
|        - |  7433 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7434 | `	ph7_vm_func *pFunc;` |
|        - |  7435 | `	ph7_value sResult;` |
|        - |  7436 | `	ph7_value *pCtxAttr;` |
|        - |  7437 | `	SyString sAttrName;` |
|        - |  7438 | `	sxi32 rc;` |
|       28 |  7439 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7440 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7441 | `	}` |
|       28 |  7442 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7443 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7444 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7445 | `	if( pExecCtx != 0 ){` |
|        3 |  7446 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7447 | `			"Cannot start a fiber that has already been started");` |
|        - |  7448 | `	}` |
|        - |  7449 | `	/* Resolve callable */` |
|       26 |  7450 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7451 | `	if( pFunc == 0 ){` |
|      ! 0 |  7452 | `		return PH7_EXCEPTION;` |
|        - |  7453 | `	}` |
|        - |  7454 | `	/* Create execution context now that we know the function */` |
|       26 |  7455 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7456 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7457 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7458 | `			"Fiber::start(): out of memory");` |
|        - |  7459 | `	}` |
|        - |  7460 | `	/* Store context in $this->__ctx */` |
|       26 |  7461 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7462 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7463 | `	if( pCtxAttr ){` |
|       26 |  7464 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7465 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7466 | `	}` |
|        - |  7467 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7468 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7469 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7470 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7471 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7472 | `	/* Unpack the args array and install into the frame */` |
|        - |  7473 | `	{` |
|       26 |  7474 | `		ph7_value **apValues = 0;` |
|       26 |  7475 | `		int nActual = 0;` |
|       26 |  7476 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7477 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7478 | `			ph7_hashmap_node *pNode;` |
|       26 |  7479 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7480 | `			if( nCount > 0 ){` |
|        3 |  7481 | `				sxu32 idx = 0;` |
|        4 |  7482 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7483 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7484 | `				if( apValues ){` |
|        3 |  7485 | `					pNode = pMap->pFirst;` |
|        7 |  7486 | `					while( pNode && idx < nCount ){` |
|        5 |  7487 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7488 | `						idx++;` |
|        5 |  7489 | `						pNode = pNode->pPrev;` |
|        1 |  7490 | `					}` |
|        3 |  7491 | `					nActual = (int)idx;` |
|        1 |  7492 | `				}` |
|        1 |  7493 | `			}` |
|       12 |  7494 | `		}` |
|       26 |  7495 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7496 | `		if( apValues ){` |
|        3 |  7497 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7498 | `		}` |
|        - |  7499 | `	}` |
|        - |  7500 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7501 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7502 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7503 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7504 | `		return PH7_ABORT;` |
|        - |  7505 | `	}` |
|       26 |  7506 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7507 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7508 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7509 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7510 | `		return PH7_ABORT;` |
|        - |  7511 | `	}` |
|       26 |  7512 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7513 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7514 | `		return PH7_EXCEPTION;` |
|        - |  7515 | `	}` |
|       26 |  7516 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7517 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7518 | `	return PH7_OK;` |
|       15 |  7519 |  |
|        - |  7520 | `/*` |
|        - |  7521 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7522 | ` */` |
|       36 |  7523 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7524 |  |
|       38 |  7525 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7526 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7527 | `	ph7_value sResult;` |
|        - |  7528 | `	ph7_value *pResumeVal;` |
|        - |  7529 | `	sxi32 rc;` |
|       38 |  7530 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7531 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7532 | `		return PH7_OK;` |
|        - |  7533 | `	}` |
|       38 |  7534 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7535 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7536 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7537 | `		return PH7_OK;` |
|        - |  7538 | `	}` |
|       38 |  7539 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7540 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7541 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7542 | `	}` |
|       36 |  7543 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7544 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7545 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7546 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7547 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7548 | `		return PH7_ABORT;` |
|        - |  7549 | `	}` |
|       36 |  7550 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7551 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7552 | `		return PH7_EXCEPTION;` |
|        - |  7553 | `	}` |
|       36 |  7554 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7555 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7556 | `	return PH7_OK;` |
|       20 |  7557 |  |
|        - |  7558 | `/*` |
|        - |  7559 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7560 | ` */` |
|        6 |  7561 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7562 |  |
|        8 |  7563 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7564 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7565 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7566 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7567 | `		return PH7_OK;` |
|        - |  7568 | `	}` |
|        8 |  7569 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7570 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7571 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7572 | `		return PH7_OK;` |
|        - |  7573 | `	}` |
|        8 |  7574 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7575 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7576 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7577 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7578 | `		}` |
|      ! 0 |  7579 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7580 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7581 | `	}` |
|        8 |  7582 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7583 | `	return PH7_OK;` |
|        5 |  7584 |  |
|        - |  7585 | `/*` |
|        - |  7586 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7587 | ` */` |
|        6 |  7588 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7589 |  |
|        - |  7590 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7591 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7592 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7593 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7594 | `	return PH7_OK;` |
|        4 |  7595 |  |
|      ! 0 |  7596 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7597 |  |
|        - |  7598 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7599 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7600 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7601 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7602 | `	return PH7_OK;` |
|      ! 0 |  7603 |  |
|        6 |  7604 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7605 |  |
|        - |  7606 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7607 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7608 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7609 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7610 | `	return PH7_OK;` |
|        4 |  7611 |  |
|        6 |  7612 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7613 |  |
|        - |  7614 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7615 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7616 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7617 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7618 | `	return PH7_OK;` |
|        4 |  7619 |  |
|        - |  7620 | `/*` |
|        - |  7621 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7622 | ` */` |
|        4 |  7623 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7624 |  |
|        5 |  7625 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7626 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7627 | `	if( nArg < 1 ){` |
|      ! 0 |  7628 | `		return PH7_OK;` |
|        - |  7629 | `	}` |
|        5 |  7630 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7631 | `	if( pExecCtx ){` |
|        5 |  7632 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7633 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7634 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7635 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7636 | `			SyString sAttrName;` |
|        - |  7637 | `			ph7_value *pAttr;` |
|        5 |  7638 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7639 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7640 | `			if( pAttr ){` |
|        5 |  7641 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7642 | `			}` |
|        2 |  7643 | `		}` |
|        2 |  7644 | `	}` |
|        5 |  7645 | `	return PH7_OK;` |
|        3 |  7646 |  |
|        - |  7647 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7648 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7649 |  |
|        - |  7650 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7651 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7652 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7653 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7654 |  |
|      ! 0 |  7655 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7656 |  |
|        - |  7657 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7658 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7659 | `	ph7_exec_ctx *pCtx;` |
|        - |  7660 | `	ph7_vm_func *pFunc;` |
|        - |  7661 | `	ph7_value *pCallable;` |
|        - |  7662 | `	ph7_value *pCtxAttr;` |
|        - |  7663 | `	SyString sAttrName;` |
|        - |  7664 | `	/* Must not already be started */` |
|      ! 0 |  7665 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7666 | `	if( pCtx != 0 ){` |
|      ! 0 |  7667 | `		return SXERR_INVALID;` |
|        - |  7668 | `	}` |
|      ! 0 |  7669 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7670 | `		return SXERR_INVALID;` |
|        - |  7671 | `	}` |
|      ! 0 |  7672 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7673 | `	/* Get the callable */` |
|      ! 0 |  7674 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7675 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7676 | `	if( pCallable == 0 ){` |
|      ! 0 |  7677 | `		return SXERR_INVALID;` |
|        - |  7678 | `	}` |
|        - |  7679 | `	/* Resolve callable */` |
|      ! 0 |  7680 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7681 | `		SyString sName;` |
|        - |  7682 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7683 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7684 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7685 | `		if( pEntry == 0 ){` |
|      ! 0 |  7686 | `			return SXERR_NOTFOUND;` |
|        - |  7687 | `		}` |
|      ! 0 |  7688 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7689 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7690 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7691 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7692 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7693 | `		if( pMethod == 0 ){` |
|      ! 0 |  7694 | `			return SXERR_INVALID;` |
|        - |  7695 | `		}` |
|      ! 0 |  7696 | `		pClosureThis = pClosure;` |
|      ! 0 |  7697 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7698 | `	}else{` |
|      ! 0 |  7699 | `		return SXERR_INVALID;` |
|        - |  7700 | `	}` |
|        - |  7701 | `	/* Create context */` |
|      ! 0 |  7702 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7703 | `	if( pCtx == 0 ){` |
|      ! 0 |  7704 | `		return SXERR_MEM;` |
|        - |  7705 | `	}` |
|        - |  7706 | `	/* Store in __ctx */` |
|      ! 0 |  7707 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7708 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7709 | `	if( pCtxAttr ){` |
|      ! 0 |  7710 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7711 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7712 | `	}` |
|        - |  7713 | `	/* Set up frame with args */` |
|      ! 0 |  7714 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7715 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7716 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7717 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7718 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7719 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7720 |  |
|      ! 0 |  7721 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7722 |  |
|      ! 0 |  7723 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7724 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7725 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7726 |  |
|      ! 0 |  7727 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7728 |  |
|      ! 0 |  7729 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7730 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7731 |  |
|      ! 0 |  7732 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7733 |  |
|      ! 0 |  7734 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7735 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7736 |  |
|      ! 0 |  7737 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7738 |  |
|      ! 0 |  7739 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7740 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7741 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7742 |  |
|        - |  7743 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7744 | `/*` |
|        - |  7745 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7746 | ` */` |
|       18 |  7747 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7748 |  |
|        - |  7749 | `	ph7_generator *pGen;` |
|       20 |  7750 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7751 | `	if( pGen == 0 ){` |
|      ! 0 |  7752 | `		return 0;` |
|        - |  7753 | `	}` |
|       20 |  7754 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7755 | `	pGen->pCtx = pCtx;` |
|       20 |  7756 | `	pGen->iImplicitKey = 0;` |
|       20 |  7757 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7758 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7759 | `	/* Link the generator back to the exec context */` |
|       20 |  7760 | `	pCtx->pPrivate = pGen;` |
|       20 |  7761 | `	return pGen;` |
|       11 |  7762 |  |
|        - |  7763 | `/*` |
|        - |  7764 | ` * Release a generator and its execution context.` |
|        - |  7765 | ` */` |
|      ! 0 |  7766 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7767 |  |
|      ! 0 |  7768 | `	if( pGen == 0 ){` |
|      ! 0 |  7769 | `		return;` |
|        - |  7770 | `	}` |
|      ! 0 |  7771 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7772 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7773 | `	if( pGen->pCtx ){` |
|      ! 0 |  7774 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7775 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7776 | `		pGen->pCtx = 0;` |
|      ! 0 |  7777 | `	}` |
|      ! 0 |  7778 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7779 |  |
|        - |  7780 | `/*` |
|        - |  7781 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7782 | ` */` |
|      192 |  7783 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7784 |  |
|        - |  7785 | `	ph7_class_instance *pThis;` |
|        - |  7786 | `	SyString sAttr;` |
|        - |  7787 | `	ph7_value *pAttr;` |
|      194 |  7788 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7789 | `		return 0;` |
|        - |  7790 | `	}` |
|      194 |  7791 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7792 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7793 | `		return 0;` |
|        - |  7794 | `	}` |
|      194 |  7795 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7796 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7797 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7798 | `		return 0;` |
|        - |  7799 | `	}` |
|      194 |  7800 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7801 |  |
|        - |  7802 | `/*` |
|        - |  7803 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7804 | ` */` |
|       18 |  7805 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7806 |  |
|        - |  7807 | `	ph7_generator *pGen;` |
|        - |  7808 | `	sxi32 rc;` |
|       20 |  7809 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7810 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7811 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7812 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7813 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7814 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7815 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7816 | `	}` |
|       20 |  7817 | `	return PH7_OK;` |
|       11 |  7818 |  |
|        - |  7819 | `/*` |
|        - |  7820 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7821 | ` */` |
|       52 |  7822 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7823 |  |
|        - |  7824 | `	ph7_generator *pGen;` |
|       54 |  7825 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7826 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7827 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7828 | `	return PH7_OK;` |
|       28 |  7829 |  |
|        - |  7830 | `/*` |
|        - |  7831 | ` * Generator::current() — return the last yielded value.` |
|        - |  7832 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7833 | ` */` |
|       56 |  7834 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7835 |  |
|        - |  7836 | `	ph7_generator *pGen;` |
|        - |  7837 | `	sxi32 rc;` |
|       58 |  7838 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7839 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7840 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7841 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7842 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7843 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7844 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7845 | `	}` |
|       58 |  7846 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7847 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7848 | `	}else{` |
|      ! 0 |  7849 | `		ph7_result_null(pCtx);` |
|        - |  7850 | `	}` |
|       58 |  7851 | `	return PH7_OK;` |
|       30 |  7852 |  |
|        - |  7853 | `/*` |
|        - |  7854 | ` * Generator::key() — return the last yielded key.` |
|        - |  7855 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7856 | ` */` |
|       12 |  7857 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7858 |  |
|        - |  7859 | `	ph7_generator *pGen;` |
|        - |  7860 | `	sxi32 rc;` |
|       13 |  7861 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7862 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7863 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7864 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7865 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7866 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7867 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7868 | `	}` |
|       13 |  7869 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7870 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7871 | `	}else{` |
|      ! 0 |  7872 | `		ph7_result_null(pCtx);` |
|        - |  7873 | `	}` |
|       13 |  7874 | `	return PH7_OK;` |
|        7 |  7875 |  |
|        - |  7876 | `/*` |
|        - |  7877 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7878 | ` */` |
|       48 |  7879 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7880 |  |
|        - |  7881 | `	ph7_generator *pGen;` |
|        - |  7882 | `	sxi32 rc;` |
|       50 |  7883 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7884 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7885 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7886 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7887 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7888 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7889 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7890 | `	}else{` |
|      ! 0 |  7891 | `		return PH7_OK;` |
|        - |  7892 | `	}` |
|       50 |  7893 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7894 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7895 | `	return PH7_OK;` |
|       26 |  7896 |  |
|        - |  7897 | `/*` |
|        - |  7898 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7899 | ` */` |
|        4 |  7900 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7901 |  |
|        - |  7902 | `	ph7_generator *pGen;` |
|        - |  7903 | `	ph7_value *pSendVal;` |
|        - |  7904 | `	sxi32 rc;` |
|        5 |  7905 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7906 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7907 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7908 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7909 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7910 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7911 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7912 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7913 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7914 | `	}else{` |
|      ! 0 |  7915 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7916 | `		return PH7_OK;` |
|        - |  7917 | `	}` |
|        5 |  7918 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7919 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7920 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7921 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7922 | `	}else{` |
|        3 |  7923 | `		ph7_result_null(pCtx);` |
|        - |  7924 | `	}` |
|        5 |  7925 | `	return PH7_OK;` |
|        3 |  7926 |  |
|        - |  7927 | `/*` |
|        - |  7928 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7929 | ` *` |
|        - |  7930 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7931 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7932 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7933 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7934 | ` * the exception to the caller.` |
|        - |  7935 | ` */` |
|      ! 0 |  7936 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7937 |  |
|        - |  7938 | `	ph7_generator *pGen;` |
|        - |  7939 | `	const char *zMsg;` |
|        - |  7940 | `	int nLen;` |
|      ! 0 |  7941 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7942 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7943 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7944 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7945 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7946 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7947 | `			"Cannot throw into a closed generator");` |
|        - |  7948 | `	}` |
|        - |  7949 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7950 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7951 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7952 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7953 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7954 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7955 | `	nLen = 0;` |
|      ! 0 |  7956 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7957 | `		/* Try to get the exception's message */` |
|        - |  7958 | `		SyString sAttr;` |
|        - |  7959 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7960 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7961 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7962 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7963 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7964 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7965 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7966 | `		}` |
|      ! 0 |  7967 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7968 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7969 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7970 | `	}` |
|      ! 0 |  7971 | `	(void)nLen;` |
|      ! 0 |  7972 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7973 |  |
|        - |  7974 | `/*` |
|        - |  7975 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7976 | ` */` |
|        2 |  7977 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7978 |  |
|        - |  7979 | `	ph7_generator *pGen;` |
|        3 |  7980 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7981 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7982 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7983 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7984 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7985 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7986 | `	}` |
|        3 |  7987 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7988 | `	return PH7_OK;` |
|        2 |  7989 |  |
|        - |  7990 | `/*` |
|        - |  7991 | ` * Generator::__destruct() — clean up.` |
|        - |  7992 | ` */` |
|      ! 0 |  7993 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7994 |  |
|        - |  7995 | `	ph7_generator *pGen;` |
|      ! 0 |  7996 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7997 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7998 | `	if( pGen ){` |
|      ! 0 |  7999 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8000 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8001 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8002 | `			SyString sAttrName;` |
|        - |  8003 | `			ph7_value *pAttr;` |
|      ! 0 |  8004 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8005 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8006 | `			if( pAttr ){` |
|      ! 0 |  8007 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8008 | `			}` |
|      ! 0 |  8009 | `		}` |
|      ! 0 |  8010 | `	}` |
|      ! 0 |  8011 | `	return PH7_OK;` |
|      ! 0 |  8012 |  |
|        - |  8013 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8014 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8015 | `/*` |
|        - |  8016 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8017 | ` * the desired message.` |
|        - |  8018 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8019 | ` * in 'api.c' for additional information.` |
|        - |  8020 | ` */` |
|      370 |  8021 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8022 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8023 | `	SyString *pString /* Message to output */` |
|        - |  8024 | `	)` |
|        2 |  8025 |  |
|      372 |  8026 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8027 | `	sxi32 rc = SXRET_OK;` |
|        - |  8028 | `	/* Call the output consumer */` |
|      372 |  8029 | `	if( pString->nByte > 0 ){` |
|      372 |  8030 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8031 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8032 | `	}` |
|      372 |  8033 | `	return rc;` |
|        2 |  8034 |  |
|        - |  8035 | `/*` |
|        - |  8036 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8037 | ` * callback to consume the formatted message.` |
|        - |  8038 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8039 | ` * in 'api.c' for additional information.` |
|        - |  8040 | ` */` |
|        2 |  8041 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8042 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8043 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8044 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8045 | `	)` |
|        1 |  8046 |  |
|        3 |  8047 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8048 | `	sxi32 rc = SXRET_OK;` |
|        - |  8049 | `	SyBlob sWorker;` |
|        - |  8050 | `	/* Format the message and call the output consumer */` |
|        3 |  8051 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8052 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8053 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8054 | `		/* Consume the formatted message */` |
|        3 |  8055 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8056 | `	}` |
|        3 |  8057 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8058 | `	/* Release the working buffer */` |
|        3 |  8059 | `	SyBlobRelease(&sWorker);` |
|        3 |  8060 | `	return rc;` |
|        1 |  8061 |  |
|        - |  8062 | `/*` |
|        - |  8063 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8064 | ` * This function never fail and always return a pointer` |
|        - |  8065 | ` * to a null terminated string.` |
|        - |  8066 | ` */` |
|       12 |  8067 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8068 |  |
|       13 |  8069 | `	const char *zOp = "Unknown     ";` |
|       13 |  8070 | `	switch(nOp){` |
|        3 |  8071 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8072 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8073 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8074 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8075 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8076 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8077 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8078 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8079 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8080 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8081 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8082 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8083 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8084 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8085 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8086 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8087 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8088 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8089 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8090 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8091 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8092 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8093 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8094 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8095 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8096 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8097 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8098 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8099 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8100 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8101 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8102 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8103 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8104 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8105 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8106 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8107 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8108 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8109 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8110 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8111 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8112 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8113 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8114 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8115 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8116 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8117 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8118 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8119 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8120 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8121 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8122 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8123 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8124 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8125 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8126 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8127 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8128 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8129 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8130 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8131 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8132 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8133 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8134 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8135 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8136 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8137 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8138 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8139 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8140 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8141 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8142 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8143 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8144 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8145 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8146 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8147 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8148 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8149 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8150 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8151 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8152 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8153 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8154 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8155 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8156 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8157 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8158 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8159 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8160 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8161 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8162 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8163 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8164 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8165 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8166 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8167 | `	default:` |
|      ! 0 |  8168 | `		break;` |
|        - |  8169 | `	}` |
|       13 |  8170 | `	return zOp;` |
|        1 |  8171 |  |
|        - |  8172 | `/*` |
|        - |  8173 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8174 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8175 | ` * is responsible of consuming the generated dump.` |
|        - |  8176 | ` */` |
|        2 |  8177 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8178 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8179 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8180 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8181 | `	)` |
|        1 |  8182 |  |
|        - |  8183 | `	sxi32 rc;` |
|        3 |  8184 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8185 | `	return rc;` |
|        1 |  8186 |  |
|        - |  8187 | `/*` |
|        - |  8188 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8189 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8190 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8191 | ` * in 'compile.c' for additional information.` |
|        - |  8192 | ` */` |
|       14 |  8193 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8194 |  |
|       15 |  8195 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8196 | `	/* Evaluate and expand constant value */` |
|       15 |  8197 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8198 |  |
|        - |  8199 | `/*` |
|        - |  8200 | ` * Section:` |
|        - |  8201 | ` *  Function handling functions.` |
|        - |  8202 | ` * Status:` |
|        - |  8203 | ` *    Stable.` |
|        - |  8204 | ` */` |
|        - |  8205 | `/*` |
|        - |  8206 | ` * int func_num_args(void)` |
|        - |  8207 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8208 | ` * Parameters` |
|        - |  8209 | ` *   None.` |
|        - |  8210 | ` * Return` |
|        - |  8211 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8212 | ` *  or -1 if called from the globe scope.` |
|        - |  8213 | ` */` |
|      944 |  8214 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8215 |  |
|        - |  8216 | `	VmFrame *pFrame;` |
|        - |  8217 | `	ph7_vm *pVm;` |
|        - |  8218 | `	/* Point to the target VM */` |
|      946 |  8219 | `	pVm = pCtx->pVm;` |
|        - |  8220 | `	/* Current frame */` |
|      946 |  8221 | `	pFrame = pVm->pFrame;` |
|      946 |  8222 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8223 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8224 | `		SXUNUSED(nArg);` |
|      ! 0 |  8225 | `		SXUNUSED(apArg);` |
|        - |  8226 | `		/* Global frame,return -1 */` |
|      ! 0 |  8227 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8228 | `		return SXRET_OK;` |
|        - |  8229 | `	}` |
|        - |  8230 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8231 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8232 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8233 | `	return SXRET_OK;` |
|      474 |  8234 |  |
|        - |  8235 | `/*` |
|        - |  8236 | ` * value func_get_arg(int $arg_num)` |
|        - |  8237 | ` *   Return an item from the argument list.` |
|        - |  8238 | ` * Parameters` |
|        - |  8239 | ` *  Argument number(index start from zero).` |
|        - |  8240 | ` * Return` |
|        - |  8241 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8242 | ` */` |
|       22 |  8243 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8244 |  |
|       24 |  8245 | `	ph7_value *pObj = 0;` |
|       24 |  8246 | `	VmSlot *pSlot = 0;` |
|        - |  8247 | `	VmFrame *pFrame;` |
|        - |  8248 | `	ph7_vm *pVm;` |
|        - |  8249 | `	/* Point to the target VM */` |
|       24 |  8250 | `	pVm = pCtx->pVm;` |
|        - |  8251 | `	/* Current frame */` |
|       24 |  8252 | `	pFrame = pVm->pFrame;` |
|       24 |  8253 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8254 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8255 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8256 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8257 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8258 | `		return SXRET_OK;` |
|        - |  8259 | `	}` |
|        - |  8260 | `	/* Extract the desired index */` |
|       21 |  8261 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8262 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8263 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8264 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8265 | `		return SXRET_OK;` |
|        - |  8266 | `	}` |
|        - |  8267 | `	/* Extract the desired argument */` |
|       21 |  8268 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8269 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8270 | `			/* Return the desired argument */` |
|       21 |  8271 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8272 | `		}else{` |
|        - |  8273 | `			/* No such argument,return false */` |
|      ! 0 |  8274 | `			ph7_result_bool(pCtx,0);` |
|        - |  8275 | `		}` |
|       11 |  8276 | `	}else{` |
|        - |  8277 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8278 | `		ph7_result_bool(pCtx,0);` |
|        - |  8279 | `	}` |
|       21 |  8280 | `	return SXRET_OK;` |
|       13 |  8281 |  |
|        - |  8282 | `/*` |
|        - |  8283 | ` * array func_get_args_byref(void)` |
|        - |  8284 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8285 | ` * Parameters` |
|        - |  8286 | ` *  None.` |
|        - |  8287 | ` * Return` |
|        - |  8288 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8289 | ` *  member of the current user-defined function's argument list.` |
|        - |  8290 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8291 | ` * NOTE:` |
|        - |  8292 | ` *  Arguments are returned to the array by reference.` |
|        - |  8293 | ` */` |
|        2 |  8294 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8295 |  |
|        - |  8296 | `	ph7_value *pArray;` |
|        - |  8297 | `	VmFrame *pFrame;` |
|        - |  8298 | `	VmSlot *aSlot;` |
|        - |  8299 | `	sxu32 n;` |
|        - |  8300 | `	/* Point to the current frame */` |
|        3 |  8301 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8302 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8303 | `	if( pFrame->pParent == 0 ){` |
|        - |  8304 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8305 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8306 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8307 | `		return SXRET_OK;` |
|        - |  8308 | `	}` |
|        - |  8309 | `	/* Create a new array */` |
|        3 |  8310 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8311 | `	if( pArray == 0 ){` |
|      ! 0 |  8312 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8313 | `		SXUNUSED(apArg);` |
|      ! 0 |  8314 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8315 | `		return SXRET_OK;` |
|        - |  8316 | `	}` |
|        - |  8317 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8318 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8319 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8320 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8321 | `	}` |
|        - |  8322 | `	/* Return the freshly created array */` |
|        3 |  8323 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8324 | `	return SXRET_OK;` |
|        2 |  8325 |  |
|        - |  8326 | `/*` |
|        - |  8327 | ` * array func_get_args(void)` |
|        - |  8328 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8329 | ` * Parameters` |
|        - |  8330 | ` *  None.` |
|        - |  8331 | ` * Return` |
|        - |  8332 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8333 | ` *  member of the current user-defined function's argument list.` |
|        - |  8334 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8335 | ` */` |
|       88 |  8336 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8337 |  |
|       90 |  8338 | `	ph7_value *pObj = 0;` |
|        - |  8339 | `	ph7_value *pArray;` |
|        - |  8340 | `	VmFrame *pFrame;` |
|        - |  8341 | `	VmSlot *aSlot;` |
|        - |  8342 | `	sxu32 n;` |
|        - |  8343 | `	/* Point to the current frame */` |
|       90 |  8344 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8345 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8346 | `	if( pFrame->pParent == 0 ){` |
|        - |  8347 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8348 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8349 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8350 | `		return SXRET_OK;` |
|        - |  8351 | `	}` |
|        - |  8352 | `	/* Create a new array */` |
|       90 |  8353 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8354 | `	if( pArray == 0 ){` |
|      ! 0 |  8355 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8356 | `		SXUNUSED(apArg);` |
|      ! 0 |  8357 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8358 | `		return SXRET_OK;` |
|        - |  8359 | `	}` |
|        - |  8360 | `	/* Start filling the array with the given arguments */` |
|       90 |  8361 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8362 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8363 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8364 | `		if( pObj ){` |
|      134 |  8365 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8366 | `		}` |
|       68 |  8367 | `	}` |
|        - |  8368 | `	/* Return the freshly created array */` |
|       90 |  8369 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8370 | `	return SXRET_OK;` |
|       46 |  8371 |  |
|        - |  8372 | `/*` |
|        - |  8373 | ` * bool function_exists(string $name)` |
|        - |  8374 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8375 | ` * Parameters` |
|        - |  8376 | ` *  The name of the desired function.` |
|        - |  8377 | ` * Return` |
|        - |  8378 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8379 | ` */` |
|     1682 |  8380 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8381 |  |
|        - |  8382 | `	const char *zName;` |
|        - |  8383 | `	ph7_vm *pVm;` |
|        - |  8384 | `	int nLen;` |
|        - |  8385 | `	int res;` |
|     1684 |  8386 | `	if( nArg < 1 ){` |
|        - |  8387 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8388 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8389 | `		return SXRET_OK;` |
|        - |  8390 | `	}` |
|        - |  8391 | `	/* Point to the target VM */` |
|     1684 |  8392 | `	pVm = pCtx->pVm;` |
|        - |  8393 | `	/* Extract the function name */` |
|     1684 |  8394 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8395 | `	/* Assume the function is not defined */` |
|     1684 |  8396 | `	res = 0;` |
|        - |  8397 | `	/* Perform the lookup */` |
|     2523 |  8398 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8399 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8400 | `			/* Function is defined */` |
|      206 |  8401 | `			res = 1;` |
|      102 |  8402 | `	}` |
|     1684 |  8403 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8404 | `	return SXRET_OK;` |
|      843 |  8405 |  |
|        - |  8406 | `/*` |
|        - |  8407 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8408 | ` * [i.e: Whether it is callable or not].` |
|        - |  8409 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8410 | ` */` |
|    17706 |  8411 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8412 |  |
|    17708 |  8413 | `	int res = 0;` |
|    17708 |  8414 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8415 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8416 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8417 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8418 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8419 | `		if( pMethod && CallInvoke ){` |
|        - |  8420 | `			ph7_value sResult;` |
|        - |  8421 | `			sxi32 rc;` |
|        - |  8422 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8423 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8424 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8425 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8426 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8427 | `			}` |
|      ! 0 |  8428 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8429 | `		}` |
|    17708 |  8430 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8431 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8432 | `		if( pMap->nEntry == 2 ){` |
|        - |  8433 | `			ph7_class *pClass;` |
|        - |  8434 | `			ph7_value *pV;` |
|        - |  8435 | `			/* Extract the target class */` |
|       12 |  8436 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8437 | `			if( pV ){` |
|       12 |  8438 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8439 | `				if( pClass ){` |
|        - |  8440 | `					ph7_class_method *pMethod;` |
|        - |  8441 | `					/* Extract the target method */` |
|       10 |  8442 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8443 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8444 | `						/* Perform the lookup */` |
|       10 |  8445 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8446 | `						if( pMethod ){` |
|        - |  8447 | `							/* Method is callable */` |
|        5 |  8448 | `							res = 1;` |
|        2 |  8449 | `						}` |
|        4 |  8450 | `					}` |
|        4 |  8451 | `				}` |
|        5 |  8452 | `			}` |
|        7 |  8453 | `		}` |
|    17695 |  8454 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8455 | `		const char *zName;` |
|        - |  8456 | `		int nLen;` |
|        - |  8457 | `		/* Extract the name */` |
|     5018 |  8458 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8459 | `		/* Perform the lookup */` |
|     5033 |  8460 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8461 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8462 | `				/* Function is callable */` |
|     5000 |  8463 | `				res = 1;` |
|     2499 |  8464 | `		}` |
|     2508 |  8465 | `	}` |
|    17708 |  8466 | `	return res;` |
|        2 |  8467 |  |
|        - |  8468 | `/*` |
|        - |  8469 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8470 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8471 | ` * Parameters` |
|        - |  8472 | ` * $name` |
|        - |  8473 | ` *    The callback function to check` |
|        - |  8474 | ` * $syntax_only` |
|        - |  8475 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8476 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8477 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8478 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8479 | ` *    a string.` |
|        - |  8480 | ` * Return` |
|        - |  8481 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8482 | ` */` |
|       14 |  8483 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8484 |  |
|        - |  8485 | `	ph7_vm *pVm;` |
|        - |  8486 | `	int res;` |
|       15 |  8487 | `	if( nArg < 1 ){` |
|        - |  8488 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8489 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8490 | `		return SXRET_OK;` |
|        - |  8491 | `	}` |
|        - |  8492 | `	/* Point to the target VM */` |
|       15 |  8493 | `	pVm = pCtx->pVm;` |
|        - |  8494 | `	/* Perform the requested operation */` |
|       15 |  8495 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8496 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8497 | `	return SXRET_OK;` |
|        8 |  8498 |  |
|        - |  8499 | `/*` |
|        - |  8500 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8501 | ` * defined below.` |
|        - |  8502 | ` */` |
|     1200 |  8503 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8504 |  |
|     1201 |  8505 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8506 | `	ph7_value sName;` |
|        - |  8507 | `	sxi32 rc;` |
|        - |  8508 | `	/* Prepare the function name for insertion */` |
|     1201 |  8509 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  8510 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8511 | `	/* Perform the insertion */` |
|     1201 |  8512 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  8513 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  8514 | `	return rc;` |
|        1 |  8515 |  |
|        - |  8516 | `/*` |
|        - |  8517 | ` * array get_defined_functions(void)` |
|        - |  8518 | ` *  Returns an array of all defined functions.` |
|        - |  8519 | ` * Parameter` |
|        - |  8520 | ` *  None.` |
|        - |  8521 | ` * Return` |
|        - |  8522 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8523 | ` *  both built-in (internal) and user-defined.` |
|        - |  8524 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8525 | ` *  defined ones using $arr["user"].` |
|        - |  8526 | ` * Note:` |
|        - |  8527 | ` *  NULL is returned on failure.` |
|        - |  8528 | ` */` |
|        2 |  8529 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8530 |  |
|        - |  8531 | `	ph7_value *pArray,*pEntry;` |
|        - |  8532 | `	/* NOTE:` |
|        - |  8533 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8534 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8535 | `	 */` |
|        3 |  8536 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8537 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8538 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8539 | `		SXUNUSED(apArg);` |
|        - |  8540 | `		/* Return NULL */` |
|      ! 0 |  8541 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8542 | `		return SXRET_OK;` |
|        - |  8543 | `	}` |
|        3 |  8544 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8545 | `	if( pEntry == 0 ){` |
|        - |  8546 | `		/* Return NULL */` |
|      ! 0 |  8547 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8548 | `		return SXRET_OK;` |
|        - |  8549 | `	}` |
|        - |  8550 | `	/* Fill with the appropriate information */` |
|        3 |  8551 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8552 | `	/* Create the 'internal' index */` |
|        3 |  8553 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8554 | `	/* Create the user-func array */` |
|        3 |  8555 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8556 | `	if( pEntry == 0 ){` |
|        - |  8557 | `		/* Return NULL */` |
|      ! 0 |  8558 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8559 | `		return SXRET_OK;` |
|        - |  8560 | `	}` |
|        - |  8561 | `	/* Fill with the appropriate information */` |
|        3 |  8562 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8563 | `	/* Create the 'user' index */` |
|        3 |  8564 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8565 | `	/* Return the multi-dimensional array */` |
|        3 |  8566 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8567 | `	return SXRET_OK;` |
|        2 |  8568 |  |
|        - |  8569 | `/*` |
|        - |  8570 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8571 | ` *  Register a function for execution on shutdown.` |
|        - |  8572 | ` * Note` |
|        - |  8573 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8574 | ` *  be called in the same order as they were registered.` |
|        - |  8575 | ` * Parameters` |
|        - |  8576 | ` *  $callback` |
|        - |  8577 | ` *   The shutdown callback to register.` |
|        - |  8578 | ` * $param` |
|        - |  8579 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8580 | ` * Return` |
|        - |  8581 | ` *  Nothing.` |
|        - |  8582 | ` */` |
|        2 |  8583 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8584 |  |
|        - |  8585 | `	VmShutdownCB sEntry;` |
|        - |  8586 | `	int i,j;` |
|        3 |  8587 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8588 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8589 | `		return PH7_OK;` |
|        - |  8590 | `	}` |
|        - |  8591 | `	/* Zero the Entry */` |
|        3 |  8592 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8593 | `	/* Initialize fields */` |
|        3 |  8594 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8595 | `	/* Save the callback name for later invocation name */` |
|        3 |  8596 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8597 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8598 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8599 | `	}` |
|        - |  8600 | `	/* Copy arguments */` |
|        3 |  8601 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8602 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8603 | `			/* Limit reached */` |
|      ! 0 |  8604 | `			break;` |
|        - |  8605 | `		}` |
|      ! 0 |  8606 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8607 | `	}` |
|        3 |  8608 | `	sEntry.nArg = j;` |
|        - |  8609 | `	/* Install the callback */` |
|        3 |  8610 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8611 | `	return PH7_OK;` |
|        2 |  8612 |  |
|        - |  8613 | `/*` |
|        - |  8614 | ` * Section:` |
|        - |  8615 | ` *  Class handling functions.` |
|        - |  8616 | ` * Status:` |
|        - |  8617 | ` *    Stable.` |
|        - |  8618 | ` */` |
|        - |  8619 | `/*` |
|        - |  8620 | ` * Extract the top active class. NULL is returned` |
|        - |  8621 | ` * if the class stack is empty.` |
|        - |  8622 | ` */` |
|      574 |  8623 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8624 |  |
|      576 |  8625 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8626 | `	ph7_class **apClass;` |
|      576 |  8627 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8628 | `		/* Empty stack,return NULL */` |
|       15 |  8629 | `		return 0;` |
|        - |  8630 | `	}` |
|        - |  8631 | `	/* Peek the last entry */` |
|      562 |  8632 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      562 |  8633 | `	return apClass[pSet->nUsed - 1];` |
|      289 |  8634 |  |
|        - |  8635 | `/*` |
|        - |  8636 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8637 | ` *   Get the class that declared the currently executing method.` |
|        - |  8638 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8639 | ` *` |
|        - |  8640 | ` * Parameters` |
|        - |  8641 | ` *   pVm: Target VM` |
|        - |  8642 | ` *` |
|        - |  8643 | ` * Return` |
|        - |  8644 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8645 | ` *   - Not executing within a class method` |
|        - |  8646 | ` *` |
|        - |  8647 | ` * Note` |
|        - |  8648 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8649 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8650 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8651 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8652 | ` *   declaring class.` |
|        - |  8653 | ` */` |
|       60 |  8654 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8655 |  |
|       62 |  8656 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8657 | `	ph7_vm_func *pVmFunc;` |
|        - |  8658 |  |
|        - |  8659 | `	/* Skip exception frames to find the actual method frame */` |
|       62 |  8660 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8661 |  |
|        - |  8662 | `	/* Check if we're in a method context */` |
|       62 |  8663 | `	if( pFrame->pParent ){` |
|       58 |  8664 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       58 |  8665 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8666 | `			/* Return the declaring class */` |
|       58 |  8667 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8668 | `		}` |
|      ! 0 |  8669 | `	}` |
|        - |  8670 |  |
|        5 |  8671 | `	return 0;` |
|       32 |  8672 |  |
|        - |  8673 |  |
|        - |  8674 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8675 | `/*` |
|        - |  8676 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8677 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8678 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8679 | ` * return value indicates failure.` |
|        - |  8680 | ` */` |
|     1508 |  8681 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8682 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8683 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8684 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8685 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8686 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8687 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8688 | `	)` |
|        2 |  8689 |  |
|        - |  8690 | `	ph7_value *aStack;` |
|        - |  8691 | `	VmInstr aInstr[2];` |
|        - |  8692 | `	int iCursor;` |
|        - |  8693 | `	int i;` |
|        - |  8694 | `	/* Create a new operand stack */` |
|     1510 |  8695 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1510 |  8696 | `	if( aStack == 0 ){` |
|      ! 0 |  8697 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8698 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8699 | `		return SXERR_MEM;` |
|        - |  8700 | `	}` |
|        - |  8701 | `	/* Fill the operand stack with the given arguments */` |
|     2124 |  8702 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      616 |  8703 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8704 | `		/*` |
|        - |  8705 | `		 * Symisc eXtension:` |
|        - |  8706 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8707 | `		 */` |
|      616 |  8708 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      309 |  8709 | `	}` |
|     1510 |  8710 | `	iCursor = nArg + 1;` |
|     1510 |  8711 | `	if( pThis ){` |
|        - |  8712 | `		/*` |
|        - |  8713 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8714 | `		 */` |
|     1504 |  8715 | `		pThis->iRef++; /* Increment reference count */` |
|     1504 |  8716 | `		aStack[i].x.pOther = pThis;` |
|     1504 |  8717 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      751 |  8718 | `	}` |
|     1510 |  8719 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1510 |  8720 | `	i++;` |
|        - |  8721 | `	/* Push method name */` |
|     1510 |  8722 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1510 |  8723 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1510 |  8724 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1510 |  8725 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8726 | `	/* Emit the CALL istruction */` |
|     1510 |  8727 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1510 |  8728 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1510 |  8729 | `	aInstr[0].iP2 = 0;` |
|     1510 |  8730 | `	aInstr[0].p3  = 0;` |
|        - |  8731 | `	/* Emit the DONE instruction */` |
|     1510 |  8732 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1510 |  8733 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1510 |  8734 | `	aInstr[1].iP2 = 0;` |
|     1510 |  8735 | `	aInstr[1].p3  = 0;` |
|        - |  8736 | `	/* Execute the method body (if available) */` |
|     1510 |  8737 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8738 | `	/* Clean up the mess left behind */` |
|     1510 |  8739 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1510 |  8740 | `	return PH7_OK;` |
|      756 |  8741 |  |
|        - |  8742 | `/*` |
|        - |  8743 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8744 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8745 | ` * in the apArg[] array.` |
|        - |  8746 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8747 | ` * return value indicates failure.` |
|        - |  8748 | ` */` |
|      960 |  8749 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8750 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8751 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8752 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8753 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8754 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8755 | `	)` |
|        2 |  8756 |  |
|        - |  8757 | `	ph7_value *aStack;` |
|        - |  8758 | `	VmInstr aInstr[2];` |
|        - |  8759 | `	int i;` |
|      962 |  8760 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8761 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  8762 | `		if( pResult ){` |
|        - |  8763 | `			/* Assume a null return value */` |
|      ! 0 |  8764 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8765 | `		}` |
|      479 |  8766 | `		return SXERR_INVALID;` |
|        - |  8767 | `	}` |
|      484 |  8768 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8769 | `		/* Class method */` |
|       11 |  8770 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8771 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8772 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8773 | `		ph7_class *pClass = 0;` |
|        - |  8774 | `		ph7_value *pValue;` |
|        - |  8775 | `		sxi32 rc;` |
|       11 |  8776 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8777 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8778 | `			if( pResult ){` |
|        - |  8779 | `				/* Assume a null return value */` |
|      ! 0 |  8780 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8781 | `			}` |
|      ! 0 |  8782 | `			return SXRET_OK;` |
|        - |  8783 | `		}` |
|        - |  8784 | `		/* Extract the class name or an instance of it */` |
|       11 |  8785 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8786 | `		if( pValue ){` |
|       11 |  8787 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8788 | `		}` |
|       11 |  8789 | `		if( pClass == 0 ){` |
|        - |  8790 | `			/* No such class,return NULL */` |
|      ! 0 |  8791 | `			if( pResult ){` |
|      ! 0 |  8792 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8793 | `			}` |
|      ! 0 |  8794 | `			return SXRET_OK;` |
|        - |  8795 | `		}` |
|       11 |  8796 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8797 | `			/* Point to the class instance */` |
|        5 |  8798 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8799 | `		}` |
|        - |  8800 | `		/* Try to extract the method */` |
|       11 |  8801 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8802 | `		if( pValue ){` |
|       11 |  8803 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8804 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8805 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8806 | `			}` |
|        5 |  8807 | `		}` |
|       11 |  8808 | `		if( pMethod == 0 ){` |
|        - |  8809 | `			/* No such method,return NULL */` |
|      ! 0 |  8810 | `			if( pResult ){` |
|      ! 0 |  8811 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8812 | `			}` |
|      ! 0 |  8813 | `			return SXRET_OK;` |
|        - |  8814 | `		}` |
|        - |  8815 | `		/* Call the class method */` |
|       11 |  8816 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8817 | `		return rc;` |
|        - |  8818 | `	}` |
|        - |  8819 | `	/* Create a new operand stack */` |
|      474 |  8820 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8821 | `	if( aStack == 0 ){` |
|      ! 0 |  8822 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8823 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8824 | `		if( pResult ){` |
|        - |  8825 | `			/* Assume a null return value */` |
|      ! 0 |  8826 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8827 | `		}` |
|      ! 0 |  8828 | `		return SXERR_MEM;` |
|        - |  8829 | `	}` |
|        - |  8830 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8831 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8832 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8833 | `		/*` |
|        - |  8834 | `		 * Symisc eXtension:` |
|        - |  8835 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8836 | `		 */` |
|     1050 |  8837 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8838 | `	}` |
|        - |  8839 | `	/* Push the function name */` |
|      474 |  8840 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8841 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8842 | `	/* Emit the CALL istruction */` |
|      474 |  8843 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8844 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8845 | `	aInstr[0].iP2 = 0;` |
|      474 |  8846 | `	aInstr[0].p3  = 0;` |
|        - |  8847 | `	/* Emit the DONE instruction */` |
|      474 |  8848 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8849 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8850 | `	aInstr[1].iP2 = 0;` |
|      474 |  8851 | `	aInstr[1].p3  = 0;` |
|        - |  8852 | `	/* Execute the function body (if available) */` |
|      474 |  8853 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8854 | `	/* Clean up the mess left behind */` |
|      474 |  8855 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8856 | `	return PH7_OK;` |
|      482 |  8857 |  |
|        - |  8858 | `/*` |
|        - |  8859 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8860 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8861 | ` * parameter.` |
|        - |  8862 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8863 | ` * return value indicates failure.` |
|        - |  8864 | ` */` |
|      236 |  8865 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8866 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8867 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8868 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8869 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8870 | `	)` |
|        1 |  8871 |  |
|        - |  8872 | `	ph7_value *pArg;` |
|        - |  8873 | `	SySet aArg;` |
|        - |  8874 | `	va_list ap;` |
|        - |  8875 | `	sxi32 rc;` |
|      237 |  8876 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8877 | `	/* Copy arguments one after one */` |
|      237 |  8878 | `	va_start(ap,pResult);` |
|      393 |  8879 | `	for(;;){` |
|      787 |  8880 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8881 | `		if( pArg == 0 ){` |
|      237 |  8882 | `			break;` |
|        - |  8883 | `		}` |
|      551 |  8884 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8885 | `	}` |
|        - |  8886 | `	/* Call the core routine */` |
|      237 |  8887 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8888 | `	/* Cleanup */` |
|      237 |  8889 | `	SySetRelease(&aArg);` |
|      237 |  8890 | `	return rc;` |
|        1 |  8891 |  |
|        - |  8892 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8893 | `/*` |
|        - |  8894 | ` * bool defined(string $name)` |
|        - |  8895 | ` *  Checks whether a given named constant exists.` |
|        - |  8896 | ` * Parameter:` |
|        - |  8897 | ` *  Name of the desired constant.` |
|        - |  8898 | ` * Return` |
|        - |  8899 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8900 | ` */` |
|       14 |  8901 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8902 |  |
|        - |  8903 | `	const char *zName;` |
|       16 |  8904 | `	int nLen = 0;` |
|       16 |  8905 | `	int res = 0;` |
|       16 |  8906 | `	if( nArg < 1 ){` |
|        - |  8907 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8908 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8910 | `		return SXRET_OK;` |
|        - |  8911 | `	}` |
|        - |  8912 | `	/* Extract constant name */` |
|       16 |  8913 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8914 | `	/* Perform the lookup */` |
|       16 |  8915 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8916 | `		/* Already defined */` |
|       10 |  8917 | `		res = 1;` |
|        4 |  8918 | `	}` |
|       16 |  8919 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8920 | `	return SXRET_OK;` |
|        9 |  8921 |  |
|        - |  8922 | `/*` |
|        - |  8923 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8924 | ` * below.` |
|        - |  8925 | ` */` |
|       10 |  8926 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8927 |  |
|       12 |  8928 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8929 | `	/* Expand constant value */` |
|       12 |  8930 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  8931 |  |
|        - |  8932 | `/*` |
|        - |  8933 | ` * bool define(string $constant_name,expression value)` |
|        - |  8934 | ` *  Defines a named constant at runtime.` |
|        - |  8935 | ` * Parameter:` |
|        - |  8936 | ` *  $constant_name` |
|        - |  8937 | ` *   The name of the constant` |
|        - |  8938 | ` *  $value` |
|        - |  8939 | ` *   Constant value` |
|        - |  8940 | ` * Return:` |
|        - |  8941 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8942 | ` */` |
|       12 |  8943 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8944 |  |
|        - |  8945 | `	const char *zName;  /* Constant name */` |
|        - |  8946 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  8947 | `	int nLen = 0;       /* Name length */` |
|        - |  8948 | `	sxi32 rc;` |
|       14 |  8949 | `	if( nArg < 2 ){` |
|        - |  8950 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8951 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8952 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8953 | `		return SXRET_OK;` |
|        - |  8954 | `	}` |
|       14 |  8955 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8956 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8957 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8958 | `		return SXRET_OK;` |
|        - |  8959 | `	}` |
|        - |  8960 | `	/* Extract constant name */` |
|       14 |  8961 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  8962 | `	if( nLen < 1 ){` |
|      ! 0 |  8963 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8964 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8965 | `		return SXRET_OK;` |
|        - |  8966 | `	}` |
|        - |  8967 | `	/* Duplicate constant value */` |
|       14 |  8968 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  8969 | `	if( pValue == 0 ){` |
|      ! 0 |  8970 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8971 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8972 | `		return SXRET_OK;` |
|        - |  8973 | `	}` |
|        - |  8974 | `	/* Initialize the memory object */` |
|       14 |  8975 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8976 | `	/* Register the constant */` |
|       14 |  8977 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  8978 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8979 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8980 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8981 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8982 | `		return SXRET_OK;` |
|        - |  8983 | `	}` |
|        - |  8984 | `	/* Duplicate constant value */` |
|       14 |  8985 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  8986 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8987 | `		/* Lower case the constant name */` |
|      ! 0 |  8988 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8989 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8990 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8991 | `				/* UTF-8 stream */` |
|      ! 0 |  8992 | `				zCur++;` |
|      ! 0 |  8993 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8994 | `					zCur++;` |
|      ! 0 |  8995 | `				}` |
|      ! 0 |  8996 | `				continue;` |
|        - |  8997 | `			}` |
|      ! 0 |  8998 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8999 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9000 | `				zCur[0] = (char)c;` |
|      ! 0 |  9001 | `			}` |
|      ! 0 |  9002 | `			zCur++;` |
|      ! 0 |  9003 | `		}` |
|        - |  9004 | `		/* Finally,register the constant */` |
|      ! 0 |  9005 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9006 | `	}` |
|        - |  9007 | `	/* All done,return TRUE */` |
|       14 |  9008 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9009 | `	return SXRET_OK;` |
|        8 |  9010 |  |
|        - |  9011 | `/*` |
|        - |  9012 | ` * value constant(string $name)` |
|        - |  9013 | ` *  Returns the value of a constant` |
|        - |  9014 | ` * Parameter` |
|        - |  9015 | ` *  $name` |
|        - |  9016 | ` *    Name of the constant.` |
|        - |  9017 | ` * Return` |
|        - |  9018 | ` *  Constant value or NULL if not defined.` |
|        - |  9019 | ` */` |
|        8 |  9020 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9021 |  |
|        - |  9022 | `	SyHashEntry *pEntry;` |
|        - |  9023 | `	ph7_constant *pCons;` |
|        - |  9024 | `	const char *zName; /* Constant name */` |
|        - |  9025 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9026 | `	int nLen;` |
|       10 |  9027 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9028 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9029 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9030 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9031 | `		return SXRET_OK;` |
|        - |  9032 | `	}` |
|        - |  9033 | `	/* Extract the constant name */` |
|       10 |  9034 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9035 | `	/* Perform the query */` |
|       10 |  9036 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9037 | `	if( pEntry == 0 ){` |
|        3 |  9038 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9039 | `		ph7_result_null(pCtx);` |
|        3 |  9040 | `		return SXRET_OK;` |
|        - |  9041 | `	}` |
|        8 |  9042 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9043 | `	/* Point to the structure that describe the constant */` |
|        8 |  9044 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9045 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9046 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9047 | `	/* Return that value */` |
|        8 |  9048 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9049 | `	/* Cleanup */` |
|        8 |  9050 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9051 | `	return SXRET_OK;` |
|        6 |  9052 |  |
|        - |  9053 | `/*` |
|        - |  9054 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9055 | ` * defined below.` |
|        - |  9056 | ` */` |
|      452 |  9057 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9058 |  |
|      453 |  9059 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9060 | `	ph7_value sName;` |
|        - |  9061 | `	sxi32 rc;` |
|        - |  9062 | `	/* Prepare the constant name for insertion */` |
|      453 |  9063 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9064 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9065 | `	/* Perform the insertion */` |
|      453 |  9066 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9067 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9068 | `	return rc;` |
|        1 |  9069 |  |
|        - |  9070 | `/*` |
|        - |  9071 | ` * array get_defined_constants(void)` |
|        - |  9072 | ` *  Returns an associative array with the names of all defined` |
|        - |  9073 | ` *  constants.` |
|        - |  9074 | ` * Parameters` |
|        - |  9075 | ` *  NONE.` |
|        - |  9076 | ` * Returns` |
|        - |  9077 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9078 | ` */` |
|        2 |  9079 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9080 |  |
|        - |  9081 | `	ph7_value *pArray;` |
|        - |  9082 | `	/* Create the array first*/` |
|        3 |  9083 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9084 | `	if( pArray == 0 ){` |
|      ! 0 |  9085 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9086 | `		SXUNUSED(apArg);` |
|        - |  9087 | `		/* Return NULL */` |
|      ! 0 |  9088 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9089 | `		return SXRET_OK;` |
|        - |  9090 | `	}` |
|        - |  9091 | `	/* Fill the array with the defined constants */` |
|        3 |  9092 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9093 | `	/* Return the created array */` |
|        3 |  9094 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9095 | `	return SXRET_OK;` |
|        2 |  9096 |  |
|        - |  9097 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9098 | `/*` |
|        - |  9099 | ` * Section:` |
|        - |  9100 | ` *  Random numbers/string generators.` |
|        - |  9101 | ` * Status:` |
|        - |  9102 | ` *    Stable.` |
|        - |  9103 | ` */` |
|        - |  9104 | `/*` |
|        - |  9105 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9106 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9107 | ` * used by te SQLite3 library.` |
|        - |  9108 | ` */` |
|     2405 |  9109 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9110 |  |
|        - |  9111 | `	sxu32 iNum;` |
|     2407 |  9112 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2407 |  9113 | `	return iNum;` |
|        2 |  9114 |  |
|        - |  9115 | `/*` |
|        - |  9116 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9117 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9118 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9119 | ` * by te SQLite3 library.` |
|        - |  9120 | ` */` |
|   125014 |  9121 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9122 |  |
|        - |  9123 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9124 | `	int i;` |
|        - |  9125 | `	/* Generate a binary string first */` |
|   125016 |  9126 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9127 | `	/* Turn the binary string into english based alphabet */` |
|  1375324 |  9128 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1250310 |  9129 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   625156 |  9130 | `	 }` |
|   125016 |  9131 |  |
|        - |  9132 | `/*` |
|        - |  9133 | ` * int rand()` |
|        - |  9134 | ` * int mt_rand()` |
|        - |  9135 | ` * int rand(int $min,int $max)` |
|        - |  9136 | ` * int mt_rand(int $min,int $max)` |
|        - |  9137 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9138 | ` * Parameter` |
|        - |  9139 | ` *  $min` |
|        - |  9140 | ` *    The lowest value to return (default: 0)` |
|        - |  9141 | ` *  $max` |
|        - |  9142 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9143 | ` * Return` |
|        - |  9144 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9145 | ` * Note:` |
|        - |  9146 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9147 | ` *  by te SQLite3 library.` |
|        - |  9148 | ` */` |
|       20 |  9149 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9150 |  |
|        - |  9151 | `	sxu32 iNum;` |
|        - |  9152 | `	/* Generate the random number */` |
|       21 |  9153 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9154 | `	if( nArg > 1 ){` |
|        - |  9155 | `		sxu32 iMin,iMax;` |
|        3 |  9156 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9157 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9158 | `		if( iMin < iMax ){` |
|        3 |  9159 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9160 | `			if( iDiv > 0 ){` |
|        3 |  9161 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9162 | `			}` |
|        1 |  9163 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9164 | `			iNum %= iMax;` |
|      ! 0 |  9165 | `		}` |
|        1 |  9166 | `	}` |
|        - |  9167 | `	/* Return the number */` |
|       21 |  9168 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9169 | `	return SXRET_OK;` |
|        1 |  9170 |  |
|        - |  9171 | `/*` |
|        - |  9172 | ` * int getrandmax(void)` |
|        - |  9173 | ` * int mt_getrandmax(void)` |
|        - |  9174 | ` * int rc4_getrandmax(void)` |
|        - |  9175 | ` *   Show largest possible random value` |
|        - |  9176 | ` * Return` |
|        - |  9177 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9178 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9179 | ` * Note:` |
|        - |  9180 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9181 | ` *  by te SQLite3 library.` |
|        - |  9182 | ` */` |
|        4 |  9183 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9184 |  |
|        2 |  9185 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9186 | `	SXUNUSED(apArg);` |
|        5 |  9187 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9188 | `	return SXRET_OK;` |
|        1 |  9189 |  |
|        - |  9190 | `/*` |
|        - |  9191 | ` * string rand_str()` |
|        - |  9192 | ` * string rand_str(int $len)` |
|        - |  9193 | ` *  Generate a random string (English alphabet).` |
|        - |  9194 | ` * Parameter` |
|        - |  9195 | ` *  $len` |
|        - |  9196 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9197 | ` * Return` |
|        - |  9198 | ` *   A pseudo random string.` |
|        - |  9199 | ` * Note:` |
|        - |  9200 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9201 | ` *  by te SQLite3 library.` |
|        - |  9202 | ` *  This function is a symisc extension.` |
|        - |  9203 | ` */` |
|      120 |  9204 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9205 |  |
|        - |  9206 | `	char zString[1024];` |
|      122 |  9207 | `	int iLen = 0x10;` |
|      122 |  9208 | `	if( nArg > 0 ){` |
|        - |  9209 | `		/* Get the desired length */` |
|      122 |  9210 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9211 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9212 | `			/* Default length */` |
|        3 |  9213 | `			iLen = 0x10;` |
|        1 |  9214 | `		}` |
|       60 |  9215 | `	}` |
|        - |  9216 | `	/* Generate the random string */` |
|      122 |  9217 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9218 | `	/* Return the generated string */` |
|      122 |  9219 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9220 | `	return SXRET_OK;` |
|        2 |  9221 |  |
|        - |  9222 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9223 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9224 | `/* Unique ID private data */` |
|        - |  9225 | `struct unique_id_data` |
|        - |  9226 |  |
|        - |  9227 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9228 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9229 | `};` |
|        - |  9230 | `/*` |
|        - |  9231 | ` * Binary to hex consumer callback.` |
|        - |  9232 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9233 | ` * defined below.` |
|        - |  9234 | ` */` |
|      192 |  9235 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9236 |  |
|      193 |  9237 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9238 | `	sxu32 nBuflen;` |
|        - |  9239 | `	/* Extract result buffer length */` |
|      193 |  9240 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9241 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9242 | `			/*` |
|        - |  9243 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9244 | `			 * string will be 13 characters long` |
|        - |  9245 | `			 */` |
|       25 |  9246 | `		return SXERR_ABORT;` |
|        - |  9247 | `	}` |
|      169 |  9248 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9249 | `		return SXERR_ABORT;` |
|        - |  9250 | `	}` |
|        - |  9251 | `	/* Safely Consume the hex stream */` |
|      169 |  9252 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9253 | `	return SXRET_OK;` |
|       97 |  9254 |  |
|        - |  9255 | `/*` |
|        - |  9256 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9257 | ` *  Generate a unique ID` |
|        - |  9258 | ` * Parameter` |
|        - |  9259 | ` * $prefix` |
|        - |  9260 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9261 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9262 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9263 | ` * $more_entropy` |
|        - |  9264 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9265 | ` *  that the result will be unique.` |
|        - |  9266 | ` * Return` |
|        - |  9267 | ` *  Returns the unique identifier, as a string.` |
|        - |  9268 | ` */` |
|       24 |  9269 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9270 |  |
|        - |  9271 | `	struct unique_id_data sUniq;` |
|        - |  9272 | `	unsigned char zDigest[20];` |
|       25 |  9273 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9274 | `	const char *zPrefix;` |
|        - |  9275 | `	SHA1Context sCtx;` |
|        - |  9276 | `	char zRandom[7];` |
|        - |  9277 | `	int nPrefix;` |
|        - |  9278 | `	int entropy;` |
|        - |  9279 | `	/* Generate a random string first */` |
|       25 |  9280 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9281 | `	/* Initialize fields */` |
|       25 |  9282 | `	zPrefix = 0;` |
|       25 |  9283 | `	nPrefix = 0;` |
|       25 |  9284 | `	entropy = 0;` |
|       25 |  9285 | `	if( nArg > 0 ){` |
|        - |  9286 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9287 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9288 | `		if( nArg > 1 ){` |
|      ! 0 |  9289 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9290 | `		}` |
|      ! 0 |  9291 | `	}` |
|       25 |  9292 | `	SHA1Init(&sCtx);` |
|        - |  9293 | `	/* Generate the random ID */` |
|       25 |  9294 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9295 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9296 | `	}` |
|        - |  9297 | `	/* Append the random ID */` |
|       25 |  9298 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9299 | `	/* Append the random string */` |
|       25 |  9300 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9301 | `	/* Increment the number */` |
|       25 |  9302 | `	pVm->unique_id++;` |
|       25 |  9303 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9304 | `	/* Hexify the digest */` |
|       25 |  9305 | `	sUniq.pCtx = pCtx;` |
|       25 |  9306 | `	sUniq.entropy = entropy;` |
|       25 |  9307 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9308 | `	/* All done */` |
|       25 |  9309 | `	return PH7_OK;` |
|        1 |  9310 |  |
|        - |  9311 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9312 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9313 | `/*` |
|        - |  9314 | ` * Section:` |
|        - |  9315 | ` *  Language construct implementation as foreign functions.` |
|        - |  9316 | ` * Status:` |
|        - |  9317 | ` *    Stable.` |
|        - |  9318 | ` */` |
|        - |  9319 | `/*` |
|        - |  9320 | ` * void echo($string...)` |
|        - |  9321 | ` *  Output one or more messages.` |
|        - |  9322 | ` * Parameters` |
|        - |  9323 | ` *  $string` |
|        - |  9324 | ` *   Message to output.` |
|        - |  9325 | ` * Return` |
|        - |  9326 | ` *  NULL.` |
|        - |  9327 | ` */` |
|      ! 0 |  9328 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9329 |  |
|        - |  9330 | `	const char *zData;` |
|      ! 0 |  9331 | `	int nDataLen = 0;` |
|        - |  9332 | `	ph7_vm *pVm;` |
|        - |  9333 | `	int i,rc;` |
|        - |  9334 | `	/* Point to the target VM */` |
|      ! 0 |  9335 | `	pVm = pCtx->pVm;` |
|        - |  9336 | `	/* Output */` |
|      ! 0 |  9337 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9338 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9339 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9340 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9341 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9342 | `			if( rc == SXERR_ABORT ){` |
|        - |  9343 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9344 | `				return PH7_ABORT;` |
|        - |  9345 | `			}` |
|      ! 0 |  9346 | `		}` |
|      ! 0 |  9347 | `	}` |
|      ! 0 |  9348 | `	return SXRET_OK;` |
|      ! 0 |  9349 |  |
|        - |  9350 | `/*` |
|        - |  9351 | ` * int print($string...)` |
|        - |  9352 | ` *  Output one or more messages.` |
|        - |  9353 | ` * Parameters` |
|        - |  9354 | ` *  $string` |
|        - |  9355 | ` *   Message to output.` |
|        - |  9356 | ` * Return` |
|        - |  9357 | ` *  1 always.` |
|        - |  9358 | ` */` |
|        2 |  9359 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9360 |  |
|        - |  9361 | `	const char *zData;` |
|        3 |  9362 | `	int nDataLen = 0;` |
|        - |  9363 | `	ph7_vm *pVm;` |
|        - |  9364 | `	int i,rc;` |
|        - |  9365 | `	/* Point to the target VM */` |
|        3 |  9366 | `	pVm = pCtx->pVm;` |
|        - |  9367 | `	/* Output */` |
|        5 |  9368 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9369 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9370 | `		if( nDataLen > 0 ){` |
|        3 |  9371 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9372 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9373 | `			if( rc == SXERR_ABORT ){` |
|        - |  9374 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9375 | `				return PH7_ABORT;` |
|        - |  9376 | `			}` |
|        1 |  9377 | `		}` |
|        2 |  9378 | `	}` |
|        - |  9379 | `	/* Return 1 */` |
|        3 |  9380 | `	ph7_result_int(pCtx,1);` |
|        3 |  9381 | `	return SXRET_OK;` |
|        2 |  9382 |  |
|        - |  9383 | `/*` |
|        - |  9384 | ` * void exit(string $msg)` |
|        - |  9385 | ` * void exit(int $status)` |
|        - |  9386 | ` * void die(string $ms)` |
|        - |  9387 | ` * void die(int $status)` |
|        - |  9388 | ` *   Output a message and terminate program execution.` |
|        - |  9389 | ` * Parameter` |
|        - |  9390 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9391 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9392 | ` *  and not printed` |
|        - |  9393 | ` * Return` |
|        - |  9394 | ` *  NULL` |
|        - |  9395 | ` */` |
|      ! 0 |  9396 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9397 |  |
|      ! 0 |  9398 | `	if( nArg > 0 ){` |
|      ! 0 |  9399 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9400 | `			const char *zData;` |
|      ! 0 |  9401 | `			int iLen = 0;` |
|        - |  9402 | `			/* Print exit message */` |
|      ! 0 |  9403 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9404 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9405 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9406 | `			sxi32 iExitStatus;` |
|        - |  9407 | `			/* Record exit status code */` |
|      ! 0 |  9408 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9409 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9410 | `		}` |
|      ! 0 |  9411 | `	}` |
|        - |  9412 | `	/* Check if we are in an included file */` |
|      ! 0 |  9413 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9414 | `		/* Exit the entire process */` |
|      ! 0 |  9415 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9416 | `	}` |
|        - |  9417 | `	/* Abort processing immediately */` |
|      ! 0 |  9418 | `	return PH7_ABORT;` |
|      ! 0 |  9419 |  |
|        - |  9420 | `/*` |
|        - |  9421 | ` * bool isset($var,...)` |
|        - |  9422 | ` *  Finds out whether a variable is set.` |
|        - |  9423 | ` * Parameters` |
|        - |  9424 | ` *  One or more variable to check.` |
|        - |  9425 | ` * Return` |
|        - |  9426 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9427 | ` */` |
|    75870 |  9428 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9429 |  |
|        - |  9430 | `	ph7_value *pObj;` |
|    75872 |  9431 | `	int res = 0;` |
|        - |  9432 | `	int i;` |
|    75872 |  9433 | `	if( nArg < 1 ){` |
|        - |  9434 | `		/* Missing arguments,return false */` |
|      ! 0 |  9435 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9436 | `		return SXRET_OK;` |
|        - |  9437 | `	}` |
|        - |  9438 | `	/* Iterate over available arguments */` |
|    99942 |  9439 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    75872 |  9440 | `		pObj = apArg[i];` |
|    75872 |  9441 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    51276 |  9442 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9443 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9444 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9445 | `			}` |
|    25637 |  9446 | `		}` |
|    75872 |  9447 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    75872 |  9448 | `		if( !res ){` |
|        - |  9449 | `			/* Variable not set,return FALSE */` |
|    51802 |  9450 | `			ph7_result_bool(pCtx,0);` |
|    51802 |  9451 | `			return SXRET_OK;` |
|        - |  9452 | `		}` |
|    12037 |  9453 | `	}` |
|        - |  9454 | `	/* All given variable are set,return TRUE */` |
|    24072 |  9455 | `	ph7_result_bool(pCtx,1);` |
|    24072 |  9456 | `	return SXRET_OK;` |
|    37937 |  9457 |  |
|        - |  9458 | `/*` |
|        - |  9459 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9460 | ` * frame,the reference table and discard it's contents.` |
|        - |  9461 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9462 | ` */` |
|  3033636 |  9463 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9464 |  |
|        - |  9465 | `	ph7_value *pObj;` |
|        - |  9466 | `	VmRefObj *pRef;` |
|  3033638 |  9467 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3033638 |  9468 | `	if( pObj ){` |
|        - |  9469 | `		/* Release the object */` |
|  3033638 |  9470 | `		PH7_MemObjRelease(pObj);` |
|  1516818 |  9471 | `	}` |
|        - |  9472 | `	/* Remove old reference links */` |
|  3033638 |  9473 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3033638 |  9474 | `	if( pRef ){` |
|  3033632 |  9475 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9476 | `		/* Unlink from the reference table */` |
|  3033632 |  9477 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3033632 |  9478 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9479 | `			VmSlot sFree;` |
|        - |  9480 | `			/* Restore to the free list */` |
|  3033626 |  9481 | `			sFree.nIdx = nObjIdx;` |
|  3033626 |  9482 | `			sFree.pUserData = 0;` |
|  3033626 |  9483 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1516812 |  9484 | `		}` |
|  1516815 |  9485 | `	}` |
|  3033638 |  9486 | `	return SXRET_OK;` |
|        2 |  9487 |  |
|        - |  9488 | `/*` |
|        - |  9489 | ` * void unset($var,...)` |
|        - |  9490 | ` *   Unset one or more given variable.` |
|        - |  9491 | ` * Parameters` |
|        - |  9492 | ` *  One or more variable to unset.` |
|        - |  9493 | ` * Return` |
|        - |  9494 | ` *  Nothing.` |
|        - |  9495 | ` */` |
|     6764 |  9496 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9497 |  |
|        - |  9498 | `	ph7_value *pObj;` |
|        - |  9499 | `	ph7_vm *pVm;` |
|        - |  9500 | `	int i;` |
|        - |  9501 | `	/* Point to the target VM */` |
|     6766 |  9502 | `	pVm = pCtx->pVm;` |
|        - |  9503 | `	/* Iterate and unset */` |
|    13530 |  9504 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6766 |  9505 | `		pObj = apArg[i];` |
|     6766 |  9506 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9507 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9508 | `				/* Throw an error */` |
|      ! 0 |  9509 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9510 | `			}` |
|      ! 0 |  9511 | `		}else{` |
|     6766 |  9512 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9513 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6766 |  9514 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6760 |  9515 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3379 |  9516 | `			}` |
|        - |  9517 | `		}` |
|     3384 |  9518 | `	}` |
|     6766 |  9519 | `	return SXRET_OK;` |
|        2 |  9520 |  |
|        - |  9521 | `/*` |
|        - |  9522 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9523 | ` */` |
|      110 |  9524 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9525 |  |
|      111 |  9526 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9527 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9528 | `	ph7_value *pObj;` |
|        - |  9529 | `	sxu32 nIdx;` |
|        - |  9530 | `	/* Extract the memory object */` |
|      111 |  9531 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9532 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9533 | `	if( pObj ){` |
|      111 |  9534 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9535 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9536 | `				SyString sName;` |
|        - |  9537 | `				ph7_value sKey;` |
|        - |  9538 | `				/* Perform the insertion */` |
|      109 |  9539 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9540 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9541 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9542 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9543 | `			}` |
|       54 |  9544 | `		}` |
|       55 |  9545 | `	}` |
|      111 |  9546 | `	return SXRET_OK;` |
|        1 |  9547 |  |
|        - |  9548 | `/*` |
|        - |  9549 | ` * array get_defined_vars(void)` |
|        - |  9550 | ` *  Returns an array of all defined variables.` |
|        - |  9551 | ` * Parameter` |
|        - |  9552 | ` *  None` |
|        - |  9553 | ` * Return` |
|        - |  9554 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9555 | ` */` |
|        2 |  9556 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9557 |  |
|        3 |  9558 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9559 | `	ph7_value *pArray;` |
|        - |  9560 | `	/* Create a new array */` |
|        3 |  9561 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9562 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9563 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9564 | `		SXUNUSED(apArg);` |
|        - |  9565 | `		/* Return NULL */` |
|      ! 0 |  9566 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9567 | `		return SXRET_OK;` |
|        - |  9568 | `	}` |
|        - |  9569 | `	/* Superglobals first */` |
|        3 |  9570 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9571 | `	/* Then variable defined in the current frame */` |
|        3 |  9572 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9573 | `	/* Finally,return the created array */` |
|        3 |  9574 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9575 | `	return SXRET_OK;` |
|        2 |  9576 |  |
|        - |  9577 | `/*` |
|        - |  9578 | ` * bool gettype($var)` |
|        - |  9579 | ` *  Get the type of a variable` |
|        - |  9580 | ` * Parameters` |
|        - |  9581 | ` *   $var` |
|        - |  9582 | ` *    The variable being type checked.` |
|        - |  9583 | ` * Return` |
|        - |  9584 | ` *   String representation of the given variable type.` |
|        - |  9585 | ` */` |
|       32 |  9586 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9587 |  |
|       34 |  9588 | `	const char *zType = "Empty";` |
|       34 |  9589 | `	if( nArg > 0 ){` |
|       34 |  9590 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9591 | `	}` |
|        - |  9592 | `	/* Return the variable type */` |
|       34 |  9593 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9594 | `	return SXRET_OK;` |
|        2 |  9595 |  |
|        - |  9596 | `/*` |
|        - |  9597 | ` * string get_resource_type(resource $handle)` |
|        - |  9598 | ` *  This function gets the type of the given resource.` |
|        - |  9599 | ` * Parameters` |
|        - |  9600 | ` *  $handle` |
|        - |  9601 | ` *  The evaluated resource handle.` |
|        - |  9602 | ` * Return` |
|        - |  9603 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9604 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9605 | ` *  the return value will be the string Unknown.` |
|        - |  9606 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9607 | ` *  is not a resource.` |
|        - |  9608 | ` */` |
|        2 |  9609 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9610 |  |
|        3 |  9611 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9612 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9613 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9614 | `		return PH7_OK;` |
|        - |  9615 | `	}` |
|        3 |  9616 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9617 | `	return SXRET_OK;` |
|        2 |  9618 |  |
|        - |  9619 | `/*` |
|        - |  9620 | ` * void var_dump(expression,....)` |
|        - |  9621 | ` *   var_dump � Dumps information about a variable` |
|        - |  9622 | ` * Parameters` |
|        - |  9623 | ` *   One or more expression to dump.` |
|        - |  9624 | ` * Returns` |
|        - |  9625 | ` *  Nothing.` |
|        - |  9626 | ` */` |
|      218 |  9627 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9628 |  |
|        - |  9629 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9630 | `	int i;` |
|      220 |  9631 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9632 | `	/* Dump one or more expressions */` |
|      444 |  9633 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9634 | `		ph7_value *pObj = apArg[i];` |
|        - |  9635 | `		/* Reset the working buffer */` |
|      226 |  9636 | `		SyBlobReset(&sDump);` |
|        - |  9637 | `		/* Dump the given expression */` |
|      226 |  9638 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9639 | `		/* Output */` |
|      226 |  9640 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9641 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9642 | `		}` |
|      114 |  9643 | `	}` |
|        - |  9644 | `	/* Release the working buffer */` |
|      220 |  9645 | `	SyBlobRelease(&sDump);` |
|      220 |  9646 | `	return SXRET_OK;` |
|        2 |  9647 |  |
|        - |  9648 | `/*` |
|        - |  9649 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9650 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9651 | ` * Parameters` |
|        - |  9652 | ` *   expression: Expression to dump` |
|        - |  9653 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9654 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9655 | ` *            print_r() will return the information rather than print it.` |
|        - |  9656 | ` * Return` |
|        - |  9657 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9658 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9659 | ` */` |
|       16 |  9660 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9661 |  |
|       17 |  9662 | `	int ret_string = 0;` |
|        - |  9663 | `	SyBlob sDump;` |
|       17 |  9664 | `	if( nArg < 1 ){` |
|        - |  9665 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9666 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9667 | `		return SXRET_OK;` |
|        - |  9668 | `	}` |
|       17 |  9669 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9670 | `	if ( nArg > 1 ){` |
|        - |  9671 | `		/* Where to redirect output */` |
|       11 |  9672 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9673 | `	}` |
|        - |  9674 | `	/* Generate dump */` |
|       17 |  9675 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9676 | `	if( !ret_string ){` |
|        - |  9677 | `		/* Output dump */` |
|        7 |  9678 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9679 | `		/* Return true */` |
|        7 |  9680 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9681 | `	}else{` |
|        - |  9682 | `		/* Generated dump as return value */` |
|       11 |  9683 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9684 | `	}` |
|        - |  9685 | `	/* Release the working buffer */` |
|       17 |  9686 | `	SyBlobRelease(&sDump);` |
|       17 |  9687 | `	return SXRET_OK;` |
|        9 |  9688 |  |
|        - |  9689 | `/*` |
|        - |  9690 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9691 | ` * Same job as print_r. (see coment above)` |
|        - |  9692 | ` */` |
|        2 |  9693 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9694 |  |
|        3 |  9695 | `	int ret_string = 0;` |
|        - |  9696 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9697 | `	if( nArg < 1 ){` |
|        - |  9698 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9699 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9700 | `		return SXRET_OK;` |
|        - |  9701 | `	}` |
|        3 |  9702 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9703 | `	if ( nArg > 1 ){` |
|        - |  9704 | `		/* Where to redirect output */` |
|        3 |  9705 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9706 | `	}` |
|        - |  9707 | `	/* Generate dump */` |
|        3 |  9708 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9709 | `	if( !ret_string ){` |
|        - |  9710 | `		/* Output dump */` |
|      ! 0 |  9711 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9712 | `		/* Return NULL */` |
|      ! 0 |  9713 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9714 | `	}else{` |
|        - |  9715 | `		/* Generated dump as return value */` |
|        3 |  9716 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9717 | `	}` |
|        - |  9718 | `	/* Release the working buffer */` |
|        3 |  9719 | `	SyBlobRelease(&sDump);` |
|        3 |  9720 | `	return SXRET_OK;` |
|        2 |  9721 |  |
|        - |  9722 | `/*` |
|        - |  9723 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9724 | ` *  Set/get the various assert flags.` |
|        - |  9725 | ` * Parameter` |
|        - |  9726 | ` * $what` |
|        - |  9727 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9728 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9729 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9730 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9731 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9732 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9733 | ` * $value` |
|        - |  9734 | ` *   An optional new value for the option.` |
|        - |  9735 | ` * Return` |
|        - |  9736 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9737 | ` */` |
|       28 |  9738 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9739 |  |
|       30 |  9740 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9741 | `	int iOption;` |
|        - |  9742 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9743 | `	if( nArg < 1 ){` |
|        3 |  9744 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9745 | `			"ArgumentCountError",` |
|        - |  9746 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9747 | `			);` |
|        - |  9748 | `	}` |
|        - |  9749 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9750 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9751 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9752 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9753 | `			"TypeError",` |
|        - |  9754 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9755 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9756 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9757 | `			);` |
|        - |  9758 | `	}` |
|       28 |  9759 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9760 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9761 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9762 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9763 | `	switch( iOption ){` |
|        5 |  9764 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9765 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9766 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9767 | `		if( nArg > 1 ){` |
|        5 |  9768 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9769 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9770 | `			}else{` |
|        3 |  9771 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9772 | `			}` |
|        2 |  9773 | `		}` |
|       12 |  9774 | `		break;` |
|        1 |  9775 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9776 | `		/* Return old callback or null */` |
|        3 |  9777 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9778 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9779 | `		}else{` |
|        3 |  9780 | `			ph7_result_null(pCtx);` |
|        - |  9781 | `		}` |
|        3 |  9782 | `		if( nArg > 1 ){` |
|      ! 0 |  9783 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9784 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9785 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9786 | `			}else{` |
|      ! 0 |  9787 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9788 | `			}` |
|      ! 0 |  9789 | `		}` |
|        3 |  9790 | `		break;` |
|        5 |  9791 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9792 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9793 | `		if( nArg > 1 ){` |
|        5 |  9794 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9795 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9796 | `			}else{` |
|        3 |  9797 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9798 | `			}` |
|        2 |  9799 | `		}` |
|       11 |  9800 | `		break;` |
|      ! 0 |  9801 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9802 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9803 | `		break;` |
|        1 |  9804 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9805 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9806 | `		break;` |
|      ! 0 |  9807 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9808 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9809 | `		break;` |
|        1 |  9810 | `	default:` |
|        - |  9811 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9812 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9813 | `			"ValueError",` |
|        - |  9814 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9815 | `			);` |
|        - |  9816 | `	}` |
|       26 |  9817 | `	return PH7_OK;` |
|       16 |  9818 |  |
|        - |  9819 | `/*` |
|        - |  9820 | ` * bool assert(mixed $assertion)` |
|        - |  9821 | ` *  Checks if assertion is FALSE.` |
|        - |  9822 | ` * Parameter` |
|        - |  9823 | ` *  $assertion` |
|        - |  9824 | ` *    The assertion to test.` |
|        - |  9825 | ` * Return` |
|        - |  9826 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9827 | ` */` |
|       24 |  9828 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9829 |  |
|       26 |  9830 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9831 | `	int iFlags,iResult;` |
|        - |  9832 | `	const char *zDesc;` |
|        - |  9833 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9834 | `	if( nArg < 1 ){` |
|        3 |  9835 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9836 | `			"ArgumentCountError",` |
|        - |  9837 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9838 | `			);` |
|        - |  9839 | `	}` |
|       24 |  9840 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9841 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9842 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9843 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9844 | `		return PH7_OK;` |
|        - |  9845 | `	}` |
|        - |  9846 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9847 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9848 | `	if( !iResult ){` |
|        - |  9849 | `		/* Assertion failed */` |
|        - |  9850 | `		/* Extract optional description */` |
|       13 |  9851 | `		zDesc = 0;` |
|       13 |  9852 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9853 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9854 | `		}` |
|       13 |  9855 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9856 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9857 | `			ph7_value sFile,sLine;` |
|        - |  9858 | `			ph7_value *apCbArg[3];` |
|        - |  9859 | `			SyString *pFile;` |
|        - |  9860 | `			/* Extract the processed script */` |
|      ! 0 |  9861 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9862 | `			if( pFile == 0 ){` |
|      ! 0 |  9863 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9864 | `			}` |
|        - |  9865 | `			/* Invoke the callback */` |
|      ! 0 |  9866 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9867 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9868 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9869 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9870 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9871 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9872 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9873 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9874 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9875 | `		}` |
|       13 |  9876 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9877 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9878 | `			return PH7_ABORT;` |
|        - |  9879 | `		}` |
|        - |  9880 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9881 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9882 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9883 | `				"AssertionError",` |
|        - |  9884 | `				"%s",` |
|        1 |  9885 | `				zDesc` |
|        - |  9886 | `				);` |
|      ! 0 |  9887 | `		}else{` |
|       11 |  9888 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9889 | `				"AssertionError",` |
|        - |  9890 | `				"assert(false)"` |
|        - |  9891 | `				);` |
|        - |  9892 | `		}` |
|        - |  9893 | `	}` |
|        - |  9894 | `	/* Assertion passed */` |
|       11 |  9895 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9896 | `	return PH7_OK;` |
|       14 |  9897 |  |
|        - |  9898 | `/*` |
|        - |  9899 | ` * Section:` |
|        - |  9900 | ` *  Error reporting functions.` |
|        - |  9901 | ` * Status:` |
|        - |  9902 | ` *    Stable.` |
|        - |  9903 | ` */` |
|        - |  9904 | `/*` |
|        - |  9905 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9906 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9907 | ` * Parameters` |
|        - |  9908 | ` *  $error_msg` |
|        - |  9909 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9910 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9911 | ` * $error_type` |
|        - |  9912 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9913 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9914 | ` * Return` |
|        - |  9915 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9916 | ` */` |
|       12 |  9917 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9918 |  |
|       14 |  9919 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9920 | `	int rc = PH7_OK;` |
|       14 |  9921 | `	if( nArg > 0 ){` |
|        - |  9922 | `		const char *zErr;` |
|        - |  9923 | `		int nLen;` |
|        - |  9924 | `		/* Extract the error message */` |
|       12 |  9925 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9926 | `		if( nArg > 1 ){` |
|        - |  9927 | `			/* Extract the error type */` |
|       12 |  9928 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9929 | `			switch( nErr ){` |
|        1 |  9930 | `			case 1:   /* E_ERROR */` |
|        - |  9931 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9932 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9933 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9934 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9935 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9936 | `				break;` |
|        1 |  9937 | `			case 2:   /* E_WARNING */` |
|        - |  9938 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9939 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9940 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9941 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9942 | `				break;` |
|        3 |  9943 | `			default:` |
|        8 |  9944 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9945 | `				break;` |
|        - |  9946 | `			}` |
|        5 |  9947 | `		}` |
|        - |  9948 | `		/* Report error */` |
|       12 |  9949 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9950 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9951 | `			return rc;` |
|        - |  9952 | `		}` |
|        - |  9953 | `		/* Return true */` |
|       12 |  9954 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9955 | `	}else{` |
|        - |  9956 | `		/* Missing arguments,return FALSE */` |
|        3 |  9957 | `		ph7_result_bool(pCtx,0);` |
|        - |  9958 | `	}` |
|       14 |  9959 | `	return rc;` |
|        8 |  9960 |  |
|        - |  9961 | `/*` |
|        - |  9962 | ` * int error_reporting([int $level])` |
|        - |  9963 | ` *  Sets which PHP errors are reported.` |
|        - |  9964 | ` * Parameters` |
|        - |  9965 | ` *  $level` |
|        - |  9966 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9967 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9968 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9969 | ` *   levels will not always behave as expected.` |
|        - |  9970 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9971 | ` *   in the predefined constants.` |
|        - |  9972 | ` * Return` |
|        - |  9973 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9974 | ` *   parameter is given.` |
|        - |  9975 | ` */` |
|       38 |  9976 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9977 |  |
|       40 |  9978 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9979 | `	int nOld;` |
|        - |  9980 | `	/* Extract the old reporting level */` |
|       40 |  9981 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 |  9982 | `	if( nArg > 0 ){` |
|        - |  9983 | `		int nNew;` |
|        - |  9984 | `		/* Extract the desired error reporting level */` |
|       32 |  9985 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 |  9986 | `		if( !nNew ){` |
|        - |  9987 | `			/* Do not report errors at all */` |
|        5 |  9988 | `			pVm->bErrReport = 0;` |
|        3 |  9989 | `		}else{` |
|        - |  9990 | `			/* Report all errors */` |
|       28 |  9991 | `			pVm->bErrReport = 1;` |
|        - |  9992 | `		}` |
|       15 |  9993 | `	}` |
|        - |  9994 | `	/* Return the old level */` |
|       40 |  9995 | `	ph7_result_int(pCtx,nOld);` |
|       40 |  9996 | `	return PH7_OK;` |
|        2 |  9997 |  |
|        - |  9998 | `/*` |
|        - |  9999 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10000 | ` *  Send an error message somewhere.` |
|        - | 10001 | ` * Parameter` |
|        - | 10002 | ` *  $message` |
|        - | 10003 | ` *   The error message that should be logged.` |
|        - | 10004 | ` *  $message_type` |
|        - | 10005 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10006 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10007 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10008 | ` *       This is the default option.` |
|        - | 10009 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10010 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10011 | ` *    2  No longer an option.` |
|        - | 10012 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10013 | ` *       to the end of the message string.` |
|        - | 10014 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10015 | ` *  $destination` |
|        - | 10016 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10017 | ` *  $extra_headers` |
|        - | 10018 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10019 | ` * Return` |
|        - | 10020 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10021 | ` * NOTE:` |
|        - | 10022 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10023 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10024 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10025 | ` *  Otherwise this function is no-op.` |
|        - | 10026 | ` */` |
|        4 | 10027 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10028 |  |
|        - | 10029 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10030 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10031 | `	int iType = 0;` |
|        5 | 10032 | `	if( nArg < 1 ){` |
|        - | 10033 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10034 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10035 | `		return PH7_OK;` |
|        - | 10036 | `	}` |
|        5 | 10037 | `	if( pVm->xErrLog  ){` |
|        - | 10038 | `		/* Invoke the user callback */` |
|      ! 0 | 10039 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10040 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10041 | `		if( nArg > 1 ){` |
|      ! 0 | 10042 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10043 | `			if( nArg > 2 ){` |
|      ! 0 | 10044 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10045 | `				if( nArg > 3 ){` |
|      ! 0 | 10046 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10047 | `				}` |
|      ! 0 | 10048 | `			}` |
|      ! 0 | 10049 | `		}` |
|      ! 0 | 10050 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10051 | `	}` |
|        - | 10052 | `	/* Retun TRUE */` |
|        5 | 10053 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10054 | `	return PH7_OK;` |
|        3 | 10055 |  |
|        - | 10056 | `/*` |
|        - | 10057 | ` * bool restore_exception_handler(void)` |
|        - | 10058 | ` *  Restores the previously defined exception handler function.` |
|        - | 10059 | ` * Parameter` |
|        - | 10060 | ` *  None` |
|        - | 10061 | ` * Return` |
|        - | 10062 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10063 | ` */` |
|        4 | 10064 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10065 |  |
|        5 | 10066 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10067 | `	ph7_value *pOld,*pNew;` |
|        - | 10068 | `	/* Point to the old and the new handler */` |
|        5 | 10069 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10070 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10071 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10072 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10073 | `		SXUNUSED(apArg);` |
|        - | 10074 | `		/* No installed handler,return FALSE */` |
|        5 | 10075 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10076 | `		return PH7_OK;` |
|        - | 10077 | `	}` |
|        - | 10078 | `	/* Copy the old handler */` |
|      ! 0 | 10079 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10080 | `	PH7_MemObjRelease(pOld);` |
|        - | 10081 | `	/* Return TRUE */` |
|      ! 0 | 10082 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10083 | `	return PH7_OK;` |
|        3 | 10084 |  |
|        - | 10085 | `/*` |
|        - | 10086 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10087 | ` *  Sets a user-defined exception handler function.` |
|        - | 10088 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10089 | ` * NOTE` |
|        - | 10090 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10091 | ` *  the satndard PHP engine.` |
|        - | 10092 | ` * Parameters` |
|        - | 10093 | ` *  $exception_handler` |
|        - | 10094 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10095 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10096 | ` *   that was thrown.` |
|        - | 10097 | ` *  Note:` |
|        - | 10098 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10099 | ` * Return` |
|        - | 10100 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10101 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10102 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10103 | ` */` |
|        4 | 10104 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10105 |  |
|        6 | 10106 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10107 | `	ph7_value *pOld,*pNew;` |
|        - | 10108 | `	/* Point to the old and the new handler */` |
|        6 | 10109 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10110 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10111 | `	/* Return the old handler */` |
|        6 | 10112 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10113 | `	if( nArg > 0 ){` |
|        6 | 10114 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10115 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10116 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10117 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10118 | `		}else{` |
|        6 | 10119 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10120 | `			/* Install the new handler */` |
|        6 | 10121 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10122 | `		}` |
|        2 | 10123 | `	}` |
|        6 | 10124 | `	return PH7_OK;` |
|        2 | 10125 |  |
|        - | 10126 | `/*` |
|        - | 10127 | ` * bool restore_error_handler(void)` |
|        - | 10128 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10129 | ` * Parameters:` |
|        - | 10130 | ` *  None.` |
|        - | 10131 | ` * Return` |
|        - | 10132 | ` *  Always TRUE.` |
|        - | 10133 | ` */` |
|        4 | 10134 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10135 |  |
|        5 | 10136 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10137 | `	ph7_value *pOld,*pNew;` |
|        - | 10138 | `	/* Point to the old and the new handler */` |
|        5 | 10139 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10140 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10141 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10142 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10143 | `		SXUNUSED(apArg);` |
|        - | 10144 | `		/* No installed callback,return FALSE */` |
|        5 | 10145 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10146 | `		return PH7_OK;` |
|        - | 10147 | `	}` |
|        - | 10148 | `	/* Copy the old callback */` |
|      ! 0 | 10149 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10150 | `	PH7_MemObjRelease(pOld);` |
|        - | 10151 | `	/* Return TRUE */` |
|      ! 0 | 10152 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10153 | `	return PH7_OK;` |
|        3 | 10154 |  |
|        - | 10155 | `/*` |
|        - | 10156 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10157 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10158 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10159 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10160 | ` *  Sets a user-defined error handler function.` |
|        - | 10161 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10162 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10163 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10164 | ` *  conditions (using trigger_error()).` |
|        - | 10165 | ` * Parameters` |
|        - | 10166 | ` *  $error_handler` |
|        - | 10167 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10168 | ` *   describing the error.` |
|        - | 10169 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10170 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10171 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10172 | ` *   The function can be shown as:` |
|        - | 10173 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10174 | ` *     errno` |
|        - | 10175 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10176 | ` *   errstr` |
|        - | 10177 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10178 | ` *   errfile` |
|        - | 10179 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10180 | ` *     was raised in, as a string.` |
|        - | 10181 | ` *  Note:` |
|        - | 10182 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10183 | ` * Return` |
|        - | 10184 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10185 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10186 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10187 | ` */` |
|     9302 | 10188 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10189 |  |
|     9304 | 10190 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10191 | `	ph7_value *pOld,*pNew;` |
|        - | 10192 | `	/* Point to the old and the new handler */` |
|     9304 | 10193 | `	pOld = &pVm->aErrCB[0];` |
|     9304 | 10194 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10195 | `	/* Return the old handler */` |
|     9304 | 10196 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9304 | 10197 | `	if( nArg > 0 ){` |
|     9304 | 10198 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10199 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4651 | 10200 | `			PH7_MemObjRelease(pNew);` |
|     4651 | 10201 | `			ph7_result_bool(pCtx,1);` |
|     2326 | 10202 | `		}else{` |
|     4654 | 10203 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10204 | `			/* Install the new handler */` |
|     4654 | 10205 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10206 | `		}` |
|     4651 | 10207 | `	}` |
|     9304 | 10208 | `	return PH7_OK;` |
|        2 | 10209 |  |
|        - | 10210 | `/*` |
|        - | 10211 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10212 | ` *  Generates a backtrace.` |
|        - | 10213 | ` * Paramaeter` |
|        - | 10214 | ` *  $options` |
|        - | 10215 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10216 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10217 | ` *   all the function/method arguments, to save memory.` |
|        - | 10218 | ` * $limit` |
|        - | 10219 | ` *   (Not Used)` |
|        - | 10220 | ` * Return` |
|        - | 10221 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10222 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10223 | ` *          Name        Type      Description` |
|        - | 10224 | ` *          ------      ------     -----------` |
|        - | 10225 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10226 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10227 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10228 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10229 | ` *          object      object    The current object.` |
|        - | 10230 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10231 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10232 | ` */` |
|      522 | 10233 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10234 |  |
|      524 | 10235 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10236 | `	ph7_value *pArray;` |
|        - | 10237 | `	ph7_class *pClass;` |
|        - | 10238 | `	ph7_value *pValue;` |
|        - | 10239 | `	SyString *pFile;` |
|        - | 10240 | `	/* Create a new array */` |
|      524 | 10241 | `	pArray = ph7_context_new_array(pCtx);` |
|      524 | 10242 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      524 | 10243 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10244 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10245 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10246 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10247 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10248 | `		SXUNUSED(apArg);` |
|      ! 0 | 10249 | `		return PH7_OK;` |
|        - | 10250 | `	}` |
|        - | 10251 | `	/* Dump running function name and it's arguments  */` |
|      524 | 10252 | `	if( pVm->pFrame->pParent ){` |
|      524 | 10253 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10254 | `		ph7_vm_func *pFunc;` |
|        - | 10255 | `		ph7_value *pArg;` |
|      524 | 10256 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      524 | 10257 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      524 | 10258 | `		if( pFrame->pParent && pFunc ){` |
|      524 | 10259 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      524 | 10260 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      524 | 10261 | `			ph7_value_reset_string_cursor(pValue);` |
|      261 | 10262 | `		}` |
|        - | 10263 | `		/* Function arguments */` |
|      524 | 10264 | `		pArg = ph7_context_new_array(pCtx);` |
|      524 | 10265 | `		if( pArg  ){` |
|        - | 10266 | `			ph7_value *pObj;` |
|        - | 10267 | `			VmSlot *aSlot;` |
|        - | 10268 | `			sxu32 n;` |
|        - | 10269 | `			/* Start filling the array with the given arguments */` |
|      524 | 10270 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2082 | 10271 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1560 | 10272 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1560 | 10273 | `				if( pObj ){` |
|     1560 | 10274 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      779 | 10275 | `				}` |
|      781 | 10276 | `			}` |
|        - | 10277 | `			/* Save the array */` |
|      524 | 10278 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      261 | 10279 | `		}` |
|      261 | 10280 | `	}` |
|      524 | 10281 | `	ph7_value_int(pValue,1);` |
|        - | 10282 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10283 | `	 * line numbers at run-time. )` |
|        - | 10284 | `	 */` |
|      524 | 10285 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10286 | `	/* Current processed script */` |
|      524 | 10287 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      524 | 10288 | `	if( pFile ){` |
|      524 | 10289 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      524 | 10290 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      524 | 10291 | `		ph7_value_reset_string_cursor(pValue);` |
|      261 | 10292 | `	}` |
|        - | 10293 | `	/* Top class */` |
|      524 | 10294 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      524 | 10295 | `	if( pClass ){` |
|      520 | 10296 | `		ph7_value_reset_string_cursor(pValue);` |
|      520 | 10297 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      520 | 10298 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      259 | 10299 | `	}` |
|        - | 10300 | `	/* Return the freshly created array */` |
|      524 | 10301 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10302 | `	/*` |
|        - | 10303 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10304 | `	 * as soon we return from this function.` |
|        - | 10305 | `	 */` |
|      524 | 10306 | `	return PH7_OK;` |
|      263 | 10307 |  |
|        - | 10308 | `/*` |
|        - | 10309 | ` * Generate a small backtrace.` |
|        - | 10310 | ` * Store the generated dump in the given BLOB` |
|        - | 10311 | ` */` |
|        4 | 10312 | `static int VmMiniBacktrace(` |
|        - | 10313 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10314 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10315 | `	)` |
|        1 | 10316 |  |
|        5 | 10317 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10318 | `	ph7_vm_func *pFunc;` |
|        - | 10319 | `	ph7_class *pClass;` |
|        - | 10320 | `	SyString *pFile;` |
|        - | 10321 | `	/* Called function */` |
|        5 | 10322 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10323 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10324 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10325 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10326 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10327 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10328 | `	}else{` |
|      ! 0 | 10329 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10330 | `	}` |
|        5 | 10331 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10332 | `	/* Current processed script */` |
|        5 | 10333 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10334 | `	if( pFile ){` |
|        5 | 10335 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10336 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10337 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10338 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10339 | `	}` |
|        - | 10340 | `	/* Top class */` |
|        5 | 10341 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10342 | `	if( pClass ){` |
|      ! 0 | 10343 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10344 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10345 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10346 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10347 | `	}` |
|        5 | 10348 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10349 | `	/* All done */` |
|        5 | 10350 | `	return SXRET_OK;` |
|        1 | 10351 |  |
|        - | 10352 | `/*` |
|        - | 10353 | ` * void debug_print_backtrace()` |
|        - | 10354 | ` *  Prints a backtrace` |
|        - | 10355 | ` * Parameters` |
|        - | 10356 | ` * None` |
|        - | 10357 | ` * Return` |
|        - | 10358 | ` * NULL` |
|        - | 10359 | ` */` |
|        2 | 10360 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10361 |  |
|        3 | 10362 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10363 | `	SyBlob sDump;` |
|        3 | 10364 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10365 | `	/* Generate the backtrace */` |
|        3 | 10366 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10367 | `	/* Output backtrace */` |
|        3 | 10368 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10369 | `	/* All done,cleanup */` |
|        3 | 10370 | `	SyBlobRelease(&sDump);` |
|        1 | 10371 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10372 | `	SXUNUSED(apArg);` |
|        3 | 10373 | `	return PH7_OK;` |
|        1 | 10374 |  |
|        - | 10375 | `/*` |
|        - | 10376 | ` * string debug_string_backtrace()` |
|        - | 10377 | ` *  Generate a backtrace` |
|        - | 10378 | ` * Parameters` |
|        - | 10379 | ` * None` |
|        - | 10380 | ` * Return` |
|        - | 10381 | ` *  A mini backtrace().` |
|        - | 10382 | ` * Note that this is a symisc extension.` |
|        - | 10383 | ` */` |
|        2 | 10384 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10385 |  |
|        3 | 10386 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10387 | `	SyBlob sDump;` |
|        3 | 10388 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10389 | `	/* Generate the backtrace */` |
|        3 | 10390 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10391 | `	/* Return the backtrace */` |
|        3 | 10392 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10393 | `	/* All done,cleanup */` |
|        3 | 10394 | `	SyBlobRelease(&sDump);` |
|        1 | 10395 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10396 | `	SXUNUSED(apArg);` |
|        3 | 10397 | `	return PH7_OK;` |
|        1 | 10398 |  |
|        - | 10399 | `/*` |
|        - | 10400 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10401 | ` * exception is triggered.` |
|        - | 10402 | ` */` |
|      480 | 10403 | `static sxi32 VmUncaughtException(` |
|        - | 10404 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10405 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10406 | `	)` |
|        1 | 10407 |  |
|        - | 10408 | `	ph7_value *apArg[2],sArg;` |
|      481 | 10409 | `	int nArg = 1;` |
|        - | 10410 | `	sxi32 rc;` |
|      481 | 10411 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10412 | `		/* Nesting limit reached */` |
|      ! 0 | 10413 | `		return SXRET_OK;` |
|        - | 10414 | `	}` |
|        - | 10415 | `	/* Call any exception handler if available */` |
|      481 | 10416 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 10417 | `	if( pThis ){` |
|        - | 10418 | `		/* Load the exception instance */` |
|      481 | 10419 | `		sArg.x.pOther = pThis;` |
|      481 | 10420 | `		pThis->iRef++;` |
|      481 | 10421 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 10422 | `	}else{` |
|      ! 0 | 10423 | `		nArg = 0;` |
|        - | 10424 | `	}` |
|      481 | 10425 | `	apArg[0] = &sArg;` |
|        - | 10426 | `	/* Call the exception handler if available */` |
|      481 | 10427 | `	pVm->nExceptDepth++;` |
|      481 | 10428 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 10429 | `	pVm->nExceptDepth--;` |
|      481 | 10430 | `	if( rc != SXRET_OK ){` |
|        - | 10431 | `		SyBlob sMsgBuf;` |
|      479 | 10432 | `		const char *zClass = "Exception";` |
|      479 | 10433 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10434 | `		const char *zMsg;` |
|        - | 10435 | `		sxu32 nMsg;` |
|        - | 10436 | `		const char *zFuncName;` |
|        - | 10437 | `		int nFuncLen;` |
|      479 | 10438 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 10439 | `		if( pThis ){` |
|        - | 10440 | `			ph7_class_method *pGetMessage;` |
|        - | 10441 | `			ph7_value sMsg;` |
|        - | 10442 | `			const char *zTmp;` |
|        - | 10443 | `			int nTmp;` |
|      479 | 10444 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 10445 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 10446 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 10447 | `			if( pGetMessage ){` |
|      479 | 10448 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 10449 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 10450 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 10451 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 10452 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 10453 | `					}` |
|      239 | 10454 | `				}` |
|      479 | 10455 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 10456 | `			}` |
|      239 | 10457 | `		}` |
|      479 | 10458 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10459 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10460 | `		}` |
|      479 | 10461 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 10462 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 10463 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 10464 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 10465 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10466 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 10467 | `		rc = SXERR_ABORT;` |
|      239 | 10468 | `	}` |
|      481 | 10469 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 10470 | `	return rc;` |
|      241 | 10471 |  |
|        - | 10472 | `/*` |
|        - | 10473 | ` * Throw a user exception.` |
|        - | 10474 | ` *` |
|        - | 10475 | ` * Exception dispatch follows this sequence:` |
|        - | 10476 | ` *` |
|        - | 10477 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10478 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10479 | ` *` |
|        - | 10480 | ` * 2. If NO catch matches:` |
|        - | 10481 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10482 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10483 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10484 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10485 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10486 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10487 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10488 | ` *` |
|        - | 10489 | ` * 3. If a catch DOES match:` |
|        - | 10490 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10491 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10492 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10493 | ` *       finally block.` |
|        - | 10494 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10495 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10496 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10497 | ` *       in pPendingException (step 2c).` |
|        - | 10498 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10499 | ` *    d. Run finally (if present).` |
|        - | 10500 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10501 | ` *       that handlers are restored and finally has run.` |
|        - | 10502 | ` */` |
|      522 | 10503 | `static sxi32 VmThrowException(` |
|        - | 10504 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10505 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10506 | `	)` |
|        2 | 10507 |  |
|        - | 10508 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10509 | `	ph7_exception **apException;` |
|        - | 10510 | `	ph7_exception *pException;` |
|        - | 10511 | `	/* Point to the stack of loaded exceptions */` |
|      524 | 10512 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      524 | 10513 | `	pException = 0;` |
|      524 | 10514 | `	pCatch = 0;` |
|      524 | 10515 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10516 | `		ph7_exception_block *aCatch;` |
|        - | 10517 | `		ph7_class *pClass;` |
|        - | 10518 | `		sxu32 j;` |
|        - | 10519 | `		/* Locate the appropriate block to execute */` |
|       40 | 10520 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10521 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10522 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10523 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10524 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10525 | `			/* Extract the target class */` |
|       38 | 10526 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10527 | `			if( pClass == 0 ){` |
|        - | 10528 | `				/* No such class */` |
|      ! 0 | 10529 | `				continue;` |
|        - | 10530 | `			}` |
|       38 | 10531 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10532 | `				/* Catch block found,break immeditaley */` |
|       38 | 10533 | `				pCatch = &aCatch[j];` |
|       38 | 10534 | `				break;` |
|        - | 10535 | `			}` |
|      ! 0 | 10536 | `		}` |
|       19 | 10537 | `	}` |
|        - | 10538 | `	/* Execute the cached block if available */` |
|      524 | 10539 | `	if( pCatch == 0 ){` |
|        - | 10540 | `		sxi32 rc;` |
|        - | 10541 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      488 | 10542 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10543 | `			pException->iFinallyDone = 1;` |
|        3 | 10544 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10545 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10546 | `				return SXERR_ABORT;` |
|        - | 10547 | `			}` |
|        1 | 10548 | `		}` |
|        - | 10549 | `		/* Check if there is an outer exception handler on the stack */` |
|      488 | 10550 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10551 | `			/* Re-throw to the outer handler */` |
|        3 | 10552 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10553 | `		}` |
|        - | 10554 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10555 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10556 | `		 * exception instead of reporting it uncaught.` |
|        - | 10557 | `		 */` |
|      486 | 10558 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10559 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10560 | `			 * by looking for a catch frame on the stack.` |
|        - | 10561 | `			 */` |
|      486 | 10562 | `			VmFrame *pF = pVm->pFrame;` |
|      486 | 10563 | `			int inCatch = 0;` |
|      972 | 10564 | `			while( pF ){` |
|      492 | 10565 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        5 | 10566 | `					inCatch = 1;` |
|        5 | 10567 | `					break;` |
|        - | 10568 | `				}` |
|      487 | 10569 | `				pF = pF->pParent;` |
|        1 | 10570 | `			}` |
|      486 | 10571 | `			if( inCatch ){` |
|        - | 10572 | `				/* Defer — will be re-thrown after finally runs */` |
|        5 | 10573 | `				pThis->iRef++;` |
|        5 | 10574 | `				pVm->pPendingException = pThis;` |
|        5 | 10575 | `				return SXRET_OK;` |
|        - | 10576 | `			}` |
|      240 | 10577 | `		}` |
|        - | 10578 | `		/* Truly uncaught */` |
|      481 | 10579 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 10580 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10581 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10582 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10583 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10584 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10585 | `			}` |
|      ! 0 | 10586 | `		}` |
|      481 | 10587 | `		return rc;` |
|      ! 0 | 10588 | `	}else{` |
|       38 | 10589 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10590 | `		ph7_exception **apSaved = 0;` |
|        - | 10591 | `		sxu32 nSavedCount;` |
|        - | 10592 | `		sxi32 rc;` |
|       38 | 10593 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10594 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10595 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10596 | `		}` |
|        - | 10597 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10598 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10599 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10600 | `		 */` |
|       38 | 10601 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10602 | `		if( nSavedCount > 0 ){` |
|       10 | 10603 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10604 | `				nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10605 | `			if( apSaved ){` |
|       10 | 10606 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10607 | `					nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10608 | `				SySetReset(&pVm->aException);` |
|        3 | 10609 | `			}` |
|        3 | 10610 | `		}` |
|        - | 10611 | `		/* Create a private frame first */` |
|       38 | 10612 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10613 | `		if( rc == SXRET_OK ){` |
|       38 | 10614 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10615 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10616 | `			if( pObj ){` |
|       38 | 10617 | `				pThis->iRef++;` |
|       38 | 10618 | `				pObj->x.pOther = pThis;` |
|       38 | 10619 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10620 | `			}` |
|        - | 10621 | `			/* Execute the catch block */` |
|       38 | 10622 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10623 | `			/* Leave the frame */` |
|       38 | 10624 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10625 | `		}` |
|        - | 10626 | `		/* Restore the outer exception handlers */` |
|       38 | 10627 | `		if( apSaved ){` |
|        - | 10628 | `			sxu32 k;` |
|        - | 10629 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10630 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10631 | `			 * Restore the original outer entries.` |
|        - | 10632 | `			 */` |
|        7 | 10633 | `			SySetReset(&pVm->aException);` |
|       13 | 10634 | `			for(k = 0; k < nSavedCount; k++){` |
|        7 | 10635 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        4 | 10636 | `			}` |
|        7 | 10637 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10638 | `		}` |
|        - | 10639 | `		/* Execute the finally block after catch */` |
|       38 | 10640 | `		if( pException->iHasFinally ){` |
|       12 | 10641 | `			pException->iFinallyDone = 1;` |
|        - | 10642 | `			{` |
|       12 | 10643 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       12 | 10644 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10645 | `					return SXERR_ABORT;` |
|        - | 10646 | `				}` |
|        - | 10647 | `			}` |
|        5 | 10648 | `		}` |
|       38 | 10649 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10650 | `			return SXERR_ABORT;` |
|        - | 10651 | `		}` |
|        - | 10652 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10653 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10654 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10655 | `		 */` |
|       38 | 10656 | `		if( pVm->pPendingException ){` |
|        5 | 10657 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        5 | 10658 | `			pVm->pPendingException = 0;` |
|        5 | 10659 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10660 | `		}` |
|        - | 10661 | `	}` |
|        - | 10662 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10663 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10664 | `	 */` |
|       34 | 10665 | `	return SXRET_OK;` |
|      263 | 10666 |  |
|        - | 10667 | `/*` |
|        - | 10668 | ` * Section:` |
|        - | 10669 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10670 | ` * Status:` |
|        - | 10671 | ` *    Stable.` |
|        - | 10672 | ` */` |
|        - | 10673 | `/*` |
|        - | 10674 | ` * string ph7version(void)` |
|        - | 10675 | ` *  Returns the running version of the PH7 version.` |
|        - | 10676 | ` * Parameters` |
|        - | 10677 | ` *  None` |
|        - | 10678 | ` * Return` |
|        - | 10679 | ` * Current PH7 version.` |
|        - | 10680 | ` */` |
|        2 | 10681 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10682 |  |
|        1 | 10683 | `	SXUNUSED(nArg);` |
|        1 | 10684 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10685 | `	/* Current engine version */` |
|        3 | 10686 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10687 | `	return PH7_OK;` |
|        1 | 10688 |  |
|        - | 10689 | `/*` |
|        - | 10690 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10691 | ` */` |
|        - | 10692 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10693 | ` "<html><head>"\` |
|        - | 10694 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10695 | ` "<style type=\"text/css\">"\` |
|        - | 10696 | ` "div {"\` |
|        - | 10697 | `     "border: 1px solid #cccccc;"\` |
|        - | 10698 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10699 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10700 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10701 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10702 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10703 | `     "-o-border-radius: 10px;"\` |
|        - | 10704 | `     "border-radius: 10px;"\` |
|        - | 10705 | `     "padding-left: 2em;"\` |
|        - | 10706 | `     "background-color: white;"\` |
|        - | 10707 | `     "margin-left: auto;"\` |
|        - | 10708 | `     "font-family: verdana;"\` |
|        - | 10709 | `     "padding-right: 2em;"\` |
|        - | 10710 | `     "margin-right: auto;"\` |
|        - | 10711 | `     "}"\` |
|        - | 10712 | `     "body {"\` |
|        - | 10713 | `     "padding: 0.2em;"\` |
|        - | 10714 | `     "font-style: normal;"\` |
|        - | 10715 | `     "font-size: medium;"\` |
|        - | 10716 | `     "background-color: #f2f2f2;"\` |
|        - | 10717 | `     "}"\` |
|        - | 10718 | `     "hr {"\` |
|        - | 10719 | `     "border-style: solid none none;"\` |
|        - | 10720 | `     "border-width: 1px medium medium;"\` |
|        - | 10721 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10722 | `     "height: 1px;"\` |
|        - | 10723 | `     "}"\` |
|        - | 10724 | `     "a {"\` |
|        - | 10725 | `     "color: #3366cc;"\` |
|        - | 10726 | `     "text-decoration: none;"\` |
|        - | 10727 | `     "}"\` |
|        - | 10728 | `     "a:hover {"\` |
|        - | 10729 | `     "color: #999999;"\` |
|        - | 10730 | `     "}"\` |
|        - | 10731 | `     "a:active {"\` |
|        - | 10732 | `     "color: #663399;"\` |
|        - | 10733 | `     "}"\` |
|        - | 10734 | `     "h1 {"\` |
|        - | 10735 | `     "margin: 0;"\` |
|        - | 10736 | `     "padding: 0;"\` |
|        - | 10737 | `     "font-family: Verdana;"\` |
|        - | 10738 | `     "font-weight: bold;"\` |
|        - | 10739 | `     "font-style: normal;"\` |
|        - | 10740 | `     "font-size: medium;"\` |
|        - | 10741 | `     "text-transform: capitalize;"\` |
|        - | 10742 | `     "color: #0a328c;"\` |
|        - | 10743 | `     "}"\` |
|        - | 10744 | `     "p {"\` |
|        - | 10745 | `     "margin: 0 auto;"\` |
|        - | 10746 | `     "font-size: medium;"\` |
|        - | 10747 | `     "font-style: normal;"\` |
|        - | 10748 | `     "font-family: verdana;"\` |
|        - | 10749 | `     "}"\` |
|        - | 10750 | `"</style></head><body>"\` |
|        - | 10751 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10752 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10753 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10754 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10755 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10756 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10757 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10758 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10759 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10760 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10761 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10762 |  |
|        - | 10763 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10764 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10765 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10766 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10767 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10768 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10769 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10770 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10771 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10772 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10773 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10774 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10775 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10776 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10777 |  |
|        - | 10778 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10779 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10780 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10781 | `"&nbsp;*<br>"\` |
|        - | 10782 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10783 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10784 | `"&nbsp;* are met:<br>"\` |
|        - | 10785 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10786 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10787 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10788 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10789 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10790 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10791 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10792 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10793 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10794 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10795 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10796 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10797 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10798 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10799 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10800 | `"&nbsp;*<br>"\` |
|        - | 10801 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10802 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10803 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10804 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10805 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10806 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10807 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10808 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10809 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10810 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10811 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10812 | `"&nbsp;*/<br>"\` |
|        - | 10813 | `"</span></small></small></p>"\` |
|        - | 10814 | `"</div></body></html>"` |
|        - | 10815 | `/*` |
|        - | 10816 | ` * bool ph7credits(void)` |
|        - | 10817 | ` * bool ph7info(void)` |
|        - | 10818 | ` * bool ph7copyright(void)` |
|        - | 10819 | ` *  Prints out the credits for PH7 engine` |
|        - | 10820 | ` * Parameters` |
|        - | 10821 | ` *  None` |
|        - | 10822 | ` * Return` |
|        - | 10823 | ` *  Always TRUE` |
|        - | 10824 | ` */` |
|        2 | 10825 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10826 |  |
|        3 | 10827 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10828 | `	/* Expand the HTML page above*/` |
|        3 | 10829 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10830 | `	ph7_context_output_format(` |
|        1 | 10831 | `		pCtx,` |
|        - | 10832 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10833 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10834 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10835 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10836 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10837 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10838 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10839 | `#ifdef __WINNT__` |
|        - | 10840 | `		"Windows NT"` |
|        - | 10841 | `#elif defined(__UNIXES__)` |
|        - | 10842 | `		"UNIX-Like"` |
|        - | 10843 | `#else` |
|        - | 10844 | `		"Other OS"` |
|        - | 10845 | `#endif` |
|        - | 10846 | `		);` |
|        3 | 10847 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10848 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10849 | `	SXUNUSED(apArg);` |
|        - | 10850 | `	/* Return TRUE */` |
|        - | 10851 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10852 | `	return PH7_OK;` |
|        1 | 10853 |  |
|        - | 10854 | `/*` |
|        - | 10855 | ` * Section:` |
|        - | 10856 | ` *    URL related routines.` |
|        - | 10857 | ` * Status:` |
|        - | 10858 | ` *    Stable.` |
|        - | 10859 | ` */` |
|        - | 10860 | `/*` |
|        - | 10861 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10862 | ` *  Parse a URL and return its fields.` |
|        - | 10863 | ` * Parameters` |
|        - | 10864 | ` *  $url` |
|        - | 10865 | ` *   The URL to parse.` |
|        - | 10866 | ` * $component` |
|        - | 10867 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10868 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10869 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10870 | ` *  in which case the return value will be an integer).` |
|        - | 10871 | ` * Return` |
|        - | 10872 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10873 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10874 | ` *  this array are:` |
|        - | 10875 | ` *   scheme - e.g. http` |
|        - | 10876 | ` *   host` |
|        - | 10877 | ` *   port` |
|        - | 10878 | ` *   user` |
|        - | 10879 | ` *   pass` |
|        - | 10880 | ` *   path` |
|        - | 10881 | ` *   query - after the question mark ?` |
|        - | 10882 | ` *   fragment - after the hashmark #` |
|        - | 10883 | ` * Note:` |
|        - | 10884 | ` *  FALSE is returned on failure.` |
|        - | 10885 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10886 | ` *  with the standard PHP engine.` |
|        - | 10887 | ` */` |
|       28 | 10888 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10889 |  |
|        - | 10890 | `	const char *zStr; /* Input string */` |
|        - | 10891 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10892 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10893 | `	int nLen;` |
|        - | 10894 | `	sxi32 rc;` |
|       29 | 10895 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10896 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10897 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10898 | `		return PH7_OK;` |
|        - | 10899 | `	}` |
|        - | 10900 | `	/* Extract the given URI */` |
|       29 | 10901 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10902 | `	if( nLen < 1 ){` |
|        - | 10903 | `		/* Nothing to process,return FALSE */` |
|        3 | 10904 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10905 | `		return PH7_OK;` |
|        - | 10906 | `	}` |
|        - | 10907 | `	/* Get a parse */` |
|       27 | 10908 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10909 | `	if( rc != SXRET_OK ){` |
|        - | 10910 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10911 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10912 | `		return PH7_OK;` |
|        - | 10913 | `	}` |
|       27 | 10914 | `	if( nArg > 1 ){` |
|      ! 0 | 10915 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10916 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10917 | `		switch(nComponent){` |
|      ! 0 | 10918 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10919 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10920 | `			if( pComp->nByte < 1 ){` |
|        - | 10921 | `				/* No available value,return NULL */` |
|      ! 0 | 10922 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10923 | `			}else{` |
|      ! 0 | 10924 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10925 | `			}` |
|      ! 0 | 10926 | `			break;` |
|      ! 0 | 10927 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10928 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10929 | `			if( pComp->nByte < 1 ){` |
|        - | 10930 | `				/* No available value,return NULL */` |
|      ! 0 | 10931 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10932 | `			}else{` |
|      ! 0 | 10933 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10934 | `			}` |
|      ! 0 | 10935 | `			break;` |
|      ! 0 | 10936 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10937 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10938 | `			if( pComp->nByte < 1 ){` |
|        - | 10939 | `				/* No available value,return NULL */` |
|      ! 0 | 10940 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10941 | `			}else{` |
|      ! 0 | 10942 | `				int iPort = 0;` |
|        - | 10943 | `				/* Cast the value to integer */` |
|      ! 0 | 10944 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10945 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10946 | `			}` |
|      ! 0 | 10947 | `			break;` |
|      ! 0 | 10948 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10949 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10950 | `			if( pComp->nByte < 1 ){` |
|        - | 10951 | `				/* No available value,return NULL */` |
|      ! 0 | 10952 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10953 | `			}else{` |
|      ! 0 | 10954 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10955 | `			}` |
|      ! 0 | 10956 | `			break;` |
|      ! 0 | 10957 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10958 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10959 | `			if( pComp->nByte < 1 ){` |
|        - | 10960 | `				/* No available value,return NULL */` |
|      ! 0 | 10961 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10962 | `			}else{` |
|      ! 0 | 10963 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10964 | `			}` |
|      ! 0 | 10965 | `			break;` |
|      ! 0 | 10966 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10967 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10968 | `			if( pComp->nByte < 1 ){` |
|        - | 10969 | `				/* No available value,return NULL */` |
|      ! 0 | 10970 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10971 | `			}else{` |
|      ! 0 | 10972 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10973 | `			}` |
|      ! 0 | 10974 | `			break;` |
|      ! 0 | 10975 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10976 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10977 | `			if( pComp->nByte < 1 ){` |
|        - | 10978 | `				/* No available value,return NULL */` |
|      ! 0 | 10979 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10980 | `			}else{` |
|      ! 0 | 10981 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10982 | `			}` |
|      ! 0 | 10983 | `			break;` |
|      ! 0 | 10984 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10985 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10986 | `			if( pComp->nByte < 1 ){` |
|        - | 10987 | `				/* No available value,return NULL */` |
|      ! 0 | 10988 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10989 | `			}else{` |
|      ! 0 | 10990 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10991 | `			}` |
|      ! 0 | 10992 | `			break;` |
|      ! 0 | 10993 | `		default:` |
|        - | 10994 | `			/* No such entry,return NULL */` |
|      ! 0 | 10995 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10996 | `			break;` |
|        - | 10997 | `		}` |
|      ! 0 | 10998 | `	}else{` |
|        - | 10999 | `		ph7_value *pArray,*pValue;` |
|        - | 11000 | `		/* Return an associative array */` |
|       27 | 11001 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11002 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11003 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11004 | `			/* Out of memory */` |
|      ! 0 | 11005 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11006 | `			/* Return false */` |
|      ! 0 | 11007 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11008 | `			return PH7_OK;` |
|        - | 11009 | `		}` |
|        - | 11010 | `		/* Fill the array */` |
|       27 | 11011 | `		pComp = &sURI.sScheme;` |
|       27 | 11012 | `		if( pComp->nByte > 0 ){` |
|       19 | 11013 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11014 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11015 | `		}` |
|        - | 11016 | `		/* Reset the string cursor */` |
|       27 | 11017 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11018 | `		pComp = &sURI.sHost;` |
|       27 | 11019 | `		if( pComp->nByte > 0 ){` |
|       25 | 11020 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11021 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11022 | `		}` |
|        - | 11023 | `		/* Reset the string cursor */` |
|       27 | 11024 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11025 | `		pComp = &sURI.sPort;` |
|       27 | 11026 | `		if( pComp->nByte > 0 ){` |
|       11 | 11027 | `			int iPort = 0;/* cc warning */` |
|        - | 11028 | `			/* Convert to integer */` |
|       11 | 11029 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11030 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11031 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11032 | `		}` |
|        - | 11033 | `		/* Reset the string cursor */` |
|       27 | 11034 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11035 | `		pComp = &sURI.sUser;` |
|       27 | 11036 | `		if( pComp->nByte > 0 ){` |
|        7 | 11037 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11038 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11039 | `		}` |
|        - | 11040 | `		/* Reset the string cursor */` |
|       27 | 11041 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11042 | `		pComp = &sURI.sPass;` |
|       27 | 11043 | `		if( pComp->nByte > 0 ){` |
|        7 | 11044 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11045 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11046 | `		}` |
|        - | 11047 | `		/* Reset the string cursor */` |
|       27 | 11048 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11049 | `		pComp = &sURI.sPath;` |
|       27 | 11050 | `		if( pComp->nByte > 0 ){` |
|       17 | 11051 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11052 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11053 | `		}` |
|        - | 11054 | `		/* Reset the string cursor */` |
|       27 | 11055 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11056 | `		pComp = &sURI.sQuery;` |
|       27 | 11057 | `		if( pComp->nByte > 0 ){` |
|        5 | 11058 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11059 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11060 | `		}` |
|        - | 11061 | `		/* Reset the string cursor */` |
|       27 | 11062 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11063 | `		pComp = &sURI.sFragment;` |
|       27 | 11064 | `		if( pComp->nByte > 0 ){` |
|        5 | 11065 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11066 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11067 | `		}` |
|        - | 11068 | `		/* Return the created array */` |
|       27 | 11069 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11070 | `		/* NOTE:` |
|        - | 11071 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11072 | `		 * automatically as soon we return from this function.` |
|        - | 11073 | `		 */` |
|        - | 11074 | `	}` |
|        - | 11075 | `	/* All done */` |
|       27 | 11076 | `	return PH7_OK;` |
|       15 | 11077 |  |
|        - | 11078 | `/*` |
|        - | 11079 | ` * Section:` |
|        - | 11080 | ` *   Array related routines.` |
|        - | 11081 | ` * Status:` |
|        - | 11082 | ` *    Stable.` |
|        - | 11083 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11084 | ` *  Array related functions that need access to the underlying` |
|        - | 11085 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11086 | ` */` |
|        - | 11087 | `/*` |
|        - | 11088 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11089 | ` * of the following structure.` |
|        - | 11090 | ` */` |
|        - | 11091 | `struct compact_data` |
|        - | 11092 |  |
|        - | 11093 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11094 | `	int nRecCount;      /* Recursion count */` |
|        - | 11095 | `};` |
|        - | 11096 | `/*` |
|        - | 11097 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11098 | ` */` |
|      ! 0 | 11099 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11100 |  |
|      ! 0 | 11101 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11102 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11103 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11104 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11105 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11106 | `		SyString sVar;` |
|      ! 0 | 11107 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11108 | `		if( sVar.nByte > 0 ){` |
|        - | 11109 | `			/* Query the current frame */` |
|      ! 0 | 11110 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11111 | `			/* ^` |
|        - | 11112 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11113 | `			 */` |
|      ! 0 | 11114 | `			if( pKey ){` |
|        - | 11115 | `				/* Perform the insertion */` |
|      ! 0 | 11116 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11117 | `			}` |
|      ! 0 | 11118 | `		}` |
|      ! 0 | 11119 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11120 | `		int rc;` |
|        - | 11121 | `		/* Recursively traverse this array */` |
|      ! 0 | 11122 | `		pData->nRecCount++;` |
|      ! 0 | 11123 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11124 | `		pData->nRecCount--;` |
|      ! 0 | 11125 | `		return rc;` |
|        - | 11126 | `	}` |
|      ! 0 | 11127 | `	return SXRET_OK;` |
|      ! 0 | 11128 |  |
|        - | 11129 | `/*` |
|        - | 11130 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11131 | ` *  Create array containing variables and their values.` |
|        - | 11132 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11133 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11134 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11135 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11136 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11137 | ` * Parameters` |
|        - | 11138 | ` *  $varname` |
|        - | 11139 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11140 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11141 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11142 | ` *   it recursively.` |
|        - | 11143 | ` * Return` |
|        - | 11144 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11145 | ` */` |
|        2 | 11146 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11147 |  |
|        - | 11148 | `	ph7_value *pArray,*pObj;` |
|        3 | 11149 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11150 | `	const char *zName;` |
|        - | 11151 | `	SyString sVar;` |
|        - | 11152 | `	int i,nLen;` |
|        3 | 11153 | `	if( nArg < 1 ){` |
|        - | 11154 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11155 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11156 | `		return PH7_OK;` |
|        - | 11157 | `	}` |
|        - | 11158 | `	/* Create the array */` |
|        3 | 11159 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11160 | `	if( pArray == 0 ){` |
|        - | 11161 | `		/* Out of memory */` |
|      ! 0 | 11162 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11163 | `		/* Return NULL */` |
|      ! 0 | 11164 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11165 | `		return PH7_OK;` |
|        - | 11166 | `	}` |
|        - | 11167 | `	/* Perform the requested operation */` |
|        7 | 11168 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11169 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11170 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11171 | `				struct compact_data sData;` |
|      ! 0 | 11172 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11173 | `				/* Recursively walk the array */` |
|      ! 0 | 11174 | `				sData.nRecCount = 0;` |
|      ! 0 | 11175 | `				sData.pArray = pArray;` |
|      ! 0 | 11176 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11177 | `			}` |
|      ! 0 | 11178 | `		}else{` |
|        - | 11179 | `			/* Extract variable name */` |
|        5 | 11180 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11181 | `			if( nLen > 0 ){` |
|        5 | 11182 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11183 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11184 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11185 | `				if( pObj ){` |
|        5 | 11186 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11187 | `				}` |
|        2 | 11188 | `			}` |
|        - | 11189 | `		}` |
|        3 | 11190 | `	}` |
|        - | 11191 | `	/* Return the array */` |
|        3 | 11192 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11193 | `	return PH7_OK;` |
|        2 | 11194 |  |
|        - | 11195 | `/*` |
|        - | 11196 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11197 | ` * of the following structure.` |
|        - | 11198 | ` */` |
|        - | 11199 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11200 | `struct extract_aux_data` |
|        - | 11201 |  |
|        - | 11202 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11203 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11204 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11205 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11206 | `	int iFlags;           /* Control flags */` |
|        - | 11207 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11208 | `};` |
|        - | 11209 | `/* Forward declaration */` |
|        - | 11210 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11211 | `/*` |
|        - | 11212 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11213 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11214 | ` * Parameters` |
|        - | 11215 | ` * $var_array` |
|        - | 11216 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11217 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11218 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11219 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11220 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11221 | ` * $extract_type` |
|        - | 11222 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11223 | ` *  It can be one of the following values:` |
|        - | 11224 | ` *   EXTR_OVERWRITE` |
|        - | 11225 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11226 | ` *   EXTR_SKIP` |
|        - | 11227 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11228 | ` *   EXTR_PREFIX_SAME` |
|        - | 11229 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11230 | ` *   EXTR_PREFIX_ALL` |
|        - | 11231 | ` *       Prefix all variable names with prefix.` |
|        - | 11232 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11233 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11234 | ` *   EXTR_IF_EXISTS` |
|        - | 11235 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11236 | ` *       otherwise do nothing.` |
|        - | 11237 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11238 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11239 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11240 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11241 | ` *      the current symbol table.` |
|        - | 11242 | ` * $prefix` |
|        - | 11243 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11244 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11245 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11246 | ` *  underscore character.` |
|        - | 11247 | ` * Return` |
|        - | 11248 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11249 | ` */` |
|        4 | 11250 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11251 |  |
|        - | 11252 | `	extract_aux_data sAux;` |
|        - | 11253 | `	ph7_hashmap *pMap;` |
|        5 | 11254 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11255 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11256 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11257 | `		return PH7_OK;` |
|        - | 11258 | `	}` |
|        - | 11259 | `	/* Point to the target hashmap */` |
|        5 | 11260 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11261 | `	if( pMap->nEntry < 1 ){` |
|        - | 11262 | `		/* Empty map,return  0 */` |
|      ! 0 | 11263 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11264 | `		return PH7_OK;` |
|        - | 11265 | `	}` |
|        - | 11266 | `	/* Prepare the aux data */` |
|        5 | 11267 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11268 | `	if( nArg > 1 ){` |
|        3 | 11269 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11270 | `		if( nArg > 2 ){` |
|      ! 0 | 11271 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11272 | `		}` |
|        1 | 11273 | `	}` |
|        5 | 11274 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11275 | `	/* Invoke the worker callback */` |
|        5 | 11276 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11277 | `	/* Number of variables successfully imported */` |
|        5 | 11278 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11279 | `	return PH7_OK;` |
|        3 | 11280 |  |
|        - | 11281 | `/*` |
|        - | 11282 | ` * Worker callback for the [extract()] function defined` |
|        - | 11283 | ` * below.` |
|        - | 11284 | ` */` |
|        8 | 11285 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11286 |  |
|        9 | 11287 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11288 | `	int iFlags = pAux->iFlags;` |
|        9 | 11289 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11290 | `	ph7_value *pObj;` |
|        - | 11291 | `	SyString sVar;` |
|        9 | 11292 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11293 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11294 | `	}` |
|        - | 11295 | `	/* Perform a string cast */` |
|        9 | 11296 | `	PH7_MemObjToString(pKey);` |
|        9 | 11297 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11298 | `		/* Unavailable variable name */` |
|      ! 0 | 11299 | `		return SXRET_OK;` |
|        - | 11300 | `	}` |
|        9 | 11301 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11302 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11303 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11304 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11305 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11306 | `			);` |
|      ! 0 | 11307 | `	}else{` |
|       13 | 11308 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11309 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11310 | `	}` |
|        9 | 11311 | `	sVar.zString = pAux->zWorker;` |
|        - | 11312 | `	/* Try to extract the variable */` |
|        9 | 11313 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11314 | `	if( pObj ){` |
|        - | 11315 | `		/* Collision */` |
|        5 | 11316 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11317 | `			return SXRET_OK;` |
|        - | 11318 | `		}` |
|        5 | 11319 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11320 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11321 | `				/* Already prefixed */` |
|      ! 0 | 11322 | `				return SXRET_OK;` |
|        - | 11323 | `			}` |
|      ! 0 | 11324 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11325 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11326 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11327 | `				);` |
|      ! 0 | 11328 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11329 | `		}` |
|        3 | 11330 | `	}else{` |
|        - | 11331 | `		/* Create the variable */` |
|        5 | 11332 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11333 | `	}` |
|        9 | 11334 | `	if( pObj ){` |
|        - | 11335 | `		/* Overwrite the old value */` |
|        9 | 11336 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11337 | `		/* Increment counter */` |
|        9 | 11338 | `		pAux->iCount++;` |
|        4 | 11339 | `	}` |
|        9 | 11340 | `	return SXRET_OK;` |
|        5 | 11341 |  |
|        - | 11342 | `/*` |
|        - | 11343 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11344 | ` * defined below.` |
|        - | 11345 | ` */` |
|        2 | 11346 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11347 |  |
|        3 | 11348 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11349 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11350 | `	ph7_value *pObj;` |
|        - | 11351 | `	SyString sVar;` |
|        - | 11352 | `	/* Perform a string cast */` |
|        3 | 11353 | `	PH7_MemObjToString(pKey);` |
|        3 | 11354 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11355 | `		/* Unavailable variable name */` |
|      ! 0 | 11356 | `		return SXRET_OK;` |
|        - | 11357 | `	}` |
|        3 | 11358 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11359 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11360 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11361 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11362 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11363 | `			);` |
|        2 | 11364 | `	}else{` |
|      ! 0 | 11365 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11366 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11367 | `	}` |
|        3 | 11368 | `	sVar.zString = pAux->zWorker;` |
|        - | 11369 | `	/* Extract the variable */` |
|        3 | 11370 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11371 | `	if( pObj ){` |
|        3 | 11372 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11373 | `	}` |
|        3 | 11374 | `	return SXRET_OK;` |
|        2 | 11375 |  |
|        - | 11376 | `/*` |
|        - | 11377 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11378 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11379 | ` * Parameters` |
|        - | 11380 | ` * $types` |
|        - | 11381 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11382 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11383 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11384 | ` *  POST includes the POST uploaded file information.` |
|        - | 11385 | ` *  Note:` |
|        - | 11386 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11387 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11388 | ` * $prefix` |
|        - | 11389 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11390 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11391 | ` *  variable named $pref_userid.` |
|        - | 11392 | ` * Return` |
|        - | 11393 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11394 | ` */` |
|        2 | 11395 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11396 |  |
|        - | 11397 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11398 | `	extract_aux_data sAux;` |
|        - | 11399 | `	int nLen,nPrefixLen;` |
|        - | 11400 | `	ph7_value *pSuper;` |
|        - | 11401 | `	ph7_vm *pVm;` |
|        - | 11402 | `	/* By default import only $_GET variables  */` |
|        3 | 11403 | `	zImport = "G";` |
|        3 | 11404 | `	nLen = (int)sizeof(char);` |
|        3 | 11405 | `	zPrefix = 0;` |
|        3 | 11406 | `	nPrefixLen = 0;` |
|        3 | 11407 | `	if( nArg > 0 ){` |
|        3 | 11408 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11409 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11410 | `		}` |
|        3 | 11411 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11412 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11413 | `		}` |
|        1 | 11414 | `	}` |
|        - | 11415 | `	/* Point to the underlying VM */` |
|        3 | 11416 | `	pVm = pCtx->pVm;` |
|        - | 11417 | `	/* Initialize the aux data */` |
|        3 | 11418 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11419 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11420 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11421 | `	sAux.pVm = pVm;` |
|        - | 11422 | `	/* Extract */` |
|        3 | 11423 | `	zEnd = &zImport[nLen];` |
|        5 | 11424 | `	while( zImport < zEnd ){` |
|        3 | 11425 | `		int c = zImport[0];` |
|        3 | 11426 | `		pSuper = 0;` |
|        3 | 11427 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11428 | `			/* Import $_GET variables */` |
|        3 | 11429 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11430 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11431 | `			/* Import $_POST variables */` |
|      ! 0 | 11432 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11433 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11434 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11435 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11436 | `		}` |
|        3 | 11437 | `		if( pSuper ){` |
|        - | 11438 | `			/* Iterate throw array entries */` |
|        3 | 11439 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11440 | `		}` |
|        - | 11441 | `		/* Advance the cursor */` |
|        3 | 11442 | `		zImport++;` |
|        1 | 11443 | `	}` |
|        - | 11444 | `	/* All done,return TRUE*/` |
|        3 | 11445 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11446 | `	return PH7_OK;` |
|        1 | 11447 |  |
|        - | 11448 | `/*` |
|        - | 11449 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11450 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11451 | ` * information.` |
|        - | 11452 | ` */` |
|    10720 | 11453 | `static sxi32 VmEvalChunk(` |
|        - | 11454 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11455 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11456 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11457 | `	int iFlags,         /* Compile flag */` |
|        - | 11458 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11459 | `	)` |
|        2 | 11460 |  |
|        - | 11461 | `	SySet *pByteCode,aByteCode;` |
|        - | 11462 | `	SyBlob sSavedNs;` |
|    10722 | 11463 | `	ProcConsumer xErr = 0;` |
|    10722 | 11464 | `	void *pErrData = 0;` |
|        - | 11465 | `	/* Initialize bytecode container */` |
|    10722 | 11466 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10722 | 11467 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11468 | `	/* Reset the code generator */` |
|    10722 | 11469 | `	if( bTrueReturn ){` |
|        - | 11470 | `		/* Included file,log compile-time errors */` |
|     8110 | 11471 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8110 | 11472 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4054 | 11473 | `	}` |
|    10722 | 11474 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11475 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11476 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11477 | `	 * the caller's namespace is restored. */` |
|    10722 | 11478 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10722 | 11479 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10722 | 11480 | `	if( bTrueReturn ){` |
|        - | 11481 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8110 | 11482 | `		SyBlobReset(&pVm->sNamespace);` |
|     4054 | 11483 | `	}` |
|        - | 11484 | `	/* Swap bytecode container */` |
|    10722 | 11485 | `	pByteCode = pVm->pByteContainer;` |
|    10722 | 11486 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11487 | `	/* Compile the chunk */` |
|    10722 | 11488 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16082 | 11489 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11490 | `		/* Compilation error,return false */` |
|        3 | 11491 | `		if( pCtx ){` |
|        3 | 11492 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11493 | `		}` |
|        2 | 11494 | `	}else{` |
|        - | 11495 | `		/* Mount any newly defined classes */` |
|        - | 11496 | `		SyHashEntry *pEntry;` |
|        - | 11497 | `		ph7_class *pClass;` |
|        - | 11498 | `		ph7_value sResult; /* Return value */` |
|        - | 11499 | `		sxi32 rc;` |
|    10720 | 11500 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   368441 | 11501 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   352364 | 11502 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11503 | `			/* Only mount classes that haven't been mounted yet */` |
|   352364 | 11504 | `			if( !pClass->bMounted ){` |
|    82202 | 11505 | `				rc = VmMountUserClass(pVm,pClass);` |
|    82202 | 11506 | `				if( rc != SXRET_OK ){` |
|        - | 11507 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11508 | `					if( pCtx ){` |
|      ! 0 | 11509 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11510 | `					}` |
|      ! 0 | 11511 | `					goto Cleanup;` |
|        - | 11512 | `				}` |
|    41100 | 11513 | `			}` |
|        2 | 11514 | `		}` |
|    10720 | 11515 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11516 | `			/* Out of memory */` |
|      ! 0 | 11517 | `			if( pCtx ){` |
|      ! 0 | 11518 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11519 | `			}` |
|      ! 0 | 11520 | `			goto Cleanup;` |
|        - | 11521 | `		}` |
|    10720 | 11522 | `		if( bTrueReturn ){` |
|        - | 11523 | `			/* Assume a boolean true return value */` |
|     8110 | 11524 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4056 | 11525 | `		}else{` |
|        - | 11526 | `			/* Assume a null return value */` |
|     2612 | 11527 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11528 | `		}` |
|        - | 11529 | `		/* Execute the compiled chunk */` |
|    10720 | 11530 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10720 | 11531 | `		if( pCtx ){` |
|        - | 11532 | `			/* Set the execution result */` |
|     8128 | 11533 | `			ph7_result_value(pCtx,&sResult);` |
|     4063 | 11534 | `		}` |
|    10720 | 11535 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11536 | `	}` |
|     5360 | 11537 | `Cleanup:` |
|        - | 11538 | `	/* Cleanup the mess left behind */` |
|    10722 | 11539 | `	pVm->pByteContainer = pByteCode;` |
|    10722 | 11540 | `	SySetRelease(&aByteCode);` |
|        - | 11541 | `	/* Restore caller's namespace state */` |
|    10722 | 11542 | `	SyBlobReset(&pVm->sNamespace);` |
|    10722 | 11543 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10722 | 11544 | `	SyBlobRelease(&sSavedNs);` |
|    10722 | 11545 | `	return SXRET_OK;` |
|        2 | 11546 |  |
|        - | 11547 | `/*` |
|        - | 11548 | ` * value eval(string $code)` |
|        - | 11549 | ` *   Evaluate a string as PHP code.` |
|        - | 11550 | ` * Parameter` |
|        - | 11551 | ` *  code: PHP code to evaluate.` |
|        - | 11552 | ` * Return` |
|        - | 11553 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11554 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11555 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11556 | ` */` |
|       22 | 11557 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11558 |  |
|        - | 11559 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11560 | `	if( nArg < 1 ){` |
|        - | 11561 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11562 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11563 | `		return SXRET_OK;` |
|        - | 11564 | `	}` |
|        - | 11565 | `	/* Chunk to evaluate */` |
|       24 | 11566 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11567 | `	if( sChunk.nByte < 1 ){` |
|        - | 11568 | `		/* Empty string,return NULL */` |
|        3 | 11569 | `		ph7_result_null(pCtx);` |
|        3 | 11570 | `		return SXRET_OK;` |
|        - | 11571 | `	}` |
|        - | 11572 | `	/* Eval the chunk */` |
|       22 | 11573 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11574 | `	return SXRET_OK;` |
|       13 | 11575 |  |
|        - | 11576 | `/*` |
|        - | 11577 | ` * Check if a file path is already included.` |
|        - | 11578 | ` */` |
|    16212 | 11579 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11580 |  |
|        - | 11581 | `	SyString *aEntries;` |
|        - | 11582 | `	sxu32 n;` |
|    16214 | 11583 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11584 | `	/* Perform a linear search */` |
| 65662734 | 11585 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 65646528 | 11586 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11587 | `			/* Already included */` |
|        7 | 11588 | `			return TRUE;` |
|        - | 11589 | `		}` |
| 32823262 | 11590 | `	}` |
|    16208 | 11591 | `	return FALSE;` |
|     8108 | 11592 |  |
|        - | 11593 | `/*` |
|        - | 11594 | ` * Push a file path in the appropriate VM container.` |
|        - | 11595 | ` */` |
|    18796 | 11596 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11597 |  |
|        - | 11598 | `	SyString sPath;` |
|        - | 11599 | `	char *zDup;` |
|        - | 11600 | `#ifdef __WINNT__` |
|        - | 11601 | `	char *zCur;` |
|        - | 11602 | `#endif` |
|        - | 11603 | `	sxi32 rc;` |
|    18798 | 11604 | `	if( nLen < 0 ){` |
|     2586 | 11605 | `		nLen = SyStrlen(zPath);` |
|     1292 | 11606 | `	}` |
|        - | 11607 | `	/* Duplicate the file path first */` |
|    18798 | 11608 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18798 | 11609 | `	if( zDup == 0 ){` |
|      ! 0 | 11610 | `		return SXERR_MEM;` |
|        - | 11611 | `	}` |
|        - | 11612 | `#ifdef __WINNT__` |
|        - | 11613 | `	/* Normalize path on windows` |
|        - | 11614 | `	 * Example:` |
|        - | 11615 | `	 *    Path/To/File.php` |
|        - | 11616 | `	 * becomes` |
|        - | 11617 | `	 *   path\to\file.php` |
|        - | 11618 | `	 */` |
|        2 | 11619 | `	zCur = zDup;` |
|        2 | 11620 | `	while( zCur[0] != 0 ){` |
|        2 | 11621 | `		if( zCur[0] == '/' ){` |
|        2 | 11622 | `			zCur[0] = '\\';` |
|        2 | 11623 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11624 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11625 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11626 | `		}` |
|        2 | 11627 | `		zCur++;` |
|        2 | 11628 | `	}` |
|        - | 11629 | `#endif` |
|        - | 11630 | `	/* Install the file path */` |
|    18798 | 11631 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18798 | 11632 | `	if( !bMain ){` |
|    16214 | 11633 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11634 | `			/* Already included */` |
|        7 | 11635 | `			*pNew = 0;` |
|        4 | 11636 | `		}else{` |
|        - | 11637 | `			/* Insert in the corresponding container */` |
|    16208 | 11638 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16208 | 11639 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11640 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11641 | `				return rc;` |
|        - | 11642 | `			}` |
|    16208 | 11643 | `			*pNew = 1;` |
|        - | 11644 | `		}` |
|     8106 | 11645 | `	}` |
|    18798 | 11646 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18798 | 11647 | `	return SXRET_OK;` |
|     9400 | 11648 |  |
|        - | 11649 | `/*` |
|        - | 11650 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11651 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11652 | ` * indicates failure.` |
|        - | 11653 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11654 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11655 | ` * operations.` |
|        - | 11656 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11657 | ` * this function is a no-op.` |
|        - | 11658 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11659 | ` * constructs for more information.` |
|        - | 11660 | ` */` |
|     8118 | 11661 | `static sxi32 VmExecIncludedFile(` |
|        - | 11662 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11663 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11664 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11665 | `	 )` |
|        2 | 11666 |  |
|        - | 11667 | `	sxi32 rc;` |
|        - | 11668 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11669 | `	const ph7_io_stream *pStream;` |
|        - | 11670 | `	SyBlob sContents;` |
|        - | 11671 | `	void *pHandle;` |
|        - | 11672 | `	ph7_vm *pVm;` |
|        - | 11673 | `	int isNew;` |
|        - | 11674 | `	/* Initialize fields */` |
|     8120 | 11675 | `	pVm = pCtx->pVm;` |
|     8120 | 11676 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8120 | 11677 | `	isNew = 0;` |
|        - | 11678 | `	/* Extract the associated stream */` |
|     8120 | 11679 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11680 | `	/*` |
|        - | 11681 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11682 | `	 * in a read-only mode.` |
|        - | 11683 | `	 */` |
|     8120 | 11684 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8120 | 11685 | `	if( pHandle == 0 ){` |
|        8 | 11686 | `		return SXERR_IO;` |
|        - | 11687 | `	}` |
|     8114 | 11688 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8114 | 11689 | `	if( IncludeOnce && !isNew ){` |
|        - | 11690 | `		/* Already included */` |
|        5 | 11691 | `		rc = SXERR_EXISTS;` |
|        3 | 11692 | `	}else{` |
|        - | 11693 | `		/* Read the whole file contents */` |
|     8110 | 11694 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8110 | 11695 | `		if( rc == SXRET_OK ){` |
|        - | 11696 | `			SyString sScript;` |
|        - | 11697 | `			/* Compile and execute the script */` |
|     8110 | 11698 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8110 | 11699 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4054 | 11700 | `		}` |
|        - | 11701 | `	}` |
|        - | 11702 | `	/* Pop from the set of included file */` |
|     8114 | 11703 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11704 | `	/* Close the handle */` |
|     8114 | 11705 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11706 | `	/* Release the working buffer */` |
|     8114 | 11707 | `	SyBlobRelease(&sContents);` |
|        - | 11708 | `#else` |
|        - | 11709 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11710 | `	SXUNUSED(pPath);` |
|        - | 11711 | `	SXUNUSED(IncludeOnce);` |
|        - | 11712 | `	rc = SXERR_IO;` |
|        - | 11713 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8114 | 11714 | `	return rc;` |
|     4061 | 11715 |  |
|        - | 11716 | `/*` |
|        - | 11717 | ` * string get_include_path(void)` |
|        - | 11718 | ` *  Gets the current include_path configuration option.` |
|        - | 11719 | ` * Parameter` |
|        - | 11720 | ` *  None` |
|        - | 11721 | ` * Return` |
|        - | 11722 | ` *  Included paths as a string` |
|        - | 11723 | ` */` |
|        2 | 11724 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11725 |  |
|        3 | 11726 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11727 | `	SyString *aEntry;` |
|        - | 11728 | `	int dir_sep;` |
|        - | 11729 | `	sxu32 n;` |
|        - | 11730 | `#ifdef __WINNT__` |
|        1 | 11731 | `	dir_sep = ';';` |
|        - | 11732 | `#else` |
|        - | 11733 | `	/* Assume UNIX path separator */` |
|        2 | 11734 | `	dir_sep = ':';` |
|        - | 11735 | `#endif` |
|        1 | 11736 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11737 | `	SXUNUSED(apArg);` |
|        - | 11738 | `	/* Point to the list of import paths */` |
|        3 | 11739 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11740 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11741 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11742 | `		if( n > 0 ){` |
|        - | 11743 | `			/* Append dir seprator */` |
|      ! 0 | 11744 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11745 | `		}` |
|        - | 11746 | `		/* Append path */` |
|        3 | 11747 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11748 | `	}` |
|        3 | 11749 | `	return PH7_OK;` |
|        1 | 11750 |  |
|        - | 11751 | `/*` |
|        - | 11752 | ` * string get_get_included_files(void)` |
|        - | 11753 | ` *  Gets the current include_path configuration option.` |
|        - | 11754 | ` * Parameter` |
|        - | 11755 | ` *  None` |
|        - | 11756 | ` * Return` |
|        - | 11757 | ` *  Included paths as a string` |
|        - | 11758 | ` */` |
|        2 | 11759 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11760 |  |
|        3 | 11761 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11762 | `	ph7_value *pArray,*pWorker;` |
|        - | 11763 | `	SyString *pEntry;` |
|        - | 11764 | `	int c,d;` |
|        - | 11765 | `	/* Create an array and a working value */` |
|        3 | 11766 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11767 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11768 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11769 | `		/* Out of memory,return null */` |
|      ! 0 | 11770 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11771 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11772 | `		SXUNUSED(apArg);` |
|      ! 0 | 11773 | `		return PH7_OK;` |
|        - | 11774 | `	}` |
|        3 | 11775 | `	c = d = '/';` |
|        - | 11776 | `#ifdef __WINNT__` |
|        1 | 11777 | `	d = '\\';` |
|        - | 11778 | `#endif` |
|        - | 11779 | `	/* Iterate throw entries */` |
|        3 | 11780 | `	SySetResetCursor(pFiles);` |
|     3839 | 11781 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11782 | `		const char *zBase,*zEnd;` |
|        - | 11783 | `		int iLen;` |
|        - | 11784 | `		/* reset the string cursor */` |
|     3837 | 11785 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11786 | `		/* Extract base name */` |
|     3837 | 11787 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11788 | `		/* Ignore trailing '/' */` |
|     5755 | 11789 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11790 | `			zEnd--;` |
|      ! 0 | 11791 | `		}` |
|     3837 | 11792 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 11793 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 11794 | `			zEnd--;` |
|        1 | 11795 | `		}` |
|     3837 | 11796 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 11797 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11798 | `		/* Copy entry name */` |
|     3837 | 11799 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11800 | `		/* Perform the insertion */` |
|     3837 | 11801 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11802 | `	}` |
|        - | 11803 | `	/* All done,return the created array */` |
|        3 | 11804 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11805 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11806 | `	 * by the engine as soon we return from this foreign` |
|        - | 11807 | `	 * function.` |
|        - | 11808 | `	 */` |
|        3 | 11809 | `	return PH7_OK;` |
|        2 | 11810 |  |
|        - | 11811 | `/*` |
|        - | 11812 | ` * include:` |
|        - | 11813 | ` * According to the PHP reference manual.` |
|        - | 11814 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11815 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11816 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11817 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11818 | ` *  and the current working directory before failing. The include()` |
|        - | 11819 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11820 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11821 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11822 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11823 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11824 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11825 | ` *  directory to find the requested file.` |
|        - | 11826 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11827 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11828 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11829 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11830 | ` */` |
|     8100 | 11831 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11832 |  |
|        - | 11833 | `	SyString sFile;` |
|        - | 11834 | `	sxi32 rc;` |
|     8102 | 11835 | `	if( nArg < 1 ){` |
|        - | 11836 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11837 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11838 | `		return SXRET_OK;` |
|        - | 11839 | `	}` |
|        - | 11840 | `	/* File to include */` |
|     8102 | 11841 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8102 | 11842 | `	if( sFile.nByte < 1 ){` |
|        - | 11843 | `		/* Empty string,return NULL */` |
|      ! 0 | 11844 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11845 | `		return SXRET_OK;` |
|        - | 11846 | `	}` |
|        - | 11847 | `	/* Open,compile and execute the desired script */` |
|     8102 | 11848 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8102 | 11849 | `	if( rc != SXRET_OK ){` |
|        - | 11850 | `		/* Emit a warning and return false */` |
|        3 | 11851 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11852 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11853 | `	}` |
|     8102 | 11854 | `	return SXRET_OK;` |
|     4052 | 11855 |  |
|        - | 11856 | `/*` |
|        - | 11857 | ` * include_once:` |
|        - | 11858 | ` *  According to the PHP reference manual.` |
|        - | 11859 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11860 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11861 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11862 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11863 | ` *   just once.` |
|        - | 11864 | ` */` |
|        4 | 11865 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11866 |  |
|        - | 11867 | `	SyString sFile;` |
|        - | 11868 | `	sxi32 rc;` |
|        5 | 11869 | `	if( nArg < 1 ){` |
|        - | 11870 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11871 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11872 | `		return SXRET_OK;` |
|        - | 11873 | `	}` |
|        - | 11874 | `	/* File to include */` |
|        5 | 11875 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11876 | `	if( sFile.nByte < 1 ){` |
|        - | 11877 | `		/* Empty string,return NULL */` |
|      ! 0 | 11878 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11879 | `		return SXRET_OK;` |
|        - | 11880 | `	}` |
|        - | 11881 | `	/* Open,compile and execute the desired script */` |
|        5 | 11882 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11883 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11884 | `		/* File already included,return TRUE */` |
|        3 | 11885 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11886 | `		return SXRET_OK;` |
|        - | 11887 | `	}` |
|        3 | 11888 | `	if( rc != SXRET_OK ){` |
|        - | 11889 | `		/* Emit a warning and return false */` |
|      ! 0 | 11890 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11891 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11892 | ` 	}` |
|        3 | 11893 | `	return SXRET_OK;` |
|        3 | 11894 |  |
|        - | 11895 | `/*` |
|        - | 11896 | ` * require.` |
|        - | 11897 | ` *  According to the PHP reference manual.` |
|        - | 11898 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11899 | ` *   also produce a fatal level error.` |
|        - | 11900 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11901 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11902 | ` */` |
|        6 | 11903 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11904 |  |
|        - | 11905 | `	SyString sFile;` |
|        - | 11906 | `	sxi32 rc;` |
|        8 | 11907 | `	if( nArg < 1 ){` |
|        - | 11908 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11909 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11910 | `		return SXRET_OK;` |
|        - | 11911 | `	}` |
|        - | 11912 | `	/* File to include */` |
|        8 | 11913 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11914 | `	if( sFile.nByte < 1 ){` |
|        - | 11915 | `		/* Empty string,return NULL */` |
|      ! 0 | 11916 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11917 | `		return SXRET_OK;` |
|        - | 11918 | `	}` |
|        - | 11919 | `	/* Open,compile and execute the desired script */` |
|        8 | 11920 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 11921 | `	if( rc != SXRET_OK ){` |
|        - | 11922 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11923 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11924 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11925 | `		return PH7_ABORT;` |
|        - | 11926 | `	}` |
|        8 | 11927 | `	return SXRET_OK;` |
|        5 | 11928 |  |
|        - | 11929 | `/*` |
|        - | 11930 | ` * require_once:` |
|        - | 11931 | ` *  According to the PHP reference manual.` |
|        - | 11932 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11933 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11934 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11935 | ` *   and how it differs from its non _once siblings.` |
|        - | 11936 | ` */` |
|        4 | 11937 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11938 |  |
|        - | 11939 | `	SyString sFile;` |
|        - | 11940 | `	sxi32 rc;` |
|        5 | 11941 | `	if( nArg < 1 ){` |
|        - | 11942 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11943 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11944 | `		return SXRET_OK;` |
|        - | 11945 | `	}` |
|        - | 11946 | `	/* File to include */` |
|        5 | 11947 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11948 | `	if( sFile.nByte < 1 ){` |
|        - | 11949 | `		/* Empty string,return NULL */` |
|      ! 0 | 11950 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11951 | `		return SXRET_OK;` |
|        - | 11952 | `	}` |
|        - | 11953 | `	/* Open,compile and execute the desired script */` |
|        5 | 11954 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11955 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11956 | `		/* File already included,return TRUE */` |
|        3 | 11957 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11958 | `		return SXRET_OK;` |
|        - | 11959 | `	}` |
|        3 | 11960 | `	if( rc != SXRET_OK ){` |
|        - | 11961 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11962 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11963 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11964 | `		return PH7_ABORT;` |
|        - | 11965 | `	}` |
|        3 | 11966 | `	return SXRET_OK;` |
|        3 | 11967 |  |
|        - | 11968 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11969 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11970 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11971 | `/*` |
|        - | 11972 | ` * Section:` |
|        - | 11973 | ` *  SPL Autoloading functions.` |
|        - | 11974 | ` * Status:` |
|        - | 11975 | ` *  Stable.` |
|        - | 11976 | ` */` |
|        - | 11977 | `/*` |
|        - | 11978 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 11979 | ` *  Register given function as __autoload() implementation.` |
|        - | 11980 | ` * Parameters` |
|        - | 11981 | ` *  callback` |
|        - | 11982 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 11983 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 11984 | ` *  throw` |
|        - | 11985 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 11986 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 11987 | ` *  prepend` |
|        - | 11988 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 11989 | ` *   autoload stack instead of appending it.` |
|        - | 11990 | ` * Return` |
|        - | 11991 | ` *  TRUE on success, FALSE on failure.` |
|        - | 11992 | ` */` |
|       34 | 11993 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11994 |  |
|        - | 11995 | `	VmAutoloadCB sEntry;` |
|       36 | 11996 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 11997 | `	int iPrepend = 0;` |
|        - | 11998 | `	sxu32 n;` |
|       36 | 11999 | `	if( nArg < 1 ){` |
|        - | 12000 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12001 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12002 | `		/* Check for duplicates first */` |
|        9 | 12003 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12004 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12005 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12006 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12007 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12008 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12009 | `				return SXRET_OK;` |
|        - | 12010 | `			}` |
|      ! 0 | 12011 | `		}` |
|        5 | 12012 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12013 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12014 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12015 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12016 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12017 | `		return SXRET_OK;` |
|        - | 12018 | `	}` |
|        - | 12019 | `	/* Validate that the callback is callable */` |
|       28 | 12020 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12021 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12022 | `		if( nArg >= 2 ){` |
|      ! 0 | 12023 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12024 | `		}` |
|      ! 0 | 12025 | `		if( iThrow ){` |
|      ! 0 | 12026 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12027 | `				"Argument is not callable");` |
|      ! 0 | 12028 | `		}` |
|      ! 0 | 12029 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12030 | `		return SXRET_OK;` |
|        - | 12031 | `	}` |
|        - | 12032 | `	/* Check for duplicates */` |
|       46 | 12033 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12034 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12035 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12036 | `			/* Already registered */` |
|      ! 0 | 12037 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12038 | `			return SXRET_OK;` |
|        - | 12039 | `		}` |
|       11 | 12040 | `	}` |
|        - | 12041 | `	/* Check prepend flag */` |
|       28 | 12042 | `	if( nArg >= 3 ){` |
|        3 | 12043 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12044 | `	}` |
|        - | 12045 | `	/* Store the callback */` |
|       28 | 12046 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12047 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12048 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12049 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12050 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12051 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12052 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12053 | `		VmAutoloadCB *aBase;` |
|        3 | 12054 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12055 | `		/* Rotate: move last entry to front */` |
|        3 | 12056 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12057 | `		if( aBase ){` |
|        - | 12058 | `			VmAutoloadCB sTemp;` |
|        - | 12059 | `			sxu32 i;` |
|        3 | 12060 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12061 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12062 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12063 | `			}` |
|        3 | 12064 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12065 | `		}` |
|        2 | 12066 | `	}else{` |
|       26 | 12067 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12068 | `	}` |
|       28 | 12069 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12070 | `	return SXRET_OK;` |
|       19 | 12071 |  |
|        - | 12072 | `/*` |
|        - | 12073 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12074 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12075 | ` * Parameters` |
|        - | 12076 | ` *  callback` |
|        - | 12077 | ` *   The autoload function being unregistered.` |
|        - | 12078 | ` * Return` |
|        - | 12079 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12080 | ` */` |
|       32 | 12081 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12082 |  |
|       34 | 12083 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12084 | `	sxu32 n,nEntry;` |
|       34 | 12085 | `	if( nArg < 1 ){` |
|      ! 0 | 12086 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12087 | `		return SXRET_OK;` |
|        - | 12088 | `	}` |
|       34 | 12089 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12090 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12091 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12092 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12093 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12094 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12095 | `			sxu32 i;` |
|       32 | 12096 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12097 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12098 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12099 | `			}` |
|        - | 12100 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12101 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12102 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12103 | `			return SXRET_OK;` |
|        - | 12104 | `		}` |
|        3 | 12105 | `	}` |
|        3 | 12106 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12107 | `	return SXRET_OK;` |
|       18 | 12108 |  |
|        - | 12109 | `/*` |
|        - | 12110 | ` * array spl_autoload_functions(void)` |
|        - | 12111 | ` *  Return all registered __autoload() functions.` |
|        - | 12112 | ` * Return` |
|        - | 12113 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12114 | ` *  an empty array is returned.` |
|        - | 12115 | ` */` |
|       20 | 12116 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12117 |  |
|       21 | 12118 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12119 | `	ph7_value *pArray;` |
|        - | 12120 | `	sxu32 n,nEntry;` |
|       10 | 12121 | `	SXUNUSED(nArg);` |
|       10 | 12122 | `	SXUNUSED(apArg);` |
|       21 | 12123 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12124 | `	if( pArray == 0 ){` |
|      ! 0 | 12125 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12126 | `		return SXRET_OK;` |
|        - | 12127 | `	}` |
|       21 | 12128 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12129 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12130 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12131 | `		if( pEntry ){` |
|       15 | 12132 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12133 | `		}` |
|        8 | 12134 | `	}` |
|       21 | 12135 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12136 | `	return SXRET_OK;` |
|       11 | 12137 |  |
|        - | 12138 | `/*` |
|        - | 12139 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12140 | ` *  Default implementation of __autoload().` |
|        - | 12141 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12142 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12143 | ` * Parameters` |
|        - | 12144 | ` *  class` |
|        - | 12145 | ` *   The class name being searched.` |
|        - | 12146 | ` *  file_extensions` |
|        - | 12147 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12148 | ` */` |
|        2 | 12149 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12150 |  |
|        - | 12151 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12152 | `	SyBlob sPath;` |
|        - | 12153 | `	int nClass;` |
|        - | 12154 | `	sxi32 rc;` |
|        3 | 12155 | `	if( nArg < 1 ){` |
|      ! 0 | 12156 | `		return SXRET_OK;` |
|        - | 12157 | `	}` |
|        3 | 12158 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12159 | `	if( nClass < 1 ){` |
|      ! 0 | 12160 | `		return SXRET_OK;` |
|        - | 12161 | `	}` |
|        - | 12162 | `	/* Default extensions */` |
|        3 | 12163 | `	zExt = ".php,.inc";` |
|        3 | 12164 | `	if( nArg >= 2 ){` |
|        - | 12165 | `		int nExt;` |
|      ! 0 | 12166 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12167 | `		if( nExt < 1 ){` |
|      ! 0 | 12168 | `			zExt = ".php,.inc";` |
|      ! 0 | 12169 | `		}` |
|      ! 0 | 12170 | `	}` |
|        3 | 12171 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12172 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12173 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12174 | `	zCur = zExt;` |
|        7 | 12175 | `	while( zCur < zEnd ){` |
|        - | 12176 | `		const char *zComma;` |
|        - | 12177 | `		SyString sFile;` |
|        - | 12178 | `		int i;` |
|        - | 12179 | `		/* Find next comma or end */` |
|        5 | 12180 | `		zComma = zCur;` |
|       21 | 12181 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12182 | `			zComma++;` |
|        1 | 12183 | `		}` |
|        - | 12184 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12185 | `		SyBlobReset(&sPath);` |
|       69 | 12186 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12187 | `			char c = zClass[i];` |
|       65 | 12188 | `			if( c == '\\' ){` |
|      ! 0 | 12189 | `				c = '/';` |
|       65 | 12190 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12191 | `				c = c + ('a' - 'A');` |
|        6 | 12192 | `			}` |
|       65 | 12193 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12194 | `		}` |
|        - | 12195 | `		/* Append extension */` |
|        5 | 12196 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12197 | `		/* Try to include the file */` |
|        5 | 12198 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12199 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12200 | `		if( rc == SXRET_OK ){` |
|        - | 12201 | `			/* File included successfully */` |
|      ! 0 | 12202 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12203 | `			return SXRET_OK;` |
|        - | 12204 | `		}` |
|        - | 12205 | `		/* Move past the comma */` |
|        5 | 12206 | `		zCur = zComma;` |
|        5 | 12207 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12208 | `			zCur++;` |
|        1 | 12209 | `		}` |
|        1 | 12210 | `	}` |
|        3 | 12211 | `	SyBlobRelease(&sPath);` |
|        3 | 12212 | `	return SXRET_OK;` |
|        2 | 12213 |  |
|        - | 12214 | `/* Table of built-in VM functions. */` |
|        - | 12215 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12216 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12217 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12218 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12219 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12220 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12221 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12222 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12223 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12224 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12225 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12226 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12227 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12228 | `	    /* Constants management */` |
|        - | 12229 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12230 | `	{ "define",   vm_builtin_define               },` |
|        - | 12231 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12232 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12233 | `	   /* Class/Object functions */` |
|        - | 12234 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12235 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12236 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12237 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12238 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12239 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12240 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12241 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12242 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12243 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12244 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12245 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12246 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12247 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12248 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12249 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12250 | `	   /* SPL Autoloading */` |
|        - | 12251 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12252 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12253 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12254 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12255 | `	   /* Random numbers/strings generators */` |
|        - | 12256 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12257 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12258 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12259 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12260 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12261 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12262 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12263 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12264 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12265 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12266 | `	   /* Language constructs functions */` |
|        - | 12267 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12268 | `	{ "print", vm_builtin_print                   },` |
|        - | 12269 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12270 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12271 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12272 | `	  /* Variable handling functions */` |
|        - | 12273 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12274 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12275 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12276 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12277 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12278 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12279 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12280 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12281 | `	  /* Ouput control functions */` |
|        - | 12282 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12283 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12284 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12285 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12286 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12287 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12288 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12289 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12290 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12291 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12292 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12293 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12294 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12295 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12296 | `	  /* Assertion functions */` |
|        - | 12297 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12298 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12299 | `	  /* Error reporting functions */` |
|        - | 12300 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12301 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12302 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12303 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12304 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12305 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12306 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12307 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12308 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12309 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12310 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12311 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12312 | `	  /* Release info */` |
|        - | 12313 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12314 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12315 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12316 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12317 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12318 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12319 | `	  /* hashmap */` |
|        - | 12320 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12321 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12322 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12323 | `	  /* URL related function */` |
|        - | 12324 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12325 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12326 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12327 | `	   /* XML processing functions */` |
|        - | 12328 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12329 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12330 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12331 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12332 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12333 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12334 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12335 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12336 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12337 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12338 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12339 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12340 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12341 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12342 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12343 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12344 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12345 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12346 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12347 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12348 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12349 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12350 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12351 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12352 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12353 | `	   /* Command line processing */` |
|        - | 12354 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12355 | `	   /* JSON encoding/decoding */` |
|        - | 12356 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12357 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12358 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12359 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12360 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12361 | `	   /* Files/URI inclusion facility */` |
|        - | 12362 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12363 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12364 | `	{ "include",      vm_builtin_include          },` |
|        - | 12365 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12366 | `	{ "require",      vm_builtin_require          },` |
|        - | 12367 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12368 | `};` |
|        - | 12369 | `/*` |
|        - | 12370 | ` * Register the built-in VM functions defined above.` |
|        - | 12371 | ` */` |
|     2332 | 12372 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12373 |  |
|        - | 12374 | `	sxi32 rc;` |
|        - | 12375 | `	sxu32 n;` |
|   300830 | 12376 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12377 | `		/* Note that these special functions have access` |
|        - | 12378 | `		 * to the underlying virtual machine as their` |
|        - | 12379 | `		 * private data.` |
|        - | 12380 | `		 */` |
|   298498 | 12381 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   298498 | 12382 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12383 | `			return rc;` |
|        - | 12384 | `		}` |
|   149250 | 12385 | `	}` |
|     2334 | 12386 | `	return SXRET_OK;` |
|     1168 | 12387 |  |
|        - | 12388 | `/*` |
|        - | 12389 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12390 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12391 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12392 | ` */` |
|    32696 | 12393 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12394 |  |
|    32698 | 12395 | `	if( !iLoadable ){` |
|    31484 | 12396 | `		return pClass;` |
|        - | 12397 | `	}` |
|     1216 | 12398 | `	while(pClass){` |
|     1216 | 12399 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1216 | 12400 | `			return pClass;` |
|        - | 12401 | `		}` |
|      ! 0 | 12402 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12403 | `	}` |
|      ! 0 | 12404 | `	return 0;` |
|    16350 | 12405 |  |
|        - | 12406 | `/*` |
|        - | 12407 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12408 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12409 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12410 | ` * registered in the VM's class table.` |
|        - | 12411 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12412 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12413 | ` */` |
|       30 | 12414 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12415 |  |
|        - | 12416 | `	VmAutoloadCB *pEntry;` |
|        - | 12417 | `	ph7_value sArg,sResult;` |
|        - | 12418 | `	SyHashEntry *pHashEntry;` |
|        - | 12419 | `	ph7_class *pClass;` |
|        - | 12420 | `	sxu32 n,nEntry;` |
|       32 | 12421 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       32 | 12422 | `	if( nEntry < 1 ){` |
|       18 | 12423 | `		return 0;` |
|        - | 12424 | `	}` |
|        - | 12425 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12426 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12427 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12428 | `	}` |
|        - | 12429 | `	/* Mark this class as being autoloaded */` |
|       14 | 12430 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12431 | `	/* Prepare the class name argument */` |
|       14 | 12432 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12433 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12434 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12435 | `	pClass = 0;` |
|       28 | 12436 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12437 | `		ph7_value *apArg[1];` |
|       24 | 12438 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12439 | `		if( pEntry == 0 ){` |
|      ! 0 | 12440 | `			continue;` |
|        - | 12441 | `		}` |
|       24 | 12442 | `		apArg[0] = &sArg;` |
|       24 | 12443 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12444 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12445 | `			continue;` |
|        - | 12446 | `		}` |
|        - | 12447 | `		/* Check if the class is now available */` |
|       24 | 12448 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12449 | `		if( pHashEntry ){` |
|       10 | 12450 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12451 | `			if( pClass ){` |
|       10 | 12452 | `				break;` |
|        - | 12453 | `			}` |
|      ! 0 | 12454 | `		}` |
|        9 | 12455 | `	}` |
|       14 | 12456 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12457 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12458 | `	/* Remove reentrancy guard */` |
|       14 | 12459 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12460 | `	return pClass;` |
|       17 | 12461 |  |
|        - | 12462 | `/*` |
|        - | 12463 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12464 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12465 | ` */` |
|       18 | 12466 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12467 |  |
|       20 | 12468 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12469 |  |
|        - | 12470 | `/*` |
|        - | 12471 | ` * Check if the given name refer to an installed class.` |
|        - | 12472 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12473 | ` */` |
|    32700 | 12474 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12475 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12476 | `	const char *zName,  /* Name of the target class */` |
|        - | 12477 | `	sxu32 nByte,        /* zName length */` |
|        - | 12478 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12479 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12480 | `						 */` |
|        - | 12481 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12482 | `	)` |
|        2 | 12483 |  |
|        - | 12484 | `	SyHashEntry *pEntry;` |
|        - | 12485 | `	ph7_class *pClass;` |
|    16350 | 12486 | `	SXUNUSED(iNest);` |
|        - | 12487 | `	/* Exact class lookup.` |
|        - | 12488 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12489 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    32702 | 12490 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    32702 | 12491 | `	if( pEntry == 0 ){` |
|        - | 12492 | `		/* Class not found in hash table — try autoload before giving up */` |
|       14 | 12493 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12494 | `	}` |
|    32690 | 12495 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    32690 | 12496 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    16352 | 12497 |  |
|        - | 12498 | `/*` |
|        - | 12499 | ` * Reference Table Implementation` |
|        - | 12500 | ` * Status: stable <chm@symisc.net>` |
|        - | 12501 | ` * Intro` |
|        - | 12502 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12503 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12504 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12505 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12506 | ` *  Refer to the official for more information on this powerful` |
|        - | 12507 | ` *  extension.` |
|        - | 12508 | ` */` |
|        - | 12509 | `/*` |
|        - | 12510 | ` * Allocate a new reference entry.` |
|        - | 12511 | ` */` |
|  3067210 | 12512 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12513 |  |
|        - | 12514 | `	VmRefObj *pRef;` |
|        - | 12515 | `	/* Allocate a new instance */` |
|  3067212 | 12516 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3067212 | 12517 | `	if( pRef == 0 ){` |
|      ! 0 | 12518 | `		return 0;` |
|        - | 12519 | `	}` |
|        - | 12520 | `	/* Zero the structure */` |
|  3067212 | 12521 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12522 | `	/* Initialize fields */` |
|  3067212 | 12523 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3067212 | 12524 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3067212 | 12525 | `	pRef->nIdx = nIdx;` |
|  3067212 | 12526 | `	return pRef;` |
|  1533607 | 12527 |  |
|        - | 12528 | `/*` |
|        - | 12529 | ` * Default hash function used by the reference table` |
|        - | 12530 | ` * for lookup/insertion operations.` |
|        - | 12531 | ` */` |
| 16926714 | 12532 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12533 |  |
|        - | 12534 | `	/* Calculate the hash based on the memory object index */` |
| 16926716 | 12535 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12536 |  |
|        - | 12537 | `/*` |
|        - | 12538 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12539 | ` * in the reference table.` |
|        - | 12540 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12541 | ` * otherwise.` |
|        - | 12542 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12543 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12544 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12545 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12546 | ` * Refer to the official for more information on this powerful` |
|        - | 12547 | ` * extension.` |
|        - | 12548 | ` */` |
|  9154764 | 12549 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12550 |  |
|        - | 12551 | `	VmRefObj *pRef;` |
|        - | 12552 | `	sxu32 nBucket;` |
|        - | 12553 | `	/* Point to the appropriate bucket */` |
|  9154766 | 12554 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12555 | `	/* Perform the lookup */` |
|  9154766 | 12556 | `	pRef = pVm->apRefObj[nBucket];` |
| 19977157 | 12557 | `	for(;;){` |
| 39947712 | 12558 | `		if( pRef == 0 ){` |
|  3147212 | 12559 | `			break;` |
|        - | 12560 | `		}` |
| 36800502 | 12561 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12562 | `			/* Entry found */` |
|  6007556 | 12563 | `			return pRef;` |
|        - | 12564 | `		}` |
|        - | 12565 | `		/* Point to the next entry */` |
| 30792948 | 12566 | `		pRef = pRef->pNextCollide;` |
|        2 | 12567 | `	}` |
|        - | 12568 | `	/* No such entry,return NULL */` |
|  3147212 | 12569 | `	return 0;` |
|  4577384 | 12570 |  |
|        - | 12571 | `/*` |
|        - | 12572 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12573 | ` *` |
|        - | 12574 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12575 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12576 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12577 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12578 | ` * Refer to the official for more information on this powerful` |
|        - | 12579 | ` * extension.` |
|        - | 12580 | ` */` |
|  3067210 | 12581 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12582 |  |
|        - | 12583 | `	sxu32 nBucket;` |
|  3067212 | 12584 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12585 | `		VmRefObj **apNew;` |
|        - | 12586 | `		sxu32 nNew;` |
|        - | 12587 | `		/* Allocate a larger table */` |
|     3990 | 12588 | `		nNew = pVm->nRefSize << 1;` |
|     3990 | 12589 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3990 | 12590 | `		if( apNew ){` |
|     3990 | 12591 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12592 | `			sxu32 n;` |
|        - | 12593 | `			/* Zero the structure */` |
|     3990 | 12594 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12595 | `			/* Rehash all referenced entries */` |
|  2840776 | 12596 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12597 | `				/* Remove old collision links */` |
|  2836788 | 12598 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12599 | `				/* Point to the appropriate bucket */` |
|  2836788 | 12600 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12601 | `				/* Insert the entry  */` |
|  2836788 | 12602 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2836788 | 12603 | `				if( apNew[nBucket] ){` |
|  2298896 | 12604 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12605 | `				}` |
|  2836788 | 12606 | `				apNew[nBucket] = pEntry;` |
|        - | 12607 | `				/* Point to the next entry */` |
|  2836788 | 12608 | `				pEntry = pEntry->pNext;` |
|  1418395 | 12609 | `			}` |
|        - | 12610 | `			/* Release the old table */` |
|     3990 | 12611 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12612 | `			/* Install the new one */` |
|     3990 | 12613 | `			pVm->apRefObj = apNew;` |
|     3990 | 12614 | `			pVm->nRefSize = nNew;` |
|     1994 | 12615 | `		}` |
|     1994 | 12616 | `	}` |
|        - | 12617 | `	/* Point to the appropriate bucket */` |
|  3067212 | 12618 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12619 | `	/* Insert the entry */` |
|  3067212 | 12620 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3067212 | 12621 | `	if( pVm->apRefObj[nBucket] ){` |
|  2535208 | 12622 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1267736 | 12623 | `	}` |
|  3067212 | 12624 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3067212 | 12625 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3067212 | 12626 | `	pVm->nRefUsed++;` |
|  3067212 | 12627 | `	return SXRET_OK;` |
|        2 | 12628 |  |
|        - | 12629 | `/*` |
|        - | 12630 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12631 | ` * the reference table.` |
|        - | 12632 | ` * This function is invoked when the user perform an unset` |
|        - | 12633 | ` * call [i.e: unset($var); ].` |
|        - | 12634 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12635 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12636 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12637 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12638 | ` * Refer to the official for more information on this powerful` |
|        - | 12639 | ` * extension.` |
|        - | 12640 | ` */` |
|  3033630 | 12641 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12642 |  |
|        - | 12643 | `	ph7_hashmap_node **apNode;` |
|        - | 12644 | `	SyHashEntry **apEntry;` |
|        - | 12645 | `	sxu32 n;` |
|        - | 12646 | `	/* Point to the reference table */` |
|  3033632 | 12647 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3033632 | 12648 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12649 | `	/* Unlink the entry from the reference table */` |
|  3119584 | 12650 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85954 | 12651 | `		if( apEntry[n] ){` |
|    85904 | 12652 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42951 | 12653 | `		}` |
|    42978 | 12654 | `	}` |
|  5984008 | 12655 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2950378 | 12656 | `		if( apNode[n] ){` |
|     6880 | 12657 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3439 | 12658 | `		}` |
|  1475190 | 12659 | `	}` |
|  3033632 | 12660 | `	if( pRef->pPrevCollide ){` |
|  1165678 | 12661 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   583085 | 12662 | `	}else{` |
|  1867956 | 12663 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12664 | `	}` |
|  3033632 | 12665 | `	if( pRef->pNextCollide ){` |
|  1723718 | 12666 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   862062 | 12667 | `	}` |
|  3033632 | 12668 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12669 | `	/* Release the node */` |
|  3033632 | 12670 | `	SySetRelease(&pRef->aReference);` |
|  3033632 | 12671 | `	SySetRelease(&pRef->aArrEntries);` |
|  3033632 | 12672 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3033632 | 12673 | `	pVm->nRefUsed--;` |
|  3033632 | 12674 | `	return SXRET_OK;` |
|        2 | 12675 |  |
|        - | 12676 | `/*` |
|        - | 12677 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12678 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12679 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12680 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12681 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12682 | ` * Refer to the official for more information on this powerful` |
|        - | 12683 | ` * extension.` |
|        - | 12684 | ` */` |
|  3097574 | 12685 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12686 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12687 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12688 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12689 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12690 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12691 | `	)` |
|        2 | 12692 |  |
|  3097576 | 12693 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12694 | `	VmRefObj *pRef;` |
|        - | 12695 | `	/* Check if the referenced object already exists */` |
|  3097576 | 12696 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3097576 | 12697 | `	if( pRef == 0 ){` |
|        - | 12698 | `		/* Create a new entry */` |
|  3067212 | 12699 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3067212 | 12700 | `		if( pRef == 0 ){` |
|      ! 0 | 12701 | `			return SXERR_MEM;` |
|        - | 12702 | `		}` |
|  3067212 | 12703 | `		pRef->iFlags = iFlags;` |
|        - | 12704 | `		/* Install the entry */` |
|  3067212 | 12705 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1533605 | 12706 | `	}` |
|  3097576 | 12707 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3097576 | 12708 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12709 | `		VmSlot sRef;` |
|        - | 12710 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12711 | `		 * be deleted when we leave this frame.` |
|        - | 12712 | `		 */` |
|    80080 | 12713 | `		sRef.nIdx = nIdx;` |
|    80080 | 12714 | `		sRef.pUserData = pEntry;` |
|    80080 | 12715 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12716 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12717 | `		}` |
|    40039 | 12718 | `	}` |
|  3097576 | 12719 | `	if( pEntry ){` |
|        - | 12720 | `		/* Address of the hash-entry */` |
|   110252 | 12721 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55125 | 12722 | `	}` |
|  3097576 | 12723 | `	if( pMapEntry ){` |
|        - | 12724 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2982280 | 12725 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1491139 | 12726 | `	}` |
|  3097576 | 12727 | `	return SXRET_OK;` |
|  1548789 | 12728 |  |
|        - | 12729 | `/*` |
|        - | 12730 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12731 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12732 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12733 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12734 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12735 | ` * Refer to the official for more information on this powerful` |
|        - | 12736 | ` * extension.` |
|        - | 12737 | ` */` |
|  3023554 | 12738 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12739 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12740 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12741 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12742 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12743 | `	)` |
|        2 | 12744 |  |
|        - | 12745 | `	VmRefObj *pRef;` |
|        - | 12746 | `	sxu32 n;` |
|        - | 12747 | `	/* Check if the referenced object already exists */` |
|  3023556 | 12748 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3023556 | 12749 | `	if( pRef == 0 ){` |
|        - | 12750 | `		/* Not such entry */` |
|    79996 | 12751 | `		return SXERR_NOTFOUND;` |
|        - | 12752 | `	}` |
|        - | 12753 | `	/* Remove the desired entry */` |
|  2943562 | 12754 | `	if( pEntry ){` |
|        - | 12755 | `		SyHashEntry **apEntry;` |
|       56 | 12756 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12757 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12758 | `			if( apEntry[n] == pEntry ){` |
|        - | 12759 | `				/* Nullify the entry */` |
|       56 | 12760 | `				apEntry[n] = 0;` |
|        - | 12761 | `				/*` |
|        - | 12762 | `				 * NOTE:` |
|        - | 12763 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12764 | `				 * we avoid wasting spaces.` |
|        - | 12765 | `				 */` |
|       27 | 12766 | `			}` |
|       79 | 12767 | `		}` |
|       27 | 12768 | `	}` |
|  2943562 | 12769 | `	if( pMapEntry ){` |
|        - | 12770 | `		ph7_hashmap_node **apNode;` |
|  2943508 | 12771 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5887108 | 12772 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2943602 | 12773 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12774 | `				/* nullify the entry */` |
|  2943508 | 12775 | `				apNode[n] = 0;` |
|  1471753 | 12776 | `			}` |
|  1471802 | 12777 | `		}` |
|  1471753 | 12778 | `	}` |
|  2943562 | 12779 | `	return SXRET_OK;` |
|  1511779 | 12780 |  |
|        - | 12781 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12782 | `/*` |
|        - | 12783 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12784 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12785 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12786 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12787 | ` * For more information on how to register IO stream devices,please` |
|        - | 12788 | ` * refer to the official documentation.` |
|        - | 12789 | ` */` |
|    24598 | 12790 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12791 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12792 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12793 | `	int nByte              /* *pzDevice length*/` |
|        - | 12794 | `	)` |
|        2 | 12795 |  |
|        - | 12796 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12797 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12798 | `	SyString sDev,sCur;` |
|        - | 12799 | `	sxu32 n,nEntry;` |
|        - | 12800 | `	int rc;` |
|        - | 12801 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24600 | 12802 | `	zNext = zCur = zIn = *pzDevice;` |
|    24600 | 12803 | `	zEnd = &zIn[nByte];` |
|  1567210 | 12804 | `	while( zIn < zEnd ){` |
|  1542614 | 12805 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12806 | `			/* Got one */` |
|        3 | 12807 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12808 | `			break;` |
|        - | 12809 | `		}` |
|        - | 12810 | `		/* Advance the cursor */` |
|  1542612 | 12811 | `		zIn++;` |
|        2 | 12812 | `	}` |
|    24600 | 12813 | `	if( zIn >= zEnd ){` |
|        - | 12814 | `		/* No such scheme,return the default stream */` |
|    24598 | 12815 | `		return pVm->pDefStream;` |
|        - | 12816 | `	}` |
|        3 | 12817 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12818 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12819 | `	SyStringFullTrim(&sDev);` |
|        - | 12820 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12821 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12822 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12823 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12824 | `		pStream = apStream[n];` |
|        3 | 12825 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12826 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12827 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12828 | `		if( rc == 0 ){` |
|        - | 12829 | `			/* Stream device found */` |
|        3 | 12830 | `			*pzDevice = zNext;` |
|        3 | 12831 | `			return pStream;` |
|        - | 12832 | `		}` |
|      ! 0 | 12833 | `	}` |
|        - | 12834 | `	/* No such stream,return NULL */` |
|      ! 0 | 12835 | `	return 0;` |
|    12301 | 12836 |  |
|        - | 12837 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12838 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12839 |  |
