# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5733/7466 lines (76.79%)

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
|   843494 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   843496 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   843462 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   843452 |   104 | `	return FALSE;` |
|   421771 |   105 |  |
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
|   551206 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   551208 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   551208 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   551204 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   551204 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   551204 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   551204 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   551204 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   551204 |   152 | `	pCons->xExpand = xExpand;` |
|   551204 |   153 | `	pCons->pUserData = pUserData;` |
|   551204 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   551204 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   551204 |   161 | `	return SXRET_OK;` |
|   275605 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1211858 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1211860 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1211860 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1211860 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1211860 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1211860 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1211860 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1211860 |   195 | `	pFunc->pVm   = pVm;` |
|  1211860 |   196 | `	pFunc->xFunc = xFunc;` |
|  1211860 |   197 | `	pFunc->pUserData = pUserData;` |
|  1211860 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1211860 |   200 | `	*ppOut = pFunc;` |
|  1211860 |   201 | `	return SXRET_OK;` |
|   605931 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1214398 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1214400 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1214400 |   223 | `	if( pEntry ){` |
|     2542 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2542 |   225 | `		pFunc->pUserData = pUserData;` |
|     2542 |   226 | `		pFunc->xFunc = xFunc;` |
|     2542 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2542 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1211860 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1211860 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1211860 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1211860 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1211860 |   243 | `	return SXRET_OK;` |
|   607201 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   173536 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   173538 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   173538 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   173538 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   173538 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   173538 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   173538 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   173538 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   173538 |   272 | `	pFunc->iFlags = iFlags;` |
|   173538 |   273 | `	pFunc->pUserData = pUserData;` |
|   173538 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   173538 |   275 | `	return SXRET_OK;` |
|        2 |   276 |  |
|        - |   277 | `/*` |
|        - |   278 | ` * Namespace-aware function lookup.` |
|        - |   279 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   280 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   281 | ` */` |
|        - |   282 | `/*` |
|        - |   283 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   284 | ` */` |
|   681836 |   285 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   286 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   287 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   288 | `	SyString *pName     /* Function name */` |
|        - |   289 | `	)` |
|        2 |   290 |  |
|        - |   291 | `	SyHashEntry *pEntry;` |
|        - |   292 | `	sxi32 rc;` |
|   681838 |   293 | `	if( pName == 0 ){` |
|        - |   294 | `		/* Use the built-in name */` |
|    37566 |   295 | `		pName = &pFunc->sName;` |
|    18782 |   296 | `	}` |
|        - |   297 | `	/* Check for duplicates (functions with the same name) first */` |
|   681838 |   298 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   681838 |   299 | `	if( pEntry ){` |
|   531152 |   300 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   531152 |   301 | `		if( pLink != pFunc ){` |
|        - |   302 | `			/* Link */` |
|      188 |   303 | `			pFunc->pNextName = pLink;` |
|      188 |   304 | `			pEntry->pUserData = pFunc;` |
|       93 |   305 | `		}` |
|   531152 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* First time seen */` |
|   150688 |   309 | `	pFunc->pNextName = 0;` |
|   150688 |   310 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   150688 |   311 | `	return rc;` |
|   340920 |   312 |  |
|        - |   313 | `/*` |
|        - |   314 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   315 | ` */` |
|    48680 |   316 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   317 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   318 | `	ph7_class *pClass /* Target Class */` |
|        - |   319 | `	)` |
|        2 |   320 |  |
|    48682 |   321 | `	SyString *pName = &pClass->sName;` |
|        - |   322 | `	SyHashEntry *pEntry;` |
|        - |   323 | `	sxi32 rc;` |
|        - |   324 | `	/* Check for duplicates */` |
|    48682 |   325 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    48682 |   326 | `	if( pEntry ){` |
|       31 |   327 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   328 | `		/* Link entry with the same name */` |
|       31 |   329 | `		pClass->pNextName = pLink;` |
|       31 |   330 | `		pEntry->pUserData = pClass;` |
|       31 |   331 | `		return SXRET_OK;` |
|        - |   332 | `	}` |
|    48652 |   333 | `	pClass->pNextName = 0;` |
|        - |   334 | `	/* Perform a simple hashtable insertion */` |
|    48652 |   335 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    48652 |   336 | `	return rc;` |
|    24342 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Instruction builder interface.` |
|        - |   340 | ` */` |
|  3498536 |   341 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   342 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   343 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   344 | `	sxi32 iP1,    /* First operand */` |
|        - |   345 | `	sxu32 iP2,    /* Second operand */` |
|        - |   346 | `	void *p3,     /* Third operand */` |
|        - |   347 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   348 | `	)` |
|        2 |   349 |  |
|        - |   350 | `	VmInstr sInstr;` |
|        - |   351 | `	sxi32 rc;` |
|        - |   352 | `	/* Fill the VM instruction */` |
|  3498538 |   353 | `	sInstr.iOp = (sxu8)iOp;` |
|  3498538 |   354 | `	sInstr.iP1 = iP1;` |
|  3498538 |   355 | `	sInstr.iP2 = iP2;` |
|  3498538 |   356 | `	sInstr.p3  = p3;` |
|  3498538 |   357 | `	if( pIndex ){` |
|        - |   358 | `		/* Instruction index in the bytecode array */` |
|   201532 |   359 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   100765 |   360 | `	}` |
|        - |   361 | `	/* Finally,record the instruction */` |
|  3498538 |   362 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3498538 |   363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   364 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   365 | `		/* Fall throw */` |
|      ! 0 |   366 | `	}` |
|  3498538 |   367 | `	return rc;` |
|        2 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Swap the current bytecode container with the given one.` |
|        - |   371 | ` */` |
|   415912 |   372 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   373 |  |
|   415914 |   374 | `	if( pContainer == 0 ){` |
|        - |   375 | `		/* Point to the default container */` |
|      ! 0 |   376 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   377 | `	}else{` |
|        - |   378 | `		/* Change container */` |
|   415914 |   379 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   380 | `	}` |
|   415914 |   381 | `	return SXRET_OK;` |
|        2 |   382 |  |
|        - |   383 | `/*` |
|        - |   384 | ` * Return the current bytecode container.` |
|        - |   385 | ` */` |
|   207956 |   386 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   387 |  |
|   207958 |   388 | `	return pVm->pByteContainer;` |
|        2 |   389 |  |
|        - |   390 | `/*` |
|        - |   391 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   392 | ` */` |
|   198632 |   393 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   394 |  |
|        - |   395 | `	VmInstr *pInstr;` |
|   198634 |   396 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   198634 |   397 | `	return pInstr;` |
|        2 |   398 |  |
|        - |   399 | `/*` |
|        - |   400 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   401 | ` */` |
|  1048438 |   402 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   403 |  |
|  1048440 |   404 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Pop the last VM instruction.` |
|        - |   408 | ` */` |
|   189088 |   409 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   410 |  |
|   189090 |   411 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   412 |  |
|        - |   413 | `/*` |
|        - |   414 | ` * Peek the last VM instruction.` |
|        - |   415 | ` */` |
|   678368 |   416 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   417 |  |
|   678370 |   418 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   419 |  |
|    29344 |   420 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   421 |  |
|        - |   422 | `	VmInstr *aInstr;` |
|        - |   423 | `	sxu32 n;` |
|    29346 |   424 | `	n = SySetUsed(pVm->pByteContainer);` |
|    29346 |   425 | `	if( n < 2 ){` |
|      ! 0 |   426 | `		return 0;` |
|        - |   427 | `	}` |
|    29346 |   428 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    29346 |   429 | `	return &aInstr[n - 2];` |
|    14674 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Allocate a new virtual machine frame.` |
|        - |   433 | ` */` |
|    18076 |   434 | `static VmFrame * VmNewFrame(` |
|        - |   435 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   436 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   437 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   438 | `	)` |
|        2 |   439 |  |
|        - |   440 | `	VmFrame *pFrame;` |
|        - |   441 | `	/* Allocate a new vm frame */` |
|    18078 |   442 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    18078 |   443 | `	if( pFrame == 0 ){` |
|      ! 0 |   444 | `		return 0;` |
|        - |   445 | `	}` |
|        - |   446 | `	/* Zero the structure */` |
|    18078 |   447 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   448 | `	/* Initialize frame fields */` |
|    18078 |   449 | `	pFrame->pUserData = pUserData;` |
|    18078 |   450 | `	pFrame->pThis = pThis;` |
|    18078 |   451 | `	pFrame->pVm = pVm;` |
|    18078 |   452 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    18078 |   453 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    18078 |   454 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    18078 |   455 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    18078 |   456 | `	return pFrame;` |
|     9040 |   457 |  |
|        - |   458 | `/* Forward declaration */` |
|        - |   459 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   460 | `/*` |
|        - |   461 | ` * Enter a VM frame.` |
|        - |   462 | ` */` |
|    18030 |   463 | `static sxi32 VmEnterFrame(` |
|        - |   464 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   465 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   468 | `	)` |
|        2 |   469 |  |
|        - |   470 | `	VmFrame *pFrame;` |
|        - |   471 | `	/* Allocate a new frame */` |
|    18032 |   472 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    18032 |   473 | `	if( pFrame == 0 ){` |
|      ! 0 |   474 | `		return SXERR_MEM;` |
|        - |   475 | `	}` |
|        - |   476 | `	/* Link to the list of active VM frame */` |
|    18032 |   477 | `	pFrame->pParent = pVm->pFrame;` |
|    18032 |   478 | `	pVm->pFrame = pFrame;` |
|    18032 |   479 | `	if( ppFrame ){` |
|        - |   480 | `		/* Write a pointer to the new VM frame */` |
|    15210 |   481 | `		*ppFrame = pFrame;` |
|     7604 |   482 | `	}` |
|    18032 |   483 | `	return SXRET_OK;` |
|     9017 |   484 |  |
|        - |   485 | `/*` |
|        - |   486 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   487 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   488 | ` * information.` |
|        - |   489 | ` */` |
|       52 |   490 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   491 |  |
|        - |   492 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   493 | `	SyHashEntry *pEntry = 0;` |
|        - |   494 | `	sxi32 rc;` |
|        - |   495 | `	/* Point to the upper frame */` |
|       54 |   496 | `	pFrame = pVm->pFrame;` |
|       54 |   497 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       54 |   498 | `	pTarget = pFrame;` |
|       54 |   499 | `	pFrame = pTarget->pParent;` |
|       54 |   500 | `	while( pFrame ){` |
|       54 |   501 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   502 | `			/* Query the current frame */` |
|       54 |   503 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   504 | `			if( pEntry ){` |
|        - |   505 | `				/* Variable found */` |
|       54 |   506 | `				break;` |
|        - |   507 | `			}` |
|      ! 0 |   508 | `		}` |
|        - |   509 | `		/* Point to the upper frame */` |
|      ! 0 |   510 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   511 | `	}` |
|       54 |   512 | `	if( pEntry == 0 ){` |
|        - |   513 | `		/* Inexistant variable */` |
|      ! 0 |   514 | `		return SXERR_NOTFOUND;` |
|        - |   515 | `	}` |
|        - |   516 | `	/* Link to the current frame */` |
|       54 |   517 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   518 | `	if( rc == SXRET_OK ){` |
|        - |   519 | `		sxu32 nIdx;` |
|       54 |   520 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   521 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   522 | `	}` |
|       54 |   523 | `	return rc;` |
|       28 |   524 |  |
|        - |   525 | `/*` |
|        - |   526 | ` * Leave the top-most active frame.` |
|        - |   527 | ` */` |
|    15202 |   528 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   529 |  |
|    15204 |   530 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    15204 |   531 | `	if( pCurFrame ){` |
|        - |   532 | `		/* Unlink from the list of active VM frame */` |
|    15204 |   533 | `		pVm->pFrame = pCurFrame->pParent;` |
|    15204 |   534 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   535 | `			VmSlot  *aSlot;` |
|        - |   536 | `			sxu32 n;` |
|        - |   537 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    15046 |   538 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   102542 |   539 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   540 | `				/* Unset the local variable */` |
|    87498 |   541 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    43750 |   542 | `			}` |
|        - |   543 | `			/* Remove local reference */` |
|    15046 |   544 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   102598 |   545 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    87554 |   546 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    43778 |   547 | `			}` |
|     7522 |   548 | `		}` |
|        - |   549 | `		/* Release internal containers */` |
|    15204 |   550 | `		SyHashRelease(&pCurFrame->hVar);` |
|    15204 |   551 | `		SySetRelease(&pCurFrame->sArg);` |
|    15204 |   552 | `		SySetRelease(&pCurFrame->sLocal);` |
|    15204 |   553 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   554 | `		/* Release the whole structure */` |
|    15204 |   555 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     7601 |   556 | `	}` |
|    15204 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   560 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   561 | ` * should be skipped when looking for the real execution context.` |
|        - |   562 | ` */` |
|  6659740 |   563 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   564 |  |
|  6660604 |   565 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      864 |   566 | `		pFrame = pFrame->pParent;` |
|        2 |   567 | `	}` |
|  6659742 |   568 | `	return pFrame;` |
|        2 |   569 |  |
|        - |   570 | `/*` |
|        - |   571 | ` * Compare two functions signature and return the comparison result.` |
|        - |   572 | ` */` |
|      836 |   573 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   574 |  |
|      837 |   575 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   576 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   577 | `	const char *zSin = pSecond->zString;` |
|      837 |   578 | `	const char *zFin = pFirst->zString;` |
|      837 |   579 | `	const char *zPtr = zFin;` |
|      421 |   580 | `	for(;;){` |
|      843 |   581 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   582 | `			break;` |
|        - |   583 | `		}` |
|       19 |   584 | `		if( zFin[0] != zSin[0] ){` |
|        - |   585 | `			/* mismatch */` |
|       13 |   586 | `			break;` |
|        - |   587 | `		}` |
|        7 |   588 | `		zFin++;` |
|        7 |   589 | `		zSin++;` |
|        1 |   590 | `	}` |
|      837 |   591 | `	return (int)(zFin-zPtr);` |
|        1 |   592 |  |
|        - |   593 | `/*` |
|        - |   594 | ` * Select the appropriate VM function for the current call context.` |
|        - |   595 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   596 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   597 | ` * Refer to the official documentation for more information.` |
|        - |   598 | ` */` |
|      138 |   599 | `static ph7_vm_func * VmOverload(` |
|        - |   600 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   601 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   602 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   603 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   604 | `	)` |
|        2 |   605 |  |
|        - |   606 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   607 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   608 | `	ph7_vm_func *pLink;` |
|        - |   609 | `	SyString sArgSig;` |
|        - |   610 | `	SyBlob sSig;` |
|        - |   611 |  |
|      140 |   612 | `	pLink = pList;` |
|      140 |   613 | `	i = 0;` |
|        - |   614 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   615 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   616 | `		if( pLink == 0 ){` |
|       78 |   617 | `			break;` |
|        - |   618 | `		}` |
|      948 |   619 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   620 | `			/* Candidate for overloading */` |
|      902 |   621 | `			apSet[i++] = pLink;` |
|      450 |   622 | `		}` |
|        - |   623 | `		/* Point to the next entry */` |
|      948 |   624 | `		pLink = pLink->pNextName;` |
|        2 |   625 | `	}` |
|      140 |   626 | `	if( i < 1 ){` |
|        - |   627 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   628 | `		return pList;` |
|        - |   629 | `	}` |
|      140 |   630 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   631 | `		/* Return the only candidate */` |
|       32 |   632 | `		return apSet[0];` |
|        - |   633 | `	}` |
|        - |   634 | `	/* Calculate function signature */` |
|      109 |   635 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   636 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   637 | `		int c = 'n'; /* null */` |
|      259 |   638 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   639 | `			/* Hashmap */` |
|       45 |   640 | `			c = 'h';` |
|      237 |   641 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   642 | `			/* bool */` |
|      ! 0 |   643 | `			c = 'b';` |
|      215 |   644 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   645 | `			/* int */` |
|        7 |   646 | `			c = 'i';` |
|      212 |   647 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   648 | `			/* String */` |
|      107 |   649 | `			c = 's';` |
|      156 |   650 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   651 | `			/* Float */` |
|      ! 0 |   652 | `			c = 'f';` |
|      103 |   653 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   654 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   655 | `			int marker = 'o';` |
|        3 |   656 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   657 | `			SyString *pName = &pClass->sName;` |
|        3 |   658 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   659 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   660 | `			c = -1;` |
|        1 |   661 | `		}` |
|      259 |   662 | `		if( c > 0 ){` |
|      257 |   663 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   664 | `		}` |
|      130 |   665 | `	}` |
|      109 |   666 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   667 | `	iTarget = 0;` |
|      109 |   668 | `	iMax = -1;` |
|        - |   669 | `	/* Select the appropriate function */` |
|      945 |   670 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   671 | `		/* Compare the two signatures */` |
|      837 |   672 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   673 | `		if( iCur > iMax ){` |
|      113 |   674 | `			iMax = iCur;` |
|      113 |   675 | `			iTarget = j;` |
|       56 |   676 | `		}` |
|      419 |   677 | `	}` |
|      109 |   678 | `	SyBlobRelease(&sSig);` |
|        - |   679 | `	/* Appropriate function for the current call context */` |
|      109 |   680 | `	return apSet[iTarget];` |
|       71 |   681 |  |
|        - |   682 | `/* Forward declaration */` |
|        - |   683 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   684 | `/*` |
|        - |   685 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   686 | ` * it can be instanciated from the executed PHP script.` |
|        - |   687 | ` */` |
|   133826 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|   133828 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   563059 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   362320 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   362320 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   704 | `			ph7_value *pMemObj;` |
|        - |   705 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1494 |   706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1494 |   707 | `			if( pMemObj == 0 ){` |
|      ! 0 |   708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   709 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   710 | `					&pClass->sName,&pAttr->sName` |
|        - |   711 | `					);` |
|      ! 0 |   712 | `				return SXERR_MEM;` |
|        - |   713 | `			}` |
|     1494 |   714 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   715 | `				/* Initialize attribute default value (any complex expression) */` |
|     1492 |   716 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      745 |   717 | `			}` |
|        - |   718 | `			/* Record attribute index */` |
|     1494 |   719 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   720 | `			/* Install static attribute in the reference table */` |
|     1494 |   721 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   722 | `			/* If this is a typed static property, register the slot so the` |
|        - |   723 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   724 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   725 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1494 |   726 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        8 |   727 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        8 |   728 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   729 | `					return SXERR_MEM;` |
|        - |   730 | `				}` |
|        8 |   731 | `				pVmAttrS->pAttr = pAttr;` |
|        8 |   732 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        8 |   733 | `				pVmAttrS->iState = 0;` |
|        8 |   734 | `				pVmAttrS->pOwner = pClass;` |
|        - |   735 | `				/* Static typed property with no default starts uninitialized */` |
|        6 |   736 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        6 |   737 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |   738 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        1 |   739 | `				}` |
|        8 |   740 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   741 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   742 | `					return SXERR_MEM;` |
|        - |   743 | `				}` |
|        3 |   744 | `			}` |
|      746 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Install class methods */` |
|   133828 |   748 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   749 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   750 | `		 */` |
|    58196 |   751 | `		return SXRET_OK;` |
|        - |   752 | `	}` |
|        - |   753 | `	/* Create constructor alias if not yet done */` |
|    75634 |   754 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   755 | `		/* User constructor with the same base class name */` |
|     5840 |   756 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5840 |   757 | `		if( pEntry ){` |
|      ! 0 |   758 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   759 | `			/* Create the alias */` |
|      ! 0 |   760 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   761 | `		}` |
|     2919 |   762 | `	}` |
|        - |   763 | `	/* Install the methods now */` |
|    75634 |   764 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   757730 |   765 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   644282 |   766 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   644282 |   767 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   644274 |   768 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   644274 |   769 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   770 | `				return rc;` |
|        - |   771 | `			}` |
|   322136 |   772 | `		}` |
|        2 |   773 | `	}` |
|        - |   774 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    75634 |   775 | `	pClass->bMounted = TRUE;` |
|    75634 |   776 | `	return SXRET_OK;` |
|    66915 |   777 |  |
|        - |   778 | `/*` |
|        - |   779 | ` * Allocate a private frame for attributes of the given` |
|        - |   780 | ` * class instance (Object in the PHP jargon).` |
|        - |   781 | ` */` |
|     1478 |   782 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   784 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   785 | `	)` |
|        2 |   786 |  |
|     1480 |   787 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   788 | `	ph7_class_attr *pAttr;` |
|        - |   789 | `	SyHashEntry *pEntry;` |
|        - |   790 | `	sxi32 rc;` |
|        - |   791 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1480 |   792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5994 |   793 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   794 | `		VmClassAttr *pVmAttr;` |
|        - |   795 | `		/* Extract the current attribute */` |
|     4516 |   796 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4516 |   797 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     4516 |   798 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   799 | `			return SXERR_MEM;` |
|        - |   800 | `		}` |
|     4516 |   801 | `		pVmAttr->pAttr = pAttr;` |
|     4516 |   802 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   803 | `			ph7_value *pMemObj;` |
|        - |   804 | `			/* Reserve a memory object for this attribute */` |
|     4492 |   805 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4492 |   806 | `			if( pMemObj == 0 ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|     4492 |   810 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     4492 |   811 | `			pVmAttr->iState = 0;` |
|     4492 |   812 | `			pVmAttr->pOwner = pClass;` |
|     4492 |   813 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   814 | `				/* Initialize attribute default value (any complex expression) */` |
|     1524 |   815 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     3731 |   816 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   817 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   818 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       28 |   819 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       13 |   820 | `			}` |
|     4492 |   821 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     4492 |   822 | `			if( rc != SXRET_OK ){` |
|        - |   823 | `				VmSlot sSlot;` |
|        - |   824 | `				/* Restore memory object */` |
|      ! 0 |   825 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   826 | `				sSlot.pUserData = 0;` |
|      ! 0 |   827 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|        - |   831 | `			/* Install attribute in the reference table */` |
|     4492 |   832 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   833 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   834 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   835 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     4492 |   836 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      116 |   837 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      116 |   838 | `				if( rc != SXRET_OK ){` |
|        - |   839 | `					VmSlot sSlot;` |
|      ! 0 |   840 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   841 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   842 | `					sSlot.pUserData = 0;` |
|      ! 0 |   843 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   844 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   845 | `					return SXERR_MEM;` |
|        - |   846 | `				}` |
|       57 |   847 | `			}` |
|     2247 |   848 | `		}else{` |
|        - |   849 | `			/* Install static/constant attribute */` |
|       26 |   850 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   851 | `			pVmAttr->iState = 0;` |
|       26 |   852 | `			pVmAttr->pOwner = pClass;` |
|       26 |   853 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   854 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   855 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   856 | `				return SXERR_MEM;` |
|        - |   857 | `			}` |
|        - |   858 | `		}` |
|        2 |   859 | `	}` |
|     1480 |   860 | `	return SXRET_OK;` |
|      741 |   861 |  |
|        - |   862 | `/* Forward declaration */` |
|        - |   863 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   864 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   865 | `/*` |
|        - |   866 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   867 | ` */` |
|        - |   868 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   869 | `/*` |
|        - |   870 | ` * Reserve a constant memory object.` |
|        - |   871 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   872 | ` */` |
|   401660 |   873 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   874 |  |
|        - |   875 | `	ph7_value *pObj;` |
|        - |   876 | `	sxi32 rc;` |
|   401662 |   877 | `	if( pIndex ){` |
|        - |   878 | `		/* Object index in the object table */` |
|   393196 |   879 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   196597 |   880 | `	}` |
|        - |   881 | `	/* Reserve a slot for the new object */` |
|   401662 |   882 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   401662 |   883 | `	if( rc != SXRET_OK ){` |
|        - |   884 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   885 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   886 | `		 */` |
|      ! 0 |   887 | `		return 0;` |
|        - |   888 | `	}` |
|   401662 |   889 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   401662 |   890 | `	return pObj;` |
|   200832 |   891 |  |
|        - |   892 | `/*` |
|        - |   893 | ` * Reserve a memory object.` |
|        - |   894 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   895 | ` */` |
|  2146398 |   896 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   897 |  |
|        - |   898 | `	ph7_value *pObj;` |
|        - |   899 | `	sxi32 rc;` |
|  2146400 |   900 | `	if( pIndex ){` |
|        - |   901 | `		/* Object index in the object table */` |
|  2146400 |   902 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073199 |   903 | `	}` |
|        - |   904 | `	/* Reserve a slot for the new object */` |
|  2146400 |   905 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2146400 |   906 | `	if( rc != SXRET_OK ){` |
|        - |   907 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   908 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   909 | `		 */` |
|      ! 0 |   910 | `		return 0;` |
|        - |   911 | `	}` |
|  2146400 |   912 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2146400 |   913 | `	return pObj;` |
|  1073201 |   914 |  |
|        - |   915 | `/* Forward declaration */` |
|        - |   916 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   917 | `/* Forward declarations for Fiber C functions */` |
|        - |   918 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   919 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   920 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   921 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   922 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   923 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   924 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   925 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   926 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   927 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   928 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   929 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   930 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   931 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   932 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   933 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |   934 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |   935 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |   936 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   937 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   938 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   939 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   940 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   941 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   942 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   943 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   944 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   945 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   946 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   947 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   948 | `/*` |
|        - |   949 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   950 | ` * directly as foreign functions.` |
|        - |   951 | ` */` |
|        - |   952 | `#define PH7_BUILTIN_LIB \` |
|        - |   953 | `	"class Exception { "\` |
|        - |   954 | `    "protected $message = 'Unknown exception';"\` |
|        - |   955 | `    "protected $code = 0;"\` |
|        - |   956 | `    "protected $file;"\` |
|        - |   957 | `    "protected $line;"\` |
|        - |   958 | `    "protected $trace;"\` |
|        - |   959 | `    "protected $previous;"\` |
|        - |   960 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   961 | `	"   if( isset($message) ){"\` |
|        - |   962 | `	"	  $this->message = $message;"\` |
|        - |   963 | `	"   }"\` |
|        - |   964 | `	"   $this->code = $code;"\` |
|        - |   965 | `	"   $this->file = __FILE__;"\` |
|        - |   966 | `	"   $this->line = __LINE__;"\` |
|        - |   967 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   968 | `	"   if( isset($previous) ){"\` |
|        - |   969 | `	"     $this->previous = $previous;"\` |
|        - |   970 | `	"   }"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	"public function getMessage(){"\` |
|        - |   973 | `	"   return $this->message;"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	" public function getCode(){"\` |
|        - |   976 | `	"  return $this->code;"\` |
|        - |   977 | `	"}"\` |
|        - |   978 | `	"public function getFile(){"\` |
|        - |   979 | `	"  return $this->file;"\` |
|        - |   980 | `	"}"\` |
|        - |   981 | `	"public function getLine(){"\` |
|        - |   982 | `	"  return $this->line;"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"public function getTrace(){"\` |
|        - |   985 | `	"   return $this->trace;"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"public function getTraceAsString(){"\` |
|        - |   988 | `	"  return debug_string_backtrace();"\` |
|        - |   989 | `	"}"\` |
|        - |   990 | `	"public function getPrevious(){"\` |
|        - |   991 | `	"    return $this->previous;"\` |
|        - |   992 | `	"}"\` |
|        - |   993 | `	"public function __toString(){"\` |
|        - |   994 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   995 | `    "}"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"class Error extends Exception { }"\` |
|        - |   998 | `	"class TypeError extends Error { }"\` |
|        - |   999 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1000 | `	"class ValueError extends Error { }"\` |
|        - |  1001 | `	"class FiberError extends Error { }"\` |
|        - |  1002 | `	"class AssertionError extends Error { }"\` |
|        - |  1003 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1004 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1005 | `	"class ErrorException extends Exception { "\` |
|        - |  1006 | `	"protected $severity;"\` |
|        - |  1007 | `	"public function __construct(string $message = null,"\` |
|        - |  1008 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |  1009 | `	"   if( isset($message) ){"\` |
|        - |  1010 | `	"	  $this->message = $message;"\` |
|        - |  1011 | `	"   }"\` |
|        - |  1012 | `	"   $this->severity = $severity;"\` |
|        - |  1013 | `	"   $this->code = $code;"\` |
|        - |  1014 | `	"   $this->file = $filename;"\` |
|        - |  1015 | `	"   $this->line = $lineno;"\` |
|        - |  1016 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1017 | `	"   if( isset($previous) ){"\` |
|        - |  1018 | `	"     $this->previous = $previous;"\` |
|        - |  1019 | `	"   }"\` |
|        - |  1020 | `	"}"\` |
|        - |  1021 | `	"public function getSeverity(){"\` |
|        - |  1022 | `	"   return $this->severity;"\` |
|        - |  1023 | `    "}"\` |
|        - |  1024 | `	"}"\` |
|        - |  1025 | `	"interface Iterator {"\` |
|        - |  1026 | `	"public function current();"\` |
|        - |  1027 | `	"public function key();"\` |
|        - |  1028 | `	"public function next();"\` |
|        - |  1029 | `	"public function rewind();"\` |
|        - |  1030 | `	"public function valid();"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"interface IteratorAggregate {"\` |
|        - |  1033 | `	"public function getIterator();"\` |
|        - |  1034 | `	"}"\` |
|        - |  1035 | `	"interface Serializable {"\` |
|        - |  1036 | `	"public function serialize();"\` |
|        - |  1037 | `	"public function unserialize(string $serialized);"\` |
|        - |  1038 | `	"}"\` |
|        - |  1039 | `	"/* Directory releated IO */"\` |
|        - |  1040 | `	"class Directory {"\` |
|        - |  1041 | `	"public $handle = null;"\` |
|        - |  1042 | `	"public $path  = null;"\` |
|        - |  1043 | `	"public function __construct(string $path)"\` |
|        - |  1044 | `	"{"\` |
|        - |  1045 | `	"   $this->handle = opendir($path);"\` |
|        - |  1046 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1047 | `	"      $this->path = $path;"\` |
|        - |  1048 | `	"   }"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"public function __destruct()"\` |
|        - |  1051 | `	"{"\` |
|        - |  1052 | `	"  if( $this->handle != null ){"\` |
|        - |  1053 | `	"       closedir($this->handle);"\` |
|        - |  1054 | `	"  }"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"public function read()"\` |
|        - |  1057 | `	"{"\` |
|        - |  1058 | `	"    return readdir($this->handle);"\` |
|        - |  1059 | `	"}"\` |
|        - |  1060 | `	"public function rewind()"\` |
|        - |  1061 | `	"{"\` |
|        - |  1062 | `	"    rewinddir($this->handle);"\` |
|        - |  1063 | `	"}"\` |
|        - |  1064 | `	"public function close()"\` |
|        - |  1065 | `	"{"\` |
|        - |  1066 | `	"    closedir($this->handle);"\` |
|        - |  1067 | `	"    $this->handle = null;"\` |
|        - |  1068 | `	"}"\` |
|        - |  1069 | `	"}"\` |
|        - |  1070 | `	"class Fiber {"\` |
|        - |  1071 | `	"  private $__ctx;"\` |
|        - |  1072 | `	"  private $__callable;"\` |
|        - |  1073 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1074 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1075 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1076 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1077 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1078 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1079 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1080 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1081 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1082 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"class Generator implements Iterator {"\` |
|        - |  1085 | `	"  private $__ctx;"\` |
|        - |  1086 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1087 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1088 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1089 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1090 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1091 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1092 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1093 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1094 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1095 | `	"}"\` |
|        - |  1096 | `	"class stdClass{"\` |
|        - |  1097 | `	"  public $value;"\` |
|        - |  1098 | `	" /* Magic methods */"\` |
|        - |  1099 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1100 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1101 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1102 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1103 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1104 | `	"}"\` |
|        - |  1105 | `	"function dir(string $path){"\` |
|        - |  1106 | `	"   return new Directory($path);"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"function Dir(string $path){"\` |
|        - |  1109 | `	"   return new Directory($path);"\` |
|        - |  1110 | `	"}"\` |
|        - |  1111 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1112 | `    "{"\` |
|        - |  1113 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1114 | `	"  $aDir = array();"\` |
|        - |  1115 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1116 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1117 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1118 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1119 | `	"   }"\` |
|        - |  1120 | `	"  closedir($pHandle);"\` |
|        - |  1121 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1122 | `	"      rsort($aDir);"\` |
|        - |  1123 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1124 | `	"      sort($aDir);"\` |
|        - |  1125 | `	"  }"\` |
|        - |  1126 | `	"  return $aDir;"\` |
|        - |  1127 | `	"}"\` |
|        - |  1128 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1129 | `	"/* Open the target directory */"\` |
|        - |  1130 | `	"$zDir = dirname($pattern);"\` |
|        - |  1131 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1132 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1133 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1134 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1135 | `	"	return FALSE;"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"$pattern = basename($pattern);"\` |
|        - |  1138 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1139 | `	"/* Loop throw available entries */"\` |
|        - |  1140 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1141 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1142 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1143 | `	"	if( $rc ){"\` |
|        - |  1144 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1145 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1146 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1147 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1148 | `	"		  }"\` |
|        - |  1149 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1150 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1151 | `	"		 continue;"\` |
|        - |  1152 | `	"	   }"\` |
|        - |  1153 | `	"	   /* Add the entry */"\` |
|        - |  1154 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1155 | `	"	}"\` |
|        - |  1156 | `	" }"\` |
|        - |  1157 | `	"/* Close the handle */"\` |
|        - |  1158 | `	"closedir($pHandle);"\` |
|        - |  1159 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1160 | `	"  /* Sort the array */"\` |
|        - |  1161 | `	"  sort($pArray);"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1164 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1165 | `	"  $pArray[] = $pattern;"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"/* Return the created array */"\` |
|        - |  1168 | `	"return $pArray;"\` |
|        - |  1169 | `   "}"\` |
|        - |  1170 | `   "/* Creates a temporary file */"\` |
|        - |  1171 | `   "function tmpfile(){"\` |
|        - |  1172 | `   "  /* Extract the temp directory */"\` |
|        - |  1173 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1174 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1175 | `   "    /* Use the current dir */"\` |
|        - |  1176 | `   "    $zTempDir = '.';"\` |
|        - |  1177 | `   "  }"\` |
|        - |  1178 | `   "  /* Create the file */"\` |
|        - |  1179 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1180 | `   "  return $pHandle;"\` |
|        - |  1181 | `   "}"\` |
|        - |  1182 | `   "/* Creates a temporary filename */"\` |
|        - |  1183 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1184 | `   "{"\` |
|        - |  1185 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1186 | `   "}"\` |
|        - |  1187 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1188 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1189 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1190 | `   "/* Copy arguments */"\` |
|        - |  1191 | `   "$nArgs = func_num_args();"\` |
|        - |  1192 | `   "$pNew = array();"\` |
|        - |  1193 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1194 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1195 | `    "}"\` |
|        - |  1196 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1197 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1198 | `	"/* Erase */"\` |
|        - |  1199 | `	"array_erase($pArray);"\` |
|        - |  1200 | `	"/* Unshift */"\` |
|        - |  1201 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1202 | `	"return sizeof($pArray);"\` |
|        - |  1203 | `    "}"\` |
|        - |  1204 | `	"function array_merge_recursive(){"\` |
|        - |  1205 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1206 | `    "$arrays = func_get_args();"\` |
|        - |  1207 | `    "$narrays = count($arrays);"\` |
|        - |  1208 | `    "$ret = array();"\` |
|        - |  1209 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1210 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1211 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1212 | `	 " }"\` |
|        - |  1213 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1214 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1215 | `     "  if( $keyIsInt ) {"\` |
|        - |  1216 | `     "   $ret[] = $value;"\` |
|        - |  1217 | `     "  } else {"\` |
|        - |  1218 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1219 | `     "    $cur = $ret[$key];"\` |
|        - |  1220 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1221 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1222 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1223 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1224 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1225 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1226 | `     "    } else {"\` |
|        - |  1227 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1228 | `     "    }"\` |
|        - |  1229 | `     "   } else {"\` |
|        - |  1230 | `     "    $ret[$key] = $value;"\` |
|        - |  1231 | `     "   }"\` |
|        - |  1232 | `     "  }"\` |
|        - |  1233 | `     " }"\` |
|        - |  1234 | `	 " }"\` |
|        - |  1235 | `	 " return $ret;"\` |
|        - |  1236 | `    "}"\` |
|        - |  1237 | `	"function max(){"\` |
|        - |  1238 | `    "  $pArgs = func_get_args();"\` |
|        - |  1239 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1240 | `	"  return null;"\` |
|        - |  1241 | `    " }"\` |
|        - |  1242 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1243 | `    " $pArg = $pArgs[0];"\` |
|        - |  1244 | `	" if( !is_array($pArg) ){"\` |
|        - |  1245 | `	"   return $pArg; "\` |
|        - |  1246 | `	" }"\` |
|        - |  1247 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1248 | `	"   return null;"\` |
|        - |  1249 | `	" }"\` |
|        - |  1250 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1251 | `	" reset($pArg);"\` |
|        - |  1252 | `	" $max = current($pArg);"\` |
|        - |  1253 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1254 | `	"   if( $val > $max ){"\` |
|        - |  1255 | `	"     $max = $val;"\` |
|        - |  1256 | `    " }"\` |
|        - |  1257 | `	" }"\` |
|        - |  1258 | `	" return $max;"\` |
|        - |  1259 | `    " }"\` |
|        - |  1260 | `    " $max = $pArgs[0];"\` |
|        - |  1261 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1262 | `    " $val = $pArgs[$i];"\` |
|        - |  1263 | `	"if( $val > $max ){"\` |
|        - |  1264 | `	" $max = $val;"\` |
|        - |  1265 | `	"}"\` |
|        - |  1266 | `    " }"\` |
|        - |  1267 | `	" return $max;"\` |
|        - |  1268 | `    "}"\` |
|        - |  1269 | `	"function min(){"\` |
|        - |  1270 | `    "  $pArgs = func_get_args();"\` |
|        - |  1271 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1272 | `	"  return null;"\` |
|        - |  1273 | `    " }"\` |
|        - |  1274 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1275 | `    " $pArg = $pArgs[0];"\` |
|        - |  1276 | `	" if( !is_array($pArg) ){"\` |
|        - |  1277 | `	"   return $pArg; "\` |
|        - |  1278 | `	" }"\` |
|        - |  1279 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1280 | `	"   return null;"\` |
|        - |  1281 | `	" }"\` |
|        - |  1282 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1283 | `	" reset($pArg);"\` |
|        - |  1284 | `	" $min = current($pArg);"\` |
|        - |  1285 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1286 | `	"   if( $val < $min ){"\` |
|        - |  1287 | `	"     $min = $val;"\` |
|        - |  1288 | `    " }"\` |
|        - |  1289 | `	" }"\` |
|        - |  1290 | `	" return $min;"\` |
|        - |  1291 | `    " }"\` |
|        - |  1292 | `    " $min = $pArgs[0];"\` |
|        - |  1293 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1294 | `    " $val = $pArgs[$i];"\` |
|        - |  1295 | `	"if( $val < $min ){"\` |
|        - |  1296 | `	" $min = $val;"\` |
|        - |  1297 | `	" }"\` |
|        - |  1298 | `    " }"\` |
|        - |  1299 | `	" return $min;"\` |
|        - |  1300 | `	"}"\` |
|        - |  1301 | `	"function fileowner(string $file){"\` |
|        - |  1302 | `    " $a = stat($file);"\` |
|        - |  1303 | `	" if( !is_array($a) ){"\` |
|        - |  1304 | `	"	return false;"\` |
|        - |  1305 | `	" }"\` |
|        - |  1306 | `	" return $a['uid'];"\` |
|        - |  1307 | `    "}"\` |
|        - |  1308 | `    "function filegroup(string $file){"\` |
|        - |  1309 | `	" $a = stat($file);"\` |
|        - |  1310 | `	" if( !is_array($a) ){"\` |
|        - |  1311 | `	"	return false;"\` |
|        - |  1312 | `	" }"\` |
|        - |  1313 | `	" return $a['gid'];"\` |
|        - |  1314 | `    "}"\` |
|        - |  1315 | `	 "function fileinode(string $file){"\` |
|        - |  1316 | `	" $a = stat($file);"\` |
|        - |  1317 | `	" if( !is_array($a) ){"\` |
|        - |  1318 | `	"	return false;"\` |
|        - |  1319 | `	" }"\` |
|        - |  1320 | `	" return $a['ino'];"\` |
|        - |  1321 | `    "}"` |
|        - |  1322 |  |
|        - |  1323 | `/*` |
|        - |  1324 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1325 | ` * start compiling the target PHP program.` |
|        - |  1326 | ` */` |
|     2822 |  1327 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1328 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1329 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1330 | `	 )` |
|        2 |  1331 |  |
|        - |  1332 | `	SyString sBuiltin;` |
|        - |  1333 | `	ph7_value *pObj;` |
|        - |  1334 | `	sxi32 rc;` |
|        - |  1335 | `	/* Zero the structure */` |
|     2824 |  1336 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1337 | `	/* Initialize VM fields */` |
|     2824 |  1338 | `	pVm->pEngine = &(*pEngine);` |
|     2824 |  1339 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1340 | `	/* Instructions containers */` |
|     2824 |  1341 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2824 |  1342 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2824 |  1343 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1344 | `	/* Object containers */` |
|     2824 |  1345 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2824 |  1346 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1347 | `	/* Virtual machine internal containers */` |
|     2824 |  1348 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2824 |  1349 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2824 |  1350 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2824 |  1351 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2824 |  1352 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2824 |  1353 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2824 |  1354 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2824 |  1355 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2824 |  1356 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2824 |  1357 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2824 |  1358 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2824 |  1359 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2824 |  1360 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2824 |  1361 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2824 |  1362 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2824 |  1363 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2824 |  1364 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2824 |  1365 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2824 |  1366 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2824 |  1367 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2824 |  1368 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2824 |  1369 | `	pVm->pPendingException = 0;` |
|        - |  1370 | `	/* Configuration containers */` |
|     2824 |  1371 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2824 |  1372 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2824 |  1373 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2824 |  1374 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2824 |  1375 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2824 |  1376 | `	pVm->iResponseStatus = 200;` |
|     2824 |  1377 | `	pVm->bHeadersSent = 0;` |
|     2824 |  1378 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1379 | `	/* Error callbacks containers */` |
|     2824 |  1380 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2824 |  1381 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2824 |  1382 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2824 |  1383 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2824 |  1384 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1385 | `	/* Set a default recursion limit */` |
|        - |  1386 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2824 |  1387 | `	pVm->nMaxDepth = 32;` |
|        - |  1388 | `#else` |
|        - |  1389 | `	pVm->nMaxDepth = 16;` |
|        - |  1390 | `#endif` |
|        - |  1391 | `	/* Default assertion flags */` |
|     2824 |  1392 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1393 | `	/* JSON return status */` |
|     2824 |  1394 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1395 | `	/* PRNG context */` |
|     2824 |  1396 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1397 | `	/* Install the null constant */` |
|     2824 |  1398 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2824 |  1399 | `	if( pObj == 0 ){` |
|      ! 0 |  1400 | `		rc = SXERR_MEM;` |
|      ! 0 |  1401 | `		goto Err;` |
|        - |  1402 | `	}` |
|     2824 |  1403 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1404 | `	/* Install the boolean TRUE constant */` |
|     2824 |  1405 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2824 |  1406 | `	if( pObj == 0 ){` |
|      ! 0 |  1407 | `		rc = SXERR_MEM;` |
|      ! 0 |  1408 | `		goto Err;` |
|        - |  1409 | `	}` |
|     2824 |  1410 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1411 | `	/* Install the boolean FALSE constant */` |
|     2824 |  1412 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2824 |  1413 | `	if( pObj == 0 ){` |
|      ! 0 |  1414 | `		rc = SXERR_MEM;` |
|      ! 0 |  1415 | `		goto Err;` |
|        - |  1416 | `	}` |
|     2824 |  1417 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1418 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1419 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1420 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2824 |  1421 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2824 |  1422 | `	if( pObj == 0 ){` |
|      ! 0 |  1423 | `		rc = SXERR_MEM;` |
|      ! 0 |  1424 | `		goto Err;` |
|        - |  1425 | `	}` |
|     2824 |  1426 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1427 | `	/* Create the global frame */` |
|     2824 |  1428 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2824 |  1429 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1430 | `		goto Err;` |
|        - |  1431 | `	}` |
|        - |  1432 | `	/* Initialize the code generator */` |
|     2824 |  1433 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2824 |  1434 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1435 | `		goto Err;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* VM correctly initialized,set the magic number */` |
|     2824 |  1438 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2824 |  1439 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1440 | `	/* Compile the built-in library */` |
|     2824 |  1441 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1442 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2824 |  1443 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1444 | `	/* Register Fiber internal C functions */` |
|     2824 |  1445 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2824 |  1446 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2824 |  1447 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2824 |  1448 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2824 |  1449 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2824 |  1450 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2824 |  1451 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2824 |  1452 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2824 |  1453 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2824 |  1454 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1455 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2824 |  1456 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2824 |  1457 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2824 |  1458 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2824 |  1459 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2824 |  1460 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2824 |  1461 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2824 |  1462 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2824 |  1463 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2824 |  1464 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2824 |  1465 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1466 | `	/* Reset the code generator */` |
|     2824 |  1467 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2824 |  1468 | `	return SXRET_OK;` |
|      ! 0 |  1469 | `Err:` |
|      ! 0 |  1470 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1471 | `	return rc;` |
|     1413 |  1472 |  |
|        - |  1473 | `/*` |
|        - |  1474 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1475 | ` * routine which store the output in an internal blob.` |
|        - |  1476 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1477 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1478 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1479 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1480 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1481 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1482 | ` * to finish executing and extracting the output.` |
|        - |  1483 | ` */` |
|       38 |  1484 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1485 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1486 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1487 | `	void *pUserData     /* User private data */` |
|        - |  1488 | `	)` |
|      ! 0 |  1489 |  |
|        - |  1490 | `	 sxi32 rc;` |
|        - |  1491 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1492 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1493 | `	 return rc;` |
|      ! 0 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1497 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1498 | ` */` |
|    15940 |  1499 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1500 |  |
|    15942 |  1501 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    15942 |  1502 | `	if( xCons != VmObConsumer ){` |
|     6916 |  1503 | `		pVm->nOutputLen += nLen;` |
|     6916 |  1504 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      888 |  1505 | `			pVm->bHeadersSent = 1;` |
|      443 |  1506 | `		}` |
|     3457 |  1507 | `	}` |
|    15942 |  1508 |  |
|        - |  1509 | `#define VM_STACK_GUARD 16` |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1512 | ` * our compiled PHP program.` |
|        - |  1513 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1514 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1515 | ` */` |
|    36574 |  1516 | `static ph7_value * VmNewOperandStack(` |
|        - |  1517 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1518 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1519 | `	)` |
|        2 |  1520 |  |
|        - |  1521 | `	ph7_value *pStack;` |
|        - |  1522 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1523 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1524 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1525 | `  ** on the maximum stack depth required.` |
|        - |  1526 | `  **` |
|        - |  1527 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1528 | `  */` |
|    36576 |  1529 | `	nInstr += VM_STACK_GUARD;` |
|    36576 |  1530 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    36576 |  1531 | `	if( pStack == 0 ){` |
|      ! 0 |  1532 | `		return 0;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Initialize the operand stack */` |
|  2353486 |  1535 | `	while( nInstr > 0 ){` |
|  2316912 |  1536 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2316912 |  1537 | `		--nInstr;` |
|        2 |  1538 | `	}` |
|        - |  1539 | `	/* Ready for bytecode execution */` |
|    36576 |  1540 | `	return pStack;` |
|    18289 |  1541 |  |
|        - |  1542 | `/* Forward declaration */` |
|        - |  1543 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1544 | `/*` |
|        - |  1545 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1546 | ` * This routine gets called by the PH7 engine after` |
|        - |  1547 | ` * successful compilation of the target PHP program.` |
|        - |  1548 | ` */` |
|     2540 |  1549 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1550 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1551 | `	)` |
|        2 |  1552 |  |
|        - |  1553 | `	SyHashEntry *pEntry;` |
|        - |  1554 | `	sxi32 rc;` |
|     2542 |  1555 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1556 | `		/* Initialize your VM first */` |
|      ! 0 |  1557 | `		return SXERR_CORRUPT;` |
|        - |  1558 | `	}` |
|        - |  1559 | `	/* Mark the VM ready for byte-code execution */` |
|     2542 |  1560 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1561 | `	/* Release the code generator now we have compiled our program */` |
|     2542 |  1562 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1563 | `	/* Emit the DONE instruction */` |
|     2542 |  1564 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2542 |  1565 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1566 | `		return SXERR_MEM;` |
|        - |  1567 | `	}` |
|        - |  1568 | `	/* Script return value */` |
|     2542 |  1569 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1570 | `	/* Allocate a new operand stack */` |
|     2542 |  1571 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2542 |  1572 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1573 | `		return SXERR_MEM;` |
|        - |  1574 | `	}` |
|        - |  1575 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1576 | `	 * private data. */` |
|     2542 |  1577 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2542 |  1578 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1579 | `	/* Allocate the reference table */` |
|     2542 |  1580 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2542 |  1581 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2542 |  1582 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1583 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1584 | `		return SXERR_MEM;` |
|        - |  1585 | `	}` |
|        - |  1586 | `	/* Zero the reference table */` |
|     2542 |  1587 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1588 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2542 |  1589 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2542 |  1590 | `	if( rc != SXRET_OK ){` |
|        - |  1591 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1592 | `		return rc;` |
|        - |  1593 | `	}` |
|        - |  1594 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2542 |  1595 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2542 |  1596 | `	if( rc != SXRET_OK ){` |
|        - |  1597 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1598 | `		return rc;` |
|        - |  1599 | `	}` |
|        - |  1600 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2542 |  1601 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1602 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2542 |  1603 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1604 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2542 |  1605 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1606 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1607 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2542 |  1608 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2542 |  1609 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1610 | `#endif` |
|        - |  1611 | `	/* Initialize and install static and constants class attributes */` |
|     2542 |  1612 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    45968 |  1613 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    43428 |  1614 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    43428 |  1615 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1616 | `			return rc;` |
|        - |  1617 | `		}` |
|        2 |  1618 | `	}` |
|        - |  1619 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2542 |  1620 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1621 | `	/* VM is ready for bytecode execution */` |
|     2542 |  1622 | `	return SXRET_OK;` |
|     1272 |  1623 |  |
|        - |  1624 | `/*` |
|        - |  1625 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1626 | ` */` |
|      ! 0 |  1627 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1628 |  |
|      ! 0 |  1629 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1630 | `		return SXERR_CORRUPT;` |
|        - |  1631 | `	}` |
|        - |  1632 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1633 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1634 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1635 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1636 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1637 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1638 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1639 | `	pVm->bHttpContext = 0;` |
|        - |  1640 | `	/* Set the ready flag */` |
|      ! 0 |  1641 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1642 | `	return SXRET_OK;` |
|      ! 0 |  1643 |  |
|        - |  1644 | `/*` |
|        - |  1645 | ` * Release a Virtual Machine.` |
|        - |  1646 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1647 | ` */` |
|     2532 |  1648 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1649 |  |
|        - |  1650 | `	/* Set the stale magic number */` |
|     2534 |  1651 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1652 | `	/* Release the private memory subsystem */` |
|     2534 |  1653 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2534 |  1654 | `	return SXRET_OK;` |
|        2 |  1655 |  |
|        - |  1656 | `/*` |
|        - |  1657 | ` * Initialize a foreign function call context.` |
|        - |  1658 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1659 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1660 | ` * functions.` |
|        - |  1661 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1662 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1663 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1664 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1665 | ` */` |
|   624806 |  1666 | `static sxi32 VmInitCallContext(` |
|        - |  1667 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1668 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1669 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1670 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1671 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1672 | `	)` |
|        2 |  1673 |  |
|   624808 |  1674 | `	pOut->pFunc = pFunc;` |
|   624808 |  1675 | `	pOut->pVm   = pVm;` |
|   624808 |  1676 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   624808 |  1677 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1678 | `	/* Assume a null return value */` |
|   624808 |  1679 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   624808 |  1680 | `	pOut->pRet = pRet;` |
|   624808 |  1681 | `	pOut->iFlags = iFlags;` |
|   624808 |  1682 | `	return SXRET_OK;` |
|        2 |  1683 |  |
|        - |  1684 | `/*` |
|        - |  1685 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1686 | ` * left behind.` |
|        - |  1687 | ` */` |
|   624806 |  1688 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1689 |  |
|        - |  1690 | `	sxu32 n;` |
|   624808 |  1691 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7574 |  1692 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    21778 |  1693 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    14206 |  1694 | `			if( apObj[n] == 0 ){` |
|        - |  1695 | `				/* Already released */` |
|      298 |  1696 | `				continue;` |
|        - |  1697 | `			}` |
|    13910 |  1698 | `			PH7_MemObjRelease(apObj[n]);` |
|    13910 |  1699 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6956 |  1700 | `		}` |
|     7574 |  1701 | `		SySetRelease(&pCtx->sVar);` |
|     3786 |  1702 | `	}` |
|   624808 |  1703 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1704 | `		ph7_aux_data *aAux;` |
|        - |  1705 | `		void *pChunk;` |
|        - |  1706 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1707 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1708 | `		 */` |
|        9 |  1709 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1710 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1711 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1712 | `			/* Release the chunk */` |
|       25 |  1713 | `			if( pChunk ){` |
|       25 |  1714 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1715 | `			}` |
|       13 |  1716 | `		}` |
|        9 |  1717 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1718 | `	}` |
|   624808 |  1719 |  |
|        - |  1720 | `/*` |
|        - |  1721 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1722 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1723 | ` */` |
|      296 |  1724 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1725 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1726 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1727 | `	)` |
|        2 |  1728 |  |
|      298 |  1729 | `	if( pValue == 0 ){` |
|        - |  1730 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1731 | `		return;` |
|        - |  1732 | `	}` |
|      298 |  1733 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1734 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1735 | `		sxu32 n;` |
|     1054 |  1736 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1737 | `			if( apObj[n] == pValue ){` |
|      298 |  1738 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1739 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1740 | `				/* Mark as released */` |
|      298 |  1741 | `				apObj[n] = 0;` |
|      298 |  1742 | `				break;` |
|        - |  1743 | `			}` |
|      380 |  1744 | `		}` |
|      148 |  1745 | `	}` |
|      150 |  1746 |  |
|        - |  1747 | `/*` |
|        - |  1748 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1749 | ` */` |
|  3592598 |  1750 | `static void VmPopOperand(` |
|        - |  1751 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1752 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|  3592600 |  1755 | `	ph7_value *pTos = *ppTos;` |
|  7643154 |  1756 | `	while( nPop > 0 ){` |
|  4050556 |  1757 | `		PH7_MemObjRelease(pTos);` |
|  4050556 |  1758 | `		pTos--;` |
|  4050556 |  1759 | `		nPop--;` |
|        2 |  1760 | `	}` |
|        - |  1761 | `	/* Top of the stack */` |
|  3592600 |  1762 | `	*ppTos = pTos;` |
|  3592600 |  1763 |  |
|        - |  1764 | `/*` |
|        - |  1765 | ` * Reserve a memory object.` |
|        - |  1766 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1767 | ` */` |
|  3101924 |  1768 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1769 |  |
|  3101926 |  1770 | `	ph7_value *pObj = 0;` |
|        - |  1771 | `	VmSlot *pSlot;` |
|        - |  1772 | `	sxu32 nIdx;` |
|        - |  1773 | `	/* Check for a free slot */` |
|  3101926 |  1774 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3101926 |  1775 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3101926 |  1776 | `	if( pSlot ){` |
|   955528 |  1777 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   955528 |  1778 | `		nIdx = pSlot->nIdx;` |
|   477763 |  1779 | `	}` |
|  3101926 |  1780 | `	if( pObj == 0 ){` |
|        - |  1781 | `		/* Reserve a new memory object */` |
|  2146400 |  1782 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2146400 |  1783 | `		if( pObj == 0 ){` |
|      ! 0 |  1784 | `			return 0;` |
|        - |  1785 | `		}` |
|  1073199 |  1786 | `	}` |
|        - |  1787 | `	/* Set a null default value */` |
|  3101926 |  1788 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3101926 |  1789 | `	pObj->nIdx = nIdx;` |
|  3101926 |  1790 | `	return pObj;` |
|  1550964 |  1791 |  |
|        - |  1792 | `/*` |
|        - |  1793 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1794 | ` */` |
|    32656 |  1795 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1796 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1797 | `	const char *zKey,  /* Entry key */` |
|        - |  1798 | `	sxu32 nByte,       /* Key length */` |
|        - |  1799 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1800 | `	)` |
|        2 |  1801 |  |
|        - |  1802 | `	ph7_value sKey;` |
|        - |  1803 | `	sxi32 rc;` |
|    32658 |  1804 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32658 |  1805 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1806 | `	/* Perform the insertion */` |
|    32658 |  1807 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32658 |  1808 | `	PH7_MemObjRelease(&sKey);` |
|    32658 |  1809 | `	return rc;` |
|        2 |  1810 |  |
|        - |  1811 | `/*` |
|        - |  1812 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1813 | ` * Return a pointer to the variable value on success.` |
|        - |  1814 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1815 | ` */` |
|  3344292 |  1816 | `static ph7_value * VmExtractMemObj(` |
|        - |  1817 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1818 | `	const SyString *pName, /* Variable name */` |
|        - |  1819 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1820 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1821 | `	)` |
|        2 |  1822 |  |
|  3344294 |  1823 | `	int bNullify = FALSE;` |
|        - |  1824 | `	SyHashEntry *pEntry;` |
|        - |  1825 | `	VmFrame *pFrame;` |
|        - |  1826 | `	ph7_value *pObj;` |
|        - |  1827 | `	sxu32 nIdx;` |
|        - |  1828 | `	sxi32 rc;` |
|        - |  1829 | `	/* Point to the top active frame */` |
|  3344294 |  1830 | `	pFrame = pVm->pFrame;` |
|  3344294 |  1831 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1832 | `	/* Perform the lookup */` |
|  3344294 |  1833 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1834 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1835 | `		pName = &sAnnon;` |
|        - |  1836 | `		/* Always nullify the object */` |
|      ! 0 |  1837 | `		bNullify = TRUE;` |
|      ! 0 |  1838 | `		bDup = FALSE;` |
|      ! 0 |  1839 | `	}` |
|        - |  1840 | `	/* Check the superglobals table first */` |
|  3344294 |  1841 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3344294 |  1842 | `	if( pEntry == 0 ){` |
|        - |  1843 | `		/* Query the top active frame */` |
|  3344254 |  1844 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3344254 |  1845 | `		if( pEntry == 0 ){` |
|    94888 |  1846 | `			char *zName = (char *)pName->zString;` |
|        - |  1847 | `			VmSlot sLocal;` |
|    94888 |  1848 | `			if( !bCreate ){` |
|        - |  1849 | `				/* Do not create the variable,return NULL instead */` |
|      116 |  1850 | `				return 0;` |
|        - |  1851 | `			}` |
|        - |  1852 | `			/* No such variable,automatically create a new one and install` |
|        - |  1853 | `			 * it in the current frame.` |
|        - |  1854 | `			 */` |
|    94774 |  1855 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    94774 |  1856 | `			if( pObj == 0 ){` |
|      ! 0 |  1857 | `				return 0;` |
|        - |  1858 | `			}` |
|    94774 |  1859 | `			nIdx = pObj->nIdx;` |
|    94774 |  1860 | `			if( bDup ){` |
|        - |  1861 | `				/* Duplicate name */` |
|      168 |  1862 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1863 | `				if( zName == 0 ){` |
|      ! 0 |  1864 | `					return 0;` |
|        - |  1865 | `				}` |
|       83 |  1866 | `			}` |
|        - |  1867 | `			/* Link to the top active VM frame */` |
|    94774 |  1868 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    94774 |  1869 | `			if( rc != SXRET_OK ){` |
|        - |  1870 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1871 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1872 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1873 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1874 | `				return 0;` |
|        - |  1875 | `			}` |
|    94774 |  1876 | `			if( pFrame->pParent != 0 ){` |
|        - |  1877 | `				/* Local variable */` |
|    87546 |  1878 | `				sLocal.nIdx = nIdx;` |
|    87546 |  1879 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    43774 |  1880 | `			}else{` |
|        - |  1881 | `				/* Register in the $GLOBALS array */` |
|     7230 |  1882 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1883 | `			}` |
|        - |  1884 | `			/* Install in the reference table */` |
|    94774 |  1885 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1886 | `			/* Save object index */` |
|    94774 |  1887 | `			pObj->nIdx = nIdx;` |
|    47388 |  1888 | `		}else{` |
|        - |  1889 | `			/* Extract variable contents */` |
|  3249368 |  1890 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3249368 |  1891 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3249368 |  1892 | `			if( bNullify && pObj ){` |
|      ! 0 |  1893 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1894 | `			}` |
|        - |  1895 | `		}` |
|  1672181 |  1896 | `	}else{` |
|        - |  1897 | `		/* Superglobal */` |
|       42 |  1898 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1899 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1900 | `	}` |
|  3344180 |  1901 | `	return pObj;` |
|  1672258 |  1902 |  |
|        - |  1903 | `/*` |
|        - |  1904 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1905 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1906 | ` */` |
|     2844 |  1907 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1908 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1909 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1910 | `	sxu32 nByte        /* zName length */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|        - |  1913 | `	SyHashEntry *pEntry;` |
|        - |  1914 | `	ph7_value *pValue;` |
|        - |  1915 | `	sxu32 nIdx;` |
|        - |  1916 | `	/* Query the superglobal table */` |
|     2846 |  1917 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2846 |  1918 | `	if( pEntry == 0 ){` |
|        - |  1919 | `		/* No such entry */` |
|      ! 0 |  1920 | `		return 0;` |
|        - |  1921 | `	}` |
|        - |  1922 | `	/* Extract the superglobal index in the global object pool */` |
|     2846 |  1923 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1924 | `	/* Extract the variable value  */` |
|     2846 |  1925 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2846 |  1926 | `	return pValue;` |
|     1424 |  1927 |  |
|        - |  1928 | `/*` |
|        - |  1929 | ` * Perform a raw hashmap insertion.` |
|        - |  1930 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1931 | ` */` |
|     2874 |  1932 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1933 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1934 | `	const char *zKey,   /* Entry key */` |
|        - |  1935 | `	int nKeylen,        /* zKey length*/` |
|        - |  1936 | `	const char *zData,  /* Entry data */` |
|        - |  1937 | `	int nLen            /* zData length */` |
|        - |  1938 | `	)` |
|        2 |  1939 |  |
|        - |  1940 | `	ph7_value sKey,sValue;` |
|        - |  1941 | `	sxi32 rc;` |
|     2876 |  1942 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2876 |  1943 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2876 |  1944 | `	if( zKey ){` |
|     2854 |  1945 | `		if( nKeylen < 0 ){` |
|     2802 |  1946 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1400 |  1947 | `		}` |
|     2854 |  1948 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1426 |  1949 | `	}` |
|     2876 |  1950 | `	if( zData ){` |
|     2876 |  1951 | `		if( nLen < 0 ){` |
|        - |  1952 | `			/* Compute length automatically */` |
|      144 |  1953 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1954 | `		}` |
|     2876 |  1955 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1437 |  1956 | `	}` |
|        - |  1957 | `	/* Perform the insertion */` |
|     2876 |  1958 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2876 |  1959 | `	PH7_MemObjRelease(&sKey);` |
|     2876 |  1960 | `	PH7_MemObjRelease(&sValue);` |
|     2876 |  1961 | `	return rc;` |
|        2 |  1962 |  |
|        - |  1963 | `/*` |
|        - |  1964 | ` * Configure a working virtual machine instance.` |
|        - |  1965 | ` *` |
|        - |  1966 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1967 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1968 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1969 | ` * The second argument to this function is an integer configuration option` |
|        - |  1970 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1971 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1972 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1973 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1974 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1975 | ` */` |
|    40970 |  1976 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1977 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1978 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1979 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1980 | `	)` |
|        2 |  1981 |  |
|    40972 |  1982 | `	sxi32 rc = SXRET_OK;` |
|    40972 |  1983 | `	switch(nOp){` |
|     1262 |  1984 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2526 |  1985 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2526 |  1986 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1987 | `		/* VM output consumer callback */` |
|        - |  1988 | `#ifdef UNTRUST` |
|        - |  1989 | `		if( xConsumer == 0 ){` |
|        - |  1990 | `			rc = SXERR_CORRUPT;` |
|        - |  1991 | `			break;` |
|        - |  1992 | `		}` |
|        - |  1993 | `#endif` |
|        - |  1994 | `		/* Install the output consumer */` |
|     2526 |  1995 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2526 |  1996 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2526 |  1997 | `		break;` |
|        - |  1998 | `							   }` |
|     1270 |  1999 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2000 | `		/* Import path */` |
|        - |  2001 | `		  const char *zPath;` |
|        - |  2002 | `		  SyString sPath;` |
|     2542 |  2003 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2004 | `#if defined(UNTRUST)` |
|        - |  2005 | `		  if( zPath == 0 ){` |
|        - |  2006 | `			  rc = SXERR_EMPTY;` |
|        - |  2007 | `			  break;` |
|        - |  2008 | `		  }` |
|        - |  2009 | `#endif` |
|     2542 |  2010 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2011 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2012 | `#ifdef __WINNT__` |
|        2 |  2013 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2014 | `#endif` |
|     5082 |  2015 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2016 | `		  /* Remove leading and trailing white spaces */` |
|     2542 |  2017 | `		  SyStringFullTrim(&sPath);` |
|     2542 |  2018 | `		  if( sPath.nByte > 0 ){` |
|        - |  2019 | `			  /* Store the path in the corresponding conatiner */` |
|     2542 |  2020 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1270 |  2021 | `		  }` |
|     2542 |  2022 | `		  break;` |
|        - |  2023 | `									 }` |
|     1270 |  2024 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2025 | `		/* Run-Time Error report */` |
|     2542 |  2026 | `		pVm->bErrReport = 1;` |
|     2542 |  2027 | `		break;` |
|      ! 0 |  2028 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2029 | `		/* Recursion depth */` |
|      ! 0 |  2030 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2031 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2032 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2033 | `		}` |
|      ! 0 |  2034 | `		break;` |
|        - |  2035 | `									   }` |
|      ! 0 |  2036 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2037 | `		/* VM output length in bytes */` |
|      ! 0 |  2038 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2039 | `#ifdef UNTRUST` |
|        - |  2040 | `		if( pOut == 0 ){` |
|        - |  2041 | `			rc = SXERR_CORRUPT;` |
|        - |  2042 | `			break;` |
|        - |  2043 | `		}` |
|        - |  2044 | `#endif` |
|      ! 0 |  2045 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2046 | `		break;` |
|        - |  2047 | `							   }` |
|        - |  2048 |  |
|    12700 |  2049 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2050 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2051 | `		/* Create a new superglobal/global variable */` |
|    25402 |  2052 | `		const char *zName = va_arg(ap,const char *);` |
|    25402 |  2053 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2054 | `		SyHashEntry *pEntry;` |
|        - |  2055 | `		ph7_value *pObj;` |
|        - |  2056 | `		sxu32 nByte;` |
|        - |  2057 | `		sxu32 nIdx;` |
|        - |  2058 | `#ifdef UNTRUST` |
|        - |  2059 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2060 | `			rc = SXERR_CORRUPT;` |
|        - |  2061 | `			break;` |
|        - |  2062 | `		}` |
|        - |  2063 | `#endif` |
|    25402 |  2064 | `		nByte = SyStrlen(zName);` |
|    25402 |  2065 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2066 | `			/* Check if the superglobal is already installed */` |
|    25402 |  2067 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12702 |  2068 | `		}else{` |
|        - |  2069 | `			/* Query the top active VM frame */` |
|      ! 0 |  2070 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2071 | `		}` |
|    25402 |  2072 | `		if( pEntry ){` |
|        - |  2073 | `			/* Variable already installed */` |
|      ! 0 |  2074 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2075 | `			/* Extract contents */` |
|      ! 0 |  2076 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2077 | `			if( pObj ){` |
|        - |  2078 | `				/* Overwrite old contents */` |
|      ! 0 |  2079 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2080 | `			}` |
|      ! 0 |  2081 | `		}else{` |
|        - |  2082 | `			/* Install a new variable */` |
|    25402 |  2083 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25402 |  2084 | `			if( pObj == 0 ){` |
|      ! 0 |  2085 | `				rc = SXERR_MEM;` |
|      ! 0 |  2086 | `				break;` |
|        - |  2087 | `			}` |
|    25402 |  2088 | `			nIdx = pObj->nIdx;` |
|        - |  2089 | `			/* Copy value */` |
|    25402 |  2090 | `			PH7_MemObjStore(pValue,pObj);` |
|    25402 |  2091 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2092 | `				/* Install the superglobal */` |
|    25402 |  2093 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12702 |  2094 | `			}else{` |
|        - |  2095 | `				/* Install in the current frame */` |
|      ! 0 |  2096 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2097 | `			}` |
|    25402 |  2098 | `			if( rc == SXRET_OK ){` |
|        - |  2099 | `				SyHashEntry *pRef;` |
|    25402 |  2100 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25402 |  2101 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12702 |  2102 | `				}else{` |
|      ! 0 |  2103 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2104 | `				}` |
|        - |  2105 | `				/* Install in the reference table */` |
|    25402 |  2106 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25402 |  2107 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2108 | `					/* Register in the $GLOBALS array */` |
|    25402 |  2109 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12700 |  2110 | `				}` |
|    12700 |  2111 | `			}` |
|        - |  2112 | `		}` |
|    25402 |  2113 | `		break;` |
|        - |  2114 | `									}` |
|     1400 |  2115 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2116 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2117 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2118 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2119 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2120 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2121 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2802 |  2122 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2802 |  2123 | `		const char *zValue = va_arg(ap,const char *);` |
|     2802 |  2124 | `		int nLen = va_arg(ap,int);` |
|        - |  2125 | `		ph7_hashmap *pMap;` |
|        - |  2126 | `		ph7_value *pValue;` |
|     2802 |  2127 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2128 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2129 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2801 |  2130 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2131 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2132 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2800 |  2133 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2134 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2135 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2800 |  2136 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2137 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2138 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2800 |  2139 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2140 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2141 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2800 |  2142 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2143 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2144 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2145 | `		}else{` |
|        - |  2146 | `			/* Extract the $_SERVER superglobal */` |
|     2800 |  2147 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2148 | `		}` |
|     2802 |  2149 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2150 | `			/* No such entry */` |
|      ! 0 |  2151 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2152 | `			break;` |
|        - |  2153 | `		}` |
|        - |  2154 | `		/* Point to the hashmap */` |
|     2802 |  2155 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2156 | `		/* Perform the insertion */` |
|     2802 |  2157 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2802 |  2158 | `		break;` |
|        - |  2159 | `								   }` |
|       11 |  2160 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2161 | `		/* Script arguments */` |
|       24 |  2162 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2163 | `		ph7_hashmap *pMap;` |
|        - |  2164 | `		ph7_value *pValue;` |
|        - |  2165 | `		sxu32 n;` |
|       24 |  2166 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2167 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2168 | `			break;` |
|        - |  2169 | `		}` |
|        - |  2170 | `		/* Extract the $argv array */` |
|       24 |  2171 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2172 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2173 | `			/* No such entry */` |
|      ! 0 |  2174 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2175 | `			break;` |
|        - |  2176 | `		}` |
|        - |  2177 | `		/* Point to the hashmap */` |
|       24 |  2178 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2179 | `		/* Perform the insertion */` |
|       24 |  2180 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2181 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2182 | `		if( rc == SXRET_OK ){` |
|       24 |  2183 | `			if( pMap->nEntry > 1 ){` |
|        - |  2184 | `				/* Append space separator first */` |
|       18 |  2185 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2186 | `			}` |
|       24 |  2187 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2188 | `		}` |
|       24 |  2189 | `		break;` |
|        - |  2190 | `								  }` |
|      ! 0 |  2191 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2192 | `		/* error_log() consumer */` |
|      ! 0 |  2193 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2194 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2195 | `		break;` |
|        - |  2196 | `										}` |
|      ! 0 |  2197 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2198 | `		/* Script return value */` |
|      ! 0 |  2199 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2200 | `#ifdef UNTRUST` |
|        - |  2201 | `		if( ppValue == 0 ){` |
|        - |  2202 | `			rc = SXERR_CORRUPT;` |
|        - |  2203 | `			break;` |
|        - |  2204 | `		}` |
|        - |  2205 | `#endif` |
|      ! 0 |  2206 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2207 | `		break;` |
|        - |  2208 | `								   }` |
|     2540 |  2209 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2210 | `		/* Register an IO stream device */` |
|     5082 |  2211 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2212 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7620 |  2213 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5082 |  2214 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2215 | `				/* Invalid stream */` |
|      ! 0 |  2216 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2217 | `				break;` |
|        - |  2218 | `		}` |
|     5082 |  2219 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2220 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2542 |  2221 | `			pVm->pDefStream = pStream;` |
|     1270 |  2222 | `		}` |
|        - |  2223 | `		/* Insert in the appropriate container */` |
|     5082 |  2224 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5082 |  2225 | `		break;` |
|        - |  2226 | `								  }` |
|        8 |  2227 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2228 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2229 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2230 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2231 | `#ifdef UNTRUST` |
|        - |  2232 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2233 | `			rc = SXERR_CORRUPT;` |
|        - |  2234 | `			break;` |
|        - |  2235 | `		}` |
|        - |  2236 | `#endif` |
|       16 |  2237 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2238 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2239 | `		break;` |
|        - |  2240 | `									   }` |
|        8 |  2241 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2242 | `		/* Raw HTTP request*/` |
|       16 |  2243 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2244 | `		int nByte = va_arg(ap,int);` |
|       16 |  2245 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2246 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2247 | `			break;` |
|        - |  2248 | `		}` |
|       16 |  2249 | `		if( nByte < 0 ){` |
|        - |  2250 | `			/* Compute length automatically */` |
|      ! 0 |  2251 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2252 | `		}` |
|        - |  2253 | `		/* Process the request */` |
|       16 |  2254 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2255 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2256 | `		if( rc == SXRET_OK ){` |
|       16 |  2257 | `			pVm->bHttpContext = 1;` |
|        8 |  2258 | `		}` |
|       16 |  2259 | `		break;` |
|        - |  2260 | `									}` |
|        8 |  2261 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2262 | `		/* Extract HTTP response status code */` |
|       16 |  2263 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2264 | `		if( pStatus ){` |
|       16 |  2265 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2266 | `		}` |
|       16 |  2267 | `		break;` |
|        - |  2268 | `										}` |
|        8 |  2269 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2270 | `		/* Iterate response headers via callback */` |
|        - |  2271 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2272 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2273 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2274 | `		if( xCallback ){` |
|       16 |  2275 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2276 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2277 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2278 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2279 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2280 | `							   pUserData);` |
|       12 |  2281 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2282 | `					break;` |
|        - |  2283 | `				}` |
|        6 |  2284 | `			}` |
|        8 |  2285 | `		}` |
|       16 |  2286 | `		break;` |
|        - |  2287 | `										 }` |
|      ! 0 |  2288 | `	default:` |
|        - |  2289 | `		/* Unknown configuration option */` |
|      ! 0 |  2290 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2291 | `		break;` |
|        - |  2292 | `	}` |
|    40972 |  2293 | `	return rc;` |
|        2 |  2294 |  |
|        - |  2295 | `/* Forward declaration */` |
|        - |  2296 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2297 | `/*` |
|        - |  2298 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2299 | ` * format.` |
|        - |  2300 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2301 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2302 | ` * (STDOUT).` |
|        - |  2303 | ` */` |
|        2 |  2304 | `static sxi32 VmByteCodeDump(` |
|        - |  2305 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2306 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2307 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2308 | `	)` |
|        1 |  2309 |  |
|        - |  2310 | `	static const char zDump[] = {` |
|        - |  2311 | `		"====================================================\n"` |
|        - |  2312 | `		"PH7 VM Dump\n"` |
|        - |  2313 | `		"====================================================\n"` |
|        - |  2314 | `	};` |
|        - |  2315 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2316 | `	sxi32 rc = SXRET_OK;` |
|        - |  2317 | `	sxu32 n;` |
|        - |  2318 | `	/* Point to the PH7 instructions */` |
|        3 |  2319 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2320 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2321 | `	n = 0;` |
|        3 |  2322 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2323 | `	/* Dump instructions */` |
|        7 |  2324 | `	for(;;){` |
|       15 |  2325 | `		if( pInstr >= pEnd ){` |
|        - |  2326 | `			/* No more instructions */` |
|        3 |  2327 | `			break;` |
|        - |  2328 | `		}` |
|        - |  2329 | `		/* Format and call the consumer callback */` |
|       19 |  2330 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2331 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2332 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2333 | `		if( rc != SXRET_OK ){` |
|        - |  2334 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2335 | `			return rc;` |
|        - |  2336 | `		}` |
|       13 |  2337 | `		++n;` |
|       13 |  2338 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2339 | `	}` |
|        3 |  2340 | `	return rc;` |
|        2 |  2341 |  |
|        - |  2342 | `/* Forward declaration */` |
|        - |  2343 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2344 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2345 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2347 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2348 | `/*` |
|        - |  2349 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2350 | ` * consumer callback.` |
|        - |  2351 | ` */` |
|      564 |  2352 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2353 |  |
|      565 |  2354 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      565 |  2355 | `	sxi32 rc = SXRET_OK;` |
|        - |  2356 | `	/* Append a new line */` |
|        - |  2357 | `#ifdef __WINNT__` |
|        1 |  2358 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2359 | `#else` |
|      564 |  2360 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2361 | `#endif` |
|        - |  2362 | `	/* Invoke the output consumer callback */` |
|      565 |  2363 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      565 |  2364 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      565 |  2365 | `	return rc;` |
|        1 |  2366 |  |
|        - |  2367 | `/*` |
|        - |  2368 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2369 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2370 | ` * information.` |
|        - |  2371 | ` */` |
|      134 |  2372 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2373 |  |
|      136 |  2374 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2375 | `		ph7_value apArg[4];` |
|        - |  2376 | `		ph7_value *apArgPtr[4];` |
|        - |  2377 | `		ph7_value sResult;` |
|        - |  2378 | `		SyString sErr;` |
|        - |  2379 | `		/* Prepare arguments */` |
|       61 |  2380 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2381 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2382 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2383 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2384 | `		if( pFile ){` |
|       61 |  2385 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2386 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2387 | `		}else{` |
|      ! 0 |  2388 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2389 | `		}` |
|       61 |  2390 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2391 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2392 | `		/* Set up pointer array */` |
|       61 |  2393 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2394 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2395 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2396 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2397 | `		/* Call the handler */` |
|       61 |  2398 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2399 | `		/* Check return value */` |
|       61 |  2400 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2401 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2402 | `		}` |
|        - |  2403 | `		/* Release */` |
|       61 |  2404 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2405 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2406 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2407 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2408 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2409 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2410 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2411 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2412 | `	}` |
|        - |  2413 | `	/* No handler, always call error handler */` |
|       75 |  2414 | `	return TRUE;` |
|       69 |  2415 |  |
|       98 |  2416 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2417 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2418 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2419 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2420 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2421 | `	)` |
|        2 |  2422 |  |
|      100 |  2423 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2424 | `	SyString *pFile;` |
|        - |  2425 | `	char *zErr;` |
|      100 |  2426 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2427 | `	if( !pVm->bErrReport ){` |
|        - |  2428 | `		/* Don't bother reporting errors */` |
|        3 |  2429 | `		return SXRET_OK;` |
|        - |  2430 | `	}` |
|        - |  2431 | `	/* Reset the working buffer */` |
|       98 |  2432 | `	SyBlobReset(pWorker);` |
|        - |  2433 | `	/* Peek the processed file if available */` |
|       98 |  2434 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2435 | `	if( pFile ){` |
|        - |  2436 | `		/* Append file name */` |
|       98 |  2437 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2438 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2439 | `	}` |
|        - |  2440 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2441 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2442 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2443 | `	 * E_DEPRECATED). */` |
|       98 |  2444 | `	zErr = "Error:  ";` |
|       98 |  2445 | `	switch(iErr){` |
|       19 |  2446 | `	case PH7_CTX_WARNING:` |
|       40 |  2447 | `		zErr = "Warning:  ";` |
|       40 |  2448 | `		break;` |
|        6 |  2449 | `	case PH7_CTX_NOTICE:` |
|       14 |  2450 | `		zErr = "Notice:  ";` |
|       12 |  2451 | `		break;` |
|       23 |  2452 | `	default:` |
|        - |  2453 | `		/* keep iErr unchanged */` |
|       46 |  2454 | `		break;` |
|        - |  2455 | `	}` |
|       98 |  2456 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2457 | `	if( pFuncName ){` |
|        - |  2458 | `		/* Append function name first */` |
|       23 |  2459 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2460 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2461 | `	}` |
|       98 |  2462 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2463 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2464 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2465 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2466 | `	}` |
|       98 |  2467 | `	return rc;` |
|       51 |  2468 |  |
|        - |  2469 | `/*` |
|        - |  2470 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2471 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2472 | ` * information.` |
|        - |  2473 | ` */` |
|       38 |  2474 | `static sxi32 VmThrowErrorAp(` |
|        - |  2475 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2476 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2477 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2478 | `	const char *zFormat, /* Format message */` |
|        - |  2479 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2480 | `	)` |
|        2 |  2481 |  |
|       40 |  2482 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2483 | `	SyBlob sMsg;` |
|        - |  2484 | `	SyString *pFile;` |
|        - |  2485 | `	char *zErr;` |
|       40 |  2486 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2487 | `	if( !pVm->bErrReport ){` |
|        - |  2488 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2489 | `		return SXRET_OK;` |
|        - |  2490 | `	}` |
|        - |  2491 | `	/* Reset the working buffer */` |
|       40 |  2492 | `	SyBlobReset(pWorker);` |
|        - |  2493 | `	/* Peek the processed file if available */` |
|       40 |  2494 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2495 | `	if( pFile ){` |
|        - |  2496 | `		/* Append file name */` |
|       40 |  2497 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2498 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2499 | `	}` |
|        - |  2500 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2501 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2502 | `	 * the correct errno value. */` |
|       40 |  2503 | `	zErr = "Error:  ";` |
|       40 |  2504 | `	switch(iErr){` |
|        4 |  2505 | `	case PH7_CTX_WARNING:` |
|        9 |  2506 | `		zErr = "Warning:  ";` |
|        9 |  2507 | `		break;` |
|        3 |  2508 | `	case PH7_CTX_NOTICE:` |
|        7 |  2509 | `		zErr = "Notice:  ";` |
|        6 |  2510 | `		break;` |
|       12 |  2511 | `	default:` |
|        - |  2512 | `		/* do not change iErr */` |
|       24 |  2513 | `		break;` |
|        - |  2514 | `	}` |
|       40 |  2515 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2516 | `	if( pFuncName ){` |
|        - |  2517 | `		/* Append function name first */` |
|       26 |  2518 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2519 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2520 | `	}` |
|        - |  2521 | `	/* Format the raw message */` |
|       40 |  2522 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2523 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2524 | `	/* Check if a user error handler is installed */` |
|       40 |  2525 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2526 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2527 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2528 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2529 | `	}` |
|       40 |  2530 | `	SyBlobRelease(&sMsg);` |
|       40 |  2531 | `	return rc;` |
|       21 |  2532 |  |
|        - |  2533 | `/*` |
|        - |  2534 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2535 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2536 | ` * possible.` |
|        - |  2537 | ` */` |
|       36 |  2538 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2539 |  |
|        - |  2540 | `	ph7_class *pClass;` |
|       37 |  2541 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2542 | `	ph7_class_instance *pThis;` |
|        - |  2543 | `	ph7_class_method *pCons;` |
|        - |  2544 | `	ph7_value sArg;` |
|        - |  2545 | `	ph7_value *apArg[1];` |
|        - |  2546 | `	SyBlob sMsg;` |
|        - |  2547 | `	SyString sMsgStr;` |
|        - |  2548 | `	VmFrame *pFrame;` |
|        - |  2549 | `	sxi32 rc;` |
|       37 |  2550 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       37 |  2551 | `	if( pClass == 0 ){` |
|      ! 0 |  2552 | `		return PH7_ABORT;` |
|        - |  2553 | `	}` |
|       37 |  2554 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       37 |  2555 | `	if( pThis == 0 ){` |
|      ! 0 |  2556 | `		return PH7_ABORT;` |
|        - |  2557 | `	}` |
|       37 |  2558 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2559 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2560 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2561 | `	{` |
|       37 |  2562 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       37 |  2563 | `		if( pOwner ){` |
|       37 |  2564 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       18 |  2565 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       19 |  2566 | `		}else{` |
|      ! 0 |  2567 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2568 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2569 | `		}` |
|        - |  2570 | `	}` |
|       37 |  2571 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       37 |  2572 | `	if( pCons ){` |
|       37 |  2573 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       37 |  2574 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       37 |  2575 | `		apArg[0] = &sArg;` |
|       37 |  2576 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       37 |  2577 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  2578 | `	}` |
|       37 |  2579 | `	SyBlobRelease(&sMsg);` |
|       37 |  2580 | `	pFrame = pVm->pFrame;` |
|       37 |  2581 | `	if( pFrame ){` |
|       37 |  2582 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       37 |  2583 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  2584 | `	}` |
|       37 |  2585 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       37 |  2586 | `	PH7_ClassInstanceUnref(pThis);` |
|       37 |  2587 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2588 | `		return PH7_ABORT;` |
|        - |  2589 | `	}` |
|       37 |  2590 | `	return PH7_EXCEPTION;` |
|       19 |  2591 |  |
|        - |  2592 |  |
|        - |  2593 | `/*` |
|        - |  2594 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2595 | ` */` |
|        4 |  2596 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2597 |  |
|        - |  2598 | `	ph7_class *pErrClass;` |
|        - |  2599 | `	ph7_class_instance *pThis;` |
|        - |  2600 | `	ph7_class_method *pCons;` |
|        - |  2601 | `	ph7_value sArg;` |
|        - |  2602 | `	ph7_value *apArg[1];` |
|        - |  2603 | `	SyBlob sMsg;` |
|        - |  2604 | `	SyString sMsgStr;` |
|        - |  2605 | `	VmFrame *pFrame;` |
|        - |  2606 | `	sxi32 rc;` |
|        5 |  2607 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2608 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2609 | `		return PH7_ABORT;` |
|        - |  2610 | `	}` |
|        5 |  2611 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2612 | `	if( pThis == 0 ){` |
|      ! 0 |  2613 | `		return PH7_ABORT;` |
|        - |  2614 | `	}` |
|        5 |  2615 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2616 | `	{` |
|        5 |  2617 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2618 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2619 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2620 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2621 | `	}` |
|        5 |  2622 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2623 | `	if( pCons ){` |
|        5 |  2624 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2625 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2626 | `		apArg[0] = &sArg;` |
|        5 |  2627 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2628 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2629 | `	}` |
|        5 |  2630 | `	SyBlobRelease(&sMsg);` |
|        5 |  2631 | `	pFrame = pVm->pFrame;` |
|        5 |  2632 | `	if( pFrame ){` |
|        5 |  2633 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2634 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2635 | `	}` |
|        5 |  2636 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2637 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2638 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2639 | `		return PH7_ABORT;` |
|        - |  2640 | `	}` |
|        5 |  2641 | `	return PH7_EXCEPTION;` |
|        3 |  2642 |  |
|        - |  2643 |  |
|        - |  2644 | `/*` |
|        - |  2645 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2646 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2647 | ` * For class types, instanceof is verified.` |
|        - |  2648 | ` *` |
|        - |  2649 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2650 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2651 | ` */` |
|        - |  2652 | `/*` |
|        - |  2653 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2654 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2655 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2656 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2657 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2658 | ` */` |
|       16 |  2659 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2660 |  |
|        - |  2661 | `	const char *z, *zEnd, *zTail;` |
|        - |  2662 | `	sxu32 n;` |
|        - |  2663 | `	sxu8 bReal;` |
|        - |  2664 | `	sxi32 rc;` |
|       18 |  2665 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2666 | `		return 0;` |
|        - |  2667 | `	}` |
|       18 |  2668 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2669 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2670 | `	zEnd = z + n;` |
|       18 |  2671 | `	if( n == 0 ){` |
|      ! 0 |  2672 | `		return 0;` |
|        - |  2673 | `	}` |
|       18 |  2674 | `	zTail = 0;` |
|       18 |  2675 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2676 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        5 |  2677 | `		return 0;` |
|        - |  2678 | `	}` |
|        - |  2679 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       14 |  2680 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2681 | `		zTail++;` |
|      ! 0 |  2682 | `	}` |
|       14 |  2683 | `	return zTail == zEnd ? 1 : 0;` |
|       10 |  2684 |  |
|        - |  2685 |  |
|        - |  2686 | `/*` |
|        - |  2687 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2688 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2689 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2690 | ` *   0 if it's not strictly numeric.` |
|        - |  2691 | ` */` |
|       16 |  2692 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2693 |  |
|        - |  2694 | `	const char *z, *zEnd, *zTail;` |
|        - |  2695 | `	sxu32 n;` |
|       18 |  2696 | `	sxu8 bReal = 0;` |
|        - |  2697 | `	sxi32 rc;` |
|       18 |  2698 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2699 | `		return 0;` |
|        - |  2700 | `	}` |
|       18 |  2701 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2702 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2703 | `	zEnd = z + n;` |
|       18 |  2704 | `	if( n == 0 ) return 0;` |
|       18 |  2705 | `	zTail = 0;` |
|       18 |  2706 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2707 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2708 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2709 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2710 | `	return bReal ? 2 : 1;` |
|       10 |  2711 |  |
|        - |  2712 |  |
|        - |  2713 | `/*` |
|        - |  2714 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts* using` |
|        - |  2715 | ` * PHP 8 weak-mode union semantics. Returns SXRET_OK on accept (pValue may` |
|        - |  2716 | ` * have been mutated by the cast), SXERR_INVALID on reject. Caller is` |
|        - |  2717 | ` * responsible for the actual TypeError throw.` |
|        - |  2718 | ` *` |
|        - |  2719 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2720 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2721 | ` */` |
|       90 |  2722 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable)` |
|        2 |  2723 |  |
|        - |  2724 | `	sxu32 i;` |
|        - |  2725 | `	ph7_type_alt *aAlts;` |
|        - |  2726 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2727 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|       92 |  2728 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2729 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2730 | `	}` |
|       80 |  2731 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       80 |  2732 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       80 |  2733 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      236 |  2734 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      158 |  2735 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      134 |  2736 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      134 |  2737 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      134 |  2738 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       68 |  2739 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       40 |  2740 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2741 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       80 |  2742 | `	}` |
|        - |  2743 | `	/* Object handling */` |
|       80 |  2744 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2745 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2746 | `		if( bHasClassAlt ){` |
|       14 |  2747 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2748 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2749 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2750 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2751 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2752 | `			}` |
|       26 |  2753 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2754 | `				ph7_class *pExpected;` |
|        - |  2755 | `				SyString *pCN;` |
|       22 |  2756 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2757 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2758 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2759 | `					pExpected = pSelfNow;` |
|       22 |  2760 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2761 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2762 | `				}else{` |
|       22 |  2763 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2764 | `				}` |
|       22 |  2765 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2766 | `					return SXRET_OK;` |
|        - |  2767 | `				}` |
|        8 |  2768 | `			}` |
|        2 |  2769 | `		}` |
|        9 |  2770 | `		return SXERR_INVALID;` |
|        - |  2771 | `	}` |
|        - |  2772 | `	/* Array handling */` |
|       64 |  2773 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2774 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2775 | `	}` |
|        - |  2776 | `	/* Scalar handling — exact match first */` |
|       58 |  2777 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       22 |  2778 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2779 | `	}` |
|       38 |  2780 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2781 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2782 | `	}` |
|       34 |  2783 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       34 |  2784 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2785 | `	}` |
|       18 |  2786 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2787 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2788 | `	}` |
|        - |  2789 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2790 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2791 | `	 * to match PHP's union RFC. */` |
|        - |  2792 | `	{` |
|       18 |  2793 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2794 | `		if( bHasInt ){` |
|        - |  2795 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2796 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2797 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2798 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2799 | `				return SXRET_OK;` |
|        - |  2800 | `			}` |
|       18 |  2801 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2802 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2803 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2804 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2805 | `					return SXRET_OK;` |
|        - |  2806 | `				}` |
|      ! 0 |  2807 | `			}` |
|       18 |  2808 | `			if( kind == 1 ){` |
|        9 |  2809 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2810 | `				return SXRET_OK;` |
|        - |  2811 | `			}` |
|        4 |  2812 | `		}` |
|       10 |  2813 | `		if( bHasFloat ){` |
|       10 |  2814 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2815 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2816 | `				return SXRET_OK;` |
|        - |  2817 | `			}` |
|       10 |  2818 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2819 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2820 | `				return SXRET_OK;` |
|        - |  2821 | `			}` |
|        1 |  2822 | `		}` |
|        3 |  2823 | `		if( bHasString ){` |
|      ! 0 |  2824 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2825 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2826 | `				return SXRET_OK;` |
|        - |  2827 | `			}` |
|      ! 0 |  2828 | `		}` |
|        3 |  2829 | `		if( bHasBool ){` |
|      ! 0 |  2830 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2831 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2832 | `				return SXRET_OK;` |
|        - |  2833 | `			}` |
|      ! 0 |  2834 | `		}` |
|        - |  2835 | `	}` |
|        3 |  2836 | `	return SXERR_INVALID;` |
|       47 |  2837 |  |
|        - |  2838 |  |
|        - |  2839 | `/*` |
|        - |  2840 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2841 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2842 | ` */` |
|       16 |  2843 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2844 |  |
|       17 |  2845 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       25 |  2846 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       16 |  2847 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       17 |  2848 | `	return zBuf;` |
|        1 |  2849 |  |
|        - |  2850 |  |
|    12056 |  2851 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2852 |  |
|        - |  2853 | `	SyHashEntry *pSlot;` |
|        - |  2854 | `	VmClassAttr *pVmAttr;` |
|        - |  2855 | `	ph7_class_attr *pAttr;` |
|    12058 |  2856 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    12058 |  2857 | `	if( pSlot == 0 ){` |
|    11912 |  2858 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2859 | `	}` |
|      148 |  2860 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      148 |  2861 | `	pAttr = pVmAttr->pAttr;` |
|      148 |  2862 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2863 | `		return SXRET_OK;` |
|        - |  2864 | `	}` |
|        - |  2865 | `	/* Union type: dispatch to the shared coercion helper. */` |
|      148 |  2866 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  2867 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  2868 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0);` |
|       16 |  2869 | `		if( rc == SXRET_OK ){` |
|        9 |  2870 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  2871 | `			return SXRET_OK;` |
|        - |  2872 | `		}` |
|        7 |  2873 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2874 | `			char zBuf[128];` |
|        4 |  2875 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  2876 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2877 | `		}` |
|        5 |  2878 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2879 | `	}` |
|        - |  2880 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      134 |  2881 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       10 |  2882 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        8 |  2883 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        8 |  2884 | `			return SXRET_OK;` |
|        - |  2885 | `		}` |
|        3 |  2886 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  2887 | `	}` |
|        - |  2888 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  2889 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  2890 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      126 |  2891 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  2892 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  2893 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  2894 | `			return SXRET_OK;` |
|        - |  2895 | `		}` |
|        7 |  2896 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2897 | `	}` |
|      116 |  2898 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  2899 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  2900 | `		 * currently active on the self-stack. */` |
|       20 |  2901 | `		ph7_class *pExpected = 0;` |
|       20 |  2902 | `		SyString *pClassName = &pAttr->sClass;` |
|       20 |  2903 | `		ph7_class *pSelfNow = 0;` |
|       20 |  2904 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2905 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2906 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2907 | `		}` |
|       20 |  2908 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  2909 | `			pExpected = pSelfNow;` |
|       18 |  2910 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  2911 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2912 | `		}else{` |
|       16 |  2913 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  2914 | `		}` |
|       20 |  2915 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  2916 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2917 | `		}` |
|       20 |  2918 | `		if( pExpected ){` |
|       16 |  2919 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       16 |  2920 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  2921 | `				char zBuf[128];` |
|        7 |  2922 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2923 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2924 | `			}` |
|        5 |  2925 | `		}` |
|       16 |  2926 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       16 |  2927 | `		return SXRET_OK;` |
|        - |  2928 | `	}` |
|        - |  2929 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  2930 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|       98 |  2931 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2932 | `		char zBuf[128];` |
|        7 |  2933 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2934 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2935 | `	}` |
|       94 |  2936 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  2937 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  2938 | `		if( xCast ){` |
|        - |  2939 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  2940 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  2941 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2942 | `			}` |
|       24 |  2943 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  2944 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2945 | `			}` |
|        - |  2946 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  2947 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  2948 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  2949 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  2950 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  2951 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  2952 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  2953 | `			}` |
|       12 |  2954 | `			xCast(pValue);` |
|        5 |  2955 | `		}` |
|        5 |  2956 | `	}` |
|       80 |  2957 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       80 |  2958 | `	return SXRET_OK;` |
|     6030 |  2959 |  |
|        - |  2960 |  |
|        - |  2961 | `/*` |
|        - |  2962 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2963 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2964 | ` * information.` |
|        - |  2965 | ` * ------------------------------------` |
|        - |  2966 | ` * Simple boring wrapper function.` |
|        - |  2967 | ` * ------------------------------------` |
|        - |  2968 | ` */` |
|       14 |  2969 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2970 |  |
|        - |  2971 | `	va_list ap;` |
|        - |  2972 | `	sxi32 rc;` |
|       15 |  2973 | `	va_start(ap,zFormat);` |
|       15 |  2974 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2975 | `	va_end(ap);` |
|       15 |  2976 | `	return rc;` |
|        1 |  2977 |  |
|        - |  2978 | `/*` |
|        - |  2979 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2980 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2981 | ` */` |
|       30 |  2982 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2983 |  |
|        - |  2984 | `	ph7_class *pClass;` |
|        - |  2985 | `	ph7_class_instance *pThis;` |
|        - |  2986 | `	ph7_class_method *pCons;` |
|        - |  2987 | `	ph7_value sArg;` |
|        - |  2988 | `	ph7_value *apArg[1];` |
|        - |  2989 | `	SyBlob sMsg;` |
|        - |  2990 | `	SyString sMsgStr;` |
|        - |  2991 | `	VmFrame *pFrame;` |
|        - |  2992 | `	sxi32 rc;` |
|       31 |  2993 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       31 |  2994 | `	if( pClass == 0 ){` |
|      ! 0 |  2995 | `		return PH7_ABORT;` |
|        - |  2996 | `	}` |
|       31 |  2997 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       31 |  2998 | `	if( pThis == 0 ){` |
|      ! 0 |  2999 | `		return PH7_ABORT;` |
|        - |  3000 | `	}` |
|       31 |  3001 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       31 |  3002 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       15 |  3003 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       31 |  3004 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       31 |  3005 | `	if( pCons ){` |
|       31 |  3006 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3007 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       31 |  3008 | `		apArg[0] = &sArg;` |
|       31 |  3009 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       31 |  3010 | `		PH7_MemObjRelease(&sArg);` |
|       15 |  3011 | `	}` |
|       31 |  3012 | `	SyBlobRelease(&sMsg);` |
|       31 |  3013 | `	pFrame = pVm->pFrame;` |
|       31 |  3014 | `	if( pFrame ){` |
|       31 |  3015 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       31 |  3016 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       15 |  3017 | `	}` |
|       31 |  3018 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       31 |  3019 | `	PH7_ClassInstanceUnref(pThis);` |
|       31 |  3020 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3021 | `		return PH7_ABORT;` |
|        - |  3022 | `	}` |
|       31 |  3023 | `	return PH7_EXCEPTION;` |
|       16 |  3024 |  |
|        - |  3025 | `/*` |
|        - |  3026 | ` * Report a fatal named-argument error.` |
|        - |  3027 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3028 | ` */` |
|        6 |  3029 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3030 |  |
|        7 |  3031 | `	const char *zFunc = 0;` |
|        7 |  3032 | `	int nFunc = 0;` |
|        7 |  3033 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3034 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3035 |  |
|        - |  3036 | `/*` |
|        - |  3037 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3038 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3039 | ` * information.` |
|        - |  3040 | ` * ------------------------------------` |
|        - |  3041 | ` * Simple boring wrapper function.` |
|        - |  3042 | ` * ------------------------------------` |
|        - |  3043 | ` */` |
|       24 |  3044 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3045 |  |
|        - |  3046 | `	sxi32 rc;` |
|       26 |  3047 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3048 | `	return rc;` |
|        2 |  3049 |  |
|        - |  3050 | `/*` |
|        - |  3051 | ` * Resolve function context from the current frame.` |
|        - |  3052 | ` */` |
|      960 |  3053 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3054 |  |
|        - |  3055 | `	VmFrame *pFrame;` |
|        - |  3056 | `	ph7_vm_func *pFunc;` |
|      961 |  3057 | `	*pzFuncName = 0;` |
|      961 |  3058 | `	*pnFuncLen = 0;` |
|      961 |  3059 | `	pFrame = pVm->pFrame;` |
|      961 |  3060 | `	if( pFrame == 0 ){` |
|      ! 0 |  3061 | `		return;` |
|        - |  3062 | `	}` |
|      961 |  3063 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      961 |  3064 | `	if( pFrame->pParent == 0 ){` |
|      947 |  3065 | `		return;` |
|        - |  3066 | `	}` |
|       15 |  3067 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  3068 | `	if( pFunc == 0 ){` |
|      ! 0 |  3069 | `		return;` |
|        - |  3070 | `	}` |
|       15 |  3071 | `	*pzFuncName = pFunc->sName.zString;` |
|       15 |  3072 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      481 |  3073 |  |
|        - |  3074 | `/*` |
|        - |  3075 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3076 | ` */` |
|      488 |  3077 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3078 |  |
|        - |  3079 | `	SyBlob sOut;` |
|        - |  3080 | `	SyString *pFile;` |
|      489 |  3081 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3082 | `		return PH7_OK;` |
|        - |  3083 | `	}` |
|      489 |  3084 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3085 | `		zClass = "Exception";` |
|      ! 0 |  3086 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3087 | `	}` |
|      489 |  3088 | `	if( zMsg == 0 ){` |
|      ! 0 |  3089 | `		zMsg = "Unknown exception";` |
|      ! 0 |  3090 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  3091 | `	}` |
|      489 |  3092 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  3093 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  3094 | `	}` |
|      489 |  3095 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      489 |  3096 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      489 |  3097 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      489 |  3098 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      489 |  3099 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      489 |  3100 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      489 |  3101 | `	if( pFile ){` |
|      489 |  3102 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      489 |  3103 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      489 |  3104 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      244 |  3105 | `	}` |
|      489 |  3106 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      489 |  3107 | `	if( pFile ){` |
|      489 |  3108 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      489 |  3109 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      489 |  3110 | `		if( zFuncName && nFuncLen > 0 ){` |
|       15 |  3111 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        8 |  3112 | `		}else{` |
|      475 |  3113 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3114 | `		}` |
|      244 |  3115 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3116 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3117 | `	}else{` |
|      ! 0 |  3118 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3119 | `	}` |
|      489 |  3120 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      489 |  3121 | `	if( pFile ){` |
|      489 |  3122 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      489 |  3123 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      489 |  3124 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      489 |  3125 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      244 |  3126 | `	}` |
|      489 |  3127 | `	VmCallErrorHandler(pVm,&sOut);` |
|      489 |  3128 | `	SyBlobRelease(&sOut);` |
|      489 |  3129 | `	return PH7_ABORT;` |
|      245 |  3130 |  |
|        - |  3131 | `/*` |
|        - |  3132 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3133 | ` */` |
|      480 |  3134 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3135 |  |
|        - |  3136 | `	ph7_vm *pVm;` |
|        - |  3137 | `	ph7_class *pClass;` |
|        - |  3138 | `	ph7_class_instance *pThis;` |
|        - |  3139 | `	ph7_class_method *pCons;` |
|        - |  3140 | `	ph7_value sArg;` |
|        - |  3141 | `	ph7_value *apArg[1];` |
|        - |  3142 | `	SyBlob sMsg;` |
|        - |  3143 | `	SyString sMsgStr;` |
|        - |  3144 | `	VmFrame *pFrame;` |
|        - |  3145 | `	va_list ap;` |
|        - |  3146 | `	sxi32 rc;` |
|        - |  3147 |  |
|      482 |  3148 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3149 | `		return PH7_ABORT;` |
|        - |  3150 | `	}` |
|      482 |  3151 | `	pVm = pCtx->pVm;` |
|      482 |  3152 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3153 | `		zClass = "Error";` |
|      ! 0 |  3154 | `	}` |
|      482 |  3155 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  3156 | `	if( pClass == 0 ){` |
|      ! 0 |  3157 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3158 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3159 | `			zClass` |
|        - |  3160 | `			);` |
|        - |  3161 | `	}` |
|      482 |  3162 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  3163 | `	if( pThis == 0 ){` |
|      ! 0 |  3164 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3165 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3166 | `			);` |
|        - |  3167 | `	}` |
|        - |  3168 |  |
|      482 |  3169 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  3170 | `	va_start(ap,zFormat);` |
|      482 |  3171 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  3172 | `	va_end(ap);` |
|        - |  3173 |  |
|      482 |  3174 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  3175 | `	if( pCons ){` |
|      482 |  3176 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  3177 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  3178 | `		apArg[0] = &sArg;` |
|      482 |  3179 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  3180 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  3181 | `	}` |
|      482 |  3182 | `	SyBlobRelease(&sMsg);` |
|        - |  3183 |  |
|      482 |  3184 | `	pFrame = pVm->pFrame;` |
|      482 |  3185 | `	if( pFrame ){` |
|      482 |  3186 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  3187 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  3188 | `	}` |
|      482 |  3189 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  3190 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  3191 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3192 | `		return PH7_ABORT;` |
|        - |  3193 | `	}` |
|       12 |  3194 | `	return PH7_EXCEPTION;` |
|      242 |  3195 |  |
|        - |  3196 | `/*` |
|        - |  3197 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3198 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3199 | ` */` |
|      ! 0 |  3200 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3201 |  |
|        - |  3202 | `	ph7_vm *pVm;` |
|        - |  3203 | `	SyBlob sMsg;` |
|      ! 0 |  3204 | `	const char *zFuncName = 0;` |
|      ! 0 |  3205 | `	int nFuncLen = 0;` |
|        - |  3206 | `	va_list ap;` |
|        - |  3207 | `	sxi32 rc;` |
|        - |  3208 |  |
|      ! 0 |  3209 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3210 | `		return PH7_OK;` |
|        - |  3211 | `	}` |
|      ! 0 |  3212 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3213 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3214 | `		zClass = "Error";` |
|      ! 0 |  3215 | `	}` |
|        - |  3216 |  |
|      ! 0 |  3217 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3218 |  |
|      ! 0 |  3219 | `	va_start(ap,zFormat);` |
|      ! 0 |  3220 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3221 | `	va_end(ap);` |
|        - |  3222 |  |
|      ! 0 |  3223 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3224 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3225 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3226 | `	}` |
|      ! 0 |  3227 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3228 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3229 | `	}` |
|      ! 0 |  3230 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3231 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3232 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3233 | `	return rc;` |
|      ! 0 |  3234 |  |
|        - |  3235 | `/*` |
|        - |  3236 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3237 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3238 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3239 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3240 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3241 | ` * when VmByteCodeExec returns.` |
|        - |  3242 | ` */` |
|      144 |  3243 | `static sxi32 VmSuspendCtx(` |
|        - |  3244 | `	ph7_vm *pVm,` |
|        - |  3245 | `	ph7_exec_ctx *pCtx,` |
|        - |  3246 | `	sxi32 pc,` |
|        - |  3247 | `	sxi32 nTos` |
|        - |  3248 | `	)` |
|        2 |  3249 |  |
|       72 |  3250 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3251 | `	pCtx->pc = pc;` |
|      146 |  3252 | `	pCtx->nTos = nTos;` |
|      146 |  3253 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3254 | `	return PH7_SUSPEND;` |
|        2 |  3255 |  |
|        - |  3256 | `/*` |
|        - |  3257 | ` * Resolve named-argument mapping.` |
|        - |  3258 | ` *` |
|        - |  3259 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3260 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3261 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3262 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3263 | ` * every formal parameter that received a value.` |
|        - |  3264 | ` *` |
|        - |  3265 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3266 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3267 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3268 | ` */` |
|       92 |  3269 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3270 | `	ph7_vm *pVm,` |
|        - |  3271 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3272 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3273 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3274 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3275 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3276 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3277 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3278 |  |
|        2 |  3279 |  |
|       94 |  3280 | `	sxi32 posIdx = 0;` |
|        - |  3281 | `	sxu32 i;` |
|        - |  3282 | `	char zErrMsg[256];` |
|       94 |  3283 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      278 |  3284 | `	for( i = 0; i < nActual; i++ ){` |
|      186 |  3285 | `		aSlot[i] = -2;` |
|       94 |  3286 | `	}` |
|      272 |  3287 | `	for( i = 0; i < nActual; i++ ){` |
|      269 |  3288 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3289 | `			/* Named argument — find formal by name */` |
|      174 |  3290 | `			int found = 0;` |
|        - |  3291 | `			sxu32 k;` |
|      288 |  3292 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      274 |  3293 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      265 |  3294 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      252 |  3295 | `						pMap->aNames[i].zString,` |
|      378 |  3296 | `						pMap->aNames[i].nByte) == 0 ){` |
|      162 |  3297 | `					if( aUsed[k] ){` |
|        7 |  3298 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3299 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3300 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3301 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3302 | `						return PH7_ABORT;` |
|        - |  3303 | `					}` |
|      158 |  3304 | `					aSlot[i] = (sxi32)k;` |
|      158 |  3305 | `					aUsed[k] = 1;` |
|      158 |  3306 | `					found = 1;` |
|      158 |  3307 | `					break;` |
|        - |  3308 | `				}` |
|       59 |  3309 | `			}` |
|      170 |  3310 | `			if( !found ){` |
|       14 |  3311 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3312 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3313 | `				}else{` |
|        4 |  3314 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3315 | `						"Unknown named parameter $%.*s",` |
|        2 |  3316 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3317 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3318 | `					return PH7_ABORT;` |
|        - |  3319 | `				}` |
|        5 |  3320 | `			}` |
|       85 |  3321 | `		}else{` |
|        - |  3322 | `			/* Positional argument */` |
|       14 |  3323 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       14 |  3324 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3325 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3326 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3327 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3328 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3329 | `					return PH7_ABORT;` |
|        - |  3330 | `				}` |
|       14 |  3331 | `				aSlot[i] = posIdx;` |
|       14 |  3332 | `				aUsed[posIdx] = 1;` |
|        6 |  3333 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3334 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3335 | `			}` |
|       14 |  3336 | `			posIdx++;` |
|        - |  3337 | `		}` |
|       91 |  3338 | `	}` |
|       87 |  3339 | `	return SXRET_OK;` |
|       48 |  3340 |  |
|        - |  3341 | `/*` |
|        - |  3342 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3343 | ` *` |
|        - |  3344 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3345 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3346 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3347 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3348 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3349 | ` * then the program execution is halted.` |
|        - |  3350 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3351 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3352 | ` * or to reset the VM to it's initial state.` |
|        - |  3353 | ` */` |
|    36672 |  3354 | `static sxi32 VmByteCodeExec(` |
|        - |  3355 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3356 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3357 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3358 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3359 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3360 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3361 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3362 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3363 | `	)` |
|        2 |  3364 |  |
|        - |  3365 | `	VmInstr *pInstr;` |
|        - |  3366 | `	ph7_value *pTos;` |
|        - |  3367 | `	SySet aArg;` |
|        - |  3368 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3369 | `	sxi32 pc;` |
|        - |  3370 | `	sxi32 rc;` |
|        - |  3371 | `	/* Argument container */` |
|    36674 |  3372 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    36674 |  3373 | `	if( nTos < 0 ){` |
|    34436 |  3374 | `		pTos = &pStack[-1];` |
|    17219 |  3375 | `	}else{` |
|     2240 |  3376 | `		pTos = &pStack[nTos];` |
|        - |  3377 | `	}` |
|    36674 |  3378 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    36674 |  3379 | `	pc = nPc;` |
|        - |  3380 | `/*` |
|        - |  3381 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3382 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3383 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3384 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3385 | ` */` |
|        - |  3386 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3387 | `	{ \` |
|        - |  3388 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3389 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3390 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3391 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3392 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3393 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3394 | `				break; \` |
|        - |  3395 | `			} \` |
|        - |  3396 | `			goto Exception; \` |
|        - |  3397 | `		} \` |
|        - |  3398 | `	}` |
|        - |  3399 | `	/* Execute as much as we can */` |
|  5376329 |  3400 | `	for(;;){` |
|        - |  3401 | `		/* Fetch the instruction to execute */` |
| 10751956 |  3402 | `		pInstr = &aInstr[pc];` |
| 10751956 |  3403 | `		rc = SXRET_OK;` |
|        - |  3404 | `/*` |
|        - |  3405 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3406 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3407 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3408 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3409 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3410 | ` */` |
| 10751956 |  3411 | `		switch(pInstr->iOp){` |
|        - |  3412 | `/*` |
|        - |  3413 | ` * DONE: P1 * *` |
|        - |  3414 | ` *` |
|        - |  3415 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3416 | ` * and return immediately.` |
|        - |  3417 | ` */` |
|    18009 |  3418 | `case PH7_OP_DONE:` |
|    36020 |  3419 | `	if( pInstr->iP1 ){` |
|        - |  3420 | `#ifdef UNTRUST` |
|        - |  3421 | `		if( pTos < pStack ){` |
|        - |  3422 | `			goto Abort;` |
|        - |  3423 | `		}` |
|        - |  3424 | `#endif` |
|    21012 |  3425 | `		if( pLastRef ){` |
|    13518 |  3426 | `			*pLastRef = pTos->nIdx;` |
|     6758 |  3427 | `		}` |
|    21012 |  3428 | `		if( pResult ){` |
|        - |  3429 | `			/* Execution result */` |
|    19934 |  3430 | `			PH7_MemObjStore(pTos,pResult);` |
|     9966 |  3431 | `		}` |
|    21012 |  3432 | `		VmPopOperand(&pTos,1);` |
|    25515 |  3433 | `	}else if( pLastRef ){` |
|        - |  3434 | `		/* Nothing referenced */` |
|     1320 |  3435 | `		*pLastRef = SXU32_HIGH;` |
|      659 |  3436 | `	}` |
|        - |  3437 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3438 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3439 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3440 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3441 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3442 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3443 | `	 * block can override it.` |
|        - |  3444 | `	 */` |
|    36022 |  3445 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3446 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3447 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3448 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3449 | `		pExc->pFrame = 0;` |
|        3 |  3450 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3451 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3452 | `			pExc->iFinallyDone = 1;` |
|        - |  3453 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3454 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3455 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3456 | `				goto Abort;` |
|        - |  3457 | `			}` |
|        1 |  3458 | `		}` |
|        1 |  3459 | `	}` |
|    36020 |  3460 | `	goto Done;` |
|        - |  3461 | `/*` |
|        - |  3462 | ` * HALT: P1 * *` |
|        - |  3463 | ` *` |
|        - |  3464 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3465 | ` * and abort immediately.` |
|        - |  3466 | ` */` |
|        4 |  3467 | `case PH7_OP_HALT:` |
|        9 |  3468 | `	if( pInstr->iP1 ){` |
|        - |  3469 | `#ifdef UNTRUST` |
|        - |  3470 | `		if( pTos < pStack ){` |
|        - |  3471 | `			goto Abort;` |
|        - |  3472 | `		}` |
|        - |  3473 | `#endif` |
|        9 |  3474 | `		if( pLastRef ){` |
|      ! 0 |  3475 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3476 | `		}` |
|        9 |  3477 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3478 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3479 | `				/* Output the exit message */` |
|        7 |  3480 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3481 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3482 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3483 | `			}` |
|        7 |  3484 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3485 | `			/* Record exit status */` |
|        5 |  3486 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3487 | `		}` |
|        9 |  3488 | `		VmPopOperand(&pTos,1);` |
|        4 |  3489 | `	}else if( pLastRef ){` |
|        - |  3490 | `		/* Nothing referenced */` |
|      ! 0 |  3491 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3492 | `	}` |
|        - |  3493 | `	/* Check if we're in an included file context */` |
|        9 |  3494 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3495 | `		/* Terminate the entire process */` |
|        9 |  3496 | `		exit(pVm->iExitStatus);` |
|        - |  3497 | `	}` |
|      ! 0 |  3498 | `	goto Abort;` |
|        - |  3499 | `/*` |
|        - |  3500 | ` * JMP: * P2 *` |
|        - |  3501 | ` *` |
|        - |  3502 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3503 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3504 | ` */` |
|   230845 |  3505 | `case PH7_OP_JMP:` |
|   461736 |  3506 | `	pc = pInstr->iP2 - 1;` |
|   461736 |  3507 | `	break;` |
|        - |  3508 | `/*` |
|        - |  3509 | ` * JZ: P1 P2 *` |
|        - |  3510 | ` *` |
|        - |  3511 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3512 | ` * entry in the stack if P1 is zero.` |
|        - |  3513 | ` */` |
|   543844 |  3514 | `case PH7_OP_JZ:` |
|        - |  3515 | `#ifdef UNTRUST` |
|        - |  3516 | `	if( pTos < pStack ){` |
|        - |  3517 | `		goto Abort;` |
|        - |  3518 | `	}` |
|        - |  3519 | `#endif` |
|        - |  3520 | `	/* Get a boolean value */` |
|  1087778 |  3521 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  3522 | `		PH7_MemObjToBool(pTos);` |
|       80 |  3523 | `	}` |
|  1087778 |  3524 | `	if( !pTos->x.iVal ){` |
|        - |  3525 | `		/* Take the jump */` |
|   554836 |  3526 | `		pc = pInstr->iP2 - 1;` |
|   277417 |  3527 | `	}` |
|  1087778 |  3528 | `	if( !pInstr->iP1 ){` |
|   863766 |  3529 | `		VmPopOperand(&pTos,1);` |
|   431904 |  3530 | `	}` |
|  1087778 |  3531 | `	break;` |
|        - |  3532 | `/*` |
|        - |  3533 | ` * JNZ: P1 P2 *` |
|        - |  3534 | ` *` |
|        - |  3535 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3536 | ` * entry in the stack if P1 is zero.` |
|        - |  3537 | ` */` |
|    56988 |  3538 | `case PH7_OP_JNZ:` |
|        - |  3539 | `#ifdef UNTRUST` |
|        - |  3540 | `	if( pTos < pStack ){` |
|        - |  3541 | `		goto Abort;` |
|        - |  3542 | `	}` |
|        - |  3543 | `#endif` |
|        - |  3544 | `	/* Get a boolean value */` |
|   113978 |  3545 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3546 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3547 | `	}` |
|   113978 |  3548 | `	if( pTos->x.iVal ){` |
|        - |  3549 | `		/* Take the jump */` |
|     4968 |  3550 | `		pc = pInstr->iP2 - 1;` |
|     2483 |  3551 | `	}` |
|   113978 |  3552 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3553 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3554 | `	}` |
|   113978 |  3555 | `	break;` |
|        - |  3556 | `/*` |
|        - |  3557 | ` * NOOP: * * *` |
|        - |  3558 | ` *` |
|        - |  3559 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3560 | ` * destination.` |
|        - |  3561 | ` */` |
|      ! 0 |  3562 | `case PH7_OP_NOOP:` |
|      ! 0 |  3563 | `	break;` |
|        - |  3564 | `/*` |
|        - |  3565 | ` * POP: P1 * *` |
|        - |  3566 | ` *` |
|        - |  3567 | ` * Pop P1 elements from the operand stack.` |
|        - |  3568 | ` */` |
|   420899 |  3569 | `case PH7_OP_POP: {` |
|   841844 |  3570 | `	sxi32 n = pInstr->iP1;` |
|   841844 |  3571 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3572 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3573 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3574 | `	}` |
|   841844 |  3575 | `	VmPopOperand(&pTos,n);` |
|   841844 |  3576 | `	break;` |
|        - |  3577 | `				 }` |
|        - |  3578 | `/*` |
|        - |  3579 | ` * DUP: * * *` |
|        - |  3580 | ` *` |
|        - |  3581 | ` * Duplicate the top of the stack.` |
|        - |  3582 | ` */` |
|       41 |  3583 | `case PH7_OP_DUP:` |
|        - |  3584 | `#ifdef UNTRUST` |
|        - |  3585 | `	if( pTos < pStack ){` |
|        - |  3586 | `		goto Abort;` |
|        - |  3587 | `	}` |
|        - |  3588 | `#endif` |
|       84 |  3589 | `	pTos++;` |
|       84 |  3590 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3591 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3592 | `	break;` |
|        - |  3593 | `/*` |
|        - |  3594 | ` * NSSWITCH: * * P3` |
|        - |  3595 | ` *` |
|        - |  3596 | ` * Switch the active namespace at runtime.` |
|        - |  3597 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3598 | ` */` |
|     7050 |  3599 | `case PH7_OP_NSSWITCH:` |
|    14102 |  3600 | `	SyBlobReset(&pVm->sNamespace);` |
|    14102 |  3601 | `	if( pInstr->p3 ){` |
|       96 |  3602 | `		const char *zNs = (const char *)pInstr->p3;` |
|       96 |  3603 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       47 |  3604 | `	}` |
|        - |  3605 | `	/* Clear namespace-scoped use-const imports */` |
|    14102 |  3606 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14102 |  3607 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14102 |  3608 | `	break;` |
|        - |  3609 | `/* OP_USECONST P1 * P3` |
|        - |  3610 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3611 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3612 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3613 | ` */` |
|        7 |  3614 | `case PH7_OP_USECONST: {` |
|       16 |  3615 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3616 | `	if( azPair ){` |
|       16 |  3617 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3618 | `	}` |
|       16 |  3619 | `	break;` |
|        - |  3620 | `				}` |
|        - |  3621 | `/*` |
|        - |  3622 | ` * CVT_INT: * * *` |
|        - |  3623 | ` *` |
|        - |  3624 | ` * Force the top of the stack to be an integer.` |
|        - |  3625 | ` */` |
|       77 |  3626 | `case PH7_OP_CVT_INT:` |
|        - |  3627 | `#ifdef UNTRUST` |
|        - |  3628 | `	if( pTos < pStack ){` |
|        - |  3629 | `		goto Abort;` |
|        - |  3630 | `	}` |
|        - |  3631 | `#endif` |
|      156 |  3632 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3633 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3634 | `	}` |
|        - |  3635 | `	/* Invalidate any prior representation */` |
|      156 |  3636 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3637 | `	break;` |
|        - |  3638 | `/*` |
|        - |  3639 | ` * CVT_REAL: * * *` |
|        - |  3640 | ` *` |
|        - |  3641 | ` * Force the top of the stack to be a real.` |
|        - |  3642 | ` */` |
|        4 |  3643 | `case PH7_OP_CVT_REAL:` |
|        - |  3644 | `#ifdef UNTRUST` |
|        - |  3645 | `	if( pTos < pStack ){` |
|        - |  3646 | `		goto Abort;` |
|        - |  3647 | `	}` |
|        - |  3648 | `#endif` |
|        9 |  3649 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3650 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3651 | `	}` |
|        - |  3652 | `	/* Invalidate any prior representation */` |
|        9 |  3653 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3654 | `	break;` |
|        - |  3655 | `/*` |
|        - |  3656 | ` * CVT_STR: * * *` |
|        - |  3657 | ` *` |
|        - |  3658 | ` * Force the top of the stack to be a string.` |
|        - |  3659 | ` */` |
|      146 |  3660 | `case PH7_OP_CVT_STR:` |
|        - |  3661 | `#ifdef UNTRUST` |
|        - |  3662 | `	if( pTos < pStack ){` |
|        - |  3663 | `		goto Abort;` |
|        - |  3664 | `	}` |
|        - |  3665 | `#endif` |
|      294 |  3666 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3667 | `		PH7_MemObjToString(pTos);` |
|      146 |  3668 | `	}` |
|      294 |  3669 | `	break;` |
|        - |  3670 | `/*` |
|        - |  3671 | ` * CVT_BOOL: * * *` |
|        - |  3672 | ` *` |
|        - |  3673 | ` * Force the top of the stack to be a boolean.` |
|        - |  3674 | ` */` |
|        5 |  3675 | `case PH7_OP_CVT_BOOL:` |
|        - |  3676 | `#ifdef UNTRUST` |
|        - |  3677 | `	if( pTos < pStack ){` |
|        - |  3678 | `		goto Abort;` |
|        - |  3679 | `	}` |
|        - |  3680 | `#endif` |
|       11 |  3681 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3682 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3683 | `	}` |
|       11 |  3684 | `	break;` |
|        - |  3685 | `/*` |
|        - |  3686 | ` * CVT_NULL: * * *` |
|        - |  3687 | ` *` |
|        - |  3688 | ` * Nullify the top of the stack.` |
|        - |  3689 | ` */` |
|        3 |  3690 | `case PH7_OP_CVT_NULL:` |
|        - |  3691 | `#ifdef UNTRUST` |
|        - |  3692 | `	if( pTos < pStack ){` |
|        - |  3693 | `		goto Abort;` |
|        - |  3694 | `	}` |
|        - |  3695 | `#endif` |
|        7 |  3696 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3697 | `	break;` |
|        - |  3698 | `/*` |
|        - |  3699 | ` * CVT_NUMC: * * *` |
|        - |  3700 | ` *` |
|        - |  3701 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3702 | ` */` |
|      ! 0 |  3703 | `case PH7_OP_CVT_NUMC:` |
|        - |  3704 | `#ifdef UNTRUST` |
|        - |  3705 | `	if( pTos < pStack ){` |
|        - |  3706 | `		goto Abort;` |
|        - |  3707 | `	}` |
|        - |  3708 | `#endif` |
|        - |  3709 | `	/* Force a numeric cast */` |
|      ! 0 |  3710 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3711 | `	break;` |
|        - |  3712 | `/*` |
|        - |  3713 | ` * CVT_ARRAY: * * *` |
|        - |  3714 | ` *` |
|        - |  3715 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3716 | ` */` |
|       10 |  3717 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3718 | `#ifdef UNTRUST` |
|        - |  3719 | `	if( pTos < pStack ){` |
|        - |  3720 | `		goto Abort;` |
|        - |  3721 | `	}` |
|        - |  3722 | `#endif` |
|        - |  3723 | `	/* Force a hashmap cast */` |
|       21 |  3724 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3725 | `	if( rc != SXRET_OK ){` |
|        - |  3726 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3727 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3728 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3729 | `	}` |
|       21 |  3730 | `	break;` |
|        - |  3731 | `/*` |
|        - |  3732 | ` * CVT_OBJ: * * *` |
|        - |  3733 | ` *` |
|        - |  3734 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3735 | ` */` |
|        8 |  3736 | `case PH7_OP_CVT_OBJ:` |
|        - |  3737 | `#ifdef UNTRUST` |
|        - |  3738 | `	if( pTos < pStack ){` |
|        - |  3739 | `		goto Abort;` |
|        - |  3740 | `	}` |
|        - |  3741 | `#endif` |
|       17 |  3742 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3743 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3744 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3745 | `	}` |
|       17 |  3746 | `	break;` |
|        - |  3747 | `/*` |
|        - |  3748 | ` * ERR_CTRL * * *` |
|        - |  3749 | ` *` |
|        - |  3750 | ` * Error control operator.` |
|        - |  3751 | ` */` |
|    14264 |  3752 | `case PH7_OP_ERR_CTRL:` |
|        - |  3753 | `	/*` |
|        - |  3754 | `	 * TICKET 1433-038:` |
|        - |  3755 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3756 | `	 * use the public API,to control error output.` |
|        - |  3757 | `	 */` |
|    28528 |  3758 | `	break;` |
|        - |  3759 | `/*` |
|        - |  3760 | ` * IS_A * * *` |
|        - |  3761 | ` *` |
|        - |  3762 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3763 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3764 | ` * holding a class name or an object).` |
|        - |  3765 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3766 | ` */` |
|       23 |  3767 | `case PH7_OP_IS_A:{` |
|       48 |  3768 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3769 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3770 | `#ifdef UNTRUST` |
|        - |  3771 | `	if( pNos < pStack ){` |
|        - |  3772 | `		goto Abort;` |
|        - |  3773 | `	}` |
|        - |  3774 | `#endif` |
|       48 |  3775 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3776 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3777 | `		ph7_class *pClass = 0;` |
|        - |  3778 | `		/* Extract the target class */` |
|       46 |  3779 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3780 | `			/* Instance already loaded */` |
|      ! 0 |  3781 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3782 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3783 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3784 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3785 | `			/* Handle self/static/parent keywords */` |
|       46 |  3786 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3787 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3788 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3789 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3790 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3791 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3792 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3793 | `					pClass = pSelf->pBase;` |
|        2 |  3794 | `				}` |
|        3 |  3795 | `			}else{` |
|       36 |  3796 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3797 | `			}` |
|       22 |  3798 | `		}` |
|       46 |  3799 | `		if( pClass ){` |
|        - |  3800 | `			/* Perform the query */` |
|       46 |  3801 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3802 | `		}` |
|       22 |  3803 | `	}` |
|        - |  3804 | `	/* Push result */` |
|       48 |  3805 | `	VmPopOperand(&pTos,1);` |
|       48 |  3806 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3807 | `	pTos->x.iVal = iRes;` |
|       48 |  3808 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3809 | `	break;` |
|        - |  3810 | `				 }` |
|        - |  3811 |  |
|        - |  3812 | `/*` |
|        - |  3813 | ` * LOADC P1 P2 *` |
|        - |  3814 | ` *` |
|        - |  3815 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3816 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3817 | ` */` |
|   910862 |  3818 | `case PH7_OP_LOADC: {` |
|        - |  3819 | `	ph7_value *pObj;` |
|        - |  3820 | `	/* Reserve a room */` |
|  1821770 |  3821 | `	pTos++;` |
|  2723837 |  3822 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1821770 |  3823 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3824 | `			SyHashEntry *pEntry;` |
|        - |  3825 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3826 | `			{` |
|        - |  3827 | `				SyHashEntry *pConstImport;` |
|    26522 |  3828 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    17680 |  3829 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17682 |  3830 | `				if( pConstImport ){` |
|       11 |  3831 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3832 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3833 | `					if( pEntry ){` |
|       11 |  3834 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3835 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3836 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3837 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3838 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3839 | `						break;` |
|        - |  3840 | `					}` |
|        - |  3841 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3842 | `				}` |
|        - |  3843 | `			}` |
|        - |  3844 | `			/* Candidate for expansion via user defined callbacks */` |
|    17672 |  3845 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17672 |  3846 | `			if( pEntry ){` |
|    17668 |  3847 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3848 | `				/* Set a NULL default value */` |
|    17668 |  3849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    17668 |  3850 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3851 | `				/* Invoke the callback and deal with the expanded value */` |
|    17668 |  3852 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3853 | `				/* Mark as constant */` |
|    17668 |  3854 | `				pTos->nIdx = SXU32_HIGH;` |
|    17668 |  3855 | `				break;` |
|        - |  3856 | `			}` |
|        - |  3857 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3858 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3859 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3860 | `			{` |
|        6 |  3861 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3862 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3863 | `				sxu32 j;` |
|        6 |  3864 | `				int isQualified = 0;` |
|       32 |  3865 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3866 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3867 | `				}` |
|        6 |  3868 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3869 | `					/* Try current_namespace\name */` |
|      ! 0 |  3870 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3871 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3872 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3873 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3874 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3875 | `					if( pEntry ){` |
|      ! 0 |  3876 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3877 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3878 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3879 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3880 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3881 | `						break;` |
|        - |  3882 | `					}` |
|        - |  3883 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3884 | `				}` |
|        6 |  3885 | `				if( isQualified ){` |
|        - |  3886 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3887 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3888 | `					SyBlob sErr;` |
|        3 |  3889 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3890 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3891 | `					if( pErrFile ){` |
|        3 |  3892 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3893 | `					}` |
|        3 |  3894 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3895 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3896 | `					SyBlobRelease(&sErr);` |
|        3 |  3897 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3898 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3899 | `					goto LoadC_Done;` |
|        - |  3900 | `				}` |
|        - |  3901 | `			}` |
|        1 |  3902 | `		}` |
|  1804092 |  3903 | `		PH7_MemObjLoad(pObj,pTos);` |
|   902069 |  3904 | `	}else{` |
|        - |  3905 | `		/* Set a NULL value */` |
|      ! 0 |  3906 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3907 | `	}` |
|   902024 |  3908 | `LoadC_Done:` |
|        - |  3909 | `	/* Mark as constant */` |
|  1804094 |  3910 | `	pTos->nIdx = SXU32_HIGH;` |
|  1804094 |  3911 | `	break;` |
|        - |  3912 | `				  }` |
|        - |  3913 | `/*` |
|        - |  3914 | ` * LOAD: P1 * P3` |
|        - |  3915 | ` *` |
|        - |  3916 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3917 | ` * from the P3 operand.` |
|        - |  3918 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3919 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3920 | ` */` |
|  1448510 |  3921 | `case PH7_OP_LOAD:{` |
|        - |  3922 | `	ph7_value *pObj;` |
|        - |  3923 | `	SyString sName;` |
|  2897242 |  3924 | `	if( pInstr->p3 == 0 ){` |
|        - |  3925 | `		/* Take the variable name from the top of the stack */` |
|        - |  3926 | `#ifdef UNTRUST` |
|        - |  3927 | `		if( pTos < pStack ){` |
|        - |  3928 | `			goto Abort;` |
|        - |  3929 | `		}` |
|        - |  3930 | `#endif` |
|        - |  3931 | `		/* Force a string cast */` |
|       19 |  3932 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3933 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3934 | `		}` |
|       19 |  3935 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3936 | `	}else{` |
|  2897224 |  3937 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3938 | `		/* Reserve a room for the target object */` |
|  2897224 |  3939 | `		pTos++;` |
|        - |  3940 | `	}` |
|        - |  3941 | `	/* Extract the requested memory object */` |
|  2897242 |  3942 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2897242 |  3943 | `	if( pObj == 0 ){` |
|       28 |  3944 | `		if( pInstr->iP1 ){` |
|        - |  3945 | `			/* Variable not found,load NULL */` |
|       28 |  3946 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3947 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3948 | `			}else{` |
|       28 |  3949 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3950 | `			}` |
|       28 |  3951 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1448525 |  3952 | `			break;` |
|      ! 0 |  3953 | `		}else{` |
|        - |  3954 | `			/* Fatal error */` |
|      ! 0 |  3955 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3956 | `			goto Abort;` |
|        - |  3957 | `		}` |
|        - |  3958 | `	}` |
|        - |  3959 | `	/* Load variable contents */` |
|  2897216 |  3960 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2897216 |  3961 | `	pTos->nIdx = pObj->nIdx;` |
|  2897216 |  3962 | `	break;` |
|        - |  3963 | `				   }` |
|        - |  3964 | `/*` |
|        - |  3965 | ` * LOAD_MAP P1 * *` |
|        - |  3966 | ` *` |
|        - |  3967 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3968 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3969 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3970 | ` */` |
|    20322 |  3971 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3972 | `	ph7_hashmap *pMap;` |
|        - |  3973 | `	/* Allocate a new hashmap instance */` |
|    40646 |  3974 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    40646 |  3975 | `	if( pMap == 0 ){` |
|      ! 0 |  3976 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3977 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3978 | `		goto Abort;` |
|        - |  3979 | `	}` |
|    40646 |  3980 | `	if( pInstr->iP1 > 0 ){` |
|     2362 |  3981 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3982 | `		/* Perform the insertion */` |
|     7230 |  3983 | `		while( pEntry < pTos ){` |
|     4870 |  3984 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3985 | `				/* Insertion by reference */` |
|      142 |  3986 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3987 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3988 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3989 | `					);` |
|       48 |  3990 | `			}else{` |
|        - |  3991 | `				/* Standard insertion */` |
|     7163 |  3992 | `				PH7_HashmapInsert(pMap,` |
|     4774 |  3993 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2387 |  3994 | `					&pEntry[1]` |
|        - |  3995 | `				);` |
|        - |  3996 | `			}` |
|        - |  3997 | `			/* Next pair on the stack */` |
|     4870 |  3998 | `			pEntry += 2;` |
|        2 |  3999 | `		}` |
|        - |  4000 | `		/* Pop P1 elements */` |
|     2362 |  4001 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1180 |  4002 | `	}` |
|        - |  4003 | `	/* Push the hashmap */` |
|    40646 |  4004 | `	pTos++;` |
|    40646 |  4005 | `	pTos->nIdx = SXU32_HIGH;` |
|    40646 |  4006 | `	pTos->x.pOther = pMap;` |
|    40646 |  4007 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    40646 |  4008 | `	break;` |
|        - |  4009 | `					  }` |
|        - |  4010 | `/*` |
|        - |  4011 | ` * LOAD_LIST: P1 * *` |
|        - |  4012 | ` *` |
|        - |  4013 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4014 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4015 | ` * Caveats:` |
|        - |  4016 | ` *  This implementation support only a single nesting level.` |
|        - |  4017 | ` */` |
|       48 |  4018 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4019 | `	ph7_value *pEntry;` |
|       98 |  4020 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4021 | `		/* Empty list,break immediately */` |
|      ! 0 |  4022 | `		break;` |
|        - |  4023 | `	}` |
|       98 |  4024 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4025 | `#ifdef UNTRUST` |
|        - |  4026 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4027 | `		goto Abort;` |
|        - |  4028 | `	}` |
|        - |  4029 | `#endif` |
|       98 |  4030 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4031 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4032 | `		ph7_hashmap_node *pNode;` |
|        - |  4033 | `		ph7_value sKey,*pObj;` |
|        - |  4034 | `		/* Start Copying */` |
|       91 |  4035 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4036 | `		while( pEntry <= pTos ){` |
|      193 |  4037 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4038 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4039 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4040 | `					if( rc == SXRET_OK ){` |
|        - |  4041 | `						/* Store node value */` |
|      165 |  4042 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4043 | `					}else{` |
|        - |  4044 | `						/* Undefined array key */` |
|        - |  4045 | `						char zMsg[128];` |
|      ! 0 |  4046 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4047 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4048 | `						PH7_MemObjRelease(pObj);` |
|        - |  4049 | `					}` |
|       82 |  4050 | `				}` |
|       82 |  4051 | `			}` |
|      193 |  4052 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4053 | `			pEntry++;` |
|        1 |  4054 | `		}` |
|       46 |  4055 | `	}else{` |
|        - |  4056 | `		/* Source is not an array */` |
|        - |  4057 | `		ph7_value *pObj;` |
|       18 |  4058 | `		while( pEntry <= pTos ){` |
|       12 |  4059 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4060 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4061 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4062 | `				}` |
|        5 |  4063 | `			}` |
|       12 |  4064 | `			pEntry++;` |
|        2 |  4065 | `		}` |
|        8 |  4066 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4067 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4068 | `			const char *zType = "unknown";` |
|        3 |  4069 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4070 | `			char zMsg[256];` |
|        3 |  4071 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4072 | `				zType = "string";` |
|        1 |  4073 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4074 | `				zType = "int";` |
|      ! 0 |  4075 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4076 | `				zType = "float";` |
|      ! 0 |  4077 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4078 | `				zType = "object";` |
|      ! 0 |  4079 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4080 | `				zType = "resource";` |
|      ! 0 |  4081 | `			}` |
|        3 |  4082 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4083 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4084 | `		}` |
|        - |  4085 | `	}` |
|       98 |  4086 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4087 | `	break;` |
|        - |  4088 | `					   }` |
|        - |  4089 | `/*` |
|        - |  4090 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4091 | ` *` |
|        - |  4092 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4093 | ` * from the stack.` |
|        - |  4094 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4095 | ` * instead.` |
|        - |  4096 | ` */` |
|   232800 |  4097 | `case PH7_OP_LOAD_IDX: {` |
|   465646 |  4098 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   465646 |  4099 | `	ph7_hashmap *pMap = 0;` |
|        - |  4100 | `	ph7_value *pIdx;` |
|   465646 |  4101 | `	pIdx = 0;` |
|   465646 |  4102 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4103 | `		if( !pInstr->iP2){` |
|        - |  4104 | `			/* No available index,load NULL */` |
|      ! 0 |  4105 | `			if( pTos >= pStack ){` |
|      ! 0 |  4106 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4107 | `			}else{` |
|        - |  4108 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4109 | `				pTos++;` |
|      ! 0 |  4110 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4111 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4112 | `			}` |
|        - |  4113 | `			/* Emit a notice */` |
|      ! 0 |  4114 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4115 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4116 | `			break;` |
|        - |  4117 | `		}` |
|      ! 0 |  4118 | `	}else{` |
|   465646 |  4119 | `		pIdx = pTos;` |
|   465646 |  4120 | `		pTos--;` |
|        - |  4121 | `	}` |
|   465646 |  4122 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4123 | `		/* String access */` |
|   364222 |  4124 | `		if( pIdx ){` |
|        - |  4125 | `			sxu32 nOfft;` |
|   364222 |  4126 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4127 | `				/* Force an int cast */` |
|      ! 0 |  4128 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4129 | `			}` |
|   364222 |  4130 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   364222 |  4131 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4132 | `				/* Invalid offset,load null */` |
|      ! 0 |  4133 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4134 | `			}else{` |
|   364222 |  4135 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   364222 |  4136 | `				int c = zData[nOfft];` |
|   364222 |  4137 | `				PH7_MemObjRelease(pTos);` |
|   364222 |  4138 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   364222 |  4139 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4140 | `			}` |
|   182134 |  4141 | `		}else{` |
|        - |  4142 | `			/* No available index,load NULL */` |
|      ! 0 |  4143 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4144 | `		}` |
|   364222 |  4145 | `		break;` |
|        - |  4146 | `	}` |
|   101426 |  4147 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4148 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4149 | `			ph7_value *pObj;` |
|        3 |  4150 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4151 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4152 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4153 | `			}` |
|        1 |  4154 | `		}` |
|        1 |  4155 | `	}` |
|   101426 |  4156 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   101426 |  4157 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   101426 |  4158 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4159 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4160 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4161 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4162 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4163 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4164 | `		}` |
|        - |  4165 | `		/* Point to the hashmap */` |
|   101426 |  4166 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   101426 |  4167 | `		if( pIdx ){` |
|        - |  4168 | `			/* Load the desired entry */` |
|   101426 |  4169 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    50712 |  4170 | `		}` |
|   101426 |  4171 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4172 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4173 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4174 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4175 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4176 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4177 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4178 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4179 | `			 * correct for the outermost write. */` |
|       19 |  4180 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4181 | `			if( !needWrite && pNode ){` |
|       13 |  4182 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4183 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4184 | `					needWrite = 1;` |
|        3 |  4185 | `				}` |
|        6 |  4186 | `			}` |
|       19 |  4187 | `			if( needWrite ){` |
|       13 |  4188 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4189 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4190 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4191 | `					 * into the new map's storage. */` |
|        7 |  4192 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4193 | `					if( pIdx ){` |
|        7 |  4194 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4195 | `					}` |
|        3 |  4196 | `				}` |
|        6 |  4197 | `			}` |
|        9 |  4198 | `		}` |
|   101426 |  4199 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4200 | `			/* Create a new empty entry */` |
|      273 |  4201 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4202 | `			if( rc == SXRET_OK ){` |
|        - |  4203 | `				/* Point to the last inserted entry */` |
|      273 |  4204 | `				pNode = pMap->pLast;` |
|      136 |  4205 | `			}` |
|      136 |  4206 | `		}` |
|    50712 |  4207 | `	}` |
|   101426 |  4208 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4209 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4210 | `		char zMsg[128];` |
|      ! 0 |  4211 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4212 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4213 | `		}` |
|      ! 0 |  4214 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4215 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4216 | `	}` |
|   101426 |  4217 | `	if( pIdx ){` |
|   101426 |  4218 | `		PH7_MemObjRelease(pIdx);` |
|    50712 |  4219 | `	}` |
|   101426 |  4220 | `	if( rc == SXRET_OK ){` |
|        - |  4221 | `		/* Load entry contents */` |
|    45650 |  4222 | `		if( pMap->iRef < 2 ){` |
|        - |  4223 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4224 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4225 | `			 */` |
|       24 |  4226 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4227 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4228 | `		}else{` |
|    45628 |  4229 | `			pTos->nIdx = pNode->nValIdx;` |
|    45628 |  4230 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    45628 |  4231 | `			PH7_HashmapUnref(pMap);` |
|        - |  4232 | `		}` |
|    22826 |  4233 | `	}else{` |
|        - |  4234 | `		/* No such entry,load NULL */` |
|    55778 |  4235 | `		PH7_MemObjRelease(pTos);` |
|    55778 |  4236 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4237 | `	}` |
|   101426 |  4238 | `	break;` |
|        - |  4239 | `					  }` |
|        - |  4240 | `/*` |
|        - |  4241 | ` * LOAD_CLOSURE * * P3` |
|        - |  4242 | ` *` |
|        - |  4243 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4244 | ` * name in the stack.` |
|        - |  4245 | ` */` |
|       44 |  4246 | `case PH7_OP_LOAD_CLOSURE:{` |
|       89 |  4247 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       89 |  4248 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4249 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4250 | `		ph7_vm_func *pClosure;` |
|        - |  4251 | `		char *zName;` |
|        - |  4252 | `		sxu32 mLen;` |
|        - |  4253 | `		sxu32 n;` |
|        - |  4254 | `		/* Create a new VM function */` |
|       89 |  4255 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4256 | `		/* Generate an unique closure name */` |
|       89 |  4257 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       89 |  4258 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4259 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4260 | `			goto Abort;` |
|        - |  4261 | `		}` |
|       89 |  4262 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       89 |  4263 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4264 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4265 | `		}` |
|        - |  4266 | `		/* Zero the stucture */` |
|       89 |  4267 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4268 | `		/* Perform a structure assignment on read-only items */` |
|       89 |  4269 | `		pClosure->aArgs = pFunc->aArgs;` |
|       89 |  4270 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       89 |  4271 | `		pClosure->aStatic = pFunc->aStatic;` |
|       89 |  4272 | `		pClosure->iFlags = pFunc->iFlags;` |
|       89 |  4273 | `		pClosure->pUserData = pFunc->pUserData;` |
|       89 |  4274 | `		pClosure->sSignature = pFunc->sSignature;` |
|       89 |  4275 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       89 |  4276 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       89 |  4277 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       89 |  4278 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       89 |  4279 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4280 | `		/* Register the closure */` |
|       89 |  4281 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4282 | `		/* Set up closure environment */` |
|       89 |  4283 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       89 |  4284 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      241 |  4285 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4286 | `			ph7_value *pValue;` |
|      153 |  4287 | `			pEnv = &aEnv[n];` |
|      153 |  4288 | `			sEnv.sName  = pEnv->sName;` |
|      153 |  4289 | `			sEnv.iFlags = pEnv->iFlags;` |
|      153 |  4290 | `			sEnv.nIdx = SXU32_HIGH;` |
|      153 |  4291 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      153 |  4292 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4293 | `				/* Pass by reference */` |
|      ! 0 |  4294 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4295 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4296 | `					);` |
|      ! 0 |  4297 | `			}` |
|        - |  4298 | `			/* Standard pass by value */` |
|      153 |  4299 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      153 |  4300 | `			if( pValue ){` |
|        - |  4301 | `				/* Copy imported value */` |
|       69 |  4302 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4303 | `			}` |
|        - |  4304 | `			/* Insert the imported variable */` |
|      153 |  4305 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       77 |  4306 | `		}` |
|        - |  4307 | `		/* Finally,load the closure name on the stack */` |
|       89 |  4308 | `		pTos++;` |
|       89 |  4309 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       44 |  4310 | `	}` |
|       89 |  4311 | `	break;` |
|        - |  4312 | `						 }` |
|        - |  4313 | `/*` |
|        - |  4314 | ` * STORE * P2 P3` |
|        - |  4315 | ` *` |
|        - |  4316 | ` * Perform a store (Assignment) operation.` |
|        - |  4317 | ` */` |
|   125935 |  4318 | `case PH7_OP_STORE: {` |
|        - |  4319 | `	ph7_value *pObj;` |
|        - |  4320 | `	SyString sName;` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pTos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|   251872 |  4326 | `	if( pInstr->iP2 ){` |
|        - |  4327 | `		sxu32 nIdx;` |
|        - |  4328 | `		sxi32 rcT;` |
|        - |  4329 | `		/* Member store operation */` |
|     3638 |  4330 | `		nIdx = pTos->nIdx;` |
|     3638 |  4331 | `		VmPopOperand(&pTos,1);` |
|     3638 |  4332 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4333 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4334 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4335 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4336 | `		}else{` |
|        - |  4337 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4338 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     3634 |  4339 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     3634 |  4340 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4341 | `				goto Abort;` |
|        - |  4342 | `			}` |
|     3634 |  4343 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4344 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4345 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4346 | `				 * propagate out of the VM loop. */` |
|       35 |  4347 | `				VmPopOperand(&pTos,1);` |
|        - |  4348 | `				{` |
|       35 |  4349 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       35 |  4350 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       35 |  4351 | `						pc = pFrm2->iExceptionJump - 1;` |
|   125953 |  4352 | `						break;` |
|        - |  4353 | `					}` |
|        - |  4354 | `				}` |
|      ! 0 |  4355 | `				goto Exception;` |
|        - |  4356 | `			}` |
|        - |  4357 | `			/* Point to the desired memory object */` |
|     3600 |  4358 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3600 |  4359 | `			if( pObj ){` |
|        - |  4360 | `				/* Perform the store operation */` |
|     3600 |  4361 | `				PH7_MemObjStore(pTos,pObj);` |
|     1799 |  4362 | `			}` |
|        - |  4363 | `		}` |
|     3604 |  4364 | `		break;` |
|   248236 |  4365 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4366 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4367 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4368 | `			/* Force a string cast */` |
|      ! 0 |  4369 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4370 | `		}` |
|        7 |  4371 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4372 | `		pTos--;` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `		if( pTos < pStack  ){` |
|        - |  4375 | `			goto Abort;` |
|        - |  4376 | `		}` |
|        - |  4377 | `#endif` |
|        4 |  4378 | `	}else{` |
|   248230 |  4379 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4380 | `	}` |
|        - |  4381 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   248236 |  4382 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   248236 |  4383 | `	if( pObj == 0 ){` |
|      ! 0 |  4384 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4385 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4386 | `		goto Abort;` |
|        - |  4387 | `	}` |
|   248236 |  4388 | `	if( !pInstr->p3 ){` |
|        7 |  4389 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4390 | `	}` |
|        - |  4391 | `	/* Perform the store operation */` |
|   248236 |  4392 | `	PH7_MemObjStore(pTos,pObj);` |
|   248236 |  4393 | `	break;` |
|        - |  4394 | `				   }` |
|        - |  4395 | `/*` |
|        - |  4396 | ` * STORE_IDX:   P1 * P3` |
|        - |  4397 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4398 | ` *` |
|        - |  4399 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4400 | ` */` |
|    88853 |  4401 | `case PH7_OP_STORE_IDX:` |
|        - |  4402 | `case PH7_OP_STORE_IDX_REF: {` |
|   177708 |  4403 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4404 | `	ph7_value *pKey;` |
|        - |  4405 | `	sxu32 nIdx;` |
|   177708 |  4406 | `	if( pInstr->iP1 ){` |
|        - |  4407 | `		/* Key is next on stack */` |
|    60114 |  4408 | `		pKey = pTos;` |
|    60114 |  4409 | `		pTos--;` |
|    30058 |  4410 | `	}else{` |
|   117596 |  4411 | `		pKey = 0;` |
|        - |  4412 | `	}` |
|   177708 |  4413 | `	nIdx = pTos->nIdx;` |
|   177708 |  4414 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4415 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4416 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4417 | `		 * checking true sharing count, then re-add after separation. */` |
|   177656 |  4418 | `		if( nIdx != SXU32_HIGH ){` |
|   177656 |  4419 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   266483 |  4420 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   177656 |  4421 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4422 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4423 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4424 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4425 | `				 * refcounts if the backing array was already separated. */` |
|   177656 |  4426 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   177656 |  4427 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   177656 |  4428 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   177656 |  4429 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   177656 |  4430 | `					pTos->x.pOther = pMap;` |
|    88829 |  4431 | `				}else{` |
|        - |  4432 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4433 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4434 | `					pMap = pCur;` |
|        - |  4435 | `				}` |
|    88829 |  4436 | `			}else{` |
|      ! 0 |  4437 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4438 | `			}` |
|    88829 |  4439 | `		}else{` |
|      ! 0 |  4440 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4441 | `		}` |
|   177656 |  4442 | `		if( pMap->iRef < 2 ){` |
|        - |  4443 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4444 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4445 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4446 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4447 | `			pMap->iRef = 2;` |
|      ! 0 |  4448 | `		}` |
|    88829 |  4449 | `	}else{` |
|        - |  4450 | `		ph7_value *pObj;` |
|       53 |  4451 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4452 | `		if( pObj == 0 ){` |
|      ! 0 |  4453 | `			if( pKey ){` |
|      ! 0 |  4454 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4455 | `			}` |
|      ! 0 |  4456 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4457 | `			break;` |
|        - |  4458 | `		}` |
|        - |  4459 | `		/* Phase#1: Load the array */` |
|       53 |  4460 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4461 | `			VmPopOperand(&pTos,1);` |
|       53 |  4462 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4463 | `				/* Force a string cast */` |
|      ! 0 |  4464 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4465 | `			}` |
|       53 |  4466 | `			if( pKey == 0 ){` |
|        - |  4467 | `				/* Append string */` |
|        3 |  4468 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4469 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4470 | `				}` |
|        2 |  4471 | `			}else{` |
|        - |  4472 | `				sxu32 nOfft;` |
|       51 |  4473 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4474 | `					/* Force an int cast */` |
|       51 |  4475 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4476 | `				}` |
|       51 |  4477 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4478 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4479 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4480 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4481 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4482 | `				}else{` |
|      ! 0 |  4483 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4484 | `						/* Perform an append operation */` |
|      ! 0 |  4485 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4486 | `					}` |
|        - |  4487 | `				}` |
|        - |  4488 | `			}` |
|       53 |  4489 | `			if( pKey ){` |
|       51 |  4490 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4491 | `			}` |
|       53 |  4492 | `			break;` |
|      ! 0 |  4493 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4494 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4495 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4496 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4497 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4498 | `				goto Abort;` |
|        - |  4499 | `			}` |
|      ! 0 |  4500 | `		}` |
|        - |  4501 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4502 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4503 | `	}` |
|   177656 |  4504 | `	VmPopOperand(&pTos,1);` |
|        - |  4505 | `	/* Phase#2: Perform the insertion */` |
|   177656 |  4506 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4507 | `		/* Insertion by reference */` |
|       15 |  4508 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4509 | `	}else{` |
|   177642 |  4510 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4511 | `	}` |
|   177656 |  4512 | `	if( pKey ){` |
|    60064 |  4513 | `		PH7_MemObjRelease(pKey);` |
|    30031 |  4514 | `	}` |
|   177656 |  4515 | `	break;` |
|        - |  4516 | `					   }` |
|        - |  4517 | `/*` |
|        - |  4518 | ` * INCR: P1 * *` |
|        - |  4519 | ` *` |
|        - |  4520 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4521 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4522 | ` * the stack and increment after that.` |
|        - |  4523 | ` */` |
|   159216 |  4524 | `case PH7_OP_INCR:` |
|        - |  4525 | `#ifdef UNTRUST` |
|        - |  4526 | `	if( pTos < pStack ){` |
|        - |  4527 | `		goto Abort;` |
|        - |  4528 | `	}` |
|        - |  4529 | `#endif` |
|   318478 |  4530 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   318478 |  4531 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4532 | `			ph7_value *pObj;` |
|   318478 |  4533 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4534 | `				/* Force a numeric cast */` |
|   318478 |  4535 | `				PH7_MemObjToNumeric(pObj);` |
|   318478 |  4536 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4537 | `					pObj->rVal++;` |
|        - |  4538 | `					/* Try to get an integer representation */` |
|      ! 0 |  4539 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4540 | `				}else{` |
|   318478 |  4541 | `					pObj->x.iVal++;` |
|   318478 |  4542 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4543 | `				}` |
|   318478 |  4544 | `				if( pInstr->iP1 ){` |
|        - |  4545 | `					/* Pre-icrement */` |
|       77 |  4546 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4547 | `				}` |
|   159260 |  4548 | `			}` |
|   159262 |  4549 | `		}else{` |
|      ! 0 |  4550 | `			if( pInstr->iP1 ){` |
|        - |  4551 | `				/* Force a numeric cast */` |
|      ! 0 |  4552 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4553 | `				/* Pre-increment */` |
|      ! 0 |  4554 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4555 | `					pTos->rVal++;` |
|        - |  4556 | `					/* Try to get an integer representation */` |
|      ! 0 |  4557 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4558 | `				}else{` |
|      ! 0 |  4559 | `					pTos->x.iVal++;` |
|      ! 0 |  4560 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4561 | `				}` |
|      ! 0 |  4562 | `			}` |
|        - |  4563 | `		}` |
|   159260 |  4564 | `	}` |
|   318478 |  4565 | `	break;` |
|        - |  4566 | `/*` |
|        - |  4567 | ` * DECR: P1 * *` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4570 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4571 | ` * and decrement after that.` |
|        - |  4572 | ` */` |
|        2 |  4573 | `case PH7_OP_DECR:` |
|        - |  4574 | `#ifdef UNTRUST` |
|        - |  4575 | `	if( pTos < pStack ){` |
|        - |  4576 | `		goto Abort;` |
|        - |  4577 | `	}` |
|        - |  4578 | `#endif` |
|        5 |  4579 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4580 | `		/* Force a numeric cast */` |
|        5 |  4581 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4582 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4583 | `			ph7_value *pObj;` |
|        5 |  4584 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4585 | `				/* Force a numeric cast */` |
|        5 |  4586 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4587 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4588 | `					pObj->rVal--;` |
|        - |  4589 | `					/* Try to get an integer representation */` |
|      ! 0 |  4590 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4591 | `				}else{` |
|        5 |  4592 | `					pObj->x.iVal--;` |
|        5 |  4593 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4594 | `				}` |
|        5 |  4595 | `				if( pInstr->iP1 ){` |
|        - |  4596 | `					/* Pre-icrement */` |
|      ! 0 |  4597 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4598 | `				}` |
|        2 |  4599 | `			}` |
|        3 |  4600 | `		}else{` |
|      ! 0 |  4601 | `			if( pInstr->iP1 ){` |
|        - |  4602 | `				/* Pre-increment */` |
|      ! 0 |  4603 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4604 | `					pTos->rVal--;` |
|        - |  4605 | `					/* Try to get an integer representation */` |
|      ! 0 |  4606 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4607 | `				}else{` |
|      ! 0 |  4608 | `					pTos->x.iVal--;` |
|      ! 0 |  4609 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4610 | `				}` |
|      ! 0 |  4611 | `			}` |
|        - |  4612 | `		}` |
|        2 |  4613 | `	}` |
|        5 |  4614 | `	break;` |
|        - |  4615 | `/*` |
|        - |  4616 | ` * UMINUS: * * *` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Perform a unary minus operation.` |
|        - |  4619 | ` */` |
|    26421 |  4620 | `case PH7_OP_UMINUS:` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pTos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|        - |  4626 | `	/* Force a numeric (integer,real or both) cast */` |
|    52844 |  4627 | `	PH7_MemObjToNumeric(pTos);` |
|    52844 |  4628 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4629 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4630 | `	}` |
|    52844 |  4631 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    52814 |  4632 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    26406 |  4633 | `	}` |
|    52844 |  4634 | `	break;` |
|        - |  4635 | `/*` |
|        - |  4636 | ` * UPLUS: * * *` |
|        - |  4637 | ` *` |
|        - |  4638 | ` * Perform a unary plus operation.` |
|        - |  4639 | ` */` |
|       18 |  4640 | `case PH7_OP_UPLUS:` |
|        - |  4641 | `#ifdef UNTRUST` |
|        - |  4642 | `	if( pTos < pStack ){` |
|        - |  4643 | `		goto Abort;` |
|        - |  4644 | `	}` |
|        - |  4645 | `#endif` |
|        - |  4646 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4647 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4648 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4649 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4650 | `	}` |
|       37 |  4651 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4652 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4653 | `	}` |
|       37 |  4654 | `	break;` |
|        - |  4655 | `/*` |
|        - |  4656 | ` * OP_LNOT: * * *` |
|        - |  4657 | ` *` |
|        - |  4658 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4659 | ` * with its complement.` |
|        - |  4660 | ` */` |
|    42232 |  4661 | `case PH7_OP_LNOT:` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pTos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|        - |  4667 | `	/* Force a boolean cast */` |
|    84510 |  4668 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4669 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4670 | `	}` |
|    84510 |  4671 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    84510 |  4672 | `	break;` |
|        - |  4673 | `/*` |
|        - |  4674 | ` * OP_BITNOT: * * *` |
|        - |  4675 | ` *` |
|        - |  4676 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4677 | ` * with its ones-complement.` |
|        - |  4678 | ` */` |
|       13 |  4679 | `case PH7_OP_BITNOT:` |
|        - |  4680 | `#ifdef UNTRUST` |
|        - |  4681 | `	if( pTos < pStack ){` |
|        - |  4682 | `		goto Abort;` |
|        - |  4683 | `	}` |
|        - |  4684 | `#endif` |
|        - |  4685 | `	/* Force an integer cast */` |
|       28 |  4686 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4687 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4688 | `	}` |
|       28 |  4689 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4690 | `	break;` |
|        - |  4691 | `/* OP_MUL * * *` |
|        - |  4692 | ` * OP_MUL_STORE * * *` |
|        - |  4693 | ` *` |
|        - |  4694 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4695 | ` * and push the result back onto the stack.` |
|        - |  4696 | ` */` |
|     1278 |  4697 | `case PH7_OP_MUL:` |
|        - |  4698 | `case PH7_OP_MUL_STORE: {` |
|     2558 |  4699 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4700 | `	/* Force the operand to be numeric */` |
|        - |  4701 | `#ifdef UNTRUST` |
|        - |  4702 | `	if( pNos < pStack ){` |
|        - |  4703 | `		goto Abort;` |
|        - |  4704 | `	}` |
|        - |  4705 | `#endif` |
|     2558 |  4706 | `	PH7_MemObjToNumeric(pTos);` |
|     2558 |  4707 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4708 | `	/* Perform the requested operation */` |
|     2558 |  4709 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4710 | `		/* Floating point arithemic */` |
|        - |  4711 | `		ph7_real a,b,r;` |
|       19 |  4712 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  4713 | `			PH7_MemObjToReal(pTos);` |
|        4 |  4714 | `		}` |
|       19 |  4715 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4716 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4717 | `		}` |
|       19 |  4718 | `		a = pNos->rVal;` |
|       19 |  4719 | `		b = pTos->rVal;` |
|       19 |  4720 | `		r = a * b;` |
|        - |  4721 | `		/* Push the result */` |
|       19 |  4722 | `		pNos->rVal = r;` |
|       19 |  4723 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4724 | `		/* Try to get an integer representation */` |
|       19 |  4725 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  4726 | `	}else{` |
|        - |  4727 | `		/* Integer arithmetic */` |
|        - |  4728 | `		sxi64 a,b,r;` |
|     2540 |  4729 | `		a = pNos->x.iVal;` |
|     2540 |  4730 | `		b = pTos->x.iVal;` |
|     2540 |  4731 | `		r = a * b;` |
|        - |  4732 | `		/* Push the result */` |
|     2540 |  4733 | `		pNos->x.iVal = r;` |
|     2540 |  4734 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4735 | `	}` |
|     2558 |  4736 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4737 | `		ph7_value *pObj;` |
|       32 |  4738 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4739 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  4740 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  4741 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  4742 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  4743 | `		}` |
|       15 |  4744 | `	}` |
|     2558 |  4745 | `	VmPopOperand(&pTos,1);` |
|     2558 |  4746 | `	break;` |
|        - |  4747 | `				 }` |
|        - |  4748 | `/* OP_ADD * * *` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4751 | ` * and push the result back onto the stack.` |
|        - |  4752 | ` */` |
|      487 |  4753 | `case PH7_OP_ADD:{` |
|      976 |  4754 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4755 | `#ifdef UNTRUST` |
|        - |  4756 | `	if( pNos < pStack ){` |
|        - |  4757 | `		goto Abort;` |
|        - |  4758 | `	}` |
|        - |  4759 | `#endif` |
|        - |  4760 | `	/* Perform the addition */` |
|      976 |  4761 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      976 |  4762 | `	VmPopOperand(&pTos,1);` |
|      976 |  4763 | `	break;` |
|        - |  4764 | `				}` |
|        - |  4765 | `/*` |
|        - |  4766 | ` * OP_ADD_STORE * * *` |
|        - |  4767 | ` *` |
|        - |  4768 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4769 | ` * and push the result back onto the stack.` |
|        - |  4770 | ` */` |
|      497 |  4771 | `case PH7_OP_ADD_STORE:{` |
|      996 |  4772 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4773 | `	ph7_value *pObj;` |
|        - |  4774 | `	sxu32 nIdx;` |
|        - |  4775 | `#ifdef UNTRUST` |
|        - |  4776 | `	if( pNos < pStack ){` |
|        - |  4777 | `		goto Abort;` |
|        - |  4778 | `	}` |
|        - |  4779 | `#endif` |
|        - |  4780 | `	/* Perform the addition */` |
|      996 |  4781 | `	nIdx = pTos->nIdx;` |
|      996 |  4782 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4783 | `	/* Peform the store operation */` |
|      996 |  4784 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4785 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      996 |  4786 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      996 |  4787 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      996 |  4788 | `		PH7_MemObjStore(pTos,pObj);` |
|      497 |  4789 | `	}` |
|        - |  4790 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      996 |  4791 | `	PH7_MemObjStore(pTos,pNos);` |
|      996 |  4792 | `	VmPopOperand(&pTos,1);` |
|      996 |  4793 | `	break;` |
|        - |  4794 | `				}` |
|        - |  4795 | `/* OP_SUB * * *` |
|        - |  4796 | ` *` |
|        - |  4797 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4798 | ` * first (what was next on the stack) from the second (the` |
|        - |  4799 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4800 | ` */` |
|      302 |  4801 | `case PH7_OP_SUB: {` |
|      606 |  4802 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4803 | `#ifdef UNTRUST` |
|        - |  4804 | `	if( pNos < pStack ){` |
|        - |  4805 | `		goto Abort;` |
|        - |  4806 | `	}` |
|        - |  4807 | `#endif` |
|      606 |  4808 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4809 | `		/* Floating point arithemic */` |
|        - |  4810 | `		ph7_real a,b,r;` |
|       95 |  4811 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4812 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4813 | `		}` |
|       95 |  4814 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4815 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4816 | `		}` |
|       95 |  4817 | `		a = pNos->rVal;` |
|       95 |  4818 | `		b = pTos->rVal;` |
|       95 |  4819 | `		r = a - b;` |
|        - |  4820 | `		/* Push the result */` |
|       95 |  4821 | `		pNos->rVal = r;` |
|       95 |  4822 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4823 | `		/* Try to get an integer representation */` |
|       95 |  4824 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4825 | `	}else{` |
|        - |  4826 | `		/* Integer arithmetic */` |
|        - |  4827 | `		sxi64 a,b,r;` |
|      512 |  4828 | `		a = pNos->x.iVal;` |
|      512 |  4829 | `		b = pTos->x.iVal;` |
|      512 |  4830 | `		r = a - b;` |
|        - |  4831 | `		/* Push the result */` |
|      512 |  4832 | `		pNos->x.iVal = r;` |
|      512 |  4833 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4834 | `	}` |
|      606 |  4835 | `	VmPopOperand(&pTos,1);` |
|      606 |  4836 | `	break;` |
|        - |  4837 | `				 }` |
|        - |  4838 | `/* OP_SUB_STORE * * *` |
|        - |  4839 | ` *` |
|        - |  4840 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4841 | ` * first (what was next on the stack) from the second (the` |
|        - |  4842 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4843 | ` */` |
|        4 |  4844 | `case PH7_OP_SUB_STORE: {` |
|       10 |  4845 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4846 | `	ph7_value *pObj;` |
|        - |  4847 | `#ifdef UNTRUST` |
|        - |  4848 | `	if( pNos < pStack ){` |
|        - |  4849 | `		goto Abort;` |
|        - |  4850 | `	}` |
|        - |  4851 | `#endif` |
|       10 |  4852 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4853 | `		/* Floating point arithemic */` |
|        - |  4854 | `		ph7_real a,b,r;` |
|      ! 0 |  4855 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4856 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4857 | `		}` |
|      ! 0 |  4858 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4859 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4860 | `		}` |
|      ! 0 |  4861 | `		a = pTos->rVal;` |
|      ! 0 |  4862 | `		b = pNos->rVal;` |
|      ! 0 |  4863 | `		r = a - b;` |
|        - |  4864 | `		/* Push the result */` |
|      ! 0 |  4865 | `		pNos->rVal = r;` |
|      ! 0 |  4866 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4867 | `		/* Try to get an integer representation */` |
|      ! 0 |  4868 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4869 | `	}else{` |
|        - |  4870 | `		/* Integer arithmetic */` |
|        - |  4871 | `		sxi64 a,b,r;` |
|       10 |  4872 | `		a = pTos->x.iVal;` |
|       10 |  4873 | `		b = pNos->x.iVal;` |
|       10 |  4874 | `		r = a - b;` |
|        - |  4875 | `		/* Push the result */` |
|       10 |  4876 | `		pNos->x.iVal = r;` |
|       10 |  4877 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4878 | `	}` |
|       10 |  4879 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4880 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  4881 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  4882 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  4883 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  4884 | `	}` |
|       10 |  4885 | `	VmPopOperand(&pTos,1);` |
|       10 |  4886 | `	break;` |
|        - |  4887 | `				 }` |
|        - |  4888 |  |
|        - |  4889 | `/*` |
|        - |  4890 | ` * OP_MOD * * *` |
|        - |  4891 | ` *` |
|        - |  4892 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4893 | ` * first (what was next on the stack) from the second (the` |
|        - |  4894 | ` * top of the stack) and push the remainder after division` |
|        - |  4895 | ` * onto the stack.` |
|        - |  4896 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4897 | ` */` |
|      307 |  4898 | `case PH7_OP_MOD:{` |
|      616 |  4899 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4900 | `	sxi64 a,b,r;` |
|        - |  4901 | `#ifdef UNTRUST` |
|        - |  4902 | `	if( pNos < pStack ){` |
|        - |  4903 | `		goto Abort;` |
|        - |  4904 | `	}` |
|        - |  4905 | `#endif` |
|        - |  4906 | `	/* Force the operands to be integer */` |
|      616 |  4907 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4908 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4909 | `	}` |
|      616 |  4910 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4911 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4912 | `	}` |
|        - |  4913 | `	/* Perform the requested operation */` |
|      616 |  4914 | `	a = pNos->x.iVal;` |
|      616 |  4915 | `	b = pTos->x.iVal;` |
|      616 |  4916 | `	if( b == 0 ){` |
|        3 |  4917 | `		r = 0;` |
|        3 |  4918 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4919 | `		/* goto Abort; */` |
|        2 |  4920 | `	}else{` |
|      613 |  4921 | `		r = a%b;` |
|        - |  4922 | `	}` |
|        - |  4923 | `	/* Push the result */` |
|      616 |  4924 | `	pNos->x.iVal = r;` |
|      616 |  4925 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4926 | `	VmPopOperand(&pTos,1);` |
|      616 |  4927 | `	break;` |
|        - |  4928 | `				}` |
|        - |  4929 | `/*` |
|        - |  4930 | ` * OP_MOD_STORE * * *` |
|        - |  4931 | ` *` |
|        - |  4932 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4933 | ` * first (what was next on the stack) from the second (the` |
|        - |  4934 | ` * top of the stack) and push the remainder after division` |
|        - |  4935 | ` * onto the stack.` |
|        - |  4936 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4937 | ` */` |
|        1 |  4938 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4939 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4940 | `	ph7_value *pObj;` |
|        - |  4941 | `	sxi64 a,b,r;` |
|        - |  4942 | `#ifdef UNTRUST` |
|        - |  4943 | `	if( pNos < pStack ){` |
|        - |  4944 | `		goto Abort;` |
|        - |  4945 | `	}` |
|        - |  4946 | `#endif` |
|        - |  4947 | `	/* Force the operands to be integer */` |
|        3 |  4948 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4949 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4950 | `	}` |
|        3 |  4951 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4952 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4953 | `	}` |
|        - |  4954 | `	/* Perform the requested operation */` |
|        3 |  4955 | `	a = pTos->x.iVal;` |
|        3 |  4956 | `	b = pNos->x.iVal;` |
|        3 |  4957 | `	if( b == 0 ){` |
|      ! 0 |  4958 | `		r = 0;` |
|      ! 0 |  4959 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4960 | `		/* goto Abort; */` |
|      ! 0 |  4961 | `	}else{` |
|        3 |  4962 | `		r = a%b;` |
|        - |  4963 | `	}` |
|        - |  4964 | `	/* Push the result */` |
|        3 |  4965 | `	pNos->x.iVal = r;` |
|        3 |  4966 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4967 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4968 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4969 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4970 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  4971 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4972 | `	}` |
|        3 |  4973 | `	VmPopOperand(&pTos,1);` |
|        3 |  4974 | `	break;` |
|        - |  4975 | `				}` |
|        - |  4976 | `/*` |
|        - |  4977 | ` * OP_DIV * * *` |
|        - |  4978 | ` *` |
|        - |  4979 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4980 | ` * first (what was next on the stack) from the second (the` |
|        - |  4981 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4982 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4983 | ` */` |
|       30 |  4984 | `case PH7_OP_DIV:{` |
|       62 |  4985 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4986 | `	ph7_real a,b,r;` |
|        - |  4987 | `#ifdef UNTRUST` |
|        - |  4988 | `	if( pNos < pStack ){` |
|        - |  4989 | `		goto Abort;` |
|        - |  4990 | `	}` |
|        - |  4991 | `#endif` |
|        - |  4992 | `	/* Force the operands to be real */` |
|       62 |  4993 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4994 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4995 | `	}` |
|       62 |  4996 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4997 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4998 | `	}` |
|        - |  4999 | `	/* Perform the requested operation */` |
|       62 |  5000 | `	a = pNos->rVal;` |
|       62 |  5001 | `	b = pTos->rVal;` |
|       62 |  5002 | `	if( b == 0 ){` |
|        - |  5003 | `		/* Division by zero */` |
|        3 |  5004 | `		pNos->rVal = 0;` |
|        3 |  5005 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5006 | `		/* goto Abort; */` |
|        2 |  5007 | `	}else{` |
|       59 |  5008 | `		r = a/b;` |
|        - |  5009 | `		/* Push the result */` |
|       59 |  5010 | `		pNos->rVal = r;` |
|       59 |  5011 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5012 | `		/* Try to get an integer representation */` |
|       59 |  5013 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5014 | `	}` |
|       62 |  5015 | `	VmPopOperand(&pTos,1);` |
|       62 |  5016 | `	break;` |
|        - |  5017 | `				}` |
|        - |  5018 | `/*` |
|        - |  5019 | ` * OP_DIV_STORE * * *` |
|        - |  5020 | ` *` |
|        - |  5021 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5022 | ` * first (what was next on the stack) from the second (the` |
|        - |  5023 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5024 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5025 | ` */` |
|        2 |  5026 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5027 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5028 | `	ph7_value *pObj;` |
|        - |  5029 | `	ph7_real a,b,r;` |
|        - |  5030 | `#ifdef UNTRUST` |
|        - |  5031 | `	if( pNos < pStack ){` |
|        - |  5032 | `		goto Abort;` |
|        - |  5033 | `	}` |
|        - |  5034 | `#endif` |
|        - |  5035 | `	/* Force the operands to be real */` |
|        5 |  5036 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5037 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5038 | `	}` |
|        5 |  5039 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5040 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5041 | `	}` |
|        - |  5042 | `	/* Perform the requested operation */` |
|        5 |  5043 | `	a = pTos->rVal;` |
|        5 |  5044 | `	b = pNos->rVal;` |
|        5 |  5045 | `	if( b == 0 ){` |
|        - |  5046 | `		/* Division by zero */` |
|      ! 0 |  5047 | `		r = 0;` |
|      ! 0 |  5048 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5049 | `		/* goto Abort; */` |
|      ! 0 |  5050 | `	}else{` |
|        5 |  5051 | `		r = a/b;` |
|        - |  5052 | `		/* Push the result */` |
|        5 |  5053 | `		pNos->rVal = r;` |
|        5 |  5054 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5055 | `		/* Try to get an integer representation */` |
|        5 |  5056 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5057 | `	}` |
|        5 |  5058 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5059 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5060 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5061 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5062 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5063 | `	}` |
|        5 |  5064 | `	VmPopOperand(&pTos,1);` |
|        5 |  5065 | `	break;` |
|        - |  5066 | `				}` |
|        - |  5067 | `/* OP_BAND * * *` |
|        - |  5068 | ` *` |
|        - |  5069 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5070 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5071 | ` * two elements.` |
|        - |  5072 | `*/` |
|        - |  5073 | `/* OP_BOR * * *` |
|        - |  5074 | ` *` |
|        - |  5075 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5076 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5077 | ` * two elements.` |
|        - |  5078 | ` */` |
|        - |  5079 | `/* OP_BXOR * * *` |
|        - |  5080 | ` *` |
|        - |  5081 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5082 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5083 | ` * two elements.` |
|        - |  5084 | ` */` |
|       44 |  5085 | `case PH7_OP_BAND:` |
|        - |  5086 | `case PH7_OP_BOR:` |
|        - |  5087 | `case PH7_OP_BXOR:{` |
|       90 |  5088 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5089 | `	sxi64 a,b,r;` |
|        - |  5090 | `#ifdef UNTRUST` |
|        - |  5091 | `	if( pNos < pStack ){` |
|        - |  5092 | `		goto Abort;` |
|        - |  5093 | `	}` |
|        - |  5094 | `#endif` |
|        - |  5095 | `	/* Force the operands to be integer */` |
|       90 |  5096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5098 | `	}` |
|       90 |  5099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5101 | `	}` |
|        - |  5102 | `	/* Perform the requested operation */` |
|       90 |  5103 | `	a = pNos->x.iVal;` |
|       90 |  5104 | `	b = pTos->x.iVal;` |
|       90 |  5105 | `	switch(pInstr->iOp){` |
|        7 |  5106 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5107 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5108 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5109 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5110 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5111 | `	case PH7_OP_BAND:` |
|       62 |  5112 | `	default:          r = a&b; break;` |
|        - |  5113 | `	}` |
|        - |  5114 | `	/* Push the result */` |
|       90 |  5115 | `	pNos->x.iVal = r;` |
|       90 |  5116 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5117 | `	VmPopOperand(&pTos,1);` |
|       90 |  5118 | `	break;` |
|        - |  5119 | `				 }` |
|        - |  5120 | `/* OP_BAND_STORE * * *` |
|        - |  5121 | ` *` |
|        - |  5122 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5123 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5124 | ` * two elements.` |
|        - |  5125 | `*/` |
|        - |  5126 | `/* OP_BOR_STORE * * *` |
|        - |  5127 | ` *` |
|        - |  5128 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5129 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5130 | ` * two elements.` |
|        - |  5131 | ` */` |
|        - |  5132 | `/* OP_BXOR_STORE * * *` |
|        - |  5133 | ` *` |
|        - |  5134 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5135 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5136 | ` * two elements.` |
|        - |  5137 | ` */` |
|       10 |  5138 | `case PH7_OP_BAND_STORE:` |
|        - |  5139 | `case PH7_OP_BOR_STORE:` |
|        - |  5140 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5141 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5142 | `	ph7_value *pObj;` |
|        - |  5143 | `	sxi64 a,b,r;` |
|        - |  5144 | `#ifdef UNTRUST` |
|        - |  5145 | `	if( pNos < pStack ){` |
|        - |  5146 | `		goto Abort;` |
|        - |  5147 | `	}` |
|        - |  5148 | `#endif` |
|        - |  5149 | `	/* Force the operands to be integer */` |
|       21 |  5150 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5151 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5152 | `	}` |
|       21 |  5153 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5154 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5155 | `	}` |
|        - |  5156 | `	/* Perform the requested operation */` |
|       21 |  5157 | `	a = pTos->x.iVal;` |
|       21 |  5158 | `	b = pNos->x.iVal;` |
|       21 |  5159 | `	switch(pInstr->iOp){` |
|        3 |  5160 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5161 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5162 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5163 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5164 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5165 | `	case PH7_OP_BAND:` |
|        7 |  5166 | `	default:          r = a&b; break;` |
|        - |  5167 | `	}` |
|        - |  5168 | `	/* Push the result */` |
|       21 |  5169 | `	pNos->x.iVal = r;` |
|       21 |  5170 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5171 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5172 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5173 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5174 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5175 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5176 | `	}` |
|       21 |  5177 | `	VmPopOperand(&pTos,1);` |
|       21 |  5178 | `	break;` |
|        - |  5179 | `				 }` |
|        - |  5180 | `/* OP_SHL * * *` |
|        - |  5181 | ` *` |
|        - |  5182 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5183 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5184 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5185 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5186 | ` */` |
|        - |  5187 | `/* OP_SHR * * *` |
|        - |  5188 | ` *` |
|        - |  5189 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5190 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5191 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5192 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5193 | ` */` |
|       12 |  5194 | `case PH7_OP_SHL:` |
|        - |  5195 | `case PH7_OP_SHR: {` |
|       25 |  5196 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5197 | `	sxi64 a,r;` |
|        - |  5198 | `	sxi32 b;` |
|        - |  5199 | `#ifdef UNTRUST` |
|        - |  5200 | `	if( pNos < pStack ){` |
|        - |  5201 | `		goto Abort;` |
|        - |  5202 | `	}` |
|        - |  5203 | `#endif` |
|        - |  5204 | `	/* Force the operands to be integer */` |
|       25 |  5205 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5206 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5207 | `	}` |
|       25 |  5208 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5209 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5210 | `	}` |
|        - |  5211 | `	/* Perform the requested operation */` |
|       25 |  5212 | `	a = pNos->x.iVal;` |
|       25 |  5213 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5214 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5215 | `		r = a << b;` |
|        8 |  5216 | `	}else{` |
|       11 |  5217 | `		r = a >> b;` |
|        - |  5218 | `	}` |
|        - |  5219 | `	/* Push the result */` |
|       25 |  5220 | `	pNos->x.iVal = r;` |
|       25 |  5221 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5222 | `	VmPopOperand(&pTos,1);` |
|       25 |  5223 | `	break;` |
|        - |  5224 | `				 }` |
|        - |  5225 | `/*  OP_SHL_STORE * * *` |
|        - |  5226 | ` *` |
|        - |  5227 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5228 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5229 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5230 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5231 | ` */` |
|        - |  5232 | `/* OP_SHR_STORE * * *` |
|        - |  5233 | ` *` |
|        - |  5234 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5235 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5236 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5237 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5238 | ` */` |
|        9 |  5239 | `case PH7_OP_SHL_STORE:` |
|        - |  5240 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5241 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5242 | `	ph7_value *pObj;` |
|        - |  5243 | `	sxi64 a,r;` |
|        - |  5244 | `	sxi32 b;` |
|        - |  5245 | `#ifdef UNTRUST` |
|        - |  5246 | `	if( pNos < pStack ){` |
|        - |  5247 | `		goto Abort;` |
|        - |  5248 | `	}` |
|        - |  5249 | `#endif` |
|        - |  5250 | `	/* Force the operands to be integer */` |
|       19 |  5251 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5252 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5253 | `	}` |
|       19 |  5254 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5255 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5256 | `	}` |
|        - |  5257 | `	/* Perform the requested operation */` |
|       19 |  5258 | `	a = pTos->x.iVal;` |
|       19 |  5259 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5260 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5261 | `		r = a << b;` |
|        5 |  5262 | `	}else{` |
|       11 |  5263 | `		r = a >> b;` |
|        - |  5264 | `	}` |
|        - |  5265 | `	/* Push the result */` |
|       19 |  5266 | `	pNos->x.iVal = r;` |
|       19 |  5267 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5268 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5269 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5270 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5271 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5272 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5273 | `	}` |
|       19 |  5274 | `	VmPopOperand(&pTos,1);` |
|       19 |  5275 | `	break;` |
|        - |  5276 | `				 }` |
|        - |  5277 | `/* CAT:  P1 * *` |
|        - |  5278 | ` *` |
|        - |  5279 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5280 | ` * back.` |
|        - |  5281 | ` */` |
|    66945 |  5282 | `case PH7_OP_CAT:{` |
|        - |  5283 | `	ph7_value *pNos,*pCur;` |
|   133892 |  5284 | `	if( pInstr->iP1 < 1 ){` |
|   106628 |  5285 | `		pNos = &pTos[-1];` |
|    53315 |  5286 | `	}else{` |
|    27266 |  5287 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5288 | `	}` |
|        - |  5289 | `#ifdef UNTRUST` |
|        - |  5290 | `	if( pNos < pStack ){` |
|        - |  5291 | `		goto Abort;` |
|        - |  5292 | `	}` |
|        - |  5293 | `#endif` |
|        - |  5294 | `	/* Force a string cast */` |
|   133892 |  5295 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5296 | `		PH7_MemObjToString(pNos);` |
|      817 |  5297 | `	}` |
|   133892 |  5298 | `	pCur = &pNos[1];` |
|   270320 |  5299 | `	while( pCur <= pTos ){` |
|   136430 |  5300 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50868 |  5301 | `			PH7_MemObjToString(pCur);` |
|    25433 |  5302 | `		}` |
|        - |  5303 | `		/* Perform the concatenation */` |
|   136430 |  5304 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   136388 |  5305 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    68193 |  5306 | `		}` |
|   136430 |  5307 | `		SyBlobRelease(&pCur->sBlob);` |
|   136430 |  5308 | `		pCur++;` |
|        2 |  5309 | `	}` |
|   133892 |  5310 | `	pTos = pNos;` |
|   133892 |  5311 | `	break;` |
|        - |  5312 | `				}` |
|        - |  5313 | `/*  CAT_STORE: * * *` |
|        - |  5314 | ` *` |
|        - |  5315 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5316 | ` * back.` |
|        - |  5317 | ` */` |
|     3660 |  5318 | `case PH7_OP_CAT_STORE:{` |
|     7322 |  5319 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5320 | `	ph7_value *pObj;` |
|        - |  5321 | `#ifdef UNTRUST` |
|        - |  5322 | `	if( pNos < pStack ){` |
|        - |  5323 | `		goto Abort;` |
|        - |  5324 | `	}` |
|        - |  5325 | `#endif` |
|     7322 |  5326 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5327 | `		/* Force a string cast */` |
|        3 |  5328 | `		PH7_MemObjToString(pTos);` |
|        1 |  5329 | `	}` |
|     7322 |  5330 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5331 | `		/* Force a string cast */` |
|      ! 0 |  5332 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5333 | `	}` |
|        - |  5334 | `	/* Perform the concatenation (Reverse order) */` |
|     7322 |  5335 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7322 |  5336 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3660 |  5337 | `	}` |
|        - |  5338 | `	/* Perform the store operation */` |
|     7322 |  5339 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5340 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7322 |  5341 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7322 |  5342 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7320 |  5343 | `		PH7_MemObjStore(pTos,pObj);` |
|     3659 |  5344 | `	}` |
|     7320 |  5345 | `	PH7_MemObjStore(pTos,pNos);` |
|     7320 |  5346 | `	VmPopOperand(&pTos,1);` |
|     7320 |  5347 | `	break;` |
|        - |  5348 | `				}` |
|        - |  5349 | `/* OP_AND: * * *` |
|        - |  5350 | ` *` |
|        - |  5351 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5352 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5353 | ` * stack.` |
|        - |  5354 | ` */` |
|        - |  5355 | `/* OP_OR: * * *` |
|        - |  5356 | ` *` |
|        - |  5357 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5358 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5359 | ` * stack.` |
|        - |  5360 | ` */` |
|   100963 |  5361 | `case PH7_OP_LAND:` |
|        - |  5362 | `case PH7_OP_LOR: {` |
|   201972 |  5363 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5364 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5365 | `#ifdef UNTRUST` |
|        - |  5366 | `	if( pNos < pStack ){` |
|        - |  5367 | `		goto Abort;` |
|        - |  5368 | `	}` |
|        - |  5369 | `#endif` |
|        - |  5370 | `	/* Force a boolean cast */` |
|   201972 |  5371 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5372 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5373 | `	}` |
|   201972 |  5374 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5375 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5376 | `	}` |
|   201972 |  5377 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   201972 |  5378 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   201972 |  5379 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5380 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    92962 |  5381 | `		v1 = and_logic[v1*3+v2];` |
|    46504 |  5382 | `	}else{` |
|        - |  5383 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   109012 |  5384 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5385 | `	}` |
|   201972 |  5386 | `	if( v1 == 2 ){` |
|      ! 0 |  5387 | `		v1 = 1;` |
|      ! 0 |  5388 | `	}` |
|   201972 |  5389 | `	VmPopOperand(&pTos,1);` |
|   201972 |  5390 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   201972 |  5391 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   201972 |  5392 | `	break;` |
|        - |  5393 | `				 }` |
|        - |  5394 | `/*` |
|        - |  5395 | ` * OP_NULLC: * * *` |
|        - |  5396 | ` * Null coalescing operator '??'.` |
|        - |  5397 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5398 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5399 | ` */` |
|        - |  5400 | `/*` |
|        - |  5401 | ` * OP_NULLC: * P2 *` |
|        - |  5402 | ` * Short-circuit null coalescing '??'.` |
|        - |  5403 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5404 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5405 | ` */` |
|       19 |  5406 | `case PH7_OP_NULLC: {` |
|        - |  5407 | `#ifdef UNTRUST` |
|        - |  5408 | `	if( pTos < pStack ){` |
|        - |  5409 | `		goto Abort;` |
|        - |  5410 | `	}` |
|        - |  5411 | `#endif` |
|       40 |  5412 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5413 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  5414 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  5415 | `	}else{` |
|        - |  5416 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  5417 | `		VmPopOperand(&pTos, 1);` |
|        - |  5418 | `	}` |
|       40 |  5419 | `	break;` |
|        - |  5420 |  |
|        - |  5421 | `/*` |
|        - |  5422 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5423 | ` * Null coalescing assignment short-circuit.` |
|        - |  5424 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5425 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5426 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5427 | ` */` |
|       23 |  5428 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5429 | `#ifdef UNTRUST` |
|        - |  5430 | `	if( pTos < pStack ){` |
|        - |  5431 | `		goto Abort;` |
|        - |  5432 | `	}` |
|        - |  5433 | `#endif` |
|       47 |  5434 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5435 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5436 | `	}` |
|       47 |  5437 | `	break;` |
|        - |  5438 |  |
|        - |  5439 | `/*` |
|        - |  5440 | ` * OP_NULLC_STORE: * * *` |
|        - |  5441 | ` * Null coalescing assignment store.` |
|        - |  5442 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5443 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5444 | ` * expression result.` |
|        - |  5445 | ` */` |
|       14 |  5446 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5447 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5448 | `	ph7_value *pObj;` |
|        - |  5449 | `	sxu32 nIdx;` |
|        - |  5450 | `#ifdef UNTRUST` |
|        - |  5451 | `	if( pNos < pStack ){` |
|        - |  5452 | `		goto Abort;` |
|        - |  5453 | `	}` |
|        - |  5454 | `#endif` |
|       29 |  5455 | `	nIdx = pNos->nIdx;` |
|       29 |  5456 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5457 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5458 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5459 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5460 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5461 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5462 | `	}` |
|       29 |  5463 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5464 | `	VmPopOperand(&pTos,1);` |
|       29 |  5465 | `	break;` |
|        - |  5466 |  |
|        - |  5467 | `/*` |
|        - |  5468 | ` * OP_SPREAD: * * *` |
|        - |  5469 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5470 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5471 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5472 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5473 | ` */` |
|        9 |  5474 | `case PH7_OP_SPREAD: {` |
|        - |  5475 | `#ifdef UNTRUST` |
|        - |  5476 | `	if( pTos < pStack ){` |
|        - |  5477 | `		goto Abort;` |
|        - |  5478 | `	}` |
|        - |  5479 | `#endif` |
|       20 |  5480 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  5481 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  5482 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  5483 | `		if( nEntry == 0 ){` |
|        - |  5484 | `			/* Empty array — remove from stack */` |
|        3 |  5485 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5486 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  5487 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5488 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5489 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5490 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5491 | `				VM_STACK_GUARD);` |
|      ! 0 |  5492 | `		}else{` |
|        - |  5493 | `			ph7_hashmap_node *pNode2;` |
|        - |  5494 | `			ph7_value *pElem;` |
|        - |  5495 | `			sxu32 i;` |
|        - |  5496 | `			/* Overwrite TOS with first element */` |
|       18 |  5497 | `			pNode2 = pMap->pFirst;` |
|       18 |  5498 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  5499 | `			PH7_MemObjRelease(pTos);` |
|       18 |  5500 | `			if( pElem ){` |
|       18 |  5501 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  5502 | `			}` |
|       18 |  5503 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5504 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5505 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  5506 | `			pNode2 = pNode2->pPrev;` |
|        - |  5507 | `			/* Push remaining elements */` |
|       44 |  5508 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  5509 | `				pTos++;` |
|       28 |  5510 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  5511 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  5512 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  5513 | `				if( pElem ){` |
|       28 |  5514 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  5515 | `				}` |
|       28 |  5516 | `				pNode2 = pNode2->pPrev;` |
|       15 |  5517 | `			}` |
|       18 |  5518 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5519 | `		}` |
|        9 |  5520 | `	}` |
|        - |  5521 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  5522 | `	break;` |
|        - |  5523 |  |
|        - |  5524 | `/* OP_LXOR: * * *` |
|        - |  5525 | ` *` |
|        - |  5526 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5527 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5528 | ` * stack.` |
|        - |  5529 | ` * According to the PHP language reference manual:` |
|        - |  5530 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5531 | ` *  TRUE,but not both.` |
|        - |  5532 | ` */` |
|        5 |  5533 | `case PH7_OP_LXOR:{` |
|       11 |  5534 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5535 | `	sxi32 v = 0;` |
|        - |  5536 | `#ifdef UNTRUST` |
|        - |  5537 | `	if( pNos < pStack ){` |
|        - |  5538 | `		goto Abort;` |
|        - |  5539 | `	}` |
|        - |  5540 | `#endif` |
|        - |  5541 | `	/* Force a boolean cast */` |
|       11 |  5542 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5543 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5544 | `	}` |
|       11 |  5545 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5546 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5547 | `	}` |
|       11 |  5548 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5549 | `		v = 1;` |
|        3 |  5550 | `	}` |
|       11 |  5551 | `	VmPopOperand(&pTos,1);` |
|       11 |  5552 | `	pTos->x.iVal = v;` |
|       11 |  5553 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5554 | `	break;` |
|        - |  5555 | `				 }` |
|        - |  5556 | `/* OP_EQ P1 P2 P3` |
|        - |  5557 | ` *` |
|        - |  5558 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5559 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5560 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5561 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5562 | ` */` |
|        - |  5563 | `/* OP_NEQ P1 P2 P3` |
|        - |  5564 | ` *` |
|        - |  5565 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5566 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5567 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5568 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5569 | ` */` |
|     4214 |  5570 | `case PH7_OP_EQ:` |
|        - |  5571 | `case PH7_OP_NEQ: {` |
|     8430 |  5572 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5573 | `	/* Perform the comparison and act accordingly */` |
|        - |  5574 | `#ifdef UNTRUST` |
|        - |  5575 | `	if( pNos < pStack ){` |
|        - |  5576 | `		goto Abort;` |
|        - |  5577 | `	}` |
|        - |  5578 | `#endif` |
|     8430 |  5579 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8430 |  5580 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5581 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8421 |  5582 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8386 |  5583 | `		rc = rc == 0;` |
|     4194 |  5584 | `	}else{` |
|       28 |  5585 | `		rc = rc != 0;` |
|        - |  5586 | `	}` |
|     8430 |  5587 | `	VmPopOperand(&pTos,1);` |
|     8430 |  5588 | `	if( !pInstr->iP2 ){` |
|        - |  5589 | `		/* Push comparison result without taking the jump */` |
|     8430 |  5590 | `		PH7_MemObjRelease(pTos);` |
|     8430 |  5591 | `		pTos->x.iVal = rc;` |
|        - |  5592 | `		/* Invalidate any prior representation */` |
|     8430 |  5593 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4216 |  5594 | `	}else{` |
|      ! 0 |  5595 | `		if( rc ){` |
|        - |  5596 | `			/* Jump to the desired location */` |
|      ! 0 |  5597 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5598 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5599 | `		}` |
|        - |  5600 | `	}` |
|     8430 |  5601 | `	break;` |
|        - |  5602 | `				 }` |
|        - |  5603 | `/* OP_TEQ P1 P2 *` |
|        - |  5604 | ` *` |
|        - |  5605 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5606 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5607 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5608 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5609 | ` */` |
|   145916 |  5610 | `case PH7_OP_TEQ: {` |
|   291834 |  5611 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5612 | `	/* Perform the comparison and act accordingly */` |
|        - |  5613 | `#ifdef UNTRUST` |
|        - |  5614 | `	if( pNos < pStack ){` |
|        - |  5615 | `		goto Abort;` |
|        - |  5616 | `	}` |
|        - |  5617 | `#endif` |
|   291834 |  5618 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   291834 |  5619 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5620 | `		rc = 0;` |
|        2 |  5621 | `	}else{` |
|   291832 |  5622 | `		rc = rc == 0;` |
|        - |  5623 | `	}` |
|   291834 |  5624 | `	VmPopOperand(&pTos,1);` |
|   291834 |  5625 | `	if( !pInstr->iP2 ){` |
|        - |  5626 | `		/* Push comparison result without taking the jump */` |
|   291834 |  5627 | `		PH7_MemObjRelease(pTos);` |
|   291834 |  5628 | `		pTos->x.iVal = rc;` |
|        - |  5629 | `		/* Invalidate any prior representation */` |
|   291834 |  5630 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   145918 |  5631 | `	}else{` |
|      ! 0 |  5632 | `		if( rc ){` |
|        - |  5633 | `			/* Jump to the desired location */` |
|      ! 0 |  5634 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5635 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5636 | `		}` |
|        - |  5637 | `	}` |
|   291834 |  5638 | `	break;` |
|        - |  5639 | `				 }` |
|        - |  5640 | `/* OP_TNE P1 P2 *` |
|        - |  5641 | ` *` |
|        - |  5642 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5643 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5644 | ` * instruction.` |
|        - |  5645 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5646 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5647 | ` *` |
|        - |  5648 | ` */` |
|   112625 |  5649 | `case PH7_OP_TNE: {` |
|   225252 |  5650 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5651 | `	/* Perform the comparison and act accordingly */` |
|        - |  5652 | `#ifdef UNTRUST` |
|        - |  5653 | `	if( pNos < pStack ){` |
|        - |  5654 | `		goto Abort;` |
|        - |  5655 | `	}` |
|        - |  5656 | `#endif` |
|   225252 |  5657 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   225252 |  5658 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5659 | `		rc = 1;` |
|        2 |  5660 | `	}else{` |
|   225250 |  5661 | `		rc = rc != 0;` |
|        - |  5662 | `	}` |
|   225252 |  5663 | `	VmPopOperand(&pTos,1);` |
|   225252 |  5664 | `	if( !pInstr->iP2 ){` |
|        - |  5665 | `		/* Push comparison result without taking the jump */` |
|   225252 |  5666 | `		PH7_MemObjRelease(pTos);` |
|   225252 |  5667 | `		pTos->x.iVal = rc;` |
|        - |  5668 | `		/* Invalidate any prior representation */` |
|   225252 |  5669 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112627 |  5670 | `	}else{` |
|      ! 0 |  5671 | `		if( rc ){` |
|        - |  5672 | `			/* Jump to the desired location */` |
|      ! 0 |  5673 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5674 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5675 | `		}` |
|        - |  5676 | `	}` |
|   225252 |  5677 | `	break;` |
|        - |  5678 | `				 }` |
|        - |  5679 | `/* OP_LT P1 P2 P3` |
|        - |  5680 | ` *` |
|        - |  5681 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5682 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5683 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5684 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5685 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5686 | ` *` |
|        - |  5687 | ` */` |
|        - |  5688 | `/* OP_LE P1 P2 P3` |
|        - |  5689 | ` *` |
|        - |  5690 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5691 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5692 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5693 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5694 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5695 | ` *` |
|        - |  5696 | ` */` |
|   107033 |  5697 | `case PH7_OP_LT:` |
|        - |  5698 | `case PH7_OP_LE: {` |
|   214112 |  5699 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5700 | `	/* Perform the comparison and act accordingly */` |
|        - |  5701 | `#ifdef UNTRUST` |
|        - |  5702 | `	if( pNos < pStack ){` |
|        - |  5703 | `		goto Abort;` |
|        - |  5704 | `	}` |
|        - |  5705 | `#endif` |
|   214112 |  5706 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   214112 |  5707 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5708 | `		rc = 0;` |
|   214108 |  5709 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      620 |  5710 | `		rc = rc < 1;` |
|      311 |  5711 | `	}else{` |
|   213486 |  5712 | `		rc = rc < 0;` |
|        - |  5713 | `	}` |
|   214112 |  5714 | `	VmPopOperand(&pTos,1);` |
|   214112 |  5715 | `	if( !pInstr->iP2 ){` |
|        - |  5716 | `		/* Push comparison result without taking the jump */` |
|   214112 |  5717 | `		PH7_MemObjRelease(pTos);` |
|   214112 |  5718 | `		pTos->x.iVal = rc;` |
|        - |  5719 | `		/* Invalidate any prior representation */` |
|   214112 |  5720 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   107079 |  5721 | `	}else{` |
|      ! 0 |  5722 | `		if( rc ){` |
|        - |  5723 | `			/* Jump to the desired location */` |
|      ! 0 |  5724 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5725 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5726 | `		}` |
|        - |  5727 | `	}` |
|   214112 |  5728 | `	break;` |
|        - |  5729 | `				}` |
|        - |  5730 | `/* OP_GT P1 P2 P3` |
|        - |  5731 | ` *` |
|        - |  5732 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5733 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5734 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5735 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5736 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5737 | ` *` |
|        - |  5738 | ` */` |
|        - |  5739 | `/* OP_GE P1 P2 P3` |
|        - |  5740 | ` *` |
|        - |  5741 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5742 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5743 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5744 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5745 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5746 | ` *` |
|        - |  5747 | ` */` |
|    51912 |  5748 | `case PH7_OP_GT:` |
|        - |  5749 | `case PH7_OP_GE: {` |
|   103826 |  5750 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5751 | `	/* Perform the comparison and act accordingly */` |
|        - |  5752 | `#ifdef UNTRUST` |
|        - |  5753 | `	if( pNos < pStack ){` |
|        - |  5754 | `		goto Abort;` |
|        - |  5755 | `	}` |
|        - |  5756 | `#endif` |
|   103826 |  5757 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   103826 |  5758 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5759 | `		rc = 0;` |
|   103822 |  5760 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   103658 |  5761 | `		rc = rc >= 0;` |
|    51830 |  5762 | `	}else{` |
|      162 |  5763 | `		rc = rc > 0;` |
|        - |  5764 | `	}` |
|   103826 |  5765 | `	VmPopOperand(&pTos,1);` |
|   103826 |  5766 | `	if( !pInstr->iP2 ){` |
|        - |  5767 | `		/* Push comparison result without taking the jump */` |
|   103826 |  5768 | `		PH7_MemObjRelease(pTos);` |
|   103826 |  5769 | `		pTos->x.iVal = rc;` |
|        - |  5770 | `		/* Invalidate any prior representation */` |
|   103826 |  5771 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    51914 |  5772 | `	}else{` |
|      ! 0 |  5773 | `		if( rc ){` |
|        - |  5774 | `			/* Jump to the desired location */` |
|      ! 0 |  5775 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5776 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5777 | `		}` |
|        - |  5778 | `	}` |
|   103826 |  5779 | `	break;` |
|        - |  5780 | `				}` |
|        - |  5781 | `/* OP_SPACESHIP * * *` |
|        - |  5782 | ` *` |
|        - |  5783 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5784 | ` *   -1 if left < right` |
|        - |  5785 | ` *    0 if left == right` |
|        - |  5786 | ` *    1 if left > right` |
|        - |  5787 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5788 | ` */` |
|       25 |  5789 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5790 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5791 | `#ifdef UNTRUST` |
|        - |  5792 | `	if( pNos < pStack ){` |
|        - |  5793 | `		goto Abort;` |
|        - |  5794 | `	}` |
|        - |  5795 | `#endif` |
|       51 |  5796 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5797 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5798 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5799 | `		rc = 1;` |
|        4 |  5800 | `	}else{` |
|        - |  5801 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5802 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5803 | `	}` |
|       51 |  5804 | `	VmPopOperand(&pTos,1);` |
|       51 |  5805 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5806 | `	pTos->x.iVal = rc;` |
|       51 |  5807 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5808 | `	break;` |
|        - |  5809 | `				}` |
|        - |  5810 | `/* OP_SEQ P1 P2 *` |
|        - |  5811 | ` * Strict string comparison.` |
|        - |  5812 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5813 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5814 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5815 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5816 | ` * use PH7_OP_EQ.` |
|        - |  5817 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5818 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5819 | ` */` |
|        - |  5820 | `/* OP_SNE P1 P2 *` |
|        - |  5821 | ` * Strict string comparison.` |
|        - |  5822 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5823 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5824 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5825 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5826 | ` * use PH7_OP_EQ.` |
|        - |  5827 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5828 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5829 | ` */` |
|       18 |  5830 | `case PH7_OP_SEQ:` |
|        - |  5831 | `case PH7_OP_SNE: {` |
|       38 |  5832 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5833 | `	SyString s1,s2;` |
|        - |  5834 | `	/* Perform the comparison and act accordingly */` |
|        - |  5835 | `#ifdef UNTRUST` |
|        - |  5836 | `	if( pNos < pStack ){` |
|        - |  5837 | `		goto Abort;` |
|        - |  5838 | `	}` |
|        - |  5839 | `#endif` |
|        - |  5840 | `	/* Force a string cast */` |
|       38 |  5841 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5842 | `		PH7_MemObjToString(pTos);` |
|        2 |  5843 | `	}` |
|       38 |  5844 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5845 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5846 | `	}` |
|       38 |  5847 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5848 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5849 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5850 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5851 | `		rc = rc != 0;` |
|      ! 0 |  5852 | `	}else{` |
|       38 |  5853 | `		rc = rc == 0;` |
|        - |  5854 | `	}` |
|       38 |  5855 | `	VmPopOperand(&pTos,1);` |
|       38 |  5856 | `	if( !pInstr->iP2 ){` |
|        - |  5857 | `		/* Push comparison result without taking the jump */` |
|       38 |  5858 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5859 | `		pTos->x.iVal = rc;` |
|        - |  5860 | `		/* Invalidate any prior representation */` |
|       38 |  5861 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5862 | `	}else{` |
|      ! 0 |  5863 | `		if( rc ){` |
|        - |  5864 | `			/* Jump to the desired location */` |
|      ! 0 |  5865 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5866 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5867 | `		}` |
|        - |  5868 | `	}` |
|       38 |  5869 | `	break;` |
|        - |  5870 | `				 }` |
|        - |  5871 | `/*` |
|        - |  5872 | ` * OP_LOAD_REF * * *` |
|        - |  5873 | ` * Push the index of a referenced object on the stack.` |
|        - |  5874 | ` */` |
|       57 |  5875 | `case PH7_OP_LOAD_REF: {` |
|        - |  5876 | `	sxu32 nIdx;` |
|        - |  5877 | `#ifdef UNTRUST` |
|        - |  5878 | `	if( pTos < pStack ){` |
|        - |  5879 | `		goto Abort;` |
|        - |  5880 | `	}` |
|        - |  5881 | `#endif` |
|        - |  5882 | `	/* Extract memory object index */` |
|      115 |  5883 | `	nIdx = pTos->nIdx;` |
|      115 |  5884 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5885 | `		/* Nullify the object */` |
|       95 |  5886 | `		PH7_MemObjRelease(pTos);` |
|        - |  5887 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5888 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5889 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5890 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5891 | `	}` |
|      115 |  5892 | `	break;` |
|        - |  5893 | `					  }` |
|        - |  5894 | `/*` |
|        - |  5895 | ` * OP_STORE_REF * * P3` |
|        - |  5896 | ` * Perform an assignment operation by reference.` |
|        - |  5897 | ` */` |
|       16 |  5898 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5899 | `	 SyString sName = { 0 , 0 };` |
|        - |  5900 | `	 VmFrame *pFrameLocal;` |
|        - |  5901 | `	SyHashEntry *pEntry;` |
|        - |  5902 | `	sxu32 nIdx;` |
|        - |  5903 | `#ifdef UNTRUST` |
|        - |  5904 | `	if( pTos < pStack ){` |
|        - |  5905 | `		goto Abort;` |
|        - |  5906 | `	}` |
|        - |  5907 | `#endif` |
|       34 |  5908 | `	if( pInstr->p3 == 0 ){` |
|        - |  5909 | `		char *zName;` |
|        - |  5910 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5911 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5912 | `			/* Force a string cast */` |
|      ! 0 |  5913 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5914 | `		}` |
|      ! 0 |  5915 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5916 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5917 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5918 | `			if( zName ){` |
|      ! 0 |  5919 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5920 | `			}` |
|      ! 0 |  5921 | `		}` |
|      ! 0 |  5922 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5923 | `		pTos--;` |
|      ! 0 |  5924 | `	}else{` |
|       34 |  5925 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5926 | `	}` |
|       34 |  5927 | `	nIdx = pTos->nIdx;` |
|       34 |  5928 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5929 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5930 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5931 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5932 | `		}else{` |
|        - |  5933 | `			ph7_value *pObj;` |
|        - |  5934 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5935 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5936 | `			if( pObj == 0 ){` |
|      ! 0 |  5937 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5938 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5939 | `				goto Abort;` |
|        - |  5940 | `			}` |
|        - |  5941 | `			/* Perform the store operation */` |
|      ! 0 |  5942 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5943 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5944 | `		}` |
|       34 |  5945 | `	}else if( sName.nByte > 0){` |
|       34 |  5946 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5947 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5948 | `		}else{` |
|       34 |  5949 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5950 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5951 | `			/* Query the local frame */` |
|       34 |  5952 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5953 | `			if( pEntry ){` |
|      ! 0 |  5954 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5955 | `			}else{` |
|       34 |  5956 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5957 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5958 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5959 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5960 | `				}` |
|       34 |  5961 | `				if( rc == SXRET_OK ){` |
|       34 |  5962 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5963 | `				}` |
|        - |  5964 | `			}` |
|        - |  5965 | `		}` |
|       16 |  5966 | `	}` |
|       34 |  5967 | `	break;` |
|        - |  5968 | `				 }` |
|        - |  5969 | `/*` |
|        - |  5970 | ` * OP_UPLINK P1 * *` |
|        - |  5971 | ` * Link a variable to the top active VM frame.` |
|        - |  5972 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5973 | ` */` |
|       25 |  5974 | `case PH7_OP_UPLINK: {` |
|       52 |  5975 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5976 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5977 | `		SyString sName;` |
|        - |  5978 | `		/* Perform the link */` |
|      104 |  5979 | `		while( pLink <= pTos ){` |
|       54 |  5980 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5981 | `				/* Force a string cast */` |
|      ! 0 |  5982 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5983 | `			}` |
|       54 |  5984 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5985 | `			if( sName.nByte > 0 ){` |
|       54 |  5986 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5987 | `			}` |
|       54 |  5988 | `			pLink++;` |
|        2 |  5989 | `		}` |
|       25 |  5990 | `	}` |
|       52 |  5991 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5992 | `	break;` |
|        - |  5993 | `					}` |
|        - |  5994 | `/*` |
|        - |  5995 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5996 | ` * Push an exception in the corresponding container so that` |
|        - |  5997 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5998 | ` */` |
|       79 |  5999 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      160 |  6000 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6001 | `	VmFrame *pFrameLocal;` |
|        - |  6002 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      160 |  6003 | `	pException->iFinallyDone = 0;` |
|      160 |  6004 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6005 | `	/* Create the exception frame */` |
|      160 |  6006 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      160 |  6007 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6008 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6009 | `		goto Abort;` |
|        - |  6010 | `	}` |
|        - |  6011 | `	/* Mark the special frame */` |
|      160 |  6012 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      160 |  6013 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6014 | `	/* Point to the frame that trigger the exception */` |
|      160 |  6015 | `	pFrameLocal = pFrameLocal->pParent;` |
|      160 |  6016 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      160 |  6017 | `	pException->pFrame = pFrameLocal;` |
|      160 |  6018 | `	break;` |
|        - |  6019 | `							}` |
|        - |  6020 | `/*` |
|        - |  6021 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6022 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6023 | ` */` |
|       78 |  6024 | `case PH7_OP_POP_EXCEPTION: {` |
|      158 |  6025 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      158 |  6026 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6027 | `		ph7_exception **apException;` |
|        - |  6028 | `		/* Pop the loaded exception */` |
|       28 |  6029 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  6030 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  6031 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  6032 | `		}` |
|       13 |  6033 | `	}` |
|      158 |  6034 | `	pException->pFrame = 0;` |
|        - |  6035 | `	/* Leave the exception frame */` |
|      158 |  6036 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6037 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      158 |  6038 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6039 | `		sxi32 rcFinally;` |
|       20 |  6040 | `		pException->iFinallyDone = 1;` |
|       20 |  6041 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6042 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6043 | `			goto Abort;` |
|        - |  6044 | `		}` |
|        9 |  6045 | `	}` |
|      158 |  6046 | `	break;` |
|        - |  6047 | `							}` |
|        - |  6048 |  |
|        - |  6049 | `/*` |
|        - |  6050 | ` * OP_THROW * P2 *` |
|        - |  6051 | ` * Throw an user exception.` |
|        - |  6052 | ` */` |
|       30 |  6053 | `case PH7_OP_THROW: {` |
|       62 |  6054 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  6055 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6056 | `#ifdef UNTRUST` |
|        - |  6057 | `	if( pTos < pStack ){` |
|        - |  6058 | `		goto Abort;` |
|        - |  6059 | `	}` |
|        - |  6060 | `#endif` |
|       62 |  6061 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6062 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  6063 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  6064 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  6065 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6066 | `		ph7_class *pException;` |
|        - |  6067 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  6068 | `		 */` |
|       62 |  6069 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  6070 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  6071 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  6072 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6073 | `			if( rc == SXERR_ABORT ){` |
|        - |  6074 | `				/* Abort processing immediately */` |
|      ! 0 |  6075 | `				goto Abort;` |
|        - |  6076 | `			}` |
|      ! 0 |  6077 | `		}else{` |
|        - |  6078 | `			/* Throw the exception */` |
|       62 |  6079 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  6080 | `			if( rc == SXERR_ABORT ){` |
|        - |  6081 | `				/* Abort processing immediately */` |
|        9 |  6082 | `				goto Abort;` |
|        - |  6083 | `			}` |
|        - |  6084 | `		}` |
|       28 |  6085 | `	}else{` |
|        - |  6086 | `		/* Expecting a class instance */` |
|      ! 0 |  6087 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6088 | `		if( rc == SXERR_ABORT ){` |
|        - |  6089 | `			/* Abort processing immediately */` |
|      ! 0 |  6090 | `			goto Abort;` |
|        - |  6091 | `		}` |
|        - |  6092 | `	}` |
|        - |  6093 | `	/* Pop the top entry */` |
|       54 |  6094 | `	VmPopOperand(&pTos,1);` |
|        - |  6095 | `	/* Perform an unconditional jump */` |
|       54 |  6096 | `	pc = nJump - 1;` |
|       54 |  6097 | `	break;` |
|        - |  6098 | `				   }` |
|        - |  6099 | `/*` |
|        - |  6100 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6101 | ` * Prepare a foreach step.` |
|        - |  6102 | ` */` |
|     5505 |  6103 | `case PH7_OP_FOREACH_INIT: {` |
|    11012 |  6104 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6105 | `	void *pName;` |
|        - |  6106 | `#ifdef UNTRUST` |
|        - |  6107 | `	if( pTos < pStack ){` |
|        - |  6108 | `		goto Abort;` |
|        - |  6109 | `	}` |
|        - |  6110 | `#endif` |
|    11012 |  6111 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6112 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6113 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6114 | `			/* Force a string cast */` |
|      ! 0 |  6115 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6116 | `		}` |
|        - |  6117 | `		/* Duplicate name */` |
|      ! 0 |  6118 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6119 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6120 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6121 | `		}` |
|      ! 0 |  6122 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6123 | `	}` |
|    11012 |  6124 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6125 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6126 | `			/* Force a string cast */` |
|      ! 0 |  6127 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6128 | `		}` |
|        - |  6129 | `		/* Duplicate name */` |
|      ! 0 |  6130 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6131 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6132 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6133 | `		}` |
|      ! 0 |  6134 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6135 | `	}` |
|        - |  6136 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11012 |  6137 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6138 | `		/* Jump out of the loop */` |
|      ! 0 |  6139 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6140 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6141 | `		}` |
|      ! 0 |  6142 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6143 | `	}else{` |
|        - |  6144 | `		ph7_foreach_step *pStep;` |
|    11012 |  6145 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11012 |  6146 | `		if( pStep == 0 ){` |
|      ! 0 |  6147 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6148 | `			/* Jump out of the loop */` |
|      ! 0 |  6149 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6150 | `		}else{` |
|        - |  6151 | `			/* Zero the structure */` |
|    11012 |  6152 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6153 | `			/* Prepare the step */` |
|    11012 |  6154 | `			pStep->iFlags = pInfo->iFlags;` |
|    11012 |  6155 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6156 | `				ph7_hashmap *pMap;` |
|        - |  6157 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6158 | `				 * source array so mutations don't affect other sharers. */` |
|    10980 |  6159 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6160 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6161 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6162 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6163 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6164 | `						 * variable still points at the same hashmap as` |
|        - |  6165 | `						 * the stack value. */` |
|        9 |  6166 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6167 | `							pCur->iRef--;` |
|        9 |  6168 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6169 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6170 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6171 | `						}` |
|        4 |  6172 | `					}` |
|        4 |  6173 | `				}` |
|    10980 |  6174 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6175 | `				/* Reset the internal loop cursor */` |
|    10980 |  6176 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6177 | `				/* Mark the step */` |
|    10980 |  6178 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10980 |  6179 | `				pStep->xIter.pMap = pMap;` |
|    10980 |  6180 | `				pMap->iRef++;` |
|     5491 |  6181 | `			}else{` |
|       34 |  6182 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6183 | `				ph7_class *pIteratorClass;` |
|        - |  6184 | `				/* Check if the object implements Iterator */` |
|       34 |  6185 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6186 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6187 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6188 | `					ph7_class_method *pRewind;` |
|       24 |  6189 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6190 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6191 | `					pThis->iRef++;` |
|       24 |  6192 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6193 | `					if( pRewind ){` |
|       24 |  6194 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6195 | `					}` |
|       13 |  6196 | `				}else{` |
|        - |  6197 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6198 | `					ph7_class *pIterAggClass;` |
|       12 |  6199 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6200 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6201 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6202 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6203 | `						ph7_class_method *pGetIter;` |
|        3 |  6204 | `						int iterAggOk = 0;` |
|        3 |  6205 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6206 | `						if( pGetIter ){` |
|        - |  6207 | `							ph7_value sResult;` |
|        3 |  6208 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6209 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6210 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6211 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6212 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6213 | `									ph7_class_method *pRewind;` |
|        3 |  6214 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6215 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6216 | `									pIterObj->iRef++;` |
|        - |  6217 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6218 | `									pStep->pOwner = pThis;` |
|        3 |  6219 | `									pThis->iRef++;` |
|        3 |  6220 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6221 | `									if( pRewind ){` |
|        3 |  6222 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6223 | `									}` |
|        3 |  6224 | `									iterAggOk = 1;` |
|        1 |  6225 | `								}` |
|        1 |  6226 | `							}` |
|        3 |  6227 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6228 | `						}` |
|        3 |  6229 | `						if( !iterAggOk ){` |
|        - |  6230 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6231 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6232 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6233 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6234 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6235 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6236 | `						}` |
|        2 |  6237 | `					}else{` |
|        - |  6238 | `						/* Plain object iteration via hAttr */` |
|        9 |  6239 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6240 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6241 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6242 | `						pThis->iRef++;` |
|        - |  6243 | `					}` |
|        - |  6244 | `				}` |
|        - |  6245 | `			}` |
|        - |  6246 | `		}` |
|    11012 |  6247 | `		if( pStep ){` |
|    11012 |  6248 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6249 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6250 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6251 | `				/* Jump out of the loop */` |
|      ! 0 |  6252 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6253 | `			}` |
|     5505 |  6254 | `		}` |
|        - |  6255 | `	}` |
|    11012 |  6256 | `	VmPopOperand(&pTos,1);` |
|    11012 |  6257 | `	break;` |
|        - |  6258 | `						  }` |
|        - |  6259 | `/*` |
|        - |  6260 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6261 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6262 | ` */` |
|    89702 |  6263 | `case PH7_OP_FOREACH_STEP: {` |
|   179406 |  6264 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6265 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6266 | `	ph7_value *pValue;` |
|        - |  6267 | `	VmFrame *pFrameLocal;` |
|        - |  6268 | `	/* Peek the last step */` |
|   179406 |  6269 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   179406 |  6270 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   179406 |  6271 | `	pFrameLocal = pVm->pFrame;` |
|   179406 |  6272 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   179406 |  6273 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   179278 |  6274 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6275 | `		ph7_hashmap_node *pNode;` |
|        - |  6276 | `		/* Extract the current node value */` |
|   179278 |  6277 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   179278 |  6278 | `		if( pNode == 0 ){` |
|        - |  6279 | `			/* No more entry to process */` |
|    10978 |  6280 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10978 |  6281 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6282 | `				/* Break the reference with the last element */` |
|        7 |  6283 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6284 | `			}` |
|        - |  6285 | `			/* Automatically reset the loop cursor */` |
|    10978 |  6286 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6287 | `			/* Cleanup the mess left behind */` |
|    10978 |  6288 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10978 |  6289 | `			SySetPop(&pInfo->aStep);` |
|    10978 |  6290 | `			PH7_HashmapUnref(pMap);` |
|     5490 |  6291 | `		}else{` |
|   168302 |  6292 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6293 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6294 | `				if( pKey ){` |
|      426 |  6295 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6296 | `				}` |
|      212 |  6297 | `			}` |
|   168302 |  6298 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6299 | `				SyHashEntry *pEntry;` |
|        - |  6300 | `				/* Pass by reference */` |
|       23 |  6301 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6302 | `				if( pEntry ){` |
|       21 |  6303 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6304 | `				}else{` |
|        4 |  6305 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6306 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6307 | `				}` |
|       12 |  6308 | `			}else{` |
|        - |  6309 | `				/* Make a copy of the entry value */` |
|   168280 |  6310 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   168280 |  6311 | `				if( pValue ){` |
|   168280 |  6312 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    84139 |  6313 | `				}` |
|        - |  6314 | `			}` |
|        2 |  6315 | `		}` |
|    89768 |  6316 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6317 | `		/* Iterator-based iteration.` |
|        - |  6318 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6319 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6320 | `		 */` |
|      106 |  6321 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6322 | `		ph7_class_method *pMethod;` |
|        - |  6323 | `		ph7_value sResult;` |
|      106 |  6324 | `		int isValid = 0;` |
|        - |  6325 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6326 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6327 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6328 | `		}else{` |
|       82 |  6329 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6330 | `			if( pMethod ){` |
|       82 |  6331 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6332 | `			}` |
|        - |  6333 | `		}` |
|        - |  6334 | `		/* Call valid() */` |
|      106 |  6335 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6336 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6337 | `		if( pMethod ){` |
|      106 |  6338 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6339 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6340 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6341 | `		}` |
|      106 |  6342 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6343 | `		if( !isValid ){` |
|        - |  6344 | `			/* Iterator exhausted */` |
|       24 |  6345 | `			pc = pInstr->iP2 - 1;` |
|        - |  6346 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6347 | `			if( pStep->pOwner ){` |
|        3 |  6348 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6349 | `			}` |
|       24 |  6350 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6351 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6352 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6353 | `		}else{` |
|        - |  6354 | `			/* Call current() to get value */` |
|       84 |  6355 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6356 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6357 | `			if( pMethod ){` |
|       84 |  6358 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6359 | `			}` |
|       84 |  6360 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6361 | `			if( pValue ){` |
|       84 |  6362 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6363 | `			}` |
|       84 |  6364 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6365 | `			/* Call key() if needed */` |
|       84 |  6366 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6367 | `				ph7_value sKey;` |
|       35 |  6368 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6369 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6370 | `				if( pMethod ){` |
|       35 |  6371 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6372 | `				}` |
|       35 |  6373 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6374 | `				if( pValue ){` |
|       35 |  6375 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6376 | `				}` |
|       35 |  6377 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6378 | `			}` |
|        - |  6379 | `		}` |
|       54 |  6380 | `	}else{` |
|       25 |  6381 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6382 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6383 | `		SyHashEntry *pEntry;` |
|        - |  6384 | `		/* Point to the next attribute */` |
|       29 |  6385 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6386 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6387 | `			/* Check access permission */` |
|       31 |  6388 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6389 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6390 | `					break; /* Access is granted */` |
|        - |  6391 | `			}` |
|        1 |  6392 | `		}` |
|       25 |  6393 | `		if( pEntry == 0 ){` |
|        - |  6394 | `			/* Clean up the mess left behind */` |
|        9 |  6395 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6396 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6397 | `				/* Break the reference with the last element */` |
|        3 |  6398 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6399 | `			}` |
|        9 |  6400 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6401 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6402 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6403 | `		}else{` |
|       17 |  6404 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6405 | `			ph7_value *pAttrValue;` |
|       17 |  6406 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6407 | `				/* Fill with the current attribute name */` |
|       17 |  6408 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6409 | `				if( pKey ){` |
|       17 |  6410 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6411 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6412 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6413 | `				}` |
|        8 |  6414 | `			}` |
|        - |  6415 | `			/* Extract attribute value */` |
|       17 |  6416 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6417 | `			if( pAttrValue ){` |
|       17 |  6418 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6419 | `					/* Pass by reference */` |
|        3 |  6420 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6421 | `					if( pEntry ){` |
|        3 |  6422 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6423 | `					}else{` |
|      ! 0 |  6424 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6425 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6426 | `					}` |
|        2 |  6427 | `				}else{` |
|        - |  6428 | `					/* Make a copy of the attribute value */` |
|       15 |  6429 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6430 | `					if( pValue ){` |
|       15 |  6431 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6432 | `					}` |
|        - |  6433 | `				}` |
|        8 |  6434 | `			}` |
|        - |  6435 | `		}` |
|        - |  6436 | `	}` |
|   179406 |  6437 | `	break;` |
|        - |  6438 | `						  }` |
|        - |  6439 | `/*` |
|        - |  6440 | ` * OP_MEMBER P1 P2` |
|        - |  6441 | ` * Load class attribute/method on the stack.` |
|        - |  6442 | ` */` |
|     2816 |  6443 | `case PH7_OP_MEMBER: {` |
|        - |  6444 | `	ph7_class_instance *pThis;` |
|        - |  6445 | `	ph7_value *pNos;` |
|        - |  6446 | `	SyString sName;` |
|     5634 |  6447 | `	if( !pInstr->iP1 ){` |
|     5414 |  6448 | `		pNos = &pTos[-1];` |
|        - |  6449 | `#ifdef UNTRUST` |
|        - |  6450 | `		if( pNos < pStack ){` |
|        - |  6451 | `			goto Abort;` |
|        - |  6452 | `		}` |
|        - |  6453 | `#endif` |
|     5414 |  6454 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6455 | `			ph7_class *pClass;` |
|        - |  6456 | `			/* Class already instantiated */` |
|     5414 |  6457 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6458 | `			/* Point to the instantiated class */` |
|     5414 |  6459 | `			pClass = pThis->pClass;` |
|        - |  6460 | `			/* Extract attribute name first */` |
|     5414 |  6461 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     5414 |  6462 | `			if( pInstr->iP2 ){` |
|        - |  6463 | `				/* Method call */` |
|      566 |  6464 | `				ph7_class_method *pMeth = 0;` |
|      566 |  6465 | `				if( sName.nByte > 0 ){` |
|        - |  6466 | `					/* Extract the target method */` |
|      566 |  6467 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      282 |  6468 | `				}` |
|      566 |  6469 | `				if( pMeth == 0 ){` |
|      ! 0 |  6470 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6471 | `						&pClass->sName,&sName` |
|        - |  6472 | `						);` |
|        - |  6473 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6474 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6475 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6476 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6477 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6478 | `				}else{` |
|        - |  6479 | `					/* Push method name on the stack */` |
|      566 |  6480 | `					PH7_MemObjRelease(pTos);` |
|      566 |  6481 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      566 |  6482 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6483 | `				}` |
|      566 |  6484 | `				pTos->nIdx = SXU32_HIGH;` |
|      284 |  6485 | `			}else{` |
|        - |  6486 | `				/* Attribute access */` |
|     4850 |  6487 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6488 | `				SyHashEntry *pEntry;` |
|        - |  6489 | `				/* Extract the target attribute */` |
|     4850 |  6490 | `				if( sName.nByte > 0 ){` |
|     4850 |  6491 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4850 |  6492 | `					if( pEntry ){` |
|        - |  6493 | `						/* Point to the attribute value */` |
|     4848 |  6494 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2423 |  6495 | `					}` |
|     2424 |  6496 | `				}` |
|     4850 |  6497 | `				if( pObjAttr == 0 ){` |
|        - |  6498 | `					/* No such attribute,load null */` |
|        4 |  6499 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6500 | `						&pClass->sName,&sName);` |
|        - |  6501 | `					/* Call the __get magic method if available */` |
|        3 |  6502 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6503 | `				}` |
|     4850 |  6504 | `				VmPopOperand(&pTos,1);` |
|        - |  6505 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6506 | `				 * This is due to the following case:` |
|        - |  6507 | `				 *     (new TestClass())->foo;` |
|        - |  6508 | `				 */` |
|     4850 |  6509 | `				pThis->iRef++;` |
|     4850 |  6510 | `				PH7_MemObjRelease(pTos);` |
|     4850 |  6511 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4850 |  6512 | `				if( pObjAttr ){` |
|     4848 |  6513 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6514 | `					/* Check attribute access */` |
|     4848 |  6515 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6516 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6517 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6518 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6519 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6520 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     4846 |  6521 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2442 |  6522 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       36 |  6523 | `							VmInstr *pNext = pInstr + 1;` |
|       36 |  6524 | `							int bIsLhs = 0;` |
|       36 |  6525 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       34 |  6526 | `								bIsLhs = 1;` |
|       16 |  6527 | `							}` |
|       36 |  6528 | `							if( !bIsLhs ){` |
|        3 |  6529 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6530 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6531 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6532 | `									goto Abort;` |
|        - |  6533 | `								}` |
|        - |  6534 | `								{` |
|        3 |  6535 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6536 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6537 | `										pc = pFrm2->iExceptionJump - 1;` |
|     2816 |  6538 | `										break;` |
|        - |  6539 | `									}` |
|        - |  6540 | `								}` |
|      ! 0 |  6541 | `								goto Exception;` |
|        - |  6542 | `							}` |
|       16 |  6543 | `						}` |
|        - |  6544 | `						/* Load attribute */` |
|     4846 |  6545 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4846 |  6546 | `						if( pValue ){` |
|     4846 |  6547 | `							if( pThis->iRef < 2 ){` |
|        - |  6548 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6549 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6550 | `								 */` |
|        7 |  6551 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6552 | `							}else{` |
|        - |  6553 | `								/* Simple load */` |
|     4840 |  6554 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6555 | `							}` |
|     4846 |  6556 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4844 |  6557 | `								if( pThis->iRef > 1 ){` |
|        - |  6558 | `									/* Load attribute index */` |
|     4838 |  6559 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2418 |  6560 | `								}` |
|     2421 |  6561 | `							}` |
|     2422 |  6562 | `						}` |
|     2424 |  6563 | `					}else{` |
|        - |  6564 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6565 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6566 | `						char zMsg[256];` |
|      ! 0 |  6567 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6568 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6569 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6570 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6571 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6572 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6573 | `						goto Abort;` |
|        - |  6574 | `					}` |
|     2422 |  6575 | `				}` |
|        - |  6576 | `				/* Safely unreference the object */` |
|     4848 |  6577 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6578 | `			}` |
|     2707 |  6579 | `		}else{` |
|      ! 0 |  6580 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  6581 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6582 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6583 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6584 | `		}` |
|     2707 |  6585 | `	}else{` |
|        - |  6586 | `		/* Static member access using class name */` |
|      222 |  6587 | `		pNos = pTos;` |
|      222 |  6588 | `		pThis = 0;` |
|      222 |  6589 | `		if( !pInstr->p3 ){` |
|      188 |  6590 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      188 |  6591 | `			pNos--;` |
|        - |  6592 | `#ifdef UNTRUST` |
|        - |  6593 | `			if( pNos < pStack ){` |
|        - |  6594 | `				goto Abort;` |
|        - |  6595 | `			}` |
|        - |  6596 | `#endif` |
|       95 |  6597 | `		}else{` |
|        - |  6598 | `			/* Attribute name already computed */` |
|       36 |  6599 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6600 | `		}` |
|      222 |  6601 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      222 |  6602 | `			ph7_class *pClass = 0;` |
|      222 |  6603 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6604 | `				/* Class already instantiated */` |
|        5 |  6605 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6606 | `				pClass = pThis->pClass;` |
|        5 |  6607 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6608 | `			}else{` |
|        - |  6609 | `				/* Try to extract the target class */` |
|      218 |  6610 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      218 |  6611 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      218 |  6612 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6613 | `					/* Handle self/static/parent keywords */` |
|      218 |  6614 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6615 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6616 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6617 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6618 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6619 | `						}` |
|      188 |  6620 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6621 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      157 |  6622 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  6623 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  6624 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  6625 | `							pClass = pSelf->pBase;` |
|       12 |  6626 | `						}` |
|       14 |  6627 | `					}else{` |
|      108 |  6628 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6629 | `					}` |
|      108 |  6630 | `				}` |
|        - |  6631 | `			}` |
|      222 |  6632 | `			if( pClass == 0 ){` |
|        - |  6633 | `				/* Undefined class */` |
|      ! 0 |  6634 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6635 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6636 | `					);` |
|      ! 0 |  6637 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6638 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6639 | `				}` |
|      ! 0 |  6640 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6641 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6642 | `			}else{` |
|      222 |  6643 | `				if( pInstr->iP2 ){` |
|        - |  6644 | `					/* Method call */` |
|       84 |  6645 | `					ph7_class_method *pMeth = 0;` |
|       84 |  6646 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6647 | `						/* Extract the target method */` |
|       84 |  6648 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       41 |  6649 | `					}` |
|       84 |  6650 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6651 | `						if( pMeth ){` |
|      ! 0 |  6652 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6653 | `								&pClass->sName,&sName` |
|        - |  6654 | `								);` |
|      ! 0 |  6655 | `						}else{` |
|      ! 0 |  6656 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6657 | `								&pClass->sName,&sName` |
|        - |  6658 | `								);` |
|        - |  6659 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6660 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6661 | `						}` |
|        - |  6662 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6663 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6664 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6665 | `						}` |
|      ! 0 |  6666 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6667 | `					}else{` |
|        - |  6668 | `						/* Push method name on the stack */` |
|       84 |  6669 | `						PH7_MemObjRelease(pTos);` |
|       84 |  6670 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       84 |  6671 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6672 | `					}` |
|       84 |  6673 | `					pTos->nIdx = SXU32_HIGH;` |
|       43 |  6674 | `				}else{` |
|        - |  6675 | `					/* Attribute access */` |
|      140 |  6676 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6677 | `					/* Check for special ::class pseudo-constant */` |
|      186 |  6678 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6679 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6680 | `						/* ::class returns the fully qualified class name */` |
|        - |  6681 | `						/* Pop the attribute name from the stack */` |
|       60 |  6682 | `						if( !pInstr->p3 ){` |
|       60 |  6683 | `							VmPopOperand(&pTos,1);` |
|       29 |  6684 | `						}` |
|       60 |  6685 | `						PH7_MemObjRelease(pTos);` |
|        - |  6686 | `						/* Load the class name */` |
|       60 |  6687 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6688 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6689 | `					}else{` |
|        - |  6690 | `						/* Extract the target attribute */` |
|       82 |  6691 | `						if( sName.nByte > 0 ){` |
|       82 |  6692 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       40 |  6693 | `						}` |
|       82 |  6694 | `						if( pAttr == 0 ){` |
|        - |  6695 | `							/* No such attribute,load null */` |
|      ! 0 |  6696 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6697 | `								&pClass->sName,&sName);` |
|        - |  6698 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6699 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6700 | `						}` |
|        - |  6701 | `						/* Pop the attribute name from the stack */` |
|       82 |  6702 | `						if( !pInstr->p3 ){` |
|       48 |  6703 | `							VmPopOperand(&pTos,1);` |
|       23 |  6704 | `						}` |
|       82 |  6705 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6706 | `						pTos->nIdx = SXU32_HIGH;` |
|       82 |  6707 | `						if( pAttr ){` |
|       82 |  6708 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6709 | `								/* Access to a non static attribute */` |
|      ! 0 |  6710 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6711 | `									&pClass->sName,&pAttr->sName` |
|        - |  6712 | `									);` |
|      ! 0 |  6713 | `							}else{` |
|        - |  6714 | `								ph7_value *pValue;` |
|        - |  6715 | `								/* Check if the access to the attribute is allowed */` |
|       82 |  6716 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6717 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6718 | `									 * Same LHS-of-store peek as the instance path. */` |
|       76 |  6719 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       51 |  6720 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       35 |  6721 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       22 |  6722 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 |  6723 | `										if( pS ){` |
|       24 |  6724 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       24 |  6725 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        5 |  6726 | `												VmInstr *pNext = pInstr + 1;` |
|        5 |  6727 | `												int bIsLhs = 0;` |
|        5 |  6728 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        3 |  6729 | `													bIsLhs = 1;` |
|        1 |  6730 | `												}` |
|        5 |  6731 | `												if( !bIsLhs ){` |
|        3 |  6732 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6733 | `													if( pThis ){` |
|      ! 0 |  6734 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6735 | `													}` |
|        3 |  6736 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6737 | `														goto Abort;` |
|        - |  6738 | `													}` |
|        - |  6739 | `													{` |
|        3 |  6740 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6741 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6742 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6743 | `															break;` |
|        - |  6744 | `														}` |
|        - |  6745 | `													}` |
|      ! 0 |  6746 | `													goto Exception;` |
|        - |  6747 | `												}` |
|        1 |  6748 | `											}` |
|       10 |  6749 | `										}` |
|       10 |  6750 | `									}` |
|        - |  6751 | `									/* Load the desired attribute */` |
|       76 |  6752 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       76 |  6753 | `									if( pValue ){` |
|       76 |  6754 | `										PH7_MemObjLoad(pValue,pTos);` |
|       76 |  6755 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6756 | `											/* Load index number */` |
|       34 |  6757 | `											pTos->nIdx = pAttr->nIdx;` |
|       16 |  6758 | `										}` |
|       37 |  6759 | `									}` |
|       39 |  6760 | `								}else{` |
|        - |  6761 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6762 | `									char zMsg[256];` |
|        5 |  6763 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6764 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6765 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6766 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6767 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6768 | `									}else{` |
|      ! 0 |  6769 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6770 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6771 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6772 | `									}` |
|        5 |  6773 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6774 | `									goto Abort;` |
|        - |  6775 | `								}` |
|        - |  6776 | `							}` |
|       37 |  6777 | `						}` |
|        - |  6778 | `					}` |
|        - |  6779 | `				}` |
|      216 |  6780 | `				if( pThis ){` |
|        - |  6781 | `					/* Safely unreference the object */` |
|        5 |  6782 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6783 | `				}` |
|        - |  6784 | `			}` |
|      109 |  6785 | `		}else{` |
|        - |  6786 | `			/* Pop operands */` |
|      ! 0 |  6787 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6788 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6789 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6790 | `			}` |
|      ! 0 |  6791 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6792 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6793 | `		}` |
|        - |  6794 | `	}` |
|     5626 |  6795 | `	break;` |
|        - |  6796 | `					}` |
|        - |  6797 | `/*` |
|        - |  6798 | ` * OP_NEW P1 * * *` |
|        - |  6799 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6800 | ` */` |
|      424 |  6801 | `case PH7_OP_NEW: {` |
|      850 |  6802 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      850 |  6803 | `	ph7_class *pClass = 0;` |
|        - |  6804 | `	ph7_class_instance *pNew;` |
|      850 |  6805 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6806 | `		/* Try to extract the desired class */` |
|     1274 |  6807 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      848 |  6808 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      424 |  6809 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6810 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6811 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6812 | `	}` |
|      850 |  6813 | `	if( pClass == 0 ){` |
|        - |  6814 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6815 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6816 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6817 | `			);` |
|        - |  6818 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6819 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6820 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6821 | `			/* Pop given arguments */` |
|      ! 0 |  6822 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6823 | `		}` |
|      ! 0 |  6824 | `		goto Abort;` |
|      ! 0 |  6825 | `	}else{` |
|        - |  6826 | `		ph7_class_method *pCons;` |
|        - |  6827 | `		/* Create a new class instance */` |
|      850 |  6828 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      850 |  6829 | `		if( pNew == 0 ){` |
|      ! 0 |  6830 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6831 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6832 | `				&pClass->sName` |
|        - |  6833 | `			);` |
|      ! 0 |  6834 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6835 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6836 | `				/* Pop given arguments */` |
|      ! 0 |  6837 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6838 | `			}` |
|      ! 0 |  6839 | `			break;` |
|        - |  6840 | `		}` |
|        - |  6841 | `		/* Check if a constructor is available */` |
|      850 |  6842 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      850 |  6843 | `		if( pCons == 0 ){` |
|      696 |  6844 | `			SyString *pName = &pClass->sName;` |
|        - |  6845 | `			/* Check for a constructor with the same base class name */` |
|      696 |  6846 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      347 |  6847 | `		}` |
|      850 |  6848 | `		if( pCons ){` |
|        - |  6849 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  6850 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  6851 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  6852 | `			 * (including variadic string-key packing). */` |
|      156 |  6853 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      156 |  6854 | `			SySetReset(&aArg);` |
|      318 |  6855 | `			while( pArg < pTos ){` |
|      164 |  6856 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      164 |  6857 | `				pArg++;` |
|        2 |  6858 | `			}` |
|      156 |  6859 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  6860 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6861 | `				sxu32 n;` |
|       57 |  6862 | `				n = SySetUsed(&aArg);` |
|        - |  6863 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  6864 | `				 * for named args the missing-arg check happens downstream` |
|        - |  6865 | `				 * after resolution). */` |
|      101 |  6866 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6867 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6868 | `					if( pFuncArg ){` |
|       45 |  6869 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6870 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6871 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6872 | `						}` |
|       22 |  6873 | `					}` |
|       45 |  6874 | `					n++;` |
|        1 |  6875 | `				}` |
|       28 |  6876 | `			}` |
|      156 |  6877 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  6878 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      156 |  6879 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6880 | `				pNew->iRef = 1;` |
|      ! 0 |  6881 | `			}` |
|       77 |  6882 | `		}` |
|      850 |  6883 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6884 | `			/* Pop given arguments */` |
|      138 |  6885 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       68 |  6886 | `		}` |
|      850 |  6887 | `		PH7_MemObjRelease(pTos);` |
|      850 |  6888 | `		pTos->x.pOther = pNew;` |
|      850 |  6889 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6890 | `	}` |
|      850 |  6891 | `	break;` |
|        - |  6892 | `				 }` |
|        - |  6893 | `/*` |
|        - |  6894 | ` * OP_CLONE * * *` |
|        - |  6895 | ` * Perfome a clone operation.` |
|        - |  6896 | ` */` |
|       23 |  6897 | `case PH7_OP_CLONE: {` |
|        - |  6898 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6899 | `#ifdef UNTRUST` |
|        - |  6900 | `	if( pTos < pStack ){` |
|        - |  6901 | `		goto Abort;` |
|        - |  6902 | `	}` |
|        - |  6903 | `#endif` |
|        - |  6904 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6905 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6906 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6907 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6908 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6909 | `		break;` |
|        - |  6910 | `	}` |
|        - |  6911 | `	/* Point to the source */` |
|       44 |  6912 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6913 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6914 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6915 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6916 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6917 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6918 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6919 | `		break;` |
|        - |  6920 | `	}` |
|        - |  6921 | `	/* Perform the clone operation */` |
|       44 |  6922 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6923 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6924 | `	if( pClone == 0 ){` |
|      ! 0 |  6925 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6926 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6927 | `	}else{` |
|        - |  6928 | `		/* Load the cloned object */` |
|       44 |  6929 | `		pTos->x.pOther = pClone;` |
|       44 |  6930 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6931 | `	}` |
|       44 |  6932 | `	break;` |
|        - |  6933 | `				   }` |
|        - |  6934 | `/*` |
|        - |  6935 | ` * OP_SWITCH * * P3` |
|        - |  6936 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6937 | ` */` |
|       26 |  6938 | `case PH7_OP_SWITCH: {` |
|       54 |  6939 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6940 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6941 | `	ph7_value sValue,sCaseValue;` |
|        - |  6942 | `	sxu32 n,nEntry;` |
|        - |  6943 | `#ifdef UNTRUST` |
|        - |  6944 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6945 | `		goto Abort;` |
|        - |  6946 | `	}` |
|        - |  6947 | `#endif` |
|        - |  6948 | `	/* Point to the case table  */` |
|       54 |  6949 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6950 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6951 | `	/* Select the appropriate case block to execute */` |
|       54 |  6952 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6953 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6954 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6955 | `		pCase = &aCase[n];` |
|      130 |  6956 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6957 | `		/* Execute the case expression first */` |
|      130 |  6958 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6959 | `		/* Compare the two expression */` |
|      130 |  6960 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6961 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6962 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6963 | `		if( rc == 0 ){` |
|        - |  6964 | `			/* Value match,jump to this block */` |
|       52 |  6965 | `			pc = pCase->nStart - 1;` |
|       52 |  6966 | `			break;` |
|        - |  6967 | `		}` |
|       41 |  6968 | `	}` |
|       54 |  6969 | `	VmPopOperand(&pTos,1);` |
|       54 |  6970 | `	if( n >= nEntry ){` |
|        - |  6971 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6972 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6973 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6974 | `		}else{` |
|        - |  6975 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6976 | `			pc = pSwitch->nOut - 1;` |
|        - |  6977 | `		}` |
|        1 |  6978 | `	}` |
|       54 |  6979 | `	break;` |
|        - |  6980 | `					}` |
|        - |  6981 | `/*` |
|        - |  6982 | ` * OP_YIELD P1 P2 *` |
|        - |  6983 | ` *  Yield a value from a generator function.` |
|        - |  6984 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6985 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6986 | ` */` |
|       34 |  6987 | `case PH7_OP_YIELD: {` |
|        - |  6988 | `	ph7_generator *pGen;` |
|       70 |  6989 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6990 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6991 | `		goto Abort;` |
|        - |  6992 | `	}` |
|       70 |  6993 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  6994 | `	if( pInstr->iP2 ){` |
|        - |  6995 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6996 | `#ifdef UNTRUST` |
|        - |  6997 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6998 | `#endif` |
|        7 |  6999 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7000 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7001 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7002 | `		VmPopOperand(&pTos, 1);` |
|        - |  7003 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7004 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7005 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7006 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7007 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7008 | `			}` |
|        1 |  7009 | `		}` |
|       67 |  7010 | `	}else if( pInstr->iP1 ){` |
|        - |  7011 | `		/* yield $value */` |
|        - |  7012 | `#ifdef UNTRUST` |
|        - |  7013 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7014 | `#endif` |
|       64 |  7015 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7016 | `		VmPopOperand(&pTos, 1);` |
|        - |  7017 | `		/* Auto-increment key */` |
|       64 |  7018 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7019 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7020 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7021 | `	}else{` |
|        - |  7022 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7023 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7024 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7025 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7026 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7027 | `	}` |
|        - |  7028 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7029 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7030 | `	goto Suspend;` |
|        - |  7031 |  |
|        - |  7032 | `/*` |
|        - |  7033 | ` * OP_CALL P1 * *` |
|        - |  7034 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7035 | ` *  function on the stack.` |
|        - |  7036 | ` */` |
|   319856 |  7037 | `case PH7_OP_CALL: {` |
|   639758 |  7038 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7039 | `	ph7_value *pArg;` |
|   639758 |  7040 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   639758 |  7041 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7042 | `	SyHashEntry *pEntry;` |
|        - |  7043 | `	SyString sName;` |
|        - |  7044 | `	/* Extract function name */` |
|   639758 |  7045 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7046 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7047 | `			ph7_value sResult;` |
|      ! 0 |  7048 | `			SySetReset(&aArg);` |
|      ! 0 |  7049 | `			while( pArg < pTos ){` |
|      ! 0 |  7050 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7051 | `				pArg++;` |
|      ! 0 |  7052 | `			}` |
|      ! 0 |  7053 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7054 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7055 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7056 | `			SySetReset(&aArg);` |
|        - |  7057 | `			/* Pop given arguments */` |
|      ! 0 |  7058 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7059 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7060 | `			}` |
|        - |  7061 | `			/* Copy result */` |
|      ! 0 |  7062 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7063 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7064 | `		}else{` |
|        3 |  7065 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7066 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7067 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7068 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7069 | `			}else{` |
|        - |  7070 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7071 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7072 | `			}` |
|        - |  7073 | `			/* Pop given arguments */` |
|        3 |  7074 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7075 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7076 | `			}` |
|        - |  7077 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7078 | `			PH7_MemObjRelease(pTos);` |
|        - |  7079 | `		}` |
|   319575 |  7080 | `		break;` |
|        - |  7081 | `	}` |
|   639756 |  7082 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7083 | `	/* Check for a compiled function first.` |
|        - |  7084 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7085 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   639756 |  7086 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7087 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7088 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7089 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7090 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7091 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7092 | `	{` |
|   639756 |  7093 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   639756 |  7094 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7095 | `		const char *zFunc;` |
|        - |  7096 | `		const char *zEnd;` |
|        - |  7097 | `		const char *z;` |
|        - |  7098 | `		SyString sGlobal;` |
|       20 |  7099 | `		zFunc = sName.zString;` |
|       20 |  7100 | `		zEnd  = zFunc + sName.nByte;` |
|       20 |  7101 | `		z = zEnd;` |
|        - |  7102 | `		/* Find last namespace separator */` |
|      174 |  7103 | `		while( z > zFunc ){` |
|      174 |  7104 | `			if( z[-1] == '\\' ){` |
|       20 |  7105 | `				break;` |
|        - |  7106 | `			}` |
|      156 |  7107 | `			z--;` |
|        2 |  7108 | `		}` |
|       20 |  7109 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7110 | `			/* Retry lookup using the unqualified/global function name */` |
|       20 |  7111 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       20 |  7112 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        9 |  7113 | `		}` |
|        9 |  7114 | `	}` |
|        - |  7115 | `	} /* end VmCallArgMap namespace scope */` |
|   639756 |  7116 | `	if( pEntry ){` |
|        - |  7117 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7118 | `		ph7_class_instance *pThis;` |
|        - |  7119 | `		ph7_value *pFrameStack;` |
|        - |  7120 | `		ph7_vm_func *pVmFunc;` |
|        - |  7121 | `		ph7_class *pSelf;` |
|        - |  7122 | `		VmFrame *pFrame;` |
|        - |  7123 | `		ph7_value *pObj;` |
|        - |  7124 | `		VmSlot sArg;` |
|        - |  7125 | `		sxu32 n;` |
|        - |  7126 | `		/* initialize fields */` |
|    14946 |  7127 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    14946 |  7128 | `		pThis = 0;` |
|    14946 |  7129 | `		pSelf = 0;` |
|    14946 |  7130 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7131 | `			ph7_class_method *pMeth;` |
|        - |  7132 | `			/* Class method call */` |
|     2310 |  7133 | `			ph7_value *pTarget = &pTos[-1];` |
|     2310 |  7134 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7135 | `				/* Extract the 'this' pointer */` |
|     2310 |  7136 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7137 | `					/* Instance already loaded */` |
|     2222 |  7138 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2222 |  7139 | `					pThis->iRef++;` |
|     2222 |  7140 | `					pSelf = pThis->pClass;` |
|     1110 |  7141 | `				}` |
|     2310 |  7142 | `				if( pSelf == 0 ){` |
|       90 |  7143 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7144 | `						/* "Late Static Binding" class name */` |
|      125 |  7145 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       41 |  7146 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       41 |  7147 | `					}` |
|       90 |  7148 | `					if( pSelf == 0 ){` |
|       19 |  7149 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  7150 | `					}` |
|       44 |  7151 | `				}` |
|     2310 |  7152 | `				if( pThis == 0  ){` |
|       90 |  7153 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       90 |  7154 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       90 |  7155 | `					if( pFrameLocal->pParent ){` |
|        - |  7156 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  7157 | `						pThis = pFrameLocal->pThis;` |
|       64 |  7158 | `						if( pThis ){` |
|       19 |  7159 | `							pThis->iRef++;` |
|        9 |  7160 | `						}` |
|       31 |  7161 | `					}` |
|       44 |  7162 | `				}` |
|     2310 |  7163 | `				VmPopOperand(&pTos,1);` |
|     2310 |  7164 | `				PH7_MemObjRelease(pTos);` |
|        - |  7165 | `				/* Synchronize pointers */` |
|     2310 |  7166 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7167 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7168 | `				 * user have already computed the random generated unique class method name` |
|        - |  7169 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7170 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7171 | `				 */` |
|     2310 |  7172 | `				while( pArg < pStack ){` |
|      ! 0 |  7173 | `					pArg++;` |
|      ! 0 |  7174 | `				}` |
|     2310 |  7175 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7176 | `					/* Check if the call is allowed */` |
|     2310 |  7177 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2310 |  7178 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7179 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7180 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7181 | `							char zMsg[256];` |
|      ! 0 |  7182 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7183 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7184 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7185 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7186 | `							/* Pop given arguments */` |
|      ! 0 |  7187 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7188 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7189 | `							}` |
|      ! 0 |  7190 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7191 | `							goto Abort;` |
|        - |  7192 | `						}` |
|        6 |  7193 | `					}` |
|     1154 |  7194 | `				}` |
|     1154 |  7195 | `			}` |
|     1154 |  7196 | `		}` |
|        - |  7197 | `		/* Check The recursion limit */` |
|    14946 |  7198 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7199 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7200 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7201 | `				&pVmFunc->sName);` |
|        - |  7202 | `			/* Pop given arguments */` |
|        3 |  7203 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7204 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7205 | `			}` |
|        - |  7206 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7207 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7208 | `			break;` |
|        - |  7209 | `		}` |
|    14944 |  7210 | `		if( pVmFunc->pNextName ){` |
|        - |  7211 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7212 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7213 | `		}` |
|    14944 |  7214 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7215 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7216 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7217 | `			ph7_generator *pGenerator;` |
|        - |  7218 | `			ph7_class_instance *pGenObj;` |
|        - |  7219 | `			ph7_value *pCtxAttr;` |
|        - |  7220 | `			SyString sAttrName;` |
|        - |  7221 | `			ph7_value **apCallArgs;` |
|        - |  7222 | `			int nGenArgs, iArg;` |
|        - |  7223 | `			/* Collect arguments from the operand stack */` |
|       24 |  7224 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7225 | `			apCallArgs = 0;` |
|       24 |  7226 | `			if( nGenArgs > 0 ){` |
|       14 |  7227 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7228 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7229 | `				if( apCallArgs == 0 ){` |
|        - |  7230 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7231 | `					nGenArgs = 0;` |
|      ! 0 |  7232 | `				}else{` |
|       10 |  7233 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7234 | `					int didReorder = 0;` |
|       10 |  7235 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7236 | `						/* Named-argument reordering for generator */` |
|        5 |  7237 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7238 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7239 | `						sxu32 nNV = nF;` |
|        5 |  7240 | `						sxi32 iVIdx = -1;` |
|        - |  7241 | `						sxi32 *aGSlot;` |
|        - |  7242 | `						sxu8 *aGUsed;` |
|        - |  7243 | `						sxu32 gi;` |
|       13 |  7244 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7245 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7246 | `						}` |
|        7 |  7247 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7248 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7249 | `						if( aGSlot ){` |
|        5 |  7250 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7251 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7252 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7253 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7254 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7255 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7256 | `								goto Abort;` |
|        - |  7257 | `							}` |
|        - |  7258 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7259 | `							 * append overflow (variadic / positional beyond` |
|        - |  7260 | `							 * formals) so downstream sees every argument. */` |
|        - |  7261 | `							{` |
|        5 |  7262 | `								int nOut = 0;` |
|       13 |  7263 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7264 | `									sxu32 gj;` |
|       13 |  7265 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7266 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7267 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7268 | `											break;` |
|        - |  7269 | `										}` |
|        3 |  7270 | `									}` |
|        5 |  7271 | `								}` |
|       13 |  7272 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7273 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7274 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7275 | `									}` |
|        5 |  7276 | `								}` |
|        5 |  7277 | `								nGenArgs = nOut;` |
|        - |  7278 | `							}` |
|        5 |  7279 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7280 | `							didReorder = 1;` |
|        2 |  7281 | `						}` |
|        - |  7282 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7283 | `						 * positional fill below — preserves arg order rather` |
|        - |  7284 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7285 | `					}` |
|       10 |  7286 | `					if( !didReorder ){` |
|       12 |  7287 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7288 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7289 | `						}` |
|        2 |  7290 | `					}` |
|        - |  7291 | `				}` |
|        4 |  7292 | `			}` |
|        - |  7293 | `			/* Create execution context and generator wrapper */` |
|       24 |  7294 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7295 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7296 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7297 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7298 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7299 | `				break;` |
|        - |  7300 | `			}` |
|       24 |  7301 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7302 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7303 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7304 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7305 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7306 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7307 | `				break;` |
|        - |  7308 | `			}` |
|        - |  7309 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7310 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7311 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7312 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7313 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7314 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7315 | `			if( apCallArgs ){` |
|       10 |  7316 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7317 | `			}` |
|       24 |  7318 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7319 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7320 | `				if( pThis ){` |
|      ! 0 |  7321 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7322 | `				}` |
|      ! 0 |  7323 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7324 | `					goto Abort;` |
|        - |  7325 | `				}` |
|      ! 0 |  7326 | `				break;` |
|        - |  7327 | `			}` |
|        - |  7328 | `			/* Create Generator class instance */` |
|       24 |  7329 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7330 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7331 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7332 | `				break;` |
|        - |  7333 | `			}` |
|        - |  7334 | `			/* Store generator in __ctx attribute */` |
|       24 |  7335 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7336 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7337 | `			if( pCtxAttr ){` |
|       24 |  7338 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  7339 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  7340 | `			}` |
|        - |  7341 | `			/* Pop args and function name, push Generator object */` |
|       24 |  7342 | `			PH7_MemObjRelease(pTos);` |
|       24 |  7343 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  7344 | `			pTos->x.pOther = pGenObj;` |
|       24 |  7345 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  7346 | `			pGenObj->iRef++;` |
|       24 |  7347 | `			if( pThis ){` |
|      ! 0 |  7348 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7349 | `			}` |
|       24 |  7350 | `			break;` |
|        - |  7351 | `		}` |
|        - |  7352 | `		/* Extract the formal argument set */` |
|    14922 |  7353 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7354 | `		/* Create a new VM frame  */` |
|    14922 |  7355 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    14922 |  7356 | `		if( rc != SXRET_OK ){` |
|        - |  7357 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7358 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7359 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7360 | `				&pVmFunc->sName);` |
|        - |  7361 | `			/* Pop given arguments */` |
|      ! 0 |  7362 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7363 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7364 | `			}` |
|        - |  7365 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7366 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7367 | `			break;` |
|        - |  7368 | `		}` |
|    14922 |  7369 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7370 | `			/* Install the '$this' variable */` |
|        - |  7371 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2238 |  7372 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2238 |  7373 | `			if( pObj ){` |
|        - |  7374 | `				/* Reflect the change */` |
|     2238 |  7375 | `				pObj->x.pOther = pThis;` |
|     2238 |  7376 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1118 |  7377 | `			}` |
|     1118 |  7378 | `		}` |
|    14922 |  7379 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7380 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7381 | `			/* Install static variables */` |
|      ! 0 |  7382 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7383 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7384 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7385 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7386 | `					/* Initialize the static variables */` |
|      ! 0 |  7387 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7388 | `					if( pObj ){` |
|        - |  7389 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7390 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7391 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7392 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7393 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7394 | `						}` |
|      ! 0 |  7395 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7396 | `					}else{` |
|      ! 0 |  7397 | `						continue;` |
|        - |  7398 | `					}` |
|      ! 0 |  7399 | `				}` |
|        - |  7400 | `				/* Install in the current frame */` |
|      ! 0 |  7401 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7402 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7403 | `			}` |
|      ! 0 |  7404 | `		}` |
|        - |  7405 | `		/* Push arguments in the local frame */` |
|        - |  7406 | `		{` |
|    14922 |  7407 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|    14922 |  7408 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  7409 | `			/* ============================================================` |
|        - |  7410 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  7411 | `			 *` |
|        - |  7412 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  7413 | `			 * or position, then install them in the frame.` |
|        - |  7414 | `			 * ============================================================ */` |
|       90 |  7415 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  7416 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  7417 | `			sxi32 iVariadicIdx = -1;` |
|        - |  7418 | `			sxu32 nNonVariadic;` |
|        - |  7419 | `			sxi32 *aSlot;` |
|        - |  7420 | `			sxu8  *aUsed;` |
|        - |  7421 | `			sxu32 i;` |
|        - |  7422 | `			/* Find variadic parameter index */` |
|      274 |  7423 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  7424 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  7425 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  7426 | `					break;` |
|        - |  7427 | `				}` |
|       94 |  7428 | `			}` |
|       90 |  7429 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  7430 | `			/* Allocate mapping arrays */` |
|      134 |  7431 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  7432 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  7433 | `			if( aSlot == 0 ){` |
|      ! 0 |  7434 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  7435 | `				goto Abort;` |
|        - |  7436 | `			}` |
|       90 |  7437 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  7438 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  7439 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  7440 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  7441 | `			if( rc == PH7_ABORT ){` |
|        7 |  7442 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  7443 | `				goto Abort;` |
|        - |  7444 | `			}` |
|        - |  7445 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  7446 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  7447 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  7448 | `				sxi32 iSrc = -1;` |
|      291 |  7449 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  7450 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  7451 | `						iSrc = (sxi32)i;` |
|      157 |  7452 | `						break;` |
|        - |  7453 | `					}` |
|       59 |  7454 | `				}` |
|      175 |  7455 | `				if( iSrc >= 0 ){` |
|        - |  7456 | `					/* Argument was provided — install with type checking */` |
|      157 |  7457 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  7458 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  7459 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  7460 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  7461 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  7462 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7463 | `					}` |
|        - |  7464 | `					/* Type checking: union types */` |
|      157 |  7465 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  7466 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  7467 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       13 |  7468 | `						if( rcU != SXRET_OK ){` |
|        - |  7469 | `							const char *zGiven;` |
|        - |  7470 | `							char zBuf[128];` |
|      ! 0 |  7471 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7472 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  7473 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7474 | `								zGiven = "null";` |
|      ! 0 |  7475 | `							}else{` |
|      ! 0 |  7476 | `								zGiven = ph7_type_name(pVal);` |
|        - |  7477 | `							}` |
|      ! 0 |  7478 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7479 | `								&aFormalArg[n].sName,` |
|      ! 0 |  7480 | `								SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|      ! 0 |  7481 | `									? aFormalArg[n].sTypeName.zString : "union",` |
|      ! 0 |  7482 | `								zGiven);` |
|      ! 0 |  7483 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7484 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7485 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  7486 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7487 | `							pFrameStack = 0;` |
|      ! 0 |  7488 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  7489 | `							goto SkipFuncBody;` |
|        - |  7490 | `						}` |
|      159 |  7491 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  7492 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  7493 | `						/* Scalar/class type checking */` |
|       17 |  7494 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  7495 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  7496 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  7497 | `							if( pClass ){` |
|      ! 0 |  7498 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7499 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7500 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7501 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7502 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7503 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7504 | `									}` |
|      ! 0 |  7505 | `								}else{` |
|      ! 0 |  7506 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7507 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  7508 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7509 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7510 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7511 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7512 | `									}` |
|        - |  7513 | `								}` |
|      ! 0 |  7514 | `							}` |
|       17 |  7515 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  7516 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  7517 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7518 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  7519 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7520 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7521 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  7522 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7523 | `								pFrameStack = 0;` |
|      ! 0 |  7524 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  7525 | `								goto SkipFuncBody;` |
|      ! 0 |  7526 | `							}else{` |
|        7 |  7527 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        7 |  7528 | `								if( xCast ) xCast(pVal);` |
|        - |  7529 | `							}` |
|        3 |  7530 | `						}` |
|        8 |  7531 | `					}` |
|        - |  7532 | `					/* Install: by reference or by value */` |
|      157 |  7533 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  7534 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7535 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  7536 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7537 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7538 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7539 | `							}` |
|      ! 0 |  7540 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7541 | `						}else{` |
|        7 |  7542 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  7543 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  7544 | `							if( pRefEntry == 0 ){` |
|        7 |  7545 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  7546 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  7547 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  7548 | `								sArg.pUserData = 0;` |
|        5 |  7549 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  7550 | `							}` |
|        5 |  7551 | `							pObj = 0;` |
|        - |  7552 | `						}` |
|        3 |  7553 | `					}else{` |
|      153 |  7554 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7555 | `					}` |
|      157 |  7556 | `					if( pObj ){` |
|      153 |  7557 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  7558 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  7559 | `						sArg.pUserData = 0;` |
|      153 |  7560 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  7561 | `					}` |
|       79 |  7562 | `				}else{` |
|        - |  7563 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  7564 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7565 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  7566 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  7567 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  7568 | `						if( pObj ){` |
|       19 |  7569 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  7570 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  7571 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  7572 | `							sArg.pUserData = 0;` |
|       19 |  7573 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  7574 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  7575 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7576 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7577 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  7578 | `							}` |
|        9 |  7579 | `						}` |
|        9 |  7580 | `					}` |
|        - |  7581 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  7582 | `				}` |
|       88 |  7583 | `			}` |
|        - |  7584 | `			/* Handle variadic parameter */` |
|       83 |  7585 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  7586 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  7587 | `				if( pObj ){` |
|        9 |  7588 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7589 | `					{` |
|        9 |  7590 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  7591 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  7592 | `							if( aSlot[i] == -1 ){` |
|       16 |  7593 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  7594 | `									/* Named variadic entry: insert with string key */` |
|        - |  7595 | `									ph7_value sKey;` |
|       11 |  7596 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  7597 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  7598 | `										pCallMap3->aNames[i].zString,` |
|       10 |  7599 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  7600 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  7601 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  7602 | `								}else{` |
|        - |  7603 | `									/* Positional variadic entry */` |
|      ! 0 |  7604 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  7605 | `								}` |
|        5 |  7606 | `							}` |
|       12 |  7607 | `						}` |
|        - |  7608 | `					}` |
|        9 |  7609 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  7610 | `					sArg.pUserData = 0;` |
|        9 |  7611 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  7612 | `				}` |
|        5 |  7613 | `			}else{` |
|        - |  7614 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  7615 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  7616 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  7617 | `				 * the positional-only path's behavior. */` |
|       75 |  7618 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  7619 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  7620 | `					if( aSlot[i] == -2 ){` |
|        - |  7621 | `						char zAnonBuf[32];` |
|        - |  7622 | `						SyString sAnonName;` |
|      ! 0 |  7623 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  7624 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  7625 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  7626 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  7627 | `						if( pObj ){` |
|      ! 0 |  7628 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  7629 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  7630 | `							sArg.pUserData = 0;` |
|      ! 0 |  7631 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  7632 | `						}` |
|      ! 0 |  7633 | `						nAnon++;` |
|      ! 0 |  7634 | `					}` |
|       73 |  7635 | `				}` |
|        - |  7636 | `			}` |
|        - |  7637 | `			/* Release all stack arguments */` |
|      249 |  7638 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  7639 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  7640 | `			}` |
|       83 |  7641 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  7642 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  7643 | `			n = nFormal;` |
|       42 |  7644 | `		}else{` |
|        - |  7645 | `		/* ============================================================` |
|        - |  7646 | `		 * Positional-only matching path (original)` |
|        - |  7647 | `		 * ============================================================ */` |
|    14834 |  7648 | `		n = 0;` |
|    39812 |  7649 | `		while( pArg < pTos ){` |
|    25042 |  7650 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7651 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  7652 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  7653 | `				if( pObj ){` |
|        - |  7654 | `					/* Initialize as empty array */` |
|       36 |  7655 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7656 | `					{` |
|       36 |  7657 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  7658 | `						while( pArg < pTos ){` |
|        - |  7659 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  7660 | `							 *` |
|        - |  7661 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  7662 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  7663 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  7664 | `							 * non-union variadic path below has the same limitation;` |
|        - |  7665 | `							 * fixing both wants a separate counter for elements` |
|        - |  7666 | `							 * already packed into the variadic array. */` |
|      104 |  7667 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  7668 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  7669 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       16 |  7670 | `								if( rcU != SXRET_OK ){` |
|        - |  7671 | `									const char *zGiven;` |
|        - |  7672 | `									char zBuf[128];` |
|        3 |  7673 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7674 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  7675 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7676 | `										zGiven = "null";` |
|      ! 0 |  7677 | `									}else{` |
|        3 |  7678 | `										zGiven = ph7_type_name(pArg);` |
|        - |  7679 | `									}` |
|        3 |  7680 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  7681 | `										&aFormalArg[n].sName,` |
|        2 |  7682 | `										SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|        2 |  7683 | `											? aFormalArg[n].sTypeName.zString : "union",` |
|        1 |  7684 | `										zGiven);` |
|        3 |  7685 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7686 | `										goto Abort;` |
|        - |  7687 | `									}` |
|        3 |  7688 | `									PH7_MemObjRelease(pTos);` |
|        3 |  7689 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  7690 | `									pFrameStack = 0;` |
|        3 |  7691 | `									rc = PH7_EXCEPTION;` |
|        3 |  7692 | `									goto SkipFuncBody;` |
|        - |  7693 | `								}` |
|       14 |  7694 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  7695 | `								pArg++;` |
|       14 |  7696 | `								continue;` |
|        - |  7697 | `							}` |
|        - |  7698 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7699 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  7700 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7701 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7702 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7703 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7704 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7705 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7706 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7707 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7708 | `										goto Abort;` |
|        - |  7709 | `									}` |
|        - |  7710 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7711 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7712 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7713 | `									pFrameStack = 0;` |
|      ! 0 |  7714 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7715 | `									goto SkipFuncBody;` |
|      ! 0 |  7716 | `								}else{` |
|       13 |  7717 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7718 | `									if( xCast ){` |
|       13 |  7719 | `										xCast(pArg);` |
|        6 |  7720 | `									}` |
|        - |  7721 | `								}` |
|        6 |  7722 | `							}` |
|       90 |  7723 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  7724 | `							pArg++;` |
|        2 |  7725 | `						}` |
|        - |  7726 | `					}` |
|       34 |  7727 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  7728 | `					sArg.pUserData = 0;` |
|       34 |  7729 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  7730 | `				}` |
|       34 |  7731 | `				break; /* All remaining args consumed */` |
|        - |  7732 | `			}` |
|    25008 |  7733 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    24852 |  7734 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       24 |  7735 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7736 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7737 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7738 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7739 | `						goto Abort;` |
|        - |  7740 | `					}` |
|      ! 0 |  7741 | `				}` |
|        - |  7742 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    24854 |  7743 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       77 |  7744 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       50 |  7745 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       52 |  7746 | `					if( rcU != SXRET_OK ){` |
|        - |  7747 | `						const char *zGiven;` |
|        - |  7748 | `						char zBuf[128];` |
|       19 |  7749 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  7750 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  7751 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  7752 | `							zGiven = "null";` |
|        5 |  7753 | `						}else{` |
|        5 |  7754 | `							zGiven = ph7_type_name(pArg);` |
|        - |  7755 | `						}` |
|       19 |  7756 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  7757 | `							&aFormalArg[n].sName,` |
|       18 |  7758 | `							SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|       18 |  7759 | `								? aFormalArg[n].sTypeName.zString : "union",` |
|        9 |  7760 | `							zGiven);` |
|       19 |  7761 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  7762 | `							goto Abort;` |
|        - |  7763 | `						}` |
|       19 |  7764 | `						PH7_MemObjRelease(pTos);` |
|       19 |  7765 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  7766 | `						pFrameStack = 0;` |
|       19 |  7767 | `						rc = PH7_EXCEPTION;` |
|       19 |  7768 | `						goto SkipFuncBody;` |
|        - |  7769 | `					}` |
|       17 |  7770 | `				}else` |
|        - |  7771 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7772 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    24824 |  7773 | `				if( aFormalArg[n].nType > 0` |
|    13018 |  7774 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1210 |  7775 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7776 | `						/* Argument must be a class instance [i.e: object] */` |
|       20 |  7777 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7778 | `						ph7_class *pClass;` |
|        - |  7779 | `						/* Try to extract the desired class */` |
|       20 |  7780 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  7781 | `						if( pClass ){` |
|       20 |  7782 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7783 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7784 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7785 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7786 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7787 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7788 | `								}` |
|      ! 0 |  7789 | `							}else{` |
|        - |  7790 | `								/* reuse pThis declared in outer scope */` |
|       20 |  7791 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7792 | `								/* Make sure the object is an instance of the given class */` |
|       20 |  7793 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7794 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7795 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7796 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7797 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7798 | `								}` |
|        - |  7799 | `							}` |
|       11 |  7800 | `						}` |
|     1201 |  7801 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7802 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7803 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7804 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7805 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7806 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7807 | `								goto Abort;` |
|        - |  7808 | `							}` |
|        - |  7809 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7810 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7811 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7812 | `							pFrameStack = 0;` |
|       11 |  7813 | `							rc = PH7_EXCEPTION;` |
|       11 |  7814 | `							goto SkipFuncBody;` |
|      ! 0 |  7815 | `						}else{` |
|      ! 0 |  7816 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7817 | `							/* Cast to the desired type */` |
|      ! 0 |  7818 | `							xCast(pArg);` |
|        - |  7819 | `						}` |
|      ! 0 |  7820 | `					}` |
|      599 |  7821 | `				}` |
|    24826 |  7822 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  7823 | `					/* Pass by reference */` |
|       54 |  7824 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  7825 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  7826 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  7827 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7828 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7829 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7830 | `						}` |
|        - |  7831 | `						/* Switch to pass by value */` |
|      ! 0 |  7832 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7833 | `					}else{` |
|        - |  7834 | `						SyHashEntry *pRefEntry;` |
|        - |  7835 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  7836 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  7837 | `						if( pRefEntry == 0 ){` |
|       80 |  7838 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  7839 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  7840 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  7841 | `							sArg.pUserData = 0;` |
|       54 |  7842 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  7843 | `						}` |
|       54 |  7844 | `						pObj = 0;` |
|        - |  7845 | `					}` |
|       28 |  7846 | `				}else{` |
|        - |  7847 | `					/* Pass by value,make a copy of the given argument */` |
|    24774 |  7848 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7849 | `				}` |
|    12414 |  7850 | `			}else{` |
|        - |  7851 | `				char zName[32];` |
|        - |  7852 | `				SyString sArgName;` |
|        - |  7853 | `				/* Set a dummy name */` |
|      156 |  7854 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  7855 | `				sArgName.zString = zName;` |
|        - |  7856 | `				/* Annonymous argument */` |
|      156 |  7857 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  7858 | `			}` |
|    24980 |  7859 | `			if( pObj ){` |
|    24928 |  7860 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  7861 | `				/* Insert argument index  */` |
|    24928 |  7862 | `				sArg.nIdx = pObj->nIdx;` |
|    24928 |  7863 | `				sArg.pUserData = 0;` |
|    24928 |  7864 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    12463 |  7865 | `			}` |
|    24980 |  7866 | `			PH7_MemObjRelease(pArg);` |
|    24980 |  7867 | `			pArg++;` |
|    24980 |  7868 | `			++n;` |
|        2 |  7869 | `		}` |
|        - |  7870 | `		} /* end named vs positional branch */` |
|        - |  7871 | `		/* Set up closure environment */` |
|    14886 |  7872 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7873 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  7874 | `			ph7_value *pValue;` |
|        - |  7875 | `			sxu32 iEnv;` |
|      111 |  7876 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      287 |  7877 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      177 |  7878 | `				pEnv = &aEnv[iEnv];` |
|      177 |  7879 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  7880 | `					/* Do not install null value */` |
|      105 |  7881 | `					continue;` |
|        - |  7882 | `				}` |
|       73 |  7883 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  7884 | `				if( pValue == 0 ){` |
|      ! 0 |  7885 | `					continue;` |
|        - |  7886 | `				}` |
|        - |  7887 | `				/* Invalidate any prior representation */` |
|       73 |  7888 | `				PH7_MemObjRelease(pValue);` |
|        - |  7889 | `				/* Duplicate bound variable value */` |
|       73 |  7890 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  7891 | `			}` |
|       55 |  7892 | `		}` |
|        - |  7893 | `		/* Process default values for remaining formal parameters */` |
|    17054 |  7894 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2210 |  7895 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7896 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  7897 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  7898 | `				if( pObj ){` |
|       42 |  7899 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  7900 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  7901 | `					sArg.pUserData = 0;` |
|       42 |  7902 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  7903 | `				}` |
|       42 |  7904 | `				n++;` |
|       42 |  7905 | `				break; /* Variadic is always last */` |
|        - |  7906 | `			}` |
|     2170 |  7907 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2164 |  7908 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2164 |  7909 | `				if( pObj ){` |
|        - |  7910 | `					/* Evaluate the default value and extract it's result */` |
|     2164 |  7911 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2164 |  7912 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7913 | `						goto Abort;` |
|        - |  7914 | `					}` |
|        - |  7915 | `					/* Insert argument index */` |
|     2164 |  7916 | `					sArg.nIdx = pObj->nIdx;` |
|     2164 |  7917 | `					sArg.pUserData = 0;` |
|     2164 |  7918 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  7919 | `					/* Make sure the default argument is of the correct type */` |
|     2162 |  7920 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1504 |  7921 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  7922 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7923 | `						/* Cast to the desired type */` |
|      ! 0 |  7924 | `						xCast(pObj);` |
|      ! 0 |  7925 | `					}` |
|     1081 |  7926 | `				}` |
|     1081 |  7927 | `			}` |
|     2170 |  7928 | `			++n;` |
|        2 |  7929 | `		}` |
|        - |  7930 | `		} /* end VmCallArgMap scope */` |
|        - |  7931 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  7932 | `		 * does not return anything.` |
|        - |  7933 | `		 */` |
|    14886 |  7934 | `		PH7_MemObjRelease(pTos);` |
|    14886 |  7935 | `		pTos = &pTos[-nCallArgs];` |
|        - |  7936 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    14886 |  7937 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    14886 |  7938 | `		if( pFrameStack == 0 ){` |
|        - |  7939 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7940 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7941 | `				&pVmFunc->sName);` |
|      ! 0 |  7942 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7943 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7944 | `			}` |
|      ! 0 |  7945 | `			break;` |
|        - |  7946 | `		}` |
|     7442 |  7947 | `SkipFuncBody:` |
|    14916 |  7948 | `		if( pSelf ){` |
|        - |  7949 | `			/* Push class name */` |
|     2308 |  7950 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1153 |  7951 | `		}` |
|        - |  7952 | `		/* Increment nesting level */` |
|    14916 |  7953 | `		pVm->nRecursionDepth++;` |
|    14916 |  7954 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  7955 | `			/* Execute function body */` |
|    14886 |  7956 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     7442 |  7957 | `		}` |
|        - |  7958 | `		/* Decrement nesting level */` |
|    14916 |  7959 | `		pVm->nRecursionDepth--;` |
|    14916 |  7960 | `		if( pSelf ){` |
|        - |  7961 | `			/* Pop class name */` |
|     2308 |  7962 | `			(void)SySetPop(&pVm->aSelf);` |
|     1153 |  7963 | `		}` |
|        - |  7964 | `		/* Cleanup the mess left behind */` |
|    14916 |  7965 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  7966 | `			/* Return by reference,reflect that */` |
|        9 |  7967 | `			if( n != SXU32_HIGH ){` |
|        9 |  7968 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  7969 | `				sxu32 i;` |
|        - |  7970 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  7971 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  7972 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  7973 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  7974 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7975 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7976 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  7977 | `								&pVmFunc->sName);` |
|      ! 0 |  7978 | `						}` |
|      ! 0 |  7979 | `						n = SXU32_HIGH;` |
|      ! 0 |  7980 | `						break;` |
|        - |  7981 | `					}` |
|        3 |  7982 | `				}` |
|        5 |  7983 | `			}else{` |
|      ! 0 |  7984 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7985 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7986 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  7987 | `						&pVmFunc->sName);` |
|      ! 0 |  7988 | `				}` |
|        - |  7989 | `			}` |
|        9 |  7990 | `			pTos->nIdx = n;` |
|        4 |  7991 | `		}` |
|        - |  7992 | `		/* Cleanup the mess left behind */` |
|    14916 |  7993 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  7994 | `			/* An exception was throw in this frame */` |
|       42 |  7995 | `			pFrame = pFrame->pParent;` |
|       42 |  7996 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  7997 | `				/* Pop the resutlt */` |
|       40 |  7998 | `				VmPopOperand(&pTos,1);` |
|        - |  7999 | `				/* Jump to this destination */` |
|       40 |  8000 | `				pc = pFrame->iExceptionJump - 1;` |
|       40 |  8001 | `				rc = PH7_OK;` |
|       21 |  8002 | `			}else{` |
|        3 |  8003 | `				if( pFrame->pParent ){` |
|        3 |  8004 | `					rc = PH7_EXCEPTION;` |
|        2 |  8005 | `				}else{` |
|        - |  8006 | `					/* Continue normal execution */` |
|      ! 0 |  8007 | `					rc = PH7_OK;` |
|        - |  8008 | `				}` |
|        - |  8009 | `			}` |
|       20 |  8010 | `		}` |
|        - |  8011 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    14916 |  8012 | `		if( pFrameStack ){` |
|    14886 |  8013 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     7442 |  8014 | `		}` |
|        - |  8015 | `		/* Leave the frame */` |
|    14916 |  8016 | `		VmLeaveFrame(&(*pVm));` |
|    14916 |  8017 | `		if( rc == PH7_ABORT ){` |
|        - |  8018 | `			/* Abort processing immeditaley */` |
|        9 |  8019 | `			goto Abort;` |
|    14908 |  8020 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8021 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8022 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8023 | `			 * overwriting the state saved by the inner level.` |
|        - |  8024 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8025 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8026 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8027 | `			goto Suspend;` |
|    14870 |  8028 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8029 | `			goto Exception;` |
|        - |  8030 | `		}` |
|     7435 |  8031 | `	}else{` |
|        - |  8032 | `		ph7_user_func *pFunc;` |
|        - |  8033 | `		ph7_context sCtx;` |
|        - |  8034 | `		ph7_value sRet;` |
|        - |  8035 | `		/* Look for an installed foreign function.` |
|        - |  8036 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8037 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8038 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8039 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   624812 |  8040 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8041 | `		{` |
|   624812 |  8042 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   624812 |  8043 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8044 | `			/* Compiler-qualified: try short name as global fallback */` |
|       20 |  8045 | `			const char *zShort = sName.zString;` |
|        - |  8046 | `			sxu32 i;` |
|      296 |  8047 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      278 |  8048 | `				if( sName.zString[i] == '\\' ){` |
|       24 |  8049 | `					zShort = &sName.zString[i + 1];` |
|       11 |  8050 | `				}` |
|      140 |  8051 | `			}` |
|       20 |  8052 | `			if( zShort != sName.zString ){` |
|       20 |  8053 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       20 |  8054 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        9 |  8055 | `			}` |
|        9 |  8056 | `		}` |
|        - |  8057 | `		} /* end VmCallArgMap namespace scope */` |
|   624812 |  8058 | `		if( pEntry == 0 ){` |
|        - |  8059 | `			/* Call to undefined function */` |
|        5 |  8060 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8061 | `			/* Pop given arguments */` |
|        5 |  8062 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8063 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8064 | `			}` |
|        - |  8065 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8066 | `			PH7_MemObjRelease(pTos);` |
|        8 |  8067 | `			break;` |
|        - |  8068 | `		}` |
|   624808 |  8069 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8070 | `		/* Start collecting function arguments */` |
|   624808 |  8071 | `		SySetReset(&aArg);` |
|  1680566 |  8072 | `		while( pArg < pTos ){` |
|  1055760 |  8073 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1055760 |  8074 | `			pArg++;` |
|        2 |  8075 | `		}` |
|        - |  8076 | `		/* Assume a null return value */` |
|   624808 |  8077 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8078 | `		/* Init the call context */` |
|   624808 |  8079 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8080 | `		/* Call the foreign function */` |
|   624808 |  8081 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8082 | `		/* Release the call context */` |
|   624808 |  8083 | `		VmReleaseCallContext(&sCtx);` |
|   624808 |  8084 | `		if( rc == PH7_ABORT ){` |
|      471 |  8085 | `			goto Abort;` |
|   624338 |  8086 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  8087 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  8088 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  8089 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8090 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8091 | `				goto Exception;` |
|        - |  8092 | `			}` |
|        - |  8093 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  8094 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  8095 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8096 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8097 | `			}` |
|        - |  8098 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  8099 | `			VmPopOperand(&pTos,1);` |
|        - |  8100 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  8101 | `			pFrm = pVm->pFrame;` |
|        7 |  8102 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  8103 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  8104 | `			}` |
|        7 |  8105 | `			break;` |
|        - |  8106 | `		}` |
|   624328 |  8107 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8108 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8109 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8110 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8111 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8112 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8113 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8114 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8115 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8116 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8117 | `			}` |
|        - |  8118 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8119 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8120 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8121 | `			goto Suspend;` |
|        - |  8122 | `		}` |
|   624290 |  8123 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8124 | `			/* Pop function name and arguments */` |
|   604534 |  8125 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   302288 |  8126 | `		}` |
|        - |  8127 | `		/* Save foreign function return value */` |
|   624290 |  8128 | `		PH7_MemObjStore(&sRet,pTos);` |
|   624290 |  8129 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8130 | `	}` |
|   639156 |  8131 | `	break;` |
|        - |  8132 | `				  }` |
|        - |  8133 | `/*` |
|        - |  8134 | ` * OP_CONSUME: P1 * *` |
|        - |  8135 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8136 | ` */` |
|    13087 |  8137 | `case PH7_OP_CONSUME: {` |
|    26176 |  8138 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    26176 |  8139 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8140 |  |
|    26176 |  8141 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    26176 |  8142 | `	pCur = pOut;` |
|        - |  8143 | `	/* Start the consume process  */` |
|    52350 |  8144 | `	while( pOut <= pTos ){` |
|        - |  8145 | `		/* Force a string cast */` |
|    26176 |  8146 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      470 |  8147 | `			PH7_MemObjToString(pOut);` |
|      234 |  8148 | `		}` |
|    26176 |  8149 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8150 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8151 | `			/* Invoke the output consumer callback */` |
|    15000 |  8152 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    15000 |  8153 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    15000 |  8154 | `			SyBlobRelease(&pOut->sBlob);` |
|    15000 |  8155 | `			if( rc == SXERR_ABORT ){` |
|        - |  8156 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8157 | `				goto Abort;` |
|        - |  8158 | `			}` |
|     7499 |  8159 | `		}` |
|    26176 |  8160 | `		pOut++;` |
|        2 |  8161 | `	}` |
|    26176 |  8162 | `	pTos = &pCur[-1];` |
|    26174 |  8163 | `	break;` |
|        - |  8164 | `					 }` |
|        - |  8165 |  |
|        - |  8166 | `		} /* Switch() */` |
| 10715284 |  8167 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8168 | `	} /* For(;;) */` |
|    18009 |  8169 | `Done:` |
|    36020 |  8170 | `	SySetRelease(&aArg);` |
|    36020 |  8171 | `	return SXRET_OK;` |
|       72 |  8172 | `Suspend:` |
|      146 |  8173 | `	SySetRelease(&aArg);` |
|      146 |  8174 | `	return PH7_SUSPEND;` |
|      248 |  8175 | `Abort:` |
|      497 |  8176 | `	SySetRelease(&aArg);` |
|     1719 |  8177 | `	while( pTos >= pStack ){` |
|     1223 |  8178 | `		PH7_MemObjRelease(pTos);` |
|     1223 |  8179 | `		pTos--;` |
|        1 |  8180 | `	}` |
|      497 |  8181 | `	return PH7_ABORT;` |
|        3 |  8182 | `Exception:` |
|        8 |  8183 | `	SySetRelease(&aArg);` |
|       22 |  8184 | `	while( pTos >= pStack ){` |
|       16 |  8185 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8186 | `		pTos--;` |
|        2 |  8187 | `	}` |
|        8 |  8188 | `	return PH7_EXCEPTION;` |
|    18334 |  8189 |  |
|        - |  8190 | `/*` |
|        - |  8191 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8192 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8193 | ` * See block-comment on that function for additional information.` |
|        - |  8194 | ` */` |
|    16964 |  8195 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8196 |  |
|        - |  8197 | `	ph7_value *pStack;` |
|        - |  8198 | `	sxi32 rc;` |
|        - |  8199 | `	/* Allocate a new operand stack */` |
|    16966 |  8200 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    16966 |  8201 | `	if( pStack == 0 ){` |
|      ! 0 |  8202 | `		return SXERR_MEM;` |
|        - |  8203 | `	}` |
|        - |  8204 | `	/* Execute the program */` |
|    16966 |  8205 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  8206 | `	/* Free the operand stack */` |
|    16966 |  8207 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8208 | `	/* Execution result */` |
|    16966 |  8209 | `	return rc;` |
|     8484 |  8210 |  |
|        - |  8211 | `/*` |
|        - |  8212 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8213 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8214 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8215 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8216 | ` * execution ends.` |
|        - |  8217 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8218 | ` * additional information.` |
|        - |  8219 | ` */` |
|     2532 |  8220 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8221 |  |
|        - |  8222 | `	VmShutdownCB *pEntry;` |
|        - |  8223 | `	ph7_value *apArg[10];` |
|        - |  8224 | `	sxu32 n,nEntry;` |
|        - |  8225 | `	int i;` |
|        - |  8226 | `	/* Point to the stack of registered callbacks */` |
|     2534 |  8227 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27854 |  8228 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25322 |  8229 | `		apArg[i] = 0;` |
|    12662 |  8230 | `	}` |
|     2536 |  8231 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8232 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8233 | `		if( pEntry ){` |
|        - |  8234 | `			/* Prepare callback arguments if any */` |
|        3 |  8235 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8236 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8237 | `					break;` |
|        - |  8238 | `				}` |
|      ! 0 |  8239 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8240 | `			}` |
|        - |  8241 | `			/* Invoke the callback */` |
|        3 |  8242 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8243 | `			/*` |
|        - |  8244 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8245 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8246 | `			 */` |
|        3 |  8247 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8248 | `			if( pEntry ){` |
|        3 |  8249 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8250 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8251 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8252 | `				}` |
|        1 |  8253 | `			}` |
|        1 |  8254 | `		}` |
|        2 |  8255 | `	}` |
|     2534 |  8256 | `	SySetReset(&pVm->aShutdown);` |
|     2534 |  8257 |  |
|        - |  8258 | `/*` |
|        - |  8259 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8260 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8261 | ` * See block-comment on that function for additional information.` |
|        - |  8262 | ` */` |
|     2540 |  8263 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8264 |  |
|        - |  8265 | `	/* Make sure we are ready to execute this program */` |
|     2542 |  8266 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8267 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8268 | `	}` |
|        - |  8269 | `	/* Set the execution magic number  */` |
|     2542 |  8270 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8271 | `	/* Execute the program */` |
|     2542 |  8272 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  8273 | `	/* Invoke any shutdown callbacks */` |
|     2538 |  8274 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8275 | `	/*` |
|        - |  8276 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8277 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8278 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8279 | `	 */` |
|     2538 |  8280 | `	return SXRET_OK;` |
|     1272 |  8281 |  |
|        - |  8282 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8283 | `/*` |
|        - |  8284 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8285 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8286 | ` */` |
|       46 |  8287 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8288 |  |
|        - |  8289 | `	ph7_exec_ctx *pCtx;` |
|        - |  8290 | `	ph7_value *pStack;` |
|        - |  8291 | `	VmFrame *pFrame;` |
|       48 |  8292 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8293 | `	if( pCtx == 0 ){` |
|      ! 0 |  8294 | `		return 0;` |
|        - |  8295 | `	}` |
|       48 |  8296 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  8297 | `	pCtx->pVm = pVm;` |
|       48 |  8298 | `	pCtx->pFunc = pFunc;` |
|       48 |  8299 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  8300 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  8301 | `	pCtx->pc = 0;` |
|       48 |  8302 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  8303 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  8304 | `	/* Allocate a private operand stack */` |
|       48 |  8305 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  8306 | `	if( pStack == 0 ){` |
|      ! 0 |  8307 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8308 | `		return 0;` |
|        - |  8309 | `	}` |
|       48 |  8310 | `	pCtx->pStack = pStack;` |
|        - |  8311 | `	/* Create a detached frame for the fiber */` |
|       48 |  8312 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  8313 | `	if( pFrame == 0 ){` |
|      ! 0 |  8314 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  8315 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8316 | `		return 0;` |
|        - |  8317 | `	}` |
|       48 |  8318 | `	pCtx->pFrame = pFrame;` |
|       48 |  8319 | `	return pCtx;` |
|       25 |  8320 |  |
|        - |  8321 | `/*` |
|        - |  8322 | ` * Start executing a fiber context for the first time.` |
|        - |  8323 | ` */` |
|       46 |  8324 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  8325 |  |
|        - |  8326 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8327 | `	sxi32 rc;` |
|       48 |  8328 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8329 | `		return SXERR_INVALID;` |
|        - |  8330 | `	}` |
|        - |  8331 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  8332 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  8333 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8334 | `	/* Save and set the active context */` |
|       48 |  8335 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  8336 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  8337 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  8338 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  8339 | `	pVm->nRecursionDepth++;` |
|        - |  8340 | `	/* Execute from the beginning */` |
|       71 |  8341 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  8342 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       48 |  8343 | `	pVm->nRecursionDepth--;` |
|        - |  8344 | `	/* Restore the previous context */` |
|       48 |  8345 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  8346 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8347 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  8348 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  8349 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  8350 | `		if( pResult ){` |
|       24 |  8351 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  8352 | `		}` |
|       46 |  8353 | `		return SXRET_OK;` |
|        - |  8354 | `	}` |
|        - |  8355 | `	/* Detach frame */` |
|        3 |  8356 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  8357 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  8358 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  8359 | `	}` |
|        3 |  8360 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8361 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8362 | `		return PH7_ABORT;` |
|        - |  8363 | `	}` |
|        3 |  8364 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8365 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8366 | `		return PH7_EXCEPTION;` |
|        - |  8367 | `	}` |
|        - |  8368 | `	/* Normal completion */` |
|        3 |  8369 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  8370 | `	if( pResult ){` |
|        3 |  8371 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  8372 | `	}` |
|        3 |  8373 | `	return SXRET_OK;` |
|       25 |  8374 |  |
|        - |  8375 | `/*` |
|        - |  8376 | ` * Resume a suspended fiber context.` |
|        - |  8377 | ` */` |
|       98 |  8378 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  8379 |  |
|        - |  8380 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8381 | `	sxi32 rc;` |
|      100 |  8382 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  8383 | `		return SXERR_INVALID;` |
|        - |  8384 | `	}` |
|        - |  8385 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  8386 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  8387 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  8388 | `	if( pResumeValue ){` |
|       40 |  8389 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  8390 | `	}else{` |
|       62 |  8391 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  8392 | `	}` |
|      100 |  8393 | `	pCtx->nTos++;` |
|        - |  8394 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  8395 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  8396 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8397 | `	/* Save and set the active context */` |
|      100 |  8398 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  8399 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  8400 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  8401 | `	pVm->nRecursionDepth++;` |
|        - |  8402 | `	/* Resume execution from saved PC */` |
|      149 |  8403 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  8404 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|      100 |  8405 | `	pVm->nRecursionDepth--;` |
|        - |  8406 | `	/* Restore the previous context */` |
|      100 |  8407 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  8408 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8409 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  8410 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  8411 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  8412 | `		if( pResult ){` |
|       18 |  8413 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8414 | `		}` |
|       64 |  8415 | `		return SXRET_OK;` |
|        - |  8416 | `	}` |
|        - |  8417 | `	/* Detach frame */` |
|       38 |  8418 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  8419 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  8420 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  8421 | `	}` |
|       38 |  8422 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8423 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8424 | `		return PH7_ABORT;` |
|        - |  8425 | `	}` |
|       38 |  8426 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8427 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8428 | `		return PH7_EXCEPTION;` |
|        - |  8429 | `	}` |
|        - |  8430 | `	/* Normal completion */` |
|       38 |  8431 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  8432 | `	if( pResult ){` |
|       20 |  8433 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8434 | `	}` |
|       38 |  8435 | `	return SXRET_OK;` |
|       51 |  8436 |  |
|        - |  8437 | `/*` |
|        - |  8438 | ` * Release an execution context and all its resources.` |
|        - |  8439 | ` */` |
|        4 |  8440 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8441 |  |
|        5 |  8442 | `	if( pCtx == 0 ){` |
|      ! 0 |  8443 | `		return;` |
|        - |  8444 | `	}` |
|        5 |  8445 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8446 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8447 | `		return;` |
|        - |  8448 | `	}` |
|        5 |  8449 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8450 | `	/* Release values */` |
|        5 |  8451 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8452 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8453 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8454 | `	if( pCtx->pFrame ){` |
|        - |  8455 | `		VmSlot *aSlot;` |
|        - |  8456 | `		sxu32 n;` |
|        - |  8457 | `		/* Free local variables */` |
|        5 |  8458 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8459 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8460 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8461 | `		}` |
|        - |  8462 | `		/* Remove local references */` |
|        5 |  8463 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  8464 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  8465 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  8466 | `		}` |
|        5 |  8467 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  8468 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  8469 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  8470 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  8471 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  8472 | `		pCtx->pFrame = 0;` |
|        2 |  8473 | `	}` |
|        - |  8474 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  8475 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  8476 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  8477 | `	if( pCtx->pStack ){` |
|        5 |  8478 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  8479 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  8480 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  8481 | `				PH7_MemObjRelease(pTos);` |
|        5 |  8482 | `				pTos--;` |
|        1 |  8483 | `			}` |
|        2 |  8484 | `		}` |
|        5 |  8485 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  8486 | `		pCtx->pStack = 0;` |
|        2 |  8487 | `	}` |
|        - |  8488 | `	/* Free the context itself */` |
|        5 |  8489 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  8490 |  |
|        - |  8491 | `/*` |
|        - |  8492 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  8493 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  8494 | ` */` |
|       90 |  8495 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  8496 |  |
|        - |  8497 | `	ph7_class_instance *pThis;` |
|        - |  8498 | `	SyString sAttr;` |
|        - |  8499 | `	ph7_value *pAttr;` |
|       92 |  8500 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8501 | `		return 0;` |
|        - |  8502 | `	}` |
|       92 |  8503 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  8504 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  8505 | `		return 0;` |
|        - |  8506 | `	}` |
|       92 |  8507 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  8508 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  8509 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  8510 | `		return 0;` |
|        - |  8511 | `	}` |
|       62 |  8512 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  8513 |  |
|        - |  8514 | `/*` |
|        - |  8515 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  8516 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  8517 | ` */` |
|       38 |  8518 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8519 |  |
|       40 |  8520 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  8521 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  8522 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8523 | `			"Cannot suspend outside of a fiber");` |
|        - |  8524 | `	}` |
|       40 |  8525 | `	if( nArg > 0 ){` |
|       40 |  8526 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  8527 | `	}else{` |
|      ! 0 |  8528 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  8529 | `	}` |
|       40 |  8530 | `	return PH7_SUSPEND;` |
|       21 |  8531 |  |
|        - |  8532 | `/*` |
|        - |  8533 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  8534 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  8535 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  8536 | ` */` |
|       24 |  8537 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8538 |  |
|        - |  8539 | `	ph7_class_instance *pThis;` |
|        - |  8540 | `	ph7_value *pAttr;` |
|        - |  8541 | `	SyString sAttrName;` |
|       26 |  8542 | `	if( nArg < 2 ){` |
|      ! 0 |  8543 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8544 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  8545 | `	}` |
|       26 |  8546 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8547 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8548 | `			"Fiber::__construct(): invalid $this");` |
|        - |  8549 | `	}` |
|       26 |  8550 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  8551 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  8552 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8553 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  8554 | `	}` |
|        - |  8555 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  8556 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8557 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8558 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  8559 | `	}` |
|        - |  8560 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  8561 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8562 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8563 | `	if( pAttr ){` |
|       26 |  8564 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  8565 | `	}` |
|       26 |  8566 | `	return PH7_OK;` |
|       14 |  8567 |  |
|        - |  8568 | `/*` |
|        - |  8569 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  8570 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  8571 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  8572 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  8573 | ` */` |
|       24 |  8574 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  8575 | `	ph7_class_instance **ppThis)` |
|        2 |  8576 |  |
|       26 |  8577 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8578 | `	ph7_value *pCallable;` |
|        - |  8579 | `	SyString sAttrName;` |
|       26 |  8580 | `	*ppThis = 0;` |
|       26 |  8581 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8582 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  8583 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8584 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  8585 | `		return 0;` |
|        - |  8586 | `	}` |
|       26 |  8587 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8588 | `		/* String callable — look up in user functions with overload support */` |
|        - |  8589 | `		SyString sName;` |
|        - |  8590 | `		SyHashEntry *pEntry;` |
|        - |  8591 | `		ph7_vm_func *pFunc;` |
|       26 |  8592 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  8593 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  8594 | `		if( pEntry == 0 ){` |
|      ! 0 |  8595 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  8596 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  8597 | `			return 0;` |
|        - |  8598 | `		}` |
|       26 |  8599 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  8600 | `		return pFunc;` |
|      ! 0 |  8601 | `	}else{` |
|        - |  8602 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  8603 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8604 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8605 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8606 | `		if( pMethod == 0 ){` |
|      ! 0 |  8607 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8608 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  8609 | `			return 0;` |
|        - |  8610 | `		}` |
|      ! 0 |  8611 | `		*ppThis = pClosure;` |
|      ! 0 |  8612 | `		return &pMethod->sFunc;` |
|        - |  8613 | `	}` |
|       14 |  8614 |  |
|        - |  8615 | `/*` |
|        - |  8616 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  8617 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  8618 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  8619 | ` */` |
|       46 |  8620 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  8621 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  8622 |  |
|       48 |  8623 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  8624 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  8625 | `	sxu32 nFormal, n;` |
|        - |  8626 | `	VmSlot sSlot;` |
|        - |  8627 | `	sxi32 rc;` |
|        - |  8628 | `	/* Install $this for closure/method callables */` |
|       48 |  8629 | `	if( pClosureThis ){` |
|        - |  8630 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  8631 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  8632 | `		if( pObj ){` |
|      ! 0 |  8633 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  8634 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  8635 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  8636 | `		}` |
|      ! 0 |  8637 | `	}` |
|        - |  8638 | `	/* Install static variables */` |
|       48 |  8639 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  8640 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  8641 | `		ph7_value *pVal;` |
|      ! 0 |  8642 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  8643 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  8644 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  8645 | `			if( pVal ){` |
|      ! 0 |  8646 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8647 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8648 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8649 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8650 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8651 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8652 | `				}` |
|      ! 0 |  8653 | `			}` |
|      ! 0 |  8654 | `		}` |
|      ! 0 |  8655 | `	}` |
|        - |  8656 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  8657 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  8658 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  8659 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8660 | `		ph7_value *pObj;` |
|       20 |  8661 | `		if( n < (sxu32)nArg ){` |
|        - |  8662 | `			/* Argument provided — install with type casting */` |
|       20 |  8663 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  8664 | `			if( pObj ){` |
|       20 |  8665 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8666 | `				/* Type casting */` |
|       20 |  8667 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8668 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8669 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8670 | `						if( xCast ){` |
|      ! 0 |  8671 | `							xCast(pObj);` |
|      ! 0 |  8672 | `						}` |
|      ! 0 |  8673 | `					}` |
|      ! 0 |  8674 | `				}` |
|       20 |  8675 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  8676 | `				sSlot.pUserData = 0;` |
|       20 |  8677 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  8678 | `			}` |
|        9 |  8679 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8680 | `			/* Default value */` |
|      ! 0 |  8681 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8682 | `			if( pObj ){` |
|      ! 0 |  8683 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8684 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8685 | `					return rc;` |
|        - |  8686 | `				}` |
|      ! 0 |  8687 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8688 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8689 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8690 | `						if( xCast ){` |
|      ! 0 |  8691 | `							xCast(pObj);` |
|      ! 0 |  8692 | `						}` |
|      ! 0 |  8693 | `					}` |
|      ! 0 |  8694 | `				}` |
|      ! 0 |  8695 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8696 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8697 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8698 | `			}` |
|      ! 0 |  8699 | `		}` |
|       11 |  8700 | `	}` |
|        - |  8701 | `	/* Install closure environment (captured variables) */` |
|       48 |  8702 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8703 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8704 | `		ph7_value *pValue;` |
|        - |  8705 | `		sxu32 iEnv;` |
|        3 |  8706 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8707 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8708 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8709 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8710 | `				continue;` |
|        - |  8711 | `			}` |
|        5 |  8712 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8713 | `			if( pValue == 0 ){` |
|      ! 0 |  8714 | `				continue;` |
|        - |  8715 | `			}` |
|        5 |  8716 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8717 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8718 | `		}` |
|        1 |  8719 | `	}` |
|       48 |  8720 | `	return SXRET_OK;` |
|       25 |  8721 |  |
|        - |  8722 | `/*` |
|        - |  8723 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8724 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8725 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8726 | ` */` |
|       26 |  8727 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8728 |  |
|       28 |  8729 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8730 | `	ph7_class_instance *pThis;` |
|        - |  8731 | `	ph7_class_instance *pClosureThis;` |
|        - |  8732 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8733 | `	ph7_vm_func *pFunc;` |
|        - |  8734 | `	ph7_value sResult;` |
|        - |  8735 | `	ph7_value *pCtxAttr;` |
|        - |  8736 | `	SyString sAttrName;` |
|        - |  8737 | `	sxi32 rc;` |
|       28 |  8738 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8739 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8740 | `	}` |
|       28 |  8741 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8742 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8743 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8744 | `	if( pExecCtx != 0 ){` |
|        3 |  8745 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8746 | `			"Cannot start a fiber that has already been started");` |
|        - |  8747 | `	}` |
|        - |  8748 | `	/* Resolve callable */` |
|       26 |  8749 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8750 | `	if( pFunc == 0 ){` |
|      ! 0 |  8751 | `		return PH7_EXCEPTION;` |
|        - |  8752 | `	}` |
|        - |  8753 | `	/* Create execution context now that we know the function */` |
|       26 |  8754 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8755 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8756 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8757 | `			"Fiber::start(): out of memory");` |
|        - |  8758 | `	}` |
|        - |  8759 | `	/* Store context in $this->__ctx */` |
|       26 |  8760 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8761 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8762 | `	if( pCtxAttr ){` |
|       26 |  8763 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8764 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8765 | `	}` |
|        - |  8766 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8767 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8768 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8769 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8770 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8771 | `	/* Unpack the args array and install into the frame */` |
|        - |  8772 | `	{` |
|       26 |  8773 | `		ph7_value **apValues = 0;` |
|       26 |  8774 | `		int nActual = 0;` |
|       26 |  8775 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8776 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8777 | `			ph7_hashmap_node *pNode;` |
|       26 |  8778 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8779 | `			if( nCount > 0 ){` |
|        3 |  8780 | `				sxu32 idx = 0;` |
|        4 |  8781 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8782 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8783 | `				if( apValues ){` |
|        3 |  8784 | `					pNode = pMap->pFirst;` |
|        7 |  8785 | `					while( pNode && idx < nCount ){` |
|        5 |  8786 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8787 | `						idx++;` |
|        5 |  8788 | `						pNode = pNode->pPrev;` |
|        1 |  8789 | `					}` |
|        3 |  8790 | `					nActual = (int)idx;` |
|        1 |  8791 | `				}` |
|        1 |  8792 | `			}` |
|       12 |  8793 | `		}` |
|       26 |  8794 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8795 | `		if( apValues ){` |
|        3 |  8796 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8797 | `		}` |
|        - |  8798 | `	}` |
|        - |  8799 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8800 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8801 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8802 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8803 | `		return PH7_ABORT;` |
|        - |  8804 | `	}` |
|       26 |  8805 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8806 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8807 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8808 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8809 | `		return PH7_ABORT;` |
|        - |  8810 | `	}` |
|       26 |  8811 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8812 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8813 | `		return PH7_EXCEPTION;` |
|        - |  8814 | `	}` |
|       26 |  8815 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8816 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8817 | `	return PH7_OK;` |
|       15 |  8818 |  |
|        - |  8819 | `/*` |
|        - |  8820 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  8821 | ` */` |
|       36 |  8822 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8823 |  |
|       38 |  8824 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8825 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8826 | `	ph7_value sResult;` |
|        - |  8827 | `	ph7_value *pResumeVal;` |
|        - |  8828 | `	sxi32 rc;` |
|       38 |  8829 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8830 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  8831 | `		return PH7_OK;` |
|        - |  8832 | `	}` |
|       38 |  8833 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  8834 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8835 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  8836 | `		return PH7_OK;` |
|        - |  8837 | `	}` |
|       38 |  8838 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8839 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8840 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  8841 | `	}` |
|       36 |  8842 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  8843 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  8844 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  8845 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8846 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8847 | `		return PH7_ABORT;` |
|        - |  8848 | `	}` |
|       36 |  8849 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8850 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8851 | `		return PH7_EXCEPTION;` |
|        - |  8852 | `	}` |
|       36 |  8853 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  8854 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  8855 | `	return PH7_OK;` |
|       20 |  8856 |  |
|        - |  8857 | `/*` |
|        - |  8858 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  8859 | ` */` |
|        6 |  8860 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8861 |  |
|        8 |  8862 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8863 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  8864 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8865 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8866 | `		return PH7_OK;` |
|        - |  8867 | `	}` |
|        8 |  8868 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  8869 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8870 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8871 | `		return PH7_OK;` |
|        - |  8872 | `	}` |
|        8 |  8873 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8874 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8875 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8876 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  8877 | `		}` |
|      ! 0 |  8878 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8879 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  8880 | `	}` |
|        8 |  8881 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  8882 | `	return PH7_OK;` |
|        5 |  8883 |  |
|        - |  8884 | `/*` |
|        - |  8885 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  8886 | ` */` |
|        6 |  8887 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8888 |  |
|        - |  8889 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8890 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8891 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8892 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  8893 | `	return PH7_OK;` |
|        4 |  8894 |  |
|      ! 0 |  8895 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8896 |  |
|        - |  8897 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  8898 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  8899 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8900 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  8901 | `	return PH7_OK;` |
|      ! 0 |  8902 |  |
|        6 |  8903 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8904 |  |
|        - |  8905 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8906 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8907 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8908 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  8909 | `	return PH7_OK;` |
|        4 |  8910 |  |
|        6 |  8911 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8912 |  |
|        - |  8913 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8914 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8915 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8916 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  8917 | `	return PH7_OK;` |
|        4 |  8918 |  |
|        - |  8919 | `/*` |
|        - |  8920 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  8921 | ` */` |
|        4 |  8922 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8923 |  |
|        5 |  8924 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8925 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  8926 | `	if( nArg < 1 ){` |
|      ! 0 |  8927 | `		return PH7_OK;` |
|        - |  8928 | `	}` |
|        5 |  8929 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  8930 | `	if( pExecCtx ){` |
|        5 |  8931 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  8932 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  8933 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  8934 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8935 | `			SyString sAttrName;` |
|        - |  8936 | `			ph7_value *pAttr;` |
|        5 |  8937 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  8938 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  8939 | `			if( pAttr ){` |
|        5 |  8940 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  8941 | `			}` |
|        2 |  8942 | `		}` |
|        2 |  8943 | `	}` |
|        5 |  8944 | `	return PH7_OK;` |
|        3 |  8945 |  |
|        - |  8946 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  8947 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  8948 |  |
|        - |  8949 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8950 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  8951 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8952 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  8953 |  |
|      ! 0 |  8954 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  8955 |  |
|        - |  8956 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8957 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  8958 | `	ph7_exec_ctx *pCtx;` |
|        - |  8959 | `	ph7_vm_func *pFunc;` |
|        - |  8960 | `	ph7_value *pCallable;` |
|        - |  8961 | `	ph7_value *pCtxAttr;` |
|        - |  8962 | `	SyString sAttrName;` |
|        - |  8963 | `	/* Must not already be started */` |
|      ! 0 |  8964 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8965 | `	if( pCtx != 0 ){` |
|      ! 0 |  8966 | `		return SXERR_INVALID;` |
|        - |  8967 | `	}` |
|      ! 0 |  8968 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8969 | `		return SXERR_INVALID;` |
|        - |  8970 | `	}` |
|      ! 0 |  8971 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  8972 | `	/* Get the callable */` |
|      ! 0 |  8973 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  8974 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8975 | `	if( pCallable == 0 ){` |
|      ! 0 |  8976 | `		return SXERR_INVALID;` |
|        - |  8977 | `	}` |
|        - |  8978 | `	/* Resolve callable */` |
|      ! 0 |  8979 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8980 | `		SyString sName;` |
|        - |  8981 | `		SyHashEntry *pEntry;` |
|      ! 0 |  8982 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  8983 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  8984 | `		if( pEntry == 0 ){` |
|      ! 0 |  8985 | `			return SXERR_NOTFOUND;` |
|        - |  8986 | `		}` |
|      ! 0 |  8987 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  8988 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8989 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8990 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8991 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8992 | `		if( pMethod == 0 ){` |
|      ! 0 |  8993 | `			return SXERR_INVALID;` |
|        - |  8994 | `		}` |
|      ! 0 |  8995 | `		pClosureThis = pClosure;` |
|      ! 0 |  8996 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  8997 | `	}else{` |
|      ! 0 |  8998 | `		return SXERR_INVALID;` |
|        - |  8999 | `	}` |
|        - |  9000 | `	/* Create context */` |
|      ! 0 |  9001 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9002 | `	if( pCtx == 0 ){` |
|      ! 0 |  9003 | `		return SXERR_MEM;` |
|        - |  9004 | `	}` |
|        - |  9005 | `	/* Store in __ctx */` |
|      ! 0 |  9006 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9007 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9008 | `	if( pCtxAttr ){` |
|      ! 0 |  9009 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9010 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9011 | `	}` |
|        - |  9012 | `	/* Set up frame with args */` |
|      ! 0 |  9013 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9014 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9015 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9016 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9017 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9018 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9019 |  |
|      ! 0 |  9020 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9021 |  |
|      ! 0 |  9022 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9023 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9024 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9025 |  |
|      ! 0 |  9026 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9027 |  |
|      ! 0 |  9028 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9029 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9030 |  |
|      ! 0 |  9031 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9032 |  |
|      ! 0 |  9033 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9034 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9035 |  |
|      ! 0 |  9036 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9037 |  |
|      ! 0 |  9038 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9039 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9040 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9041 |  |
|        - |  9042 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9043 | `/*` |
|        - |  9044 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9045 | ` */` |
|       22 |  9046 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9047 |  |
|        - |  9048 | `	ph7_generator *pGen;` |
|       24 |  9049 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9050 | `	if( pGen == 0 ){` |
|      ! 0 |  9051 | `		return 0;` |
|        - |  9052 | `	}` |
|       24 |  9053 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9054 | `	pGen->pCtx = pCtx;` |
|       24 |  9055 | `	pGen->iImplicitKey = 0;` |
|       24 |  9056 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9057 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9058 | `	/* Link the generator back to the exec context */` |
|       24 |  9059 | `	pCtx->pPrivate = pGen;` |
|       24 |  9060 | `	return pGen;` |
|       13 |  9061 |  |
|        - |  9062 | `/*` |
|        - |  9063 | ` * Release a generator and its execution context.` |
|        - |  9064 | ` */` |
|      ! 0 |  9065 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9066 |  |
|      ! 0 |  9067 | `	if( pGen == 0 ){` |
|      ! 0 |  9068 | `		return;` |
|        - |  9069 | `	}` |
|      ! 0 |  9070 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9071 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9072 | `	if( pGen->pCtx ){` |
|      ! 0 |  9073 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9074 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9075 | `		pGen->pCtx = 0;` |
|      ! 0 |  9076 | `	}` |
|      ! 0 |  9077 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9078 |  |
|        - |  9079 | `/*` |
|        - |  9080 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9081 | ` */` |
|      236 |  9082 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9083 |  |
|        - |  9084 | `	ph7_class_instance *pThis;` |
|        - |  9085 | `	SyString sAttr;` |
|        - |  9086 | `	ph7_value *pAttr;` |
|      238 |  9087 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9088 | `		return 0;` |
|        - |  9089 | `	}` |
|      238 |  9090 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9091 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9092 | `		return 0;` |
|        - |  9093 | `	}` |
|      238 |  9094 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9095 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9096 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9097 | `		return 0;` |
|        - |  9098 | `	}` |
|      238 |  9099 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9100 |  |
|        - |  9101 | `/*` |
|        - |  9102 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9103 | ` */` |
|       22 |  9104 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9105 |  |
|        - |  9106 | `	ph7_generator *pGen;` |
|        - |  9107 | `	sxi32 rc;` |
|       24 |  9108 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9109 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9110 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9111 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9112 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9113 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9114 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9115 | `	}` |
|       24 |  9116 | `	return PH7_OK;` |
|       13 |  9117 |  |
|        - |  9118 | `/*` |
|        - |  9119 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9120 | ` */` |
|       68 |  9121 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9122 |  |
|        - |  9123 | `	ph7_generator *pGen;` |
|       70 |  9124 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9125 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9126 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9127 | `	return PH7_OK;` |
|       36 |  9128 |  |
|        - |  9129 | `/*` |
|        - |  9130 | ` * Generator::current() — return the last yielded value.` |
|        - |  9131 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9132 | ` */` |
|       68 |  9133 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9134 |  |
|        - |  9135 | `	ph7_generator *pGen;` |
|        - |  9136 | `	sxi32 rc;` |
|       70 |  9137 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9138 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9139 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9140 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9141 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9142 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9143 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9144 | `	}` |
|       70 |  9145 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9146 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9147 | `	}else{` |
|      ! 0 |  9148 | `		ph7_result_null(pCtx);` |
|        - |  9149 | `	}` |
|       70 |  9150 | `	return PH7_OK;` |
|       36 |  9151 |  |
|        - |  9152 | `/*` |
|        - |  9153 | ` * Generator::key() — return the last yielded key.` |
|        - |  9154 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9155 | ` */` |
|       12 |  9156 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9157 |  |
|        - |  9158 | `	ph7_generator *pGen;` |
|        - |  9159 | `	sxi32 rc;` |
|       13 |  9160 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9161 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9162 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9163 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9164 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9165 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9166 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9167 | `	}` |
|       13 |  9168 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9169 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9170 | `	}else{` |
|      ! 0 |  9171 | `		ph7_result_null(pCtx);` |
|        - |  9172 | `	}` |
|       13 |  9173 | `	return PH7_OK;` |
|        7 |  9174 |  |
|        - |  9175 | `/*` |
|        - |  9176 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9177 | ` */` |
|       60 |  9178 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9179 |  |
|        - |  9180 | `	ph7_generator *pGen;` |
|        - |  9181 | `	sxi32 rc;` |
|       62 |  9182 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9183 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9184 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9185 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9186 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9187 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9188 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9189 | `	}else{` |
|      ! 0 |  9190 | `		return PH7_OK;` |
|        - |  9191 | `	}` |
|       62 |  9192 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9193 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9194 | `	return PH7_OK;` |
|       32 |  9195 |  |
|        - |  9196 | `/*` |
|        - |  9197 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9198 | ` */` |
|        4 |  9199 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9200 |  |
|        - |  9201 | `	ph7_generator *pGen;` |
|        - |  9202 | `	ph7_value *pSendVal;` |
|        - |  9203 | `	sxi32 rc;` |
|        5 |  9204 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9205 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9206 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9207 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9208 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9209 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9210 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9211 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9212 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9213 | `	}else{` |
|      ! 0 |  9214 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9215 | `		return PH7_OK;` |
|        - |  9216 | `	}` |
|        5 |  9217 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9218 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9219 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9220 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9221 | `	}else{` |
|        3 |  9222 | `		ph7_result_null(pCtx);` |
|        - |  9223 | `	}` |
|        5 |  9224 | `	return PH7_OK;` |
|        3 |  9225 |  |
|        - |  9226 | `/*` |
|        - |  9227 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9228 | ` *` |
|        - |  9229 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9230 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9231 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9232 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9233 | ` * the exception to the caller.` |
|        - |  9234 | ` */` |
|      ! 0 |  9235 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9236 |  |
|        - |  9237 | `	ph7_generator *pGen;` |
|        - |  9238 | `	const char *zMsg;` |
|        - |  9239 | `	int nLen;` |
|      ! 0 |  9240 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9241 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9242 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9243 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9244 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9245 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9246 | `			"Cannot throw into a closed generator");` |
|        - |  9247 | `	}` |
|        - |  9248 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9249 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9250 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9251 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9252 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9253 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9254 | `	nLen = 0;` |
|      ! 0 |  9255 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9256 | `		/* Try to get the exception's message */` |
|        - |  9257 | `		SyString sAttr;` |
|        - |  9258 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9259 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9260 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9261 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9262 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9263 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9264 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9265 | `		}` |
|      ! 0 |  9266 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9267 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9268 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9269 | `	}` |
|      ! 0 |  9270 | `	(void)nLen;` |
|      ! 0 |  9271 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9272 |  |
|        - |  9273 | `/*` |
|        - |  9274 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9275 | ` */` |
|        2 |  9276 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9277 |  |
|        - |  9278 | `	ph7_generator *pGen;` |
|        3 |  9279 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9280 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9281 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9282 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9283 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9284 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9285 | `	}` |
|        3 |  9286 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9287 | `	return PH7_OK;` |
|        2 |  9288 |  |
|        - |  9289 | `/*` |
|        - |  9290 | ` * Generator::__destruct() — clean up.` |
|        - |  9291 | ` */` |
|      ! 0 |  9292 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9293 |  |
|        - |  9294 | `	ph7_generator *pGen;` |
|      ! 0 |  9295 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  9296 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9297 | `	if( pGen ){` |
|      ! 0 |  9298 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  9299 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9300 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9301 | `			SyString sAttrName;` |
|        - |  9302 | `			ph7_value *pAttr;` |
|      ! 0 |  9303 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9304 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9305 | `			if( pAttr ){` |
|      ! 0 |  9306 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  9307 | `			}` |
|      ! 0 |  9308 | `		}` |
|      ! 0 |  9309 | `	}` |
|      ! 0 |  9310 | `	return PH7_OK;` |
|      ! 0 |  9311 |  |
|        - |  9312 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  9313 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  9314 | `/*` |
|        - |  9315 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  9316 | ` * the desired message.` |
|        - |  9317 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  9318 | ` * in 'api.c' for additional information.` |
|        - |  9319 | ` */` |
|      370 |  9320 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  9321 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  9322 | `	SyString *pString /* Message to output */` |
|        - |  9323 | `	)` |
|        2 |  9324 |  |
|      372 |  9325 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  9326 | `	sxi32 rc = SXRET_OK;` |
|        - |  9327 | `	/* Call the output consumer */` |
|      372 |  9328 | `	if( pString->nByte > 0 ){` |
|      372 |  9329 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  9330 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  9331 | `	}` |
|      372 |  9332 | `	return rc;` |
|        2 |  9333 |  |
|        - |  9334 | `/*` |
|        - |  9335 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  9336 | ` * callback to consume the formatted message.` |
|        - |  9337 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  9338 | ` * in 'api.c' for additional information.` |
|        - |  9339 | ` */` |
|        2 |  9340 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  9341 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  9342 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  9343 | `	va_list ap           /* Variable list of arguments */` |
|        - |  9344 | `	)` |
|        1 |  9345 |  |
|        3 |  9346 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  9347 | `	sxi32 rc = SXRET_OK;` |
|        - |  9348 | `	SyBlob sWorker;` |
|        - |  9349 | `	/* Format the message and call the output consumer */` |
|        3 |  9350 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  9351 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  9352 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  9353 | `		/* Consume the formatted message */` |
|        3 |  9354 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  9355 | `	}` |
|        3 |  9356 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  9357 | `	/* Release the working buffer */` |
|        3 |  9358 | `	SyBlobRelease(&sWorker);` |
|        3 |  9359 | `	return rc;` |
|        1 |  9360 |  |
|        - |  9361 | `/*` |
|        - |  9362 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  9363 | ` * This function never fail and always return a pointer` |
|        - |  9364 | ` * to a null terminated string.` |
|        - |  9365 | ` */` |
|       12 |  9366 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  9367 |  |
|       13 |  9368 | `	const char *zOp = "Unknown     ";` |
|       13 |  9369 | `	switch(nOp){` |
|        3 |  9370 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  9371 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  9372 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  9373 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  9374 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  9375 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  9376 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  9377 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  9378 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  9379 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  9380 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  9381 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  9382 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  9383 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  9384 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  9385 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  9386 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  9387 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  9388 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  9389 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  9390 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  9391 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  9392 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  9393 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  9394 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  9395 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  9396 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  9397 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  9398 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  9399 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  9400 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  9401 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  9402 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  9403 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  9404 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  9405 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  9406 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  9407 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9408 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9409 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9410 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9411 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9412 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9413 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9414 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9415 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9416 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9417 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9418 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9419 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9420 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9421 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9422 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9423 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9424 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9425 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9426 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9427 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9428 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9429 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9430 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9431 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9432 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9433 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9434 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9435 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9436 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9437 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9438 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9439 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9440 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9441 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9442 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9443 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9444 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9445 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9446 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9447 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9448 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9449 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9450 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9451 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9452 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9453 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9454 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9455 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9456 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9457 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9458 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9459 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9460 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9461 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9462 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9463 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9464 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9465 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9466 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9467 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9468 | `	default:` |
|      ! 0 |  9469 | `		break;` |
|        - |  9470 | `	}` |
|       13 |  9471 | `	return zOp;` |
|        1 |  9472 |  |
|        - |  9473 | `/*` |
|        - |  9474 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9475 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9476 | ` * is responsible of consuming the generated dump.` |
|        - |  9477 | ` */` |
|        2 |  9478 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9479 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9480 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9481 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9482 | `	)` |
|        1 |  9483 |  |
|        - |  9484 | `	sxi32 rc;` |
|        3 |  9485 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9486 | `	return rc;` |
|        1 |  9487 |  |
|        - |  9488 | `/*` |
|        - |  9489 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9490 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9491 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9492 | ` * in 'compile.c' for additional information.` |
|        - |  9493 | ` */` |
|       14 |  9494 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9495 |  |
|       15 |  9496 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9497 | `	/* Evaluate and expand constant value */` |
|       15 |  9498 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  9499 |  |
|        - |  9500 | `/*` |
|        - |  9501 | ` * Section:` |
|        - |  9502 | ` *  Function handling functions.` |
|        - |  9503 | ` * Status:` |
|        - |  9504 | ` *    Stable.` |
|        - |  9505 | ` */` |
|        - |  9506 | `/*` |
|        - |  9507 | ` * int func_num_args(void)` |
|        - |  9508 | ` *   Returns the number of arguments passed to the function.` |
|        - |  9509 | ` * Parameters` |
|        - |  9510 | ` *   None.` |
|        - |  9511 | ` * Return` |
|        - |  9512 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  9513 | ` *  or -1 if called from the globe scope.` |
|        - |  9514 | ` */` |
|      944 |  9515 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9516 |  |
|        - |  9517 | `	VmFrame *pFrame;` |
|        - |  9518 | `	ph7_vm *pVm;` |
|        - |  9519 | `	/* Point to the target VM */` |
|      946 |  9520 | `	pVm = pCtx->pVm;` |
|        - |  9521 | `	/* Current frame */` |
|      946 |  9522 | `	pFrame = pVm->pFrame;` |
|      946 |  9523 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  9524 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  9525 | `		SXUNUSED(nArg);` |
|      ! 0 |  9526 | `		SXUNUSED(apArg);` |
|        - |  9527 | `		/* Global frame,return -1 */` |
|      ! 0 |  9528 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  9529 | `		return SXRET_OK;` |
|        - |  9530 | `	}` |
|        - |  9531 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  9532 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  9533 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  9534 | `	return SXRET_OK;` |
|      474 |  9535 |  |
|        - |  9536 | `/*` |
|        - |  9537 | ` * value func_get_arg(int $arg_num)` |
|        - |  9538 | ` *   Return an item from the argument list.` |
|        - |  9539 | ` * Parameters` |
|        - |  9540 | ` *  Argument number(index start from zero).` |
|        - |  9541 | ` * Return` |
|        - |  9542 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  9543 | ` */` |
|       22 |  9544 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9545 |  |
|       24 |  9546 | `	ph7_value *pObj = 0;` |
|       24 |  9547 | `	VmSlot *pSlot = 0;` |
|        - |  9548 | `	VmFrame *pFrame;` |
|        - |  9549 | `	ph7_vm *pVm;` |
|        - |  9550 | `	/* Point to the target VM */` |
|       24 |  9551 | `	pVm = pCtx->pVm;` |
|        - |  9552 | `	/* Current frame */` |
|       24 |  9553 | `	pFrame = pVm->pFrame;` |
|       24 |  9554 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  9555 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  9556 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  9557 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  9558 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9559 | `		return SXRET_OK;` |
|        - |  9560 | `	}` |
|        - |  9561 | `	/* Extract the desired index */` |
|       21 |  9562 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  9563 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  9564 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  9565 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9566 | `		return SXRET_OK;` |
|        - |  9567 | `	}` |
|        - |  9568 | `	/* Extract the desired argument */` |
|       21 |  9569 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  9570 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  9571 | `			/* Return the desired argument */` |
|       21 |  9572 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  9573 | `		}else{` |
|        - |  9574 | `			/* No such argument,return false */` |
|      ! 0 |  9575 | `			ph7_result_bool(pCtx,0);` |
|        - |  9576 | `		}` |
|       11 |  9577 | `	}else{` |
|        - |  9578 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  9579 | `		ph7_result_bool(pCtx,0);` |
|        - |  9580 | `	}` |
|       21 |  9581 | `	return SXRET_OK;` |
|       13 |  9582 |  |
|        - |  9583 | `/*` |
|        - |  9584 | ` * array func_get_args_byref(void)` |
|        - |  9585 | ` *   Returns an array comprising a function's argument list.` |
|        - |  9586 | ` * Parameters` |
|        - |  9587 | ` *  None.` |
|        - |  9588 | ` * Return` |
|        - |  9589 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  9590 | ` *  member of the current user-defined function's argument list.` |
|        - |  9591 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9592 | ` * NOTE:` |
|        - |  9593 | ` *  Arguments are returned to the array by reference.` |
|        - |  9594 | ` */` |
|        2 |  9595 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9596 |  |
|        - |  9597 | `	ph7_value *pArray;` |
|        - |  9598 | `	VmFrame *pFrame;` |
|        - |  9599 | `	VmSlot *aSlot;` |
|        - |  9600 | `	sxu32 n;` |
|        - |  9601 | `	/* Point to the current frame */` |
|        3 |  9602 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  9603 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  9604 | `	if( pFrame->pParent == 0 ){` |
|        - |  9605 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9606 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9607 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9608 | `		return SXRET_OK;` |
|        - |  9609 | `	}` |
|        - |  9610 | `	/* Create a new array */` |
|        3 |  9611 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9612 | `	if( pArray == 0 ){` |
|      ! 0 |  9613 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9614 | `		SXUNUSED(apArg);` |
|      ! 0 |  9615 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9616 | `		return SXRET_OK;` |
|        - |  9617 | `	}` |
|        - |  9618 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  9619 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  9620 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  9621 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  9622 | `	}` |
|        - |  9623 | `	/* Return the freshly created array */` |
|        3 |  9624 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9625 | `	return SXRET_OK;` |
|        2 |  9626 |  |
|        - |  9627 | `/*` |
|        - |  9628 | ` * array func_get_args(void)` |
|        - |  9629 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  9630 | ` * Parameters` |
|        - |  9631 | ` *  None.` |
|        - |  9632 | ` * Return` |
|        - |  9633 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  9634 | ` *  member of the current user-defined function's argument list.` |
|        - |  9635 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9636 | ` */` |
|       88 |  9637 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9638 |  |
|       90 |  9639 | `	ph7_value *pObj = 0;` |
|        - |  9640 | `	ph7_value *pArray;` |
|        - |  9641 | `	VmFrame *pFrame;` |
|        - |  9642 | `	VmSlot *aSlot;` |
|        - |  9643 | `	sxu32 n;` |
|        - |  9644 | `	/* Point to the current frame */` |
|       90 |  9645 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  9646 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  9647 | `	if( pFrame->pParent == 0 ){` |
|        - |  9648 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9649 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9650 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9651 | `		return SXRET_OK;` |
|        - |  9652 | `	}` |
|        - |  9653 | `	/* Create a new array */` |
|       90 |  9654 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9655 | `	if( pArray == 0 ){` |
|      ! 0 |  9656 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9657 | `		SXUNUSED(apArg);` |
|      ! 0 |  9658 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9659 | `		return SXRET_OK;` |
|        - |  9660 | `	}` |
|        - |  9661 | `	/* Start filling the array with the given arguments */` |
|       90 |  9662 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9663 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9664 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9665 | `		if( pObj ){` |
|      134 |  9666 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9667 | `		}` |
|       68 |  9668 | `	}` |
|        - |  9669 | `	/* Return the freshly created array */` |
|       90 |  9670 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9671 | `	return SXRET_OK;` |
|       46 |  9672 |  |
|        - |  9673 | `/*` |
|        - |  9674 | ` * bool function_exists(string $name)` |
|        - |  9675 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9676 | ` * Parameters` |
|        - |  9677 | ` *  The name of the desired function.` |
|        - |  9678 | ` * Return` |
|        - |  9679 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9680 | ` */` |
|     1680 |  9681 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9682 |  |
|        - |  9683 | `	const char *zName;` |
|        - |  9684 | `	ph7_vm *pVm;` |
|        - |  9685 | `	int nLen;` |
|        - |  9686 | `	int res;` |
|     1682 |  9687 | `	if( nArg < 1 ){` |
|        - |  9688 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9689 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9690 | `		return SXRET_OK;` |
|        - |  9691 | `	}` |
|        - |  9692 | `	/* Point to the target VM */` |
|     1682 |  9693 | `	pVm = pCtx->pVm;` |
|        - |  9694 | `	/* Extract the function name */` |
|     1682 |  9695 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9696 | `	/* Assume the function is not defined */` |
|     1682 |  9697 | `	res = 0;` |
|        - |  9698 | `	/* Perform the lookup */` |
|     2520 |  9699 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 |  9700 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9701 | `			/* Function is defined */` |
|      206 |  9702 | `			res = 1;` |
|      102 |  9703 | `	}` |
|     1682 |  9704 | `	ph7_result_bool(pCtx,res);` |
|     1682 |  9705 | `	return SXRET_OK;` |
|      842 |  9706 |  |
|        - |  9707 | `/*` |
|        - |  9708 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9709 | ` * [i.e: Whether it is callable or not].` |
|        - |  9710 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9711 | ` */` |
|    19376 |  9712 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9713 |  |
|    19378 |  9714 | `	int res = 0;` |
|    19378 |  9715 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9716 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9717 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9718 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9719 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9720 | `		if( pMethod && CallInvoke ){` |
|        - |  9721 | `			ph7_value sResult;` |
|        - |  9722 | `			sxi32 rc;` |
|        - |  9723 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9724 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9725 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9726 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9727 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9728 | `			}` |
|      ! 0 |  9729 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9730 | `		}` |
|    19378 |  9731 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9732 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9733 | `		if( pMap->nEntry == 2 ){` |
|        - |  9734 | `			ph7_class *pClass;` |
|        - |  9735 | `			ph7_value *pV;` |
|        - |  9736 | `			/* Extract the target class */` |
|       12 |  9737 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9738 | `			if( pV ){` |
|       12 |  9739 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9740 | `				if( pClass ){` |
|        - |  9741 | `					ph7_class_method *pMethod;` |
|        - |  9742 | `					/* Extract the target method */` |
|       10 |  9743 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9744 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9745 | `						/* Perform the lookup */` |
|       10 |  9746 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9747 | `						if( pMethod ){` |
|        - |  9748 | `							/* Method is callable */` |
|        5 |  9749 | `							res = 1;` |
|        2 |  9750 | `						}` |
|        4 |  9751 | `					}` |
|        4 |  9752 | `				}` |
|        5 |  9753 | `			}` |
|        7 |  9754 | `		}` |
|    19365 |  9755 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9756 | `		const char *zName;` |
|        - |  9757 | `		int nLen;` |
|        - |  9758 | `		/* Extract the name */` |
|     5278 |  9759 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9760 | `		/* Perform the lookup */` |
|     5293 |  9761 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9762 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9763 | `				/* Function is callable */` |
|     5260 |  9764 | `				res = 1;` |
|     2629 |  9765 | `		}` |
|     2638 |  9766 | `	}` |
|    19378 |  9767 | `	return res;` |
|        2 |  9768 |  |
|        - |  9769 | `/*` |
|        - |  9770 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9771 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9772 | ` * Parameters` |
|        - |  9773 | ` * $name` |
|        - |  9774 | ` *    The callback function to check` |
|        - |  9775 | ` * $syntax_only` |
|        - |  9776 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9777 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9778 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9779 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9780 | ` *    a string.` |
|        - |  9781 | ` * Return` |
|        - |  9782 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9783 | ` */` |
|       14 |  9784 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9785 |  |
|        - |  9786 | `	ph7_vm *pVm;` |
|        - |  9787 | `	int res;` |
|       15 |  9788 | `	if( nArg < 1 ){` |
|        - |  9789 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9790 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9791 | `		return SXRET_OK;` |
|        - |  9792 | `	}` |
|        - |  9793 | `	/* Point to the target VM */` |
|       15 |  9794 | `	pVm = pCtx->pVm;` |
|        - |  9795 | `	/* Perform the requested operation */` |
|       15 |  9796 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9797 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9798 | `	return SXRET_OK;` |
|        8 |  9799 |  |
|        - |  9800 | `/*` |
|        - |  9801 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9802 | ` * defined below.` |
|        - |  9803 | ` */` |
|     1200 |  9804 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9805 |  |
|     1201 |  9806 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9807 | `	ph7_value sName;` |
|        - |  9808 | `	sxi32 rc;` |
|        - |  9809 | `	/* Prepare the function name for insertion */` |
|     1201 |  9810 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  9811 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9812 | `	/* Perform the insertion */` |
|     1201 |  9813 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  9814 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  9815 | `	return rc;` |
|        1 |  9816 |  |
|        - |  9817 | `/*` |
|        - |  9818 | ` * array get_defined_functions(void)` |
|        - |  9819 | ` *  Returns an array of all defined functions.` |
|        - |  9820 | ` * Parameter` |
|        - |  9821 | ` *  None.` |
|        - |  9822 | ` * Return` |
|        - |  9823 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  9824 | ` *  both built-in (internal) and user-defined.` |
|        - |  9825 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  9826 | ` *  defined ones using $arr["user"].` |
|        - |  9827 | ` * Note:` |
|        - |  9828 | ` *  NULL is returned on failure.` |
|        - |  9829 | ` */` |
|        2 |  9830 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9831 |  |
|        - |  9832 | `	ph7_value *pArray,*pEntry;` |
|        - |  9833 | `	/* NOTE:` |
|        - |  9834 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  9835 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  9836 | `	 */` |
|        3 |  9837 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9838 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9839 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9840 | `		SXUNUSED(apArg);` |
|        - |  9841 | `		/* Return NULL */` |
|      ! 0 |  9842 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9843 | `		return SXRET_OK;` |
|        - |  9844 | `	}` |
|        3 |  9845 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9846 | `	if( pEntry == 0 ){` |
|        - |  9847 | `		/* Return NULL */` |
|      ! 0 |  9848 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9849 | `		return SXRET_OK;` |
|        - |  9850 | `	}` |
|        - |  9851 | `	/* Fill with the appropriate information */` |
|        3 |  9852 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  9853 | `	/* Create the 'internal' index */` |
|        3 |  9854 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  9855 | `	/* Create the user-func array */` |
|        3 |  9856 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9857 | `	if( pEntry == 0 ){` |
|        - |  9858 | `		/* Return NULL */` |
|      ! 0 |  9859 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9860 | `		return SXRET_OK;` |
|        - |  9861 | `	}` |
|        - |  9862 | `	/* Fill with the appropriate information */` |
|        3 |  9863 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  9864 | `	/* Create the 'user' index */` |
|        3 |  9865 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  9866 | `	/* Return the multi-dimensional array */` |
|        3 |  9867 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9868 | `	return SXRET_OK;` |
|        2 |  9869 |  |
|        - |  9870 | `/*` |
|        - |  9871 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  9872 | ` *  Register a function for execution on shutdown.` |
|        - |  9873 | ` * Note` |
|        - |  9874 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  9875 | ` *  be called in the same order as they were registered.` |
|        - |  9876 | ` * Parameters` |
|        - |  9877 | ` *  $callback` |
|        - |  9878 | ` *   The shutdown callback to register.` |
|        - |  9879 | ` * $param` |
|        - |  9880 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  9881 | ` * Return` |
|        - |  9882 | ` *  Nothing.` |
|        - |  9883 | ` */` |
|        2 |  9884 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9885 |  |
|        - |  9886 | `	VmShutdownCB sEntry;` |
|        - |  9887 | `	int i,j;` |
|        3 |  9888 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9889 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  9890 | `		return PH7_OK;` |
|        - |  9891 | `	}` |
|        - |  9892 | `	/* Zero the Entry */` |
|        3 |  9893 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  9894 | `	/* Initialize fields */` |
|        3 |  9895 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  9896 | `	/* Save the callback name for later invocation name */` |
|        3 |  9897 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  9898 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  9899 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  9900 | `	}` |
|        - |  9901 | `	/* Copy arguments */` |
|        3 |  9902 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  9903 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  9904 | `			/* Limit reached */` |
|      ! 0 |  9905 | `			break;` |
|        - |  9906 | `		}` |
|      ! 0 |  9907 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  9908 | `	}` |
|        3 |  9909 | `	sEntry.nArg = j;` |
|        - |  9910 | `	/* Install the callback */` |
|        3 |  9911 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  9912 | `	return PH7_OK;` |
|        2 |  9913 |  |
|        - |  9914 | `/*` |
|        - |  9915 | ` * Section:` |
|        - |  9916 | ` *  Class handling functions.` |
|        - |  9917 | ` * Status:` |
|        - |  9918 | ` *    Stable.` |
|        - |  9919 | ` */` |
|        - |  9920 | `/*` |
|        - |  9921 | ` * Extract the top active class. NULL is returned` |
|        - |  9922 | ` * if the class stack is empty.` |
|        - |  9923 | ` */` |
|      672 |  9924 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  9925 |  |
|      674 |  9926 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  9927 | `	ph7_class **apClass;` |
|      674 |  9928 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  9929 | `		/* Empty stack,return NULL */` |
|       15 |  9930 | `		return 0;` |
|        - |  9931 | `	}` |
|        - |  9932 | `	/* Peek the last entry */` |
|      660 |  9933 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      660 |  9934 | `	return apClass[pSet->nUsed - 1];` |
|      338 |  9935 |  |
|        - |  9936 | `/*` |
|        - |  9937 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  9938 | ` *   Get the class that declared the currently executing method.` |
|        - |  9939 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  9940 | ` *` |
|        - |  9941 | ` * Parameters` |
|        - |  9942 | ` *   pVm: Target VM` |
|        - |  9943 | ` *` |
|        - |  9944 | ` * Return` |
|        - |  9945 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  9946 | ` *   - Not executing within a class method` |
|        - |  9947 | ` *` |
|        - |  9948 | ` * Note` |
|        - |  9949 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  9950 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  9951 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  9952 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  9953 | ` *   declaring class.` |
|        - |  9954 | ` */` |
|       96 |  9955 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  9956 |  |
|       98 |  9957 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9958 | `	ph7_vm_func *pVmFunc;` |
|        - |  9959 |  |
|        - |  9960 | `	/* Skip exception frames to find the actual method frame */` |
|       98 |  9961 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  9962 |  |
|        - |  9963 | `	/* Check if we're in a method context */` |
|       98 |  9964 | `	if( pFrame->pParent ){` |
|       94 |  9965 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       94 |  9966 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  9967 | `			/* Return the declaring class */` |
|       94 |  9968 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  9969 | `		}` |
|      ! 0 |  9970 | `	}` |
|        - |  9971 |  |
|        5 |  9972 | `	return 0;` |
|       50 |  9973 |  |
|        - |  9974 |  |
|        - |  9975 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  9976 | `/*` |
|        - |  9977 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  9978 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  9979 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  9980 | ` * return value indicates failure.` |
|        - |  9981 | ` */` |
|        - |  9982 | `/*` |
|        - |  9983 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - |  9984 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - |  9985 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - |  9986 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - |  9987 | ` */` |
|     1662 |  9988 | `static sxi32 VmCallClassMethodWithMap(` |
|        - |  9989 | `	ph7_vm *pVm,` |
|        - |  9990 | `	ph7_class_instance *pThis,` |
|        - |  9991 | `	ph7_class_method *pMethod,` |
|        - |  9992 | `	ph7_value *pResult,` |
|        - |  9993 | `	int nArg,` |
|        - |  9994 | `	ph7_value **apArg,` |
|        - |  9995 | `	VmCallArgMap *pMap` |
|        - |  9996 | `	)` |
|        2 |  9997 |  |
|        - |  9998 | `	ph7_value *aStack;` |
|        - |  9999 | `	VmInstr aInstr[2];` |
|        - | 10000 | `	int iCursor;` |
|        - | 10001 | `	int i;` |
|     1664 | 10002 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1664 | 10003 | `	if( aStack == 0 ){` |
|      ! 0 | 10004 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10005 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10006 | `		return SXERR_MEM;` |
|        - | 10007 | `	}` |
|     2406 | 10008 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      744 | 10009 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      744 | 10010 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      373 | 10011 | `	}` |
|     1664 | 10012 | `	iCursor = nArg + 1;` |
|     1664 | 10013 | `	if( pThis ){` |
|     1658 | 10014 | `		pThis->iRef++;` |
|     1658 | 10015 | `		aStack[i].x.pOther = pThis;` |
|     1658 | 10016 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      828 | 10017 | `	}` |
|     1664 | 10018 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1664 | 10019 | `	i++;` |
|     1664 | 10020 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1664 | 10021 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1664 | 10022 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1664 | 10023 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1664 | 10024 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1664 | 10025 | `	aInstr[0].iP1 = nArg;` |
|     1664 | 10026 | `	aInstr[0].iP2 = 0;` |
|     1664 | 10027 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1664 | 10028 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1664 | 10029 | `	aInstr[1].iP1 = 1;` |
|     1664 | 10030 | `	aInstr[1].iP2 = 0;` |
|     1664 | 10031 | `	aInstr[1].p3  = 0;` |
|     1664 | 10032 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|     1664 | 10033 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1664 | 10034 | `	return PH7_OK;` |
|      833 | 10035 |  |
|     1508 | 10036 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10037 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10038 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10039 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10040 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10041 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10042 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10043 | `	)` |
|        2 | 10044 |  |
|     1510 | 10045 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10046 |  |
|        - | 10047 | `/*` |
|        - | 10048 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10049 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10050 | ` * in the apArg[] array.` |
|        - | 10051 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10052 | ` * return value indicates failure.` |
|        - | 10053 | ` */` |
|      966 | 10054 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10055 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10056 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10057 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10058 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10059 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10060 | `	)` |
|        2 | 10061 |  |
|        - | 10062 | `	ph7_value *aStack;` |
|        - | 10063 | `	VmInstr aInstr[2];` |
|        - | 10064 | `	int i;` |
|      968 | 10065 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10066 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 | 10067 | `		if( pResult ){` |
|        - | 10068 | `			/* Assume a null return value */` |
|      ! 0 | 10069 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10070 | `		}` |
|      479 | 10071 | `		return SXERR_INVALID;` |
|        - | 10072 | `	}` |
|      490 | 10073 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10074 | `		/* Class method */` |
|       11 | 10075 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10076 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10077 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10078 | `		ph7_class *pClass = 0;` |
|        - | 10079 | `		ph7_value *pValue;` |
|        - | 10080 | `		sxi32 rc;` |
|       11 | 10081 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10082 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10083 | `			if( pResult ){` |
|        - | 10084 | `				/* Assume a null return value */` |
|      ! 0 | 10085 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10086 | `			}` |
|      ! 0 | 10087 | `			return SXRET_OK;` |
|        - | 10088 | `		}` |
|        - | 10089 | `		/* Extract the class name or an instance of it */` |
|       11 | 10090 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10091 | `		if( pValue ){` |
|       11 | 10092 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10093 | `		}` |
|       11 | 10094 | `		if( pClass == 0 ){` |
|        - | 10095 | `			/* No such class,return NULL */` |
|      ! 0 | 10096 | `			if( pResult ){` |
|      ! 0 | 10097 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10098 | `			}` |
|      ! 0 | 10099 | `			return SXRET_OK;` |
|        - | 10100 | `		}` |
|       11 | 10101 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10102 | `			/* Point to the class instance */` |
|        5 | 10103 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10104 | `		}` |
|        - | 10105 | `		/* Try to extract the method */` |
|       11 | 10106 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10107 | `		if( pValue ){` |
|       11 | 10108 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10109 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10110 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10111 | `			}` |
|        5 | 10112 | `		}` |
|       11 | 10113 | `		if( pMethod == 0 ){` |
|        - | 10114 | `			/* No such method,return NULL */` |
|      ! 0 | 10115 | `			if( pResult ){` |
|      ! 0 | 10116 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10117 | `			}` |
|      ! 0 | 10118 | `			return SXRET_OK;` |
|        - | 10119 | `		}` |
|        - | 10120 | `		/* Call the class method */` |
|       11 | 10121 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10122 | `		return rc;` |
|        - | 10123 | `	}` |
|        - | 10124 | `	/* Create a new operand stack */` |
|      480 | 10125 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      480 | 10126 | `	if( aStack == 0 ){` |
|      ! 0 | 10127 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10128 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10129 | `		if( pResult ){` |
|        - | 10130 | `			/* Assume a null return value */` |
|      ! 0 | 10131 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10132 | `		}` |
|      ! 0 | 10133 | `		return SXERR_MEM;` |
|        - | 10134 | `	}` |
|        - | 10135 | `	/* Fill the operand stack with the given arguments */` |
|     1534 | 10136 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1056 | 10137 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10138 | `		/*` |
|        - | 10139 | `		 * Symisc eXtension:` |
|        - | 10140 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10141 | `		 */` |
|     1056 | 10142 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      529 | 10143 | `	}` |
|        - | 10144 | `	/* Push the function name */` |
|      480 | 10145 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      480 | 10146 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10147 | `	/* Emit the CALL istruction */` |
|      480 | 10148 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      480 | 10149 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      480 | 10150 | `	aInstr[0].iP2 = 0;` |
|      480 | 10151 | `	aInstr[0].p3  = 0;` |
|        - | 10152 | `	/* Emit the DONE instruction */` |
|      480 | 10153 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      480 | 10154 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      480 | 10155 | `	aInstr[1].iP2 = 0;` |
|      480 | 10156 | `	aInstr[1].p3  = 0;` |
|        - | 10157 | `	/* Execute the function body (if available) */` |
|      480 | 10158 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - | 10159 | `	/* Clean up the mess left behind */` |
|      480 | 10160 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      480 | 10161 | `	return PH7_OK;` |
|      485 | 10162 |  |
|        - | 10163 | `/*` |
|        - | 10164 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10165 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10166 | ` * parameter.` |
|        - | 10167 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10168 | ` * return value indicates failure.` |
|        - | 10169 | ` */` |
|      236 | 10170 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10171 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10172 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10173 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10174 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10175 | `	)` |
|        1 | 10176 |  |
|        - | 10177 | `	ph7_value *pArg;` |
|        - | 10178 | `	SySet aArg;` |
|        - | 10179 | `	va_list ap;` |
|        - | 10180 | `	sxi32 rc;` |
|      237 | 10181 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10182 | `	/* Copy arguments one after one */` |
|      237 | 10183 | `	va_start(ap,pResult);` |
|      393 | 10184 | `	for(;;){` |
|      787 | 10185 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10186 | `		if( pArg == 0 ){` |
|      237 | 10187 | `			break;` |
|        - | 10188 | `		}` |
|      551 | 10189 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10190 | `	}` |
|        - | 10191 | `	/* Call the core routine */` |
|      237 | 10192 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10193 | `	/* Cleanup */` |
|      237 | 10194 | `	SySetRelease(&aArg);` |
|      237 | 10195 | `	return rc;` |
|        1 | 10196 |  |
|        - | 10197 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10198 | `/*` |
|        - | 10199 | ` * bool defined(string $name)` |
|        - | 10200 | ` *  Checks whether a given named constant exists.` |
|        - | 10201 | ` * Parameter:` |
|        - | 10202 | ` *  Name of the desired constant.` |
|        - | 10203 | ` * Return` |
|        - | 10204 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10205 | ` */` |
|       14 | 10206 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10207 |  |
|        - | 10208 | `	const char *zName;` |
|       16 | 10209 | `	int nLen = 0;` |
|       16 | 10210 | `	int res = 0;` |
|       16 | 10211 | `	if( nArg < 1 ){` |
|        - | 10212 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10213 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10214 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10215 | `		return SXRET_OK;` |
|        - | 10216 | `	}` |
|        - | 10217 | `	/* Extract constant name */` |
|       16 | 10218 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10219 | `	/* Perform the lookup */` |
|       16 | 10220 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10221 | `		/* Already defined */` |
|       10 | 10222 | `		res = 1;` |
|        4 | 10223 | `	}` |
|       16 | 10224 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10225 | `	return SXRET_OK;` |
|        9 | 10226 |  |
|        - | 10227 | `/*` |
|        - | 10228 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10229 | ` * below.` |
|        - | 10230 | ` */` |
|       10 | 10231 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10232 |  |
|       12 | 10233 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10234 | `	/* Expand constant value */` |
|       12 | 10235 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10236 |  |
|        - | 10237 | `/*` |
|        - | 10238 | ` * bool define(string $constant_name,expression value)` |
|        - | 10239 | ` *  Defines a named constant at runtime.` |
|        - | 10240 | ` * Parameter:` |
|        - | 10241 | ` *  $constant_name` |
|        - | 10242 | ` *   The name of the constant` |
|        - | 10243 | ` *  $value` |
|        - | 10244 | ` *   Constant value` |
|        - | 10245 | ` * Return:` |
|        - | 10246 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10247 | ` */` |
|       12 | 10248 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10249 |  |
|        - | 10250 | `	const char *zName;  /* Constant name */` |
|        - | 10251 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10252 | `	int nLen = 0;       /* Name length */` |
|        - | 10253 | `	sxi32 rc;` |
|       14 | 10254 | `	if( nArg < 2 ){` |
|        - | 10255 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10256 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10257 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10258 | `		return SXRET_OK;` |
|        - | 10259 | `	}` |
|       14 | 10260 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10261 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10262 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10263 | `		return SXRET_OK;` |
|        - | 10264 | `	}` |
|        - | 10265 | `	/* Extract constant name */` |
|       14 | 10266 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10267 | `	if( nLen < 1 ){` |
|      ! 0 | 10268 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10269 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10270 | `		return SXRET_OK;` |
|        - | 10271 | `	}` |
|        - | 10272 | `	/* Duplicate constant value */` |
|       14 | 10273 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10274 | `	if( pValue == 0 ){` |
|      ! 0 | 10275 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10276 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10277 | `		return SXRET_OK;` |
|        - | 10278 | `	}` |
|        - | 10279 | `	/* Initialize the memory object */` |
|       14 | 10280 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10281 | `	/* Register the constant */` |
|       14 | 10282 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10283 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10284 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10285 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10286 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10287 | `		return SXRET_OK;` |
|        - | 10288 | `	}` |
|        - | 10289 | `	/* Duplicate constant value */` |
|       14 | 10290 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10291 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 10292 | `		/* Lower case the constant name */` |
|      ! 0 | 10293 | `		char *zCur = (char *)zName;` |
|      ! 0 | 10294 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 10295 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 10296 | `				/* UTF-8 stream */` |
|      ! 0 | 10297 | `				zCur++;` |
|      ! 0 | 10298 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 10299 | `					zCur++;` |
|      ! 0 | 10300 | `				}` |
|      ! 0 | 10301 | `				continue;` |
|        - | 10302 | `			}` |
|      ! 0 | 10303 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 10304 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 10305 | `				zCur[0] = (char)c;` |
|      ! 0 | 10306 | `			}` |
|      ! 0 | 10307 | `			zCur++;` |
|      ! 0 | 10308 | `		}` |
|        - | 10309 | `		/* Finally,register the constant */` |
|      ! 0 | 10310 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 10311 | `	}` |
|        - | 10312 | `	/* All done,return TRUE */` |
|       14 | 10313 | `	ph7_result_bool(pCtx,1);` |
|       14 | 10314 | `	return SXRET_OK;` |
|        8 | 10315 |  |
|        - | 10316 | `/*` |
|        - | 10317 | ` * value constant(string $name)` |
|        - | 10318 | ` *  Returns the value of a constant` |
|        - | 10319 | ` * Parameter` |
|        - | 10320 | ` *  $name` |
|        - | 10321 | ` *    Name of the constant.` |
|        - | 10322 | ` * Return` |
|        - | 10323 | ` *  Constant value or NULL if not defined.` |
|        - | 10324 | ` */` |
|        8 | 10325 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10326 |  |
|        - | 10327 | `	SyHashEntry *pEntry;` |
|        - | 10328 | `	ph7_constant *pCons;` |
|        - | 10329 | `	const char *zName; /* Constant name */` |
|        - | 10330 | `	ph7_value sVal;    /* Constant value */` |
|        - | 10331 | `	int nLen;` |
|       10 | 10332 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10333 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 10334 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 10335 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10336 | `		return SXRET_OK;` |
|        - | 10337 | `	}` |
|        - | 10338 | `	/* Extract the constant name */` |
|       10 | 10339 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10340 | `	/* Perform the query */` |
|       10 | 10341 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 10342 | `	if( pEntry == 0 ){` |
|        3 | 10343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 10344 | `		ph7_result_null(pCtx);` |
|        3 | 10345 | `		return SXRET_OK;` |
|        - | 10346 | `	}` |
|        8 | 10347 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 10348 | `	/* Point to the structure that describe the constant */` |
|        8 | 10349 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 10350 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 10351 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 10352 | `	/* Return that value */` |
|        8 | 10353 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 10354 | `	/* Cleanup */` |
|        8 | 10355 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 10356 | `	return SXRET_OK;` |
|        6 | 10357 |  |
|        - | 10358 | `/*` |
|        - | 10359 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 10360 | ` * defined below.` |
|        - | 10361 | ` */` |
|      452 | 10362 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10363 |  |
|      453 | 10364 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10365 | `	ph7_value sName;` |
|        - | 10366 | `	sxi32 rc;` |
|        - | 10367 | `	/* Prepare the constant name for insertion */` |
|      453 | 10368 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 10369 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10370 | `	/* Perform the insertion */` |
|      453 | 10371 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 10372 | `	PH7_MemObjRelease(&sName);` |
|      453 | 10373 | `	return rc;` |
|        1 | 10374 |  |
|        - | 10375 | `/*` |
|        - | 10376 | ` * array get_defined_constants(void)` |
|        - | 10377 | ` *  Returns an associative array with the names of all defined` |
|        - | 10378 | ` *  constants.` |
|        - | 10379 | ` * Parameters` |
|        - | 10380 | ` *  NONE.` |
|        - | 10381 | ` * Returns` |
|        - | 10382 | ` *  Returns the names of all the constants currently defined.` |
|        - | 10383 | ` */` |
|        2 | 10384 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10385 |  |
|        - | 10386 | `	ph7_value *pArray;` |
|        - | 10387 | `	/* Create the array first*/` |
|        3 | 10388 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10389 | `	if( pArray == 0 ){` |
|      ! 0 | 10390 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10391 | `		SXUNUSED(apArg);` |
|        - | 10392 | `		/* Return NULL */` |
|      ! 0 | 10393 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10394 | `		return SXRET_OK;` |
|        - | 10395 | `	}` |
|        - | 10396 | `	/* Fill the array with the defined constants */` |
|        3 | 10397 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 10398 | `	/* Return the created array */` |
|        3 | 10399 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10400 | `	return SXRET_OK;` |
|        2 | 10401 |  |
|        - | 10402 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 10403 | `/*` |
|        - | 10404 | ` * Section:` |
|        - | 10405 | ` *  Random numbers/string generators.` |
|        - | 10406 | ` * Status:` |
|        - | 10407 | ` *    Stable.` |
|        - | 10408 | ` */` |
|        - | 10409 | `/*` |
|        - | 10410 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 10411 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10412 | ` * used by te SQLite3 library.` |
|        - | 10413 | ` */` |
|     2611 | 10414 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10415 |  |
|        - | 10416 | `	sxu32 iNum;` |
|     2613 | 10417 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2613 | 10418 | `	return iNum;` |
|        2 | 10419 |  |
|        - | 10420 | `/*` |
|        - | 10421 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10422 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10423 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10424 | ` * by te SQLite3 library.` |
|        - | 10425 | ` */` |
|   136104 | 10426 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10427 |  |
|        - | 10428 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10429 | `	int i;` |
|        - | 10430 | `	/* Generate a binary string first */` |
|   136106 | 10431 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10432 | `	/* Turn the binary string into english based alphabet */` |
|  1497314 | 10433 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1361210 | 10434 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   680606 | 10435 | `	 }` |
|   136106 | 10436 |  |
|        - | 10437 | `/*` |
|        - | 10438 | ` * int rand()` |
|        - | 10439 | ` * int mt_rand()` |
|        - | 10440 | ` * int rand(int $min,int $max)` |
|        - | 10441 | ` * int mt_rand(int $min,int $max)` |
|        - | 10442 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10443 | ` * Parameter` |
|        - | 10444 | ` *  $min` |
|        - | 10445 | ` *    The lowest value to return (default: 0)` |
|        - | 10446 | ` *  $max` |
|        - | 10447 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10448 | ` * Return` |
|        - | 10449 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10450 | ` * Note:` |
|        - | 10451 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10452 | ` *  by te SQLite3 library.` |
|        - | 10453 | ` */` |
|       20 | 10454 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10455 |  |
|        - | 10456 | `	sxu32 iNum;` |
|        - | 10457 | `	/* Generate the random number */` |
|       21 | 10458 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10459 | `	if( nArg > 1 ){` |
|        - | 10460 | `		sxu32 iMin,iMax;` |
|        3 | 10461 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 10462 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 10463 | `		if( iMin < iMax ){` |
|        3 | 10464 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 10465 | `			if( iDiv > 0 ){` |
|        3 | 10466 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 10467 | `			}` |
|        1 | 10468 | `		}else if(iMax > 0 ){` |
|      ! 0 | 10469 | `			iNum %= iMax;` |
|      ! 0 | 10470 | `		}` |
|        1 | 10471 | `	}` |
|        - | 10472 | `	/* Return the number */` |
|       21 | 10473 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 10474 | `	return SXRET_OK;` |
|        1 | 10475 |  |
|        - | 10476 | `/*` |
|        - | 10477 | ` * int getrandmax(void)` |
|        - | 10478 | ` * int mt_getrandmax(void)` |
|        - | 10479 | ` * int rc4_getrandmax(void)` |
|        - | 10480 | ` *   Show largest possible random value` |
|        - | 10481 | ` * Return` |
|        - | 10482 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 10483 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 10484 | ` * Note:` |
|        - | 10485 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10486 | ` *  by te SQLite3 library.` |
|        - | 10487 | ` */` |
|        4 | 10488 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10489 |  |
|        2 | 10490 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 10491 | `	SXUNUSED(apArg);` |
|        5 | 10492 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 10493 | `	return SXRET_OK;` |
|        1 | 10494 |  |
|        - | 10495 | `/*` |
|        - | 10496 | ` * string rand_str()` |
|        - | 10497 | ` * string rand_str(int $len)` |
|        - | 10498 | ` *  Generate a random string (English alphabet).` |
|        - | 10499 | ` * Parameter` |
|        - | 10500 | ` *  $len` |
|        - | 10501 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 10502 | ` * Return` |
|        - | 10503 | ` *   A pseudo random string.` |
|        - | 10504 | ` * Note:` |
|        - | 10505 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10506 | ` *  by te SQLite3 library.` |
|        - | 10507 | ` *  This function is a symisc extension.` |
|        - | 10508 | ` */` |
|      120 | 10509 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10510 |  |
|        - | 10511 | `	char zString[1024];` |
|      122 | 10512 | `	int iLen = 0x10;` |
|      122 | 10513 | `	if( nArg > 0 ){` |
|        - | 10514 | `		/* Get the desired length */` |
|      122 | 10515 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 10516 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 10517 | `			/* Default length */` |
|        3 | 10518 | `			iLen = 0x10;` |
|        1 | 10519 | `		}` |
|       60 | 10520 | `	}` |
|        - | 10521 | `	/* Generate the random string */` |
|      122 | 10522 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 10523 | `	/* Return the generated string */` |
|      122 | 10524 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 10525 | `	return SXRET_OK;` |
|        2 | 10526 |  |
|        - | 10527 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10528 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10529 | `/* Unique ID private data */` |
|        - | 10530 | `struct unique_id_data` |
|        - | 10531 |  |
|        - | 10532 | `	ph7_context *pCtx; /* Call context */` |
|        - | 10533 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 10534 | `};` |
|        - | 10535 | `/*` |
|        - | 10536 | ` * Binary to hex consumer callback.` |
|        - | 10537 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 10538 | ` * defined below.` |
|        - | 10539 | ` */` |
|      192 | 10540 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 10541 |  |
|      193 | 10542 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 10543 | `	sxu32 nBuflen;` |
|        - | 10544 | `	/* Extract result buffer length */` |
|      193 | 10545 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 10546 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 10547 | `			/*` |
|        - | 10548 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 10549 | `			 * string will be 13 characters long` |
|        - | 10550 | `			 */` |
|       25 | 10551 | `		return SXERR_ABORT;` |
|        - | 10552 | `	}` |
|      169 | 10553 | `	if( nBuflen > 22 ){` |
|      ! 0 | 10554 | `		return SXERR_ABORT;` |
|        - | 10555 | `	}` |
|        - | 10556 | `	/* Safely Consume the hex stream */` |
|      169 | 10557 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 10558 | `	return SXRET_OK;` |
|       97 | 10559 |  |
|        - | 10560 | `/*` |
|        - | 10561 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 10562 | ` *  Generate a unique ID` |
|        - | 10563 | ` * Parameter` |
|        - | 10564 | ` * $prefix` |
|        - | 10565 | ` *  Append this prefix to the generated unique ID.` |
|        - | 10566 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 10567 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 10568 | ` * $more_entropy` |
|        - | 10569 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 10570 | ` *  that the result will be unique.` |
|        - | 10571 | ` * Return` |
|        - | 10572 | ` *  Returns the unique identifier, as a string.` |
|        - | 10573 | ` */` |
|       24 | 10574 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10575 |  |
|        - | 10576 | `	struct unique_id_data sUniq;` |
|        - | 10577 | `	unsigned char zDigest[20];` |
|       25 | 10578 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10579 | `	const char *zPrefix;` |
|        - | 10580 | `	SHA1Context sCtx;` |
|        - | 10581 | `	char zRandom[7];` |
|        - | 10582 | `	int nPrefix;` |
|        - | 10583 | `	int entropy;` |
|        - | 10584 | `	/* Generate a random string first */` |
|       25 | 10585 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 10586 | `	/* Initialize fields */` |
|       25 | 10587 | `	zPrefix = 0;` |
|       25 | 10588 | `	nPrefix = 0;` |
|       25 | 10589 | `	entropy = 0;` |
|       25 | 10590 | `	if( nArg > 0 ){` |
|        - | 10591 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 10592 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 10593 | `		if( nArg > 1 ){` |
|      ! 0 | 10594 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 10595 | `		}` |
|      ! 0 | 10596 | `	}` |
|       25 | 10597 | `	SHA1Init(&sCtx);` |
|        - | 10598 | `	/* Generate the random ID */` |
|       25 | 10599 | `	if( nPrefix > 0 ){` |
|      ! 0 | 10600 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 10601 | `	}` |
|        - | 10602 | `	/* Append the random ID */` |
|       25 | 10603 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 10604 | `	/* Append the random string */` |
|       25 | 10605 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 10606 | `	/* Increment the number */` |
|       25 | 10607 | `	pVm->unique_id++;` |
|       25 | 10608 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 10609 | `	/* Hexify the digest */` |
|       25 | 10610 | `	sUniq.pCtx = pCtx;` |
|       25 | 10611 | `	sUniq.entropy = entropy;` |
|       25 | 10612 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 10613 | `	/* All done */` |
|       25 | 10614 | `	return PH7_OK;` |
|        1 | 10615 |  |
|        - | 10616 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10617 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10618 | `/*` |
|        - | 10619 | ` * Section:` |
|        - | 10620 | ` *  Language construct implementation as foreign functions.` |
|        - | 10621 | ` * Status:` |
|        - | 10622 | ` *    Stable.` |
|        - | 10623 | ` */` |
|        - | 10624 | `/*` |
|        - | 10625 | ` * void echo($string...)` |
|        - | 10626 | ` *  Output one or more messages.` |
|        - | 10627 | ` * Parameters` |
|        - | 10628 | ` *  $string` |
|        - | 10629 | ` *   Message to output.` |
|        - | 10630 | ` * Return` |
|        - | 10631 | ` *  NULL.` |
|        - | 10632 | ` */` |
|      ! 0 | 10633 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10634 |  |
|        - | 10635 | `	const char *zData;` |
|      ! 0 | 10636 | `	int nDataLen = 0;` |
|        - | 10637 | `	ph7_vm *pVm;` |
|        - | 10638 | `	int i,rc;` |
|        - | 10639 | `	/* Point to the target VM */` |
|      ! 0 | 10640 | `	pVm = pCtx->pVm;` |
|        - | 10641 | `	/* Output */` |
|      ! 0 | 10642 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 10643 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 10644 | `		if( nDataLen > 0 ){` |
|      ! 0 | 10645 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 10646 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 10647 | `			if( rc == SXERR_ABORT ){` |
|        - | 10648 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10649 | `				return PH7_ABORT;` |
|        - | 10650 | `			}` |
|      ! 0 | 10651 | `		}` |
|      ! 0 | 10652 | `	}` |
|      ! 0 | 10653 | `	return SXRET_OK;` |
|      ! 0 | 10654 |  |
|        - | 10655 | `/*` |
|        - | 10656 | ` * int print($string...)` |
|        - | 10657 | ` *  Output one or more messages.` |
|        - | 10658 | ` * Parameters` |
|        - | 10659 | ` *  $string` |
|        - | 10660 | ` *   Message to output.` |
|        - | 10661 | ` * Return` |
|        - | 10662 | ` *  1 always.` |
|        - | 10663 | ` */` |
|        2 | 10664 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10665 |  |
|        - | 10666 | `	const char *zData;` |
|        3 | 10667 | `	int nDataLen = 0;` |
|        - | 10668 | `	ph7_vm *pVm;` |
|        - | 10669 | `	int i,rc;` |
|        - | 10670 | `	/* Point to the target VM */` |
|        3 | 10671 | `	pVm = pCtx->pVm;` |
|        - | 10672 | `	/* Output */` |
|        5 | 10673 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10674 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10675 | `		if( nDataLen > 0 ){` |
|        3 | 10676 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10677 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10678 | `			if( rc == SXERR_ABORT ){` |
|        - | 10679 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10680 | `				return PH7_ABORT;` |
|        - | 10681 | `			}` |
|        1 | 10682 | `		}` |
|        2 | 10683 | `	}` |
|        - | 10684 | `	/* Return 1 */` |
|        3 | 10685 | `	ph7_result_int(pCtx,1);` |
|        3 | 10686 | `	return SXRET_OK;` |
|        2 | 10687 |  |
|        - | 10688 | `/*` |
|        - | 10689 | ` * void exit(string $msg)` |
|        - | 10690 | ` * void exit(int $status)` |
|        - | 10691 | ` * void die(string $ms)` |
|        - | 10692 | ` * void die(int $status)` |
|        - | 10693 | ` *   Output a message and terminate program execution.` |
|        - | 10694 | ` * Parameter` |
|        - | 10695 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10696 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10697 | ` *  and not printed` |
|        - | 10698 | ` * Return` |
|        - | 10699 | ` *  NULL` |
|        - | 10700 | ` */` |
|      ! 0 | 10701 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10702 |  |
|      ! 0 | 10703 | `	if( nArg > 0 ){` |
|      ! 0 | 10704 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10705 | `			const char *zData;` |
|      ! 0 | 10706 | `			int iLen = 0;` |
|        - | 10707 | `			/* Print exit message */` |
|      ! 0 | 10708 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10709 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10710 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10711 | `			sxi32 iExitStatus;` |
|        - | 10712 | `			/* Record exit status code */` |
|      ! 0 | 10713 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10714 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10715 | `		}` |
|      ! 0 | 10716 | `	}` |
|        - | 10717 | `	/* Check if we are in an included file */` |
|      ! 0 | 10718 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10719 | `		/* Exit the entire process */` |
|      ! 0 | 10720 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10721 | `	}` |
|        - | 10722 | `	/* Abort processing immediately */` |
|      ! 0 | 10723 | `	return PH7_ABORT;` |
|      ! 0 | 10724 |  |
|        - | 10725 | `/*` |
|        - | 10726 | ` * bool isset($var,...)` |
|        - | 10727 | ` *  Finds out whether a variable is set.` |
|        - | 10728 | ` * Parameters` |
|        - | 10729 | ` *  One or more variable to check.` |
|        - | 10730 | ` * Return` |
|        - | 10731 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10732 | ` */` |
|    82046 | 10733 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10734 |  |
|        - | 10735 | `	ph7_value *pObj;` |
|    82048 | 10736 | `	int res = 0;` |
|        - | 10737 | `	int i;` |
|    82048 | 10738 | `	if( nArg < 1 ){` |
|        - | 10739 | `		/* Missing arguments,return false */` |
|      ! 0 | 10740 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10741 | `		return SXRET_OK;` |
|        - | 10742 | `	}` |
|        - | 10743 | `	/* Iterate over available arguments */` |
|   107704 | 10744 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    82048 | 10745 | `		pObj = apArg[i];` |
|    82048 | 10746 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    55772 | 10747 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10748 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10749 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10750 | `			}` |
|    27885 | 10751 | `		}` |
|    82048 | 10752 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    82048 | 10753 | `		if( !res ){` |
|        - | 10754 | `			/* Variable not set,return FALSE */` |
|    56392 | 10755 | `			ph7_result_bool(pCtx,0);` |
|    56392 | 10756 | `			return SXRET_OK;` |
|        - | 10757 | `		}` |
|    12830 | 10758 | `	}` |
|        - | 10759 | `	/* All given variable are set,return TRUE */` |
|    25658 | 10760 | `	ph7_result_bool(pCtx,1);` |
|    25658 | 10761 | `	return SXRET_OK;` |
|    41025 | 10762 |  |
|        - | 10763 | `/*` |
|        - | 10764 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10765 | ` * frame,the reference table and discard it's contents.` |
|        - | 10766 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10767 | ` */` |
|  3062774 | 10768 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10769 |  |
|        - | 10770 | `	ph7_value *pObj;` |
|        - | 10771 | `	VmRefObj *pRef;` |
|  3062776 | 10772 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3062776 | 10773 | `	if( pObj ){` |
|        - | 10774 | `		/* Release the object */` |
|  3062776 | 10775 | `		PH7_MemObjRelease(pObj);` |
|  1531387 | 10776 | `	}` |
|        - | 10777 | `	/* Remove old reference links */` |
|  3062776 | 10778 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3062776 | 10779 | `	if( pRef ){` |
|  3062770 | 10780 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10781 | `		/* Unlink from the reference table */` |
|  3062770 | 10782 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3062770 | 10783 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10784 | `			VmSlot sFree;` |
|        - | 10785 | `			/* Restore to the free list */` |
|  3062764 | 10786 | `			sFree.nIdx = nObjIdx;` |
|  3062764 | 10787 | `			sFree.pUserData = 0;` |
|  3062764 | 10788 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1531381 | 10789 | `		}` |
|  1531384 | 10790 | `	}` |
|  3062776 | 10791 | `	return SXRET_OK;` |
|        2 | 10792 |  |
|        - | 10793 | `/*` |
|        - | 10794 | ` * void unset($var,...)` |
|        - | 10795 | ` *   Unset one or more given variable.` |
|        - | 10796 | ` * Parameters` |
|        - | 10797 | ` *  One or more variable to unset.` |
|        - | 10798 | ` * Return` |
|        - | 10799 | ` *  Nothing.` |
|        - | 10800 | ` */` |
|     7114 | 10801 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10802 |  |
|        - | 10803 | `	ph7_value *pObj;` |
|        - | 10804 | `	ph7_vm *pVm;` |
|        - | 10805 | `	int i;` |
|        - | 10806 | `	/* Point to the target VM */` |
|     7116 | 10807 | `	pVm = pCtx->pVm;` |
|        - | 10808 | `	/* Iterate and unset */` |
|    14230 | 10809 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7116 | 10810 | `		pObj = apArg[i];` |
|     7116 | 10811 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10812 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10813 | `				/* Throw an error */` |
|      ! 0 | 10814 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10815 | `			}` |
|      ! 0 | 10816 | `		}else{` |
|     7116 | 10817 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 10818 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7116 | 10819 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7110 | 10820 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3554 | 10821 | `			}` |
|        - | 10822 | `		}` |
|     3559 | 10823 | `	}` |
|     7116 | 10824 | `	return SXRET_OK;` |
|        2 | 10825 |  |
|        - | 10826 | `/*` |
|        - | 10827 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 10828 | ` */` |
|      110 | 10829 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10830 |  |
|      111 | 10831 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 10832 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10833 | `	ph7_value *pObj;` |
|        - | 10834 | `	sxu32 nIdx;` |
|        - | 10835 | `	/* Extract the memory object */` |
|      111 | 10836 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 10837 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 10838 | `	if( pObj ){` |
|      111 | 10839 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 10840 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 10841 | `				SyString sName;` |
|        - | 10842 | `				ph7_value sKey;` |
|        - | 10843 | `				/* Perform the insertion */` |
|      109 | 10844 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 10845 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 10846 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 10847 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 10848 | `			}` |
|       54 | 10849 | `		}` |
|       55 | 10850 | `	}` |
|      111 | 10851 | `	return SXRET_OK;` |
|        1 | 10852 |  |
|        - | 10853 | `/*` |
|        - | 10854 | ` * array get_defined_vars(void)` |
|        - | 10855 | ` *  Returns an array of all defined variables.` |
|        - | 10856 | ` * Parameter` |
|        - | 10857 | ` *  None` |
|        - | 10858 | ` * Return` |
|        - | 10859 | ` *  An array with all the variables defined in the current scope.` |
|        - | 10860 | ` */` |
|        2 | 10861 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10862 |  |
|        3 | 10863 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10864 | `	ph7_value *pArray;` |
|        - | 10865 | `	/* Create a new array */` |
|        3 | 10866 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10867 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10868 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10869 | `		SXUNUSED(apArg);` |
|        - | 10870 | `		/* Return NULL */` |
|      ! 0 | 10871 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10872 | `		return SXRET_OK;` |
|        - | 10873 | `	}` |
|        - | 10874 | `	/* Superglobals first */` |
|        3 | 10875 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 10876 | `	/* Then variable defined in the current frame */` |
|        3 | 10877 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 10878 | `	/* Finally,return the created array */` |
|        3 | 10879 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10880 | `	return SXRET_OK;` |
|        2 | 10881 |  |
|        - | 10882 | `/*` |
|        - | 10883 | ` * bool gettype($var)` |
|        - | 10884 | ` *  Get the type of a variable` |
|        - | 10885 | ` * Parameters` |
|        - | 10886 | ` *   $var` |
|        - | 10887 | ` *    The variable being type checked.` |
|        - | 10888 | ` * Return` |
|        - | 10889 | ` *   String representation of the given variable type.` |
|        - | 10890 | ` */` |
|       32 | 10891 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10892 |  |
|       34 | 10893 | `	const char *zType = "Empty";` |
|       34 | 10894 | `	if( nArg > 0 ){` |
|       34 | 10895 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 10896 | `	}` |
|        - | 10897 | `	/* Return the variable type */` |
|       34 | 10898 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 10899 | `	return SXRET_OK;` |
|        2 | 10900 |  |
|        - | 10901 | `/*` |
|        - | 10902 | ` * string get_resource_type(resource $handle)` |
|        - | 10903 | ` *  This function gets the type of the given resource.` |
|        - | 10904 | ` * Parameters` |
|        - | 10905 | ` *  $handle` |
|        - | 10906 | ` *  The evaluated resource handle.` |
|        - | 10907 | ` * Return` |
|        - | 10908 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 10909 | ` *  representing its type. If the type is not identified by this function` |
|        - | 10910 | ` *  the return value will be the string Unknown.` |
|        - | 10911 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 10912 | ` *  is not a resource.` |
|        - | 10913 | ` */` |
|        2 | 10914 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10915 |  |
|        3 | 10916 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 10917 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 10918 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10919 | `		return PH7_OK;` |
|        - | 10920 | `	}` |
|        3 | 10921 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 10922 | `	return SXRET_OK;` |
|        2 | 10923 |  |
|        - | 10924 | `/*` |
|        - | 10925 | ` * void var_dump(expression,....)` |
|        - | 10926 | ` *   var_dump � Dumps information about a variable` |
|        - | 10927 | ` * Parameters` |
|        - | 10928 | ` *   One or more expression to dump.` |
|        - | 10929 | ` * Returns` |
|        - | 10930 | ` *  Nothing.` |
|        - | 10931 | ` */` |
|      218 | 10932 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10933 |  |
|        - | 10934 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 10935 | `	int i;` |
|      220 | 10936 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 10937 | `	/* Dump one or more expressions */` |
|      444 | 10938 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 10939 | `		ph7_value *pObj = apArg[i];` |
|        - | 10940 | `		/* Reset the working buffer */` |
|      226 | 10941 | `		SyBlobReset(&sDump);` |
|        - | 10942 | `		/* Dump the given expression */` |
|      226 | 10943 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 10944 | `		/* Output */` |
|      226 | 10945 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 10946 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 10947 | `		}` |
|      114 | 10948 | `	}` |
|        - | 10949 | `	/* Release the working buffer */` |
|      220 | 10950 | `	SyBlobRelease(&sDump);` |
|      220 | 10951 | `	return SXRET_OK;` |
|        2 | 10952 |  |
|        - | 10953 | `/*` |
|        - | 10954 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 10955 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 10956 | ` * Parameters` |
|        - | 10957 | ` *   expression: Expression to dump` |
|        - | 10958 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 10959 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 10960 | ` *            print_r() will return the information rather than print it.` |
|        - | 10961 | ` * Return` |
|        - | 10962 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 10963 | ` *  Otherwise, the return value is TRUE.` |
|        - | 10964 | ` */` |
|       16 | 10965 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10966 |  |
|       17 | 10967 | `	int ret_string = 0;` |
|        - | 10968 | `	SyBlob sDump;` |
|       17 | 10969 | `	if( nArg < 1 ){` |
|        - | 10970 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 10971 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10972 | `		return SXRET_OK;` |
|        - | 10973 | `	}` |
|       17 | 10974 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 10975 | `	if ( nArg > 1 ){` |
|        - | 10976 | `		/* Where to redirect output */` |
|       11 | 10977 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 10978 | `	}` |
|        - | 10979 | `	/* Generate dump */` |
|       17 | 10980 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 10981 | `	if( !ret_string ){` |
|        - | 10982 | `		/* Output dump */` |
|        7 | 10983 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10984 | `		/* Return true */` |
|        7 | 10985 | `		ph7_result_bool(pCtx,1);` |
|        4 | 10986 | `	}else{` |
|        - | 10987 | `		/* Generated dump as return value */` |
|       11 | 10988 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10989 | `	}` |
|        - | 10990 | `	/* Release the working buffer */` |
|       17 | 10991 | `	SyBlobRelease(&sDump);` |
|       17 | 10992 | `	return SXRET_OK;` |
|        9 | 10993 |  |
|        - | 10994 | `/*` |
|        - | 10995 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 10996 | ` * Same job as print_r. (see coment above)` |
|        - | 10997 | ` */` |
|        2 | 10998 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10999 |  |
|        3 | 11000 | `	int ret_string = 0;` |
|        - | 11001 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11002 | `	if( nArg < 1 ){` |
|        - | 11003 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11004 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11005 | `		return SXRET_OK;` |
|        - | 11006 | `	}` |
|        3 | 11007 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11008 | `	if ( nArg > 1 ){` |
|        - | 11009 | `		/* Where to redirect output */` |
|        3 | 11010 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11011 | `	}` |
|        - | 11012 | `	/* Generate dump */` |
|        3 | 11013 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11014 | `	if( !ret_string ){` |
|        - | 11015 | `		/* Output dump */` |
|      ! 0 | 11016 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11017 | `		/* Return NULL */` |
|      ! 0 | 11018 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11019 | `	}else{` |
|        - | 11020 | `		/* Generated dump as return value */` |
|        3 | 11021 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11022 | `	}` |
|        - | 11023 | `	/* Release the working buffer */` |
|        3 | 11024 | `	SyBlobRelease(&sDump);` |
|        3 | 11025 | `	return SXRET_OK;` |
|        2 | 11026 |  |
|        - | 11027 | `/*` |
|        - | 11028 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11029 | ` *  Set/get the various assert flags.` |
|        - | 11030 | ` * Parameter` |
|        - | 11031 | ` * $what` |
|        - | 11032 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11033 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11034 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11035 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11036 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11037 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11038 | ` * $value` |
|        - | 11039 | ` *   An optional new value for the option.` |
|        - | 11040 | ` * Return` |
|        - | 11041 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11042 | ` */` |
|       28 | 11043 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11044 |  |
|       30 | 11045 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11046 | `	int iOption;` |
|        - | 11047 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11048 | `	if( nArg < 1 ){` |
|        3 | 11049 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11050 | `			"ArgumentCountError",` |
|        - | 11051 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11052 | `			);` |
|        - | 11053 | `	}` |
|        - | 11054 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11055 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11056 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11057 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11058 | `			"TypeError",` |
|        - | 11059 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11060 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11061 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11062 | `			);` |
|        - | 11063 | `	}` |
|       28 | 11064 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11065 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11066 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11067 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11068 | `	switch( iOption ){` |
|        5 | 11069 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11070 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11071 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11072 | `		if( nArg > 1 ){` |
|        5 | 11073 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11074 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11075 | `			}else{` |
|        3 | 11076 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11077 | `			}` |
|        2 | 11078 | `		}` |
|       12 | 11079 | `		break;` |
|        1 | 11080 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11081 | `		/* Return old callback or null */` |
|        3 | 11082 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11083 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11084 | `		}else{` |
|        3 | 11085 | `			ph7_result_null(pCtx);` |
|        - | 11086 | `		}` |
|        3 | 11087 | `		if( nArg > 1 ){` |
|      ! 0 | 11088 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11089 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11090 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11091 | `			}else{` |
|      ! 0 | 11092 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11093 | `			}` |
|      ! 0 | 11094 | `		}` |
|        3 | 11095 | `		break;` |
|        5 | 11096 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11097 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11098 | `		if( nArg > 1 ){` |
|        5 | 11099 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11100 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11101 | `			}else{` |
|        3 | 11102 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11103 | `			}` |
|        2 | 11104 | `		}` |
|       11 | 11105 | `		break;` |
|      ! 0 | 11106 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11107 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11108 | `		break;` |
|        1 | 11109 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11110 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11111 | `		break;` |
|      ! 0 | 11112 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11113 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11114 | `		break;` |
|        1 | 11115 | `	default:` |
|        - | 11116 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11117 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11118 | `			"ValueError",` |
|        - | 11119 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11120 | `			);` |
|        - | 11121 | `	}` |
|       26 | 11122 | `	return PH7_OK;` |
|       16 | 11123 |  |
|        - | 11124 | `/*` |
|        - | 11125 | ` * bool assert(mixed $assertion)` |
|        - | 11126 | ` *  Checks if assertion is FALSE.` |
|        - | 11127 | ` * Parameter` |
|        - | 11128 | ` *  $assertion` |
|        - | 11129 | ` *    The assertion to test.` |
|        - | 11130 | ` * Return` |
|        - | 11131 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11132 | ` */` |
|       24 | 11133 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11134 |  |
|       26 | 11135 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11136 | `	int iFlags,iResult;` |
|        - | 11137 | `	const char *zDesc;` |
|        - | 11138 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11139 | `	if( nArg < 1 ){` |
|        3 | 11140 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11141 | `			"ArgumentCountError",` |
|        - | 11142 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11143 | `			);` |
|        - | 11144 | `	}` |
|       24 | 11145 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11146 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11147 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11148 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11149 | `		return PH7_OK;` |
|        - | 11150 | `	}` |
|        - | 11151 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11152 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11153 | `	if( !iResult ){` |
|        - | 11154 | `		/* Assertion failed */` |
|        - | 11155 | `		/* Extract optional description */` |
|       13 | 11156 | `		zDesc = 0;` |
|       13 | 11157 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11158 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11159 | `		}` |
|       13 | 11160 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11161 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11162 | `			ph7_value sFile,sLine;` |
|        - | 11163 | `			ph7_value *apCbArg[3];` |
|        - | 11164 | `			SyString *pFile;` |
|        - | 11165 | `			/* Extract the processed script */` |
|      ! 0 | 11166 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11167 | `			if( pFile == 0 ){` |
|      ! 0 | 11168 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11169 | `			}` |
|        - | 11170 | `			/* Invoke the callback */` |
|      ! 0 | 11171 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11172 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11173 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11174 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11175 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11176 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11177 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11178 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11179 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11180 | `		}` |
|       13 | 11181 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11182 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11183 | `			return PH7_ABORT;` |
|        - | 11184 | `		}` |
|        - | 11185 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11186 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11187 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11188 | `				"AssertionError",` |
|        - | 11189 | `				"%s",` |
|        1 | 11190 | `				zDesc` |
|        - | 11191 | `				);` |
|      ! 0 | 11192 | `		}else{` |
|       11 | 11193 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11194 | `				"AssertionError",` |
|        - | 11195 | `				"assert(false)"` |
|        - | 11196 | `				);` |
|        - | 11197 | `		}` |
|        - | 11198 | `	}` |
|        - | 11199 | `	/* Assertion passed */` |
|       11 | 11200 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11201 | `	return PH7_OK;` |
|       14 | 11202 |  |
|        - | 11203 | `/*` |
|        - | 11204 | ` * Section:` |
|        - | 11205 | ` *  Error reporting functions.` |
|        - | 11206 | ` * Status:` |
|        - | 11207 | ` *    Stable.` |
|        - | 11208 | ` */` |
|        - | 11209 | `/*` |
|        - | 11210 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11211 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11212 | ` * Parameters` |
|        - | 11213 | ` *  $error_msg` |
|        - | 11214 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11215 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11216 | ` * $error_type` |
|        - | 11217 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11218 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11219 | ` * Return` |
|        - | 11220 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11221 | ` */` |
|       12 | 11222 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11223 |  |
|       14 | 11224 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11225 | `	int rc = PH7_OK;` |
|       14 | 11226 | `	if( nArg > 0 ){` |
|        - | 11227 | `		const char *zErr;` |
|        - | 11228 | `		int nLen;` |
|        - | 11229 | `		/* Extract the error message */` |
|       12 | 11230 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11231 | `		if( nArg > 1 ){` |
|        - | 11232 | `			/* Extract the error type */` |
|       12 | 11233 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11234 | `			switch( nErr ){` |
|        1 | 11235 | `			case 1:   /* E_ERROR */` |
|        - | 11236 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11237 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11238 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11239 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11240 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11241 | `				break;` |
|        1 | 11242 | `			case 2:   /* E_WARNING */` |
|        - | 11243 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11244 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11245 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11246 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11247 | `				break;` |
|        3 | 11248 | `			default:` |
|        8 | 11249 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11250 | `				break;` |
|        - | 11251 | `			}` |
|        5 | 11252 | `		}` |
|        - | 11253 | `		/* Report error */` |
|       12 | 11254 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11255 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11256 | `			return rc;` |
|        - | 11257 | `		}` |
|        - | 11258 | `		/* Return true */` |
|       12 | 11259 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11260 | `	}else{` |
|        - | 11261 | `		/* Missing arguments,return FALSE */` |
|        3 | 11262 | `		ph7_result_bool(pCtx,0);` |
|        - | 11263 | `	}` |
|       14 | 11264 | `	return rc;` |
|        8 | 11265 |  |
|        - | 11266 | `/*` |
|        - | 11267 | ` * int error_reporting([int $level])` |
|        - | 11268 | ` *  Sets which PHP errors are reported.` |
|        - | 11269 | ` * Parameters` |
|        - | 11270 | ` *  $level` |
|        - | 11271 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11272 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11273 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11274 | ` *   levels will not always behave as expected.` |
|        - | 11275 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11276 | ` *   in the predefined constants.` |
|        - | 11277 | ` * Return` |
|        - | 11278 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11279 | ` *   parameter is given.` |
|        - | 11280 | ` */` |
|       38 | 11281 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11282 |  |
|       40 | 11283 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11284 | `	int nOld;` |
|        - | 11285 | `	/* Extract the old reporting level */` |
|       40 | 11286 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11287 | `	if( nArg > 0 ){` |
|        - | 11288 | `		int nNew;` |
|        - | 11289 | `		/* Extract the desired error reporting level */` |
|       32 | 11290 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11291 | `		if( !nNew ){` |
|        - | 11292 | `			/* Do not report errors at all */` |
|        5 | 11293 | `			pVm->bErrReport = 0;` |
|        3 | 11294 | `		}else{` |
|        - | 11295 | `			/* Report all errors */` |
|       28 | 11296 | `			pVm->bErrReport = 1;` |
|        - | 11297 | `		}` |
|       15 | 11298 | `	}` |
|        - | 11299 | `	/* Return the old level */` |
|       40 | 11300 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 11301 | `	return PH7_OK;` |
|        2 | 11302 |  |
|        - | 11303 | `/*` |
|        - | 11304 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 11305 | ` *  Send an error message somewhere.` |
|        - | 11306 | ` * Parameter` |
|        - | 11307 | ` *  $message` |
|        - | 11308 | ` *   The error message that should be logged.` |
|        - | 11309 | ` *  $message_type` |
|        - | 11310 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 11311 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 11312 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 11313 | ` *       This is the default option.` |
|        - | 11314 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 11315 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 11316 | ` *    2  No longer an option.` |
|        - | 11317 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 11318 | ` *       to the end of the message string.` |
|        - | 11319 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 11320 | ` *  $destination` |
|        - | 11321 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 11322 | ` *  $extra_headers` |
|        - | 11323 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 11324 | ` * Return` |
|        - | 11325 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11326 | ` * NOTE:` |
|        - | 11327 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 11328 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 11329 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 11330 | ` *  Otherwise this function is no-op.` |
|        - | 11331 | ` */` |
|        4 | 11332 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11333 |  |
|        - | 11334 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 11335 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 11336 | `	int iType = 0;` |
|        5 | 11337 | `	if( nArg < 1 ){` |
|        - | 11338 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 11339 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11340 | `		return PH7_OK;` |
|        - | 11341 | `	}` |
|        5 | 11342 | `	if( pVm->xErrLog  ){` |
|        - | 11343 | `		/* Invoke the user callback */` |
|      ! 0 | 11344 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 11345 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 11346 | `		if( nArg > 1 ){` |
|      ! 0 | 11347 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11348 | `			if( nArg > 2 ){` |
|      ! 0 | 11349 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 11350 | `				if( nArg > 3 ){` |
|      ! 0 | 11351 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 11352 | `				}` |
|      ! 0 | 11353 | `			}` |
|      ! 0 | 11354 | `		}` |
|      ! 0 | 11355 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 11356 | `	}` |
|        - | 11357 | `	/* Retun TRUE */` |
|        5 | 11358 | `	ph7_result_bool(pCtx,1);` |
|        5 | 11359 | `	return PH7_OK;` |
|        3 | 11360 |  |
|        - | 11361 | `/*` |
|        - | 11362 | ` * bool restore_exception_handler(void)` |
|        - | 11363 | ` *  Restores the previously defined exception handler function.` |
|        - | 11364 | ` * Parameter` |
|        - | 11365 | ` *  None` |
|        - | 11366 | ` * Return` |
|        - | 11367 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 11368 | ` */` |
|        4 | 11369 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11370 |  |
|        5 | 11371 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11372 | `	ph7_value *pOld,*pNew;` |
|        - | 11373 | `	/* Point to the old and the new handler */` |
|        5 | 11374 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 11375 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 11376 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11377 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11378 | `		SXUNUSED(apArg);` |
|        - | 11379 | `		/* No installed handler,return FALSE */` |
|        5 | 11380 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11381 | `		return PH7_OK;` |
|        - | 11382 | `	}` |
|        - | 11383 | `	/* Copy the old handler */` |
|      ! 0 | 11384 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11385 | `	PH7_MemObjRelease(pOld);` |
|        - | 11386 | `	/* Return TRUE */` |
|      ! 0 | 11387 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11388 | `	return PH7_OK;` |
|        3 | 11389 |  |
|        - | 11390 | `/*` |
|        - | 11391 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 11392 | ` *  Sets a user-defined exception handler function.` |
|        - | 11393 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 11394 | ` * NOTE` |
|        - | 11395 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 11396 | ` *  the satndard PHP engine.` |
|        - | 11397 | ` * Parameters` |
|        - | 11398 | ` *  $exception_handler` |
|        - | 11399 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 11400 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 11401 | ` *   that was thrown.` |
|        - | 11402 | ` *  Note:` |
|        - | 11403 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11404 | ` * Return` |
|        - | 11405 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 11406 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11407 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11408 | ` */` |
|        4 | 11409 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11410 |  |
|        6 | 11411 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11412 | `	ph7_value *pOld,*pNew;` |
|        - | 11413 | `	/* Point to the old and the new handler */` |
|        6 | 11414 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11415 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11416 | `	/* Return the old handler */` |
|        6 | 11417 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11418 | `	if( nArg > 0 ){` |
|        6 | 11419 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11420 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11421 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11422 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11423 | `		}else{` |
|        6 | 11424 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11425 | `			/* Install the new handler */` |
|        6 | 11426 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11427 | `		}` |
|        2 | 11428 | `	}` |
|        6 | 11429 | `	return PH7_OK;` |
|        2 | 11430 |  |
|        - | 11431 | `/*` |
|        - | 11432 | ` * bool restore_error_handler(void)` |
|        - | 11433 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11434 | ` * Parameters:` |
|        - | 11435 | ` *  None.` |
|        - | 11436 | ` * Return` |
|        - | 11437 | ` *  Always TRUE.` |
|        - | 11438 | ` */` |
|        4 | 11439 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11440 |  |
|        5 | 11441 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11442 | `	ph7_value *pOld,*pNew;` |
|        - | 11443 | `	/* Point to the old and the new handler */` |
|        5 | 11444 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 11445 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 11446 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11447 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11448 | `		SXUNUSED(apArg);` |
|        - | 11449 | `		/* No installed callback,return FALSE */` |
|        5 | 11450 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11451 | `		return PH7_OK;` |
|        - | 11452 | `	}` |
|        - | 11453 | `	/* Copy the old callback */` |
|      ! 0 | 11454 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11455 | `	PH7_MemObjRelease(pOld);` |
|        - | 11456 | `	/* Return TRUE */` |
|      ! 0 | 11457 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11458 | `	return PH7_OK;` |
|        3 | 11459 |  |
|        - | 11460 | `/*` |
|        - | 11461 | ` * value set_error_handler(callable $error_handler)` |
|        - | 11462 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11463 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11464 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11465 | ` *  Sets a user-defined error handler function.` |
|        - | 11466 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 11467 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 11468 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 11469 | ` *  conditions (using trigger_error()).` |
|        - | 11470 | ` * Parameters` |
|        - | 11471 | ` *  $error_handler` |
|        - | 11472 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 11473 | ` *   describing the error.` |
|        - | 11474 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 11475 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 11476 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 11477 | ` *   The function can be shown as:` |
|        - | 11478 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 11479 | ` *     errno` |
|        - | 11480 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 11481 | ` *   errstr` |
|        - | 11482 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 11483 | ` *   errfile` |
|        - | 11484 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 11485 | ` *     was raised in, as a string.` |
|        - | 11486 | ` *  Note:` |
|        - | 11487 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11488 | ` * Return` |
|        - | 11489 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 11490 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11491 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11492 | ` */` |
|     9818 | 11493 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11494 |  |
|     9820 | 11495 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11496 | `	ph7_value *pOld,*pNew;` |
|        - | 11497 | `	/* Point to the old and the new handler */` |
|     9820 | 11498 | `	pOld = &pVm->aErrCB[0];` |
|     9820 | 11499 | `	pNew = &pVm->aErrCB[1];` |
|        - | 11500 | `	/* Return the old handler */` |
|     9820 | 11501 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9820 | 11502 | `	if( nArg > 0 ){` |
|     9820 | 11503 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11504 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4909 | 11505 | `			PH7_MemObjRelease(pNew);` |
|     4909 | 11506 | `			ph7_result_bool(pCtx,1);` |
|     2455 | 11507 | `		}else{` |
|     4912 | 11508 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11509 | `			/* Install the new handler */` |
|     4912 | 11510 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11511 | `		}` |
|     4909 | 11512 | `	}` |
|     9820 | 11513 | `	return PH7_OK;` |
|        2 | 11514 |  |
|        - | 11515 | `/*` |
|        - | 11516 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 11517 | ` *  Generates a backtrace.` |
|        - | 11518 | ` * Paramaeter` |
|        - | 11519 | ` *  $options` |
|        - | 11520 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 11521 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 11522 | ` *   all the function/method arguments, to save memory.` |
|        - | 11523 | ` * $limit` |
|        - | 11524 | ` *   (Not Used)` |
|        - | 11525 | ` * Return` |
|        - | 11526 | ` *  An array.The possible returned elements are as follows:` |
|        - | 11527 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 11528 | ` *          Name        Type      Description` |
|        - | 11529 | ` *          ------      ------     -----------` |
|        - | 11530 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 11531 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 11532 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 11533 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 11534 | ` *          object      object    The current object.` |
|        - | 11535 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 11536 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 11537 | ` */` |
|      614 | 11538 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11539 |  |
|      616 | 11540 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11541 | `	ph7_value *pArray;` |
|        - | 11542 | `	ph7_class *pClass;` |
|        - | 11543 | `	ph7_value *pValue;` |
|        - | 11544 | `	SyString *pFile;` |
|        - | 11545 | `	/* Create a new array */` |
|      616 | 11546 | `	pArray = ph7_context_new_array(pCtx);` |
|      616 | 11547 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      616 | 11548 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11549 | `		/* Out of memory,return NULL */` |
|      ! 0 | 11550 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11551 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11552 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11553 | `		SXUNUSED(apArg);` |
|      ! 0 | 11554 | `		return PH7_OK;` |
|        - | 11555 | `	}` |
|        - | 11556 | `	/* Dump running function name and it's arguments  */` |
|      616 | 11557 | `	if( pVm->pFrame->pParent ){` |
|      616 | 11558 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 11559 | `		ph7_vm_func *pFunc;` |
|        - | 11560 | `		ph7_value *pArg;` |
|      616 | 11561 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      616 | 11562 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      616 | 11563 | `		if( pFrame->pParent && pFunc ){` |
|      616 | 11564 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      616 | 11565 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      616 | 11566 | `			ph7_value_reset_string_cursor(pValue);` |
|      307 | 11567 | `		}` |
|        - | 11568 | `		/* Function arguments */` |
|      616 | 11569 | `		pArg = ph7_context_new_array(pCtx);` |
|      616 | 11570 | `		if( pArg  ){` |
|        - | 11571 | `			ph7_value *pObj;` |
|        - | 11572 | `			VmSlot *aSlot;` |
|        - | 11573 | `			sxu32 n;` |
|        - | 11574 | `			/* Start filling the array with the given arguments */` |
|      616 | 11575 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2450 | 11576 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1836 | 11577 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1836 | 11578 | `				if( pObj ){` |
|     1836 | 11579 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      917 | 11580 | `				}` |
|      919 | 11581 | `			}` |
|        - | 11582 | `			/* Save the array */` |
|      616 | 11583 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      307 | 11584 | `		}` |
|      307 | 11585 | `	}` |
|      616 | 11586 | `	ph7_value_int(pValue,1);` |
|        - | 11587 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 11588 | `	 * line numbers at run-time. )` |
|        - | 11589 | `	 */` |
|      616 | 11590 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 11591 | `	/* Current processed script */` |
|      616 | 11592 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      616 | 11593 | `	if( pFile ){` |
|      616 | 11594 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      616 | 11595 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      616 | 11596 | `		ph7_value_reset_string_cursor(pValue);` |
|      307 | 11597 | `	}` |
|        - | 11598 | `	/* Top class */` |
|      616 | 11599 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      616 | 11600 | `	if( pClass ){` |
|      612 | 11601 | `		ph7_value_reset_string_cursor(pValue);` |
|      612 | 11602 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      612 | 11603 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      305 | 11604 | `	}` |
|        - | 11605 | `	/* Return the freshly created array */` |
|      616 | 11606 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11607 | `	/*` |
|        - | 11608 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 11609 | `	 * as soon we return from this function.` |
|        - | 11610 | `	 */` |
|      616 | 11611 | `	return PH7_OK;` |
|      309 | 11612 |  |
|        - | 11613 | `/*` |
|        - | 11614 | ` * Generate a small backtrace.` |
|        - | 11615 | ` * Store the generated dump in the given BLOB` |
|        - | 11616 | ` */` |
|        4 | 11617 | `static int VmMiniBacktrace(` |
|        - | 11618 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11619 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 11620 | `	)` |
|        1 | 11621 |  |
|        5 | 11622 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11623 | `	ph7_vm_func *pFunc;` |
|        - | 11624 | `	ph7_class *pClass;` |
|        - | 11625 | `	SyString *pFile;` |
|        - | 11626 | `	/* Called function */` |
|        5 | 11627 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 11628 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 11629 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11630 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 11631 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 11632 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 11633 | `	}else{` |
|      ! 0 | 11634 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 11635 | `	}` |
|        5 | 11636 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 11637 | `	/* Current processed script */` |
|        5 | 11638 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 11639 | `	if( pFile ){` |
|        5 | 11640 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11641 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 11642 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 11643 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 11644 | `	}` |
|        - | 11645 | `	/* Top class */` |
|        5 | 11646 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 11647 | `	if( pClass ){` |
|      ! 0 | 11648 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 11649 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 11650 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 11651 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11652 | `	}` |
|        5 | 11653 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11654 | `	/* All done */` |
|        5 | 11655 | `	return SXRET_OK;` |
|        1 | 11656 |  |
|        - | 11657 | `/*` |
|        - | 11658 | ` * void debug_print_backtrace()` |
|        - | 11659 | ` *  Prints a backtrace` |
|        - | 11660 | ` * Parameters` |
|        - | 11661 | ` * None` |
|        - | 11662 | ` * Return` |
|        - | 11663 | ` * NULL` |
|        - | 11664 | ` */` |
|        2 | 11665 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11666 |  |
|        3 | 11667 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11668 | `	SyBlob sDump;` |
|        3 | 11669 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11670 | `	/* Generate the backtrace */` |
|        3 | 11671 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11672 | `	/* Output backtrace */` |
|        3 | 11673 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11674 | `	/* All done,cleanup */` |
|        3 | 11675 | `	SyBlobRelease(&sDump);` |
|        1 | 11676 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11677 | `	SXUNUSED(apArg);` |
|        3 | 11678 | `	return PH7_OK;` |
|        1 | 11679 |  |
|        - | 11680 | `/*` |
|        - | 11681 | ` * string debug_string_backtrace()` |
|        - | 11682 | ` *  Generate a backtrace` |
|        - | 11683 | ` * Parameters` |
|        - | 11684 | ` * None` |
|        - | 11685 | ` * Return` |
|        - | 11686 | ` *  A mini backtrace().` |
|        - | 11687 | ` * Note that this is a symisc extension.` |
|        - | 11688 | ` */` |
|        2 | 11689 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11690 |  |
|        3 | 11691 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11692 | `	SyBlob sDump;` |
|        3 | 11693 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11694 | `	/* Generate the backtrace */` |
|        3 | 11695 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11696 | `	/* Return the backtrace */` |
|        3 | 11697 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11698 | `	/* All done,cleanup */` |
|        3 | 11699 | `	SyBlobRelease(&sDump);` |
|        1 | 11700 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11701 | `	SXUNUSED(apArg);` |
|        3 | 11702 | `	return PH7_OK;` |
|        1 | 11703 |  |
|        - | 11704 | `/*` |
|        - | 11705 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11706 | ` * exception is triggered.` |
|        - | 11707 | ` */` |
|      480 | 11708 | `static sxi32 VmUncaughtException(` |
|        - | 11709 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11710 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11711 | `	)` |
|        1 | 11712 |  |
|        - | 11713 | `	ph7_value *apArg[2],sArg;` |
|      481 | 11714 | `	int nArg = 1;` |
|        - | 11715 | `	sxi32 rc;` |
|      481 | 11716 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11717 | `		/* Nesting limit reached */` |
|      ! 0 | 11718 | `		return SXRET_OK;` |
|        - | 11719 | `	}` |
|        - | 11720 | `	/* Call any exception handler if available */` |
|      481 | 11721 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 11722 | `	if( pThis ){` |
|        - | 11723 | `		/* Load the exception instance */` |
|      481 | 11724 | `		sArg.x.pOther = pThis;` |
|      481 | 11725 | `		pThis->iRef++;` |
|      481 | 11726 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 11727 | `	}else{` |
|      ! 0 | 11728 | `		nArg = 0;` |
|        - | 11729 | `	}` |
|      481 | 11730 | `	apArg[0] = &sArg;` |
|        - | 11731 | `	/* Call the exception handler if available */` |
|      481 | 11732 | `	pVm->nExceptDepth++;` |
|      481 | 11733 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 11734 | `	pVm->nExceptDepth--;` |
|      481 | 11735 | `	if( rc != SXRET_OK ){` |
|        - | 11736 | `		SyBlob sMsgBuf;` |
|      479 | 11737 | `		const char *zClass = "Exception";` |
|      479 | 11738 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11739 | `		const char *zMsg;` |
|        - | 11740 | `		sxu32 nMsg;` |
|        - | 11741 | `		const char *zFuncName;` |
|        - | 11742 | `		int nFuncLen;` |
|      479 | 11743 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 11744 | `		if( pThis ){` |
|        - | 11745 | `			ph7_class_method *pGetMessage;` |
|        - | 11746 | `			ph7_value sMsg;` |
|        - | 11747 | `			const char *zTmp;` |
|        - | 11748 | `			int nTmp;` |
|      479 | 11749 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 11750 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 11751 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 11752 | `			if( pGetMessage ){` |
|      479 | 11753 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 11754 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 11755 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 11756 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 11757 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 11758 | `					}` |
|      239 | 11759 | `				}` |
|      479 | 11760 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 11761 | `			}` |
|      239 | 11762 | `		}` |
|      479 | 11763 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 11764 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 11765 | `		}` |
|      479 | 11766 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 11767 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 11768 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 11769 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 11770 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11771 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 11772 | `		rc = SXERR_ABORT;` |
|      239 | 11773 | `	}` |
|      481 | 11774 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 11775 | `	return rc;` |
|      241 | 11776 |  |
|        - | 11777 | `/*` |
|        - | 11778 | ` * Throw a user exception.` |
|        - | 11779 | ` *` |
|        - | 11780 | ` * Exception dispatch follows this sequence:` |
|        - | 11781 | ` *` |
|        - | 11782 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11783 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11784 | ` *` |
|        - | 11785 | ` * 2. If NO catch matches:` |
|        - | 11786 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11787 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11788 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11789 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11790 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11791 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11792 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11793 | ` *` |
|        - | 11794 | ` * 3. If a catch DOES match:` |
|        - | 11795 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11796 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11797 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11798 | ` *       finally block.` |
|        - | 11799 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11800 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11801 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11802 | ` *       in pPendingException (step 2c).` |
|        - | 11803 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11804 | ` *    d. Run finally (if present).` |
|        - | 11805 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11806 | ` *       that handlers are restored and finally has run.` |
|        - | 11807 | ` */` |
|      618 | 11808 | `static sxi32 VmThrowException(` |
|        - | 11809 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11810 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11811 | `	)` |
|        2 | 11812 |  |
|        - | 11813 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11814 | `	ph7_exception **apException;` |
|        - | 11815 | `	ph7_exception *pException;` |
|        - | 11816 | `	/* Point to the stack of loaded exceptions */` |
|      620 | 11817 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      620 | 11818 | `	pException = 0;` |
|      620 | 11819 | `	pCatch = 0;` |
|      620 | 11820 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11821 | `		ph7_exception_block *aCatch;` |
|        - | 11822 | `		ph7_class *pClass;` |
|        - | 11823 | `		SyString *aNames;` |
|        - | 11824 | `		sxu32 nNames;` |
|        - | 11825 | `		int matched;` |
|        - | 11826 | `		sxu32 j,k;` |
|        - | 11827 | `		/* Locate the appropriate block to execute */` |
|      134 | 11828 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      134 | 11829 | `		(void)SySetPop(&pVm->aException);` |
|      134 | 11830 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      136 | 11831 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 11832 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      134 | 11833 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      134 | 11834 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      134 | 11835 | `			matched = 0;` |
|      148 | 11836 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 11837 | `				/* Extract the target class */` |
|      146 | 11838 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|      146 | 11839 | `				if( pClass == 0 ){` |
|        - | 11840 | `					/* No such class */` |
|      ! 0 | 11841 | `					continue;` |
|        - | 11842 | `				}` |
|      146 | 11843 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      132 | 11844 | `					matched = 1;` |
|      132 | 11845 | `					break;` |
|        - | 11846 | `				}` |
|        8 | 11847 | `			}` |
|      134 | 11848 | `			if( matched ){` |
|        - | 11849 | `				/* Catch block found,break immediately */` |
|      132 | 11850 | `				pCatch = &aCatch[j];` |
|      132 | 11851 | `				break;` |
|        - | 11852 | `			}` |
|        2 | 11853 | `		}` |
|       66 | 11854 | `	}` |
|        - | 11855 | `	/* Execute the cached block if available */` |
|      620 | 11856 | `	if( pCatch == 0 ){` |
|        - | 11857 | `		sxi32 rc;` |
|        - | 11858 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 11859 | `		if( pException && pException->iHasFinally ){` |
|        3 | 11860 | `			pException->iFinallyDone = 1;` |
|        3 | 11861 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 11862 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11863 | `				return SXERR_ABORT;` |
|        - | 11864 | `			}` |
|        1 | 11865 | `		}` |
|        - | 11866 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 11867 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11868 | `			/* Re-throw to the outer handler */` |
|        3 | 11869 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 11870 | `		}` |
|        - | 11871 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 11872 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 11873 | `		 * exception instead of reporting it uncaught.` |
|        - | 11874 | `		 */` |
|      488 | 11875 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 11876 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 11877 | `			 * by looking for a catch frame on the stack.` |
|        - | 11878 | `			 */` |
|      488 | 11879 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 11880 | `			int inCatch = 0;` |
|      974 | 11881 | `			while( pF ){` |
|      494 | 11882 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 11883 | `					inCatch = 1;` |
|        7 | 11884 | `					break;` |
|        - | 11885 | `				}` |
|      487 | 11886 | `				pF = pF->pParent;` |
|        1 | 11887 | `			}` |
|      488 | 11888 | `			if( inCatch ){` |
|        - | 11889 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 11890 | `				pThis->iRef++;` |
|        7 | 11891 | `				pVm->pPendingException = pThis;` |
|        7 | 11892 | `				return SXRET_OK;` |
|        - | 11893 | `			}` |
|      240 | 11894 | `		}` |
|        - | 11895 | `		/* Truly uncaught */` |
|      481 | 11896 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 11897 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 11898 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 11899 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 11900 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 11901 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 11902 | `			}` |
|      ! 0 | 11903 | `		}` |
|      481 | 11904 | `		return rc;` |
|      ! 0 | 11905 | `	}else{` |
|      132 | 11906 | `		VmFrame *pFrame = pVm->pFrame;` |
|      132 | 11907 | `		ph7_exception **apSaved = 0;` |
|        - | 11908 | `		sxu32 nSavedCount;` |
|        - | 11909 | `		sxi32 rc;` |
|      132 | 11910 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      132 | 11911 | `		if( pException->pFrame == pFrame ){` |
|       88 | 11912 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       43 | 11913 | `		}` |
|        - | 11914 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 11915 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 11916 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 11917 | `		 */` |
|      132 | 11918 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      132 | 11919 | `		if( nSavedCount > 0 ){` |
|       13 | 11920 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 11921 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11922 | `			if( apSaved ){` |
|       13 | 11923 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 11924 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11925 | `				SySetReset(&pVm->aException);` |
|        4 | 11926 | `			}` |
|        4 | 11927 | `		}` |
|        - | 11928 | `		/* Create a private frame first */` |
|      132 | 11929 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      132 | 11930 | `		if( rc == SXRET_OK ){` |
|      132 | 11931 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      132 | 11932 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      132 | 11933 | `			if( pObj ){` |
|      132 | 11934 | `				pThis->iRef++;` |
|      132 | 11935 | `				pObj->x.pOther = pThis;` |
|      132 | 11936 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       65 | 11937 | `			}` |
|        - | 11938 | `			/* Execute the catch block */` |
|      132 | 11939 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 11940 | `			/* Leave the frame */` |
|      132 | 11941 | `			VmLeaveFrame(&(*pVm));` |
|       65 | 11942 | `		}` |
|        - | 11943 | `		/* Restore the outer exception handlers */` |
|      132 | 11944 | `		if( apSaved ){` |
|        - | 11945 | `			sxu32 k;` |
|        - | 11946 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 11947 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 11948 | `			 * Restore the original outer entries.` |
|        - | 11949 | `			 */` |
|        9 | 11950 | `			SySetReset(&pVm->aException);` |
|       17 | 11951 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 11952 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 11953 | `			}` |
|        9 | 11954 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 11955 | `		}` |
|        - | 11956 | `		/* Execute the finally block after catch */` |
|      132 | 11957 | `		if( pException->iHasFinally ){` |
|       16 | 11958 | `			pException->iFinallyDone = 1;` |
|        - | 11959 | `			{` |
|       16 | 11960 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 11961 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 11962 | `					return SXERR_ABORT;` |
|        - | 11963 | `				}` |
|        - | 11964 | `			}` |
|        7 | 11965 | `		}` |
|      132 | 11966 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11967 | `			return SXERR_ABORT;` |
|        - | 11968 | `		}` |
|        - | 11969 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 11970 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 11971 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 11972 | `		 */` |
|      132 | 11973 | `		if( pVm->pPendingException ){` |
|        7 | 11974 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 11975 | `			pVm->pPendingException = 0;` |
|        7 | 11976 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 11977 | `		}` |
|        - | 11978 | `	}` |
|        - | 11979 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 11980 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 11981 | `	 */` |
|      126 | 11982 | `	return SXRET_OK;` |
|      311 | 11983 |  |
|        - | 11984 | `/*` |
|        - | 11985 | ` * Section:` |
|        - | 11986 | ` *  Version,Credits and Copyright related functions.` |
|        - | 11987 | ` * Status:` |
|        - | 11988 | ` *    Stable.` |
|        - | 11989 | ` */` |
|        - | 11990 | `/*` |
|        - | 11991 | ` * string ph7version(void)` |
|        - | 11992 | ` *  Returns the running version of the PH7 version.` |
|        - | 11993 | ` * Parameters` |
|        - | 11994 | ` *  None` |
|        - | 11995 | ` * Return` |
|        - | 11996 | ` * Current PH7 version.` |
|        - | 11997 | ` */` |
|        2 | 11998 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11999 |  |
|        1 | 12000 | `	SXUNUSED(nArg);` |
|        1 | 12001 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12002 | `	/* Current engine version */` |
|        3 | 12003 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12004 | `	return PH7_OK;` |
|        1 | 12005 |  |
|        - | 12006 | `/*` |
|        - | 12007 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12008 | ` */` |
|        - | 12009 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12010 | ` "<html><head>"\` |
|        - | 12011 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12012 | ` "<style type=\"text/css\">"\` |
|        - | 12013 | ` "div {"\` |
|        - | 12014 | `     "border: 1px solid #cccccc;"\` |
|        - | 12015 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12016 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12017 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12018 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12019 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12020 | `     "-o-border-radius: 10px;"\` |
|        - | 12021 | `     "border-radius: 10px;"\` |
|        - | 12022 | `     "padding-left: 2em;"\` |
|        - | 12023 | `     "background-color: white;"\` |
|        - | 12024 | `     "margin-left: auto;"\` |
|        - | 12025 | `     "font-family: verdana;"\` |
|        - | 12026 | `     "padding-right: 2em;"\` |
|        - | 12027 | `     "margin-right: auto;"\` |
|        - | 12028 | `     "}"\` |
|        - | 12029 | `     "body {"\` |
|        - | 12030 | `     "padding: 0.2em;"\` |
|        - | 12031 | `     "font-style: normal;"\` |
|        - | 12032 | `     "font-size: medium;"\` |
|        - | 12033 | `     "background-color: #f2f2f2;"\` |
|        - | 12034 | `     "}"\` |
|        - | 12035 | `     "hr {"\` |
|        - | 12036 | `     "border-style: solid none none;"\` |
|        - | 12037 | `     "border-width: 1px medium medium;"\` |
|        - | 12038 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12039 | `     "height: 1px;"\` |
|        - | 12040 | `     "}"\` |
|        - | 12041 | `     "a {"\` |
|        - | 12042 | `     "color: #3366cc;"\` |
|        - | 12043 | `     "text-decoration: none;"\` |
|        - | 12044 | `     "}"\` |
|        - | 12045 | `     "a:hover {"\` |
|        - | 12046 | `     "color: #999999;"\` |
|        - | 12047 | `     "}"\` |
|        - | 12048 | `     "a:active {"\` |
|        - | 12049 | `     "color: #663399;"\` |
|        - | 12050 | `     "}"\` |
|        - | 12051 | `     "h1 {"\` |
|        - | 12052 | `     "margin: 0;"\` |
|        - | 12053 | `     "padding: 0;"\` |
|        - | 12054 | `     "font-family: Verdana;"\` |
|        - | 12055 | `     "font-weight: bold;"\` |
|        - | 12056 | `     "font-style: normal;"\` |
|        - | 12057 | `     "font-size: medium;"\` |
|        - | 12058 | `     "text-transform: capitalize;"\` |
|        - | 12059 | `     "color: #0a328c;"\` |
|        - | 12060 | `     "}"\` |
|        - | 12061 | `     "p {"\` |
|        - | 12062 | `     "margin: 0 auto;"\` |
|        - | 12063 | `     "font-size: medium;"\` |
|        - | 12064 | `     "font-style: normal;"\` |
|        - | 12065 | `     "font-family: verdana;"\` |
|        - | 12066 | `     "}"\` |
|        - | 12067 | `"</style></head><body>"\` |
|        - | 12068 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12069 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12070 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12071 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12072 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12073 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12074 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12075 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12076 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12077 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12078 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12079 |  |
|        - | 12080 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12081 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12082 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12083 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12084 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12085 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12086 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12087 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12088 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12089 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12090 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12091 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12092 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12093 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12094 |  |
|        - | 12095 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12096 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12097 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12098 | `"&nbsp;*<br>"\` |
|        - | 12099 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12100 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12101 | `"&nbsp;* are met:<br>"\` |
|        - | 12102 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12103 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12104 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12105 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12106 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12107 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12108 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12109 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12110 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12111 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12112 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12113 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12114 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12115 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12116 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12117 | `"&nbsp;*<br>"\` |
|        - | 12118 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12119 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12120 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12121 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12122 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12123 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12124 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12125 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12126 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12127 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12128 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12129 | `"&nbsp;*/<br>"\` |
|        - | 12130 | `"</span></small></small></p>"\` |
|        - | 12131 | `"</div></body></html>"` |
|        - | 12132 | `/*` |
|        - | 12133 | ` * bool ph7credits(void)` |
|        - | 12134 | ` * bool ph7info(void)` |
|        - | 12135 | ` * bool ph7copyright(void)` |
|        - | 12136 | ` *  Prints out the credits for PH7 engine` |
|        - | 12137 | ` * Parameters` |
|        - | 12138 | ` *  None` |
|        - | 12139 | ` * Return` |
|        - | 12140 | ` *  Always TRUE` |
|        - | 12141 | ` */` |
|        2 | 12142 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12143 |  |
|        3 | 12144 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12145 | `	/* Expand the HTML page above*/` |
|        3 | 12146 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12147 | `	ph7_context_output_format(` |
|        1 | 12148 | `		pCtx,` |
|        - | 12149 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12150 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12151 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12152 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12153 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12154 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12155 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12156 | `#ifdef __WINNT__` |
|        - | 12157 | `		"Windows NT"` |
|        - | 12158 | `#elif defined(__UNIXES__)` |
|        - | 12159 | `		"UNIX-Like"` |
|        - | 12160 | `#else` |
|        - | 12161 | `		"Other OS"` |
|        - | 12162 | `#endif` |
|        - | 12163 | `		);` |
|        3 | 12164 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12165 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12166 | `	SXUNUSED(apArg);` |
|        - | 12167 | `	/* Return TRUE */` |
|        - | 12168 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12169 | `	return PH7_OK;` |
|        1 | 12170 |  |
|        - | 12171 | `/*` |
|        - | 12172 | ` * Section:` |
|        - | 12173 | ` *    URL related routines.` |
|        - | 12174 | ` * Status:` |
|        - | 12175 | ` *    Stable.` |
|        - | 12176 | ` */` |
|        - | 12177 | `/*` |
|        - | 12178 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12179 | ` *  Parse a URL and return its fields.` |
|        - | 12180 | ` * Parameters` |
|        - | 12181 | ` *  $url` |
|        - | 12182 | ` *   The URL to parse.` |
|        - | 12183 | ` * $component` |
|        - | 12184 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12185 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12186 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12187 | ` *  in which case the return value will be an integer).` |
|        - | 12188 | ` * Return` |
|        - | 12189 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12190 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12191 | ` *  this array are:` |
|        - | 12192 | ` *   scheme - e.g. http` |
|        - | 12193 | ` *   host` |
|        - | 12194 | ` *   port` |
|        - | 12195 | ` *   user` |
|        - | 12196 | ` *   pass` |
|        - | 12197 | ` *   path` |
|        - | 12198 | ` *   query - after the question mark ?` |
|        - | 12199 | ` *   fragment - after the hashmark #` |
|        - | 12200 | ` * Note:` |
|        - | 12201 | ` *  FALSE is returned on failure.` |
|        - | 12202 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12203 | ` *  with the standard PHP engine.` |
|        - | 12204 | ` */` |
|       28 | 12205 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12206 |  |
|        - | 12207 | `	const char *zStr; /* Input string */` |
|        - | 12208 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12209 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12210 | `	int nLen;` |
|        - | 12211 | `	sxi32 rc;` |
|       29 | 12212 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12213 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12214 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12215 | `		return PH7_OK;` |
|        - | 12216 | `	}` |
|        - | 12217 | `	/* Extract the given URI */` |
|       29 | 12218 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12219 | `	if( nLen < 1 ){` |
|        - | 12220 | `		/* Nothing to process,return FALSE */` |
|        3 | 12221 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12222 | `		return PH7_OK;` |
|        - | 12223 | `	}` |
|        - | 12224 | `	/* Get a parse */` |
|       27 | 12225 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12226 | `	if( rc != SXRET_OK ){` |
|        - | 12227 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12229 | `		return PH7_OK;` |
|        - | 12230 | `	}` |
|       27 | 12231 | `	if( nArg > 1 ){` |
|      ! 0 | 12232 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12233 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12234 | `		switch(nComponent){` |
|      ! 0 | 12235 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12236 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12237 | `			if( pComp->nByte < 1 ){` |
|        - | 12238 | `				/* No available value,return NULL */` |
|      ! 0 | 12239 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12240 | `			}else{` |
|      ! 0 | 12241 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12242 | `			}` |
|      ! 0 | 12243 | `			break;` |
|      ! 0 | 12244 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12245 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12246 | `			if( pComp->nByte < 1 ){` |
|        - | 12247 | `				/* No available value,return NULL */` |
|      ! 0 | 12248 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12249 | `			}else{` |
|      ! 0 | 12250 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12251 | `			}` |
|      ! 0 | 12252 | `			break;` |
|      ! 0 | 12253 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12254 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12255 | `			if( pComp->nByte < 1 ){` |
|        - | 12256 | `				/* No available value,return NULL */` |
|      ! 0 | 12257 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12258 | `			}else{` |
|      ! 0 | 12259 | `				int iPort = 0;` |
|        - | 12260 | `				/* Cast the value to integer */` |
|      ! 0 | 12261 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12262 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12263 | `			}` |
|      ! 0 | 12264 | `			break;` |
|      ! 0 | 12265 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12266 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12267 | `			if( pComp->nByte < 1 ){` |
|        - | 12268 | `				/* No available value,return NULL */` |
|      ! 0 | 12269 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12270 | `			}else{` |
|      ! 0 | 12271 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12272 | `			}` |
|      ! 0 | 12273 | `			break;` |
|      ! 0 | 12274 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12275 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12276 | `			if( pComp->nByte < 1 ){` |
|        - | 12277 | `				/* No available value,return NULL */` |
|      ! 0 | 12278 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12279 | `			}else{` |
|      ! 0 | 12280 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12281 | `			}` |
|      ! 0 | 12282 | `			break;` |
|      ! 0 | 12283 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12284 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12285 | `			if( pComp->nByte < 1 ){` |
|        - | 12286 | `				/* No available value,return NULL */` |
|      ! 0 | 12287 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12288 | `			}else{` |
|      ! 0 | 12289 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12290 | `			}` |
|      ! 0 | 12291 | `			break;` |
|      ! 0 | 12292 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 12293 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 12294 | `			if( pComp->nByte < 1 ){` |
|        - | 12295 | `				/* No available value,return NULL */` |
|      ! 0 | 12296 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12297 | `			}else{` |
|      ! 0 | 12298 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12299 | `			}` |
|      ! 0 | 12300 | `			break;` |
|      ! 0 | 12301 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 12302 | `			pComp = &sURI.sPath;` |
|      ! 0 | 12303 | `			if( pComp->nByte < 1 ){` |
|        - | 12304 | `				/* No available value,return NULL */` |
|      ! 0 | 12305 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12306 | `			}else{` |
|      ! 0 | 12307 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12308 | `			}` |
|      ! 0 | 12309 | `			break;` |
|      ! 0 | 12310 | `		default:` |
|        - | 12311 | `			/* No such entry,return NULL */` |
|      ! 0 | 12312 | `			ph7_result_null(pCtx);` |
|      ! 0 | 12313 | `			break;` |
|        - | 12314 | `		}` |
|      ! 0 | 12315 | `	}else{` |
|        - | 12316 | `		ph7_value *pArray,*pValue;` |
|        - | 12317 | `		/* Return an associative array */` |
|       27 | 12318 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 12319 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 12320 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12321 | `			/* Out of memory */` |
|      ! 0 | 12322 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12323 | `			/* Return false */` |
|      ! 0 | 12324 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 12325 | `			return PH7_OK;` |
|        - | 12326 | `		}` |
|        - | 12327 | `		/* Fill the array */` |
|       27 | 12328 | `		pComp = &sURI.sScheme;` |
|       27 | 12329 | `		if( pComp->nByte > 0 ){` |
|       19 | 12330 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 12331 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 12332 | `		}` |
|        - | 12333 | `		/* Reset the string cursor */` |
|       27 | 12334 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12335 | `		pComp = &sURI.sHost;` |
|       27 | 12336 | `		if( pComp->nByte > 0 ){` |
|       25 | 12337 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 12338 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 12339 | `		}` |
|        - | 12340 | `		/* Reset the string cursor */` |
|       27 | 12341 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12342 | `		pComp = &sURI.sPort;` |
|       27 | 12343 | `		if( pComp->nByte > 0 ){` |
|       11 | 12344 | `			int iPort = 0;/* cc warning */` |
|        - | 12345 | `			/* Convert to integer */` |
|       11 | 12346 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 12347 | `			ph7_value_int(pValue,iPort);` |
|       11 | 12348 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 12349 | `		}` |
|        - | 12350 | `		/* Reset the string cursor */` |
|       27 | 12351 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12352 | `		pComp = &sURI.sUser;` |
|       27 | 12353 | `		if( pComp->nByte > 0 ){` |
|        7 | 12354 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12355 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 12356 | `		}` |
|        - | 12357 | `		/* Reset the string cursor */` |
|       27 | 12358 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12359 | `		pComp = &sURI.sPass;` |
|       27 | 12360 | `		if( pComp->nByte > 0 ){` |
|        7 | 12361 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12362 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 12363 | `		}` |
|        - | 12364 | `		/* Reset the string cursor */` |
|       27 | 12365 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12366 | `		pComp = &sURI.sPath;` |
|       27 | 12367 | `		if( pComp->nByte > 0 ){` |
|       17 | 12368 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 12369 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 12370 | `		}` |
|        - | 12371 | `		/* Reset the string cursor */` |
|       27 | 12372 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12373 | `		pComp = &sURI.sQuery;` |
|       27 | 12374 | `		if( pComp->nByte > 0 ){` |
|        5 | 12375 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12376 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 12377 | `		}` |
|        - | 12378 | `		/* Reset the string cursor */` |
|       27 | 12379 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12380 | `		pComp = &sURI.sFragment;` |
|       27 | 12381 | `		if( pComp->nByte > 0 ){` |
|        5 | 12382 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12383 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 12384 | `		}` |
|        - | 12385 | `		/* Return the created array */` |
|       27 | 12386 | `		ph7_result_value(pCtx,pArray);` |
|        - | 12387 | `		/* NOTE:` |
|        - | 12388 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 12389 | `		 * automatically as soon we return from this function.` |
|        - | 12390 | `		 */` |
|        - | 12391 | `	}` |
|        - | 12392 | `	/* All done */` |
|       27 | 12393 | `	return PH7_OK;` |
|       15 | 12394 |  |
|        - | 12395 | `/*` |
|        - | 12396 | ` * Section:` |
|        - | 12397 | ` *   Array related routines.` |
|        - | 12398 | ` * Status:` |
|        - | 12399 | ` *    Stable.` |
|        - | 12400 | ` * Note 2012-5-21 01:04:15:` |
|        - | 12401 | ` *  Array related functions that need access to the underlying` |
|        - | 12402 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 12403 | ` */` |
|        - | 12404 | `/*` |
|        - | 12405 | ` * The [compact()] function store it's state information in an instance` |
|        - | 12406 | ` * of the following structure.` |
|        - | 12407 | ` */` |
|        - | 12408 | `struct compact_data` |
|        - | 12409 |  |
|        - | 12410 | `	ph7_value *pArray;  /* Target array */` |
|        - | 12411 | `	int nRecCount;      /* Recursion count */` |
|        - | 12412 | `};` |
|        - | 12413 | `/*` |
|        - | 12414 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12415 | ` */` |
|      ! 0 | 12416 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12417 |  |
|      ! 0 | 12418 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12419 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12420 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12421 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12422 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12423 | `		SyString sVar;` |
|      ! 0 | 12424 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12425 | `		if( sVar.nByte > 0 ){` |
|        - | 12426 | `			/* Query the current frame */` |
|      ! 0 | 12427 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12428 | `			/* ^` |
|        - | 12429 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12430 | `			 */` |
|      ! 0 | 12431 | `			if( pKey ){` |
|        - | 12432 | `				/* Perform the insertion */` |
|      ! 0 | 12433 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12434 | `			}` |
|      ! 0 | 12435 | `		}` |
|      ! 0 | 12436 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12437 | `		int rc;` |
|        - | 12438 | `		/* Recursively traverse this array */` |
|      ! 0 | 12439 | `		pData->nRecCount++;` |
|      ! 0 | 12440 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12441 | `		pData->nRecCount--;` |
|      ! 0 | 12442 | `		return rc;` |
|        - | 12443 | `	}` |
|      ! 0 | 12444 | `	return SXRET_OK;` |
|      ! 0 | 12445 |  |
|        - | 12446 | `/*` |
|        - | 12447 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12448 | ` *  Create array containing variables and their values.` |
|        - | 12449 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12450 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12451 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12452 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12453 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12454 | ` * Parameters` |
|        - | 12455 | ` *  $varname` |
|        - | 12456 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12457 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12458 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12459 | ` *   it recursively.` |
|        - | 12460 | ` * Return` |
|        - | 12461 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 12462 | ` */` |
|        2 | 12463 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12464 |  |
|        - | 12465 | `	ph7_value *pArray,*pObj;` |
|        3 | 12466 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12467 | `	const char *zName;` |
|        - | 12468 | `	SyString sVar;` |
|        - | 12469 | `	int i,nLen;` |
|        3 | 12470 | `	if( nArg < 1 ){` |
|        - | 12471 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 12472 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12473 | `		return PH7_OK;` |
|        - | 12474 | `	}` |
|        - | 12475 | `	/* Create the array */` |
|        3 | 12476 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12477 | `	if( pArray == 0 ){` |
|        - | 12478 | `		/* Out of memory */` |
|      ! 0 | 12479 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12480 | `		/* Return NULL */` |
|      ! 0 | 12481 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12482 | `		return PH7_OK;` |
|        - | 12483 | `	}` |
|        - | 12484 | `	/* Perform the requested operation */` |
|        7 | 12485 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 12486 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 12487 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 12488 | `				struct compact_data sData;` |
|      ! 0 | 12489 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 12490 | `				/* Recursively walk the array */` |
|      ! 0 | 12491 | `				sData.nRecCount = 0;` |
|      ! 0 | 12492 | `				sData.pArray = pArray;` |
|      ! 0 | 12493 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 12494 | `			}` |
|      ! 0 | 12495 | `		}else{` |
|        - | 12496 | `			/* Extract variable name */` |
|        5 | 12497 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 12498 | `			if( nLen > 0 ){` |
|        5 | 12499 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 12500 | `				/* Check if the variable is available in the current frame */` |
|        5 | 12501 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 12502 | `				if( pObj ){` |
|        5 | 12503 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 12504 | `				}` |
|        2 | 12505 | `			}` |
|        - | 12506 | `		}` |
|        3 | 12507 | `	}` |
|        - | 12508 | `	/* Return the array */` |
|        3 | 12509 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12510 | `	return PH7_OK;` |
|        2 | 12511 |  |
|        - | 12512 | `/*` |
|        - | 12513 | ` * The [extract()] function store it's state information in an instance` |
|        - | 12514 | ` * of the following structure.` |
|        - | 12515 | ` */` |
|        - | 12516 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 12517 | `struct extract_aux_data` |
|        - | 12518 |  |
|        - | 12519 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 12520 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 12521 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 12522 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 12523 | `	int iFlags;           /* Control flags */` |
|        - | 12524 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 12525 | `};` |
|        - | 12526 | `/* Forward declaration */` |
|        - | 12527 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 12528 | `/*` |
|        - | 12529 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 12530 | ` *   Import variables into the current symbol table from an array.` |
|        - | 12531 | ` * Parameters` |
|        - | 12532 | ` * $var_array` |
|        - | 12533 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 12534 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 12535 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 12536 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 12537 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 12538 | ` * $extract_type` |
|        - | 12539 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 12540 | ` *  It can be one of the following values:` |
|        - | 12541 | ` *   EXTR_OVERWRITE` |
|        - | 12542 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 12543 | ` *   EXTR_SKIP` |
|        - | 12544 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 12545 | ` *   EXTR_PREFIX_SAME` |
|        - | 12546 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 12547 | ` *   EXTR_PREFIX_ALL` |
|        - | 12548 | ` *       Prefix all variable names with prefix.` |
|        - | 12549 | ` *   EXTR_PREFIX_INVALID` |
|        - | 12550 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 12551 | ` *   EXTR_IF_EXISTS` |
|        - | 12552 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 12553 | ` *       otherwise do nothing.` |
|        - | 12554 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 12555 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 12556 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 12557 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 12558 | ` *      the current symbol table.` |
|        - | 12559 | ` * $prefix` |
|        - | 12560 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 12561 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 12562 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 12563 | ` *  underscore character.` |
|        - | 12564 | ` * Return` |
|        - | 12565 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 12566 | ` */` |
|        4 | 12567 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12568 |  |
|        - | 12569 | `	extract_aux_data sAux;` |
|        - | 12570 | `	ph7_hashmap *pMap;` |
|        5 | 12571 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 12572 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 12573 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12574 | `		return PH7_OK;` |
|        - | 12575 | `	}` |
|        - | 12576 | `	/* Point to the target hashmap */` |
|        5 | 12577 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 12578 | `	if( pMap->nEntry < 1 ){` |
|        - | 12579 | `		/* Empty map,return  0 */` |
|      ! 0 | 12580 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12581 | `		return PH7_OK;` |
|        - | 12582 | `	}` |
|        - | 12583 | `	/* Prepare the aux data */` |
|        5 | 12584 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 12585 | `	if( nArg > 1 ){` |
|        3 | 12586 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 12587 | `		if( nArg > 2 ){` |
|      ! 0 | 12588 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 12589 | `		}` |
|        1 | 12590 | `	}` |
|        5 | 12591 | `	sAux.pVm = pCtx->pVm;` |
|        - | 12592 | `	/* Invoke the worker callback */` |
|        5 | 12593 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 12594 | `	/* Number of variables successfully imported */` |
|        5 | 12595 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 12596 | `	return PH7_OK;` |
|        3 | 12597 |  |
|        - | 12598 | `/*` |
|        - | 12599 | ` * Worker callback for the [extract()] function defined` |
|        - | 12600 | ` * below.` |
|        - | 12601 | ` */` |
|        8 | 12602 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12603 |  |
|        9 | 12604 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 12605 | `	int iFlags = pAux->iFlags;` |
|        9 | 12606 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12607 | `	ph7_value *pObj;` |
|        - | 12608 | `	SyString sVar;` |
|        9 | 12609 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 12610 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 12611 | `	}` |
|        - | 12612 | `	/* Perform a string cast */` |
|        9 | 12613 | `	PH7_MemObjToString(pKey);` |
|        9 | 12614 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12615 | `		/* Unavailable variable name */` |
|      ! 0 | 12616 | `		return SXRET_OK;` |
|        - | 12617 | `	}` |
|        9 | 12618 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 12619 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 12620 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12621 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12622 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12623 | `			);` |
|      ! 0 | 12624 | `	}else{` |
|       13 | 12625 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 12626 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12627 | `	}` |
|        9 | 12628 | `	sVar.zString = pAux->zWorker;` |
|        - | 12629 | `	/* Try to extract the variable */` |
|        9 | 12630 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 12631 | `	if( pObj ){` |
|        - | 12632 | `		/* Collision */` |
|        5 | 12633 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 12634 | `			return SXRET_OK;` |
|        - | 12635 | `		}` |
|        5 | 12636 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 12637 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 12638 | `				/* Already prefixed */` |
|      ! 0 | 12639 | `				return SXRET_OK;` |
|        - | 12640 | `			}` |
|      ! 0 | 12641 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12642 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12643 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12644 | `				);` |
|      ! 0 | 12645 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 12646 | `		}` |
|        3 | 12647 | `	}else{` |
|        - | 12648 | `		/* Create the variable */` |
|        5 | 12649 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 12650 | `	}` |
|        9 | 12651 | `	if( pObj ){` |
|        - | 12652 | `		/* Overwrite the old value */` |
|        9 | 12653 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12654 | `		/* Increment counter */` |
|        9 | 12655 | `		pAux->iCount++;` |
|        4 | 12656 | `	}` |
|        9 | 12657 | `	return SXRET_OK;` |
|        5 | 12658 |  |
|        - | 12659 | `/*` |
|        - | 12660 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12661 | ` * defined below.` |
|        - | 12662 | ` */` |
|        2 | 12663 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12664 |  |
|        3 | 12665 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12666 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12667 | `	ph7_value *pObj;` |
|        - | 12668 | `	SyString sVar;` |
|        - | 12669 | `	/* Perform a string cast */` |
|        3 | 12670 | `	PH7_MemObjToString(pKey);` |
|        3 | 12671 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12672 | `		/* Unavailable variable name */` |
|      ! 0 | 12673 | `		return SXRET_OK;` |
|        - | 12674 | `	}` |
|        3 | 12675 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12676 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12677 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12678 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12679 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12680 | `			);` |
|        2 | 12681 | `	}else{` |
|      ! 0 | 12682 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12683 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12684 | `	}` |
|        3 | 12685 | `	sVar.zString = pAux->zWorker;` |
|        - | 12686 | `	/* Extract the variable */` |
|        3 | 12687 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12688 | `	if( pObj ){` |
|        3 | 12689 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12690 | `	}` |
|        3 | 12691 | `	return SXRET_OK;` |
|        2 | 12692 |  |
|        - | 12693 | `/*` |
|        - | 12694 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12695 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12696 | ` * Parameters` |
|        - | 12697 | ` * $types` |
|        - | 12698 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12699 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12700 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12701 | ` *  POST includes the POST uploaded file information.` |
|        - | 12702 | ` *  Note:` |
|        - | 12703 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12704 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12705 | ` * $prefix` |
|        - | 12706 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12707 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12708 | ` *  variable named $pref_userid.` |
|        - | 12709 | ` * Return` |
|        - | 12710 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12711 | ` */` |
|        2 | 12712 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12713 |  |
|        - | 12714 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12715 | `	extract_aux_data sAux;` |
|        - | 12716 | `	int nLen,nPrefixLen;` |
|        - | 12717 | `	ph7_value *pSuper;` |
|        - | 12718 | `	ph7_vm *pVm;` |
|        - | 12719 | `	/* By default import only $_GET variables  */` |
|        3 | 12720 | `	zImport = "G";` |
|        3 | 12721 | `	nLen = (int)sizeof(char);` |
|        3 | 12722 | `	zPrefix = 0;` |
|        3 | 12723 | `	nPrefixLen = 0;` |
|        3 | 12724 | `	if( nArg > 0 ){` |
|        3 | 12725 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12726 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12727 | `		}` |
|        3 | 12728 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12729 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12730 | `		}` |
|        1 | 12731 | `	}` |
|        - | 12732 | `	/* Point to the underlying VM */` |
|        3 | 12733 | `	pVm = pCtx->pVm;` |
|        - | 12734 | `	/* Initialize the aux data */` |
|        3 | 12735 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12736 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12737 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12738 | `	sAux.pVm = pVm;` |
|        - | 12739 | `	/* Extract */` |
|        3 | 12740 | `	zEnd = &zImport[nLen];` |
|        5 | 12741 | `	while( zImport < zEnd ){` |
|        3 | 12742 | `		int c = zImport[0];` |
|        3 | 12743 | `		pSuper = 0;` |
|        3 | 12744 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12745 | `			/* Import $_GET variables */` |
|        3 | 12746 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12747 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12748 | `			/* Import $_POST variables */` |
|      ! 0 | 12749 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12750 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12751 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12752 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12753 | `		}` |
|        3 | 12754 | `		if( pSuper ){` |
|        - | 12755 | `			/* Iterate throw array entries */` |
|        3 | 12756 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12757 | `		}` |
|        - | 12758 | `		/* Advance the cursor */` |
|        3 | 12759 | `		zImport++;` |
|        1 | 12760 | `	}` |
|        - | 12761 | `	/* All done,return TRUE*/` |
|        3 | 12762 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12763 | `	return PH7_OK;` |
|        1 | 12764 |  |
|        - | 12765 | `/*` |
|        - | 12766 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12767 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12768 | ` * information.` |
|        - | 12769 | ` */` |
|    11466 | 12770 | `static sxi32 VmEvalChunk(` |
|        - | 12771 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12772 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12773 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12774 | `	int iFlags,         /* Compile flag */` |
|        - | 12775 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12776 | `	)` |
|        2 | 12777 |  |
|        - | 12778 | `	SySet *pByteCode,aByteCode;` |
|        - | 12779 | `	SyBlob sSavedNs;` |
|    11468 | 12780 | `	ProcConsumer xErr = 0;` |
|    11468 | 12781 | `	void *pErrData = 0;` |
|        - | 12782 | `	/* Initialize bytecode container */` |
|    11468 | 12783 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11468 | 12784 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12785 | `	/* Reset the code generator */` |
|    11468 | 12786 | `	if( bTrueReturn ){` |
|        - | 12787 | `		/* Included file,log compile-time errors */` |
|     8626 | 12788 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8626 | 12789 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4312 | 12790 | `	}` |
|    11468 | 12791 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12792 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12793 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12794 | `	 * the caller's namespace is restored. */` |
|    11468 | 12795 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11468 | 12796 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11468 | 12797 | `	if( bTrueReturn ){` |
|        - | 12798 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8626 | 12799 | `		SyBlobReset(&pVm->sNamespace);` |
|     4312 | 12800 | `	}` |
|        - | 12801 | `	/* Swap bytecode container */` |
|    11468 | 12802 | `	pByteCode = pVm->pByteContainer;` |
|    11468 | 12803 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12804 | `	/* Compile the chunk */` |
|    11468 | 12805 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17201 | 12806 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12807 | `		/* Compilation error,return false */` |
|        3 | 12808 | `		if( pCtx ){` |
|        3 | 12809 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12810 | `		}` |
|        2 | 12811 | `	}else{` |
|        - | 12812 | `		/* Mount any newly defined classes */` |
|        - | 12813 | `		SyHashEntry *pEntry;` |
|        - | 12814 | `		ph7_class *pClass;` |
|        - | 12815 | `		ph7_value sResult; /* Return value */` |
|        - | 12816 | `		sxi32 rc;` |
|    11466 | 12817 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   473368 | 12818 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   456172 | 12819 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 12820 | `			/* Only mount classes that haven't been mounted yet */` |
|   456172 | 12821 | `			if( !pClass->bMounted ){` |
|    90402 | 12822 | `				rc = VmMountUserClass(pVm,pClass);` |
|    90402 | 12823 | `				if( rc != SXRET_OK ){` |
|        - | 12824 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 12825 | `					if( pCtx ){` |
|      ! 0 | 12826 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 12827 | `					}` |
|      ! 0 | 12828 | `					goto Cleanup;` |
|        - | 12829 | `				}` |
|    45200 | 12830 | `			}` |
|        2 | 12831 | `		}` |
|    11466 | 12832 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 12833 | `			/* Out of memory */` |
|      ! 0 | 12834 | `			if( pCtx ){` |
|      ! 0 | 12835 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 12836 | `			}` |
|      ! 0 | 12837 | `			goto Cleanup;` |
|        - | 12838 | `		}` |
|    11466 | 12839 | `		if( bTrueReturn ){` |
|        - | 12840 | `			/* Assume a boolean true return value */` |
|     8626 | 12841 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4314 | 12842 | `		}else{` |
|        - | 12843 | `			/* Assume a null return value */` |
|     2842 | 12844 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 12845 | `		}` |
|        - | 12846 | `		/* Execute the compiled chunk */` |
|    11466 | 12847 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11466 | 12848 | `		if( pCtx ){` |
|        - | 12849 | `			/* Set the execution result */` |
|     8644 | 12850 | `			ph7_result_value(pCtx,&sResult);` |
|     4321 | 12851 | `		}` |
|    11466 | 12852 | `		PH7_MemObjRelease(&sResult);` |
|        - | 12853 | `	}` |
|     5733 | 12854 | `Cleanup:` |
|        - | 12855 | `	/* Cleanup the mess left behind */` |
|    11468 | 12856 | `	pVm->pByteContainer = pByteCode;` |
|    11468 | 12857 | `	SySetRelease(&aByteCode);` |
|        - | 12858 | `	/* Restore caller's namespace state */` |
|    11468 | 12859 | `	SyBlobReset(&pVm->sNamespace);` |
|    11468 | 12860 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11468 | 12861 | `	SyBlobRelease(&sSavedNs);` |
|    11468 | 12862 | `	return SXRET_OK;` |
|        2 | 12863 |  |
|        - | 12864 | `/*` |
|        - | 12865 | ` * value eval(string $code)` |
|        - | 12866 | ` *   Evaluate a string as PHP code.` |
|        - | 12867 | ` * Parameter` |
|        - | 12868 | ` *  code: PHP code to evaluate.` |
|        - | 12869 | ` * Return` |
|        - | 12870 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 12871 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 12872 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 12873 | ` */` |
|       22 | 12874 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12875 |  |
|        - | 12876 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 12877 | `	if( nArg < 1 ){` |
|        - | 12878 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12879 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12880 | `		return SXRET_OK;` |
|        - | 12881 | `	}` |
|        - | 12882 | `	/* Chunk to evaluate */` |
|       24 | 12883 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 12884 | `	if( sChunk.nByte < 1 ){` |
|        - | 12885 | `		/* Empty string,return NULL */` |
|        3 | 12886 | `		ph7_result_null(pCtx);` |
|        3 | 12887 | `		return SXRET_OK;` |
|        - | 12888 | `	}` |
|        - | 12889 | `	/* Eval the chunk */` |
|       22 | 12890 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 12891 | `	return SXRET_OK;` |
|       13 | 12892 |  |
|        - | 12893 | `/*` |
|        - | 12894 | ` * Check if a file path is already included.` |
|        - | 12895 | ` */` |
|    17244 | 12896 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 12897 |  |
|        - | 12898 | `	SyString *aEntries;` |
|        - | 12899 | `	sxu32 n;` |
|    17246 | 12900 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 12901 | `	/* Perform a linear search */` |
| 74289738 | 12902 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 74272500 | 12903 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 12904 | `			/* Already included */` |
|        7 | 12905 | `			return TRUE;` |
|        - | 12906 | `		}` |
| 37136248 | 12907 | `	}` |
|    17240 | 12908 | `	return FALSE;` |
|     8624 | 12909 |  |
|        - | 12910 | `/*` |
|        - | 12911 | ` * Push a file path in the appropriate VM container.` |
|        - | 12912 | ` */` |
|    20058 | 12913 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 12914 |  |
|        - | 12915 | `	SyString sPath;` |
|        - | 12916 | `	char *zDup;` |
|        - | 12917 | `#ifdef __WINNT__` |
|        - | 12918 | `	char *zCur;` |
|        - | 12919 | `#endif` |
|        - | 12920 | `	sxi32 rc;` |
|    20060 | 12921 | `	if( nLen < 0 ){` |
|     2816 | 12922 | `		nLen = SyStrlen(zPath);` |
|     1407 | 12923 | `	}` |
|        - | 12924 | `	/* Duplicate the file path first */` |
|    20060 | 12925 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20060 | 12926 | `	if( zDup == 0 ){` |
|      ! 0 | 12927 | `		return SXERR_MEM;` |
|        - | 12928 | `	}` |
|        - | 12929 | `#ifdef __WINNT__` |
|        - | 12930 | `	/* Normalize path on windows` |
|        - | 12931 | `	 * Example:` |
|        - | 12932 | `	 *    Path/To/File.php` |
|        - | 12933 | `	 * becomes` |
|        - | 12934 | `	 *   path\to\file.php` |
|        - | 12935 | `	 */` |
|        2 | 12936 | `	zCur = zDup;` |
|        2 | 12937 | `	while( zCur[0] != 0 ){` |
|        2 | 12938 | `		if( zCur[0] == '/' ){` |
|        2 | 12939 | `			zCur[0] = '\\';` |
|        2 | 12940 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 12941 | `			int c = SyToLower(zCur[0]);` |
|        1 | 12942 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 12943 | `		}` |
|        2 | 12944 | `		zCur++;` |
|        2 | 12945 | `	}` |
|        - | 12946 | `#endif` |
|        - | 12947 | `	/* Install the file path */` |
|    20060 | 12948 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20060 | 12949 | `	if( !bMain ){` |
|    17246 | 12950 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 12951 | `			/* Already included */` |
|        7 | 12952 | `			*pNew = 0;` |
|        4 | 12953 | `		}else{` |
|        - | 12954 | `			/* Insert in the corresponding container */` |
|    17240 | 12955 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17240 | 12956 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12957 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 12958 | `				return rc;` |
|        - | 12959 | `			}` |
|    17240 | 12960 | `			*pNew = 1;` |
|        - | 12961 | `		}` |
|     8622 | 12962 | `	}` |
|    20060 | 12963 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20060 | 12964 | `	return SXRET_OK;` |
|    10031 | 12965 |  |
|        - | 12966 | `/*` |
|        - | 12967 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 12968 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 12969 | ` * indicates failure.` |
|        - | 12970 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 12971 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 12972 | ` * operations.` |
|        - | 12973 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 12974 | ` * this function is a no-op.` |
|        - | 12975 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 12976 | ` * constructs for more information.` |
|        - | 12977 | ` */` |
|     8634 | 12978 | `static sxi32 VmExecIncludedFile(` |
|        - | 12979 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 12980 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 12981 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 12982 | `	 )` |
|        2 | 12983 |  |
|        - | 12984 | `	sxi32 rc;` |
|        - | 12985 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12986 | `	const ph7_io_stream *pStream;` |
|        - | 12987 | `	SyBlob sContents;` |
|        - | 12988 | `	void *pHandle;` |
|        - | 12989 | `	ph7_vm *pVm;` |
|        - | 12990 | `	int isNew;` |
|        - | 12991 | `	/* Initialize fields */` |
|     8636 | 12992 | `	pVm = pCtx->pVm;` |
|     8636 | 12993 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8636 | 12994 | `	isNew = 0;` |
|        - | 12995 | `	/* Extract the associated stream */` |
|     8636 | 12996 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 12997 | `	/*` |
|        - | 12998 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 12999 | `	 * in a read-only mode.` |
|        - | 13000 | `	 */` |
|     8636 | 13001 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8636 | 13002 | `	if( pHandle == 0 ){` |
|        8 | 13003 | `		return SXERR_IO;` |
|        - | 13004 | `	}` |
|     8630 | 13005 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8630 | 13006 | `	if( IncludeOnce && !isNew ){` |
|        - | 13007 | `		/* Already included */` |
|        5 | 13008 | `		rc = SXERR_EXISTS;` |
|        3 | 13009 | `	}else{` |
|        - | 13010 | `		/* Read the whole file contents */` |
|     8626 | 13011 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8626 | 13012 | `		if( rc == SXRET_OK ){` |
|        - | 13013 | `			SyString sScript;` |
|        - | 13014 | `			/* Compile and execute the script */` |
|     8626 | 13015 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8626 | 13016 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4312 | 13017 | `		}` |
|        - | 13018 | `	}` |
|        - | 13019 | `	/* Pop from the set of included file */` |
|     8630 | 13020 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13021 | `	/* Close the handle */` |
|     8630 | 13022 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13023 | `	/* Release the working buffer */` |
|     8630 | 13024 | `	SyBlobRelease(&sContents);` |
|        - | 13025 | `#else` |
|        - | 13026 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13027 | `	SXUNUSED(pPath);` |
|        - | 13028 | `	SXUNUSED(IncludeOnce);` |
|        - | 13029 | `	rc = SXERR_IO;` |
|        - | 13030 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8630 | 13031 | `	return rc;` |
|     4319 | 13032 |  |
|        - | 13033 | `/*` |
|        - | 13034 | ` * string get_include_path(void)` |
|        - | 13035 | ` *  Gets the current include_path configuration option.` |
|        - | 13036 | ` * Parameter` |
|        - | 13037 | ` *  None` |
|        - | 13038 | ` * Return` |
|        - | 13039 | ` *  Included paths as a string` |
|        - | 13040 | ` */` |
|        2 | 13041 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13042 |  |
|        3 | 13043 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13044 | `	SyString *aEntry;` |
|        - | 13045 | `	int dir_sep;` |
|        - | 13046 | `	sxu32 n;` |
|        - | 13047 | `#ifdef __WINNT__` |
|        1 | 13048 | `	dir_sep = ';';` |
|        - | 13049 | `#else` |
|        - | 13050 | `	/* Assume UNIX path separator */` |
|        2 | 13051 | `	dir_sep = ':';` |
|        - | 13052 | `#endif` |
|        1 | 13053 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13054 | `	SXUNUSED(apArg);` |
|        - | 13055 | `	/* Point to the list of import paths */` |
|        3 | 13056 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13057 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13058 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13059 | `		if( n > 0 ){` |
|        - | 13060 | `			/* Append dir seprator */` |
|      ! 0 | 13061 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13062 | `		}` |
|        - | 13063 | `		/* Append path */` |
|        3 | 13064 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13065 | `	}` |
|        3 | 13066 | `	return PH7_OK;` |
|        1 | 13067 |  |
|        - | 13068 | `/*` |
|        - | 13069 | ` * string get_get_included_files(void)` |
|        - | 13070 | ` *  Gets the current include_path configuration option.` |
|        - | 13071 | ` * Parameter` |
|        - | 13072 | ` *  None` |
|        - | 13073 | ` * Return` |
|        - | 13074 | ` *  Included paths as a string` |
|        - | 13075 | ` */` |
|        2 | 13076 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13077 |  |
|        3 | 13078 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13079 | `	ph7_value *pArray,*pWorker;` |
|        - | 13080 | `	SyString *pEntry;` |
|        - | 13081 | `	int c,d;` |
|        - | 13082 | `	/* Create an array and a working value */` |
|        3 | 13083 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13084 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13085 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13086 | `		/* Out of memory,return null */` |
|      ! 0 | 13087 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13088 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13089 | `		SXUNUSED(apArg);` |
|      ! 0 | 13090 | `		return PH7_OK;` |
|        - | 13091 | `	}` |
|        3 | 13092 | `	c = d = '/';` |
|        - | 13093 | `#ifdef __WINNT__` |
|        1 | 13094 | `	d = '\\';` |
|        - | 13095 | `#endif` |
|        - | 13096 | `	/* Iterate throw entries */` |
|        3 | 13097 | `	SySetResetCursor(pFiles);` |
|     3839 | 13098 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13099 | `		const char *zBase,*zEnd;` |
|        - | 13100 | `		int iLen;` |
|        - | 13101 | `		/* reset the string cursor */` |
|     3837 | 13102 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13103 | `		/* Extract base name */` |
|     3837 | 13104 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13105 | `		/* Ignore trailing '/' */` |
|     5755 | 13106 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13107 | `			zEnd--;` |
|      ! 0 | 13108 | `		}` |
|     3837 | 13109 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13110 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13111 | `			zEnd--;` |
|        1 | 13112 | `		}` |
|     3837 | 13113 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13114 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13115 | `		/* Copy entry name */` |
|     3837 | 13116 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13117 | `		/* Perform the insertion */` |
|     3837 | 13118 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13119 | `	}` |
|        - | 13120 | `	/* All done,return the created array */` |
|        3 | 13121 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13122 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13123 | `	 * by the engine as soon we return from this foreign` |
|        - | 13124 | `	 * function.` |
|        - | 13125 | `	 */` |
|        3 | 13126 | `	return PH7_OK;` |
|        2 | 13127 |  |
|        - | 13128 | `/*` |
|        - | 13129 | ` * include:` |
|        - | 13130 | ` * According to the PHP reference manual.` |
|        - | 13131 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13132 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13133 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13134 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13135 | ` *  and the current working directory before failing. The include()` |
|        - | 13136 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13137 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13138 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13139 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13140 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13141 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13142 | ` *  directory to find the requested file.` |
|        - | 13143 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13144 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13145 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13146 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13147 | ` */` |
|     8616 | 13148 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13149 |  |
|        - | 13150 | `	SyString sFile;` |
|        - | 13151 | `	sxi32 rc;` |
|     8618 | 13152 | `	if( nArg < 1 ){` |
|        - | 13153 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13154 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13155 | `		return SXRET_OK;` |
|        - | 13156 | `	}` |
|        - | 13157 | `	/* File to include */` |
|     8618 | 13158 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8618 | 13159 | `	if( sFile.nByte < 1 ){` |
|        - | 13160 | `		/* Empty string,return NULL */` |
|      ! 0 | 13161 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13162 | `		return SXRET_OK;` |
|        - | 13163 | `	}` |
|        - | 13164 | `	/* Open,compile and execute the desired script */` |
|     8618 | 13165 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8618 | 13166 | `	if( rc != SXRET_OK ){` |
|        - | 13167 | `		/* Emit a warning and return false */` |
|        3 | 13168 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13169 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13170 | `	}` |
|     8618 | 13171 | `	return SXRET_OK;` |
|     4310 | 13172 |  |
|        - | 13173 | `/*` |
|        - | 13174 | ` * include_once:` |
|        - | 13175 | ` *  According to the PHP reference manual.` |
|        - | 13176 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13177 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13178 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13179 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13180 | ` *   just once.` |
|        - | 13181 | ` */` |
|        4 | 13182 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13183 |  |
|        - | 13184 | `	SyString sFile;` |
|        - | 13185 | `	sxi32 rc;` |
|        5 | 13186 | `	if( nArg < 1 ){` |
|        - | 13187 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13188 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13189 | `		return SXRET_OK;` |
|        - | 13190 | `	}` |
|        - | 13191 | `	/* File to include */` |
|        5 | 13192 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13193 | `	if( sFile.nByte < 1 ){` |
|        - | 13194 | `		/* Empty string,return NULL */` |
|      ! 0 | 13195 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13196 | `		return SXRET_OK;` |
|        - | 13197 | `	}` |
|        - | 13198 | `	/* Open,compile and execute the desired script */` |
|        5 | 13199 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13200 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13201 | `		/* File already included,return TRUE */` |
|        3 | 13202 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13203 | `		return SXRET_OK;` |
|        - | 13204 | `	}` |
|        3 | 13205 | `	if( rc != SXRET_OK ){` |
|        - | 13206 | `		/* Emit a warning and return false */` |
|      ! 0 | 13207 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13208 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13209 | ` 	}` |
|        3 | 13210 | `	return SXRET_OK;` |
|        3 | 13211 |  |
|        - | 13212 | `/*` |
|        - | 13213 | ` * require.` |
|        - | 13214 | ` *  According to the PHP reference manual.` |
|        - | 13215 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13216 | ` *   also produce a fatal level error.` |
|        - | 13217 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13218 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13219 | ` */` |
|        6 | 13220 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13221 |  |
|        - | 13222 | `	SyString sFile;` |
|        - | 13223 | `	sxi32 rc;` |
|        8 | 13224 | `	if( nArg < 1 ){` |
|        - | 13225 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13226 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13227 | `		return SXRET_OK;` |
|        - | 13228 | `	}` |
|        - | 13229 | `	/* File to include */` |
|        8 | 13230 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13231 | `	if( sFile.nByte < 1 ){` |
|        - | 13232 | `		/* Empty string,return NULL */` |
|      ! 0 | 13233 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13234 | `		return SXRET_OK;` |
|        - | 13235 | `	}` |
|        - | 13236 | `	/* Open,compile and execute the desired script */` |
|        8 | 13237 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13238 | `	if( rc != SXRET_OK ){` |
|        - | 13239 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13240 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13241 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13242 | `		return PH7_ABORT;` |
|        - | 13243 | `	}` |
|        8 | 13244 | `	return SXRET_OK;` |
|        5 | 13245 |  |
|        - | 13246 | `/*` |
|        - | 13247 | ` * require_once:` |
|        - | 13248 | ` *  According to the PHP reference manual.` |
|        - | 13249 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13250 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13251 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13252 | ` *   and how it differs from its non _once siblings.` |
|        - | 13253 | ` */` |
|        4 | 13254 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13255 |  |
|        - | 13256 | `	SyString sFile;` |
|        - | 13257 | `	sxi32 rc;` |
|        5 | 13258 | `	if( nArg < 1 ){` |
|        - | 13259 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13260 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13261 | `		return SXRET_OK;` |
|        - | 13262 | `	}` |
|        - | 13263 | `	/* File to include */` |
|        5 | 13264 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13265 | `	if( sFile.nByte < 1 ){` |
|        - | 13266 | `		/* Empty string,return NULL */` |
|      ! 0 | 13267 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13268 | `		return SXRET_OK;` |
|        - | 13269 | `	}` |
|        - | 13270 | `	/* Open,compile and execute the desired script */` |
|        5 | 13271 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13272 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13273 | `		/* File already included,return TRUE */` |
|        3 | 13274 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13275 | `		return SXRET_OK;` |
|        - | 13276 | `	}` |
|        3 | 13277 | `	if( rc != SXRET_OK ){` |
|        - | 13278 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13279 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13280 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13281 | `		return PH7_ABORT;` |
|        - | 13282 | `	}` |
|        3 | 13283 | `	return SXRET_OK;` |
|        3 | 13284 |  |
|        - | 13285 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13286 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13287 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13288 | `/*` |
|        - | 13289 | ` * Section:` |
|        - | 13290 | ` *  SPL Autoloading functions.` |
|        - | 13291 | ` * Status:` |
|        - | 13292 | ` *  Stable.` |
|        - | 13293 | ` */` |
|        - | 13294 | `/*` |
|        - | 13295 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 13296 | ` *  Register given function as __autoload() implementation.` |
|        - | 13297 | ` * Parameters` |
|        - | 13298 | ` *  callback` |
|        - | 13299 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 13300 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 13301 | ` *  throw` |
|        - | 13302 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 13303 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 13304 | ` *  prepend` |
|        - | 13305 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 13306 | ` *   autoload stack instead of appending it.` |
|        - | 13307 | ` * Return` |
|        - | 13308 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13309 | ` */` |
|       34 | 13310 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13311 |  |
|        - | 13312 | `	VmAutoloadCB sEntry;` |
|       36 | 13313 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 13314 | `	int iPrepend = 0;` |
|        - | 13315 | `	sxu32 n;` |
|       36 | 13316 | `	if( nArg < 1 ){` |
|        - | 13317 | `		/* No callback provided — register default spl_autoload.` |
|        - | 13318 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 13319 | `		/* Check for duplicates first */` |
|        9 | 13320 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 13321 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 13322 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 13323 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 13324 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 13325 | `				ph7_result_bool(pCtx,1);` |
|        5 | 13326 | `				return SXRET_OK;` |
|        - | 13327 | `			}` |
|      ! 0 | 13328 | `		}` |
|        5 | 13329 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 13330 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 13331 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 13332 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 13333 | `		ph7_result_bool(pCtx,1);` |
|        5 | 13334 | `		return SXRET_OK;` |
|        - | 13335 | `	}` |
|        - | 13336 | `	/* Validate that the callback is callable */` |
|       28 | 13337 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 13338 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 13339 | `		if( nArg >= 2 ){` |
|      ! 0 | 13340 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13341 | `		}` |
|      ! 0 | 13342 | `		if( iThrow ){` |
|      ! 0 | 13343 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 13344 | `				"Argument is not callable");` |
|      ! 0 | 13345 | `		}` |
|      ! 0 | 13346 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13347 | `		return SXRET_OK;` |
|        - | 13348 | `	}` |
|        - | 13349 | `	/* Check for duplicates */` |
|       46 | 13350 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 13351 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 13352 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13353 | `			/* Already registered */` |
|      ! 0 | 13354 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13355 | `			return SXRET_OK;` |
|        - | 13356 | `		}` |
|       11 | 13357 | `	}` |
|        - | 13358 | `	/* Check prepend flag */` |
|       28 | 13359 | `	if( nArg >= 3 ){` |
|        3 | 13360 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 13361 | `	}` |
|        - | 13362 | `	/* Store the callback */` |
|       28 | 13363 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 13364 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 13365 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 13366 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 13367 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 13368 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 13369 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 13370 | `		VmAutoloadCB *aBase;` |
|        3 | 13371 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13372 | `		/* Rotate: move last entry to front */` |
|        3 | 13373 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 13374 | `		if( aBase ){` |
|        - | 13375 | `			VmAutoloadCB sTemp;` |
|        - | 13376 | `			sxu32 i;` |
|        3 | 13377 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 13378 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 13379 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 13380 | `			}` |
|        3 | 13381 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 13382 | `		}` |
|        2 | 13383 | `	}else{` |
|       26 | 13384 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13385 | `	}` |
|       28 | 13386 | `	ph7_result_bool(pCtx,1);` |
|       28 | 13387 | `	return SXRET_OK;` |
|       19 | 13388 |  |
|        - | 13389 | `/*` |
|        - | 13390 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 13391 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 13392 | ` * Parameters` |
|        - | 13393 | ` *  callback` |
|        - | 13394 | ` *   The autoload function being unregistered.` |
|        - | 13395 | ` * Return` |
|        - | 13396 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13397 | ` */` |
|       32 | 13398 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13399 |  |
|       34 | 13400 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13401 | `	sxu32 n,nEntry;` |
|       34 | 13402 | `	if( nArg < 1 ){` |
|      ! 0 | 13403 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13404 | `		return SXRET_OK;` |
|        - | 13405 | `	}` |
|       34 | 13406 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13407 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 13408 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 13409 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13410 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 13411 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13412 | `			sxu32 i;` |
|       32 | 13413 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13414 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13415 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13416 | `			}` |
|        - | 13417 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13418 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13419 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13420 | `			return SXRET_OK;` |
|        - | 13421 | `		}` |
|        3 | 13422 | `	}` |
|        3 | 13423 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13424 | `	return SXRET_OK;` |
|       18 | 13425 |  |
|        - | 13426 | `/*` |
|        - | 13427 | ` * array spl_autoload_functions(void)` |
|        - | 13428 | ` *  Return all registered __autoload() functions.` |
|        - | 13429 | ` * Return` |
|        - | 13430 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13431 | ` *  an empty array is returned.` |
|        - | 13432 | ` */` |
|       20 | 13433 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13434 |  |
|       21 | 13435 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13436 | `	ph7_value *pArray;` |
|        - | 13437 | `	sxu32 n,nEntry;` |
|       10 | 13438 | `	SXUNUSED(nArg);` |
|       10 | 13439 | `	SXUNUSED(apArg);` |
|       21 | 13440 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13441 | `	if( pArray == 0 ){` |
|      ! 0 | 13442 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13443 | `		return SXRET_OK;` |
|        - | 13444 | `	}` |
|       21 | 13445 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13446 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13447 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13448 | `		if( pEntry ){` |
|       15 | 13449 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13450 | `		}` |
|        8 | 13451 | `	}` |
|       21 | 13452 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13453 | `	return SXRET_OK;` |
|       11 | 13454 |  |
|        - | 13455 | `/*` |
|        - | 13456 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13457 | ` *  Default implementation of __autoload().` |
|        - | 13458 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13459 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13460 | ` * Parameters` |
|        - | 13461 | ` *  class` |
|        - | 13462 | ` *   The class name being searched.` |
|        - | 13463 | ` *  file_extensions` |
|        - | 13464 | ` *   Comma-separated list of file extensions to try.` |
|        - | 13465 | ` */` |
|        2 | 13466 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13467 |  |
|        - | 13468 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 13469 | `	SyBlob sPath;` |
|        - | 13470 | `	int nClass;` |
|        - | 13471 | `	sxi32 rc;` |
|        3 | 13472 | `	if( nArg < 1 ){` |
|      ! 0 | 13473 | `		return SXRET_OK;` |
|        - | 13474 | `	}` |
|        3 | 13475 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 13476 | `	if( nClass < 1 ){` |
|      ! 0 | 13477 | `		return SXRET_OK;` |
|        - | 13478 | `	}` |
|        - | 13479 | `	/* Default extensions */` |
|        3 | 13480 | `	zExt = ".php,.inc";` |
|        3 | 13481 | `	if( nArg >= 2 ){` |
|        - | 13482 | `		int nExt;` |
|      ! 0 | 13483 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 13484 | `		if( nExt < 1 ){` |
|      ! 0 | 13485 | `			zExt = ".php,.inc";` |
|      ! 0 | 13486 | `		}` |
|      ! 0 | 13487 | `	}` |
|        3 | 13488 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 13489 | `	/* Iterate over comma-separated extensions */` |
|        3 | 13490 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 13491 | `	zCur = zExt;` |
|        7 | 13492 | `	while( zCur < zEnd ){` |
|        - | 13493 | `		const char *zComma;` |
|        - | 13494 | `		SyString sFile;` |
|        - | 13495 | `		int i;` |
|        - | 13496 | `		/* Find next comma or end */` |
|        5 | 13497 | `		zComma = zCur;` |
|       21 | 13498 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 13499 | `			zComma++;` |
|        1 | 13500 | `		}` |
|        - | 13501 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 13502 | `		SyBlobReset(&sPath);` |
|       69 | 13503 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 13504 | `			char c = zClass[i];` |
|       65 | 13505 | `			if( c == '\\' ){` |
|      ! 0 | 13506 | `				c = '/';` |
|       65 | 13507 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 13508 | `				c = c + ('a' - 'A');` |
|        6 | 13509 | `			}` |
|       65 | 13510 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 13511 | `		}` |
|        - | 13512 | `		/* Append extension */` |
|        5 | 13513 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 13514 | `		/* Try to include the file */` |
|        5 | 13515 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 13516 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 13517 | `		if( rc == SXRET_OK ){` |
|        - | 13518 | `			/* File included successfully */` |
|      ! 0 | 13519 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 13520 | `			return SXRET_OK;` |
|        - | 13521 | `		}` |
|        - | 13522 | `		/* Move past the comma */` |
|        5 | 13523 | `		zCur = zComma;` |
|        5 | 13524 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 13525 | `			zCur++;` |
|        1 | 13526 | `		}` |
|        1 | 13527 | `	}` |
|        3 | 13528 | `	SyBlobRelease(&sPath);` |
|        3 | 13529 | `	return SXRET_OK;` |
|        2 | 13530 |  |
|        - | 13531 | `/* Table of built-in VM functions. */` |
|        - | 13532 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13533 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13534 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13535 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13536 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13537 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13538 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13539 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13540 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13541 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13542 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13543 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13544 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13545 | `	    /* Constants management */` |
|        - | 13546 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13547 | `	{ "define",   vm_builtin_define               },` |
|        - | 13548 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13549 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13550 | `	   /* Class/Object functions */` |
|        - | 13551 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13552 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13553 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13554 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13555 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13556 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13557 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13558 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13559 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13560 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13561 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13562 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13563 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13564 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13565 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13566 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13567 | `	   /* SPL Autoloading */` |
|        - | 13568 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 13569 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 13570 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 13571 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 13572 | `	   /* Random numbers/strings generators */` |
|        - | 13573 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13574 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13575 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13576 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13577 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13578 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13579 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13580 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13581 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13582 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13583 | `	   /* Language constructs functions */` |
|        - | 13584 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13585 | `	{ "print", vm_builtin_print                   },` |
|        - | 13586 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13587 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13588 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13589 | `	  /* Variable handling functions */` |
|        - | 13590 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13591 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13592 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13593 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13594 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13595 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13596 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13597 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13598 | `	  /* Ouput control functions */` |
|        - | 13599 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13600 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13601 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13602 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13603 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13604 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13605 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13606 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13607 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13608 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13609 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13610 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13611 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13612 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13613 | `	  /* Assertion functions */` |
|        - | 13614 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13615 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13616 | `	  /* Error reporting functions */` |
|        - | 13617 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13618 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13619 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13620 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13621 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13622 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13623 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13624 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13625 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13626 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13627 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13628 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13629 | `	  /* Release info */` |
|        - | 13630 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13631 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13632 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13633 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13634 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13635 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13636 | `	  /* hashmap */` |
|        - | 13637 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13638 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13639 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13640 | `	  /* URL related function */` |
|        - | 13641 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13642 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13643 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13644 | `	   /* XML processing functions */` |
|        - | 13645 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13646 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13647 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13648 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13649 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13650 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13651 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13652 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13653 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13654 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13655 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13656 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13657 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13658 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13659 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13660 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13661 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13662 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13663 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13664 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13665 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13666 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13667 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13668 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13669 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13670 | `	   /* Command line processing */` |
|        - | 13671 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13672 | `	   /* JSON encoding/decoding */` |
|        - | 13673 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13674 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13675 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13676 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13677 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13678 | `	   /* Files/URI inclusion facility */` |
|        - | 13679 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13680 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13681 | `	{ "include",      vm_builtin_include          },` |
|        - | 13682 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13683 | `	{ "require",      vm_builtin_require          },` |
|        - | 13684 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13685 | `};` |
|        - | 13686 | `/*` |
|        - | 13687 | ` * Register the built-in VM functions defined above.` |
|        - | 13688 | ` */` |
|     2540 | 13689 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13690 |  |
|        - | 13691 | `	sxi32 rc;` |
|        - | 13692 | `	sxu32 n;` |
|   327662 | 13693 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13694 | `		/* Note that these special functions have access` |
|        - | 13695 | `		 * to the underlying virtual machine as their` |
|        - | 13696 | `		 * private data.` |
|        - | 13697 | `		 */` |
|   325122 | 13698 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   325122 | 13699 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13700 | `			return rc;` |
|        - | 13701 | `		}` |
|   162562 | 13702 | `	}` |
|     2542 | 13703 | `	return SXRET_OK;` |
|     1272 | 13704 |  |
|        - | 13705 | `/*` |
|        - | 13706 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13707 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13708 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13709 | ` */` |
|    35998 | 13710 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13711 |  |
|    36000 | 13712 | `	if( !iLoadable ){` |
|    34346 | 13713 | `		return pClass;` |
|        - | 13714 | `	}` |
|     1656 | 13715 | `	while(pClass){` |
|     1656 | 13716 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1656 | 13717 | `			return pClass;` |
|        - | 13718 | `		}` |
|      ! 0 | 13719 | `		pClass = pClass->pNextName;` |
|      ! 0 | 13720 | `	}` |
|      ! 0 | 13721 | `	return 0;` |
|    18001 | 13722 |  |
|        - | 13723 | `/*` |
|        - | 13724 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13725 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13726 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13727 | ` * registered in the VM's class table.` |
|        - | 13728 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13729 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13730 | ` */` |
|       36 | 13731 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13732 |  |
|        - | 13733 | `	VmAutoloadCB *pEntry;` |
|        - | 13734 | `	ph7_value sArg,sResult;` |
|        - | 13735 | `	SyHashEntry *pHashEntry;` |
|        - | 13736 | `	ph7_class *pClass;` |
|        - | 13737 | `	sxu32 n,nEntry;` |
|       38 | 13738 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13739 | `	if( nEntry < 1 ){` |
|       24 | 13740 | `		return 0;` |
|        - | 13741 | `	}` |
|        - | 13742 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13743 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13744 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13745 | `	}` |
|        - | 13746 | `	/* Mark this class as being autoloaded */` |
|       14 | 13747 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13748 | `	/* Prepare the class name argument */` |
|       14 | 13749 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13750 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13751 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13752 | `	pClass = 0;` |
|       28 | 13753 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13754 | `		ph7_value *apArg[1];` |
|       24 | 13755 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13756 | `		if( pEntry == 0 ){` |
|      ! 0 | 13757 | `			continue;` |
|        - | 13758 | `		}` |
|       24 | 13759 | `		apArg[0] = &sArg;` |
|       24 | 13760 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13761 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13762 | `			continue;` |
|        - | 13763 | `		}` |
|        - | 13764 | `		/* Check if the class is now available */` |
|       24 | 13765 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13766 | `		if( pHashEntry ){` |
|       10 | 13767 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13768 | `			if( pClass ){` |
|       10 | 13769 | `				break;` |
|        - | 13770 | `			}` |
|      ! 0 | 13771 | `		}` |
|        9 | 13772 | `	}` |
|       14 | 13773 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13774 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13775 | `	/* Remove reentrancy guard */` |
|       14 | 13776 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13777 | `	return pClass;` |
|       20 | 13778 |  |
|        - | 13779 | `/*` |
|        - | 13780 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13781 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13782 | ` */` |
|       18 | 13783 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13784 |  |
|       20 | 13785 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13786 |  |
|        - | 13787 | `/*` |
|        - | 13788 | ` * Check if the given name refer to an installed class.` |
|        - | 13789 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13790 | ` */` |
|    36008 | 13791 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13792 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13793 | `	const char *zName,  /* Name of the target class */` |
|        - | 13794 | `	sxu32 nByte,        /* zName length */` |
|        - | 13795 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13796 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13797 | `						 */` |
|        - | 13798 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13799 | `	)` |
|        2 | 13800 |  |
|        - | 13801 | `	SyHashEntry *pEntry;` |
|        - | 13802 | `	ph7_class *pClass;` |
|    18004 | 13803 | `	SXUNUSED(iNest);` |
|        - | 13804 | `	/* Exact class lookup.` |
|        - | 13805 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13806 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    36010 | 13807 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    36010 | 13808 | `	if( pEntry == 0 ){` |
|        - | 13809 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 13810 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13811 | `	}` |
|    35992 | 13812 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    35992 | 13813 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    18006 | 13814 |  |
|        - | 13815 | `/*` |
|        - | 13816 | ` * Reference Table Implementation` |
|        - | 13817 | ` * Status: stable <chm@symisc.net>` |
|        - | 13818 | ` * Intro` |
|        - | 13819 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13820 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13821 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13822 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13823 | ` *  Refer to the official for more information on this powerful` |
|        - | 13824 | ` *  extension.` |
|        - | 13825 | ` */` |
|        - | 13826 | `/*` |
|        - | 13827 | ` * Allocate a new reference entry.` |
|        - | 13828 | ` */` |
|  3099384 | 13829 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13830 |  |
|        - | 13831 | `	VmRefObj *pRef;` |
|        - | 13832 | `	/* Allocate a new instance */` |
|  3099386 | 13833 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3099386 | 13834 | `	if( pRef == 0 ){` |
|      ! 0 | 13835 | `		return 0;` |
|        - | 13836 | `	}` |
|        - | 13837 | `	/* Zero the structure */` |
|  3099386 | 13838 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13839 | `	/* Initialize fields */` |
|  3099386 | 13840 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3099386 | 13841 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3099386 | 13842 | `	pRef->nIdx = nIdx;` |
|  3099386 | 13843 | `	return pRef;` |
|  1549694 | 13844 |  |
|        - | 13845 | `/*` |
|        - | 13846 | ` * Default hash function used by the reference table` |
|        - | 13847 | ` * for lookup/insertion operations.` |
|        - | 13848 | ` */` |
| 17081083 | 13849 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13850 |  |
|        - | 13851 | `	/* Calculate the hash based on the memory object index */` |
| 17081085 | 13852 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13853 |  |
|        - | 13854 | `/*` |
|        - | 13855 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13856 | ` * in the reference table.` |
|        - | 13857 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13858 | ` * otherwise.` |
|        - | 13859 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13860 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13861 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13862 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13863 | ` * Refer to the official for more information on this powerful` |
|        - | 13864 | ` * extension.` |
|        - | 13865 | ` */` |
|  9246686 | 13866 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13867 |  |
|        - | 13868 | `	VmRefObj *pRef;` |
|        - | 13869 | `	sxu32 nBucket;` |
|        - | 13870 | `	/* Point to the appropriate bucket */` |
|  9246688 | 13871 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13872 | `	/* Perform the lookup */` |
|  9246688 | 13873 | `	pRef = pVm->apRefObj[nBucket];` |
| 20120087 | 13874 | `	for(;;){` |
| 40226845 | 13875 | `		if( pRef == 0 ){` |
|  3186896 | 13876 | `			break;` |
|        - | 13877 | `		}` |
| 37039951 | 13878 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13879 | `			/* Entry found */` |
|  6059794 | 13880 | `			return pRef;` |
|        - | 13881 | `		}` |
|        - | 13882 | `		/* Point to the next entry */` |
| 30980159 | 13883 | `		pRef = pRef->pNextCollide;` |
|        2 | 13884 | `	}` |
|        - | 13885 | `	/* No such entry,return NULL */` |
|  3186896 | 13886 | `	return 0;` |
|  4623345 | 13887 |  |
|        - | 13888 | `/*` |
|        - | 13889 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13890 | ` *` |
|        - | 13891 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13892 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13893 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13894 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13895 | ` * Refer to the official for more information on this powerful` |
|        - | 13896 | ` * extension.` |
|        - | 13897 | ` */` |
|  3099384 | 13898 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13899 |  |
|        - | 13900 | `	sxu32 nBucket;` |
|  3099386 | 13901 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 13902 | `		VmRefObj **apNew;` |
|        - | 13903 | `		sxu32 nNew;` |
|        - | 13904 | `		/* Allocate a larger table */` |
|     4336 | 13905 | `		nNew = pVm->nRefSize << 1;` |
|     4336 | 13906 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4336 | 13907 | `		if( apNew ){` |
|     4336 | 13908 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 13909 | `			sxu32 n;` |
|        - | 13910 | `			/* Zero the structure */` |
|     4336 | 13911 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 13912 | `			/* Rehash all referenced entries */` |
|  2844416 | 13913 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 13914 | `				/* Remove old collision links */` |
|  2840082 | 13915 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 13916 | `				/* Point to the appropriate bucket */` |
|  2840082 | 13917 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 13918 | `				/* Insert the entry  */` |
|  2840082 | 13919 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2840082 | 13920 | `				if( apNew[nBucket] ){` |
|  2298896 | 13921 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 13922 | `				}` |
|  2840082 | 13923 | `				apNew[nBucket] = pEntry;` |
|        - | 13924 | `				/* Point to the next entry */` |
|  2840082 | 13925 | `				pEntry = pEntry->pNext;` |
|  1420042 | 13926 | `			}` |
|        - | 13927 | `			/* Release the old table */` |
|     4336 | 13928 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 13929 | `			/* Install the new one */` |
|     4336 | 13930 | `			pVm->apRefObj = apNew;` |
|     4336 | 13931 | `			pVm->nRefSize = nNew;` |
|     2167 | 13932 | `		}` |
|     2167 | 13933 | `	}` |
|        - | 13934 | `	/* Point to the appropriate bucket */` |
|  3099386 | 13935 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 13936 | `	/* Insert the entry */` |
|  3099386 | 13937 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3099386 | 13938 | `	if( pVm->apRefObj[nBucket] ){` |
|  2549957 | 13939 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1275006 | 13940 | `	}` |
|  3099386 | 13941 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3099386 | 13942 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3099386 | 13943 | `	pVm->nRefUsed++;` |
|  3099386 | 13944 | `	return SXRET_OK;` |
|        2 | 13945 |  |
|        - | 13946 | `/*` |
|        - | 13947 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 13948 | ` * the reference table.` |
|        - | 13949 | ` * This function is invoked when the user perform an unset` |
|        - | 13950 | ` * call [i.e: unset($var); ].` |
|        - | 13951 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13952 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13953 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13954 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13955 | ` * Refer to the official for more information on this powerful` |
|        - | 13956 | ` * extension.` |
|        - | 13957 | ` */` |
|  3062768 | 13958 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13959 |  |
|        - | 13960 | `	ph7_hashmap_node **apNode;` |
|        - | 13961 | `	SyHashEntry **apEntry;` |
|        - | 13962 | `	sxu32 n;` |
|        - | 13963 | `	/* Point to the reference table */` |
|  3062770 | 13964 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3062770 | 13965 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 13966 | `	/* Unlink the entry from the reference table */` |
|  3156584 | 13967 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    93816 | 13968 | `		if( apEntry[n] ){` |
|    93766 | 13969 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    46882 | 13970 | `		}` |
|    46909 | 13971 | `	}` |
|  6034114 | 13972 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2971346 | 13973 | `		if( apNode[n] ){` |
|     7232 | 13974 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3615 | 13975 | `		}` |
|  1485674 | 13976 | `	}` |
|  3062770 | 13977 | `	if( pRef->pPrevCollide ){` |
|  1167837 | 13978 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   584158 | 13979 | `	}else{` |
|  1894935 | 13980 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 13981 | `	}` |
|  3062770 | 13982 | `	if( pRef->pNextCollide ){` |
|  1737758 | 13983 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   868881 | 13984 | `	}` |
|  3062770 | 13985 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 13986 | `	/* Release the node */` |
|  3062770 | 13987 | `	SySetRelease(&pRef->aReference);` |
|  3062770 | 13988 | `	SySetRelease(&pRef->aArrEntries);` |
|  3062770 | 13989 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3062770 | 13990 | `	pVm->nRefUsed--;` |
|  3062770 | 13991 | `	return SXRET_OK;` |
|        2 | 13992 |  |
|        - | 13993 | `/*` |
|        - | 13994 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13995 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13996 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13997 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13998 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13999 | ` * Refer to the official for more information on this powerful` |
|        - | 14000 | ` * extension.` |
|        - | 14001 | ` */` |
|  3132234 | 14002 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14003 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14004 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14005 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14006 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14007 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14008 | `	)` |
|        2 | 14009 |  |
|  3132236 | 14010 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14011 | `	VmRefObj *pRef;` |
|        - | 14012 | `	/* Check if the referenced object already exists */` |
|  3132236 | 14013 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3132236 | 14014 | `	if( pRef == 0 ){` |
|        - | 14015 | `		/* Create a new entry */` |
|  3099386 | 14016 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3099386 | 14017 | `		if( pRef == 0 ){` |
|      ! 0 | 14018 | `			return SXERR_MEM;` |
|        - | 14019 | `		}` |
|  3099386 | 14020 | `		pRef->iFlags = iFlags;` |
|        - | 14021 | `		/* Install the entry */` |
|  3099386 | 14022 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1549692 | 14023 | `	}` |
|  3132236 | 14024 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3132236 | 14025 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14026 | `		VmSlot sRef;` |
|        - | 14027 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14028 | `		 * be deleted when we leave this frame.` |
|        - | 14029 | `		 */` |
|    87602 | 14030 | `		sRef.nIdx = nIdx;` |
|    87602 | 14031 | `		sRef.pUserData = pEntry;` |
|    87602 | 14032 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14033 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14034 | `		}` |
|    43800 | 14035 | `	}` |
|  3132236 | 14036 | `	if( pEntry ){` |
|        - | 14037 | `		/* Address of the hash-entry */` |
|   120258 | 14038 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    60128 | 14039 | `	}` |
|  3132236 | 14040 | `	if( pMapEntry ){` |
|        - | 14041 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3005998 | 14042 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1502998 | 14043 | `	}` |
|  3132236 | 14044 | `	return SXRET_OK;` |
|  1566119 | 14045 |  |
|        - | 14046 | `/*` |
|        - | 14047 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14048 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14049 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14050 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14051 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14052 | ` * Refer to the official for more information on this powerful` |
|        - | 14053 | ` * extension.` |
|        - | 14054 | ` */` |
|  3051678 | 14055 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14056 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14057 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14058 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14059 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14060 | `	)` |
|        2 | 14061 |  |
|        - | 14062 | `	VmRefObj *pRef;` |
|        - | 14063 | `	sxu32 n;` |
|        - | 14064 | `	/* Check if the referenced object already exists */` |
|  3051680 | 14065 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3051680 | 14066 | `	if( pRef == 0 ){` |
|        - | 14067 | `		/* Not such entry */` |
|    87506 | 14068 | `		return SXERR_NOTFOUND;` |
|        - | 14069 | `	}` |
|        - | 14070 | `	/* Remove the desired entry */` |
|  2964176 | 14071 | `	if( pEntry ){` |
|        - | 14072 | `		SyHashEntry **apEntry;` |
|       56 | 14073 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 14074 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 14075 | `			if( apEntry[n] == pEntry ){` |
|        - | 14076 | `				/* Nullify the entry */` |
|       56 | 14077 | `				apEntry[n] = 0;` |
|        - | 14078 | `				/*` |
|        - | 14079 | `				 * NOTE:` |
|        - | 14080 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14081 | `				 * we avoid wasting spaces.` |
|        - | 14082 | `				 */` |
|       27 | 14083 | `			}` |
|       79 | 14084 | `		}` |
|       27 | 14085 | `	}` |
|  2964176 | 14086 | `	if( pMapEntry ){` |
|        - | 14087 | `		ph7_hashmap_node **apNode;` |
|  2964122 | 14088 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5928336 | 14089 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2964216 | 14090 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14091 | `				/* nullify the entry */` |
|  2964122 | 14092 | `				apNode[n] = 0;` |
|  1482060 | 14093 | `			}` |
|  1482109 | 14094 | `		}` |
|  1482060 | 14095 | `	}` |
|  2964176 | 14096 | `	return SXRET_OK;` |
|  1525841 | 14097 |  |
|        - | 14098 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14099 | `/*` |
|        - | 14100 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14101 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14102 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14103 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14104 | ` * For more information on how to register IO stream devices,please` |
|        - | 14105 | ` * refer to the official documentation.` |
|        - | 14106 | ` */` |
|    26234 | 14107 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14108 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14109 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14110 | `	int nByte              /* *pzDevice length*/` |
|        - | 14111 | `	)` |
|        2 | 14112 |  |
|        - | 14113 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14114 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14115 | `	SyString sDev,sCur;` |
|        - | 14116 | `	sxu32 n,nEntry;` |
|        - | 14117 | `	int rc;` |
|        - | 14118 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    26236 | 14119 | `	zNext = zCur = zIn = *pzDevice;` |
|    26236 | 14120 | `	zEnd = &zIn[nByte];` |
|  1668578 | 14121 | `	while( zIn < zEnd ){` |
|  1642346 | 14122 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14123 | `			/* Got one */` |
|        3 | 14124 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14125 | `			break;` |
|        - | 14126 | `		}` |
|        - | 14127 | `		/* Advance the cursor */` |
|  1642344 | 14128 | `		zIn++;` |
|        2 | 14129 | `	}` |
|    26236 | 14130 | `	if( zIn >= zEnd ){` |
|        - | 14131 | `		/* No such scheme,return the default stream */` |
|    26234 | 14132 | `		return pVm->pDefStream;` |
|        - | 14133 | `	}` |
|        3 | 14134 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14135 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14136 | `	SyStringFullTrim(&sDev);` |
|        - | 14137 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14138 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14139 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14140 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14141 | `		pStream = apStream[n];` |
|        3 | 14142 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14143 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14144 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14145 | `		if( rc == 0 ){` |
|        - | 14146 | `			/* Stream device found */` |
|        3 | 14147 | `			*pzDevice = zNext;` |
|        3 | 14148 | `			return pStream;` |
|        - | 14149 | `		}` |
|      ! 0 | 14150 | `	}` |
|        - | 14151 | `	/* No such stream,return NULL */` |
|      ! 0 | 14152 | `	return 0;` |
|    13119 | 14153 |  |
|        - | 14154 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14155 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14156 |  |
