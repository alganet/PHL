# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5002/6546 lines (76.41%)

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
|   786386 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   786388 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       31 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   786358 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   786350 |   104 | `	return FALSE;` |
|   393217 |   105 |  |
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
|  3194468 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3194470 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3194470 |   352 | `	sInstr.iP1 = iP1;` |
|  3194470 |   353 | `	sInstr.iP2 = iP2;` |
|  3194470 |   354 | `	sInstr.p3  = p3;` |
|  3194470 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   184330 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    92164 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3194470 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3194470 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3194470 |   365 | `	return rc;` |
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
|   957524 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   957526 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   172708 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   172710 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   618692 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   618694 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
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
|    15996 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    15998 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    15998 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    15998 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    15998 |   447 | `	pFrame->pUserData = pUserData;` |
|    15998 |   448 | `	pFrame->pThis = pThis;` |
|    15998 |   449 | `	pFrame->pVm = pVm;` |
|    15998 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    15998 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    15998 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    15998 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    15998 |   454 | `	return pFrame;` |
|     8000 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    15954 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    15956 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15956 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    15956 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    15956 |   476 | `	pVm->pFrame = pFrame;` |
|    15956 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13380 |   479 | `		*ppFrame = pFrame;` |
|     6689 |   480 | `	}` |
|    15956 |   481 | `	return SXRET_OK;` |
|     7979 |   482 |  |
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
|    13378 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13380 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13380 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13380 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13380 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13316 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    92518 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    79204 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39603 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13316 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    92574 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    79260 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39631 |   545 | `			}` |
|     6657 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13380 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13380 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13380 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13380 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13380 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6689 |   554 | `	}` |
|    13380 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6367990 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6368268 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6367992 |   566 | `	return pFrame;` |
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
|   111216 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   111218 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   437414 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
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
|   111218 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    52156 |   724 | `		return SXRET_OK;` |
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
|    55610 |   750 |  |
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
|   366278 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   366280 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   358552 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   179275 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   366280 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   366280 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   366280 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   366280 |   840 | `	return pObj;` |
|   183141 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2141988 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2141990 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2141990 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070994 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2141990 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2141990 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2141990 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2141990 |   863 | `	return pObj;` |
|  1070996 |   864 |  |
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
|    13994 |  1442 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1443 |  |
|    13996 |  1444 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13996 |  1445 | `	if( xCons != VmObConsumer ){` |
|     6238 |  1446 | `		pVm->nOutputLen += nLen;` |
|     6238 |  1447 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      820 |  1448 | `			pVm->bHeadersSent = 1;` |
|      409 |  1449 | `		}` |
|     3118 |  1450 | `	}` |
|    13996 |  1451 |  |
|        - |  1452 | `#define VM_STACK_GUARD 16` |
|        - |  1453 | `/*` |
|        - |  1454 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1455 | ` * our compiled PHP program.` |
|        - |  1456 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1457 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1458 | ` */` |
|    32814 |  1459 | `static ph7_value * VmNewOperandStack(` |
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
|    32816 |  1472 | `	nInstr += VM_STACK_GUARD;` |
|    32816 |  1473 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32816 |  1474 | `	if( pStack == 0 ){` |
|      ! 0 |  1475 | `		return 0;` |
|        - |  1476 | `	}` |
|        - |  1477 | `	/* Initialize the operand stack */` |
|  2053610 |  1478 | `	while( nInstr > 0 ){` |
|  2020796 |  1479 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2020796 |  1480 | `		--nInstr;` |
|        2 |  1481 | `	}` |
|        - |  1482 | `	/* Ready for bytecode execution */` |
|    32816 |  1483 | `	return pStack;` |
|    16409 |  1484 |  |
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
|   577818 |  1609 | `static sxi32 VmInitCallContext(` |
|        - |  1610 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1611 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1612 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1613 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1614 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1615 | `	)` |
|        2 |  1616 |  |
|   577820 |  1617 | `	pOut->pFunc = pFunc;` |
|   577820 |  1618 | `	pOut->pVm   = pVm;` |
|   577820 |  1619 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   577820 |  1620 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1621 | `	/* Assume a null return value */` |
|   577820 |  1622 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   577820 |  1623 | `	pOut->pRet = pRet;` |
|   577820 |  1624 | `	pOut->iFlags = iFlags;` |
|   577820 |  1625 | `	return SXRET_OK;` |
|        2 |  1626 |  |
|        - |  1627 | `/*` |
|        - |  1628 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1629 | ` * left behind.` |
|        - |  1630 | ` */` |
|   577818 |  1631 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1632 |  |
|        - |  1633 | `	sxu32 n;` |
|   577820 |  1634 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7034 |  1635 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20068 |  1636 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13036 |  1637 | `			if( apObj[n] == 0 ){` |
|        - |  1638 | `				/* Already released */` |
|      298 |  1639 | `				continue;` |
|        - |  1640 | `			}` |
|    12740 |  1641 | `			PH7_MemObjRelease(apObj[n]);` |
|    12740 |  1642 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6371 |  1643 | `		}` |
|     7034 |  1644 | `		SySetRelease(&pCtx->sVar);` |
|     3516 |  1645 | `	}` |
|   577820 |  1646 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   577820 |  1662 |  |
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
|  3345142 |  1693 | `static void VmPopOperand(` |
|        - |  1694 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1695 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1696 | `	)` |
|        2 |  1697 |  |
|  3345144 |  1698 | `	ph7_value *pTos = *ppTos;` |
|  7111360 |  1699 | `	while( nPop > 0 ){` |
|  3766218 |  1700 | `		PH7_MemObjRelease(pTos);` |
|  3766218 |  1701 | `		pTos--;` |
|  3766218 |  1702 | `		nPop--;` |
|        2 |  1703 | `	}` |
|        - |  1704 | `	/* Top of the stack */` |
|  3345144 |  1705 | `	*ppTos = pTos;` |
|  3345144 |  1706 |  |
|        - |  1707 | `/*` |
|        - |  1708 | ` * Reserve a memory object.` |
|        - |  1709 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1710 | ` */` |
|  3056610 |  1711 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1712 |  |
|  3056612 |  1713 | `	ph7_value *pObj = 0;` |
|        - |  1714 | `	VmSlot *pSlot;` |
|        - |  1715 | `	sxu32 nIdx;` |
|        - |  1716 | `	/* Check for a free slot */` |
|  3056612 |  1717 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3056612 |  1718 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3056612 |  1719 | `	if( pSlot ){` |
|   914624 |  1720 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   914624 |  1721 | `		nIdx = pSlot->nIdx;` |
|   457311 |  1722 | `	}` |
|  3056612 |  1723 | `	if( pObj == 0 ){` |
|        - |  1724 | `		/* Reserve a new memory object */` |
|  2141990 |  1725 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2141990 |  1726 | `		if( pObj == 0 ){` |
|      ! 0 |  1727 | `			return 0;` |
|        - |  1728 | `		}` |
|  1070994 |  1729 | `	}` |
|        - |  1730 | `	/* Set a null default value */` |
|  3056612 |  1731 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3056612 |  1732 | `	pObj->nIdx = nIdx;` |
|  3056612 |  1733 | `	return pObj;` |
|  1528307 |  1734 |  |
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
|  3116270 |  1759 | `static ph7_value * VmExtractMemObj(` |
|        - |  1760 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1761 | `	const SyString *pName, /* Variable name */` |
|        - |  1762 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1763 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1764 | `	)` |
|        2 |  1765 |  |
|  3116272 |  1766 | `	int bNullify = FALSE;` |
|        - |  1767 | `	SyHashEntry *pEntry;` |
|        - |  1768 | `	VmFrame *pFrame;` |
|        - |  1769 | `	ph7_value *pObj;` |
|        - |  1770 | `	sxu32 nIdx;` |
|        - |  1771 | `	sxi32 rc;` |
|        - |  1772 | `	/* Point to the top active frame */` |
|  3116272 |  1773 | `	pFrame = pVm->pFrame;` |
|  3116272 |  1774 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1775 | `	/* Perform the lookup */` |
|  3116272 |  1776 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1777 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1778 | `		pName = &sAnnon;` |
|        - |  1779 | `		/* Always nullify the object */` |
|      ! 0 |  1780 | `		bNullify = TRUE;` |
|      ! 0 |  1781 | `		bDup = FALSE;` |
|      ! 0 |  1782 | `	}` |
|        - |  1783 | `	/* Check the superglobals table first */` |
|  3116272 |  1784 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3116272 |  1785 | `	if( pEntry == 0 ){` |
|        - |  1786 | `		/* Query the top active frame */` |
|  3116232 |  1787 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3116232 |  1788 | `		if( pEntry == 0 ){` |
|    86102 |  1789 | `			char *zName = (char *)pName->zString;` |
|        - |  1790 | `			VmSlot sLocal;` |
|    86102 |  1791 | `			if( !bCreate ){` |
|        - |  1792 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1793 | `				return 0;` |
|        - |  1794 | `			}` |
|        - |  1795 | `			/* No such variable,automatically create a new one and install` |
|        - |  1796 | `			 * it in the current frame.` |
|        - |  1797 | `			 */` |
|    86066 |  1798 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86066 |  1799 | `			if( pObj == 0 ){` |
|      ! 0 |  1800 | `				return 0;` |
|        - |  1801 | `			}` |
|    86066 |  1802 | `			nIdx = pObj->nIdx;` |
|    86066 |  1803 | `			if( bDup ){` |
|        - |  1804 | `				/* Duplicate name */` |
|      168 |  1805 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1806 | `				if( zName == 0 ){` |
|      ! 0 |  1807 | `					return 0;` |
|        - |  1808 | `				}` |
|       83 |  1809 | `			}` |
|        - |  1810 | `			/* Link to the top active VM frame */` |
|    86066 |  1811 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86066 |  1812 | `			if( rc != SXRET_OK ){` |
|        - |  1813 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1814 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1815 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1816 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1817 | `				return 0;` |
|        - |  1818 | `			}` |
|    86066 |  1819 | `			if( pFrame->pParent != 0 ){` |
|        - |  1820 | `				/* Local variable */` |
|    79240 |  1821 | `				sLocal.nIdx = nIdx;` |
|    79240 |  1822 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39621 |  1823 | `			}else{` |
|        - |  1824 | `				/* Register in the $GLOBALS array */` |
|     6828 |  1825 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1826 | `			}` |
|        - |  1827 | `			/* Install in the reference table */` |
|    86066 |  1828 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1829 | `			/* Save object index */` |
|    86066 |  1830 | `			pObj->nIdx = nIdx;` |
|    43034 |  1831 | `		}else{` |
|        - |  1832 | `			/* Extract variable contents */` |
|  3030132 |  1833 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3030132 |  1834 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3030132 |  1835 | `			if( bNullify && pObj ){` |
|      ! 0 |  1836 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1837 | `			}` |
|        - |  1838 | `		}` |
|  1558209 |  1839 | `	}else{` |
|        - |  1840 | `		/* Superglobal */` |
|       42 |  1841 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1842 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1843 | `	}` |
|  3116236 |  1844 | `	return pObj;` |
|  1558247 |  1845 |  |
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
|    32900 |  2726 | `static sxi32 VmByteCodeExec(` |
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
|    32902 |  2744 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32902 |  2745 | `	if( nTos < 0 ){` |
|    30852 |  2746 | `		pTos = &pStack[-1];` |
|    15427 |  2747 | `	}else{` |
|     2052 |  2748 | `		pTos = &pStack[nTos];` |
|        - |  2749 | `	}` |
|    32902 |  2750 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32902 |  2751 | `	pc = nPc;` |
|        - |  2752 | `	/* Execute as much as we can */` |
|  5005077 |  2753 | `	for(;;){` |
|        - |  2754 | `		/* Fetch the instruction to execute */` |
| 10009452 |  2755 | `		pInstr = &aInstr[pc];` |
| 10009452 |  2756 | `		rc = SXRET_OK;` |
|        - |  2757 | `/*` |
|        - |  2758 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2759 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2760 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2761 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2762 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2763 | ` */` |
| 10009452 |  2764 | `		switch(pInstr->iOp){` |
|        - |  2765 | `/*` |
|        - |  2766 | ` * DONE: P1 * *` |
|        - |  2767 | ` *` |
|        - |  2768 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2769 | ` * and return immediately.` |
|        - |  2770 | ` */` |
|    16139 |  2771 | `case PH7_OP_DONE:` |
|    32280 |  2772 | `	if( pInstr->iP1 ){` |
|        - |  2773 | `#ifdef UNTRUST` |
|        - |  2774 | `		if( pTos < pStack ){` |
|        - |  2775 | `			goto Abort;` |
|        - |  2776 | `		}` |
|        - |  2777 | `#endif` |
|    18728 |  2778 | `		if( pLastRef ){` |
|    12206 |  2779 | `			*pLastRef = pTos->nIdx;` |
|     6102 |  2780 | `		}` |
|    18728 |  2781 | `		if( pResult ){` |
|        - |  2782 | `			/* Execution result */` |
|    17784 |  2783 | `			PH7_MemObjStore(pTos,pResult);` |
|     8891 |  2784 | `		}` |
|    18728 |  2785 | `		VmPopOperand(&pTos,1);` |
|    22917 |  2786 | `	}else if( pLastRef ){` |
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
|    32282 |  2798 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    32280 |  2813 | `	goto Done;` |
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
|   215863 |  2858 | `case PH7_OP_JMP:` |
|   431772 |  2859 | `	pc = pInstr->iP2 - 1;` |
|   431772 |  2860 | `	break;` |
|        - |  2861 | `/*` |
|        - |  2862 | ` * JZ: P1 P2 *` |
|        - |  2863 | ` *` |
|        - |  2864 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2865 | ` * entry in the stack if P1 is zero.` |
|        - |  2866 | ` */` |
|   504639 |  2867 | `case PH7_OP_JZ:` |
|        - |  2868 | `#ifdef UNTRUST` |
|        - |  2869 | `	if( pTos < pStack ){` |
|        - |  2870 | `		goto Abort;` |
|        - |  2871 | `	}` |
|        - |  2872 | `#endif` |
|        - |  2873 | `	/* Get a boolean value */` |
|  1009368 |  2874 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2875 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2876 | `	}` |
|  1009368 |  2877 | `	if( !pTos->x.iVal ){` |
|        - |  2878 | `		/* Take the jump */` |
|   510368 |  2879 | `		pc = pInstr->iP2 - 1;` |
|   255183 |  2880 | `	}` |
|  1009368 |  2881 | `	if( !pInstr->iP1 ){` |
|   802604 |  2882 | `		VmPopOperand(&pTos,1);` |
|   401323 |  2883 | `	}` |
|  1009368 |  2884 | `	break;` |
|        - |  2885 | `/*` |
|        - |  2886 | ` * JNZ: P1 P2 *` |
|        - |  2887 | ` *` |
|        - |  2888 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2889 | ` * entry in the stack if P1 is zero.` |
|        - |  2890 | ` */` |
|    53516 |  2891 | `case PH7_OP_JNZ:` |
|        - |  2892 | `#ifdef UNTRUST` |
|        - |  2893 | `	if( pTos < pStack ){` |
|        - |  2894 | `		goto Abort;` |
|        - |  2895 | `	}` |
|        - |  2896 | `#endif` |
|        - |  2897 | `	/* Get a boolean value */` |
|   107034 |  2898 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2899 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2900 | `	}` |
|   107034 |  2901 | `	if( pTos->x.iVal ){` |
|        - |  2902 | `		/* Take the jump */` |
|     4538 |  2903 | `		pc = pInstr->iP2 - 1;` |
|     2268 |  2904 | `	}` |
|   107034 |  2905 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2906 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2907 | `	}` |
|   107034 |  2908 | `	break;` |
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
|   393701 |  2922 | `case PH7_OP_POP: {` |
|   787448 |  2923 | `	sxi32 n = pInstr->iP1;` |
|   787448 |  2924 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2925 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2926 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2927 | `	}` |
|   787448 |  2928 | `	VmPopOperand(&pTos,n);` |
|   787448 |  2929 | `	break;` |
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
|     6493 |  2952 | `case PH7_OP_NSSWITCH:` |
|    12988 |  2953 | `	SyBlobReset(&pVm->sNamespace);` |
|    12988 |  2954 | `	if( pInstr->p3 ){` |
|       62 |  2955 | `		const char *zNs = (const char *)pInstr->p3;` |
|       62 |  2956 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       30 |  2957 | `	}` |
|    12988 |  2958 | `	break;` |
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
|    13036 |  3090 | `case PH7_OP_ERR_CTRL:` |
|        - |  3091 | `	/*` |
|        - |  3092 | `	 * TICKET 1433-038:` |
|        - |  3093 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3094 | `	 * use the public API,to control error output.` |
|        - |  3095 | `	 */` |
|    26072 |  3096 | `	break;` |
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
|   841963 |  3156 | `case PH7_OP_LOADC: {` |
|        - |  3157 | `	ph7_value *pObj;` |
|        - |  3158 | `	/* Reserve a room */` |
|  1683972 |  3159 | `	pTos++;` |
|  2517767 |  3160 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1683972 |  3161 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3162 | `			SyHashEntry *pEntry;` |
|        - |  3163 | `			/* Candidate for expansion via user defined callbacks */` |
|    16428 |  3164 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16428 |  3165 | `			if( pEntry ){` |
|    16424 |  3166 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3167 | `				/* Set a NULL default value */` |
|    16424 |  3168 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16424 |  3169 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3170 | `				/* Invoke the callback and deal with the expanded value */` |
|    16424 |  3171 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3172 | `				/* Mark as constant */` |
|    16424 |  3173 | `				pTos->nIdx = SXU32_HIGH;` |
|    16424 |  3174 | `				break;` |
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
|  1667548 |  3206 | `		PH7_MemObjLoad(pObj,pTos);` |
|   833797 |  3207 | `	}else{` |
|        - |  3208 | `		/* Set a NULL value */` |
|      ! 0 |  3209 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3210 | `	}` |
|   833752 |  3211 | `LoadC_Done:` |
|        - |  3212 | `	/* Mark as constant */` |
|  1667550 |  3213 | `	pTos->nIdx = SXU32_HIGH;` |
|  1667550 |  3214 | `	break;` |
|        - |  3215 | `				  }` |
|        - |  3216 | `/*` |
|        - |  3217 | ` * LOAD: P1 * P3` |
|        - |  3218 | ` *` |
|        - |  3219 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3220 | ` * from the P3 operand.` |
|        - |  3221 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3222 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3223 | ` */` |
|  1353675 |  3224 | `case PH7_OP_LOAD:{` |
|        - |  3225 | `	ph7_value *pObj;` |
|        - |  3226 | `	SyString sName;` |
|  2707572 |  3227 | `	if( pInstr->p3 == 0 ){` |
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
|  2707554 |  3240 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3241 | `		/* Reserve a room for the target object */` |
|  2707554 |  3242 | `		pTos++;` |
|        - |  3243 | `	}` |
|        - |  3244 | `	/* Extract the requested memory object */` |
|  2707572 |  3245 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2707572 |  3246 | `	if( pObj == 0 ){` |
|       26 |  3247 | `		if( pInstr->iP1 ){` |
|        - |  3248 | `			/* Variable not found,load NULL */` |
|       26 |  3249 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3250 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3251 | `			}else{` |
|       26 |  3252 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3253 | `			}` |
|       26 |  3254 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1353689 |  3255 | `			break;` |
|      ! 0 |  3256 | `		}else{` |
|        - |  3257 | `			/* Fatal error */` |
|      ! 0 |  3258 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3259 | `			goto Abort;` |
|        - |  3260 | `		}` |
|        - |  3261 | `	}` |
|        - |  3262 | `	/* Load variable contents */` |
|  2707548 |  3263 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2707548 |  3264 | `	pTos->nIdx = pObj->nIdx;` |
|  2707548 |  3265 | `	break;` |
|        - |  3266 | `				   }` |
|        - |  3267 | `/*` |
|        - |  3268 | ` * LOAD_MAP P1 * *` |
|        - |  3269 | ` *` |
|        - |  3270 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3271 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3272 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3273 | ` */` |
|    18751 |  3274 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3275 | `	ph7_hashmap *pMap;` |
|        - |  3276 | `	/* Allocate a new hashmap instance */` |
|    37504 |  3277 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    37504 |  3278 | `	if( pMap == 0 ){` |
|      ! 0 |  3279 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3280 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3281 | `		goto Abort;` |
|        - |  3282 | `	}` |
|    37504 |  3283 | `	if( pInstr->iP1 > 0 ){` |
|     2264 |  3284 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3285 | `		/* Perform the insertion */` |
|     6928 |  3286 | `		while( pEntry < pTos ){` |
|     4666 |  3287 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3288 | `				/* Insertion by reference */` |
|      142 |  3289 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3290 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3291 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3292 | `					);` |
|       48 |  3293 | `			}else{` |
|        - |  3294 | `				/* Standard insertion */` |
|     6857 |  3295 | `				PH7_HashmapInsert(pMap,` |
|     4570 |  3296 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2285 |  3297 | `					&pEntry[1]` |
|        - |  3298 | `				);` |
|        - |  3299 | `			}` |
|        - |  3300 | `			/* Next pair on the stack */` |
|     4666 |  3301 | `			pEntry += 2;` |
|        2 |  3302 | `		}` |
|        - |  3303 | `		/* Pop P1 elements */` |
|     2264 |  3304 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1131 |  3305 | `	}` |
|        - |  3306 | `	/* Push the hashmap */` |
|    37504 |  3307 | `	pTos++;` |
|    37504 |  3308 | `	pTos->nIdx = SXU32_HIGH;` |
|    37504 |  3309 | `	pTos->x.pOther = pMap;` |
|    37504 |  3310 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    37504 |  3311 | `	break;` |
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
|   216895 |  3367 | `case PH7_OP_LOAD_IDX: {` |
|   433836 |  3368 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   433836 |  3369 | `	ph7_hashmap *pMap = 0;` |
|        - |  3370 | `	ph7_value *pIdx;` |
|   433836 |  3371 | `	pIdx = 0;` |
|   433836 |  3372 | `	if( pInstr->iP1 == 0 ){` |
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
|   433836 |  3389 | `		pIdx = pTos;` |
|   433836 |  3390 | `		pTos--;` |
|        - |  3391 | `	}` |
|   433836 |  3392 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
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
|    93162 |  3417 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3418 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3419 | `			ph7_value *pObj;` |
|      ! 0 |  3420 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3421 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3422 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3423 | `			}` |
|      ! 0 |  3424 | `		}` |
|      ! 0 |  3425 | `	}` |
|    93162 |  3426 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    93162 |  3427 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    93162 |  3428 | `		if( pInstr->iP2 ){` |
|        - |  3429 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3430 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3431 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3432 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3433 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3434 | `		}` |
|        - |  3435 | `		/* Point to the hashmap */` |
|    93162 |  3436 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    93162 |  3437 | `		if( pIdx ){` |
|        - |  3438 | `			/* Load the desired entry */` |
|    93162 |  3439 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    46580 |  3440 | `		}` |
|    93162 |  3441 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3442 | `			/* Create a new empty entry */` |
|      265 |  3443 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3444 | `			if( rc == SXRET_OK ){` |
|        - |  3445 | `				/* Point to the last inserted entry */` |
|      265 |  3446 | `				pNode = pMap->pLast;` |
|      132 |  3447 | `			}` |
|      132 |  3448 | `		}` |
|    46580 |  3449 | `	}` |
|    93162 |  3450 | `	if( pIdx ){` |
|    93162 |  3451 | `		PH7_MemObjRelease(pIdx);` |
|    46580 |  3452 | `	}` |
|    93162 |  3453 | `	if( rc == SXRET_OK ){` |
|        - |  3454 | `		/* Load entry contents */` |
|    42556 |  3455 | `		if( pMap->iRef < 2 ){` |
|        - |  3456 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3457 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3458 | `			 */` |
|       24 |  3459 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3460 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3461 | `		}else{` |
|    42534 |  3462 | `			pTos->nIdx = pNode->nValIdx;` |
|    42534 |  3463 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    42534 |  3464 | `			PH7_HashmapUnref(pMap);` |
|        - |  3465 | `		}` |
|    21279 |  3466 | `	}else{` |
|        - |  3467 | `		/* No such entry,load NULL */` |
|    50608 |  3468 | `		PH7_MemObjRelease(pTos);` |
|    50608 |  3469 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3470 | `	}` |
|    93162 |  3471 | `	break;` |
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
|   115397 |  3549 | `case PH7_OP_STORE: {` |
|        - |  3550 | `	ph7_value *pObj;` |
|        - |  3551 | `	SyString sName;` |
|        - |  3552 | `#ifdef UNTRUST` |
|        - |  3553 | `	if( pTos < pStack ){` |
|        - |  3554 | `		goto Abort;` |
|        - |  3555 | `	}` |
|        - |  3556 | `#endif` |
|   230796 |  3557 | `	if( pInstr->iP2 ){` |
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
|   116885 |  3574 | `		break;` |
|   227824 |  3575 | `	}else if( pInstr->p3 == 0 ){` |
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
|   227818 |  3589 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3590 | `	}` |
|        - |  3591 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   227824 |  3592 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   227824 |  3593 | `	if( pObj == 0 ){` |
|      ! 0 |  3594 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3595 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3596 | `		goto Abort;` |
|        - |  3597 | `	}` |
|   227824 |  3598 | `	if( !pInstr->p3 ){` |
|        7 |  3599 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3600 | `	}` |
|        - |  3601 | `	/* Perform the store operation */` |
|   227824 |  3602 | `	PH7_MemObjStore(pTos,pObj);` |
|   227824 |  3603 | `	break;` |
|        - |  3604 | `				   }` |
|        - |  3605 | `/*` |
|        - |  3606 | ` * STORE_IDX:   P1 * P3` |
|        - |  3607 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3608 | ` *` |
|        - |  3609 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3610 | ` */` |
|    83485 |  3611 | `case PH7_OP_STORE_IDX:` |
|        - |  3612 | `case PH7_OP_STORE_IDX_REF: {` |
|   166972 |  3613 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3614 | `	ph7_value *pKey;` |
|        - |  3615 | `	sxu32 nIdx;` |
|   166972 |  3616 | `	if( pInstr->iP1 ){` |
|        - |  3617 | `		/* Key is next on stack */` |
|    57962 |  3618 | `		pKey = pTos;` |
|    57962 |  3619 | `		pTos--;` |
|    28982 |  3620 | `	}else{` |
|   109012 |  3621 | `		pKey = 0;` |
|        - |  3622 | `	}` |
|   166972 |  3623 | `	nIdx = pTos->nIdx;` |
|   166972 |  3624 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3625 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3626 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3627 | `		 * checking true sharing count, then re-add after separation. */` |
|   166920 |  3628 | `		if( nIdx != SXU32_HIGH ){` |
|   166920 |  3629 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   250379 |  3630 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   166920 |  3631 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3632 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3633 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3634 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3635 | `				 * refcounts if the backing array was already separated. */` |
|   166920 |  3636 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   166920 |  3637 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   166920 |  3638 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   166920 |  3639 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   166920 |  3640 | `					pTos->x.pOther = pMap;` |
|    83461 |  3641 | `				}else{` |
|        - |  3642 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3643 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3644 | `					pMap = pCur;` |
|        - |  3645 | `				}` |
|    83461 |  3646 | `			}else{` |
|      ! 0 |  3647 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3648 | `			}` |
|    83461 |  3649 | `		}else{` |
|      ! 0 |  3650 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3651 | `		}` |
|   166920 |  3652 | `		if( pMap->iRef < 2 ){` |
|        - |  3653 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3654 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3655 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3656 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3657 | `			pMap->iRef = 2;` |
|      ! 0 |  3658 | `		}` |
|    83461 |  3659 | `	}else{` |
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
|   166920 |  3714 | `	VmPopOperand(&pTos,1);` |
|        - |  3715 | `	/* Phase#2: Perform the insertion */` |
|   166920 |  3716 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3717 | `		/* Insertion by reference */` |
|       15 |  3718 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3719 | `	}else{` |
|   166906 |  3720 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3721 | `	}` |
|   166920 |  3722 | `	if( pKey ){` |
|    57912 |  3723 | `		PH7_MemObjRelease(pKey);` |
|    28955 |  3724 | `	}` |
|   166920 |  3725 | `	break;` |
|        - |  3726 | `					   }` |
|        - |  3727 | `/*` |
|        - |  3728 | ` * INCR: P1 * *` |
|        - |  3729 | ` *` |
|        - |  3730 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3731 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3732 | ` * the stack and increment after that.` |
|        - |  3733 | ` */` |
|   151414 |  3734 | `case PH7_OP_INCR:` |
|        - |  3735 | `#ifdef UNTRUST` |
|        - |  3736 | `	if( pTos < pStack ){` |
|        - |  3737 | `		goto Abort;` |
|        - |  3738 | `	}` |
|        - |  3739 | `#endif` |
|   302874 |  3740 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302874 |  3741 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3742 | `			ph7_value *pObj;` |
|   302874 |  3743 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3744 | `				/* Force a numeric cast */` |
|   302874 |  3745 | `				PH7_MemObjToNumeric(pObj);` |
|   302874 |  3746 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3747 | `					pObj->rVal++;` |
|        - |  3748 | `					/* Try to get an integer representation */` |
|      ! 0 |  3749 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3750 | `				}else{` |
|   302874 |  3751 | `					pObj->x.iVal++;` |
|   302874 |  3752 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3753 | `				}` |
|   302874 |  3754 | `				if( pInstr->iP1 ){` |
|        - |  3755 | `					/* Pre-icrement */` |
|       71 |  3756 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3757 | `				}` |
|   151458 |  3758 | `			}` |
|   151460 |  3759 | `		}else{` |
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
|   151458 |  3774 | `	}` |
|   302874 |  3775 | `	break;` |
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
|    24266 |  3830 | `case PH7_OP_UMINUS:` |
|        - |  3831 | `#ifdef UNTRUST` |
|        - |  3832 | `	if( pTos < pStack ){` |
|        - |  3833 | `		goto Abort;` |
|        - |  3834 | `	}` |
|        - |  3835 | `#endif` |
|        - |  3836 | `	/* Force a numeric (integer,real or both) cast */` |
|    48534 |  3837 | `	PH7_MemObjToNumeric(pTos);` |
|    48534 |  3838 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3839 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3840 | `	}` |
|    48534 |  3841 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    48504 |  3842 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24251 |  3843 | `	}` |
|    48534 |  3844 | `	break;` |
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
|    39987 |  3871 | `case PH7_OP_LNOT:` |
|        - |  3872 | `#ifdef UNTRUST` |
|        - |  3873 | `	if( pTos < pStack ){` |
|        - |  3874 | `		goto Abort;` |
|        - |  3875 | `	}` |
|        - |  3876 | `#endif` |
|        - |  3877 | `	/* Force a boolean cast */` |
|    80020 |  3878 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3879 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3880 | `	}` |
|    80020 |  3881 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80020 |  3882 | `	break;` |
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
|      451 |  3962 | `case PH7_OP_ADD:{` |
|      904 |  3963 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3964 | `#ifdef UNTRUST` |
|        - |  3965 | `	if( pNos < pStack ){` |
|        - |  3966 | `		goto Abort;` |
|        - |  3967 | `	}` |
|        - |  3968 | `#endif` |
|        - |  3969 | `	/* Perform the addition */` |
|      904 |  3970 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      904 |  3971 | `	VmPopOperand(&pTos,1);` |
|      904 |  3972 | `	break;` |
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
|    63399 |  4485 | `case PH7_OP_CAT:{` |
|        - |  4486 | `	ph7_value *pNos,*pCur;` |
|   126800 |  4487 | `	if( pInstr->iP1 < 1 ){` |
|    99748 |  4488 | `		pNos = &pTos[-1];` |
|    49875 |  4489 | `	}else{` |
|    27054 |  4490 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4491 | `	}` |
|        - |  4492 | `#ifdef UNTRUST` |
|        - |  4493 | `	if( pNos < pStack ){` |
|        - |  4494 | `		goto Abort;` |
|        - |  4495 | `	}` |
|        - |  4496 | `#endif` |
|        - |  4497 | `	/* Force a string cast */` |
|   126800 |  4498 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1348 |  4499 | `		PH7_MemObjToString(pNos);` |
|      673 |  4500 | `	}` |
|   126800 |  4501 | `	pCur = &pNos[1];` |
|   255650 |  4502 | `	while( pCur <= pTos ){` |
|   128852 |  4503 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50624 |  4504 | `			PH7_MemObjToString(pCur);` |
|    25311 |  4505 | `		}` |
|        - |  4506 | `		/* Perform the concatenation */` |
|   128852 |  4507 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   128814 |  4508 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64406 |  4509 | `		}` |
|   128852 |  4510 | `		SyBlobRelease(&pCur->sBlob);` |
|   128852 |  4511 | `		pCur++;` |
|        2 |  4512 | `	}` |
|   126800 |  4513 | `	pTos = pNos;` |
|   126800 |  4514 | `	break;` |
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
|    94867 |  4563 | `case PH7_OP_LAND:` |
|        - |  4564 | `case PH7_OP_LOR: {` |
|   189780 |  4565 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4566 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4567 | `#ifdef UNTRUST` |
|        - |  4568 | `	if( pNos < pStack ){` |
|        - |  4569 | `		goto Abort;` |
|        - |  4570 | `	}` |
|        - |  4571 | `#endif` |
|        - |  4572 | `	/* Force a boolean cast */` |
|   189780 |  4573 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4574 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4575 | `	}` |
|   189780 |  4576 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4577 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4578 | `	}` |
|   189780 |  4579 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189780 |  4580 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189780 |  4581 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4582 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    87284 |  4583 | `		v1 = and_logic[v1*3+v2];` |
|    43665 |  4584 | `	}else{` |
|        - |  4585 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102498 |  4586 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4587 | `	}` |
|   189780 |  4588 | `	if( v1 == 2 ){` |
|      ! 0 |  4589 | `		v1 = 1;` |
|      ! 0 |  4590 | `	}` |
|   189780 |  4591 | `	VmPopOperand(&pTos,1);` |
|   189780 |  4592 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189780 |  4593 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189780 |  4594 | `	break;` |
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
|     3985 |  4726 | `case PH7_OP_EQ:` |
|        - |  4727 | `case PH7_OP_NEQ: {` |
|     7972 |  4728 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4729 | `	/* Perform the comparison and act accordingly */` |
|        - |  4730 | `#ifdef UNTRUST` |
|        - |  4731 | `	if( pNos < pStack ){` |
|        - |  4732 | `		goto Abort;` |
|        - |  4733 | `	}` |
|        - |  4734 | `#endif` |
|     7972 |  4735 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7972 |  4736 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4737 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7963 |  4738 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7928 |  4739 | `		rc = rc == 0;` |
|     3965 |  4740 | `	}else{` |
|       28 |  4741 | `		rc = rc != 0;` |
|        - |  4742 | `	}` |
|     7972 |  4743 | `	VmPopOperand(&pTos,1);` |
|     7972 |  4744 | `	if( !pInstr->iP2 ){` |
|        - |  4745 | `		/* Push comparison result without taking the jump */` |
|     7972 |  4746 | `		PH7_MemObjRelease(pTos);` |
|     7972 |  4747 | `		pTos->x.iVal = rc;` |
|        - |  4748 | `		/* Invalidate any prior representation */` |
|     7972 |  4749 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3987 |  4750 | `	}else{` |
|      ! 0 |  4751 | `		if( rc ){` |
|        - |  4752 | `			/* Jump to the desired location */` |
|      ! 0 |  4753 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4754 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4755 | `		}` |
|        - |  4756 | `	}` |
|     7972 |  4757 | `	break;` |
|        - |  4758 | `				 }` |
|        - |  4759 | `/* OP_TEQ P1 P2 *` |
|        - |  4760 | ` *` |
|        - |  4761 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4762 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4763 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4764 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4765 | ` */` |
|   133668 |  4766 | `case PH7_OP_TEQ: {` |
|   267338 |  4767 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4768 | `	/* Perform the comparison and act accordingly */` |
|        - |  4769 | `#ifdef UNTRUST` |
|        - |  4770 | `	if( pNos < pStack ){` |
|        - |  4771 | `		goto Abort;` |
|        - |  4772 | `	}` |
|        - |  4773 | `#endif` |
|   267338 |  4774 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   267338 |  4775 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4776 | `		rc = 0;` |
|        2 |  4777 | `	}else{` |
|   267336 |  4778 | `		rc = rc == 0;` |
|        - |  4779 | `	}` |
|   267338 |  4780 | `	VmPopOperand(&pTos,1);` |
|   267338 |  4781 | `	if( !pInstr->iP2 ){` |
|        - |  4782 | `		/* Push comparison result without taking the jump */` |
|   267338 |  4783 | `		PH7_MemObjRelease(pTos);` |
|   267338 |  4784 | `		pTos->x.iVal = rc;` |
|        - |  4785 | `		/* Invalidate any prior representation */` |
|   267338 |  4786 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   133670 |  4787 | `	}else{` |
|      ! 0 |  4788 | `		if( rc ){` |
|        - |  4789 | `			/* Jump to the desired location */` |
|      ! 0 |  4790 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4791 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4792 | `		}` |
|        - |  4793 | `	}` |
|   267338 |  4794 | `	break;` |
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
|   104290 |  4805 | `case PH7_OP_TNE: {` |
|   208582 |  4806 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4807 | `	/* Perform the comparison and act accordingly */` |
|        - |  4808 | `#ifdef UNTRUST` |
|        - |  4809 | `	if( pNos < pStack ){` |
|        - |  4810 | `		goto Abort;` |
|        - |  4811 | `	}` |
|        - |  4812 | `#endif` |
|   208582 |  4813 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   208582 |  4814 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4815 | `		rc = 1;` |
|        2 |  4816 | `	}else{` |
|   208580 |  4817 | `		rc = rc != 0;` |
|        - |  4818 | `	}` |
|   208582 |  4819 | `	VmPopOperand(&pTos,1);` |
|   208582 |  4820 | `	if( !pInstr->iP2 ){` |
|        - |  4821 | `		/* Push comparison result without taking the jump */` |
|   208582 |  4822 | `		PH7_MemObjRelease(pTos);` |
|   208582 |  4823 | `		pTos->x.iVal = rc;` |
|        - |  4824 | `		/* Invalidate any prior representation */` |
|   208582 |  4825 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   104292 |  4826 | `	}else{` |
|      ! 0 |  4827 | `		if( rc ){` |
|        - |  4828 | `			/* Jump to the desired location */` |
|      ! 0 |  4829 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4830 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4831 | `		}` |
|        - |  4832 | `	}` |
|   208582 |  4833 | `	break;` |
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
|        - |  4937 | `/* OP_SEQ P1 P2 *` |
|        - |  4938 | ` * Strict string comparison.` |
|        - |  4939 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4940 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4941 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4942 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4943 | ` * use PH7_OP_EQ.` |
|        - |  4944 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4945 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4946 | ` */` |
|        - |  4947 | `/* OP_SNE P1 P2 *` |
|        - |  4948 | ` * Strict string comparison.` |
|        - |  4949 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4950 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4951 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4952 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4953 | ` * use PH7_OP_EQ.` |
|        - |  4954 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4955 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4956 | ` */` |
|       18 |  4957 | `case PH7_OP_SEQ:` |
|        - |  4958 | `case PH7_OP_SNE: {` |
|       38 |  4959 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4960 | `	SyString s1,s2;` |
|        - |  4961 | `	/* Perform the comparison and act accordingly */` |
|        - |  4962 | `#ifdef UNTRUST` |
|        - |  4963 | `	if( pNos < pStack ){` |
|        - |  4964 | `		goto Abort;` |
|        - |  4965 | `	}` |
|        - |  4966 | `#endif` |
|        - |  4967 | `	/* Force a string cast */` |
|       38 |  4968 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4969 | `		PH7_MemObjToString(pTos);` |
|        2 |  4970 | `	}` |
|       38 |  4971 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4972 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4973 | `	}` |
|       38 |  4974 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4975 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4976 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4977 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4978 | `		rc = rc != 0;` |
|      ! 0 |  4979 | `	}else{` |
|       38 |  4980 | `		rc = rc == 0;` |
|        - |  4981 | `	}` |
|       38 |  4982 | `	VmPopOperand(&pTos,1);` |
|       38 |  4983 | `	if( !pInstr->iP2 ){` |
|        - |  4984 | `		/* Push comparison result without taking the jump */` |
|       38 |  4985 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4986 | `		pTos->x.iVal = rc;` |
|        - |  4987 | `		/* Invalidate any prior representation */` |
|       38 |  4988 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4989 | `	}else{` |
|      ! 0 |  4990 | `		if( rc ){` |
|        - |  4991 | `			/* Jump to the desired location */` |
|      ! 0 |  4992 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4993 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4994 | `		}` |
|        - |  4995 | `	}` |
|       38 |  4996 | `	break;` |
|        - |  4997 | `				 }` |
|        - |  4998 | `/*` |
|        - |  4999 | ` * OP_LOAD_REF * * *` |
|        - |  5000 | ` * Push the index of a referenced object on the stack.` |
|        - |  5001 | ` */` |
|       57 |  5002 | `case PH7_OP_LOAD_REF: {` |
|        - |  5003 | `	sxu32 nIdx;` |
|        - |  5004 | `#ifdef UNTRUST` |
|        - |  5005 | `	if( pTos < pStack ){` |
|        - |  5006 | `		goto Abort;` |
|        - |  5007 | `	}` |
|        - |  5008 | `#endif` |
|        - |  5009 | `	/* Extract memory object index */` |
|      115 |  5010 | `	nIdx = pTos->nIdx;` |
|      115 |  5011 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5012 | `		/* Nullify the object */` |
|       95 |  5013 | `		PH7_MemObjRelease(pTos);` |
|        - |  5014 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5015 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5016 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5017 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5018 | `	}` |
|      115 |  5019 | `	break;` |
|        - |  5020 | `					  }` |
|        - |  5021 | `/*` |
|        - |  5022 | ` * OP_STORE_REF * * P3` |
|        - |  5023 | ` * Perform an assignment operation by reference.` |
|        - |  5024 | ` */` |
|       15 |  5025 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5026 | `	 SyString sName = { 0 , 0 };` |
|        - |  5027 | `	 VmFrame *pFrameLocal;` |
|        - |  5028 | `	SyHashEntry *pEntry;` |
|        - |  5029 | `	sxu32 nIdx;` |
|        - |  5030 | `#ifdef UNTRUST` |
|        - |  5031 | `	if( pTos < pStack ){` |
|        - |  5032 | `		goto Abort;` |
|        - |  5033 | `	}` |
|        - |  5034 | `#endif` |
|       32 |  5035 | `	if( pInstr->p3 == 0 ){` |
|        - |  5036 | `		char *zName;` |
|        - |  5037 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5038 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5039 | `			/* Force a string cast */` |
|      ! 0 |  5040 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5041 | `		}` |
|      ! 0 |  5042 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5043 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5044 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5045 | `			if( zName ){` |
|      ! 0 |  5046 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5047 | `			}` |
|      ! 0 |  5048 | `		}` |
|      ! 0 |  5049 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5050 | `		pTos--;` |
|      ! 0 |  5051 | `	}else{` |
|       32 |  5052 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5053 | `	}` |
|       32 |  5054 | `	nIdx = pTos->nIdx;` |
|       32 |  5055 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5056 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5057 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5058 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5059 | `		}else{` |
|        - |  5060 | `			ph7_value *pObj;` |
|        - |  5061 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5062 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5063 | `			if( pObj == 0 ){` |
|      ! 0 |  5064 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5065 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5066 | `				goto Abort;` |
|        - |  5067 | `			}` |
|        - |  5068 | `			/* Perform the store operation */` |
|      ! 0 |  5069 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5070 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5071 | `		}` |
|       32 |  5072 | `	}else if( sName.nByte > 0){` |
|       32 |  5073 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5074 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5075 | `		}else{` |
|       32 |  5076 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5077 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5078 | `			/* Query the local frame */` |
|       32 |  5079 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5080 | `			if( pEntry ){` |
|      ! 0 |  5081 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5082 | `			}else{` |
|       32 |  5083 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5084 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5085 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5086 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5087 | `				}` |
|       32 |  5088 | `				if( rc == SXRET_OK ){` |
|       32 |  5089 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5090 | `				}` |
|        - |  5091 | `			}` |
|        - |  5092 | `		}` |
|       15 |  5093 | `	}` |
|       32 |  5094 | `	break;` |
|        - |  5095 | `				 }` |
|        - |  5096 | `/*` |
|        - |  5097 | ` * OP_UPLINK P1 * *` |
|        - |  5098 | ` * Link a variable to the top active VM frame.` |
|        - |  5099 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5100 | ` */` |
|       25 |  5101 | `case PH7_OP_UPLINK: {` |
|       52 |  5102 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5103 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5104 | `		SyString sName;` |
|        - |  5105 | `		/* Perform the link */` |
|      104 |  5106 | `		while( pLink <= pTos ){` |
|       54 |  5107 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5108 | `				/* Force a string cast */` |
|      ! 0 |  5109 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5110 | `			}` |
|       54 |  5111 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5112 | `			if( sName.nByte > 0 ){` |
|       54 |  5113 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5114 | `			}` |
|       54 |  5115 | `			pLink++;` |
|        2 |  5116 | `		}` |
|       25 |  5117 | `	}` |
|       52 |  5118 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5119 | `	break;` |
|        - |  5120 | `					}` |
|        - |  5121 | `/*` |
|        - |  5122 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5123 | ` * Push an exception in the corresponding container so that` |
|        - |  5124 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5125 | ` */` |
|       32 |  5126 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5127 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5128 | `	VmFrame *pFrameLocal;` |
|        - |  5129 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5130 | `	pException->iFinallyDone = 0;` |
|       66 |  5131 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5132 | `	/* Create the exception frame */` |
|       66 |  5133 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5134 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5135 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5136 | `		goto Abort;` |
|        - |  5137 | `	}` |
|        - |  5138 | `	/* Mark the special frame */` |
|       66 |  5139 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5140 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5141 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5142 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5143 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5144 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5145 | `	break;` |
|        - |  5146 | `							}` |
|        - |  5147 | `/*` |
|        - |  5148 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5149 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5150 | ` */` |
|       31 |  5151 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5152 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5153 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5154 | `		ph7_exception **apException;` |
|        - |  5155 | `		/* Pop the loaded exception */` |
|       28 |  5156 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5157 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5158 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5159 | `		}` |
|       13 |  5160 | `	}` |
|       64 |  5161 | `	pException->pFrame = 0;` |
|        - |  5162 | `	/* Leave the exception frame */` |
|       64 |  5163 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5164 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5165 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5166 | `		sxi32 rcFinally;` |
|       20 |  5167 | `		pException->iFinallyDone = 1;` |
|       20 |  5168 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5169 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5170 | `			goto Abort;` |
|        - |  5171 | `		}` |
|        9 |  5172 | `	}` |
|       64 |  5173 | `	break;` |
|        - |  5174 | `							}` |
|        - |  5175 |  |
|        - |  5176 | `/*` |
|        - |  5177 | ` * OP_THROW * P2 *` |
|        - |  5178 | ` * Throw an user exception.` |
|        - |  5179 | ` */` |
|       18 |  5180 | `case PH7_OP_THROW: {` |
|       38 |  5181 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5182 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5183 | `#ifdef UNTRUST` |
|        - |  5184 | `	if( pTos < pStack ){` |
|        - |  5185 | `		goto Abort;` |
|        - |  5186 | `	}` |
|        - |  5187 | `#endif` |
|       38 |  5188 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5189 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5190 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5191 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5192 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5193 | `		ph7_class *pException;` |
|        - |  5194 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5195 | `		 */` |
|       38 |  5196 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5197 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5198 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5199 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5200 | `			if( rc == SXERR_ABORT ){` |
|        - |  5201 | `				/* Abort processing immediately */` |
|      ! 0 |  5202 | `				goto Abort;` |
|        - |  5203 | `			}` |
|      ! 0 |  5204 | `		}else{` |
|        - |  5205 | `			/* Throw the exception */` |
|       38 |  5206 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5207 | `			if( rc == SXERR_ABORT ){` |
|        - |  5208 | `				/* Abort processing immediately */` |
|        9 |  5209 | `				goto Abort;` |
|        - |  5210 | `			}` |
|        - |  5211 | `		}` |
|       16 |  5212 | `	}else{` |
|        - |  5213 | `		/* Expecting a class instance */` |
|      ! 0 |  5214 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5215 | `		if( rc == SXERR_ABORT ){` |
|        - |  5216 | `			/* Abort processing immediately */` |
|      ! 0 |  5217 | `			goto Abort;` |
|        - |  5218 | `		}` |
|        - |  5219 | `	}` |
|        - |  5220 | `	/* Pop the top entry */` |
|       30 |  5221 | `	VmPopOperand(&pTos,1);` |
|        - |  5222 | `	/* Perform an unconditional jump */` |
|       30 |  5223 | `	pc = nJump - 1;` |
|       30 |  5224 | `	break;` |
|        - |  5225 | `				   }` |
|        - |  5226 | `/*` |
|        - |  5227 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5228 | ` * Prepare a foreach step.` |
|        - |  5229 | ` */` |
|     5061 |  5230 | `case PH7_OP_FOREACH_INIT: {` |
|    10124 |  5231 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5232 | `	void *pName;` |
|        - |  5233 | `#ifdef UNTRUST` |
|        - |  5234 | `	if( pTos < pStack ){` |
|        - |  5235 | `		goto Abort;` |
|        - |  5236 | `	}` |
|        - |  5237 | `#endif` |
|    10124 |  5238 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5239 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5240 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5241 | `			/* Force a string cast */` |
|      ! 0 |  5242 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5243 | `		}` |
|        - |  5244 | `		/* Duplicate name */` |
|      ! 0 |  5245 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5246 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5247 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5248 | `		}` |
|      ! 0 |  5249 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5250 | `	}` |
|    10124 |  5251 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5252 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5253 | `			/* Force a string cast */` |
|      ! 0 |  5254 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5255 | `		}` |
|        - |  5256 | `		/* Duplicate name */` |
|      ! 0 |  5257 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5258 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5259 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5260 | `		}` |
|      ! 0 |  5261 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5262 | `	}` |
|        - |  5263 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10124 |  5264 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5265 | `		/* Jump out of the loop */` |
|      ! 0 |  5266 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5267 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5268 | `		}` |
|      ! 0 |  5269 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5270 | `	}else{` |
|        - |  5271 | `		ph7_foreach_step *pStep;` |
|    10124 |  5272 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10124 |  5273 | `		if( pStep == 0 ){` |
|      ! 0 |  5274 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5275 | `			/* Jump out of the loop */` |
|      ! 0 |  5276 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5277 | `		}else{` |
|        - |  5278 | `			/* Zero the structure */` |
|    10124 |  5279 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5280 | `			/* Prepare the step */` |
|    10124 |  5281 | `			pStep->iFlags = pInfo->iFlags;` |
|    10124 |  5282 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5283 | `				ph7_hashmap *pMap;` |
|        - |  5284 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5285 | `				 * source array so mutations don't affect other sharers. */` |
|    10096 |  5286 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5287 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5288 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5289 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5290 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5291 | `						 * variable still points at the same hashmap as` |
|        - |  5292 | `						 * the stack value. */` |
|        9 |  5293 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5294 | `							pCur->iRef--;` |
|        9 |  5295 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5296 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5297 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5298 | `						}` |
|        4 |  5299 | `					}` |
|        4 |  5300 | `				}` |
|    10096 |  5301 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5302 | `				/* Reset the internal loop cursor */` |
|    10096 |  5303 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5304 | `				/* Mark the step */` |
|    10096 |  5305 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10096 |  5306 | `				pStep->xIter.pMap = pMap;` |
|    10096 |  5307 | `				pMap->iRef++;` |
|     5049 |  5308 | `			}else{` |
|       30 |  5309 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5310 | `				ph7_class *pIteratorClass;` |
|        - |  5311 | `				/* Check if the object implements Iterator */` |
|       30 |  5312 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5313 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5314 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5315 | `					ph7_class_method *pRewind;` |
|       20 |  5316 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5317 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5318 | `					pThis->iRef++;` |
|       20 |  5319 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5320 | `					if( pRewind ){` |
|       20 |  5321 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5322 | `					}` |
|       11 |  5323 | `				}else{` |
|        - |  5324 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5325 | `					ph7_class *pIterAggClass;` |
|       12 |  5326 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5327 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5328 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5329 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5330 | `						ph7_class_method *pGetIter;` |
|        3 |  5331 | `						int iterAggOk = 0;` |
|        3 |  5332 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5333 | `						if( pGetIter ){` |
|        - |  5334 | `							ph7_value sResult;` |
|        3 |  5335 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5336 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5337 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5338 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5339 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5340 | `									ph7_class_method *pRewind;` |
|        3 |  5341 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5342 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5343 | `									pIterObj->iRef++;` |
|        - |  5344 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5345 | `									pStep->pOwner = pThis;` |
|        3 |  5346 | `									pThis->iRef++;` |
|        3 |  5347 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5348 | `									if( pRewind ){` |
|        3 |  5349 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5350 | `									}` |
|        3 |  5351 | `									iterAggOk = 1;` |
|        1 |  5352 | `								}` |
|        1 |  5353 | `							}` |
|        3 |  5354 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5355 | `						}` |
|        3 |  5356 | `						if( !iterAggOk ){` |
|        - |  5357 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5358 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5359 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5360 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5361 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5362 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5363 | `						}` |
|        2 |  5364 | `					}else{` |
|        - |  5365 | `						/* Plain object iteration via hAttr */` |
|        9 |  5366 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5367 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5368 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5369 | `						pThis->iRef++;` |
|        - |  5370 | `					}` |
|        - |  5371 | `				}` |
|        - |  5372 | `			}` |
|        - |  5373 | `		}` |
|    10124 |  5374 | `		if( pStep ){` |
|    10124 |  5375 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5376 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5377 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5378 | `				/* Jump out of the loop */` |
|      ! 0 |  5379 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5380 | `			}` |
|     5061 |  5381 | `		}` |
|        - |  5382 | `	}` |
|    10124 |  5383 | `	VmPopOperand(&pTos,1);` |
|    10124 |  5384 | `	break;` |
|        - |  5385 | `						  }` |
|        - |  5386 | `/*` |
|        - |  5387 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5388 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5389 | ` */` |
|    81945 |  5390 | `case PH7_OP_FOREACH_STEP: {` |
|   163892 |  5391 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5392 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5393 | `	ph7_value *pValue;` |
|        - |  5394 | `	VmFrame *pFrameLocal;` |
|        - |  5395 | `	/* Peek the last step */` |
|   163892 |  5396 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   163892 |  5397 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   163892 |  5398 | `	pFrameLocal = pVm->pFrame;` |
|   163892 |  5399 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   163892 |  5400 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   163780 |  5401 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5402 | `		ph7_hashmap_node *pNode;` |
|        - |  5403 | `		/* Extract the current node value */` |
|   163780 |  5404 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   163780 |  5405 | `		if( pNode == 0 ){` |
|        - |  5406 | `			/* No more entry to process */` |
|    10094 |  5407 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10094 |  5408 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5409 | `				/* Break the reference with the last element */` |
|        7 |  5410 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5411 | `			}` |
|        - |  5412 | `			/* Automatically reset the loop cursor */` |
|    10094 |  5413 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5414 | `			/* Cleanup the mess left behind */` |
|    10094 |  5415 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10094 |  5416 | `			SySetPop(&pInfo->aStep);` |
|    10094 |  5417 | `			PH7_HashmapUnref(pMap);` |
|     5048 |  5418 | `		}else{` |
|   153688 |  5419 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5420 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5421 | `				if( pKey ){` |
|      416 |  5422 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5423 | `				}` |
|      207 |  5424 | `			}` |
|   153688 |  5425 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5426 | `				SyHashEntry *pEntry;` |
|        - |  5427 | `				/* Pass by reference */` |
|       23 |  5428 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5429 | `				if( pEntry ){` |
|       23 |  5430 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5431 | `				}else{` |
|      ! 0 |  5432 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5433 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5434 | `				}` |
|       12 |  5435 | `			}else{` |
|        - |  5436 | `				/* Make a copy of the entry value */` |
|   153666 |  5437 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   153666 |  5438 | `				if( pValue ){` |
|   153666 |  5439 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    76832 |  5440 | `				}` |
|        - |  5441 | `			}` |
|        2 |  5442 | `		}` |
|    82003 |  5443 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5444 | `		/* Iterator-based iteration.` |
|        - |  5445 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5446 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5447 | `		 */` |
|       90 |  5448 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5449 | `		ph7_class_method *pMethod;` |
|        - |  5450 | `		ph7_value sResult;` |
|       90 |  5451 | `		int isValid = 0;` |
|        - |  5452 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5453 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5454 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5455 | `		}else{` |
|       70 |  5456 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5457 | `			if( pMethod ){` |
|       70 |  5458 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5459 | `			}` |
|        - |  5460 | `		}` |
|        - |  5461 | `		/* Call valid() */` |
|       90 |  5462 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5463 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5464 | `		if( pMethod ){` |
|       90 |  5465 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5466 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5467 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5468 | `		}` |
|       90 |  5469 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5470 | `		if( !isValid ){` |
|        - |  5471 | `			/* Iterator exhausted */` |
|       20 |  5472 | `			pc = pInstr->iP2 - 1;` |
|        - |  5473 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5474 | `			if( pStep->pOwner ){` |
|        3 |  5475 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5476 | `			}` |
|       20 |  5477 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5478 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5479 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5480 | `		}else{` |
|        - |  5481 | `			/* Call current() to get value */` |
|       72 |  5482 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5483 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5484 | `			if( pMethod ){` |
|       72 |  5485 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5486 | `			}` |
|       72 |  5487 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5488 | `			if( pValue ){` |
|       72 |  5489 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5490 | `			}` |
|       72 |  5491 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5492 | `			/* Call key() if needed */` |
|       72 |  5493 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5494 | `				ph7_value sKey;` |
|       35 |  5495 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5496 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5497 | `				if( pMethod ){` |
|       35 |  5498 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5499 | `				}` |
|       35 |  5500 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5501 | `				if( pValue ){` |
|       35 |  5502 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5503 | `				}` |
|       35 |  5504 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5505 | `			}` |
|        - |  5506 | `		}` |
|       46 |  5507 | `	}else{` |
|       25 |  5508 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5509 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5510 | `		SyHashEntry *pEntry;` |
|        - |  5511 | `		/* Point to the next attribute */` |
|       29 |  5512 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5513 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5514 | `			/* Check access permission */` |
|       31 |  5515 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5516 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5517 | `					break; /* Access is granted */` |
|        - |  5518 | `			}` |
|        1 |  5519 | `		}` |
|       25 |  5520 | `		if( pEntry == 0 ){` |
|        - |  5521 | `			/* Clean up the mess left behind */` |
|        9 |  5522 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5523 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5524 | `				/* Break the reference with the last element */` |
|        3 |  5525 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5526 | `			}` |
|        9 |  5527 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5528 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5529 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5530 | `		}else{` |
|       17 |  5531 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5532 | `			ph7_value *pAttrValue;` |
|       17 |  5533 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5534 | `				/* Fill with the current attribute name */` |
|       17 |  5535 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5536 | `				if( pKey ){` |
|       17 |  5537 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5538 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5539 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5540 | `				}` |
|        8 |  5541 | `			}` |
|        - |  5542 | `			/* Extract attribute value */` |
|       17 |  5543 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5544 | `			if( pAttrValue ){` |
|       17 |  5545 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5546 | `					/* Pass by reference */` |
|        3 |  5547 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5548 | `					if( pEntry ){` |
|        3 |  5549 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5550 | `					}else{` |
|      ! 0 |  5551 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5552 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5553 | `					}` |
|        2 |  5554 | `				}else{` |
|        - |  5555 | `					/* Make a copy of the attribute value */` |
|       15 |  5556 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5557 | `					if( pValue ){` |
|       15 |  5558 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5559 | `					}` |
|        - |  5560 | `				}` |
|        8 |  5561 | `			}` |
|        - |  5562 | `		}` |
|        - |  5563 | `	}` |
|   163892 |  5564 | `	break;` |
|        - |  5565 | `						  }` |
|        - |  5566 | `/*` |
|        - |  5567 | ` * OP_MEMBER P1 P2` |
|        - |  5568 | ` * Load class attribute/method on the stack.` |
|        - |  5569 | ` */` |
|     2210 |  5570 | `case PH7_OP_MEMBER: {` |
|        - |  5571 | `	ph7_class_instance *pThis;` |
|        - |  5572 | `	ph7_value *pNos;` |
|        - |  5573 | `	SyString sName;` |
|     4422 |  5574 | `	if( !pInstr->iP1 ){` |
|     4280 |  5575 | `		pNos = &pTos[-1];` |
|        - |  5576 | `#ifdef UNTRUST` |
|        - |  5577 | `		if( pNos < pStack ){` |
|        - |  5578 | `			goto Abort;` |
|        - |  5579 | `		}` |
|        - |  5580 | `#endif` |
|     4280 |  5581 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5582 | `			ph7_class *pClass;` |
|        - |  5583 | `			/* Class already instantiated */` |
|     4280 |  5584 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5585 | `			/* Point to the instantiated class */` |
|     4280 |  5586 | `			pClass = pThis->pClass;` |
|        - |  5587 | `			/* Extract attribute name first */` |
|     4280 |  5588 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4280 |  5589 | `			if( pInstr->iP2 ){` |
|        - |  5590 | `				/* Method call */` |
|      436 |  5591 | `				ph7_class_method *pMeth = 0;` |
|      436 |  5592 | `				if( sName.nByte > 0 ){` |
|        - |  5593 | `					/* Extract the target method */` |
|      436 |  5594 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      217 |  5595 | `				}` |
|      436 |  5596 | `				if( pMeth == 0 ){` |
|      ! 0 |  5597 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5598 | `						&pClass->sName,&sName` |
|        - |  5599 | `						);` |
|        - |  5600 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5601 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5602 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5603 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5604 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5605 | `				}else{` |
|        - |  5606 | `					/* Push method name on the stack */` |
|      436 |  5607 | `					PH7_MemObjRelease(pTos);` |
|      436 |  5608 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      436 |  5609 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5610 | `				}` |
|      436 |  5611 | `				pTos->nIdx = SXU32_HIGH;` |
|      219 |  5612 | `			}else{` |
|        - |  5613 | `				/* Attribute access */` |
|     3846 |  5614 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5615 | `				SyHashEntry *pEntry;` |
|        - |  5616 | `				/* Extract the target attribute */` |
|     3846 |  5617 | `				if( sName.nByte > 0 ){` |
|     3846 |  5618 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3846 |  5619 | `					if( pEntry ){` |
|        - |  5620 | `						/* Point to the attribute value */` |
|     3844 |  5621 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1921 |  5622 | `					}` |
|     1922 |  5623 | `				}` |
|     3846 |  5624 | `				if( pObjAttr == 0 ){` |
|        - |  5625 | `					/* No such attribute,load null */` |
|        4 |  5626 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5627 | `						&pClass->sName,&sName);` |
|        - |  5628 | `					/* Call the __get magic method if available */` |
|        3 |  5629 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5630 | `				}` |
|     3846 |  5631 | `				VmPopOperand(&pTos,1);` |
|        - |  5632 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5633 | `				 * This is due to the following case:` |
|        - |  5634 | `				 *     (new TestClass())->foo;` |
|        - |  5635 | `				 */` |
|     3846 |  5636 | `				pThis->iRef++;` |
|     3846 |  5637 | `				PH7_MemObjRelease(pTos);` |
|     3846 |  5638 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3846 |  5639 | `				if( pObjAttr ){` |
|     3844 |  5640 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5641 | `					/* Check attribute access */` |
|     3844 |  5642 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5643 | `						/* Load attribute */` |
|     3844 |  5644 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3844 |  5645 | `						if( pValue ){` |
|     3844 |  5646 | `							if( pThis->iRef < 2 ){` |
|        - |  5647 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5648 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5649 | `								 */` |
|        3 |  5650 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5651 | `							}else{` |
|        - |  5652 | `								/* Simple load */` |
|     3842 |  5653 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5654 | `							}` |
|     3844 |  5655 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3842 |  5656 | `								if( pThis->iRef > 1 ){` |
|        - |  5657 | `									/* Load attribute index */` |
|     3840 |  5658 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1919 |  5659 | `								}` |
|     1920 |  5660 | `							}` |
|     1921 |  5661 | `						}` |
|     1921 |  5662 | `					}` |
|     1921 |  5663 | `				}` |
|        - |  5664 | `				/* Safely unreference the object */` |
|     3846 |  5665 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5666 | `			}` |
|     2141 |  5667 | `		}else{` |
|      ! 0 |  5668 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5669 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5670 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5671 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5672 | `		}` |
|     2141 |  5673 | `	}else{` |
|        - |  5674 | `		/* Static member access using class name */` |
|      144 |  5675 | `		pNos = pTos;` |
|      144 |  5676 | `		pThis = 0;` |
|      144 |  5677 | `		if( !pInstr->p3 ){` |
|      132 |  5678 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      132 |  5679 | `			pNos--;` |
|        - |  5680 | `#ifdef UNTRUST` |
|        - |  5681 | `			if( pNos < pStack ){` |
|        - |  5682 | `				goto Abort;` |
|        - |  5683 | `			}` |
|        - |  5684 | `#endif` |
|       67 |  5685 | `		}else{` |
|        - |  5686 | `			/* Attribute name already computed */` |
|       14 |  5687 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5688 | `		}` |
|      144 |  5689 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      144 |  5690 | `			ph7_class *pClass = 0;` |
|      144 |  5691 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5692 | `				/* Class already instantiated */` |
|        5 |  5693 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5694 | `				pClass = pThis->pClass;` |
|        5 |  5695 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5696 | `			}else{` |
|        - |  5697 | `				/* Try to extract the target class */` |
|      140 |  5698 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      140 |  5699 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      140 |  5700 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5701 | `					/* Handle self/static/parent keywords */` |
|      140 |  5702 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       36 |  5703 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       36 |  5704 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5705 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5706 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5707 | `						}` |
|      123 |  5708 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       22 |  5709 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      103 |  5710 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       16 |  5711 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       16 |  5712 | `						if( pSelf && pSelf->pBase ){` |
|       16 |  5713 | `							pClass = pSelf->pBase;` |
|        7 |  5714 | `						}` |
|        9 |  5715 | `					}else{` |
|       72 |  5716 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5717 | `					}` |
|       69 |  5718 | `				}` |
|        - |  5719 | `			}` |
|      144 |  5720 | `			if( pClass == 0 ){` |
|        - |  5721 | `				/* Undefined class */` |
|      ! 0 |  5722 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5723 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5724 | `					);` |
|      ! 0 |  5725 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5726 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5727 | `				}` |
|      ! 0 |  5728 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5729 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5730 | `			}else{` |
|      144 |  5731 | `				if( pInstr->iP2 ){` |
|        - |  5732 | `					/* Method call */` |
|       68 |  5733 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5734 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5735 | `						/* Extract the target method */` |
|       68 |  5736 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5737 | `					}` |
|       68 |  5738 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5739 | `						if( pMeth ){` |
|      ! 0 |  5740 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5741 | `								&pClass->sName,&sName` |
|        - |  5742 | `								);` |
|      ! 0 |  5743 | `						}else{` |
|      ! 0 |  5744 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5745 | `								&pClass->sName,&sName` |
|        - |  5746 | `								);` |
|        - |  5747 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5748 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5749 | `						}` |
|        - |  5750 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5751 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5752 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5753 | `						}` |
|      ! 0 |  5754 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5755 | `					}else{` |
|        - |  5756 | `						/* Push method name on the stack */` |
|       68 |  5757 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5758 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5759 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5760 | `					}` |
|       68 |  5761 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5762 | `				}else{` |
|        - |  5763 | `					/* Attribute access */` |
|       78 |  5764 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5765 | `					/* Check for special ::class pseudo-constant */` |
|      113 |  5766 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5767 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5768 | `						/* ::class returns the fully qualified class name */` |
|        - |  5769 | `						/* Pop the attribute name from the stack */` |
|       60 |  5770 | `						if( !pInstr->p3 ){` |
|       60 |  5771 | `							VmPopOperand(&pTos,1);` |
|       29 |  5772 | `						}` |
|       60 |  5773 | `						PH7_MemObjRelease(pTos);` |
|        - |  5774 | `						/* Load the class name */` |
|       60 |  5775 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5776 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5777 | `					}else{` |
|        - |  5778 | `						/* Extract the target attribute */` |
|       20 |  5779 | `						if( sName.nByte > 0 ){` |
|       20 |  5780 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5781 | `						}` |
|       20 |  5782 | `						if( pAttr == 0 ){` |
|        - |  5783 | `							/* No such attribute,load null */` |
|      ! 0 |  5784 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5785 | `								&pClass->sName,&sName);` |
|        - |  5786 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5787 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5788 | `						}` |
|        - |  5789 | `						/* Pop the attribute name from the stack */` |
|       20 |  5790 | `						if( !pInstr->p3 ){` |
|        7 |  5791 | `							VmPopOperand(&pTos,1);` |
|        3 |  5792 | `						}` |
|       20 |  5793 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5794 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5795 | `						if( pAttr ){` |
|       20 |  5796 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5797 | `								/* Access to a non static attribute */` |
|      ! 0 |  5798 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5799 | `									&pClass->sName,&pAttr->sName` |
|        - |  5800 | `									);` |
|      ! 0 |  5801 | `							}else{` |
|        - |  5802 | `								ph7_value *pValue;` |
|        - |  5803 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5804 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5805 | `									/* Load the desired attribute */` |
|       20 |  5806 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5807 | `									if( pValue ){` |
|       20 |  5808 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5809 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5810 | `											/* Load index number */` |
|       14 |  5811 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5812 | `										}` |
|        9 |  5813 | `									}` |
|        9 |  5814 | `								}` |
|        - |  5815 | `							}` |
|        9 |  5816 | `						}` |
|        - |  5817 | `					}` |
|        - |  5818 | `				}` |
|      144 |  5819 | `				if( pThis ){` |
|        - |  5820 | `					/* Safely unreference the object */` |
|        5 |  5821 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5822 | `				}` |
|        - |  5823 | `			}` |
|       73 |  5824 | `		}else{` |
|        - |  5825 | `			/* Pop operands */` |
|      ! 0 |  5826 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5827 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5828 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5829 | `			}` |
|      ! 0 |  5830 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5831 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5832 | `		}` |
|        - |  5833 | `	}` |
|     4422 |  5834 | `	break;` |
|        - |  5835 | `					}` |
|        - |  5836 | `/*` |
|        - |  5837 | ` * OP_NEW P1 * * *` |
|        - |  5838 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5839 | ` */` |
|      328 |  5840 | `case PH7_OP_NEW: {` |
|      658 |  5841 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      658 |  5842 | `	ph7_class *pClass = 0;` |
|        - |  5843 | `	ph7_class_instance *pNew;` |
|      658 |  5844 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5845 | `		/* Try to extract the desired class */` |
|      986 |  5846 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      656 |  5847 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      328 |  5848 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5849 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5850 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5851 | `	}` |
|      658 |  5852 | `	if( pClass == 0 ){` |
|        - |  5853 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5854 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5855 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5856 | `			);` |
|        - |  5857 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5858 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5859 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5860 | `			/* Pop given arguments */` |
|      ! 0 |  5861 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5862 | `		}` |
|      ! 0 |  5863 | `		goto Abort;` |
|      ! 0 |  5864 | `	}else{` |
|        - |  5865 | `		ph7_class_method *pCons;` |
|        - |  5866 | `		/* Create a new class instance */` |
|      658 |  5867 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      658 |  5868 | `		if( pNew == 0 ){` |
|      ! 0 |  5869 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5870 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5871 | `				&pClass->sName` |
|        - |  5872 | `			);` |
|      ! 0 |  5873 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5874 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5875 | `				/* Pop given arguments */` |
|      ! 0 |  5876 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5877 | `			}` |
|      ! 0 |  5878 | `			break;` |
|        - |  5879 | `		}` |
|        - |  5880 | `		/* Check if a constructor is available */` |
|      658 |  5881 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      658 |  5882 | `		if( pCons == 0 ){` |
|      544 |  5883 | `			SyString *pName = &pClass->sName;` |
|        - |  5884 | `			/* Check for a constructor with the same base class name */` |
|      544 |  5885 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      271 |  5886 | `		}` |
|      658 |  5887 | `		if( pCons ){` |
|        - |  5888 | `			/* Call the class constructor */` |
|      116 |  5889 | `			SySetReset(&aArg);` |
|      220 |  5890 | `			while( pArg < pTos ){` |
|      106 |  5891 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  5892 | `				pArg++;` |
|        2 |  5893 | `			}` |
|      116 |  5894 | `			if( pVm->bErrReport ){` |
|        - |  5895 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5896 | `				sxu32 n;` |
|       57 |  5897 | `				n = SySetUsed(&aArg);` |
|        - |  5898 | `				/* Emit a notice for missing arguments */` |
|      101 |  5899 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  5900 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  5901 | `					if( pFuncArg ){` |
|       45 |  5902 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5903 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5904 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5905 | `						}` |
|       22 |  5906 | `					}` |
|       45 |  5907 | `					n++;` |
|        1 |  5908 | `				}` |
|       28 |  5909 | `			}` |
|      116 |  5910 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5911 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  5912 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5913 | `				pNew->iRef = 1;` |
|      ! 0 |  5914 | `			}` |
|       57 |  5915 | `		}` |
|      658 |  5916 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5917 | `			/* Pop given arguments */` |
|       98 |  5918 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  5919 | `		}` |
|      658 |  5920 | `		PH7_MemObjRelease(pTos);` |
|      658 |  5921 | `		pTos->x.pOther = pNew;` |
|      658 |  5922 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5923 | `	}` |
|      658 |  5924 | `	break;` |
|        - |  5925 | `				 }` |
|        - |  5926 | `/*` |
|        - |  5927 | ` * OP_CLONE * * *` |
|        - |  5928 | ` * Perfome a clone operation.` |
|        - |  5929 | ` */` |
|       23 |  5930 | `case PH7_OP_CLONE: {` |
|        - |  5931 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5932 | `#ifdef UNTRUST` |
|        - |  5933 | `	if( pTos < pStack ){` |
|        - |  5934 | `		goto Abort;` |
|        - |  5935 | `	}` |
|        - |  5936 | `#endif` |
|        - |  5937 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5938 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5939 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5940 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5941 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5942 | `		break;` |
|        - |  5943 | `	}` |
|        - |  5944 | `	/* Point to the source */` |
|       44 |  5945 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5946 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5947 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5948 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5949 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5950 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5951 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5952 | `		break;` |
|        - |  5953 | `	}` |
|        - |  5954 | `	/* Perform the clone operation */` |
|       44 |  5955 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5956 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5957 | `	if( pClone == 0 ){` |
|      ! 0 |  5958 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5959 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5960 | `	}else{` |
|        - |  5961 | `		/* Load the cloned object */` |
|       44 |  5962 | `		pTos->x.pOther = pClone;` |
|       44 |  5963 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5964 | `	}` |
|       44 |  5965 | `	break;` |
|        - |  5966 | `				   }` |
|        - |  5967 | `/*` |
|        - |  5968 | ` * OP_SWITCH * * P3` |
|        - |  5969 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5970 | ` */` |
|       21 |  5971 | `case PH7_OP_SWITCH: {` |
|       44 |  5972 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5973 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5974 | `	ph7_value sValue,sCaseValue;` |
|        - |  5975 | `	sxu32 n,nEntry;` |
|        - |  5976 | `#ifdef UNTRUST` |
|        - |  5977 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5978 | `		goto Abort;` |
|        - |  5979 | `	}` |
|        - |  5980 | `#endif` |
|        - |  5981 | `	/* Point to the case table  */` |
|       44 |  5982 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  5983 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5984 | `	/* Select the appropriate case block to execute */` |
|       44 |  5985 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  5986 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  5987 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  5988 | `		pCase = &aCase[n];` |
|      102 |  5989 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5990 | `		/* Execute the case expression first */` |
|      102 |  5991 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5992 | `		/* Compare the two expression */` |
|      102 |  5993 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  5994 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  5995 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  5996 | `		if( rc == 0 ){` |
|        - |  5997 | `			/* Value match,jump to this block */` |
|       44 |  5998 | `			pc = pCase->nStart - 1;` |
|       44 |  5999 | `			break;` |
|        - |  6000 | `		}` |
|       31 |  6001 | `	}` |
|       44 |  6002 | `	VmPopOperand(&pTos,1);` |
|       44 |  6003 | `	if( n >= nEntry ){` |
|        - |  6004 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6005 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6006 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6007 | `		}else{` |
|        - |  6008 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6009 | `			pc = pSwitch->nOut - 1;` |
|        - |  6010 | `		}` |
|      ! 0 |  6011 | `	}` |
|       44 |  6012 | `	break;` |
|        - |  6013 | `					}` |
|        - |  6014 | `/*` |
|        - |  6015 | ` * OP_YIELD P1 P2 *` |
|        - |  6016 | ` *  Yield a value from a generator function.` |
|        - |  6017 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6018 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6019 | ` */` |
|       28 |  6020 | `case PH7_OP_YIELD: {` |
|        - |  6021 | `	ph7_generator *pGen;` |
|       58 |  6022 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6023 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6024 | `		goto Abort;` |
|        - |  6025 | `	}` |
|       58 |  6026 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6027 | `	if( pInstr->iP2 ){` |
|        - |  6028 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6029 | `#ifdef UNTRUST` |
|        - |  6030 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6031 | `#endif` |
|        7 |  6032 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6033 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6034 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6035 | `		VmPopOperand(&pTos, 1);` |
|        - |  6036 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6037 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6038 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6039 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6040 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6041 | `			}` |
|        1 |  6042 | `		}` |
|       55 |  6043 | `	}else if( pInstr->iP1 ){` |
|        - |  6044 | `		/* yield $value */` |
|        - |  6045 | `#ifdef UNTRUST` |
|        - |  6046 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6047 | `#endif` |
|       52 |  6048 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6049 | `		VmPopOperand(&pTos, 1);` |
|        - |  6050 | `		/* Auto-increment key */` |
|       52 |  6051 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6052 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6053 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6054 | `	}else{` |
|        - |  6055 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6056 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6057 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6058 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6059 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6060 | `	}` |
|        - |  6061 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6062 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6063 | `	goto Suspend;` |
|        - |  6064 |  |
|        - |  6065 | `/*` |
|        - |  6066 | ` * OP_CALL P1 * *` |
|        - |  6067 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6068 | ` *  function on the stack.` |
|        - |  6069 | ` */` |
|   295539 |  6070 | `case PH7_OP_CALL: {` |
|   591124 |  6071 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6072 | `	ph7_value *pArg;` |
|   591124 |  6073 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   591124 |  6074 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6075 | `	SyHashEntry *pEntry;` |
|        - |  6076 | `	SyString sName;` |
|        - |  6077 | `	/* Extract function name */` |
|   591124 |  6078 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6079 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6080 | `			ph7_value sResult;` |
|      ! 0 |  6081 | `			SySetReset(&aArg);` |
|      ! 0 |  6082 | `			while( pArg < pTos ){` |
|      ! 0 |  6083 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6084 | `				pArg++;` |
|      ! 0 |  6085 | `			}` |
|      ! 0 |  6086 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6087 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6088 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6089 | `			SySetReset(&aArg);` |
|        - |  6090 | `			/* Pop given arguments */` |
|      ! 0 |  6091 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6092 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6093 | `			}` |
|        - |  6094 | `			/* Copy result */` |
|      ! 0 |  6095 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6096 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6097 | `		}else{` |
|        3 |  6098 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6099 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6100 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6101 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6102 | `			}else{` |
|        - |  6103 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6104 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6105 | `			}` |
|        - |  6106 | `			/* Pop given arguments */` |
|        3 |  6107 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6108 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6109 | `			}` |
|        - |  6110 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6111 | `			PH7_MemObjRelease(pTos);` |
|        - |  6112 | `		}` |
|   295266 |  6113 | `		break;` |
|        - |  6114 | `	}` |
|   591122 |  6115 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6116 | `	/* Check for a compiled function first.` |
|        - |  6117 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6118 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   591122 |  6119 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6120 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6121 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6122 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6123 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6124 | `	 * function calls inside namespaces. */` |
|   591122 |  6125 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6126 | `		const char *zFunc;` |
|        - |  6127 | `		const char *zEnd;` |
|        - |  6128 | `		const char *z;` |
|        - |  6129 | `		SyString sGlobal;` |
|       15 |  6130 | `		zFunc = sName.zString;` |
|       15 |  6131 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6132 | `		z = zEnd;` |
|        - |  6133 | `		/* Find last namespace separator */` |
|      133 |  6134 | `		while( z > zFunc ){` |
|      133 |  6135 | `			if( z[-1] == '\\' ){` |
|       15 |  6136 | `				break;` |
|        - |  6137 | `			}` |
|      119 |  6138 | `			z--;` |
|        1 |  6139 | `		}` |
|       15 |  6140 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6141 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6142 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6143 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6144 | `		}` |
|        7 |  6145 | `	}` |
|   591122 |  6146 | `	if( pEntry ){` |
|        - |  6147 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6148 | `		ph7_class_instance *pThis;` |
|        - |  6149 | `		ph7_value *pFrameStack;` |
|        - |  6150 | `		ph7_vm_func *pVmFunc;` |
|        - |  6151 | `		ph7_class *pSelf;` |
|        - |  6152 | `		VmFrame *pFrame;` |
|        - |  6153 | `		ph7_value *pObj;` |
|        - |  6154 | `		VmSlot sArg;` |
|        - |  6155 | `		sxu32 n;` |
|        - |  6156 | `		/* initialize fields */` |
|    13300 |  6157 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13300 |  6158 | `		pThis = 0;` |
|    13300 |  6159 | `		pSelf = 0;` |
|    13300 |  6160 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6161 | `			ph7_class_method *pMeth;` |
|        - |  6162 | `			/* Class method call */` |
|     1994 |  6163 | `			ph7_value *pTarget = &pTos[-1];` |
|     1994 |  6164 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6165 | `				/* Extract the 'this' pointer */` |
|     1994 |  6166 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6167 | `					/* Instance already loaded */` |
|     1922 |  6168 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1922 |  6169 | `					pThis->iRef++;` |
|     1922 |  6170 | `					pSelf = pThis->pClass;` |
|      960 |  6171 | `				}` |
|     1994 |  6172 | `				if( pSelf == 0 ){` |
|       74 |  6173 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6174 | `						/* "Late Static Binding" class name */` |
|      101 |  6175 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6176 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6177 | `					}` |
|       74 |  6178 | `					if( pSelf == 0 ){` |
|       13 |  6179 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6180 | `					}` |
|       36 |  6181 | `				}` |
|     1994 |  6182 | `				if( pThis == 0  ){` |
|       74 |  6183 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6184 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6185 | `					if( pFrameLocal->pParent ){` |
|        - |  6186 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6187 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6188 | `						if( pThis ){` |
|       13 |  6189 | `							pThis->iRef++;` |
|        6 |  6190 | `						}` |
|       28 |  6191 | `					}` |
|       36 |  6192 | `				}` |
|     1994 |  6193 | `				VmPopOperand(&pTos,1);` |
|     1994 |  6194 | `				PH7_MemObjRelease(pTos);` |
|        - |  6195 | `				/* Synchronize pointers */` |
|     1994 |  6196 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6197 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6198 | `				 * user have already computed the random generated unique class method name` |
|        - |  6199 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6200 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6201 | `				 */` |
|     1994 |  6202 | `				while( pArg < pStack ){` |
|      ! 0 |  6203 | `					pArg++;` |
|      ! 0 |  6204 | `				}` |
|     1994 |  6205 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6206 | `					/* Check if the call is allowed */` |
|     1994 |  6207 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1994 |  6208 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6209 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6210 | `							/* Pop given arguments */` |
|      ! 0 |  6211 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6212 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6213 | `							}` |
|        - |  6214 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6215 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6216 | `							break;` |
|        - |  6217 | `						}` |
|        3 |  6218 | `					}` |
|      996 |  6219 | `				}` |
|      996 |  6220 | `			}` |
|      996 |  6221 | `		}` |
|        - |  6222 | `		/* Check The recursion limit */` |
|    13300 |  6223 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6224 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6225 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6226 | `				&pVmFunc->sName);` |
|        - |  6227 | `			/* Pop given arguments */` |
|        3 |  6228 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6229 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6230 | `			}` |
|        - |  6231 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6232 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6233 | `			break;` |
|        - |  6234 | `		}` |
|    13298 |  6235 | `		if( pVmFunc->pNextName ){` |
|        - |  6236 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6237 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6238 | `		}` |
|    13298 |  6239 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6240 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6241 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6242 | `			ph7_generator *pGenerator;` |
|        - |  6243 | `			ph7_class_instance *pGenObj;` |
|        - |  6244 | `			ph7_value *pCtxAttr;` |
|        - |  6245 | `			SyString sAttrName;` |
|        - |  6246 | `			ph7_value **apCallArgs;` |
|        - |  6247 | `			int nGenArgs, iArg;` |
|        - |  6248 | `			/* Collect arguments from the operand stack */` |
|       20 |  6249 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6250 | `			apCallArgs = 0;` |
|       20 |  6251 | `			if( nGenArgs > 0 ){` |
|        8 |  6252 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6253 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6254 | `				if( apCallArgs == 0 ){` |
|        - |  6255 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6256 | `					nGenArgs = 0;` |
|      ! 0 |  6257 | `				}else{` |
|       12 |  6258 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6259 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6260 | `					}` |
|        - |  6261 | `				}` |
|        2 |  6262 | `			}` |
|        - |  6263 | `			/* Create execution context and generator wrapper */` |
|       20 |  6264 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6265 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6266 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6267 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6268 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6269 | `				break;` |
|        - |  6270 | `			}` |
|       20 |  6271 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6272 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6273 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6274 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6275 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6276 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6277 | `				break;` |
|        - |  6278 | `			}` |
|        - |  6279 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6280 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6281 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6282 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6283 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6284 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6285 | `			if( apCallArgs ){` |
|        6 |  6286 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6287 | `			}` |
|       20 |  6288 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6289 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6290 | `				if( pThis ){` |
|      ! 0 |  6291 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6292 | `				}` |
|      ! 0 |  6293 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6294 | `					goto Abort;` |
|        - |  6295 | `				}` |
|      ! 0 |  6296 | `				break;` |
|        - |  6297 | `			}` |
|        - |  6298 | `			/* Create Generator class instance */` |
|       20 |  6299 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6300 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6301 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6302 | `				break;` |
|        - |  6303 | `			}` |
|        - |  6304 | `			/* Store generator in __ctx attribute */` |
|       20 |  6305 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6306 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6307 | `			if( pCtxAttr ){` |
|       20 |  6308 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6309 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6310 | `			}` |
|        - |  6311 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6312 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6313 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6314 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6315 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6316 | `			pGenObj->iRef++;` |
|       20 |  6317 | `			if( pThis ){` |
|      ! 0 |  6318 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6319 | `			}` |
|       20 |  6320 | `			break;` |
|        - |  6321 | `		}` |
|        - |  6322 | `		/* Extract the formal argument set */` |
|    13280 |  6323 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6324 | `		/* Create a new VM frame  */` |
|    13280 |  6325 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13280 |  6326 | `		if( rc != SXRET_OK ){` |
|        - |  6327 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6328 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6329 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6330 | `				&pVmFunc->sName);` |
|        - |  6331 | `			/* Pop given arguments */` |
|      ! 0 |  6332 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6333 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6334 | `			}` |
|        - |  6335 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6336 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6337 | `			break;` |
|        - |  6338 | `		}` |
|    13280 |  6339 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6340 | `			/* Install the '$this' variable */` |
|        - |  6341 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1932 |  6342 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1932 |  6343 | `			if( pObj ){` |
|        - |  6344 | `				/* Reflect the change */` |
|     1932 |  6345 | `				pObj->x.pOther = pThis;` |
|     1932 |  6346 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      965 |  6347 | `			}` |
|      965 |  6348 | `		}` |
|    13280 |  6349 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6350 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6351 | `			/* Install static variables */` |
|      ! 0 |  6352 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6353 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6354 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6355 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6356 | `					/* Initialize the static variables */` |
|      ! 0 |  6357 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6358 | `					if( pObj ){` |
|        - |  6359 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6360 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6361 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6362 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6363 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6364 | `						}` |
|      ! 0 |  6365 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6366 | `					}else{` |
|      ! 0 |  6367 | `						continue;` |
|        - |  6368 | `					}` |
|      ! 0 |  6369 | `				}` |
|        - |  6370 | `				/* Install in the current frame */` |
|      ! 0 |  6371 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6372 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6373 | `			}` |
|      ! 0 |  6374 | `		}` |
|        - |  6375 | `		/* Push arguments in the local frame */` |
|    13280 |  6376 | `		n = 0;` |
|    35982 |  6377 | `		while( pArg < pTos ){` |
|    22724 |  6378 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6379 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6380 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6381 | `				if( pObj ){` |
|        - |  6382 | `					/* Initialize as empty array */` |
|       21 |  6383 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6384 | `					{` |
|       21 |  6385 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6386 | `						while( pArg < pTos ){` |
|        - |  6387 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6388 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6389 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6390 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6391 | `								if( xCast ){` |
|       13 |  6392 | `									xCast(pArg);` |
|        6 |  6393 | `								}` |
|        6 |  6394 | `							}` |
|       63 |  6395 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6396 | `							pArg++;` |
|        1 |  6397 | `						}` |
|        - |  6398 | `					}` |
|       21 |  6399 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6400 | `					sArg.pUserData = 0;` |
|       21 |  6401 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6402 | `				}` |
|       21 |  6403 | `				break; /* All remaining args consumed */` |
|        - |  6404 | `			}` |
|    22704 |  6405 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22548 |  6406 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6407 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6408 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6409 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6410 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6411 | `						goto Abort;` |
|        - |  6412 | `					}` |
|      ! 0 |  6413 | `				}` |
|        - |  6414 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6415 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22560 |  6416 | `				if( aFormalArg[n].nType > 0` |
|    11851 |  6417 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1140 |  6418 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6419 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6420 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6421 | `						ph7_class *pClass;` |
|        - |  6422 | `						/* Try to extract the desired class */` |
|        5 |  6423 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6424 | `						if( pClass ){` |
|        5 |  6425 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6426 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6427 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6428 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6429 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6430 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6431 | `								}` |
|      ! 0 |  6432 | `							}else{` |
|        - |  6433 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6434 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6435 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6436 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6437 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6438 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6439 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6440 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6441 | `								}` |
|        - |  6442 | `							}` |
|        3 |  6443 | `						}` |
|     1138 |  6444 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6445 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6446 | `						/* Cast to the desired type */` |
|      ! 0 |  6447 | `						xCast(pArg);` |
|      ! 0 |  6448 | `					}` |
|      569 |  6449 | `				}` |
|    22550 |  6450 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6451 | `					/* Pass by reference */` |
|       54 |  6452 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6453 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6454 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6455 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6456 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6457 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6458 | `						}` |
|        - |  6459 | `						/* Switch to pass by value */` |
|      ! 0 |  6460 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6461 | `					}else{` |
|        - |  6462 | `						SyHashEntry *pRefEntry;` |
|        - |  6463 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6464 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6465 | `						if( pRefEntry == 0 ){` |
|       80 |  6466 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6467 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6468 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6469 | `							sArg.pUserData = 0;` |
|       54 |  6470 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6471 | `						}` |
|       54 |  6472 | `						pObj = 0;` |
|        - |  6473 | `					}` |
|       28 |  6474 | `				}else{` |
|        - |  6475 | `					/* Pass by value,make a copy of the given argument */` |
|    22498 |  6476 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6477 | `				}` |
|    11276 |  6478 | `			}else{` |
|        - |  6479 | `				char zName[32];` |
|        - |  6480 | `				SyString sArgName;` |
|        - |  6481 | `				/* Set a dummy name */` |
|      156 |  6482 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6483 | `				sArgName.zString = zName;` |
|        - |  6484 | `				/* Annonymous argument */` |
|      156 |  6485 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6486 | `			}` |
|    22704 |  6487 | `			if( pObj ){` |
|    22652 |  6488 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6489 | `				/* Insert argument index  */` |
|    22652 |  6490 | `				sArg.nIdx = pObj->nIdx;` |
|    22652 |  6491 | `				sArg.pUserData = 0;` |
|    22652 |  6492 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11325 |  6493 | `			}` |
|    22704 |  6494 | `			PH7_MemObjRelease(pArg);` |
|    22704 |  6495 | `			pArg++;` |
|    22704 |  6496 | `			++n;` |
|        2 |  6497 | `		}` |
|        - |  6498 | `		/* Set up closure environment */` |
|    13280 |  6499 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6500 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6501 | `			ph7_value *pValue;` |
|        - |  6502 | `			sxu32 iEnv;` |
|       11 |  6503 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6504 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6505 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6506 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6507 | `					/* Do not install null value */` |
|       11 |  6508 | `					continue;` |
|        - |  6509 | `				}` |
|       11 |  6510 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6511 | `				if( pValue == 0 ){` |
|      ! 0 |  6512 | `					continue;` |
|        - |  6513 | `				}` |
|        - |  6514 | `				/* Invalidate any prior representation */` |
|       11 |  6515 | `				PH7_MemObjRelease(pValue);` |
|        - |  6516 | `				/* Duplicate bound variable value */` |
|       11 |  6517 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6518 | `			}` |
|        5 |  6519 | `		}` |
|        - |  6520 | `		/* Process default values for remaining formal parameters */` |
|    15230 |  6521 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1978 |  6522 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6523 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6524 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6525 | `				if( pObj ){` |
|       27 |  6526 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6527 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6528 | `					sArg.pUserData = 0;` |
|       27 |  6529 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6530 | `				}` |
|       27 |  6531 | `				n++;` |
|       27 |  6532 | `				break; /* Variadic is always last */` |
|        - |  6533 | `			}` |
|     1952 |  6534 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1946 |  6535 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1946 |  6536 | `				if( pObj ){` |
|        - |  6537 | `					/* Evaluate the default value and extract it's result */` |
|     1946 |  6538 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1946 |  6539 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6540 | `						goto Abort;` |
|        - |  6541 | `					}` |
|        - |  6542 | `					/* Insert argument index */` |
|     1946 |  6543 | `					sArg.nIdx = pObj->nIdx;` |
|     1946 |  6544 | `					sArg.pUserData = 0;` |
|     1946 |  6545 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6546 | `					/* Make sure the default argument is of the correct type */` |
|     1946 |  6547 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6548 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6549 | `						/* Cast to the desired type */` |
|      ! 0 |  6550 | `						xCast(pObj);` |
|      ! 0 |  6551 | `					}` |
|      972 |  6552 | `				}` |
|      972 |  6553 | `			}` |
|     1952 |  6554 | `			++n;` |
|        2 |  6555 | `		}` |
|        - |  6556 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6557 | `		 * does not return anything.` |
|        - |  6558 | `		 */` |
|    13280 |  6559 | `		PH7_MemObjRelease(pTos);` |
|    13280 |  6560 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6561 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13280 |  6562 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13280 |  6563 | `		if( pFrameStack == 0 ){` |
|        - |  6564 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6565 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6566 | `				&pVmFunc->sName);` |
|      ! 0 |  6567 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6568 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6569 | `			}` |
|      ! 0 |  6570 | `			break;` |
|        - |  6571 | `		}` |
|    13280 |  6572 | `		if( pSelf ){` |
|        - |  6573 | `			/* Push class name */` |
|     1992 |  6574 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      995 |  6575 | `		}` |
|        - |  6576 | `		/* Increment nesting level */` |
|    13280 |  6577 | `		pVm->nRecursionDepth++;` |
|        - |  6578 | `		/* Execute function body */` |
|    13280 |  6579 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6580 | `		/* Decrement nesting level */` |
|    13280 |  6581 | `		pVm->nRecursionDepth--;` |
|    13280 |  6582 | `		if( pSelf ){` |
|        - |  6583 | `			/* Pop class name */` |
|     1992 |  6584 | `			(void)SySetPop(&pVm->aSelf);` |
|      995 |  6585 | `		}` |
|        - |  6586 | `		/* Cleanup the mess left behind */` |
|    13280 |  6587 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6588 | `			/* Return by reference,reflect that */` |
|        9 |  6589 | `			if( n != SXU32_HIGH ){` |
|        9 |  6590 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6591 | `				sxu32 i;` |
|        - |  6592 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6593 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6594 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6595 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6596 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6597 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6598 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6599 | `								&pVmFunc->sName);` |
|      ! 0 |  6600 | `						}` |
|      ! 0 |  6601 | `						n = SXU32_HIGH;` |
|      ! 0 |  6602 | `						break;` |
|        - |  6603 | `					}` |
|        3 |  6604 | `				}` |
|        5 |  6605 | `			}else{` |
|      ! 0 |  6606 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6607 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6608 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6609 | `						&pVmFunc->sName);` |
|      ! 0 |  6610 | `				}` |
|        - |  6611 | `			}` |
|        9 |  6612 | `			pTos->nIdx = n;` |
|        4 |  6613 | `		}` |
|        - |  6614 | `		/* Cleanup the mess left behind */` |
|    13280 |  6615 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6616 | `			/* An exception was throw in this frame */` |
|       12 |  6617 | `			pFrame = pFrame->pParent;` |
|       12 |  6618 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6619 | `				/* Pop the resutlt */` |
|       10 |  6620 | `				VmPopOperand(&pTos,1);` |
|        - |  6621 | `				/* Jump to this destination */` |
|       10 |  6622 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6623 | `				rc = PH7_OK;` |
|        6 |  6624 | `			}else{` |
|        3 |  6625 | `				if( pFrame->pParent ){` |
|        3 |  6626 | `					rc = PH7_EXCEPTION;` |
|        2 |  6627 | `				}else{` |
|        - |  6628 | `					/* Continue normal execution */` |
|      ! 0 |  6629 | `					rc = PH7_OK;` |
|        - |  6630 | `				}` |
|        - |  6631 | `			}` |
|        5 |  6632 | `		}` |
|        - |  6633 | `		/* Free the operand stack */` |
|    13280 |  6634 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6635 | `		/* Leave the frame */` |
|    13280 |  6636 | `		VmLeaveFrame(&(*pVm));` |
|    13280 |  6637 | `		if( rc == PH7_ABORT ){` |
|        - |  6638 | `			/* Abort processing immeditaley */` |
|        7 |  6639 | `			goto Abort;` |
|    13274 |  6640 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6641 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6642 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6643 | `			 * overwriting the state saved by the inner level.` |
|        - |  6644 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6645 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6646 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6647 | `			goto Suspend;` |
|    13236 |  6648 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6649 | `			goto Exception;` |
|        - |  6650 | `		}` |
|     6618 |  6651 | `	}else{` |
|        - |  6652 | `		ph7_user_func *pFunc;` |
|        - |  6653 | `		ph7_context sCtx;` |
|        - |  6654 | `		ph7_value sRet;` |
|        - |  6655 | `		/* Look for an installed foreign function.` |
|        - |  6656 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6657 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6658 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6659 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6660 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6661 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   577824 |  6662 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   577824 |  6663 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6664 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6665 | `			const char *zShort = sName.zString;` |
|        - |  6666 | `			sxu32 i;` |
|      217 |  6667 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6668 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6669 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6670 | `				}` |
|      102 |  6671 | `			}` |
|       15 |  6672 | `			if( zShort != sName.zString ){` |
|       15 |  6673 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6674 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6675 | `			}` |
|        7 |  6676 | `		}` |
|   577824 |  6677 | `		if( pEntry == 0 ){` |
|        - |  6678 | `			/* Call to undefined function */` |
|        5 |  6679 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6680 | `			/* Pop given arguments */` |
|        5 |  6681 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6682 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6683 | `			}` |
|        - |  6684 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6685 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6686 | `			break;` |
|        - |  6687 | `		}` |
|   577820 |  6688 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6689 | `		/* Start collecting function arguments */` |
|   577820 |  6690 | `		SySetReset(&aArg);` |
|  1551394 |  6691 | `		while( pArg < pTos ){` |
|   973576 |  6692 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   973576 |  6693 | `			pArg++;` |
|        2 |  6694 | `		}` |
|        - |  6695 | `		/* Assume a null return value */` |
|   577820 |  6696 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6697 | `		/* Init the call context */` |
|   577820 |  6698 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6699 | `		/* Call the foreign function */` |
|   577820 |  6700 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6701 | `		/* Release the call context */` |
|   577820 |  6702 | `		VmReleaseCallContext(&sCtx);` |
|   577820 |  6703 | `		if( rc == PH7_ABORT ){` |
|      463 |  6704 | `			goto Abort;` |
|   577358 |  6705 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6706 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6707 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6708 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6709 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6710 | `				goto Exception;` |
|        - |  6711 | `			}` |
|        - |  6712 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6713 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6714 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6715 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6716 | `			}` |
|        - |  6717 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6718 | `			VmPopOperand(&pTos,1);` |
|        - |  6719 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6720 | `			pFrm = pVm->pFrame;` |
|        7 |  6721 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6722 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6723 | `			}` |
|        7 |  6724 | `			break;` |
|        - |  6725 | `		}` |
|   577348 |  6726 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6727 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6728 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6729 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6730 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6731 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6732 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6733 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6734 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6735 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6736 | `			}` |
|        - |  6737 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6738 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6739 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6740 | `			goto Suspend;` |
|        - |  6741 | `		}` |
|   577310 |  6742 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6743 | `			/* Pop function name and arguments */` |
|   558882 |  6744 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   279462 |  6745 | `		}` |
|        - |  6746 | `		/* Save foreign function return value */` |
|   577310 |  6747 | `		PH7_MemObjStore(&sRet,pTos);` |
|   577310 |  6748 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6749 | `	}` |
|   590542 |  6750 | `	break;` |
|        - |  6751 | `				  }` |
|        - |  6752 | `/*` |
|        - |  6753 | ` * OP_CONSUME: P1 * *` |
|        - |  6754 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6755 | ` */` |
|    11704 |  6756 | `case PH7_OP_CONSUME: {` |
|    23410 |  6757 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23410 |  6758 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6759 |  |
|    23410 |  6760 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23410 |  6761 | `	pCur = pOut;` |
|        - |  6762 | `	/* Start the consume process  */` |
|    46818 |  6763 | `	while( pOut <= pTos ){` |
|        - |  6764 | `		/* Force a string cast */` |
|    23410 |  6765 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6766 | `			PH7_MemObjToString(pOut);` |
|      149 |  6767 | `		}` |
|    23410 |  6768 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6769 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6770 | `			/* Invoke the output consumer callback */` |
|    13074 |  6771 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13074 |  6772 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13074 |  6773 | `			SyBlobRelease(&pOut->sBlob);` |
|    13074 |  6774 | `			if( rc == SXERR_ABORT ){` |
|        - |  6775 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6776 | `				goto Abort;` |
|        - |  6777 | `			}` |
|     6536 |  6778 | `		}` |
|    23410 |  6779 | `		pOut++;` |
|        2 |  6780 | `	}` |
|    23410 |  6781 | `	pTos = &pCur[-1];` |
|    23408 |  6782 | `	break;` |
|        - |  6783 | `					 }` |
|        - |  6784 |  |
|        - |  6785 | `		} /* Switch() */` |
|  9976552 |  6786 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6787 | `	} /* For(;;) */` |
|    16139 |  6788 | `Done:` |
|    32280 |  6789 | `	SySetRelease(&aArg);` |
|    32280 |  6790 | `	return SXRET_OK;` |
|       66 |  6791 | `Suspend:` |
|      134 |  6792 | `	SySetRelease(&aArg);` |
|      134 |  6793 | `	return PH7_SUSPEND;` |
|      238 |  6794 | `Abort:` |
|      477 |  6795 | `	SySetRelease(&aArg);` |
|     1661 |  6796 | `	while( pTos >= pStack ){` |
|     1185 |  6797 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6798 | `		pTos--;` |
|        1 |  6799 | `	}` |
|      477 |  6800 | `	return PH7_ABORT;` |
|        3 |  6801 | `Exception:` |
|        8 |  6802 | `	SySetRelease(&aArg);` |
|       22 |  6803 | `	while( pTos >= pStack ){` |
|       16 |  6804 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6805 | `		pTos--;` |
|        2 |  6806 | `	}` |
|        8 |  6807 | `	return PH7_EXCEPTION;` |
|    16448 |  6808 |  |
|        - |  6809 | `/*` |
|        - |  6810 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6811 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6812 | ` * See block-comment on that function for additional information.` |
|        - |  6813 | ` */` |
|    15214 |  6814 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6815 |  |
|        - |  6816 | `	ph7_value *pStack;` |
|        - |  6817 | `	sxi32 rc;` |
|        - |  6818 | `	/* Allocate a new operand stack */` |
|    15216 |  6819 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15216 |  6820 | `	if( pStack == 0 ){` |
|      ! 0 |  6821 | `		return SXERR_MEM;` |
|        - |  6822 | `	}` |
|        - |  6823 | `	/* Execute the program */` |
|    15216 |  6824 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6825 | `	/* Free the operand stack */` |
|    15216 |  6826 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6827 | `	/* Execution result */` |
|    15216 |  6828 | `	return rc;` |
|     7609 |  6829 |  |
|        - |  6830 | `/*` |
|        - |  6831 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6832 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6833 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6834 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6835 | ` * execution ends.` |
|        - |  6836 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6837 | ` * additional information.` |
|        - |  6838 | ` */` |
|     2308 |  6839 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6840 |  |
|        - |  6841 | `	VmShutdownCB *pEntry;` |
|        - |  6842 | `	ph7_value *apArg[10];` |
|        - |  6843 | `	sxu32 n,nEntry;` |
|        - |  6844 | `	int i;` |
|        - |  6845 | `	/* Point to the stack of registered callbacks */` |
|     2310 |  6846 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25390 |  6847 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23082 |  6848 | `		apArg[i] = 0;` |
|    11542 |  6849 | `	}` |
|     2312 |  6850 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6851 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6852 | `		if( pEntry ){` |
|        - |  6853 | `			/* Prepare callback arguments if any */` |
|        3 |  6854 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6855 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6856 | `					break;` |
|        - |  6857 | `				}` |
|      ! 0 |  6858 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6859 | `			}` |
|        - |  6860 | `			/* Invoke the callback */` |
|        3 |  6861 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6862 | `			/*` |
|        - |  6863 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6864 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6865 | `			 */` |
|        3 |  6866 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6867 | `			if( pEntry ){` |
|        3 |  6868 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6869 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6870 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6871 | `				}` |
|        1 |  6872 | `			}` |
|        1 |  6873 | `		}` |
|        2 |  6874 | `	}` |
|     2310 |  6875 | `	SySetReset(&pVm->aShutdown);` |
|     2310 |  6876 |  |
|        - |  6877 | `/*` |
|        - |  6878 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6879 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6880 | ` * See block-comment on that function for additional information.` |
|        - |  6881 | ` */` |
|     2316 |  6882 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6883 |  |
|        - |  6884 | `	/* Make sure we are ready to execute this program */` |
|     2318 |  6885 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6886 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6887 | `	}` |
|        - |  6888 | `	/* Set the execution magic number  */` |
|     2318 |  6889 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6890 | `	/* Execute the program */` |
|     2318 |  6891 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6892 | `	/* Invoke any shutdown callbacks */` |
|     2314 |  6893 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6894 | `	/*` |
|        - |  6895 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6896 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6897 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6898 | `	 */` |
|     2314 |  6899 | `	return SXRET_OK;` |
|     1160 |  6900 |  |
|        - |  6901 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6902 | `/*` |
|        - |  6903 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6904 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6905 | ` */` |
|       42 |  6906 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  6907 |  |
|        - |  6908 | `	ph7_exec_ctx *pCtx;` |
|        - |  6909 | `	ph7_value *pStack;` |
|        - |  6910 | `	VmFrame *pFrame;` |
|       44 |  6911 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  6912 | `	if( pCtx == 0 ){` |
|      ! 0 |  6913 | `		return 0;` |
|        - |  6914 | `	}` |
|       44 |  6915 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  6916 | `	pCtx->pVm = pVm;` |
|       44 |  6917 | `	pCtx->pFunc = pFunc;` |
|       44 |  6918 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  6919 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  6920 | `	pCtx->pc = 0;` |
|       44 |  6921 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  6922 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6923 | `	/* Allocate a private operand stack */` |
|       44 |  6924 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  6925 | `	if( pStack == 0 ){` |
|      ! 0 |  6926 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6927 | `		return 0;` |
|        - |  6928 | `	}` |
|       44 |  6929 | `	pCtx->pStack = pStack;` |
|        - |  6930 | `	/* Create a detached frame for the fiber */` |
|       44 |  6931 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  6932 | `	if( pFrame == 0 ){` |
|      ! 0 |  6933 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6934 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6935 | `		return 0;` |
|        - |  6936 | `	}` |
|       44 |  6937 | `	pCtx->pFrame = pFrame;` |
|       44 |  6938 | `	return pCtx;` |
|       23 |  6939 |  |
|        - |  6940 | `/*` |
|        - |  6941 | ` * Start executing a fiber context for the first time.` |
|        - |  6942 | ` */` |
|       42 |  6943 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  6944 |  |
|        - |  6945 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6946 | `	sxi32 rc;` |
|       44 |  6947 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6948 | `		return SXERR_INVALID;` |
|        - |  6949 | `	}` |
|        - |  6950 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  6951 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  6952 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6953 | `	/* Save and set the active context */` |
|       44 |  6954 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  6955 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  6956 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  6957 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  6958 | `	pVm->nRecursionDepth++;` |
|        - |  6959 | `	/* Execute from the beginning */` |
|       65 |  6960 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6961 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  6962 | `	pVm->nRecursionDepth--;` |
|        - |  6963 | `	/* Restore the previous context */` |
|       44 |  6964 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  6965 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6966 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  6967 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  6968 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  6969 | `		if( pResult ){` |
|       24 |  6970 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6971 | `		}` |
|       42 |  6972 | `		return SXRET_OK;` |
|        - |  6973 | `	}` |
|        - |  6974 | `	/* Detach frame */` |
|        3 |  6975 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6976 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6977 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6978 | `	}` |
|        3 |  6979 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6980 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6981 | `		return PH7_ABORT;` |
|        - |  6982 | `	}` |
|        3 |  6983 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6984 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6985 | `		return PH7_EXCEPTION;` |
|        - |  6986 | `	}` |
|        - |  6987 | `	/* Normal completion */` |
|        3 |  6988 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6989 | `	if( pResult ){` |
|        3 |  6990 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6991 | `	}` |
|        3 |  6992 | `	return SXRET_OK;` |
|       23 |  6993 |  |
|        - |  6994 | `/*` |
|        - |  6995 | ` * Resume a suspended fiber context.` |
|        - |  6996 | ` */` |
|       86 |  6997 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  6998 |  |
|        - |  6999 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7000 | `	sxi32 rc;` |
|       88 |  7001 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7002 | `		return SXERR_INVALID;` |
|        - |  7003 | `	}` |
|        - |  7004 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7005 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7006 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7007 | `	if( pResumeValue ){` |
|       40 |  7008 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7009 | `	}else{` |
|       50 |  7010 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7011 | `	}` |
|       88 |  7012 | `	pCtx->nTos++;` |
|        - |  7013 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7014 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7015 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7016 | `	/* Save and set the active context */` |
|       88 |  7017 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7018 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7019 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7020 | `	pVm->nRecursionDepth++;` |
|        - |  7021 | `	/* Resume execution from saved PC */` |
|      131 |  7022 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7023 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7024 | `	pVm->nRecursionDepth--;` |
|        - |  7025 | `	/* Restore the previous context */` |
|       88 |  7026 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7027 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7028 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7029 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7030 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7031 | `		if( pResult ){` |
|       18 |  7032 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7033 | `		}` |
|       56 |  7034 | `		return SXRET_OK;` |
|        - |  7035 | `	}` |
|        - |  7036 | `	/* Detach frame */` |
|       34 |  7037 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7038 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7039 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7040 | `	}` |
|       34 |  7041 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7042 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7043 | `		return PH7_ABORT;` |
|        - |  7044 | `	}` |
|       34 |  7045 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7046 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7047 | `		return PH7_EXCEPTION;` |
|        - |  7048 | `	}` |
|        - |  7049 | `	/* Normal completion */` |
|       34 |  7050 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7051 | `	if( pResult ){` |
|       20 |  7052 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7053 | `	}` |
|       34 |  7054 | `	return SXRET_OK;` |
|       45 |  7055 |  |
|        - |  7056 | `/*` |
|        - |  7057 | ` * Release an execution context and all its resources.` |
|        - |  7058 | ` */` |
|        4 |  7059 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7060 |  |
|        5 |  7061 | `	if( pCtx == 0 ){` |
|      ! 0 |  7062 | `		return;` |
|        - |  7063 | `	}` |
|        5 |  7064 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7065 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7066 | `		return;` |
|        - |  7067 | `	}` |
|        5 |  7068 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7069 | `	/* Release values */` |
|        5 |  7070 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7071 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7072 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7073 | `	if( pCtx->pFrame ){` |
|        - |  7074 | `		VmSlot *aSlot;` |
|        - |  7075 | `		sxu32 n;` |
|        - |  7076 | `		/* Free local variables */` |
|        5 |  7077 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7078 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7079 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7080 | `		}` |
|        - |  7081 | `		/* Remove local references */` |
|        5 |  7082 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7083 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7084 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7085 | `		}` |
|        5 |  7086 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7087 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7088 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7089 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7090 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7091 | `		pCtx->pFrame = 0;` |
|        2 |  7092 | `	}` |
|        - |  7093 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7094 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7095 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7096 | `	if( pCtx->pStack ){` |
|        5 |  7097 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7098 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7099 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7100 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7101 | `				pTos--;` |
|        1 |  7102 | `			}` |
|        2 |  7103 | `		}` |
|        5 |  7104 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7105 | `		pCtx->pStack = 0;` |
|        2 |  7106 | `	}` |
|        - |  7107 | `	/* Free the context itself */` |
|        5 |  7108 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7109 |  |
|        - |  7110 | `/*` |
|        - |  7111 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7112 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7113 | ` */` |
|       90 |  7114 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7115 |  |
|        - |  7116 | `	ph7_class_instance *pThis;` |
|        - |  7117 | `	SyString sAttr;` |
|        - |  7118 | `	ph7_value *pAttr;` |
|       92 |  7119 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7120 | `		return 0;` |
|        - |  7121 | `	}` |
|       92 |  7122 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7123 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7124 | `		return 0;` |
|        - |  7125 | `	}` |
|       92 |  7126 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7127 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7128 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7129 | `		return 0;` |
|        - |  7130 | `	}` |
|       62 |  7131 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7132 |  |
|        - |  7133 | `/*` |
|        - |  7134 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7135 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7136 | ` */` |
|       38 |  7137 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7138 |  |
|       40 |  7139 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7140 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7141 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7142 | `			"Cannot suspend outside of a fiber");` |
|        - |  7143 | `	}` |
|       40 |  7144 | `	if( nArg > 0 ){` |
|       40 |  7145 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7146 | `	}else{` |
|      ! 0 |  7147 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7148 | `	}` |
|       40 |  7149 | `	return PH7_SUSPEND;` |
|       21 |  7150 |  |
|        - |  7151 | `/*` |
|        - |  7152 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7153 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7154 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7155 | ` */` |
|       24 |  7156 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7157 |  |
|        - |  7158 | `	ph7_class_instance *pThis;` |
|        - |  7159 | `	ph7_value *pAttr;` |
|        - |  7160 | `	SyString sAttrName;` |
|       26 |  7161 | `	if( nArg < 2 ){` |
|      ! 0 |  7162 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7163 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7164 | `	}` |
|       26 |  7165 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7166 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7167 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7168 | `	}` |
|       26 |  7169 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7170 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7171 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7172 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7173 | `	}` |
|        - |  7174 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7175 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7176 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7177 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7178 | `	}` |
|        - |  7179 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7180 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7181 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7182 | `	if( pAttr ){` |
|       26 |  7183 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7184 | `	}` |
|       26 |  7185 | `	return PH7_OK;` |
|       14 |  7186 |  |
|        - |  7187 | `/*` |
|        - |  7188 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7189 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7190 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7191 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7192 | ` */` |
|       24 |  7193 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7194 | `	ph7_class_instance **ppThis)` |
|        2 |  7195 |  |
|       26 |  7196 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7197 | `	ph7_value *pCallable;` |
|        - |  7198 | `	SyString sAttrName;` |
|       26 |  7199 | `	*ppThis = 0;` |
|       26 |  7200 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7201 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7202 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7203 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7204 | `		return 0;` |
|        - |  7205 | `	}` |
|       26 |  7206 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7207 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7208 | `		SyString sName;` |
|        - |  7209 | `		SyHashEntry *pEntry;` |
|        - |  7210 | `		ph7_vm_func *pFunc;` |
|       26 |  7211 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7212 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7213 | `		if( pEntry == 0 ){` |
|      ! 0 |  7214 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7215 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7216 | `			return 0;` |
|        - |  7217 | `		}` |
|       26 |  7218 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7219 | `		return pFunc;` |
|      ! 0 |  7220 | `	}else{` |
|        - |  7221 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7222 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7223 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7224 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7225 | `		if( pMethod == 0 ){` |
|      ! 0 |  7226 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7227 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7228 | `			return 0;` |
|        - |  7229 | `		}` |
|      ! 0 |  7230 | `		*ppThis = pClosure;` |
|      ! 0 |  7231 | `		return &pMethod->sFunc;` |
|        - |  7232 | `	}` |
|       14 |  7233 |  |
|        - |  7234 | `/*` |
|        - |  7235 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7236 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7237 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7238 | ` */` |
|       42 |  7239 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7240 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7241 |  |
|       44 |  7242 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7243 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7244 | `	sxu32 nFormal, n;` |
|        - |  7245 | `	VmSlot sSlot;` |
|        - |  7246 | `	sxi32 rc;` |
|        - |  7247 | `	/* Install $this for closure/method callables */` |
|       44 |  7248 | `	if( pClosureThis ){` |
|        - |  7249 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7250 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7251 | `		if( pObj ){` |
|      ! 0 |  7252 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7253 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7254 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7255 | `		}` |
|      ! 0 |  7256 | `	}` |
|        - |  7257 | `	/* Install static variables */` |
|       44 |  7258 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7259 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7260 | `		ph7_value *pVal;` |
|      ! 0 |  7261 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7262 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7263 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7264 | `			if( pVal ){` |
|      ! 0 |  7265 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7266 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7267 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7268 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7269 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7270 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7271 | `				}` |
|      ! 0 |  7272 | `			}` |
|      ! 0 |  7273 | `		}` |
|      ! 0 |  7274 | `	}` |
|        - |  7275 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7276 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7277 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7278 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7279 | `		ph7_value *pObj;` |
|       12 |  7280 | `		if( n < (sxu32)nArg ){` |
|        - |  7281 | `			/* Argument provided — install with type casting */` |
|       12 |  7282 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7283 | `			if( pObj ){` |
|       12 |  7284 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7285 | `				/* Type casting */` |
|       12 |  7286 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7287 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7288 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7289 | `						if( xCast ){` |
|      ! 0 |  7290 | `							xCast(pObj);` |
|      ! 0 |  7291 | `						}` |
|      ! 0 |  7292 | `					}` |
|      ! 0 |  7293 | `				}` |
|       12 |  7294 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7295 | `				sSlot.pUserData = 0;` |
|       12 |  7296 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7297 | `			}` |
|        5 |  7298 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7299 | `			/* Default value */` |
|      ! 0 |  7300 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7301 | `			if( pObj ){` |
|      ! 0 |  7302 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7303 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7304 | `					return rc;` |
|        - |  7305 | `				}` |
|      ! 0 |  7306 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7307 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7308 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7309 | `						if( xCast ){` |
|      ! 0 |  7310 | `							xCast(pObj);` |
|      ! 0 |  7311 | `						}` |
|      ! 0 |  7312 | `					}` |
|      ! 0 |  7313 | `				}` |
|      ! 0 |  7314 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7315 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7316 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7317 | `			}` |
|      ! 0 |  7318 | `		}` |
|        7 |  7319 | `	}` |
|        - |  7320 | `	/* Install closure environment (captured variables) */` |
|       44 |  7321 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7322 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7323 | `		ph7_value *pValue;` |
|        - |  7324 | `		sxu32 iEnv;` |
|        3 |  7325 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7326 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7327 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7328 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7329 | `				continue;` |
|        - |  7330 | `			}` |
|        5 |  7331 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7332 | `			if( pValue == 0 ){` |
|      ! 0 |  7333 | `				continue;` |
|        - |  7334 | `			}` |
|        5 |  7335 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7336 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7337 | `		}` |
|        1 |  7338 | `	}` |
|       44 |  7339 | `	return SXRET_OK;` |
|       23 |  7340 |  |
|        - |  7341 | `/*` |
|        - |  7342 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7343 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7344 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7345 | ` */` |
|       26 |  7346 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7347 |  |
|       28 |  7348 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7349 | `	ph7_class_instance *pThis;` |
|        - |  7350 | `	ph7_class_instance *pClosureThis;` |
|        - |  7351 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7352 | `	ph7_vm_func *pFunc;` |
|        - |  7353 | `	ph7_value sResult;` |
|        - |  7354 | `	ph7_value *pCtxAttr;` |
|        - |  7355 | `	SyString sAttrName;` |
|        - |  7356 | `	sxi32 rc;` |
|       28 |  7357 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7358 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7359 | `	}` |
|       28 |  7360 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7361 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7362 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7363 | `	if( pExecCtx != 0 ){` |
|        3 |  7364 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7365 | `			"Cannot start a fiber that has already been started");` |
|        - |  7366 | `	}` |
|        - |  7367 | `	/* Resolve callable */` |
|       26 |  7368 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7369 | `	if( pFunc == 0 ){` |
|      ! 0 |  7370 | `		return PH7_EXCEPTION;` |
|        - |  7371 | `	}` |
|        - |  7372 | `	/* Create execution context now that we know the function */` |
|       26 |  7373 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7374 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7375 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7376 | `			"Fiber::start(): out of memory");` |
|        - |  7377 | `	}` |
|        - |  7378 | `	/* Store context in $this->__ctx */` |
|       26 |  7379 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7380 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7381 | `	if( pCtxAttr ){` |
|       26 |  7382 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7383 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7384 | `	}` |
|        - |  7385 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7386 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7387 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7388 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7389 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7390 | `	/* Unpack the args array and install into the frame */` |
|        - |  7391 | `	{` |
|       26 |  7392 | `		ph7_value **apValues = 0;` |
|       26 |  7393 | `		int nActual = 0;` |
|       26 |  7394 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7395 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7396 | `			ph7_hashmap_node *pNode;` |
|       26 |  7397 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7398 | `			if( nCount > 0 ){` |
|        3 |  7399 | `				sxu32 idx = 0;` |
|        4 |  7400 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7401 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7402 | `				if( apValues ){` |
|        3 |  7403 | `					pNode = pMap->pFirst;` |
|        7 |  7404 | `					while( pNode && idx < nCount ){` |
|        5 |  7405 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7406 | `						idx++;` |
|        5 |  7407 | `						pNode = pNode->pPrev;` |
|        1 |  7408 | `					}` |
|        3 |  7409 | `					nActual = (int)idx;` |
|        1 |  7410 | `				}` |
|        1 |  7411 | `			}` |
|       12 |  7412 | `		}` |
|       26 |  7413 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7414 | `		if( apValues ){` |
|        3 |  7415 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7416 | `		}` |
|        - |  7417 | `	}` |
|        - |  7418 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7419 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7420 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7421 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7422 | `		return PH7_ABORT;` |
|        - |  7423 | `	}` |
|       26 |  7424 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7425 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7426 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7427 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7428 | `		return PH7_ABORT;` |
|        - |  7429 | `	}` |
|       26 |  7430 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7431 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7432 | `		return PH7_EXCEPTION;` |
|        - |  7433 | `	}` |
|       26 |  7434 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7435 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7436 | `	return PH7_OK;` |
|       15 |  7437 |  |
|        - |  7438 | `/*` |
|        - |  7439 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7440 | ` */` |
|       36 |  7441 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7442 |  |
|       38 |  7443 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7444 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7445 | `	ph7_value sResult;` |
|        - |  7446 | `	ph7_value *pResumeVal;` |
|        - |  7447 | `	sxi32 rc;` |
|       38 |  7448 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7449 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7450 | `		return PH7_OK;` |
|        - |  7451 | `	}` |
|       38 |  7452 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7453 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7454 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7455 | `		return PH7_OK;` |
|        - |  7456 | `	}` |
|       38 |  7457 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7458 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7459 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7460 | `	}` |
|       36 |  7461 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7462 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7463 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7464 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7465 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7466 | `		return PH7_ABORT;` |
|        - |  7467 | `	}` |
|       36 |  7468 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7469 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7470 | `		return PH7_EXCEPTION;` |
|        - |  7471 | `	}` |
|       36 |  7472 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7473 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7474 | `	return PH7_OK;` |
|       20 |  7475 |  |
|        - |  7476 | `/*` |
|        - |  7477 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7478 | ` */` |
|        6 |  7479 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7480 |  |
|        8 |  7481 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7482 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7483 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7484 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7485 | `		return PH7_OK;` |
|        - |  7486 | `	}` |
|        8 |  7487 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7488 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7489 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7490 | `		return PH7_OK;` |
|        - |  7491 | `	}` |
|        8 |  7492 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7493 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7494 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7495 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7496 | `		}` |
|      ! 0 |  7497 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7498 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7499 | `	}` |
|        8 |  7500 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7501 | `	return PH7_OK;` |
|        5 |  7502 |  |
|        - |  7503 | `/*` |
|        - |  7504 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7505 | ` */` |
|        6 |  7506 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7507 |  |
|        - |  7508 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7509 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7510 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7511 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7512 | `	return PH7_OK;` |
|        4 |  7513 |  |
|      ! 0 |  7514 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7515 |  |
|        - |  7516 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7517 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7518 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7519 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7520 | `	return PH7_OK;` |
|      ! 0 |  7521 |  |
|        6 |  7522 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7523 |  |
|        - |  7524 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7525 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7526 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7527 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7528 | `	return PH7_OK;` |
|        4 |  7529 |  |
|        6 |  7530 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7531 |  |
|        - |  7532 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7533 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7534 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7535 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7536 | `	return PH7_OK;` |
|        4 |  7537 |  |
|        - |  7538 | `/*` |
|        - |  7539 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7540 | ` */` |
|        4 |  7541 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7542 |  |
|        5 |  7543 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7544 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7545 | `	if( nArg < 1 ){` |
|      ! 0 |  7546 | `		return PH7_OK;` |
|        - |  7547 | `	}` |
|        5 |  7548 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7549 | `	if( pExecCtx ){` |
|        5 |  7550 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7551 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7552 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7553 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7554 | `			SyString sAttrName;` |
|        - |  7555 | `			ph7_value *pAttr;` |
|        5 |  7556 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7557 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7558 | `			if( pAttr ){` |
|        5 |  7559 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7560 | `			}` |
|        2 |  7561 | `		}` |
|        2 |  7562 | `	}` |
|        5 |  7563 | `	return PH7_OK;` |
|        3 |  7564 |  |
|        - |  7565 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7566 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7567 |  |
|        - |  7568 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7569 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7570 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7571 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7572 |  |
|      ! 0 |  7573 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7574 |  |
|        - |  7575 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7576 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7577 | `	ph7_exec_ctx *pCtx;` |
|        - |  7578 | `	ph7_vm_func *pFunc;` |
|        - |  7579 | `	ph7_value *pCallable;` |
|        - |  7580 | `	ph7_value *pCtxAttr;` |
|        - |  7581 | `	SyString sAttrName;` |
|        - |  7582 | `	/* Must not already be started */` |
|      ! 0 |  7583 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7584 | `	if( pCtx != 0 ){` |
|      ! 0 |  7585 | `		return SXERR_INVALID;` |
|        - |  7586 | `	}` |
|      ! 0 |  7587 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7588 | `		return SXERR_INVALID;` |
|        - |  7589 | `	}` |
|      ! 0 |  7590 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7591 | `	/* Get the callable */` |
|      ! 0 |  7592 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7593 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7594 | `	if( pCallable == 0 ){` |
|      ! 0 |  7595 | `		return SXERR_INVALID;` |
|        - |  7596 | `	}` |
|        - |  7597 | `	/* Resolve callable */` |
|      ! 0 |  7598 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7599 | `		SyString sName;` |
|        - |  7600 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7601 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7602 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7603 | `		if( pEntry == 0 ){` |
|      ! 0 |  7604 | `			return SXERR_NOTFOUND;` |
|        - |  7605 | `		}` |
|      ! 0 |  7606 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7607 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7608 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7609 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7610 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7611 | `		if( pMethod == 0 ){` |
|      ! 0 |  7612 | `			return SXERR_INVALID;` |
|        - |  7613 | `		}` |
|      ! 0 |  7614 | `		pClosureThis = pClosure;` |
|      ! 0 |  7615 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7616 | `	}else{` |
|      ! 0 |  7617 | `		return SXERR_INVALID;` |
|        - |  7618 | `	}` |
|        - |  7619 | `	/* Create context */` |
|      ! 0 |  7620 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7621 | `	if( pCtx == 0 ){` |
|      ! 0 |  7622 | `		return SXERR_MEM;` |
|        - |  7623 | `	}` |
|        - |  7624 | `	/* Store in __ctx */` |
|      ! 0 |  7625 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7626 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7627 | `	if( pCtxAttr ){` |
|      ! 0 |  7628 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7629 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7630 | `	}` |
|        - |  7631 | `	/* Set up frame with args */` |
|      ! 0 |  7632 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7633 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7634 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7635 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7636 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7637 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7638 |  |
|      ! 0 |  7639 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7640 |  |
|      ! 0 |  7641 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7642 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7643 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7644 |  |
|      ! 0 |  7645 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7646 |  |
|      ! 0 |  7647 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7648 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7649 |  |
|      ! 0 |  7650 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7651 |  |
|      ! 0 |  7652 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7653 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7654 |  |
|      ! 0 |  7655 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7656 |  |
|      ! 0 |  7657 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7658 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7659 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7660 |  |
|        - |  7661 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7662 | `/*` |
|        - |  7663 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7664 | ` */` |
|       18 |  7665 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7666 |  |
|        - |  7667 | `	ph7_generator *pGen;` |
|       20 |  7668 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7669 | `	if( pGen == 0 ){` |
|      ! 0 |  7670 | `		return 0;` |
|        - |  7671 | `	}` |
|       20 |  7672 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7673 | `	pGen->pCtx = pCtx;` |
|       20 |  7674 | `	pGen->iImplicitKey = 0;` |
|       20 |  7675 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7676 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7677 | `	/* Link the generator back to the exec context */` |
|       20 |  7678 | `	pCtx->pPrivate = pGen;` |
|       20 |  7679 | `	return pGen;` |
|       11 |  7680 |  |
|        - |  7681 | `/*` |
|        - |  7682 | ` * Release a generator and its execution context.` |
|        - |  7683 | ` */` |
|      ! 0 |  7684 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7685 |  |
|      ! 0 |  7686 | `	if( pGen == 0 ){` |
|      ! 0 |  7687 | `		return;` |
|        - |  7688 | `	}` |
|      ! 0 |  7689 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7690 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7691 | `	if( pGen->pCtx ){` |
|      ! 0 |  7692 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7693 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7694 | `		pGen->pCtx = 0;` |
|      ! 0 |  7695 | `	}` |
|      ! 0 |  7696 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7697 |  |
|        - |  7698 | `/*` |
|        - |  7699 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7700 | ` */` |
|      192 |  7701 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7702 |  |
|        - |  7703 | `	ph7_class_instance *pThis;` |
|        - |  7704 | `	SyString sAttr;` |
|        - |  7705 | `	ph7_value *pAttr;` |
|      194 |  7706 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7707 | `		return 0;` |
|        - |  7708 | `	}` |
|      194 |  7709 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7710 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7711 | `		return 0;` |
|        - |  7712 | `	}` |
|      194 |  7713 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7714 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7715 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7716 | `		return 0;` |
|        - |  7717 | `	}` |
|      194 |  7718 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7719 |  |
|        - |  7720 | `/*` |
|        - |  7721 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7722 | ` */` |
|       18 |  7723 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7724 |  |
|        - |  7725 | `	ph7_generator *pGen;` |
|        - |  7726 | `	sxi32 rc;` |
|       20 |  7727 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7728 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7729 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7730 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7731 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7732 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7733 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7734 | `	}` |
|       20 |  7735 | `	return PH7_OK;` |
|       11 |  7736 |  |
|        - |  7737 | `/*` |
|        - |  7738 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7739 | ` */` |
|       52 |  7740 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7741 |  |
|        - |  7742 | `	ph7_generator *pGen;` |
|       54 |  7743 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7744 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7745 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7746 | `	return PH7_OK;` |
|       28 |  7747 |  |
|        - |  7748 | `/*` |
|        - |  7749 | ` * Generator::current() — return the last yielded value.` |
|        - |  7750 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7751 | ` */` |
|       56 |  7752 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7753 |  |
|        - |  7754 | `	ph7_generator *pGen;` |
|        - |  7755 | `	sxi32 rc;` |
|       58 |  7756 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7757 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7758 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7759 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7760 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7761 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7762 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7763 | `	}` |
|       58 |  7764 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7765 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7766 | `	}else{` |
|      ! 0 |  7767 | `		ph7_result_null(pCtx);` |
|        - |  7768 | `	}` |
|       58 |  7769 | `	return PH7_OK;` |
|       30 |  7770 |  |
|        - |  7771 | `/*` |
|        - |  7772 | ` * Generator::key() — return the last yielded key.` |
|        - |  7773 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7774 | ` */` |
|       12 |  7775 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7776 |  |
|        - |  7777 | `	ph7_generator *pGen;` |
|        - |  7778 | `	sxi32 rc;` |
|       13 |  7779 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7780 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7781 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7782 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7783 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7784 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7785 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7786 | `	}` |
|       13 |  7787 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7788 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7789 | `	}else{` |
|      ! 0 |  7790 | `		ph7_result_null(pCtx);` |
|        - |  7791 | `	}` |
|       13 |  7792 | `	return PH7_OK;` |
|        7 |  7793 |  |
|        - |  7794 | `/*` |
|        - |  7795 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7796 | ` */` |
|       48 |  7797 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7798 |  |
|        - |  7799 | `	ph7_generator *pGen;` |
|        - |  7800 | `	sxi32 rc;` |
|       50 |  7801 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7802 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7803 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7804 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7805 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7806 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7807 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7808 | `	}else{` |
|      ! 0 |  7809 | `		return PH7_OK;` |
|        - |  7810 | `	}` |
|       50 |  7811 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7812 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7813 | `	return PH7_OK;` |
|       26 |  7814 |  |
|        - |  7815 | `/*` |
|        - |  7816 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7817 | ` */` |
|        4 |  7818 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7819 |  |
|        - |  7820 | `	ph7_generator *pGen;` |
|        - |  7821 | `	ph7_value *pSendVal;` |
|        - |  7822 | `	sxi32 rc;` |
|        5 |  7823 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7824 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7825 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7826 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7827 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7828 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7829 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7830 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7831 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7832 | `	}else{` |
|      ! 0 |  7833 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7834 | `		return PH7_OK;` |
|        - |  7835 | `	}` |
|        5 |  7836 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7837 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7838 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7839 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7840 | `	}else{` |
|        3 |  7841 | `		ph7_result_null(pCtx);` |
|        - |  7842 | `	}` |
|        5 |  7843 | `	return PH7_OK;` |
|        3 |  7844 |  |
|        - |  7845 | `/*` |
|        - |  7846 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7847 | ` *` |
|        - |  7848 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7849 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7850 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7851 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7852 | ` * the exception to the caller.` |
|        - |  7853 | ` */` |
|      ! 0 |  7854 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7855 |  |
|        - |  7856 | `	ph7_generator *pGen;` |
|        - |  7857 | `	const char *zMsg;` |
|        - |  7858 | `	int nLen;` |
|      ! 0 |  7859 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7860 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7861 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7862 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7863 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7864 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7865 | `			"Cannot throw into a closed generator");` |
|        - |  7866 | `	}` |
|        - |  7867 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7868 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7869 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7870 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7871 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7872 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7873 | `	nLen = 0;` |
|      ! 0 |  7874 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7875 | `		/* Try to get the exception's message */` |
|        - |  7876 | `		SyString sAttr;` |
|        - |  7877 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7878 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7879 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7880 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7881 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7882 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7883 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7884 | `		}` |
|      ! 0 |  7885 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7886 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7887 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7888 | `	}` |
|      ! 0 |  7889 | `	(void)nLen;` |
|      ! 0 |  7890 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7891 |  |
|        - |  7892 | `/*` |
|        - |  7893 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7894 | ` */` |
|        2 |  7895 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7896 |  |
|        - |  7897 | `	ph7_generator *pGen;` |
|        3 |  7898 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7899 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7900 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7901 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7902 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7903 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7904 | `	}` |
|        3 |  7905 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7906 | `	return PH7_OK;` |
|        2 |  7907 |  |
|        - |  7908 | `/*` |
|        - |  7909 | ` * Generator::__destruct() — clean up.` |
|        - |  7910 | ` */` |
|      ! 0 |  7911 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7912 |  |
|        - |  7913 | `	ph7_generator *pGen;` |
|      ! 0 |  7914 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7915 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7916 | `	if( pGen ){` |
|      ! 0 |  7917 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7918 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7919 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7920 | `			SyString sAttrName;` |
|        - |  7921 | `			ph7_value *pAttr;` |
|      ! 0 |  7922 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7923 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7924 | `			if( pAttr ){` |
|      ! 0 |  7925 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7926 | `			}` |
|      ! 0 |  7927 | `		}` |
|      ! 0 |  7928 | `	}` |
|      ! 0 |  7929 | `	return PH7_OK;` |
|      ! 0 |  7930 |  |
|        - |  7931 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7932 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7933 | `/*` |
|        - |  7934 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7935 | ` * the desired message.` |
|        - |  7936 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7937 | ` * in 'api.c' for additional information.` |
|        - |  7938 | ` */` |
|      370 |  7939 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7940 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7941 | `	SyString *pString /* Message to output */` |
|        - |  7942 | `	)` |
|        2 |  7943 |  |
|      372 |  7944 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  7945 | `	sxi32 rc = SXRET_OK;` |
|        - |  7946 | `	/* Call the output consumer */` |
|      372 |  7947 | `	if( pString->nByte > 0 ){` |
|      372 |  7948 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  7949 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  7950 | `	}` |
|      372 |  7951 | `	return rc;` |
|        2 |  7952 |  |
|        - |  7953 | `/*` |
|        - |  7954 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7955 | ` * callback to consume the formatted message.` |
|        - |  7956 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7957 | ` * in 'api.c' for additional information.` |
|        - |  7958 | ` */` |
|        2 |  7959 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7960 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7961 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7962 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7963 | `	)` |
|        1 |  7964 |  |
|        3 |  7965 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7966 | `	sxi32 rc = SXRET_OK;` |
|        - |  7967 | `	SyBlob sWorker;` |
|        - |  7968 | `	/* Format the message and call the output consumer */` |
|        3 |  7969 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7970 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7971 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7972 | `		/* Consume the formatted message */` |
|        3 |  7973 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7974 | `	}` |
|        3 |  7975 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7976 | `	/* Release the working buffer */` |
|        3 |  7977 | `	SyBlobRelease(&sWorker);` |
|        3 |  7978 | `	return rc;` |
|        1 |  7979 |  |
|        - |  7980 | `/*` |
|        - |  7981 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7982 | ` * This function never fail and always return a pointer` |
|        - |  7983 | ` * to a null terminated string.` |
|        - |  7984 | ` */` |
|       12 |  7985 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7986 |  |
|       13 |  7987 | `	const char *zOp = "Unknown     ";` |
|       13 |  7988 | `	switch(nOp){` |
|        3 |  7989 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7990 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7991 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7992 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7993 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7994 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7995 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7996 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7997 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7998 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7999 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8000 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8001 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8002 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8003 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8004 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8005 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8006 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8007 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8008 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8009 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8010 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8011 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8012 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8013 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8014 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8015 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8016 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8017 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8018 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8019 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8020 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8021 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8022 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8023 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8024 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8025 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8026 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8027 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8028 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8029 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8030 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8031 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8032 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8033 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8034 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8035 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8036 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8037 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8038 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8039 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8040 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8041 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8042 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8043 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8044 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8045 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8046 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8047 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8048 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8049 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8050 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8051 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8052 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8053 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8054 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8055 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8056 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8057 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8058 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8059 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8060 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8061 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8062 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8063 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8064 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8065 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8066 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8067 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8068 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8069 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8070 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8071 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8072 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8073 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8074 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8075 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8076 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8077 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8078 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8079 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8080 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8081 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8082 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8083 | `	default:` |
|      ! 0 |  8084 | `		break;` |
|        - |  8085 | `	}` |
|       13 |  8086 | `	return zOp;` |
|        1 |  8087 |  |
|        - |  8088 | `/*` |
|        - |  8089 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8090 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8091 | ` * is responsible of consuming the generated dump.` |
|        - |  8092 | ` */` |
|        2 |  8093 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8094 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8095 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8096 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8097 | `	)` |
|        1 |  8098 |  |
|        - |  8099 | `	sxi32 rc;` |
|        3 |  8100 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8101 | `	return rc;` |
|        1 |  8102 |  |
|        - |  8103 | `/*` |
|        - |  8104 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8105 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8106 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8107 | ` * in 'compile.c' for additional information.` |
|        - |  8108 | ` */` |
|        8 |  8109 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8110 |  |
|        9 |  8111 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8112 | `	/* Evaluate and expand constant value */` |
|        9 |  8113 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  8114 |  |
|        - |  8115 | `/*` |
|        - |  8116 | ` * Section:` |
|        - |  8117 | ` *  Function handling functions.` |
|        - |  8118 | ` * Status:` |
|        - |  8119 | ` *    Stable.` |
|        - |  8120 | ` */` |
|        - |  8121 | `/*` |
|        - |  8122 | ` * int func_num_args(void)` |
|        - |  8123 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8124 | ` * Parameters` |
|        - |  8125 | ` *   None.` |
|        - |  8126 | ` * Return` |
|        - |  8127 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8128 | ` *  or -1 if called from the globe scope.` |
|        - |  8129 | ` */` |
|      936 |  8130 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8131 |  |
|        - |  8132 | `	VmFrame *pFrame;` |
|        - |  8133 | `	ph7_vm *pVm;` |
|        - |  8134 | `	/* Point to the target VM */` |
|      938 |  8135 | `	pVm = pCtx->pVm;` |
|        - |  8136 | `	/* Current frame */` |
|      938 |  8137 | `	pFrame = pVm->pFrame;` |
|      938 |  8138 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      938 |  8139 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8140 | `		SXUNUSED(nArg);` |
|      ! 0 |  8141 | `		SXUNUSED(apArg);` |
|        - |  8142 | `		/* Global frame,return -1 */` |
|      ! 0 |  8143 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8144 | `		return SXRET_OK;` |
|        - |  8145 | `	}` |
|        - |  8146 | `	/* Total number of arguments passed to the enclosing function */` |
|      938 |  8147 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      938 |  8148 | `	ph7_result_int(pCtx,nArg);` |
|      938 |  8149 | `	return SXRET_OK;` |
|      470 |  8150 |  |
|        - |  8151 | `/*` |
|        - |  8152 | ` * value func_get_arg(int $arg_num)` |
|        - |  8153 | ` *   Return an item from the argument list.` |
|        - |  8154 | ` * Parameters` |
|        - |  8155 | ` *  Argument number(index start from zero).` |
|        - |  8156 | ` * Return` |
|        - |  8157 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8158 | ` */` |
|       22 |  8159 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8160 |  |
|       24 |  8161 | `	ph7_value *pObj = 0;` |
|       24 |  8162 | `	VmSlot *pSlot = 0;` |
|        - |  8163 | `	VmFrame *pFrame;` |
|        - |  8164 | `	ph7_vm *pVm;` |
|        - |  8165 | `	/* Point to the target VM */` |
|       24 |  8166 | `	pVm = pCtx->pVm;` |
|        - |  8167 | `	/* Current frame */` |
|       24 |  8168 | `	pFrame = pVm->pFrame;` |
|       24 |  8169 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8170 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8171 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8172 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8173 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8174 | `		return SXRET_OK;` |
|        - |  8175 | `	}` |
|        - |  8176 | `	/* Extract the desired index */` |
|       21 |  8177 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8178 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8179 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8180 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8181 | `		return SXRET_OK;` |
|        - |  8182 | `	}` |
|        - |  8183 | `	/* Extract the desired argument */` |
|       21 |  8184 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8185 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8186 | `			/* Return the desired argument */` |
|       21 |  8187 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8188 | `		}else{` |
|        - |  8189 | `			/* No such argument,return false */` |
|      ! 0 |  8190 | `			ph7_result_bool(pCtx,0);` |
|        - |  8191 | `		}` |
|       11 |  8192 | `	}else{` |
|        - |  8193 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8194 | `		ph7_result_bool(pCtx,0);` |
|        - |  8195 | `	}` |
|       21 |  8196 | `	return SXRET_OK;` |
|       13 |  8197 |  |
|        - |  8198 | `/*` |
|        - |  8199 | ` * array func_get_args_byref(void)` |
|        - |  8200 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8201 | ` * Parameters` |
|        - |  8202 | ` *  None.` |
|        - |  8203 | ` * Return` |
|        - |  8204 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8205 | ` *  member of the current user-defined function's argument list.` |
|        - |  8206 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8207 | ` * NOTE:` |
|        - |  8208 | ` *  Arguments are returned to the array by reference.` |
|        - |  8209 | ` */` |
|        2 |  8210 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8211 |  |
|        - |  8212 | `	ph7_value *pArray;` |
|        - |  8213 | `	VmFrame *pFrame;` |
|        - |  8214 | `	VmSlot *aSlot;` |
|        - |  8215 | `	sxu32 n;` |
|        - |  8216 | `	/* Point to the current frame */` |
|        3 |  8217 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8218 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8219 | `	if( pFrame->pParent == 0 ){` |
|        - |  8220 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8221 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8222 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8223 | `		return SXRET_OK;` |
|        - |  8224 | `	}` |
|        - |  8225 | `	/* Create a new array */` |
|        3 |  8226 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8227 | `	if( pArray == 0 ){` |
|      ! 0 |  8228 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8229 | `		SXUNUSED(apArg);` |
|      ! 0 |  8230 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8231 | `		return SXRET_OK;` |
|        - |  8232 | `	}` |
|        - |  8233 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8234 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8235 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8236 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8237 | `	}` |
|        - |  8238 | `	/* Return the freshly created array */` |
|        3 |  8239 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8240 | `	return SXRET_OK;` |
|        2 |  8241 |  |
|        - |  8242 | `/*` |
|        - |  8243 | ` * array func_get_args(void)` |
|        - |  8244 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8245 | ` * Parameters` |
|        - |  8246 | ` *  None.` |
|        - |  8247 | ` * Return` |
|        - |  8248 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8249 | ` *  member of the current user-defined function's argument list.` |
|        - |  8250 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8251 | ` */` |
|       88 |  8252 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8253 |  |
|       90 |  8254 | `	ph7_value *pObj = 0;` |
|        - |  8255 | `	ph7_value *pArray;` |
|        - |  8256 | `	VmFrame *pFrame;` |
|        - |  8257 | `	VmSlot *aSlot;` |
|        - |  8258 | `	sxu32 n;` |
|        - |  8259 | `	/* Point to the current frame */` |
|       90 |  8260 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8261 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8262 | `	if( pFrame->pParent == 0 ){` |
|        - |  8263 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8264 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8265 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8266 | `		return SXRET_OK;` |
|        - |  8267 | `	}` |
|        - |  8268 | `	/* Create a new array */` |
|       90 |  8269 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8270 | `	if( pArray == 0 ){` |
|      ! 0 |  8271 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8272 | `		SXUNUSED(apArg);` |
|      ! 0 |  8273 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8274 | `		return SXRET_OK;` |
|        - |  8275 | `	}` |
|        - |  8276 | `	/* Start filling the array with the given arguments */` |
|       90 |  8277 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8278 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8279 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8280 | `		if( pObj ){` |
|      134 |  8281 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8282 | `		}` |
|       68 |  8283 | `	}` |
|        - |  8284 | `	/* Return the freshly created array */` |
|       90 |  8285 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8286 | `	return SXRET_OK;` |
|       46 |  8287 |  |
|        - |  8288 | `/*` |
|        - |  8289 | ` * bool function_exists(string $name)` |
|        - |  8290 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8291 | ` * Parameters` |
|        - |  8292 | ` *  The name of the desired function.` |
|        - |  8293 | ` * Return` |
|        - |  8294 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8295 | ` */` |
|     1682 |  8296 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8297 |  |
|        - |  8298 | `	const char *zName;` |
|        - |  8299 | `	ph7_vm *pVm;` |
|        - |  8300 | `	int nLen;` |
|        - |  8301 | `	int res;` |
|     1684 |  8302 | `	if( nArg < 1 ){` |
|        - |  8303 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8304 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8305 | `		return SXRET_OK;` |
|        - |  8306 | `	}` |
|        - |  8307 | `	/* Point to the target VM */` |
|     1684 |  8308 | `	pVm = pCtx->pVm;` |
|        - |  8309 | `	/* Extract the function name */` |
|     1684 |  8310 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8311 | `	/* Assume the function is not defined */` |
|     1684 |  8312 | `	res = 0;` |
|        - |  8313 | `	/* Perform the lookup */` |
|     2523 |  8314 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8315 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8316 | `			/* Function is defined */` |
|      206 |  8317 | `			res = 1;` |
|      102 |  8318 | `	}` |
|     1684 |  8319 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8320 | `	return SXRET_OK;` |
|      843 |  8321 |  |
|        - |  8322 | `/*` |
|        - |  8323 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8324 | ` * [i.e: Whether it is callable or not].` |
|        - |  8325 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8326 | ` */` |
|    17492 |  8327 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8328 |  |
|    17494 |  8329 | `	int res = 0;` |
|    17494 |  8330 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8331 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8332 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8333 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8334 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8335 | `		if( pMethod && CallInvoke ){` |
|        - |  8336 | `			ph7_value sResult;` |
|        - |  8337 | `			sxi32 rc;` |
|        - |  8338 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8339 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8340 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8341 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8342 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8343 | `			}` |
|      ! 0 |  8344 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8345 | `		}` |
|    17494 |  8346 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8347 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8348 | `		if( pMap->nEntry == 2 ){` |
|        - |  8349 | `			ph7_class *pClass;` |
|        - |  8350 | `			ph7_value *pV;` |
|        - |  8351 | `			/* Extract the target class */` |
|       12 |  8352 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8353 | `			if( pV ){` |
|       12 |  8354 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8355 | `				if( pClass ){` |
|        - |  8356 | `					ph7_class_method *pMethod;` |
|        - |  8357 | `					/* Extract the target method */` |
|       10 |  8358 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8359 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8360 | `						/* Perform the lookup */` |
|       10 |  8361 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8362 | `						if( pMethod ){` |
|        - |  8363 | `							/* Method is callable */` |
|        5 |  8364 | `							res = 1;` |
|        2 |  8365 | `						}` |
|        4 |  8366 | `					}` |
|        4 |  8367 | `				}` |
|        5 |  8368 | `			}` |
|        7 |  8369 | `		}` |
|    17481 |  8370 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8371 | `		const char *zName;` |
|        - |  8372 | `		int nLen;` |
|        - |  8373 | `		/* Extract the name */` |
|     4970 |  8374 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8375 | `		/* Perform the lookup */` |
|     4985 |  8376 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8377 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8378 | `				/* Function is callable */` |
|     4952 |  8379 | `				res = 1;` |
|     2475 |  8380 | `		}` |
|     2484 |  8381 | `	}` |
|    17494 |  8382 | `	return res;` |
|        2 |  8383 |  |
|        - |  8384 | `/*` |
|        - |  8385 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8386 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8387 | ` * Parameters` |
|        - |  8388 | ` * $name` |
|        - |  8389 | ` *    The callback function to check` |
|        - |  8390 | ` * $syntax_only` |
|        - |  8391 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8392 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8393 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8394 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8395 | ` *    a string.` |
|        - |  8396 | ` * Return` |
|        - |  8397 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8398 | ` */` |
|       14 |  8399 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8400 |  |
|        - |  8401 | `	ph7_vm *pVm;` |
|        - |  8402 | `	int res;` |
|       15 |  8403 | `	if( nArg < 1 ){` |
|        - |  8404 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8405 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8406 | `		return SXRET_OK;` |
|        - |  8407 | `	}` |
|        - |  8408 | `	/* Point to the target VM */` |
|       15 |  8409 | `	pVm = pCtx->pVm;` |
|        - |  8410 | `	/* Perform the requested operation */` |
|       15 |  8411 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8412 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8413 | `	return SXRET_OK;` |
|        8 |  8414 |  |
|        - |  8415 | `/*` |
|        - |  8416 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8417 | ` * defined below.` |
|        - |  8418 | ` */` |
|     1196 |  8419 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8420 |  |
|     1197 |  8421 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8422 | `	ph7_value sName;` |
|        - |  8423 | `	sxi32 rc;` |
|        - |  8424 | `	/* Prepare the function name for insertion */` |
|     1197 |  8425 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1197 |  8426 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8427 | `	/* Perform the insertion */` |
|     1197 |  8428 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1197 |  8429 | `	PH7_MemObjRelease(&sName);` |
|     1197 |  8430 | `	return rc;` |
|        1 |  8431 |  |
|        - |  8432 | `/*` |
|        - |  8433 | ` * array get_defined_functions(void)` |
|        - |  8434 | ` *  Returns an array of all defined functions.` |
|        - |  8435 | ` * Parameter` |
|        - |  8436 | ` *  None.` |
|        - |  8437 | ` * Return` |
|        - |  8438 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8439 | ` *  both built-in (internal) and user-defined.` |
|        - |  8440 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8441 | ` *  defined ones using $arr["user"].` |
|        - |  8442 | ` * Note:` |
|        - |  8443 | ` *  NULL is returned on failure.` |
|        - |  8444 | ` */` |
|        2 |  8445 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8446 |  |
|        - |  8447 | `	ph7_value *pArray,*pEntry;` |
|        - |  8448 | `	/* NOTE:` |
|        - |  8449 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8450 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8451 | `	 */` |
|        3 |  8452 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8453 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8454 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8455 | `		SXUNUSED(apArg);` |
|        - |  8456 | `		/* Return NULL */` |
|      ! 0 |  8457 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8458 | `		return SXRET_OK;` |
|        - |  8459 | `	}` |
|        3 |  8460 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8461 | `	if( pEntry == 0 ){` |
|        - |  8462 | `		/* Return NULL */` |
|      ! 0 |  8463 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8464 | `		return SXRET_OK;` |
|        - |  8465 | `	}` |
|        - |  8466 | `	/* Fill with the appropriate information */` |
|        3 |  8467 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8468 | `	/* Create the 'internal' index */` |
|        3 |  8469 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8470 | `	/* Create the user-func array */` |
|        3 |  8471 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8472 | `	if( pEntry == 0 ){` |
|        - |  8473 | `		/* Return NULL */` |
|      ! 0 |  8474 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8475 | `		return SXRET_OK;` |
|        - |  8476 | `	}` |
|        - |  8477 | `	/* Fill with the appropriate information */` |
|        3 |  8478 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8479 | `	/* Create the 'user' index */` |
|        3 |  8480 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8481 | `	/* Return the multi-dimensional array */` |
|        3 |  8482 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8483 | `	return SXRET_OK;` |
|        2 |  8484 |  |
|        - |  8485 | `/*` |
|        - |  8486 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8487 | ` *  Register a function for execution on shutdown.` |
|        - |  8488 | ` * Note` |
|        - |  8489 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8490 | ` *  be called in the same order as they were registered.` |
|        - |  8491 | ` * Parameters` |
|        - |  8492 | ` *  $callback` |
|        - |  8493 | ` *   The shutdown callback to register.` |
|        - |  8494 | ` * $param` |
|        - |  8495 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8496 | ` * Return` |
|        - |  8497 | ` *  Nothing.` |
|        - |  8498 | ` */` |
|        2 |  8499 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8500 |  |
|        - |  8501 | `	VmShutdownCB sEntry;` |
|        - |  8502 | `	int i,j;` |
|        3 |  8503 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8504 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8505 | `		return PH7_OK;` |
|        - |  8506 | `	}` |
|        - |  8507 | `	/* Zero the Entry */` |
|        3 |  8508 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8509 | `	/* Initialize fields */` |
|        3 |  8510 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8511 | `	/* Save the callback name for later invocation name */` |
|        3 |  8512 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8513 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8514 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8515 | `	}` |
|        - |  8516 | `	/* Copy arguments */` |
|        3 |  8517 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8518 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8519 | `			/* Limit reached */` |
|      ! 0 |  8520 | `			break;` |
|        - |  8521 | `		}` |
|      ! 0 |  8522 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8523 | `	}` |
|        3 |  8524 | `	sEntry.nArg = j;` |
|        - |  8525 | `	/* Install the callback */` |
|        3 |  8526 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8527 | `	return PH7_OK;` |
|        2 |  8528 |  |
|        - |  8529 | `/*` |
|        - |  8530 | ` * Section:` |
|        - |  8531 | ` *  Class handling functions.` |
|        - |  8532 | ` * Status:` |
|        - |  8533 | ` *    Stable.` |
|        - |  8534 | ` */` |
|        - |  8535 | `/*` |
|        - |  8536 | ` * Extract the top active class. NULL is returned` |
|        - |  8537 | ` * if the class stack is empty.` |
|        - |  8538 | ` */` |
|      566 |  8539 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8540 |  |
|      568 |  8541 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8542 | `	ph7_class **apClass;` |
|      568 |  8543 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8544 | `		/* Empty stack,return NULL */` |
|       15 |  8545 | `		return 0;` |
|        - |  8546 | `	}` |
|        - |  8547 | `	/* Peek the last entry */` |
|      554 |  8548 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      554 |  8549 | `	return apClass[pSet->nUsed - 1];` |
|      285 |  8550 |  |
|        - |  8551 | `/*` |
|        - |  8552 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8553 | ` *   Get the class that declared the currently executing method.` |
|        - |  8554 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8555 | ` *` |
|        - |  8556 | ` * Parameters` |
|        - |  8557 | ` *   pVm: Target VM` |
|        - |  8558 | ` *` |
|        - |  8559 | ` * Return` |
|        - |  8560 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8561 | ` *   - Not executing within a class method` |
|        - |  8562 | ` *` |
|        - |  8563 | ` * Note` |
|        - |  8564 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8565 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8566 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8567 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8568 | ` *   declaring class.` |
|        - |  8569 | ` */` |
|       60 |  8570 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8571 |  |
|       62 |  8572 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8573 | `	ph7_vm_func *pVmFunc;` |
|        - |  8574 |  |
|        - |  8575 | `	/* Skip exception frames to find the actual method frame */` |
|       62 |  8576 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8577 |  |
|        - |  8578 | `	/* Check if we're in a method context */` |
|       62 |  8579 | `	if( pFrame->pParent ){` |
|       58 |  8580 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       58 |  8581 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8582 | `			/* Return the declaring class */` |
|       58 |  8583 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8584 | `		}` |
|      ! 0 |  8585 | `	}` |
|        - |  8586 |  |
|        5 |  8587 | `	return 0;` |
|       32 |  8588 |  |
|        - |  8589 |  |
|        - |  8590 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8591 | `/*` |
|        - |  8592 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8593 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8594 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8595 | ` * return value indicates failure.` |
|        - |  8596 | ` */` |
|     1492 |  8597 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8598 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8599 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8600 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8601 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8602 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8603 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8604 | `	)` |
|        2 |  8605 |  |
|        - |  8606 | `	ph7_value *aStack;` |
|        - |  8607 | `	VmInstr aInstr[2];` |
|        - |  8608 | `	int iCursor;` |
|        - |  8609 | `	int i;` |
|        - |  8610 | `	/* Create a new operand stack */` |
|     1494 |  8611 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1494 |  8612 | `	if( aStack == 0 ){` |
|      ! 0 |  8613 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8614 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8615 | `		return SXERR_MEM;` |
|        - |  8616 | `	}` |
|        - |  8617 | `	/* Fill the operand stack with the given arguments */` |
|     2100 |  8618 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      608 |  8619 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8620 | `		/*` |
|        - |  8621 | `		 * Symisc eXtension:` |
|        - |  8622 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8623 | `		 */` |
|      608 |  8624 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      305 |  8625 | `	}` |
|     1494 |  8626 | `	iCursor = nArg + 1;` |
|     1494 |  8627 | `	if( pThis ){` |
|        - |  8628 | `		/*` |
|        - |  8629 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8630 | `		 */` |
|     1488 |  8631 | `		pThis->iRef++; /* Increment reference count */` |
|     1488 |  8632 | `		aStack[i].x.pOther = pThis;` |
|     1488 |  8633 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      743 |  8634 | `	}` |
|     1494 |  8635 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1494 |  8636 | `	i++;` |
|        - |  8637 | `	/* Push method name */` |
|     1494 |  8638 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1494 |  8639 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1494 |  8640 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1494 |  8641 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8642 | `	/* Emit the CALL istruction */` |
|     1494 |  8643 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1494 |  8644 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1494 |  8645 | `	aInstr[0].iP2 = 0;` |
|     1494 |  8646 | `	aInstr[0].p3  = 0;` |
|        - |  8647 | `	/* Emit the DONE instruction */` |
|     1494 |  8648 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1494 |  8649 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1494 |  8650 | `	aInstr[1].iP2 = 0;` |
|     1494 |  8651 | `	aInstr[1].p3  = 0;` |
|        - |  8652 | `	/* Execute the method body (if available) */` |
|     1494 |  8653 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8654 | `	/* Clean up the mess left behind */` |
|     1494 |  8655 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1494 |  8656 | `	return PH7_OK;` |
|      748 |  8657 |  |
|        - |  8658 | `/*` |
|        - |  8659 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8660 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8661 | ` * in the apArg[] array.` |
|        - |  8662 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8663 | ` * return value indicates failure.` |
|        - |  8664 | ` */` |
|      952 |  8665 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8666 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8667 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8668 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8669 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8670 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8671 | `	)` |
|        2 |  8672 |  |
|        - |  8673 | `	ph7_value *aStack;` |
|        - |  8674 | `	VmInstr aInstr[2];` |
|        - |  8675 | `	int i;` |
|      954 |  8676 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8677 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8678 | `		if( pResult ){` |
|        - |  8679 | `			/* Assume a null return value */` |
|      ! 0 |  8680 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8681 | `		}` |
|      471 |  8682 | `		return SXERR_INVALID;` |
|        - |  8683 | `	}` |
|      484 |  8684 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8685 | `		/* Class method */` |
|       11 |  8686 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8687 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8688 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8689 | `		ph7_class *pClass = 0;` |
|        - |  8690 | `		ph7_value *pValue;` |
|        - |  8691 | `		sxi32 rc;` |
|       11 |  8692 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8693 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8694 | `			if( pResult ){` |
|        - |  8695 | `				/* Assume a null return value */` |
|      ! 0 |  8696 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8697 | `			}` |
|      ! 0 |  8698 | `			return SXRET_OK;` |
|        - |  8699 | `		}` |
|        - |  8700 | `		/* Extract the class name or an instance of it */` |
|       11 |  8701 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8702 | `		if( pValue ){` |
|       11 |  8703 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8704 | `		}` |
|       11 |  8705 | `		if( pClass == 0 ){` |
|        - |  8706 | `			/* No such class,return NULL */` |
|      ! 0 |  8707 | `			if( pResult ){` |
|      ! 0 |  8708 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8709 | `			}` |
|      ! 0 |  8710 | `			return SXRET_OK;` |
|        - |  8711 | `		}` |
|       11 |  8712 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8713 | `			/* Point to the class instance */` |
|        5 |  8714 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8715 | `		}` |
|        - |  8716 | `		/* Try to extract the method */` |
|       11 |  8717 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8718 | `		if( pValue ){` |
|       11 |  8719 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8720 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8721 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8722 | `			}` |
|        5 |  8723 | `		}` |
|       11 |  8724 | `		if( pMethod == 0 ){` |
|        - |  8725 | `			/* No such method,return NULL */` |
|      ! 0 |  8726 | `			if( pResult ){` |
|      ! 0 |  8727 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8728 | `			}` |
|      ! 0 |  8729 | `			return SXRET_OK;` |
|        - |  8730 | `		}` |
|        - |  8731 | `		/* Call the class method */` |
|       11 |  8732 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8733 | `		return rc;` |
|        - |  8734 | `	}` |
|        - |  8735 | `	/* Create a new operand stack */` |
|      474 |  8736 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8737 | `	if( aStack == 0 ){` |
|      ! 0 |  8738 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8739 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8740 | `		if( pResult ){` |
|        - |  8741 | `			/* Assume a null return value */` |
|      ! 0 |  8742 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8743 | `		}` |
|      ! 0 |  8744 | `		return SXERR_MEM;` |
|        - |  8745 | `	}` |
|        - |  8746 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8747 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8748 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8749 | `		/*` |
|        - |  8750 | `		 * Symisc eXtension:` |
|        - |  8751 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8752 | `		 */` |
|     1050 |  8753 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8754 | `	}` |
|        - |  8755 | `	/* Push the function name */` |
|      474 |  8756 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8757 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8758 | `	/* Emit the CALL istruction */` |
|      474 |  8759 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8760 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8761 | `	aInstr[0].iP2 = 0;` |
|      474 |  8762 | `	aInstr[0].p3  = 0;` |
|        - |  8763 | `	/* Emit the DONE instruction */` |
|      474 |  8764 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8765 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8766 | `	aInstr[1].iP2 = 0;` |
|      474 |  8767 | `	aInstr[1].p3  = 0;` |
|        - |  8768 | `	/* Execute the function body (if available) */` |
|      474 |  8769 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8770 | `	/* Clean up the mess left behind */` |
|      474 |  8771 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8772 | `	return PH7_OK;` |
|      478 |  8773 |  |
|        - |  8774 | `/*` |
|        - |  8775 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8776 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8777 | ` * parameter.` |
|        - |  8778 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8779 | ` * return value indicates failure.` |
|        - |  8780 | ` */` |
|      236 |  8781 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8782 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8783 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8784 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8785 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8786 | `	)` |
|        1 |  8787 |  |
|        - |  8788 | `	ph7_value *pArg;` |
|        - |  8789 | `	SySet aArg;` |
|        - |  8790 | `	va_list ap;` |
|        - |  8791 | `	sxi32 rc;` |
|      237 |  8792 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8793 | `	/* Copy arguments one after one */` |
|      237 |  8794 | `	va_start(ap,pResult);` |
|      393 |  8795 | `	for(;;){` |
|      787 |  8796 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8797 | `		if( pArg == 0 ){` |
|      237 |  8798 | `			break;` |
|        - |  8799 | `		}` |
|      551 |  8800 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8801 | `	}` |
|        - |  8802 | `	/* Call the core routine */` |
|      237 |  8803 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8804 | `	/* Cleanup */` |
|      237 |  8805 | `	SySetRelease(&aArg);` |
|      237 |  8806 | `	return rc;` |
|        1 |  8807 |  |
|        - |  8808 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8809 | `/*` |
|        - |  8810 | ` * bool defined(string $name)` |
|        - |  8811 | ` *  Checks whether a given named constant exists.` |
|        - |  8812 | ` * Parameter:` |
|        - |  8813 | ` *  Name of the desired constant.` |
|        - |  8814 | ` * Return` |
|        - |  8815 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8816 | ` */` |
|       14 |  8817 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8818 |  |
|        - |  8819 | `	const char *zName;` |
|       16 |  8820 | `	int nLen = 0;` |
|       16 |  8821 | `	int res = 0;` |
|       16 |  8822 | `	if( nArg < 1 ){` |
|        - |  8823 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8824 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8825 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8826 | `		return SXRET_OK;` |
|        - |  8827 | `	}` |
|        - |  8828 | `	/* Extract constant name */` |
|       16 |  8829 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8830 | `	/* Perform the lookup */` |
|       16 |  8831 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8832 | `		/* Already defined */` |
|       10 |  8833 | `		res = 1;` |
|        4 |  8834 | `	}` |
|       16 |  8835 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8836 | `	return SXRET_OK;` |
|        9 |  8837 |  |
|        - |  8838 | `/*` |
|        - |  8839 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8840 | ` * below.` |
|        - |  8841 | ` */` |
|        8 |  8842 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8843 |  |
|       10 |  8844 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8845 | `	/* Expand constant value */` |
|       10 |  8846 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8847 |  |
|        - |  8848 | `/*` |
|        - |  8849 | ` * bool define(string $constant_name,expression value)` |
|        - |  8850 | ` *  Defines a named constant at runtime.` |
|        - |  8851 | ` * Parameter:` |
|        - |  8852 | ` *  $constant_name` |
|        - |  8853 | ` *   The name of the constant` |
|        - |  8854 | ` *  $value` |
|        - |  8855 | ` *   Constant value` |
|        - |  8856 | ` * Return:` |
|        - |  8857 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8858 | ` */` |
|       10 |  8859 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8860 |  |
|        - |  8861 | `	const char *zName;  /* Constant name */` |
|        - |  8862 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8863 | `	int nLen = 0;       /* Name length */` |
|        - |  8864 | `	sxi32 rc;` |
|       12 |  8865 | `	if( nArg < 2 ){` |
|        - |  8866 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8867 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8868 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8869 | `		return SXRET_OK;` |
|        - |  8870 | `	}` |
|       12 |  8871 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8872 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8873 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8874 | `		return SXRET_OK;` |
|        - |  8875 | `	}` |
|        - |  8876 | `	/* Extract constant name */` |
|       12 |  8877 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8878 | `	if( nLen < 1 ){` |
|      ! 0 |  8879 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8880 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8881 | `		return SXRET_OK;` |
|        - |  8882 | `	}` |
|        - |  8883 | `	/* Duplicate constant value */` |
|       12 |  8884 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8885 | `	if( pValue == 0 ){` |
|      ! 0 |  8886 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8887 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8888 | `		return SXRET_OK;` |
|        - |  8889 | `	}` |
|        - |  8890 | `	/* Initialize the memory object */` |
|       12 |  8891 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8892 | `	/* Register the constant */` |
|       12 |  8893 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8894 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8895 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8896 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8897 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8898 | `		return SXRET_OK;` |
|        - |  8899 | `	}` |
|        - |  8900 | `	/* Duplicate constant value */` |
|       12 |  8901 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8902 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8903 | `		/* Lower case the constant name */` |
|      ! 0 |  8904 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8905 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8906 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8907 | `				/* UTF-8 stream */` |
|      ! 0 |  8908 | `				zCur++;` |
|      ! 0 |  8909 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8910 | `					zCur++;` |
|      ! 0 |  8911 | `				}` |
|      ! 0 |  8912 | `				continue;` |
|        - |  8913 | `			}` |
|      ! 0 |  8914 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8915 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8916 | `				zCur[0] = (char)c;` |
|      ! 0 |  8917 | `			}` |
|      ! 0 |  8918 | `			zCur++;` |
|      ! 0 |  8919 | `		}` |
|        - |  8920 | `		/* Finally,register the constant */` |
|      ! 0 |  8921 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8922 | `	}` |
|        - |  8923 | `	/* All done,return TRUE */` |
|       12 |  8924 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8925 | `	return SXRET_OK;` |
|        7 |  8926 |  |
|        - |  8927 | `/*` |
|        - |  8928 | ` * value constant(string $name)` |
|        - |  8929 | ` *  Returns the value of a constant` |
|        - |  8930 | ` * Parameter` |
|        - |  8931 | ` *  $name` |
|        - |  8932 | ` *    Name of the constant.` |
|        - |  8933 | ` * Return` |
|        - |  8934 | ` *  Constant value or NULL if not defined.` |
|        - |  8935 | ` */` |
|        8 |  8936 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8937 |  |
|        - |  8938 | `	SyHashEntry *pEntry;` |
|        - |  8939 | `	ph7_constant *pCons;` |
|        - |  8940 | `	const char *zName; /* Constant name */` |
|        - |  8941 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8942 | `	int nLen;` |
|       10 |  8943 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8944 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8945 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8946 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8947 | `		return SXRET_OK;` |
|        - |  8948 | `	}` |
|        - |  8949 | `	/* Extract the constant name */` |
|       10 |  8950 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8951 | `	/* Perform the query */` |
|       10 |  8952 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8953 | `	if( pEntry == 0 ){` |
|        3 |  8954 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8955 | `		ph7_result_null(pCtx);` |
|        3 |  8956 | `		return SXRET_OK;` |
|        - |  8957 | `	}` |
|        8 |  8958 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8959 | `	/* Point to the structure that describe the constant */` |
|        8 |  8960 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8961 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8962 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8963 | `	/* Return that value */` |
|        8 |  8964 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8965 | `	/* Cleanup */` |
|        8 |  8966 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8967 | `	return SXRET_OK;` |
|        6 |  8968 |  |
|        - |  8969 | `/*` |
|        - |  8970 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8971 | ` * defined below.` |
|        - |  8972 | ` */` |
|      444 |  8973 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8974 |  |
|      445 |  8975 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8976 | `	ph7_value sName;` |
|        - |  8977 | `	sxi32 rc;` |
|        - |  8978 | `	/* Prepare the constant name for insertion */` |
|      445 |  8979 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      445 |  8980 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8981 | `	/* Perform the insertion */` |
|      445 |  8982 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      445 |  8983 | `	PH7_MemObjRelease(&sName);` |
|      445 |  8984 | `	return rc;` |
|        1 |  8985 |  |
|        - |  8986 | `/*` |
|        - |  8987 | ` * array get_defined_constants(void)` |
|        - |  8988 | ` *  Returns an associative array with the names of all defined` |
|        - |  8989 | ` *  constants.` |
|        - |  8990 | ` * Parameters` |
|        - |  8991 | ` *  NONE.` |
|        - |  8992 | ` * Returns` |
|        - |  8993 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8994 | ` */` |
|        2 |  8995 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8996 |  |
|        - |  8997 | `	ph7_value *pArray;` |
|        - |  8998 | `	/* Create the array first*/` |
|        3 |  8999 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9000 | `	if( pArray == 0 ){` |
|      ! 0 |  9001 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9002 | `		SXUNUSED(apArg);` |
|        - |  9003 | `		/* Return NULL */` |
|      ! 0 |  9004 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9005 | `		return SXRET_OK;` |
|        - |  9006 | `	}` |
|        - |  9007 | `	/* Fill the array with the defined constants */` |
|        3 |  9008 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9009 | `	/* Return the created array */` |
|        3 |  9010 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9011 | `	return SXRET_OK;` |
|        2 |  9012 |  |
|        - |  9013 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9014 | `/*` |
|        - |  9015 | ` * Section:` |
|        - |  9016 | ` *  Random numbers/string generators.` |
|        - |  9017 | ` * Status:` |
|        - |  9018 | ` *    Stable.` |
|        - |  9019 | ` */` |
|        - |  9020 | `/*` |
|        - |  9021 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9022 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9023 | ` * used by te SQLite3 library.` |
|        - |  9024 | ` */` |
|     2391 |  9025 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9026 |  |
|        - |  9027 | `	sxu32 iNum;` |
|     2393 |  9028 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2393 |  9029 | `	return iNum;` |
|        2 |  9030 |  |
|        - |  9031 | `/*` |
|        - |  9032 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9033 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9034 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9035 | ` * by te SQLite3 library.` |
|        - |  9036 | ` */` |
|   124246 |  9037 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9038 |  |
|        - |  9039 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9040 | `	int i;` |
|        - |  9041 | `	/* Generate a binary string first */` |
|   124248 |  9042 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9043 | `	/* Turn the binary string into english based alphabet */` |
|  1366876 |  9044 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1242630 |  9045 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   621316 |  9046 | `	 }` |
|   124248 |  9047 |  |
|        - |  9048 | `/*` |
|        - |  9049 | ` * int rand()` |
|        - |  9050 | ` * int mt_rand()` |
|        - |  9051 | ` * int rand(int $min,int $max)` |
|        - |  9052 | ` * int mt_rand(int $min,int $max)` |
|        - |  9053 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9054 | ` * Parameter` |
|        - |  9055 | ` *  $min` |
|        - |  9056 | ` *    The lowest value to return (default: 0)` |
|        - |  9057 | ` *  $max` |
|        - |  9058 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9059 | ` * Return` |
|        - |  9060 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9061 | ` * Note:` |
|        - |  9062 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9063 | ` *  by te SQLite3 library.` |
|        - |  9064 | ` */` |
|       20 |  9065 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9066 |  |
|        - |  9067 | `	sxu32 iNum;` |
|        - |  9068 | `	/* Generate the random number */` |
|       21 |  9069 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9070 | `	if( nArg > 1 ){` |
|        - |  9071 | `		sxu32 iMin,iMax;` |
|        3 |  9072 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9073 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9074 | `		if( iMin < iMax ){` |
|        3 |  9075 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9076 | `			if( iDiv > 0 ){` |
|        3 |  9077 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9078 | `			}` |
|        1 |  9079 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9080 | `			iNum %= iMax;` |
|      ! 0 |  9081 | `		}` |
|        1 |  9082 | `	}` |
|        - |  9083 | `	/* Return the number */` |
|       21 |  9084 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9085 | `	return SXRET_OK;` |
|        1 |  9086 |  |
|        - |  9087 | `/*` |
|        - |  9088 | ` * int getrandmax(void)` |
|        - |  9089 | ` * int mt_getrandmax(void)` |
|        - |  9090 | ` * int rc4_getrandmax(void)` |
|        - |  9091 | ` *   Show largest possible random value` |
|        - |  9092 | ` * Return` |
|        - |  9093 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9094 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9095 | ` * Note:` |
|        - |  9096 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9097 | ` *  by te SQLite3 library.` |
|        - |  9098 | ` */` |
|        4 |  9099 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9100 |  |
|        2 |  9101 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9102 | `	SXUNUSED(apArg);` |
|        5 |  9103 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9104 | `	return SXRET_OK;` |
|        1 |  9105 |  |
|        - |  9106 | `/*` |
|        - |  9107 | ` * string rand_str()` |
|        - |  9108 | ` * string rand_str(int $len)` |
|        - |  9109 | ` *  Generate a random string (English alphabet).` |
|        - |  9110 | ` * Parameter` |
|        - |  9111 | ` *  $len` |
|        - |  9112 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9113 | ` * Return` |
|        - |  9114 | ` *   A pseudo random string.` |
|        - |  9115 | ` * Note:` |
|        - |  9116 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9117 | ` *  by te SQLite3 library.` |
|        - |  9118 | ` *  This function is a symisc extension.` |
|        - |  9119 | ` */` |
|      120 |  9120 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9121 |  |
|        - |  9122 | `	char zString[1024];` |
|      122 |  9123 | `	int iLen = 0x10;` |
|      122 |  9124 | `	if( nArg > 0 ){` |
|        - |  9125 | `		/* Get the desired length */` |
|      122 |  9126 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9127 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9128 | `			/* Default length */` |
|        3 |  9129 | `			iLen = 0x10;` |
|        1 |  9130 | `		}` |
|       60 |  9131 | `	}` |
|        - |  9132 | `	/* Generate the random string */` |
|      122 |  9133 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9134 | `	/* Return the generated string */` |
|      122 |  9135 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9136 | `	return SXRET_OK;` |
|        2 |  9137 |  |
|        - |  9138 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9139 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9140 | `/* Unique ID private data */` |
|        - |  9141 | `struct unique_id_data` |
|        - |  9142 |  |
|        - |  9143 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9144 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9145 | `};` |
|        - |  9146 | `/*` |
|        - |  9147 | ` * Binary to hex consumer callback.` |
|        - |  9148 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9149 | ` * defined below.` |
|        - |  9150 | ` */` |
|      192 |  9151 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9152 |  |
|      193 |  9153 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9154 | `	sxu32 nBuflen;` |
|        - |  9155 | `	/* Extract result buffer length */` |
|      193 |  9156 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9157 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9158 | `			/*` |
|        - |  9159 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9160 | `			 * string will be 13 characters long` |
|        - |  9161 | `			 */` |
|       25 |  9162 | `		return SXERR_ABORT;` |
|        - |  9163 | `	}` |
|      169 |  9164 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9165 | `		return SXERR_ABORT;` |
|        - |  9166 | `	}` |
|        - |  9167 | `	/* Safely Consume the hex stream */` |
|      169 |  9168 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9169 | `	return SXRET_OK;` |
|       97 |  9170 |  |
|        - |  9171 | `/*` |
|        - |  9172 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9173 | ` *  Generate a unique ID` |
|        - |  9174 | ` * Parameter` |
|        - |  9175 | ` * $prefix` |
|        - |  9176 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9177 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9178 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9179 | ` * $more_entropy` |
|        - |  9180 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9181 | ` *  that the result will be unique.` |
|        - |  9182 | ` * Return` |
|        - |  9183 | ` *  Returns the unique identifier, as a string.` |
|        - |  9184 | ` */` |
|       24 |  9185 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9186 |  |
|        - |  9187 | `	struct unique_id_data sUniq;` |
|        - |  9188 | `	unsigned char zDigest[20];` |
|       25 |  9189 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9190 | `	const char *zPrefix;` |
|        - |  9191 | `	SHA1Context sCtx;` |
|        - |  9192 | `	char zRandom[7];` |
|        - |  9193 | `	int nPrefix;` |
|        - |  9194 | `	int entropy;` |
|        - |  9195 | `	/* Generate a random string first */` |
|       25 |  9196 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9197 | `	/* Initialize fields */` |
|       25 |  9198 | `	zPrefix = 0;` |
|       25 |  9199 | `	nPrefix = 0;` |
|       25 |  9200 | `	entropy = 0;` |
|       25 |  9201 | `	if( nArg > 0 ){` |
|        - |  9202 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9203 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9204 | `		if( nArg > 1 ){` |
|      ! 0 |  9205 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9206 | `		}` |
|      ! 0 |  9207 | `	}` |
|       25 |  9208 | `	SHA1Init(&sCtx);` |
|        - |  9209 | `	/* Generate the random ID */` |
|       25 |  9210 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9211 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9212 | `	}` |
|        - |  9213 | `	/* Append the random ID */` |
|       25 |  9214 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9215 | `	/* Append the random string */` |
|       25 |  9216 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9217 | `	/* Increment the number */` |
|       25 |  9218 | `	pVm->unique_id++;` |
|       25 |  9219 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9220 | `	/* Hexify the digest */` |
|       25 |  9221 | `	sUniq.pCtx = pCtx;` |
|       25 |  9222 | `	sUniq.entropy = entropy;` |
|       25 |  9223 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9224 | `	/* All done */` |
|       25 |  9225 | `	return PH7_OK;` |
|        1 |  9226 |  |
|        - |  9227 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9228 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9229 | `/*` |
|        - |  9230 | ` * Section:` |
|        - |  9231 | ` *  Language construct implementation as foreign functions.` |
|        - |  9232 | ` * Status:` |
|        - |  9233 | ` *    Stable.` |
|        - |  9234 | ` */` |
|        - |  9235 | `/*` |
|        - |  9236 | ` * void echo($string...)` |
|        - |  9237 | ` *  Output one or more messages.` |
|        - |  9238 | ` * Parameters` |
|        - |  9239 | ` *  $string` |
|        - |  9240 | ` *   Message to output.` |
|        - |  9241 | ` * Return` |
|        - |  9242 | ` *  NULL.` |
|        - |  9243 | ` */` |
|      ! 0 |  9244 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9245 |  |
|        - |  9246 | `	const char *zData;` |
|      ! 0 |  9247 | `	int nDataLen = 0;` |
|        - |  9248 | `	ph7_vm *pVm;` |
|        - |  9249 | `	int i,rc;` |
|        - |  9250 | `	/* Point to the target VM */` |
|      ! 0 |  9251 | `	pVm = pCtx->pVm;` |
|        - |  9252 | `	/* Output */` |
|      ! 0 |  9253 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9254 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9255 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9256 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9257 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9258 | `			if( rc == SXERR_ABORT ){` |
|        - |  9259 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9260 | `				return PH7_ABORT;` |
|        - |  9261 | `			}` |
|      ! 0 |  9262 | `		}` |
|      ! 0 |  9263 | `	}` |
|      ! 0 |  9264 | `	return SXRET_OK;` |
|      ! 0 |  9265 |  |
|        - |  9266 | `/*` |
|        - |  9267 | ` * int print($string...)` |
|        - |  9268 | ` *  Output one or more messages.` |
|        - |  9269 | ` * Parameters` |
|        - |  9270 | ` *  $string` |
|        - |  9271 | ` *   Message to output.` |
|        - |  9272 | ` * Return` |
|        - |  9273 | ` *  1 always.` |
|        - |  9274 | ` */` |
|        2 |  9275 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9276 |  |
|        - |  9277 | `	const char *zData;` |
|        3 |  9278 | `	int nDataLen = 0;` |
|        - |  9279 | `	ph7_vm *pVm;` |
|        - |  9280 | `	int i,rc;` |
|        - |  9281 | `	/* Point to the target VM */` |
|        3 |  9282 | `	pVm = pCtx->pVm;` |
|        - |  9283 | `	/* Output */` |
|        5 |  9284 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9285 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9286 | `		if( nDataLen > 0 ){` |
|        3 |  9287 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9288 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9289 | `			if( rc == SXERR_ABORT ){` |
|        - |  9290 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9291 | `				return PH7_ABORT;` |
|        - |  9292 | `			}` |
|        1 |  9293 | `		}` |
|        2 |  9294 | `	}` |
|        - |  9295 | `	/* Return 1 */` |
|        3 |  9296 | `	ph7_result_int(pCtx,1);` |
|        3 |  9297 | `	return SXRET_OK;` |
|        2 |  9298 |  |
|        - |  9299 | `/*` |
|        - |  9300 | ` * void exit(string $msg)` |
|        - |  9301 | ` * void exit(int $status)` |
|        - |  9302 | ` * void die(string $ms)` |
|        - |  9303 | ` * void die(int $status)` |
|        - |  9304 | ` *   Output a message and terminate program execution.` |
|        - |  9305 | ` * Parameter` |
|        - |  9306 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9307 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9308 | ` *  and not printed` |
|        - |  9309 | ` * Return` |
|        - |  9310 | ` *  NULL` |
|        - |  9311 | ` */` |
|      ! 0 |  9312 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9313 |  |
|      ! 0 |  9314 | `	if( nArg > 0 ){` |
|      ! 0 |  9315 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9316 | `			const char *zData;` |
|      ! 0 |  9317 | `			int iLen = 0;` |
|        - |  9318 | `			/* Print exit message */` |
|      ! 0 |  9319 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9320 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9321 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9322 | `			sxi32 iExitStatus;` |
|        - |  9323 | `			/* Record exit status code */` |
|      ! 0 |  9324 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9325 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9326 | `		}` |
|      ! 0 |  9327 | `	}` |
|        - |  9328 | `	/* Check if we are in an included file */` |
|      ! 0 |  9329 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9330 | `		/* Exit the entire process */` |
|      ! 0 |  9331 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9332 | `	}` |
|        - |  9333 | `	/* Abort processing immediately */` |
|      ! 0 |  9334 | `	return PH7_ABORT;` |
|      ! 0 |  9335 |  |
|        - |  9336 | `/*` |
|        - |  9337 | ` * bool isset($var,...)` |
|        - |  9338 | ` *  Finds out whether a variable is set.` |
|        - |  9339 | ` * Parameters` |
|        - |  9340 | ` *  One or more variable to check.` |
|        - |  9341 | ` * Return` |
|        - |  9342 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9343 | ` */` |
|    74958 |  9344 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9345 |  |
|        - |  9346 | `	ph7_value *pObj;` |
|    74960 |  9347 | `	int res = 0;` |
|        - |  9348 | `	int i;` |
|    74960 |  9349 | `	if( nArg < 1 ){` |
|        - |  9350 | `		/* Missing arguments,return false */` |
|      ! 0 |  9351 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9352 | `		return SXRET_OK;` |
|        - |  9353 | `	}` |
|        - |  9354 | `	/* Iterate over available arguments */` |
|    98802 |  9355 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    74960 |  9356 | `		pObj = apArg[i];` |
|    74960 |  9357 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    50600 |  9358 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9359 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9360 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9361 | `			}` |
|    25299 |  9362 | `		}` |
|    74960 |  9363 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    74960 |  9364 | `		if( !res ){` |
|        - |  9365 | `			/* Variable not set,return FALSE */` |
|    51118 |  9366 | `			ph7_result_bool(pCtx,0);` |
|    51118 |  9367 | `			return SXRET_OK;` |
|        - |  9368 | `		}` |
|    11923 |  9369 | `	}` |
|        - |  9370 | `	/* All given variable are set,return TRUE */` |
|    23844 |  9371 | `	ph7_result_bool(pCtx,1);` |
|    23844 |  9372 | `	return SXRET_OK;` |
|    37481 |  9373 |  |
|        - |  9374 | `/*` |
|        - |  9375 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9376 | ` * frame,the reference table and discard it's contents.` |
|        - |  9377 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9378 | ` */` |
|  3020952 |  9379 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9380 |  |
|        - |  9381 | `	ph7_value *pObj;` |
|        - |  9382 | `	VmRefObj *pRef;` |
|  3020954 |  9383 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3020954 |  9384 | `	if( pObj ){` |
|        - |  9385 | `		/* Release the object */` |
|  3020954 |  9386 | `		PH7_MemObjRelease(pObj);` |
|  1510476 |  9387 | `	}` |
|        - |  9388 | `	/* Remove old reference links */` |
|  3020954 |  9389 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3020954 |  9390 | `	if( pRef ){` |
|  3020948 |  9391 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9392 | `		/* Unlink from the reference table */` |
|  3020948 |  9393 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3020948 |  9394 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9395 | `			VmSlot sFree;` |
|        - |  9396 | `			/* Restore to the free list */` |
|  3020942 |  9397 | `			sFree.nIdx = nObjIdx;` |
|  3020942 |  9398 | `			sFree.pUserData = 0;` |
|  3020942 |  9399 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1510470 |  9400 | `		}` |
|  1510473 |  9401 | `	}` |
|  3020954 |  9402 | `	return SXRET_OK;` |
|        2 |  9403 |  |
|        - |  9404 | `/*` |
|        - |  9405 | ` * void unset($var,...)` |
|        - |  9406 | ` *   Unset one or more given variable.` |
|        - |  9407 | ` * Parameters` |
|        - |  9408 | ` *  One or more variable to unset.` |
|        - |  9409 | ` * Return` |
|        - |  9410 | ` *  Nothing.` |
|        - |  9411 | ` */` |
|     6764 |  9412 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9413 |  |
|        - |  9414 | `	ph7_value *pObj;` |
|        - |  9415 | `	ph7_vm *pVm;` |
|        - |  9416 | `	int i;` |
|        - |  9417 | `	/* Point to the target VM */` |
|     6766 |  9418 | `	pVm = pCtx->pVm;` |
|        - |  9419 | `	/* Iterate and unset */` |
|    13530 |  9420 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6766 |  9421 | `		pObj = apArg[i];` |
|     6766 |  9422 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9423 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9424 | `				/* Throw an error */` |
|      ! 0 |  9425 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9426 | `			}` |
|      ! 0 |  9427 | `		}else{` |
|     6766 |  9428 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9429 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6766 |  9430 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6760 |  9431 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3379 |  9432 | `			}` |
|        - |  9433 | `		}` |
|     3384 |  9434 | `	}` |
|     6766 |  9435 | `	return SXRET_OK;` |
|        2 |  9436 |  |
|        - |  9437 | `/*` |
|        - |  9438 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9439 | ` */` |
|      110 |  9440 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9441 |  |
|      111 |  9442 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9443 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9444 | `	ph7_value *pObj;` |
|        - |  9445 | `	sxu32 nIdx;` |
|        - |  9446 | `	/* Extract the memory object */` |
|      111 |  9447 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9448 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9449 | `	if( pObj ){` |
|      111 |  9450 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9451 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9452 | `				SyString sName;` |
|        - |  9453 | `				ph7_value sKey;` |
|        - |  9454 | `				/* Perform the insertion */` |
|      109 |  9455 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9456 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9457 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9458 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9459 | `			}` |
|       54 |  9460 | `		}` |
|       55 |  9461 | `	}` |
|      111 |  9462 | `	return SXRET_OK;` |
|        1 |  9463 |  |
|        - |  9464 | `/*` |
|        - |  9465 | ` * array get_defined_vars(void)` |
|        - |  9466 | ` *  Returns an array of all defined variables.` |
|        - |  9467 | ` * Parameter` |
|        - |  9468 | ` *  None` |
|        - |  9469 | ` * Return` |
|        - |  9470 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9471 | ` */` |
|        2 |  9472 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9473 |  |
|        3 |  9474 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9475 | `	ph7_value *pArray;` |
|        - |  9476 | `	/* Create a new array */` |
|        3 |  9477 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9478 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9479 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9480 | `		SXUNUSED(apArg);` |
|        - |  9481 | `		/* Return NULL */` |
|      ! 0 |  9482 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9483 | `		return SXRET_OK;` |
|        - |  9484 | `	}` |
|        - |  9485 | `	/* Superglobals first */` |
|        3 |  9486 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9487 | `	/* Then variable defined in the current frame */` |
|        3 |  9488 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9489 | `	/* Finally,return the created array */` |
|        3 |  9490 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9491 | `	return SXRET_OK;` |
|        2 |  9492 |  |
|        - |  9493 | `/*` |
|        - |  9494 | ` * bool gettype($var)` |
|        - |  9495 | ` *  Get the type of a variable` |
|        - |  9496 | ` * Parameters` |
|        - |  9497 | ` *   $var` |
|        - |  9498 | ` *    The variable being type checked.` |
|        - |  9499 | ` * Return` |
|        - |  9500 | ` *   String representation of the given variable type.` |
|        - |  9501 | ` */` |
|       32 |  9502 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9503 |  |
|       34 |  9504 | `	const char *zType = "Empty";` |
|       34 |  9505 | `	if( nArg > 0 ){` |
|       34 |  9506 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9507 | `	}` |
|        - |  9508 | `	/* Return the variable type */` |
|       34 |  9509 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9510 | `	return SXRET_OK;` |
|        2 |  9511 |  |
|        - |  9512 | `/*` |
|        - |  9513 | ` * string get_resource_type(resource $handle)` |
|        - |  9514 | ` *  This function gets the type of the given resource.` |
|        - |  9515 | ` * Parameters` |
|        - |  9516 | ` *  $handle` |
|        - |  9517 | ` *  The evaluated resource handle.` |
|        - |  9518 | ` * Return` |
|        - |  9519 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9520 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9521 | ` *  the return value will be the string Unknown.` |
|        - |  9522 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9523 | ` *  is not a resource.` |
|        - |  9524 | ` */` |
|        2 |  9525 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9526 |  |
|        3 |  9527 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9528 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9529 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9530 | `		return PH7_OK;` |
|        - |  9531 | `	}` |
|        3 |  9532 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9533 | `	return SXRET_OK;` |
|        2 |  9534 |  |
|        - |  9535 | `/*` |
|        - |  9536 | ` * void var_dump(expression,....)` |
|        - |  9537 | ` *   var_dump � Dumps information about a variable` |
|        - |  9538 | ` * Parameters` |
|        - |  9539 | ` *   One or more expression to dump.` |
|        - |  9540 | ` * Returns` |
|        - |  9541 | ` *  Nothing.` |
|        - |  9542 | ` */` |
|      218 |  9543 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9544 |  |
|        - |  9545 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9546 | `	int i;` |
|      220 |  9547 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9548 | `	/* Dump one or more expressions */` |
|      444 |  9549 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9550 | `		ph7_value *pObj = apArg[i];` |
|        - |  9551 | `		/* Reset the working buffer */` |
|      226 |  9552 | `		SyBlobReset(&sDump);` |
|        - |  9553 | `		/* Dump the given expression */` |
|      226 |  9554 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9555 | `		/* Output */` |
|      226 |  9556 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9557 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9558 | `		}` |
|      114 |  9559 | `	}` |
|        - |  9560 | `	/* Release the working buffer */` |
|      220 |  9561 | `	SyBlobRelease(&sDump);` |
|      220 |  9562 | `	return SXRET_OK;` |
|        2 |  9563 |  |
|        - |  9564 | `/*` |
|        - |  9565 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9566 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9567 | ` * Parameters` |
|        - |  9568 | ` *   expression: Expression to dump` |
|        - |  9569 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9570 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9571 | ` *            print_r() will return the information rather than print it.` |
|        - |  9572 | ` * Return` |
|        - |  9573 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9574 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9575 | ` */` |
|       16 |  9576 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9577 |  |
|       17 |  9578 | `	int ret_string = 0;` |
|        - |  9579 | `	SyBlob sDump;` |
|       17 |  9580 | `	if( nArg < 1 ){` |
|        - |  9581 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9582 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9583 | `		return SXRET_OK;` |
|        - |  9584 | `	}` |
|       17 |  9585 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9586 | `	if ( nArg > 1 ){` |
|        - |  9587 | `		/* Where to redirect output */` |
|       11 |  9588 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9589 | `	}` |
|        - |  9590 | `	/* Generate dump */` |
|       17 |  9591 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9592 | `	if( !ret_string ){` |
|        - |  9593 | `		/* Output dump */` |
|        7 |  9594 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9595 | `		/* Return true */` |
|        7 |  9596 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9597 | `	}else{` |
|        - |  9598 | `		/* Generated dump as return value */` |
|       11 |  9599 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9600 | `	}` |
|        - |  9601 | `	/* Release the working buffer */` |
|       17 |  9602 | `	SyBlobRelease(&sDump);` |
|       17 |  9603 | `	return SXRET_OK;` |
|        9 |  9604 |  |
|        - |  9605 | `/*` |
|        - |  9606 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9607 | ` * Same job as print_r. (see coment above)` |
|        - |  9608 | ` */` |
|        2 |  9609 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9610 |  |
|        3 |  9611 | `	int ret_string = 0;` |
|        - |  9612 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9613 | `	if( nArg < 1 ){` |
|        - |  9614 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9615 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9616 | `		return SXRET_OK;` |
|        - |  9617 | `	}` |
|        3 |  9618 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9619 | `	if ( nArg > 1 ){` |
|        - |  9620 | `		/* Where to redirect output */` |
|        3 |  9621 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9622 | `	}` |
|        - |  9623 | `	/* Generate dump */` |
|        3 |  9624 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9625 | `	if( !ret_string ){` |
|        - |  9626 | `		/* Output dump */` |
|      ! 0 |  9627 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9628 | `		/* Return NULL */` |
|      ! 0 |  9629 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9630 | `	}else{` |
|        - |  9631 | `		/* Generated dump as return value */` |
|        3 |  9632 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9633 | `	}` |
|        - |  9634 | `	/* Release the working buffer */` |
|        3 |  9635 | `	SyBlobRelease(&sDump);` |
|        3 |  9636 | `	return SXRET_OK;` |
|        2 |  9637 |  |
|        - |  9638 | `/*` |
|        - |  9639 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9640 | ` *  Set/get the various assert flags.` |
|        - |  9641 | ` * Parameter` |
|        - |  9642 | ` * $what` |
|        - |  9643 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9644 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9645 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9646 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9647 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9648 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9649 | ` * $value` |
|        - |  9650 | ` *   An optional new value for the option.` |
|        - |  9651 | ` * Return` |
|        - |  9652 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9653 | ` */` |
|       28 |  9654 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9655 |  |
|       30 |  9656 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9657 | `	int iOption;` |
|        - |  9658 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9659 | `	if( nArg < 1 ){` |
|        3 |  9660 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9661 | `			"ArgumentCountError",` |
|        - |  9662 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9663 | `			);` |
|        - |  9664 | `	}` |
|        - |  9665 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9666 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9667 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9668 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9669 | `			"TypeError",` |
|        - |  9670 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9671 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9672 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9673 | `			);` |
|        - |  9674 | `	}` |
|       28 |  9675 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9676 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9677 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9678 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9679 | `	switch( iOption ){` |
|        5 |  9680 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9681 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9682 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9683 | `		if( nArg > 1 ){` |
|        5 |  9684 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9685 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9686 | `			}else{` |
|        3 |  9687 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9688 | `			}` |
|        2 |  9689 | `		}` |
|       12 |  9690 | `		break;` |
|        1 |  9691 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9692 | `		/* Return old callback or null */` |
|        3 |  9693 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9694 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9695 | `		}else{` |
|        3 |  9696 | `			ph7_result_null(pCtx);` |
|        - |  9697 | `		}` |
|        3 |  9698 | `		if( nArg > 1 ){` |
|      ! 0 |  9699 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9700 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9701 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9702 | `			}else{` |
|      ! 0 |  9703 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9704 | `			}` |
|      ! 0 |  9705 | `		}` |
|        3 |  9706 | `		break;` |
|        5 |  9707 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9708 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9709 | `		if( nArg > 1 ){` |
|        5 |  9710 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9711 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9712 | `			}else{` |
|        3 |  9713 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9714 | `			}` |
|        2 |  9715 | `		}` |
|       11 |  9716 | `		break;` |
|      ! 0 |  9717 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9718 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9719 | `		break;` |
|        1 |  9720 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9721 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9722 | `		break;` |
|      ! 0 |  9723 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9724 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9725 | `		break;` |
|        1 |  9726 | `	default:` |
|        - |  9727 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9728 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9729 | `			"ValueError",` |
|        - |  9730 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9731 | `			);` |
|        - |  9732 | `	}` |
|       26 |  9733 | `	return PH7_OK;` |
|       16 |  9734 |  |
|        - |  9735 | `/*` |
|        - |  9736 | ` * bool assert(mixed $assertion)` |
|        - |  9737 | ` *  Checks if assertion is FALSE.` |
|        - |  9738 | ` * Parameter` |
|        - |  9739 | ` *  $assertion` |
|        - |  9740 | ` *    The assertion to test.` |
|        - |  9741 | ` * Return` |
|        - |  9742 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9743 | ` */` |
|       24 |  9744 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9745 |  |
|       26 |  9746 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9747 | `	int iFlags,iResult;` |
|        - |  9748 | `	const char *zDesc;` |
|        - |  9749 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9750 | `	if( nArg < 1 ){` |
|        3 |  9751 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9752 | `			"ArgumentCountError",` |
|        - |  9753 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9754 | `			);` |
|        - |  9755 | `	}` |
|       24 |  9756 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9757 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9758 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9759 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9760 | `		return PH7_OK;` |
|        - |  9761 | `	}` |
|        - |  9762 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9763 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9764 | `	if( !iResult ){` |
|        - |  9765 | `		/* Assertion failed */` |
|        - |  9766 | `		/* Extract optional description */` |
|       13 |  9767 | `		zDesc = 0;` |
|       13 |  9768 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9769 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9770 | `		}` |
|       13 |  9771 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9772 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9773 | `			ph7_value sFile,sLine;` |
|        - |  9774 | `			ph7_value *apCbArg[3];` |
|        - |  9775 | `			SyString *pFile;` |
|        - |  9776 | `			/* Extract the processed script */` |
|      ! 0 |  9777 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9778 | `			if( pFile == 0 ){` |
|      ! 0 |  9779 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9780 | `			}` |
|        - |  9781 | `			/* Invoke the callback */` |
|      ! 0 |  9782 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9783 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9784 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9785 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9786 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9787 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9788 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9789 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9790 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9791 | `		}` |
|       13 |  9792 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9793 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9794 | `			return PH7_ABORT;` |
|        - |  9795 | `		}` |
|        - |  9796 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9797 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9798 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9799 | `				"AssertionError",` |
|        - |  9800 | `				"%s",` |
|        1 |  9801 | `				zDesc` |
|        - |  9802 | `				);` |
|      ! 0 |  9803 | `		}else{` |
|       11 |  9804 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9805 | `				"AssertionError",` |
|        - |  9806 | `				"assert(false)"` |
|        - |  9807 | `				);` |
|        - |  9808 | `		}` |
|        - |  9809 | `	}` |
|        - |  9810 | `	/* Assertion passed */` |
|       11 |  9811 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9812 | `	return PH7_OK;` |
|       14 |  9813 |  |
|        - |  9814 | `/*` |
|        - |  9815 | ` * Section:` |
|        - |  9816 | ` *  Error reporting functions.` |
|        - |  9817 | ` * Status:` |
|        - |  9818 | ` *    Stable.` |
|        - |  9819 | ` */` |
|        - |  9820 | `/*` |
|        - |  9821 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9822 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9823 | ` * Parameters` |
|        - |  9824 | ` *  $error_msg` |
|        - |  9825 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9826 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9827 | ` * $error_type` |
|        - |  9828 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9829 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9830 | ` * Return` |
|        - |  9831 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9832 | ` */` |
|       12 |  9833 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9834 |  |
|       14 |  9835 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9836 | `	int rc = PH7_OK;` |
|       14 |  9837 | `	if( nArg > 0 ){` |
|        - |  9838 | `		const char *zErr;` |
|        - |  9839 | `		int nLen;` |
|        - |  9840 | `		/* Extract the error message */` |
|       12 |  9841 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9842 | `		if( nArg > 1 ){` |
|        - |  9843 | `			/* Extract the error type */` |
|       12 |  9844 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9845 | `			switch( nErr ){` |
|        1 |  9846 | `			case 1:   /* E_ERROR */` |
|        - |  9847 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9848 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9849 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9850 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9851 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9852 | `				break;` |
|        1 |  9853 | `			case 2:   /* E_WARNING */` |
|        - |  9854 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9855 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9856 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9857 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9858 | `				break;` |
|        3 |  9859 | `			default:` |
|        8 |  9860 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9861 | `				break;` |
|        - |  9862 | `			}` |
|        5 |  9863 | `		}` |
|        - |  9864 | `		/* Report error */` |
|       12 |  9865 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9866 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9867 | `			return rc;` |
|        - |  9868 | `		}` |
|        - |  9869 | `		/* Return true */` |
|       12 |  9870 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9871 | `	}else{` |
|        - |  9872 | `		/* Missing arguments,return FALSE */` |
|        3 |  9873 | `		ph7_result_bool(pCtx,0);` |
|        - |  9874 | `	}` |
|       14 |  9875 | `	return rc;` |
|        8 |  9876 |  |
|        - |  9877 | `/*` |
|        - |  9878 | ` * int error_reporting([int $level])` |
|        - |  9879 | ` *  Sets which PHP errors are reported.` |
|        - |  9880 | ` * Parameters` |
|        - |  9881 | ` *  $level` |
|        - |  9882 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9883 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9884 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9885 | ` *   levels will not always behave as expected.` |
|        - |  9886 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9887 | ` *   in the predefined constants.` |
|        - |  9888 | ` * Return` |
|        - |  9889 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9890 | ` *   parameter is given.` |
|        - |  9891 | ` */` |
|       38 |  9892 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9893 |  |
|       40 |  9894 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9895 | `	int nOld;` |
|        - |  9896 | `	/* Extract the old reporting level */` |
|       40 |  9897 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 |  9898 | `	if( nArg > 0 ){` |
|        - |  9899 | `		int nNew;` |
|        - |  9900 | `		/* Extract the desired error reporting level */` |
|       32 |  9901 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 |  9902 | `		if( !nNew ){` |
|        - |  9903 | `			/* Do not report errors at all */` |
|        5 |  9904 | `			pVm->bErrReport = 0;` |
|        3 |  9905 | `		}else{` |
|        - |  9906 | `			/* Report all errors */` |
|       28 |  9907 | `			pVm->bErrReport = 1;` |
|        - |  9908 | `		}` |
|       15 |  9909 | `	}` |
|        - |  9910 | `	/* Return the old level */` |
|       40 |  9911 | `	ph7_result_int(pCtx,nOld);` |
|       40 |  9912 | `	return PH7_OK;` |
|        2 |  9913 |  |
|        - |  9914 | `/*` |
|        - |  9915 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9916 | ` *  Send an error message somewhere.` |
|        - |  9917 | ` * Parameter` |
|        - |  9918 | ` *  $message` |
|        - |  9919 | ` *   The error message that should be logged.` |
|        - |  9920 | ` *  $message_type` |
|        - |  9921 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9922 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9923 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9924 | ` *       This is the default option.` |
|        - |  9925 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9926 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9927 | ` *    2  No longer an option.` |
|        - |  9928 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9929 | ` *       to the end of the message string.` |
|        - |  9930 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9931 | ` *  $destination` |
|        - |  9932 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9933 | ` *  $extra_headers` |
|        - |  9934 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9935 | ` * Return` |
|        - |  9936 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9937 | ` * NOTE:` |
|        - |  9938 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9939 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9940 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9941 | ` *  Otherwise this function is no-op.` |
|        - |  9942 | ` */` |
|        4 |  9943 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9944 |  |
|        - |  9945 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9946 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9947 | `	int iType = 0;` |
|        5 |  9948 | `	if( nArg < 1 ){` |
|        - |  9949 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9950 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9951 | `		return PH7_OK;` |
|        - |  9952 | `	}` |
|        5 |  9953 | `	if( pVm->xErrLog  ){` |
|        - |  9954 | `		/* Invoke the user callback */` |
|      ! 0 |  9955 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9956 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9957 | `		if( nArg > 1 ){` |
|      ! 0 |  9958 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9959 | `			if( nArg > 2 ){` |
|      ! 0 |  9960 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9961 | `				if( nArg > 3 ){` |
|      ! 0 |  9962 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9963 | `				}` |
|      ! 0 |  9964 | `			}` |
|      ! 0 |  9965 | `		}` |
|      ! 0 |  9966 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9967 | `	}` |
|        - |  9968 | `	/* Retun TRUE */` |
|        5 |  9969 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9970 | `	return PH7_OK;` |
|        3 |  9971 |  |
|        - |  9972 | `/*` |
|        - |  9973 | ` * bool restore_exception_handler(void)` |
|        - |  9974 | ` *  Restores the previously defined exception handler function.` |
|        - |  9975 | ` * Parameter` |
|        - |  9976 | ` *  None` |
|        - |  9977 | ` * Return` |
|        - |  9978 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9979 | ` */` |
|        4 |  9980 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9981 |  |
|        5 |  9982 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9983 | `	ph7_value *pOld,*pNew;` |
|        - |  9984 | `	/* Point to the old and the new handler */` |
|        5 |  9985 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9986 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9987 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9988 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9989 | `		SXUNUSED(apArg);` |
|        - |  9990 | `		/* No installed handler,return FALSE */` |
|        5 |  9991 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9992 | `		return PH7_OK;` |
|        - |  9993 | `	}` |
|        - |  9994 | `	/* Copy the old handler */` |
|      ! 0 |  9995 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9996 | `	PH7_MemObjRelease(pOld);` |
|        - |  9997 | `	/* Return TRUE */` |
|      ! 0 |  9998 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9999 | `	return PH7_OK;` |
|        3 | 10000 |  |
|        - | 10001 | `/*` |
|        - | 10002 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10003 | ` *  Sets a user-defined exception handler function.` |
|        - | 10004 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10005 | ` * NOTE` |
|        - | 10006 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10007 | ` *  the satndard PHP engine.` |
|        - | 10008 | ` * Parameters` |
|        - | 10009 | ` *  $exception_handler` |
|        - | 10010 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10011 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10012 | ` *   that was thrown.` |
|        - | 10013 | ` *  Note:` |
|        - | 10014 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10015 | ` * Return` |
|        - | 10016 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10017 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10018 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10019 | ` */` |
|        4 | 10020 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10021 |  |
|        6 | 10022 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10023 | `	ph7_value *pOld,*pNew;` |
|        - | 10024 | `	/* Point to the old and the new handler */` |
|        6 | 10025 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10026 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10027 | `	/* Return the old handler */` |
|        6 | 10028 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10029 | `	if( nArg > 0 ){` |
|        6 | 10030 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10031 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10032 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10033 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10034 | `		}else{` |
|        6 | 10035 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10036 | `			/* Install the new handler */` |
|        6 | 10037 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10038 | `		}` |
|        2 | 10039 | `	}` |
|        6 | 10040 | `	return PH7_OK;` |
|        2 | 10041 |  |
|        - | 10042 | `/*` |
|        - | 10043 | ` * bool restore_error_handler(void)` |
|        - | 10044 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10045 | ` * Parameters:` |
|        - | 10046 | ` *  None.` |
|        - | 10047 | ` * Return` |
|        - | 10048 | ` *  Always TRUE.` |
|        - | 10049 | ` */` |
|        4 | 10050 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10051 |  |
|        5 | 10052 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10053 | `	ph7_value *pOld,*pNew;` |
|        - | 10054 | `	/* Point to the old and the new handler */` |
|        5 | 10055 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10056 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10057 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10058 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10059 | `		SXUNUSED(apArg);` |
|        - | 10060 | `		/* No installed callback,return FALSE */` |
|        5 | 10061 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10062 | `		return PH7_OK;` |
|        - | 10063 | `	}` |
|        - | 10064 | `	/* Copy the old callback */` |
|      ! 0 | 10065 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10066 | `	PH7_MemObjRelease(pOld);` |
|        - | 10067 | `	/* Return TRUE */` |
|      ! 0 | 10068 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10069 | `	return PH7_OK;` |
|        3 | 10070 |  |
|        - | 10071 | `/*` |
|        - | 10072 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10073 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10074 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10075 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10076 | ` *  Sets a user-defined error handler function.` |
|        - | 10077 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10078 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10079 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10080 | ` *  conditions (using trigger_error()).` |
|        - | 10081 | ` * Parameters` |
|        - | 10082 | ` *  $error_handler` |
|        - | 10083 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10084 | ` *   describing the error.` |
|        - | 10085 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10086 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10087 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10088 | ` *   The function can be shown as:` |
|        - | 10089 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10090 | ` *     errno` |
|        - | 10091 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10092 | ` *   errstr` |
|        - | 10093 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10094 | ` *   errfile` |
|        - | 10095 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10096 | ` *     was raised in, as a string.` |
|        - | 10097 | ` *  Note:` |
|        - | 10098 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10099 | ` * Return` |
|        - | 10100 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10101 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10102 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10103 | ` */` |
|     9206 | 10104 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10105 |  |
|     9208 | 10106 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10107 | `	ph7_value *pOld,*pNew;` |
|        - | 10108 | `	/* Point to the old and the new handler */` |
|     9208 | 10109 | `	pOld = &pVm->aErrCB[0];` |
|     9208 | 10110 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10111 | `	/* Return the old handler */` |
|     9208 | 10112 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9208 | 10113 | `	if( nArg > 0 ){` |
|     9208 | 10114 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10115 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4603 | 10116 | `			PH7_MemObjRelease(pNew);` |
|     4603 | 10117 | `			ph7_result_bool(pCtx,1);` |
|     2302 | 10118 | `		}else{` |
|     4606 | 10119 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10120 | `			/* Install the new handler */` |
|     4606 | 10121 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10122 | `		}` |
|     4603 | 10123 | `	}` |
|     9208 | 10124 | `	return PH7_OK;` |
|        2 | 10125 |  |
|        - | 10126 | `/*` |
|        - | 10127 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10128 | ` *  Generates a backtrace.` |
|        - | 10129 | ` * Paramaeter` |
|        - | 10130 | ` *  $options` |
|        - | 10131 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10132 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10133 | ` *   all the function/method arguments, to save memory.` |
|        - | 10134 | ` * $limit` |
|        - | 10135 | ` *   (Not Used)` |
|        - | 10136 | ` * Return` |
|        - | 10137 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10138 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10139 | ` *          Name        Type      Description` |
|        - | 10140 | ` *          ------      ------     -----------` |
|        - | 10141 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10142 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10143 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10144 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10145 | ` *          object      object    The current object.` |
|        - | 10146 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10147 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10148 | ` */` |
|      514 | 10149 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10150 |  |
|      516 | 10151 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10152 | `	ph7_value *pArray;` |
|        - | 10153 | `	ph7_class *pClass;` |
|        - | 10154 | `	ph7_value *pValue;` |
|        - | 10155 | `	SyString *pFile;` |
|        - | 10156 | `	/* Create a new array */` |
|      516 | 10157 | `	pArray = ph7_context_new_array(pCtx);` |
|      516 | 10158 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      516 | 10159 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10160 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10161 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10162 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10163 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10164 | `		SXUNUSED(apArg);` |
|      ! 0 | 10165 | `		return PH7_OK;` |
|        - | 10166 | `	}` |
|        - | 10167 | `	/* Dump running function name and it's arguments  */` |
|      516 | 10168 | `	if( pVm->pFrame->pParent ){` |
|      516 | 10169 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10170 | `		ph7_vm_func *pFunc;` |
|        - | 10171 | `		ph7_value *pArg;` |
|      516 | 10172 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      516 | 10173 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      516 | 10174 | `		if( pFrame->pParent && pFunc ){` |
|      516 | 10175 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      516 | 10176 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      516 | 10177 | `			ph7_value_reset_string_cursor(pValue);` |
|      257 | 10178 | `		}` |
|        - | 10179 | `		/* Function arguments */` |
|      516 | 10180 | `		pArg = ph7_context_new_array(pCtx);` |
|      516 | 10181 | `		if( pArg  ){` |
|        - | 10182 | `			ph7_value *pObj;` |
|        - | 10183 | `			VmSlot *aSlot;` |
|        - | 10184 | `			sxu32 n;` |
|        - | 10185 | `			/* Start filling the array with the given arguments */` |
|      516 | 10186 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2050 | 10187 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1536 | 10188 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1536 | 10189 | `				if( pObj ){` |
|     1536 | 10190 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      767 | 10191 | `				}` |
|      769 | 10192 | `			}` |
|        - | 10193 | `			/* Save the array */` |
|      516 | 10194 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      257 | 10195 | `		}` |
|      257 | 10196 | `	}` |
|      516 | 10197 | `	ph7_value_int(pValue,1);` |
|        - | 10198 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10199 | `	 * line numbers at run-time. )` |
|        - | 10200 | `	 */` |
|      516 | 10201 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10202 | `	/* Current processed script */` |
|      516 | 10203 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      516 | 10204 | `	if( pFile ){` |
|      516 | 10205 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      516 | 10206 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      516 | 10207 | `		ph7_value_reset_string_cursor(pValue);` |
|      257 | 10208 | `	}` |
|        - | 10209 | `	/* Top class */` |
|      516 | 10210 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      516 | 10211 | `	if( pClass ){` |
|      512 | 10212 | `		ph7_value_reset_string_cursor(pValue);` |
|      512 | 10213 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      512 | 10214 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      255 | 10215 | `	}` |
|        - | 10216 | `	/* Return the freshly created array */` |
|      516 | 10217 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10218 | `	/*` |
|        - | 10219 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10220 | `	 * as soon we return from this function.` |
|        - | 10221 | `	 */` |
|      516 | 10222 | `	return PH7_OK;` |
|      259 | 10223 |  |
|        - | 10224 | `/*` |
|        - | 10225 | ` * Generate a small backtrace.` |
|        - | 10226 | ` * Store the generated dump in the given BLOB` |
|        - | 10227 | ` */` |
|        4 | 10228 | `static int VmMiniBacktrace(` |
|        - | 10229 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10230 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10231 | `	)` |
|        1 | 10232 |  |
|        5 | 10233 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10234 | `	ph7_vm_func *pFunc;` |
|        - | 10235 | `	ph7_class *pClass;` |
|        - | 10236 | `	SyString *pFile;` |
|        - | 10237 | `	/* Called function */` |
|        5 | 10238 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10239 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10240 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10241 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10242 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10243 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10244 | `	}else{` |
|      ! 0 | 10245 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10246 | `	}` |
|        5 | 10247 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10248 | `	/* Current processed script */` |
|        5 | 10249 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10250 | `	if( pFile ){` |
|        5 | 10251 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10252 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10253 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10254 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10255 | `	}` |
|        - | 10256 | `	/* Top class */` |
|        5 | 10257 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10258 | `	if( pClass ){` |
|      ! 0 | 10259 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10260 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10261 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10262 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10263 | `	}` |
|        5 | 10264 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10265 | `	/* All done */` |
|        5 | 10266 | `	return SXRET_OK;` |
|        1 | 10267 |  |
|        - | 10268 | `/*` |
|        - | 10269 | ` * void debug_print_backtrace()` |
|        - | 10270 | ` *  Prints a backtrace` |
|        - | 10271 | ` * Parameters` |
|        - | 10272 | ` * None` |
|        - | 10273 | ` * Return` |
|        - | 10274 | ` * NULL` |
|        - | 10275 | ` */` |
|        2 | 10276 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10277 |  |
|        3 | 10278 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10279 | `	SyBlob sDump;` |
|        3 | 10280 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10281 | `	/* Generate the backtrace */` |
|        3 | 10282 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10283 | `	/* Output backtrace */` |
|        3 | 10284 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10285 | `	/* All done,cleanup */` |
|        3 | 10286 | `	SyBlobRelease(&sDump);` |
|        1 | 10287 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10288 | `	SXUNUSED(apArg);` |
|        3 | 10289 | `	return PH7_OK;` |
|        1 | 10290 |  |
|        - | 10291 | `/*` |
|        - | 10292 | ` * string debug_string_backtrace()` |
|        - | 10293 | ` *  Generate a backtrace` |
|        - | 10294 | ` * Parameters` |
|        - | 10295 | ` * None` |
|        - | 10296 | ` * Return` |
|        - | 10297 | ` *  A mini backtrace().` |
|        - | 10298 | ` * Note that this is a symisc extension.` |
|        - | 10299 | ` */` |
|        2 | 10300 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10301 |  |
|        3 | 10302 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10303 | `	SyBlob sDump;` |
|        3 | 10304 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10305 | `	/* Generate the backtrace */` |
|        3 | 10306 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10307 | `	/* Return the backtrace */` |
|        3 | 10308 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10309 | `	/* All done,cleanup */` |
|        3 | 10310 | `	SyBlobRelease(&sDump);` |
|        1 | 10311 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10312 | `	SXUNUSED(apArg);` |
|        3 | 10313 | `	return PH7_OK;` |
|        1 | 10314 |  |
|        - | 10315 | `/*` |
|        - | 10316 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10317 | ` * exception is triggered.` |
|        - | 10318 | ` */` |
|      472 | 10319 | `static sxi32 VmUncaughtException(` |
|        - | 10320 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10321 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10322 | `	)` |
|        1 | 10323 |  |
|        - | 10324 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10325 | `	int nArg = 1;` |
|        - | 10326 | `	sxi32 rc;` |
|      473 | 10327 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10328 | `		/* Nesting limit reached */` |
|      ! 0 | 10329 | `		return SXRET_OK;` |
|        - | 10330 | `	}` |
|        - | 10331 | `	/* Call any exception handler if available */` |
|      473 | 10332 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10333 | `	if( pThis ){` |
|        - | 10334 | `		/* Load the exception instance */` |
|      473 | 10335 | `		sArg.x.pOther = pThis;` |
|      473 | 10336 | `		pThis->iRef++;` |
|      473 | 10337 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10338 | `	}else{` |
|      ! 0 | 10339 | `		nArg = 0;` |
|        - | 10340 | `	}` |
|      473 | 10341 | `	apArg[0] = &sArg;` |
|        - | 10342 | `	/* Call the exception handler if available */` |
|      473 | 10343 | `	pVm->nExceptDepth++;` |
|      473 | 10344 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10345 | `	pVm->nExceptDepth--;` |
|      473 | 10346 | `	if( rc != SXRET_OK ){` |
|        - | 10347 | `		SyBlob sMsgBuf;` |
|      471 | 10348 | `		const char *zClass = "Exception";` |
|      471 | 10349 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10350 | `		const char *zMsg;` |
|        - | 10351 | `		sxu32 nMsg;` |
|        - | 10352 | `		const char *zFuncName;` |
|        - | 10353 | `		int nFuncLen;` |
|      471 | 10354 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10355 | `		if( pThis ){` |
|        - | 10356 | `			ph7_class_method *pGetMessage;` |
|        - | 10357 | `			ph7_value sMsg;` |
|        - | 10358 | `			const char *zTmp;` |
|        - | 10359 | `			int nTmp;` |
|      471 | 10360 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10361 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10362 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10363 | `			if( pGetMessage ){` |
|      471 | 10364 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10365 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10366 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10367 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10368 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10369 | `					}` |
|      235 | 10370 | `				}` |
|      471 | 10371 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10372 | `			}` |
|      235 | 10373 | `		}` |
|      471 | 10374 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10375 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10376 | `		}` |
|      471 | 10377 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10378 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10379 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10380 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10381 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10382 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10383 | `		rc = SXERR_ABORT;` |
|      235 | 10384 | `	}` |
|      473 | 10385 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10386 | `	return rc;` |
|      237 | 10387 |  |
|        - | 10388 | `/*` |
|        - | 10389 | ` * Throw a user exception.` |
|        - | 10390 | ` *` |
|        - | 10391 | ` * Exception dispatch follows this sequence:` |
|        - | 10392 | ` *` |
|        - | 10393 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10394 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10395 | ` *` |
|        - | 10396 | ` * 2. If NO catch matches:` |
|        - | 10397 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10398 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10399 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10400 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10401 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10402 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10403 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10404 | ` *` |
|        - | 10405 | ` * 3. If a catch DOES match:` |
|        - | 10406 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10407 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10408 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10409 | ` *       finally block.` |
|        - | 10410 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10411 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10412 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10413 | ` *       in pPendingException (step 2c).` |
|        - | 10414 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10415 | ` *    d. Run finally (if present).` |
|        - | 10416 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10417 | ` *       that handlers are restored and finally has run.` |
|        - | 10418 | ` */` |
|      514 | 10419 | `static sxi32 VmThrowException(` |
|        - | 10420 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10421 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10422 | `	)` |
|        2 | 10423 |  |
|        - | 10424 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10425 | `	ph7_exception **apException;` |
|        - | 10426 | `	ph7_exception *pException;` |
|        - | 10427 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10428 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10429 | `	pException = 0;` |
|      516 | 10430 | `	pCatch = 0;` |
|      516 | 10431 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10432 | `		ph7_exception_block *aCatch;` |
|        - | 10433 | `		ph7_class *pClass;` |
|        - | 10434 | `		sxu32 j;` |
|        - | 10435 | `		/* Locate the appropriate block to execute */` |
|       40 | 10436 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10437 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10438 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10439 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10440 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10441 | `			/* Extract the target class */` |
|       38 | 10442 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10443 | `			if( pClass == 0 ){` |
|        - | 10444 | `				/* No such class */` |
|      ! 0 | 10445 | `				continue;` |
|        - | 10446 | `			}` |
|       38 | 10447 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10448 | `				/* Catch block found,break immeditaley */` |
|       38 | 10449 | `				pCatch = &aCatch[j];` |
|       38 | 10450 | `				break;` |
|        - | 10451 | `			}` |
|      ! 0 | 10452 | `		}` |
|       19 | 10453 | `	}` |
|        - | 10454 | `	/* Execute the cached block if available */` |
|      516 | 10455 | `	if( pCatch == 0 ){` |
|        - | 10456 | `		sxi32 rc;` |
|        - | 10457 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10458 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10459 | `			pException->iFinallyDone = 1;` |
|        3 | 10460 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10461 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10462 | `				return SXERR_ABORT;` |
|        - | 10463 | `			}` |
|        1 | 10464 | `		}` |
|        - | 10465 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10466 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10467 | `			/* Re-throw to the outer handler */` |
|        3 | 10468 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10469 | `		}` |
|        - | 10470 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10471 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10472 | `		 * exception instead of reporting it uncaught.` |
|        - | 10473 | `		 */` |
|      478 | 10474 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10475 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10476 | `			 * by looking for a catch frame on the stack.` |
|        - | 10477 | `			 */` |
|      478 | 10478 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10479 | `			int inCatch = 0;` |
|      956 | 10480 | `			while( pF ){` |
|      484 | 10481 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        5 | 10482 | `					inCatch = 1;` |
|        5 | 10483 | `					break;` |
|        - | 10484 | `				}` |
|      479 | 10485 | `				pF = pF->pParent;` |
|        1 | 10486 | `			}` |
|      478 | 10487 | `			if( inCatch ){` |
|        - | 10488 | `				/* Defer — will be re-thrown after finally runs */` |
|        5 | 10489 | `				pThis->iRef++;` |
|        5 | 10490 | `				pVm->pPendingException = pThis;` |
|        5 | 10491 | `				return SXRET_OK;` |
|        - | 10492 | `			}` |
|      236 | 10493 | `		}` |
|        - | 10494 | `		/* Truly uncaught */` |
|      473 | 10495 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10496 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10497 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10498 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10499 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10500 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10501 | `			}` |
|      ! 0 | 10502 | `		}` |
|      473 | 10503 | `		return rc;` |
|      ! 0 | 10504 | `	}else{` |
|       38 | 10505 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10506 | `		ph7_exception **apSaved = 0;` |
|        - | 10507 | `		sxu32 nSavedCount;` |
|        - | 10508 | `		sxi32 rc;` |
|       38 | 10509 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10510 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10511 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10512 | `		}` |
|        - | 10513 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10514 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10515 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10516 | `		 */` |
|       38 | 10517 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10518 | `		if( nSavedCount > 0 ){` |
|       10 | 10519 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10520 | `				nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10521 | `			if( apSaved ){` |
|       10 | 10522 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10523 | `					nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10524 | `				SySetReset(&pVm->aException);` |
|        3 | 10525 | `			}` |
|        3 | 10526 | `		}` |
|        - | 10527 | `		/* Create a private frame first */` |
|       38 | 10528 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10529 | `		if( rc == SXRET_OK ){` |
|       38 | 10530 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10531 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10532 | `			if( pObj ){` |
|       38 | 10533 | `				pThis->iRef++;` |
|       38 | 10534 | `				pObj->x.pOther = pThis;` |
|       38 | 10535 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10536 | `			}` |
|        - | 10537 | `			/* Execute the catch block */` |
|       38 | 10538 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10539 | `			/* Leave the frame */` |
|       38 | 10540 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10541 | `		}` |
|        - | 10542 | `		/* Restore the outer exception handlers */` |
|       38 | 10543 | `		if( apSaved ){` |
|        - | 10544 | `			sxu32 k;` |
|        - | 10545 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10546 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10547 | `			 * Restore the original outer entries.` |
|        - | 10548 | `			 */` |
|        7 | 10549 | `			SySetReset(&pVm->aException);` |
|       13 | 10550 | `			for(k = 0; k < nSavedCount; k++){` |
|        7 | 10551 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        4 | 10552 | `			}` |
|        7 | 10553 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10554 | `		}` |
|        - | 10555 | `		/* Execute the finally block after catch */` |
|       38 | 10556 | `		if( pException->iHasFinally ){` |
|       12 | 10557 | `			pException->iFinallyDone = 1;` |
|        - | 10558 | `			{` |
|       12 | 10559 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       12 | 10560 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10561 | `					return SXERR_ABORT;` |
|        - | 10562 | `				}` |
|        - | 10563 | `			}` |
|        5 | 10564 | `		}` |
|       38 | 10565 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10566 | `			return SXERR_ABORT;` |
|        - | 10567 | `		}` |
|        - | 10568 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10569 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10570 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10571 | `		 */` |
|       38 | 10572 | `		if( pVm->pPendingException ){` |
|        5 | 10573 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        5 | 10574 | `			pVm->pPendingException = 0;` |
|        5 | 10575 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10576 | `		}` |
|        - | 10577 | `	}` |
|        - | 10578 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10579 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10580 | `	 */` |
|       34 | 10581 | `	return SXRET_OK;` |
|      259 | 10582 |  |
|        - | 10583 | `/*` |
|        - | 10584 | ` * Section:` |
|        - | 10585 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10586 | ` * Status:` |
|        - | 10587 | ` *    Stable.` |
|        - | 10588 | ` */` |
|        - | 10589 | `/*` |
|        - | 10590 | ` * string ph7version(void)` |
|        - | 10591 | ` *  Returns the running version of the PH7 version.` |
|        - | 10592 | ` * Parameters` |
|        - | 10593 | ` *  None` |
|        - | 10594 | ` * Return` |
|        - | 10595 | ` * Current PH7 version.` |
|        - | 10596 | ` */` |
|        2 | 10597 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10598 |  |
|        1 | 10599 | `	SXUNUSED(nArg);` |
|        1 | 10600 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10601 | `	/* Current engine version */` |
|        3 | 10602 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10603 | `	return PH7_OK;` |
|        1 | 10604 |  |
|        - | 10605 | `/*` |
|        - | 10606 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10607 | ` */` |
|        - | 10608 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10609 | ` "<html><head>"\` |
|        - | 10610 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10611 | ` "<style type=\"text/css\">"\` |
|        - | 10612 | ` "div {"\` |
|        - | 10613 | `     "border: 1px solid #cccccc;"\` |
|        - | 10614 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10615 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10616 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10617 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10618 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10619 | `     "-o-border-radius: 10px;"\` |
|        - | 10620 | `     "border-radius: 10px;"\` |
|        - | 10621 | `     "padding-left: 2em;"\` |
|        - | 10622 | `     "background-color: white;"\` |
|        - | 10623 | `     "margin-left: auto;"\` |
|        - | 10624 | `     "font-family: verdana;"\` |
|        - | 10625 | `     "padding-right: 2em;"\` |
|        - | 10626 | `     "margin-right: auto;"\` |
|        - | 10627 | `     "}"\` |
|        - | 10628 | `     "body {"\` |
|        - | 10629 | `     "padding: 0.2em;"\` |
|        - | 10630 | `     "font-style: normal;"\` |
|        - | 10631 | `     "font-size: medium;"\` |
|        - | 10632 | `     "background-color: #f2f2f2;"\` |
|        - | 10633 | `     "}"\` |
|        - | 10634 | `     "hr {"\` |
|        - | 10635 | `     "border-style: solid none none;"\` |
|        - | 10636 | `     "border-width: 1px medium medium;"\` |
|        - | 10637 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10638 | `     "height: 1px;"\` |
|        - | 10639 | `     "}"\` |
|        - | 10640 | `     "a {"\` |
|        - | 10641 | `     "color: #3366cc;"\` |
|        - | 10642 | `     "text-decoration: none;"\` |
|        - | 10643 | `     "}"\` |
|        - | 10644 | `     "a:hover {"\` |
|        - | 10645 | `     "color: #999999;"\` |
|        - | 10646 | `     "}"\` |
|        - | 10647 | `     "a:active {"\` |
|        - | 10648 | `     "color: #663399;"\` |
|        - | 10649 | `     "}"\` |
|        - | 10650 | `     "h1 {"\` |
|        - | 10651 | `     "margin: 0;"\` |
|        - | 10652 | `     "padding: 0;"\` |
|        - | 10653 | `     "font-family: Verdana;"\` |
|        - | 10654 | `     "font-weight: bold;"\` |
|        - | 10655 | `     "font-style: normal;"\` |
|        - | 10656 | `     "font-size: medium;"\` |
|        - | 10657 | `     "text-transform: capitalize;"\` |
|        - | 10658 | `     "color: #0a328c;"\` |
|        - | 10659 | `     "}"\` |
|        - | 10660 | `     "p {"\` |
|        - | 10661 | `     "margin: 0 auto;"\` |
|        - | 10662 | `     "font-size: medium;"\` |
|        - | 10663 | `     "font-style: normal;"\` |
|        - | 10664 | `     "font-family: verdana;"\` |
|        - | 10665 | `     "}"\` |
|        - | 10666 | `"</style></head><body>"\` |
|        - | 10667 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10668 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10669 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10670 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10671 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10672 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10673 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10674 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10675 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10676 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10677 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10678 |  |
|        - | 10679 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10680 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10681 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10682 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10683 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10684 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10685 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10686 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10687 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10688 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10689 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10690 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10691 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10692 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10693 |  |
|        - | 10694 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10695 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10696 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10697 | `"&nbsp;*<br>"\` |
|        - | 10698 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10699 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10700 | `"&nbsp;* are met:<br>"\` |
|        - | 10701 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10702 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10703 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10704 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10705 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10706 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10707 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10708 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10709 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10710 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10711 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10712 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10713 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10714 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10715 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10716 | `"&nbsp;*<br>"\` |
|        - | 10717 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10718 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10719 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10720 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10721 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10722 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10723 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10724 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10725 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10726 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10727 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10728 | `"&nbsp;*/<br>"\` |
|        - | 10729 | `"</span></small></small></p>"\` |
|        - | 10730 | `"</div></body></html>"` |
|        - | 10731 | `/*` |
|        - | 10732 | ` * bool ph7credits(void)` |
|        - | 10733 | ` * bool ph7info(void)` |
|        - | 10734 | ` * bool ph7copyright(void)` |
|        - | 10735 | ` *  Prints out the credits for PH7 engine` |
|        - | 10736 | ` * Parameters` |
|        - | 10737 | ` *  None` |
|        - | 10738 | ` * Return` |
|        - | 10739 | ` *  Always TRUE` |
|        - | 10740 | ` */` |
|        2 | 10741 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10742 |  |
|        3 | 10743 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10744 | `	/* Expand the HTML page above*/` |
|        3 | 10745 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10746 | `	ph7_context_output_format(` |
|        1 | 10747 | `		pCtx,` |
|        - | 10748 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10749 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10750 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10751 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10752 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10753 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10754 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10755 | `#ifdef __WINNT__` |
|        - | 10756 | `		"Windows NT"` |
|        - | 10757 | `#elif defined(__UNIXES__)` |
|        - | 10758 | `		"UNIX-Like"` |
|        - | 10759 | `#else` |
|        - | 10760 | `		"Other OS"` |
|        - | 10761 | `#endif` |
|        - | 10762 | `		);` |
|        3 | 10763 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10764 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10765 | `	SXUNUSED(apArg);` |
|        - | 10766 | `	/* Return TRUE */` |
|        - | 10767 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10768 | `	return PH7_OK;` |
|        1 | 10769 |  |
|        - | 10770 | `/*` |
|        - | 10771 | ` * Section:` |
|        - | 10772 | ` *    URL related routines.` |
|        - | 10773 | ` * Status:` |
|        - | 10774 | ` *    Stable.` |
|        - | 10775 | ` */` |
|        - | 10776 | `/*` |
|        - | 10777 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10778 | ` *  Parse a URL and return its fields.` |
|        - | 10779 | ` * Parameters` |
|        - | 10780 | ` *  $url` |
|        - | 10781 | ` *   The URL to parse.` |
|        - | 10782 | ` * $component` |
|        - | 10783 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10784 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10785 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10786 | ` *  in which case the return value will be an integer).` |
|        - | 10787 | ` * Return` |
|        - | 10788 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10789 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10790 | ` *  this array are:` |
|        - | 10791 | ` *   scheme - e.g. http` |
|        - | 10792 | ` *   host` |
|        - | 10793 | ` *   port` |
|        - | 10794 | ` *   user` |
|        - | 10795 | ` *   pass` |
|        - | 10796 | ` *   path` |
|        - | 10797 | ` *   query - after the question mark ?` |
|        - | 10798 | ` *   fragment - after the hashmark #` |
|        - | 10799 | ` * Note:` |
|        - | 10800 | ` *  FALSE is returned on failure.` |
|        - | 10801 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10802 | ` *  with the standard PHP engine.` |
|        - | 10803 | ` */` |
|       28 | 10804 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10805 |  |
|        - | 10806 | `	const char *zStr; /* Input string */` |
|        - | 10807 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10808 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10809 | `	int nLen;` |
|        - | 10810 | `	sxi32 rc;` |
|       29 | 10811 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10812 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10813 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10814 | `		return PH7_OK;` |
|        - | 10815 | `	}` |
|        - | 10816 | `	/* Extract the given URI */` |
|       29 | 10817 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10818 | `	if( nLen < 1 ){` |
|        - | 10819 | `		/* Nothing to process,return FALSE */` |
|        3 | 10820 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10821 | `		return PH7_OK;` |
|        - | 10822 | `	}` |
|        - | 10823 | `	/* Get a parse */` |
|       27 | 10824 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10825 | `	if( rc != SXRET_OK ){` |
|        - | 10826 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10827 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10828 | `		return PH7_OK;` |
|        - | 10829 | `	}` |
|       27 | 10830 | `	if( nArg > 1 ){` |
|      ! 0 | 10831 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10832 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10833 | `		switch(nComponent){` |
|      ! 0 | 10834 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10835 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10836 | `			if( pComp->nByte < 1 ){` |
|        - | 10837 | `				/* No available value,return NULL */` |
|      ! 0 | 10838 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10839 | `			}else{` |
|      ! 0 | 10840 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10841 | `			}` |
|      ! 0 | 10842 | `			break;` |
|      ! 0 | 10843 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10844 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10845 | `			if( pComp->nByte < 1 ){` |
|        - | 10846 | `				/* No available value,return NULL */` |
|      ! 0 | 10847 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10848 | `			}else{` |
|      ! 0 | 10849 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10850 | `			}` |
|      ! 0 | 10851 | `			break;` |
|      ! 0 | 10852 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10853 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10854 | `			if( pComp->nByte < 1 ){` |
|        - | 10855 | `				/* No available value,return NULL */` |
|      ! 0 | 10856 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10857 | `			}else{` |
|      ! 0 | 10858 | `				int iPort = 0;` |
|        - | 10859 | `				/* Cast the value to integer */` |
|      ! 0 | 10860 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10861 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10862 | `			}` |
|      ! 0 | 10863 | `			break;` |
|      ! 0 | 10864 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10865 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10866 | `			if( pComp->nByte < 1 ){` |
|        - | 10867 | `				/* No available value,return NULL */` |
|      ! 0 | 10868 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10869 | `			}else{` |
|      ! 0 | 10870 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10871 | `			}` |
|      ! 0 | 10872 | `			break;` |
|      ! 0 | 10873 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10874 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10875 | `			if( pComp->nByte < 1 ){` |
|        - | 10876 | `				/* No available value,return NULL */` |
|      ! 0 | 10877 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10878 | `			}else{` |
|      ! 0 | 10879 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10880 | `			}` |
|      ! 0 | 10881 | `			break;` |
|      ! 0 | 10882 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10883 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10884 | `			if( pComp->nByte < 1 ){` |
|        - | 10885 | `				/* No available value,return NULL */` |
|      ! 0 | 10886 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10887 | `			}else{` |
|      ! 0 | 10888 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10889 | `			}` |
|      ! 0 | 10890 | `			break;` |
|      ! 0 | 10891 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10892 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10893 | `			if( pComp->nByte < 1 ){` |
|        - | 10894 | `				/* No available value,return NULL */` |
|      ! 0 | 10895 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10896 | `			}else{` |
|      ! 0 | 10897 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10898 | `			}` |
|      ! 0 | 10899 | `			break;` |
|      ! 0 | 10900 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10901 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10902 | `			if( pComp->nByte < 1 ){` |
|        - | 10903 | `				/* No available value,return NULL */` |
|      ! 0 | 10904 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10905 | `			}else{` |
|      ! 0 | 10906 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10907 | `			}` |
|      ! 0 | 10908 | `			break;` |
|      ! 0 | 10909 | `		default:` |
|        - | 10910 | `			/* No such entry,return NULL */` |
|      ! 0 | 10911 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10912 | `			break;` |
|        - | 10913 | `		}` |
|      ! 0 | 10914 | `	}else{` |
|        - | 10915 | `		ph7_value *pArray,*pValue;` |
|        - | 10916 | `		/* Return an associative array */` |
|       27 | 10917 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10918 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10919 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10920 | `			/* Out of memory */` |
|      ! 0 | 10921 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10922 | `			/* Return false */` |
|      ! 0 | 10923 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10924 | `			return PH7_OK;` |
|        - | 10925 | `		}` |
|        - | 10926 | `		/* Fill the array */` |
|       27 | 10927 | `		pComp = &sURI.sScheme;` |
|       27 | 10928 | `		if( pComp->nByte > 0 ){` |
|       19 | 10929 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10930 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10931 | `		}` |
|        - | 10932 | `		/* Reset the string cursor */` |
|       27 | 10933 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10934 | `		pComp = &sURI.sHost;` |
|       27 | 10935 | `		if( pComp->nByte > 0 ){` |
|       25 | 10936 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10937 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10938 | `		}` |
|        - | 10939 | `		/* Reset the string cursor */` |
|       27 | 10940 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10941 | `		pComp = &sURI.sPort;` |
|       27 | 10942 | `		if( pComp->nByte > 0 ){` |
|       11 | 10943 | `			int iPort = 0;/* cc warning */` |
|        - | 10944 | `			/* Convert to integer */` |
|       11 | 10945 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10946 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10947 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10948 | `		}` |
|        - | 10949 | `		/* Reset the string cursor */` |
|       27 | 10950 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10951 | `		pComp = &sURI.sUser;` |
|       27 | 10952 | `		if( pComp->nByte > 0 ){` |
|        7 | 10953 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10954 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10955 | `		}` |
|        - | 10956 | `		/* Reset the string cursor */` |
|       27 | 10957 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10958 | `		pComp = &sURI.sPass;` |
|       27 | 10959 | `		if( pComp->nByte > 0 ){` |
|        7 | 10960 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10961 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10962 | `		}` |
|        - | 10963 | `		/* Reset the string cursor */` |
|       27 | 10964 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10965 | `		pComp = &sURI.sPath;` |
|       27 | 10966 | `		if( pComp->nByte > 0 ){` |
|       17 | 10967 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10968 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10969 | `		}` |
|        - | 10970 | `		/* Reset the string cursor */` |
|       27 | 10971 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10972 | `		pComp = &sURI.sQuery;` |
|       27 | 10973 | `		if( pComp->nByte > 0 ){` |
|        5 | 10974 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10975 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10976 | `		}` |
|        - | 10977 | `		/* Reset the string cursor */` |
|       27 | 10978 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10979 | `		pComp = &sURI.sFragment;` |
|       27 | 10980 | `		if( pComp->nByte > 0 ){` |
|        5 | 10981 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10982 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10983 | `		}` |
|        - | 10984 | `		/* Return the created array */` |
|       27 | 10985 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10986 | `		/* NOTE:` |
|        - | 10987 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10988 | `		 * automatically as soon we return from this function.` |
|        - | 10989 | `		 */` |
|        - | 10990 | `	}` |
|        - | 10991 | `	/* All done */` |
|       27 | 10992 | `	return PH7_OK;` |
|       15 | 10993 |  |
|        - | 10994 | `/*` |
|        - | 10995 | ` * Section:` |
|        - | 10996 | ` *   Array related routines.` |
|        - | 10997 | ` * Status:` |
|        - | 10998 | ` *    Stable.` |
|        - | 10999 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11000 | ` *  Array related functions that need access to the underlying` |
|        - | 11001 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11002 | ` */` |
|        - | 11003 | `/*` |
|        - | 11004 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11005 | ` * of the following structure.` |
|        - | 11006 | ` */` |
|        - | 11007 | `struct compact_data` |
|        - | 11008 |  |
|        - | 11009 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11010 | `	int nRecCount;      /* Recursion count */` |
|        - | 11011 | `};` |
|        - | 11012 | `/*` |
|        - | 11013 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11014 | ` */` |
|      ! 0 | 11015 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11016 |  |
|      ! 0 | 11017 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11018 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11019 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11020 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11021 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11022 | `		SyString sVar;` |
|      ! 0 | 11023 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11024 | `		if( sVar.nByte > 0 ){` |
|        - | 11025 | `			/* Query the current frame */` |
|      ! 0 | 11026 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11027 | `			/* ^` |
|        - | 11028 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11029 | `			 */` |
|      ! 0 | 11030 | `			if( pKey ){` |
|        - | 11031 | `				/* Perform the insertion */` |
|      ! 0 | 11032 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11033 | `			}` |
|      ! 0 | 11034 | `		}` |
|      ! 0 | 11035 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11036 | `		int rc;` |
|        - | 11037 | `		/* Recursively traverse this array */` |
|      ! 0 | 11038 | `		pData->nRecCount++;` |
|      ! 0 | 11039 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11040 | `		pData->nRecCount--;` |
|      ! 0 | 11041 | `		return rc;` |
|        - | 11042 | `	}` |
|      ! 0 | 11043 | `	return SXRET_OK;` |
|      ! 0 | 11044 |  |
|        - | 11045 | `/*` |
|        - | 11046 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11047 | ` *  Create array containing variables and their values.` |
|        - | 11048 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11049 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11050 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11051 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11052 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11053 | ` * Parameters` |
|        - | 11054 | ` *  $varname` |
|        - | 11055 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11056 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11057 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11058 | ` *   it recursively.` |
|        - | 11059 | ` * Return` |
|        - | 11060 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11061 | ` */` |
|        2 | 11062 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11063 |  |
|        - | 11064 | `	ph7_value *pArray,*pObj;` |
|        3 | 11065 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11066 | `	const char *zName;` |
|        - | 11067 | `	SyString sVar;` |
|        - | 11068 | `	int i,nLen;` |
|        3 | 11069 | `	if( nArg < 1 ){` |
|        - | 11070 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11071 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11072 | `		return PH7_OK;` |
|        - | 11073 | `	}` |
|        - | 11074 | `	/* Create the array */` |
|        3 | 11075 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11076 | `	if( pArray == 0 ){` |
|        - | 11077 | `		/* Out of memory */` |
|      ! 0 | 11078 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11079 | `		/* Return NULL */` |
|      ! 0 | 11080 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11081 | `		return PH7_OK;` |
|        - | 11082 | `	}` |
|        - | 11083 | `	/* Perform the requested operation */` |
|        7 | 11084 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11085 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11086 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11087 | `				struct compact_data sData;` |
|      ! 0 | 11088 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11089 | `				/* Recursively walk the array */` |
|      ! 0 | 11090 | `				sData.nRecCount = 0;` |
|      ! 0 | 11091 | `				sData.pArray = pArray;` |
|      ! 0 | 11092 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11093 | `			}` |
|      ! 0 | 11094 | `		}else{` |
|        - | 11095 | `			/* Extract variable name */` |
|        5 | 11096 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11097 | `			if( nLen > 0 ){` |
|        5 | 11098 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11099 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11100 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11101 | `				if( pObj ){` |
|        5 | 11102 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11103 | `				}` |
|        2 | 11104 | `			}` |
|        - | 11105 | `		}` |
|        3 | 11106 | `	}` |
|        - | 11107 | `	/* Return the array */` |
|        3 | 11108 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11109 | `	return PH7_OK;` |
|        2 | 11110 |  |
|        - | 11111 | `/*` |
|        - | 11112 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11113 | ` * of the following structure.` |
|        - | 11114 | ` */` |
|        - | 11115 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11116 | `struct extract_aux_data` |
|        - | 11117 |  |
|        - | 11118 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11119 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11120 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11121 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11122 | `	int iFlags;           /* Control flags */` |
|        - | 11123 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11124 | `};` |
|        - | 11125 | `/* Forward declaration */` |
|        - | 11126 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11127 | `/*` |
|        - | 11128 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11129 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11130 | ` * Parameters` |
|        - | 11131 | ` * $var_array` |
|        - | 11132 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11133 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11134 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11135 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11136 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11137 | ` * $extract_type` |
|        - | 11138 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11139 | ` *  It can be one of the following values:` |
|        - | 11140 | ` *   EXTR_OVERWRITE` |
|        - | 11141 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11142 | ` *   EXTR_SKIP` |
|        - | 11143 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11144 | ` *   EXTR_PREFIX_SAME` |
|        - | 11145 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11146 | ` *   EXTR_PREFIX_ALL` |
|        - | 11147 | ` *       Prefix all variable names with prefix.` |
|        - | 11148 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11149 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11150 | ` *   EXTR_IF_EXISTS` |
|        - | 11151 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11152 | ` *       otherwise do nothing.` |
|        - | 11153 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11154 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11155 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11156 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11157 | ` *      the current symbol table.` |
|        - | 11158 | ` * $prefix` |
|        - | 11159 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11160 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11161 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11162 | ` *  underscore character.` |
|        - | 11163 | ` * Return` |
|        - | 11164 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11165 | ` */` |
|        4 | 11166 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11167 |  |
|        - | 11168 | `	extract_aux_data sAux;` |
|        - | 11169 | `	ph7_hashmap *pMap;` |
|        5 | 11170 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11171 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11172 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11173 | `		return PH7_OK;` |
|        - | 11174 | `	}` |
|        - | 11175 | `	/* Point to the target hashmap */` |
|        5 | 11176 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11177 | `	if( pMap->nEntry < 1 ){` |
|        - | 11178 | `		/* Empty map,return  0 */` |
|      ! 0 | 11179 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11180 | `		return PH7_OK;` |
|        - | 11181 | `	}` |
|        - | 11182 | `	/* Prepare the aux data */` |
|        5 | 11183 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11184 | `	if( nArg > 1 ){` |
|        3 | 11185 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11186 | `		if( nArg > 2 ){` |
|      ! 0 | 11187 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11188 | `		}` |
|        1 | 11189 | `	}` |
|        5 | 11190 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11191 | `	/* Invoke the worker callback */` |
|        5 | 11192 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11193 | `	/* Number of variables successfully imported */` |
|        5 | 11194 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11195 | `	return PH7_OK;` |
|        3 | 11196 |  |
|        - | 11197 | `/*` |
|        - | 11198 | ` * Worker callback for the [extract()] function defined` |
|        - | 11199 | ` * below.` |
|        - | 11200 | ` */` |
|        8 | 11201 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11202 |  |
|        9 | 11203 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11204 | `	int iFlags = pAux->iFlags;` |
|        9 | 11205 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11206 | `	ph7_value *pObj;` |
|        - | 11207 | `	SyString sVar;` |
|        9 | 11208 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11209 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11210 | `	}` |
|        - | 11211 | `	/* Perform a string cast */` |
|        9 | 11212 | `	PH7_MemObjToString(pKey);` |
|        9 | 11213 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11214 | `		/* Unavailable variable name */` |
|      ! 0 | 11215 | `		return SXRET_OK;` |
|        - | 11216 | `	}` |
|        9 | 11217 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11218 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11219 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11220 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11221 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11222 | `			);` |
|      ! 0 | 11223 | `	}else{` |
|       13 | 11224 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11225 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11226 | `	}` |
|        9 | 11227 | `	sVar.zString = pAux->zWorker;` |
|        - | 11228 | `	/* Try to extract the variable */` |
|        9 | 11229 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11230 | `	if( pObj ){` |
|        - | 11231 | `		/* Collision */` |
|        5 | 11232 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11233 | `			return SXRET_OK;` |
|        - | 11234 | `		}` |
|        5 | 11235 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11236 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11237 | `				/* Already prefixed */` |
|      ! 0 | 11238 | `				return SXRET_OK;` |
|        - | 11239 | `			}` |
|      ! 0 | 11240 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11241 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11242 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11243 | `				);` |
|      ! 0 | 11244 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11245 | `		}` |
|        3 | 11246 | `	}else{` |
|        - | 11247 | `		/* Create the variable */` |
|        5 | 11248 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11249 | `	}` |
|        9 | 11250 | `	if( pObj ){` |
|        - | 11251 | `		/* Overwrite the old value */` |
|        9 | 11252 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11253 | `		/* Increment counter */` |
|        9 | 11254 | `		pAux->iCount++;` |
|        4 | 11255 | `	}` |
|        9 | 11256 | `	return SXRET_OK;` |
|        5 | 11257 |  |
|        - | 11258 | `/*` |
|        - | 11259 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11260 | ` * defined below.` |
|        - | 11261 | ` */` |
|        2 | 11262 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11263 |  |
|        3 | 11264 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11265 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11266 | `	ph7_value *pObj;` |
|        - | 11267 | `	SyString sVar;` |
|        - | 11268 | `	/* Perform a string cast */` |
|        3 | 11269 | `	PH7_MemObjToString(pKey);` |
|        3 | 11270 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11271 | `		/* Unavailable variable name */` |
|      ! 0 | 11272 | `		return SXRET_OK;` |
|        - | 11273 | `	}` |
|        3 | 11274 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11275 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11276 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11277 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11278 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11279 | `			);` |
|        2 | 11280 | `	}else{` |
|      ! 0 | 11281 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11282 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11283 | `	}` |
|        3 | 11284 | `	sVar.zString = pAux->zWorker;` |
|        - | 11285 | `	/* Extract the variable */` |
|        3 | 11286 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11287 | `	if( pObj ){` |
|        3 | 11288 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11289 | `	}` |
|        3 | 11290 | `	return SXRET_OK;` |
|        2 | 11291 |  |
|        - | 11292 | `/*` |
|        - | 11293 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11294 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11295 | ` * Parameters` |
|        - | 11296 | ` * $types` |
|        - | 11297 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11298 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11299 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11300 | ` *  POST includes the POST uploaded file information.` |
|        - | 11301 | ` *  Note:` |
|        - | 11302 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11303 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11304 | ` * $prefix` |
|        - | 11305 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11306 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11307 | ` *  variable named $pref_userid.` |
|        - | 11308 | ` * Return` |
|        - | 11309 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11310 | ` */` |
|        2 | 11311 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11312 |  |
|        - | 11313 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11314 | `	extract_aux_data sAux;` |
|        - | 11315 | `	int nLen,nPrefixLen;` |
|        - | 11316 | `	ph7_value *pSuper;` |
|        - | 11317 | `	ph7_vm *pVm;` |
|        - | 11318 | `	/* By default import only $_GET variables  */` |
|        3 | 11319 | `	zImport = "G";` |
|        3 | 11320 | `	nLen = (int)sizeof(char);` |
|        3 | 11321 | `	zPrefix = 0;` |
|        3 | 11322 | `	nPrefixLen = 0;` |
|        3 | 11323 | `	if( nArg > 0 ){` |
|        3 | 11324 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11325 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11326 | `		}` |
|        3 | 11327 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11328 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11329 | `		}` |
|        1 | 11330 | `	}` |
|        - | 11331 | `	/* Point to the underlying VM */` |
|        3 | 11332 | `	pVm = pCtx->pVm;` |
|        - | 11333 | `	/* Initialize the aux data */` |
|        3 | 11334 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11335 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11336 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11337 | `	sAux.pVm = pVm;` |
|        - | 11338 | `	/* Extract */` |
|        3 | 11339 | `	zEnd = &zImport[nLen];` |
|        5 | 11340 | `	while( zImport < zEnd ){` |
|        3 | 11341 | `		int c = zImport[0];` |
|        3 | 11342 | `		pSuper = 0;` |
|        3 | 11343 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11344 | `			/* Import $_GET variables */` |
|        3 | 11345 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11346 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11347 | `			/* Import $_POST variables */` |
|      ! 0 | 11348 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11349 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11350 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11351 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11352 | `		}` |
|        3 | 11353 | `		if( pSuper ){` |
|        - | 11354 | `			/* Iterate throw array entries */` |
|        3 | 11355 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11356 | `		}` |
|        - | 11357 | `		/* Advance the cursor */` |
|        3 | 11358 | `		zImport++;` |
|        1 | 11359 | `	}` |
|        - | 11360 | `	/* All done,return TRUE*/` |
|        3 | 11361 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11362 | `	return PH7_OK;` |
|        1 | 11363 |  |
|        - | 11364 | `/*` |
|        - | 11365 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11366 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11367 | ` * information.` |
|        - | 11368 | ` */` |
|    10610 | 11369 | `static sxi32 VmEvalChunk(` |
|        - | 11370 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11371 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11372 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11373 | `	int iFlags,         /* Compile flag */` |
|        - | 11374 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11375 | `	)` |
|        2 | 11376 |  |
|        - | 11377 | `	SySet *pByteCode,aByteCode;` |
|        - | 11378 | `	SyBlob sSavedNs;` |
|    10612 | 11379 | `	ProcConsumer xErr = 0;` |
|    10612 | 11380 | `	void *pErrData = 0;` |
|        - | 11381 | `	/* Initialize bytecode container */` |
|    10612 | 11382 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10612 | 11383 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11384 | `	/* Reset the code generator */` |
|    10612 | 11385 | `	if( bTrueReturn ){` |
|        - | 11386 | `		/* Included file,log compile-time errors */` |
|     8016 | 11387 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8016 | 11388 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4007 | 11389 | `	}` |
|    10612 | 11390 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11391 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11392 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11393 | `	 * the caller's namespace is restored. */` |
|    10612 | 11394 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10612 | 11395 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10612 | 11396 | `	if( bTrueReturn ){` |
|        - | 11397 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8016 | 11398 | `		SyBlobReset(&pVm->sNamespace);` |
|     4007 | 11399 | `	}` |
|        - | 11400 | `	/* Swap bytecode container */` |
|    10612 | 11401 | `	pByteCode = pVm->pByteContainer;` |
|    10612 | 11402 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11403 | `	/* Compile the chunk */` |
|    10612 | 11404 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15917 | 11405 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11406 | `		/* Compilation error,return false */` |
|        3 | 11407 | `		if( pCtx ){` |
|        3 | 11408 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11409 | `		}` |
|        2 | 11410 | `	}else{` |
|        - | 11411 | `		/* Mount any newly defined classes */` |
|        - | 11412 | `		SyHashEntry *pEntry;` |
|        - | 11413 | `		ph7_class *pClass;` |
|        - | 11414 | `		ph7_value sResult; /* Return value */` |
|        - | 11415 | `		sxi32 rc;` |
|    10610 | 11416 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   342242 | 11417 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   326330 | 11418 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11419 | `			/* Only mount classes that haven't been mounted yet */` |
|   326330 | 11420 | `			if( !pClass->bMounted ){` |
|    76302 | 11421 | `				rc = VmMountUserClass(pVm,pClass);` |
|    76302 | 11422 | `				if( rc != SXRET_OK ){` |
|        - | 11423 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11424 | `					if( pCtx ){` |
|      ! 0 | 11425 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11426 | `					}` |
|      ! 0 | 11427 | `					goto Cleanup;` |
|        - | 11428 | `				}` |
|    38150 | 11429 | `			}` |
|        2 | 11430 | `		}` |
|    10610 | 11431 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11432 | `			/* Out of memory */` |
|      ! 0 | 11433 | `			if( pCtx ){` |
|      ! 0 | 11434 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11435 | `			}` |
|      ! 0 | 11436 | `			goto Cleanup;` |
|        - | 11437 | `		}` |
|    10610 | 11438 | `		if( bTrueReturn ){` |
|        - | 11439 | `			/* Assume a boolean true return value */` |
|     8016 | 11440 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4009 | 11441 | `		}else{` |
|        - | 11442 | `			/* Assume a null return value */` |
|     2596 | 11443 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11444 | `		}` |
|        - | 11445 | `		/* Execute the compiled chunk */` |
|    10610 | 11446 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10610 | 11447 | `		if( pCtx ){` |
|        - | 11448 | `			/* Set the execution result */` |
|     8034 | 11449 | `			ph7_result_value(pCtx,&sResult);` |
|     4016 | 11450 | `		}` |
|    10610 | 11451 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11452 | `	}` |
|     5305 | 11453 | `Cleanup:` |
|        - | 11454 | `	/* Cleanup the mess left behind */` |
|    10612 | 11455 | `	pVm->pByteContainer = pByteCode;` |
|    10612 | 11456 | `	SySetRelease(&aByteCode);` |
|        - | 11457 | `	/* Restore caller's namespace state */` |
|    10612 | 11458 | `	SyBlobReset(&pVm->sNamespace);` |
|    10612 | 11459 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10612 | 11460 | `	SyBlobRelease(&sSavedNs);` |
|    10612 | 11461 | `	return SXRET_OK;` |
|        2 | 11462 |  |
|        - | 11463 | `/*` |
|        - | 11464 | ` * value eval(string $code)` |
|        - | 11465 | ` *   Evaluate a string as PHP code.` |
|        - | 11466 | ` * Parameter` |
|        - | 11467 | ` *  code: PHP code to evaluate.` |
|        - | 11468 | ` * Return` |
|        - | 11469 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11470 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11471 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11472 | ` */` |
|       22 | 11473 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11474 |  |
|        - | 11475 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11476 | `	if( nArg < 1 ){` |
|        - | 11477 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11478 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11479 | `		return SXRET_OK;` |
|        - | 11480 | `	}` |
|        - | 11481 | `	/* Chunk to evaluate */` |
|       24 | 11482 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11483 | `	if( sChunk.nByte < 1 ){` |
|        - | 11484 | `		/* Empty string,return NULL */` |
|        3 | 11485 | `		ph7_result_null(pCtx);` |
|        3 | 11486 | `		return SXRET_OK;` |
|        - | 11487 | `	}` |
|        - | 11488 | `	/* Eval the chunk */` |
|       22 | 11489 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11490 | `	return SXRET_OK;` |
|       13 | 11491 |  |
|        - | 11492 | `/*` |
|        - | 11493 | ` * Check if a file path is already included.` |
|        - | 11494 | ` */` |
|    16024 | 11495 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11496 |  |
|        - | 11497 | `	SyString *aEntries;` |
|        - | 11498 | `	sxu32 n;` |
|    16026 | 11499 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11500 | `	/* Perform a linear search */` |
| 64148224 | 11501 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 64132206 | 11502 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11503 | `			/* Already included */` |
|        7 | 11504 | `			return TRUE;` |
|        - | 11505 | `		}` |
| 32066101 | 11506 | `	}` |
|    16020 | 11507 | `	return FALSE;` |
|     8014 | 11508 |  |
|        - | 11509 | `/*` |
|        - | 11510 | ` * Push a file path in the appropriate VM container.` |
|        - | 11511 | ` */` |
|    18592 | 11512 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11513 |  |
|        - | 11514 | `	SyString sPath;` |
|        - | 11515 | `	char *zDup;` |
|        - | 11516 | `#ifdef __WINNT__` |
|        - | 11517 | `	char *zCur;` |
|        - | 11518 | `#endif` |
|        - | 11519 | `	sxi32 rc;` |
|    18594 | 11520 | `	if( nLen < 0 ){` |
|     2570 | 11521 | `		nLen = SyStrlen(zPath);` |
|     1284 | 11522 | `	}` |
|        - | 11523 | `	/* Duplicate the file path first */` |
|    18594 | 11524 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18594 | 11525 | `	if( zDup == 0 ){` |
|      ! 0 | 11526 | `		return SXERR_MEM;` |
|        - | 11527 | `	}` |
|        - | 11528 | `#ifdef __WINNT__` |
|        - | 11529 | `	/* Normalize path on windows` |
|        - | 11530 | `	 * Example:` |
|        - | 11531 | `	 *    Path/To/File.php` |
|        - | 11532 | `	 * becomes` |
|        - | 11533 | `	 *   path\to\file.php` |
|        - | 11534 | `	 */` |
|        2 | 11535 | `	zCur = zDup;` |
|        2 | 11536 | `	while( zCur[0] != 0 ){` |
|        2 | 11537 | `		if( zCur[0] == '/' ){` |
|        2 | 11538 | `			zCur[0] = '\\';` |
|        2 | 11539 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11540 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11541 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11542 | `		}` |
|        2 | 11543 | `		zCur++;` |
|        2 | 11544 | `	}` |
|        - | 11545 | `#endif` |
|        - | 11546 | `	/* Install the file path */` |
|    18594 | 11547 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18594 | 11548 | `	if( !bMain ){` |
|    16026 | 11549 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11550 | `			/* Already included */` |
|        7 | 11551 | `			*pNew = 0;` |
|        4 | 11552 | `		}else{` |
|        - | 11553 | `			/* Insert in the corresponding container */` |
|    16020 | 11554 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16020 | 11555 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11556 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11557 | `				return rc;` |
|        - | 11558 | `			}` |
|    16020 | 11559 | `			*pNew = 1;` |
|        - | 11560 | `		}` |
|     8012 | 11561 | `	}` |
|    18594 | 11562 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18594 | 11563 | `	return SXRET_OK;` |
|     9298 | 11564 |  |
|        - | 11565 | `/*` |
|        - | 11566 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11567 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11568 | ` * indicates failure.` |
|        - | 11569 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11570 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11571 | ` * operations.` |
|        - | 11572 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11573 | ` * this function is a no-op.` |
|        - | 11574 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11575 | ` * constructs for more information.` |
|        - | 11576 | ` */` |
|     8024 | 11577 | `static sxi32 VmExecIncludedFile(` |
|        - | 11578 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11579 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11580 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11581 | `	 )` |
|        2 | 11582 |  |
|        - | 11583 | `	sxi32 rc;` |
|        - | 11584 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11585 | `	const ph7_io_stream *pStream;` |
|        - | 11586 | `	SyBlob sContents;` |
|        - | 11587 | `	void *pHandle;` |
|        - | 11588 | `	ph7_vm *pVm;` |
|        - | 11589 | `	int isNew;` |
|        - | 11590 | `	/* Initialize fields */` |
|     8026 | 11591 | `	pVm = pCtx->pVm;` |
|     8026 | 11592 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8026 | 11593 | `	isNew = 0;` |
|        - | 11594 | `	/* Extract the associated stream */` |
|     8026 | 11595 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11596 | `	/*` |
|        - | 11597 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11598 | `	 * in a read-only mode.` |
|        - | 11599 | `	 */` |
|     8026 | 11600 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8026 | 11601 | `	if( pHandle == 0 ){` |
|        8 | 11602 | `		return SXERR_IO;` |
|        - | 11603 | `	}` |
|     8020 | 11604 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8020 | 11605 | `	if( IncludeOnce && !isNew ){` |
|        - | 11606 | `		/* Already included */` |
|        5 | 11607 | `		rc = SXERR_EXISTS;` |
|        3 | 11608 | `	}else{` |
|        - | 11609 | `		/* Read the whole file contents */` |
|     8016 | 11610 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8016 | 11611 | `		if( rc == SXRET_OK ){` |
|        - | 11612 | `			SyString sScript;` |
|        - | 11613 | `			/* Compile and execute the script */` |
|     8016 | 11614 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8016 | 11615 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4007 | 11616 | `		}` |
|        - | 11617 | `	}` |
|        - | 11618 | `	/* Pop from the set of included file */` |
|     8020 | 11619 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11620 | `	/* Close the handle */` |
|     8020 | 11621 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11622 | `	/* Release the working buffer */` |
|     8020 | 11623 | `	SyBlobRelease(&sContents);` |
|        - | 11624 | `#else` |
|        - | 11625 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11626 | `	SXUNUSED(pPath);` |
|        - | 11627 | `	SXUNUSED(IncludeOnce);` |
|        - | 11628 | `	rc = SXERR_IO;` |
|        - | 11629 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8020 | 11630 | `	return rc;` |
|     4014 | 11631 |  |
|        - | 11632 | `/*` |
|        - | 11633 | ` * string get_include_path(void)` |
|        - | 11634 | ` *  Gets the current include_path configuration option.` |
|        - | 11635 | ` * Parameter` |
|        - | 11636 | ` *  None` |
|        - | 11637 | ` * Return` |
|        - | 11638 | ` *  Included paths as a string` |
|        - | 11639 | ` */` |
|        2 | 11640 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11641 |  |
|        3 | 11642 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11643 | `	SyString *aEntry;` |
|        - | 11644 | `	int dir_sep;` |
|        - | 11645 | `	sxu32 n;` |
|        - | 11646 | `#ifdef __WINNT__` |
|        1 | 11647 | `	dir_sep = ';';` |
|        - | 11648 | `#else` |
|        - | 11649 | `	/* Assume UNIX path separator */` |
|        2 | 11650 | `	dir_sep = ':';` |
|        - | 11651 | `#endif` |
|        1 | 11652 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11653 | `	SXUNUSED(apArg);` |
|        - | 11654 | `	/* Point to the list of import paths */` |
|        3 | 11655 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11656 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11657 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11658 | `		if( n > 0 ){` |
|        - | 11659 | `			/* Append dir seprator */` |
|      ! 0 | 11660 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11661 | `		}` |
|        - | 11662 | `		/* Append path */` |
|        3 | 11663 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11664 | `	}` |
|        3 | 11665 | `	return PH7_OK;` |
|        1 | 11666 |  |
|        - | 11667 | `/*` |
|        - | 11668 | ` * string get_get_included_files(void)` |
|        - | 11669 | ` *  Gets the current include_path configuration option.` |
|        - | 11670 | ` * Parameter` |
|        - | 11671 | ` *  None` |
|        - | 11672 | ` * Return` |
|        - | 11673 | ` *  Included paths as a string` |
|        - | 11674 | ` */` |
|        2 | 11675 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11676 |  |
|        3 | 11677 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11678 | `	ph7_value *pArray,*pWorker;` |
|        - | 11679 | `	SyString *pEntry;` |
|        - | 11680 | `	int c,d;` |
|        - | 11681 | `	/* Create an array and a working value */` |
|        3 | 11682 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11683 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11684 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11685 | `		/* Out of memory,return null */` |
|      ! 0 | 11686 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11687 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11688 | `		SXUNUSED(apArg);` |
|      ! 0 | 11689 | `		return PH7_OK;` |
|        - | 11690 | `	}` |
|        3 | 11691 | `	c = d = '/';` |
|        - | 11692 | `#ifdef __WINNT__` |
|        1 | 11693 | `	d = '\\';` |
|        - | 11694 | `#endif` |
|        - | 11695 | `	/* Iterate throw entries */` |
|        3 | 11696 | `	SySetResetCursor(pFiles);` |
|     3811 | 11697 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11698 | `		const char *zBase,*zEnd;` |
|        - | 11699 | `		int iLen;` |
|        - | 11700 | `		/* reset the string cursor */` |
|     3809 | 11701 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11702 | `		/* Extract base name */` |
|     3809 | 11703 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11704 | `		/* Ignore trailing '/' */` |
|     5713 | 11705 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11706 | `			zEnd--;` |
|      ! 0 | 11707 | `		}` |
|     3809 | 11708 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   117501 | 11709 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   111789 | 11710 | `			zEnd--;` |
|        1 | 11711 | `		}` |
|     3809 | 11712 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3809 | 11713 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11714 | `		/* Copy entry name */` |
|     3809 | 11715 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11716 | `		/* Perform the insertion */` |
|     3809 | 11717 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11718 | `	}` |
|        - | 11719 | `	/* All done,return the created array */` |
|        3 | 11720 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11721 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11722 | `	 * by the engine as soon we return from this foreign` |
|        - | 11723 | `	 * function.` |
|        - | 11724 | `	 */` |
|        3 | 11725 | `	return PH7_OK;` |
|        2 | 11726 |  |
|        - | 11727 | `/*` |
|        - | 11728 | ` * include:` |
|        - | 11729 | ` * According to the PHP reference manual.` |
|        - | 11730 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11731 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11732 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11733 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11734 | ` *  and the current working directory before failing. The include()` |
|        - | 11735 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11736 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11737 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11738 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11739 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11740 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11741 | ` *  directory to find the requested file.` |
|        - | 11742 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11743 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11744 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11745 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11746 | ` */` |
|     8006 | 11747 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11748 |  |
|        - | 11749 | `	SyString sFile;` |
|        - | 11750 | `	sxi32 rc;` |
|     8008 | 11751 | `	if( nArg < 1 ){` |
|        - | 11752 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11753 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11754 | `		return SXRET_OK;` |
|        - | 11755 | `	}` |
|        - | 11756 | `	/* File to include */` |
|     8008 | 11757 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8008 | 11758 | `	if( sFile.nByte < 1 ){` |
|        - | 11759 | `		/* Empty string,return NULL */` |
|      ! 0 | 11760 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11761 | `		return SXRET_OK;` |
|        - | 11762 | `	}` |
|        - | 11763 | `	/* Open,compile and execute the desired script */` |
|     8008 | 11764 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8008 | 11765 | `	if( rc != SXRET_OK ){` |
|        - | 11766 | `		/* Emit a warning and return false */` |
|        3 | 11767 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11768 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11769 | `	}` |
|     8008 | 11770 | `	return SXRET_OK;` |
|     4005 | 11771 |  |
|        - | 11772 | `/*` |
|        - | 11773 | ` * include_once:` |
|        - | 11774 | ` *  According to the PHP reference manual.` |
|        - | 11775 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11776 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11777 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11778 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11779 | ` *   just once.` |
|        - | 11780 | ` */` |
|        4 | 11781 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11782 |  |
|        - | 11783 | `	SyString sFile;` |
|        - | 11784 | `	sxi32 rc;` |
|        5 | 11785 | `	if( nArg < 1 ){` |
|        - | 11786 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11787 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11788 | `		return SXRET_OK;` |
|        - | 11789 | `	}` |
|        - | 11790 | `	/* File to include */` |
|        5 | 11791 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11792 | `	if( sFile.nByte < 1 ){` |
|        - | 11793 | `		/* Empty string,return NULL */` |
|      ! 0 | 11794 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11795 | `		return SXRET_OK;` |
|        - | 11796 | `	}` |
|        - | 11797 | `	/* Open,compile and execute the desired script */` |
|        5 | 11798 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11799 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11800 | `		/* File already included,return TRUE */` |
|        3 | 11801 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11802 | `		return SXRET_OK;` |
|        - | 11803 | `	}` |
|        3 | 11804 | `	if( rc != SXRET_OK ){` |
|        - | 11805 | `		/* Emit a warning and return false */` |
|      ! 0 | 11806 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11807 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11808 | ` 	}` |
|        3 | 11809 | `	return SXRET_OK;` |
|        3 | 11810 |  |
|        - | 11811 | `/*` |
|        - | 11812 | ` * require.` |
|        - | 11813 | ` *  According to the PHP reference manual.` |
|        - | 11814 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11815 | ` *   also produce a fatal level error.` |
|        - | 11816 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11817 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11818 | ` */` |
|        6 | 11819 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11820 |  |
|        - | 11821 | `	SyString sFile;` |
|        - | 11822 | `	sxi32 rc;` |
|        8 | 11823 | `	if( nArg < 1 ){` |
|        - | 11824 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11825 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11826 | `		return SXRET_OK;` |
|        - | 11827 | `	}` |
|        - | 11828 | `	/* File to include */` |
|        8 | 11829 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11830 | `	if( sFile.nByte < 1 ){` |
|        - | 11831 | `		/* Empty string,return NULL */` |
|      ! 0 | 11832 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11833 | `		return SXRET_OK;` |
|        - | 11834 | `	}` |
|        - | 11835 | `	/* Open,compile and execute the desired script */` |
|        8 | 11836 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 11837 | `	if( rc != SXRET_OK ){` |
|        - | 11838 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11839 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11840 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11841 | `		return PH7_ABORT;` |
|        - | 11842 | `	}` |
|        8 | 11843 | `	return SXRET_OK;` |
|        5 | 11844 |  |
|        - | 11845 | `/*` |
|        - | 11846 | ` * require_once:` |
|        - | 11847 | ` *  According to the PHP reference manual.` |
|        - | 11848 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11849 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11850 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11851 | ` *   and how it differs from its non _once siblings.` |
|        - | 11852 | ` */` |
|        4 | 11853 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11854 |  |
|        - | 11855 | `	SyString sFile;` |
|        - | 11856 | `	sxi32 rc;` |
|        5 | 11857 | `	if( nArg < 1 ){` |
|        - | 11858 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11859 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11860 | `		return SXRET_OK;` |
|        - | 11861 | `	}` |
|        - | 11862 | `	/* File to include */` |
|        5 | 11863 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11864 | `	if( sFile.nByte < 1 ){` |
|        - | 11865 | `		/* Empty string,return NULL */` |
|      ! 0 | 11866 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11867 | `		return SXRET_OK;` |
|        - | 11868 | `	}` |
|        - | 11869 | `	/* Open,compile and execute the desired script */` |
|        5 | 11870 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11871 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11872 | `		/* File already included,return TRUE */` |
|        3 | 11873 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11874 | `		return SXRET_OK;` |
|        - | 11875 | `	}` |
|        3 | 11876 | `	if( rc != SXRET_OK ){` |
|        - | 11877 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11878 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11879 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11880 | `		return PH7_ABORT;` |
|        - | 11881 | `	}` |
|        3 | 11882 | `	return SXRET_OK;` |
|        3 | 11883 |  |
|        - | 11884 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11885 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11886 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11887 | `/*` |
|        - | 11888 | ` * Section:` |
|        - | 11889 | ` *  SPL Autoloading functions.` |
|        - | 11890 | ` * Status:` |
|        - | 11891 | ` *  Stable.` |
|        - | 11892 | ` */` |
|        - | 11893 | `/*` |
|        - | 11894 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 11895 | ` *  Register given function as __autoload() implementation.` |
|        - | 11896 | ` * Parameters` |
|        - | 11897 | ` *  callback` |
|        - | 11898 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 11899 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 11900 | ` *  throw` |
|        - | 11901 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 11902 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 11903 | ` *  prepend` |
|        - | 11904 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 11905 | ` *   autoload stack instead of appending it.` |
|        - | 11906 | ` * Return` |
|        - | 11907 | ` *  TRUE on success, FALSE on failure.` |
|        - | 11908 | ` */` |
|       34 | 11909 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11910 |  |
|        - | 11911 | `	VmAutoloadCB sEntry;` |
|       36 | 11912 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 11913 | `	int iPrepend = 0;` |
|        - | 11914 | `	sxu32 n;` |
|       36 | 11915 | `	if( nArg < 1 ){` |
|        - | 11916 | `		/* No callback provided — register default spl_autoload.` |
|        - | 11917 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 11918 | `		/* Check for duplicates first */` |
|        9 | 11919 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 11920 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 11921 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 11922 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 11923 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 11924 | `				ph7_result_bool(pCtx,1);` |
|        5 | 11925 | `				return SXRET_OK;` |
|        - | 11926 | `			}` |
|      ! 0 | 11927 | `		}` |
|        5 | 11928 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 11929 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 11930 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 11931 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 11932 | `		ph7_result_bool(pCtx,1);` |
|        5 | 11933 | `		return SXRET_OK;` |
|        - | 11934 | `	}` |
|        - | 11935 | `	/* Validate that the callback is callable */` |
|       28 | 11936 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 11937 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 11938 | `		if( nArg >= 2 ){` |
|      ! 0 | 11939 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11940 | `		}` |
|      ! 0 | 11941 | `		if( iThrow ){` |
|      ! 0 | 11942 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 11943 | `				"Argument is not callable");` |
|      ! 0 | 11944 | `		}` |
|      ! 0 | 11945 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11946 | `		return SXRET_OK;` |
|        - | 11947 | `	}` |
|        - | 11948 | `	/* Check for duplicates */` |
|       46 | 11949 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 11950 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 11951 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 11952 | `			/* Already registered */` |
|      ! 0 | 11953 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11954 | `			return SXRET_OK;` |
|        - | 11955 | `		}` |
|       11 | 11956 | `	}` |
|        - | 11957 | `	/* Check prepend flag */` |
|       28 | 11958 | `	if( nArg >= 3 ){` |
|        3 | 11959 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 11960 | `	}` |
|        - | 11961 | `	/* Store the callback */` |
|       28 | 11962 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 11963 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 11964 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 11965 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 11966 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 11967 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 11968 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 11969 | `		VmAutoloadCB *aBase;` |
|        3 | 11970 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 11971 | `		/* Rotate: move last entry to front */` |
|        3 | 11972 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 11973 | `		if( aBase ){` |
|        - | 11974 | `			VmAutoloadCB sTemp;` |
|        - | 11975 | `			sxu32 i;` |
|        3 | 11976 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 11977 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 11978 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 11979 | `			}` |
|        3 | 11980 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 11981 | `		}` |
|        2 | 11982 | `	}else{` |
|       26 | 11983 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 11984 | `	}` |
|       28 | 11985 | `	ph7_result_bool(pCtx,1);` |
|       28 | 11986 | `	return SXRET_OK;` |
|       19 | 11987 |  |
|        - | 11988 | `/*` |
|        - | 11989 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 11990 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 11991 | ` * Parameters` |
|        - | 11992 | ` *  callback` |
|        - | 11993 | ` *   The autoload function being unregistered.` |
|        - | 11994 | ` * Return` |
|        - | 11995 | ` *  TRUE on success, FALSE on failure.` |
|        - | 11996 | ` */` |
|       32 | 11997 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11998 |  |
|       34 | 11999 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12000 | `	sxu32 n,nEntry;` |
|       34 | 12001 | `	if( nArg < 1 ){` |
|      ! 0 | 12002 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12003 | `		return SXRET_OK;` |
|        - | 12004 | `	}` |
|       34 | 12005 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12006 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12007 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12008 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12009 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12010 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12011 | `			sxu32 i;` |
|       32 | 12012 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12013 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12014 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12015 | `			}` |
|        - | 12016 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12017 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12018 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12019 | `			return SXRET_OK;` |
|        - | 12020 | `		}` |
|        3 | 12021 | `	}` |
|        3 | 12022 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12023 | `	return SXRET_OK;` |
|       18 | 12024 |  |
|        - | 12025 | `/*` |
|        - | 12026 | ` * array spl_autoload_functions(void)` |
|        - | 12027 | ` *  Return all registered __autoload() functions.` |
|        - | 12028 | ` * Return` |
|        - | 12029 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12030 | ` *  an empty array is returned.` |
|        - | 12031 | ` */` |
|       20 | 12032 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12033 |  |
|       21 | 12034 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12035 | `	ph7_value *pArray;` |
|        - | 12036 | `	sxu32 n,nEntry;` |
|       10 | 12037 | `	SXUNUSED(nArg);` |
|       10 | 12038 | `	SXUNUSED(apArg);` |
|       21 | 12039 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12040 | `	if( pArray == 0 ){` |
|      ! 0 | 12041 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12042 | `		return SXRET_OK;` |
|        - | 12043 | `	}` |
|       21 | 12044 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12045 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12046 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12047 | `		if( pEntry ){` |
|       15 | 12048 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12049 | `		}` |
|        8 | 12050 | `	}` |
|       21 | 12051 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12052 | `	return SXRET_OK;` |
|       11 | 12053 |  |
|        - | 12054 | `/*` |
|        - | 12055 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12056 | ` *  Default implementation of __autoload().` |
|        - | 12057 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12058 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12059 | ` * Parameters` |
|        - | 12060 | ` *  class` |
|        - | 12061 | ` *   The class name being searched.` |
|        - | 12062 | ` *  file_extensions` |
|        - | 12063 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12064 | ` */` |
|        2 | 12065 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12066 |  |
|        - | 12067 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12068 | `	SyBlob sPath;` |
|        - | 12069 | `	int nClass;` |
|        - | 12070 | `	sxi32 rc;` |
|        3 | 12071 | `	if( nArg < 1 ){` |
|      ! 0 | 12072 | `		return SXRET_OK;` |
|        - | 12073 | `	}` |
|        3 | 12074 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12075 | `	if( nClass < 1 ){` |
|      ! 0 | 12076 | `		return SXRET_OK;` |
|        - | 12077 | `	}` |
|        - | 12078 | `	/* Default extensions */` |
|        3 | 12079 | `	zExt = ".php,.inc";` |
|        3 | 12080 | `	if( nArg >= 2 ){` |
|        - | 12081 | `		int nExt;` |
|      ! 0 | 12082 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12083 | `		if( nExt < 1 ){` |
|      ! 0 | 12084 | `			zExt = ".php,.inc";` |
|      ! 0 | 12085 | `		}` |
|      ! 0 | 12086 | `	}` |
|        3 | 12087 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12088 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12089 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12090 | `	zCur = zExt;` |
|        7 | 12091 | `	while( zCur < zEnd ){` |
|        - | 12092 | `		const char *zComma;` |
|        - | 12093 | `		SyString sFile;` |
|        - | 12094 | `		int i;` |
|        - | 12095 | `		/* Find next comma or end */` |
|        5 | 12096 | `		zComma = zCur;` |
|       21 | 12097 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12098 | `			zComma++;` |
|        1 | 12099 | `		}` |
|        - | 12100 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12101 | `		SyBlobReset(&sPath);` |
|       69 | 12102 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12103 | `			char c = zClass[i];` |
|       65 | 12104 | `			if( c == '\\' ){` |
|      ! 0 | 12105 | `				c = '/';` |
|       65 | 12106 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12107 | `				c = c + ('a' - 'A');` |
|        6 | 12108 | `			}` |
|       65 | 12109 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12110 | `		}` |
|        - | 12111 | `		/* Append extension */` |
|        5 | 12112 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12113 | `		/* Try to include the file */` |
|        5 | 12114 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12115 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12116 | `		if( rc == SXRET_OK ){` |
|        - | 12117 | `			/* File included successfully */` |
|      ! 0 | 12118 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12119 | `			return SXRET_OK;` |
|        - | 12120 | `		}` |
|        - | 12121 | `		/* Move past the comma */` |
|        5 | 12122 | `		zCur = zComma;` |
|        5 | 12123 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12124 | `			zCur++;` |
|        1 | 12125 | `		}` |
|        1 | 12126 | `	}` |
|        3 | 12127 | `	SyBlobRelease(&sPath);` |
|        3 | 12128 | `	return SXRET_OK;` |
|        2 | 12129 |  |
|        - | 12130 | `/* Table of built-in VM functions. */` |
|        - | 12131 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12132 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12133 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12134 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12135 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12136 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12137 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12138 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12139 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12140 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12141 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12142 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12143 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12144 | `	    /* Constants management */` |
|        - | 12145 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12146 | `	{ "define",   vm_builtin_define               },` |
|        - | 12147 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12148 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12149 | `	   /* Class/Object functions */` |
|        - | 12150 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12151 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12152 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12153 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12154 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12155 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12156 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12157 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12158 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12159 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12160 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12161 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12162 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12163 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12164 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12165 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12166 | `	   /* SPL Autoloading */` |
|        - | 12167 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12168 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12169 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12170 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12171 | `	   /* Random numbers/strings generators */` |
|        - | 12172 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12173 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12174 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12175 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12176 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12177 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12178 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12179 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12180 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12181 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12182 | `	   /* Language constructs functions */` |
|        - | 12183 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12184 | `	{ "print", vm_builtin_print                   },` |
|        - | 12185 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12186 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12187 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12188 | `	  /* Variable handling functions */` |
|        - | 12189 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12190 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12191 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12192 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12193 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12194 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12195 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12196 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12197 | `	  /* Ouput control functions */` |
|        - | 12198 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12199 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12200 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12201 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12202 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12203 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12204 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12205 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12206 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12207 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12208 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12209 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12210 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12211 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12212 | `	  /* Assertion functions */` |
|        - | 12213 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12214 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12215 | `	  /* Error reporting functions */` |
|        - | 12216 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12217 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12218 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12219 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12220 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12221 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12222 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12223 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12224 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12225 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12226 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12227 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12228 | `	  /* Release info */` |
|        - | 12229 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12230 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12231 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12232 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12233 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12234 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12235 | `	  /* hashmap */` |
|        - | 12236 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12237 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12238 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12239 | `	  /* URL related function */` |
|        - | 12240 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12241 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12242 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12243 | `	   /* XML processing functions */` |
|        - | 12244 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12245 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12246 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12247 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12248 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12249 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12250 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12251 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12252 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12253 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12254 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12255 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12256 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12257 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12258 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12259 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12260 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12261 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12262 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12263 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12264 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12265 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12266 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12267 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12268 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12269 | `	   /* Command line processing */` |
|        - | 12270 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12271 | `	   /* JSON encoding/decoding */` |
|        - | 12272 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12273 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12274 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12275 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12276 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12277 | `	   /* Files/URI inclusion facility */` |
|        - | 12278 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12279 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12280 | `	{ "include",      vm_builtin_include          },` |
|        - | 12281 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12282 | `	{ "require",      vm_builtin_require          },` |
|        - | 12283 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12284 | `};` |
|        - | 12285 | `/*` |
|        - | 12286 | ` * Register the built-in VM functions defined above.` |
|        - | 12287 | ` */` |
|     2316 | 12288 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12289 |  |
|        - | 12290 | `	sxi32 rc;` |
|        - | 12291 | `	sxu32 n;` |
|   298766 | 12292 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12293 | `		/* Note that these special functions have access` |
|        - | 12294 | `		 * to the underlying virtual machine as their` |
|        - | 12295 | `		 * private data.` |
|        - | 12296 | `		 */` |
|   296450 | 12297 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   296450 | 12298 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12299 | `			return rc;` |
|        - | 12300 | `		}` |
|   148226 | 12301 | `	}` |
|     2318 | 12302 | `	return SXRET_OK;` |
|     1160 | 12303 |  |
|        - | 12304 | `/*` |
|        - | 12305 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12306 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12307 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12308 | ` */` |
|    27342 | 12309 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12310 |  |
|    27344 | 12311 | `	if( !iLoadable ){` |
|    26140 | 12312 | `		return pClass;` |
|        - | 12313 | `	}` |
|     1206 | 12314 | `	while(pClass){` |
|     1206 | 12315 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1206 | 12316 | `			return pClass;` |
|        - | 12317 | `		}` |
|      ! 0 | 12318 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12319 | `	}` |
|      ! 0 | 12320 | `	return 0;` |
|    13673 | 12321 |  |
|        - | 12322 | `/*` |
|        - | 12323 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12324 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12325 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12326 | ` * registered in the VM's class table.` |
|        - | 12327 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12328 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12329 | ` */` |
|       30 | 12330 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12331 |  |
|        - | 12332 | `	VmAutoloadCB *pEntry;` |
|        - | 12333 | `	ph7_value sArg,sResult;` |
|        - | 12334 | `	SyHashEntry *pHashEntry;` |
|        - | 12335 | `	ph7_class *pClass;` |
|        - | 12336 | `	sxu32 n,nEntry;` |
|       32 | 12337 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       32 | 12338 | `	if( nEntry < 1 ){` |
|       18 | 12339 | `		return 0;` |
|        - | 12340 | `	}` |
|        - | 12341 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12342 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12343 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12344 | `	}` |
|        - | 12345 | `	/* Mark this class as being autoloaded */` |
|       14 | 12346 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12347 | `	/* Prepare the class name argument */` |
|       14 | 12348 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12349 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12350 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12351 | `	pClass = 0;` |
|       28 | 12352 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12353 | `		ph7_value *apArg[1];` |
|       24 | 12354 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12355 | `		if( pEntry == 0 ){` |
|      ! 0 | 12356 | `			continue;` |
|        - | 12357 | `		}` |
|       24 | 12358 | `		apArg[0] = &sArg;` |
|       24 | 12359 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12360 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12361 | `			continue;` |
|        - | 12362 | `		}` |
|        - | 12363 | `		/* Check if the class is now available */` |
|       24 | 12364 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12365 | `		if( pHashEntry ){` |
|       10 | 12366 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12367 | `			if( pClass ){` |
|       10 | 12368 | `				break;` |
|        - | 12369 | `			}` |
|      ! 0 | 12370 | `		}` |
|        9 | 12371 | `	}` |
|       14 | 12372 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12373 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12374 | `	/* Remove reentrancy guard */` |
|       14 | 12375 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12376 | `	return pClass;` |
|       17 | 12377 |  |
|        - | 12378 | `/*` |
|        - | 12379 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12380 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12381 | ` */` |
|       18 | 12382 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12383 |  |
|       20 | 12384 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12385 |  |
|        - | 12386 | `/*` |
|        - | 12387 | ` * Check if the given name refer to an installed class.` |
|        - | 12388 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12389 | ` */` |
|    27346 | 12390 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12391 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12392 | `	const char *zName,  /* Name of the target class */` |
|        - | 12393 | `	sxu32 nByte,        /* zName length */` |
|        - | 12394 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12395 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12396 | `						 */` |
|        - | 12397 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12398 | `	)` |
|        2 | 12399 |  |
|        - | 12400 | `	SyHashEntry *pEntry;` |
|        - | 12401 | `	ph7_class *pClass;` |
|    13673 | 12402 | `	SXUNUSED(iNest);` |
|        - | 12403 | `	/* Exact class lookup.` |
|        - | 12404 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12405 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    27348 | 12406 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    27348 | 12407 | `	if( pEntry == 0 ){` |
|        - | 12408 | `		/* Class not found in hash table — try autoload before giving up */` |
|       14 | 12409 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12410 | `	}` |
|    27336 | 12411 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    27336 | 12412 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    13675 | 12413 |  |
|        - | 12414 | `/*` |
|        - | 12415 | ` * Reference Table Implementation` |
|        - | 12416 | ` * Status: stable <chm@symisc.net>` |
|        - | 12417 | ` * Intro` |
|        - | 12418 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12419 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12420 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12421 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12422 | ` *  Refer to the official for more information on this powerful` |
|        - | 12423 | ` *  extension.` |
|        - | 12424 | ` */` |
|        - | 12425 | `/*` |
|        - | 12426 | ` * Allocate a new reference entry.` |
|        - | 12427 | ` */` |
|  3054294 | 12428 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12429 |  |
|        - | 12430 | `	VmRefObj *pRef;` |
|        - | 12431 | `	/* Allocate a new instance */` |
|  3054296 | 12432 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3054296 | 12433 | `	if( pRef == 0 ){` |
|      ! 0 | 12434 | `		return 0;` |
|        - | 12435 | `	}` |
|        - | 12436 | `	/* Zero the structure */` |
|  3054296 | 12437 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12438 | `	/* Initialize fields */` |
|  3054296 | 12439 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3054296 | 12440 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3054296 | 12441 | `	pRef->nIdx = nIdx;` |
|  3054296 | 12442 | `	return pRef;` |
|  1527149 | 12443 |  |
|        - | 12444 | `/*` |
|        - | 12445 | ` * Default hash function used by the reference table` |
|        - | 12446 | ` * for lookup/insertion operations.` |
|        - | 12447 | ` */` |
| 16871425 | 12448 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12449 |  |
|        - | 12450 | `	/* Calculate the hash based on the memory object index */` |
| 16871427 | 12451 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12452 |  |
|        - | 12453 | `/*` |
|        - | 12454 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12455 | ` * in the reference table.` |
|        - | 12456 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12457 | ` * otherwise.` |
|        - | 12458 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12459 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12460 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12461 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12462 | ` * Refer to the official for more information on this powerful` |
|        - | 12463 | ` * extension.` |
|        - | 12464 | ` */` |
|  9116368 | 12465 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12466 |  |
|        - | 12467 | `	VmRefObj *pRef;` |
|        - | 12468 | `	sxu32 nBucket;` |
|        - | 12469 | `	/* Point to the appropriate bucket */` |
|  9116370 | 12470 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12471 | `	/* Perform the lookup */` |
|  9116370 | 12472 | `	pRef = pVm->apRefObj[nBucket];` |
| 19840777 | 12473 | `	for(;;){` |
| 39671276 | 12474 | `		if( pRef == 0 ){` |
|  3133512 | 12475 | `			break;` |
|        - | 12476 | `		}` |
| 36537766 | 12477 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12478 | `			/* Entry found */` |
|  5982860 | 12479 | `			return pRef;` |
|        - | 12480 | `		}` |
|        - | 12481 | `		/* Point to the next entry */` |
| 30554908 | 12482 | `		pRef = pRef->pNextCollide;` |
|        2 | 12483 | `	}` |
|        - | 12484 | `	/* No such entry,return NULL */` |
|  3133512 | 12485 | `	return 0;` |
|  4558186 | 12486 |  |
|        - | 12487 | `/*` |
|        - | 12488 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12489 | ` *` |
|        - | 12490 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12491 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12492 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12493 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12494 | ` * Refer to the official for more information on this powerful` |
|        - | 12495 | ` * extension.` |
|        - | 12496 | ` */` |
|  3054294 | 12497 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12498 |  |
|        - | 12499 | `	sxu32 nBucket;` |
|  3054296 | 12500 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12501 | `		VmRefObj **apNew;` |
|        - | 12502 | `		sxu32 nNew;` |
|        - | 12503 | `		/* Allocate a larger table */` |
|     3958 | 12504 | `		nNew = pVm->nRefSize << 1;` |
|     3958 | 12505 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3958 | 12506 | `		if( apNew ){` |
|     3958 | 12507 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12508 | `			sxu32 n;` |
|        - | 12509 | `			/* Zero the structure */` |
|     3958 | 12510 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12511 | `			/* Rehash all referenced entries */` |
|  2840384 | 12512 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12513 | `				/* Remove old collision links */` |
|  2836428 | 12514 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12515 | `				/* Point to the appropriate bucket */` |
|  2836428 | 12516 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12517 | `				/* Insert the entry  */` |
|  2836428 | 12518 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2836428 | 12519 | `				if( apNew[nBucket] ){` |
|  2298896 | 12520 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12521 | `				}` |
|  2836428 | 12522 | `				apNew[nBucket] = pEntry;` |
|        - | 12523 | `				/* Point to the next entry */` |
|  2836428 | 12524 | `				pEntry = pEntry->pNext;` |
|  1418215 | 12525 | `			}` |
|        - | 12526 | `			/* Release the old table */` |
|     3958 | 12527 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12528 | `			/* Install the new one */` |
|     3958 | 12529 | `			pVm->apRefObj = apNew;` |
|     3958 | 12530 | `			pVm->nRefSize = nNew;` |
|     1978 | 12531 | `		}` |
|     1978 | 12532 | `	}` |
|        - | 12533 | `	/* Point to the appropriate bucket */` |
|  3054296 | 12534 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12535 | `	/* Insert the entry */` |
|  3054296 | 12536 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3054296 | 12537 | `	if( pVm->apRefObj[nBucket] ){` |
|  2525309 | 12538 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1262598 | 12539 | `	}` |
|  3054296 | 12540 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3054296 | 12541 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3054296 | 12542 | `	pVm->nRefUsed++;` |
|  3054296 | 12543 | `	return SXRET_OK;` |
|        2 | 12544 |  |
|        - | 12545 | `/*` |
|        - | 12546 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12547 | ` * the reference table.` |
|        - | 12548 | ` * This function is invoked when the user perform an unset` |
|        - | 12549 | ` * call [i.e: unset($var); ].` |
|        - | 12550 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12551 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12552 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12553 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12554 | ` * Refer to the official for more information on this powerful` |
|        - | 12555 | ` * extension.` |
|        - | 12556 | ` */` |
|  3020946 | 12557 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12558 |  |
|        - | 12559 | `	ph7_hashmap_node **apNode;` |
|        - | 12560 | `	SyHashEntry **apEntry;` |
|        - | 12561 | `	sxu32 n;` |
|        - | 12562 | `	/* Point to the reference table */` |
|  3020948 | 12563 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3020948 | 12564 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12565 | `	/* Unlink the entry from the reference table */` |
|  3106116 | 12566 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85170 | 12567 | `		if( apEntry[n] ){` |
|    85120 | 12568 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42559 | 12569 | `		}` |
|    42586 | 12570 | `	}` |
|  5959472 | 12571 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2938526 | 12572 | `		if( apNode[n] ){` |
|     6880 | 12573 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3439 | 12574 | `		}` |
|  1469264 | 12575 | `	}` |
|  3020948 | 12576 | `	if( pRef->pPrevCollide ){` |
|  1156611 | 12577 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   578273 | 12578 | `	}else{` |
|  1864339 | 12579 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12580 | `	}` |
|  3020948 | 12581 | `	if( pRef->pNextCollide ){` |
|  1714464 | 12582 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   857127 | 12583 | `	}` |
|  3020948 | 12584 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12585 | `	/* Release the node */` |
|  3020948 | 12586 | `	SySetRelease(&pRef->aReference);` |
|  3020948 | 12587 | `	SySetRelease(&pRef->aArrEntries);` |
|  3020948 | 12588 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3020948 | 12589 | `	pVm->nRefUsed--;` |
|  3020948 | 12590 | `	return SXRET_OK;` |
|        2 | 12591 |  |
|        - | 12592 | `/*` |
|        - | 12593 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12594 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12595 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12596 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12597 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12598 | ` * Refer to the official for more information on this powerful` |
|        - | 12599 | ` * extension.` |
|        - | 12600 | ` */` |
|  3084498 | 12601 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12602 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12603 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12604 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12605 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12606 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12607 | `	)` |
|        2 | 12608 |  |
|  3084500 | 12609 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12610 | `	VmRefObj *pRef;` |
|        - | 12611 | `	/* Check if the referenced object already exists */` |
|  3084500 | 12612 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3084500 | 12613 | `	if( pRef == 0 ){` |
|        - | 12614 | `		/* Create a new entry */` |
|  3054296 | 12615 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3054296 | 12616 | `		if( pRef == 0 ){` |
|      ! 0 | 12617 | `			return SXERR_MEM;` |
|        - | 12618 | `		}` |
|  3054296 | 12619 | `		pRef->iFlags = iFlags;` |
|        - | 12620 | `		/* Install the entry */` |
|  3054296 | 12621 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1527147 | 12622 | `	}` |
|  3084500 | 12623 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3084500 | 12624 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12625 | `		VmSlot sRef;` |
|        - | 12626 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12627 | `		 * be deleted when we leave this frame.` |
|        - | 12628 | `		 */` |
|    79296 | 12629 | `		sRef.nIdx = nIdx;` |
|    79296 | 12630 | `		sRef.pUserData = pEntry;` |
|    79296 | 12631 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12632 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12633 | `		}` |
|    39647 | 12634 | `	}` |
|  3084500 | 12635 | `	if( pEntry ){` |
|        - | 12636 | `		/* Address of the hash-entry */` |
|   109308 | 12637 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    54653 | 12638 | `	}` |
|  3084500 | 12639 | `	if( pMapEntry ){` |
|        - | 12640 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2970196 | 12641 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1485097 | 12642 | `	}` |
|  3084500 | 12643 | `	return SXRET_OK;` |
|  1542251 | 12644 |  |
|        - | 12645 | `/*` |
|        - | 12646 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12647 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12648 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12649 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12650 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12651 | ` * Refer to the official for more information on this powerful` |
|        - | 12652 | ` * extension.` |
|        - | 12653 | ` */` |
|  3010918 | 12654 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12655 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12656 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12657 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12658 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12659 | `	)` |
|        2 | 12660 |  |
|        - | 12661 | `	VmRefObj *pRef;` |
|        - | 12662 | `	sxu32 n;` |
|        - | 12663 | `	/* Check if the referenced object already exists */` |
|  3010920 | 12664 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3010920 | 12665 | `	if( pRef == 0 ){` |
|        - | 12666 | `		/* Not such entry */` |
|    79212 | 12667 | `		return SXERR_NOTFOUND;` |
|        - | 12668 | `	}` |
|        - | 12669 | `	/* Remove the desired entry */` |
|  2931710 | 12670 | `	if( pEntry ){` |
|        - | 12671 | `		SyHashEntry **apEntry;` |
|       56 | 12672 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12673 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12674 | `			if( apEntry[n] == pEntry ){` |
|        - | 12675 | `				/* Nullify the entry */` |
|       56 | 12676 | `				apEntry[n] = 0;` |
|        - | 12677 | `				/*` |
|        - | 12678 | `				 * NOTE:` |
|        - | 12679 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12680 | `				 * we avoid wasting spaces.` |
|        - | 12681 | `				 */` |
|       27 | 12682 | `			}` |
|       79 | 12683 | `		}` |
|       27 | 12684 | `	}` |
|  2931710 | 12685 | `	if( pMapEntry ){` |
|        - | 12686 | `		ph7_hashmap_node **apNode;` |
|  2931656 | 12687 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5863404 | 12688 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2931750 | 12689 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12690 | `				/* nullify the entry */` |
|  2931656 | 12691 | `				apNode[n] = 0;` |
|  1465827 | 12692 | `			}` |
|  1465876 | 12693 | `		}` |
|  1465827 | 12694 | `	}` |
|  2931710 | 12695 | `	return SXRET_OK;` |
|  1505461 | 12696 |  |
|        - | 12697 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12698 | `/*` |
|        - | 12699 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12700 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12701 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12702 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12703 | ` * For more information on how to register IO stream devices,please` |
|        - | 12704 | ` * refer to the official documentation.` |
|        - | 12705 | ` */` |
|    24330 | 12706 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12707 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12708 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12709 | `	int nByte              /* *pzDevice length*/` |
|        - | 12710 | `	)` |
|        2 | 12711 |  |
|        - | 12712 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12713 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12714 | `	SyString sDev,sCur;` |
|        - | 12715 | `	sxu32 n,nEntry;` |
|        - | 12716 | `	int rc;` |
|        - | 12717 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24332 | 12718 | `	zNext = zCur = zIn = *pzDevice;` |
|    24332 | 12719 | `	zEnd = &zIn[nByte];` |
|  1551150 | 12720 | `	while( zIn < zEnd ){` |
|  1526822 | 12721 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12722 | `			/* Got one */` |
|        3 | 12723 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12724 | `			break;` |
|        - | 12725 | `		}` |
|        - | 12726 | `		/* Advance the cursor */` |
|  1526820 | 12727 | `		zIn++;` |
|        2 | 12728 | `	}` |
|    24332 | 12729 | `	if( zIn >= zEnd ){` |
|        - | 12730 | `		/* No such scheme,return the default stream */` |
|    24330 | 12731 | `		return pVm->pDefStream;` |
|        - | 12732 | `	}` |
|        3 | 12733 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12734 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12735 | `	SyStringFullTrim(&sDev);` |
|        - | 12736 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12737 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12738 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12739 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12740 | `		pStream = apStream[n];` |
|        3 | 12741 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12742 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12743 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12744 | `		if( rc == 0 ){` |
|        - | 12745 | `			/* Stream device found */` |
|        3 | 12746 | `			*pzDevice = zNext;` |
|        3 | 12747 | `			return pStream;` |
|        - | 12748 | `		}` |
|      ! 0 | 12749 | `	}` |
|        - | 12750 | `	/* No such stream,return NULL */` |
|      ! 0 | 12751 | `	return 0;` |
|    12167 | 12752 |  |
|        - | 12753 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12754 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12755 |  |
