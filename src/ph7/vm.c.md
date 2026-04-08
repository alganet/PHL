# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5014/6559 lines (76.44%)

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
|   786772 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   786774 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   786740 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   786730 |   104 | `	return FALSE;` |
|   393410 |   105 |  |
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
|   502590 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   502592 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   502592 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   502588 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   502588 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   502588 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   502588 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   502588 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   502588 |   152 | `	pCons->xExpand = xExpand;` |
|   502588 |   153 | `	pCons->pUserData = pUserData;` |
|   502588 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   502588 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   502588 |   161 | `	return SXRET_OK;` |
|   251297 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1100408 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1100410 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1100410 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1100410 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1100410 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1100410 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1100410 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1100410 |   195 | `	pFunc->pVm   = pVm;` |
|  1100410 |   196 | `	pFunc->xFunc = xFunc;` |
|  1100410 |   197 | `	pFunc->pUserData = pUserData;` |
|  1100410 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1100410 |   200 | `	*ppOut = pFunc;` |
|  1100410 |   201 | `	return SXRET_OK;` |
|   550206 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1102724 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1102726 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1102726 |   223 | `	if( pEntry ){` |
|     2318 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2318 |   225 | `		pFunc->pUserData = pUserData;` |
|     2318 |   226 | `		pFunc->xFunc = xFunc;` |
|     2318 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2318 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1100410 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1100410 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1100410 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1100410 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1100410 |   243 | `	return SXRET_OK;` |
|   551364 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   158244 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   158246 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   158246 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   158246 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   158246 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   158246 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   158246 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   158246 |   270 | `	pFunc->iFlags = iFlags;` |
|   158246 |   271 | `	pFunc->pUserData = pUserData;` |
|   158246 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   158246 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   533586 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   533588 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    34138 |   293 | `		pName = &pFunc->sName;` |
|    17068 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   533588 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   533588 |   297 | `	if( pEntry ){` |
|   396216 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   396216 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      184 |   301 | `			pFunc->pNextName = pLink;` |
|      184 |   302 | `			pEntry->pUserData = pFunc;` |
|       91 |   303 | `		}` |
|   396216 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   137374 |   307 | `	pFunc->pNextName = 0;` |
|   137374 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   137374 |   309 | `	return rc;` |
|   266795 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    39136 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    39138 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    39138 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    39138 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    39108 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    39108 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    39108 |   334 | `	return rc;` |
|    19570 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3194928 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3194930 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3194930 |   352 | `	sInstr.iP1 = iP1;` |
|  3194930 |   353 | `	sInstr.iP2 = iP2;` |
|  3194930 |   354 | `	sInstr.p3  = p3;` |
|  3194930 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   184330 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    92164 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3194930 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3194930 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3194930 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   378912 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   378914 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   378914 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   378914 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   189456 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   189458 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   181676 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   181678 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   181678 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   957526 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   957528 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   172710 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   172712 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   618694 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   618696 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    26492 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    26494 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26494 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    26494 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26494 |   427 | `	return &aInstr[n - 2];` |
|    13248 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    15998 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16000 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16000 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16000 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16000 |   447 | `	pFrame->pUserData = pUserData;` |
|    16000 |   448 | `	pFrame->pThis = pThis;` |
|    16000 |   449 | `	pFrame->pVm = pVm;` |
|    16000 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16000 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16000 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16000 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16000 |   454 | `	return pFrame;` |
|     8001 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    15956 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    15958 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15958 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    15958 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    15958 |   476 | `	pVm->pFrame = pFrame;` |
|    15958 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13382 |   479 | `		*ppFrame = pFrame;` |
|     6690 |   480 | `	}` |
|    15958 |   481 | `	return SXRET_OK;` |
|     7980 |   482 |  |
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
|    13380 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13382 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13382 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13382 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13382 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13318 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    92538 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    79222 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39612 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13318 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    92594 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    79278 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39640 |   545 | `			}` |
|     6658 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13382 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13382 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13382 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13382 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13382 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6690 |   554 | `	}` |
|    13382 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6369618 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6369896 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6369620 |   566 | `	return pFrame;` |
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
|   111230 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   111232 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   437435 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   697 | `		/* Extract the current attribute */` |
|   270590 |   698 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   270590 |   699 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   111232 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    52170 |   724 | `		return SXRET_OK;` |
|        - |   725 | `	}` |
|        - |   726 | `	/* Create constructor alias if not yet done */` |
|    59064 |   727 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   728 | `		/* User constructor with the same base class name */` |
|     5208 |   729 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5208 |   730 | `		if( pEntry ){` |
|      ! 0 |   731 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   732 | `			/* Create the alias */` |
|      ! 0 |   733 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   734 | `		}` |
|     2603 |   735 | `	}` |
|        - |   736 | `	/* Install the methods now */` |
|    59064 |   737 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   588053 |   738 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   499460 |   739 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   499460 |   740 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   499452 |   741 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   499452 |   742 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   743 | `				return rc;` |
|        - |   744 | `			}` |
|   249725 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    59064 |   748 | `	pClass->bMounted = TRUE;` |
|    59064 |   749 | `	return SXRET_OK;` |
|    55617 |   750 |  |
|        - |   751 | `/*` |
|        - |   752 | ` * Allocate a private frame for attributes of the given` |
|        - |   753 | ` * class instance (Object in the PHP jargon).` |
|        - |   754 | ` */` |
|     1204 |   755 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   756 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   757 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   758 | `	)` |
|        2 |   759 |  |
|     1206 |   760 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   761 | `	ph7_class_attr *pAttr;` |
|        - |   762 | `	SyHashEntry *pEntry;` |
|        - |   763 | `	sxi32 rc;` |
|        - |   764 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1206 |   765 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4918 |   766 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   767 | `		VmClassAttr *pVmAttr;` |
|        - |   768 | `		/* Extract the current attribute */` |
|     3714 |   769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3714 |   770 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3714 |   771 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   772 | `			return SXERR_MEM;` |
|        - |   773 | `		}` |
|     3714 |   774 | `		pVmAttr->pAttr = pAttr;` |
|     3714 |   775 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   776 | `			ph7_value *pMemObj;` |
|        - |   777 | `			/* Reserve a memory object for this attribute */` |
|     3708 |   778 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3708 |   779 | `			if( pMemObj == 0 ){` |
|      ! 0 |   780 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   781 | `				return SXERR_MEM;` |
|        - |   782 | `			}` |
|     3708 |   783 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3708 |   784 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   785 | `				/* Initialize attribute default value (any complex expression) */` |
|     1196 |   786 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      597 |   787 | `			}` |
|     3708 |   788 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3708 |   789 | `			if( rc != SXRET_OK ){` |
|        - |   790 | `				VmSlot sSlot;` |
|        - |   791 | `				/* Restore memory object */` |
|      ! 0 |   792 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   793 | `				sSlot.pUserData = 0;` |
|      ! 0 |   794 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `			/* Install attribute in the reference table */` |
|     3708 |   799 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1855 |   800 | `		}else{` |
|        - |   801 | `			/* Install static/constant attribute */` |
|        8 |   802 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   803 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   804 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|        - |   808 | `		}` |
|        2 |   809 | `	}` |
|     1206 |   810 | `	return SXRET_OK;` |
|      604 |   811 |  |
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
|   366462 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   366464 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   358736 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   179367 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   366464 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   366464 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   366464 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   366464 |   840 | `	return pObj;` |
|   183233 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2141990 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2141992 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2141992 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070995 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2141992 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2141992 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2141992 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2141992 |   863 | `	return pObj;` |
|  1070997 |   864 |  |
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
|        - |   950 | `	"class ErrorException extends Exception { "\` |
|        - |   951 | `	"protected $severity;"\` |
|        - |   952 | `	"public function __construct(string $message = null,"\` |
|        - |   953 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   954 | `	"   if( isset($message) ){"\` |
|        - |   955 | `	"	  $this->message = $message;"\` |
|        - |   956 | `	"   }"\` |
|        - |   957 | `	"   $this->severity = $severity;"\` |
|        - |   958 | `	"   $this->code = $code;"\` |
|        - |   959 | `	"   $this->file = $filename;"\` |
|        - |   960 | `	"   $this->line = $lineno;"\` |
|        - |   961 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   962 | `	"   if( isset($previous) ){"\` |
|        - |   963 | `	"     $this->previous = $previous;"\` |
|        - |   964 | `	"   }"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"public function getSeverity(){"\` |
|        - |   967 | `	"   return $this->severity;"\` |
|        - |   968 | `    "}"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"interface Iterator {"\` |
|        - |   971 | `	"public function current();"\` |
|        - |   972 | `	"public function key();"\` |
|        - |   973 | `	"public function next();"\` |
|        - |   974 | `	"public function rewind();"\` |
|        - |   975 | `	"public function valid();"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"interface IteratorAggregate {"\` |
|        - |   978 | `	"public function getIterator();"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"interface Serializable {"\` |
|        - |   981 | `	"public function serialize();"\` |
|        - |   982 | `	"public function unserialize(string $serialized);"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"/* Directory releated IO */"\` |
|        - |   985 | `	"class Directory {"\` |
|        - |   986 | `	"public $handle = null;"\` |
|        - |   987 | `	"public $path  = null;"\` |
|        - |   988 | `	"public function __construct(string $path)"\` |
|        - |   989 | `	"{"\` |
|        - |   990 | `	"   $this->handle = opendir($path);"\` |
|        - |   991 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   992 | `	"      $this->path = $path;"\` |
|        - |   993 | `	"   }"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"public function __destruct()"\` |
|        - |   996 | `	"{"\` |
|        - |   997 | `	"  if( $this->handle != null ){"\` |
|        - |   998 | `	"       closedir($this->handle);"\` |
|        - |   999 | `	"  }"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"public function read()"\` |
|        - |  1002 | `	"{"\` |
|        - |  1003 | `	"    return readdir($this->handle);"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"public function rewind()"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"    rewinddir($this->handle);"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"public function close()"\` |
|        - |  1010 | `	"{"\` |
|        - |  1011 | `	"    closedir($this->handle);"\` |
|        - |  1012 | `	"    $this->handle = null;"\` |
|        - |  1013 | `	"}"\` |
|        - |  1014 | `	"}"\` |
|        - |  1015 | `	"class Fiber {"\` |
|        - |  1016 | `	"  private $__ctx;"\` |
|        - |  1017 | `	"  private $__callable;"\` |
|        - |  1018 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1019 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1020 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1021 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1022 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1023 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1024 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1025 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1026 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1027 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1028 | `	"}"\` |
|        - |  1029 | `	"class Generator implements Iterator {"\` |
|        - |  1030 | `	"  private $__ctx;"\` |
|        - |  1031 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1032 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1033 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1034 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1035 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1036 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1037 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1038 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1039 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"class stdClass{"\` |
|        - |  1042 | `	"  public $value;"\` |
|        - |  1043 | `	" /* Magic methods */"\` |
|        - |  1044 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1045 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1046 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1047 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1048 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"function dir(string $path){"\` |
|        - |  1051 | `	"   return new Directory($path);"\` |
|        - |  1052 | `	"}"\` |
|        - |  1053 | `	"function Dir(string $path){"\` |
|        - |  1054 | `	"   return new Directory($path);"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1057 | `    "{"\` |
|        - |  1058 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1059 | `	"  $aDir = array();"\` |
|        - |  1060 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1061 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1062 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1063 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1064 | `	"   }"\` |
|        - |  1065 | `	"  closedir($pHandle);"\` |
|        - |  1066 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1067 | `	"      rsort($aDir);"\` |
|        - |  1068 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1069 | `	"      sort($aDir);"\` |
|        - |  1070 | `	"  }"\` |
|        - |  1071 | `	"  return $aDir;"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1074 | `	"/* Open the target directory */"\` |
|        - |  1075 | `	"$zDir = dirname($pattern);"\` |
|        - |  1076 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1077 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1078 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1079 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1080 | `	"	return FALSE;"\` |
|        - |  1081 | `	"}"\` |
|        - |  1082 | `	"$pattern = basename($pattern);"\` |
|        - |  1083 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1084 | `	"/* Loop throw available entries */"\` |
|        - |  1085 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1086 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1087 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1088 | `	"	if( $rc ){"\` |
|        - |  1089 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1090 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1091 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1092 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1093 | `	"		  }"\` |
|        - |  1094 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1095 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1096 | `	"		 continue;"\` |
|        - |  1097 | `	"	   }"\` |
|        - |  1098 | `	"	   /* Add the entry */"\` |
|        - |  1099 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1100 | `	"	}"\` |
|        - |  1101 | `	" }"\` |
|        - |  1102 | `	"/* Close the handle */"\` |
|        - |  1103 | `	"closedir($pHandle);"\` |
|        - |  1104 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1105 | `	"  /* Sort the array */"\` |
|        - |  1106 | `	"  sort($pArray);"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1109 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1110 | `	"  $pArray[] = $pattern;"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"/* Return the created array */"\` |
|        - |  1113 | `	"return $pArray;"\` |
|        - |  1114 | `   "}"\` |
|        - |  1115 | `   "/* Creates a temporary file */"\` |
|        - |  1116 | `   "function tmpfile(){"\` |
|        - |  1117 | `   "  /* Extract the temp directory */"\` |
|        - |  1118 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1119 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1120 | `   "    /* Use the current dir */"\` |
|        - |  1121 | `   "    $zTempDir = '.';"\` |
|        - |  1122 | `   "  }"\` |
|        - |  1123 | `   "  /* Create the file */"\` |
|        - |  1124 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1125 | `   "  return $pHandle;"\` |
|        - |  1126 | `   "}"\` |
|        - |  1127 | `   "/* Creates a temporary filename */"\` |
|        - |  1128 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1129 | `   "{"\` |
|        - |  1130 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1131 | `   "}"\` |
|        - |  1132 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1133 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1134 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1135 | `   "/* Copy arguments */"\` |
|        - |  1136 | `   "$nArgs = func_num_args();"\` |
|        - |  1137 | `   "$pNew = array();"\` |
|        - |  1138 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1139 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1140 | `    "}"\` |
|        - |  1141 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1142 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1143 | `	"/* Erase */"\` |
|        - |  1144 | `	"array_erase($pArray);"\` |
|        - |  1145 | `	"/* Unshift */"\` |
|        - |  1146 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1147 | `	"return sizeof($pArray);"\` |
|        - |  1148 | `    "}"\` |
|        - |  1149 | `	"function array_merge_recursive(){"\` |
|        - |  1150 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1151 | `    "$arrays = func_get_args();"\` |
|        - |  1152 | `    "$narrays = count($arrays);"\` |
|        - |  1153 | `    "$ret = array();"\` |
|        - |  1154 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1155 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1156 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1157 | `	 " }"\` |
|        - |  1158 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1159 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1160 | `     "  if( $keyIsInt ) {"\` |
|        - |  1161 | `     "   $ret[] = $value;"\` |
|        - |  1162 | `     "  } else {"\` |
|        - |  1163 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1164 | `     "    $cur = $ret[$key];"\` |
|        - |  1165 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1166 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1167 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1168 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1169 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1171 | `     "    } else {"\` |
|        - |  1172 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1173 | `     "    }"\` |
|        - |  1174 | `     "   } else {"\` |
|        - |  1175 | `     "    $ret[$key] = $value;"\` |
|        - |  1176 | `     "   }"\` |
|        - |  1177 | `     "  }"\` |
|        - |  1178 | `     " }"\` |
|        - |  1179 | `	 " }"\` |
|        - |  1180 | `	 " return $ret;"\` |
|        - |  1181 | `    "}"\` |
|        - |  1182 | `	"function max(){"\` |
|        - |  1183 | `    "  $pArgs = func_get_args();"\` |
|        - |  1184 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1185 | `	"  return null;"\` |
|        - |  1186 | `    " }"\` |
|        - |  1187 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1188 | `    " $pArg = $pArgs[0];"\` |
|        - |  1189 | `	" if( !is_array($pArg) ){"\` |
|        - |  1190 | `	"   return $pArg; "\` |
|        - |  1191 | `	" }"\` |
|        - |  1192 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1193 | `	"   return null;"\` |
|        - |  1194 | `	" }"\` |
|        - |  1195 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1196 | `	" reset($pArg);"\` |
|        - |  1197 | `	" $max = current($pArg);"\` |
|        - |  1198 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1199 | `	"   if( $val > $max ){"\` |
|        - |  1200 | `	"     $max = $val;"\` |
|        - |  1201 | `    " }"\` |
|        - |  1202 | `	" }"\` |
|        - |  1203 | `	" return $max;"\` |
|        - |  1204 | `    " }"\` |
|        - |  1205 | `    " $max = $pArgs[0];"\` |
|        - |  1206 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1207 | `    " $val = $pArgs[$i];"\` |
|        - |  1208 | `	"if( $val > $max ){"\` |
|        - |  1209 | `	" $max = $val;"\` |
|        - |  1210 | `	"}"\` |
|        - |  1211 | `    " }"\` |
|        - |  1212 | `	" return $max;"\` |
|        - |  1213 | `    "}"\` |
|        - |  1214 | `	"function min(){"\` |
|        - |  1215 | `    "  $pArgs = func_get_args();"\` |
|        - |  1216 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1217 | `	"  return null;"\` |
|        - |  1218 | `    " }"\` |
|        - |  1219 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1220 | `    " $pArg = $pArgs[0];"\` |
|        - |  1221 | `	" if( !is_array($pArg) ){"\` |
|        - |  1222 | `	"   return $pArg; "\` |
|        - |  1223 | `	" }"\` |
|        - |  1224 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1225 | `	"   return null;"\` |
|        - |  1226 | `	" }"\` |
|        - |  1227 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1228 | `	" reset($pArg);"\` |
|        - |  1229 | `	" $min = current($pArg);"\` |
|        - |  1230 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1231 | `	"   if( $val < $min ){"\` |
|        - |  1232 | `	"     $min = $val;"\` |
|        - |  1233 | `    " }"\` |
|        - |  1234 | `	" }"\` |
|        - |  1235 | `	" return $min;"\` |
|        - |  1236 | `    " }"\` |
|        - |  1237 | `    " $min = $pArgs[0];"\` |
|        - |  1238 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1239 | `    " $val = $pArgs[$i];"\` |
|        - |  1240 | `	"if( $val < $min ){"\` |
|        - |  1241 | `	" $min = $val;"\` |
|        - |  1242 | `	" }"\` |
|        - |  1243 | `    " }"\` |
|        - |  1244 | `	" return $min;"\` |
|        - |  1245 | `	"}"\` |
|        - |  1246 | `	"function fileowner(string $file){"\` |
|        - |  1247 | `    " $a = stat($file);"\` |
|        - |  1248 | `	" if( !is_array($a) ){"\` |
|        - |  1249 | `	"	return false;"\` |
|        - |  1250 | `	" }"\` |
|        - |  1251 | `	" return $a['uid'];"\` |
|        - |  1252 | `    "}"\` |
|        - |  1253 | `    "function filegroup(string $file){"\` |
|        - |  1254 | `	" $a = stat($file);"\` |
|        - |  1255 | `	" if( !is_array($a) ){"\` |
|        - |  1256 | `	"	return false;"\` |
|        - |  1257 | `	" }"\` |
|        - |  1258 | `	" return $a['gid'];"\` |
|        - |  1259 | `    "}"\` |
|        - |  1260 | `	 "function fileinode(string $file){"\` |
|        - |  1261 | `	" $a = stat($file);"\` |
|        - |  1262 | `	" if( !is_array($a) ){"\` |
|        - |  1263 | `	"	return false;"\` |
|        - |  1264 | `	" }"\` |
|        - |  1265 | `	" return $a['ino'];"\` |
|        - |  1266 | `    "}"` |
|        - |  1267 |  |
|        - |  1268 | `/*` |
|        - |  1269 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1270 | ` * start compiling the target PHP program.` |
|        - |  1271 | ` */` |
|     2576 |  1272 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1273 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1274 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1275 | `	 )` |
|        2 |  1276 |  |
|        - |  1277 | `	SyString sBuiltin;` |
|        - |  1278 | `	ph7_value *pObj;` |
|        - |  1279 | `	sxi32 rc;` |
|        - |  1280 | `	/* Zero the structure */` |
|     2578 |  1281 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1282 | `	/* Initialize VM fields */` |
|     2578 |  1283 | `	pVm->pEngine = &(*pEngine);` |
|     2578 |  1284 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1285 | `	/* Instructions containers */` |
|     2578 |  1286 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2578 |  1287 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2578 |  1288 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1289 | `	/* Object containers */` |
|     2578 |  1290 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2578 |  1291 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1292 | `	/* Virtual machine internal containers */` |
|     2578 |  1293 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2578 |  1294 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2578 |  1295 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2578 |  1296 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2578 |  1297 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2578 |  1298 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2578 |  1299 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2578 |  1300 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2578 |  1301 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2578 |  1302 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2578 |  1303 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2578 |  1304 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2578 |  1305 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2578 |  1306 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2578 |  1307 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2578 |  1308 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2578 |  1309 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2578 |  1310 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2578 |  1311 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2578 |  1312 | `	pVm->pPendingException = 0;` |
|        - |  1313 | `	/* Configuration containers */` |
|     2578 |  1314 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1315 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1316 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2578 |  1317 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2578 |  1318 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2578 |  1319 | `	pVm->iResponseStatus = 200;` |
|     2578 |  1320 | `	pVm->bHeadersSent = 0;` |
|     2578 |  1321 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1322 | `	/* Error callbacks containers */` |
|     2578 |  1323 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2578 |  1324 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2578 |  1325 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2578 |  1326 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2578 |  1327 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1328 | `	/* Set a default recursion limit */` |
|        - |  1329 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2578 |  1330 | `	pVm->nMaxDepth = 32;` |
|        - |  1331 | `#else` |
|        - |  1332 | `	pVm->nMaxDepth = 16;` |
|        - |  1333 | `#endif` |
|        - |  1334 | `	/* Default assertion flags */` |
|     2578 |  1335 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1336 | `	/* JSON return status */` |
|     2578 |  1337 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1338 | `	/* PRNG context */` |
|     2578 |  1339 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1340 | `	/* Install the null constant */` |
|     2578 |  1341 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1342 | `	if( pObj == 0 ){` |
|      ! 0 |  1343 | `		rc = SXERR_MEM;` |
|      ! 0 |  1344 | `		goto Err;` |
|        - |  1345 | `	}` |
|     2578 |  1346 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1347 | `	/* Install the boolean TRUE constant */` |
|     2578 |  1348 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1349 | `	if( pObj == 0 ){` |
|      ! 0 |  1350 | `		rc = SXERR_MEM;` |
|      ! 0 |  1351 | `		goto Err;` |
|        - |  1352 | `	}` |
|     2578 |  1353 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1354 | `	/* Install the boolean FALSE constant */` |
|     2578 |  1355 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2578 |  1356 | `	if( pObj == 0 ){` |
|      ! 0 |  1357 | `		rc = SXERR_MEM;` |
|      ! 0 |  1358 | `		goto Err;` |
|        - |  1359 | `	}` |
|     2578 |  1360 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1361 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1362 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1363 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2578 |  1364 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2578 |  1365 | `	if( pObj == 0 ){` |
|      ! 0 |  1366 | `		rc = SXERR_MEM;` |
|      ! 0 |  1367 | `		goto Err;` |
|        - |  1368 | `	}` |
|     2578 |  1369 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1370 | `	/* Create the global frame */` |
|     2578 |  1371 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2578 |  1372 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1373 | `		goto Err;` |
|        - |  1374 | `	}` |
|        - |  1375 | `	/* Initialize the code generator */` |
|     2578 |  1376 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2578 |  1377 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1378 | `		goto Err;` |
|        - |  1379 | `	}` |
|        - |  1380 | `	/* VM correctly initialized,set the magic number */` |
|     2578 |  1381 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2578 |  1382 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1383 | `	/* Compile the built-in library */` |
|     2578 |  1384 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1385 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2578 |  1386 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1387 | `	/* Register Fiber internal C functions */` |
|     2578 |  1388 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2578 |  1389 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2578 |  1390 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2578 |  1391 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2578 |  1392 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2578 |  1393 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2578 |  1394 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2578 |  1395 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2578 |  1396 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2578 |  1397 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1398 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2578 |  1399 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2578 |  1400 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2578 |  1401 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2578 |  1402 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2578 |  1403 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2578 |  1404 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2578 |  1405 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2578 |  1406 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2578 |  1407 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2578 |  1408 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1409 | `	/* Reset the code generator */` |
|     2578 |  1410 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2578 |  1411 | `	return SXRET_OK;` |
|      ! 0 |  1412 | `Err:` |
|      ! 0 |  1413 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1414 | `	return rc;` |
|     1290 |  1415 |  |
|        - |  1416 | `/*` |
|        - |  1417 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1418 | ` * routine which store the output in an internal blob.` |
|        - |  1419 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1420 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1421 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1422 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1423 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1424 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1425 | ` * to finish executing and extracting the output.` |
|        - |  1426 | ` */` |
|       38 |  1427 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1428 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1429 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1430 | `	void *pUserData     /* User private data */` |
|        - |  1431 | `	)` |
|      ! 0 |  1432 |  |
|        - |  1433 | `	 sxi32 rc;` |
|        - |  1434 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1435 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1436 | `	 return rc;` |
|      ! 0 |  1437 |  |
|        - |  1438 | `/*` |
|        - |  1439 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1440 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1441 | ` */` |
|    14046 |  1442 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1443 |  |
|    14048 |  1444 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14048 |  1445 | `	if( xCons != VmObConsumer ){` |
|     6240 |  1446 | `		pVm->nOutputLen += nLen;` |
|     6240 |  1447 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      820 |  1448 | `			pVm->bHeadersSent = 1;` |
|      409 |  1449 | `		}` |
|     3119 |  1450 | `	}` |
|    14048 |  1451 |  |
|        - |  1452 | `#define VM_STACK_GUARD 16` |
|        - |  1453 | `/*` |
|        - |  1454 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1455 | ` * our compiled PHP program.` |
|        - |  1456 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1457 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1458 | ` */` |
|    32818 |  1459 | `static ph7_value * VmNewOperandStack(` |
|        - |  1460 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1461 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1462 | `	)` |
|        2 |  1463 |  |
|        - |  1464 | `	ph7_value *pStack;` |
|        - |  1465 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1466 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1467 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1468 | `  ** on the maximum stack depth required.` |
|        - |  1469 | `  **` |
|        - |  1470 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1471 | `  */` |
|    32820 |  1472 | `	nInstr += VM_STACK_GUARD;` |
|    32820 |  1473 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32820 |  1474 | `	if( pStack == 0 ){` |
|      ! 0 |  1475 | `		return 0;` |
|        - |  1476 | `	}` |
|        - |  1477 | `	/* Initialize the operand stack */` |
|  2054368 |  1478 | `	while( nInstr > 0 ){` |
|  2021550 |  1479 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2021550 |  1480 | `		--nInstr;` |
|        2 |  1481 | `	}` |
|        - |  1482 | `	/* Ready for bytecode execution */` |
|    32820 |  1483 | `	return pStack;` |
|    16411 |  1484 |  |
|        - |  1485 | `/* Forward declaration */` |
|        - |  1486 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1487 | `/*` |
|        - |  1488 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1489 | ` * This routine gets called by the PH7 engine after` |
|        - |  1490 | ` * successful compilation of the target PHP program.` |
|        - |  1491 | ` */` |
|     2316 |  1492 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1493 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1494 | `	)` |
|        2 |  1495 |  |
|        - |  1496 | `	SyHashEntry *pEntry;` |
|        - |  1497 | `	sxi32 rc;` |
|     2318 |  1498 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1499 | `		/* Initialize your VM first */` |
|      ! 0 |  1500 | `		return SXERR_CORRUPT;` |
|        - |  1501 | `	}` |
|        - |  1502 | `	/* Mark the VM ready for byte-code execution */` |
|     2318 |  1503 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1504 | `	/* Release the code generator now we have compiled our program */` |
|     2318 |  1505 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1506 | `	/* Emit the DONE instruction */` |
|     2318 |  1507 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2318 |  1508 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1509 | `		return SXERR_MEM;` |
|        - |  1510 | `	}` |
|        - |  1511 | `	/* Script return value */` |
|     2318 |  1512 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1513 | `	/* Allocate a new operand stack */` |
|     2318 |  1514 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2318 |  1515 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1516 | `		return SXERR_MEM;` |
|        - |  1517 | `	}` |
|        - |  1518 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1519 | `	 * private data. */` |
|     2318 |  1520 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2318 |  1521 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1522 | `	/* Allocate the reference table */` |
|     2318 |  1523 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2318 |  1524 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2318 |  1525 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1526 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1527 | `		return SXERR_MEM;` |
|        - |  1528 | `	}` |
|        - |  1529 | `	/* Zero the reference table */` |
|     2318 |  1530 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1531 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2318 |  1532 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2318 |  1533 | `	if( rc != SXRET_OK ){` |
|        - |  1534 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1535 | `		return rc;` |
|        - |  1536 | `	}` |
|        - |  1537 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2318 |  1538 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2318 |  1539 | `	if( rc != SXRET_OK ){` |
|        - |  1540 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1541 | `		return rc;` |
|        - |  1542 | `	}` |
|        - |  1543 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2318 |  1544 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1545 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2318 |  1546 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1547 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2318 |  1548 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1549 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1550 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2318 |  1551 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2318 |  1552 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1553 | `#endif` |
|        - |  1554 | `	/* Initialize and install static and constants class attributes */` |
|     2318 |  1555 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    37234 |  1556 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    34918 |  1557 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    34918 |  1558 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1559 | `			return rc;` |
|        - |  1560 | `		}` |
|        2 |  1561 | `	}` |
|        - |  1562 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2318 |  1563 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1564 | `	/* VM is ready for bytecode execution */` |
|     2318 |  1565 | `	return SXRET_OK;` |
|     1160 |  1566 |  |
|        - |  1567 | `/*` |
|        - |  1568 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1569 | ` */` |
|      ! 0 |  1570 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1571 |  |
|      ! 0 |  1572 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1573 | `		return SXERR_CORRUPT;` |
|        - |  1574 | `	}` |
|        - |  1575 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1576 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1577 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1578 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1579 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1580 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1581 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1582 | `	pVm->bHttpContext = 0;` |
|        - |  1583 | `	/* Set the ready flag */` |
|      ! 0 |  1584 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1585 | `	return SXRET_OK;` |
|      ! 0 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Release a Virtual Machine.` |
|        - |  1589 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1590 | ` */` |
|     2308 |  1591 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1592 |  |
|        - |  1593 | `	/* Set the stale magic number */` |
|     2310 |  1594 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1595 | `	/* Release the private memory subsystem */` |
|     2310 |  1596 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2310 |  1597 | `	return SXRET_OK;` |
|        2 |  1598 |  |
|        - |  1599 | `/*` |
|        - |  1600 | ` * Initialize a foreign function call context.` |
|        - |  1601 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1602 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1603 | ` * functions.` |
|        - |  1604 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1605 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1606 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1607 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1608 | ` */` |
|   578102 |  1609 | `static sxi32 VmInitCallContext(` |
|        - |  1610 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1611 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1612 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1613 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1614 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1615 | `	)` |
|        2 |  1616 |  |
|   578104 |  1617 | `	pOut->pFunc = pFunc;` |
|   578104 |  1618 | `	pOut->pVm   = pVm;` |
|   578104 |  1619 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   578104 |  1620 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1621 | `	/* Assume a null return value */` |
|   578104 |  1622 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   578104 |  1623 | `	pOut->pRet = pRet;` |
|   578104 |  1624 | `	pOut->iFlags = iFlags;` |
|   578104 |  1625 | `	return SXRET_OK;` |
|        2 |  1626 |  |
|        - |  1627 | `/*` |
|        - |  1628 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1629 | ` * left behind.` |
|        - |  1630 | ` */` |
|   578102 |  1631 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1632 |  |
|        - |  1633 | `	sxu32 n;` |
|   578104 |  1634 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7036 |  1635 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20074 |  1636 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13040 |  1637 | `			if( apObj[n] == 0 ){` |
|        - |  1638 | `				/* Already released */` |
|      298 |  1639 | `				continue;` |
|        - |  1640 | `			}` |
|    12744 |  1641 | `			PH7_MemObjRelease(apObj[n]);` |
|    12744 |  1642 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6373 |  1643 | `		}` |
|     7036 |  1644 | `		SySetRelease(&pCtx->sVar);` |
|     3517 |  1645 | `	}` |
|   578104 |  1646 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1647 | `		ph7_aux_data *aAux;` |
|        - |  1648 | `		void *pChunk;` |
|        - |  1649 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1650 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1651 | `		 */` |
|        9 |  1652 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1653 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1654 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1655 | `			/* Release the chunk */` |
|       25 |  1656 | `			if( pChunk ){` |
|       25 |  1657 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1658 | `			}` |
|       13 |  1659 | `		}` |
|        9 |  1660 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1661 | `	}` |
|   578104 |  1662 |  |
|        - |  1663 | `/*` |
|        - |  1664 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1665 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1666 | ` */` |
|      296 |  1667 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1668 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1669 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1670 | `	)` |
|        2 |  1671 |  |
|      298 |  1672 | `	if( pValue == 0 ){` |
|        - |  1673 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1674 | `		return;` |
|        - |  1675 | `	}` |
|      298 |  1676 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1677 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1678 | `		sxu32 n;` |
|     1054 |  1679 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1680 | `			if( apObj[n] == pValue ){` |
|      298 |  1681 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1682 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1683 | `				/* Mark as released */` |
|      298 |  1684 | `				apObj[n] = 0;` |
|      298 |  1685 | `				break;` |
|        - |  1686 | `			}` |
|      380 |  1687 | `		}` |
|      148 |  1688 | `	}` |
|      150 |  1689 |  |
|        - |  1690 | `/*` |
|        - |  1691 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1692 | ` */` |
|  3346636 |  1693 | `static void VmPopOperand(` |
|        - |  1694 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1695 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1696 | `	)` |
|        2 |  1697 |  |
|  3346638 |  1698 | `	ph7_value *pTos = *ppTos;` |
|  7114752 |  1699 | `	while( nPop > 0 ){` |
|  3768116 |  1700 | `		PH7_MemObjRelease(pTos);` |
|  3768116 |  1701 | `		pTos--;` |
|  3768116 |  1702 | `		nPop--;` |
|        2 |  1703 | `	}` |
|        - |  1704 | `	/* Top of the stack */` |
|  3346638 |  1705 | `	*ppTos = pTos;` |
|  3346638 |  1706 |  |
|        - |  1707 | `/*` |
|        - |  1708 | ` * Reserve a memory object.` |
|        - |  1709 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1710 | ` */` |
|  3056974 |  1711 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1712 |  |
|  3056976 |  1713 | `	ph7_value *pObj = 0;` |
|        - |  1714 | `	VmSlot *pSlot;` |
|        - |  1715 | `	sxu32 nIdx;` |
|        - |  1716 | `	/* Check for a free slot */` |
|  3056976 |  1717 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3056976 |  1718 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3056976 |  1719 | `	if( pSlot ){` |
|   914986 |  1720 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   914986 |  1721 | `		nIdx = pSlot->nIdx;` |
|   457492 |  1722 | `	}` |
|  3056976 |  1723 | `	if( pObj == 0 ){` |
|        - |  1724 | `		/* Reserve a new memory object */` |
|  2141992 |  1725 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2141992 |  1726 | `		if( pObj == 0 ){` |
|      ! 0 |  1727 | `			return 0;` |
|        - |  1728 | `		}` |
|  1070995 |  1729 | `	}` |
|        - |  1730 | `	/* Set a null default value */` |
|  3056976 |  1731 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3056976 |  1732 | `	pObj->nIdx = nIdx;` |
|  3056976 |  1733 | `	return pObj;` |
|  1528489 |  1734 |  |
|        - |  1735 | `/*` |
|        - |  1736 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1737 | ` */` |
|    30012 |  1738 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1739 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1740 | `	const char *zKey,  /* Entry key */` |
|        - |  1741 | `	sxu32 nByte,       /* Key length */` |
|        - |  1742 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1743 | `	)` |
|        2 |  1744 |  |
|        - |  1745 | `	ph7_value sKey;` |
|        - |  1746 | `	sxi32 rc;` |
|    30014 |  1747 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30014 |  1748 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1749 | `	/* Perform the insertion */` |
|    30014 |  1750 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30014 |  1751 | `	PH7_MemObjRelease(&sKey);` |
|    30014 |  1752 | `	return rc;` |
|        2 |  1753 |  |
|        - |  1754 | `/*` |
|        - |  1755 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1756 | ` * Return a pointer to the variable value on success.` |
|        - |  1757 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1758 | ` */` |
|  3117352 |  1759 | `static ph7_value * VmExtractMemObj(` |
|        - |  1760 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1761 | `	const SyString *pName, /* Variable name */` |
|        - |  1762 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1763 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1764 | `	)` |
|        2 |  1765 |  |
|  3117354 |  1766 | `	int bNullify = FALSE;` |
|        - |  1767 | `	SyHashEntry *pEntry;` |
|        - |  1768 | `	VmFrame *pFrame;` |
|        - |  1769 | `	ph7_value *pObj;` |
|        - |  1770 | `	sxu32 nIdx;` |
|        - |  1771 | `	sxi32 rc;` |
|        - |  1772 | `	/* Point to the top active frame */` |
|  3117354 |  1773 | `	pFrame = pVm->pFrame;` |
|  3117354 |  1774 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1775 | `	/* Perform the lookup */` |
|  3117354 |  1776 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1777 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1778 | `		pName = &sAnnon;` |
|        - |  1779 | `		/* Always nullify the object */` |
|      ! 0 |  1780 | `		bNullify = TRUE;` |
|      ! 0 |  1781 | `		bDup = FALSE;` |
|      ! 0 |  1782 | `	}` |
|        - |  1783 | `	/* Check the superglobals table first */` |
|  3117354 |  1784 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3117354 |  1785 | `	if( pEntry == 0 ){` |
|        - |  1786 | `		/* Query the top active frame */` |
|  3117314 |  1787 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3117314 |  1788 | `		if( pEntry == 0 ){` |
|    86120 |  1789 | `			char *zName = (char *)pName->zString;` |
|        - |  1790 | `			VmSlot sLocal;` |
|    86120 |  1791 | `			if( !bCreate ){` |
|        - |  1792 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1793 | `				return 0;` |
|        - |  1794 | `			}` |
|        - |  1795 | `			/* No such variable,automatically create a new one and install` |
|        - |  1796 | `			 * it in the current frame.` |
|        - |  1797 | `			 */` |
|    86084 |  1798 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86084 |  1799 | `			if( pObj == 0 ){` |
|      ! 0 |  1800 | `				return 0;` |
|        - |  1801 | `			}` |
|    86084 |  1802 | `			nIdx = pObj->nIdx;` |
|    86084 |  1803 | `			if( bDup ){` |
|        - |  1804 | `				/* Duplicate name */` |
|      168 |  1805 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1806 | `				if( zName == 0 ){` |
|      ! 0 |  1807 | `					return 0;` |
|        - |  1808 | `				}` |
|       83 |  1809 | `			}` |
|        - |  1810 | `			/* Link to the top active VM frame */` |
|    86084 |  1811 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86084 |  1812 | `			if( rc != SXRET_OK ){` |
|        - |  1813 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1814 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1815 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1816 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1817 | `				return 0;` |
|        - |  1818 | `			}` |
|    86084 |  1819 | `			if( pFrame->pParent != 0 ){` |
|        - |  1820 | `				/* Local variable */` |
|    79258 |  1821 | `				sLocal.nIdx = nIdx;` |
|    79258 |  1822 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39630 |  1823 | `			}else{` |
|        - |  1824 | `				/* Register in the $GLOBALS array */` |
|     6828 |  1825 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1826 | `			}` |
|        - |  1827 | `			/* Install in the reference table */` |
|    86084 |  1828 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1829 | `			/* Save object index */` |
|    86084 |  1830 | `			pObj->nIdx = nIdx;` |
|    43043 |  1831 | `		}else{` |
|        - |  1832 | `			/* Extract variable contents */` |
|  3031196 |  1833 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3031196 |  1834 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3031196 |  1835 | `			if( bNullify && pObj ){` |
|      ! 0 |  1836 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1837 | `			}` |
|        - |  1838 | `		}` |
|  1558750 |  1839 | `	}else{` |
|        - |  1840 | `		/* Superglobal */` |
|       42 |  1841 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1842 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1843 | `	}` |
|  3117318 |  1844 | `	return pObj;` |
|  1558788 |  1845 |  |
|        - |  1846 | `/*` |
|        - |  1847 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1848 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1849 | ` */` |
|     2620 |  1850 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1851 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1852 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1853 | `	sxu32 nByte        /* zName length */` |
|        - |  1854 | `	)` |
|        2 |  1855 |  |
|        - |  1856 | `	SyHashEntry *pEntry;` |
|        - |  1857 | `	ph7_value *pValue;` |
|        - |  1858 | `	sxu32 nIdx;` |
|        - |  1859 | `	/* Query the superglobal table */` |
|     2622 |  1860 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2622 |  1861 | `	if( pEntry == 0 ){` |
|        - |  1862 | `		/* No such entry */` |
|      ! 0 |  1863 | `		return 0;` |
|        - |  1864 | `	}` |
|        - |  1865 | `	/* Extract the superglobal index in the global object pool */` |
|     2622 |  1866 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1867 | `	/* Extract the variable value  */` |
|     2622 |  1868 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2622 |  1869 | `	return pValue;` |
|     1312 |  1870 |  |
|        - |  1871 | `/*` |
|        - |  1872 | ` * Perform a raw hashmap insertion.` |
|        - |  1873 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1874 | ` */` |
|     2650 |  1875 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1876 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1877 | `	const char *zKey,   /* Entry key */` |
|        - |  1878 | `	int nKeylen,        /* zKey length*/` |
|        - |  1879 | `	const char *zData,  /* Entry data */` |
|        - |  1880 | `	int nLen            /* zData length */` |
|        - |  1881 | `	)` |
|        2 |  1882 |  |
|        - |  1883 | `	ph7_value sKey,sValue;` |
|        - |  1884 | `	sxi32 rc;` |
|     2652 |  1885 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2652 |  1886 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2652 |  1887 | `	if( zKey ){` |
|     2630 |  1888 | `		if( nKeylen < 0 ){` |
|     2578 |  1889 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1288 |  1890 | `		}` |
|     2630 |  1891 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1314 |  1892 | `	}` |
|     2652 |  1893 | `	if( zData ){` |
|     2652 |  1894 | `		if( nLen < 0 ){` |
|        - |  1895 | `			/* Compute length automatically */` |
|      144 |  1896 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1897 | `		}` |
|     2652 |  1898 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1325 |  1899 | `	}` |
|        - |  1900 | `	/* Perform the insertion */` |
|     2652 |  1901 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2652 |  1902 | `	PH7_MemObjRelease(&sKey);` |
|     2652 |  1903 | `	PH7_MemObjRelease(&sValue);` |
|     2652 |  1904 | `	return rc;` |
|        2 |  1905 |  |
|        - |  1906 | `/*` |
|        - |  1907 | ` * Configure a working virtual machine instance.` |
|        - |  1908 | ` *` |
|        - |  1909 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1910 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1911 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1912 | ` * The second argument to this function is an integer configuration option` |
|        - |  1913 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1914 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1915 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1916 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1917 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1918 | ` */` |
|    37386 |  1919 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1920 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1921 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1922 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1923 | `	)` |
|        2 |  1924 |  |
|    37388 |  1925 | `	sxi32 rc = SXRET_OK;` |
|    37388 |  1926 | `	switch(nOp){` |
|     1150 |  1927 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2302 |  1928 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2302 |  1929 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1930 | `		/* VM output consumer callback */` |
|        - |  1931 | `#ifdef UNTRUST` |
|        - |  1932 | `		if( xConsumer == 0 ){` |
|        - |  1933 | `			rc = SXERR_CORRUPT;` |
|        - |  1934 | `			break;` |
|        - |  1935 | `		}` |
|        - |  1936 | `#endif` |
|        - |  1937 | `		/* Install the output consumer */` |
|     2302 |  1938 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2302 |  1939 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2302 |  1940 | `		break;` |
|        - |  1941 | `							   }` |
|     1158 |  1942 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1943 | `		/* Import path */` |
|        - |  1944 | `		  const char *zPath;` |
|        - |  1945 | `		  SyString sPath;` |
|     2318 |  1946 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1947 | `#if defined(UNTRUST)` |
|        - |  1948 | `		  if( zPath == 0 ){` |
|        - |  1949 | `			  rc = SXERR_EMPTY;` |
|        - |  1950 | `			  break;` |
|        - |  1951 | `		  }` |
|        - |  1952 | `#endif` |
|     2318 |  1953 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1954 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1955 | `#ifdef __WINNT__` |
|        2 |  1956 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1957 | `#endif` |
|     4634 |  1958 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1959 | `		  /* Remove leading and trailing white spaces */` |
|     2318 |  1960 | `		  SyStringFullTrim(&sPath);` |
|     2318 |  1961 | `		  if( sPath.nByte > 0 ){` |
|        - |  1962 | `			  /* Store the path in the corresponding conatiner */` |
|     2318 |  1963 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1158 |  1964 | `		  }` |
|     2318 |  1965 | `		  break;` |
|        - |  1966 | `									 }` |
|     1158 |  1967 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1968 | `		/* Run-Time Error report */` |
|     2318 |  1969 | `		pVm->bErrReport = 1;` |
|     2318 |  1970 | `		break;` |
|      ! 0 |  1971 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1972 | `		/* Recursion depth */` |
|      ! 0 |  1973 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1974 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1975 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1976 | `		}` |
|      ! 0 |  1977 | `		break;` |
|        - |  1978 | `									   }` |
|      ! 0 |  1979 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1980 | `		/* VM output length in bytes */` |
|      ! 0 |  1981 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1982 | `#ifdef UNTRUST` |
|        - |  1983 | `		if( pOut == 0 ){` |
|        - |  1984 | `			rc = SXERR_CORRUPT;` |
|        - |  1985 | `			break;` |
|        - |  1986 | `		}` |
|        - |  1987 | `#endif` |
|      ! 0 |  1988 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1989 | `		break;` |
|        - |  1990 | `							   }` |
|        - |  1991 |  |
|    11580 |  1992 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1993 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1994 | `		/* Create a new superglobal/global variable */` |
|    23162 |  1995 | `		const char *zName = va_arg(ap,const char *);` |
|    23162 |  1996 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1997 | `		SyHashEntry *pEntry;` |
|        - |  1998 | `		ph7_value *pObj;` |
|        - |  1999 | `		sxu32 nByte;` |
|        - |  2000 | `		sxu32 nIdx;` |
|        - |  2001 | `#ifdef UNTRUST` |
|        - |  2002 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2003 | `			rc = SXERR_CORRUPT;` |
|        - |  2004 | `			break;` |
|        - |  2005 | `		}` |
|        - |  2006 | `#endif` |
|    23162 |  2007 | `		nByte = SyStrlen(zName);` |
|    23162 |  2008 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2009 | `			/* Check if the superglobal is already installed */` |
|    23162 |  2010 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11582 |  2011 | `		}else{` |
|        - |  2012 | `			/* Query the top active VM frame */` |
|      ! 0 |  2013 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2014 | `		}` |
|    23162 |  2015 | `		if( pEntry ){` |
|        - |  2016 | `			/* Variable already installed */` |
|      ! 0 |  2017 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2018 | `			/* Extract contents */` |
|      ! 0 |  2019 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2020 | `			if( pObj ){` |
|        - |  2021 | `				/* Overwrite old contents */` |
|      ! 0 |  2022 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2023 | `			}` |
|      ! 0 |  2024 | `		}else{` |
|        - |  2025 | `			/* Install a new variable */` |
|    23162 |  2026 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23162 |  2027 | `			if( pObj == 0 ){` |
|      ! 0 |  2028 | `				rc = SXERR_MEM;` |
|      ! 0 |  2029 | `				break;` |
|        - |  2030 | `			}` |
|    23162 |  2031 | `			nIdx = pObj->nIdx;` |
|        - |  2032 | `			/* Copy value */` |
|    23162 |  2033 | `			PH7_MemObjStore(pValue,pObj);` |
|    23162 |  2034 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2035 | `				/* Install the superglobal */` |
|    23162 |  2036 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11582 |  2037 | `			}else{` |
|        - |  2038 | `				/* Install in the current frame */` |
|      ! 0 |  2039 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2040 | `			}` |
|    23162 |  2041 | `			if( rc == SXRET_OK ){` |
|        - |  2042 | `				SyHashEntry *pRef;` |
|    23162 |  2043 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23162 |  2044 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11582 |  2045 | `				}else{` |
|      ! 0 |  2046 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2047 | `				}` |
|        - |  2048 | `				/* Install in the reference table */` |
|    23162 |  2049 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23162 |  2050 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2051 | `					/* Register in the $GLOBALS array */` |
|    23162 |  2052 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11580 |  2053 | `				}` |
|    11580 |  2054 | `			}` |
|        - |  2055 | `		}` |
|    23162 |  2056 | `		break;` |
|        - |  2057 | `									}` |
|     1288 |  2058 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2059 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2060 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2061 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2062 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2063 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2578 |  2065 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2578 |  2066 | `		const char *zValue = va_arg(ap,const char *);` |
|     2578 |  2067 | `		int nLen = va_arg(ap,int);` |
|        - |  2068 | `		ph7_hashmap *pMap;` |
|        - |  2069 | `		ph7_value *pValue;` |
|     2578 |  2070 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2071 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2072 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2577 |  2073 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2074 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2576 |  2076 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2077 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2576 |  2079 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2080 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2081 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2576 |  2082 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2083 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2084 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2576 |  2085 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2086 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2087 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2088 | `		}else{` |
|        - |  2089 | `			/* Extract the $_SERVER superglobal */` |
|     2576 |  2090 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2091 | `		}` |
|     2578 |  2092 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2093 | `			/* No such entry */` |
|      ! 0 |  2094 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2095 | `			break;` |
|        - |  2096 | `		}` |
|        - |  2097 | `		/* Point to the hashmap */` |
|     2578 |  2098 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2099 | `		/* Perform the insertion */` |
|     2578 |  2100 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2578 |  2101 | `		break;` |
|        - |  2102 | `								   }` |
|       11 |  2103 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2104 | `		/* Script arguments */` |
|       24 |  2105 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2106 | `		ph7_hashmap *pMap;` |
|        - |  2107 | `		ph7_value *pValue;` |
|        - |  2108 | `		sxu32 n;` |
|       24 |  2109 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2110 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2111 | `			break;` |
|        - |  2112 | `		}` |
|        - |  2113 | `		/* Extract the $argv array */` |
|       24 |  2114 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2115 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2116 | `			/* No such entry */` |
|      ! 0 |  2117 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2118 | `			break;` |
|        - |  2119 | `		}` |
|        - |  2120 | `		/* Point to the hashmap */` |
|       24 |  2121 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2122 | `		/* Perform the insertion */` |
|       24 |  2123 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2124 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2125 | `		if( rc == SXRET_OK ){` |
|       24 |  2126 | `			if( pMap->nEntry > 1 ){` |
|        - |  2127 | `				/* Append space separator first */` |
|       18 |  2128 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2129 | `			}` |
|       24 |  2130 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2131 | `		}` |
|       24 |  2132 | `		break;` |
|        - |  2133 | `								  }` |
|      ! 0 |  2134 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2135 | `		/* error_log() consumer */` |
|      ! 0 |  2136 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2137 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2138 | `		break;` |
|        - |  2139 | `										}` |
|      ! 0 |  2140 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2141 | `		/* Script return value */` |
|      ! 0 |  2142 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2143 | `#ifdef UNTRUST` |
|        - |  2144 | `		if( ppValue == 0 ){` |
|        - |  2145 | `			rc = SXERR_CORRUPT;` |
|        - |  2146 | `			break;` |
|        - |  2147 | `		}` |
|        - |  2148 | `#endif` |
|      ! 0 |  2149 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2150 | `		break;` |
|        - |  2151 | `								   }` |
|     2316 |  2152 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2153 | `		/* Register an IO stream device */` |
|     4634 |  2154 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2155 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6948 |  2156 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4634 |  2157 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2158 | `				/* Invalid stream */` |
|      ! 0 |  2159 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2160 | `				break;` |
|        - |  2161 | `		}` |
|     4634 |  2162 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2163 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2318 |  2164 | `			pVm->pDefStream = pStream;` |
|     1158 |  2165 | `		}` |
|        - |  2166 | `		/* Insert in the appropriate container */` |
|     4634 |  2167 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4634 |  2168 | `		break;` |
|        - |  2169 | `								  }` |
|        8 |  2170 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2171 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2172 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2173 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2174 | `#ifdef UNTRUST` |
|        - |  2175 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2176 | `			rc = SXERR_CORRUPT;` |
|        - |  2177 | `			break;` |
|        - |  2178 | `		}` |
|        - |  2179 | `#endif` |
|       16 |  2180 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2181 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2182 | `		break;` |
|        - |  2183 | `									   }` |
|        8 |  2184 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2185 | `		/* Raw HTTP request*/` |
|       16 |  2186 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2187 | `		int nByte = va_arg(ap,int);` |
|       16 |  2188 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2189 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2190 | `			break;` |
|        - |  2191 | `		}` |
|       16 |  2192 | `		if( nByte < 0 ){` |
|        - |  2193 | `			/* Compute length automatically */` |
|      ! 0 |  2194 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2195 | `		}` |
|        - |  2196 | `		/* Process the request */` |
|       16 |  2197 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2198 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2199 | `		if( rc == SXRET_OK ){` |
|       16 |  2200 | `			pVm->bHttpContext = 1;` |
|        8 |  2201 | `		}` |
|       16 |  2202 | `		break;` |
|        - |  2203 | `									}` |
|        8 |  2204 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2205 | `		/* Extract HTTP response status code */` |
|       16 |  2206 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2207 | `		if( pStatus ){` |
|       16 |  2208 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2209 | `		}` |
|       16 |  2210 | `		break;` |
|        - |  2211 | `										}` |
|        8 |  2212 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2213 | `		/* Iterate response headers via callback */` |
|        - |  2214 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2215 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2216 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2217 | `		if( xCallback ){` |
|       16 |  2218 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2219 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2220 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2221 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2222 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2223 | `							   pUserData);` |
|       12 |  2224 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2225 | `					break;` |
|        - |  2226 | `				}` |
|        6 |  2227 | `			}` |
|        8 |  2228 | `		}` |
|       16 |  2229 | `		break;` |
|        - |  2230 | `										 }` |
|      ! 0 |  2231 | `	default:` |
|        - |  2232 | `		/* Unknown configuration option */` |
|      ! 0 |  2233 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2234 | `		break;` |
|        - |  2235 | `	}` |
|    37388 |  2236 | `	return rc;` |
|        2 |  2237 |  |
|        - |  2238 | `/* Forward declaration */` |
|        - |  2239 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2240 | `/*` |
|        - |  2241 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2242 | ` * format.` |
|        - |  2243 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2244 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2245 | ` * (STDOUT).` |
|        - |  2246 | ` */` |
|        2 |  2247 | `static sxi32 VmByteCodeDump(` |
|        - |  2248 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2249 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2250 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2251 | `	)` |
|        1 |  2252 |  |
|        - |  2253 | `	static const char zDump[] = {` |
|        - |  2254 | `		"====================================================\n"` |
|        - |  2255 | `		"PH7 VM Dump\n"` |
|        - |  2256 | `		"====================================================\n"` |
|        - |  2257 | `	};` |
|        - |  2258 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2259 | `	sxi32 rc = SXRET_OK;` |
|        - |  2260 | `	sxu32 n;` |
|        - |  2261 | `	/* Point to the PH7 instructions */` |
|        3 |  2262 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2263 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2264 | `	n = 0;` |
|        3 |  2265 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2266 | `	/* Dump instructions */` |
|        7 |  2267 | `	for(;;){` |
|       15 |  2268 | `		if( pInstr >= pEnd ){` |
|        - |  2269 | `			/* No more instructions */` |
|        3 |  2270 | `			break;` |
|        - |  2271 | `		}` |
|        - |  2272 | `		/* Format and call the consumer callback */` |
|       19 |  2273 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2274 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2275 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2276 | `		if( rc != SXRET_OK ){` |
|        - |  2277 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2278 | `			return rc;` |
|        - |  2279 | `		}` |
|       13 |  2280 | `		++n;` |
|       13 |  2281 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2282 | `	}` |
|        3 |  2283 | `	return rc;` |
|        2 |  2284 |  |
|        - |  2285 | `/* Forward declaration */` |
|        - |  2286 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2287 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2288 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2289 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2290 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2291 | `/*` |
|        - |  2292 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2293 | ` * consumer callback.` |
|        - |  2294 | ` */` |
|      544 |  2295 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2296 |  |
|      545 |  2297 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2298 | `	sxi32 rc = SXRET_OK;` |
|        - |  2299 | `	/* Append a new line */` |
|        - |  2300 | `#ifdef __WINNT__` |
|        1 |  2301 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2302 | `#else` |
|      544 |  2303 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2304 | `#endif` |
|        - |  2305 | `	/* Invoke the output consumer callback */` |
|      545 |  2306 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2307 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2308 | `	return rc;` |
|        1 |  2309 |  |
|        - |  2310 | `/*` |
|        - |  2311 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2312 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2313 | ` * information.` |
|        - |  2314 | ` */` |
|      132 |  2315 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2316 |  |
|      134 |  2317 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2318 | `		ph7_value apArg[4];` |
|        - |  2319 | `		ph7_value *apArgPtr[4];` |
|        - |  2320 | `		ph7_value sResult;` |
|        - |  2321 | `		SyString sErr;` |
|        - |  2322 | `		/* Prepare arguments */` |
|       61 |  2323 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2324 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2325 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2326 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2327 | `		if( pFile ){` |
|       61 |  2328 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2329 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2330 | `		}else{` |
|      ! 0 |  2331 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2332 | `		}` |
|       61 |  2333 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2334 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2335 | `		/* Set up pointer array */` |
|       61 |  2336 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2337 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2338 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2339 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2340 | `		/* Call the handler */` |
|       61 |  2341 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2342 | `		/* Check return value */` |
|       61 |  2343 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2344 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2345 | `		}` |
|        - |  2346 | `		/* Release */` |
|       61 |  2347 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2348 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2349 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2350 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2351 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2352 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2353 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2354 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2355 | `	}` |
|        - |  2356 | `	/* No handler, always call error handler */` |
|       73 |  2357 | `	return TRUE;` |
|       68 |  2358 |  |
|       96 |  2359 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2360 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2361 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2362 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2363 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2364 | `	)` |
|        2 |  2365 |  |
|       98 |  2366 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2367 | `	SyString *pFile;` |
|        - |  2368 | `	char *zErr;` |
|       98 |  2369 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2370 | `	if( !pVm->bErrReport ){` |
|        - |  2371 | `		/* Don't bother reporting errors */` |
|        3 |  2372 | `		return SXRET_OK;` |
|        - |  2373 | `	}` |
|        - |  2374 | `	/* Reset the working buffer */` |
|       96 |  2375 | `	SyBlobReset(pWorker);` |
|        - |  2376 | `	/* Peek the processed file if available */` |
|       96 |  2377 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2378 | `	if( pFile ){` |
|        - |  2379 | `		/* Append file name */` |
|       96 |  2380 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2381 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2382 | `	}` |
|        - |  2383 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2384 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2385 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2386 | `	 * E_DEPRECATED). */` |
|       96 |  2387 | `	zErr = "Error:  ";` |
|       96 |  2388 | `	switch(iErr){` |
|       18 |  2389 | `	case PH7_CTX_WARNING:` |
|       38 |  2390 | `		zErr = "Warning:  ";` |
|       38 |  2391 | `		break;` |
|        6 |  2392 | `	case PH7_CTX_NOTICE:` |
|       14 |  2393 | `		zErr = "Notice:  ";` |
|       12 |  2394 | `		break;` |
|       23 |  2395 | `	default:` |
|        - |  2396 | `		/* keep iErr unchanged */` |
|       46 |  2397 | `		break;` |
|        - |  2398 | `	}` |
|       96 |  2399 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2400 | `	if( pFuncName ){` |
|        - |  2401 | `		/* Append function name first */` |
|       23 |  2402 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2403 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2404 | `	}` |
|       96 |  2405 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2406 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2407 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2408 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2409 | `	}` |
|       96 |  2410 | `	return rc;` |
|       50 |  2411 |  |
|        - |  2412 | `/*` |
|        - |  2413 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2414 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2415 | ` * information.` |
|        - |  2416 | ` */` |
|       38 |  2417 | `static sxi32 VmThrowErrorAp(` |
|        - |  2418 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2419 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2420 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2421 | `	const char *zFormat, /* Format message */` |
|        - |  2422 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2423 | `	)` |
|        2 |  2424 |  |
|       40 |  2425 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2426 | `	SyBlob sMsg;` |
|        - |  2427 | `	SyString *pFile;` |
|        - |  2428 | `	char *zErr;` |
|       40 |  2429 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2430 | `	if( !pVm->bErrReport ){` |
|        - |  2431 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2432 | `		return SXRET_OK;` |
|        - |  2433 | `	}` |
|        - |  2434 | `	/* Reset the working buffer */` |
|       40 |  2435 | `	SyBlobReset(pWorker);` |
|        - |  2436 | `	/* Peek the processed file if available */` |
|       40 |  2437 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2438 | `	if( pFile ){` |
|        - |  2439 | `		/* Append file name */` |
|       40 |  2440 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2441 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2442 | `	}` |
|        - |  2443 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2444 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2445 | `	 * the correct errno value. */` |
|       40 |  2446 | `	zErr = "Error:  ";` |
|       40 |  2447 | `	switch(iErr){` |
|        4 |  2448 | `	case PH7_CTX_WARNING:` |
|        9 |  2449 | `		zErr = "Warning:  ";` |
|        9 |  2450 | `		break;` |
|        3 |  2451 | `	case PH7_CTX_NOTICE:` |
|        7 |  2452 | `		zErr = "Notice:  ";` |
|        6 |  2453 | `		break;` |
|       12 |  2454 | `	default:` |
|        - |  2455 | `		/* do not change iErr */` |
|       24 |  2456 | `		break;` |
|        - |  2457 | `	}` |
|       40 |  2458 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2459 | `	if( pFuncName ){` |
|        - |  2460 | `		/* Append function name first */` |
|       26 |  2461 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2462 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2463 | `	}` |
|        - |  2464 | `	/* Format the raw message */` |
|       40 |  2465 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2466 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2467 | `	/* Check if a user error handler is installed */` |
|       40 |  2468 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2469 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2470 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2471 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2472 | `	}` |
|       40 |  2473 | `	SyBlobRelease(&sMsg);` |
|       40 |  2474 | `	return rc;` |
|       21 |  2475 |  |
|        - |  2476 | `/*` |
|        - |  2477 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2478 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2479 | ` * information.` |
|        - |  2480 | ` * ------------------------------------` |
|        - |  2481 | ` * Simple boring wrapper function.` |
|        - |  2482 | ` * ------------------------------------` |
|        - |  2483 | ` */` |
|       14 |  2484 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2485 |  |
|        - |  2486 | `	va_list ap;` |
|        - |  2487 | `	sxi32 rc;` |
|       15 |  2488 | `	va_start(ap,zFormat);` |
|       15 |  2489 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2490 | `	va_end(ap);` |
|       15 |  2491 | `	return rc;` |
|        1 |  2492 |  |
|        - |  2493 | `/*` |
|        - |  2494 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2495 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2496 | ` * information.` |
|        - |  2497 | ` * ------------------------------------` |
|        - |  2498 | ` * Simple boring wrapper function.` |
|        - |  2499 | ` * ------------------------------------` |
|        - |  2500 | ` */` |
|       24 |  2501 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2502 |  |
|        - |  2503 | `	sxi32 rc;` |
|       26 |  2504 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2505 | `	return rc;` |
|        2 |  2506 |  |
|        - |  2507 | `/*` |
|        - |  2508 | ` * Resolve function context from the current frame.` |
|        - |  2509 | ` */` |
|      934 |  2510 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2511 |  |
|        - |  2512 | `	VmFrame *pFrame;` |
|        - |  2513 | `	ph7_vm_func *pFunc;` |
|      935 |  2514 | `	*pzFuncName = 0;` |
|      935 |  2515 | `	*pnFuncLen = 0;` |
|      935 |  2516 | `	pFrame = pVm->pFrame;` |
|      935 |  2517 | `	if( pFrame == 0 ){` |
|      ! 0 |  2518 | `		return;` |
|        - |  2519 | `	}` |
|      935 |  2520 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2521 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2522 | `		return;` |
|        - |  2523 | `	}` |
|        7 |  2524 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2525 | `	if( pFunc == 0 ){` |
|      ! 0 |  2526 | `		return;` |
|        - |  2527 | `	}` |
|        7 |  2528 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2529 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2530 |  |
|        - |  2531 | `/*` |
|        - |  2532 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2533 | ` */` |
|      470 |  2534 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2535 |  |
|        - |  2536 | `	SyBlob sOut;` |
|        - |  2537 | `	SyString *pFile;` |
|      471 |  2538 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2539 | `		return PH7_OK;` |
|        - |  2540 | `	}` |
|      471 |  2541 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2542 | `		zClass = "Exception";` |
|      ! 0 |  2543 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2544 | `	}` |
|      471 |  2545 | `	if( zMsg == 0 ){` |
|      ! 0 |  2546 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2547 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2548 | `	}` |
|      471 |  2549 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2550 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2551 | `	}` |
|      471 |  2552 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2553 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2554 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2555 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2556 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2557 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2558 | `	if( pFile ){` |
|      471 |  2559 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2560 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2561 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2562 | `	}` |
|      471 |  2563 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2564 | `	if( pFile ){` |
|      471 |  2565 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2566 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2567 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2568 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2569 | `		}else{` |
|      465 |  2570 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2571 | `		}` |
|      235 |  2572 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2573 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2574 | `	}else{` |
|      ! 0 |  2575 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2576 | `	}` |
|      471 |  2577 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2578 | `	if( pFile ){` |
|      471 |  2579 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2580 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2581 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2582 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2583 | `	}` |
|      471 |  2584 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2585 | `	SyBlobRelease(&sOut);` |
|      471 |  2586 | `	return PH7_ABORT;` |
|      236 |  2587 |  |
|        - |  2588 | `/*` |
|        - |  2589 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2590 | ` */` |
|      472 |  2591 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2592 |  |
|        - |  2593 | `	ph7_vm *pVm;` |
|        - |  2594 | `	ph7_class *pClass;` |
|        - |  2595 | `	ph7_class_instance *pThis;` |
|        - |  2596 | `	ph7_class_method *pCons;` |
|        - |  2597 | `	ph7_value sArg;` |
|        - |  2598 | `	ph7_value *apArg[1];` |
|        - |  2599 | `	SyBlob sMsg;` |
|        - |  2600 | `	SyString sMsgStr;` |
|        - |  2601 | `	VmFrame *pFrame;` |
|        - |  2602 | `	va_list ap;` |
|        - |  2603 | `	sxi32 rc;` |
|        - |  2604 |  |
|      474 |  2605 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2606 | `		return PH7_ABORT;` |
|        - |  2607 | `	}` |
|      474 |  2608 | `	pVm = pCtx->pVm;` |
|      474 |  2609 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2610 | `		zClass = "Error";` |
|      ! 0 |  2611 | `	}` |
|      474 |  2612 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      474 |  2613 | `	if( pClass == 0 ){` |
|      ! 0 |  2614 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2615 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2616 | `			zClass` |
|        - |  2617 | `			);` |
|        - |  2618 | `	}` |
|      474 |  2619 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      474 |  2620 | `	if( pThis == 0 ){` |
|      ! 0 |  2621 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2622 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2623 | `			);` |
|        - |  2624 | `	}` |
|        - |  2625 |  |
|      474 |  2626 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      474 |  2627 | `	va_start(ap,zFormat);` |
|      474 |  2628 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      474 |  2629 | `	va_end(ap);` |
|        - |  2630 |  |
|      474 |  2631 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      474 |  2632 | `	if( pCons ){` |
|      474 |  2633 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      474 |  2634 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      474 |  2635 | `		apArg[0] = &sArg;` |
|      474 |  2636 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      474 |  2637 | `		PH7_MemObjRelease(&sArg);` |
|      236 |  2638 | `	}` |
|      474 |  2639 | `	SyBlobRelease(&sMsg);` |
|        - |  2640 |  |
|      474 |  2641 | `	pFrame = pVm->pFrame;` |
|      474 |  2642 | `	if( pFrame ){` |
|      474 |  2643 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      474 |  2644 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      236 |  2645 | `	}` |
|      474 |  2646 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      474 |  2647 | `	PH7_ClassInstanceUnref(pThis);` |
|      474 |  2648 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2649 | `		return PH7_ABORT;` |
|        - |  2650 | `	}` |
|       12 |  2651 | `	return PH7_EXCEPTION;` |
|      238 |  2652 |  |
|        - |  2653 | `/*` |
|        - |  2654 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2655 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2656 | ` */` |
|      ! 0 |  2657 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2658 |  |
|        - |  2659 | `	ph7_vm *pVm;` |
|        - |  2660 | `	SyBlob sMsg;` |
|      ! 0 |  2661 | `	const char *zFuncName = 0;` |
|      ! 0 |  2662 | `	int nFuncLen = 0;` |
|        - |  2663 | `	va_list ap;` |
|        - |  2664 | `	sxi32 rc;` |
|        - |  2665 |  |
|      ! 0 |  2666 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2667 | `		return PH7_OK;` |
|        - |  2668 | `	}` |
|      ! 0 |  2669 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2670 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2671 | `		zClass = "Error";` |
|      ! 0 |  2672 | `	}` |
|        - |  2673 |  |
|      ! 0 |  2674 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2675 |  |
|      ! 0 |  2676 | `	va_start(ap,zFormat);` |
|      ! 0 |  2677 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2678 | `	va_end(ap);` |
|        - |  2679 |  |
|      ! 0 |  2680 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2681 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2682 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2683 | `	}` |
|      ! 0 |  2684 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2685 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2686 | `	}` |
|      ! 0 |  2687 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2688 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2689 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2690 | `	return rc;` |
|      ! 0 |  2691 |  |
|        - |  2692 | `/*` |
|        - |  2693 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2694 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2695 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2696 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2697 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2698 | ` * when VmByteCodeExec returns.` |
|        - |  2699 | ` */` |
|      132 |  2700 | `static sxi32 VmSuspendCtx(` |
|        - |  2701 | `	ph7_vm *pVm,` |
|        - |  2702 | `	ph7_exec_ctx *pCtx,` |
|        - |  2703 | `	sxi32 pc,` |
|        - |  2704 | `	sxi32 nTos` |
|        - |  2705 | `	)` |
|        2 |  2706 |  |
|       66 |  2707 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2708 | `	pCtx->pc = pc;` |
|      134 |  2709 | `	pCtx->nTos = nTos;` |
|      134 |  2710 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2711 | `	return PH7_SUSPEND;` |
|        2 |  2712 |  |
|        - |  2713 | `/*` |
|        - |  2714 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2715 | ` *` |
|        - |  2716 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2717 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2718 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2719 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2720 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2721 | ` * then the program execution is halted.` |
|        - |  2722 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2723 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2724 | ` * or to reset the VM to it's initial state.` |
|        - |  2725 | ` */` |
|    32904 |  2726 | `static sxi32 VmByteCodeExec(` |
|        - |  2727 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2728 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2729 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2730 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2731 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2732 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2733 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2734 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2735 | `	)` |
|        2 |  2736 |  |
|        - |  2737 | `	VmInstr *pInstr;` |
|        - |  2738 | `	ph7_value *pTos;` |
|        - |  2739 | `	SySet aArg;` |
|        - |  2740 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2741 | `	sxi32 pc;` |
|        - |  2742 | `	sxi32 rc;` |
|        - |  2743 | `	/* Argument container */` |
|    32906 |  2744 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32906 |  2745 | `	if( nTos < 0 ){` |
|    30856 |  2746 | `		pTos = &pStack[-1];` |
|    15429 |  2747 | `	}else{` |
|     2052 |  2748 | `		pTos = &pStack[nTos];` |
|        - |  2749 | `	}` |
|    32906 |  2750 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32906 |  2751 | `	pc = nPc;` |
|        - |  2752 | `	/* Execute as much as we can */` |
|  5007323 |  2753 | `	for(;;){` |
|        - |  2754 | `		/* Fetch the instruction to execute */` |
| 10013944 |  2755 | `		pInstr = &aInstr[pc];` |
| 10013944 |  2756 | `		rc = SXRET_OK;` |
|        - |  2757 | `/*` |
|        - |  2758 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2759 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2760 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2761 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2762 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2763 | ` */` |
| 10013944 |  2764 | `		switch(pInstr->iOp){` |
|        - |  2765 | `/*` |
|        - |  2766 | ` * DONE: P1 * *` |
|        - |  2767 | ` *` |
|        - |  2768 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2769 | ` * and return immediately.` |
|        - |  2770 | ` */` |
|    16141 |  2771 | `case PH7_OP_DONE:` |
|    32284 |  2772 | `	if( pInstr->iP1 ){` |
|        - |  2773 | `#ifdef UNTRUST` |
|        - |  2774 | `		if( pTos < pStack ){` |
|        - |  2775 | `			goto Abort;` |
|        - |  2776 | `		}` |
|        - |  2777 | `#endif` |
|    18730 |  2778 | `		if( pLastRef ){` |
|    12208 |  2779 | `			*pLastRef = pTos->nIdx;` |
|     6103 |  2780 | `		}` |
|    18730 |  2781 | `		if( pResult ){` |
|        - |  2782 | `			/* Execution result */` |
|    17786 |  2783 | `			PH7_MemObjStore(pTos,pResult);` |
|     8892 |  2784 | `		}` |
|    18730 |  2785 | `		VmPopOperand(&pTos,1);` |
|    22920 |  2786 | `	}else if( pLastRef ){` |
|        - |  2787 | `		/* Nothing referenced */` |
|     1028 |  2788 | `		*pLastRef = SXU32_HIGH;` |
|      513 |  2789 | `	}` |
|        - |  2790 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2791 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2792 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2793 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2794 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2795 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2796 | `	 * block can override it.` |
|        - |  2797 | `	 */` |
|    32286 |  2798 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2799 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2800 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2801 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2802 | `		pExc->pFrame = 0;` |
|        3 |  2803 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2804 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2805 | `			pExc->iFinallyDone = 1;` |
|        - |  2806 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2807 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2808 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2809 | `				goto Abort;` |
|        - |  2810 | `			}` |
|        1 |  2811 | `		}` |
|        1 |  2812 | `	}` |
|    32284 |  2813 | `	goto Done;` |
|        - |  2814 | `/*` |
|        - |  2815 | ` * HALT: P1 * *` |
|        - |  2816 | ` *` |
|        - |  2817 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2818 | ` * and abort immediately.` |
|        - |  2819 | ` */` |
|        4 |  2820 | `case PH7_OP_HALT:` |
|        9 |  2821 | `	if( pInstr->iP1 ){` |
|        - |  2822 | `#ifdef UNTRUST` |
|        - |  2823 | `		if( pTos < pStack ){` |
|        - |  2824 | `			goto Abort;` |
|        - |  2825 | `		}` |
|        - |  2826 | `#endif` |
|        9 |  2827 | `		if( pLastRef ){` |
|      ! 0 |  2828 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2829 | `		}` |
|        9 |  2830 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2831 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2832 | `				/* Output the exit message */` |
|        7 |  2833 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2834 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2835 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2836 | `			}` |
|        7 |  2837 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2838 | `			/* Record exit status */` |
|        5 |  2839 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2840 | `		}` |
|        9 |  2841 | `		VmPopOperand(&pTos,1);` |
|        4 |  2842 | `	}else if( pLastRef ){` |
|        - |  2843 | `		/* Nothing referenced */` |
|      ! 0 |  2844 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2845 | `	}` |
|        - |  2846 | `	/* Check if we're in an included file context */` |
|        9 |  2847 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2848 | `		/* Terminate the entire process */` |
|        9 |  2849 | `		exit(pVm->iExitStatus);` |
|        - |  2850 | `	}` |
|      ! 0 |  2851 | `	goto Abort;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * JMP: * P2 *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2856 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2857 | ` */` |
|   215963 |  2858 | `case PH7_OP_JMP:` |
|   431972 |  2859 | `	pc = pInstr->iP2 - 1;` |
|   431972 |  2860 | `	break;` |
|        - |  2861 | `/*` |
|        - |  2862 | ` * JZ: P1 P2 *` |
|        - |  2863 | ` *` |
|        - |  2864 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2865 | ` * entry in the stack if P1 is zero.` |
|        - |  2866 | ` */` |
|   504917 |  2867 | `case PH7_OP_JZ:` |
|        - |  2868 | `#ifdef UNTRUST` |
|        - |  2869 | `	if( pTos < pStack ){` |
|        - |  2870 | `		goto Abort;` |
|        - |  2871 | `	}` |
|        - |  2872 | `#endif` |
|        - |  2873 | `	/* Get a boolean value */` |
|  1009924 |  2874 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2875 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2876 | `	}` |
|  1009924 |  2877 | `	if( !pTos->x.iVal ){` |
|        - |  2878 | `		/* Take the jump */` |
|   510728 |  2879 | `		pc = pInstr->iP2 - 1;` |
|   255363 |  2880 | `	}` |
|  1009924 |  2881 | `	if( !pInstr->iP1 ){` |
|   802982 |  2882 | `		VmPopOperand(&pTos,1);` |
|   401512 |  2883 | `	}` |
|  1009924 |  2884 | `	break;` |
|        - |  2885 | `/*` |
|        - |  2886 | ` * JNZ: P1 P2 *` |
|        - |  2887 | ` *` |
|        - |  2888 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2889 | ` * entry in the stack if P1 is zero.` |
|        - |  2890 | ` */` |
|    53518 |  2891 | `case PH7_OP_JNZ:` |
|        - |  2892 | `#ifdef UNTRUST` |
|        - |  2893 | `	if( pTos < pStack ){` |
|        - |  2894 | `		goto Abort;` |
|        - |  2895 | `	}` |
|        - |  2896 | `#endif` |
|        - |  2897 | `	/* Get a boolean value */` |
|   107038 |  2898 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2899 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2900 | `	}` |
|   107038 |  2901 | `	if( pTos->x.iVal ){` |
|        - |  2902 | `		/* Take the jump */` |
|     4540 |  2903 | `		pc = pInstr->iP2 - 1;` |
|     2269 |  2904 | `	}` |
|   107038 |  2905 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2906 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2907 | `	}` |
|   107038 |  2908 | `	break;` |
|        - |  2909 | `/*` |
|        - |  2910 | ` * NOOP: * * *` |
|        - |  2911 | ` *` |
|        - |  2912 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2913 | ` * destination.` |
|        - |  2914 | ` */` |
|      ! 0 |  2915 | `case PH7_OP_NOOP:` |
|      ! 0 |  2916 | `	break;` |
|        - |  2917 | `/*` |
|        - |  2918 | ` * POP: P1 * *` |
|        - |  2919 | ` *` |
|        - |  2920 | ` * Pop P1 elements from the operand stack.` |
|        - |  2921 | ` */` |
|   393830 |  2922 | `case PH7_OP_POP: {` |
|   787706 |  2923 | `	sxi32 n = pInstr->iP1;` |
|   787706 |  2924 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2925 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2926 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2927 | `	}` |
|   787706 |  2928 | `	VmPopOperand(&pTos,n);` |
|   787706 |  2929 | `	break;` |
|        - |  2930 | `				 }` |
|        - |  2931 | `/*` |
|        - |  2932 | ` * DUP: * * *` |
|        - |  2933 | ` *` |
|        - |  2934 | ` * Duplicate the top of the stack.` |
|        - |  2935 | ` */` |
|       35 |  2936 | `case PH7_OP_DUP:` |
|        - |  2937 | `#ifdef UNTRUST` |
|        - |  2938 | `	if( pTos < pStack ){` |
|        - |  2939 | `		goto Abort;` |
|        - |  2940 | `	}` |
|        - |  2941 | `#endif` |
|       72 |  2942 | `	pTos++;` |
|       72 |  2943 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2944 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2945 | `	break;` |
|        - |  2946 | `/*` |
|        - |  2947 | ` * NSSWITCH: * * P3` |
|        - |  2948 | ` *` |
|        - |  2949 | ` * Switch the active namespace at runtime.` |
|        - |  2950 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2951 | ` */` |
|     6494 |  2952 | `case PH7_OP_NSSWITCH:` |
|    12990 |  2953 | `	SyBlobReset(&pVm->sNamespace);` |
|    12990 |  2954 | `	if( pInstr->p3 ){` |
|       62 |  2955 | `		const char *zNs = (const char *)pInstr->p3;` |
|       62 |  2956 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       30 |  2957 | `	}` |
|    12990 |  2958 | `	break;` |
|        - |  2959 | `/*` |
|        - |  2960 | ` * CVT_INT: * * *` |
|        - |  2961 | ` *` |
|        - |  2962 | ` * Force the top of the stack to be an integer.` |
|        - |  2963 | ` */` |
|       35 |  2964 | `case PH7_OP_CVT_INT:` |
|        - |  2965 | `#ifdef UNTRUST` |
|        - |  2966 | `	if( pTos < pStack ){` |
|        - |  2967 | `		goto Abort;` |
|        - |  2968 | `	}` |
|        - |  2969 | `#endif` |
|       72 |  2970 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2971 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2972 | `	}` |
|        - |  2973 | `	/* Invalidate any prior representation */` |
|       72 |  2974 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2975 | `	break;` |
|        - |  2976 | `/*` |
|        - |  2977 | ` * CVT_REAL: * * *` |
|        - |  2978 | ` *` |
|        - |  2979 | ` * Force the top of the stack to be a real.` |
|        - |  2980 | ` */` |
|        4 |  2981 | `case PH7_OP_CVT_REAL:` |
|        - |  2982 | `#ifdef UNTRUST` |
|        - |  2983 | `	if( pTos < pStack ){` |
|        - |  2984 | `		goto Abort;` |
|        - |  2985 | `	}` |
|        - |  2986 | `#endif` |
|        9 |  2987 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2988 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2989 | `	}` |
|        - |  2990 | `	/* Invalidate any prior representation */` |
|        9 |  2991 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2992 | `	break;` |
|        - |  2993 | `/*` |
|        - |  2994 | ` * CVT_STR: * * *` |
|        - |  2995 | ` *` |
|        - |  2996 | ` * Force the top of the stack to be a string.` |
|        - |  2997 | ` */` |
|      146 |  2998 | `case PH7_OP_CVT_STR:` |
|        - |  2999 | `#ifdef UNTRUST` |
|        - |  3000 | `	if( pTos < pStack ){` |
|        - |  3001 | `		goto Abort;` |
|        - |  3002 | `	}` |
|        - |  3003 | `#endif` |
|      294 |  3004 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3005 | `		PH7_MemObjToString(pTos);` |
|      146 |  3006 | `	}` |
|      294 |  3007 | `	break;` |
|        - |  3008 | `/*` |
|        - |  3009 | ` * CVT_BOOL: * * *` |
|        - |  3010 | ` *` |
|        - |  3011 | ` * Force the top of the stack to be a boolean.` |
|        - |  3012 | ` */` |
|        5 |  3013 | `case PH7_OP_CVT_BOOL:` |
|        - |  3014 | `#ifdef UNTRUST` |
|        - |  3015 | `	if( pTos < pStack ){` |
|        - |  3016 | `		goto Abort;` |
|        - |  3017 | `	}` |
|        - |  3018 | `#endif` |
|       11 |  3019 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3020 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3021 | `	}` |
|       11 |  3022 | `	break;` |
|        - |  3023 | `/*` |
|        - |  3024 | ` * CVT_NULL: * * *` |
|        - |  3025 | ` *` |
|        - |  3026 | ` * Nullify the top of the stack.` |
|        - |  3027 | ` */` |
|        3 |  3028 | `case PH7_OP_CVT_NULL:` |
|        - |  3029 | `#ifdef UNTRUST` |
|        - |  3030 | `	if( pTos < pStack ){` |
|        - |  3031 | `		goto Abort;` |
|        - |  3032 | `	}` |
|        - |  3033 | `#endif` |
|        7 |  3034 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3035 | `	break;` |
|        - |  3036 | `/*` |
|        - |  3037 | ` * CVT_NUMC: * * *` |
|        - |  3038 | ` *` |
|        - |  3039 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3040 | ` */` |
|      ! 0 |  3041 | `case PH7_OP_CVT_NUMC:` |
|        - |  3042 | `#ifdef UNTRUST` |
|        - |  3043 | `	if( pTos < pStack ){` |
|        - |  3044 | `		goto Abort;` |
|        - |  3045 | `	}` |
|        - |  3046 | `#endif` |
|        - |  3047 | `	/* Force a numeric cast */` |
|      ! 0 |  3048 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3049 | `	break;` |
|        - |  3050 | `/*` |
|        - |  3051 | ` * CVT_ARRAY: * * *` |
|        - |  3052 | ` *` |
|        - |  3053 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3054 | ` */` |
|       10 |  3055 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3056 | `#ifdef UNTRUST` |
|        - |  3057 | `	if( pTos < pStack ){` |
|        - |  3058 | `		goto Abort;` |
|        - |  3059 | `	}` |
|        - |  3060 | `#endif` |
|        - |  3061 | `	/* Force a hashmap cast */` |
|       21 |  3062 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3063 | `	if( rc != SXRET_OK ){` |
|        - |  3064 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3065 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3066 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3067 | `	}` |
|       21 |  3068 | `	break;` |
|        - |  3069 | `/*` |
|        - |  3070 | ` * CVT_OBJ: * * *` |
|        - |  3071 | ` *` |
|        - |  3072 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3073 | ` */` |
|        8 |  3074 | `case PH7_OP_CVT_OBJ:` |
|        - |  3075 | `#ifdef UNTRUST` |
|        - |  3076 | `	if( pTos < pStack ){` |
|        - |  3077 | `		goto Abort;` |
|        - |  3078 | `	}` |
|        - |  3079 | `#endif` |
|       17 |  3080 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3081 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3082 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3083 | `	}` |
|       17 |  3084 | `	break;` |
|        - |  3085 | `/*` |
|        - |  3086 | ` * ERR_CTRL * * *` |
|        - |  3087 | ` *` |
|        - |  3088 | ` * Error control operator.` |
|        - |  3089 | ` */` |
|    13042 |  3090 | `case PH7_OP_ERR_CTRL:` |
|        - |  3091 | `	/*` |
|        - |  3092 | `	 * TICKET 1433-038:` |
|        - |  3093 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3094 | `	 * use the public API,to control error output.` |
|        - |  3095 | `	 */` |
|    26084 |  3096 | `	break;` |
|        - |  3097 | `/*` |
|        - |  3098 | ` * IS_A * * *` |
|        - |  3099 | ` *` |
|        - |  3100 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3101 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3102 | ` * holding a class name or an object).` |
|        - |  3103 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3104 | ` */` |
|       23 |  3105 | `case PH7_OP_IS_A:{` |
|       48 |  3106 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3107 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3108 | `#ifdef UNTRUST` |
|        - |  3109 | `	if( pNos < pStack ){` |
|        - |  3110 | `		goto Abort;` |
|        - |  3111 | `	}` |
|        - |  3112 | `#endif` |
|       48 |  3113 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3114 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3115 | `		ph7_class *pClass = 0;` |
|        - |  3116 | `		/* Extract the target class */` |
|       46 |  3117 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3118 | `			/* Instance already loaded */` |
|      ! 0 |  3119 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3120 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3121 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3122 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3123 | `			/* Handle self/static/parent keywords */` |
|       46 |  3124 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3125 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3126 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3127 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3128 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3129 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3130 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3131 | `					pClass = pSelf->pBase;` |
|        2 |  3132 | `				}` |
|        3 |  3133 | `			}else{` |
|       36 |  3134 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3135 | `			}` |
|       22 |  3136 | `		}` |
|       46 |  3137 | `		if( pClass ){` |
|        - |  3138 | `			/* Perform the query */` |
|       46 |  3139 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3140 | `		}` |
|       22 |  3141 | `	}` |
|        - |  3142 | `	/* Push result */` |
|       48 |  3143 | `	VmPopOperand(&pTos,1);` |
|       48 |  3144 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3145 | `	pTos->x.iVal = iRes;` |
|       48 |  3146 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3147 | `	break;` |
|        - |  3148 | `				 }` |
|        - |  3149 |  |
|        - |  3150 | `/*` |
|        - |  3151 | ` * LOADC P1 P2 *` |
|        - |  3152 | ` *` |
|        - |  3153 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3154 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3155 | ` */` |
|   842597 |  3156 | `case PH7_OP_LOADC: {` |
|        - |  3157 | `	ph7_value *pObj;` |
|        - |  3158 | `	/* Reserve a room */` |
|  1685240 |  3159 | `	pTos++;` |
|  2519663 |  3160 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1685240 |  3161 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3162 | `			SyHashEntry *pEntry;` |
|        - |  3163 | `			/* Candidate for expansion via user defined callbacks */` |
|    16440 |  3164 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16440 |  3165 | `			if( pEntry ){` |
|    16436 |  3166 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3167 | `				/* Set a NULL default value */` |
|    16436 |  3168 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16436 |  3169 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3170 | `				/* Invoke the callback and deal with the expanded value */` |
|    16436 |  3171 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3172 | `				/* Mark as constant */` |
|    16436 |  3173 | `				pTos->nIdx = SXU32_HIGH;` |
|    16436 |  3174 | `				break;` |
|        - |  3175 | `			}` |
|        - |  3176 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3177 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3178 | `			 * through to string value for backward compatibility. */` |
|        - |  3179 | `			{` |
|        6 |  3180 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3181 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3182 | `				sxu32 j;` |
|       32 |  3183 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3184 | `					if( zLit[j] == '\\' ){` |
|        - |  3185 | `						/* Qualified name: must be a real constant.` |
|        - |  3186 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3187 | `						{` |
|        3 |  3188 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3189 | `							SyBlob sErr;` |
|        3 |  3190 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3191 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3192 | `							if( pErrFile ){` |
|        3 |  3193 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3194 | `							}` |
|        3 |  3195 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3196 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3197 | `							SyBlobRelease(&sErr);` |
|        - |  3198 | `						}` |
|        3 |  3199 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3200 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3201 | `						goto LoadC_Done;` |
|        - |  3202 | `					}` |
|       15 |  3203 | `				}` |
|        - |  3204 | `			}` |
|        1 |  3205 | `		}` |
|  1668804 |  3206 | `		PH7_MemObjLoad(pObj,pTos);` |
|   834425 |  3207 | `	}else{` |
|        - |  3208 | `		/* Set a NULL value */` |
|      ! 0 |  3209 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3210 | `	}` |
|   834380 |  3211 | `LoadC_Done:` |
|        - |  3212 | `	/* Mark as constant */` |
|  1668806 |  3213 | `	pTos->nIdx = SXU32_HIGH;` |
|  1668806 |  3214 | `	break;` |
|        - |  3215 | `				  }` |
|        - |  3216 | `/*` |
|        - |  3217 | ` * LOAD: P1 * P3` |
|        - |  3218 | ` *` |
|        - |  3219 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3220 | ` * from the P3 operand.` |
|        - |  3221 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3222 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3223 | ` */` |
|  1354088 |  3224 | `case PH7_OP_LOAD:{` |
|        - |  3225 | `	ph7_value *pObj;` |
|        - |  3226 | `	SyString sName;` |
|  2708398 |  3227 | `	if( pInstr->p3 == 0 ){` |
|        - |  3228 | `		/* Take the variable name from the top of the stack */` |
|        - |  3229 | `#ifdef UNTRUST` |
|        - |  3230 | `		if( pTos < pStack ){` |
|        - |  3231 | `			goto Abort;` |
|        - |  3232 | `		}` |
|        - |  3233 | `#endif` |
|        - |  3234 | `		/* Force a string cast */` |
|       19 |  3235 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3236 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3237 | `		}` |
|       19 |  3238 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3239 | `	}else{` |
|  2708380 |  3240 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3241 | `		/* Reserve a room for the target object */` |
|  2708380 |  3242 | `		pTos++;` |
|        - |  3243 | `	}` |
|        - |  3244 | `	/* Extract the requested memory object */` |
|  2708398 |  3245 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2708398 |  3246 | `	if( pObj == 0 ){` |
|       26 |  3247 | `		if( pInstr->iP1 ){` |
|        - |  3248 | `			/* Variable not found,load NULL */` |
|       26 |  3249 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3250 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3251 | `			}else{` |
|       26 |  3252 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3253 | `			}` |
|       26 |  3254 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1354102 |  3255 | `			break;` |
|      ! 0 |  3256 | `		}else{` |
|        - |  3257 | `			/* Fatal error */` |
|      ! 0 |  3258 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3259 | `			goto Abort;` |
|        - |  3260 | `		}` |
|        - |  3261 | `	}` |
|        - |  3262 | `	/* Load variable contents */` |
|  2708374 |  3263 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2708374 |  3264 | `	pTos->nIdx = pObj->nIdx;` |
|  2708374 |  3265 | `	break;` |
|        - |  3266 | `				   }` |
|        - |  3267 | `/*` |
|        - |  3268 | ` * LOAD_MAP P1 * *` |
|        - |  3269 | ` *` |
|        - |  3270 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3271 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3272 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3273 | ` */` |
|    18763 |  3274 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3275 | `	ph7_hashmap *pMap;` |
|        - |  3276 | `	/* Allocate a new hashmap instance */` |
|    37528 |  3277 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    37528 |  3278 | `	if( pMap == 0 ){` |
|      ! 0 |  3279 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3280 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3281 | `		goto Abort;` |
|        - |  3282 | `	}` |
|    37528 |  3283 | `	if( pInstr->iP1 > 0 ){` |
|     2276 |  3284 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3285 | `		/* Perform the insertion */` |
|     6960 |  3286 | `		while( pEntry < pTos ){` |
|     4686 |  3287 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3288 | `				/* Insertion by reference */` |
|      142 |  3289 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3290 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3291 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3292 | `					);` |
|       48 |  3293 | `			}else{` |
|        - |  3294 | `				/* Standard insertion */` |
|     6887 |  3295 | `				PH7_HashmapInsert(pMap,` |
|     4590 |  3296 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2295 |  3297 | `					&pEntry[1]` |
|        - |  3298 | `				);` |
|        - |  3299 | `			}` |
|        - |  3300 | `			/* Next pair on the stack */` |
|     4686 |  3301 | `			pEntry += 2;` |
|        2 |  3302 | `		}` |
|        - |  3303 | `		/* Pop P1 elements */` |
|     2276 |  3304 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1137 |  3305 | `	}` |
|        - |  3306 | `	/* Push the hashmap */` |
|    37528 |  3307 | `	pTos++;` |
|    37528 |  3308 | `	pTos->nIdx = SXU32_HIGH;` |
|    37528 |  3309 | `	pTos->x.pOther = pMap;` |
|    37528 |  3310 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    37528 |  3311 | `	break;` |
|        - |  3312 | `					  }` |
|        - |  3313 | `/*` |
|        - |  3314 | ` * LOAD_LIST: P1 * *` |
|        - |  3315 | ` *` |
|        - |  3316 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3317 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3318 | ` * Caveats:` |
|        - |  3319 | ` *  This implementation support only a single nesting level.` |
|        - |  3320 | ` */` |
|       26 |  3321 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3322 | `	ph7_value *pEntry;` |
|       53 |  3323 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3324 | `		/* Empty list,break immediately */` |
|      ! 0 |  3325 | `		break;` |
|        - |  3326 | `	}` |
|       53 |  3327 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3328 | `#ifdef UNTRUST` |
|        - |  3329 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3330 | `		goto Abort;` |
|        - |  3331 | `	}` |
|        - |  3332 | `#endif` |
|       53 |  3333 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       49 |  3334 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3335 | `		ph7_hashmap_node *pNode;` |
|        - |  3336 | `		ph7_value sKey,*pObj;` |
|        - |  3337 | `		/* Start Copying */` |
|       49 |  3338 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      153 |  3339 | `		while( pEntry <= pTos ){` |
|      105 |  3340 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       97 |  3341 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       97 |  3342 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       97 |  3343 | `					if( rc == SXRET_OK ){` |
|        - |  3344 | `						/* Store node value */` |
|       97 |  3345 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       49 |  3346 | `					}else{` |
|        - |  3347 | `						/* Nullify the variable */` |
|      ! 0 |  3348 | `						PH7_MemObjRelease(pObj);` |
|        - |  3349 | `					}` |
|       48 |  3350 | `				}` |
|       48 |  3351 | `			}` |
|      105 |  3352 | `			sKey.x.iVal++; /* Next numeric index */` |
|      105 |  3353 | `			pEntry++;` |
|        1 |  3354 | `		}` |
|       24 |  3355 | `	}` |
|       53 |  3356 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       53 |  3357 | `	break;` |
|        - |  3358 | `					   }` |
|        - |  3359 | `/*` |
|        - |  3360 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3361 | ` *` |
|        - |  3362 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3363 | ` * from the stack.` |
|        - |  3364 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3365 | ` * instead.` |
|        - |  3366 | ` */` |
|   216913 |  3367 | `case PH7_OP_LOAD_IDX: {` |
|   433872 |  3368 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   433872 |  3369 | `	ph7_hashmap *pMap = 0;` |
|        - |  3370 | `	ph7_value *pIdx;` |
|   433872 |  3371 | `	pIdx = 0;` |
|   433872 |  3372 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3373 | `		if( !pInstr->iP2){` |
|        - |  3374 | `			/* No available index,load NULL */` |
|      ! 0 |  3375 | `			if( pTos >= pStack ){` |
|      ! 0 |  3376 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3377 | `			}else{` |
|        - |  3378 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3379 | `				pTos++;` |
|      ! 0 |  3380 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3381 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3382 | `			}` |
|        - |  3383 | `			/* Emit a notice */` |
|      ! 0 |  3384 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3385 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3386 | `			break;` |
|        - |  3387 | `		}` |
|      ! 0 |  3388 | `	}else{` |
|   433872 |  3389 | `		pIdx = pTos;` |
|   433872 |  3390 | `		pTos--;` |
|        - |  3391 | `	}` |
|   433872 |  3392 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3393 | `		/* String access */` |
|   340676 |  3394 | `		if( pIdx ){` |
|        - |  3395 | `			sxu32 nOfft;` |
|   340676 |  3396 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3397 | `				/* Force an int cast */` |
|      ! 0 |  3398 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3399 | `			}` |
|   340676 |  3400 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340676 |  3401 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3402 | `				/* Invalid offset,load null */` |
|      ! 0 |  3403 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3404 | `			}else{` |
|   340676 |  3405 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340676 |  3406 | `				int c = zData[nOfft];` |
|   340676 |  3407 | `				PH7_MemObjRelease(pTos);` |
|   340676 |  3408 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340676 |  3409 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3410 | `			}` |
|   170361 |  3411 | `		}else{` |
|        - |  3412 | `			/* No available index,load NULL */` |
|      ! 0 |  3413 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3414 | `		}` |
|   340676 |  3415 | `		break;` |
|        - |  3416 | `	}` |
|    93198 |  3417 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3418 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3419 | `			ph7_value *pObj;` |
|      ! 0 |  3420 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3421 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3422 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3423 | `			}` |
|      ! 0 |  3424 | `		}` |
|      ! 0 |  3425 | `	}` |
|    93198 |  3426 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    93198 |  3427 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    93198 |  3428 | `		if( pInstr->iP2 ){` |
|        - |  3429 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3430 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3431 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3432 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3433 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3434 | `		}` |
|        - |  3435 | `		/* Point to the hashmap */` |
|    93198 |  3436 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    93198 |  3437 | `		if( pIdx ){` |
|        - |  3438 | `			/* Load the desired entry */` |
|    93198 |  3439 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    46598 |  3440 | `		}` |
|    93198 |  3441 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3442 | `			/* Create a new empty entry */` |
|      265 |  3443 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3444 | `			if( rc == SXRET_OK ){` |
|        - |  3445 | `				/* Point to the last inserted entry */` |
|      265 |  3446 | `				pNode = pMap->pLast;` |
|      132 |  3447 | `			}` |
|      132 |  3448 | `		}` |
|    46598 |  3449 | `	}` |
|    93198 |  3450 | `	if( pIdx ){` |
|    93198 |  3451 | `		PH7_MemObjRelease(pIdx);` |
|    46598 |  3452 | `	}` |
|    93198 |  3453 | `	if( rc == SXRET_OK ){` |
|        - |  3454 | `		/* Load entry contents */` |
|    42564 |  3455 | `		if( pMap->iRef < 2 ){` |
|        - |  3456 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3457 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3458 | `			 */` |
|       24 |  3459 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3460 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3461 | `		}else{` |
|    42542 |  3462 | `			pTos->nIdx = pNode->nValIdx;` |
|    42542 |  3463 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    42542 |  3464 | `			PH7_HashmapUnref(pMap);` |
|        - |  3465 | `		}` |
|    21283 |  3466 | `	}else{` |
|        - |  3467 | `		/* No such entry,load NULL */` |
|    50636 |  3468 | `		PH7_MemObjRelease(pTos);` |
|    50636 |  3469 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3470 | `	}` |
|    93198 |  3471 | `	break;` |
|        - |  3472 | `					  }` |
|        - |  3473 | `/*` |
|        - |  3474 | ` * LOAD_CLOSURE * * P3` |
|        - |  3475 | ` *` |
|        - |  3476 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3477 | ` * name in the stack.` |
|        - |  3478 | ` */` |
|        4 |  3479 | `case PH7_OP_LOAD_CLOSURE:{` |
|        9 |  3480 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        9 |  3481 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3482 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3483 | `		ph7_vm_func *pClosure;` |
|        - |  3484 | `		char *zName;` |
|        - |  3485 | `		sxu32 mLen;` |
|        - |  3486 | `		sxu32 n;` |
|        - |  3487 | `		/* Create a new VM function */` |
|        9 |  3488 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3489 | `		/* Generate an unique closure name */` |
|        9 |  3490 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        9 |  3491 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3492 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3493 | `			goto Abort;` |
|        - |  3494 | `		}` |
|        9 |  3495 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        9 |  3496 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3497 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3498 | `		}` |
|        - |  3499 | `		/* Zero the stucture */` |
|        9 |  3500 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3501 | `		/* Perform a structure assignment on read-only items */` |
|        9 |  3502 | `		pClosure->aArgs = pFunc->aArgs;` |
|        9 |  3503 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        9 |  3504 | `		pClosure->aStatic = pFunc->aStatic;` |
|        9 |  3505 | `		pClosure->iFlags = pFunc->iFlags;` |
|        9 |  3506 | `		pClosure->pUserData = pFunc->pUserData;` |
|        9 |  3507 | `		pClosure->sSignature = pFunc->sSignature;` |
|        9 |  3508 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|        9 |  3509 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|        9 |  3510 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3511 | `		/* Register the closure */` |
|        9 |  3512 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3513 | `		/* Set up closure environment */` |
|        9 |  3514 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        9 |  3515 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       27 |  3516 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3517 | `			ph7_value *pValue;` |
|       19 |  3518 | `			pEnv = &aEnv[n];` |
|       19 |  3519 | `			sEnv.sName  = pEnv->sName;` |
|       19 |  3520 | `			sEnv.iFlags = pEnv->iFlags;` |
|       19 |  3521 | `			sEnv.nIdx = SXU32_HIGH;` |
|       19 |  3522 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       19 |  3523 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3524 | `				/* Pass by reference */` |
|      ! 0 |  3525 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3526 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3527 | `					);` |
|      ! 0 |  3528 | `			}` |
|        - |  3529 | `			/* Standard pass by value */` |
|       19 |  3530 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       19 |  3531 | `			if( pValue ){` |
|        - |  3532 | `				/* Copy imported value */` |
|       11 |  3533 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3534 | `			}` |
|        - |  3535 | `			/* Insert the imported variable */` |
|       19 |  3536 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       10 |  3537 | `		}` |
|        - |  3538 | `		/* Finally,load the closure name on the stack */` |
|        9 |  3539 | `		pTos++;` |
|        9 |  3540 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3541 | `	}` |
|        9 |  3542 | `	break;` |
|        - |  3543 | `						 }` |
|        - |  3544 | `/*` |
|        - |  3545 | ` * STORE * P2 P3` |
|        - |  3546 | ` *` |
|        - |  3547 | ` * Perform a store (Assignment) operation.` |
|        - |  3548 | ` */` |
|   115434 |  3549 | `case PH7_OP_STORE: {` |
|        - |  3550 | `	ph7_value *pObj;` |
|        - |  3551 | `	SyString sName;` |
|        - |  3552 | `#ifdef UNTRUST` |
|        - |  3553 | `	if( pTos < pStack ){` |
|        - |  3554 | `		goto Abort;` |
|        - |  3555 | `	}` |
|        - |  3556 | `#endif` |
|   230870 |  3557 | `	if( pInstr->iP2 ){` |
|        - |  3558 | `		sxu32 nIdx;` |
|        - |  3559 | `		/* Member store operation */` |
|     2974 |  3560 | `		nIdx = pTos->nIdx;` |
|     2974 |  3561 | `		VmPopOperand(&pTos,1);` |
|     2974 |  3562 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3563 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3564 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3565 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3566 | `		}else{` |
|        - |  3567 | `			/* Point to the desired memory object */` |
|     2970 |  3568 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2970 |  3569 | `			if( pObj ){` |
|        - |  3570 | `				/* Perform the store operation */` |
|     2970 |  3571 | `				PH7_MemObjStore(pTos,pObj);` |
|     1484 |  3572 | `			}` |
|        - |  3573 | `		}` |
|   116922 |  3574 | `		break;` |
|   227898 |  3575 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3576 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3577 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3578 | `			/* Force a string cast */` |
|      ! 0 |  3579 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3580 | `		}` |
|        7 |  3581 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3582 | `		pTos--;` |
|        - |  3583 | `#ifdef UNTRUST` |
|        - |  3584 | `		if( pTos < pStack  ){` |
|        - |  3585 | `			goto Abort;` |
|        - |  3586 | `		}` |
|        - |  3587 | `#endif` |
|        4 |  3588 | `	}else{` |
|   227892 |  3589 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3590 | `	}` |
|        - |  3591 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   227898 |  3592 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   227898 |  3593 | `	if( pObj == 0 ){` |
|      ! 0 |  3594 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3595 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3596 | `		goto Abort;` |
|        - |  3597 | `	}` |
|   227898 |  3598 | `	if( !pInstr->p3 ){` |
|        7 |  3599 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3600 | `	}` |
|        - |  3601 | `	/* Perform the store operation */` |
|   227898 |  3602 | `	PH7_MemObjStore(pTos,pObj);` |
|   227898 |  3603 | `	break;` |
|        - |  3604 | `				   }` |
|        - |  3605 | `/*` |
|        - |  3606 | ` * STORE_IDX:   P1 * P3` |
|        - |  3607 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3608 | ` *` |
|        - |  3609 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3610 | ` */` |
|    83565 |  3611 | `case PH7_OP_STORE_IDX:` |
|        - |  3612 | `case PH7_OP_STORE_IDX_REF: {` |
|   167132 |  3613 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3614 | `	ph7_value *pKey;` |
|        - |  3615 | `	sxu32 nIdx;` |
|   167132 |  3616 | `	if( pInstr->iP1 ){` |
|        - |  3617 | `		/* Key is next on stack */` |
|    57970 |  3618 | `		pKey = pTos;` |
|    57970 |  3619 | `		pTos--;` |
|    28986 |  3620 | `	}else{` |
|   109164 |  3621 | `		pKey = 0;` |
|        - |  3622 | `	}` |
|   167132 |  3623 | `	nIdx = pTos->nIdx;` |
|   167132 |  3624 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3625 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3626 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3627 | `		 * checking true sharing count, then re-add after separation. */` |
|   167080 |  3628 | `		if( nIdx != SXU32_HIGH ){` |
|   167080 |  3629 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   250619 |  3630 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   167080 |  3631 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3632 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3633 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3634 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3635 | `				 * refcounts if the backing array was already separated. */` |
|   167080 |  3636 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   167080 |  3637 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   167080 |  3638 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   167080 |  3639 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   167080 |  3640 | `					pTos->x.pOther = pMap;` |
|    83541 |  3641 | `				}else{` |
|        - |  3642 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3643 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3644 | `					pMap = pCur;` |
|        - |  3645 | `				}` |
|    83541 |  3646 | `			}else{` |
|      ! 0 |  3647 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3648 | `			}` |
|    83541 |  3649 | `		}else{` |
|      ! 0 |  3650 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3651 | `		}` |
|   167080 |  3652 | `		if( pMap->iRef < 2 ){` |
|        - |  3653 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3654 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3655 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3656 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3657 | `			pMap->iRef = 2;` |
|      ! 0 |  3658 | `		}` |
|    83541 |  3659 | `	}else{` |
|        - |  3660 | `		ph7_value *pObj;` |
|       53 |  3661 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3662 | `		if( pObj == 0 ){` |
|      ! 0 |  3663 | `			if( pKey ){` |
|      ! 0 |  3664 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3665 | `			}` |
|      ! 0 |  3666 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3667 | `			break;` |
|        - |  3668 | `		}` |
|        - |  3669 | `		/* Phase#1: Load the array */` |
|       53 |  3670 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3671 | `			VmPopOperand(&pTos,1);` |
|       53 |  3672 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3673 | `				/* Force a string cast */` |
|      ! 0 |  3674 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3675 | `			}` |
|       53 |  3676 | `			if( pKey == 0 ){` |
|        - |  3677 | `				/* Append string */` |
|        3 |  3678 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3679 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3680 | `				}` |
|        2 |  3681 | `			}else{` |
|        - |  3682 | `				sxu32 nOfft;` |
|       51 |  3683 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3684 | `					/* Force an int cast */` |
|       51 |  3685 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3686 | `				}` |
|       51 |  3687 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3688 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3689 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3690 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3691 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3692 | `				}else{` |
|      ! 0 |  3693 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3694 | `						/* Perform an append operation */` |
|      ! 0 |  3695 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3696 | `					}` |
|        - |  3697 | `				}` |
|        - |  3698 | `			}` |
|       53 |  3699 | `			if( pKey ){` |
|       51 |  3700 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3701 | `			}` |
|       53 |  3702 | `			break;` |
|      ! 0 |  3703 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3704 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3705 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3706 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3707 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3708 | `				goto Abort;` |
|        - |  3709 | `			}` |
|      ! 0 |  3710 | `		}` |
|        - |  3711 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3712 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3713 | `	}` |
|   167080 |  3714 | `	VmPopOperand(&pTos,1);` |
|        - |  3715 | `	/* Phase#2: Perform the insertion */` |
|   167080 |  3716 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3717 | `		/* Insertion by reference */` |
|       15 |  3718 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3719 | `	}else{` |
|   167066 |  3720 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3721 | `	}` |
|   167080 |  3722 | `	if( pKey ){` |
|    57920 |  3723 | `		PH7_MemObjRelease(pKey);` |
|    28959 |  3724 | `	}` |
|   167080 |  3725 | `	break;` |
|        - |  3726 | `					   }` |
|        - |  3727 | `/*` |
|        - |  3728 | ` * INCR: P1 * *` |
|        - |  3729 | ` *` |
|        - |  3730 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3731 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3732 | ` * the stack and increment after that.` |
|        - |  3733 | ` */` |
|   151416 |  3734 | `case PH7_OP_INCR:` |
|        - |  3735 | `#ifdef UNTRUST` |
|        - |  3736 | `	if( pTos < pStack ){` |
|        - |  3737 | `		goto Abort;` |
|        - |  3738 | `	}` |
|        - |  3739 | `#endif` |
|   302878 |  3740 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302878 |  3741 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3742 | `			ph7_value *pObj;` |
|   302878 |  3743 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3744 | `				/* Force a numeric cast */` |
|   302878 |  3745 | `				PH7_MemObjToNumeric(pObj);` |
|   302878 |  3746 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3747 | `					pObj->rVal++;` |
|        - |  3748 | `					/* Try to get an integer representation */` |
|      ! 0 |  3749 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3750 | `				}else{` |
|   302878 |  3751 | `					pObj->x.iVal++;` |
|   302878 |  3752 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3753 | `				}` |
|   302878 |  3754 | `				if( pInstr->iP1 ){` |
|        - |  3755 | `					/* Pre-icrement */` |
|       71 |  3756 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3757 | `				}` |
|   151460 |  3758 | `			}` |
|   151462 |  3759 | `		}else{` |
|      ! 0 |  3760 | `			if( pInstr->iP1 ){` |
|        - |  3761 | `				/* Force a numeric cast */` |
|      ! 0 |  3762 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3763 | `				/* Pre-increment */` |
|      ! 0 |  3764 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3765 | `					pTos->rVal++;` |
|        - |  3766 | `					/* Try to get an integer representation */` |
|      ! 0 |  3767 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3768 | `				}else{` |
|      ! 0 |  3769 | `					pTos->x.iVal++;` |
|      ! 0 |  3770 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3771 | `				}` |
|      ! 0 |  3772 | `			}` |
|        - |  3773 | `		}` |
|   151460 |  3774 | `	}` |
|   302878 |  3775 | `	break;` |
|        - |  3776 | `/*` |
|        - |  3777 | ` * DECR: P1 * *` |
|        - |  3778 | ` *` |
|        - |  3779 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3780 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3781 | ` * and decrement after that.` |
|        - |  3782 | ` */` |
|        2 |  3783 | `case PH7_OP_DECR:` |
|        - |  3784 | `#ifdef UNTRUST` |
|        - |  3785 | `	if( pTos < pStack ){` |
|        - |  3786 | `		goto Abort;` |
|        - |  3787 | `	}` |
|        - |  3788 | `#endif` |
|        5 |  3789 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3790 | `		/* Force a numeric cast */` |
|        5 |  3791 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3792 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3793 | `			ph7_value *pObj;` |
|        5 |  3794 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3795 | `				/* Force a numeric cast */` |
|        5 |  3796 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3797 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3798 | `					pObj->rVal--;` |
|        - |  3799 | `					/* Try to get an integer representation */` |
|      ! 0 |  3800 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3801 | `				}else{` |
|        5 |  3802 | `					pObj->x.iVal--;` |
|        5 |  3803 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3804 | `				}` |
|        5 |  3805 | `				if( pInstr->iP1 ){` |
|        - |  3806 | `					/* Pre-icrement */` |
|      ! 0 |  3807 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3808 | `				}` |
|        2 |  3809 | `			}` |
|        3 |  3810 | `		}else{` |
|      ! 0 |  3811 | `			if( pInstr->iP1 ){` |
|        - |  3812 | `				/* Pre-increment */` |
|      ! 0 |  3813 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3814 | `					pTos->rVal--;` |
|        - |  3815 | `					/* Try to get an integer representation */` |
|      ! 0 |  3816 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3817 | `				}else{` |
|      ! 0 |  3818 | `					pTos->x.iVal--;` |
|      ! 0 |  3819 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3820 | `				}` |
|      ! 0 |  3821 | `			}` |
|        - |  3822 | `		}` |
|        2 |  3823 | `	}` |
|        5 |  3824 | `	break;` |
|        - |  3825 | `/*` |
|        - |  3826 | ` * UMINUS: * * *` |
|        - |  3827 | ` *` |
|        - |  3828 | ` * Perform a unary minus operation.` |
|        - |  3829 | ` */` |
|    24274 |  3830 | `case PH7_OP_UMINUS:` |
|        - |  3831 | `#ifdef UNTRUST` |
|        - |  3832 | `	if( pTos < pStack ){` |
|        - |  3833 | `		goto Abort;` |
|        - |  3834 | `	}` |
|        - |  3835 | `#endif` |
|        - |  3836 | `	/* Force a numeric (integer,real or both) cast */` |
|    48550 |  3837 | `	PH7_MemObjToNumeric(pTos);` |
|    48550 |  3838 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3839 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3840 | `	}` |
|    48550 |  3841 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    48520 |  3842 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24259 |  3843 | `	}` |
|    48550 |  3844 | `	break;` |
|        - |  3845 | `/*` |
|        - |  3846 | ` * UPLUS: * * *` |
|        - |  3847 | ` *` |
|        - |  3848 | ` * Perform a unary plus operation.` |
|        - |  3849 | ` */` |
|       17 |  3850 | `case PH7_OP_UPLUS:` |
|        - |  3851 | `#ifdef UNTRUST` |
|        - |  3852 | `	if( pTos < pStack ){` |
|        - |  3853 | `		goto Abort;` |
|        - |  3854 | `	}` |
|        - |  3855 | `#endif` |
|        - |  3856 | `	/* Force a numeric (integer,real or both) cast */` |
|       35 |  3857 | `	PH7_MemObjToNumeric(pTos);` |
|       35 |  3858 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3859 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3860 | `	}` |
|       35 |  3861 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       35 |  3862 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       17 |  3863 | `	}` |
|       35 |  3864 | `	break;` |
|        - |  3865 | `/*` |
|        - |  3866 | ` * OP_LNOT: * * *` |
|        - |  3867 | ` *` |
|        - |  3868 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3869 | ` * with its complement.` |
|        - |  3870 | ` */` |
|    39989 |  3871 | `case PH7_OP_LNOT:` |
|        - |  3872 | `#ifdef UNTRUST` |
|        - |  3873 | `	if( pTos < pStack ){` |
|        - |  3874 | `		goto Abort;` |
|        - |  3875 | `	}` |
|        - |  3876 | `#endif` |
|        - |  3877 | `	/* Force a boolean cast */` |
|    80024 |  3878 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3879 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3880 | `	}` |
|    80024 |  3881 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80024 |  3882 | `	break;` |
|        - |  3883 | `/*` |
|        - |  3884 | ` * OP_BITNOT: * * *` |
|        - |  3885 | ` *` |
|        - |  3886 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3887 | ` * with its ones-complement.` |
|        - |  3888 | ` */` |
|       13 |  3889 | `case PH7_OP_BITNOT:` |
|        - |  3890 | `#ifdef UNTRUST` |
|        - |  3891 | `	if( pTos < pStack ){` |
|        - |  3892 | `		goto Abort;` |
|        - |  3893 | `	}` |
|        - |  3894 | `#endif` |
|        - |  3895 | `	/* Force an integer cast */` |
|       28 |  3896 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3897 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3898 | `	}` |
|       28 |  3899 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  3900 | `	break;` |
|        - |  3901 | `/* OP_MUL * * *` |
|        - |  3902 | ` * OP_MUL_STORE * * *` |
|        - |  3903 | ` *` |
|        - |  3904 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3905 | ` * and push the result back onto the stack.` |
|        - |  3906 | ` */` |
|     1249 |  3907 | `case PH7_OP_MUL:` |
|        - |  3908 | `case PH7_OP_MUL_STORE: {` |
|     2500 |  3909 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3910 | `	/* Force the operand to be numeric */` |
|        - |  3911 | `#ifdef UNTRUST` |
|        - |  3912 | `	if( pNos < pStack ){` |
|        - |  3913 | `		goto Abort;` |
|        - |  3914 | `	}` |
|        - |  3915 | `#endif` |
|     2500 |  3916 | `	PH7_MemObjToNumeric(pTos);` |
|     2500 |  3917 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3918 | `	/* Perform the requested operation */` |
|     2500 |  3919 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3920 | `		/* Floating point arithemic */` |
|        - |  3921 | `		ph7_real a,b,r;` |
|       17 |  3922 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3923 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3924 | `		}` |
|       17 |  3925 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3926 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3927 | `		}` |
|       17 |  3928 | `		a = pNos->rVal;` |
|       17 |  3929 | `		b = pTos->rVal;` |
|       17 |  3930 | `		r = a * b;` |
|        - |  3931 | `		/* Push the result */` |
|       17 |  3932 | `		pNos->rVal = r;` |
|       17 |  3933 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3934 | `		/* Try to get an integer representation */` |
|       17 |  3935 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3936 | `	}else{` |
|        - |  3937 | `		/* Integer arithmetic */` |
|        - |  3938 | `		sxi64 a,b,r;` |
|     2484 |  3939 | `		a = pNos->x.iVal;` |
|     2484 |  3940 | `		b = pTos->x.iVal;` |
|     2484 |  3941 | `		r = a * b;` |
|        - |  3942 | `		/* Push the result */` |
|     2484 |  3943 | `		pNos->x.iVal = r;` |
|     2484 |  3944 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3945 | `	}` |
|     2500 |  3946 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3947 | `		ph7_value *pObj;` |
|       27 |  3948 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3949 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       27 |  3950 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  3951 | `			PH7_MemObjStore(pNos,pObj);` |
|       13 |  3952 | `		}` |
|       13 |  3953 | `	}` |
|     2500 |  3954 | `	VmPopOperand(&pTos,1);` |
|     2500 |  3955 | `	break;` |
|        - |  3956 | `				 }` |
|        - |  3957 | `/* OP_ADD * * *` |
|        - |  3958 | ` *` |
|        - |  3959 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3960 | ` * and push the result back onto the stack.` |
|        - |  3961 | ` */` |
|      452 |  3962 | `case PH7_OP_ADD:{` |
|      906 |  3963 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3964 | `#ifdef UNTRUST` |
|        - |  3965 | `	if( pNos < pStack ){` |
|        - |  3966 | `		goto Abort;` |
|        - |  3967 | `	}` |
|        - |  3968 | `#endif` |
|        - |  3969 | `	/* Perform the addition */` |
|      906 |  3970 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      906 |  3971 | `	VmPopOperand(&pTos,1);` |
|      906 |  3972 | `	break;` |
|        - |  3973 | `				}` |
|        - |  3974 | `/*` |
|        - |  3975 | ` * OP_ADD_STORE * * *` |
|        - |  3976 | ` *` |
|        - |  3977 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3978 | ` * and push the result back onto the stack.` |
|        - |  3979 | ` */` |
|      495 |  3980 | `case PH7_OP_ADD_STORE:{` |
|      992 |  3981 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3982 | `	ph7_value *pObj;` |
|        - |  3983 | `	sxu32 nIdx;` |
|        - |  3984 | `#ifdef UNTRUST` |
|        - |  3985 | `	if( pNos < pStack ){` |
|        - |  3986 | `		goto Abort;` |
|        - |  3987 | `	}` |
|        - |  3988 | `#endif` |
|        - |  3989 | `	/* Perform the addition */` |
|      992 |  3990 | `	nIdx = pTos->nIdx;` |
|      992 |  3991 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3992 | `	/* Peform the store operation */` |
|      992 |  3993 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3994 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      992 |  3995 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      992 |  3996 | `		PH7_MemObjStore(pTos,pObj);` |
|      495 |  3997 | `	}` |
|        - |  3998 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      992 |  3999 | `	PH7_MemObjStore(pTos,pNos);` |
|      992 |  4000 | `	VmPopOperand(&pTos,1);` |
|      992 |  4001 | `	break;` |
|        - |  4002 | `				}` |
|        - |  4003 | `/* OP_SUB * * *` |
|        - |  4004 | ` *` |
|        - |  4005 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4006 | ` * first (what was next on the stack) from the second (the` |
|        - |  4007 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4008 | ` */` |
|      300 |  4009 | `case PH7_OP_SUB: {` |
|      602 |  4010 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4011 | `#ifdef UNTRUST` |
|        - |  4012 | `	if( pNos < pStack ){` |
|        - |  4013 | `		goto Abort;` |
|        - |  4014 | `	}` |
|        - |  4015 | `#endif` |
|      602 |  4016 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4017 | `		/* Floating point arithemic */` |
|        - |  4018 | `		ph7_real a,b,r;` |
|       95 |  4019 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4020 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4021 | `		}` |
|       95 |  4022 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4023 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4024 | `		}` |
|       95 |  4025 | `		a = pNos->rVal;` |
|       95 |  4026 | `		b = pTos->rVal;` |
|       95 |  4027 | `		r = a - b;` |
|        - |  4028 | `		/* Push the result */` |
|       95 |  4029 | `		pNos->rVal = r;` |
|       95 |  4030 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4031 | `		/* Try to get an integer representation */` |
|       95 |  4032 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4033 | `	}else{` |
|        - |  4034 | `		/* Integer arithmetic */` |
|        - |  4035 | `		sxi64 a,b,r;` |
|      508 |  4036 | `		a = pNos->x.iVal;` |
|      508 |  4037 | `		b = pTos->x.iVal;` |
|      508 |  4038 | `		r = a - b;` |
|        - |  4039 | `		/* Push the result */` |
|      508 |  4040 | `		pNos->x.iVal = r;` |
|      508 |  4041 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4042 | `	}` |
|      602 |  4043 | `	VmPopOperand(&pTos,1);` |
|      602 |  4044 | `	break;` |
|        - |  4045 | `				 }` |
|        - |  4046 | `/* OP_SUB_STORE * * *` |
|        - |  4047 | ` *` |
|        - |  4048 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4049 | ` * first (what was next on the stack) from the second (the` |
|        - |  4050 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4051 | ` */` |
|        2 |  4052 | `case PH7_OP_SUB_STORE: {` |
|        5 |  4053 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4054 | `	ph7_value *pObj;` |
|        - |  4055 | `#ifdef UNTRUST` |
|        - |  4056 | `	if( pNos < pStack ){` |
|        - |  4057 | `		goto Abort;` |
|        - |  4058 | `	}` |
|        - |  4059 | `#endif` |
|        5 |  4060 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4061 | `		/* Floating point arithemic */` |
|        - |  4062 | `		ph7_real a,b,r;` |
|      ! 0 |  4063 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4064 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4065 | `		}` |
|      ! 0 |  4066 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4067 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4068 | `		}` |
|      ! 0 |  4069 | `		a = pTos->rVal;` |
|      ! 0 |  4070 | `		b = pNos->rVal;` |
|      ! 0 |  4071 | `		r = a - b;` |
|        - |  4072 | `		/* Push the result */` |
|      ! 0 |  4073 | `		pNos->rVal = r;` |
|      ! 0 |  4074 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4075 | `		/* Try to get an integer representation */` |
|      ! 0 |  4076 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4077 | `	}else{` |
|        - |  4078 | `		/* Integer arithmetic */` |
|        - |  4079 | `		sxi64 a,b,r;` |
|        5 |  4080 | `		a = pTos->x.iVal;` |
|        5 |  4081 | `		b = pNos->x.iVal;` |
|        5 |  4082 | `		r = a - b;` |
|        - |  4083 | `		/* Push the result */` |
|        5 |  4084 | `		pNos->x.iVal = r;` |
|        5 |  4085 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4086 | `	}` |
|        5 |  4087 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4088 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4089 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4090 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4091 | `	}` |
|        5 |  4092 | `	VmPopOperand(&pTos,1);` |
|        5 |  4093 | `	break;` |
|        - |  4094 | `				 }` |
|        - |  4095 |  |
|        - |  4096 | `/*` |
|        - |  4097 | ` * OP_MOD * * *` |
|        - |  4098 | ` *` |
|        - |  4099 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4100 | ` * first (what was next on the stack) from the second (the` |
|        - |  4101 | ` * top of the stack) and push the remainder after division` |
|        - |  4102 | ` * onto the stack.` |
|        - |  4103 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4104 | ` */` |
|      306 |  4105 | `case PH7_OP_MOD:{` |
|      614 |  4106 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4107 | `	sxi64 a,b,r;` |
|        - |  4108 | `#ifdef UNTRUST` |
|        - |  4109 | `	if( pNos < pStack ){` |
|        - |  4110 | `		goto Abort;` |
|        - |  4111 | `	}` |
|        - |  4112 | `#endif` |
|        - |  4113 | `	/* Force the operands to be integer */` |
|      614 |  4114 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4115 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4116 | `	}` |
|      614 |  4117 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4118 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4119 | `	}` |
|        - |  4120 | `	/* Perform the requested operation */` |
|      614 |  4121 | `	a = pNos->x.iVal;` |
|      614 |  4122 | `	b = pTos->x.iVal;` |
|      614 |  4123 | `	if( b == 0 ){` |
|        3 |  4124 | `		r = 0;` |
|        3 |  4125 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4126 | `		/* goto Abort; */` |
|        2 |  4127 | `	}else{` |
|      611 |  4128 | `		r = a%b;` |
|        - |  4129 | `	}` |
|        - |  4130 | `	/* Push the result */` |
|      614 |  4131 | `	pNos->x.iVal = r;` |
|      614 |  4132 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      614 |  4133 | `	VmPopOperand(&pTos,1);` |
|      614 |  4134 | `	break;` |
|        - |  4135 | `				}` |
|        - |  4136 | `/*` |
|        - |  4137 | ` * OP_MOD_STORE * * *` |
|        - |  4138 | ` *` |
|        - |  4139 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4140 | ` * first (what was next on the stack) from the second (the` |
|        - |  4141 | ` * top of the stack) and push the remainder after division` |
|        - |  4142 | ` * onto the stack.` |
|        - |  4143 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4144 | ` */` |
|        1 |  4145 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4146 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4147 | `	ph7_value *pObj;` |
|        - |  4148 | `	sxi64 a,b,r;` |
|        - |  4149 | `#ifdef UNTRUST` |
|        - |  4150 | `	if( pNos < pStack ){` |
|        - |  4151 | `		goto Abort;` |
|        - |  4152 | `	}` |
|        - |  4153 | `#endif` |
|        - |  4154 | `	/* Force the operands to be integer */` |
|        3 |  4155 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4156 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4157 | `	}` |
|        3 |  4158 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4159 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4160 | `	}` |
|        - |  4161 | `	/* Perform the requested operation */` |
|        3 |  4162 | `	a = pTos->x.iVal;` |
|        3 |  4163 | `	b = pNos->x.iVal;` |
|        3 |  4164 | `	if( b == 0 ){` |
|      ! 0 |  4165 | `		r = 0;` |
|      ! 0 |  4166 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4167 | `		/* goto Abort; */` |
|      ! 0 |  4168 | `	}else{` |
|        3 |  4169 | `		r = a%b;` |
|        - |  4170 | `	}` |
|        - |  4171 | `	/* Push the result */` |
|        3 |  4172 | `	pNos->x.iVal = r;` |
|        3 |  4173 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4174 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4175 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4176 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4177 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4178 | `	}` |
|        3 |  4179 | `	VmPopOperand(&pTos,1);` |
|        3 |  4180 | `	break;` |
|        - |  4181 | `				}` |
|        - |  4182 | `/*` |
|        - |  4183 | ` * OP_DIV * * *` |
|        - |  4184 | ` *` |
|        - |  4185 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4186 | ` * first (what was next on the stack) from the second (the` |
|        - |  4187 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4188 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4189 | ` */` |
|       29 |  4190 | `case PH7_OP_DIV:{` |
|       60 |  4191 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4192 | `	ph7_real a,b,r;` |
|        - |  4193 | `#ifdef UNTRUST` |
|        - |  4194 | `	if( pNos < pStack ){` |
|        - |  4195 | `		goto Abort;` |
|        - |  4196 | `	}` |
|        - |  4197 | `#endif` |
|        - |  4198 | `	/* Force the operands to be real */` |
|       60 |  4199 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 |  4200 | `		PH7_MemObjToReal(pTos);` |
|       27 |  4201 | `	}` |
|       60 |  4202 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       22 |  4203 | `		PH7_MemObjToReal(pNos);` |
|       10 |  4204 | `	}` |
|        - |  4205 | `	/* Perform the requested operation */` |
|       60 |  4206 | `	a = pNos->rVal;` |
|       60 |  4207 | `	b = pTos->rVal;` |
|       60 |  4208 | `	if( b == 0 ){` |
|        - |  4209 | `		/* Division by zero */` |
|        3 |  4210 | `		pNos->rVal = 0;` |
|        3 |  4211 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4212 | `		/* goto Abort; */` |
|        2 |  4213 | `	}else{` |
|       57 |  4214 | `		r = a/b;` |
|        - |  4215 | `		/* Push the result */` |
|       57 |  4216 | `		pNos->rVal = r;` |
|       57 |  4217 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4218 | `		/* Try to get an integer representation */` |
|       57 |  4219 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4220 | `	}` |
|       60 |  4221 | `	VmPopOperand(&pTos,1);` |
|       60 |  4222 | `	break;` |
|        - |  4223 | `				}` |
|        - |  4224 | `/*` |
|        - |  4225 | ` * OP_DIV_STORE * * *` |
|        - |  4226 | ` *` |
|        - |  4227 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4228 | ` * first (what was next on the stack) from the second (the` |
|        - |  4229 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4230 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4231 | ` */` |
|        1 |  4232 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4233 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4234 | `	ph7_value *pObj;` |
|        - |  4235 | `	ph7_real a,b,r;` |
|        - |  4236 | `#ifdef UNTRUST` |
|        - |  4237 | `	if( pNos < pStack ){` |
|        - |  4238 | `		goto Abort;` |
|        - |  4239 | `	}` |
|        - |  4240 | `#endif` |
|        - |  4241 | `	/* Force the operands to be real */` |
|        3 |  4242 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4243 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4244 | `	}` |
|        3 |  4245 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4246 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4247 | `	}` |
|        - |  4248 | `	/* Perform the requested operation */` |
|        3 |  4249 | `	a = pTos->rVal;` |
|        3 |  4250 | `	b = pNos->rVal;` |
|        3 |  4251 | `	if( b == 0 ){` |
|        - |  4252 | `		/* Division by zero */` |
|      ! 0 |  4253 | `		r = 0;` |
|      ! 0 |  4254 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4255 | `		/* goto Abort; */` |
|      ! 0 |  4256 | `	}else{` |
|        3 |  4257 | `		r = a/b;` |
|        - |  4258 | `		/* Push the result */` |
|        3 |  4259 | `		pNos->rVal = r;` |
|        3 |  4260 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4261 | `		/* Try to get an integer representation */` |
|        3 |  4262 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4263 | `	}` |
|        3 |  4264 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4265 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4266 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4267 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4268 | `	}` |
|        3 |  4269 | `	VmPopOperand(&pTos,1);` |
|        3 |  4270 | `	break;` |
|        - |  4271 | `				}` |
|        - |  4272 | `/* OP_BAND * * *` |
|        - |  4273 | ` *` |
|        - |  4274 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4275 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4276 | ` * two elements.` |
|        - |  4277 | `*/` |
|        - |  4278 | `/* OP_BOR * * *` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4281 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4282 | ` * two elements.` |
|        - |  4283 | ` */` |
|        - |  4284 | `/* OP_BXOR * * *` |
|        - |  4285 | ` *` |
|        - |  4286 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4287 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4288 | ` * two elements.` |
|        - |  4289 | ` */` |
|       44 |  4290 | `case PH7_OP_BAND:` |
|        - |  4291 | `case PH7_OP_BOR:` |
|        - |  4292 | `case PH7_OP_BXOR:{` |
|       90 |  4293 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4294 | `	sxi64 a,b,r;` |
|        - |  4295 | `#ifdef UNTRUST` |
|        - |  4296 | `	if( pNos < pStack ){` |
|        - |  4297 | `		goto Abort;` |
|        - |  4298 | `	}` |
|        - |  4299 | `#endif` |
|        - |  4300 | `	/* Force the operands to be integer */` |
|       90 |  4301 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4302 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4303 | `	}` |
|       90 |  4304 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4305 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4306 | `	}` |
|        - |  4307 | `	/* Perform the requested operation */` |
|       90 |  4308 | `	a = pNos->x.iVal;` |
|       90 |  4309 | `	b = pTos->x.iVal;` |
|       90 |  4310 | `	switch(pInstr->iOp){` |
|        7 |  4311 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4312 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4313 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4314 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4315 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4316 | `	case PH7_OP_BAND:` |
|       62 |  4317 | `	default:          r = a&b; break;` |
|        - |  4318 | `	}` |
|        - |  4319 | `	/* Push the result */` |
|       90 |  4320 | `	pNos->x.iVal = r;` |
|       90 |  4321 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4322 | `	VmPopOperand(&pTos,1);` |
|       90 |  4323 | `	break;` |
|        - |  4324 | `				 }` |
|        - |  4325 | `/* OP_BAND_STORE * * *` |
|        - |  4326 | ` *` |
|        - |  4327 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4328 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4329 | ` * two elements.` |
|        - |  4330 | `*/` |
|        - |  4331 | `/* OP_BOR_STORE * * *` |
|        - |  4332 | ` *` |
|        - |  4333 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4334 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4335 | ` * two elements.` |
|        - |  4336 | ` */` |
|        - |  4337 | `/* OP_BXOR_STORE * * *` |
|        - |  4338 | ` *` |
|        - |  4339 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4340 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4341 | ` * two elements.` |
|        - |  4342 | ` */` |
|       10 |  4343 | `case PH7_OP_BAND_STORE:` |
|        - |  4344 | `case PH7_OP_BOR_STORE:` |
|        - |  4345 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4346 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4347 | `	ph7_value *pObj;` |
|        - |  4348 | `	sxi64 a,b,r;` |
|        - |  4349 | `#ifdef UNTRUST` |
|        - |  4350 | `	if( pNos < pStack ){` |
|        - |  4351 | `		goto Abort;` |
|        - |  4352 | `	}` |
|        - |  4353 | `#endif` |
|        - |  4354 | `	/* Force the operands to be integer */` |
|       21 |  4355 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4356 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4357 | `	}` |
|       21 |  4358 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4359 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4360 | `	}` |
|        - |  4361 | `	/* Perform the requested operation */` |
|       21 |  4362 | `	a = pTos->x.iVal;` |
|       21 |  4363 | `	b = pNos->x.iVal;` |
|       21 |  4364 | `	switch(pInstr->iOp){` |
|        3 |  4365 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4366 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4367 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4368 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4369 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4370 | `	case PH7_OP_BAND:` |
|        7 |  4371 | `	default:          r = a&b; break;` |
|        - |  4372 | `	}` |
|        - |  4373 | `	/* Push the result */` |
|       21 |  4374 | `	pNos->x.iVal = r;` |
|       21 |  4375 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4376 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4377 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4378 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4379 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4380 | `	}` |
|       21 |  4381 | `	VmPopOperand(&pTos,1);` |
|       21 |  4382 | `	break;` |
|        - |  4383 | `				 }` |
|        - |  4384 | `/* OP_SHL * * *` |
|        - |  4385 | ` *` |
|        - |  4386 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4387 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4388 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4389 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4390 | ` */` |
|        - |  4391 | `/* OP_SHR * * *` |
|        - |  4392 | ` *` |
|        - |  4393 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4394 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4395 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4396 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4397 | ` */` |
|       12 |  4398 | `case PH7_OP_SHL:` |
|        - |  4399 | `case PH7_OP_SHR: {` |
|       25 |  4400 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4401 | `	sxi64 a,r;` |
|        - |  4402 | `	sxi32 b;` |
|        - |  4403 | `#ifdef UNTRUST` |
|        - |  4404 | `	if( pNos < pStack ){` |
|        - |  4405 | `		goto Abort;` |
|        - |  4406 | `	}` |
|        - |  4407 | `#endif` |
|        - |  4408 | `	/* Force the operands to be integer */` |
|       25 |  4409 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4410 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4411 | `	}` |
|       25 |  4412 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4413 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4414 | `	}` |
|        - |  4415 | `	/* Perform the requested operation */` |
|       25 |  4416 | `	a = pNos->x.iVal;` |
|       25 |  4417 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4418 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4419 | `		r = a << b;` |
|        8 |  4420 | `	}else{` |
|       11 |  4421 | `		r = a >> b;` |
|        - |  4422 | `	}` |
|        - |  4423 | `	/* Push the result */` |
|       25 |  4424 | `	pNos->x.iVal = r;` |
|       25 |  4425 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4426 | `	VmPopOperand(&pTos,1);` |
|       25 |  4427 | `	break;` |
|        - |  4428 | `				 }` |
|        - |  4429 | `/*  OP_SHL_STORE * * *` |
|        - |  4430 | ` *` |
|        - |  4431 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4432 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4433 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4434 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4435 | ` */` |
|        - |  4436 | `/* OP_SHR_STORE * * *` |
|        - |  4437 | ` *` |
|        - |  4438 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4439 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4440 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4441 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4442 | ` */` |
|        9 |  4443 | `case PH7_OP_SHL_STORE:` |
|        - |  4444 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4445 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4446 | `	ph7_value *pObj;` |
|        - |  4447 | `	sxi64 a,r;` |
|        - |  4448 | `	sxi32 b;` |
|        - |  4449 | `#ifdef UNTRUST` |
|        - |  4450 | `	if( pNos < pStack ){` |
|        - |  4451 | `		goto Abort;` |
|        - |  4452 | `	}` |
|        - |  4453 | `#endif` |
|        - |  4454 | `	/* Force the operands to be integer */` |
|       19 |  4455 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4456 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4457 | `	}` |
|       19 |  4458 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4459 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4460 | `	}` |
|        - |  4461 | `	/* Perform the requested operation */` |
|       19 |  4462 | `	a = pTos->x.iVal;` |
|       19 |  4463 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4464 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4465 | `		r = a << b;` |
|        5 |  4466 | `	}else{` |
|       11 |  4467 | `		r = a >> b;` |
|        - |  4468 | `	}` |
|        - |  4469 | `	/* Push the result */` |
|       19 |  4470 | `	pNos->x.iVal = r;` |
|       19 |  4471 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4472 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4473 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4474 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4475 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4476 | `	}` |
|       19 |  4477 | `	VmPopOperand(&pTos,1);` |
|       19 |  4478 | `	break;` |
|        - |  4479 | `				 }` |
|        - |  4480 | `/* CAT:  P1 * *` |
|        - |  4481 | ` *` |
|        - |  4482 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4483 | ` * back.` |
|        - |  4484 | ` */` |
|    63458 |  4485 | `case PH7_OP_CAT:{` |
|        - |  4486 | `	ph7_value *pNos,*pCur;` |
|   126918 |  4487 | `	if( pInstr->iP1 < 1 ){` |
|    99866 |  4488 | `		pNos = &pTos[-1];` |
|    49934 |  4489 | `	}else{` |
|    27054 |  4490 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4491 | `	}` |
|        - |  4492 | `#ifdef UNTRUST` |
|        - |  4493 | `	if( pNos < pStack ){` |
|        - |  4494 | `		goto Abort;` |
|        - |  4495 | `	}` |
|        - |  4496 | `#endif` |
|        - |  4497 | `	/* Force a string cast */` |
|   126918 |  4498 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1348 |  4499 | `		PH7_MemObjToString(pNos);` |
|      673 |  4500 | `	}` |
|   126918 |  4501 | `	pCur = &pNos[1];` |
|   255886 |  4502 | `	while( pCur <= pTos ){` |
|   128970 |  4503 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50674 |  4504 | `			PH7_MemObjToString(pCur);` |
|    25336 |  4505 | `		}` |
|        - |  4506 | `		/* Perform the concatenation */` |
|   128970 |  4507 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   128932 |  4508 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64465 |  4509 | `		}` |
|   128970 |  4510 | `		SyBlobRelease(&pCur->sBlob);` |
|   128970 |  4511 | `		pCur++;` |
|        2 |  4512 | `	}` |
|   126918 |  4513 | `	pTos = pNos;` |
|   126918 |  4514 | `	break;` |
|        - |  4515 | `				}` |
|        - |  4516 | `/*  CAT_STORE: * * *` |
|        - |  4517 | ` *` |
|        - |  4518 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4519 | ` * back.` |
|        - |  4520 | ` */` |
|     3386 |  4521 | `case PH7_OP_CAT_STORE:{` |
|     6774 |  4522 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4523 | `	ph7_value *pObj;` |
|        - |  4524 | `#ifdef UNTRUST` |
|        - |  4525 | `	if( pNos < pStack ){` |
|        - |  4526 | `		goto Abort;` |
|        - |  4527 | `	}` |
|        - |  4528 | `#endif` |
|     6774 |  4529 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4530 | `		/* Force a string cast */` |
|      ! 0 |  4531 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4532 | `	}` |
|     6774 |  4533 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4534 | `		/* Force a string cast */` |
|      ! 0 |  4535 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4536 | `	}` |
|        - |  4537 | `	/* Perform the concatenation (Reverse order) */` |
|     6774 |  4538 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6774 |  4539 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3386 |  4540 | `	}` |
|        - |  4541 | `	/* Perform the store operation */` |
|     6774 |  4542 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4543 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6774 |  4544 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6774 |  4545 | `		PH7_MemObjStore(pTos,pObj);` |
|     3386 |  4546 | `	}` |
|     6774 |  4547 | `	PH7_MemObjStore(pTos,pNos);` |
|     6774 |  4548 | `	VmPopOperand(&pTos,1);` |
|     6774 |  4549 | `	break;` |
|        - |  4550 | `				}` |
|        - |  4551 | `/* OP_AND: * * *` |
|        - |  4552 | ` *` |
|        - |  4553 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4554 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4555 | ` * stack.` |
|        - |  4556 | ` */` |
|        - |  4557 | `/* OP_OR: * * *` |
|        - |  4558 | ` *` |
|        - |  4559 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4560 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4561 | ` * stack.` |
|        - |  4562 | ` */` |
|    94873 |  4563 | `case PH7_OP_LAND:` |
|        - |  4564 | `case PH7_OP_LOR: {` |
|   189792 |  4565 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4566 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4567 | `#ifdef UNTRUST` |
|        - |  4568 | `	if( pNos < pStack ){` |
|        - |  4569 | `		goto Abort;` |
|        - |  4570 | `	}` |
|        - |  4571 | `#endif` |
|        - |  4572 | `	/* Force a boolean cast */` |
|   189792 |  4573 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4574 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4575 | `	}` |
|   189792 |  4576 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4577 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4578 | `	}` |
|   189792 |  4579 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189792 |  4580 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189792 |  4581 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4582 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    87294 |  4583 | `		v1 = and_logic[v1*3+v2];` |
|    43670 |  4584 | `	}else{` |
|        - |  4585 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102500 |  4586 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4587 | `	}` |
|   189792 |  4588 | `	if( v1 == 2 ){` |
|      ! 0 |  4589 | `		v1 = 1;` |
|      ! 0 |  4590 | `	}` |
|   189792 |  4591 | `	VmPopOperand(&pTos,1);` |
|   189792 |  4592 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189792 |  4593 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189792 |  4594 | `	break;` |
|        - |  4595 | `				 }` |
|        - |  4596 | `/*` |
|        - |  4597 | ` * OP_NULLC: * * *` |
|        - |  4598 | ` * Null coalescing operator '??'.` |
|        - |  4599 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4600 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4601 | ` */` |
|        - |  4602 | `/*` |
|        - |  4603 | ` * OP_NULLC: * P2 *` |
|        - |  4604 | ` * Short-circuit null coalescing '??'.` |
|        - |  4605 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4606 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4607 | ` */` |
|       19 |  4608 | `case PH7_OP_NULLC: {` |
|        - |  4609 | `#ifdef UNTRUST` |
|        - |  4610 | `	if( pTos < pStack ){` |
|        - |  4611 | `		goto Abort;` |
|        - |  4612 | `	}` |
|        - |  4613 | `#endif` |
|       40 |  4614 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4615 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4616 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4617 | `	}else{` |
|        - |  4618 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4619 | `		VmPopOperand(&pTos, 1);` |
|        - |  4620 | `	}` |
|       40 |  4621 | `	break;` |
|        - |  4622 |  |
|        - |  4623 | `/*` |
|        - |  4624 | ` * OP_SPREAD: * * *` |
|        - |  4625 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4626 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4627 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4628 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4629 | ` */` |
|        7 |  4630 | `case PH7_OP_SPREAD: {` |
|        - |  4631 | `#ifdef UNTRUST` |
|        - |  4632 | `	if( pTos < pStack ){` |
|        - |  4633 | `		goto Abort;` |
|        - |  4634 | `	}` |
|        - |  4635 | `#endif` |
|       15 |  4636 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4637 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4638 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4639 | `		if( nEntry == 0 ){` |
|        - |  4640 | `			/* Empty array — remove from stack */` |
|        3 |  4641 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4642 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4643 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4644 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4645 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4646 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4647 | `				VM_STACK_GUARD);` |
|      ! 0 |  4648 | `		}else{` |
|        - |  4649 | `			ph7_hashmap_node *pNode2;` |
|        - |  4650 | `			ph7_value *pElem;` |
|        - |  4651 | `			sxu32 i;` |
|        - |  4652 | `			/* Overwrite TOS with first element */` |
|       13 |  4653 | `			pNode2 = pMap->pFirst;` |
|       13 |  4654 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4655 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4656 | `			if( pElem ){` |
|       13 |  4657 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4658 | `			}` |
|       13 |  4659 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4660 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4661 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4662 | `			pNode2 = pNode2->pPrev;` |
|        - |  4663 | `			/* Push remaining elements */` |
|       33 |  4664 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4665 | `				pTos++;` |
|       21 |  4666 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4667 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4668 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4669 | `				if( pElem ){` |
|       21 |  4670 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4671 | `				}` |
|       21 |  4672 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4673 | `			}` |
|       13 |  4674 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4675 | `		}` |
|        7 |  4676 | `	}` |
|        - |  4677 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4678 | `	break;` |
|        - |  4679 |  |
|        - |  4680 | `/* OP_LXOR: * * *` |
|        - |  4681 | ` *` |
|        - |  4682 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4683 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4684 | ` * stack.` |
|        - |  4685 | ` * According to the PHP language reference manual:` |
|        - |  4686 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4687 | ` *  TRUE,but not both.` |
|        - |  4688 | ` */` |
|        5 |  4689 | `case PH7_OP_LXOR:{` |
|       11 |  4690 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4691 | `	sxi32 v = 0;` |
|        - |  4692 | `#ifdef UNTRUST` |
|        - |  4693 | `	if( pNos < pStack ){` |
|        - |  4694 | `		goto Abort;` |
|        - |  4695 | `	}` |
|        - |  4696 | `#endif` |
|        - |  4697 | `	/* Force a boolean cast */` |
|       11 |  4698 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4699 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4700 | `	}` |
|       11 |  4701 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4702 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4703 | `	}` |
|       11 |  4704 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4705 | `		v = 1;` |
|        3 |  4706 | `	}` |
|       11 |  4707 | `	VmPopOperand(&pTos,1);` |
|       11 |  4708 | `	pTos->x.iVal = v;` |
|       11 |  4709 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4710 | `	break;` |
|        - |  4711 | `				 }` |
|        - |  4712 | `/* OP_EQ P1 P2 P3` |
|        - |  4713 | ` *` |
|        - |  4714 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4715 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4716 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4717 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4718 | ` */` |
|        - |  4719 | `/* OP_NEQ P1 P2 P3` |
|        - |  4720 | ` *` |
|        - |  4721 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4722 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4723 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4724 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4725 | ` */` |
|     3986 |  4726 | `case PH7_OP_EQ:` |
|        - |  4727 | `case PH7_OP_NEQ: {` |
|     7974 |  4728 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4729 | `	/* Perform the comparison and act accordingly */` |
|        - |  4730 | `#ifdef UNTRUST` |
|        - |  4731 | `	if( pNos < pStack ){` |
|        - |  4732 | `		goto Abort;` |
|        - |  4733 | `	}` |
|        - |  4734 | `#endif` |
|     7974 |  4735 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7974 |  4736 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4737 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7965 |  4738 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7930 |  4739 | `		rc = rc == 0;` |
|     3966 |  4740 | `	}else{` |
|       28 |  4741 | `		rc = rc != 0;` |
|        - |  4742 | `	}` |
|     7974 |  4743 | `	VmPopOperand(&pTos,1);` |
|     7974 |  4744 | `	if( !pInstr->iP2 ){` |
|        - |  4745 | `		/* Push comparison result without taking the jump */` |
|     7974 |  4746 | `		PH7_MemObjRelease(pTos);` |
|     7974 |  4747 | `		pTos->x.iVal = rc;` |
|        - |  4748 | `		/* Invalidate any prior representation */` |
|     7974 |  4749 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3988 |  4750 | `	}else{` |
|      ! 0 |  4751 | `		if( rc ){` |
|        - |  4752 | `			/* Jump to the desired location */` |
|      ! 0 |  4753 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4754 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4755 | `		}` |
|        - |  4756 | `	}` |
|     7974 |  4757 | `	break;` |
|        - |  4758 | `				 }` |
|        - |  4759 | `/* OP_TEQ P1 P2 *` |
|        - |  4760 | ` *` |
|        - |  4761 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4762 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4763 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4764 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4765 | ` */` |
|   133755 |  4766 | `case PH7_OP_TEQ: {` |
|   267512 |  4767 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4768 | `	/* Perform the comparison and act accordingly */` |
|        - |  4769 | `#ifdef UNTRUST` |
|        - |  4770 | `	if( pNos < pStack ){` |
|        - |  4771 | `		goto Abort;` |
|        - |  4772 | `	}` |
|        - |  4773 | `#endif` |
|   267512 |  4774 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   267512 |  4775 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4776 | `		rc = 0;` |
|        2 |  4777 | `	}else{` |
|   267510 |  4778 | `		rc = rc == 0;` |
|        - |  4779 | `	}` |
|   267512 |  4780 | `	VmPopOperand(&pTos,1);` |
|   267512 |  4781 | `	if( !pInstr->iP2 ){` |
|        - |  4782 | `		/* Push comparison result without taking the jump */` |
|   267512 |  4783 | `		PH7_MemObjRelease(pTos);` |
|   267512 |  4784 | `		pTos->x.iVal = rc;` |
|        - |  4785 | `		/* Invalidate any prior representation */` |
|   267512 |  4786 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   133757 |  4787 | `	}else{` |
|      ! 0 |  4788 | `		if( rc ){` |
|        - |  4789 | `			/* Jump to the desired location */` |
|      ! 0 |  4790 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4791 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4792 | `		}` |
|        - |  4793 | `	}` |
|   267512 |  4794 | `	break;` |
|        - |  4795 | `				 }` |
|        - |  4796 | `/* OP_TNE P1 P2 *` |
|        - |  4797 | ` *` |
|        - |  4798 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4799 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4800 | ` * instruction.` |
|        - |  4801 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4802 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4803 | ` *` |
|        - |  4804 | ` */` |
|   104370 |  4805 | `case PH7_OP_TNE: {` |
|   208742 |  4806 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4807 | `	/* Perform the comparison and act accordingly */` |
|        - |  4808 | `#ifdef UNTRUST` |
|        - |  4809 | `	if( pNos < pStack ){` |
|        - |  4810 | `		goto Abort;` |
|        - |  4811 | `	}` |
|        - |  4812 | `#endif` |
|   208742 |  4813 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   208742 |  4814 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4815 | `		rc = 1;` |
|        2 |  4816 | `	}else{` |
|   208740 |  4817 | `		rc = rc != 0;` |
|        - |  4818 | `	}` |
|   208742 |  4819 | `	VmPopOperand(&pTos,1);` |
|   208742 |  4820 | `	if( !pInstr->iP2 ){` |
|        - |  4821 | `		/* Push comparison result without taking the jump */` |
|   208742 |  4822 | `		PH7_MemObjRelease(pTos);` |
|   208742 |  4823 | `		pTos->x.iVal = rc;` |
|        - |  4824 | `		/* Invalidate any prior representation */` |
|   208742 |  4825 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   104372 |  4826 | `	}else{` |
|      ! 0 |  4827 | `		if( rc ){` |
|        - |  4828 | `			/* Jump to the desired location */` |
|      ! 0 |  4829 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4830 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4831 | `		}` |
|        - |  4832 | `	}` |
|   208742 |  4833 | `	break;` |
|        - |  4834 | `				 }` |
|        - |  4835 | `/* OP_LT P1 P2 P3` |
|        - |  4836 | ` *` |
|        - |  4837 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4838 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4839 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4840 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4841 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4842 | ` *` |
|        - |  4843 | ` */` |
|        - |  4844 | `/* OP_LE P1 P2 P3` |
|        - |  4845 | ` *` |
|        - |  4846 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4847 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4848 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4849 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4850 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4851 | ` *` |
|        - |  4852 | ` */` |
|   102455 |  4853 | `case PH7_OP_LT:` |
|        - |  4854 | `case PH7_OP_LE: {` |
|   204956 |  4855 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4856 | `	/* Perform the comparison and act accordingly */` |
|        - |  4857 | `#ifdef UNTRUST` |
|        - |  4858 | `	if( pNos < pStack ){` |
|        - |  4859 | `		goto Abort;` |
|        - |  4860 | `	}` |
|        - |  4861 | `#endif` |
|   204956 |  4862 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204956 |  4863 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4864 | `		rc = 0;` |
|   204952 |  4865 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      432 |  4866 | `		rc = rc < 1;` |
|      217 |  4867 | `	}else{` |
|   204518 |  4868 | `		rc = rc < 0;` |
|        - |  4869 | `	}` |
|   204956 |  4870 | `	VmPopOperand(&pTos,1);` |
|   204956 |  4871 | `	if( !pInstr->iP2 ){` |
|        - |  4872 | `		/* Push comparison result without taking the jump */` |
|   204956 |  4873 | `		PH7_MemObjRelease(pTos);` |
|   204956 |  4874 | `		pTos->x.iVal = rc;` |
|        - |  4875 | `		/* Invalidate any prior representation */` |
|   204956 |  4876 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102501 |  4877 | `	}else{` |
|      ! 0 |  4878 | `		if( rc ){` |
|        - |  4879 | `			/* Jump to the desired location */` |
|      ! 0 |  4880 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4881 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4882 | `		}` |
|        - |  4883 | `	}` |
|   204956 |  4884 | `	break;` |
|        - |  4885 | `				}` |
|        - |  4886 | `/* OP_GT P1 P2 P3` |
|        - |  4887 | ` *` |
|        - |  4888 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4889 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4890 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4891 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4892 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4893 | ` *` |
|        - |  4894 | ` */` |
|        - |  4895 | `/* OP_GE P1 P2 P3` |
|        - |  4896 | ` *` |
|        - |  4897 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4898 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4899 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4900 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4901 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4902 | ` *` |
|        - |  4903 | ` */` |
|    48773 |  4904 | `case PH7_OP_GT:` |
|        - |  4905 | `case PH7_OP_GE: {` |
|    97548 |  4906 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4907 | `	/* Perform the comparison and act accordingly */` |
|        - |  4908 | `#ifdef UNTRUST` |
|        - |  4909 | `	if( pNos < pStack ){` |
|        - |  4910 | `		goto Abort;` |
|        - |  4911 | `	}` |
|        - |  4912 | `#endif` |
|    97548 |  4913 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97548 |  4914 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4915 | `		rc = 0;` |
|    97544 |  4916 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97390 |  4917 | `		rc = rc >= 0;` |
|    48696 |  4918 | `	}else{` |
|      152 |  4919 | `		rc = rc > 0;` |
|        - |  4920 | `	}` |
|    97548 |  4921 | `	VmPopOperand(&pTos,1);` |
|    97548 |  4922 | `	if( !pInstr->iP2 ){` |
|        - |  4923 | `		/* Push comparison result without taking the jump */` |
|    97548 |  4924 | `		PH7_MemObjRelease(pTos);` |
|    97548 |  4925 | `		pTos->x.iVal = rc;` |
|        - |  4926 | `		/* Invalidate any prior representation */` |
|    97548 |  4927 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48775 |  4928 | `	}else{` |
|      ! 0 |  4929 | `		if( rc ){` |
|        - |  4930 | `			/* Jump to the desired location */` |
|      ! 0 |  4931 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4932 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4933 | `		}` |
|        - |  4934 | `	}` |
|    97548 |  4935 | `	break;` |
|        - |  4936 | `				}` |
|        - |  4937 | `/* OP_SPACESHIP * * *` |
|        - |  4938 | ` *` |
|        - |  4939 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  4940 | ` *   -1 if left < right` |
|        - |  4941 | ` *    0 if left == right` |
|        - |  4942 | ` *    1 if left > right` |
|        - |  4943 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  4944 | ` */` |
|       25 |  4945 | `case PH7_OP_SPACESHIP: {` |
|       51 |  4946 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4947 | `#ifdef UNTRUST` |
|        - |  4948 | `	if( pNos < pStack ){` |
|        - |  4949 | `		goto Abort;` |
|        - |  4950 | `	}` |
|        - |  4951 | `#endif` |
|       51 |  4952 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  4953 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  4954 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  4955 | `		rc = 1;` |
|        4 |  4956 | `	}else{` |
|        - |  4957 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  4958 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  4959 | `	}` |
|       51 |  4960 | `	VmPopOperand(&pTos,1);` |
|       51 |  4961 | `	PH7_MemObjRelease(pTos);` |
|       51 |  4962 | `	pTos->x.iVal = rc;` |
|       51 |  4963 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  4964 | `	break;` |
|        - |  4965 | `				}` |
|        - |  4966 | `/* OP_SEQ P1 P2 *` |
|        - |  4967 | ` * Strict string comparison.` |
|        - |  4968 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4969 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4970 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4971 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4972 | ` * use PH7_OP_EQ.` |
|        - |  4973 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4974 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4975 | ` */` |
|        - |  4976 | `/* OP_SNE P1 P2 *` |
|        - |  4977 | ` * Strict string comparison.` |
|        - |  4978 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4979 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4980 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4981 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4982 | ` * use PH7_OP_EQ.` |
|        - |  4983 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4984 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4985 | ` */` |
|       18 |  4986 | `case PH7_OP_SEQ:` |
|        - |  4987 | `case PH7_OP_SNE: {` |
|       38 |  4988 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4989 | `	SyString s1,s2;` |
|        - |  4990 | `	/* Perform the comparison and act accordingly */` |
|        - |  4991 | `#ifdef UNTRUST` |
|        - |  4992 | `	if( pNos < pStack ){` |
|        - |  4993 | `		goto Abort;` |
|        - |  4994 | `	}` |
|        - |  4995 | `#endif` |
|        - |  4996 | `	/* Force a string cast */` |
|       38 |  4997 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4998 | `		PH7_MemObjToString(pTos);` |
|        2 |  4999 | `	}` |
|       38 |  5000 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5001 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5002 | `	}` |
|       38 |  5003 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5004 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5005 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5006 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5007 | `		rc = rc != 0;` |
|      ! 0 |  5008 | `	}else{` |
|       38 |  5009 | `		rc = rc == 0;` |
|        - |  5010 | `	}` |
|       38 |  5011 | `	VmPopOperand(&pTos,1);` |
|       38 |  5012 | `	if( !pInstr->iP2 ){` |
|        - |  5013 | `		/* Push comparison result without taking the jump */` |
|       38 |  5014 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5015 | `		pTos->x.iVal = rc;` |
|        - |  5016 | `		/* Invalidate any prior representation */` |
|       38 |  5017 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5018 | `	}else{` |
|      ! 0 |  5019 | `		if( rc ){` |
|        - |  5020 | `			/* Jump to the desired location */` |
|      ! 0 |  5021 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5022 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5023 | `		}` |
|        - |  5024 | `	}` |
|       38 |  5025 | `	break;` |
|        - |  5026 | `				 }` |
|        - |  5027 | `/*` |
|        - |  5028 | ` * OP_LOAD_REF * * *` |
|        - |  5029 | ` * Push the index of a referenced object on the stack.` |
|        - |  5030 | ` */` |
|       57 |  5031 | `case PH7_OP_LOAD_REF: {` |
|        - |  5032 | `	sxu32 nIdx;` |
|        - |  5033 | `#ifdef UNTRUST` |
|        - |  5034 | `	if( pTos < pStack ){` |
|        - |  5035 | `		goto Abort;` |
|        - |  5036 | `	}` |
|        - |  5037 | `#endif` |
|        - |  5038 | `	/* Extract memory object index */` |
|      115 |  5039 | `	nIdx = pTos->nIdx;` |
|      115 |  5040 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5041 | `		/* Nullify the object */` |
|       95 |  5042 | `		PH7_MemObjRelease(pTos);` |
|        - |  5043 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5044 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5045 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5046 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5047 | `	}` |
|      115 |  5048 | `	break;` |
|        - |  5049 | `					  }` |
|        - |  5050 | `/*` |
|        - |  5051 | ` * OP_STORE_REF * * P3` |
|        - |  5052 | ` * Perform an assignment operation by reference.` |
|        - |  5053 | ` */` |
|       15 |  5054 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5055 | `	 SyString sName = { 0 , 0 };` |
|        - |  5056 | `	 VmFrame *pFrameLocal;` |
|        - |  5057 | `	SyHashEntry *pEntry;` |
|        - |  5058 | `	sxu32 nIdx;` |
|        - |  5059 | `#ifdef UNTRUST` |
|        - |  5060 | `	if( pTos < pStack ){` |
|        - |  5061 | `		goto Abort;` |
|        - |  5062 | `	}` |
|        - |  5063 | `#endif` |
|       32 |  5064 | `	if( pInstr->p3 == 0 ){` |
|        - |  5065 | `		char *zName;` |
|        - |  5066 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5067 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5068 | `			/* Force a string cast */` |
|      ! 0 |  5069 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5070 | `		}` |
|      ! 0 |  5071 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5072 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5073 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5074 | `			if( zName ){` |
|      ! 0 |  5075 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5076 | `			}` |
|      ! 0 |  5077 | `		}` |
|      ! 0 |  5078 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5079 | `		pTos--;` |
|      ! 0 |  5080 | `	}else{` |
|       32 |  5081 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5082 | `	}` |
|       32 |  5083 | `	nIdx = pTos->nIdx;` |
|       32 |  5084 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5085 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5086 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5087 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5088 | `		}else{` |
|        - |  5089 | `			ph7_value *pObj;` |
|        - |  5090 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5091 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5092 | `			if( pObj == 0 ){` |
|      ! 0 |  5093 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5094 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5095 | `				goto Abort;` |
|        - |  5096 | `			}` |
|        - |  5097 | `			/* Perform the store operation */` |
|      ! 0 |  5098 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5099 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5100 | `		}` |
|       32 |  5101 | `	}else if( sName.nByte > 0){` |
|       32 |  5102 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5103 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5104 | `		}else{` |
|       32 |  5105 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5106 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5107 | `			/* Query the local frame */` |
|       32 |  5108 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5109 | `			if( pEntry ){` |
|      ! 0 |  5110 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5111 | `			}else{` |
|       32 |  5112 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5113 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5114 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5115 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5116 | `				}` |
|       32 |  5117 | `				if( rc == SXRET_OK ){` |
|       32 |  5118 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5119 | `				}` |
|        - |  5120 | `			}` |
|        - |  5121 | `		}` |
|       15 |  5122 | `	}` |
|       32 |  5123 | `	break;` |
|        - |  5124 | `				 }` |
|        - |  5125 | `/*` |
|        - |  5126 | ` * OP_UPLINK P1 * *` |
|        - |  5127 | ` * Link a variable to the top active VM frame.` |
|        - |  5128 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5129 | ` */` |
|       25 |  5130 | `case PH7_OP_UPLINK: {` |
|       52 |  5131 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5132 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5133 | `		SyString sName;` |
|        - |  5134 | `		/* Perform the link */` |
|      104 |  5135 | `		while( pLink <= pTos ){` |
|       54 |  5136 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5137 | `				/* Force a string cast */` |
|      ! 0 |  5138 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5139 | `			}` |
|       54 |  5140 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5141 | `			if( sName.nByte > 0 ){` |
|       54 |  5142 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5143 | `			}` |
|       54 |  5144 | `			pLink++;` |
|        2 |  5145 | `		}` |
|       25 |  5146 | `	}` |
|       52 |  5147 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5148 | `	break;` |
|        - |  5149 | `					}` |
|        - |  5150 | `/*` |
|        - |  5151 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5152 | ` * Push an exception in the corresponding container so that` |
|        - |  5153 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5154 | ` */` |
|       32 |  5155 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5156 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5157 | `	VmFrame *pFrameLocal;` |
|        - |  5158 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5159 | `	pException->iFinallyDone = 0;` |
|       66 |  5160 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5161 | `	/* Create the exception frame */` |
|       66 |  5162 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5163 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5164 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5165 | `		goto Abort;` |
|        - |  5166 | `	}` |
|        - |  5167 | `	/* Mark the special frame */` |
|       66 |  5168 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5169 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5170 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5171 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5172 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5173 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5174 | `	break;` |
|        - |  5175 | `							}` |
|        - |  5176 | `/*` |
|        - |  5177 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5178 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5179 | ` */` |
|       31 |  5180 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5181 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5182 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5183 | `		ph7_exception **apException;` |
|        - |  5184 | `		/* Pop the loaded exception */` |
|       28 |  5185 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5186 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5187 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5188 | `		}` |
|       13 |  5189 | `	}` |
|       64 |  5190 | `	pException->pFrame = 0;` |
|        - |  5191 | `	/* Leave the exception frame */` |
|       64 |  5192 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5193 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5194 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5195 | `		sxi32 rcFinally;` |
|       20 |  5196 | `		pException->iFinallyDone = 1;` |
|       20 |  5197 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5198 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5199 | `			goto Abort;` |
|        - |  5200 | `		}` |
|        9 |  5201 | `	}` |
|       64 |  5202 | `	break;` |
|        - |  5203 | `							}` |
|        - |  5204 |  |
|        - |  5205 | `/*` |
|        - |  5206 | ` * OP_THROW * P2 *` |
|        - |  5207 | ` * Throw an user exception.` |
|        - |  5208 | ` */` |
|       18 |  5209 | `case PH7_OP_THROW: {` |
|       38 |  5210 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5211 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5212 | `#ifdef UNTRUST` |
|        - |  5213 | `	if( pTos < pStack ){` |
|        - |  5214 | `		goto Abort;` |
|        - |  5215 | `	}` |
|        - |  5216 | `#endif` |
|       38 |  5217 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5218 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5219 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5220 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5221 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5222 | `		ph7_class *pException;` |
|        - |  5223 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5224 | `		 */` |
|       38 |  5225 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5226 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5227 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5228 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5229 | `			if( rc == SXERR_ABORT ){` |
|        - |  5230 | `				/* Abort processing immediately */` |
|      ! 0 |  5231 | `				goto Abort;` |
|        - |  5232 | `			}` |
|      ! 0 |  5233 | `		}else{` |
|        - |  5234 | `			/* Throw the exception */` |
|       38 |  5235 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5236 | `			if( rc == SXERR_ABORT ){` |
|        - |  5237 | `				/* Abort processing immediately */` |
|        9 |  5238 | `				goto Abort;` |
|        - |  5239 | `			}` |
|        - |  5240 | `		}` |
|       16 |  5241 | `	}else{` |
|        - |  5242 | `		/* Expecting a class instance */` |
|      ! 0 |  5243 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5244 | `		if( rc == SXERR_ABORT ){` |
|        - |  5245 | `			/* Abort processing immediately */` |
|      ! 0 |  5246 | `			goto Abort;` |
|        - |  5247 | `		}` |
|        - |  5248 | `	}` |
|        - |  5249 | `	/* Pop the top entry */` |
|       30 |  5250 | `	VmPopOperand(&pTos,1);` |
|        - |  5251 | `	/* Perform an unconditional jump */` |
|       30 |  5252 | `	pc = nJump - 1;` |
|       30 |  5253 | `	break;` |
|        - |  5254 | `				   }` |
|        - |  5255 | `/*` |
|        - |  5256 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5257 | ` * Prepare a foreach step.` |
|        - |  5258 | ` */` |
|     5063 |  5259 | `case PH7_OP_FOREACH_INIT: {` |
|    10128 |  5260 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5261 | `	void *pName;` |
|        - |  5262 | `#ifdef UNTRUST` |
|        - |  5263 | `	if( pTos < pStack ){` |
|        - |  5264 | `		goto Abort;` |
|        - |  5265 | `	}` |
|        - |  5266 | `#endif` |
|    10128 |  5267 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5268 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5269 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5270 | `			/* Force a string cast */` |
|      ! 0 |  5271 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5272 | `		}` |
|        - |  5273 | `		/* Duplicate name */` |
|      ! 0 |  5274 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5275 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5276 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5277 | `		}` |
|      ! 0 |  5278 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5279 | `	}` |
|    10128 |  5280 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5281 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5282 | `			/* Force a string cast */` |
|      ! 0 |  5283 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5284 | `		}` |
|        - |  5285 | `		/* Duplicate name */` |
|      ! 0 |  5286 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5287 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5288 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5289 | `		}` |
|      ! 0 |  5290 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5291 | `	}` |
|        - |  5292 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10128 |  5293 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5294 | `		/* Jump out of the loop */` |
|      ! 0 |  5295 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5296 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5297 | `		}` |
|      ! 0 |  5298 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5299 | `	}else{` |
|        - |  5300 | `		ph7_foreach_step *pStep;` |
|    10128 |  5301 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10128 |  5302 | `		if( pStep == 0 ){` |
|      ! 0 |  5303 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5304 | `			/* Jump out of the loop */` |
|      ! 0 |  5305 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5306 | `		}else{` |
|        - |  5307 | `			/* Zero the structure */` |
|    10128 |  5308 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5309 | `			/* Prepare the step */` |
|    10128 |  5310 | `			pStep->iFlags = pInfo->iFlags;` |
|    10128 |  5311 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5312 | `				ph7_hashmap *pMap;` |
|        - |  5313 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5314 | `				 * source array so mutations don't affect other sharers. */` |
|    10100 |  5315 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5316 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5317 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5318 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5319 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5320 | `						 * variable still points at the same hashmap as` |
|        - |  5321 | `						 * the stack value. */` |
|        9 |  5322 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5323 | `							pCur->iRef--;` |
|        9 |  5324 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5325 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5326 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5327 | `						}` |
|        4 |  5328 | `					}` |
|        4 |  5329 | `				}` |
|    10100 |  5330 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5331 | `				/* Reset the internal loop cursor */` |
|    10100 |  5332 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5333 | `				/* Mark the step */` |
|    10100 |  5334 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10100 |  5335 | `				pStep->xIter.pMap = pMap;` |
|    10100 |  5336 | `				pMap->iRef++;` |
|     5051 |  5337 | `			}else{` |
|       30 |  5338 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5339 | `				ph7_class *pIteratorClass;` |
|        - |  5340 | `				/* Check if the object implements Iterator */` |
|       30 |  5341 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5342 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5343 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5344 | `					ph7_class_method *pRewind;` |
|       20 |  5345 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5346 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5347 | `					pThis->iRef++;` |
|       20 |  5348 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5349 | `					if( pRewind ){` |
|       20 |  5350 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5351 | `					}` |
|       11 |  5352 | `				}else{` |
|        - |  5353 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5354 | `					ph7_class *pIterAggClass;` |
|       12 |  5355 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5356 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5357 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5358 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5359 | `						ph7_class_method *pGetIter;` |
|        3 |  5360 | `						int iterAggOk = 0;` |
|        3 |  5361 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5362 | `						if( pGetIter ){` |
|        - |  5363 | `							ph7_value sResult;` |
|        3 |  5364 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5365 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5366 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5367 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5368 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5369 | `									ph7_class_method *pRewind;` |
|        3 |  5370 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5371 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5372 | `									pIterObj->iRef++;` |
|        - |  5373 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5374 | `									pStep->pOwner = pThis;` |
|        3 |  5375 | `									pThis->iRef++;` |
|        3 |  5376 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5377 | `									if( pRewind ){` |
|        3 |  5378 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5379 | `									}` |
|        3 |  5380 | `									iterAggOk = 1;` |
|        1 |  5381 | `								}` |
|        1 |  5382 | `							}` |
|        3 |  5383 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5384 | `						}` |
|        3 |  5385 | `						if( !iterAggOk ){` |
|        - |  5386 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5387 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5388 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5389 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5390 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5391 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5392 | `						}` |
|        2 |  5393 | `					}else{` |
|        - |  5394 | `						/* Plain object iteration via hAttr */` |
|        9 |  5395 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5396 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5397 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5398 | `						pThis->iRef++;` |
|        - |  5399 | `					}` |
|        - |  5400 | `				}` |
|        - |  5401 | `			}` |
|        - |  5402 | `		}` |
|    10128 |  5403 | `		if( pStep ){` |
|    10128 |  5404 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5405 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5406 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5407 | `				/* Jump out of the loop */` |
|      ! 0 |  5408 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5409 | `			}` |
|     5063 |  5410 | `		}` |
|        - |  5411 | `	}` |
|    10128 |  5412 | `	VmPopOperand(&pTos,1);` |
|    10128 |  5413 | `	break;` |
|        - |  5414 | `						  }` |
|        - |  5415 | `/*` |
|        - |  5416 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5417 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5418 | ` */` |
|    82036 |  5419 | `case PH7_OP_FOREACH_STEP: {` |
|   164074 |  5420 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5421 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5422 | `	ph7_value *pValue;` |
|        - |  5423 | `	VmFrame *pFrameLocal;` |
|        - |  5424 | `	/* Peek the last step */` |
|   164074 |  5425 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   164074 |  5426 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   164074 |  5427 | `	pFrameLocal = pVm->pFrame;` |
|   164074 |  5428 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   164074 |  5429 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   163962 |  5430 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5431 | `		ph7_hashmap_node *pNode;` |
|        - |  5432 | `		/* Extract the current node value */` |
|   163962 |  5433 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   163962 |  5434 | `		if( pNode == 0 ){` |
|        - |  5435 | `			/* No more entry to process */` |
|    10098 |  5436 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10098 |  5437 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5438 | `				/* Break the reference with the last element */` |
|        7 |  5439 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5440 | `			}` |
|        - |  5441 | `			/* Automatically reset the loop cursor */` |
|    10098 |  5442 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5443 | `			/* Cleanup the mess left behind */` |
|    10098 |  5444 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10098 |  5445 | `			SySetPop(&pInfo->aStep);` |
|    10098 |  5446 | `			PH7_HashmapUnref(pMap);` |
|     5050 |  5447 | `		}else{` |
|   153866 |  5448 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5449 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5450 | `				if( pKey ){` |
|      416 |  5451 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5452 | `				}` |
|      207 |  5453 | `			}` |
|   153866 |  5454 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5455 | `				SyHashEntry *pEntry;` |
|        - |  5456 | `				/* Pass by reference */` |
|       23 |  5457 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5458 | `				if( pEntry ){` |
|       23 |  5459 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5460 | `				}else{` |
|      ! 0 |  5461 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5462 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5463 | `				}` |
|       12 |  5464 | `			}else{` |
|        - |  5465 | `				/* Make a copy of the entry value */` |
|   153844 |  5466 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   153844 |  5467 | `				if( pValue ){` |
|   153844 |  5468 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    76921 |  5469 | `				}` |
|        - |  5470 | `			}` |
|        2 |  5471 | `		}` |
|    82094 |  5472 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5473 | `		/* Iterator-based iteration.` |
|        - |  5474 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5475 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5476 | `		 */` |
|       90 |  5477 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5478 | `		ph7_class_method *pMethod;` |
|        - |  5479 | `		ph7_value sResult;` |
|       90 |  5480 | `		int isValid = 0;` |
|        - |  5481 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5482 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5483 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5484 | `		}else{` |
|       70 |  5485 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5486 | `			if( pMethod ){` |
|       70 |  5487 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5488 | `			}` |
|        - |  5489 | `		}` |
|        - |  5490 | `		/* Call valid() */` |
|       90 |  5491 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5492 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5493 | `		if( pMethod ){` |
|       90 |  5494 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5495 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5496 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5497 | `		}` |
|       90 |  5498 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5499 | `		if( !isValid ){` |
|        - |  5500 | `			/* Iterator exhausted */` |
|       20 |  5501 | `			pc = pInstr->iP2 - 1;` |
|        - |  5502 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5503 | `			if( pStep->pOwner ){` |
|        3 |  5504 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5505 | `			}` |
|       20 |  5506 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5507 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5508 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5509 | `		}else{` |
|        - |  5510 | `			/* Call current() to get value */` |
|       72 |  5511 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5512 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5513 | `			if( pMethod ){` |
|       72 |  5514 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5515 | `			}` |
|       72 |  5516 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5517 | `			if( pValue ){` |
|       72 |  5518 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5519 | `			}` |
|       72 |  5520 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5521 | `			/* Call key() if needed */` |
|       72 |  5522 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5523 | `				ph7_value sKey;` |
|       35 |  5524 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5525 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5526 | `				if( pMethod ){` |
|       35 |  5527 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5528 | `				}` |
|       35 |  5529 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5530 | `				if( pValue ){` |
|       35 |  5531 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5532 | `				}` |
|       35 |  5533 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5534 | `			}` |
|        - |  5535 | `		}` |
|       46 |  5536 | `	}else{` |
|       25 |  5537 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5538 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5539 | `		SyHashEntry *pEntry;` |
|        - |  5540 | `		/* Point to the next attribute */` |
|       29 |  5541 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5542 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5543 | `			/* Check access permission */` |
|       31 |  5544 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5545 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5546 | `					break; /* Access is granted */` |
|        - |  5547 | `			}` |
|        1 |  5548 | `		}` |
|       25 |  5549 | `		if( pEntry == 0 ){` |
|        - |  5550 | `			/* Clean up the mess left behind */` |
|        9 |  5551 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5552 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5553 | `				/* Break the reference with the last element */` |
|        3 |  5554 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5555 | `			}` |
|        9 |  5556 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5557 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5558 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5559 | `		}else{` |
|       17 |  5560 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5561 | `			ph7_value *pAttrValue;` |
|       17 |  5562 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5563 | `				/* Fill with the current attribute name */` |
|       17 |  5564 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5565 | `				if( pKey ){` |
|       17 |  5566 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5567 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5568 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5569 | `				}` |
|        8 |  5570 | `			}` |
|        - |  5571 | `			/* Extract attribute value */` |
|       17 |  5572 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5573 | `			if( pAttrValue ){` |
|       17 |  5574 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5575 | `					/* Pass by reference */` |
|        3 |  5576 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5577 | `					if( pEntry ){` |
|        3 |  5578 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5579 | `					}else{` |
|      ! 0 |  5580 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5581 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5582 | `					}` |
|        2 |  5583 | `				}else{` |
|        - |  5584 | `					/* Make a copy of the attribute value */` |
|       15 |  5585 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5586 | `					if( pValue ){` |
|       15 |  5587 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5588 | `					}` |
|        - |  5589 | `				}` |
|        8 |  5590 | `			}` |
|        - |  5591 | `		}` |
|        - |  5592 | `	}` |
|   164074 |  5593 | `	break;` |
|        - |  5594 | `						  }` |
|        - |  5595 | `/*` |
|        - |  5596 | ` * OP_MEMBER P1 P2` |
|        - |  5597 | ` * Load class attribute/method on the stack.` |
|        - |  5598 | ` */` |
|     2210 |  5599 | `case PH7_OP_MEMBER: {` |
|        - |  5600 | `	ph7_class_instance *pThis;` |
|        - |  5601 | `	ph7_value *pNos;` |
|        - |  5602 | `	SyString sName;` |
|     4422 |  5603 | `	if( !pInstr->iP1 ){` |
|     4280 |  5604 | `		pNos = &pTos[-1];` |
|        - |  5605 | `#ifdef UNTRUST` |
|        - |  5606 | `		if( pNos < pStack ){` |
|        - |  5607 | `			goto Abort;` |
|        - |  5608 | `		}` |
|        - |  5609 | `#endif` |
|     4280 |  5610 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5611 | `			ph7_class *pClass;` |
|        - |  5612 | `			/* Class already instantiated */` |
|     4280 |  5613 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5614 | `			/* Point to the instantiated class */` |
|     4280 |  5615 | `			pClass = pThis->pClass;` |
|        - |  5616 | `			/* Extract attribute name first */` |
|     4280 |  5617 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4280 |  5618 | `			if( pInstr->iP2 ){` |
|        - |  5619 | `				/* Method call */` |
|      436 |  5620 | `				ph7_class_method *pMeth = 0;` |
|      436 |  5621 | `				if( sName.nByte > 0 ){` |
|        - |  5622 | `					/* Extract the target method */` |
|      436 |  5623 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      217 |  5624 | `				}` |
|      436 |  5625 | `				if( pMeth == 0 ){` |
|      ! 0 |  5626 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5627 | `						&pClass->sName,&sName` |
|        - |  5628 | `						);` |
|        - |  5629 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5630 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5631 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5632 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5633 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5634 | `				}else{` |
|        - |  5635 | `					/* Push method name on the stack */` |
|      436 |  5636 | `					PH7_MemObjRelease(pTos);` |
|      436 |  5637 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      436 |  5638 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5639 | `				}` |
|      436 |  5640 | `				pTos->nIdx = SXU32_HIGH;` |
|      219 |  5641 | `			}else{` |
|        - |  5642 | `				/* Attribute access */` |
|     3846 |  5643 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5644 | `				SyHashEntry *pEntry;` |
|        - |  5645 | `				/* Extract the target attribute */` |
|     3846 |  5646 | `				if( sName.nByte > 0 ){` |
|     3846 |  5647 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3846 |  5648 | `					if( pEntry ){` |
|        - |  5649 | `						/* Point to the attribute value */` |
|     3844 |  5650 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1921 |  5651 | `					}` |
|     1922 |  5652 | `				}` |
|     3846 |  5653 | `				if( pObjAttr == 0 ){` |
|        - |  5654 | `					/* No such attribute,load null */` |
|        4 |  5655 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5656 | `						&pClass->sName,&sName);` |
|        - |  5657 | `					/* Call the __get magic method if available */` |
|        3 |  5658 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5659 | `				}` |
|     3846 |  5660 | `				VmPopOperand(&pTos,1);` |
|        - |  5661 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5662 | `				 * This is due to the following case:` |
|        - |  5663 | `				 *     (new TestClass())->foo;` |
|        - |  5664 | `				 */` |
|     3846 |  5665 | `				pThis->iRef++;` |
|     3846 |  5666 | `				PH7_MemObjRelease(pTos);` |
|     3846 |  5667 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3846 |  5668 | `				if( pObjAttr ){` |
|     3844 |  5669 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5670 | `					/* Check attribute access */` |
|     3844 |  5671 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5672 | `						/* Load attribute */` |
|     3844 |  5673 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3844 |  5674 | `						if( pValue ){` |
|     3844 |  5675 | `							if( pThis->iRef < 2 ){` |
|        - |  5676 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5677 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5678 | `								 */` |
|        3 |  5679 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5680 | `							}else{` |
|        - |  5681 | `								/* Simple load */` |
|     3842 |  5682 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5683 | `							}` |
|     3844 |  5684 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3842 |  5685 | `								if( pThis->iRef > 1 ){` |
|        - |  5686 | `									/* Load attribute index */` |
|     3840 |  5687 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1919 |  5688 | `								}` |
|     1920 |  5689 | `							}` |
|     1921 |  5690 | `						}` |
|     1921 |  5691 | `					}` |
|     1921 |  5692 | `				}` |
|        - |  5693 | `				/* Safely unreference the object */` |
|     3846 |  5694 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5695 | `			}` |
|     2141 |  5696 | `		}else{` |
|      ! 0 |  5697 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5698 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5699 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5700 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5701 | `		}` |
|     2141 |  5702 | `	}else{` |
|        - |  5703 | `		/* Static member access using class name */` |
|      144 |  5704 | `		pNos = pTos;` |
|      144 |  5705 | `		pThis = 0;` |
|      144 |  5706 | `		if( !pInstr->p3 ){` |
|      132 |  5707 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      132 |  5708 | `			pNos--;` |
|        - |  5709 | `#ifdef UNTRUST` |
|        - |  5710 | `			if( pNos < pStack ){` |
|        - |  5711 | `				goto Abort;` |
|        - |  5712 | `			}` |
|        - |  5713 | `#endif` |
|       67 |  5714 | `		}else{` |
|        - |  5715 | `			/* Attribute name already computed */` |
|       14 |  5716 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5717 | `		}` |
|      144 |  5718 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      144 |  5719 | `			ph7_class *pClass = 0;` |
|      144 |  5720 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5721 | `				/* Class already instantiated */` |
|        5 |  5722 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5723 | `				pClass = pThis->pClass;` |
|        5 |  5724 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5725 | `			}else{` |
|        - |  5726 | `				/* Try to extract the target class */` |
|      140 |  5727 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      140 |  5728 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      140 |  5729 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5730 | `					/* Handle self/static/parent keywords */` |
|      140 |  5731 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       36 |  5732 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       36 |  5733 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5734 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5735 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5736 | `						}` |
|      123 |  5737 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       22 |  5738 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      103 |  5739 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       16 |  5740 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       16 |  5741 | `						if( pSelf && pSelf->pBase ){` |
|       16 |  5742 | `							pClass = pSelf->pBase;` |
|        7 |  5743 | `						}` |
|        9 |  5744 | `					}else{` |
|       72 |  5745 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5746 | `					}` |
|       69 |  5747 | `				}` |
|        - |  5748 | `			}` |
|      144 |  5749 | `			if( pClass == 0 ){` |
|        - |  5750 | `				/* Undefined class */` |
|      ! 0 |  5751 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5752 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5753 | `					);` |
|      ! 0 |  5754 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5755 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5756 | `				}` |
|      ! 0 |  5757 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5758 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5759 | `			}else{` |
|      144 |  5760 | `				if( pInstr->iP2 ){` |
|        - |  5761 | `					/* Method call */` |
|       68 |  5762 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5763 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5764 | `						/* Extract the target method */` |
|       68 |  5765 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5766 | `					}` |
|       68 |  5767 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5768 | `						if( pMeth ){` |
|      ! 0 |  5769 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5770 | `								&pClass->sName,&sName` |
|        - |  5771 | `								);` |
|      ! 0 |  5772 | `						}else{` |
|      ! 0 |  5773 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5774 | `								&pClass->sName,&sName` |
|        - |  5775 | `								);` |
|        - |  5776 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5777 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5778 | `						}` |
|        - |  5779 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5780 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5781 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5782 | `						}` |
|      ! 0 |  5783 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5784 | `					}else{` |
|        - |  5785 | `						/* Push method name on the stack */` |
|       68 |  5786 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5787 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5788 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5789 | `					}` |
|       68 |  5790 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5791 | `				}else{` |
|        - |  5792 | `					/* Attribute access */` |
|       78 |  5793 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5794 | `					/* Check for special ::class pseudo-constant */` |
|      113 |  5795 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5796 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5797 | `						/* ::class returns the fully qualified class name */` |
|        - |  5798 | `						/* Pop the attribute name from the stack */` |
|       60 |  5799 | `						if( !pInstr->p3 ){` |
|       60 |  5800 | `							VmPopOperand(&pTos,1);` |
|       29 |  5801 | `						}` |
|       60 |  5802 | `						PH7_MemObjRelease(pTos);` |
|        - |  5803 | `						/* Load the class name */` |
|       60 |  5804 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5805 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5806 | `					}else{` |
|        - |  5807 | `						/* Extract the target attribute */` |
|       20 |  5808 | `						if( sName.nByte > 0 ){` |
|       20 |  5809 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5810 | `						}` |
|       20 |  5811 | `						if( pAttr == 0 ){` |
|        - |  5812 | `							/* No such attribute,load null */` |
|      ! 0 |  5813 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5814 | `								&pClass->sName,&sName);` |
|        - |  5815 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5816 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5817 | `						}` |
|        - |  5818 | `						/* Pop the attribute name from the stack */` |
|       20 |  5819 | `						if( !pInstr->p3 ){` |
|        7 |  5820 | `							VmPopOperand(&pTos,1);` |
|        3 |  5821 | `						}` |
|       20 |  5822 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5823 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5824 | `						if( pAttr ){` |
|       20 |  5825 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5826 | `								/* Access to a non static attribute */` |
|      ! 0 |  5827 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5828 | `									&pClass->sName,&pAttr->sName` |
|        - |  5829 | `									);` |
|      ! 0 |  5830 | `							}else{` |
|        - |  5831 | `								ph7_value *pValue;` |
|        - |  5832 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5833 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5834 | `									/* Load the desired attribute */` |
|       20 |  5835 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5836 | `									if( pValue ){` |
|       20 |  5837 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5838 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5839 | `											/* Load index number */` |
|       14 |  5840 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5841 | `										}` |
|        9 |  5842 | `									}` |
|        9 |  5843 | `								}` |
|        - |  5844 | `							}` |
|        9 |  5845 | `						}` |
|        - |  5846 | `					}` |
|        - |  5847 | `				}` |
|      144 |  5848 | `				if( pThis ){` |
|        - |  5849 | `					/* Safely unreference the object */` |
|        5 |  5850 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5851 | `				}` |
|        - |  5852 | `			}` |
|       73 |  5853 | `		}else{` |
|        - |  5854 | `			/* Pop operands */` |
|      ! 0 |  5855 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5856 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5857 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5858 | `			}` |
|      ! 0 |  5859 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5860 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5861 | `		}` |
|        - |  5862 | `	}` |
|     4422 |  5863 | `	break;` |
|        - |  5864 | `					}` |
|        - |  5865 | `/*` |
|        - |  5866 | ` * OP_NEW P1 * * *` |
|        - |  5867 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5868 | ` */` |
|      328 |  5869 | `case PH7_OP_NEW: {` |
|      658 |  5870 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      658 |  5871 | `	ph7_class *pClass = 0;` |
|        - |  5872 | `	ph7_class_instance *pNew;` |
|      658 |  5873 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5874 | `		/* Try to extract the desired class */` |
|      986 |  5875 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      656 |  5876 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      328 |  5877 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5878 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5879 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5880 | `	}` |
|      658 |  5881 | `	if( pClass == 0 ){` |
|        - |  5882 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5883 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5884 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5885 | `			);` |
|        - |  5886 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5887 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5888 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5889 | `			/* Pop given arguments */` |
|      ! 0 |  5890 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5891 | `		}` |
|      ! 0 |  5892 | `		goto Abort;` |
|      ! 0 |  5893 | `	}else{` |
|        - |  5894 | `		ph7_class_method *pCons;` |
|        - |  5895 | `		/* Create a new class instance */` |
|      658 |  5896 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      658 |  5897 | `		if( pNew == 0 ){` |
|      ! 0 |  5898 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5899 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5900 | `				&pClass->sName` |
|        - |  5901 | `			);` |
|      ! 0 |  5902 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5903 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5904 | `				/* Pop given arguments */` |
|      ! 0 |  5905 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5906 | `			}` |
|      ! 0 |  5907 | `			break;` |
|        - |  5908 | `		}` |
|        - |  5909 | `		/* Check if a constructor is available */` |
|      658 |  5910 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      658 |  5911 | `		if( pCons == 0 ){` |
|      544 |  5912 | `			SyString *pName = &pClass->sName;` |
|        - |  5913 | `			/* Check for a constructor with the same base class name */` |
|      544 |  5914 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      271 |  5915 | `		}` |
|      658 |  5916 | `		if( pCons ){` |
|        - |  5917 | `			/* Call the class constructor */` |
|      116 |  5918 | `			SySetReset(&aArg);` |
|      220 |  5919 | `			while( pArg < pTos ){` |
|      106 |  5920 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  5921 | `				pArg++;` |
|        2 |  5922 | `			}` |
|      116 |  5923 | `			if( pVm->bErrReport ){` |
|        - |  5924 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5925 | `				sxu32 n;` |
|       57 |  5926 | `				n = SySetUsed(&aArg);` |
|        - |  5927 | `				/* Emit a notice for missing arguments */` |
|      101 |  5928 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  5929 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  5930 | `					if( pFuncArg ){` |
|       45 |  5931 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5932 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5933 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5934 | `						}` |
|       22 |  5935 | `					}` |
|       45 |  5936 | `					n++;` |
|        1 |  5937 | `				}` |
|       28 |  5938 | `			}` |
|      116 |  5939 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5940 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  5941 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5942 | `				pNew->iRef = 1;` |
|      ! 0 |  5943 | `			}` |
|       57 |  5944 | `		}` |
|      658 |  5945 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5946 | `			/* Pop given arguments */` |
|       98 |  5947 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  5948 | `		}` |
|      658 |  5949 | `		PH7_MemObjRelease(pTos);` |
|      658 |  5950 | `		pTos->x.pOther = pNew;` |
|      658 |  5951 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5952 | `	}` |
|      658 |  5953 | `	break;` |
|        - |  5954 | `				 }` |
|        - |  5955 | `/*` |
|        - |  5956 | ` * OP_CLONE * * *` |
|        - |  5957 | ` * Perfome a clone operation.` |
|        - |  5958 | ` */` |
|       23 |  5959 | `case PH7_OP_CLONE: {` |
|        - |  5960 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5961 | `#ifdef UNTRUST` |
|        - |  5962 | `	if( pTos < pStack ){` |
|        - |  5963 | `		goto Abort;` |
|        - |  5964 | `	}` |
|        - |  5965 | `#endif` |
|        - |  5966 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5967 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5968 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5969 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5970 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5971 | `		break;` |
|        - |  5972 | `	}` |
|        - |  5973 | `	/* Point to the source */` |
|       44 |  5974 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5975 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5976 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5977 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5978 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5979 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5980 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5981 | `		break;` |
|        - |  5982 | `	}` |
|        - |  5983 | `	/* Perform the clone operation */` |
|       44 |  5984 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5985 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5986 | `	if( pClone == 0 ){` |
|      ! 0 |  5987 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5988 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5989 | `	}else{` |
|        - |  5990 | `		/* Load the cloned object */` |
|       44 |  5991 | `		pTos->x.pOther = pClone;` |
|       44 |  5992 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5993 | `	}` |
|       44 |  5994 | `	break;` |
|        - |  5995 | `				   }` |
|        - |  5996 | `/*` |
|        - |  5997 | ` * OP_SWITCH * * P3` |
|        - |  5998 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5999 | ` */` |
|       21 |  6000 | `case PH7_OP_SWITCH: {` |
|       44 |  6001 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6002 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6003 | `	ph7_value sValue,sCaseValue;` |
|        - |  6004 | `	sxu32 n,nEntry;` |
|        - |  6005 | `#ifdef UNTRUST` |
|        - |  6006 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6007 | `		goto Abort;` |
|        - |  6008 | `	}` |
|        - |  6009 | `#endif` |
|        - |  6010 | `	/* Point to the case table  */` |
|       44 |  6011 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  6012 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6013 | `	/* Select the appropriate case block to execute */` |
|       44 |  6014 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  6015 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  6016 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  6017 | `		pCase = &aCase[n];` |
|      102 |  6018 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6019 | `		/* Execute the case expression first */` |
|      102 |  6020 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6021 | `		/* Compare the two expression */` |
|      102 |  6022 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  6023 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  6024 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  6025 | `		if( rc == 0 ){` |
|        - |  6026 | `			/* Value match,jump to this block */` |
|       44 |  6027 | `			pc = pCase->nStart - 1;` |
|       44 |  6028 | `			break;` |
|        - |  6029 | `		}` |
|       31 |  6030 | `	}` |
|       44 |  6031 | `	VmPopOperand(&pTos,1);` |
|       44 |  6032 | `	if( n >= nEntry ){` |
|        - |  6033 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6034 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6035 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6036 | `		}else{` |
|        - |  6037 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6038 | `			pc = pSwitch->nOut - 1;` |
|        - |  6039 | `		}` |
|      ! 0 |  6040 | `	}` |
|       44 |  6041 | `	break;` |
|        - |  6042 | `					}` |
|        - |  6043 | `/*` |
|        - |  6044 | ` * OP_YIELD P1 P2 *` |
|        - |  6045 | ` *  Yield a value from a generator function.` |
|        - |  6046 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6047 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6048 | ` */` |
|       28 |  6049 | `case PH7_OP_YIELD: {` |
|        - |  6050 | `	ph7_generator *pGen;` |
|       58 |  6051 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6052 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6053 | `		goto Abort;` |
|        - |  6054 | `	}` |
|       58 |  6055 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6056 | `	if( pInstr->iP2 ){` |
|        - |  6057 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6058 | `#ifdef UNTRUST` |
|        - |  6059 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6060 | `#endif` |
|        7 |  6061 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6062 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6063 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6064 | `		VmPopOperand(&pTos, 1);` |
|        - |  6065 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6066 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6067 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6068 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6069 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6070 | `			}` |
|        1 |  6071 | `		}` |
|       55 |  6072 | `	}else if( pInstr->iP1 ){` |
|        - |  6073 | `		/* yield $value */` |
|        - |  6074 | `#ifdef UNTRUST` |
|        - |  6075 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6076 | `#endif` |
|       52 |  6077 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6078 | `		VmPopOperand(&pTos, 1);` |
|        - |  6079 | `		/* Auto-increment key */` |
|       52 |  6080 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6081 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6082 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6083 | `	}else{` |
|        - |  6084 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6085 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6086 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6087 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6088 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6089 | `	}` |
|        - |  6090 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6091 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6092 | `	goto Suspend;` |
|        - |  6093 |  |
|        - |  6094 | `/*` |
|        - |  6095 | ` * OP_CALL P1 * *` |
|        - |  6096 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6097 | ` *  function on the stack.` |
|        - |  6098 | ` */` |
|   295682 |  6099 | `case PH7_OP_CALL: {` |
|   591410 |  6100 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6101 | `	ph7_value *pArg;` |
|   591410 |  6102 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   591410 |  6103 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6104 | `	SyHashEntry *pEntry;` |
|        - |  6105 | `	SyString sName;` |
|        - |  6106 | `	/* Extract function name */` |
|   591410 |  6107 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6108 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6109 | `			ph7_value sResult;` |
|      ! 0 |  6110 | `			SySetReset(&aArg);` |
|      ! 0 |  6111 | `			while( pArg < pTos ){` |
|      ! 0 |  6112 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6113 | `				pArg++;` |
|      ! 0 |  6114 | `			}` |
|      ! 0 |  6115 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6116 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6117 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6118 | `			SySetReset(&aArg);` |
|        - |  6119 | `			/* Pop given arguments */` |
|      ! 0 |  6120 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6121 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6122 | `			}` |
|        - |  6123 | `			/* Copy result */` |
|      ! 0 |  6124 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6125 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6126 | `		}else{` |
|        3 |  6127 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6128 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6129 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6130 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6131 | `			}else{` |
|        - |  6132 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6133 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6134 | `			}` |
|        - |  6135 | `			/* Pop given arguments */` |
|        3 |  6136 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6137 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6138 | `			}` |
|        - |  6139 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6140 | `			PH7_MemObjRelease(pTos);` |
|        - |  6141 | `		}` |
|   295409 |  6142 | `		break;` |
|        - |  6143 | `	}` |
|   591408 |  6144 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6145 | `	/* Check for a compiled function first.` |
|        - |  6146 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6147 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   591408 |  6148 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6149 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6150 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6151 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6152 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6153 | `	 * function calls inside namespaces. */` |
|   591408 |  6154 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6155 | `		const char *zFunc;` |
|        - |  6156 | `		const char *zEnd;` |
|        - |  6157 | `		const char *z;` |
|        - |  6158 | `		SyString sGlobal;` |
|       15 |  6159 | `		zFunc = sName.zString;` |
|       15 |  6160 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6161 | `		z = zEnd;` |
|        - |  6162 | `		/* Find last namespace separator */` |
|      133 |  6163 | `		while( z > zFunc ){` |
|      133 |  6164 | `			if( z[-1] == '\\' ){` |
|       15 |  6165 | `				break;` |
|        - |  6166 | `			}` |
|      119 |  6167 | `			z--;` |
|        1 |  6168 | `		}` |
|       15 |  6169 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6170 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6171 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6172 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6173 | `		}` |
|        7 |  6174 | `	}` |
|   591408 |  6175 | `	if( pEntry ){` |
|        - |  6176 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6177 | `		ph7_class_instance *pThis;` |
|        - |  6178 | `		ph7_value *pFrameStack;` |
|        - |  6179 | `		ph7_vm_func *pVmFunc;` |
|        - |  6180 | `		ph7_class *pSelf;` |
|        - |  6181 | `		VmFrame *pFrame;` |
|        - |  6182 | `		ph7_value *pObj;` |
|        - |  6183 | `		VmSlot sArg;` |
|        - |  6184 | `		sxu32 n;` |
|        - |  6185 | `		/* initialize fields */` |
|    13302 |  6186 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13302 |  6187 | `		pThis = 0;` |
|    13302 |  6188 | `		pSelf = 0;` |
|    13302 |  6189 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6190 | `			ph7_class_method *pMeth;` |
|        - |  6191 | `			/* Class method call */` |
|     1994 |  6192 | `			ph7_value *pTarget = &pTos[-1];` |
|     1994 |  6193 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6194 | `				/* Extract the 'this' pointer */` |
|     1994 |  6195 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6196 | `					/* Instance already loaded */` |
|     1922 |  6197 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1922 |  6198 | `					pThis->iRef++;` |
|     1922 |  6199 | `					pSelf = pThis->pClass;` |
|      960 |  6200 | `				}` |
|     1994 |  6201 | `				if( pSelf == 0 ){` |
|       74 |  6202 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6203 | `						/* "Late Static Binding" class name */` |
|      101 |  6204 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6205 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6206 | `					}` |
|       74 |  6207 | `					if( pSelf == 0 ){` |
|       13 |  6208 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6209 | `					}` |
|       36 |  6210 | `				}` |
|     1994 |  6211 | `				if( pThis == 0  ){` |
|       74 |  6212 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6213 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6214 | `					if( pFrameLocal->pParent ){` |
|        - |  6215 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6216 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6217 | `						if( pThis ){` |
|       13 |  6218 | `							pThis->iRef++;` |
|        6 |  6219 | `						}` |
|       28 |  6220 | `					}` |
|       36 |  6221 | `				}` |
|     1994 |  6222 | `				VmPopOperand(&pTos,1);` |
|     1994 |  6223 | `				PH7_MemObjRelease(pTos);` |
|        - |  6224 | `				/* Synchronize pointers */` |
|     1994 |  6225 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6226 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6227 | `				 * user have already computed the random generated unique class method name` |
|        - |  6228 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6229 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6230 | `				 */` |
|     1994 |  6231 | `				while( pArg < pStack ){` |
|      ! 0 |  6232 | `					pArg++;` |
|      ! 0 |  6233 | `				}` |
|     1994 |  6234 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6235 | `					/* Check if the call is allowed */` |
|     1994 |  6236 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1994 |  6237 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6238 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6239 | `							/* Pop given arguments */` |
|      ! 0 |  6240 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6241 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6242 | `							}` |
|        - |  6243 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6244 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6245 | `							break;` |
|        - |  6246 | `						}` |
|        3 |  6247 | `					}` |
|      996 |  6248 | `				}` |
|      996 |  6249 | `			}` |
|      996 |  6250 | `		}` |
|        - |  6251 | `		/* Check The recursion limit */` |
|    13302 |  6252 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6253 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6254 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6255 | `				&pVmFunc->sName);` |
|        - |  6256 | `			/* Pop given arguments */` |
|        3 |  6257 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6258 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6259 | `			}` |
|        - |  6260 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6261 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6262 | `			break;` |
|        - |  6263 | `		}` |
|    13300 |  6264 | `		if( pVmFunc->pNextName ){` |
|        - |  6265 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6266 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6267 | `		}` |
|    13300 |  6268 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6269 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6270 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6271 | `			ph7_generator *pGenerator;` |
|        - |  6272 | `			ph7_class_instance *pGenObj;` |
|        - |  6273 | `			ph7_value *pCtxAttr;` |
|        - |  6274 | `			SyString sAttrName;` |
|        - |  6275 | `			ph7_value **apCallArgs;` |
|        - |  6276 | `			int nGenArgs, iArg;` |
|        - |  6277 | `			/* Collect arguments from the operand stack */` |
|       20 |  6278 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6279 | `			apCallArgs = 0;` |
|       20 |  6280 | `			if( nGenArgs > 0 ){` |
|        8 |  6281 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6282 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6283 | `				if( apCallArgs == 0 ){` |
|        - |  6284 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6285 | `					nGenArgs = 0;` |
|      ! 0 |  6286 | `				}else{` |
|       12 |  6287 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6288 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6289 | `					}` |
|        - |  6290 | `				}` |
|        2 |  6291 | `			}` |
|        - |  6292 | `			/* Create execution context and generator wrapper */` |
|       20 |  6293 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6294 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6295 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6296 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6297 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6298 | `				break;` |
|        - |  6299 | `			}` |
|       20 |  6300 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6301 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6302 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6303 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6304 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6305 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6306 | `				break;` |
|        - |  6307 | `			}` |
|        - |  6308 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6309 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6310 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6311 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6312 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6313 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6314 | `			if( apCallArgs ){` |
|        6 |  6315 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6316 | `			}` |
|       20 |  6317 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6318 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6319 | `				if( pThis ){` |
|      ! 0 |  6320 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6321 | `				}` |
|      ! 0 |  6322 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6323 | `					goto Abort;` |
|        - |  6324 | `				}` |
|      ! 0 |  6325 | `				break;` |
|        - |  6326 | `			}` |
|        - |  6327 | `			/* Create Generator class instance */` |
|       20 |  6328 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6329 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6330 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6331 | `				break;` |
|        - |  6332 | `			}` |
|        - |  6333 | `			/* Store generator in __ctx attribute */` |
|       20 |  6334 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6335 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6336 | `			if( pCtxAttr ){` |
|       20 |  6337 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6338 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6339 | `			}` |
|        - |  6340 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6341 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6342 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6343 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6344 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6345 | `			pGenObj->iRef++;` |
|       20 |  6346 | `			if( pThis ){` |
|      ! 0 |  6347 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6348 | `			}` |
|       20 |  6349 | `			break;` |
|        - |  6350 | `		}` |
|        - |  6351 | `		/* Extract the formal argument set */` |
|    13282 |  6352 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6353 | `		/* Create a new VM frame  */` |
|    13282 |  6354 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13282 |  6355 | `		if( rc != SXRET_OK ){` |
|        - |  6356 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6357 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6358 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6359 | `				&pVmFunc->sName);` |
|        - |  6360 | `			/* Pop given arguments */` |
|      ! 0 |  6361 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6362 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6363 | `			}` |
|        - |  6364 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6365 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6366 | `			break;` |
|        - |  6367 | `		}` |
|    13282 |  6368 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6369 | `			/* Install the '$this' variable */` |
|        - |  6370 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1932 |  6371 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1932 |  6372 | `			if( pObj ){` |
|        - |  6373 | `				/* Reflect the change */` |
|     1932 |  6374 | `				pObj->x.pOther = pThis;` |
|     1932 |  6375 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      965 |  6376 | `			}` |
|      965 |  6377 | `		}` |
|    13282 |  6378 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6379 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6380 | `			/* Install static variables */` |
|      ! 0 |  6381 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6382 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6383 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6384 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6385 | `					/* Initialize the static variables */` |
|      ! 0 |  6386 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6387 | `					if( pObj ){` |
|        - |  6388 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6389 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6390 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6391 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6392 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6393 | `						}` |
|      ! 0 |  6394 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6395 | `					}else{` |
|      ! 0 |  6396 | `						continue;` |
|        - |  6397 | `					}` |
|      ! 0 |  6398 | `				}` |
|        - |  6399 | `				/* Install in the current frame */` |
|      ! 0 |  6400 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6401 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6402 | `			}` |
|      ! 0 |  6403 | `		}` |
|        - |  6404 | `		/* Push arguments in the local frame */` |
|    13282 |  6405 | `		n = 0;` |
|    35988 |  6406 | `		while( pArg < pTos ){` |
|    22728 |  6407 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6408 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6409 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6410 | `				if( pObj ){` |
|        - |  6411 | `					/* Initialize as empty array */` |
|       21 |  6412 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6413 | `					{` |
|       21 |  6414 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6415 | `						while( pArg < pTos ){` |
|        - |  6416 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6417 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6418 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6419 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6420 | `								if( xCast ){` |
|       13 |  6421 | `									xCast(pArg);` |
|        6 |  6422 | `								}` |
|        6 |  6423 | `							}` |
|       63 |  6424 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6425 | `							pArg++;` |
|        1 |  6426 | `						}` |
|        - |  6427 | `					}` |
|       21 |  6428 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6429 | `					sArg.pUserData = 0;` |
|       21 |  6430 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6431 | `				}` |
|       21 |  6432 | `				break; /* All remaining args consumed */` |
|        - |  6433 | `			}` |
|    22708 |  6434 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22552 |  6435 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6436 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6437 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6438 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6439 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6440 | `						goto Abort;` |
|        - |  6441 | `					}` |
|      ! 0 |  6442 | `				}` |
|        - |  6443 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6444 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22564 |  6445 | `				if( aFormalArg[n].nType > 0` |
|    11853 |  6446 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1140 |  6447 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6448 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6449 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6450 | `						ph7_class *pClass;` |
|        - |  6451 | `						/* Try to extract the desired class */` |
|        5 |  6452 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6453 | `						if( pClass ){` |
|        5 |  6454 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6455 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6456 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6457 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6458 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6459 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6460 | `								}` |
|      ! 0 |  6461 | `							}else{` |
|        - |  6462 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6463 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6464 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6465 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6466 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6467 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6468 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6469 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6470 | `								}` |
|        - |  6471 | `							}` |
|        3 |  6472 | `						}` |
|     1138 |  6473 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6474 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6475 | `						/* Cast to the desired type */` |
|      ! 0 |  6476 | `						xCast(pArg);` |
|      ! 0 |  6477 | `					}` |
|      569 |  6478 | `				}` |
|    22554 |  6479 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6480 | `					/* Pass by reference */` |
|       54 |  6481 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6482 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6483 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6484 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6485 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6486 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6487 | `						}` |
|        - |  6488 | `						/* Switch to pass by value */` |
|      ! 0 |  6489 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6490 | `					}else{` |
|        - |  6491 | `						SyHashEntry *pRefEntry;` |
|        - |  6492 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6493 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6494 | `						if( pRefEntry == 0 ){` |
|       80 |  6495 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6496 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6497 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6498 | `							sArg.pUserData = 0;` |
|       54 |  6499 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6500 | `						}` |
|       54 |  6501 | `						pObj = 0;` |
|        - |  6502 | `					}` |
|       28 |  6503 | `				}else{` |
|        - |  6504 | `					/* Pass by value,make a copy of the given argument */` |
|    22502 |  6505 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6506 | `				}` |
|    11278 |  6507 | `			}else{` |
|        - |  6508 | `				char zName[32];` |
|        - |  6509 | `				SyString sArgName;` |
|        - |  6510 | `				/* Set a dummy name */` |
|      156 |  6511 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6512 | `				sArgName.zString = zName;` |
|        - |  6513 | `				/* Annonymous argument */` |
|      156 |  6514 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6515 | `			}` |
|    22708 |  6516 | `			if( pObj ){` |
|    22656 |  6517 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6518 | `				/* Insert argument index  */` |
|    22656 |  6519 | `				sArg.nIdx = pObj->nIdx;` |
|    22656 |  6520 | `				sArg.pUserData = 0;` |
|    22656 |  6521 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11327 |  6522 | `			}` |
|    22708 |  6523 | `			PH7_MemObjRelease(pArg);` |
|    22708 |  6524 | `			pArg++;` |
|    22708 |  6525 | `			++n;` |
|        2 |  6526 | `		}` |
|        - |  6527 | `		/* Set up closure environment */` |
|    13282 |  6528 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6529 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6530 | `			ph7_value *pValue;` |
|        - |  6531 | `			sxu32 iEnv;` |
|       11 |  6532 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6533 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6534 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6535 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6536 | `					/* Do not install null value */` |
|       11 |  6537 | `					continue;` |
|        - |  6538 | `				}` |
|       11 |  6539 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6540 | `				if( pValue == 0 ){` |
|      ! 0 |  6541 | `					continue;` |
|        - |  6542 | `				}` |
|        - |  6543 | `				/* Invalidate any prior representation */` |
|       11 |  6544 | `				PH7_MemObjRelease(pValue);` |
|        - |  6545 | `				/* Duplicate bound variable value */` |
|       11 |  6546 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6547 | `			}` |
|        5 |  6548 | `		}` |
|        - |  6549 | `		/* Process default values for remaining formal parameters */` |
|    15232 |  6550 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1978 |  6551 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6552 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6553 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6554 | `				if( pObj ){` |
|       27 |  6555 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6556 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6557 | `					sArg.pUserData = 0;` |
|       27 |  6558 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6559 | `				}` |
|       27 |  6560 | `				n++;` |
|       27 |  6561 | `				break; /* Variadic is always last */` |
|        - |  6562 | `			}` |
|     1952 |  6563 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1946 |  6564 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1946 |  6565 | `				if( pObj ){` |
|        - |  6566 | `					/* Evaluate the default value and extract it's result */` |
|     1946 |  6567 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1946 |  6568 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6569 | `						goto Abort;` |
|        - |  6570 | `					}` |
|        - |  6571 | `					/* Insert argument index */` |
|     1946 |  6572 | `					sArg.nIdx = pObj->nIdx;` |
|     1946 |  6573 | `					sArg.pUserData = 0;` |
|     1946 |  6574 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6575 | `					/* Make sure the default argument is of the correct type */` |
|     1946 |  6576 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6577 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6578 | `						/* Cast to the desired type */` |
|      ! 0 |  6579 | `						xCast(pObj);` |
|      ! 0 |  6580 | `					}` |
|      972 |  6581 | `				}` |
|      972 |  6582 | `			}` |
|     1952 |  6583 | `			++n;` |
|        2 |  6584 | `		}` |
|        - |  6585 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6586 | `		 * does not return anything.` |
|        - |  6587 | `		 */` |
|    13282 |  6588 | `		PH7_MemObjRelease(pTos);` |
|    13282 |  6589 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6590 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13282 |  6591 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13282 |  6592 | `		if( pFrameStack == 0 ){` |
|        - |  6593 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6594 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6595 | `				&pVmFunc->sName);` |
|      ! 0 |  6596 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6597 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6598 | `			}` |
|      ! 0 |  6599 | `			break;` |
|        - |  6600 | `		}` |
|    13282 |  6601 | `		if( pSelf ){` |
|        - |  6602 | `			/* Push class name */` |
|     1992 |  6603 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      995 |  6604 | `		}` |
|        - |  6605 | `		/* Increment nesting level */` |
|    13282 |  6606 | `		pVm->nRecursionDepth++;` |
|        - |  6607 | `		/* Execute function body */` |
|    13282 |  6608 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6609 | `		/* Decrement nesting level */` |
|    13282 |  6610 | `		pVm->nRecursionDepth--;` |
|    13282 |  6611 | `		if( pSelf ){` |
|        - |  6612 | `			/* Pop class name */` |
|     1992 |  6613 | `			(void)SySetPop(&pVm->aSelf);` |
|      995 |  6614 | `		}` |
|        - |  6615 | `		/* Cleanup the mess left behind */` |
|    13282 |  6616 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6617 | `			/* Return by reference,reflect that */` |
|        9 |  6618 | `			if( n != SXU32_HIGH ){` |
|        9 |  6619 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6620 | `				sxu32 i;` |
|        - |  6621 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6622 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6623 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6624 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6625 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6626 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6627 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6628 | `								&pVmFunc->sName);` |
|      ! 0 |  6629 | `						}` |
|      ! 0 |  6630 | `						n = SXU32_HIGH;` |
|      ! 0 |  6631 | `						break;` |
|        - |  6632 | `					}` |
|        3 |  6633 | `				}` |
|        5 |  6634 | `			}else{` |
|      ! 0 |  6635 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6636 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6637 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6638 | `						&pVmFunc->sName);` |
|      ! 0 |  6639 | `				}` |
|        - |  6640 | `			}` |
|        9 |  6641 | `			pTos->nIdx = n;` |
|        4 |  6642 | `		}` |
|        - |  6643 | `		/* Cleanup the mess left behind */` |
|    13282 |  6644 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6645 | `			/* An exception was throw in this frame */` |
|       12 |  6646 | `			pFrame = pFrame->pParent;` |
|       12 |  6647 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6648 | `				/* Pop the resutlt */` |
|       10 |  6649 | `				VmPopOperand(&pTos,1);` |
|        - |  6650 | `				/* Jump to this destination */` |
|       10 |  6651 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6652 | `				rc = PH7_OK;` |
|        6 |  6653 | `			}else{` |
|        3 |  6654 | `				if( pFrame->pParent ){` |
|        3 |  6655 | `					rc = PH7_EXCEPTION;` |
|        2 |  6656 | `				}else{` |
|        - |  6657 | `					/* Continue normal execution */` |
|      ! 0 |  6658 | `					rc = PH7_OK;` |
|        - |  6659 | `				}` |
|        - |  6660 | `			}` |
|        5 |  6661 | `		}` |
|        - |  6662 | `		/* Free the operand stack */` |
|    13282 |  6663 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6664 | `		/* Leave the frame */` |
|    13282 |  6665 | `		VmLeaveFrame(&(*pVm));` |
|    13282 |  6666 | `		if( rc == PH7_ABORT ){` |
|        - |  6667 | `			/* Abort processing immeditaley */` |
|        7 |  6668 | `			goto Abort;` |
|    13276 |  6669 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6670 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6671 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6672 | `			 * overwriting the state saved by the inner level.` |
|        - |  6673 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6674 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6675 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6676 | `			goto Suspend;` |
|    13238 |  6677 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6678 | `			goto Exception;` |
|        - |  6679 | `		}` |
|     6619 |  6680 | `	}else{` |
|        - |  6681 | `		ph7_user_func *pFunc;` |
|        - |  6682 | `		ph7_context sCtx;` |
|        - |  6683 | `		ph7_value sRet;` |
|        - |  6684 | `		/* Look for an installed foreign function.` |
|        - |  6685 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6686 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6687 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6688 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6689 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6690 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   578108 |  6691 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   578108 |  6692 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6693 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6694 | `			const char *zShort = sName.zString;` |
|        - |  6695 | `			sxu32 i;` |
|      217 |  6696 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6697 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6698 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6699 | `				}` |
|      102 |  6700 | `			}` |
|       15 |  6701 | `			if( zShort != sName.zString ){` |
|       15 |  6702 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6703 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6704 | `			}` |
|        7 |  6705 | `		}` |
|   578108 |  6706 | `		if( pEntry == 0 ){` |
|        - |  6707 | `			/* Call to undefined function */` |
|        5 |  6708 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6709 | `			/* Pop given arguments */` |
|        5 |  6710 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6711 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6712 | `			}` |
|        - |  6713 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6714 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6715 | `			break;` |
|        - |  6716 | `		}` |
|   578104 |  6717 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6718 | `		/* Start collecting function arguments */` |
|   578104 |  6719 | `		SySetReset(&aArg);` |
|  1552334 |  6720 | `		while( pArg < pTos ){` |
|   974232 |  6721 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   974232 |  6722 | `			pArg++;` |
|        2 |  6723 | `		}` |
|        - |  6724 | `		/* Assume a null return value */` |
|   578104 |  6725 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6726 | `		/* Init the call context */` |
|   578104 |  6727 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6728 | `		/* Call the foreign function */` |
|   578104 |  6729 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6730 | `		/* Release the call context */` |
|   578104 |  6731 | `		VmReleaseCallContext(&sCtx);` |
|   578104 |  6732 | `		if( rc == PH7_ABORT ){` |
|      463 |  6733 | `			goto Abort;` |
|   577642 |  6734 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6735 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6736 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6737 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6738 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6739 | `				goto Exception;` |
|        - |  6740 | `			}` |
|        - |  6741 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6742 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6743 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6744 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6745 | `			}` |
|        - |  6746 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6747 | `			VmPopOperand(&pTos,1);` |
|        - |  6748 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6749 | `			pFrm = pVm->pFrame;` |
|        7 |  6750 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6751 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6752 | `			}` |
|        7 |  6753 | `			break;` |
|        - |  6754 | `		}` |
|   577632 |  6755 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6756 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6757 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6758 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6759 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6760 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6761 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6762 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6763 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6764 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6765 | `			}` |
|        - |  6766 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6767 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6768 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6769 | `			goto Suspend;` |
|        - |  6770 | `		}` |
|   577594 |  6771 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6772 | `			/* Pop function name and arguments */` |
|   559162 |  6773 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   279602 |  6774 | `		}` |
|        - |  6775 | `		/* Save foreign function return value */` |
|   577594 |  6776 | `		PH7_MemObjStore(&sRet,pTos);` |
|   577594 |  6777 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6778 | `	}` |
|   590828 |  6779 | `	break;` |
|        - |  6780 | `				  }` |
|        - |  6781 | `/*` |
|        - |  6782 | ` * OP_CONSUME: P1 * *` |
|        - |  6783 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6784 | ` */` |
|    11731 |  6785 | `case PH7_OP_CONSUME: {` |
|    23464 |  6786 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23464 |  6787 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6788 |  |
|    23464 |  6789 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23464 |  6790 | `	pCur = pOut;` |
|        - |  6791 | `	/* Start the consume process  */` |
|    46926 |  6792 | `	while( pOut <= pTos ){` |
|        - |  6793 | `		/* Force a string cast */` |
|    23464 |  6794 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6795 | `			PH7_MemObjToString(pOut);` |
|      149 |  6796 | `		}` |
|    23464 |  6797 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6798 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6799 | `			/* Invoke the output consumer callback */` |
|    13126 |  6800 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13126 |  6801 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13126 |  6802 | `			SyBlobRelease(&pOut->sBlob);` |
|    13126 |  6803 | `			if( rc == SXERR_ABORT ){` |
|        - |  6804 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6805 | `				goto Abort;` |
|        - |  6806 | `			}` |
|     6562 |  6807 | `		}` |
|    23464 |  6808 | `		pOut++;` |
|        2 |  6809 | `	}` |
|    23464 |  6810 | `	pTos = &pCur[-1];` |
|    23462 |  6811 | `	break;` |
|        - |  6812 | `					 }` |
|        - |  6813 |  |
|        - |  6814 | `		} /* Switch() */` |
|  9981040 |  6815 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6816 | `	} /* For(;;) */` |
|    16141 |  6817 | `Done:` |
|    32284 |  6818 | `	SySetRelease(&aArg);` |
|    32284 |  6819 | `	return SXRET_OK;` |
|       66 |  6820 | `Suspend:` |
|      134 |  6821 | `	SySetRelease(&aArg);` |
|      134 |  6822 | `	return PH7_SUSPEND;` |
|      238 |  6823 | `Abort:` |
|      477 |  6824 | `	SySetRelease(&aArg);` |
|     1661 |  6825 | `	while( pTos >= pStack ){` |
|     1185 |  6826 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6827 | `		pTos--;` |
|        1 |  6828 | `	}` |
|      477 |  6829 | `	return PH7_ABORT;` |
|        3 |  6830 | `Exception:` |
|        8 |  6831 | `	SySetRelease(&aArg);` |
|       22 |  6832 | `	while( pTos >= pStack ){` |
|       16 |  6833 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6834 | `		pTos--;` |
|        2 |  6835 | `	}` |
|        8 |  6836 | `	return PH7_EXCEPTION;` |
|    16450 |  6837 |  |
|        - |  6838 | `/*` |
|        - |  6839 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6840 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6841 | ` * See block-comment on that function for additional information.` |
|        - |  6842 | ` */` |
|    15216 |  6843 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6844 |  |
|        - |  6845 | `	ph7_value *pStack;` |
|        - |  6846 | `	sxi32 rc;` |
|        - |  6847 | `	/* Allocate a new operand stack */` |
|    15218 |  6848 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15218 |  6849 | `	if( pStack == 0 ){` |
|      ! 0 |  6850 | `		return SXERR_MEM;` |
|        - |  6851 | `	}` |
|        - |  6852 | `	/* Execute the program */` |
|    15218 |  6853 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6854 | `	/* Free the operand stack */` |
|    15218 |  6855 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6856 | `	/* Execution result */` |
|    15218 |  6857 | `	return rc;` |
|     7610 |  6858 |  |
|        - |  6859 | `/*` |
|        - |  6860 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6861 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6862 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6863 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6864 | ` * execution ends.` |
|        - |  6865 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6866 | ` * additional information.` |
|        - |  6867 | ` */` |
|     2308 |  6868 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6869 |  |
|        - |  6870 | `	VmShutdownCB *pEntry;` |
|        - |  6871 | `	ph7_value *apArg[10];` |
|        - |  6872 | `	sxu32 n,nEntry;` |
|        - |  6873 | `	int i;` |
|        - |  6874 | `	/* Point to the stack of registered callbacks */` |
|     2310 |  6875 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25390 |  6876 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23082 |  6877 | `		apArg[i] = 0;` |
|    11542 |  6878 | `	}` |
|     2312 |  6879 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6880 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6881 | `		if( pEntry ){` |
|        - |  6882 | `			/* Prepare callback arguments if any */` |
|        3 |  6883 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6884 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6885 | `					break;` |
|        - |  6886 | `				}` |
|      ! 0 |  6887 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6888 | `			}` |
|        - |  6889 | `			/* Invoke the callback */` |
|        3 |  6890 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6891 | `			/*` |
|        - |  6892 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6893 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6894 | `			 */` |
|        3 |  6895 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6896 | `			if( pEntry ){` |
|        3 |  6897 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6898 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6899 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6900 | `				}` |
|        1 |  6901 | `			}` |
|        1 |  6902 | `		}` |
|        2 |  6903 | `	}` |
|     2310 |  6904 | `	SySetReset(&pVm->aShutdown);` |
|     2310 |  6905 |  |
|        - |  6906 | `/*` |
|        - |  6907 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6908 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6909 | ` * See block-comment on that function for additional information.` |
|        - |  6910 | ` */` |
|     2316 |  6911 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6912 |  |
|        - |  6913 | `	/* Make sure we are ready to execute this program */` |
|     2318 |  6914 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6915 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6916 | `	}` |
|        - |  6917 | `	/* Set the execution magic number  */` |
|     2318 |  6918 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6919 | `	/* Execute the program */` |
|     2318 |  6920 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6921 | `	/* Invoke any shutdown callbacks */` |
|     2314 |  6922 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6923 | `	/*` |
|        - |  6924 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6925 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6926 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6927 | `	 */` |
|     2314 |  6928 | `	return SXRET_OK;` |
|     1160 |  6929 |  |
|        - |  6930 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6931 | `/*` |
|        - |  6932 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6933 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6934 | ` */` |
|       42 |  6935 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  6936 |  |
|        - |  6937 | `	ph7_exec_ctx *pCtx;` |
|        - |  6938 | `	ph7_value *pStack;` |
|        - |  6939 | `	VmFrame *pFrame;` |
|       44 |  6940 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  6941 | `	if( pCtx == 0 ){` |
|      ! 0 |  6942 | `		return 0;` |
|        - |  6943 | `	}` |
|       44 |  6944 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  6945 | `	pCtx->pVm = pVm;` |
|       44 |  6946 | `	pCtx->pFunc = pFunc;` |
|       44 |  6947 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  6948 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  6949 | `	pCtx->pc = 0;` |
|       44 |  6950 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  6951 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6952 | `	/* Allocate a private operand stack */` |
|       44 |  6953 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  6954 | `	if( pStack == 0 ){` |
|      ! 0 |  6955 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6956 | `		return 0;` |
|        - |  6957 | `	}` |
|       44 |  6958 | `	pCtx->pStack = pStack;` |
|        - |  6959 | `	/* Create a detached frame for the fiber */` |
|       44 |  6960 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  6961 | `	if( pFrame == 0 ){` |
|      ! 0 |  6962 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6963 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6964 | `		return 0;` |
|        - |  6965 | `	}` |
|       44 |  6966 | `	pCtx->pFrame = pFrame;` |
|       44 |  6967 | `	return pCtx;` |
|       23 |  6968 |  |
|        - |  6969 | `/*` |
|        - |  6970 | ` * Start executing a fiber context for the first time.` |
|        - |  6971 | ` */` |
|       42 |  6972 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  6973 |  |
|        - |  6974 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6975 | `	sxi32 rc;` |
|       44 |  6976 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6977 | `		return SXERR_INVALID;` |
|        - |  6978 | `	}` |
|        - |  6979 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  6980 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  6981 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6982 | `	/* Save and set the active context */` |
|       44 |  6983 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  6984 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  6985 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  6986 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  6987 | `	pVm->nRecursionDepth++;` |
|        - |  6988 | `	/* Execute from the beginning */` |
|       65 |  6989 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6990 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  6991 | `	pVm->nRecursionDepth--;` |
|        - |  6992 | `	/* Restore the previous context */` |
|       44 |  6993 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  6994 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6995 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  6996 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  6997 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  6998 | `		if( pResult ){` |
|       24 |  6999 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7000 | `		}` |
|       42 |  7001 | `		return SXRET_OK;` |
|        - |  7002 | `	}` |
|        - |  7003 | `	/* Detach frame */` |
|        3 |  7004 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7005 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7006 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7007 | `	}` |
|        3 |  7008 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7009 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7010 | `		return PH7_ABORT;` |
|        - |  7011 | `	}` |
|        3 |  7012 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7013 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7014 | `		return PH7_EXCEPTION;` |
|        - |  7015 | `	}` |
|        - |  7016 | `	/* Normal completion */` |
|        3 |  7017 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7018 | `	if( pResult ){` |
|        3 |  7019 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7020 | `	}` |
|        3 |  7021 | `	return SXRET_OK;` |
|       23 |  7022 |  |
|        - |  7023 | `/*` |
|        - |  7024 | ` * Resume a suspended fiber context.` |
|        - |  7025 | ` */` |
|       86 |  7026 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7027 |  |
|        - |  7028 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7029 | `	sxi32 rc;` |
|       88 |  7030 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7031 | `		return SXERR_INVALID;` |
|        - |  7032 | `	}` |
|        - |  7033 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7034 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7035 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7036 | `	if( pResumeValue ){` |
|       40 |  7037 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7038 | `	}else{` |
|       50 |  7039 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7040 | `	}` |
|       88 |  7041 | `	pCtx->nTos++;` |
|        - |  7042 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7043 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7044 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7045 | `	/* Save and set the active context */` |
|       88 |  7046 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7047 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7048 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7049 | `	pVm->nRecursionDepth++;` |
|        - |  7050 | `	/* Resume execution from saved PC */` |
|      131 |  7051 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7052 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7053 | `	pVm->nRecursionDepth--;` |
|        - |  7054 | `	/* Restore the previous context */` |
|       88 |  7055 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7056 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7057 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7058 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7059 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7060 | `		if( pResult ){` |
|       18 |  7061 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7062 | `		}` |
|       56 |  7063 | `		return SXRET_OK;` |
|        - |  7064 | `	}` |
|        - |  7065 | `	/* Detach frame */` |
|       34 |  7066 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7067 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7068 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7069 | `	}` |
|       34 |  7070 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7071 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7072 | `		return PH7_ABORT;` |
|        - |  7073 | `	}` |
|       34 |  7074 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7075 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7076 | `		return PH7_EXCEPTION;` |
|        - |  7077 | `	}` |
|        - |  7078 | `	/* Normal completion */` |
|       34 |  7079 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7080 | `	if( pResult ){` |
|       20 |  7081 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7082 | `	}` |
|       34 |  7083 | `	return SXRET_OK;` |
|       45 |  7084 |  |
|        - |  7085 | `/*` |
|        - |  7086 | ` * Release an execution context and all its resources.` |
|        - |  7087 | ` */` |
|        4 |  7088 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7089 |  |
|        5 |  7090 | `	if( pCtx == 0 ){` |
|      ! 0 |  7091 | `		return;` |
|        - |  7092 | `	}` |
|        5 |  7093 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7094 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7095 | `		return;` |
|        - |  7096 | `	}` |
|        5 |  7097 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7098 | `	/* Release values */` |
|        5 |  7099 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7100 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7101 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7102 | `	if( pCtx->pFrame ){` |
|        - |  7103 | `		VmSlot *aSlot;` |
|        - |  7104 | `		sxu32 n;` |
|        - |  7105 | `		/* Free local variables */` |
|        5 |  7106 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7107 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7108 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7109 | `		}` |
|        - |  7110 | `		/* Remove local references */` |
|        5 |  7111 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7112 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7113 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7114 | `		}` |
|        5 |  7115 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7116 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7117 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7118 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7119 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7120 | `		pCtx->pFrame = 0;` |
|        2 |  7121 | `	}` |
|        - |  7122 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7123 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7124 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7125 | `	if( pCtx->pStack ){` |
|        5 |  7126 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7127 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7128 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7129 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7130 | `				pTos--;` |
|        1 |  7131 | `			}` |
|        2 |  7132 | `		}` |
|        5 |  7133 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7134 | `		pCtx->pStack = 0;` |
|        2 |  7135 | `	}` |
|        - |  7136 | `	/* Free the context itself */` |
|        5 |  7137 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7138 |  |
|        - |  7139 | `/*` |
|        - |  7140 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7141 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7142 | ` */` |
|       90 |  7143 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7144 |  |
|        - |  7145 | `	ph7_class_instance *pThis;` |
|        - |  7146 | `	SyString sAttr;` |
|        - |  7147 | `	ph7_value *pAttr;` |
|       92 |  7148 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7149 | `		return 0;` |
|        - |  7150 | `	}` |
|       92 |  7151 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7152 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7153 | `		return 0;` |
|        - |  7154 | `	}` |
|       92 |  7155 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7156 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7157 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7158 | `		return 0;` |
|        - |  7159 | `	}` |
|       62 |  7160 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7161 |  |
|        - |  7162 | `/*` |
|        - |  7163 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7164 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7165 | ` */` |
|       38 |  7166 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7167 |  |
|       40 |  7168 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7169 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7170 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7171 | `			"Cannot suspend outside of a fiber");` |
|        - |  7172 | `	}` |
|       40 |  7173 | `	if( nArg > 0 ){` |
|       40 |  7174 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7175 | `	}else{` |
|      ! 0 |  7176 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7177 | `	}` |
|       40 |  7178 | `	return PH7_SUSPEND;` |
|       21 |  7179 |  |
|        - |  7180 | `/*` |
|        - |  7181 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7182 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7183 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7184 | ` */` |
|       24 |  7185 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7186 |  |
|        - |  7187 | `	ph7_class_instance *pThis;` |
|        - |  7188 | `	ph7_value *pAttr;` |
|        - |  7189 | `	SyString sAttrName;` |
|       26 |  7190 | `	if( nArg < 2 ){` |
|      ! 0 |  7191 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7192 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7193 | `	}` |
|       26 |  7194 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7195 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7196 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7197 | `	}` |
|       26 |  7198 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7199 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7200 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7201 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7202 | `	}` |
|        - |  7203 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7204 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7205 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7206 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7207 | `	}` |
|        - |  7208 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7209 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7210 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7211 | `	if( pAttr ){` |
|       26 |  7212 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7213 | `	}` |
|       26 |  7214 | `	return PH7_OK;` |
|       14 |  7215 |  |
|        - |  7216 | `/*` |
|        - |  7217 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7218 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7219 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7220 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7221 | ` */` |
|       24 |  7222 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7223 | `	ph7_class_instance **ppThis)` |
|        2 |  7224 |  |
|       26 |  7225 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7226 | `	ph7_value *pCallable;` |
|        - |  7227 | `	SyString sAttrName;` |
|       26 |  7228 | `	*ppThis = 0;` |
|       26 |  7229 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7230 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7231 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7232 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7233 | `		return 0;` |
|        - |  7234 | `	}` |
|       26 |  7235 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7236 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7237 | `		SyString sName;` |
|        - |  7238 | `		SyHashEntry *pEntry;` |
|        - |  7239 | `		ph7_vm_func *pFunc;` |
|       26 |  7240 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7241 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7242 | `		if( pEntry == 0 ){` |
|      ! 0 |  7243 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7244 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7245 | `			return 0;` |
|        - |  7246 | `		}` |
|       26 |  7247 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7248 | `		return pFunc;` |
|      ! 0 |  7249 | `	}else{` |
|        - |  7250 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7251 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7252 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7253 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7254 | `		if( pMethod == 0 ){` |
|      ! 0 |  7255 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7256 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7257 | `			return 0;` |
|        - |  7258 | `		}` |
|      ! 0 |  7259 | `		*ppThis = pClosure;` |
|      ! 0 |  7260 | `		return &pMethod->sFunc;` |
|        - |  7261 | `	}` |
|       14 |  7262 |  |
|        - |  7263 | `/*` |
|        - |  7264 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7265 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7266 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7267 | ` */` |
|       42 |  7268 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7269 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7270 |  |
|       44 |  7271 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7272 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7273 | `	sxu32 nFormal, n;` |
|        - |  7274 | `	VmSlot sSlot;` |
|        - |  7275 | `	sxi32 rc;` |
|        - |  7276 | `	/* Install $this for closure/method callables */` |
|       44 |  7277 | `	if( pClosureThis ){` |
|        - |  7278 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7279 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7280 | `		if( pObj ){` |
|      ! 0 |  7281 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7282 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7283 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7284 | `		}` |
|      ! 0 |  7285 | `	}` |
|        - |  7286 | `	/* Install static variables */` |
|       44 |  7287 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7288 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7289 | `		ph7_value *pVal;` |
|      ! 0 |  7290 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7291 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7292 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7293 | `			if( pVal ){` |
|      ! 0 |  7294 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7295 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7296 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7297 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7298 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7299 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7300 | `				}` |
|      ! 0 |  7301 | `			}` |
|      ! 0 |  7302 | `		}` |
|      ! 0 |  7303 | `	}` |
|        - |  7304 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7305 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7306 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7307 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7308 | `		ph7_value *pObj;` |
|       12 |  7309 | `		if( n < (sxu32)nArg ){` |
|        - |  7310 | `			/* Argument provided — install with type casting */` |
|       12 |  7311 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7312 | `			if( pObj ){` |
|       12 |  7313 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7314 | `				/* Type casting */` |
|       12 |  7315 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7316 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7317 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7318 | `						if( xCast ){` |
|      ! 0 |  7319 | `							xCast(pObj);` |
|      ! 0 |  7320 | `						}` |
|      ! 0 |  7321 | `					}` |
|      ! 0 |  7322 | `				}` |
|       12 |  7323 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7324 | `				sSlot.pUserData = 0;` |
|       12 |  7325 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7326 | `			}` |
|        5 |  7327 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7328 | `			/* Default value */` |
|      ! 0 |  7329 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7330 | `			if( pObj ){` |
|      ! 0 |  7331 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7332 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7333 | `					return rc;` |
|        - |  7334 | `				}` |
|      ! 0 |  7335 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7336 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7337 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7338 | `						if( xCast ){` |
|      ! 0 |  7339 | `							xCast(pObj);` |
|      ! 0 |  7340 | `						}` |
|      ! 0 |  7341 | `					}` |
|      ! 0 |  7342 | `				}` |
|      ! 0 |  7343 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7344 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7345 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7346 | `			}` |
|      ! 0 |  7347 | `		}` |
|        7 |  7348 | `	}` |
|        - |  7349 | `	/* Install closure environment (captured variables) */` |
|       44 |  7350 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7351 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7352 | `		ph7_value *pValue;` |
|        - |  7353 | `		sxu32 iEnv;` |
|        3 |  7354 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7355 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7356 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7357 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7358 | `				continue;` |
|        - |  7359 | `			}` |
|        5 |  7360 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7361 | `			if( pValue == 0 ){` |
|      ! 0 |  7362 | `				continue;` |
|        - |  7363 | `			}` |
|        5 |  7364 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7365 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7366 | `		}` |
|        1 |  7367 | `	}` |
|       44 |  7368 | `	return SXRET_OK;` |
|       23 |  7369 |  |
|        - |  7370 | `/*` |
|        - |  7371 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7372 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7373 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7374 | ` */` |
|       26 |  7375 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7376 |  |
|       28 |  7377 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7378 | `	ph7_class_instance *pThis;` |
|        - |  7379 | `	ph7_class_instance *pClosureThis;` |
|        - |  7380 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7381 | `	ph7_vm_func *pFunc;` |
|        - |  7382 | `	ph7_value sResult;` |
|        - |  7383 | `	ph7_value *pCtxAttr;` |
|        - |  7384 | `	SyString sAttrName;` |
|        - |  7385 | `	sxi32 rc;` |
|       28 |  7386 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7387 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7388 | `	}` |
|       28 |  7389 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7390 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7391 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7392 | `	if( pExecCtx != 0 ){` |
|        3 |  7393 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7394 | `			"Cannot start a fiber that has already been started");` |
|        - |  7395 | `	}` |
|        - |  7396 | `	/* Resolve callable */` |
|       26 |  7397 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7398 | `	if( pFunc == 0 ){` |
|      ! 0 |  7399 | `		return PH7_EXCEPTION;` |
|        - |  7400 | `	}` |
|        - |  7401 | `	/* Create execution context now that we know the function */` |
|       26 |  7402 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7403 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7404 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7405 | `			"Fiber::start(): out of memory");` |
|        - |  7406 | `	}` |
|        - |  7407 | `	/* Store context in $this->__ctx */` |
|       26 |  7408 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7409 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7410 | `	if( pCtxAttr ){` |
|       26 |  7411 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7412 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7413 | `	}` |
|        - |  7414 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7415 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7416 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7417 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7418 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7419 | `	/* Unpack the args array and install into the frame */` |
|        - |  7420 | `	{` |
|       26 |  7421 | `		ph7_value **apValues = 0;` |
|       26 |  7422 | `		int nActual = 0;` |
|       26 |  7423 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7424 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7425 | `			ph7_hashmap_node *pNode;` |
|       26 |  7426 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7427 | `			if( nCount > 0 ){` |
|        3 |  7428 | `				sxu32 idx = 0;` |
|        4 |  7429 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7430 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7431 | `				if( apValues ){` |
|        3 |  7432 | `					pNode = pMap->pFirst;` |
|        7 |  7433 | `					while( pNode && idx < nCount ){` |
|        5 |  7434 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7435 | `						idx++;` |
|        5 |  7436 | `						pNode = pNode->pPrev;` |
|        1 |  7437 | `					}` |
|        3 |  7438 | `					nActual = (int)idx;` |
|        1 |  7439 | `				}` |
|        1 |  7440 | `			}` |
|       12 |  7441 | `		}` |
|       26 |  7442 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7443 | `		if( apValues ){` |
|        3 |  7444 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7445 | `		}` |
|        - |  7446 | `	}` |
|        - |  7447 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7448 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7449 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7450 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7451 | `		return PH7_ABORT;` |
|        - |  7452 | `	}` |
|       26 |  7453 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7454 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7455 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7456 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7457 | `		return PH7_ABORT;` |
|        - |  7458 | `	}` |
|       26 |  7459 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7460 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7461 | `		return PH7_EXCEPTION;` |
|        - |  7462 | `	}` |
|       26 |  7463 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7464 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7465 | `	return PH7_OK;` |
|       15 |  7466 |  |
|        - |  7467 | `/*` |
|        - |  7468 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7469 | ` */` |
|       36 |  7470 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7471 |  |
|       38 |  7472 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7473 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7474 | `	ph7_value sResult;` |
|        - |  7475 | `	ph7_value *pResumeVal;` |
|        - |  7476 | `	sxi32 rc;` |
|       38 |  7477 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7478 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7479 | `		return PH7_OK;` |
|        - |  7480 | `	}` |
|       38 |  7481 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7482 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7483 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7484 | `		return PH7_OK;` |
|        - |  7485 | `	}` |
|       38 |  7486 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7487 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7488 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7489 | `	}` |
|       36 |  7490 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7491 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7492 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7493 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7494 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7495 | `		return PH7_ABORT;` |
|        - |  7496 | `	}` |
|       36 |  7497 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7498 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7499 | `		return PH7_EXCEPTION;` |
|        - |  7500 | `	}` |
|       36 |  7501 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7502 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7503 | `	return PH7_OK;` |
|       20 |  7504 |  |
|        - |  7505 | `/*` |
|        - |  7506 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7507 | ` */` |
|        6 |  7508 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7509 |  |
|        8 |  7510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7511 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7512 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7513 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7514 | `		return PH7_OK;` |
|        - |  7515 | `	}` |
|        8 |  7516 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7517 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7518 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7519 | `		return PH7_OK;` |
|        - |  7520 | `	}` |
|        8 |  7521 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7522 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7523 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7524 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7525 | `		}` |
|      ! 0 |  7526 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7527 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7528 | `	}` |
|        8 |  7529 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7530 | `	return PH7_OK;` |
|        5 |  7531 |  |
|        - |  7532 | `/*` |
|        - |  7533 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7534 | ` */` |
|        6 |  7535 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7536 |  |
|        - |  7537 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7538 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7539 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7540 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7541 | `	return PH7_OK;` |
|        4 |  7542 |  |
|      ! 0 |  7543 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7544 |  |
|        - |  7545 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7546 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7547 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7548 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7549 | `	return PH7_OK;` |
|      ! 0 |  7550 |  |
|        6 |  7551 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7552 |  |
|        - |  7553 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7554 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7555 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7556 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7557 | `	return PH7_OK;` |
|        4 |  7558 |  |
|        6 |  7559 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7560 |  |
|        - |  7561 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7562 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7563 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7564 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7565 | `	return PH7_OK;` |
|        4 |  7566 |  |
|        - |  7567 | `/*` |
|        - |  7568 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7569 | ` */` |
|        4 |  7570 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7571 |  |
|        5 |  7572 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7573 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7574 | `	if( nArg < 1 ){` |
|      ! 0 |  7575 | `		return PH7_OK;` |
|        - |  7576 | `	}` |
|        5 |  7577 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7578 | `	if( pExecCtx ){` |
|        5 |  7579 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7580 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7581 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7582 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7583 | `			SyString sAttrName;` |
|        - |  7584 | `			ph7_value *pAttr;` |
|        5 |  7585 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7586 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7587 | `			if( pAttr ){` |
|        5 |  7588 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7589 | `			}` |
|        2 |  7590 | `		}` |
|        2 |  7591 | `	}` |
|        5 |  7592 | `	return PH7_OK;` |
|        3 |  7593 |  |
|        - |  7594 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7595 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7596 |  |
|        - |  7597 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7598 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7599 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7600 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7601 |  |
|      ! 0 |  7602 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7603 |  |
|        - |  7604 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7605 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7606 | `	ph7_exec_ctx *pCtx;` |
|        - |  7607 | `	ph7_vm_func *pFunc;` |
|        - |  7608 | `	ph7_value *pCallable;` |
|        - |  7609 | `	ph7_value *pCtxAttr;` |
|        - |  7610 | `	SyString sAttrName;` |
|        - |  7611 | `	/* Must not already be started */` |
|      ! 0 |  7612 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7613 | `	if( pCtx != 0 ){` |
|      ! 0 |  7614 | `		return SXERR_INVALID;` |
|        - |  7615 | `	}` |
|      ! 0 |  7616 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7617 | `		return SXERR_INVALID;` |
|        - |  7618 | `	}` |
|      ! 0 |  7619 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7620 | `	/* Get the callable */` |
|      ! 0 |  7621 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7622 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7623 | `	if( pCallable == 0 ){` |
|      ! 0 |  7624 | `		return SXERR_INVALID;` |
|        - |  7625 | `	}` |
|        - |  7626 | `	/* Resolve callable */` |
|      ! 0 |  7627 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7628 | `		SyString sName;` |
|        - |  7629 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7630 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7631 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7632 | `		if( pEntry == 0 ){` |
|      ! 0 |  7633 | `			return SXERR_NOTFOUND;` |
|        - |  7634 | `		}` |
|      ! 0 |  7635 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7636 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7637 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7638 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7639 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7640 | `		if( pMethod == 0 ){` |
|      ! 0 |  7641 | `			return SXERR_INVALID;` |
|        - |  7642 | `		}` |
|      ! 0 |  7643 | `		pClosureThis = pClosure;` |
|      ! 0 |  7644 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7645 | `	}else{` |
|      ! 0 |  7646 | `		return SXERR_INVALID;` |
|        - |  7647 | `	}` |
|        - |  7648 | `	/* Create context */` |
|      ! 0 |  7649 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7650 | `	if( pCtx == 0 ){` |
|      ! 0 |  7651 | `		return SXERR_MEM;` |
|        - |  7652 | `	}` |
|        - |  7653 | `	/* Store in __ctx */` |
|      ! 0 |  7654 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7655 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7656 | `	if( pCtxAttr ){` |
|      ! 0 |  7657 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7658 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7659 | `	}` |
|        - |  7660 | `	/* Set up frame with args */` |
|      ! 0 |  7661 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7662 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7663 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7664 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7665 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7666 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7667 |  |
|      ! 0 |  7668 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7669 |  |
|      ! 0 |  7670 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7671 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7672 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7673 |  |
|      ! 0 |  7674 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7675 |  |
|      ! 0 |  7676 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7677 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7678 |  |
|      ! 0 |  7679 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7680 |  |
|      ! 0 |  7681 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7682 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7683 |  |
|      ! 0 |  7684 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7685 |  |
|      ! 0 |  7686 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7687 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7688 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7689 |  |
|        - |  7690 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7691 | `/*` |
|        - |  7692 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7693 | ` */` |
|       18 |  7694 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7695 |  |
|        - |  7696 | `	ph7_generator *pGen;` |
|       20 |  7697 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7698 | `	if( pGen == 0 ){` |
|      ! 0 |  7699 | `		return 0;` |
|        - |  7700 | `	}` |
|       20 |  7701 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7702 | `	pGen->pCtx = pCtx;` |
|       20 |  7703 | `	pGen->iImplicitKey = 0;` |
|       20 |  7704 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7705 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7706 | `	/* Link the generator back to the exec context */` |
|       20 |  7707 | `	pCtx->pPrivate = pGen;` |
|       20 |  7708 | `	return pGen;` |
|       11 |  7709 |  |
|        - |  7710 | `/*` |
|        - |  7711 | ` * Release a generator and its execution context.` |
|        - |  7712 | ` */` |
|      ! 0 |  7713 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7714 |  |
|      ! 0 |  7715 | `	if( pGen == 0 ){` |
|      ! 0 |  7716 | `		return;` |
|        - |  7717 | `	}` |
|      ! 0 |  7718 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7719 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7720 | `	if( pGen->pCtx ){` |
|      ! 0 |  7721 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7722 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7723 | `		pGen->pCtx = 0;` |
|      ! 0 |  7724 | `	}` |
|      ! 0 |  7725 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7726 |  |
|        - |  7727 | `/*` |
|        - |  7728 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7729 | ` */` |
|      192 |  7730 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7731 |  |
|        - |  7732 | `	ph7_class_instance *pThis;` |
|        - |  7733 | `	SyString sAttr;` |
|        - |  7734 | `	ph7_value *pAttr;` |
|      194 |  7735 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7736 | `		return 0;` |
|        - |  7737 | `	}` |
|      194 |  7738 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7739 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7740 | `		return 0;` |
|        - |  7741 | `	}` |
|      194 |  7742 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7743 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7744 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7745 | `		return 0;` |
|        - |  7746 | `	}` |
|      194 |  7747 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7748 |  |
|        - |  7749 | `/*` |
|        - |  7750 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7751 | ` */` |
|       18 |  7752 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7753 |  |
|        - |  7754 | `	ph7_generator *pGen;` |
|        - |  7755 | `	sxi32 rc;` |
|       20 |  7756 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7757 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7758 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7759 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7760 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7761 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7762 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7763 | `	}` |
|       20 |  7764 | `	return PH7_OK;` |
|       11 |  7765 |  |
|        - |  7766 | `/*` |
|        - |  7767 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7768 | ` */` |
|       52 |  7769 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7770 |  |
|        - |  7771 | `	ph7_generator *pGen;` |
|       54 |  7772 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7773 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7774 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7775 | `	return PH7_OK;` |
|       28 |  7776 |  |
|        - |  7777 | `/*` |
|        - |  7778 | ` * Generator::current() — return the last yielded value.` |
|        - |  7779 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7780 | ` */` |
|       56 |  7781 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7782 |  |
|        - |  7783 | `	ph7_generator *pGen;` |
|        - |  7784 | `	sxi32 rc;` |
|       58 |  7785 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7786 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7787 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7788 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7789 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7790 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7791 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7792 | `	}` |
|       58 |  7793 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7794 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7795 | `	}else{` |
|      ! 0 |  7796 | `		ph7_result_null(pCtx);` |
|        - |  7797 | `	}` |
|       58 |  7798 | `	return PH7_OK;` |
|       30 |  7799 |  |
|        - |  7800 | `/*` |
|        - |  7801 | ` * Generator::key() — return the last yielded key.` |
|        - |  7802 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7803 | ` */` |
|       12 |  7804 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7805 |  |
|        - |  7806 | `	ph7_generator *pGen;` |
|        - |  7807 | `	sxi32 rc;` |
|       13 |  7808 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7809 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7810 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7811 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7812 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7813 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7814 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7815 | `	}` |
|       13 |  7816 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7817 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7818 | `	}else{` |
|      ! 0 |  7819 | `		ph7_result_null(pCtx);` |
|        - |  7820 | `	}` |
|       13 |  7821 | `	return PH7_OK;` |
|        7 |  7822 |  |
|        - |  7823 | `/*` |
|        - |  7824 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7825 | ` */` |
|       48 |  7826 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7827 |  |
|        - |  7828 | `	ph7_generator *pGen;` |
|        - |  7829 | `	sxi32 rc;` |
|       50 |  7830 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7831 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7832 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7833 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7834 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7835 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7836 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7837 | `	}else{` |
|      ! 0 |  7838 | `		return PH7_OK;` |
|        - |  7839 | `	}` |
|       50 |  7840 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7841 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7842 | `	return PH7_OK;` |
|       26 |  7843 |  |
|        - |  7844 | `/*` |
|        - |  7845 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7846 | ` */` |
|        4 |  7847 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7848 |  |
|        - |  7849 | `	ph7_generator *pGen;` |
|        - |  7850 | `	ph7_value *pSendVal;` |
|        - |  7851 | `	sxi32 rc;` |
|        5 |  7852 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7853 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7854 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7855 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7856 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7857 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7858 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7859 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7860 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7861 | `	}else{` |
|      ! 0 |  7862 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7863 | `		return PH7_OK;` |
|        - |  7864 | `	}` |
|        5 |  7865 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7866 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7867 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7868 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7869 | `	}else{` |
|        3 |  7870 | `		ph7_result_null(pCtx);` |
|        - |  7871 | `	}` |
|        5 |  7872 | `	return PH7_OK;` |
|        3 |  7873 |  |
|        - |  7874 | `/*` |
|        - |  7875 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7876 | ` *` |
|        - |  7877 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7878 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7879 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7880 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7881 | ` * the exception to the caller.` |
|        - |  7882 | ` */` |
|      ! 0 |  7883 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7884 |  |
|        - |  7885 | `	ph7_generator *pGen;` |
|        - |  7886 | `	const char *zMsg;` |
|        - |  7887 | `	int nLen;` |
|      ! 0 |  7888 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7889 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7890 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7891 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7892 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7893 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7894 | `			"Cannot throw into a closed generator");` |
|        - |  7895 | `	}` |
|        - |  7896 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7897 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7898 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7899 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7900 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7901 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7902 | `	nLen = 0;` |
|      ! 0 |  7903 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7904 | `		/* Try to get the exception's message */` |
|        - |  7905 | `		SyString sAttr;` |
|        - |  7906 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7907 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7908 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7909 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7910 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7911 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7912 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7913 | `		}` |
|      ! 0 |  7914 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7915 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7916 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7917 | `	}` |
|      ! 0 |  7918 | `	(void)nLen;` |
|      ! 0 |  7919 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7920 |  |
|        - |  7921 | `/*` |
|        - |  7922 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7923 | ` */` |
|        2 |  7924 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7925 |  |
|        - |  7926 | `	ph7_generator *pGen;` |
|        3 |  7927 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7928 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7929 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7930 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7931 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7932 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7933 | `	}` |
|        3 |  7934 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7935 | `	return PH7_OK;` |
|        2 |  7936 |  |
|        - |  7937 | `/*` |
|        - |  7938 | ` * Generator::__destruct() — clean up.` |
|        - |  7939 | ` */` |
|      ! 0 |  7940 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7941 |  |
|        - |  7942 | `	ph7_generator *pGen;` |
|      ! 0 |  7943 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7944 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7945 | `	if( pGen ){` |
|      ! 0 |  7946 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7947 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7948 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7949 | `			SyString sAttrName;` |
|        - |  7950 | `			ph7_value *pAttr;` |
|      ! 0 |  7951 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7952 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7953 | `			if( pAttr ){` |
|      ! 0 |  7954 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7955 | `			}` |
|      ! 0 |  7956 | `		}` |
|      ! 0 |  7957 | `	}` |
|      ! 0 |  7958 | `	return PH7_OK;` |
|      ! 0 |  7959 |  |
|        - |  7960 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7961 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7962 | `/*` |
|        - |  7963 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7964 | ` * the desired message.` |
|        - |  7965 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7966 | ` * in 'api.c' for additional information.` |
|        - |  7967 | ` */` |
|      370 |  7968 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7969 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7970 | `	SyString *pString /* Message to output */` |
|        - |  7971 | `	)` |
|        2 |  7972 |  |
|      372 |  7973 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  7974 | `	sxi32 rc = SXRET_OK;` |
|        - |  7975 | `	/* Call the output consumer */` |
|      372 |  7976 | `	if( pString->nByte > 0 ){` |
|      372 |  7977 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  7978 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  7979 | `	}` |
|      372 |  7980 | `	return rc;` |
|        2 |  7981 |  |
|        - |  7982 | `/*` |
|        - |  7983 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7984 | ` * callback to consume the formatted message.` |
|        - |  7985 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7986 | ` * in 'api.c' for additional information.` |
|        - |  7987 | ` */` |
|        2 |  7988 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7989 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7990 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7991 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7992 | `	)` |
|        1 |  7993 |  |
|        3 |  7994 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7995 | `	sxi32 rc = SXRET_OK;` |
|        - |  7996 | `	SyBlob sWorker;` |
|        - |  7997 | `	/* Format the message and call the output consumer */` |
|        3 |  7998 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7999 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8000 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8001 | `		/* Consume the formatted message */` |
|        3 |  8002 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8003 | `	}` |
|        3 |  8004 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8005 | `	/* Release the working buffer */` |
|        3 |  8006 | `	SyBlobRelease(&sWorker);` |
|        3 |  8007 | `	return rc;` |
|        1 |  8008 |  |
|        - |  8009 | `/*` |
|        - |  8010 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8011 | ` * This function never fail and always return a pointer` |
|        - |  8012 | ` * to a null terminated string.` |
|        - |  8013 | ` */` |
|       12 |  8014 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8015 |  |
|       13 |  8016 | `	const char *zOp = "Unknown     ";` |
|       13 |  8017 | `	switch(nOp){` |
|        3 |  8018 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8019 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8020 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8021 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8022 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8023 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8024 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8025 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8026 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8027 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8028 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8029 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8030 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8031 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8032 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8033 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8034 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8035 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8036 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8037 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8038 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8039 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8040 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8041 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8042 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8043 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8044 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8045 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8046 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8047 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8048 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8049 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8050 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8051 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8052 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8053 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8054 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8055 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8056 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8057 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8058 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8059 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8060 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8061 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8062 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8063 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8064 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8065 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8066 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8067 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8068 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8069 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8070 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8071 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8072 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8073 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8074 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8075 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8076 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8077 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8078 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8079 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8080 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8081 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8082 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8083 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8084 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8085 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8086 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8087 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8088 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8089 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8090 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8091 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8092 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8093 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8094 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8095 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8096 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8097 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8098 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8099 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8100 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8101 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8102 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8103 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8104 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8105 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8106 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8107 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8108 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8109 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8110 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8111 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8112 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8113 | `	default:` |
|      ! 0 |  8114 | `		break;` |
|        - |  8115 | `	}` |
|       13 |  8116 | `	return zOp;` |
|        1 |  8117 |  |
|        - |  8118 | `/*` |
|        - |  8119 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8120 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8121 | ` * is responsible of consuming the generated dump.` |
|        - |  8122 | ` */` |
|        2 |  8123 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8124 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8125 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8126 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8127 | `	)` |
|        1 |  8128 |  |
|        - |  8129 | `	sxi32 rc;` |
|        3 |  8130 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8131 | `	return rc;` |
|        1 |  8132 |  |
|        - |  8133 | `/*` |
|        - |  8134 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8135 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8136 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8137 | ` * in 'compile.c' for additional information.` |
|        - |  8138 | ` */` |
|        8 |  8139 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8140 |  |
|        9 |  8141 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8142 | `	/* Evaluate and expand constant value */` |
|        9 |  8143 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  8144 |  |
|        - |  8145 | `/*` |
|        - |  8146 | ` * Section:` |
|        - |  8147 | ` *  Function handling functions.` |
|        - |  8148 | ` * Status:` |
|        - |  8149 | ` *    Stable.` |
|        - |  8150 | ` */` |
|        - |  8151 | `/*` |
|        - |  8152 | ` * int func_num_args(void)` |
|        - |  8153 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8154 | ` * Parameters` |
|        - |  8155 | ` *   None.` |
|        - |  8156 | ` * Return` |
|        - |  8157 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8158 | ` *  or -1 if called from the globe scope.` |
|        - |  8159 | ` */` |
|      936 |  8160 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8161 |  |
|        - |  8162 | `	VmFrame *pFrame;` |
|        - |  8163 | `	ph7_vm *pVm;` |
|        - |  8164 | `	/* Point to the target VM */` |
|      938 |  8165 | `	pVm = pCtx->pVm;` |
|        - |  8166 | `	/* Current frame */` |
|      938 |  8167 | `	pFrame = pVm->pFrame;` |
|      938 |  8168 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      938 |  8169 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8170 | `		SXUNUSED(nArg);` |
|      ! 0 |  8171 | `		SXUNUSED(apArg);` |
|        - |  8172 | `		/* Global frame,return -1 */` |
|      ! 0 |  8173 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8174 | `		return SXRET_OK;` |
|        - |  8175 | `	}` |
|        - |  8176 | `	/* Total number of arguments passed to the enclosing function */` |
|      938 |  8177 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      938 |  8178 | `	ph7_result_int(pCtx,nArg);` |
|      938 |  8179 | `	return SXRET_OK;` |
|      470 |  8180 |  |
|        - |  8181 | `/*` |
|        - |  8182 | ` * value func_get_arg(int $arg_num)` |
|        - |  8183 | ` *   Return an item from the argument list.` |
|        - |  8184 | ` * Parameters` |
|        - |  8185 | ` *  Argument number(index start from zero).` |
|        - |  8186 | ` * Return` |
|        - |  8187 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8188 | ` */` |
|       22 |  8189 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8190 |  |
|       24 |  8191 | `	ph7_value *pObj = 0;` |
|       24 |  8192 | `	VmSlot *pSlot = 0;` |
|        - |  8193 | `	VmFrame *pFrame;` |
|        - |  8194 | `	ph7_vm *pVm;` |
|        - |  8195 | `	/* Point to the target VM */` |
|       24 |  8196 | `	pVm = pCtx->pVm;` |
|        - |  8197 | `	/* Current frame */` |
|       24 |  8198 | `	pFrame = pVm->pFrame;` |
|       24 |  8199 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8200 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8201 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8202 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8203 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8204 | `		return SXRET_OK;` |
|        - |  8205 | `	}` |
|        - |  8206 | `	/* Extract the desired index */` |
|       21 |  8207 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8208 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8209 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8210 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8211 | `		return SXRET_OK;` |
|        - |  8212 | `	}` |
|        - |  8213 | `	/* Extract the desired argument */` |
|       21 |  8214 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8215 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8216 | `			/* Return the desired argument */` |
|       21 |  8217 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8218 | `		}else{` |
|        - |  8219 | `			/* No such argument,return false */` |
|      ! 0 |  8220 | `			ph7_result_bool(pCtx,0);` |
|        - |  8221 | `		}` |
|       11 |  8222 | `	}else{` |
|        - |  8223 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8224 | `		ph7_result_bool(pCtx,0);` |
|        - |  8225 | `	}` |
|       21 |  8226 | `	return SXRET_OK;` |
|       13 |  8227 |  |
|        - |  8228 | `/*` |
|        - |  8229 | ` * array func_get_args_byref(void)` |
|        - |  8230 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8231 | ` * Parameters` |
|        - |  8232 | ` *  None.` |
|        - |  8233 | ` * Return` |
|        - |  8234 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8235 | ` *  member of the current user-defined function's argument list.` |
|        - |  8236 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8237 | ` * NOTE:` |
|        - |  8238 | ` *  Arguments are returned to the array by reference.` |
|        - |  8239 | ` */` |
|        2 |  8240 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8241 |  |
|        - |  8242 | `	ph7_value *pArray;` |
|        - |  8243 | `	VmFrame *pFrame;` |
|        - |  8244 | `	VmSlot *aSlot;` |
|        - |  8245 | `	sxu32 n;` |
|        - |  8246 | `	/* Point to the current frame */` |
|        3 |  8247 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8248 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8249 | `	if( pFrame->pParent == 0 ){` |
|        - |  8250 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8251 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8252 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8253 | `		return SXRET_OK;` |
|        - |  8254 | `	}` |
|        - |  8255 | `	/* Create a new array */` |
|        3 |  8256 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8257 | `	if( pArray == 0 ){` |
|      ! 0 |  8258 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8259 | `		SXUNUSED(apArg);` |
|      ! 0 |  8260 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8261 | `		return SXRET_OK;` |
|        - |  8262 | `	}` |
|        - |  8263 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8264 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8265 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8266 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8267 | `	}` |
|        - |  8268 | `	/* Return the freshly created array */` |
|        3 |  8269 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8270 | `	return SXRET_OK;` |
|        2 |  8271 |  |
|        - |  8272 | `/*` |
|        - |  8273 | ` * array func_get_args(void)` |
|        - |  8274 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8275 | ` * Parameters` |
|        - |  8276 | ` *  None.` |
|        - |  8277 | ` * Return` |
|        - |  8278 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8279 | ` *  member of the current user-defined function's argument list.` |
|        - |  8280 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8281 | ` */` |
|       88 |  8282 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8283 |  |
|       90 |  8284 | `	ph7_value *pObj = 0;` |
|        - |  8285 | `	ph7_value *pArray;` |
|        - |  8286 | `	VmFrame *pFrame;` |
|        - |  8287 | `	VmSlot *aSlot;` |
|        - |  8288 | `	sxu32 n;` |
|        - |  8289 | `	/* Point to the current frame */` |
|       90 |  8290 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8291 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8292 | `	if( pFrame->pParent == 0 ){` |
|        - |  8293 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8294 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8295 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8296 | `		return SXRET_OK;` |
|        - |  8297 | `	}` |
|        - |  8298 | `	/* Create a new array */` |
|       90 |  8299 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8300 | `	if( pArray == 0 ){` |
|      ! 0 |  8301 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8302 | `		SXUNUSED(apArg);` |
|      ! 0 |  8303 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8304 | `		return SXRET_OK;` |
|        - |  8305 | `	}` |
|        - |  8306 | `	/* Start filling the array with the given arguments */` |
|       90 |  8307 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8308 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8309 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8310 | `		if( pObj ){` |
|      134 |  8311 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8312 | `		}` |
|       68 |  8313 | `	}` |
|        - |  8314 | `	/* Return the freshly created array */` |
|       90 |  8315 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8316 | `	return SXRET_OK;` |
|       46 |  8317 |  |
|        - |  8318 | `/*` |
|        - |  8319 | ` * bool function_exists(string $name)` |
|        - |  8320 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8321 | ` * Parameters` |
|        - |  8322 | ` *  The name of the desired function.` |
|        - |  8323 | ` * Return` |
|        - |  8324 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8325 | ` */` |
|     1682 |  8326 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8327 |  |
|        - |  8328 | `	const char *zName;` |
|        - |  8329 | `	ph7_vm *pVm;` |
|        - |  8330 | `	int nLen;` |
|        - |  8331 | `	int res;` |
|     1684 |  8332 | `	if( nArg < 1 ){` |
|        - |  8333 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8334 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8335 | `		return SXRET_OK;` |
|        - |  8336 | `	}` |
|        - |  8337 | `	/* Point to the target VM */` |
|     1684 |  8338 | `	pVm = pCtx->pVm;` |
|        - |  8339 | `	/* Extract the function name */` |
|     1684 |  8340 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8341 | `	/* Assume the function is not defined */` |
|     1684 |  8342 | `	res = 0;` |
|        - |  8343 | `	/* Perform the lookup */` |
|     2523 |  8344 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8345 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8346 | `			/* Function is defined */` |
|      206 |  8347 | `			res = 1;` |
|      102 |  8348 | `	}` |
|     1684 |  8349 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8350 | `	return SXRET_OK;` |
|      843 |  8351 |  |
|        - |  8352 | `/*` |
|        - |  8353 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8354 | ` * [i.e: Whether it is callable or not].` |
|        - |  8355 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8356 | ` */` |
|    17546 |  8357 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8358 |  |
|    17548 |  8359 | `	int res = 0;` |
|    17548 |  8360 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8361 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8362 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8363 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8364 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8365 | `		if( pMethod && CallInvoke ){` |
|        - |  8366 | `			ph7_value sResult;` |
|        - |  8367 | `			sxi32 rc;` |
|        - |  8368 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8369 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8370 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8371 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8372 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8373 | `			}` |
|      ! 0 |  8374 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8375 | `		}` |
|    17548 |  8376 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8377 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8378 | `		if( pMap->nEntry == 2 ){` |
|        - |  8379 | `			ph7_class *pClass;` |
|        - |  8380 | `			ph7_value *pV;` |
|        - |  8381 | `			/* Extract the target class */` |
|       12 |  8382 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8383 | `			if( pV ){` |
|       12 |  8384 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8385 | `				if( pClass ){` |
|        - |  8386 | `					ph7_class_method *pMethod;` |
|        - |  8387 | `					/* Extract the target method */` |
|       10 |  8388 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8389 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8390 | `						/* Perform the lookup */` |
|       10 |  8391 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8392 | `						if( pMethod ){` |
|        - |  8393 | `							/* Method is callable */` |
|        5 |  8394 | `							res = 1;` |
|        2 |  8395 | `						}` |
|        4 |  8396 | `					}` |
|        4 |  8397 | `				}` |
|        5 |  8398 | `			}` |
|        7 |  8399 | `		}` |
|    17535 |  8400 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8401 | `		const char *zName;` |
|        - |  8402 | `		int nLen;` |
|        - |  8403 | `		/* Extract the name */` |
|     4972 |  8404 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8405 | `		/* Perform the lookup */` |
|     4987 |  8406 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8407 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8408 | `				/* Function is callable */` |
|     4954 |  8409 | `				res = 1;` |
|     2476 |  8410 | `		}` |
|     2485 |  8411 | `	}` |
|    17548 |  8412 | `	return res;` |
|        2 |  8413 |  |
|        - |  8414 | `/*` |
|        - |  8415 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8416 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8417 | ` * Parameters` |
|        - |  8418 | ` * $name` |
|        - |  8419 | ` *    The callback function to check` |
|        - |  8420 | ` * $syntax_only` |
|        - |  8421 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8422 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8423 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8424 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8425 | ` *    a string.` |
|        - |  8426 | ` * Return` |
|        - |  8427 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8428 | ` */` |
|       14 |  8429 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8430 |  |
|        - |  8431 | `	ph7_vm *pVm;` |
|        - |  8432 | `	int res;` |
|       15 |  8433 | `	if( nArg < 1 ){` |
|        - |  8434 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8435 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8436 | `		return SXRET_OK;` |
|        - |  8437 | `	}` |
|        - |  8438 | `	/* Point to the target VM */` |
|       15 |  8439 | `	pVm = pCtx->pVm;` |
|        - |  8440 | `	/* Perform the requested operation */` |
|       15 |  8441 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8442 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8443 | `	return SXRET_OK;` |
|        8 |  8444 |  |
|        - |  8445 | `/*` |
|        - |  8446 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8447 | ` * defined below.` |
|        - |  8448 | ` */` |
|     1196 |  8449 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8450 |  |
|     1197 |  8451 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8452 | `	ph7_value sName;` |
|        - |  8453 | `	sxi32 rc;` |
|        - |  8454 | `	/* Prepare the function name for insertion */` |
|     1197 |  8455 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1197 |  8456 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8457 | `	/* Perform the insertion */` |
|     1197 |  8458 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1197 |  8459 | `	PH7_MemObjRelease(&sName);` |
|     1197 |  8460 | `	return rc;` |
|        1 |  8461 |  |
|        - |  8462 | `/*` |
|        - |  8463 | ` * array get_defined_functions(void)` |
|        - |  8464 | ` *  Returns an array of all defined functions.` |
|        - |  8465 | ` * Parameter` |
|        - |  8466 | ` *  None.` |
|        - |  8467 | ` * Return` |
|        - |  8468 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8469 | ` *  both built-in (internal) and user-defined.` |
|        - |  8470 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8471 | ` *  defined ones using $arr["user"].` |
|        - |  8472 | ` * Note:` |
|        - |  8473 | ` *  NULL is returned on failure.` |
|        - |  8474 | ` */` |
|        2 |  8475 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8476 |  |
|        - |  8477 | `	ph7_value *pArray,*pEntry;` |
|        - |  8478 | `	/* NOTE:` |
|        - |  8479 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8480 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8481 | `	 */` |
|        3 |  8482 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8483 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8484 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8485 | `		SXUNUSED(apArg);` |
|        - |  8486 | `		/* Return NULL */` |
|      ! 0 |  8487 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8488 | `		return SXRET_OK;` |
|        - |  8489 | `	}` |
|        3 |  8490 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8491 | `	if( pEntry == 0 ){` |
|        - |  8492 | `		/* Return NULL */` |
|      ! 0 |  8493 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8494 | `		return SXRET_OK;` |
|        - |  8495 | `	}` |
|        - |  8496 | `	/* Fill with the appropriate information */` |
|        3 |  8497 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8498 | `	/* Create the 'internal' index */` |
|        3 |  8499 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8500 | `	/* Create the user-func array */` |
|        3 |  8501 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8502 | `	if( pEntry == 0 ){` |
|        - |  8503 | `		/* Return NULL */` |
|      ! 0 |  8504 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8505 | `		return SXRET_OK;` |
|        - |  8506 | `	}` |
|        - |  8507 | `	/* Fill with the appropriate information */` |
|        3 |  8508 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8509 | `	/* Create the 'user' index */` |
|        3 |  8510 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8511 | `	/* Return the multi-dimensional array */` |
|        3 |  8512 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8513 | `	return SXRET_OK;` |
|        2 |  8514 |  |
|        - |  8515 | `/*` |
|        - |  8516 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8517 | ` *  Register a function for execution on shutdown.` |
|        - |  8518 | ` * Note` |
|        - |  8519 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8520 | ` *  be called in the same order as they were registered.` |
|        - |  8521 | ` * Parameters` |
|        - |  8522 | ` *  $callback` |
|        - |  8523 | ` *   The shutdown callback to register.` |
|        - |  8524 | ` * $param` |
|        - |  8525 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8526 | ` * Return` |
|        - |  8527 | ` *  Nothing.` |
|        - |  8528 | ` */` |
|        2 |  8529 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8530 |  |
|        - |  8531 | `	VmShutdownCB sEntry;` |
|        - |  8532 | `	int i,j;` |
|        3 |  8533 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8534 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8535 | `		return PH7_OK;` |
|        - |  8536 | `	}` |
|        - |  8537 | `	/* Zero the Entry */` |
|        3 |  8538 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8539 | `	/* Initialize fields */` |
|        3 |  8540 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8541 | `	/* Save the callback name for later invocation name */` |
|        3 |  8542 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8543 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8544 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8545 | `	}` |
|        - |  8546 | `	/* Copy arguments */` |
|        3 |  8547 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8548 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8549 | `			/* Limit reached */` |
|      ! 0 |  8550 | `			break;` |
|        - |  8551 | `		}` |
|      ! 0 |  8552 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8553 | `	}` |
|        3 |  8554 | `	sEntry.nArg = j;` |
|        - |  8555 | `	/* Install the callback */` |
|        3 |  8556 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8557 | `	return PH7_OK;` |
|        2 |  8558 |  |
|        - |  8559 | `/*` |
|        - |  8560 | ` * Section:` |
|        - |  8561 | ` *  Class handling functions.` |
|        - |  8562 | ` * Status:` |
|        - |  8563 | ` *    Stable.` |
|        - |  8564 | ` */` |
|        - |  8565 | `/*` |
|        - |  8566 | ` * Extract the top active class. NULL is returned` |
|        - |  8567 | ` * if the class stack is empty.` |
|        - |  8568 | ` */` |
|      566 |  8569 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8570 |  |
|      568 |  8571 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8572 | `	ph7_class **apClass;` |
|      568 |  8573 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8574 | `		/* Empty stack,return NULL */` |
|       15 |  8575 | `		return 0;` |
|        - |  8576 | `	}` |
|        - |  8577 | `	/* Peek the last entry */` |
|      554 |  8578 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      554 |  8579 | `	return apClass[pSet->nUsed - 1];` |
|      285 |  8580 |  |
|        - |  8581 | `/*` |
|        - |  8582 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8583 | ` *   Get the class that declared the currently executing method.` |
|        - |  8584 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8585 | ` *` |
|        - |  8586 | ` * Parameters` |
|        - |  8587 | ` *   pVm: Target VM` |
|        - |  8588 | ` *` |
|        - |  8589 | ` * Return` |
|        - |  8590 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8591 | ` *   - Not executing within a class method` |
|        - |  8592 | ` *` |
|        - |  8593 | ` * Note` |
|        - |  8594 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8595 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8596 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8597 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8598 | ` *   declaring class.` |
|        - |  8599 | ` */` |
|       60 |  8600 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8601 |  |
|       62 |  8602 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8603 | `	ph7_vm_func *pVmFunc;` |
|        - |  8604 |  |
|        - |  8605 | `	/* Skip exception frames to find the actual method frame */` |
|       62 |  8606 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8607 |  |
|        - |  8608 | `	/* Check if we're in a method context */` |
|       62 |  8609 | `	if( pFrame->pParent ){` |
|       58 |  8610 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       58 |  8611 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8612 | `			/* Return the declaring class */` |
|       58 |  8613 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8614 | `		}` |
|      ! 0 |  8615 | `	}` |
|        - |  8616 |  |
|        5 |  8617 | `	return 0;` |
|       32 |  8618 |  |
|        - |  8619 |  |
|        - |  8620 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8621 | `/*` |
|        - |  8622 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8623 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8624 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8625 | ` * return value indicates failure.` |
|        - |  8626 | ` */` |
|     1492 |  8627 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8628 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8629 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8630 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8631 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8632 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8633 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8634 | `	)` |
|        2 |  8635 |  |
|        - |  8636 | `	ph7_value *aStack;` |
|        - |  8637 | `	VmInstr aInstr[2];` |
|        - |  8638 | `	int iCursor;` |
|        - |  8639 | `	int i;` |
|        - |  8640 | `	/* Create a new operand stack */` |
|     1494 |  8641 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1494 |  8642 | `	if( aStack == 0 ){` |
|      ! 0 |  8643 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8644 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8645 | `		return SXERR_MEM;` |
|        - |  8646 | `	}` |
|        - |  8647 | `	/* Fill the operand stack with the given arguments */` |
|     2100 |  8648 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      608 |  8649 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8650 | `		/*` |
|        - |  8651 | `		 * Symisc eXtension:` |
|        - |  8652 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8653 | `		 */` |
|      608 |  8654 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      305 |  8655 | `	}` |
|     1494 |  8656 | `	iCursor = nArg + 1;` |
|     1494 |  8657 | `	if( pThis ){` |
|        - |  8658 | `		/*` |
|        - |  8659 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8660 | `		 */` |
|     1488 |  8661 | `		pThis->iRef++; /* Increment reference count */` |
|     1488 |  8662 | `		aStack[i].x.pOther = pThis;` |
|     1488 |  8663 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      743 |  8664 | `	}` |
|     1494 |  8665 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1494 |  8666 | `	i++;` |
|        - |  8667 | `	/* Push method name */` |
|     1494 |  8668 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1494 |  8669 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1494 |  8670 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1494 |  8671 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8672 | `	/* Emit the CALL istruction */` |
|     1494 |  8673 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1494 |  8674 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1494 |  8675 | `	aInstr[0].iP2 = 0;` |
|     1494 |  8676 | `	aInstr[0].p3  = 0;` |
|        - |  8677 | `	/* Emit the DONE instruction */` |
|     1494 |  8678 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1494 |  8679 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1494 |  8680 | `	aInstr[1].iP2 = 0;` |
|     1494 |  8681 | `	aInstr[1].p3  = 0;` |
|        - |  8682 | `	/* Execute the method body (if available) */` |
|     1494 |  8683 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8684 | `	/* Clean up the mess left behind */` |
|     1494 |  8685 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1494 |  8686 | `	return PH7_OK;` |
|      748 |  8687 |  |
|        - |  8688 | `/*` |
|        - |  8689 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8690 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8691 | ` * in the apArg[] array.` |
|        - |  8692 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8693 | ` * return value indicates failure.` |
|        - |  8694 | ` */` |
|      952 |  8695 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8696 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8697 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8698 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8699 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8700 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8701 | `	)` |
|        2 |  8702 |  |
|        - |  8703 | `	ph7_value *aStack;` |
|        - |  8704 | `	VmInstr aInstr[2];` |
|        - |  8705 | `	int i;` |
|      954 |  8706 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8707 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8708 | `		if( pResult ){` |
|        - |  8709 | `			/* Assume a null return value */` |
|      ! 0 |  8710 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8711 | `		}` |
|      471 |  8712 | `		return SXERR_INVALID;` |
|        - |  8713 | `	}` |
|      484 |  8714 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8715 | `		/* Class method */` |
|       11 |  8716 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8717 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8718 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8719 | `		ph7_class *pClass = 0;` |
|        - |  8720 | `		ph7_value *pValue;` |
|        - |  8721 | `		sxi32 rc;` |
|       11 |  8722 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8723 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8724 | `			if( pResult ){` |
|        - |  8725 | `				/* Assume a null return value */` |
|      ! 0 |  8726 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8727 | `			}` |
|      ! 0 |  8728 | `			return SXRET_OK;` |
|        - |  8729 | `		}` |
|        - |  8730 | `		/* Extract the class name or an instance of it */` |
|       11 |  8731 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8732 | `		if( pValue ){` |
|       11 |  8733 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8734 | `		}` |
|       11 |  8735 | `		if( pClass == 0 ){` |
|        - |  8736 | `			/* No such class,return NULL */` |
|      ! 0 |  8737 | `			if( pResult ){` |
|      ! 0 |  8738 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8739 | `			}` |
|      ! 0 |  8740 | `			return SXRET_OK;` |
|        - |  8741 | `		}` |
|       11 |  8742 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8743 | `			/* Point to the class instance */` |
|        5 |  8744 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8745 | `		}` |
|        - |  8746 | `		/* Try to extract the method */` |
|       11 |  8747 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8748 | `		if( pValue ){` |
|       11 |  8749 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8750 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8751 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8752 | `			}` |
|        5 |  8753 | `		}` |
|       11 |  8754 | `		if( pMethod == 0 ){` |
|        - |  8755 | `			/* No such method,return NULL */` |
|      ! 0 |  8756 | `			if( pResult ){` |
|      ! 0 |  8757 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8758 | `			}` |
|      ! 0 |  8759 | `			return SXRET_OK;` |
|        - |  8760 | `		}` |
|        - |  8761 | `		/* Call the class method */` |
|       11 |  8762 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8763 | `		return rc;` |
|        - |  8764 | `	}` |
|        - |  8765 | `	/* Create a new operand stack */` |
|      474 |  8766 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8767 | `	if( aStack == 0 ){` |
|      ! 0 |  8768 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8769 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8770 | `		if( pResult ){` |
|        - |  8771 | `			/* Assume a null return value */` |
|      ! 0 |  8772 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8773 | `		}` |
|      ! 0 |  8774 | `		return SXERR_MEM;` |
|        - |  8775 | `	}` |
|        - |  8776 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8777 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8778 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8779 | `		/*` |
|        - |  8780 | `		 * Symisc eXtension:` |
|        - |  8781 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8782 | `		 */` |
|     1050 |  8783 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8784 | `	}` |
|        - |  8785 | `	/* Push the function name */` |
|      474 |  8786 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8787 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8788 | `	/* Emit the CALL istruction */` |
|      474 |  8789 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8790 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8791 | `	aInstr[0].iP2 = 0;` |
|      474 |  8792 | `	aInstr[0].p3  = 0;` |
|        - |  8793 | `	/* Emit the DONE instruction */` |
|      474 |  8794 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8795 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8796 | `	aInstr[1].iP2 = 0;` |
|      474 |  8797 | `	aInstr[1].p3  = 0;` |
|        - |  8798 | `	/* Execute the function body (if available) */` |
|      474 |  8799 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8800 | `	/* Clean up the mess left behind */` |
|      474 |  8801 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8802 | `	return PH7_OK;` |
|      478 |  8803 |  |
|        - |  8804 | `/*` |
|        - |  8805 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8806 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8807 | ` * parameter.` |
|        - |  8808 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8809 | ` * return value indicates failure.` |
|        - |  8810 | ` */` |
|      236 |  8811 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8812 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8813 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8814 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8815 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8816 | `	)` |
|        1 |  8817 |  |
|        - |  8818 | `	ph7_value *pArg;` |
|        - |  8819 | `	SySet aArg;` |
|        - |  8820 | `	va_list ap;` |
|        - |  8821 | `	sxi32 rc;` |
|      237 |  8822 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8823 | `	/* Copy arguments one after one */` |
|      237 |  8824 | `	va_start(ap,pResult);` |
|      393 |  8825 | `	for(;;){` |
|      787 |  8826 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8827 | `		if( pArg == 0 ){` |
|      237 |  8828 | `			break;` |
|        - |  8829 | `		}` |
|      551 |  8830 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8831 | `	}` |
|        - |  8832 | `	/* Call the core routine */` |
|      237 |  8833 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8834 | `	/* Cleanup */` |
|      237 |  8835 | `	SySetRelease(&aArg);` |
|      237 |  8836 | `	return rc;` |
|        1 |  8837 |  |
|        - |  8838 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8839 | `/*` |
|        - |  8840 | ` * bool defined(string $name)` |
|        - |  8841 | ` *  Checks whether a given named constant exists.` |
|        - |  8842 | ` * Parameter:` |
|        - |  8843 | ` *  Name of the desired constant.` |
|        - |  8844 | ` * Return` |
|        - |  8845 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8846 | ` */` |
|       14 |  8847 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8848 |  |
|        - |  8849 | `	const char *zName;` |
|       16 |  8850 | `	int nLen = 0;` |
|       16 |  8851 | `	int res = 0;` |
|       16 |  8852 | `	if( nArg < 1 ){` |
|        - |  8853 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8854 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8855 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8856 | `		return SXRET_OK;` |
|        - |  8857 | `	}` |
|        - |  8858 | `	/* Extract constant name */` |
|       16 |  8859 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8860 | `	/* Perform the lookup */` |
|       16 |  8861 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8862 | `		/* Already defined */` |
|       10 |  8863 | `		res = 1;` |
|        4 |  8864 | `	}` |
|       16 |  8865 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8866 | `	return SXRET_OK;` |
|        9 |  8867 |  |
|        - |  8868 | `/*` |
|        - |  8869 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8870 | ` * below.` |
|        - |  8871 | ` */` |
|        8 |  8872 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8873 |  |
|       10 |  8874 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8875 | `	/* Expand constant value */` |
|       10 |  8876 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8877 |  |
|        - |  8878 | `/*` |
|        - |  8879 | ` * bool define(string $constant_name,expression value)` |
|        - |  8880 | ` *  Defines a named constant at runtime.` |
|        - |  8881 | ` * Parameter:` |
|        - |  8882 | ` *  $constant_name` |
|        - |  8883 | ` *   The name of the constant` |
|        - |  8884 | ` *  $value` |
|        - |  8885 | ` *   Constant value` |
|        - |  8886 | ` * Return:` |
|        - |  8887 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8888 | ` */` |
|       10 |  8889 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8890 |  |
|        - |  8891 | `	const char *zName;  /* Constant name */` |
|        - |  8892 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8893 | `	int nLen = 0;       /* Name length */` |
|        - |  8894 | `	sxi32 rc;` |
|       12 |  8895 | `	if( nArg < 2 ){` |
|        - |  8896 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8897 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8898 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8899 | `		return SXRET_OK;` |
|        - |  8900 | `	}` |
|       12 |  8901 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8902 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8903 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8904 | `		return SXRET_OK;` |
|        - |  8905 | `	}` |
|        - |  8906 | `	/* Extract constant name */` |
|       12 |  8907 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8908 | `	if( nLen < 1 ){` |
|      ! 0 |  8909 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8910 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8911 | `		return SXRET_OK;` |
|        - |  8912 | `	}` |
|        - |  8913 | `	/* Duplicate constant value */` |
|       12 |  8914 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8915 | `	if( pValue == 0 ){` |
|      ! 0 |  8916 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8917 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8918 | `		return SXRET_OK;` |
|        - |  8919 | `	}` |
|        - |  8920 | `	/* Initialize the memory object */` |
|       12 |  8921 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8922 | `	/* Register the constant */` |
|       12 |  8923 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8924 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8925 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8926 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8927 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8928 | `		return SXRET_OK;` |
|        - |  8929 | `	}` |
|        - |  8930 | `	/* Duplicate constant value */` |
|       12 |  8931 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8932 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8933 | `		/* Lower case the constant name */` |
|      ! 0 |  8934 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8935 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8936 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8937 | `				/* UTF-8 stream */` |
|      ! 0 |  8938 | `				zCur++;` |
|      ! 0 |  8939 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8940 | `					zCur++;` |
|      ! 0 |  8941 | `				}` |
|      ! 0 |  8942 | `				continue;` |
|        - |  8943 | `			}` |
|      ! 0 |  8944 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8945 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8946 | `				zCur[0] = (char)c;` |
|      ! 0 |  8947 | `			}` |
|      ! 0 |  8948 | `			zCur++;` |
|      ! 0 |  8949 | `		}` |
|        - |  8950 | `		/* Finally,register the constant */` |
|      ! 0 |  8951 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8952 | `	}` |
|        - |  8953 | `	/* All done,return TRUE */` |
|       12 |  8954 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8955 | `	return SXRET_OK;` |
|        7 |  8956 |  |
|        - |  8957 | `/*` |
|        - |  8958 | ` * value constant(string $name)` |
|        - |  8959 | ` *  Returns the value of a constant` |
|        - |  8960 | ` * Parameter` |
|        - |  8961 | ` *  $name` |
|        - |  8962 | ` *    Name of the constant.` |
|        - |  8963 | ` * Return` |
|        - |  8964 | ` *  Constant value or NULL if not defined.` |
|        - |  8965 | ` */` |
|        8 |  8966 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8967 |  |
|        - |  8968 | `	SyHashEntry *pEntry;` |
|        - |  8969 | `	ph7_constant *pCons;` |
|        - |  8970 | `	const char *zName; /* Constant name */` |
|        - |  8971 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8972 | `	int nLen;` |
|       10 |  8973 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8974 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8975 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8976 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8977 | `		return SXRET_OK;` |
|        - |  8978 | `	}` |
|        - |  8979 | `	/* Extract the constant name */` |
|       10 |  8980 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8981 | `	/* Perform the query */` |
|       10 |  8982 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8983 | `	if( pEntry == 0 ){` |
|        3 |  8984 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8985 | `		ph7_result_null(pCtx);` |
|        3 |  8986 | `		return SXRET_OK;` |
|        - |  8987 | `	}` |
|        8 |  8988 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8989 | `	/* Point to the structure that describe the constant */` |
|        8 |  8990 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8991 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8992 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8993 | `	/* Return that value */` |
|        8 |  8994 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8995 | `	/* Cleanup */` |
|        8 |  8996 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8997 | `	return SXRET_OK;` |
|        6 |  8998 |  |
|        - |  8999 | `/*` |
|        - |  9000 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9001 | ` * defined below.` |
|        - |  9002 | ` */` |
|      444 |  9003 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9004 |  |
|      445 |  9005 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9006 | `	ph7_value sName;` |
|        - |  9007 | `	sxi32 rc;` |
|        - |  9008 | `	/* Prepare the constant name for insertion */` |
|      445 |  9009 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      445 |  9010 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9011 | `	/* Perform the insertion */` |
|      445 |  9012 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      445 |  9013 | `	PH7_MemObjRelease(&sName);` |
|      445 |  9014 | `	return rc;` |
|        1 |  9015 |  |
|        - |  9016 | `/*` |
|        - |  9017 | ` * array get_defined_constants(void)` |
|        - |  9018 | ` *  Returns an associative array with the names of all defined` |
|        - |  9019 | ` *  constants.` |
|        - |  9020 | ` * Parameters` |
|        - |  9021 | ` *  NONE.` |
|        - |  9022 | ` * Returns` |
|        - |  9023 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9024 | ` */` |
|        2 |  9025 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9026 |  |
|        - |  9027 | `	ph7_value *pArray;` |
|        - |  9028 | `	/* Create the array first*/` |
|        3 |  9029 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9030 | `	if( pArray == 0 ){` |
|      ! 0 |  9031 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9032 | `		SXUNUSED(apArg);` |
|        - |  9033 | `		/* Return NULL */` |
|      ! 0 |  9034 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9035 | `		return SXRET_OK;` |
|        - |  9036 | `	}` |
|        - |  9037 | `	/* Fill the array with the defined constants */` |
|        3 |  9038 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9039 | `	/* Return the created array */` |
|        3 |  9040 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9041 | `	return SXRET_OK;` |
|        2 |  9042 |  |
|        - |  9043 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9044 | `/*` |
|        - |  9045 | ` * Section:` |
|        - |  9046 | ` *  Random numbers/string generators.` |
|        - |  9047 | ` * Status:` |
|        - |  9048 | ` *    Stable.` |
|        - |  9049 | ` */` |
|        - |  9050 | `/*` |
|        - |  9051 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9052 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9053 | ` * used by te SQLite3 library.` |
|        - |  9054 | ` */` |
|     2389 |  9055 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9056 |  |
|        - |  9057 | `	sxu32 iNum;` |
|     2391 |  9058 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2391 |  9059 | `	return iNum;` |
|        2 |  9060 |  |
|        - |  9061 | `/*` |
|        - |  9062 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9063 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9064 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9065 | ` * by te SQLite3 library.` |
|        - |  9066 | ` */` |
|   124246 |  9067 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9068 |  |
|        - |  9069 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9070 | `	int i;` |
|        - |  9071 | `	/* Generate a binary string first */` |
|   124248 |  9072 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9073 | `	/* Turn the binary string into english based alphabet */` |
|  1366876 |  9074 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1242630 |  9075 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   621316 |  9076 | `	 }` |
|   124248 |  9077 |  |
|        - |  9078 | `/*` |
|        - |  9079 | ` * int rand()` |
|        - |  9080 | ` * int mt_rand()` |
|        - |  9081 | ` * int rand(int $min,int $max)` |
|        - |  9082 | ` * int mt_rand(int $min,int $max)` |
|        - |  9083 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9084 | ` * Parameter` |
|        - |  9085 | ` *  $min` |
|        - |  9086 | ` *    The lowest value to return (default: 0)` |
|        - |  9087 | ` *  $max` |
|        - |  9088 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9089 | ` * Return` |
|        - |  9090 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9091 | ` * Note:` |
|        - |  9092 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9093 | ` *  by te SQLite3 library.` |
|        - |  9094 | ` */` |
|       20 |  9095 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9096 |  |
|        - |  9097 | `	sxu32 iNum;` |
|        - |  9098 | `	/* Generate the random number */` |
|       21 |  9099 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9100 | `	if( nArg > 1 ){` |
|        - |  9101 | `		sxu32 iMin,iMax;` |
|        3 |  9102 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9103 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9104 | `		if( iMin < iMax ){` |
|        3 |  9105 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9106 | `			if( iDiv > 0 ){` |
|        3 |  9107 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9108 | `			}` |
|        1 |  9109 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9110 | `			iNum %= iMax;` |
|      ! 0 |  9111 | `		}` |
|        1 |  9112 | `	}` |
|        - |  9113 | `	/* Return the number */` |
|       21 |  9114 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9115 | `	return SXRET_OK;` |
|        1 |  9116 |  |
|        - |  9117 | `/*` |
|        - |  9118 | ` * int getrandmax(void)` |
|        - |  9119 | ` * int mt_getrandmax(void)` |
|        - |  9120 | ` * int rc4_getrandmax(void)` |
|        - |  9121 | ` *   Show largest possible random value` |
|        - |  9122 | ` * Return` |
|        - |  9123 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9124 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9125 | ` * Note:` |
|        - |  9126 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9127 | ` *  by te SQLite3 library.` |
|        - |  9128 | ` */` |
|        4 |  9129 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9130 |  |
|        2 |  9131 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9132 | `	SXUNUSED(apArg);` |
|        5 |  9133 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9134 | `	return SXRET_OK;` |
|        1 |  9135 |  |
|        - |  9136 | `/*` |
|        - |  9137 | ` * string rand_str()` |
|        - |  9138 | ` * string rand_str(int $len)` |
|        - |  9139 | ` *  Generate a random string (English alphabet).` |
|        - |  9140 | ` * Parameter` |
|        - |  9141 | ` *  $len` |
|        - |  9142 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9143 | ` * Return` |
|        - |  9144 | ` *   A pseudo random string.` |
|        - |  9145 | ` * Note:` |
|        - |  9146 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9147 | ` *  by te SQLite3 library.` |
|        - |  9148 | ` *  This function is a symisc extension.` |
|        - |  9149 | ` */` |
|      120 |  9150 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9151 |  |
|        - |  9152 | `	char zString[1024];` |
|      122 |  9153 | `	int iLen = 0x10;` |
|      122 |  9154 | `	if( nArg > 0 ){` |
|        - |  9155 | `		/* Get the desired length */` |
|      122 |  9156 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9157 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9158 | `			/* Default length */` |
|        3 |  9159 | `			iLen = 0x10;` |
|        1 |  9160 | `		}` |
|       60 |  9161 | `	}` |
|        - |  9162 | `	/* Generate the random string */` |
|      122 |  9163 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9164 | `	/* Return the generated string */` |
|      122 |  9165 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9166 | `	return SXRET_OK;` |
|        2 |  9167 |  |
|        - |  9168 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9169 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9170 | `/* Unique ID private data */` |
|        - |  9171 | `struct unique_id_data` |
|        - |  9172 |  |
|        - |  9173 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9174 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9175 | `};` |
|        - |  9176 | `/*` |
|        - |  9177 | ` * Binary to hex consumer callback.` |
|        - |  9178 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9179 | ` * defined below.` |
|        - |  9180 | ` */` |
|      192 |  9181 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9182 |  |
|      193 |  9183 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9184 | `	sxu32 nBuflen;` |
|        - |  9185 | `	/* Extract result buffer length */` |
|      193 |  9186 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9187 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9188 | `			/*` |
|        - |  9189 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9190 | `			 * string will be 13 characters long` |
|        - |  9191 | `			 */` |
|       25 |  9192 | `		return SXERR_ABORT;` |
|        - |  9193 | `	}` |
|      169 |  9194 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9195 | `		return SXERR_ABORT;` |
|        - |  9196 | `	}` |
|        - |  9197 | `	/* Safely Consume the hex stream */` |
|      169 |  9198 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9199 | `	return SXRET_OK;` |
|       97 |  9200 |  |
|        - |  9201 | `/*` |
|        - |  9202 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9203 | ` *  Generate a unique ID` |
|        - |  9204 | ` * Parameter` |
|        - |  9205 | ` * $prefix` |
|        - |  9206 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9207 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9208 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9209 | ` * $more_entropy` |
|        - |  9210 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9211 | ` *  that the result will be unique.` |
|        - |  9212 | ` * Return` |
|        - |  9213 | ` *  Returns the unique identifier, as a string.` |
|        - |  9214 | ` */` |
|       24 |  9215 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9216 |  |
|        - |  9217 | `	struct unique_id_data sUniq;` |
|        - |  9218 | `	unsigned char zDigest[20];` |
|       25 |  9219 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9220 | `	const char *zPrefix;` |
|        - |  9221 | `	SHA1Context sCtx;` |
|        - |  9222 | `	char zRandom[7];` |
|        - |  9223 | `	int nPrefix;` |
|        - |  9224 | `	int entropy;` |
|        - |  9225 | `	/* Generate a random string first */` |
|       25 |  9226 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9227 | `	/* Initialize fields */` |
|       25 |  9228 | `	zPrefix = 0;` |
|       25 |  9229 | `	nPrefix = 0;` |
|       25 |  9230 | `	entropy = 0;` |
|       25 |  9231 | `	if( nArg > 0 ){` |
|        - |  9232 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9233 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9234 | `		if( nArg > 1 ){` |
|      ! 0 |  9235 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9236 | `		}` |
|      ! 0 |  9237 | `	}` |
|       25 |  9238 | `	SHA1Init(&sCtx);` |
|        - |  9239 | `	/* Generate the random ID */` |
|       25 |  9240 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9241 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9242 | `	}` |
|        - |  9243 | `	/* Append the random ID */` |
|       25 |  9244 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9245 | `	/* Append the random string */` |
|       25 |  9246 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9247 | `	/* Increment the number */` |
|       25 |  9248 | `	pVm->unique_id++;` |
|       25 |  9249 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9250 | `	/* Hexify the digest */` |
|       25 |  9251 | `	sUniq.pCtx = pCtx;` |
|       25 |  9252 | `	sUniq.entropy = entropy;` |
|       25 |  9253 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9254 | `	/* All done */` |
|       25 |  9255 | `	return PH7_OK;` |
|        1 |  9256 |  |
|        - |  9257 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9258 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9259 | `/*` |
|        - |  9260 | ` * Section:` |
|        - |  9261 | ` *  Language construct implementation as foreign functions.` |
|        - |  9262 | ` * Status:` |
|        - |  9263 | ` *    Stable.` |
|        - |  9264 | ` */` |
|        - |  9265 | `/*` |
|        - |  9266 | ` * void echo($string...)` |
|        - |  9267 | ` *  Output one or more messages.` |
|        - |  9268 | ` * Parameters` |
|        - |  9269 | ` *  $string` |
|        - |  9270 | ` *   Message to output.` |
|        - |  9271 | ` * Return` |
|        - |  9272 | ` *  NULL.` |
|        - |  9273 | ` */` |
|      ! 0 |  9274 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9275 |  |
|        - |  9276 | `	const char *zData;` |
|      ! 0 |  9277 | `	int nDataLen = 0;` |
|        - |  9278 | `	ph7_vm *pVm;` |
|        - |  9279 | `	int i,rc;` |
|        - |  9280 | `	/* Point to the target VM */` |
|      ! 0 |  9281 | `	pVm = pCtx->pVm;` |
|        - |  9282 | `	/* Output */` |
|      ! 0 |  9283 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9284 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9285 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9286 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9287 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9288 | `			if( rc == SXERR_ABORT ){` |
|        - |  9289 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9290 | `				return PH7_ABORT;` |
|        - |  9291 | `			}` |
|      ! 0 |  9292 | `		}` |
|      ! 0 |  9293 | `	}` |
|      ! 0 |  9294 | `	return SXRET_OK;` |
|      ! 0 |  9295 |  |
|        - |  9296 | `/*` |
|        - |  9297 | ` * int print($string...)` |
|        - |  9298 | ` *  Output one or more messages.` |
|        - |  9299 | ` * Parameters` |
|        - |  9300 | ` *  $string` |
|        - |  9301 | ` *   Message to output.` |
|        - |  9302 | ` * Return` |
|        - |  9303 | ` *  1 always.` |
|        - |  9304 | ` */` |
|        2 |  9305 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9306 |  |
|        - |  9307 | `	const char *zData;` |
|        3 |  9308 | `	int nDataLen = 0;` |
|        - |  9309 | `	ph7_vm *pVm;` |
|        - |  9310 | `	int i,rc;` |
|        - |  9311 | `	/* Point to the target VM */` |
|        3 |  9312 | `	pVm = pCtx->pVm;` |
|        - |  9313 | `	/* Output */` |
|        5 |  9314 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9315 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9316 | `		if( nDataLen > 0 ){` |
|        3 |  9317 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9318 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9319 | `			if( rc == SXERR_ABORT ){` |
|        - |  9320 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9321 | `				return PH7_ABORT;` |
|        - |  9322 | `			}` |
|        1 |  9323 | `		}` |
|        2 |  9324 | `	}` |
|        - |  9325 | `	/* Return 1 */` |
|        3 |  9326 | `	ph7_result_int(pCtx,1);` |
|        3 |  9327 | `	return SXRET_OK;` |
|        2 |  9328 |  |
|        - |  9329 | `/*` |
|        - |  9330 | ` * void exit(string $msg)` |
|        - |  9331 | ` * void exit(int $status)` |
|        - |  9332 | ` * void die(string $ms)` |
|        - |  9333 | ` * void die(int $status)` |
|        - |  9334 | ` *   Output a message and terminate program execution.` |
|        - |  9335 | ` * Parameter` |
|        - |  9336 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9337 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9338 | ` *  and not printed` |
|        - |  9339 | ` * Return` |
|        - |  9340 | ` *  NULL` |
|        - |  9341 | ` */` |
|      ! 0 |  9342 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9343 |  |
|      ! 0 |  9344 | `	if( nArg > 0 ){` |
|      ! 0 |  9345 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9346 | `			const char *zData;` |
|      ! 0 |  9347 | `			int iLen = 0;` |
|        - |  9348 | `			/* Print exit message */` |
|      ! 0 |  9349 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9350 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9351 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9352 | `			sxi32 iExitStatus;` |
|        - |  9353 | `			/* Record exit status code */` |
|      ! 0 |  9354 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9355 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9356 | `		}` |
|      ! 0 |  9357 | `	}` |
|        - |  9358 | `	/* Check if we are in an included file */` |
|      ! 0 |  9359 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9360 | `		/* Exit the entire process */` |
|      ! 0 |  9361 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9362 | `	}` |
|        - |  9363 | `	/* Abort processing immediately */` |
|      ! 0 |  9364 | `	return PH7_ABORT;` |
|      ! 0 |  9365 |  |
|        - |  9366 | `/*` |
|        - |  9367 | ` * bool isset($var,...)` |
|        - |  9368 | ` *  Finds out whether a variable is set.` |
|        - |  9369 | ` * Parameters` |
|        - |  9370 | ` *  One or more variable to check.` |
|        - |  9371 | ` * Return` |
|        - |  9372 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9373 | ` */` |
|    74990 |  9374 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9375 |  |
|        - |  9376 | `	ph7_value *pObj;` |
|    74992 |  9377 | `	int res = 0;` |
|        - |  9378 | `	int i;` |
|    74992 |  9379 | `	if( nArg < 1 ){` |
|        - |  9380 | `		/* Missing arguments,return false */` |
|      ! 0 |  9381 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9382 | `		return SXRET_OK;` |
|        - |  9383 | `	}` |
|        - |  9384 | `	/* Iterate over available arguments */` |
|    98838 |  9385 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    74992 |  9386 | `		pObj = apArg[i];` |
|    74992 |  9387 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    50628 |  9388 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9389 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9390 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9391 | `			}` |
|    25313 |  9392 | `		}` |
|    74992 |  9393 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    74992 |  9394 | `		if( !res ){` |
|        - |  9395 | `			/* Variable not set,return FALSE */` |
|    51146 |  9396 | `			ph7_result_bool(pCtx,0);` |
|    51146 |  9397 | `			return SXRET_OK;` |
|        - |  9398 | `		}` |
|    11925 |  9399 | `	}` |
|        - |  9400 | `	/* All given variable are set,return TRUE */` |
|    23848 |  9401 | `	ph7_result_bool(pCtx,1);` |
|    23848 |  9402 | `	return SXRET_OK;` |
|    37497 |  9403 |  |
|        - |  9404 | `/*` |
|        - |  9405 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9406 | ` * frame,the reference table and discard it's contents.` |
|        - |  9407 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9408 | ` */` |
|  3021314 |  9409 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9410 |  |
|        - |  9411 | `	ph7_value *pObj;` |
|        - |  9412 | `	VmRefObj *pRef;` |
|  3021316 |  9413 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3021316 |  9414 | `	if( pObj ){` |
|        - |  9415 | `		/* Release the object */` |
|  3021316 |  9416 | `		PH7_MemObjRelease(pObj);` |
|  1510657 |  9417 | `	}` |
|        - |  9418 | `	/* Remove old reference links */` |
|  3021316 |  9419 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3021316 |  9420 | `	if( pRef ){` |
|  3021310 |  9421 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9422 | `		/* Unlink from the reference table */` |
|  3021310 |  9423 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3021310 |  9424 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9425 | `			VmSlot sFree;` |
|        - |  9426 | `			/* Restore to the free list */` |
|  3021304 |  9427 | `			sFree.nIdx = nObjIdx;` |
|  3021304 |  9428 | `			sFree.pUserData = 0;` |
|  3021304 |  9429 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1510651 |  9430 | `		}` |
|  1510654 |  9431 | `	}` |
|  3021316 |  9432 | `	return SXRET_OK;` |
|        2 |  9433 |  |
|        - |  9434 | `/*` |
|        - |  9435 | ` * void unset($var,...)` |
|        - |  9436 | ` *   Unset one or more given variable.` |
|        - |  9437 | ` * Parameters` |
|        - |  9438 | ` *  One or more variable to unset.` |
|        - |  9439 | ` * Return` |
|        - |  9440 | ` *  Nothing.` |
|        - |  9441 | ` */` |
|     6764 |  9442 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9443 |  |
|        - |  9444 | `	ph7_value *pObj;` |
|        - |  9445 | `	ph7_vm *pVm;` |
|        - |  9446 | `	int i;` |
|        - |  9447 | `	/* Point to the target VM */` |
|     6766 |  9448 | `	pVm = pCtx->pVm;` |
|        - |  9449 | `	/* Iterate and unset */` |
|    13530 |  9450 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6766 |  9451 | `		pObj = apArg[i];` |
|     6766 |  9452 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9453 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9454 | `				/* Throw an error */` |
|      ! 0 |  9455 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9456 | `			}` |
|      ! 0 |  9457 | `		}else{` |
|     6766 |  9458 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9459 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6766 |  9460 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6760 |  9461 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3379 |  9462 | `			}` |
|        - |  9463 | `		}` |
|     3384 |  9464 | `	}` |
|     6766 |  9465 | `	return SXRET_OK;` |
|        2 |  9466 |  |
|        - |  9467 | `/*` |
|        - |  9468 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9469 | ` */` |
|      110 |  9470 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9471 |  |
|      111 |  9472 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9473 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9474 | `	ph7_value *pObj;` |
|        - |  9475 | `	sxu32 nIdx;` |
|        - |  9476 | `	/* Extract the memory object */` |
|      111 |  9477 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9478 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9479 | `	if( pObj ){` |
|      111 |  9480 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9481 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9482 | `				SyString sName;` |
|        - |  9483 | `				ph7_value sKey;` |
|        - |  9484 | `				/* Perform the insertion */` |
|      109 |  9485 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9486 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9487 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9488 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9489 | `			}` |
|       54 |  9490 | `		}` |
|       55 |  9491 | `	}` |
|      111 |  9492 | `	return SXRET_OK;` |
|        1 |  9493 |  |
|        - |  9494 | `/*` |
|        - |  9495 | ` * array get_defined_vars(void)` |
|        - |  9496 | ` *  Returns an array of all defined variables.` |
|        - |  9497 | ` * Parameter` |
|        - |  9498 | ` *  None` |
|        - |  9499 | ` * Return` |
|        - |  9500 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9501 | ` */` |
|        2 |  9502 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9503 |  |
|        3 |  9504 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9505 | `	ph7_value *pArray;` |
|        - |  9506 | `	/* Create a new array */` |
|        3 |  9507 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9508 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9509 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9510 | `		SXUNUSED(apArg);` |
|        - |  9511 | `		/* Return NULL */` |
|      ! 0 |  9512 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9513 | `		return SXRET_OK;` |
|        - |  9514 | `	}` |
|        - |  9515 | `	/* Superglobals first */` |
|        3 |  9516 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9517 | `	/* Then variable defined in the current frame */` |
|        3 |  9518 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9519 | `	/* Finally,return the created array */` |
|        3 |  9520 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9521 | `	return SXRET_OK;` |
|        2 |  9522 |  |
|        - |  9523 | `/*` |
|        - |  9524 | ` * bool gettype($var)` |
|        - |  9525 | ` *  Get the type of a variable` |
|        - |  9526 | ` * Parameters` |
|        - |  9527 | ` *   $var` |
|        - |  9528 | ` *    The variable being type checked.` |
|        - |  9529 | ` * Return` |
|        - |  9530 | ` *   String representation of the given variable type.` |
|        - |  9531 | ` */` |
|       32 |  9532 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9533 |  |
|       34 |  9534 | `	const char *zType = "Empty";` |
|       34 |  9535 | `	if( nArg > 0 ){` |
|       34 |  9536 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9537 | `	}` |
|        - |  9538 | `	/* Return the variable type */` |
|       34 |  9539 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9540 | `	return SXRET_OK;` |
|        2 |  9541 |  |
|        - |  9542 | `/*` |
|        - |  9543 | ` * string get_resource_type(resource $handle)` |
|        - |  9544 | ` *  This function gets the type of the given resource.` |
|        - |  9545 | ` * Parameters` |
|        - |  9546 | ` *  $handle` |
|        - |  9547 | ` *  The evaluated resource handle.` |
|        - |  9548 | ` * Return` |
|        - |  9549 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9550 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9551 | ` *  the return value will be the string Unknown.` |
|        - |  9552 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9553 | ` *  is not a resource.` |
|        - |  9554 | ` */` |
|        2 |  9555 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9556 |  |
|        3 |  9557 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9558 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9559 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9560 | `		return PH7_OK;` |
|        - |  9561 | `	}` |
|        3 |  9562 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9563 | `	return SXRET_OK;` |
|        2 |  9564 |  |
|        - |  9565 | `/*` |
|        - |  9566 | ` * void var_dump(expression,....)` |
|        - |  9567 | ` *   var_dump � Dumps information about a variable` |
|        - |  9568 | ` * Parameters` |
|        - |  9569 | ` *   One or more expression to dump.` |
|        - |  9570 | ` * Returns` |
|        - |  9571 | ` *  Nothing.` |
|        - |  9572 | ` */` |
|      218 |  9573 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9574 |  |
|        - |  9575 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9576 | `	int i;` |
|      220 |  9577 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9578 | `	/* Dump one or more expressions */` |
|      444 |  9579 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9580 | `		ph7_value *pObj = apArg[i];` |
|        - |  9581 | `		/* Reset the working buffer */` |
|      226 |  9582 | `		SyBlobReset(&sDump);` |
|        - |  9583 | `		/* Dump the given expression */` |
|      226 |  9584 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9585 | `		/* Output */` |
|      226 |  9586 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9587 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9588 | `		}` |
|      114 |  9589 | `	}` |
|        - |  9590 | `	/* Release the working buffer */` |
|      220 |  9591 | `	SyBlobRelease(&sDump);` |
|      220 |  9592 | `	return SXRET_OK;` |
|        2 |  9593 |  |
|        - |  9594 | `/*` |
|        - |  9595 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9596 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9597 | ` * Parameters` |
|        - |  9598 | ` *   expression: Expression to dump` |
|        - |  9599 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9600 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9601 | ` *            print_r() will return the information rather than print it.` |
|        - |  9602 | ` * Return` |
|        - |  9603 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9604 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9605 | ` */` |
|       16 |  9606 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9607 |  |
|       17 |  9608 | `	int ret_string = 0;` |
|        - |  9609 | `	SyBlob sDump;` |
|       17 |  9610 | `	if( nArg < 1 ){` |
|        - |  9611 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9612 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9613 | `		return SXRET_OK;` |
|        - |  9614 | `	}` |
|       17 |  9615 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9616 | `	if ( nArg > 1 ){` |
|        - |  9617 | `		/* Where to redirect output */` |
|       11 |  9618 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9619 | `	}` |
|        - |  9620 | `	/* Generate dump */` |
|       17 |  9621 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9622 | `	if( !ret_string ){` |
|        - |  9623 | `		/* Output dump */` |
|        7 |  9624 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9625 | `		/* Return true */` |
|        7 |  9626 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9627 | `	}else{` |
|        - |  9628 | `		/* Generated dump as return value */` |
|       11 |  9629 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9630 | `	}` |
|        - |  9631 | `	/* Release the working buffer */` |
|       17 |  9632 | `	SyBlobRelease(&sDump);` |
|       17 |  9633 | `	return SXRET_OK;` |
|        9 |  9634 |  |
|        - |  9635 | `/*` |
|        - |  9636 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9637 | ` * Same job as print_r. (see coment above)` |
|        - |  9638 | ` */` |
|        2 |  9639 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9640 |  |
|        3 |  9641 | `	int ret_string = 0;` |
|        - |  9642 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9643 | `	if( nArg < 1 ){` |
|        - |  9644 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9645 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9646 | `		return SXRET_OK;` |
|        - |  9647 | `	}` |
|        3 |  9648 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9649 | `	if ( nArg > 1 ){` |
|        - |  9650 | `		/* Where to redirect output */` |
|        3 |  9651 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9652 | `	}` |
|        - |  9653 | `	/* Generate dump */` |
|        3 |  9654 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9655 | `	if( !ret_string ){` |
|        - |  9656 | `		/* Output dump */` |
|      ! 0 |  9657 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9658 | `		/* Return NULL */` |
|      ! 0 |  9659 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9660 | `	}else{` |
|        - |  9661 | `		/* Generated dump as return value */` |
|        3 |  9662 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9663 | `	}` |
|        - |  9664 | `	/* Release the working buffer */` |
|        3 |  9665 | `	SyBlobRelease(&sDump);` |
|        3 |  9666 | `	return SXRET_OK;` |
|        2 |  9667 |  |
|        - |  9668 | `/*` |
|        - |  9669 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9670 | ` *  Set/get the various assert flags.` |
|        - |  9671 | ` * Parameter` |
|        - |  9672 | ` * $what` |
|        - |  9673 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9674 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9675 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9676 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9677 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9678 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9679 | ` * $value` |
|        - |  9680 | ` *   An optional new value for the option.` |
|        - |  9681 | ` * Return` |
|        - |  9682 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9683 | ` */` |
|       28 |  9684 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9685 |  |
|       30 |  9686 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9687 | `	int iOption;` |
|        - |  9688 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9689 | `	if( nArg < 1 ){` |
|        3 |  9690 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9691 | `			"ArgumentCountError",` |
|        - |  9692 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9693 | `			);` |
|        - |  9694 | `	}` |
|        - |  9695 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9696 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9697 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9698 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9699 | `			"TypeError",` |
|        - |  9700 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9701 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9702 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9703 | `			);` |
|        - |  9704 | `	}` |
|       28 |  9705 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9706 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9707 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9708 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9709 | `	switch( iOption ){` |
|        5 |  9710 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9711 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9712 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9713 | `		if( nArg > 1 ){` |
|        5 |  9714 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9715 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9716 | `			}else{` |
|        3 |  9717 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9718 | `			}` |
|        2 |  9719 | `		}` |
|       12 |  9720 | `		break;` |
|        1 |  9721 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9722 | `		/* Return old callback or null */` |
|        3 |  9723 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9724 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9725 | `		}else{` |
|        3 |  9726 | `			ph7_result_null(pCtx);` |
|        - |  9727 | `		}` |
|        3 |  9728 | `		if( nArg > 1 ){` |
|      ! 0 |  9729 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9730 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9731 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9732 | `			}else{` |
|      ! 0 |  9733 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9734 | `			}` |
|      ! 0 |  9735 | `		}` |
|        3 |  9736 | `		break;` |
|        5 |  9737 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9738 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9739 | `		if( nArg > 1 ){` |
|        5 |  9740 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9741 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9742 | `			}else{` |
|        3 |  9743 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9744 | `			}` |
|        2 |  9745 | `		}` |
|       11 |  9746 | `		break;` |
|      ! 0 |  9747 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9748 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9749 | `		break;` |
|        1 |  9750 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9751 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9752 | `		break;` |
|      ! 0 |  9753 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9754 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9755 | `		break;` |
|        1 |  9756 | `	default:` |
|        - |  9757 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9758 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9759 | `			"ValueError",` |
|        - |  9760 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9761 | `			);` |
|        - |  9762 | `	}` |
|       26 |  9763 | `	return PH7_OK;` |
|       16 |  9764 |  |
|        - |  9765 | `/*` |
|        - |  9766 | ` * bool assert(mixed $assertion)` |
|        - |  9767 | ` *  Checks if assertion is FALSE.` |
|        - |  9768 | ` * Parameter` |
|        - |  9769 | ` *  $assertion` |
|        - |  9770 | ` *    The assertion to test.` |
|        - |  9771 | ` * Return` |
|        - |  9772 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9773 | ` */` |
|       24 |  9774 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9775 |  |
|       26 |  9776 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9777 | `	int iFlags,iResult;` |
|        - |  9778 | `	const char *zDesc;` |
|        - |  9779 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9780 | `	if( nArg < 1 ){` |
|        3 |  9781 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9782 | `			"ArgumentCountError",` |
|        - |  9783 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9784 | `			);` |
|        - |  9785 | `	}` |
|       24 |  9786 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9787 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9788 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9789 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9790 | `		return PH7_OK;` |
|        - |  9791 | `	}` |
|        - |  9792 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9793 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9794 | `	if( !iResult ){` |
|        - |  9795 | `		/* Assertion failed */` |
|        - |  9796 | `		/* Extract optional description */` |
|       13 |  9797 | `		zDesc = 0;` |
|       13 |  9798 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9799 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9800 | `		}` |
|       13 |  9801 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9802 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9803 | `			ph7_value sFile,sLine;` |
|        - |  9804 | `			ph7_value *apCbArg[3];` |
|        - |  9805 | `			SyString *pFile;` |
|        - |  9806 | `			/* Extract the processed script */` |
|      ! 0 |  9807 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9808 | `			if( pFile == 0 ){` |
|      ! 0 |  9809 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9810 | `			}` |
|        - |  9811 | `			/* Invoke the callback */` |
|      ! 0 |  9812 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9813 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9814 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9815 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9816 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9817 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9818 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9819 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9820 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9821 | `		}` |
|       13 |  9822 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9823 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9824 | `			return PH7_ABORT;` |
|        - |  9825 | `		}` |
|        - |  9826 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9827 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9828 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9829 | `				"AssertionError",` |
|        - |  9830 | `				"%s",` |
|        1 |  9831 | `				zDesc` |
|        - |  9832 | `				);` |
|      ! 0 |  9833 | `		}else{` |
|       11 |  9834 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9835 | `				"AssertionError",` |
|        - |  9836 | `				"assert(false)"` |
|        - |  9837 | `				);` |
|        - |  9838 | `		}` |
|        - |  9839 | `	}` |
|        - |  9840 | `	/* Assertion passed */` |
|       11 |  9841 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9842 | `	return PH7_OK;` |
|       14 |  9843 |  |
|        - |  9844 | `/*` |
|        - |  9845 | ` * Section:` |
|        - |  9846 | ` *  Error reporting functions.` |
|        - |  9847 | ` * Status:` |
|        - |  9848 | ` *    Stable.` |
|        - |  9849 | ` */` |
|        - |  9850 | `/*` |
|        - |  9851 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9852 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9853 | ` * Parameters` |
|        - |  9854 | ` *  $error_msg` |
|        - |  9855 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9856 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9857 | ` * $error_type` |
|        - |  9858 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9859 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9860 | ` * Return` |
|        - |  9861 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9862 | ` */` |
|       12 |  9863 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9864 |  |
|       14 |  9865 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9866 | `	int rc = PH7_OK;` |
|       14 |  9867 | `	if( nArg > 0 ){` |
|        - |  9868 | `		const char *zErr;` |
|        - |  9869 | `		int nLen;` |
|        - |  9870 | `		/* Extract the error message */` |
|       12 |  9871 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9872 | `		if( nArg > 1 ){` |
|        - |  9873 | `			/* Extract the error type */` |
|       12 |  9874 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9875 | `			switch( nErr ){` |
|        1 |  9876 | `			case 1:   /* E_ERROR */` |
|        - |  9877 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9878 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9879 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9880 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9881 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9882 | `				break;` |
|        1 |  9883 | `			case 2:   /* E_WARNING */` |
|        - |  9884 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9885 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9886 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9887 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9888 | `				break;` |
|        3 |  9889 | `			default:` |
|        8 |  9890 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9891 | `				break;` |
|        - |  9892 | `			}` |
|        5 |  9893 | `		}` |
|        - |  9894 | `		/* Report error */` |
|       12 |  9895 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9896 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9897 | `			return rc;` |
|        - |  9898 | `		}` |
|        - |  9899 | `		/* Return true */` |
|       12 |  9900 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9901 | `	}else{` |
|        - |  9902 | `		/* Missing arguments,return FALSE */` |
|        3 |  9903 | `		ph7_result_bool(pCtx,0);` |
|        - |  9904 | `	}` |
|       14 |  9905 | `	return rc;` |
|        8 |  9906 |  |
|        - |  9907 | `/*` |
|        - |  9908 | ` * int error_reporting([int $level])` |
|        - |  9909 | ` *  Sets which PHP errors are reported.` |
|        - |  9910 | ` * Parameters` |
|        - |  9911 | ` *  $level` |
|        - |  9912 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9913 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9914 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9915 | ` *   levels will not always behave as expected.` |
|        - |  9916 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9917 | ` *   in the predefined constants.` |
|        - |  9918 | ` * Return` |
|        - |  9919 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9920 | ` *   parameter is given.` |
|        - |  9921 | ` */` |
|       38 |  9922 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9923 |  |
|       40 |  9924 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9925 | `	int nOld;` |
|        - |  9926 | `	/* Extract the old reporting level */` |
|       40 |  9927 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 |  9928 | `	if( nArg > 0 ){` |
|        - |  9929 | `		int nNew;` |
|        - |  9930 | `		/* Extract the desired error reporting level */` |
|       32 |  9931 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 |  9932 | `		if( !nNew ){` |
|        - |  9933 | `			/* Do not report errors at all */` |
|        5 |  9934 | `			pVm->bErrReport = 0;` |
|        3 |  9935 | `		}else{` |
|        - |  9936 | `			/* Report all errors */` |
|       28 |  9937 | `			pVm->bErrReport = 1;` |
|        - |  9938 | `		}` |
|       15 |  9939 | `	}` |
|        - |  9940 | `	/* Return the old level */` |
|       40 |  9941 | `	ph7_result_int(pCtx,nOld);` |
|       40 |  9942 | `	return PH7_OK;` |
|        2 |  9943 |  |
|        - |  9944 | `/*` |
|        - |  9945 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9946 | ` *  Send an error message somewhere.` |
|        - |  9947 | ` * Parameter` |
|        - |  9948 | ` *  $message` |
|        - |  9949 | ` *   The error message that should be logged.` |
|        - |  9950 | ` *  $message_type` |
|        - |  9951 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9952 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9953 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9954 | ` *       This is the default option.` |
|        - |  9955 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9956 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9957 | ` *    2  No longer an option.` |
|        - |  9958 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9959 | ` *       to the end of the message string.` |
|        - |  9960 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9961 | ` *  $destination` |
|        - |  9962 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9963 | ` *  $extra_headers` |
|        - |  9964 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9965 | ` * Return` |
|        - |  9966 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9967 | ` * NOTE:` |
|        - |  9968 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9969 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9970 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9971 | ` *  Otherwise this function is no-op.` |
|        - |  9972 | ` */` |
|        4 |  9973 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9974 |  |
|        - |  9975 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9976 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9977 | `	int iType = 0;` |
|        5 |  9978 | `	if( nArg < 1 ){` |
|        - |  9979 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9980 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9981 | `		return PH7_OK;` |
|        - |  9982 | `	}` |
|        5 |  9983 | `	if( pVm->xErrLog  ){` |
|        - |  9984 | `		/* Invoke the user callback */` |
|      ! 0 |  9985 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9986 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9987 | `		if( nArg > 1 ){` |
|      ! 0 |  9988 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9989 | `			if( nArg > 2 ){` |
|      ! 0 |  9990 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9991 | `				if( nArg > 3 ){` |
|      ! 0 |  9992 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9993 | `				}` |
|      ! 0 |  9994 | `			}` |
|      ! 0 |  9995 | `		}` |
|      ! 0 |  9996 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9997 | `	}` |
|        - |  9998 | `	/* Retun TRUE */` |
|        5 |  9999 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10000 | `	return PH7_OK;` |
|        3 | 10001 |  |
|        - | 10002 | `/*` |
|        - | 10003 | ` * bool restore_exception_handler(void)` |
|        - | 10004 | ` *  Restores the previously defined exception handler function.` |
|        - | 10005 | ` * Parameter` |
|        - | 10006 | ` *  None` |
|        - | 10007 | ` * Return` |
|        - | 10008 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10009 | ` */` |
|        4 | 10010 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10011 |  |
|        5 | 10012 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10013 | `	ph7_value *pOld,*pNew;` |
|        - | 10014 | `	/* Point to the old and the new handler */` |
|        5 | 10015 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10016 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10017 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10018 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10019 | `		SXUNUSED(apArg);` |
|        - | 10020 | `		/* No installed handler,return FALSE */` |
|        5 | 10021 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10022 | `		return PH7_OK;` |
|        - | 10023 | `	}` |
|        - | 10024 | `	/* Copy the old handler */` |
|      ! 0 | 10025 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10026 | `	PH7_MemObjRelease(pOld);` |
|        - | 10027 | `	/* Return TRUE */` |
|      ! 0 | 10028 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10029 | `	return PH7_OK;` |
|        3 | 10030 |  |
|        - | 10031 | `/*` |
|        - | 10032 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10033 | ` *  Sets a user-defined exception handler function.` |
|        - | 10034 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10035 | ` * NOTE` |
|        - | 10036 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10037 | ` *  the satndard PHP engine.` |
|        - | 10038 | ` * Parameters` |
|        - | 10039 | ` *  $exception_handler` |
|        - | 10040 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10041 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10042 | ` *   that was thrown.` |
|        - | 10043 | ` *  Note:` |
|        - | 10044 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10045 | ` * Return` |
|        - | 10046 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10047 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10048 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10049 | ` */` |
|        4 | 10050 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10051 |  |
|        6 | 10052 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10053 | `	ph7_value *pOld,*pNew;` |
|        - | 10054 | `	/* Point to the old and the new handler */` |
|        6 | 10055 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10056 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10057 | `	/* Return the old handler */` |
|        6 | 10058 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10059 | `	if( nArg > 0 ){` |
|        6 | 10060 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10061 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10062 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10063 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10064 | `		}else{` |
|        6 | 10065 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10066 | `			/* Install the new handler */` |
|        6 | 10067 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10068 | `		}` |
|        2 | 10069 | `	}` |
|        6 | 10070 | `	return PH7_OK;` |
|        2 | 10071 |  |
|        - | 10072 | `/*` |
|        - | 10073 | ` * bool restore_error_handler(void)` |
|        - | 10074 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10075 | ` * Parameters:` |
|        - | 10076 | ` *  None.` |
|        - | 10077 | ` * Return` |
|        - | 10078 | ` *  Always TRUE.` |
|        - | 10079 | ` */` |
|        4 | 10080 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10081 |  |
|        5 | 10082 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10083 | `	ph7_value *pOld,*pNew;` |
|        - | 10084 | `	/* Point to the old and the new handler */` |
|        5 | 10085 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10086 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10087 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10088 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10089 | `		SXUNUSED(apArg);` |
|        - | 10090 | `		/* No installed callback,return FALSE */` |
|        5 | 10091 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10092 | `		return PH7_OK;` |
|        - | 10093 | `	}` |
|        - | 10094 | `	/* Copy the old callback */` |
|      ! 0 | 10095 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10096 | `	PH7_MemObjRelease(pOld);` |
|        - | 10097 | `	/* Return TRUE */` |
|      ! 0 | 10098 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10099 | `	return PH7_OK;` |
|        3 | 10100 |  |
|        - | 10101 | `/*` |
|        - | 10102 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10103 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10104 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10105 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10106 | ` *  Sets a user-defined error handler function.` |
|        - | 10107 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10108 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10109 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10110 | ` *  conditions (using trigger_error()).` |
|        - | 10111 | ` * Parameters` |
|        - | 10112 | ` *  $error_handler` |
|        - | 10113 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10114 | ` *   describing the error.` |
|        - | 10115 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10116 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10117 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10118 | ` *   The function can be shown as:` |
|        - | 10119 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10120 | ` *     errno` |
|        - | 10121 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10122 | ` *   errstr` |
|        - | 10123 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10124 | ` *   errfile` |
|        - | 10125 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10126 | ` *     was raised in, as a string.` |
|        - | 10127 | ` *  Note:` |
|        - | 10128 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10129 | ` * Return` |
|        - | 10130 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10131 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10132 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10133 | ` */` |
|     9210 | 10134 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10135 |  |
|     9212 | 10136 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10137 | `	ph7_value *pOld,*pNew;` |
|        - | 10138 | `	/* Point to the old and the new handler */` |
|     9212 | 10139 | `	pOld = &pVm->aErrCB[0];` |
|     9212 | 10140 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10141 | `	/* Return the old handler */` |
|     9212 | 10142 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9212 | 10143 | `	if( nArg > 0 ){` |
|     9212 | 10144 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10145 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4605 | 10146 | `			PH7_MemObjRelease(pNew);` |
|     4605 | 10147 | `			ph7_result_bool(pCtx,1);` |
|     2303 | 10148 | `		}else{` |
|     4608 | 10149 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10150 | `			/* Install the new handler */` |
|     4608 | 10151 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10152 | `		}` |
|     4605 | 10153 | `	}` |
|     9212 | 10154 | `	return PH7_OK;` |
|        2 | 10155 |  |
|        - | 10156 | `/*` |
|        - | 10157 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10158 | ` *  Generates a backtrace.` |
|        - | 10159 | ` * Paramaeter` |
|        - | 10160 | ` *  $options` |
|        - | 10161 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10162 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10163 | ` *   all the function/method arguments, to save memory.` |
|        - | 10164 | ` * $limit` |
|        - | 10165 | ` *   (Not Used)` |
|        - | 10166 | ` * Return` |
|        - | 10167 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10168 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10169 | ` *          Name        Type      Description` |
|        - | 10170 | ` *          ------      ------     -----------` |
|        - | 10171 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10172 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10173 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10174 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10175 | ` *          object      object    The current object.` |
|        - | 10176 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10177 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10178 | ` */` |
|      514 | 10179 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10180 |  |
|      516 | 10181 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10182 | `	ph7_value *pArray;` |
|        - | 10183 | `	ph7_class *pClass;` |
|        - | 10184 | `	ph7_value *pValue;` |
|        - | 10185 | `	SyString *pFile;` |
|        - | 10186 | `	/* Create a new array */` |
|      516 | 10187 | `	pArray = ph7_context_new_array(pCtx);` |
|      516 | 10188 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      516 | 10189 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10190 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10191 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10192 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10193 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10194 | `		SXUNUSED(apArg);` |
|      ! 0 | 10195 | `		return PH7_OK;` |
|        - | 10196 | `	}` |
|        - | 10197 | `	/* Dump running function name and it's arguments  */` |
|      516 | 10198 | `	if( pVm->pFrame->pParent ){` |
|      516 | 10199 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10200 | `		ph7_vm_func *pFunc;` |
|        - | 10201 | `		ph7_value *pArg;` |
|      516 | 10202 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      516 | 10203 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      516 | 10204 | `		if( pFrame->pParent && pFunc ){` |
|      516 | 10205 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      516 | 10206 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      516 | 10207 | `			ph7_value_reset_string_cursor(pValue);` |
|      257 | 10208 | `		}` |
|        - | 10209 | `		/* Function arguments */` |
|      516 | 10210 | `		pArg = ph7_context_new_array(pCtx);` |
|      516 | 10211 | `		if( pArg  ){` |
|        - | 10212 | `			ph7_value *pObj;` |
|        - | 10213 | `			VmSlot *aSlot;` |
|        - | 10214 | `			sxu32 n;` |
|        - | 10215 | `			/* Start filling the array with the given arguments */` |
|      516 | 10216 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2050 | 10217 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1536 | 10218 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1536 | 10219 | `				if( pObj ){` |
|     1536 | 10220 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      767 | 10221 | `				}` |
|      769 | 10222 | `			}` |
|        - | 10223 | `			/* Save the array */` |
|      516 | 10224 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      257 | 10225 | `		}` |
|      257 | 10226 | `	}` |
|      516 | 10227 | `	ph7_value_int(pValue,1);` |
|        - | 10228 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10229 | `	 * line numbers at run-time. )` |
|        - | 10230 | `	 */` |
|      516 | 10231 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10232 | `	/* Current processed script */` |
|      516 | 10233 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      516 | 10234 | `	if( pFile ){` |
|      516 | 10235 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      516 | 10236 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      516 | 10237 | `		ph7_value_reset_string_cursor(pValue);` |
|      257 | 10238 | `	}` |
|        - | 10239 | `	/* Top class */` |
|      516 | 10240 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      516 | 10241 | `	if( pClass ){` |
|      512 | 10242 | `		ph7_value_reset_string_cursor(pValue);` |
|      512 | 10243 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      512 | 10244 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      255 | 10245 | `	}` |
|        - | 10246 | `	/* Return the freshly created array */` |
|      516 | 10247 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10248 | `	/*` |
|        - | 10249 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10250 | `	 * as soon we return from this function.` |
|        - | 10251 | `	 */` |
|      516 | 10252 | `	return PH7_OK;` |
|      259 | 10253 |  |
|        - | 10254 | `/*` |
|        - | 10255 | ` * Generate a small backtrace.` |
|        - | 10256 | ` * Store the generated dump in the given BLOB` |
|        - | 10257 | ` */` |
|        4 | 10258 | `static int VmMiniBacktrace(` |
|        - | 10259 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10260 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10261 | `	)` |
|        1 | 10262 |  |
|        5 | 10263 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10264 | `	ph7_vm_func *pFunc;` |
|        - | 10265 | `	ph7_class *pClass;` |
|        - | 10266 | `	SyString *pFile;` |
|        - | 10267 | `	/* Called function */` |
|        5 | 10268 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10269 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10270 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10271 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10272 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10273 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10274 | `	}else{` |
|      ! 0 | 10275 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10276 | `	}` |
|        5 | 10277 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10278 | `	/* Current processed script */` |
|        5 | 10279 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10280 | `	if( pFile ){` |
|        5 | 10281 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10282 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10283 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10284 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10285 | `	}` |
|        - | 10286 | `	/* Top class */` |
|        5 | 10287 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10288 | `	if( pClass ){` |
|      ! 0 | 10289 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10290 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10291 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10292 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10293 | `	}` |
|        5 | 10294 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10295 | `	/* All done */` |
|        5 | 10296 | `	return SXRET_OK;` |
|        1 | 10297 |  |
|        - | 10298 | `/*` |
|        - | 10299 | ` * void debug_print_backtrace()` |
|        - | 10300 | ` *  Prints a backtrace` |
|        - | 10301 | ` * Parameters` |
|        - | 10302 | ` * None` |
|        - | 10303 | ` * Return` |
|        - | 10304 | ` * NULL` |
|        - | 10305 | ` */` |
|        2 | 10306 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10307 |  |
|        3 | 10308 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10309 | `	SyBlob sDump;` |
|        3 | 10310 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10311 | `	/* Generate the backtrace */` |
|        3 | 10312 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10313 | `	/* Output backtrace */` |
|        3 | 10314 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10315 | `	/* All done,cleanup */` |
|        3 | 10316 | `	SyBlobRelease(&sDump);` |
|        1 | 10317 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10318 | `	SXUNUSED(apArg);` |
|        3 | 10319 | `	return PH7_OK;` |
|        1 | 10320 |  |
|        - | 10321 | `/*` |
|        - | 10322 | ` * string debug_string_backtrace()` |
|        - | 10323 | ` *  Generate a backtrace` |
|        - | 10324 | ` * Parameters` |
|        - | 10325 | ` * None` |
|        - | 10326 | ` * Return` |
|        - | 10327 | ` *  A mini backtrace().` |
|        - | 10328 | ` * Note that this is a symisc extension.` |
|        - | 10329 | ` */` |
|        2 | 10330 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10331 |  |
|        3 | 10332 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10333 | `	SyBlob sDump;` |
|        3 | 10334 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10335 | `	/* Generate the backtrace */` |
|        3 | 10336 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10337 | `	/* Return the backtrace */` |
|        3 | 10338 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10339 | `	/* All done,cleanup */` |
|        3 | 10340 | `	SyBlobRelease(&sDump);` |
|        1 | 10341 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10342 | `	SXUNUSED(apArg);` |
|        3 | 10343 | `	return PH7_OK;` |
|        1 | 10344 |  |
|        - | 10345 | `/*` |
|        - | 10346 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10347 | ` * exception is triggered.` |
|        - | 10348 | ` */` |
|      472 | 10349 | `static sxi32 VmUncaughtException(` |
|        - | 10350 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10351 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10352 | `	)` |
|        1 | 10353 |  |
|        - | 10354 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10355 | `	int nArg = 1;` |
|        - | 10356 | `	sxi32 rc;` |
|      473 | 10357 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10358 | `		/* Nesting limit reached */` |
|      ! 0 | 10359 | `		return SXRET_OK;` |
|        - | 10360 | `	}` |
|        - | 10361 | `	/* Call any exception handler if available */` |
|      473 | 10362 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10363 | `	if( pThis ){` |
|        - | 10364 | `		/* Load the exception instance */` |
|      473 | 10365 | `		sArg.x.pOther = pThis;` |
|      473 | 10366 | `		pThis->iRef++;` |
|      473 | 10367 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10368 | `	}else{` |
|      ! 0 | 10369 | `		nArg = 0;` |
|        - | 10370 | `	}` |
|      473 | 10371 | `	apArg[0] = &sArg;` |
|        - | 10372 | `	/* Call the exception handler if available */` |
|      473 | 10373 | `	pVm->nExceptDepth++;` |
|      473 | 10374 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10375 | `	pVm->nExceptDepth--;` |
|      473 | 10376 | `	if( rc != SXRET_OK ){` |
|        - | 10377 | `		SyBlob sMsgBuf;` |
|      471 | 10378 | `		const char *zClass = "Exception";` |
|      471 | 10379 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10380 | `		const char *zMsg;` |
|        - | 10381 | `		sxu32 nMsg;` |
|        - | 10382 | `		const char *zFuncName;` |
|        - | 10383 | `		int nFuncLen;` |
|      471 | 10384 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10385 | `		if( pThis ){` |
|        - | 10386 | `			ph7_class_method *pGetMessage;` |
|        - | 10387 | `			ph7_value sMsg;` |
|        - | 10388 | `			const char *zTmp;` |
|        - | 10389 | `			int nTmp;` |
|      471 | 10390 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10391 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10392 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10393 | `			if( pGetMessage ){` |
|      471 | 10394 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10395 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10396 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10397 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10398 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10399 | `					}` |
|      235 | 10400 | `				}` |
|      471 | 10401 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10402 | `			}` |
|      235 | 10403 | `		}` |
|      471 | 10404 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10405 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10406 | `		}` |
|      471 | 10407 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10408 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10409 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10410 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10411 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10412 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10413 | `		rc = SXERR_ABORT;` |
|      235 | 10414 | `	}` |
|      473 | 10415 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10416 | `	return rc;` |
|      237 | 10417 |  |
|        - | 10418 | `/*` |
|        - | 10419 | ` * Throw a user exception.` |
|        - | 10420 | ` *` |
|        - | 10421 | ` * Exception dispatch follows this sequence:` |
|        - | 10422 | ` *` |
|        - | 10423 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10424 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10425 | ` *` |
|        - | 10426 | ` * 2. If NO catch matches:` |
|        - | 10427 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10428 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10429 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10430 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10431 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10432 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10433 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10434 | ` *` |
|        - | 10435 | ` * 3. If a catch DOES match:` |
|        - | 10436 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10437 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10438 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10439 | ` *       finally block.` |
|        - | 10440 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10441 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10442 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10443 | ` *       in pPendingException (step 2c).` |
|        - | 10444 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10445 | ` *    d. Run finally (if present).` |
|        - | 10446 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10447 | ` *       that handlers are restored and finally has run.` |
|        - | 10448 | ` */` |
|      514 | 10449 | `static sxi32 VmThrowException(` |
|        - | 10450 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10451 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10452 | `	)` |
|        2 | 10453 |  |
|        - | 10454 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10455 | `	ph7_exception **apException;` |
|        - | 10456 | `	ph7_exception *pException;` |
|        - | 10457 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10458 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10459 | `	pException = 0;` |
|      516 | 10460 | `	pCatch = 0;` |
|      516 | 10461 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10462 | `		ph7_exception_block *aCatch;` |
|        - | 10463 | `		ph7_class *pClass;` |
|        - | 10464 | `		sxu32 j;` |
|        - | 10465 | `		/* Locate the appropriate block to execute */` |
|       40 | 10466 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10467 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10468 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10469 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10470 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10471 | `			/* Extract the target class */` |
|       38 | 10472 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10473 | `			if( pClass == 0 ){` |
|        - | 10474 | `				/* No such class */` |
|      ! 0 | 10475 | `				continue;` |
|        - | 10476 | `			}` |
|       38 | 10477 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10478 | `				/* Catch block found,break immeditaley */` |
|       38 | 10479 | `				pCatch = &aCatch[j];` |
|       38 | 10480 | `				break;` |
|        - | 10481 | `			}` |
|      ! 0 | 10482 | `		}` |
|       19 | 10483 | `	}` |
|        - | 10484 | `	/* Execute the cached block if available */` |
|      516 | 10485 | `	if( pCatch == 0 ){` |
|        - | 10486 | `		sxi32 rc;` |
|        - | 10487 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10488 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10489 | `			pException->iFinallyDone = 1;` |
|        3 | 10490 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10491 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10492 | `				return SXERR_ABORT;` |
|        - | 10493 | `			}` |
|        1 | 10494 | `		}` |
|        - | 10495 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10496 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10497 | `			/* Re-throw to the outer handler */` |
|        3 | 10498 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10499 | `		}` |
|        - | 10500 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10501 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10502 | `		 * exception instead of reporting it uncaught.` |
|        - | 10503 | `		 */` |
|      478 | 10504 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10505 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10506 | `			 * by looking for a catch frame on the stack.` |
|        - | 10507 | `			 */` |
|      478 | 10508 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10509 | `			int inCatch = 0;` |
|      956 | 10510 | `			while( pF ){` |
|      484 | 10511 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        5 | 10512 | `					inCatch = 1;` |
|        5 | 10513 | `					break;` |
|        - | 10514 | `				}` |
|      479 | 10515 | `				pF = pF->pParent;` |
|        1 | 10516 | `			}` |
|      478 | 10517 | `			if( inCatch ){` |
|        - | 10518 | `				/* Defer — will be re-thrown after finally runs */` |
|        5 | 10519 | `				pThis->iRef++;` |
|        5 | 10520 | `				pVm->pPendingException = pThis;` |
|        5 | 10521 | `				return SXRET_OK;` |
|        - | 10522 | `			}` |
|      236 | 10523 | `		}` |
|        - | 10524 | `		/* Truly uncaught */` |
|      473 | 10525 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10526 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10527 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10528 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10529 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10530 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10531 | `			}` |
|      ! 0 | 10532 | `		}` |
|      473 | 10533 | `		return rc;` |
|      ! 0 | 10534 | `	}else{` |
|       38 | 10535 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10536 | `		ph7_exception **apSaved = 0;` |
|        - | 10537 | `		sxu32 nSavedCount;` |
|        - | 10538 | `		sxi32 rc;` |
|       38 | 10539 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10540 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10541 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10542 | `		}` |
|        - | 10543 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10544 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10545 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10546 | `		 */` |
|       38 | 10547 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10548 | `		if( nSavedCount > 0 ){` |
|       10 | 10549 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10550 | `				nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10551 | `			if( apSaved ){` |
|       10 | 10552 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10553 | `					nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10554 | `				SySetReset(&pVm->aException);` |
|        3 | 10555 | `			}` |
|        3 | 10556 | `		}` |
|        - | 10557 | `		/* Create a private frame first */` |
|       38 | 10558 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10559 | `		if( rc == SXRET_OK ){` |
|       38 | 10560 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10561 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10562 | `			if( pObj ){` |
|       38 | 10563 | `				pThis->iRef++;` |
|       38 | 10564 | `				pObj->x.pOther = pThis;` |
|       38 | 10565 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10566 | `			}` |
|        - | 10567 | `			/* Execute the catch block */` |
|       38 | 10568 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10569 | `			/* Leave the frame */` |
|       38 | 10570 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10571 | `		}` |
|        - | 10572 | `		/* Restore the outer exception handlers */` |
|       38 | 10573 | `		if( apSaved ){` |
|        - | 10574 | `			sxu32 k;` |
|        - | 10575 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10576 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10577 | `			 * Restore the original outer entries.` |
|        - | 10578 | `			 */` |
|        7 | 10579 | `			SySetReset(&pVm->aException);` |
|       13 | 10580 | `			for(k = 0; k < nSavedCount; k++){` |
|        7 | 10581 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        4 | 10582 | `			}` |
|        7 | 10583 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10584 | `		}` |
|        - | 10585 | `		/* Execute the finally block after catch */` |
|       38 | 10586 | `		if( pException->iHasFinally ){` |
|       12 | 10587 | `			pException->iFinallyDone = 1;` |
|        - | 10588 | `			{` |
|       12 | 10589 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       12 | 10590 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10591 | `					return SXERR_ABORT;` |
|        - | 10592 | `				}` |
|        - | 10593 | `			}` |
|        5 | 10594 | `		}` |
|       38 | 10595 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10596 | `			return SXERR_ABORT;` |
|        - | 10597 | `		}` |
|        - | 10598 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10599 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10600 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10601 | `		 */` |
|       38 | 10602 | `		if( pVm->pPendingException ){` |
|        5 | 10603 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        5 | 10604 | `			pVm->pPendingException = 0;` |
|        5 | 10605 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10606 | `		}` |
|        - | 10607 | `	}` |
|        - | 10608 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10609 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10610 | `	 */` |
|       34 | 10611 | `	return SXRET_OK;` |
|      259 | 10612 |  |
|        - | 10613 | `/*` |
|        - | 10614 | ` * Section:` |
|        - | 10615 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10616 | ` * Status:` |
|        - | 10617 | ` *    Stable.` |
|        - | 10618 | ` */` |
|        - | 10619 | `/*` |
|        - | 10620 | ` * string ph7version(void)` |
|        - | 10621 | ` *  Returns the running version of the PH7 version.` |
|        - | 10622 | ` * Parameters` |
|        - | 10623 | ` *  None` |
|        - | 10624 | ` * Return` |
|        - | 10625 | ` * Current PH7 version.` |
|        - | 10626 | ` */` |
|        2 | 10627 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10628 |  |
|        1 | 10629 | `	SXUNUSED(nArg);` |
|        1 | 10630 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10631 | `	/* Current engine version */` |
|        3 | 10632 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10633 | `	return PH7_OK;` |
|        1 | 10634 |  |
|        - | 10635 | `/*` |
|        - | 10636 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10637 | ` */` |
|        - | 10638 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10639 | ` "<html><head>"\` |
|        - | 10640 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10641 | ` "<style type=\"text/css\">"\` |
|        - | 10642 | ` "div {"\` |
|        - | 10643 | `     "border: 1px solid #cccccc;"\` |
|        - | 10644 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10645 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10646 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10647 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10648 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10649 | `     "-o-border-radius: 10px;"\` |
|        - | 10650 | `     "border-radius: 10px;"\` |
|        - | 10651 | `     "padding-left: 2em;"\` |
|        - | 10652 | `     "background-color: white;"\` |
|        - | 10653 | `     "margin-left: auto;"\` |
|        - | 10654 | `     "font-family: verdana;"\` |
|        - | 10655 | `     "padding-right: 2em;"\` |
|        - | 10656 | `     "margin-right: auto;"\` |
|        - | 10657 | `     "}"\` |
|        - | 10658 | `     "body {"\` |
|        - | 10659 | `     "padding: 0.2em;"\` |
|        - | 10660 | `     "font-style: normal;"\` |
|        - | 10661 | `     "font-size: medium;"\` |
|        - | 10662 | `     "background-color: #f2f2f2;"\` |
|        - | 10663 | `     "}"\` |
|        - | 10664 | `     "hr {"\` |
|        - | 10665 | `     "border-style: solid none none;"\` |
|        - | 10666 | `     "border-width: 1px medium medium;"\` |
|        - | 10667 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10668 | `     "height: 1px;"\` |
|        - | 10669 | `     "}"\` |
|        - | 10670 | `     "a {"\` |
|        - | 10671 | `     "color: #3366cc;"\` |
|        - | 10672 | `     "text-decoration: none;"\` |
|        - | 10673 | `     "}"\` |
|        - | 10674 | `     "a:hover {"\` |
|        - | 10675 | `     "color: #999999;"\` |
|        - | 10676 | `     "}"\` |
|        - | 10677 | `     "a:active {"\` |
|        - | 10678 | `     "color: #663399;"\` |
|        - | 10679 | `     "}"\` |
|        - | 10680 | `     "h1 {"\` |
|        - | 10681 | `     "margin: 0;"\` |
|        - | 10682 | `     "padding: 0;"\` |
|        - | 10683 | `     "font-family: Verdana;"\` |
|        - | 10684 | `     "font-weight: bold;"\` |
|        - | 10685 | `     "font-style: normal;"\` |
|        - | 10686 | `     "font-size: medium;"\` |
|        - | 10687 | `     "text-transform: capitalize;"\` |
|        - | 10688 | `     "color: #0a328c;"\` |
|        - | 10689 | `     "}"\` |
|        - | 10690 | `     "p {"\` |
|        - | 10691 | `     "margin: 0 auto;"\` |
|        - | 10692 | `     "font-size: medium;"\` |
|        - | 10693 | `     "font-style: normal;"\` |
|        - | 10694 | `     "font-family: verdana;"\` |
|        - | 10695 | `     "}"\` |
|        - | 10696 | `"</style></head><body>"\` |
|        - | 10697 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10698 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10699 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10700 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10701 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10702 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10703 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10704 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10705 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10706 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10707 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10708 |  |
|        - | 10709 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10710 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10711 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10712 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10713 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10714 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10715 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10716 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10717 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10718 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10719 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10720 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10721 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10722 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10723 |  |
|        - | 10724 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10725 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10726 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10727 | `"&nbsp;*<br>"\` |
|        - | 10728 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10729 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10730 | `"&nbsp;* are met:<br>"\` |
|        - | 10731 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10732 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10733 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10734 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10735 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10736 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10737 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10738 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10739 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10740 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10741 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10742 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10743 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10744 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10745 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10746 | `"&nbsp;*<br>"\` |
|        - | 10747 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10748 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10749 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10750 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10751 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10752 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10753 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10754 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10755 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10756 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10757 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10758 | `"&nbsp;*/<br>"\` |
|        - | 10759 | `"</span></small></small></p>"\` |
|        - | 10760 | `"</div></body></html>"` |
|        - | 10761 | `/*` |
|        - | 10762 | ` * bool ph7credits(void)` |
|        - | 10763 | ` * bool ph7info(void)` |
|        - | 10764 | ` * bool ph7copyright(void)` |
|        - | 10765 | ` *  Prints out the credits for PH7 engine` |
|        - | 10766 | ` * Parameters` |
|        - | 10767 | ` *  None` |
|        - | 10768 | ` * Return` |
|        - | 10769 | ` *  Always TRUE` |
|        - | 10770 | ` */` |
|        2 | 10771 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10772 |  |
|        3 | 10773 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10774 | `	/* Expand the HTML page above*/` |
|        3 | 10775 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10776 | `	ph7_context_output_format(` |
|        1 | 10777 | `		pCtx,` |
|        - | 10778 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10779 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10780 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10781 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10782 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10783 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10784 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10785 | `#ifdef __WINNT__` |
|        - | 10786 | `		"Windows NT"` |
|        - | 10787 | `#elif defined(__UNIXES__)` |
|        - | 10788 | `		"UNIX-Like"` |
|        - | 10789 | `#else` |
|        - | 10790 | `		"Other OS"` |
|        - | 10791 | `#endif` |
|        - | 10792 | `		);` |
|        3 | 10793 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10794 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10795 | `	SXUNUSED(apArg);` |
|        - | 10796 | `	/* Return TRUE */` |
|        - | 10797 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10798 | `	return PH7_OK;` |
|        1 | 10799 |  |
|        - | 10800 | `/*` |
|        - | 10801 | ` * Section:` |
|        - | 10802 | ` *    URL related routines.` |
|        - | 10803 | ` * Status:` |
|        - | 10804 | ` *    Stable.` |
|        - | 10805 | ` */` |
|        - | 10806 | `/*` |
|        - | 10807 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10808 | ` *  Parse a URL and return its fields.` |
|        - | 10809 | ` * Parameters` |
|        - | 10810 | ` *  $url` |
|        - | 10811 | ` *   The URL to parse.` |
|        - | 10812 | ` * $component` |
|        - | 10813 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10814 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10815 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10816 | ` *  in which case the return value will be an integer).` |
|        - | 10817 | ` * Return` |
|        - | 10818 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10819 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10820 | ` *  this array are:` |
|        - | 10821 | ` *   scheme - e.g. http` |
|        - | 10822 | ` *   host` |
|        - | 10823 | ` *   port` |
|        - | 10824 | ` *   user` |
|        - | 10825 | ` *   pass` |
|        - | 10826 | ` *   path` |
|        - | 10827 | ` *   query - after the question mark ?` |
|        - | 10828 | ` *   fragment - after the hashmark #` |
|        - | 10829 | ` * Note:` |
|        - | 10830 | ` *  FALSE is returned on failure.` |
|        - | 10831 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10832 | ` *  with the standard PHP engine.` |
|        - | 10833 | ` */` |
|       28 | 10834 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10835 |  |
|        - | 10836 | `	const char *zStr; /* Input string */` |
|        - | 10837 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10838 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10839 | `	int nLen;` |
|        - | 10840 | `	sxi32 rc;` |
|       29 | 10841 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10842 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10843 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10844 | `		return PH7_OK;` |
|        - | 10845 | `	}` |
|        - | 10846 | `	/* Extract the given URI */` |
|       29 | 10847 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10848 | `	if( nLen < 1 ){` |
|        - | 10849 | `		/* Nothing to process,return FALSE */` |
|        3 | 10850 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10851 | `		return PH7_OK;` |
|        - | 10852 | `	}` |
|        - | 10853 | `	/* Get a parse */` |
|       27 | 10854 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10855 | `	if( rc != SXRET_OK ){` |
|        - | 10856 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10857 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10858 | `		return PH7_OK;` |
|        - | 10859 | `	}` |
|       27 | 10860 | `	if( nArg > 1 ){` |
|      ! 0 | 10861 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10862 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10863 | `		switch(nComponent){` |
|      ! 0 | 10864 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10865 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10866 | `			if( pComp->nByte < 1 ){` |
|        - | 10867 | `				/* No available value,return NULL */` |
|      ! 0 | 10868 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10869 | `			}else{` |
|      ! 0 | 10870 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10871 | `			}` |
|      ! 0 | 10872 | `			break;` |
|      ! 0 | 10873 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10874 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10875 | `			if( pComp->nByte < 1 ){` |
|        - | 10876 | `				/* No available value,return NULL */` |
|      ! 0 | 10877 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10878 | `			}else{` |
|      ! 0 | 10879 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10880 | `			}` |
|      ! 0 | 10881 | `			break;` |
|      ! 0 | 10882 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10883 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10884 | `			if( pComp->nByte < 1 ){` |
|        - | 10885 | `				/* No available value,return NULL */` |
|      ! 0 | 10886 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10887 | `			}else{` |
|      ! 0 | 10888 | `				int iPort = 0;` |
|        - | 10889 | `				/* Cast the value to integer */` |
|      ! 0 | 10890 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10891 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10892 | `			}` |
|      ! 0 | 10893 | `			break;` |
|      ! 0 | 10894 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10895 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10896 | `			if( pComp->nByte < 1 ){` |
|        - | 10897 | `				/* No available value,return NULL */` |
|      ! 0 | 10898 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10899 | `			}else{` |
|      ! 0 | 10900 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10901 | `			}` |
|      ! 0 | 10902 | `			break;` |
|      ! 0 | 10903 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10904 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10905 | `			if( pComp->nByte < 1 ){` |
|        - | 10906 | `				/* No available value,return NULL */` |
|      ! 0 | 10907 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10908 | `			}else{` |
|      ! 0 | 10909 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10910 | `			}` |
|      ! 0 | 10911 | `			break;` |
|      ! 0 | 10912 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10913 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10914 | `			if( pComp->nByte < 1 ){` |
|        - | 10915 | `				/* No available value,return NULL */` |
|      ! 0 | 10916 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10917 | `			}else{` |
|      ! 0 | 10918 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10919 | `			}` |
|      ! 0 | 10920 | `			break;` |
|      ! 0 | 10921 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10922 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10923 | `			if( pComp->nByte < 1 ){` |
|        - | 10924 | `				/* No available value,return NULL */` |
|      ! 0 | 10925 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10926 | `			}else{` |
|      ! 0 | 10927 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10928 | `			}` |
|      ! 0 | 10929 | `			break;` |
|      ! 0 | 10930 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10931 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10932 | `			if( pComp->nByte < 1 ){` |
|        - | 10933 | `				/* No available value,return NULL */` |
|      ! 0 | 10934 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10935 | `			}else{` |
|      ! 0 | 10936 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10937 | `			}` |
|      ! 0 | 10938 | `			break;` |
|      ! 0 | 10939 | `		default:` |
|        - | 10940 | `			/* No such entry,return NULL */` |
|      ! 0 | 10941 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10942 | `			break;` |
|        - | 10943 | `		}` |
|      ! 0 | 10944 | `	}else{` |
|        - | 10945 | `		ph7_value *pArray,*pValue;` |
|        - | 10946 | `		/* Return an associative array */` |
|       27 | 10947 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10948 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10949 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10950 | `			/* Out of memory */` |
|      ! 0 | 10951 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10952 | `			/* Return false */` |
|      ! 0 | 10953 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10954 | `			return PH7_OK;` |
|        - | 10955 | `		}` |
|        - | 10956 | `		/* Fill the array */` |
|       27 | 10957 | `		pComp = &sURI.sScheme;` |
|       27 | 10958 | `		if( pComp->nByte > 0 ){` |
|       19 | 10959 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10960 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10961 | `		}` |
|        - | 10962 | `		/* Reset the string cursor */` |
|       27 | 10963 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10964 | `		pComp = &sURI.sHost;` |
|       27 | 10965 | `		if( pComp->nByte > 0 ){` |
|       25 | 10966 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10967 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10968 | `		}` |
|        - | 10969 | `		/* Reset the string cursor */` |
|       27 | 10970 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10971 | `		pComp = &sURI.sPort;` |
|       27 | 10972 | `		if( pComp->nByte > 0 ){` |
|       11 | 10973 | `			int iPort = 0;/* cc warning */` |
|        - | 10974 | `			/* Convert to integer */` |
|       11 | 10975 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10976 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10977 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10978 | `		}` |
|        - | 10979 | `		/* Reset the string cursor */` |
|       27 | 10980 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10981 | `		pComp = &sURI.sUser;` |
|       27 | 10982 | `		if( pComp->nByte > 0 ){` |
|        7 | 10983 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10984 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10985 | `		}` |
|        - | 10986 | `		/* Reset the string cursor */` |
|       27 | 10987 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10988 | `		pComp = &sURI.sPass;` |
|       27 | 10989 | `		if( pComp->nByte > 0 ){` |
|        7 | 10990 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10991 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10992 | `		}` |
|        - | 10993 | `		/* Reset the string cursor */` |
|       27 | 10994 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10995 | `		pComp = &sURI.sPath;` |
|       27 | 10996 | `		if( pComp->nByte > 0 ){` |
|       17 | 10997 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10998 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10999 | `		}` |
|        - | 11000 | `		/* Reset the string cursor */` |
|       27 | 11001 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11002 | `		pComp = &sURI.sQuery;` |
|       27 | 11003 | `		if( pComp->nByte > 0 ){` |
|        5 | 11004 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11005 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11006 | `		}` |
|        - | 11007 | `		/* Reset the string cursor */` |
|       27 | 11008 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11009 | `		pComp = &sURI.sFragment;` |
|       27 | 11010 | `		if( pComp->nByte > 0 ){` |
|        5 | 11011 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11012 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11013 | `		}` |
|        - | 11014 | `		/* Return the created array */` |
|       27 | 11015 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11016 | `		/* NOTE:` |
|        - | 11017 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11018 | `		 * automatically as soon we return from this function.` |
|        - | 11019 | `		 */` |
|        - | 11020 | `	}` |
|        - | 11021 | `	/* All done */` |
|       27 | 11022 | `	return PH7_OK;` |
|       15 | 11023 |  |
|        - | 11024 | `/*` |
|        - | 11025 | ` * Section:` |
|        - | 11026 | ` *   Array related routines.` |
|        - | 11027 | ` * Status:` |
|        - | 11028 | ` *    Stable.` |
|        - | 11029 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11030 | ` *  Array related functions that need access to the underlying` |
|        - | 11031 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11032 | ` */` |
|        - | 11033 | `/*` |
|        - | 11034 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11035 | ` * of the following structure.` |
|        - | 11036 | ` */` |
|        - | 11037 | `struct compact_data` |
|        - | 11038 |  |
|        - | 11039 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11040 | `	int nRecCount;      /* Recursion count */` |
|        - | 11041 | `};` |
|        - | 11042 | `/*` |
|        - | 11043 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11044 | ` */` |
|      ! 0 | 11045 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11046 |  |
|      ! 0 | 11047 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11048 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11049 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11050 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11051 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11052 | `		SyString sVar;` |
|      ! 0 | 11053 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11054 | `		if( sVar.nByte > 0 ){` |
|        - | 11055 | `			/* Query the current frame */` |
|      ! 0 | 11056 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11057 | `			/* ^` |
|        - | 11058 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11059 | `			 */` |
|      ! 0 | 11060 | `			if( pKey ){` |
|        - | 11061 | `				/* Perform the insertion */` |
|      ! 0 | 11062 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11063 | `			}` |
|      ! 0 | 11064 | `		}` |
|      ! 0 | 11065 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11066 | `		int rc;` |
|        - | 11067 | `		/* Recursively traverse this array */` |
|      ! 0 | 11068 | `		pData->nRecCount++;` |
|      ! 0 | 11069 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11070 | `		pData->nRecCount--;` |
|      ! 0 | 11071 | `		return rc;` |
|        - | 11072 | `	}` |
|      ! 0 | 11073 | `	return SXRET_OK;` |
|      ! 0 | 11074 |  |
|        - | 11075 | `/*` |
|        - | 11076 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11077 | ` *  Create array containing variables and their values.` |
|        - | 11078 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11079 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11080 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11081 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11082 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11083 | ` * Parameters` |
|        - | 11084 | ` *  $varname` |
|        - | 11085 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11086 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11087 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11088 | ` *   it recursively.` |
|        - | 11089 | ` * Return` |
|        - | 11090 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11091 | ` */` |
|        2 | 11092 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11093 |  |
|        - | 11094 | `	ph7_value *pArray,*pObj;` |
|        3 | 11095 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11096 | `	const char *zName;` |
|        - | 11097 | `	SyString sVar;` |
|        - | 11098 | `	int i,nLen;` |
|        3 | 11099 | `	if( nArg < 1 ){` |
|        - | 11100 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11101 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11102 | `		return PH7_OK;` |
|        - | 11103 | `	}` |
|        - | 11104 | `	/* Create the array */` |
|        3 | 11105 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11106 | `	if( pArray == 0 ){` |
|        - | 11107 | `		/* Out of memory */` |
|      ! 0 | 11108 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11109 | `		/* Return NULL */` |
|      ! 0 | 11110 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11111 | `		return PH7_OK;` |
|        - | 11112 | `	}` |
|        - | 11113 | `	/* Perform the requested operation */` |
|        7 | 11114 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11115 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11116 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11117 | `				struct compact_data sData;` |
|      ! 0 | 11118 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11119 | `				/* Recursively walk the array */` |
|      ! 0 | 11120 | `				sData.nRecCount = 0;` |
|      ! 0 | 11121 | `				sData.pArray = pArray;` |
|      ! 0 | 11122 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11123 | `			}` |
|      ! 0 | 11124 | `		}else{` |
|        - | 11125 | `			/* Extract variable name */` |
|        5 | 11126 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11127 | `			if( nLen > 0 ){` |
|        5 | 11128 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11129 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11130 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11131 | `				if( pObj ){` |
|        5 | 11132 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11133 | `				}` |
|        2 | 11134 | `			}` |
|        - | 11135 | `		}` |
|        3 | 11136 | `	}` |
|        - | 11137 | `	/* Return the array */` |
|        3 | 11138 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11139 | `	return PH7_OK;` |
|        2 | 11140 |  |
|        - | 11141 | `/*` |
|        - | 11142 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11143 | ` * of the following structure.` |
|        - | 11144 | ` */` |
|        - | 11145 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11146 | `struct extract_aux_data` |
|        - | 11147 |  |
|        - | 11148 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11149 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11150 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11151 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11152 | `	int iFlags;           /* Control flags */` |
|        - | 11153 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11154 | `};` |
|        - | 11155 | `/* Forward declaration */` |
|        - | 11156 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11157 | `/*` |
|        - | 11158 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11159 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11160 | ` * Parameters` |
|        - | 11161 | ` * $var_array` |
|        - | 11162 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11163 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11164 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11165 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11166 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11167 | ` * $extract_type` |
|        - | 11168 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11169 | ` *  It can be one of the following values:` |
|        - | 11170 | ` *   EXTR_OVERWRITE` |
|        - | 11171 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11172 | ` *   EXTR_SKIP` |
|        - | 11173 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11174 | ` *   EXTR_PREFIX_SAME` |
|        - | 11175 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11176 | ` *   EXTR_PREFIX_ALL` |
|        - | 11177 | ` *       Prefix all variable names with prefix.` |
|        - | 11178 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11179 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11180 | ` *   EXTR_IF_EXISTS` |
|        - | 11181 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11182 | ` *       otherwise do nothing.` |
|        - | 11183 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11184 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11185 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11186 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11187 | ` *      the current symbol table.` |
|        - | 11188 | ` * $prefix` |
|        - | 11189 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11190 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11191 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11192 | ` *  underscore character.` |
|        - | 11193 | ` * Return` |
|        - | 11194 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11195 | ` */` |
|        4 | 11196 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11197 |  |
|        - | 11198 | `	extract_aux_data sAux;` |
|        - | 11199 | `	ph7_hashmap *pMap;` |
|        5 | 11200 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11201 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11202 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11203 | `		return PH7_OK;` |
|        - | 11204 | `	}` |
|        - | 11205 | `	/* Point to the target hashmap */` |
|        5 | 11206 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11207 | `	if( pMap->nEntry < 1 ){` |
|        - | 11208 | `		/* Empty map,return  0 */` |
|      ! 0 | 11209 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11210 | `		return PH7_OK;` |
|        - | 11211 | `	}` |
|        - | 11212 | `	/* Prepare the aux data */` |
|        5 | 11213 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11214 | `	if( nArg > 1 ){` |
|        3 | 11215 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11216 | `		if( nArg > 2 ){` |
|      ! 0 | 11217 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11218 | `		}` |
|        1 | 11219 | `	}` |
|        5 | 11220 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11221 | `	/* Invoke the worker callback */` |
|        5 | 11222 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11223 | `	/* Number of variables successfully imported */` |
|        5 | 11224 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11225 | `	return PH7_OK;` |
|        3 | 11226 |  |
|        - | 11227 | `/*` |
|        - | 11228 | ` * Worker callback for the [extract()] function defined` |
|        - | 11229 | ` * below.` |
|        - | 11230 | ` */` |
|        8 | 11231 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11232 |  |
|        9 | 11233 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11234 | `	int iFlags = pAux->iFlags;` |
|        9 | 11235 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11236 | `	ph7_value *pObj;` |
|        - | 11237 | `	SyString sVar;` |
|        9 | 11238 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11239 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11240 | `	}` |
|        - | 11241 | `	/* Perform a string cast */` |
|        9 | 11242 | `	PH7_MemObjToString(pKey);` |
|        9 | 11243 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11244 | `		/* Unavailable variable name */` |
|      ! 0 | 11245 | `		return SXRET_OK;` |
|        - | 11246 | `	}` |
|        9 | 11247 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11248 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11249 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11250 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11251 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11252 | `			);` |
|      ! 0 | 11253 | `	}else{` |
|       13 | 11254 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11255 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11256 | `	}` |
|        9 | 11257 | `	sVar.zString = pAux->zWorker;` |
|        - | 11258 | `	/* Try to extract the variable */` |
|        9 | 11259 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11260 | `	if( pObj ){` |
|        - | 11261 | `		/* Collision */` |
|        5 | 11262 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11263 | `			return SXRET_OK;` |
|        - | 11264 | `		}` |
|        5 | 11265 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11266 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11267 | `				/* Already prefixed */` |
|      ! 0 | 11268 | `				return SXRET_OK;` |
|        - | 11269 | `			}` |
|      ! 0 | 11270 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11271 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11272 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11273 | `				);` |
|      ! 0 | 11274 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11275 | `		}` |
|        3 | 11276 | `	}else{` |
|        - | 11277 | `		/* Create the variable */` |
|        5 | 11278 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11279 | `	}` |
|        9 | 11280 | `	if( pObj ){` |
|        - | 11281 | `		/* Overwrite the old value */` |
|        9 | 11282 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11283 | `		/* Increment counter */` |
|        9 | 11284 | `		pAux->iCount++;` |
|        4 | 11285 | `	}` |
|        9 | 11286 | `	return SXRET_OK;` |
|        5 | 11287 |  |
|        - | 11288 | `/*` |
|        - | 11289 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11290 | ` * defined below.` |
|        - | 11291 | ` */` |
|        2 | 11292 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11293 |  |
|        3 | 11294 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11295 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11296 | `	ph7_value *pObj;` |
|        - | 11297 | `	SyString sVar;` |
|        - | 11298 | `	/* Perform a string cast */` |
|        3 | 11299 | `	PH7_MemObjToString(pKey);` |
|        3 | 11300 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11301 | `		/* Unavailable variable name */` |
|      ! 0 | 11302 | `		return SXRET_OK;` |
|        - | 11303 | `	}` |
|        3 | 11304 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11305 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11306 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11307 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11308 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11309 | `			);` |
|        2 | 11310 | `	}else{` |
|      ! 0 | 11311 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11312 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11313 | `	}` |
|        3 | 11314 | `	sVar.zString = pAux->zWorker;` |
|        - | 11315 | `	/* Extract the variable */` |
|        3 | 11316 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11317 | `	if( pObj ){` |
|        3 | 11318 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11319 | `	}` |
|        3 | 11320 | `	return SXRET_OK;` |
|        2 | 11321 |  |
|        - | 11322 | `/*` |
|        - | 11323 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11324 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11325 | ` * Parameters` |
|        - | 11326 | ` * $types` |
|        - | 11327 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11328 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11329 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11330 | ` *  POST includes the POST uploaded file information.` |
|        - | 11331 | ` *  Note:` |
|        - | 11332 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11333 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11334 | ` * $prefix` |
|        - | 11335 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11336 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11337 | ` *  variable named $pref_userid.` |
|        - | 11338 | ` * Return` |
|        - | 11339 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11340 | ` */` |
|        2 | 11341 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11342 |  |
|        - | 11343 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11344 | `	extract_aux_data sAux;` |
|        - | 11345 | `	int nLen,nPrefixLen;` |
|        - | 11346 | `	ph7_value *pSuper;` |
|        - | 11347 | `	ph7_vm *pVm;` |
|        - | 11348 | `	/* By default import only $_GET variables  */` |
|        3 | 11349 | `	zImport = "G";` |
|        3 | 11350 | `	nLen = (int)sizeof(char);` |
|        3 | 11351 | `	zPrefix = 0;` |
|        3 | 11352 | `	nPrefixLen = 0;` |
|        3 | 11353 | `	if( nArg > 0 ){` |
|        3 | 11354 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11355 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11356 | `		}` |
|        3 | 11357 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11358 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11359 | `		}` |
|        1 | 11360 | `	}` |
|        - | 11361 | `	/* Point to the underlying VM */` |
|        3 | 11362 | `	pVm = pCtx->pVm;` |
|        - | 11363 | `	/* Initialize the aux data */` |
|        3 | 11364 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11365 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11366 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11367 | `	sAux.pVm = pVm;` |
|        - | 11368 | `	/* Extract */` |
|        3 | 11369 | `	zEnd = &zImport[nLen];` |
|        5 | 11370 | `	while( zImport < zEnd ){` |
|        3 | 11371 | `		int c = zImport[0];` |
|        3 | 11372 | `		pSuper = 0;` |
|        3 | 11373 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11374 | `			/* Import $_GET variables */` |
|        3 | 11375 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11376 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11377 | `			/* Import $_POST variables */` |
|      ! 0 | 11378 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11379 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11380 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11381 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11382 | `		}` |
|        3 | 11383 | `		if( pSuper ){` |
|        - | 11384 | `			/* Iterate throw array entries */` |
|        3 | 11385 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11386 | `		}` |
|        - | 11387 | `		/* Advance the cursor */` |
|        3 | 11388 | `		zImport++;` |
|        1 | 11389 | `	}` |
|        - | 11390 | `	/* All done,return TRUE*/` |
|        3 | 11391 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11392 | `	return PH7_OK;` |
|        1 | 11393 |  |
|        - | 11394 | `/*` |
|        - | 11395 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11396 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11397 | ` * information.` |
|        - | 11398 | ` */` |
|    10612 | 11399 | `static sxi32 VmEvalChunk(` |
|        - | 11400 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11401 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11402 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11403 | `	int iFlags,         /* Compile flag */` |
|        - | 11404 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11405 | `	)` |
|        2 | 11406 |  |
|        - | 11407 | `	SySet *pByteCode,aByteCode;` |
|        - | 11408 | `	SyBlob sSavedNs;` |
|    10614 | 11409 | `	ProcConsumer xErr = 0;` |
|    10614 | 11410 | `	void *pErrData = 0;` |
|        - | 11411 | `	/* Initialize bytecode container */` |
|    10614 | 11412 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10614 | 11413 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11414 | `	/* Reset the code generator */` |
|    10614 | 11415 | `	if( bTrueReturn ){` |
|        - | 11416 | `		/* Included file,log compile-time errors */` |
|     8018 | 11417 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8018 | 11418 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4008 | 11419 | `	}` |
|    10614 | 11420 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11421 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11422 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11423 | `	 * the caller's namespace is restored. */` |
|    10614 | 11424 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10614 | 11425 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10614 | 11426 | `	if( bTrueReturn ){` |
|        - | 11427 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8018 | 11428 | `		SyBlobReset(&pVm->sNamespace);` |
|     4008 | 11429 | `	}` |
|        - | 11430 | `	/* Swap bytecode container */` |
|    10614 | 11431 | `	pByteCode = pVm->pByteContainer;` |
|    10614 | 11432 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11433 | `	/* Compile the chunk */` |
|    10614 | 11434 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15920 | 11435 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11436 | `		/* Compilation error,return false */` |
|        3 | 11437 | `		if( pCtx ){` |
|        3 | 11438 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11439 | `		}` |
|        2 | 11440 | `	}else{` |
|        - | 11441 | `		/* Mount any newly defined classes */` |
|        - | 11442 | `		SyHashEntry *pEntry;` |
|        - | 11443 | `		ph7_class *pClass;` |
|        - | 11444 | `		ph7_value sResult; /* Return value */` |
|        - | 11445 | `		sxi32 rc;` |
|    10612 | 11446 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   342355 | 11447 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   326440 | 11448 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11449 | `			/* Only mount classes that haven't been mounted yet */` |
|   326440 | 11450 | `			if( !pClass->bMounted ){` |
|    76316 | 11451 | `				rc = VmMountUserClass(pVm,pClass);` |
|    76316 | 11452 | `				if( rc != SXRET_OK ){` |
|        - | 11453 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11454 | `					if( pCtx ){` |
|      ! 0 | 11455 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11456 | `					}` |
|      ! 0 | 11457 | `					goto Cleanup;` |
|        - | 11458 | `				}` |
|    38157 | 11459 | `			}` |
|        2 | 11460 | `		}` |
|    10612 | 11461 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11462 | `			/* Out of memory */` |
|      ! 0 | 11463 | `			if( pCtx ){` |
|      ! 0 | 11464 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11465 | `			}` |
|      ! 0 | 11466 | `			goto Cleanup;` |
|        - | 11467 | `		}` |
|    10612 | 11468 | `		if( bTrueReturn ){` |
|        - | 11469 | `			/* Assume a boolean true return value */` |
|     8018 | 11470 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4010 | 11471 | `		}else{` |
|        - | 11472 | `			/* Assume a null return value */` |
|     2596 | 11473 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11474 | `		}` |
|        - | 11475 | `		/* Execute the compiled chunk */` |
|    10612 | 11476 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10612 | 11477 | `		if( pCtx ){` |
|        - | 11478 | `			/* Set the execution result */` |
|     8036 | 11479 | `			ph7_result_value(pCtx,&sResult);` |
|     4017 | 11480 | `		}` |
|    10612 | 11481 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11482 | `	}` |
|     5306 | 11483 | `Cleanup:` |
|        - | 11484 | `	/* Cleanup the mess left behind */` |
|    10614 | 11485 | `	pVm->pByteContainer = pByteCode;` |
|    10614 | 11486 | `	SySetRelease(&aByteCode);` |
|        - | 11487 | `	/* Restore caller's namespace state */` |
|    10614 | 11488 | `	SyBlobReset(&pVm->sNamespace);` |
|    10614 | 11489 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10614 | 11490 | `	SyBlobRelease(&sSavedNs);` |
|    10614 | 11491 | `	return SXRET_OK;` |
|        2 | 11492 |  |
|        - | 11493 | `/*` |
|        - | 11494 | ` * value eval(string $code)` |
|        - | 11495 | ` *   Evaluate a string as PHP code.` |
|        - | 11496 | ` * Parameter` |
|        - | 11497 | ` *  code: PHP code to evaluate.` |
|        - | 11498 | ` * Return` |
|        - | 11499 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11500 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11501 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11502 | ` */` |
|       22 | 11503 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11504 |  |
|        - | 11505 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11506 | `	if( nArg < 1 ){` |
|        - | 11507 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11508 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11509 | `		return SXRET_OK;` |
|        - | 11510 | `	}` |
|        - | 11511 | `	/* Chunk to evaluate */` |
|       24 | 11512 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11513 | `	if( sChunk.nByte < 1 ){` |
|        - | 11514 | `		/* Empty string,return NULL */` |
|        3 | 11515 | `		ph7_result_null(pCtx);` |
|        3 | 11516 | `		return SXRET_OK;` |
|        - | 11517 | `	}` |
|        - | 11518 | `	/* Eval the chunk */` |
|       22 | 11519 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11520 | `	return SXRET_OK;` |
|       13 | 11521 |  |
|        - | 11522 | `/*` |
|        - | 11523 | ` * Check if a file path is already included.` |
|        - | 11524 | ` */` |
|    16028 | 11525 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11526 |  |
|        - | 11527 | `	SyString *aEntries;` |
|        - | 11528 | `	sxu32 n;` |
|    16030 | 11529 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11530 | `	/* Perform a linear search */` |
| 64180258 | 11531 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 64164236 | 11532 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11533 | `			/* Already included */` |
|        7 | 11534 | `			return TRUE;` |
|        - | 11535 | `		}` |
| 32082116 | 11536 | `	}` |
|    16024 | 11537 | `	return FALSE;` |
|     8016 | 11538 |  |
|        - | 11539 | `/*` |
|        - | 11540 | ` * Push a file path in the appropriate VM container.` |
|        - | 11541 | ` */` |
|    18596 | 11542 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11543 |  |
|        - | 11544 | `	SyString sPath;` |
|        - | 11545 | `	char *zDup;` |
|        - | 11546 | `#ifdef __WINNT__` |
|        - | 11547 | `	char *zCur;` |
|        - | 11548 | `#endif` |
|        - | 11549 | `	sxi32 rc;` |
|    18598 | 11550 | `	if( nLen < 0 ){` |
|     2570 | 11551 | `		nLen = SyStrlen(zPath);` |
|     1284 | 11552 | `	}` |
|        - | 11553 | `	/* Duplicate the file path first */` |
|    18598 | 11554 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18598 | 11555 | `	if( zDup == 0 ){` |
|      ! 0 | 11556 | `		return SXERR_MEM;` |
|        - | 11557 | `	}` |
|        - | 11558 | `#ifdef __WINNT__` |
|        - | 11559 | `	/* Normalize path on windows` |
|        - | 11560 | `	 * Example:` |
|        - | 11561 | `	 *    Path/To/File.php` |
|        - | 11562 | `	 * becomes` |
|        - | 11563 | `	 *   path\to\file.php` |
|        - | 11564 | `	 */` |
|        2 | 11565 | `	zCur = zDup;` |
|        2 | 11566 | `	while( zCur[0] != 0 ){` |
|        2 | 11567 | `		if( zCur[0] == '/' ){` |
|        2 | 11568 | `			zCur[0] = '\\';` |
|        2 | 11569 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11570 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11571 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11572 | `		}` |
|        2 | 11573 | `		zCur++;` |
|        2 | 11574 | `	}` |
|        - | 11575 | `#endif` |
|        - | 11576 | `	/* Install the file path */` |
|    18598 | 11577 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18598 | 11578 | `	if( !bMain ){` |
|    16030 | 11579 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11580 | `			/* Already included */` |
|        7 | 11581 | `			*pNew = 0;` |
|        4 | 11582 | `		}else{` |
|        - | 11583 | `			/* Insert in the corresponding container */` |
|    16024 | 11584 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16024 | 11585 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11586 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11587 | `				return rc;` |
|        - | 11588 | `			}` |
|    16024 | 11589 | `			*pNew = 1;` |
|        - | 11590 | `		}` |
|     8014 | 11591 | `	}` |
|    18598 | 11592 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18598 | 11593 | `	return SXRET_OK;` |
|     9300 | 11594 |  |
|        - | 11595 | `/*` |
|        - | 11596 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11597 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11598 | ` * indicates failure.` |
|        - | 11599 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11600 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11601 | ` * operations.` |
|        - | 11602 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11603 | ` * this function is a no-op.` |
|        - | 11604 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11605 | ` * constructs for more information.` |
|        - | 11606 | ` */` |
|     8026 | 11607 | `static sxi32 VmExecIncludedFile(` |
|        - | 11608 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11609 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11610 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11611 | `	 )` |
|        2 | 11612 |  |
|        - | 11613 | `	sxi32 rc;` |
|        - | 11614 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11615 | `	const ph7_io_stream *pStream;` |
|        - | 11616 | `	SyBlob sContents;` |
|        - | 11617 | `	void *pHandle;` |
|        - | 11618 | `	ph7_vm *pVm;` |
|        - | 11619 | `	int isNew;` |
|        - | 11620 | `	/* Initialize fields */` |
|     8028 | 11621 | `	pVm = pCtx->pVm;` |
|     8028 | 11622 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8028 | 11623 | `	isNew = 0;` |
|        - | 11624 | `	/* Extract the associated stream */` |
|     8028 | 11625 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11626 | `	/*` |
|        - | 11627 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11628 | `	 * in a read-only mode.` |
|        - | 11629 | `	 */` |
|     8028 | 11630 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8028 | 11631 | `	if( pHandle == 0 ){` |
|        8 | 11632 | `		return SXERR_IO;` |
|        - | 11633 | `	}` |
|     8022 | 11634 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8022 | 11635 | `	if( IncludeOnce && !isNew ){` |
|        - | 11636 | `		/* Already included */` |
|        5 | 11637 | `		rc = SXERR_EXISTS;` |
|        3 | 11638 | `	}else{` |
|        - | 11639 | `		/* Read the whole file contents */` |
|     8018 | 11640 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8018 | 11641 | `		if( rc == SXRET_OK ){` |
|        - | 11642 | `			SyString sScript;` |
|        - | 11643 | `			/* Compile and execute the script */` |
|     8018 | 11644 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8018 | 11645 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4008 | 11646 | `		}` |
|        - | 11647 | `	}` |
|        - | 11648 | `	/* Pop from the set of included file */` |
|     8022 | 11649 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11650 | `	/* Close the handle */` |
|     8022 | 11651 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11652 | `	/* Release the working buffer */` |
|     8022 | 11653 | `	SyBlobRelease(&sContents);` |
|        - | 11654 | `#else` |
|        - | 11655 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11656 | `	SXUNUSED(pPath);` |
|        - | 11657 | `	SXUNUSED(IncludeOnce);` |
|        - | 11658 | `	rc = SXERR_IO;` |
|        - | 11659 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8022 | 11660 | `	return rc;` |
|     4015 | 11661 |  |
|        - | 11662 | `/*` |
|        - | 11663 | ` * string get_include_path(void)` |
|        - | 11664 | ` *  Gets the current include_path configuration option.` |
|        - | 11665 | ` * Parameter` |
|        - | 11666 | ` *  None` |
|        - | 11667 | ` * Return` |
|        - | 11668 | ` *  Included paths as a string` |
|        - | 11669 | ` */` |
|        2 | 11670 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11671 |  |
|        3 | 11672 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11673 | `	SyString *aEntry;` |
|        - | 11674 | `	int dir_sep;` |
|        - | 11675 | `	sxu32 n;` |
|        - | 11676 | `#ifdef __WINNT__` |
|        1 | 11677 | `	dir_sep = ';';` |
|        - | 11678 | `#else` |
|        - | 11679 | `	/* Assume UNIX path separator */` |
|        2 | 11680 | `	dir_sep = ':';` |
|        - | 11681 | `#endif` |
|        1 | 11682 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11683 | `	SXUNUSED(apArg);` |
|        - | 11684 | `	/* Point to the list of import paths */` |
|        3 | 11685 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11686 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11687 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11688 | `		if( n > 0 ){` |
|        - | 11689 | `			/* Append dir seprator */` |
|      ! 0 | 11690 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11691 | `		}` |
|        - | 11692 | `		/* Append path */` |
|        3 | 11693 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11694 | `	}` |
|        3 | 11695 | `	return PH7_OK;` |
|        1 | 11696 |  |
|        - | 11697 | `/*` |
|        - | 11698 | ` * string get_get_included_files(void)` |
|        - | 11699 | ` *  Gets the current include_path configuration option.` |
|        - | 11700 | ` * Parameter` |
|        - | 11701 | ` *  None` |
|        - | 11702 | ` * Return` |
|        - | 11703 | ` *  Included paths as a string` |
|        - | 11704 | ` */` |
|        2 | 11705 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11706 |  |
|        3 | 11707 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11708 | `	ph7_value *pArray,*pWorker;` |
|        - | 11709 | `	SyString *pEntry;` |
|        - | 11710 | `	int c,d;` |
|        - | 11711 | `	/* Create an array and a working value */` |
|        3 | 11712 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11713 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11714 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11715 | `		/* Out of memory,return null */` |
|      ! 0 | 11716 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11717 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11718 | `		SXUNUSED(apArg);` |
|      ! 0 | 11719 | `		return PH7_OK;` |
|        - | 11720 | `	}` |
|        3 | 11721 | `	c = d = '/';` |
|        - | 11722 | `#ifdef __WINNT__` |
|        1 | 11723 | `	d = '\\';` |
|        - | 11724 | `#endif` |
|        - | 11725 | `	/* Iterate throw entries */` |
|        3 | 11726 | `	SySetResetCursor(pFiles);` |
|     3811 | 11727 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11728 | `		const char *zBase,*zEnd;` |
|        - | 11729 | `		int iLen;` |
|        - | 11730 | `		/* reset the string cursor */` |
|     3809 | 11731 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11732 | `		/* Extract base name */` |
|     3809 | 11733 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11734 | `		/* Ignore trailing '/' */` |
|     5713 | 11735 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11736 | `			zEnd--;` |
|      ! 0 | 11737 | `		}` |
|     3809 | 11738 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   117501 | 11739 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   111789 | 11740 | `			zEnd--;` |
|        1 | 11741 | `		}` |
|     3809 | 11742 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3809 | 11743 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11744 | `		/* Copy entry name */` |
|     3809 | 11745 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11746 | `		/* Perform the insertion */` |
|     3809 | 11747 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11748 | `	}` |
|        - | 11749 | `	/* All done,return the created array */` |
|        3 | 11750 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11751 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11752 | `	 * by the engine as soon we return from this foreign` |
|        - | 11753 | `	 * function.` |
|        - | 11754 | `	 */` |
|        3 | 11755 | `	return PH7_OK;` |
|        2 | 11756 |  |
|        - | 11757 | `/*` |
|        - | 11758 | ` * include:` |
|        - | 11759 | ` * According to the PHP reference manual.` |
|        - | 11760 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11761 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11762 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11763 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11764 | ` *  and the current working directory before failing. The include()` |
|        - | 11765 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11766 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11767 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11768 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11769 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11770 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11771 | ` *  directory to find the requested file.` |
|        - | 11772 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11773 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11774 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11775 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11776 | ` */` |
|     8008 | 11777 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11778 |  |
|        - | 11779 | `	SyString sFile;` |
|        - | 11780 | `	sxi32 rc;` |
|     8010 | 11781 | `	if( nArg < 1 ){` |
|        - | 11782 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11783 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11784 | `		return SXRET_OK;` |
|        - | 11785 | `	}` |
|        - | 11786 | `	/* File to include */` |
|     8010 | 11787 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8010 | 11788 | `	if( sFile.nByte < 1 ){` |
|        - | 11789 | `		/* Empty string,return NULL */` |
|      ! 0 | 11790 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11791 | `		return SXRET_OK;` |
|        - | 11792 | `	}` |
|        - | 11793 | `	/* Open,compile and execute the desired script */` |
|     8010 | 11794 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8010 | 11795 | `	if( rc != SXRET_OK ){` |
|        - | 11796 | `		/* Emit a warning and return false */` |
|        3 | 11797 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11798 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11799 | `	}` |
|     8010 | 11800 | `	return SXRET_OK;` |
|     4006 | 11801 |  |
|        - | 11802 | `/*` |
|        - | 11803 | ` * include_once:` |
|        - | 11804 | ` *  According to the PHP reference manual.` |
|        - | 11805 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11806 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11807 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11808 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11809 | ` *   just once.` |
|        - | 11810 | ` */` |
|        4 | 11811 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11812 |  |
|        - | 11813 | `	SyString sFile;` |
|        - | 11814 | `	sxi32 rc;` |
|        5 | 11815 | `	if( nArg < 1 ){` |
|        - | 11816 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11817 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11818 | `		return SXRET_OK;` |
|        - | 11819 | `	}` |
|        - | 11820 | `	/* File to include */` |
|        5 | 11821 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11822 | `	if( sFile.nByte < 1 ){` |
|        - | 11823 | `		/* Empty string,return NULL */` |
|      ! 0 | 11824 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11825 | `		return SXRET_OK;` |
|        - | 11826 | `	}` |
|        - | 11827 | `	/* Open,compile and execute the desired script */` |
|        5 | 11828 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11829 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11830 | `		/* File already included,return TRUE */` |
|        3 | 11831 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11832 | `		return SXRET_OK;` |
|        - | 11833 | `	}` |
|        3 | 11834 | `	if( rc != SXRET_OK ){` |
|        - | 11835 | `		/* Emit a warning and return false */` |
|      ! 0 | 11836 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11837 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11838 | ` 	}` |
|        3 | 11839 | `	return SXRET_OK;` |
|        3 | 11840 |  |
|        - | 11841 | `/*` |
|        - | 11842 | ` * require.` |
|        - | 11843 | ` *  According to the PHP reference manual.` |
|        - | 11844 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11845 | ` *   also produce a fatal level error.` |
|        - | 11846 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11847 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11848 | ` */` |
|        6 | 11849 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11850 |  |
|        - | 11851 | `	SyString sFile;` |
|        - | 11852 | `	sxi32 rc;` |
|        8 | 11853 | `	if( nArg < 1 ){` |
|        - | 11854 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11855 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11856 | `		return SXRET_OK;` |
|        - | 11857 | `	}` |
|        - | 11858 | `	/* File to include */` |
|        8 | 11859 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11860 | `	if( sFile.nByte < 1 ){` |
|        - | 11861 | `		/* Empty string,return NULL */` |
|      ! 0 | 11862 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11863 | `		return SXRET_OK;` |
|        - | 11864 | `	}` |
|        - | 11865 | `	/* Open,compile and execute the desired script */` |
|        8 | 11866 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 11867 | `	if( rc != SXRET_OK ){` |
|        - | 11868 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11869 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11870 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11871 | `		return PH7_ABORT;` |
|        - | 11872 | `	}` |
|        8 | 11873 | `	return SXRET_OK;` |
|        5 | 11874 |  |
|        - | 11875 | `/*` |
|        - | 11876 | ` * require_once:` |
|        - | 11877 | ` *  According to the PHP reference manual.` |
|        - | 11878 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11879 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11880 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11881 | ` *   and how it differs from its non _once siblings.` |
|        - | 11882 | ` */` |
|        4 | 11883 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11884 |  |
|        - | 11885 | `	SyString sFile;` |
|        - | 11886 | `	sxi32 rc;` |
|        5 | 11887 | `	if( nArg < 1 ){` |
|        - | 11888 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11889 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11890 | `		return SXRET_OK;` |
|        - | 11891 | `	}` |
|        - | 11892 | `	/* File to include */` |
|        5 | 11893 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11894 | `	if( sFile.nByte < 1 ){` |
|        - | 11895 | `		/* Empty string,return NULL */` |
|      ! 0 | 11896 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11897 | `		return SXRET_OK;` |
|        - | 11898 | `	}` |
|        - | 11899 | `	/* Open,compile and execute the desired script */` |
|        5 | 11900 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11901 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11902 | `		/* File already included,return TRUE */` |
|        3 | 11903 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11904 | `		return SXRET_OK;` |
|        - | 11905 | `	}` |
|        3 | 11906 | `	if( rc != SXRET_OK ){` |
|        - | 11907 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11908 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11909 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11910 | `		return PH7_ABORT;` |
|        - | 11911 | `	}` |
|        3 | 11912 | `	return SXRET_OK;` |
|        3 | 11913 |  |
|        - | 11914 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11915 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11916 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11917 | `/*` |
|        - | 11918 | ` * Section:` |
|        - | 11919 | ` *  SPL Autoloading functions.` |
|        - | 11920 | ` * Status:` |
|        - | 11921 | ` *  Stable.` |
|        - | 11922 | ` */` |
|        - | 11923 | `/*` |
|        - | 11924 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 11925 | ` *  Register given function as __autoload() implementation.` |
|        - | 11926 | ` * Parameters` |
|        - | 11927 | ` *  callback` |
|        - | 11928 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 11929 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 11930 | ` *  throw` |
|        - | 11931 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 11932 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 11933 | ` *  prepend` |
|        - | 11934 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 11935 | ` *   autoload stack instead of appending it.` |
|        - | 11936 | ` * Return` |
|        - | 11937 | ` *  TRUE on success, FALSE on failure.` |
|        - | 11938 | ` */` |
|       34 | 11939 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11940 |  |
|        - | 11941 | `	VmAutoloadCB sEntry;` |
|       36 | 11942 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 11943 | `	int iPrepend = 0;` |
|        - | 11944 | `	sxu32 n;` |
|       36 | 11945 | `	if( nArg < 1 ){` |
|        - | 11946 | `		/* No callback provided — register default spl_autoload.` |
|        - | 11947 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 11948 | `		/* Check for duplicates first */` |
|        9 | 11949 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 11950 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 11951 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 11952 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 11953 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 11954 | `				ph7_result_bool(pCtx,1);` |
|        5 | 11955 | `				return SXRET_OK;` |
|        - | 11956 | `			}` |
|      ! 0 | 11957 | `		}` |
|        5 | 11958 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 11959 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 11960 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 11961 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 11962 | `		ph7_result_bool(pCtx,1);` |
|        5 | 11963 | `		return SXRET_OK;` |
|        - | 11964 | `	}` |
|        - | 11965 | `	/* Validate that the callback is callable */` |
|       28 | 11966 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 11967 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 11968 | `		if( nArg >= 2 ){` |
|      ! 0 | 11969 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11970 | `		}` |
|      ! 0 | 11971 | `		if( iThrow ){` |
|      ! 0 | 11972 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 11973 | `				"Argument is not callable");` |
|      ! 0 | 11974 | `		}` |
|      ! 0 | 11975 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11976 | `		return SXRET_OK;` |
|        - | 11977 | `	}` |
|        - | 11978 | `	/* Check for duplicates */` |
|       46 | 11979 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 11980 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 11981 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 11982 | `			/* Already registered */` |
|      ! 0 | 11983 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11984 | `			return SXRET_OK;` |
|        - | 11985 | `		}` |
|       11 | 11986 | `	}` |
|        - | 11987 | `	/* Check prepend flag */` |
|       28 | 11988 | `	if( nArg >= 3 ){` |
|        3 | 11989 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 11990 | `	}` |
|        - | 11991 | `	/* Store the callback */` |
|       28 | 11992 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 11993 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 11994 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 11995 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 11996 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 11997 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 11998 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 11999 | `		VmAutoloadCB *aBase;` |
|        3 | 12000 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12001 | `		/* Rotate: move last entry to front */` |
|        3 | 12002 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12003 | `		if( aBase ){` |
|        - | 12004 | `			VmAutoloadCB sTemp;` |
|        - | 12005 | `			sxu32 i;` |
|        3 | 12006 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12007 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12008 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12009 | `			}` |
|        3 | 12010 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12011 | `		}` |
|        2 | 12012 | `	}else{` |
|       26 | 12013 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12014 | `	}` |
|       28 | 12015 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12016 | `	return SXRET_OK;` |
|       19 | 12017 |  |
|        - | 12018 | `/*` |
|        - | 12019 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12020 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12021 | ` * Parameters` |
|        - | 12022 | ` *  callback` |
|        - | 12023 | ` *   The autoload function being unregistered.` |
|        - | 12024 | ` * Return` |
|        - | 12025 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12026 | ` */` |
|       32 | 12027 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12028 |  |
|       34 | 12029 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12030 | `	sxu32 n,nEntry;` |
|       34 | 12031 | `	if( nArg < 1 ){` |
|      ! 0 | 12032 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12033 | `		return SXRET_OK;` |
|        - | 12034 | `	}` |
|       34 | 12035 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12036 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12037 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12038 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12039 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12040 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12041 | `			sxu32 i;` |
|       32 | 12042 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12043 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12044 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12045 | `			}` |
|        - | 12046 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12047 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12048 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12049 | `			return SXRET_OK;` |
|        - | 12050 | `		}` |
|        3 | 12051 | `	}` |
|        3 | 12052 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12053 | `	return SXRET_OK;` |
|       18 | 12054 |  |
|        - | 12055 | `/*` |
|        - | 12056 | ` * array spl_autoload_functions(void)` |
|        - | 12057 | ` *  Return all registered __autoload() functions.` |
|        - | 12058 | ` * Return` |
|        - | 12059 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12060 | ` *  an empty array is returned.` |
|        - | 12061 | ` */` |
|       20 | 12062 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12063 |  |
|       21 | 12064 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12065 | `	ph7_value *pArray;` |
|        - | 12066 | `	sxu32 n,nEntry;` |
|       10 | 12067 | `	SXUNUSED(nArg);` |
|       10 | 12068 | `	SXUNUSED(apArg);` |
|       21 | 12069 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12070 | `	if( pArray == 0 ){` |
|      ! 0 | 12071 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12072 | `		return SXRET_OK;` |
|        - | 12073 | `	}` |
|       21 | 12074 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12075 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12076 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12077 | `		if( pEntry ){` |
|       15 | 12078 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12079 | `		}` |
|        8 | 12080 | `	}` |
|       21 | 12081 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12082 | `	return SXRET_OK;` |
|       11 | 12083 |  |
|        - | 12084 | `/*` |
|        - | 12085 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12086 | ` *  Default implementation of __autoload().` |
|        - | 12087 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12088 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12089 | ` * Parameters` |
|        - | 12090 | ` *  class` |
|        - | 12091 | ` *   The class name being searched.` |
|        - | 12092 | ` *  file_extensions` |
|        - | 12093 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12094 | ` */` |
|        2 | 12095 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12096 |  |
|        - | 12097 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12098 | `	SyBlob sPath;` |
|        - | 12099 | `	int nClass;` |
|        - | 12100 | `	sxi32 rc;` |
|        3 | 12101 | `	if( nArg < 1 ){` |
|      ! 0 | 12102 | `		return SXRET_OK;` |
|        - | 12103 | `	}` |
|        3 | 12104 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12105 | `	if( nClass < 1 ){` |
|      ! 0 | 12106 | `		return SXRET_OK;` |
|        - | 12107 | `	}` |
|        - | 12108 | `	/* Default extensions */` |
|        3 | 12109 | `	zExt = ".php,.inc";` |
|        3 | 12110 | `	if( nArg >= 2 ){` |
|        - | 12111 | `		int nExt;` |
|      ! 0 | 12112 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12113 | `		if( nExt < 1 ){` |
|      ! 0 | 12114 | `			zExt = ".php,.inc";` |
|      ! 0 | 12115 | `		}` |
|      ! 0 | 12116 | `	}` |
|        3 | 12117 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12118 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12119 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12120 | `	zCur = zExt;` |
|        7 | 12121 | `	while( zCur < zEnd ){` |
|        - | 12122 | `		const char *zComma;` |
|        - | 12123 | `		SyString sFile;` |
|        - | 12124 | `		int i;` |
|        - | 12125 | `		/* Find next comma or end */` |
|        5 | 12126 | `		zComma = zCur;` |
|       21 | 12127 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12128 | `			zComma++;` |
|        1 | 12129 | `		}` |
|        - | 12130 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12131 | `		SyBlobReset(&sPath);` |
|       69 | 12132 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12133 | `			char c = zClass[i];` |
|       65 | 12134 | `			if( c == '\\' ){` |
|      ! 0 | 12135 | `				c = '/';` |
|       65 | 12136 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12137 | `				c = c + ('a' - 'A');` |
|        6 | 12138 | `			}` |
|       65 | 12139 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12140 | `		}` |
|        - | 12141 | `		/* Append extension */` |
|        5 | 12142 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12143 | `		/* Try to include the file */` |
|        5 | 12144 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12145 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12146 | `		if( rc == SXRET_OK ){` |
|        - | 12147 | `			/* File included successfully */` |
|      ! 0 | 12148 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12149 | `			return SXRET_OK;` |
|        - | 12150 | `		}` |
|        - | 12151 | `		/* Move past the comma */` |
|        5 | 12152 | `		zCur = zComma;` |
|        5 | 12153 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12154 | `			zCur++;` |
|        1 | 12155 | `		}` |
|        1 | 12156 | `	}` |
|        3 | 12157 | `	SyBlobRelease(&sPath);` |
|        3 | 12158 | `	return SXRET_OK;` |
|        2 | 12159 |  |
|        - | 12160 | `/* Table of built-in VM functions. */` |
|        - | 12161 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12162 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12163 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12164 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12165 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12166 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12167 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12168 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12169 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12170 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12171 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12172 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12173 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12174 | `	    /* Constants management */` |
|        - | 12175 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12176 | `	{ "define",   vm_builtin_define               },` |
|        - | 12177 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12178 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12179 | `	   /* Class/Object functions */` |
|        - | 12180 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12181 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12182 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12183 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12184 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12185 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12186 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12187 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12188 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12189 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12190 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12191 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12192 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12193 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12194 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12195 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12196 | `	   /* SPL Autoloading */` |
|        - | 12197 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12198 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12199 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12200 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12201 | `	   /* Random numbers/strings generators */` |
|        - | 12202 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12203 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12204 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12205 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12206 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12207 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12208 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12209 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12210 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12211 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12212 | `	   /* Language constructs functions */` |
|        - | 12213 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12214 | `	{ "print", vm_builtin_print                   },` |
|        - | 12215 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12216 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12217 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12218 | `	  /* Variable handling functions */` |
|        - | 12219 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12220 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12221 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12222 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12223 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12224 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12225 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12226 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12227 | `	  /* Ouput control functions */` |
|        - | 12228 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12229 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12230 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12231 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12232 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12233 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12234 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12235 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12236 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12237 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12238 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12239 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12240 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12241 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12242 | `	  /* Assertion functions */` |
|        - | 12243 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12244 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12245 | `	  /* Error reporting functions */` |
|        - | 12246 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12247 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12248 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12249 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12250 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12251 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12252 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12253 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12254 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12255 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12256 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12257 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12258 | `	  /* Release info */` |
|        - | 12259 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12260 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12261 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12262 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12263 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12264 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12265 | `	  /* hashmap */` |
|        - | 12266 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12267 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12268 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12269 | `	  /* URL related function */` |
|        - | 12270 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12271 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12272 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12273 | `	   /* XML processing functions */` |
|        - | 12274 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12275 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12276 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12277 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12278 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12279 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12280 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12281 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12282 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12283 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12284 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12285 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12286 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12287 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12288 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12289 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12290 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12291 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12292 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12293 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12294 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12295 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12296 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12297 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12298 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12299 | `	   /* Command line processing */` |
|        - | 12300 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12301 | `	   /* JSON encoding/decoding */` |
|        - | 12302 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12303 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12304 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12305 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12306 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12307 | `	   /* Files/URI inclusion facility */` |
|        - | 12308 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12309 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12310 | `	{ "include",      vm_builtin_include          },` |
|        - | 12311 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12312 | `	{ "require",      vm_builtin_require          },` |
|        - | 12313 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12314 | `};` |
|        - | 12315 | `/*` |
|        - | 12316 | ` * Register the built-in VM functions defined above.` |
|        - | 12317 | ` */` |
|     2316 | 12318 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12319 |  |
|        - | 12320 | `	sxi32 rc;` |
|        - | 12321 | `	sxu32 n;` |
|   298766 | 12322 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12323 | `		/* Note that these special functions have access` |
|        - | 12324 | `		 * to the underlying virtual machine as their` |
|        - | 12325 | `		 * private data.` |
|        - | 12326 | `		 */` |
|   296450 | 12327 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   296450 | 12328 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12329 | `			return rc;` |
|        - | 12330 | `		}` |
|   148226 | 12331 | `	}` |
|     2318 | 12332 | `	return SXRET_OK;` |
|     1160 | 12333 |  |
|        - | 12334 | `/*` |
|        - | 12335 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12336 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12337 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12338 | ` */` |
|    27342 | 12339 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12340 |  |
|    27344 | 12341 | `	if( !iLoadable ){` |
|    26140 | 12342 | `		return pClass;` |
|        - | 12343 | `	}` |
|     1206 | 12344 | `	while(pClass){` |
|     1206 | 12345 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1206 | 12346 | `			return pClass;` |
|        - | 12347 | `		}` |
|      ! 0 | 12348 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12349 | `	}` |
|      ! 0 | 12350 | `	return 0;` |
|    13673 | 12351 |  |
|        - | 12352 | `/*` |
|        - | 12353 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12354 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12355 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12356 | ` * registered in the VM's class table.` |
|        - | 12357 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12358 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12359 | ` */` |
|       30 | 12360 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12361 |  |
|        - | 12362 | `	VmAutoloadCB *pEntry;` |
|        - | 12363 | `	ph7_value sArg,sResult;` |
|        - | 12364 | `	SyHashEntry *pHashEntry;` |
|        - | 12365 | `	ph7_class *pClass;` |
|        - | 12366 | `	sxu32 n,nEntry;` |
|       32 | 12367 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       32 | 12368 | `	if( nEntry < 1 ){` |
|       18 | 12369 | `		return 0;` |
|        - | 12370 | `	}` |
|        - | 12371 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12372 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12373 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12374 | `	}` |
|        - | 12375 | `	/* Mark this class as being autoloaded */` |
|       14 | 12376 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12377 | `	/* Prepare the class name argument */` |
|       14 | 12378 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12379 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12380 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12381 | `	pClass = 0;` |
|       28 | 12382 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12383 | `		ph7_value *apArg[1];` |
|       24 | 12384 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12385 | `		if( pEntry == 0 ){` |
|      ! 0 | 12386 | `			continue;` |
|        - | 12387 | `		}` |
|       24 | 12388 | `		apArg[0] = &sArg;` |
|       24 | 12389 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12390 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12391 | `			continue;` |
|        - | 12392 | `		}` |
|        - | 12393 | `		/* Check if the class is now available */` |
|       24 | 12394 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12395 | `		if( pHashEntry ){` |
|       10 | 12396 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12397 | `			if( pClass ){` |
|       10 | 12398 | `				break;` |
|        - | 12399 | `			}` |
|      ! 0 | 12400 | `		}` |
|        9 | 12401 | `	}` |
|       14 | 12402 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12403 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12404 | `	/* Remove reentrancy guard */` |
|       14 | 12405 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12406 | `	return pClass;` |
|       17 | 12407 |  |
|        - | 12408 | `/*` |
|        - | 12409 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12410 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12411 | ` */` |
|       18 | 12412 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12413 |  |
|       20 | 12414 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12415 |  |
|        - | 12416 | `/*` |
|        - | 12417 | ` * Check if the given name refer to an installed class.` |
|        - | 12418 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12419 | ` */` |
|    27346 | 12420 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12421 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12422 | `	const char *zName,  /* Name of the target class */` |
|        - | 12423 | `	sxu32 nByte,        /* zName length */` |
|        - | 12424 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12425 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12426 | `						 */` |
|        - | 12427 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12428 | `	)` |
|        2 | 12429 |  |
|        - | 12430 | `	SyHashEntry *pEntry;` |
|        - | 12431 | `	ph7_class *pClass;` |
|    13673 | 12432 | `	SXUNUSED(iNest);` |
|        - | 12433 | `	/* Exact class lookup.` |
|        - | 12434 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12435 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    27348 | 12436 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    27348 | 12437 | `	if( pEntry == 0 ){` |
|        - | 12438 | `		/* Class not found in hash table — try autoload before giving up */` |
|       14 | 12439 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12440 | `	}` |
|    27336 | 12441 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    27336 | 12442 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    13675 | 12443 |  |
|        - | 12444 | `/*` |
|        - | 12445 | ` * Reference Table Implementation` |
|        - | 12446 | ` * Status: stable <chm@symisc.net>` |
|        - | 12447 | ` * Intro` |
|        - | 12448 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12449 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12450 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12451 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12452 | ` *  Refer to the official for more information on this powerful` |
|        - | 12453 | ` *  extension.` |
|        - | 12454 | ` */` |
|        - | 12455 | `/*` |
|        - | 12456 | ` * Allocate a new reference entry.` |
|        - | 12457 | ` */` |
|  3054658 | 12458 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12459 |  |
|        - | 12460 | `	VmRefObj *pRef;` |
|        - | 12461 | `	/* Allocate a new instance */` |
|  3054660 | 12462 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3054660 | 12463 | `	if( pRef == 0 ){` |
|      ! 0 | 12464 | `		return 0;` |
|        - | 12465 | `	}` |
|        - | 12466 | `	/* Zero the structure */` |
|  3054660 | 12467 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12468 | `	/* Initialize fields */` |
|  3054660 | 12469 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3054660 | 12470 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3054660 | 12471 | `	pRef->nIdx = nIdx;` |
|  3054660 | 12472 | `	return pRef;` |
|  1527331 | 12473 |  |
|        - | 12474 | `/*` |
|        - | 12475 | ` * Default hash function used by the reference table` |
|        - | 12476 | ` * for lookup/insertion operations.` |
|        - | 12477 | ` */` |
| 16873223 | 12478 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12479 |  |
|        - | 12480 | `	/* Calculate the hash based on the memory object index */` |
| 16873225 | 12481 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12482 |  |
|        - | 12483 | `/*` |
|        - | 12484 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12485 | ` * in the reference table.` |
|        - | 12486 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12487 | ` * otherwise.` |
|        - | 12488 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12489 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12490 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12491 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12492 | ` * Refer to the official for more information on this powerful` |
|        - | 12493 | ` * extension.` |
|        - | 12494 | ` */` |
|  9117456 | 12495 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12496 |  |
|        - | 12497 | `	VmRefObj *pRef;` |
|        - | 12498 | `	sxu32 nBucket;` |
|        - | 12499 | `	/* Point to the appropriate bucket */` |
|  9117458 | 12500 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12501 | `	/* Perform the lookup */` |
|  9117458 | 12502 | `	pRef = pVm->apRefObj[nBucket];` |
| 19841795 | 12503 | `	for(;;){` |
| 39669663 | 12504 | `		if( pRef == 0 ){` |
|  3133894 | 12505 | `			break;` |
|        - | 12506 | `		}` |
| 36535771 | 12507 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12508 | `			/* Entry found */` |
|  5983566 | 12509 | `			return pRef;` |
|        - | 12510 | `		}` |
|        - | 12511 | `		/* Point to the next entry */` |
| 30552207 | 12512 | `		pRef = pRef->pNextCollide;` |
|        2 | 12513 | `	}` |
|        - | 12514 | `	/* No such entry,return NULL */` |
|  3133894 | 12515 | `	return 0;` |
|  4558730 | 12516 |  |
|        - | 12517 | `/*` |
|        - | 12518 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12519 | ` *` |
|        - | 12520 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12521 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12522 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12523 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12524 | ` * Refer to the official for more information on this powerful` |
|        - | 12525 | ` * extension.` |
|        - | 12526 | ` */` |
|  3054658 | 12527 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12528 |  |
|        - | 12529 | `	sxu32 nBucket;` |
|  3054660 | 12530 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12531 | `		VmRefObj **apNew;` |
|        - | 12532 | `		sxu32 nNew;` |
|        - | 12533 | `		/* Allocate a larger table */` |
|     3958 | 12534 | `		nNew = pVm->nRefSize << 1;` |
|     3958 | 12535 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3958 | 12536 | `		if( apNew ){` |
|     3958 | 12537 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12538 | `			sxu32 n;` |
|        - | 12539 | `			/* Zero the structure */` |
|     3958 | 12540 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12541 | `			/* Rehash all referenced entries */` |
|  2840384 | 12542 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12543 | `				/* Remove old collision links */` |
|  2836428 | 12544 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12545 | `				/* Point to the appropriate bucket */` |
|  2836428 | 12546 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12547 | `				/* Insert the entry  */` |
|  2836428 | 12548 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2836428 | 12549 | `				if( apNew[nBucket] ){` |
|  2298896 | 12550 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12551 | `				}` |
|  2836428 | 12552 | `				apNew[nBucket] = pEntry;` |
|        - | 12553 | `				/* Point to the next entry */` |
|  2836428 | 12554 | `				pEntry = pEntry->pNext;` |
|  1418215 | 12555 | `			}` |
|        - | 12556 | `			/* Release the old table */` |
|     3958 | 12557 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12558 | `			/* Install the new one */` |
|     3958 | 12559 | `			pVm->apRefObj = apNew;` |
|     3958 | 12560 | `			pVm->nRefSize = nNew;` |
|     1978 | 12561 | `		}` |
|     1978 | 12562 | `	}` |
|        - | 12563 | `	/* Point to the appropriate bucket */` |
|  3054660 | 12564 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12565 | `	/* Insert the entry */` |
|  3054660 | 12566 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3054660 | 12567 | `	if( pVm->apRefObj[nBucket] ){` |
|  2525298 | 12568 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1262590 | 12569 | `	}` |
|  3054660 | 12570 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3054660 | 12571 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3054660 | 12572 | `	pVm->nRefUsed++;` |
|  3054660 | 12573 | `	return SXRET_OK;` |
|        2 | 12574 |  |
|        - | 12575 | `/*` |
|        - | 12576 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12577 | ` * the reference table.` |
|        - | 12578 | ` * This function is invoked when the user perform an unset` |
|        - | 12579 | ` * call [i.e: unset($var); ].` |
|        - | 12580 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12581 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12582 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12583 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12584 | ` * Refer to the official for more information on this powerful` |
|        - | 12585 | ` * extension.` |
|        - | 12586 | ` */` |
|  3021308 | 12587 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12588 |  |
|        - | 12589 | `	ph7_hashmap_node **apNode;` |
|        - | 12590 | `	SyHashEntry **apEntry;` |
|        - | 12591 | `	sxu32 n;` |
|        - | 12592 | `	/* Point to the reference table */` |
|  3021310 | 12593 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3021310 | 12594 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12595 | `	/* Unlink the entry from the reference table */` |
|  3106496 | 12596 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85188 | 12597 | `		if( apEntry[n] ){` |
|    85138 | 12598 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42568 | 12599 | `		}` |
|    42595 | 12600 | `	}` |
|  5960178 | 12601 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2938870 | 12602 | `		if( apNode[n] ){` |
|     6880 | 12603 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3439 | 12604 | `		}` |
|  1469436 | 12605 | `	}` |
|  3021310 | 12606 | `	if( pRef->pPrevCollide ){` |
|  1156627 | 12607 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   578281 | 12608 | `	}else{` |
|  1864685 | 12609 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12610 | `	}` |
|  3021310 | 12611 | `	if( pRef->pNextCollide ){` |
|  1714445 | 12612 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   857115 | 12613 | `	}` |
|  3021310 | 12614 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12615 | `	/* Release the node */` |
|  3021310 | 12616 | `	SySetRelease(&pRef->aReference);` |
|  3021310 | 12617 | `	SySetRelease(&pRef->aArrEntries);` |
|  3021310 | 12618 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3021310 | 12619 | `	pVm->nRefUsed--;` |
|  3021310 | 12620 | `	return SXRET_OK;` |
|        2 | 12621 |  |
|        - | 12622 | `/*` |
|        - | 12623 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12624 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12625 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12626 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12627 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12628 | ` * Refer to the official for more information on this powerful` |
|        - | 12629 | ` * extension.` |
|        - | 12630 | ` */` |
|  3084862 | 12631 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12632 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12633 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12634 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12635 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12636 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12637 | `	)` |
|        2 | 12638 |  |
|  3084864 | 12639 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12640 | `	VmRefObj *pRef;` |
|        - | 12641 | `	/* Check if the referenced object already exists */` |
|  3084864 | 12642 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3084864 | 12643 | `	if( pRef == 0 ){` |
|        - | 12644 | `		/* Create a new entry */` |
|  3054660 | 12645 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3054660 | 12646 | `		if( pRef == 0 ){` |
|      ! 0 | 12647 | `			return SXERR_MEM;` |
|        - | 12648 | `		}` |
|  3054660 | 12649 | `		pRef->iFlags = iFlags;` |
|        - | 12650 | `		/* Install the entry */` |
|  3054660 | 12651 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1527329 | 12652 | `	}` |
|  3084864 | 12653 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3084864 | 12654 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12655 | `		VmSlot sRef;` |
|        - | 12656 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12657 | `		 * be deleted when we leave this frame.` |
|        - | 12658 | `		 */` |
|    79314 | 12659 | `		sRef.nIdx = nIdx;` |
|    79314 | 12660 | `		sRef.pUserData = pEntry;` |
|    79314 | 12661 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12662 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12663 | `		}` |
|    39656 | 12664 | `	}` |
|  3084864 | 12665 | `	if( pEntry ){` |
|        - | 12666 | `		/* Address of the hash-entry */` |
|   109326 | 12667 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    54662 | 12668 | `	}` |
|  3084864 | 12669 | `	if( pMapEntry ){` |
|        - | 12670 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2970542 | 12671 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1485270 | 12672 | `	}` |
|  3084864 | 12673 | `	return SXRET_OK;` |
|  1542433 | 12674 |  |
|        - | 12675 | `/*` |
|        - | 12676 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12677 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12678 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12679 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12680 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12681 | ` * Refer to the official for more information on this powerful` |
|        - | 12682 | ` * extension.` |
|        - | 12683 | ` */` |
|  3011280 | 12684 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12685 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12686 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12687 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12688 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12689 | `	)` |
|        2 | 12690 |  |
|        - | 12691 | `	VmRefObj *pRef;` |
|        - | 12692 | `	sxu32 n;` |
|        - | 12693 | `	/* Check if the referenced object already exists */` |
|  3011282 | 12694 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3011282 | 12695 | `	if( pRef == 0 ){` |
|        - | 12696 | `		/* Not such entry */` |
|    79230 | 12697 | `		return SXERR_NOTFOUND;` |
|        - | 12698 | `	}` |
|        - | 12699 | `	/* Remove the desired entry */` |
|  2932054 | 12700 | `	if( pEntry ){` |
|        - | 12701 | `		SyHashEntry **apEntry;` |
|       56 | 12702 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12703 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12704 | `			if( apEntry[n] == pEntry ){` |
|        - | 12705 | `				/* Nullify the entry */` |
|       56 | 12706 | `				apEntry[n] = 0;` |
|        - | 12707 | `				/*` |
|        - | 12708 | `				 * NOTE:` |
|        - | 12709 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12710 | `				 * we avoid wasting spaces.` |
|        - | 12711 | `				 */` |
|       27 | 12712 | `			}` |
|       79 | 12713 | `		}` |
|       27 | 12714 | `	}` |
|  2932054 | 12715 | `	if( pMapEntry ){` |
|        - | 12716 | `		ph7_hashmap_node **apNode;` |
|  2932000 | 12717 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5864092 | 12718 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2932094 | 12719 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12720 | `				/* nullify the entry */` |
|  2932000 | 12721 | `				apNode[n] = 0;` |
|  1465999 | 12722 | `			}` |
|  1466048 | 12723 | `		}` |
|  1465999 | 12724 | `	}` |
|  2932054 | 12725 | `	return SXRET_OK;` |
|  1505642 | 12726 |  |
|        - | 12727 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12728 | `/*` |
|        - | 12729 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12730 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12731 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12732 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12733 | ` * For more information on how to register IO stream devices,please` |
|        - | 12734 | ` * refer to the official documentation.` |
|        - | 12735 | ` */` |
|    24336 | 12736 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12737 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12738 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12739 | `	int nByte              /* *pzDevice length*/` |
|        - | 12740 | `	)` |
|        2 | 12741 |  |
|        - | 12742 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12743 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12744 | `	SyString sDev,sCur;` |
|        - | 12745 | `	sxu32 n,nEntry;` |
|        - | 12746 | `	int rc;` |
|        - | 12747 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24338 | 12748 | `	zNext = zCur = zIn = *pzDevice;` |
|    24338 | 12749 | `	zEnd = &zIn[nByte];` |
|  1551464 | 12750 | `	while( zIn < zEnd ){` |
|  1527130 | 12751 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12752 | `			/* Got one */` |
|        3 | 12753 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12754 | `			break;` |
|        - | 12755 | `		}` |
|        - | 12756 | `		/* Advance the cursor */` |
|  1527128 | 12757 | `		zIn++;` |
|        2 | 12758 | `	}` |
|    24338 | 12759 | `	if( zIn >= zEnd ){` |
|        - | 12760 | `		/* No such scheme,return the default stream */` |
|    24336 | 12761 | `		return pVm->pDefStream;` |
|        - | 12762 | `	}` |
|        3 | 12763 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12764 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12765 | `	SyStringFullTrim(&sDev);` |
|        - | 12766 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12767 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12768 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12769 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12770 | `		pStream = apStream[n];` |
|        3 | 12771 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12772 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12773 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12774 | `		if( rc == 0 ){` |
|        - | 12775 | `			/* Stream device found */` |
|        3 | 12776 | `			*pzDevice = zNext;` |
|        3 | 12777 | `			return pStream;` |
|        - | 12778 | `		}` |
|      ! 0 | 12779 | `	}` |
|        - | 12780 | `	/* No such stream,return NULL */` |
|      ! 0 | 12781 | `	return 0;` |
|    12170 | 12782 |  |
|        - | 12783 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12784 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12785 |  |
