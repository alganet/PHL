# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5398/7014 lines (76.96%)

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
|   828334 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   828336 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   828302 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   828292 |   104 | `	return FALSE;` |
|   414191 |   105 |  |
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
|   536884 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   536886 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   536886 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   536882 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   536882 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   536882 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   536882 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   536882 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   536882 |   152 | `	pCons->xExpand = xExpand;` |
|   536882 |   153 | `	pCons->pUserData = pUserData;` |
|   536882 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   536882 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   536882 |   161 | `	return SXRET_OK;` |
|   268444 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1180356 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1180358 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1180358 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1180358 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1180358 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1180358 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1180358 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1180358 |   195 | `	pFunc->pVm   = pVm;` |
|  1180358 |   196 | `	pFunc->xFunc = xFunc;` |
|  1180358 |   197 | `	pFunc->pUserData = pUserData;` |
|  1180358 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1180358 |   200 | `	*ppOut = pFunc;` |
|  1180358 |   201 | `	return SXRET_OK;` |
|   590180 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1182830 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1182832 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1182832 |   223 | `	if( pEntry ){` |
|     2476 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2476 |   225 | `		pFunc->pUserData = pUserData;` |
|     2476 |   226 | `		pFunc->xFunc = xFunc;` |
|     2476 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2476 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1180358 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1180358 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1180358 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1180358 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1180358 |   243 | `	return SXRET_OK;` |
|   591417 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   168912 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   168914 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   168914 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   168914 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   168914 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   168914 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   168914 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   168914 |   270 | `	pFunc->iFlags = iFlags;` |
|   168914 |   271 | `	pFunc->pUserData = pUserData;` |
|   168914 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   168914 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   663968 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   663970 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    36506 |   293 | `		pName = &pFunc->sName;` |
|    18252 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   663970 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   663970 |   297 | `	if( pEntry ){` |
|   517312 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   517312 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      188 |   301 | `			pFunc->pNextName = pLink;` |
|      188 |   302 | `			pEntry->pUserData = pFunc;` |
|       93 |   303 | `		}` |
|   517312 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   146660 |   307 | `	pFunc->pNextName = 0;` |
|   146660 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   146660 |   309 | `	return rc;` |
|   331986 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    47372 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    47374 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    47374 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    47374 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    47344 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    47344 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    47344 |   334 | `	return rc;` |
|    23688 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3408978 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3408980 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3408980 |   352 | `	sInstr.iP1 = iP1;` |
|  3408980 |   353 | `	sInstr.iP2 = iP2;` |
|  3408980 |   354 | `	sInstr.p3  = p3;` |
|  3408980 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   196346 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    98172 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3408980 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3408980 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3408980 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   404788 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   404790 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   404790 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   404790 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   202394 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   202396 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   193520 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   193522 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   193522 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|  1021056 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|  1021058 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   184238 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   184240 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   660600 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   660602 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    28512 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    28514 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    28514 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    28514 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    28514 |   427 | `	return &aInstr[n - 2];` |
|    14258 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    17400 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    17402 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    17402 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    17402 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    17402 |   447 | `	pFrame->pUserData = pUserData;` |
|    17402 |   448 | `	pFrame->pThis = pThis;` |
|    17402 |   449 | `	pFrame->pVm = pVm;` |
|    17402 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    17402 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    17402 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    17402 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    17402 |   454 | `	return pFrame;` |
|     8702 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    17358 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    17360 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    17360 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    17360 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    17360 |   476 | `	pVm->pFrame = pFrame;` |
|    17360 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    14612 |   479 | `		*ppFrame = pFrame;` |
|     7305 |   480 | `	}` |
|    17360 |   481 | `	return SXRET_OK;` |
|     8681 |   482 |  |
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
|    14610 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    14612 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    14612 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    14612 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    14612 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    14480 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    99624 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    85146 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    42574 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    14480 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    99680 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    85202 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    42602 |   545 | `			}` |
|     7239 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    14612 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    14612 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    14612 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    14612 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    14612 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     7305 |   554 | `	}` |
|    14612 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6586586 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6587382 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      796 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6586588 |   566 | `	return pFrame;` |
|        2 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Compare two functions signature and return the comparison result.` |
|        - |   570 | ` */` |
|      836 |   571 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   572 |  |
|      837 |   573 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   574 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   575 | `	const char *zSin = pSecond->zString;` |
|      837 |   576 | `	const char *zFin = pFirst->zString;` |
|      837 |   577 | `	const char *zPtr = zFin;` |
|      421 |   578 | `	for(;;){` |
|      843 |   579 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   580 | `			break;` |
|        - |   581 | `		}` |
|       19 |   582 | `		if( zFin[0] != zSin[0] ){` |
|        - |   583 | `			/* mismatch */` |
|       13 |   584 | `			break;` |
|        - |   585 | `		}` |
|        7 |   586 | `		zFin++;` |
|        7 |   587 | `		zSin++;` |
|        1 |   588 | `	}` |
|      837 |   589 | `	return (int)(zFin-zPtr);` |
|        1 |   590 |  |
|        - |   591 | `/*` |
|        - |   592 | ` * Select the appropriate VM function for the current call context.` |
|        - |   593 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   594 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   595 | ` * Refer to the official documentation for more information.` |
|        - |   596 | ` */` |
|      138 |   597 | `static ph7_vm_func * VmOverload(` |
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
|      140 |   610 | `	pLink = pList;` |
|      140 |   611 | `	i = 0;` |
|        - |   612 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   613 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   614 | `		if( pLink == 0 ){` |
|       78 |   615 | `			break;` |
|        - |   616 | `		}` |
|      948 |   617 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   618 | `			/* Candidate for overloading */` |
|      902 |   619 | `			apSet[i++] = pLink;` |
|      450 |   620 | `		}` |
|        - |   621 | `		/* Point to the next entry */` |
|      948 |   622 | `		pLink = pLink->pNextName;` |
|        2 |   623 | `	}` |
|      140 |   624 | `	if( i < 1 ){` |
|        - |   625 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   626 | `		return pList;` |
|        - |   627 | `	}` |
|      140 |   628 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   629 | `		/* Return the only candidate */` |
|       32 |   630 | `		return apSet[0];` |
|        - |   631 | `	}` |
|        - |   632 | `	/* Calculate function signature */` |
|      109 |   633 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   634 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   635 | `		int c = 'n'; /* null */` |
|      259 |   636 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   637 | `			/* Hashmap */` |
|       45 |   638 | `			c = 'h';` |
|      237 |   639 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   640 | `			/* bool */` |
|      ! 0 |   641 | `			c = 'b';` |
|      215 |   642 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   643 | `			/* int */` |
|        7 |   644 | `			c = 'i';` |
|      212 |   645 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   646 | `			/* String */` |
|      107 |   647 | `			c = 's';` |
|      156 |   648 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   649 | `			/* Float */` |
|      ! 0 |   650 | `			c = 'f';` |
|      103 |   651 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   652 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   653 | `			int marker = 'o';` |
|        3 |   654 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   655 | `			SyString *pName = &pClass->sName;` |
|        3 |   656 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   657 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   658 | `			c = -1;` |
|        1 |   659 | `		}` |
|      259 |   660 | `		if( c > 0 ){` |
|      257 |   661 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   662 | `		}` |
|      130 |   663 | `	}` |
|      109 |   664 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   665 | `	iTarget = 0;` |
|      109 |   666 | `	iMax = -1;` |
|        - |   667 | `	/* Select the appropriate function */` |
|      945 |   668 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   669 | `		/* Compare the two signatures */` |
|      837 |   670 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   671 | `		if( iCur > iMax ){` |
|      113 |   672 | `			iMax = iCur;` |
|      113 |   673 | `			iTarget = j;` |
|       56 |   674 | `		}` |
|      419 |   675 | `	}` |
|      109 |   676 | `	SyBlobRelease(&sSig);` |
|        - |   677 | `	/* Appropriate function for the current call context */` |
|      109 |   678 | `	return apSet[iTarget];` |
|       71 |   679 |  |
|        - |   680 | `/* Forward declaration */` |
|        - |   681 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   682 | `/*` |
|        - |   683 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   684 | ` * it can be instanciated from the executed PHP script.` |
|        - |   685 | ` */` |
|   130480 |   686 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   687 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   688 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   689 | `	)` |
|        2 |   690 |  |
|        - |   691 | `	ph7_class_method *pMeth;` |
|        - |   692 | `	ph7_class_attr *pAttr;` |
|        - |   693 | `	SyHashEntry *pEntry;` |
|        - |   694 | `	sxi32 rc;` |
|        - |   695 | `	/* Reset the loop cursor */` |
|   130482 |   696 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   697 | `	/* Process only static and constant attribute */` |
|   548576 |   698 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   699 | `		/* Extract the current attribute */` |
|   352856 |   700 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   352856 |   701 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   702 | `			ph7_value *pMemObj;` |
|        - |   703 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1470 |   704 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1470 |   705 | `			if( pMemObj == 0 ){` |
|      ! 0 |   706 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   707 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   708 | `					&pClass->sName,&pAttr->sName` |
|        - |   709 | `					);` |
|      ! 0 |   710 | `				return SXERR_MEM;` |
|        - |   711 | `			}` |
|     1470 |   712 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   713 | `				/* Initialize attribute default value (any complex expression) */` |
|     1468 |   714 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      733 |   715 | `			}` |
|        - |   716 | `			/* Record attribute index */` |
|     1470 |   717 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   718 | `			/* Install static attribute in the reference table */` |
|     1470 |   719 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   720 | `			/* If this is a typed static property, register the slot so the` |
|        - |   721 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   722 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   723 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1470 |   724 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        8 |   725 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        8 |   726 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   727 | `					return SXERR_MEM;` |
|        - |   728 | `				}` |
|        8 |   729 | `				pVmAttrS->pAttr = pAttr;` |
|        8 |   730 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        8 |   731 | `				pVmAttrS->iState = 0;` |
|        8 |   732 | `				pVmAttrS->pOwner = pClass;` |
|        - |   733 | `				/* Static typed property with no default starts uninitialized */` |
|        6 |   734 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        6 |   735 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        3 |   736 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        1 |   737 | `				}` |
|        8 |   738 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   739 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   740 | `					return SXERR_MEM;` |
|        - |   741 | `				}` |
|        3 |   742 | `			}` |
|      734 |   743 | `		}` |
|        2 |   744 | `	}` |
|        - |   745 | `	/* Install class methods */` |
|   130482 |   746 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   747 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   748 | `		 */` |
|    56860 |   749 | `		return SXRET_OK;` |
|        - |   750 | `	}` |
|        - |   751 | `	/* Create constructor alias if not yet done */` |
|    73624 |   752 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   753 | `		/* User constructor with the same base class name */` |
|     5654 |   754 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5654 |   755 | `		if( pEntry ){` |
|      ! 0 |   756 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   757 | `			/* Create the alias */` |
|      ! 0 |   758 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   759 | `		}` |
|     2826 |   760 | `	}` |
|        - |   761 | `	/* Install the methods now */` |
|    73624 |   762 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   737907 |   763 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   627474 |   764 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   627474 |   765 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   627466 |   766 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   627466 |   767 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   768 | `				return rc;` |
|        - |   769 | `			}` |
|   313732 |   770 | `		}` |
|        2 |   771 | `	}` |
|        - |   772 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    73624 |   773 | `	pClass->bMounted = TRUE;` |
|    73624 |   774 | `	return SXRET_OK;` |
|    65242 |   775 |  |
|        - |   776 | `/*` |
|        - |   777 | ` * Allocate a private frame for attributes of the given` |
|        - |   778 | ` * class instance (Object in the PHP jargon).` |
|        - |   779 | ` */` |
|     1402 |   780 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   781 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   782 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   783 | `	)` |
|        2 |   784 |  |
|     1404 |   785 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   786 | `	ph7_class_attr *pAttr;` |
|        - |   787 | `	SyHashEntry *pEntry;` |
|        - |   788 | `	sxi32 rc;` |
|        - |   789 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1404 |   790 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5708 |   791 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   792 | `		VmClassAttr *pVmAttr;` |
|        - |   793 | `		/* Extract the current attribute */` |
|     4306 |   794 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4306 |   795 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     4306 |   796 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   797 | `			return SXERR_MEM;` |
|        - |   798 | `		}` |
|     4306 |   799 | `		pVmAttr->pAttr = pAttr;` |
|     4306 |   800 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   801 | `			ph7_value *pMemObj;` |
|        - |   802 | `			/* Reserve a memory object for this attribute */` |
|     4282 |   803 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4282 |   804 | `			if( pMemObj == 0 ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|     4282 |   808 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     4282 |   809 | `			pVmAttr->iState = 0;` |
|     4282 |   810 | `			pVmAttr->pOwner = pClass;` |
|     4282 |   811 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   812 | `				/* Initialize attribute default value (any complex expression) */` |
|     1448 |   813 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     3559 |   814 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   815 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   816 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       24 |   817 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       11 |   818 | `			}` |
|     4282 |   819 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     4282 |   820 | `			if( rc != SXRET_OK ){` |
|        - |   821 | `				VmSlot sSlot;` |
|        - |   822 | `				/* Restore memory object */` |
|      ! 0 |   823 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   824 | `				sSlot.pUserData = 0;` |
|      ! 0 |   825 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   826 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   827 | `				return SXERR_MEM;` |
|        - |   828 | `			}` |
|        - |   829 | `			/* Install attribute in the reference table */` |
|     4282 |   830 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   831 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   832 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   833 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     4282 |   834 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      104 |   835 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      104 |   836 | `				if( rc != SXRET_OK ){` |
|        - |   837 | `					VmSlot sSlot;` |
|      ! 0 |   838 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   839 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   840 | `					sSlot.pUserData = 0;` |
|      ! 0 |   841 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   842 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   843 | `					return SXERR_MEM;` |
|        - |   844 | `				}` |
|       51 |   845 | `			}` |
|     2142 |   846 | `		}else{` |
|        - |   847 | `			/* Install static/constant attribute */` |
|       26 |   848 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   849 | `			pVmAttr->iState = 0;` |
|       26 |   850 | `			pVmAttr->pOwner = pClass;` |
|       26 |   851 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   854 | `				return SXERR_MEM;` |
|        - |   855 | `			}` |
|        - |   856 | `		}` |
|        2 |   857 | `	}` |
|     1404 |   858 | `	return SXRET_OK;` |
|      703 |   859 |  |
|        - |   860 | `/* Forward declaration */` |
|        - |   861 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   862 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   863 | `/*` |
|        - |   864 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   865 | ` */` |
|        - |   866 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   867 | `/*` |
|        - |   868 | ` * Reserve a constant memory object.` |
|        - |   869 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   870 | ` */` |
|   391370 |   871 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   872 |  |
|        - |   873 | `	ph7_value *pObj;` |
|        - |   874 | `	sxi32 rc;` |
|   391372 |   875 | `	if( pIndex ){` |
|        - |   876 | `		/* Object index in the object table */` |
|   383128 |   877 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   191563 |   878 | `	}` |
|        - |   879 | `	/* Reserve a slot for the new object */` |
|   391372 |   880 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   391372 |   881 | `	if( rc != SXRET_OK ){` |
|        - |   882 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   883 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   884 | `		 */` |
|      ! 0 |   885 | `		return 0;` |
|        - |   886 | `	}` |
|   391372 |   887 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   391372 |   888 | `	return pObj;` |
|   195687 |   889 |  |
|        - |   890 | `/*` |
|        - |   891 | ` * Reserve a memory object.` |
|        - |   892 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   893 | ` */` |
|  2145070 |   894 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   895 |  |
|        - |   896 | `	ph7_value *pObj;` |
|        - |   897 | `	sxi32 rc;` |
|  2145072 |   898 | `	if( pIndex ){` |
|        - |   899 | `		/* Object index in the object table */` |
|  2145072 |   900 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1072535 |   901 | `	}` |
|        - |   902 | `	/* Reserve a slot for the new object */` |
|  2145072 |   903 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2145072 |   904 | `	if( rc != SXRET_OK ){` |
|        - |   905 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   906 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   907 | `		 */` |
|      ! 0 |   908 | `		return 0;` |
|        - |   909 | `	}` |
|  2145072 |   910 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2145072 |   911 | `	return pObj;` |
|  1072537 |   912 |  |
|        - |   913 | `/* Forward declaration */` |
|        - |   914 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   915 | `/* Forward declarations for Fiber C functions */` |
|        - |   916 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   917 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   918 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   919 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   920 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   921 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   922 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   923 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   924 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   925 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   926 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   927 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   928 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   929 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   930 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   931 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   932 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   933 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   934 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   935 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   936 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   937 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   938 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   939 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   940 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   941 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   942 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   943 | `/*` |
|        - |   944 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   945 | ` * directly as foreign functions.` |
|        - |   946 | ` */` |
|        - |   947 | `#define PH7_BUILTIN_LIB \` |
|        - |   948 | `	"class Exception { "\` |
|        - |   949 | `    "protected $message = 'Unknown exception';"\` |
|        - |   950 | `    "protected $code = 0;"\` |
|        - |   951 | `    "protected $file;"\` |
|        - |   952 | `    "protected $line;"\` |
|        - |   953 | `    "protected $trace;"\` |
|        - |   954 | `    "protected $previous;"\` |
|        - |   955 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   956 | `	"   if( isset($message) ){"\` |
|        - |   957 | `	"	  $this->message = $message;"\` |
|        - |   958 | `	"   }"\` |
|        - |   959 | `	"   $this->code = $code;"\` |
|        - |   960 | `	"   $this->file = __FILE__;"\` |
|        - |   961 | `	"   $this->line = __LINE__;"\` |
|        - |   962 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   963 | `	"   if( isset($previous) ){"\` |
|        - |   964 | `	"     $this->previous = $previous;"\` |
|        - |   965 | `	"   }"\` |
|        - |   966 | `	"}"\` |
|        - |   967 | `	"public function getMessage(){"\` |
|        - |   968 | `	"   return $this->message;"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	" public function getCode(){"\` |
|        - |   971 | `	"  return $this->code;"\` |
|        - |   972 | `	"}"\` |
|        - |   973 | `	"public function getFile(){"\` |
|        - |   974 | `	"  return $this->file;"\` |
|        - |   975 | `	"}"\` |
|        - |   976 | `	"public function getLine(){"\` |
|        - |   977 | `	"  return $this->line;"\` |
|        - |   978 | `	"}"\` |
|        - |   979 | `	"public function getTrace(){"\` |
|        - |   980 | `	"   return $this->trace;"\` |
|        - |   981 | `	"}"\` |
|        - |   982 | `	"public function getTraceAsString(){"\` |
|        - |   983 | `	"  return debug_string_backtrace();"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"public function getPrevious(){"\` |
|        - |   986 | `	"    return $this->previous;"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"public function __toString(){"\` |
|        - |   989 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   990 | `    "}"\` |
|        - |   991 | `	"}"\` |
|        - |   992 | `	"class Error extends Exception { }"\` |
|        - |   993 | `	"class TypeError extends Error { }"\` |
|        - |   994 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   995 | `	"class ValueError extends Error { }"\` |
|        - |   996 | `	"class FiberError extends Error { }"\` |
|        - |   997 | `	"class AssertionError extends Error { }"\` |
|        - |   998 | `	"class ArithmeticError extends Error { }"\` |
|        - |   999 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1000 | `	"class ErrorException extends Exception { "\` |
|        - |  1001 | `	"protected $severity;"\` |
|        - |  1002 | `	"public function __construct(string $message = null,"\` |
|        - |  1003 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |  1004 | `	"   if( isset($message) ){"\` |
|        - |  1005 | `	"	  $this->message = $message;"\` |
|        - |  1006 | `	"   }"\` |
|        - |  1007 | `	"   $this->severity = $severity;"\` |
|        - |  1008 | `	"   $this->code = $code;"\` |
|        - |  1009 | `	"   $this->file = $filename;"\` |
|        - |  1010 | `	"   $this->line = $lineno;"\` |
|        - |  1011 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1012 | `	"   if( isset($previous) ){"\` |
|        - |  1013 | `	"     $this->previous = $previous;"\` |
|        - |  1014 | `	"   }"\` |
|        - |  1015 | `	"}"\` |
|        - |  1016 | `	"public function getSeverity(){"\` |
|        - |  1017 | `	"   return $this->severity;"\` |
|        - |  1018 | `    "}"\` |
|        - |  1019 | `	"}"\` |
|        - |  1020 | `	"interface Iterator {"\` |
|        - |  1021 | `	"public function current();"\` |
|        - |  1022 | `	"public function key();"\` |
|        - |  1023 | `	"public function next();"\` |
|        - |  1024 | `	"public function rewind();"\` |
|        - |  1025 | `	"public function valid();"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"interface IteratorAggregate {"\` |
|        - |  1028 | `	"public function getIterator();"\` |
|        - |  1029 | `	"}"\` |
|        - |  1030 | `	"interface Serializable {"\` |
|        - |  1031 | `	"public function serialize();"\` |
|        - |  1032 | `	"public function unserialize(string $serialized);"\` |
|        - |  1033 | `	"}"\` |
|        - |  1034 | `	"/* Directory releated IO */"\` |
|        - |  1035 | `	"class Directory {"\` |
|        - |  1036 | `	"public $handle = null;"\` |
|        - |  1037 | `	"public $path  = null;"\` |
|        - |  1038 | `	"public function __construct(string $path)"\` |
|        - |  1039 | `	"{"\` |
|        - |  1040 | `	"   $this->handle = opendir($path);"\` |
|        - |  1041 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1042 | `	"      $this->path = $path;"\` |
|        - |  1043 | `	"   }"\` |
|        - |  1044 | `	"}"\` |
|        - |  1045 | `	"public function __destruct()"\` |
|        - |  1046 | `	"{"\` |
|        - |  1047 | `	"  if( $this->handle != null ){"\` |
|        - |  1048 | `	"       closedir($this->handle);"\` |
|        - |  1049 | `	"  }"\` |
|        - |  1050 | `	"}"\` |
|        - |  1051 | `	"public function read()"\` |
|        - |  1052 | `	"{"\` |
|        - |  1053 | `	"    return readdir($this->handle);"\` |
|        - |  1054 | `	"}"\` |
|        - |  1055 | `	"public function rewind()"\` |
|        - |  1056 | `	"{"\` |
|        - |  1057 | `	"    rewinddir($this->handle);"\` |
|        - |  1058 | `	"}"\` |
|        - |  1059 | `	"public function close()"\` |
|        - |  1060 | `	"{"\` |
|        - |  1061 | `	"    closedir($this->handle);"\` |
|        - |  1062 | `	"    $this->handle = null;"\` |
|        - |  1063 | `	"}"\` |
|        - |  1064 | `	"}"\` |
|        - |  1065 | `	"class Fiber {"\` |
|        - |  1066 | `	"  private $__ctx;"\` |
|        - |  1067 | `	"  private $__callable;"\` |
|        - |  1068 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1069 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1070 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1071 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1072 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1073 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1074 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1075 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1076 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1077 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1078 | `	"}"\` |
|        - |  1079 | `	"class Generator implements Iterator {"\` |
|        - |  1080 | `	"  private $__ctx;"\` |
|        - |  1081 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1082 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1083 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1084 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1085 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1086 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1087 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1088 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1089 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1090 | `	"}"\` |
|        - |  1091 | `	"class stdClass{"\` |
|        - |  1092 | `	"  public $value;"\` |
|        - |  1093 | `	" /* Magic methods */"\` |
|        - |  1094 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1095 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1096 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1097 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1098 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"function dir(string $path){"\` |
|        - |  1101 | `	"   return new Directory($path);"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"function Dir(string $path){"\` |
|        - |  1104 | `	"   return new Directory($path);"\` |
|        - |  1105 | `	"}"\` |
|        - |  1106 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1107 | `    "{"\` |
|        - |  1108 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1109 | `	"  $aDir = array();"\` |
|        - |  1110 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1111 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1112 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1113 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1114 | `	"   }"\` |
|        - |  1115 | `	"  closedir($pHandle);"\` |
|        - |  1116 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1117 | `	"      rsort($aDir);"\` |
|        - |  1118 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1119 | `	"      sort($aDir);"\` |
|        - |  1120 | `	"  }"\` |
|        - |  1121 | `	"  return $aDir;"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1124 | `	"/* Open the target directory */"\` |
|        - |  1125 | `	"$zDir = dirname($pattern);"\` |
|        - |  1126 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1127 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1128 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1129 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1130 | `	"	return FALSE;"\` |
|        - |  1131 | `	"}"\` |
|        - |  1132 | `	"$pattern = basename($pattern);"\` |
|        - |  1133 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1134 | `	"/* Loop throw available entries */"\` |
|        - |  1135 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1136 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1137 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1138 | `	"	if( $rc ){"\` |
|        - |  1139 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1140 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1141 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1142 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1143 | `	"		  }"\` |
|        - |  1144 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1145 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1146 | `	"		 continue;"\` |
|        - |  1147 | `	"	   }"\` |
|        - |  1148 | `	"	   /* Add the entry */"\` |
|        - |  1149 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1150 | `	"	}"\` |
|        - |  1151 | `	" }"\` |
|        - |  1152 | `	"/* Close the handle */"\` |
|        - |  1153 | `	"closedir($pHandle);"\` |
|        - |  1154 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1155 | `	"  /* Sort the array */"\` |
|        - |  1156 | `	"  sort($pArray);"\` |
|        - |  1157 | `	"}"\` |
|        - |  1158 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1159 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1160 | `	"  $pArray[] = $pattern;"\` |
|        - |  1161 | `	"}"\` |
|        - |  1162 | `	"/* Return the created array */"\` |
|        - |  1163 | `	"return $pArray;"\` |
|        - |  1164 | `   "}"\` |
|        - |  1165 | `   "/* Creates a temporary file */"\` |
|        - |  1166 | `   "function tmpfile(){"\` |
|        - |  1167 | `   "  /* Extract the temp directory */"\` |
|        - |  1168 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1169 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1170 | `   "    /* Use the current dir */"\` |
|        - |  1171 | `   "    $zTempDir = '.';"\` |
|        - |  1172 | `   "  }"\` |
|        - |  1173 | `   "  /* Create the file */"\` |
|        - |  1174 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1175 | `   "  return $pHandle;"\` |
|        - |  1176 | `   "}"\` |
|        - |  1177 | `   "/* Creates a temporary filename */"\` |
|        - |  1178 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1179 | `   "{"\` |
|        - |  1180 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1181 | `   "}"\` |
|        - |  1182 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1183 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1184 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1185 | `   "/* Copy arguments */"\` |
|        - |  1186 | `   "$nArgs = func_num_args();"\` |
|        - |  1187 | `   "$pNew = array();"\` |
|        - |  1188 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1189 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1190 | `    "}"\` |
|        - |  1191 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1192 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1193 | `	"/* Erase */"\` |
|        - |  1194 | `	"array_erase($pArray);"\` |
|        - |  1195 | `	"/* Unshift */"\` |
|        - |  1196 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1197 | `	"return sizeof($pArray);"\` |
|        - |  1198 | `    "}"\` |
|        - |  1199 | `	"function array_merge_recursive(){"\` |
|        - |  1200 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1201 | `    "$arrays = func_get_args();"\` |
|        - |  1202 | `    "$narrays = count($arrays);"\` |
|        - |  1203 | `    "$ret = array();"\` |
|        - |  1204 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1205 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1206 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1207 | `	 " }"\` |
|        - |  1208 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1209 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1210 | `     "  if( $keyIsInt ) {"\` |
|        - |  1211 | `     "   $ret[] = $value;"\` |
|        - |  1212 | `     "  } else {"\` |
|        - |  1213 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1214 | `     "    $cur = $ret[$key];"\` |
|        - |  1215 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1216 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1217 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1218 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1219 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1220 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1221 | `     "    } else {"\` |
|        - |  1222 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1223 | `     "    }"\` |
|        - |  1224 | `     "   } else {"\` |
|        - |  1225 | `     "    $ret[$key] = $value;"\` |
|        - |  1226 | `     "   }"\` |
|        - |  1227 | `     "  }"\` |
|        - |  1228 | `     " }"\` |
|        - |  1229 | `	 " }"\` |
|        - |  1230 | `	 " return $ret;"\` |
|        - |  1231 | `    "}"\` |
|        - |  1232 | `	"function max(){"\` |
|        - |  1233 | `    "  $pArgs = func_get_args();"\` |
|        - |  1234 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1235 | `	"  return null;"\` |
|        - |  1236 | `    " }"\` |
|        - |  1237 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1238 | `    " $pArg = $pArgs[0];"\` |
|        - |  1239 | `	" if( !is_array($pArg) ){"\` |
|        - |  1240 | `	"   return $pArg; "\` |
|        - |  1241 | `	" }"\` |
|        - |  1242 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1243 | `	"   return null;"\` |
|        - |  1244 | `	" }"\` |
|        - |  1245 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1246 | `	" reset($pArg);"\` |
|        - |  1247 | `	" $max = current($pArg);"\` |
|        - |  1248 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1249 | `	"   if( $val > $max ){"\` |
|        - |  1250 | `	"     $max = $val;"\` |
|        - |  1251 | `    " }"\` |
|        - |  1252 | `	" }"\` |
|        - |  1253 | `	" return $max;"\` |
|        - |  1254 | `    " }"\` |
|        - |  1255 | `    " $max = $pArgs[0];"\` |
|        - |  1256 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1257 | `    " $val = $pArgs[$i];"\` |
|        - |  1258 | `	"if( $val > $max ){"\` |
|        - |  1259 | `	" $max = $val;"\` |
|        - |  1260 | `	"}"\` |
|        - |  1261 | `    " }"\` |
|        - |  1262 | `	" return $max;"\` |
|        - |  1263 | `    "}"\` |
|        - |  1264 | `	"function min(){"\` |
|        - |  1265 | `    "  $pArgs = func_get_args();"\` |
|        - |  1266 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1267 | `	"  return null;"\` |
|        - |  1268 | `    " }"\` |
|        - |  1269 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1270 | `    " $pArg = $pArgs[0];"\` |
|        - |  1271 | `	" if( !is_array($pArg) ){"\` |
|        - |  1272 | `	"   return $pArg; "\` |
|        - |  1273 | `	" }"\` |
|        - |  1274 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1275 | `	"   return null;"\` |
|        - |  1276 | `	" }"\` |
|        - |  1277 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1278 | `	" reset($pArg);"\` |
|        - |  1279 | `	" $min = current($pArg);"\` |
|        - |  1280 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1281 | `	"   if( $val < $min ){"\` |
|        - |  1282 | `	"     $min = $val;"\` |
|        - |  1283 | `    " }"\` |
|        - |  1284 | `	" }"\` |
|        - |  1285 | `	" return $min;"\` |
|        - |  1286 | `    " }"\` |
|        - |  1287 | `    " $min = $pArgs[0];"\` |
|        - |  1288 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1289 | `    " $val = $pArgs[$i];"\` |
|        - |  1290 | `	"if( $val < $min ){"\` |
|        - |  1291 | `	" $min = $val;"\` |
|        - |  1292 | `	" }"\` |
|        - |  1293 | `    " }"\` |
|        - |  1294 | `	" return $min;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"function fileowner(string $file){"\` |
|        - |  1297 | `    " $a = stat($file);"\` |
|        - |  1298 | `	" if( !is_array($a) ){"\` |
|        - |  1299 | `	"	return false;"\` |
|        - |  1300 | `	" }"\` |
|        - |  1301 | `	" return $a['uid'];"\` |
|        - |  1302 | `    "}"\` |
|        - |  1303 | `    "function filegroup(string $file){"\` |
|        - |  1304 | `	" $a = stat($file);"\` |
|        - |  1305 | `	" if( !is_array($a) ){"\` |
|        - |  1306 | `	"	return false;"\` |
|        - |  1307 | `	" }"\` |
|        - |  1308 | `	" return $a['gid'];"\` |
|        - |  1309 | `    "}"\` |
|        - |  1310 | `	 "function fileinode(string $file){"\` |
|        - |  1311 | `	" $a = stat($file);"\` |
|        - |  1312 | `	" if( !is_array($a) ){"\` |
|        - |  1313 | `	"	return false;"\` |
|        - |  1314 | `	" }"\` |
|        - |  1315 | `	" return $a['ino'];"\` |
|        - |  1316 | `    "}"` |
|        - |  1317 |  |
|        - |  1318 | `/*` |
|        - |  1319 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1320 | ` * start compiling the target PHP program.` |
|        - |  1321 | ` */` |
|     2748 |  1322 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1323 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1324 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1325 | `	 )` |
|        2 |  1326 |  |
|        - |  1327 | `	SyString sBuiltin;` |
|        - |  1328 | `	ph7_value *pObj;` |
|        - |  1329 | `	sxi32 rc;` |
|        - |  1330 | `	/* Zero the structure */` |
|     2750 |  1331 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1332 | `	/* Initialize VM fields */` |
|     2750 |  1333 | `	pVm->pEngine = &(*pEngine);` |
|     2750 |  1334 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1335 | `	/* Instructions containers */` |
|     2750 |  1336 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2750 |  1337 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2750 |  1338 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1339 | `	/* Object containers */` |
|     2750 |  1340 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2750 |  1341 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1342 | `	/* Virtual machine internal containers */` |
|     2750 |  1343 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2750 |  1344 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2750 |  1345 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2750 |  1346 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2750 |  1347 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2750 |  1348 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2750 |  1349 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2750 |  1350 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2750 |  1351 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2750 |  1352 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2750 |  1353 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2750 |  1354 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2750 |  1355 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2750 |  1356 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2750 |  1357 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2750 |  1358 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2750 |  1359 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2750 |  1360 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2750 |  1361 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2750 |  1362 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2750 |  1363 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2750 |  1364 | `	pVm->pPendingException = 0;` |
|        - |  1365 | `	/* Configuration containers */` |
|     2750 |  1366 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2750 |  1367 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2750 |  1368 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2750 |  1369 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2750 |  1370 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2750 |  1371 | `	pVm->iResponseStatus = 200;` |
|     2750 |  1372 | `	pVm->bHeadersSent = 0;` |
|     2750 |  1373 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1374 | `	/* Error callbacks containers */` |
|     2750 |  1375 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2750 |  1376 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2750 |  1377 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2750 |  1378 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2750 |  1379 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1380 | `	/* Set a default recursion limit */` |
|        - |  1381 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2750 |  1382 | `	pVm->nMaxDepth = 32;` |
|        - |  1383 | `#else` |
|        - |  1384 | `	pVm->nMaxDepth = 16;` |
|        - |  1385 | `#endif` |
|        - |  1386 | `	/* Default assertion flags */` |
|     2750 |  1387 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1388 | `	/* JSON return status */` |
|     2750 |  1389 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1390 | `	/* PRNG context */` |
|     2750 |  1391 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1392 | `	/* Install the null constant */` |
|     2750 |  1393 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2750 |  1394 | `	if( pObj == 0 ){` |
|      ! 0 |  1395 | `		rc = SXERR_MEM;` |
|      ! 0 |  1396 | `		goto Err;` |
|        - |  1397 | `	}` |
|     2750 |  1398 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1399 | `	/* Install the boolean TRUE constant */` |
|     2750 |  1400 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2750 |  1401 | `	if( pObj == 0 ){` |
|      ! 0 |  1402 | `		rc = SXERR_MEM;` |
|      ! 0 |  1403 | `		goto Err;` |
|        - |  1404 | `	}` |
|     2750 |  1405 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1406 | `	/* Install the boolean FALSE constant */` |
|     2750 |  1407 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2750 |  1408 | `	if( pObj == 0 ){` |
|      ! 0 |  1409 | `		rc = SXERR_MEM;` |
|      ! 0 |  1410 | `		goto Err;` |
|        - |  1411 | `	}` |
|     2750 |  1412 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1413 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1414 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1415 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2750 |  1416 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2750 |  1417 | `	if( pObj == 0 ){` |
|      ! 0 |  1418 | `		rc = SXERR_MEM;` |
|      ! 0 |  1419 | `		goto Err;` |
|        - |  1420 | `	}` |
|     2750 |  1421 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1422 | `	/* Create the global frame */` |
|     2750 |  1423 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2750 |  1424 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1425 | `		goto Err;` |
|        - |  1426 | `	}` |
|        - |  1427 | `	/* Initialize the code generator */` |
|     2750 |  1428 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2750 |  1429 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1430 | `		goto Err;` |
|        - |  1431 | `	}` |
|        - |  1432 | `	/* VM correctly initialized,set the magic number */` |
|     2750 |  1433 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2750 |  1434 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1435 | `	/* Compile the built-in library */` |
|     2750 |  1436 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1437 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2750 |  1438 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1439 | `	/* Register Fiber internal C functions */` |
|     2750 |  1440 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2750 |  1441 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2750 |  1442 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2750 |  1443 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2750 |  1444 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2750 |  1445 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2750 |  1446 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2750 |  1447 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2750 |  1448 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2750 |  1449 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1450 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2750 |  1451 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2750 |  1452 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2750 |  1453 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2750 |  1454 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2750 |  1455 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2750 |  1456 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2750 |  1457 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2750 |  1458 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2750 |  1459 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2750 |  1460 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1461 | `	/* Reset the code generator */` |
|     2750 |  1462 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2750 |  1463 | `	return SXRET_OK;` |
|      ! 0 |  1464 | `Err:` |
|      ! 0 |  1465 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1466 | `	return rc;` |
|     1376 |  1467 |  |
|        - |  1468 | `/*` |
|        - |  1469 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1470 | ` * routine which store the output in an internal blob.` |
|        - |  1471 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1472 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1473 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1474 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1475 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1476 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1477 | ` * to finish executing and extracting the output.` |
|        - |  1478 | ` */` |
|       38 |  1479 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1480 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1481 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1482 | `	void *pUserData     /* User private data */` |
|        - |  1483 | `	)` |
|      ! 0 |  1484 |  |
|        - |  1485 | `	 sxi32 rc;` |
|        - |  1486 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1487 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1488 | `	 return rc;` |
|      ! 0 |  1489 |  |
|        - |  1490 | `/*` |
|        - |  1491 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1492 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1493 | ` */` |
|    15470 |  1494 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1495 |  |
|    15472 |  1496 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    15472 |  1497 | `	if( xCons != VmObConsumer ){` |
|     6758 |  1498 | `		pVm->nOutputLen += nLen;` |
|     6758 |  1499 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      862 |  1500 | `			pVm->bHeadersSent = 1;` |
|      430 |  1501 | `		}` |
|     3378 |  1502 | `	}` |
|    15472 |  1503 |  |
|        - |  1504 | `#define VM_STACK_GUARD 16` |
|        - |  1505 | `/*` |
|        - |  1506 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1507 | ` * our compiled PHP program.` |
|        - |  1508 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1509 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1510 | ` */` |
|    35512 |  1511 | `static ph7_value * VmNewOperandStack(` |
|        - |  1512 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1513 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1514 | `	)` |
|        2 |  1515 |  |
|        - |  1516 | `	ph7_value *pStack;` |
|        - |  1517 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1518 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1519 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1520 | `  ** on the maximum stack depth required.` |
|        - |  1521 | `  **` |
|        - |  1522 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1523 | `  */` |
|    35514 |  1524 | `	nInstr += VM_STACK_GUARD;` |
|    35514 |  1525 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    35514 |  1526 | `	if( pStack == 0 ){` |
|      ! 0 |  1527 | `		return 0;` |
|        - |  1528 | `	}` |
|        - |  1529 | `	/* Initialize the operand stack */` |
|  2284050 |  1530 | `	while( nInstr > 0 ){` |
|  2248538 |  1531 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2248538 |  1532 | `		--nInstr;` |
|        2 |  1533 | `	}` |
|        - |  1534 | `	/* Ready for bytecode execution */` |
|    35514 |  1535 | `	return pStack;` |
|    17758 |  1536 |  |
|        - |  1537 | `/* Forward declaration */` |
|        - |  1538 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1539 | `/*` |
|        - |  1540 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1541 | ` * This routine gets called by the PH7 engine after` |
|        - |  1542 | ` * successful compilation of the target PHP program.` |
|        - |  1543 | ` */` |
|     2474 |  1544 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1545 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1546 | `	)` |
|        2 |  1547 |  |
|        - |  1548 | `	SyHashEntry *pEntry;` |
|        - |  1549 | `	sxi32 rc;` |
|     2476 |  1550 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1551 | `		/* Initialize your VM first */` |
|      ! 0 |  1552 | `		return SXERR_CORRUPT;` |
|        - |  1553 | `	}` |
|        - |  1554 | `	/* Mark the VM ready for byte-code execution */` |
|     2476 |  1555 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1556 | `	/* Release the code generator now we have compiled our program */` |
|     2476 |  1557 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1558 | `	/* Emit the DONE instruction */` |
|     2476 |  1559 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2476 |  1560 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1561 | `		return SXERR_MEM;` |
|        - |  1562 | `	}` |
|        - |  1563 | `	/* Script return value */` |
|     2476 |  1564 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1565 | `	/* Allocate a new operand stack */` |
|     2476 |  1566 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2476 |  1567 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1568 | `		return SXERR_MEM;` |
|        - |  1569 | `	}` |
|        - |  1570 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1571 | `	 * private data. */` |
|     2476 |  1572 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2476 |  1573 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1574 | `	/* Allocate the reference table */` |
|     2476 |  1575 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2476 |  1576 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2476 |  1577 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1578 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1579 | `		return SXERR_MEM;` |
|        - |  1580 | `	}` |
|        - |  1581 | `	/* Zero the reference table */` |
|     2476 |  1582 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1583 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2476 |  1584 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2476 |  1585 | `	if( rc != SXRET_OK ){` |
|        - |  1586 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1587 | `		return rc;` |
|        - |  1588 | `	}` |
|        - |  1589 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2476 |  1590 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2476 |  1591 | `	if( rc != SXRET_OK ){` |
|        - |  1592 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1593 | `		return rc;` |
|        - |  1594 | `	}` |
|        - |  1595 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2476 |  1596 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1597 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2476 |  1598 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1599 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2476 |  1600 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1601 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1602 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2476 |  1603 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2476 |  1604 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1605 | `#endif` |
|        - |  1606 | `	/* Initialize and install static and constants class attributes */` |
|     2476 |  1607 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    44756 |  1608 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    42282 |  1609 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    42282 |  1610 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1611 | `			return rc;` |
|        - |  1612 | `		}` |
|        2 |  1613 | `	}` |
|        - |  1614 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2476 |  1615 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1616 | `	/* VM is ready for bytecode execution */` |
|     2476 |  1617 | `	return SXRET_OK;` |
|     1239 |  1618 |  |
|        - |  1619 | `/*` |
|        - |  1620 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1621 | ` */` |
|      ! 0 |  1622 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1623 |  |
|      ! 0 |  1624 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1625 | `		return SXERR_CORRUPT;` |
|        - |  1626 | `	}` |
|        - |  1627 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1628 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1629 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1630 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1631 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1632 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1633 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1634 | `	pVm->bHttpContext = 0;` |
|        - |  1635 | `	/* Set the ready flag */` |
|      ! 0 |  1636 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1637 | `	return SXRET_OK;` |
|      ! 0 |  1638 |  |
|        - |  1639 | `/*` |
|        - |  1640 | ` * Release a Virtual Machine.` |
|        - |  1641 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1642 | ` */` |
|     2466 |  1643 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1644 |  |
|        - |  1645 | `	/* Set the stale magic number */` |
|     2468 |  1646 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1647 | `	/* Release the private memory subsystem */` |
|     2468 |  1648 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2468 |  1649 | `	return SXRET_OK;` |
|        2 |  1650 |  |
|        - |  1651 | `/*` |
|        - |  1652 | ` * Initialize a foreign function call context.` |
|        - |  1653 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1654 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1655 | ` * functions.` |
|        - |  1656 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1657 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1658 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1659 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1660 | ` */` |
|   613906 |  1661 | `static sxi32 VmInitCallContext(` |
|        - |  1662 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1663 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1664 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1665 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1666 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1667 | `	)` |
|        2 |  1668 |  |
|   613908 |  1669 | `	pOut->pFunc = pFunc;` |
|   613908 |  1670 | `	pOut->pVm   = pVm;` |
|   613908 |  1671 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   613908 |  1672 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1673 | `	/* Assume a null return value */` |
|   613908 |  1674 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   613908 |  1675 | `	pOut->pRet = pRet;` |
|   613908 |  1676 | `	pOut->iFlags = iFlags;` |
|   613908 |  1677 | `	return SXRET_OK;` |
|        2 |  1678 |  |
|        - |  1679 | `/*` |
|        - |  1680 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1681 | ` * left behind.` |
|        - |  1682 | ` */` |
|   613906 |  1683 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1684 |  |
|        - |  1685 | `	sxu32 n;` |
|   613908 |  1686 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7448 |  1687 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    21374 |  1688 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13928 |  1689 | `			if( apObj[n] == 0 ){` |
|        - |  1690 | `				/* Already released */` |
|      298 |  1691 | `				continue;` |
|        - |  1692 | `			}` |
|    13632 |  1693 | `			PH7_MemObjRelease(apObj[n]);` |
|    13632 |  1694 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6817 |  1695 | `		}` |
|     7448 |  1696 | `		SySetRelease(&pCtx->sVar);` |
|     3723 |  1697 | `	}` |
|   613908 |  1698 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1699 | `		ph7_aux_data *aAux;` |
|        - |  1700 | `		void *pChunk;` |
|        - |  1701 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1702 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1703 | `		 */` |
|        9 |  1704 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1705 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1706 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1707 | `			/* Release the chunk */` |
|       25 |  1708 | `			if( pChunk ){` |
|       25 |  1709 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1710 | `			}` |
|       13 |  1711 | `		}` |
|        9 |  1712 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1713 | `	}` |
|   613908 |  1714 |  |
|        - |  1715 | `/*` |
|        - |  1716 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1717 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1718 | ` */` |
|      296 |  1719 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1720 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1721 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1722 | `	)` |
|        2 |  1723 |  |
|      298 |  1724 | `	if( pValue == 0 ){` |
|        - |  1725 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1726 | `		return;` |
|        - |  1727 | `	}` |
|      298 |  1728 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1729 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1730 | `		sxu32 n;` |
|     1054 |  1731 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1732 | `			if( apObj[n] == pValue ){` |
|      298 |  1733 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1734 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1735 | `				/* Mark as released */` |
|      298 |  1736 | `				apObj[n] = 0;` |
|      298 |  1737 | `				break;` |
|        - |  1738 | `			}` |
|      380 |  1739 | `		}` |
|      148 |  1740 | `	}` |
|      150 |  1741 |  |
|        - |  1742 | `/*` |
|        - |  1743 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1744 | ` */` |
|  3529716 |  1745 | `static void VmPopOperand(` |
|        - |  1746 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1747 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1748 | `	)` |
|        2 |  1749 |  |
|  3529718 |  1750 | `	ph7_value *pTos = *ppTos;` |
|  7509190 |  1751 | `	while( nPop > 0 ){` |
|  3979474 |  1752 | `		PH7_MemObjRelease(pTos);` |
|  3979474 |  1753 | `		pTos--;` |
|  3979474 |  1754 | `		nPop--;` |
|        2 |  1755 | `	}` |
|        - |  1756 | `	/* Top of the stack */` |
|  3529718 |  1757 | `	*ppTos = pTos;` |
|  3529718 |  1758 |  |
|        - |  1759 | `/*` |
|        - |  1760 | ` * Reserve a memory object.` |
|        - |  1761 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1762 | ` */` |
|  3093356 |  1763 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1764 |  |
|  3093358 |  1765 | `	ph7_value *pObj = 0;` |
|        - |  1766 | `	VmSlot *pSlot;` |
|        - |  1767 | `	sxu32 nIdx;` |
|        - |  1768 | `	/* Check for a free slot */` |
|  3093358 |  1769 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3093358 |  1770 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3093358 |  1771 | `	if( pSlot ){` |
|   948288 |  1772 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   948288 |  1773 | `		nIdx = pSlot->nIdx;` |
|   474143 |  1774 | `	}` |
|  3093358 |  1775 | `	if( pObj == 0 ){` |
|        - |  1776 | `		/* Reserve a new memory object */` |
|  2145072 |  1777 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2145072 |  1778 | `		if( pObj == 0 ){` |
|      ! 0 |  1779 | `			return 0;` |
|        - |  1780 | `		}` |
|  1072535 |  1781 | `	}` |
|        - |  1782 | `	/* Set a null default value */` |
|  3093358 |  1783 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3093358 |  1784 | `	pObj->nIdx = nIdx;` |
|  3093358 |  1785 | `	return pObj;` |
|  1546680 |  1786 |  |
|        - |  1787 | `/*` |
|        - |  1788 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1789 | ` */` |
|    31968 |  1790 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1791 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1792 | `	const char *zKey,  /* Entry key */` |
|        - |  1793 | `	sxu32 nByte,       /* Key length */` |
|        - |  1794 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1795 | `	)` |
|        2 |  1796 |  |
|        - |  1797 | `	ph7_value sKey;` |
|        - |  1798 | `	sxi32 rc;` |
|    31970 |  1799 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    31970 |  1800 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1801 | `	/* Perform the insertion */` |
|    31970 |  1802 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    31970 |  1803 | `	PH7_MemObjRelease(&sKey);` |
|    31970 |  1804 | `	return rc;` |
|        2 |  1805 |  |
|        - |  1806 | `/*` |
|        - |  1807 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1808 | ` * Return a pointer to the variable value on success.` |
|        - |  1809 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1810 | ` */` |
|  3283878 |  1811 | `static ph7_value * VmExtractMemObj(` |
|        - |  1812 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1813 | `	const SyString *pName, /* Variable name */` |
|        - |  1814 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1815 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1816 | `	)` |
|        2 |  1817 |  |
|  3283880 |  1818 | `	int bNullify = FALSE;` |
|        - |  1819 | `	SyHashEntry *pEntry;` |
|        - |  1820 | `	VmFrame *pFrame;` |
|        - |  1821 | `	ph7_value *pObj;` |
|        - |  1822 | `	sxu32 nIdx;` |
|        - |  1823 | `	sxi32 rc;` |
|        - |  1824 | `	/* Point to the top active frame */` |
|  3283880 |  1825 | `	pFrame = pVm->pFrame;` |
|  3283880 |  1826 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1827 | `	/* Perform the lookup */` |
|  3283880 |  1828 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1829 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1830 | `		pName = &sAnnon;` |
|        - |  1831 | `		/* Always nullify the object */` |
|      ! 0 |  1832 | `		bNullify = TRUE;` |
|      ! 0 |  1833 | `		bDup = FALSE;` |
|      ! 0 |  1834 | `	}` |
|        - |  1835 | `	/* Check the superglobals table first */` |
|  3283880 |  1836 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3283880 |  1837 | `	if( pEntry == 0 ){` |
|        - |  1838 | `		/* Query the top active frame */` |
|  3283840 |  1839 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3283840 |  1840 | `		if( pEntry == 0 ){` |
|    92496 |  1841 | `			char *zName = (char *)pName->zString;` |
|        - |  1842 | `			VmSlot sLocal;` |
|    92496 |  1843 | `			if( !bCreate ){` |
|        - |  1844 | `				/* Do not create the variable,return NULL instead */` |
|      116 |  1845 | `				return 0;` |
|        - |  1846 | `			}` |
|        - |  1847 | `			/* No such variable,automatically create a new one and install` |
|        - |  1848 | `			 * it in the current frame.` |
|        - |  1849 | `			 */` |
|    92382 |  1850 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    92382 |  1851 | `			if( pObj == 0 ){` |
|      ! 0 |  1852 | `				return 0;` |
|        - |  1853 | `			}` |
|    92382 |  1854 | `			nIdx = pObj->nIdx;` |
|    92382 |  1855 | `			if( bDup ){` |
|        - |  1856 | `				/* Duplicate name */` |
|      168 |  1857 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1858 | `				if( zName == 0 ){` |
|      ! 0 |  1859 | `					return 0;` |
|        - |  1860 | `				}` |
|       83 |  1861 | `			}` |
|        - |  1862 | `			/* Link to the top active VM frame */` |
|    92382 |  1863 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    92382 |  1864 | `			if( rc != SXRET_OK ){` |
|        - |  1865 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1866 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1867 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1868 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1869 | `				return 0;` |
|        - |  1870 | `			}` |
|    92382 |  1871 | `			if( pFrame->pParent != 0 ){` |
|        - |  1872 | `				/* Local variable */` |
|    85182 |  1873 | `				sLocal.nIdx = nIdx;` |
|    85182 |  1874 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    42592 |  1875 | `			}else{` |
|        - |  1876 | `				/* Register in the $GLOBALS array */` |
|     7202 |  1877 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1878 | `			}` |
|        - |  1879 | `			/* Install in the reference table */` |
|    92382 |  1880 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1881 | `			/* Save object index */` |
|    92382 |  1882 | `			pObj->nIdx = nIdx;` |
|    46192 |  1883 | `		}else{` |
|        - |  1884 | `			/* Extract variable contents */` |
|  3191346 |  1885 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3191346 |  1886 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3191346 |  1887 | `			if( bNullify && pObj ){` |
|      ! 0 |  1888 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1889 | `			}` |
|        - |  1890 | `		}` |
|  1641974 |  1891 | `	}else{` |
|        - |  1892 | `		/* Superglobal */` |
|       42 |  1893 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1894 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1895 | `	}` |
|  3283766 |  1896 | `	return pObj;` |
|  1642051 |  1897 |  |
|        - |  1898 | `/*` |
|        - |  1899 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1900 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1901 | ` */` |
|     2778 |  1902 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1903 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1904 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1905 | `	sxu32 nByte        /* zName length */` |
|        - |  1906 | `	)` |
|        2 |  1907 |  |
|        - |  1908 | `	SyHashEntry *pEntry;` |
|        - |  1909 | `	ph7_value *pValue;` |
|        - |  1910 | `	sxu32 nIdx;` |
|        - |  1911 | `	/* Query the superglobal table */` |
|     2780 |  1912 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2780 |  1913 | `	if( pEntry == 0 ){` |
|        - |  1914 | `		/* No such entry */` |
|      ! 0 |  1915 | `		return 0;` |
|        - |  1916 | `	}` |
|        - |  1917 | `	/* Extract the superglobal index in the global object pool */` |
|     2780 |  1918 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1919 | `	/* Extract the variable value  */` |
|     2780 |  1920 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2780 |  1921 | `	return pValue;` |
|     1391 |  1922 |  |
|        - |  1923 | `/*` |
|        - |  1924 | ` * Perform a raw hashmap insertion.` |
|        - |  1925 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1926 | ` */` |
|     2808 |  1927 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1928 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1929 | `	const char *zKey,   /* Entry key */` |
|        - |  1930 | `	int nKeylen,        /* zKey length*/` |
|        - |  1931 | `	const char *zData,  /* Entry data */` |
|        - |  1932 | `	int nLen            /* zData length */` |
|        - |  1933 | `	)` |
|        2 |  1934 |  |
|        - |  1935 | `	ph7_value sKey,sValue;` |
|        - |  1936 | `	sxi32 rc;` |
|     2810 |  1937 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2810 |  1938 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2810 |  1939 | `	if( zKey ){` |
|     2788 |  1940 | `		if( nKeylen < 0 ){` |
|     2736 |  1941 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1367 |  1942 | `		}` |
|     2788 |  1943 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1393 |  1944 | `	}` |
|     2810 |  1945 | `	if( zData ){` |
|     2810 |  1946 | `		if( nLen < 0 ){` |
|        - |  1947 | `			/* Compute length automatically */` |
|      144 |  1948 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1949 | `		}` |
|     2810 |  1950 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1404 |  1951 | `	}` |
|        - |  1952 | `	/* Perform the insertion */` |
|     2810 |  1953 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2810 |  1954 | `	PH7_MemObjRelease(&sKey);` |
|     2810 |  1955 | `	PH7_MemObjRelease(&sValue);` |
|     2810 |  1956 | `	return rc;` |
|        2 |  1957 |  |
|        - |  1958 | `/*` |
|        - |  1959 | ` * Configure a working virtual machine instance.` |
|        - |  1960 | ` *` |
|        - |  1961 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1962 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1963 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1964 | ` * The second argument to this function is an integer configuration option` |
|        - |  1965 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1966 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1967 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1968 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1969 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1970 | ` */` |
|    39914 |  1971 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1972 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1973 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1974 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1975 | `	)` |
|        2 |  1976 |  |
|    39916 |  1977 | `	sxi32 rc = SXRET_OK;` |
|    39916 |  1978 | `	switch(nOp){` |
|     1229 |  1979 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2460 |  1980 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2460 |  1981 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1982 | `		/* VM output consumer callback */` |
|        - |  1983 | `#ifdef UNTRUST` |
|        - |  1984 | `		if( xConsumer == 0 ){` |
|        - |  1985 | `			rc = SXERR_CORRUPT;` |
|        - |  1986 | `			break;` |
|        - |  1987 | `		}` |
|        - |  1988 | `#endif` |
|        - |  1989 | `		/* Install the output consumer */` |
|     2460 |  1990 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2460 |  1991 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2460 |  1992 | `		break;` |
|        - |  1993 | `							   }` |
|     1237 |  1994 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1995 | `		/* Import path */` |
|        - |  1996 | `		  const char *zPath;` |
|        - |  1997 | `		  SyString sPath;` |
|     2476 |  1998 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1999 | `#if defined(UNTRUST)` |
|        - |  2000 | `		  if( zPath == 0 ){` |
|        - |  2001 | `			  rc = SXERR_EMPTY;` |
|        - |  2002 | `			  break;` |
|        - |  2003 | `		  }` |
|        - |  2004 | `#endif` |
|     2476 |  2005 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2006 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2007 | `#ifdef __WINNT__` |
|        2 |  2008 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2009 | `#endif` |
|     4950 |  2010 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2011 | `		  /* Remove leading and trailing white spaces */` |
|     2476 |  2012 | `		  SyStringFullTrim(&sPath);` |
|     2476 |  2013 | `		  if( sPath.nByte > 0 ){` |
|        - |  2014 | `			  /* Store the path in the corresponding conatiner */` |
|     2476 |  2015 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1237 |  2016 | `		  }` |
|     2476 |  2017 | `		  break;` |
|        - |  2018 | `									 }` |
|     1237 |  2019 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2020 | `		/* Run-Time Error report */` |
|     2476 |  2021 | `		pVm->bErrReport = 1;` |
|     2476 |  2022 | `		break;` |
|      ! 0 |  2023 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2024 | `		/* Recursion depth */` |
|      ! 0 |  2025 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2026 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2027 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2028 | `		}` |
|      ! 0 |  2029 | `		break;` |
|        - |  2030 | `									   }` |
|      ! 0 |  2031 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2032 | `		/* VM output length in bytes */` |
|      ! 0 |  2033 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2034 | `#ifdef UNTRUST` |
|        - |  2035 | `		if( pOut == 0 ){` |
|        - |  2036 | `			rc = SXERR_CORRUPT;` |
|        - |  2037 | `			break;` |
|        - |  2038 | `		}` |
|        - |  2039 | `#endif` |
|      ! 0 |  2040 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2041 | `		break;` |
|        - |  2042 | `							   }` |
|        - |  2043 |  |
|    12370 |  2044 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2045 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2046 | `		/* Create a new superglobal/global variable */` |
|    24742 |  2047 | `		const char *zName = va_arg(ap,const char *);` |
|    24742 |  2048 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2049 | `		SyHashEntry *pEntry;` |
|        - |  2050 | `		ph7_value *pObj;` |
|        - |  2051 | `		sxu32 nByte;` |
|        - |  2052 | `		sxu32 nIdx;` |
|        - |  2053 | `#ifdef UNTRUST` |
|        - |  2054 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2055 | `			rc = SXERR_CORRUPT;` |
|        - |  2056 | `			break;` |
|        - |  2057 | `		}` |
|        - |  2058 | `#endif` |
|    24742 |  2059 | `		nByte = SyStrlen(zName);` |
|    24742 |  2060 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2061 | `			/* Check if the superglobal is already installed */` |
|    24742 |  2062 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12372 |  2063 | `		}else{` |
|        - |  2064 | `			/* Query the top active VM frame */` |
|      ! 0 |  2065 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2066 | `		}` |
|    24742 |  2067 | `		if( pEntry ){` |
|        - |  2068 | `			/* Variable already installed */` |
|      ! 0 |  2069 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2070 | `			/* Extract contents */` |
|      ! 0 |  2071 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2072 | `			if( pObj ){` |
|        - |  2073 | `				/* Overwrite old contents */` |
|      ! 0 |  2074 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2075 | `			}` |
|      ! 0 |  2076 | `		}else{` |
|        - |  2077 | `			/* Install a new variable */` |
|    24742 |  2078 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    24742 |  2079 | `			if( pObj == 0 ){` |
|      ! 0 |  2080 | `				rc = SXERR_MEM;` |
|      ! 0 |  2081 | `				break;` |
|        - |  2082 | `			}` |
|    24742 |  2083 | `			nIdx = pObj->nIdx;` |
|        - |  2084 | `			/* Copy value */` |
|    24742 |  2085 | `			PH7_MemObjStore(pValue,pObj);` |
|    24742 |  2086 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2087 | `				/* Install the superglobal */` |
|    24742 |  2088 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12372 |  2089 | `			}else{` |
|        - |  2090 | `				/* Install in the current frame */` |
|      ! 0 |  2091 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2092 | `			}` |
|    24742 |  2093 | `			if( rc == SXRET_OK ){` |
|        - |  2094 | `				SyHashEntry *pRef;` |
|    24742 |  2095 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    24742 |  2096 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12372 |  2097 | `				}else{` |
|      ! 0 |  2098 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2099 | `				}` |
|        - |  2100 | `				/* Install in the reference table */` |
|    24742 |  2101 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    24742 |  2102 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2103 | `					/* Register in the $GLOBALS array */` |
|    24742 |  2104 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12370 |  2105 | `				}` |
|    12370 |  2106 | `			}` |
|        - |  2107 | `		}` |
|    24742 |  2108 | `		break;` |
|        - |  2109 | `									}` |
|     1367 |  2110 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2111 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2112 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2113 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2114 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2115 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2116 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2736 |  2117 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2736 |  2118 | `		const char *zValue = va_arg(ap,const char *);` |
|     2736 |  2119 | `		int nLen = va_arg(ap,int);` |
|        - |  2120 | `		ph7_hashmap *pMap;` |
|        - |  2121 | `		ph7_value *pValue;` |
|     2736 |  2122 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2123 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2124 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2735 |  2125 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2126 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2127 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2734 |  2128 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2129 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2130 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2734 |  2131 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2132 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2133 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2734 |  2134 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2135 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2136 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2734 |  2137 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2138 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2139 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2140 | `		}else{` |
|        - |  2141 | `			/* Extract the $_SERVER superglobal */` |
|     2734 |  2142 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2143 | `		}` |
|     2736 |  2144 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2145 | `			/* No such entry */` |
|      ! 0 |  2146 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2147 | `			break;` |
|        - |  2148 | `		}` |
|        - |  2149 | `		/* Point to the hashmap */` |
|     2736 |  2150 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2151 | `		/* Perform the insertion */` |
|     2736 |  2152 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2736 |  2153 | `		break;` |
|        - |  2154 | `								   }` |
|       11 |  2155 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2156 | `		/* Script arguments */` |
|       24 |  2157 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2158 | `		ph7_hashmap *pMap;` |
|        - |  2159 | `		ph7_value *pValue;` |
|        - |  2160 | `		sxu32 n;` |
|       24 |  2161 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2162 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2163 | `			break;` |
|        - |  2164 | `		}` |
|        - |  2165 | `		/* Extract the $argv array */` |
|       24 |  2166 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2167 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2168 | `			/* No such entry */` |
|      ! 0 |  2169 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2170 | `			break;` |
|        - |  2171 | `		}` |
|        - |  2172 | `		/* Point to the hashmap */` |
|       24 |  2173 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2174 | `		/* Perform the insertion */` |
|       24 |  2175 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2176 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2177 | `		if( rc == SXRET_OK ){` |
|       24 |  2178 | `			if( pMap->nEntry > 1 ){` |
|        - |  2179 | `				/* Append space separator first */` |
|       18 |  2180 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2181 | `			}` |
|       24 |  2182 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2183 | `		}` |
|       24 |  2184 | `		break;` |
|        - |  2185 | `								  }` |
|      ! 0 |  2186 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2187 | `		/* error_log() consumer */` |
|      ! 0 |  2188 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2189 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2190 | `		break;` |
|        - |  2191 | `										}` |
|      ! 0 |  2192 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2193 | `		/* Script return value */` |
|      ! 0 |  2194 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2195 | `#ifdef UNTRUST` |
|        - |  2196 | `		if( ppValue == 0 ){` |
|        - |  2197 | `			rc = SXERR_CORRUPT;` |
|        - |  2198 | `			break;` |
|        - |  2199 | `		}` |
|        - |  2200 | `#endif` |
|      ! 0 |  2201 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2202 | `		break;` |
|        - |  2203 | `								   }` |
|     2474 |  2204 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2205 | `		/* Register an IO stream device */` |
|     4950 |  2206 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2207 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7422 |  2208 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4950 |  2209 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2210 | `				/* Invalid stream */` |
|      ! 0 |  2211 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2212 | `				break;` |
|        - |  2213 | `		}` |
|     4950 |  2214 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2215 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2476 |  2216 | `			pVm->pDefStream = pStream;` |
|     1237 |  2217 | `		}` |
|        - |  2218 | `		/* Insert in the appropriate container */` |
|     4950 |  2219 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4950 |  2220 | `		break;` |
|        - |  2221 | `								  }` |
|        8 |  2222 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2223 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2224 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2225 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2226 | `#ifdef UNTRUST` |
|        - |  2227 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2228 | `			rc = SXERR_CORRUPT;` |
|        - |  2229 | `			break;` |
|        - |  2230 | `		}` |
|        - |  2231 | `#endif` |
|       16 |  2232 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2233 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2234 | `		break;` |
|        - |  2235 | `									   }` |
|        8 |  2236 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2237 | `		/* Raw HTTP request*/` |
|       16 |  2238 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2239 | `		int nByte = va_arg(ap,int);` |
|       16 |  2240 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2241 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2242 | `			break;` |
|        - |  2243 | `		}` |
|       16 |  2244 | `		if( nByte < 0 ){` |
|        - |  2245 | `			/* Compute length automatically */` |
|      ! 0 |  2246 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2247 | `		}` |
|        - |  2248 | `		/* Process the request */` |
|       16 |  2249 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2250 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2251 | `		if( rc == SXRET_OK ){` |
|       16 |  2252 | `			pVm->bHttpContext = 1;` |
|        8 |  2253 | `		}` |
|       16 |  2254 | `		break;` |
|        - |  2255 | `									}` |
|        8 |  2256 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2257 | `		/* Extract HTTP response status code */` |
|       16 |  2258 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2259 | `		if( pStatus ){` |
|       16 |  2260 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2261 | `		}` |
|       16 |  2262 | `		break;` |
|        - |  2263 | `										}` |
|        8 |  2264 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2265 | `		/* Iterate response headers via callback */` |
|        - |  2266 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2267 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2268 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2269 | `		if( xCallback ){` |
|       16 |  2270 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2271 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2272 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2273 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2274 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2275 | `							   pUserData);` |
|       12 |  2276 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2277 | `					break;` |
|        - |  2278 | `				}` |
|        6 |  2279 | `			}` |
|        8 |  2280 | `		}` |
|       16 |  2281 | `		break;` |
|        - |  2282 | `										 }` |
|      ! 0 |  2283 | `	default:` |
|        - |  2284 | `		/* Unknown configuration option */` |
|      ! 0 |  2285 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2286 | `		break;` |
|        - |  2287 | `	}` |
|    39916 |  2288 | `	return rc;` |
|        2 |  2289 |  |
|        - |  2290 | `/* Forward declaration */` |
|        - |  2291 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2292 | `/*` |
|        - |  2293 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2294 | ` * format.` |
|        - |  2295 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2296 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2297 | ` * (STDOUT).` |
|        - |  2298 | ` */` |
|        2 |  2299 | `static sxi32 VmByteCodeDump(` |
|        - |  2300 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2301 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2302 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2303 | `	)` |
|        1 |  2304 |  |
|        - |  2305 | `	static const char zDump[] = {` |
|        - |  2306 | `		"====================================================\n"` |
|        - |  2307 | `		"PH7 VM Dump\n"` |
|        - |  2308 | `		"====================================================\n"` |
|        - |  2309 | `	};` |
|        - |  2310 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2311 | `	sxi32 rc = SXRET_OK;` |
|        - |  2312 | `	sxu32 n;` |
|        - |  2313 | `	/* Point to the PH7 instructions */` |
|        3 |  2314 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2315 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2316 | `	n = 0;` |
|        3 |  2317 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2318 | `	/* Dump instructions */` |
|        7 |  2319 | `	for(;;){` |
|       15 |  2320 | `		if( pInstr >= pEnd ){` |
|        - |  2321 | `			/* No more instructions */` |
|        3 |  2322 | `			break;` |
|        - |  2323 | `		}` |
|        - |  2324 | `		/* Format and call the consumer callback */` |
|       19 |  2325 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2326 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2327 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2328 | `		if( rc != SXRET_OK ){` |
|        - |  2329 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2330 | `			return rc;` |
|        - |  2331 | `		}` |
|       13 |  2332 | `		++n;` |
|       13 |  2333 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2334 | `	}` |
|        3 |  2335 | `	return rc;` |
|        2 |  2336 |  |
|        - |  2337 | `/* Forward declaration */` |
|        - |  2338 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2339 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2340 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2341 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2342 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2343 | `/*` |
|        - |  2344 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2345 | ` * consumer callback.` |
|        - |  2346 | ` */` |
|      558 |  2347 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2348 |  |
|      559 |  2349 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      559 |  2350 | `	sxi32 rc = SXRET_OK;` |
|        - |  2351 | `	/* Append a new line */` |
|        - |  2352 | `#ifdef __WINNT__` |
|        1 |  2353 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2354 | `#else` |
|      558 |  2355 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2356 | `#endif` |
|        - |  2357 | `	/* Invoke the output consumer callback */` |
|      559 |  2358 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      559 |  2359 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      559 |  2360 | `	return rc;` |
|        1 |  2361 |  |
|        - |  2362 | `/*` |
|        - |  2363 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2364 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2365 | ` * information.` |
|        - |  2366 | ` */` |
|      134 |  2367 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2368 |  |
|      136 |  2369 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2370 | `		ph7_value apArg[4];` |
|        - |  2371 | `		ph7_value *apArgPtr[4];` |
|        - |  2372 | `		ph7_value sResult;` |
|        - |  2373 | `		SyString sErr;` |
|        - |  2374 | `		/* Prepare arguments */` |
|       61 |  2375 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2376 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2377 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2378 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2379 | `		if( pFile ){` |
|       61 |  2380 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2381 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2382 | `		}else{` |
|      ! 0 |  2383 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2384 | `		}` |
|       61 |  2385 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2386 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2387 | `		/* Set up pointer array */` |
|       61 |  2388 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2389 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2390 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2391 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2392 | `		/* Call the handler */` |
|       61 |  2393 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2394 | `		/* Check return value */` |
|       61 |  2395 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2396 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2397 | `		}` |
|        - |  2398 | `		/* Release */` |
|       61 |  2399 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2400 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2401 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2402 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2403 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2404 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2405 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2406 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2407 | `	}` |
|        - |  2408 | `	/* No handler, always call error handler */` |
|       75 |  2409 | `	return TRUE;` |
|       69 |  2410 |  |
|       98 |  2411 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2412 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2413 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2414 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2415 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2416 | `	)` |
|        2 |  2417 |  |
|      100 |  2418 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2419 | `	SyString *pFile;` |
|        - |  2420 | `	char *zErr;` |
|      100 |  2421 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2422 | `	if( !pVm->bErrReport ){` |
|        - |  2423 | `		/* Don't bother reporting errors */` |
|        3 |  2424 | `		return SXRET_OK;` |
|        - |  2425 | `	}` |
|        - |  2426 | `	/* Reset the working buffer */` |
|       98 |  2427 | `	SyBlobReset(pWorker);` |
|        - |  2428 | `	/* Peek the processed file if available */` |
|       98 |  2429 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2430 | `	if( pFile ){` |
|        - |  2431 | `		/* Append file name */` |
|       98 |  2432 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2433 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2434 | `	}` |
|        - |  2435 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2436 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2437 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2438 | `	 * E_DEPRECATED). */` |
|       98 |  2439 | `	zErr = "Error:  ";` |
|       98 |  2440 | `	switch(iErr){` |
|       19 |  2441 | `	case PH7_CTX_WARNING:` |
|       40 |  2442 | `		zErr = "Warning:  ";` |
|       40 |  2443 | `		break;` |
|        6 |  2444 | `	case PH7_CTX_NOTICE:` |
|       14 |  2445 | `		zErr = "Notice:  ";` |
|       12 |  2446 | `		break;` |
|       23 |  2447 | `	default:` |
|        - |  2448 | `		/* keep iErr unchanged */` |
|       46 |  2449 | `		break;` |
|        - |  2450 | `	}` |
|       98 |  2451 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2452 | `	if( pFuncName ){` |
|        - |  2453 | `		/* Append function name first */` |
|       23 |  2454 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2455 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2456 | `	}` |
|       98 |  2457 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2458 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2459 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2460 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2461 | `	}` |
|       98 |  2462 | `	return rc;` |
|       51 |  2463 |  |
|        - |  2464 | `/*` |
|        - |  2465 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2466 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2467 | ` * information.` |
|        - |  2468 | ` */` |
|       38 |  2469 | `static sxi32 VmThrowErrorAp(` |
|        - |  2470 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2471 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2472 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2473 | `	const char *zFormat, /* Format message */` |
|        - |  2474 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2475 | `	)` |
|        2 |  2476 |  |
|       40 |  2477 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2478 | `	SyBlob sMsg;` |
|        - |  2479 | `	SyString *pFile;` |
|        - |  2480 | `	char *zErr;` |
|       40 |  2481 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2482 | `	if( !pVm->bErrReport ){` |
|        - |  2483 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2484 | `		return SXRET_OK;` |
|        - |  2485 | `	}` |
|        - |  2486 | `	/* Reset the working buffer */` |
|       40 |  2487 | `	SyBlobReset(pWorker);` |
|        - |  2488 | `	/* Peek the processed file if available */` |
|       40 |  2489 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2490 | `	if( pFile ){` |
|        - |  2491 | `		/* Append file name */` |
|       40 |  2492 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2493 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2494 | `	}` |
|        - |  2495 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2496 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2497 | `	 * the correct errno value. */` |
|       40 |  2498 | `	zErr = "Error:  ";` |
|       40 |  2499 | `	switch(iErr){` |
|        4 |  2500 | `	case PH7_CTX_WARNING:` |
|        9 |  2501 | `		zErr = "Warning:  ";` |
|        9 |  2502 | `		break;` |
|        3 |  2503 | `	case PH7_CTX_NOTICE:` |
|        7 |  2504 | `		zErr = "Notice:  ";` |
|        6 |  2505 | `		break;` |
|       12 |  2506 | `	default:` |
|        - |  2507 | `		/* do not change iErr */` |
|       24 |  2508 | `		break;` |
|        - |  2509 | `	}` |
|       40 |  2510 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2511 | `	if( pFuncName ){` |
|        - |  2512 | `		/* Append function name first */` |
|       26 |  2513 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2514 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2515 | `	}` |
|        - |  2516 | `	/* Format the raw message */` |
|       40 |  2517 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2518 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2519 | `	/* Check if a user error handler is installed */` |
|       40 |  2520 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2521 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2522 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2523 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2524 | `	}` |
|       40 |  2525 | `	SyBlobRelease(&sMsg);` |
|       40 |  2526 | `	return rc;` |
|       21 |  2527 |  |
|        - |  2528 | `/*` |
|        - |  2529 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2530 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2531 | ` * possible.` |
|        - |  2532 | ` */` |
|       30 |  2533 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2534 |  |
|        - |  2535 | `	ph7_class *pClass;` |
|       31 |  2536 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2537 | `	ph7_class_instance *pThis;` |
|        - |  2538 | `	ph7_class_method *pCons;` |
|        - |  2539 | `	ph7_value sArg;` |
|        - |  2540 | `	ph7_value *apArg[1];` |
|        - |  2541 | `	SyBlob sMsg;` |
|        - |  2542 | `	SyString sMsgStr;` |
|        - |  2543 | `	VmFrame *pFrame;` |
|        - |  2544 | `	sxi32 rc;` |
|       31 |  2545 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       31 |  2546 | `	if( pClass == 0 ){` |
|      ! 0 |  2547 | `		return PH7_ABORT;` |
|        - |  2548 | `	}` |
|       31 |  2549 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       31 |  2550 | `	if( pThis == 0 ){` |
|      ! 0 |  2551 | `		return PH7_ABORT;` |
|        - |  2552 | `	}` |
|       31 |  2553 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2554 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2555 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2556 | `	{` |
|       31 |  2557 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       31 |  2558 | `		if( pOwner ){` |
|       31 |  2559 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       15 |  2560 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       16 |  2561 | `		}else{` |
|      ! 0 |  2562 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2563 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2564 | `		}` |
|        - |  2565 | `	}` |
|       31 |  2566 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       31 |  2567 | `	if( pCons ){` |
|       31 |  2568 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  2569 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       31 |  2570 | `		apArg[0] = &sArg;` |
|       31 |  2571 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       31 |  2572 | `		PH7_MemObjRelease(&sArg);` |
|       15 |  2573 | `	}` |
|       31 |  2574 | `	SyBlobRelease(&sMsg);` |
|       31 |  2575 | `	pFrame = pVm->pFrame;` |
|       31 |  2576 | `	if( pFrame ){` |
|       31 |  2577 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       31 |  2578 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       15 |  2579 | `	}` |
|       31 |  2580 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       31 |  2581 | `	PH7_ClassInstanceUnref(pThis);` |
|       31 |  2582 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2583 | `		return PH7_ABORT;` |
|        - |  2584 | `	}` |
|       31 |  2585 | `	return PH7_EXCEPTION;` |
|       16 |  2586 |  |
|        - |  2587 |  |
|        - |  2588 | `/*` |
|        - |  2589 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2590 | ` */` |
|        4 |  2591 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2592 |  |
|        - |  2593 | `	ph7_class *pErrClass;` |
|        - |  2594 | `	ph7_class_instance *pThis;` |
|        - |  2595 | `	ph7_class_method *pCons;` |
|        - |  2596 | `	ph7_value sArg;` |
|        - |  2597 | `	ph7_value *apArg[1];` |
|        - |  2598 | `	SyBlob sMsg;` |
|        - |  2599 | `	SyString sMsgStr;` |
|        - |  2600 | `	VmFrame *pFrame;` |
|        - |  2601 | `	sxi32 rc;` |
|        5 |  2602 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2603 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2604 | `		return PH7_ABORT;` |
|        - |  2605 | `	}` |
|        5 |  2606 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2607 | `	if( pThis == 0 ){` |
|      ! 0 |  2608 | `		return PH7_ABORT;` |
|        - |  2609 | `	}` |
|        5 |  2610 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2611 | `	{` |
|        5 |  2612 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2613 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2614 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2615 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2616 | `	}` |
|        5 |  2617 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2618 | `	if( pCons ){` |
|        5 |  2619 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2620 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2621 | `		apArg[0] = &sArg;` |
|        5 |  2622 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2623 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2624 | `	}` |
|        5 |  2625 | `	SyBlobRelease(&sMsg);` |
|        5 |  2626 | `	pFrame = pVm->pFrame;` |
|        5 |  2627 | `	if( pFrame ){` |
|        5 |  2628 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2629 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2630 | `	}` |
|        5 |  2631 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2632 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2633 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2634 | `		return PH7_ABORT;` |
|        - |  2635 | `	}` |
|        5 |  2636 | `	return PH7_EXCEPTION;` |
|        3 |  2637 |  |
|        - |  2638 |  |
|        - |  2639 | `/*` |
|        - |  2640 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2641 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2642 | ` * For class types, instanceof is verified.` |
|        - |  2643 | ` *` |
|        - |  2644 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2645 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2646 | ` */` |
|        - |  2647 | `/*` |
|        - |  2648 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2649 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2650 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2651 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2652 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2653 | ` */` |
|       16 |  2654 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2655 |  |
|        - |  2656 | `	const char *z, *zEnd, *zTail;` |
|        - |  2657 | `	sxu32 n;` |
|        - |  2658 | `	sxu8 bReal;` |
|        - |  2659 | `	sxi32 rc;` |
|       18 |  2660 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2661 | `		return 0;` |
|        - |  2662 | `	}` |
|       18 |  2663 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2664 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2665 | `	zEnd = z + n;` |
|       18 |  2666 | `	if( n == 0 ){` |
|      ! 0 |  2667 | `		return 0;` |
|        - |  2668 | `	}` |
|       18 |  2669 | `	zTail = 0;` |
|       18 |  2670 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2671 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        5 |  2672 | `		return 0;` |
|        - |  2673 | `	}` |
|        - |  2674 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       14 |  2675 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2676 | `		zTail++;` |
|      ! 0 |  2677 | `	}` |
|       14 |  2678 | `	return zTail == zEnd ? 1 : 0;` |
|       10 |  2679 |  |
|        - |  2680 |  |
|        - |  2681 | `/*` |
|        - |  2682 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2683 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2684 | ` */` |
|        8 |  2685 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2686 |  |
|        9 |  2687 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       13 |  2688 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|        8 |  2689 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|        9 |  2690 | `	return zBuf;` |
|        1 |  2691 |  |
|        - |  2692 |  |
|    11730 |  2693 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2694 |  |
|        - |  2695 | `	SyHashEntry *pSlot;` |
|        - |  2696 | `	VmClassAttr *pVmAttr;` |
|        - |  2697 | `	ph7_class_attr *pAttr;` |
|    11732 |  2698 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    11732 |  2699 | `	if( pSlot == 0 ){` |
|    11604 |  2700 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2701 | `	}` |
|      130 |  2702 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      130 |  2703 | `	pAttr = pVmAttr->pAttr;` |
|      130 |  2704 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2705 | `		return SXRET_OK;` |
|        - |  2706 | `	}` |
|        - |  2707 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      130 |  2708 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|        8 |  2709 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        6 |  2710 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        6 |  2711 | `			return SXRET_OK;` |
|        - |  2712 | `		}` |
|        3 |  2713 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  2714 | `	}` |
|        - |  2715 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  2716 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  2717 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      124 |  2718 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  2719 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  2720 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  2721 | `			return SXRET_OK;` |
|        - |  2722 | `		}` |
|        7 |  2723 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2724 | `	}` |
|      114 |  2725 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  2726 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  2727 | `		 * currently active on the self-stack. */` |
|       20 |  2728 | `		ph7_class *pExpected = 0;` |
|       20 |  2729 | `		SyString *pClassName = &pAttr->sClass;` |
|       20 |  2730 | `		ph7_class *pSelfNow = 0;` |
|       20 |  2731 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2732 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2733 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2734 | `		}` |
|       20 |  2735 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  2736 | `			pExpected = pSelfNow;` |
|       18 |  2737 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  2738 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2739 | `		}else{` |
|       16 |  2740 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  2741 | `		}` |
|       20 |  2742 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  2743 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2744 | `		}` |
|       20 |  2745 | `		if( pExpected ){` |
|       16 |  2746 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       16 |  2747 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  2748 | `				char zBuf[128];` |
|        7 |  2749 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2750 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2751 | `			}` |
|        5 |  2752 | `		}` |
|       16 |  2753 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       16 |  2754 | `		return SXRET_OK;` |
|        - |  2755 | `	}` |
|        - |  2756 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  2757 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|       96 |  2758 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2759 | `		char zBuf[128];` |
|        7 |  2760 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2761 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2762 | `	}` |
|       92 |  2763 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  2764 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  2765 | `		if( xCast ){` |
|        - |  2766 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  2767 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  2768 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2769 | `			}` |
|       24 |  2770 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  2771 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2772 | `			}` |
|        - |  2773 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  2774 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  2775 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  2776 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  2777 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  2778 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  2779 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  2780 | `			}` |
|       12 |  2781 | `			xCast(pValue);` |
|        5 |  2782 | `		}` |
|        5 |  2783 | `	}` |
|       78 |  2784 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       78 |  2785 | `	return SXRET_OK;` |
|     5867 |  2786 |  |
|        - |  2787 |  |
|        - |  2788 | `/*` |
|        - |  2789 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2790 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2791 | ` * information.` |
|        - |  2792 | ` * ------------------------------------` |
|        - |  2793 | ` * Simple boring wrapper function.` |
|        - |  2794 | ` * ------------------------------------` |
|        - |  2795 | ` */` |
|       14 |  2796 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2797 |  |
|        - |  2798 | `	va_list ap;` |
|        - |  2799 | `	sxi32 rc;` |
|       15 |  2800 | `	va_start(ap,zFormat);` |
|       15 |  2801 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2802 | `	va_end(ap);` |
|       15 |  2803 | `	return rc;` |
|        1 |  2804 |  |
|        - |  2805 | `/*` |
|        - |  2806 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2807 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2808 | ` */` |
|       10 |  2809 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2810 |  |
|        - |  2811 | `	ph7_class *pClass;` |
|        - |  2812 | `	ph7_class_instance *pThis;` |
|        - |  2813 | `	ph7_class_method *pCons;` |
|        - |  2814 | `	ph7_value sArg;` |
|        - |  2815 | `	ph7_value *apArg[1];` |
|        - |  2816 | `	SyBlob sMsg;` |
|        - |  2817 | `	SyString sMsgStr;` |
|        - |  2818 | `	VmFrame *pFrame;` |
|        - |  2819 | `	sxi32 rc;` |
|       11 |  2820 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       11 |  2821 | `	if( pClass == 0 ){` |
|      ! 0 |  2822 | `		return PH7_ABORT;` |
|        - |  2823 | `	}` |
|       11 |  2824 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       11 |  2825 | `	if( pThis == 0 ){` |
|      ! 0 |  2826 | `		return PH7_ABORT;` |
|        - |  2827 | `	}` |
|       11 |  2828 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 |  2829 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|        5 |  2830 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       11 |  2831 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       11 |  2832 | `	if( pCons ){` |
|       11 |  2833 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       11 |  2834 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       11 |  2835 | `		apArg[0] = &sArg;` |
|       11 |  2836 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       11 |  2837 | `		PH7_MemObjRelease(&sArg);` |
|        5 |  2838 | `	}` |
|       11 |  2839 | `	SyBlobRelease(&sMsg);` |
|       11 |  2840 | `	pFrame = pVm->pFrame;` |
|       11 |  2841 | `	if( pFrame ){` |
|       11 |  2842 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       11 |  2843 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        5 |  2844 | `	}` |
|       11 |  2845 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       11 |  2846 | `	PH7_ClassInstanceUnref(pThis);` |
|       11 |  2847 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2848 | `		return PH7_ABORT;` |
|        - |  2849 | `	}` |
|       11 |  2850 | `	return PH7_EXCEPTION;` |
|        6 |  2851 |  |
|        - |  2852 | `/*` |
|        - |  2853 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2854 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2855 | ` * information.` |
|        - |  2856 | ` * ------------------------------------` |
|        - |  2857 | ` * Simple boring wrapper function.` |
|        - |  2858 | ` * ------------------------------------` |
|        - |  2859 | ` */` |
|       24 |  2860 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2861 |  |
|        - |  2862 | `	sxi32 rc;` |
|       26 |  2863 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2864 | `	return rc;` |
|        2 |  2865 |  |
|        - |  2866 | `/*` |
|        - |  2867 | ` * Resolve function context from the current frame.` |
|        - |  2868 | ` */` |
|      954 |  2869 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2870 |  |
|        - |  2871 | `	VmFrame *pFrame;` |
|        - |  2872 | `	ph7_vm_func *pFunc;` |
|      955 |  2873 | `	*pzFuncName = 0;` |
|      955 |  2874 | `	*pnFuncLen = 0;` |
|      955 |  2875 | `	pFrame = pVm->pFrame;` |
|      955 |  2876 | `	if( pFrame == 0 ){` |
|      ! 0 |  2877 | `		return;` |
|        - |  2878 | `	}` |
|      955 |  2879 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      955 |  2880 | `	if( pFrame->pParent == 0 ){` |
|      947 |  2881 | `		return;` |
|        - |  2882 | `	}` |
|        9 |  2883 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        9 |  2884 | `	if( pFunc == 0 ){` |
|      ! 0 |  2885 | `		return;` |
|        - |  2886 | `	}` |
|        9 |  2887 | `	*pzFuncName = pFunc->sName.zString;` |
|        9 |  2888 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      478 |  2889 |  |
|        - |  2890 | `/*` |
|        - |  2891 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2892 | ` */` |
|      482 |  2893 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2894 |  |
|        - |  2895 | `	SyBlob sOut;` |
|        - |  2896 | `	SyString *pFile;` |
|      483 |  2897 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2898 | `		return PH7_OK;` |
|        - |  2899 | `	}` |
|      483 |  2900 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2901 | `		zClass = "Exception";` |
|      ! 0 |  2902 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2903 | `	}` |
|      483 |  2904 | `	if( zMsg == 0 ){` |
|      ! 0 |  2905 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2906 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2907 | `	}` |
|      483 |  2908 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  2909 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  2910 | `	}` |
|      483 |  2911 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      483 |  2912 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      483 |  2913 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      483 |  2914 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      483 |  2915 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      483 |  2916 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      483 |  2917 | `	if( pFile ){` |
|      483 |  2918 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      483 |  2919 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2920 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      241 |  2921 | `	}` |
|      483 |  2922 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      483 |  2923 | `	if( pFile ){` |
|      483 |  2924 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      483 |  2925 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2926 | `		if( zFuncName && nFuncLen > 0 ){` |
|        9 |  2927 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        5 |  2928 | `		}else{` |
|      475 |  2929 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2930 | `		}` |
|      241 |  2931 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2932 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2933 | `	}else{` |
|      ! 0 |  2934 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2935 | `	}` |
|      483 |  2936 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      483 |  2937 | `	if( pFile ){` |
|      483 |  2938 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      483 |  2939 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      483 |  2940 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2941 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      241 |  2942 | `	}` |
|      483 |  2943 | `	VmCallErrorHandler(pVm,&sOut);` |
|      483 |  2944 | `	SyBlobRelease(&sOut);` |
|      483 |  2945 | `	return PH7_ABORT;` |
|      242 |  2946 |  |
|        - |  2947 | `/*` |
|        - |  2948 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2949 | ` */` |
|      480 |  2950 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2951 |  |
|        - |  2952 | `	ph7_vm *pVm;` |
|        - |  2953 | `	ph7_class *pClass;` |
|        - |  2954 | `	ph7_class_instance *pThis;` |
|        - |  2955 | `	ph7_class_method *pCons;` |
|        - |  2956 | `	ph7_value sArg;` |
|        - |  2957 | `	ph7_value *apArg[1];` |
|        - |  2958 | `	SyBlob sMsg;` |
|        - |  2959 | `	SyString sMsgStr;` |
|        - |  2960 | `	VmFrame *pFrame;` |
|        - |  2961 | `	va_list ap;` |
|        - |  2962 | `	sxi32 rc;` |
|        - |  2963 |  |
|      482 |  2964 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2965 | `		return PH7_ABORT;` |
|        - |  2966 | `	}` |
|      482 |  2967 | `	pVm = pCtx->pVm;` |
|      482 |  2968 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2969 | `		zClass = "Error";` |
|      ! 0 |  2970 | `	}` |
|      482 |  2971 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  2972 | `	if( pClass == 0 ){` |
|      ! 0 |  2973 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2974 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2975 | `			zClass` |
|        - |  2976 | `			);` |
|        - |  2977 | `	}` |
|      482 |  2978 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  2979 | `	if( pThis == 0 ){` |
|      ! 0 |  2980 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2981 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2982 | `			);` |
|        - |  2983 | `	}` |
|        - |  2984 |  |
|      482 |  2985 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  2986 | `	va_start(ap,zFormat);` |
|      482 |  2987 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  2988 | `	va_end(ap);` |
|        - |  2989 |  |
|      482 |  2990 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  2991 | `	if( pCons ){` |
|      482 |  2992 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  2993 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  2994 | `		apArg[0] = &sArg;` |
|      482 |  2995 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  2996 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  2997 | `	}` |
|      482 |  2998 | `	SyBlobRelease(&sMsg);` |
|        - |  2999 |  |
|      482 |  3000 | `	pFrame = pVm->pFrame;` |
|      482 |  3001 | `	if( pFrame ){` |
|      482 |  3002 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  3003 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  3004 | `	}` |
|      482 |  3005 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  3006 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  3007 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3008 | `		return PH7_ABORT;` |
|        - |  3009 | `	}` |
|       12 |  3010 | `	return PH7_EXCEPTION;` |
|      242 |  3011 |  |
|        - |  3012 | `/*` |
|        - |  3013 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3014 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3015 | ` */` |
|      ! 0 |  3016 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3017 |  |
|        - |  3018 | `	ph7_vm *pVm;` |
|        - |  3019 | `	SyBlob sMsg;` |
|      ! 0 |  3020 | `	const char *zFuncName = 0;` |
|      ! 0 |  3021 | `	int nFuncLen = 0;` |
|        - |  3022 | `	va_list ap;` |
|        - |  3023 | `	sxi32 rc;` |
|        - |  3024 |  |
|      ! 0 |  3025 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3026 | `		return PH7_OK;` |
|        - |  3027 | `	}` |
|      ! 0 |  3028 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3029 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3030 | `		zClass = "Error";` |
|      ! 0 |  3031 | `	}` |
|        - |  3032 |  |
|      ! 0 |  3033 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3034 |  |
|      ! 0 |  3035 | `	va_start(ap,zFormat);` |
|      ! 0 |  3036 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3037 | `	va_end(ap);` |
|        - |  3038 |  |
|      ! 0 |  3039 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3040 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3041 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3042 | `	}` |
|      ! 0 |  3043 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3044 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3045 | `	}` |
|      ! 0 |  3046 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3047 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3048 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3049 | `	return rc;` |
|      ! 0 |  3050 |  |
|        - |  3051 | `/*` |
|        - |  3052 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3053 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3054 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3055 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3056 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3057 | ` * when VmByteCodeExec returns.` |
|        - |  3058 | ` */` |
|      132 |  3059 | `static sxi32 VmSuspendCtx(` |
|        - |  3060 | `	ph7_vm *pVm,` |
|        - |  3061 | `	ph7_exec_ctx *pCtx,` |
|        - |  3062 | `	sxi32 pc,` |
|        - |  3063 | `	sxi32 nTos` |
|        - |  3064 | `	)` |
|        2 |  3065 |  |
|       66 |  3066 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  3067 | `	pCtx->pc = pc;` |
|      134 |  3068 | `	pCtx->nTos = nTos;` |
|      134 |  3069 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  3070 | `	return PH7_SUSPEND;` |
|        2 |  3071 |  |
|        - |  3072 | `/*` |
|        - |  3073 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3074 | ` *` |
|        - |  3075 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3076 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3077 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3078 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3079 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3080 | ` * then the program execution is halted.` |
|        - |  3081 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3082 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3083 | ` * or to reset the VM to it's initial state.` |
|        - |  3084 | ` */` |
|    35598 |  3085 | `static sxi32 VmByteCodeExec(` |
|        - |  3086 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3087 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3088 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3089 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3090 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3091 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3092 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3093 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3094 | `	)` |
|        2 |  3095 |  |
|        - |  3096 | `	VmInstr *pInstr;` |
|        - |  3097 | `	ph7_value *pTos;` |
|        - |  3098 | `	SySet aArg;` |
|        - |  3099 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3100 | `	sxi32 pc;` |
|        - |  3101 | `	sxi32 rc;` |
|        - |  3102 | `	/* Argument container */` |
|    35600 |  3103 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    35600 |  3104 | `	if( nTos < 0 ){` |
|    33452 |  3105 | `		pTos = &pStack[-1];` |
|    16727 |  3106 | `	}else{` |
|     2150 |  3107 | `		pTos = &pStack[nTos];` |
|        - |  3108 | `	}` |
|    35600 |  3109 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    35600 |  3110 | `	pc = nPc;` |
|        - |  3111 | `/*` |
|        - |  3112 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3113 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3114 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3115 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3116 | ` */` |
|        - |  3117 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3118 | `	{ \` |
|        - |  3119 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3120 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3121 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3122 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3123 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3124 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3125 | `				break; \` |
|        - |  3126 | `			} \` |
|        - |  3127 | `			goto Exception; \` |
|        - |  3128 | `		} \` |
|        - |  3129 | `	}` |
|        - |  3130 | `	/* Execute as much as we can */` |
|  5281647 |  3131 | `	for(;;){` |
|        - |  3132 | `		/* Fetch the instruction to execute */` |
| 10562592 |  3133 | `		pInstr = &aInstr[pc];` |
| 10562592 |  3134 | `		rc = SXRET_OK;` |
|        - |  3135 | `/*` |
|        - |  3136 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3137 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3138 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3139 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3140 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3141 | ` */` |
| 10562592 |  3142 | `		switch(pInstr->iOp){` |
|        - |  3143 | `/*` |
|        - |  3144 | ` * DONE: P1 * *` |
|        - |  3145 | ` *` |
|        - |  3146 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3147 | ` * and return immediately.` |
|        - |  3148 | ` */` |
|    17481 |  3149 | `case PH7_OP_DONE:` |
|    34964 |  3150 | `	if( pInstr->iP1 ){` |
|        - |  3151 | `#ifdef UNTRUST` |
|        - |  3152 | `		if( pTos < pStack ){` |
|        - |  3153 | `			goto Abort;` |
|        - |  3154 | `		}` |
|        - |  3155 | `#endif` |
|    20420 |  3156 | `		if( pLastRef ){` |
|    13174 |  3157 | `			*pLastRef = pTos->nIdx;` |
|     6586 |  3158 | `		}` |
|    20420 |  3159 | `		if( pResult ){` |
|        - |  3160 | `			/* Execution result */` |
|    19392 |  3161 | `			PH7_MemObjStore(pTos,pResult);` |
|     9695 |  3162 | `		}` |
|    20420 |  3163 | `		VmPopOperand(&pTos,1);` |
|    24755 |  3164 | `	}else if( pLastRef ){` |
|        - |  3165 | `		/* Nothing referenced */` |
|     1144 |  3166 | `		*pLastRef = SXU32_HIGH;` |
|      571 |  3167 | `	}` |
|        - |  3168 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3169 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3170 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3171 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3172 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3173 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3174 | `	 * block can override it.` |
|        - |  3175 | `	 */` |
|    34966 |  3176 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3177 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3178 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3179 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3180 | `		pExc->pFrame = 0;` |
|        3 |  3181 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3182 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3183 | `			pExc->iFinallyDone = 1;` |
|        - |  3184 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3185 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3186 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3187 | `				goto Abort;` |
|        - |  3188 | `			}` |
|        1 |  3189 | `		}` |
|        1 |  3190 | `	}` |
|    34964 |  3191 | `	goto Done;` |
|        - |  3192 | `/*` |
|        - |  3193 | ` * HALT: P1 * *` |
|        - |  3194 | ` *` |
|        - |  3195 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3196 | ` * and abort immediately.` |
|        - |  3197 | ` */` |
|        4 |  3198 | `case PH7_OP_HALT:` |
|        9 |  3199 | `	if( pInstr->iP1 ){` |
|        - |  3200 | `#ifdef UNTRUST` |
|        - |  3201 | `		if( pTos < pStack ){` |
|        - |  3202 | `			goto Abort;` |
|        - |  3203 | `		}` |
|        - |  3204 | `#endif` |
|        9 |  3205 | `		if( pLastRef ){` |
|      ! 0 |  3206 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3207 | `		}` |
|        9 |  3208 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3209 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3210 | `				/* Output the exit message */` |
|        7 |  3211 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3212 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3213 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3214 | `			}` |
|        7 |  3215 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3216 | `			/* Record exit status */` |
|        5 |  3217 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3218 | `		}` |
|        9 |  3219 | `		VmPopOperand(&pTos,1);` |
|        4 |  3220 | `	}else if( pLastRef ){` |
|        - |  3221 | `		/* Nothing referenced */` |
|      ! 0 |  3222 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3223 | `	}` |
|        - |  3224 | `	/* Check if we're in an included file context */` |
|        9 |  3225 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3226 | `		/* Terminate the entire process */` |
|        9 |  3227 | `		exit(pVm->iExitStatus);` |
|        - |  3228 | `	}` |
|      ! 0 |  3229 | `	goto Abort;` |
|        - |  3230 | `/*` |
|        - |  3231 | ` * JMP: * P2 *` |
|        - |  3232 | ` *` |
|        - |  3233 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3234 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3235 | ` */` |
|   227032 |  3236 | `case PH7_OP_JMP:` |
|   454110 |  3237 | `	pc = pInstr->iP2 - 1;` |
|   454110 |  3238 | `	break;` |
|        - |  3239 | `/*` |
|        - |  3240 | ` * JZ: P1 P2 *` |
|        - |  3241 | ` *` |
|        - |  3242 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3243 | ` * entry in the stack if P1 is zero.` |
|        - |  3244 | ` */` |
|   534085 |  3245 | `case PH7_OP_JZ:` |
|        - |  3246 | `#ifdef UNTRUST` |
|        - |  3247 | `	if( pTos < pStack ){` |
|        - |  3248 | `		goto Abort;` |
|        - |  3249 | `	}` |
|        - |  3250 | `#endif` |
|        - |  3251 | `	/* Get a boolean value */` |
|  1068260 |  3252 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  3253 | `		PH7_MemObjToBool(pTos);` |
|       80 |  3254 | `	}` |
|  1068260 |  3255 | `	if( !pTos->x.iVal ){` |
|        - |  3256 | `		/* Take the jump */` |
|   544022 |  3257 | `		pc = pInstr->iP2 - 1;` |
|   272010 |  3258 | `	}` |
|  1068260 |  3259 | `	if( !pInstr->iP1 ){` |
|   848178 |  3260 | `		VmPopOperand(&pTos,1);` |
|   424110 |  3261 | `	}` |
|  1068260 |  3262 | `	break;` |
|        - |  3263 | `/*` |
|        - |  3264 | ` * JNZ: P1 P2 *` |
|        - |  3265 | ` *` |
|        - |  3266 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3267 | ` * entry in the stack if P1 is zero.` |
|        - |  3268 | ` */` |
|    55857 |  3269 | `case PH7_OP_JNZ:` |
|        - |  3270 | `#ifdef UNTRUST` |
|        - |  3271 | `	if( pTos < pStack ){` |
|        - |  3272 | `		goto Abort;` |
|        - |  3273 | `	}` |
|        - |  3274 | `#endif` |
|        - |  3275 | `	/* Get a boolean value */` |
|   111716 |  3276 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3277 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3278 | `	}` |
|   111716 |  3279 | `	if( pTos->x.iVal ){` |
|        - |  3280 | `		/* Take the jump */` |
|     4868 |  3281 | `		pc = pInstr->iP2 - 1;` |
|     2433 |  3282 | `	}` |
|   111716 |  3283 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3284 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3285 | `	}` |
|   111716 |  3286 | `	break;` |
|        - |  3287 | `/*` |
|        - |  3288 | ` * NOOP: * * *` |
|        - |  3289 | ` *` |
|        - |  3290 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3291 | ` * destination.` |
|        - |  3292 | ` */` |
|      ! 0 |  3293 | `case PH7_OP_NOOP:` |
|      ! 0 |  3294 | `	break;` |
|        - |  3295 | `/*` |
|        - |  3296 | ` * POP: P1 * *` |
|        - |  3297 | ` *` |
|        - |  3298 | ` * Pop P1 elements from the operand stack.` |
|        - |  3299 | ` */` |
|   413872 |  3300 | `case PH7_OP_POP: {` |
|   827790 |  3301 | `	sxi32 n = pInstr->iP1;` |
|   827790 |  3302 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3303 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3304 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3305 | `	}` |
|   827790 |  3306 | `	VmPopOperand(&pTos,n);` |
|   827790 |  3307 | `	break;` |
|        - |  3308 | `				 }` |
|        - |  3309 | `/*` |
|        - |  3310 | ` * DUP: * * *` |
|        - |  3311 | ` *` |
|        - |  3312 | ` * Duplicate the top of the stack.` |
|        - |  3313 | ` */` |
|       41 |  3314 | `case PH7_OP_DUP:` |
|        - |  3315 | `#ifdef UNTRUST` |
|        - |  3316 | `	if( pTos < pStack ){` |
|        - |  3317 | `		goto Abort;` |
|        - |  3318 | `	}` |
|        - |  3319 | `#endif` |
|       84 |  3320 | `	pTos++;` |
|       84 |  3321 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3322 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3323 | `	break;` |
|        - |  3324 | `/*` |
|        - |  3325 | ` * NSSWITCH: * * P3` |
|        - |  3326 | ` *` |
|        - |  3327 | ` * Switch the active namespace at runtime.` |
|        - |  3328 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3329 | ` */` |
|     6918 |  3330 | `case PH7_OP_NSSWITCH:` |
|    13838 |  3331 | `	SyBlobReset(&pVm->sNamespace);` |
|    13838 |  3332 | `	if( pInstr->p3 ){` |
|       96 |  3333 | `		const char *zNs = (const char *)pInstr->p3;` |
|       96 |  3334 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       47 |  3335 | `	}` |
|        - |  3336 | `	/* Clear namespace-scoped use-const imports */` |
|    13838 |  3337 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13838 |  3338 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13838 |  3339 | `	break;` |
|        - |  3340 | `/* OP_USECONST P1 * P3` |
|        - |  3341 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3342 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3343 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3344 | ` */` |
|        7 |  3345 | `case PH7_OP_USECONST: {` |
|       16 |  3346 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3347 | `	if( azPair ){` |
|       16 |  3348 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3349 | `	}` |
|       16 |  3350 | `	break;` |
|        - |  3351 | `				}` |
|        - |  3352 | `/*` |
|        - |  3353 | ` * CVT_INT: * * *` |
|        - |  3354 | ` *` |
|        - |  3355 | ` * Force the top of the stack to be an integer.` |
|        - |  3356 | ` */` |
|       77 |  3357 | `case PH7_OP_CVT_INT:` |
|        - |  3358 | `#ifdef UNTRUST` |
|        - |  3359 | `	if( pTos < pStack ){` |
|        - |  3360 | `		goto Abort;` |
|        - |  3361 | `	}` |
|        - |  3362 | `#endif` |
|      156 |  3363 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3364 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3365 | `	}` |
|        - |  3366 | `	/* Invalidate any prior representation */` |
|      156 |  3367 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3368 | `	break;` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * CVT_REAL: * * *` |
|        - |  3371 | ` *` |
|        - |  3372 | ` * Force the top of the stack to be a real.` |
|        - |  3373 | ` */` |
|        4 |  3374 | `case PH7_OP_CVT_REAL:` |
|        - |  3375 | `#ifdef UNTRUST` |
|        - |  3376 | `	if( pTos < pStack ){` |
|        - |  3377 | `		goto Abort;` |
|        - |  3378 | `	}` |
|        - |  3379 | `#endif` |
|        9 |  3380 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3381 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3382 | `	}` |
|        - |  3383 | `	/* Invalidate any prior representation */` |
|        9 |  3384 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3385 | `	break;` |
|        - |  3386 | `/*` |
|        - |  3387 | ` * CVT_STR: * * *` |
|        - |  3388 | ` *` |
|        - |  3389 | ` * Force the top of the stack to be a string.` |
|        - |  3390 | ` */` |
|      146 |  3391 | `case PH7_OP_CVT_STR:` |
|        - |  3392 | `#ifdef UNTRUST` |
|        - |  3393 | `	if( pTos < pStack ){` |
|        - |  3394 | `		goto Abort;` |
|        - |  3395 | `	}` |
|        - |  3396 | `#endif` |
|      294 |  3397 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3398 | `		PH7_MemObjToString(pTos);` |
|      146 |  3399 | `	}` |
|      294 |  3400 | `	break;` |
|        - |  3401 | `/*` |
|        - |  3402 | ` * CVT_BOOL: * * *` |
|        - |  3403 | ` *` |
|        - |  3404 | ` * Force the top of the stack to be a boolean.` |
|        - |  3405 | ` */` |
|        5 |  3406 | `case PH7_OP_CVT_BOOL:` |
|        - |  3407 | `#ifdef UNTRUST` |
|        - |  3408 | `	if( pTos < pStack ){` |
|        - |  3409 | `		goto Abort;` |
|        - |  3410 | `	}` |
|        - |  3411 | `#endif` |
|       11 |  3412 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3413 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3414 | `	}` |
|       11 |  3415 | `	break;` |
|        - |  3416 | `/*` |
|        - |  3417 | ` * CVT_NULL: * * *` |
|        - |  3418 | ` *` |
|        - |  3419 | ` * Nullify the top of the stack.` |
|        - |  3420 | ` */` |
|        3 |  3421 | `case PH7_OP_CVT_NULL:` |
|        - |  3422 | `#ifdef UNTRUST` |
|        - |  3423 | `	if( pTos < pStack ){` |
|        - |  3424 | `		goto Abort;` |
|        - |  3425 | `	}` |
|        - |  3426 | `#endif` |
|        7 |  3427 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3428 | `	break;` |
|        - |  3429 | `/*` |
|        - |  3430 | ` * CVT_NUMC: * * *` |
|        - |  3431 | ` *` |
|        - |  3432 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3433 | ` */` |
|      ! 0 |  3434 | `case PH7_OP_CVT_NUMC:` |
|        - |  3435 | `#ifdef UNTRUST` |
|        - |  3436 | `	if( pTos < pStack ){` |
|        - |  3437 | `		goto Abort;` |
|        - |  3438 | `	}` |
|        - |  3439 | `#endif` |
|        - |  3440 | `	/* Force a numeric cast */` |
|      ! 0 |  3441 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3442 | `	break;` |
|        - |  3443 | `/*` |
|        - |  3444 | ` * CVT_ARRAY: * * *` |
|        - |  3445 | ` *` |
|        - |  3446 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3447 | ` */` |
|       10 |  3448 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3449 | `#ifdef UNTRUST` |
|        - |  3450 | `	if( pTos < pStack ){` |
|        - |  3451 | `		goto Abort;` |
|        - |  3452 | `	}` |
|        - |  3453 | `#endif` |
|        - |  3454 | `	/* Force a hashmap cast */` |
|       21 |  3455 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3456 | `	if( rc != SXRET_OK ){` |
|        - |  3457 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3458 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3459 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3460 | `	}` |
|       21 |  3461 | `	break;` |
|        - |  3462 | `/*` |
|        - |  3463 | ` * CVT_OBJ: * * *` |
|        - |  3464 | ` *` |
|        - |  3465 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3466 | ` */` |
|        8 |  3467 | `case PH7_OP_CVT_OBJ:` |
|        - |  3468 | `#ifdef UNTRUST` |
|        - |  3469 | `	if( pTos < pStack ){` |
|        - |  3470 | `		goto Abort;` |
|        - |  3471 | `	}` |
|        - |  3472 | `#endif` |
|       17 |  3473 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3474 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3475 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3476 | `	}` |
|       17 |  3477 | `	break;` |
|        - |  3478 | `/*` |
|        - |  3479 | ` * ERR_CTRL * * *` |
|        - |  3480 | ` *` |
|        - |  3481 | ` * Error control operator.` |
|        - |  3482 | ` */` |
|    13983 |  3483 | `case PH7_OP_ERR_CTRL:` |
|        - |  3484 | `	/*` |
|        - |  3485 | `	 * TICKET 1433-038:` |
|        - |  3486 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3487 | `	 * use the public API,to control error output.` |
|        - |  3488 | `	 */` |
|    27966 |  3489 | `	break;` |
|        - |  3490 | `/*` |
|        - |  3491 | ` * IS_A * * *` |
|        - |  3492 | ` *` |
|        - |  3493 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3494 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3495 | ` * holding a class name or an object).` |
|        - |  3496 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3497 | ` */` |
|       23 |  3498 | `case PH7_OP_IS_A:{` |
|       48 |  3499 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3500 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3501 | `#ifdef UNTRUST` |
|        - |  3502 | `	if( pNos < pStack ){` |
|        - |  3503 | `		goto Abort;` |
|        - |  3504 | `	}` |
|        - |  3505 | `#endif` |
|       48 |  3506 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3507 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3508 | `		ph7_class *pClass = 0;` |
|        - |  3509 | `		/* Extract the target class */` |
|       46 |  3510 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3511 | `			/* Instance already loaded */` |
|      ! 0 |  3512 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3513 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3514 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3515 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3516 | `			/* Handle self/static/parent keywords */` |
|       46 |  3517 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3518 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3519 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3520 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3521 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3522 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3523 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3524 | `					pClass = pSelf->pBase;` |
|        2 |  3525 | `				}` |
|        3 |  3526 | `			}else{` |
|       36 |  3527 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3528 | `			}` |
|       22 |  3529 | `		}` |
|       46 |  3530 | `		if( pClass ){` |
|        - |  3531 | `			/* Perform the query */` |
|       46 |  3532 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3533 | `		}` |
|       22 |  3534 | `	}` |
|        - |  3535 | `	/* Push result */` |
|       48 |  3536 | `	VmPopOperand(&pTos,1);` |
|       48 |  3537 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3538 | `	pTos->x.iVal = iRes;` |
|       48 |  3539 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3540 | `	break;` |
|        - |  3541 | `				 }` |
|        - |  3542 |  |
|        - |  3543 | `/*` |
|        - |  3544 | ` * LOADC P1 P2 *` |
|        - |  3545 | ` *` |
|        - |  3546 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3547 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3548 | ` */` |
|   894997 |  3549 | `case PH7_OP_LOADC: {` |
|        - |  3550 | `	ph7_value *pObj;` |
|        - |  3551 | `	/* Reserve a room */` |
|  1790040 |  3552 | `	pTos++;` |
|  2676392 |  3553 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1790040 |  3554 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3555 | `			SyHashEntry *pEntry;` |
|        - |  3556 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3557 | `			{` |
|        - |  3558 | `				SyHashEntry *pConstImport;` |
|    26072 |  3559 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    17380 |  3560 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17382 |  3561 | `				if( pConstImport ){` |
|       11 |  3562 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3563 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3564 | `					if( pEntry ){` |
|       11 |  3565 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3566 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3567 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3568 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3569 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3570 | `						break;` |
|        - |  3571 | `					}` |
|        - |  3572 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3573 | `				}` |
|        - |  3574 | `			}` |
|        - |  3575 | `			/* Candidate for expansion via user defined callbacks */` |
|    17372 |  3576 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17372 |  3577 | `			if( pEntry ){` |
|    17368 |  3578 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3579 | `				/* Set a NULL default value */` |
|    17368 |  3580 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    17368 |  3581 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3582 | `				/* Invoke the callback and deal with the expanded value */` |
|    17368 |  3583 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3584 | `				/* Mark as constant */` |
|    17368 |  3585 | `				pTos->nIdx = SXU32_HIGH;` |
|    17368 |  3586 | `				break;` |
|        - |  3587 | `			}` |
|        - |  3588 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3589 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3590 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3591 | `			{` |
|        6 |  3592 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3593 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3594 | `				sxu32 j;` |
|        6 |  3595 | `				int isQualified = 0;` |
|       32 |  3596 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3597 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3598 | `				}` |
|        6 |  3599 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3600 | `					/* Try current_namespace\name */` |
|      ! 0 |  3601 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3602 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3603 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3604 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3605 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3606 | `					if( pEntry ){` |
|      ! 0 |  3607 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3608 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3609 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3610 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3611 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3612 | `						break;` |
|        - |  3613 | `					}` |
|        - |  3614 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3615 | `				}` |
|        6 |  3616 | `				if( isQualified ){` |
|        - |  3617 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3618 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3619 | `					SyBlob sErr;` |
|        3 |  3620 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3621 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3622 | `					if( pErrFile ){` |
|        3 |  3623 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3624 | `					}` |
|        3 |  3625 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3626 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3627 | `					SyBlobRelease(&sErr);` |
|        3 |  3628 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3629 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3630 | `					goto LoadC_Done;` |
|        - |  3631 | `				}` |
|        - |  3632 | `			}` |
|        1 |  3633 | `		}` |
|  1772662 |  3634 | `		PH7_MemObjLoad(pObj,pTos);` |
|   886354 |  3635 | `	}else{` |
|        - |  3636 | `		/* Set a NULL value */` |
|      ! 0 |  3637 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3638 | `	}` |
|   886309 |  3639 | `LoadC_Done:` |
|        - |  3640 | `	/* Mark as constant */` |
|  1772664 |  3641 | `	pTos->nIdx = SXU32_HIGH;` |
|  1772664 |  3642 | `	break;` |
|        - |  3643 | `				  }` |
|        - |  3644 | `/*` |
|        - |  3645 | ` * LOAD: P1 * P3` |
|        - |  3646 | ` *` |
|        - |  3647 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3648 | ` * from the P3 operand.` |
|        - |  3649 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3650 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3651 | ` */` |
|  1422856 |  3652 | `case PH7_OP_LOAD:{` |
|        - |  3653 | `	ph7_value *pObj;` |
|        - |  3654 | `	SyString sName;` |
|  2845934 |  3655 | `	if( pInstr->p3 == 0 ){` |
|        - |  3656 | `		/* Take the variable name from the top of the stack */` |
|        - |  3657 | `#ifdef UNTRUST` |
|        - |  3658 | `		if( pTos < pStack ){` |
|        - |  3659 | `			goto Abort;` |
|        - |  3660 | `		}` |
|        - |  3661 | `#endif` |
|        - |  3662 | `		/* Force a string cast */` |
|       19 |  3663 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3664 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3665 | `		}` |
|       19 |  3666 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3667 | `	}else{` |
|  2845916 |  3668 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3669 | `		/* Reserve a room for the target object */` |
|  2845916 |  3670 | `		pTos++;` |
|        - |  3671 | `	}` |
|        - |  3672 | `	/* Extract the requested memory object */` |
|  2845934 |  3673 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2845934 |  3674 | `	if( pObj == 0 ){` |
|       28 |  3675 | `		if( pInstr->iP1 ){` |
|        - |  3676 | `			/* Variable not found,load NULL */` |
|       28 |  3677 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3678 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3679 | `			}else{` |
|       28 |  3680 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3681 | `			}` |
|       28 |  3682 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1422871 |  3683 | `			break;` |
|      ! 0 |  3684 | `		}else{` |
|        - |  3685 | `			/* Fatal error */` |
|      ! 0 |  3686 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3687 | `			goto Abort;` |
|        - |  3688 | `		}` |
|        - |  3689 | `	}` |
|        - |  3690 | `	/* Load variable contents */` |
|  2845908 |  3691 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2845908 |  3692 | `	pTos->nIdx = pObj->nIdx;` |
|  2845908 |  3693 | `	break;` |
|        - |  3694 | `				   }` |
|        - |  3695 | `/*` |
|        - |  3696 | ` * LOAD_MAP P1 * *` |
|        - |  3697 | ` *` |
|        - |  3698 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3699 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3700 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3701 | ` */` |
|    19966 |  3702 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3703 | `	ph7_hashmap *pMap;` |
|        - |  3704 | `	/* Allocate a new hashmap instance */` |
|    39934 |  3705 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    39934 |  3706 | `	if( pMap == 0 ){` |
|      ! 0 |  3707 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3708 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3709 | `		goto Abort;` |
|        - |  3710 | `	}` |
|    39934 |  3711 | `	if( pInstr->iP1 > 0 ){` |
|     2352 |  3712 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3713 | `		/* Perform the insertion */` |
|     7196 |  3714 | `		while( pEntry < pTos ){` |
|     4846 |  3715 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3716 | `				/* Insertion by reference */` |
|      142 |  3717 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3718 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3719 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3720 | `					);` |
|       48 |  3721 | `			}else{` |
|        - |  3722 | `				/* Standard insertion */` |
|     7127 |  3723 | `				PH7_HashmapInsert(pMap,` |
|     4750 |  3724 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2375 |  3725 | `					&pEntry[1]` |
|        - |  3726 | `				);` |
|        - |  3727 | `			}` |
|        - |  3728 | `			/* Next pair on the stack */` |
|     4846 |  3729 | `			pEntry += 2;` |
|        2 |  3730 | `		}` |
|        - |  3731 | `		/* Pop P1 elements */` |
|     2352 |  3732 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1175 |  3733 | `	}` |
|        - |  3734 | `	/* Push the hashmap */` |
|    39934 |  3735 | `	pTos++;` |
|    39934 |  3736 | `	pTos->nIdx = SXU32_HIGH;` |
|    39934 |  3737 | `	pTos->x.pOther = pMap;` |
|    39934 |  3738 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    39934 |  3739 | `	break;` |
|        - |  3740 | `					  }` |
|        - |  3741 | `/*` |
|        - |  3742 | ` * LOAD_LIST: P1 * *` |
|        - |  3743 | ` *` |
|        - |  3744 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3745 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3746 | ` * Caveats:` |
|        - |  3747 | ` *  This implementation support only a single nesting level.` |
|        - |  3748 | ` */` |
|       48 |  3749 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3750 | `	ph7_value *pEntry;` |
|       98 |  3751 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3752 | `		/* Empty list,break immediately */` |
|      ! 0 |  3753 | `		break;` |
|        - |  3754 | `	}` |
|       98 |  3755 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3756 | `#ifdef UNTRUST` |
|        - |  3757 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3758 | `		goto Abort;` |
|        - |  3759 | `	}` |
|        - |  3760 | `#endif` |
|       98 |  3761 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  3762 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3763 | `		ph7_hashmap_node *pNode;` |
|        - |  3764 | `		ph7_value sKey,*pObj;` |
|        - |  3765 | `		/* Start Copying */` |
|       91 |  3766 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  3767 | `		while( pEntry <= pTos ){` |
|      193 |  3768 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  3769 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  3770 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  3771 | `					if( rc == SXRET_OK ){` |
|        - |  3772 | `						/* Store node value */` |
|      165 |  3773 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  3774 | `					}else{` |
|        - |  3775 | `						/* Undefined array key */` |
|        - |  3776 | `						char zMsg[128];` |
|      ! 0 |  3777 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  3778 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3779 | `						PH7_MemObjRelease(pObj);` |
|        - |  3780 | `					}` |
|       82 |  3781 | `				}` |
|       82 |  3782 | `			}` |
|      193 |  3783 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  3784 | `			pEntry++;` |
|        1 |  3785 | `		}` |
|       46 |  3786 | `	}else{` |
|        - |  3787 | `		/* Source is not an array */` |
|        - |  3788 | `		ph7_value *pObj;` |
|       18 |  3789 | `		while( pEntry <= pTos ){` |
|       12 |  3790 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  3791 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  3792 | `					PH7_MemObjRelease(pObj);` |
|        5 |  3793 | `				}` |
|        5 |  3794 | `			}` |
|       12 |  3795 | `			pEntry++;` |
|        2 |  3796 | `		}` |
|        8 |  3797 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  3798 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  3799 | `			const char *zType = "unknown";` |
|        3 |  3800 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  3801 | `			char zMsg[256];` |
|        3 |  3802 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  3803 | `				zType = "string";` |
|        1 |  3804 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  3805 | `				zType = "int";` |
|      ! 0 |  3806 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3807 | `				zType = "float";` |
|      ! 0 |  3808 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3809 | `				zType = "object";` |
|      ! 0 |  3810 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  3811 | `				zType = "resource";` |
|      ! 0 |  3812 | `			}` |
|        3 |  3813 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  3814 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  3815 | `		}` |
|        - |  3816 | `	}` |
|       98 |  3817 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  3818 | `	break;` |
|        - |  3819 | `					   }` |
|        - |  3820 | `/*` |
|        - |  3821 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3822 | ` *` |
|        - |  3823 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3824 | ` * from the stack.` |
|        - |  3825 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3826 | ` * instead.` |
|        - |  3827 | ` */` |
|   228339 |  3828 | `case PH7_OP_LOAD_IDX: {` |
|   456724 |  3829 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   456724 |  3830 | `	ph7_hashmap *pMap = 0;` |
|        - |  3831 | `	ph7_value *pIdx;` |
|   456724 |  3832 | `	pIdx = 0;` |
|   456724 |  3833 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3834 | `		if( !pInstr->iP2){` |
|        - |  3835 | `			/* No available index,load NULL */` |
|      ! 0 |  3836 | `			if( pTos >= pStack ){` |
|      ! 0 |  3837 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3838 | `			}else{` |
|        - |  3839 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3840 | `				pTos++;` |
|      ! 0 |  3841 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3842 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3843 | `			}` |
|        - |  3844 | `			/* Emit a notice */` |
|      ! 0 |  3845 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3846 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3847 | `			break;` |
|        - |  3848 | `		}` |
|      ! 0 |  3849 | `	}else{` |
|   456724 |  3850 | `		pIdx = pTos;` |
|   456724 |  3851 | `		pTos--;` |
|        - |  3852 | `	}` |
|   456724 |  3853 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3854 | `		/* String access */` |
|   357198 |  3855 | `		if( pIdx ){` |
|        - |  3856 | `			sxu32 nOfft;` |
|   357198 |  3857 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3858 | `				/* Force an int cast */` |
|      ! 0 |  3859 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3860 | `			}` |
|   357198 |  3861 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   357198 |  3862 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3863 | `				/* Invalid offset,load null */` |
|      ! 0 |  3864 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3865 | `			}else{` |
|   357198 |  3866 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   357198 |  3867 | `				int c = zData[nOfft];` |
|   357198 |  3868 | `				PH7_MemObjRelease(pTos);` |
|   357198 |  3869 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   357198 |  3870 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3871 | `			}` |
|   178622 |  3872 | `		}else{` |
|        - |  3873 | `			/* No available index,load NULL */` |
|      ! 0 |  3874 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3875 | `		}` |
|   357198 |  3876 | `		break;` |
|        - |  3877 | `	}` |
|    99528 |  3878 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3879 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3880 | `			ph7_value *pObj;` |
|        3 |  3881 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3882 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  3883 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  3884 | `			}` |
|        1 |  3885 | `		}` |
|        1 |  3886 | `	}` |
|    99528 |  3887 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    99528 |  3888 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    99528 |  3889 | `		if( pInstr->iP2 == 1 ){` |
|        - |  3890 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3891 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3892 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3893 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  3894 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  3895 | `		}` |
|        - |  3896 | `		/* Point to the hashmap */` |
|    99528 |  3897 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    99528 |  3898 | `		if( pIdx ){` |
|        - |  3899 | `			/* Load the desired entry */` |
|    99528 |  3900 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    49763 |  3901 | `		}` |
|    99528 |  3902 | `		if( pInstr->iP2 == 3 ){` |
|        - |  3903 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  3904 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  3905 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  3906 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  3907 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  3908 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  3909 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  3910 | `			 * correct for the outermost write. */` |
|       19 |  3911 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  3912 | `			if( !needWrite && pNode ){` |
|       13 |  3913 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  3914 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  3915 | `					needWrite = 1;` |
|        3 |  3916 | `				}` |
|        6 |  3917 | `			}` |
|       19 |  3918 | `			if( needWrite ){` |
|       13 |  3919 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  3920 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  3921 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  3922 | `					 * into the new map's storage. */` |
|        7 |  3923 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  3924 | `					if( pIdx ){` |
|        7 |  3925 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  3926 | `					}` |
|        3 |  3927 | `				}` |
|        6 |  3928 | `			}` |
|        9 |  3929 | `		}` |
|    99528 |  3930 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  3931 | `			/* Create a new empty entry */` |
|      273 |  3932 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  3933 | `			if( rc == SXRET_OK ){` |
|        - |  3934 | `				/* Point to the last inserted entry */` |
|      273 |  3935 | `				pNode = pMap->pLast;` |
|      136 |  3936 | `			}` |
|      136 |  3937 | `		}` |
|    49763 |  3938 | `	}` |
|    99528 |  3939 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  3940 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  3941 | `		char zMsg[128];` |
|      ! 0 |  3942 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3943 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3944 | `		}` |
|      ! 0 |  3945 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  3946 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3947 | `	}` |
|    99528 |  3948 | `	if( pIdx ){` |
|    99528 |  3949 | `		PH7_MemObjRelease(pIdx);` |
|    49763 |  3950 | `	}` |
|    99528 |  3951 | `	if( rc == SXRET_OK ){` |
|        - |  3952 | `		/* Load entry contents */` |
|    44956 |  3953 | `		if( pMap->iRef < 2 ){` |
|        - |  3954 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3955 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3956 | `			 */` |
|       24 |  3957 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3958 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3959 | `		}else{` |
|    44934 |  3960 | `			pTos->nIdx = pNode->nValIdx;` |
|    44934 |  3961 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    44934 |  3962 | `			PH7_HashmapUnref(pMap);` |
|        - |  3963 | `		}` |
|    22479 |  3964 | `	}else{` |
|        - |  3965 | `		/* No such entry,load NULL */` |
|    54574 |  3966 | `		PH7_MemObjRelease(pTos);` |
|    54574 |  3967 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3968 | `	}` |
|    99528 |  3969 | `	break;` |
|        - |  3970 | `					  }` |
|        - |  3971 | `/*` |
|        - |  3972 | ` * LOAD_CLOSURE * * P3` |
|        - |  3973 | ` *` |
|        - |  3974 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3975 | ` * name in the stack.` |
|        - |  3976 | ` */` |
|       44 |  3977 | `case PH7_OP_LOAD_CLOSURE:{` |
|       89 |  3978 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       89 |  3979 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3980 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3981 | `		ph7_vm_func *pClosure;` |
|        - |  3982 | `		char *zName;` |
|        - |  3983 | `		sxu32 mLen;` |
|        - |  3984 | `		sxu32 n;` |
|        - |  3985 | `		/* Create a new VM function */` |
|       89 |  3986 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3987 | `		/* Generate an unique closure name */` |
|       89 |  3988 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       89 |  3989 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3990 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3991 | `			goto Abort;` |
|        - |  3992 | `		}` |
|       89 |  3993 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       89 |  3994 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3995 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3996 | `		}` |
|        - |  3997 | `		/* Zero the stucture */` |
|       89 |  3998 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3999 | `		/* Perform a structure assignment on read-only items */` |
|       89 |  4000 | `		pClosure->aArgs = pFunc->aArgs;` |
|       89 |  4001 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       89 |  4002 | `		pClosure->aStatic = pFunc->aStatic;` |
|       89 |  4003 | `		pClosure->iFlags = pFunc->iFlags;` |
|       89 |  4004 | `		pClosure->pUserData = pFunc->pUserData;` |
|       89 |  4005 | `		pClosure->sSignature = pFunc->sSignature;` |
|       89 |  4006 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       89 |  4007 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       89 |  4008 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4009 | `		/* Register the closure */` |
|       89 |  4010 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4011 | `		/* Set up closure environment */` |
|       89 |  4012 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       89 |  4013 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      241 |  4014 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4015 | `			ph7_value *pValue;` |
|      153 |  4016 | `			pEnv = &aEnv[n];` |
|      153 |  4017 | `			sEnv.sName  = pEnv->sName;` |
|      153 |  4018 | `			sEnv.iFlags = pEnv->iFlags;` |
|      153 |  4019 | `			sEnv.nIdx = SXU32_HIGH;` |
|      153 |  4020 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      153 |  4021 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4022 | `				/* Pass by reference */` |
|      ! 0 |  4023 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4024 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4025 | `					);` |
|      ! 0 |  4026 | `			}` |
|        - |  4027 | `			/* Standard pass by value */` |
|      153 |  4028 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      153 |  4029 | `			if( pValue ){` |
|        - |  4030 | `				/* Copy imported value */` |
|       69 |  4031 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4032 | `			}` |
|        - |  4033 | `			/* Insert the imported variable */` |
|      153 |  4034 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       77 |  4035 | `		}` |
|        - |  4036 | `		/* Finally,load the closure name on the stack */` |
|       89 |  4037 | `		pTos++;` |
|       89 |  4038 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       44 |  4039 | `	}` |
|       89 |  4040 | `	break;` |
|        - |  4041 | `						 }` |
|        - |  4042 | `/*` |
|        - |  4043 | ` * STORE * P2 P3` |
|        - |  4044 | ` *` |
|        - |  4045 | ` * Perform a store (Assignment) operation.` |
|        - |  4046 | ` */` |
|   123418 |  4047 | `case PH7_OP_STORE: {` |
|        - |  4048 | `	ph7_value *pObj;` |
|        - |  4049 | `	SyString sName;` |
|        - |  4050 | `#ifdef UNTRUST` |
|        - |  4051 | `	if( pTos < pStack ){` |
|        - |  4052 | `		goto Abort;` |
|        - |  4053 | `	}` |
|        - |  4054 | `#endif` |
|   246838 |  4055 | `	if( pInstr->iP2 ){` |
|        - |  4056 | `		sxu32 nIdx;` |
|        - |  4057 | `		sxi32 rcT;` |
|        - |  4058 | `		/* Member store operation */` |
|     3468 |  4059 | `		nIdx = pTos->nIdx;` |
|     3468 |  4060 | `		VmPopOperand(&pTos,1);` |
|     3468 |  4061 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4062 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4063 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4064 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4065 | `		}else{` |
|        - |  4066 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4067 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     3464 |  4068 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     3464 |  4069 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4070 | `				goto Abort;` |
|        - |  4071 | `			}` |
|     3464 |  4072 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4073 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4074 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4075 | `				 * propagate out of the VM loop. */` |
|       29 |  4076 | `				VmPopOperand(&pTos,1);` |
|        - |  4077 | `				{` |
|       29 |  4078 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       29 |  4079 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       29 |  4080 | `						pc = pFrm2->iExceptionJump - 1;` |
|   123433 |  4081 | `						break;` |
|        - |  4082 | `					}` |
|        - |  4083 | `				}` |
|      ! 0 |  4084 | `				goto Exception;` |
|        - |  4085 | `			}` |
|        - |  4086 | `			/* Point to the desired memory object */` |
|     3436 |  4087 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3436 |  4088 | `			if( pObj ){` |
|        - |  4089 | `				/* Perform the store operation */` |
|     3436 |  4090 | `				PH7_MemObjStore(pTos,pObj);` |
|     1717 |  4091 | `			}` |
|        - |  4092 | `		}` |
|     3440 |  4093 | `		break;` |
|   243372 |  4094 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4095 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4096 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4097 | `			/* Force a string cast */` |
|      ! 0 |  4098 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4099 | `		}` |
|        7 |  4100 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4101 | `		pTos--;` |
|        - |  4102 | `#ifdef UNTRUST` |
|        - |  4103 | `		if( pTos < pStack  ){` |
|        - |  4104 | `			goto Abort;` |
|        - |  4105 | `		}` |
|        - |  4106 | `#endif` |
|        4 |  4107 | `	}else{` |
|   243366 |  4108 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4109 | `	}` |
|        - |  4110 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   243372 |  4111 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   243372 |  4112 | `	if( pObj == 0 ){` |
|      ! 0 |  4113 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4114 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4115 | `		goto Abort;` |
|        - |  4116 | `	}` |
|   243372 |  4117 | `	if( !pInstr->p3 ){` |
|        7 |  4118 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4119 | `	}` |
|        - |  4120 | `	/* Perform the store operation */` |
|   243372 |  4121 | `	PH7_MemObjStore(pTos,pObj);` |
|   243372 |  4122 | `	break;` |
|        - |  4123 | `				   }` |
|        - |  4124 | `/*` |
|        - |  4125 | ` * STORE_IDX:   P1 * P3` |
|        - |  4126 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4127 | ` *` |
|        - |  4128 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4129 | ` */` |
|    87699 |  4130 | `case PH7_OP_STORE_IDX:` |
|        - |  4131 | `case PH7_OP_STORE_IDX_REF: {` |
|   175400 |  4132 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4133 | `	ph7_value *pKey;` |
|        - |  4134 | `	sxu32 nIdx;` |
|   175400 |  4135 | `	if( pInstr->iP1 ){` |
|        - |  4136 | `		/* Key is next on stack */` |
|    59616 |  4137 | `		pKey = pTos;` |
|    59616 |  4138 | `		pTos--;` |
|    29809 |  4139 | `	}else{` |
|   115786 |  4140 | `		pKey = 0;` |
|        - |  4141 | `	}` |
|   175400 |  4142 | `	nIdx = pTos->nIdx;` |
|   175400 |  4143 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4144 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4145 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4146 | `		 * checking true sharing count, then re-add after separation. */` |
|   175348 |  4147 | `		if( nIdx != SXU32_HIGH ){` |
|   175348 |  4148 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   263021 |  4149 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   175348 |  4150 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4151 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4152 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4153 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4154 | `				 * refcounts if the backing array was already separated. */` |
|   175348 |  4155 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   175348 |  4156 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   175348 |  4157 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   175348 |  4158 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   175348 |  4159 | `					pTos->x.pOther = pMap;` |
|    87675 |  4160 | `				}else{` |
|        - |  4161 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4162 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4163 | `					pMap = pCur;` |
|        - |  4164 | `				}` |
|    87675 |  4165 | `			}else{` |
|      ! 0 |  4166 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4167 | `			}` |
|    87675 |  4168 | `		}else{` |
|      ! 0 |  4169 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4170 | `		}` |
|   175348 |  4171 | `		if( pMap->iRef < 2 ){` |
|        - |  4172 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4173 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4174 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4175 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4176 | `			pMap->iRef = 2;` |
|      ! 0 |  4177 | `		}` |
|    87675 |  4178 | `	}else{` |
|        - |  4179 | `		ph7_value *pObj;` |
|       53 |  4180 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4181 | `		if( pObj == 0 ){` |
|      ! 0 |  4182 | `			if( pKey ){` |
|      ! 0 |  4183 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4184 | `			}` |
|      ! 0 |  4185 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4186 | `			break;` |
|        - |  4187 | `		}` |
|        - |  4188 | `		/* Phase#1: Load the array */` |
|       53 |  4189 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4190 | `			VmPopOperand(&pTos,1);` |
|       53 |  4191 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4192 | `				/* Force a string cast */` |
|      ! 0 |  4193 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4194 | `			}` |
|       53 |  4195 | `			if( pKey == 0 ){` |
|        - |  4196 | `				/* Append string */` |
|        3 |  4197 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4198 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4199 | `				}` |
|        2 |  4200 | `			}else{` |
|        - |  4201 | `				sxu32 nOfft;` |
|       51 |  4202 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4203 | `					/* Force an int cast */` |
|       51 |  4204 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4205 | `				}` |
|       51 |  4206 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4207 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4208 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4209 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4210 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4211 | `				}else{` |
|      ! 0 |  4212 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4213 | `						/* Perform an append operation */` |
|      ! 0 |  4214 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4215 | `					}` |
|        - |  4216 | `				}` |
|        - |  4217 | `			}` |
|       53 |  4218 | `			if( pKey ){` |
|       51 |  4219 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4220 | `			}` |
|       53 |  4221 | `			break;` |
|      ! 0 |  4222 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4223 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4224 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4225 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4226 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4227 | `				goto Abort;` |
|        - |  4228 | `			}` |
|      ! 0 |  4229 | `		}` |
|        - |  4230 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4231 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4232 | `	}` |
|   175348 |  4233 | `	VmPopOperand(&pTos,1);` |
|        - |  4234 | `	/* Phase#2: Perform the insertion */` |
|   175348 |  4235 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4236 | `		/* Insertion by reference */` |
|       15 |  4237 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4238 | `	}else{` |
|   175334 |  4239 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4240 | `	}` |
|   175348 |  4241 | `	if( pKey ){` |
|    59566 |  4242 | `		PH7_MemObjRelease(pKey);` |
|    29782 |  4243 | `	}` |
|   175348 |  4244 | `	break;` |
|        - |  4245 | `					   }` |
|        - |  4246 | `/*` |
|        - |  4247 | ` * INCR: P1 * *` |
|        - |  4248 | ` *` |
|        - |  4249 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4250 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4251 | ` * the stack and increment after that.` |
|        - |  4252 | ` */` |
|   156670 |  4253 | `case PH7_OP_INCR:` |
|        - |  4254 | `#ifdef UNTRUST` |
|        - |  4255 | `	if( pTos < pStack ){` |
|        - |  4256 | `		goto Abort;` |
|        - |  4257 | `	}` |
|        - |  4258 | `#endif` |
|   313386 |  4259 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   313386 |  4260 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4261 | `			ph7_value *pObj;` |
|   313386 |  4262 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4263 | `				/* Force a numeric cast */` |
|   313386 |  4264 | `				PH7_MemObjToNumeric(pObj);` |
|   313386 |  4265 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4266 | `					pObj->rVal++;` |
|        - |  4267 | `					/* Try to get an integer representation */` |
|      ! 0 |  4268 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4269 | `				}else{` |
|   313386 |  4270 | `					pObj->x.iVal++;` |
|   313386 |  4271 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4272 | `				}` |
|   313386 |  4273 | `				if( pInstr->iP1 ){` |
|        - |  4274 | `					/* Pre-icrement */` |
|       77 |  4275 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4276 | `				}` |
|   156714 |  4277 | `			}` |
|   156716 |  4278 | `		}else{` |
|      ! 0 |  4279 | `			if( pInstr->iP1 ){` |
|        - |  4280 | `				/* Force a numeric cast */` |
|      ! 0 |  4281 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4282 | `				/* Pre-increment */` |
|      ! 0 |  4283 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4284 | `					pTos->rVal++;` |
|        - |  4285 | `					/* Try to get an integer representation */` |
|      ! 0 |  4286 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4287 | `				}else{` |
|      ! 0 |  4288 | `					pTos->x.iVal++;` |
|      ! 0 |  4289 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4290 | `				}` |
|      ! 0 |  4291 | `			}` |
|        - |  4292 | `		}` |
|   156714 |  4293 | `	}` |
|   313386 |  4294 | `	break;` |
|        - |  4295 | `/*` |
|        - |  4296 | ` * DECR: P1 * *` |
|        - |  4297 | ` *` |
|        - |  4298 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4299 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4300 | ` * and decrement after that.` |
|        - |  4301 | ` */` |
|        2 |  4302 | `case PH7_OP_DECR:` |
|        - |  4303 | `#ifdef UNTRUST` |
|        - |  4304 | `	if( pTos < pStack ){` |
|        - |  4305 | `		goto Abort;` |
|        - |  4306 | `	}` |
|        - |  4307 | `#endif` |
|        5 |  4308 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4309 | `		/* Force a numeric cast */` |
|        5 |  4310 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4311 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4312 | `			ph7_value *pObj;` |
|        5 |  4313 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4314 | `				/* Force a numeric cast */` |
|        5 |  4315 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4316 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4317 | `					pObj->rVal--;` |
|        - |  4318 | `					/* Try to get an integer representation */` |
|      ! 0 |  4319 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4320 | `				}else{` |
|        5 |  4321 | `					pObj->x.iVal--;` |
|        5 |  4322 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4323 | `				}` |
|        5 |  4324 | `				if( pInstr->iP1 ){` |
|        - |  4325 | `					/* Pre-icrement */` |
|      ! 0 |  4326 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4327 | `				}` |
|        2 |  4328 | `			}` |
|        3 |  4329 | `		}else{` |
|      ! 0 |  4330 | `			if( pInstr->iP1 ){` |
|        - |  4331 | `				/* Pre-increment */` |
|      ! 0 |  4332 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4333 | `					pTos->rVal--;` |
|        - |  4334 | `					/* Try to get an integer representation */` |
|      ! 0 |  4335 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4336 | `				}else{` |
|      ! 0 |  4337 | `					pTos->x.iVal--;` |
|      ! 0 |  4338 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4339 | `				}` |
|      ! 0 |  4340 | `			}` |
|        - |  4341 | `		}` |
|        2 |  4342 | `	}` |
|        5 |  4343 | `	break;` |
|        - |  4344 | `/*` |
|        - |  4345 | ` * UMINUS: * * *` |
|        - |  4346 | ` *` |
|        - |  4347 | ` * Perform a unary minus operation.` |
|        - |  4348 | ` */` |
|    25923 |  4349 | `case PH7_OP_UMINUS:` |
|        - |  4350 | `#ifdef UNTRUST` |
|        - |  4351 | `	if( pTos < pStack ){` |
|        - |  4352 | `		goto Abort;` |
|        - |  4353 | `	}` |
|        - |  4354 | `#endif` |
|        - |  4355 | `	/* Force a numeric (integer,real or both) cast */` |
|    51848 |  4356 | `	PH7_MemObjToNumeric(pTos);` |
|    51848 |  4357 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4358 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4359 | `	}` |
|    51848 |  4360 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    51818 |  4361 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    25908 |  4362 | `	}` |
|    51848 |  4363 | `	break;` |
|        - |  4364 | `/*` |
|        - |  4365 | ` * UPLUS: * * *` |
|        - |  4366 | ` *` |
|        - |  4367 | ` * Perform a unary plus operation.` |
|        - |  4368 | ` */` |
|       18 |  4369 | `case PH7_OP_UPLUS:` |
|        - |  4370 | `#ifdef UNTRUST` |
|        - |  4371 | `	if( pTos < pStack ){` |
|        - |  4372 | `		goto Abort;` |
|        - |  4373 | `	}` |
|        - |  4374 | `#endif` |
|        - |  4375 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4376 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4377 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4378 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4379 | `	}` |
|       37 |  4380 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4381 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4382 | `	}` |
|       37 |  4383 | `	break;` |
|        - |  4384 | `/*` |
|        - |  4385 | ` * OP_LNOT: * * *` |
|        - |  4386 | ` *` |
|        - |  4387 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4388 | ` * with its complement.` |
|        - |  4389 | ` */` |
|    41644 |  4390 | `case PH7_OP_LNOT:` |
|        - |  4391 | `#ifdef UNTRUST` |
|        - |  4392 | `	if( pTos < pStack ){` |
|        - |  4393 | `		goto Abort;` |
|        - |  4394 | `	}` |
|        - |  4395 | `#endif` |
|        - |  4396 | `	/* Force a boolean cast */` |
|    83334 |  4397 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4398 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4399 | `	}` |
|    83334 |  4400 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    83334 |  4401 | `	break;` |
|        - |  4402 | `/*` |
|        - |  4403 | ` * OP_BITNOT: * * *` |
|        - |  4404 | ` *` |
|        - |  4405 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4406 | ` * with its ones-complement.` |
|        - |  4407 | ` */` |
|       13 |  4408 | `case PH7_OP_BITNOT:` |
|        - |  4409 | `#ifdef UNTRUST` |
|        - |  4410 | `	if( pTos < pStack ){` |
|        - |  4411 | `		goto Abort;` |
|        - |  4412 | `	}` |
|        - |  4413 | `#endif` |
|        - |  4414 | `	/* Force an integer cast */` |
|       28 |  4415 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4416 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4417 | `	}` |
|       28 |  4418 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4419 | `	break;` |
|        - |  4420 | `/* OP_MUL * * *` |
|        - |  4421 | ` * OP_MUL_STORE * * *` |
|        - |  4422 | ` *` |
|        - |  4423 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4424 | ` * and push the result back onto the stack.` |
|        - |  4425 | ` */` |
|     1272 |  4426 | `case PH7_OP_MUL:` |
|        - |  4427 | `case PH7_OP_MUL_STORE: {` |
|     2546 |  4428 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4429 | `	/* Force the operand to be numeric */` |
|        - |  4430 | `#ifdef UNTRUST` |
|        - |  4431 | `	if( pNos < pStack ){` |
|        - |  4432 | `		goto Abort;` |
|        - |  4433 | `	}` |
|        - |  4434 | `#endif` |
|     2546 |  4435 | `	PH7_MemObjToNumeric(pTos);` |
|     2546 |  4436 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4437 | `	/* Perform the requested operation */` |
|     2546 |  4438 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4439 | `		/* Floating point arithemic */` |
|        - |  4440 | `		ph7_real a,b,r;` |
|       19 |  4441 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  4442 | `			PH7_MemObjToReal(pTos);` |
|        4 |  4443 | `		}` |
|       19 |  4444 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4445 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4446 | `		}` |
|       19 |  4447 | `		a = pNos->rVal;` |
|       19 |  4448 | `		b = pTos->rVal;` |
|       19 |  4449 | `		r = a * b;` |
|        - |  4450 | `		/* Push the result */` |
|       19 |  4451 | `		pNos->rVal = r;` |
|       19 |  4452 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4453 | `		/* Try to get an integer representation */` |
|       19 |  4454 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  4455 | `	}else{` |
|        - |  4456 | `		/* Integer arithmetic */` |
|        - |  4457 | `		sxi64 a,b,r;` |
|     2528 |  4458 | `		a = pNos->x.iVal;` |
|     2528 |  4459 | `		b = pTos->x.iVal;` |
|     2528 |  4460 | `		r = a * b;` |
|        - |  4461 | `		/* Push the result */` |
|     2528 |  4462 | `		pNos->x.iVal = r;` |
|     2528 |  4463 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4464 | `	}` |
|     2546 |  4465 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4466 | `		ph7_value *pObj;` |
|       32 |  4467 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4468 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  4469 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  4470 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  4471 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  4472 | `		}` |
|       15 |  4473 | `	}` |
|     2546 |  4474 | `	VmPopOperand(&pTos,1);` |
|     2546 |  4475 | `	break;` |
|        - |  4476 | `				 }` |
|        - |  4477 | `/* OP_ADD * * *` |
|        - |  4478 | ` *` |
|        - |  4479 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4480 | ` * and push the result back onto the stack.` |
|        - |  4481 | ` */` |
|      487 |  4482 | `case PH7_OP_ADD:{` |
|      976 |  4483 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4484 | `#ifdef UNTRUST` |
|        - |  4485 | `	if( pNos < pStack ){` |
|        - |  4486 | `		goto Abort;` |
|        - |  4487 | `	}` |
|        - |  4488 | `#endif` |
|        - |  4489 | `	/* Perform the addition */` |
|      976 |  4490 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      976 |  4491 | `	VmPopOperand(&pTos,1);` |
|      976 |  4492 | `	break;` |
|        - |  4493 | `				}` |
|        - |  4494 | `/*` |
|        - |  4495 | ` * OP_ADD_STORE * * *` |
|        - |  4496 | ` *` |
|        - |  4497 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4498 | ` * and push the result back onto the stack.` |
|        - |  4499 | ` */` |
|      497 |  4500 | `case PH7_OP_ADD_STORE:{` |
|      996 |  4501 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4502 | `	ph7_value *pObj;` |
|        - |  4503 | `	sxu32 nIdx;` |
|        - |  4504 | `#ifdef UNTRUST` |
|        - |  4505 | `	if( pNos < pStack ){` |
|        - |  4506 | `		goto Abort;` |
|        - |  4507 | `	}` |
|        - |  4508 | `#endif` |
|        - |  4509 | `	/* Perform the addition */` |
|      996 |  4510 | `	nIdx = pTos->nIdx;` |
|      996 |  4511 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4512 | `	/* Peform the store operation */` |
|      996 |  4513 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4514 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      996 |  4515 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      996 |  4516 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      996 |  4517 | `		PH7_MemObjStore(pTos,pObj);` |
|      497 |  4518 | `	}` |
|        - |  4519 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      996 |  4520 | `	PH7_MemObjStore(pTos,pNos);` |
|      996 |  4521 | `	VmPopOperand(&pTos,1);` |
|      996 |  4522 | `	break;` |
|        - |  4523 | `				}` |
|        - |  4524 | `/* OP_SUB * * *` |
|        - |  4525 | ` *` |
|        - |  4526 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4527 | ` * first (what was next on the stack) from the second (the` |
|        - |  4528 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4529 | ` */` |
|      302 |  4530 | `case PH7_OP_SUB: {` |
|      606 |  4531 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4532 | `#ifdef UNTRUST` |
|        - |  4533 | `	if( pNos < pStack ){` |
|        - |  4534 | `		goto Abort;` |
|        - |  4535 | `	}` |
|        - |  4536 | `#endif` |
|      606 |  4537 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4538 | `		/* Floating point arithemic */` |
|        - |  4539 | `		ph7_real a,b,r;` |
|       95 |  4540 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4541 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4542 | `		}` |
|       95 |  4543 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4544 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4545 | `		}` |
|       95 |  4546 | `		a = pNos->rVal;` |
|       95 |  4547 | `		b = pTos->rVal;` |
|       95 |  4548 | `		r = a - b;` |
|        - |  4549 | `		/* Push the result */` |
|       95 |  4550 | `		pNos->rVal = r;` |
|       95 |  4551 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4552 | `		/* Try to get an integer representation */` |
|       95 |  4553 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4554 | `	}else{` |
|        - |  4555 | `		/* Integer arithmetic */` |
|        - |  4556 | `		sxi64 a,b,r;` |
|      512 |  4557 | `		a = pNos->x.iVal;` |
|      512 |  4558 | `		b = pTos->x.iVal;` |
|      512 |  4559 | `		r = a - b;` |
|        - |  4560 | `		/* Push the result */` |
|      512 |  4561 | `		pNos->x.iVal = r;` |
|      512 |  4562 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4563 | `	}` |
|      606 |  4564 | `	VmPopOperand(&pTos,1);` |
|      606 |  4565 | `	break;` |
|        - |  4566 | `				 }` |
|        - |  4567 | `/* OP_SUB_STORE * * *` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4570 | ` * first (what was next on the stack) from the second (the` |
|        - |  4571 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4572 | ` */` |
|        4 |  4573 | `case PH7_OP_SUB_STORE: {` |
|       10 |  4574 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4575 | `	ph7_value *pObj;` |
|        - |  4576 | `#ifdef UNTRUST` |
|        - |  4577 | `	if( pNos < pStack ){` |
|        - |  4578 | `		goto Abort;` |
|        - |  4579 | `	}` |
|        - |  4580 | `#endif` |
|       10 |  4581 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4582 | `		/* Floating point arithemic */` |
|        - |  4583 | `		ph7_real a,b,r;` |
|      ! 0 |  4584 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4585 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4586 | `		}` |
|      ! 0 |  4587 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4588 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4589 | `		}` |
|      ! 0 |  4590 | `		a = pTos->rVal;` |
|      ! 0 |  4591 | `		b = pNos->rVal;` |
|      ! 0 |  4592 | `		r = a - b;` |
|        - |  4593 | `		/* Push the result */` |
|      ! 0 |  4594 | `		pNos->rVal = r;` |
|      ! 0 |  4595 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4596 | `		/* Try to get an integer representation */` |
|      ! 0 |  4597 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4598 | `	}else{` |
|        - |  4599 | `		/* Integer arithmetic */` |
|        - |  4600 | `		sxi64 a,b,r;` |
|       10 |  4601 | `		a = pTos->x.iVal;` |
|       10 |  4602 | `		b = pNos->x.iVal;` |
|       10 |  4603 | `		r = a - b;` |
|        - |  4604 | `		/* Push the result */` |
|       10 |  4605 | `		pNos->x.iVal = r;` |
|       10 |  4606 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4607 | `	}` |
|       10 |  4608 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4609 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  4610 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  4611 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  4612 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  4613 | `	}` |
|       10 |  4614 | `	VmPopOperand(&pTos,1);` |
|       10 |  4615 | `	break;` |
|        - |  4616 | `				 }` |
|        - |  4617 |  |
|        - |  4618 | `/*` |
|        - |  4619 | ` * OP_MOD * * *` |
|        - |  4620 | ` *` |
|        - |  4621 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4622 | ` * first (what was next on the stack) from the second (the` |
|        - |  4623 | ` * top of the stack) and push the remainder after division` |
|        - |  4624 | ` * onto the stack.` |
|        - |  4625 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4626 | ` */` |
|      307 |  4627 | `case PH7_OP_MOD:{` |
|      616 |  4628 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4629 | `	sxi64 a,b,r;` |
|        - |  4630 | `#ifdef UNTRUST` |
|        - |  4631 | `	if( pNos < pStack ){` |
|        - |  4632 | `		goto Abort;` |
|        - |  4633 | `	}` |
|        - |  4634 | `#endif` |
|        - |  4635 | `	/* Force the operands to be integer */` |
|      616 |  4636 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4637 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4638 | `	}` |
|      616 |  4639 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4640 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4641 | `	}` |
|        - |  4642 | `	/* Perform the requested operation */` |
|      616 |  4643 | `	a = pNos->x.iVal;` |
|      616 |  4644 | `	b = pTos->x.iVal;` |
|      616 |  4645 | `	if( b == 0 ){` |
|        3 |  4646 | `		r = 0;` |
|        3 |  4647 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4648 | `		/* goto Abort; */` |
|        2 |  4649 | `	}else{` |
|      613 |  4650 | `		r = a%b;` |
|        - |  4651 | `	}` |
|        - |  4652 | `	/* Push the result */` |
|      616 |  4653 | `	pNos->x.iVal = r;` |
|      616 |  4654 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4655 | `	VmPopOperand(&pTos,1);` |
|      616 |  4656 | `	break;` |
|        - |  4657 | `				}` |
|        - |  4658 | `/*` |
|        - |  4659 | ` * OP_MOD_STORE * * *` |
|        - |  4660 | ` *` |
|        - |  4661 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4662 | ` * first (what was next on the stack) from the second (the` |
|        - |  4663 | ` * top of the stack) and push the remainder after division` |
|        - |  4664 | ` * onto the stack.` |
|        - |  4665 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4666 | ` */` |
|        1 |  4667 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4668 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4669 | `	ph7_value *pObj;` |
|        - |  4670 | `	sxi64 a,b,r;` |
|        - |  4671 | `#ifdef UNTRUST` |
|        - |  4672 | `	if( pNos < pStack ){` |
|        - |  4673 | `		goto Abort;` |
|        - |  4674 | `	}` |
|        - |  4675 | `#endif` |
|        - |  4676 | `	/* Force the operands to be integer */` |
|        3 |  4677 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4678 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4679 | `	}` |
|        3 |  4680 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4681 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4682 | `	}` |
|        - |  4683 | `	/* Perform the requested operation */` |
|        3 |  4684 | `	a = pTos->x.iVal;` |
|        3 |  4685 | `	b = pNos->x.iVal;` |
|        3 |  4686 | `	if( b == 0 ){` |
|      ! 0 |  4687 | `		r = 0;` |
|      ! 0 |  4688 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4689 | `		/* goto Abort; */` |
|      ! 0 |  4690 | `	}else{` |
|        3 |  4691 | `		r = a%b;` |
|        - |  4692 | `	}` |
|        - |  4693 | `	/* Push the result */` |
|        3 |  4694 | `	pNos->x.iVal = r;` |
|        3 |  4695 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4696 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4697 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4698 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4699 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  4700 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4701 | `	}` |
|        3 |  4702 | `	VmPopOperand(&pTos,1);` |
|        3 |  4703 | `	break;` |
|        - |  4704 | `				}` |
|        - |  4705 | `/*` |
|        - |  4706 | ` * OP_DIV * * *` |
|        - |  4707 | ` *` |
|        - |  4708 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4709 | ` * first (what was next on the stack) from the second (the` |
|        - |  4710 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4711 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4712 | ` */` |
|       30 |  4713 | `case PH7_OP_DIV:{` |
|       62 |  4714 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4715 | `	ph7_real a,b,r;` |
|        - |  4716 | `#ifdef UNTRUST` |
|        - |  4717 | `	if( pNos < pStack ){` |
|        - |  4718 | `		goto Abort;` |
|        - |  4719 | `	}` |
|        - |  4720 | `#endif` |
|        - |  4721 | `	/* Force the operands to be real */` |
|       62 |  4722 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4723 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4724 | `	}` |
|       62 |  4725 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4726 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4727 | `	}` |
|        - |  4728 | `	/* Perform the requested operation */` |
|       62 |  4729 | `	a = pNos->rVal;` |
|       62 |  4730 | `	b = pTos->rVal;` |
|       62 |  4731 | `	if( b == 0 ){` |
|        - |  4732 | `		/* Division by zero */` |
|        3 |  4733 | `		pNos->rVal = 0;` |
|        3 |  4734 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4735 | `		/* goto Abort; */` |
|        2 |  4736 | `	}else{` |
|       59 |  4737 | `		r = a/b;` |
|        - |  4738 | `		/* Push the result */` |
|       59 |  4739 | `		pNos->rVal = r;` |
|       59 |  4740 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4741 | `		/* Try to get an integer representation */` |
|       59 |  4742 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4743 | `	}` |
|       62 |  4744 | `	VmPopOperand(&pTos,1);` |
|       62 |  4745 | `	break;` |
|        - |  4746 | `				}` |
|        - |  4747 | `/*` |
|        - |  4748 | ` * OP_DIV_STORE * * *` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4751 | ` * first (what was next on the stack) from the second (the` |
|        - |  4752 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4753 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4754 | ` */` |
|        2 |  4755 | `case PH7_OP_DIV_STORE:{` |
|        5 |  4756 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4757 | `	ph7_value *pObj;` |
|        - |  4758 | `	ph7_real a,b,r;` |
|        - |  4759 | `#ifdef UNTRUST` |
|        - |  4760 | `	if( pNos < pStack ){` |
|        - |  4761 | `		goto Abort;` |
|        - |  4762 | `	}` |
|        - |  4763 | `#endif` |
|        - |  4764 | `	/* Force the operands to be real */` |
|        5 |  4765 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4766 | `		PH7_MemObjToReal(pTos);` |
|        2 |  4767 | `	}` |
|        5 |  4768 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4769 | `		PH7_MemObjToReal(pNos);` |
|        2 |  4770 | `	}` |
|        - |  4771 | `	/* Perform the requested operation */` |
|        5 |  4772 | `	a = pTos->rVal;` |
|        5 |  4773 | `	b = pNos->rVal;` |
|        5 |  4774 | `	if( b == 0 ){` |
|        - |  4775 | `		/* Division by zero */` |
|      ! 0 |  4776 | `		r = 0;` |
|      ! 0 |  4777 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4778 | `		/* goto Abort; */` |
|      ! 0 |  4779 | `	}else{` |
|        5 |  4780 | `		r = a/b;` |
|        - |  4781 | `		/* Push the result */` |
|        5 |  4782 | `		pNos->rVal = r;` |
|        5 |  4783 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4784 | `		/* Try to get an integer representation */` |
|        5 |  4785 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4786 | `	}` |
|        5 |  4787 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4788 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4789 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4790 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  4791 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4792 | `	}` |
|        5 |  4793 | `	VmPopOperand(&pTos,1);` |
|        5 |  4794 | `	break;` |
|        - |  4795 | `				}` |
|        - |  4796 | `/* OP_BAND * * *` |
|        - |  4797 | ` *` |
|        - |  4798 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4799 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4800 | ` * two elements.` |
|        - |  4801 | `*/` |
|        - |  4802 | `/* OP_BOR * * *` |
|        - |  4803 | ` *` |
|        - |  4804 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4805 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4806 | ` * two elements.` |
|        - |  4807 | ` */` |
|        - |  4808 | `/* OP_BXOR * * *` |
|        - |  4809 | ` *` |
|        - |  4810 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4811 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4812 | ` * two elements.` |
|        - |  4813 | ` */` |
|       44 |  4814 | `case PH7_OP_BAND:` |
|        - |  4815 | `case PH7_OP_BOR:` |
|        - |  4816 | `case PH7_OP_BXOR:{` |
|       90 |  4817 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4818 | `	sxi64 a,b,r;` |
|        - |  4819 | `#ifdef UNTRUST` |
|        - |  4820 | `	if( pNos < pStack ){` |
|        - |  4821 | `		goto Abort;` |
|        - |  4822 | `	}` |
|        - |  4823 | `#endif` |
|        - |  4824 | `	/* Force the operands to be integer */` |
|       90 |  4825 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4826 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4827 | `	}` |
|       90 |  4828 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4829 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4830 | `	}` |
|        - |  4831 | `	/* Perform the requested operation */` |
|       90 |  4832 | `	a = pNos->x.iVal;` |
|       90 |  4833 | `	b = pTos->x.iVal;` |
|       90 |  4834 | `	switch(pInstr->iOp){` |
|        7 |  4835 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4836 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4837 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4838 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4839 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4840 | `	case PH7_OP_BAND:` |
|       62 |  4841 | `	default:          r = a&b; break;` |
|        - |  4842 | `	}` |
|        - |  4843 | `	/* Push the result */` |
|       90 |  4844 | `	pNos->x.iVal = r;` |
|       90 |  4845 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4846 | `	VmPopOperand(&pTos,1);` |
|       90 |  4847 | `	break;` |
|        - |  4848 | `				 }` |
|        - |  4849 | `/* OP_BAND_STORE * * *` |
|        - |  4850 | ` *` |
|        - |  4851 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4852 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4853 | ` * two elements.` |
|        - |  4854 | `*/` |
|        - |  4855 | `/* OP_BOR_STORE * * *` |
|        - |  4856 | ` *` |
|        - |  4857 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4858 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4859 | ` * two elements.` |
|        - |  4860 | ` */` |
|        - |  4861 | `/* OP_BXOR_STORE * * *` |
|        - |  4862 | ` *` |
|        - |  4863 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4864 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4865 | ` * two elements.` |
|        - |  4866 | ` */` |
|       10 |  4867 | `case PH7_OP_BAND_STORE:` |
|        - |  4868 | `case PH7_OP_BOR_STORE:` |
|        - |  4869 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4870 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4871 | `	ph7_value *pObj;` |
|        - |  4872 | `	sxi64 a,b,r;` |
|        - |  4873 | `#ifdef UNTRUST` |
|        - |  4874 | `	if( pNos < pStack ){` |
|        - |  4875 | `		goto Abort;` |
|        - |  4876 | `	}` |
|        - |  4877 | `#endif` |
|        - |  4878 | `	/* Force the operands to be integer */` |
|       21 |  4879 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4880 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4881 | `	}` |
|       21 |  4882 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4883 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4884 | `	}` |
|        - |  4885 | `	/* Perform the requested operation */` |
|       21 |  4886 | `	a = pTos->x.iVal;` |
|       21 |  4887 | `	b = pNos->x.iVal;` |
|       21 |  4888 | `	switch(pInstr->iOp){` |
|        3 |  4889 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4890 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4891 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4892 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4893 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4894 | `	case PH7_OP_BAND:` |
|        7 |  4895 | `	default:          r = a&b; break;` |
|        - |  4896 | `	}` |
|        - |  4897 | `	/* Push the result */` |
|       21 |  4898 | `	pNos->x.iVal = r;` |
|       21 |  4899 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4900 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4901 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4902 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4903 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  4904 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4905 | `	}` |
|       21 |  4906 | `	VmPopOperand(&pTos,1);` |
|       21 |  4907 | `	break;` |
|        - |  4908 | `				 }` |
|        - |  4909 | `/* OP_SHL * * *` |
|        - |  4910 | ` *` |
|        - |  4911 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4912 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4913 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4914 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4915 | ` */` |
|        - |  4916 | `/* OP_SHR * * *` |
|        - |  4917 | ` *` |
|        - |  4918 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4919 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4920 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4921 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4922 | ` */` |
|       12 |  4923 | `case PH7_OP_SHL:` |
|        - |  4924 | `case PH7_OP_SHR: {` |
|       25 |  4925 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4926 | `	sxi64 a,r;` |
|        - |  4927 | `	sxi32 b;` |
|        - |  4928 | `#ifdef UNTRUST` |
|        - |  4929 | `	if( pNos < pStack ){` |
|        - |  4930 | `		goto Abort;` |
|        - |  4931 | `	}` |
|        - |  4932 | `#endif` |
|        - |  4933 | `	/* Force the operands to be integer */` |
|       25 |  4934 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4935 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4936 | `	}` |
|       25 |  4937 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4938 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4939 | `	}` |
|        - |  4940 | `	/* Perform the requested operation */` |
|       25 |  4941 | `	a = pNos->x.iVal;` |
|       25 |  4942 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4943 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4944 | `		r = a << b;` |
|        8 |  4945 | `	}else{` |
|       11 |  4946 | `		r = a >> b;` |
|        - |  4947 | `	}` |
|        - |  4948 | `	/* Push the result */` |
|       25 |  4949 | `	pNos->x.iVal = r;` |
|       25 |  4950 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4951 | `	VmPopOperand(&pTos,1);` |
|       25 |  4952 | `	break;` |
|        - |  4953 | `				 }` |
|        - |  4954 | `/*  OP_SHL_STORE * * *` |
|        - |  4955 | ` *` |
|        - |  4956 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4957 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4958 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4959 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4960 | ` */` |
|        - |  4961 | `/* OP_SHR_STORE * * *` |
|        - |  4962 | ` *` |
|        - |  4963 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4964 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4965 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4966 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4967 | ` */` |
|        9 |  4968 | `case PH7_OP_SHL_STORE:` |
|        - |  4969 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4970 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4971 | `	ph7_value *pObj;` |
|        - |  4972 | `	sxi64 a,r;` |
|        - |  4973 | `	sxi32 b;` |
|        - |  4974 | `#ifdef UNTRUST` |
|        - |  4975 | `	if( pNos < pStack ){` |
|        - |  4976 | `		goto Abort;` |
|        - |  4977 | `	}` |
|        - |  4978 | `#endif` |
|        - |  4979 | `	/* Force the operands to be integer */` |
|       19 |  4980 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4981 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4982 | `	}` |
|       19 |  4983 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4984 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4985 | `	}` |
|        - |  4986 | `	/* Perform the requested operation */` |
|       19 |  4987 | `	a = pTos->x.iVal;` |
|       19 |  4988 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4989 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4990 | `		r = a << b;` |
|        5 |  4991 | `	}else{` |
|       11 |  4992 | `		r = a >> b;` |
|        - |  4993 | `	}` |
|        - |  4994 | `	/* Push the result */` |
|       19 |  4995 | `	pNos->x.iVal = r;` |
|       19 |  4996 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4997 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4998 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4999 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5000 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5001 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5002 | `	}` |
|       19 |  5003 | `	VmPopOperand(&pTos,1);` |
|       19 |  5004 | `	break;` |
|        - |  5005 | `				 }` |
|        - |  5006 | `/* CAT:  P1 * *` |
|        - |  5007 | ` *` |
|        - |  5008 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5009 | ` * back.` |
|        - |  5010 | ` */` |
|    66107 |  5011 | `case PH7_OP_CAT:{` |
|        - |  5012 | `	ph7_value *pNos,*pCur;` |
|   132216 |  5013 | `	if( pInstr->iP1 < 1 ){` |
|   105044 |  5014 | `		pNos = &pTos[-1];` |
|    52523 |  5015 | `	}else{` |
|    27174 |  5016 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5017 | `	}` |
|        - |  5018 | `#ifdef UNTRUST` |
|        - |  5019 | `	if( pNos < pStack ){` |
|        - |  5020 | `		goto Abort;` |
|        - |  5021 | `	}` |
|        - |  5022 | `#endif` |
|        - |  5023 | `	/* Force a string cast */` |
|   132216 |  5024 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5025 | `		PH7_MemObjToString(pNos);` |
|      817 |  5026 | `	}` |
|   132216 |  5027 | `	pCur = &pNos[1];` |
|   266680 |  5028 | `	while( pCur <= pTos ){` |
|   134466 |  5029 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50758 |  5030 | `			PH7_MemObjToString(pCur);` |
|    25378 |  5031 | `		}` |
|        - |  5032 | `		/* Perform the concatenation */` |
|   134466 |  5033 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   134424 |  5034 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    67211 |  5035 | `		}` |
|   134466 |  5036 | `		SyBlobRelease(&pCur->sBlob);` |
|   134466 |  5037 | `		pCur++;` |
|        2 |  5038 | `	}` |
|   132216 |  5039 | `	pTos = pNos;` |
|   132216 |  5040 | `	break;` |
|        - |  5041 | `				}` |
|        - |  5042 | `/*  CAT_STORE: * * *` |
|        - |  5043 | ` *` |
|        - |  5044 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5045 | ` * back.` |
|        - |  5046 | ` */` |
|     3582 |  5047 | `case PH7_OP_CAT_STORE:{` |
|     7166 |  5048 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5049 | `	ph7_value *pObj;` |
|        - |  5050 | `#ifdef UNTRUST` |
|        - |  5051 | `	if( pNos < pStack ){` |
|        - |  5052 | `		goto Abort;` |
|        - |  5053 | `	}` |
|        - |  5054 | `#endif` |
|     7166 |  5055 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5056 | `		/* Force a string cast */` |
|        3 |  5057 | `		PH7_MemObjToString(pTos);` |
|        1 |  5058 | `	}` |
|     7166 |  5059 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5060 | `		/* Force a string cast */` |
|      ! 0 |  5061 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5062 | `	}` |
|        - |  5063 | `	/* Perform the concatenation (Reverse order) */` |
|     7166 |  5064 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7166 |  5065 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3582 |  5066 | `	}` |
|        - |  5067 | `	/* Perform the store operation */` |
|     7166 |  5068 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5069 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7166 |  5070 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7166 |  5071 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7164 |  5072 | `		PH7_MemObjStore(pTos,pObj);` |
|     3581 |  5073 | `	}` |
|     7164 |  5074 | `	PH7_MemObjStore(pTos,pNos);` |
|     7164 |  5075 | `	VmPopOperand(&pTos,1);` |
|     7164 |  5076 | `	break;` |
|        - |  5077 | `				}` |
|        - |  5078 | `/* OP_AND: * * *` |
|        - |  5079 | ` *` |
|        - |  5080 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5081 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5082 | ` * stack.` |
|        - |  5083 | ` */` |
|        - |  5084 | `/* OP_OR: * * *` |
|        - |  5085 | ` *` |
|        - |  5086 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5087 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5088 | ` * stack.` |
|        - |  5089 | ` */` |
|    99168 |  5090 | `case PH7_OP_LAND:` |
|        - |  5091 | `case PH7_OP_LOR: {` |
|   198382 |  5092 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5093 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5094 | `#ifdef UNTRUST` |
|        - |  5095 | `	if( pNos < pStack ){` |
|        - |  5096 | `		goto Abort;` |
|        - |  5097 | `	}` |
|        - |  5098 | `#endif` |
|        - |  5099 | `	/* Force a boolean cast */` |
|   198382 |  5100 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5101 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5102 | `	}` |
|   198382 |  5103 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5104 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5105 | `	}` |
|   198382 |  5106 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   198382 |  5107 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   198382 |  5108 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5109 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    91534 |  5110 | `		v1 = and_logic[v1*3+v2];` |
|    45790 |  5111 | `	}else{` |
|        - |  5112 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   106850 |  5113 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5114 | `	}` |
|   198382 |  5115 | `	if( v1 == 2 ){` |
|      ! 0 |  5116 | `		v1 = 1;` |
|      ! 0 |  5117 | `	}` |
|   198382 |  5118 | `	VmPopOperand(&pTos,1);` |
|   198382 |  5119 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   198382 |  5120 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   198382 |  5121 | `	break;` |
|        - |  5122 | `				 }` |
|        - |  5123 | `/*` |
|        - |  5124 | ` * OP_NULLC: * * *` |
|        - |  5125 | ` * Null coalescing operator '??'.` |
|        - |  5126 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5127 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5128 | ` */` |
|        - |  5129 | `/*` |
|        - |  5130 | ` * OP_NULLC: * P2 *` |
|        - |  5131 | ` * Short-circuit null coalescing '??'.` |
|        - |  5132 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5133 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5134 | ` */` |
|       19 |  5135 | `case PH7_OP_NULLC: {` |
|        - |  5136 | `#ifdef UNTRUST` |
|        - |  5137 | `	if( pTos < pStack ){` |
|        - |  5138 | `		goto Abort;` |
|        - |  5139 | `	}` |
|        - |  5140 | `#endif` |
|       40 |  5141 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5142 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  5143 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  5144 | `	}else{` |
|        - |  5145 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  5146 | `		VmPopOperand(&pTos, 1);` |
|        - |  5147 | `	}` |
|       40 |  5148 | `	break;` |
|        - |  5149 |  |
|        - |  5150 | `/*` |
|        - |  5151 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5152 | ` * Null coalescing assignment short-circuit.` |
|        - |  5153 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5154 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5155 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5156 | ` */` |
|       23 |  5157 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5158 | `#ifdef UNTRUST` |
|        - |  5159 | `	if( pTos < pStack ){` |
|        - |  5160 | `		goto Abort;` |
|        - |  5161 | `	}` |
|        - |  5162 | `#endif` |
|       47 |  5163 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5164 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5165 | `	}` |
|       47 |  5166 | `	break;` |
|        - |  5167 |  |
|        - |  5168 | `/*` |
|        - |  5169 | ` * OP_NULLC_STORE: * * *` |
|        - |  5170 | ` * Null coalescing assignment store.` |
|        - |  5171 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5172 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5173 | ` * expression result.` |
|        - |  5174 | ` */` |
|       14 |  5175 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5176 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5177 | `	ph7_value *pObj;` |
|        - |  5178 | `	sxu32 nIdx;` |
|        - |  5179 | `#ifdef UNTRUST` |
|        - |  5180 | `	if( pNos < pStack ){` |
|        - |  5181 | `		goto Abort;` |
|        - |  5182 | `	}` |
|        - |  5183 | `#endif` |
|       29 |  5184 | `	nIdx = pNos->nIdx;` |
|       29 |  5185 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5186 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5187 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5188 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5189 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5190 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5191 | `	}` |
|       29 |  5192 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5193 | `	VmPopOperand(&pTos,1);` |
|       29 |  5194 | `	break;` |
|        - |  5195 |  |
|        - |  5196 | `/*` |
|        - |  5197 | ` * OP_SPREAD: * * *` |
|        - |  5198 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5199 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5200 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5201 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5202 | ` */` |
|        7 |  5203 | `case PH7_OP_SPREAD: {` |
|        - |  5204 | `#ifdef UNTRUST` |
|        - |  5205 | `	if( pTos < pStack ){` |
|        - |  5206 | `		goto Abort;` |
|        - |  5207 | `	}` |
|        - |  5208 | `#endif` |
|       15 |  5209 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  5210 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  5211 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  5212 | `		if( nEntry == 0 ){` |
|        - |  5213 | `			/* Empty array — remove from stack */` |
|        3 |  5214 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5215 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  5216 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5217 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5218 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5219 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5220 | `				VM_STACK_GUARD);` |
|      ! 0 |  5221 | `		}else{` |
|        - |  5222 | `			ph7_hashmap_node *pNode2;` |
|        - |  5223 | `			ph7_value *pElem;` |
|        - |  5224 | `			sxu32 i;` |
|        - |  5225 | `			/* Overwrite TOS with first element */` |
|       13 |  5226 | `			pNode2 = pMap->pFirst;` |
|       13 |  5227 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  5228 | `			PH7_MemObjRelease(pTos);` |
|       13 |  5229 | `			if( pElem ){` |
|       13 |  5230 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  5231 | `			}` |
|       13 |  5232 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5233 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5234 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  5235 | `			pNode2 = pNode2->pPrev;` |
|        - |  5236 | `			/* Push remaining elements */` |
|       33 |  5237 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  5238 | `				pTos++;` |
|       21 |  5239 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  5240 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  5241 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  5242 | `				if( pElem ){` |
|       21 |  5243 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  5244 | `				}` |
|       21 |  5245 | `				pNode2 = pNode2->pPrev;` |
|       11 |  5246 | `			}` |
|       13 |  5247 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5248 | `		}` |
|        7 |  5249 | `	}` |
|        - |  5250 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  5251 | `	break;` |
|        - |  5252 |  |
|        - |  5253 | `/* OP_LXOR: * * *` |
|        - |  5254 | ` *` |
|        - |  5255 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5256 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5257 | ` * stack.` |
|        - |  5258 | ` * According to the PHP language reference manual:` |
|        - |  5259 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5260 | ` *  TRUE,but not both.` |
|        - |  5261 | ` */` |
|        5 |  5262 | `case PH7_OP_LXOR:{` |
|       11 |  5263 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5264 | `	sxi32 v = 0;` |
|        - |  5265 | `#ifdef UNTRUST` |
|        - |  5266 | `	if( pNos < pStack ){` |
|        - |  5267 | `		goto Abort;` |
|        - |  5268 | `	}` |
|        - |  5269 | `#endif` |
|        - |  5270 | `	/* Force a boolean cast */` |
|       11 |  5271 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5272 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5273 | `	}` |
|       11 |  5274 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5275 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5276 | `	}` |
|       11 |  5277 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5278 | `		v = 1;` |
|        3 |  5279 | `	}` |
|       11 |  5280 | `	VmPopOperand(&pTos,1);` |
|       11 |  5281 | `	pTos->x.iVal = v;` |
|       11 |  5282 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5283 | `	break;` |
|        - |  5284 | `				 }` |
|        - |  5285 | `/* OP_EQ P1 P2 P3` |
|        - |  5286 | ` *` |
|        - |  5287 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5288 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5289 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5290 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5291 | ` */` |
|        - |  5292 | `/* OP_NEQ P1 P2 P3` |
|        - |  5293 | ` *` |
|        - |  5294 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5295 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5296 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5297 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5298 | ` */` |
|     4164 |  5299 | `case PH7_OP_EQ:` |
|        - |  5300 | `case PH7_OP_NEQ: {` |
|     8330 |  5301 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5302 | `	/* Perform the comparison and act accordingly */` |
|        - |  5303 | `#ifdef UNTRUST` |
|        - |  5304 | `	if( pNos < pStack ){` |
|        - |  5305 | `		goto Abort;` |
|        - |  5306 | `	}` |
|        - |  5307 | `#endif` |
|     8330 |  5308 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8330 |  5309 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5310 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8321 |  5311 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8286 |  5312 | `		rc = rc == 0;` |
|     4144 |  5313 | `	}else{` |
|       28 |  5314 | `		rc = rc != 0;` |
|        - |  5315 | `	}` |
|     8330 |  5316 | `	VmPopOperand(&pTos,1);` |
|     8330 |  5317 | `	if( !pInstr->iP2 ){` |
|        - |  5318 | `		/* Push comparison result without taking the jump */` |
|     8330 |  5319 | `		PH7_MemObjRelease(pTos);` |
|     8330 |  5320 | `		pTos->x.iVal = rc;` |
|        - |  5321 | `		/* Invalidate any prior representation */` |
|     8330 |  5322 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4166 |  5323 | `	}else{` |
|      ! 0 |  5324 | `		if( rc ){` |
|        - |  5325 | `			/* Jump to the desired location */` |
|      ! 0 |  5326 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5327 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5328 | `		}` |
|        - |  5329 | `	}` |
|     8330 |  5330 | `	break;` |
|        - |  5331 | `				 }` |
|        - |  5332 | `/* OP_TEQ P1 P2 *` |
|        - |  5333 | ` *` |
|        - |  5334 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5335 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5336 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5337 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5338 | ` */` |
|   143075 |  5339 | `case PH7_OP_TEQ: {` |
|   286152 |  5340 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5341 | `	/* Perform the comparison and act accordingly */` |
|        - |  5342 | `#ifdef UNTRUST` |
|        - |  5343 | `	if( pNos < pStack ){` |
|        - |  5344 | `		goto Abort;` |
|        - |  5345 | `	}` |
|        - |  5346 | `#endif` |
|   286152 |  5347 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   286152 |  5348 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5349 | `		rc = 0;` |
|        2 |  5350 | `	}else{` |
|   286150 |  5351 | `		rc = rc == 0;` |
|        - |  5352 | `	}` |
|   286152 |  5353 | `	VmPopOperand(&pTos,1);` |
|   286152 |  5354 | `	if( !pInstr->iP2 ){` |
|        - |  5355 | `		/* Push comparison result without taking the jump */` |
|   286152 |  5356 | `		PH7_MemObjRelease(pTos);` |
|   286152 |  5357 | `		pTos->x.iVal = rc;` |
|        - |  5358 | `		/* Invalidate any prior representation */` |
|   286152 |  5359 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   143077 |  5360 | `	}else{` |
|      ! 0 |  5361 | `		if( rc ){` |
|        - |  5362 | `			/* Jump to the desired location */` |
|      ! 0 |  5363 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5364 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5365 | `		}` |
|        - |  5366 | `	}` |
|   286152 |  5367 | `	break;` |
|        - |  5368 | `				 }` |
|        - |  5369 | `/* OP_TNE P1 P2 *` |
|        - |  5370 | ` *` |
|        - |  5371 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5372 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5373 | ` * instruction.` |
|        - |  5374 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5375 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5376 | ` *` |
|        - |  5377 | ` */` |
|   110460 |  5378 | `case PH7_OP_TNE: {` |
|   220922 |  5379 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5380 | `	/* Perform the comparison and act accordingly */` |
|        - |  5381 | `#ifdef UNTRUST` |
|        - |  5382 | `	if( pNos < pStack ){` |
|        - |  5383 | `		goto Abort;` |
|        - |  5384 | `	}` |
|        - |  5385 | `#endif` |
|   220922 |  5386 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   220922 |  5387 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5388 | `		rc = 1;` |
|        2 |  5389 | `	}else{` |
|   220920 |  5390 | `		rc = rc != 0;` |
|        - |  5391 | `	}` |
|   220922 |  5392 | `	VmPopOperand(&pTos,1);` |
|   220922 |  5393 | `	if( !pInstr->iP2 ){` |
|        - |  5394 | `		/* Push comparison result without taking the jump */` |
|   220922 |  5395 | `		PH7_MemObjRelease(pTos);` |
|   220922 |  5396 | `		pTos->x.iVal = rc;` |
|        - |  5397 | `		/* Invalidate any prior representation */` |
|   220922 |  5398 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   110462 |  5399 | `	}else{` |
|      ! 0 |  5400 | `		if( rc ){` |
|        - |  5401 | `			/* Jump to the desired location */` |
|      ! 0 |  5402 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5403 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5404 | `		}` |
|        - |  5405 | `	}` |
|   220922 |  5406 | `	break;` |
|        - |  5407 | `				 }` |
|        - |  5408 | `/* OP_LT P1 P2 P3` |
|        - |  5409 | ` *` |
|        - |  5410 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5411 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5412 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5413 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5414 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5415 | ` *` |
|        - |  5416 | ` */` |
|        - |  5417 | `/* OP_LE P1 P2 P3` |
|        - |  5418 | ` *` |
|        - |  5419 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5420 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5421 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5422 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5423 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5424 | ` *` |
|        - |  5425 | ` */` |
|   105573 |  5426 | `case PH7_OP_LT:` |
|        - |  5427 | `case PH7_OP_LE: {` |
|   211192 |  5428 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5429 | `	/* Perform the comparison and act accordingly */` |
|        - |  5430 | `#ifdef UNTRUST` |
|        - |  5431 | `	if( pNos < pStack ){` |
|        - |  5432 | `		goto Abort;` |
|        - |  5433 | `	}` |
|        - |  5434 | `#endif` |
|   211192 |  5435 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   211192 |  5436 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5437 | `		rc = 0;` |
|   211188 |  5438 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      558 |  5439 | `		rc = rc < 1;` |
|      280 |  5440 | `	}else{` |
|   210628 |  5441 | `		rc = rc < 0;` |
|        - |  5442 | `	}` |
|   211192 |  5443 | `	VmPopOperand(&pTos,1);` |
|   211192 |  5444 | `	if( !pInstr->iP2 ){` |
|        - |  5445 | `		/* Push comparison result without taking the jump */` |
|   211192 |  5446 | `		PH7_MemObjRelease(pTos);` |
|   211192 |  5447 | `		pTos->x.iVal = rc;` |
|        - |  5448 | `		/* Invalidate any prior representation */` |
|   211192 |  5449 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   105619 |  5450 | `	}else{` |
|      ! 0 |  5451 | `		if( rc ){` |
|        - |  5452 | `			/* Jump to the desired location */` |
|      ! 0 |  5453 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5454 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5455 | `		}` |
|        - |  5456 | `	}` |
|   211192 |  5457 | `	break;` |
|        - |  5458 | `				}` |
|        - |  5459 | `/* OP_GT P1 P2 P3` |
|        - |  5460 | ` *` |
|        - |  5461 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5462 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5463 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5464 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5465 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5466 | ` *` |
|        - |  5467 | ` */` |
|        - |  5468 | `/* OP_GE P1 P2 P3` |
|        - |  5469 | ` *` |
|        - |  5470 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5471 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5472 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5473 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5474 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5475 | ` *` |
|        - |  5476 | ` */` |
|    50848 |  5477 | `case PH7_OP_GT:` |
|        - |  5478 | `case PH7_OP_GE: {` |
|   101698 |  5479 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5480 | `	/* Perform the comparison and act accordingly */` |
|        - |  5481 | `#ifdef UNTRUST` |
|        - |  5482 | `	if( pNos < pStack ){` |
|        - |  5483 | `		goto Abort;` |
|        - |  5484 | `	}` |
|        - |  5485 | `#endif` |
|   101698 |  5486 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   101698 |  5487 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5488 | `		rc = 0;` |
|   101694 |  5489 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   101536 |  5490 | `		rc = rc >= 0;` |
|    50769 |  5491 | `	}else{` |
|      156 |  5492 | `		rc = rc > 0;` |
|        - |  5493 | `	}` |
|   101698 |  5494 | `	VmPopOperand(&pTos,1);` |
|   101698 |  5495 | `	if( !pInstr->iP2 ){` |
|        - |  5496 | `		/* Push comparison result without taking the jump */` |
|   101698 |  5497 | `		PH7_MemObjRelease(pTos);` |
|   101698 |  5498 | `		pTos->x.iVal = rc;` |
|        - |  5499 | `		/* Invalidate any prior representation */` |
|   101698 |  5500 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    50850 |  5501 | `	}else{` |
|      ! 0 |  5502 | `		if( rc ){` |
|        - |  5503 | `			/* Jump to the desired location */` |
|      ! 0 |  5504 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5505 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5506 | `		}` |
|        - |  5507 | `	}` |
|   101698 |  5508 | `	break;` |
|        - |  5509 | `				}` |
|        - |  5510 | `/* OP_SPACESHIP * * *` |
|        - |  5511 | ` *` |
|        - |  5512 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5513 | ` *   -1 if left < right` |
|        - |  5514 | ` *    0 if left == right` |
|        - |  5515 | ` *    1 if left > right` |
|        - |  5516 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5517 | ` */` |
|       25 |  5518 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5519 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5520 | `#ifdef UNTRUST` |
|        - |  5521 | `	if( pNos < pStack ){` |
|        - |  5522 | `		goto Abort;` |
|        - |  5523 | `	}` |
|        - |  5524 | `#endif` |
|       51 |  5525 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5526 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5527 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5528 | `		rc = 1;` |
|        4 |  5529 | `	}else{` |
|        - |  5530 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5531 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5532 | `	}` |
|       51 |  5533 | `	VmPopOperand(&pTos,1);` |
|       51 |  5534 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5535 | `	pTos->x.iVal = rc;` |
|       51 |  5536 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5537 | `	break;` |
|        - |  5538 | `				}` |
|        - |  5539 | `/* OP_SEQ P1 P2 *` |
|        - |  5540 | ` * Strict string comparison.` |
|        - |  5541 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5542 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5543 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5544 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5545 | ` * use PH7_OP_EQ.` |
|        - |  5546 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5547 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5548 | ` */` |
|        - |  5549 | `/* OP_SNE P1 P2 *` |
|        - |  5550 | ` * Strict string comparison.` |
|        - |  5551 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5552 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5553 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5554 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5555 | ` * use PH7_OP_EQ.` |
|        - |  5556 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5557 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5558 | ` */` |
|       18 |  5559 | `case PH7_OP_SEQ:` |
|        - |  5560 | `case PH7_OP_SNE: {` |
|       38 |  5561 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5562 | `	SyString s1,s2;` |
|        - |  5563 | `	/* Perform the comparison and act accordingly */` |
|        - |  5564 | `#ifdef UNTRUST` |
|        - |  5565 | `	if( pNos < pStack ){` |
|        - |  5566 | `		goto Abort;` |
|        - |  5567 | `	}` |
|        - |  5568 | `#endif` |
|        - |  5569 | `	/* Force a string cast */` |
|       38 |  5570 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5571 | `		PH7_MemObjToString(pTos);` |
|        2 |  5572 | `	}` |
|       38 |  5573 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5574 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5575 | `	}` |
|       38 |  5576 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5577 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5578 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5579 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5580 | `		rc = rc != 0;` |
|      ! 0 |  5581 | `	}else{` |
|       38 |  5582 | `		rc = rc == 0;` |
|        - |  5583 | `	}` |
|       38 |  5584 | `	VmPopOperand(&pTos,1);` |
|       38 |  5585 | `	if( !pInstr->iP2 ){` |
|        - |  5586 | `		/* Push comparison result without taking the jump */` |
|       38 |  5587 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5588 | `		pTos->x.iVal = rc;` |
|        - |  5589 | `		/* Invalidate any prior representation */` |
|       38 |  5590 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5591 | `	}else{` |
|      ! 0 |  5592 | `		if( rc ){` |
|        - |  5593 | `			/* Jump to the desired location */` |
|      ! 0 |  5594 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5595 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5596 | `		}` |
|        - |  5597 | `	}` |
|       38 |  5598 | `	break;` |
|        - |  5599 | `				 }` |
|        - |  5600 | `/*` |
|        - |  5601 | ` * OP_LOAD_REF * * *` |
|        - |  5602 | ` * Push the index of a referenced object on the stack.` |
|        - |  5603 | ` */` |
|       57 |  5604 | `case PH7_OP_LOAD_REF: {` |
|        - |  5605 | `	sxu32 nIdx;` |
|        - |  5606 | `#ifdef UNTRUST` |
|        - |  5607 | `	if( pTos < pStack ){` |
|        - |  5608 | `		goto Abort;` |
|        - |  5609 | `	}` |
|        - |  5610 | `#endif` |
|        - |  5611 | `	/* Extract memory object index */` |
|      115 |  5612 | `	nIdx = pTos->nIdx;` |
|      115 |  5613 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5614 | `		/* Nullify the object */` |
|       95 |  5615 | `		PH7_MemObjRelease(pTos);` |
|        - |  5616 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5617 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5618 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5619 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5620 | `	}` |
|      115 |  5621 | `	break;` |
|        - |  5622 | `					  }` |
|        - |  5623 | `/*` |
|        - |  5624 | ` * OP_STORE_REF * * P3` |
|        - |  5625 | ` * Perform an assignment operation by reference.` |
|        - |  5626 | ` */` |
|       16 |  5627 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5628 | `	 SyString sName = { 0 , 0 };` |
|        - |  5629 | `	 VmFrame *pFrameLocal;` |
|        - |  5630 | `	SyHashEntry *pEntry;` |
|        - |  5631 | `	sxu32 nIdx;` |
|        - |  5632 | `#ifdef UNTRUST` |
|        - |  5633 | `	if( pTos < pStack ){` |
|        - |  5634 | `		goto Abort;` |
|        - |  5635 | `	}` |
|        - |  5636 | `#endif` |
|       34 |  5637 | `	if( pInstr->p3 == 0 ){` |
|        - |  5638 | `		char *zName;` |
|        - |  5639 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5640 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5641 | `			/* Force a string cast */` |
|      ! 0 |  5642 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5643 | `		}` |
|      ! 0 |  5644 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5645 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5646 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5647 | `			if( zName ){` |
|      ! 0 |  5648 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5649 | `			}` |
|      ! 0 |  5650 | `		}` |
|      ! 0 |  5651 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5652 | `		pTos--;` |
|      ! 0 |  5653 | `	}else{` |
|       34 |  5654 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5655 | `	}` |
|       34 |  5656 | `	nIdx = pTos->nIdx;` |
|       34 |  5657 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5658 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5659 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5660 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5661 | `		}else{` |
|        - |  5662 | `			ph7_value *pObj;` |
|        - |  5663 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5664 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5665 | `			if( pObj == 0 ){` |
|      ! 0 |  5666 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5667 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5668 | `				goto Abort;` |
|        - |  5669 | `			}` |
|        - |  5670 | `			/* Perform the store operation */` |
|      ! 0 |  5671 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5672 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5673 | `		}` |
|       34 |  5674 | `	}else if( sName.nByte > 0){` |
|       34 |  5675 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5676 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5677 | `		}else{` |
|       34 |  5678 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5679 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5680 | `			/* Query the local frame */` |
|       34 |  5681 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5682 | `			if( pEntry ){` |
|      ! 0 |  5683 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5684 | `			}else{` |
|       34 |  5685 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5686 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5687 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5688 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5689 | `				}` |
|       34 |  5690 | `				if( rc == SXRET_OK ){` |
|       34 |  5691 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5692 | `				}` |
|        - |  5693 | `			}` |
|        - |  5694 | `		}` |
|       16 |  5695 | `	}` |
|       34 |  5696 | `	break;` |
|        - |  5697 | `				 }` |
|        - |  5698 | `/*` |
|        - |  5699 | ` * OP_UPLINK P1 * *` |
|        - |  5700 | ` * Link a variable to the top active VM frame.` |
|        - |  5701 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5702 | ` */` |
|       25 |  5703 | `case PH7_OP_UPLINK: {` |
|       52 |  5704 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5705 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5706 | `		SyString sName;` |
|        - |  5707 | `		/* Perform the link */` |
|      104 |  5708 | `		while( pLink <= pTos ){` |
|       54 |  5709 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5710 | `				/* Force a string cast */` |
|      ! 0 |  5711 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5712 | `			}` |
|       54 |  5713 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5714 | `			if( sName.nByte > 0 ){` |
|       54 |  5715 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5716 | `			}` |
|       54 |  5717 | `			pLink++;` |
|        2 |  5718 | `		}` |
|       25 |  5719 | `	}` |
|       52 |  5720 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5721 | `	break;` |
|        - |  5722 | `					}` |
|        - |  5723 | `/*` |
|        - |  5724 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5725 | ` * Push an exception in the corresponding container so that` |
|        - |  5726 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5727 | ` */` |
|       66 |  5728 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      134 |  5729 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5730 | `	VmFrame *pFrameLocal;` |
|        - |  5731 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      134 |  5732 | `	pException->iFinallyDone = 0;` |
|      134 |  5733 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5734 | `	/* Create the exception frame */` |
|      134 |  5735 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      134 |  5736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5737 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5738 | `		goto Abort;` |
|        - |  5739 | `	}` |
|        - |  5740 | `	/* Mark the special frame */` |
|      134 |  5741 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      134 |  5742 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5743 | `	/* Point to the frame that trigger the exception */` |
|      134 |  5744 | `	pFrameLocal = pFrameLocal->pParent;` |
|      134 |  5745 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      134 |  5746 | `	pException->pFrame = pFrameLocal;` |
|      134 |  5747 | `	break;` |
|        - |  5748 | `							}` |
|        - |  5749 | `/*` |
|        - |  5750 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5751 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5752 | ` */` |
|       65 |  5753 | `case PH7_OP_POP_EXCEPTION: {` |
|      132 |  5754 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      132 |  5755 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5756 | `		ph7_exception **apException;` |
|        - |  5757 | `		/* Pop the loaded exception */` |
|       28 |  5758 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5759 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5760 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5761 | `		}` |
|       13 |  5762 | `	}` |
|      132 |  5763 | `	pException->pFrame = 0;` |
|        - |  5764 | `	/* Leave the exception frame */` |
|      132 |  5765 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5766 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      132 |  5767 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5768 | `		sxi32 rcFinally;` |
|       20 |  5769 | `		pException->iFinallyDone = 1;` |
|       20 |  5770 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5771 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5772 | `			goto Abort;` |
|        - |  5773 | `		}` |
|        9 |  5774 | `	}` |
|      132 |  5775 | `	break;` |
|        - |  5776 | `							}` |
|        - |  5777 |  |
|        - |  5778 | `/*` |
|        - |  5779 | ` * OP_THROW * P2 *` |
|        - |  5780 | ` * Throw an user exception.` |
|        - |  5781 | ` */` |
|       30 |  5782 | `case PH7_OP_THROW: {` |
|       62 |  5783 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  5784 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5785 | `#ifdef UNTRUST` |
|        - |  5786 | `	if( pTos < pStack ){` |
|        - |  5787 | `		goto Abort;` |
|        - |  5788 | `	}` |
|        - |  5789 | `#endif` |
|       62 |  5790 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5791 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  5792 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  5793 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  5794 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5795 | `		ph7_class *pException;` |
|        - |  5796 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5797 | `		 */` |
|       62 |  5798 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  5799 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5800 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5801 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5802 | `			if( rc == SXERR_ABORT ){` |
|        - |  5803 | `				/* Abort processing immediately */` |
|      ! 0 |  5804 | `				goto Abort;` |
|        - |  5805 | `			}` |
|      ! 0 |  5806 | `		}else{` |
|        - |  5807 | `			/* Throw the exception */` |
|       62 |  5808 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  5809 | `			if( rc == SXERR_ABORT ){` |
|        - |  5810 | `				/* Abort processing immediately */` |
|        9 |  5811 | `				goto Abort;` |
|        - |  5812 | `			}` |
|        - |  5813 | `		}` |
|       28 |  5814 | `	}else{` |
|        - |  5815 | `		/* Expecting a class instance */` |
|      ! 0 |  5816 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5817 | `		if( rc == SXERR_ABORT ){` |
|        - |  5818 | `			/* Abort processing immediately */` |
|      ! 0 |  5819 | `			goto Abort;` |
|        - |  5820 | `		}` |
|        - |  5821 | `	}` |
|        - |  5822 | `	/* Pop the top entry */` |
|       54 |  5823 | `	VmPopOperand(&pTos,1);` |
|        - |  5824 | `	/* Perform an unconditional jump */` |
|       54 |  5825 | `	pc = nJump - 1;` |
|       54 |  5826 | `	break;` |
|        - |  5827 | `				   }` |
|        - |  5828 | `/*` |
|        - |  5829 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5830 | ` * Prepare a foreach step.` |
|        - |  5831 | ` */` |
|     5399 |  5832 | `case PH7_OP_FOREACH_INIT: {` |
|    10800 |  5833 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5834 | `	void *pName;` |
|        - |  5835 | `#ifdef UNTRUST` |
|        - |  5836 | `	if( pTos < pStack ){` |
|        - |  5837 | `		goto Abort;` |
|        - |  5838 | `	}` |
|        - |  5839 | `#endif` |
|    10800 |  5840 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5841 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5842 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5843 | `			/* Force a string cast */` |
|      ! 0 |  5844 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5845 | `		}` |
|        - |  5846 | `		/* Duplicate name */` |
|      ! 0 |  5847 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5848 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5849 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5850 | `		}` |
|      ! 0 |  5851 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5852 | `	}` |
|    10800 |  5853 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5854 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5855 | `			/* Force a string cast */` |
|      ! 0 |  5856 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5857 | `		}` |
|        - |  5858 | `		/* Duplicate name */` |
|      ! 0 |  5859 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5860 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5861 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5862 | `		}` |
|      ! 0 |  5863 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5864 | `	}` |
|        - |  5865 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10800 |  5866 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5867 | `		/* Jump out of the loop */` |
|      ! 0 |  5868 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5869 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5870 | `		}` |
|      ! 0 |  5871 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5872 | `	}else{` |
|        - |  5873 | `		ph7_foreach_step *pStep;` |
|    10800 |  5874 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10800 |  5875 | `		if( pStep == 0 ){` |
|      ! 0 |  5876 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5877 | `			/* Jump out of the loop */` |
|      ! 0 |  5878 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5879 | `		}else{` |
|        - |  5880 | `			/* Zero the structure */` |
|    10800 |  5881 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5882 | `			/* Prepare the step */` |
|    10800 |  5883 | `			pStep->iFlags = pInfo->iFlags;` |
|    10800 |  5884 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5885 | `				ph7_hashmap *pMap;` |
|        - |  5886 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5887 | `				 * source array so mutations don't affect other sharers. */` |
|    10772 |  5888 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5889 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5890 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5891 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5892 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5893 | `						 * variable still points at the same hashmap as` |
|        - |  5894 | `						 * the stack value. */` |
|        9 |  5895 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5896 | `							pCur->iRef--;` |
|        9 |  5897 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5898 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5899 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5900 | `						}` |
|        4 |  5901 | `					}` |
|        4 |  5902 | `				}` |
|    10772 |  5903 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5904 | `				/* Reset the internal loop cursor */` |
|    10772 |  5905 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5906 | `				/* Mark the step */` |
|    10772 |  5907 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10772 |  5908 | `				pStep->xIter.pMap = pMap;` |
|    10772 |  5909 | `				pMap->iRef++;` |
|     5387 |  5910 | `			}else{` |
|       30 |  5911 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5912 | `				ph7_class *pIteratorClass;` |
|        - |  5913 | `				/* Check if the object implements Iterator */` |
|       30 |  5914 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5915 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5916 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5917 | `					ph7_class_method *pRewind;` |
|       20 |  5918 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5919 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5920 | `					pThis->iRef++;` |
|       20 |  5921 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5922 | `					if( pRewind ){` |
|       20 |  5923 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5924 | `					}` |
|       11 |  5925 | `				}else{` |
|        - |  5926 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5927 | `					ph7_class *pIterAggClass;` |
|       12 |  5928 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5929 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5930 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5931 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5932 | `						ph7_class_method *pGetIter;` |
|        3 |  5933 | `						int iterAggOk = 0;` |
|        3 |  5934 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5935 | `						if( pGetIter ){` |
|        - |  5936 | `							ph7_value sResult;` |
|        3 |  5937 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5938 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5939 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5940 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5941 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5942 | `									ph7_class_method *pRewind;` |
|        3 |  5943 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5944 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5945 | `									pIterObj->iRef++;` |
|        - |  5946 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5947 | `									pStep->pOwner = pThis;` |
|        3 |  5948 | `									pThis->iRef++;` |
|        3 |  5949 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5950 | `									if( pRewind ){` |
|        3 |  5951 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5952 | `									}` |
|        3 |  5953 | `									iterAggOk = 1;` |
|        1 |  5954 | `								}` |
|        1 |  5955 | `							}` |
|        3 |  5956 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5957 | `						}` |
|        3 |  5958 | `						if( !iterAggOk ){` |
|        - |  5959 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5960 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5961 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5962 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5963 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5964 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5965 | `						}` |
|        2 |  5966 | `					}else{` |
|        - |  5967 | `						/* Plain object iteration via hAttr */` |
|        9 |  5968 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5969 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5970 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5971 | `						pThis->iRef++;` |
|        - |  5972 | `					}` |
|        - |  5973 | `				}` |
|        - |  5974 | `			}` |
|        - |  5975 | `		}` |
|    10800 |  5976 | `		if( pStep ){` |
|    10800 |  5977 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5978 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5979 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5980 | `				/* Jump out of the loop */` |
|      ! 0 |  5981 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5982 | `			}` |
|     5399 |  5983 | `		}` |
|        - |  5984 | `	}` |
|    10800 |  5985 | `	VmPopOperand(&pTos,1);` |
|    10800 |  5986 | `	break;` |
|        - |  5987 | `						  }` |
|        - |  5988 | `/*` |
|        - |  5989 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5990 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5991 | ` */` |
|    87983 |  5992 | `case PH7_OP_FOREACH_STEP: {` |
|   175968 |  5993 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5994 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5995 | `	ph7_value *pValue;` |
|        - |  5996 | `	VmFrame *pFrameLocal;` |
|        - |  5997 | `	/* Peek the last step */` |
|   175968 |  5998 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   175968 |  5999 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   175968 |  6000 | `	pFrameLocal = pVm->pFrame;` |
|   175968 |  6001 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   175968 |  6002 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   175856 |  6003 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6004 | `		ph7_hashmap_node *pNode;` |
|        - |  6005 | `		/* Extract the current node value */` |
|   175856 |  6006 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   175856 |  6007 | `		if( pNode == 0 ){` |
|        - |  6008 | `			/* No more entry to process */` |
|    10770 |  6009 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10770 |  6010 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6011 | `				/* Break the reference with the last element */` |
|        7 |  6012 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6013 | `			}` |
|        - |  6014 | `			/* Automatically reset the loop cursor */` |
|    10770 |  6015 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6016 | `			/* Cleanup the mess left behind */` |
|    10770 |  6017 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10770 |  6018 | `			SySetPop(&pInfo->aStep);` |
|    10770 |  6019 | `			PH7_HashmapUnref(pMap);` |
|     5386 |  6020 | `		}else{` |
|   165088 |  6021 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  6022 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  6023 | `				if( pKey ){` |
|      416 |  6024 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  6025 | `				}` |
|      207 |  6026 | `			}` |
|   165088 |  6027 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6028 | `				SyHashEntry *pEntry;` |
|        - |  6029 | `				/* Pass by reference */` |
|       23 |  6030 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6031 | `				if( pEntry ){` |
|       21 |  6032 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6033 | `				}else{` |
|        4 |  6034 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6035 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6036 | `				}` |
|       12 |  6037 | `			}else{` |
|        - |  6038 | `				/* Make a copy of the entry value */` |
|   165066 |  6039 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   165066 |  6040 | `				if( pValue ){` |
|   165066 |  6041 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    82532 |  6042 | `				}` |
|        - |  6043 | `			}` |
|        2 |  6044 | `		}` |
|    88041 |  6045 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6046 | `		/* Iterator-based iteration.` |
|        - |  6047 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6048 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6049 | `		 */` |
|       90 |  6050 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6051 | `		ph7_class_method *pMethod;` |
|        - |  6052 | `		ph7_value sResult;` |
|       90 |  6053 | `		int isValid = 0;` |
|        - |  6054 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  6055 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  6056 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  6057 | `		}else{` |
|       70 |  6058 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  6059 | `			if( pMethod ){` |
|       70 |  6060 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  6061 | `			}` |
|        - |  6062 | `		}` |
|        - |  6063 | `		/* Call valid() */` |
|       90 |  6064 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  6065 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  6066 | `		if( pMethod ){` |
|       90 |  6067 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  6068 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  6069 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  6070 | `		}` |
|       90 |  6071 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  6072 | `		if( !isValid ){` |
|        - |  6073 | `			/* Iterator exhausted */` |
|       20 |  6074 | `			pc = pInstr->iP2 - 1;` |
|        - |  6075 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  6076 | `			if( pStep->pOwner ){` |
|        3 |  6077 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6078 | `			}` |
|       20 |  6079 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  6080 | `			SySetPop(&pInfo->aStep);` |
|       20 |  6081 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  6082 | `		}else{` |
|        - |  6083 | `			/* Call current() to get value */` |
|       72 |  6084 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  6085 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  6086 | `			if( pMethod ){` |
|       72 |  6087 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  6088 | `			}` |
|       72 |  6089 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  6090 | `			if( pValue ){` |
|       72 |  6091 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  6092 | `			}` |
|       72 |  6093 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6094 | `			/* Call key() if needed */` |
|       72 |  6095 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6096 | `				ph7_value sKey;` |
|       35 |  6097 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6098 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6099 | `				if( pMethod ){` |
|       35 |  6100 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6101 | `				}` |
|       35 |  6102 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6103 | `				if( pValue ){` |
|       35 |  6104 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6105 | `				}` |
|       35 |  6106 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6107 | `			}` |
|        - |  6108 | `		}` |
|       46 |  6109 | `	}else{` |
|       25 |  6110 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6111 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6112 | `		SyHashEntry *pEntry;` |
|        - |  6113 | `		/* Point to the next attribute */` |
|       29 |  6114 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6115 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6116 | `			/* Check access permission */` |
|       31 |  6117 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6118 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6119 | `					break; /* Access is granted */` |
|        - |  6120 | `			}` |
|        1 |  6121 | `		}` |
|       25 |  6122 | `		if( pEntry == 0 ){` |
|        - |  6123 | `			/* Clean up the mess left behind */` |
|        9 |  6124 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6125 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6126 | `				/* Break the reference with the last element */` |
|        3 |  6127 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6128 | `			}` |
|        9 |  6129 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6130 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6131 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6132 | `		}else{` |
|       17 |  6133 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6134 | `			ph7_value *pAttrValue;` |
|       17 |  6135 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6136 | `				/* Fill with the current attribute name */` |
|       17 |  6137 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6138 | `				if( pKey ){` |
|       17 |  6139 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6140 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6141 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6142 | `				}` |
|        8 |  6143 | `			}` |
|        - |  6144 | `			/* Extract attribute value */` |
|       17 |  6145 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6146 | `			if( pAttrValue ){` |
|       17 |  6147 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6148 | `					/* Pass by reference */` |
|        3 |  6149 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6150 | `					if( pEntry ){` |
|        3 |  6151 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6152 | `					}else{` |
|      ! 0 |  6153 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6154 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6155 | `					}` |
|        2 |  6156 | `				}else{` |
|        - |  6157 | `					/* Make a copy of the attribute value */` |
|       15 |  6158 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6159 | `					if( pValue ){` |
|       15 |  6160 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6161 | `					}` |
|        - |  6162 | `				}` |
|        8 |  6163 | `			}` |
|        - |  6164 | `		}` |
|        - |  6165 | `	}` |
|   175968 |  6166 | `	break;` |
|        - |  6167 | `						  }` |
|        - |  6168 | `/*` |
|        - |  6169 | ` * OP_MEMBER P1 P2` |
|        - |  6170 | ` * Load class attribute/method on the stack.` |
|        - |  6171 | ` */` |
|     2676 |  6172 | `case PH7_OP_MEMBER: {` |
|        - |  6173 | `	ph7_class_instance *pThis;` |
|        - |  6174 | `	ph7_value *pNos;` |
|        - |  6175 | `	SyString sName;` |
|     5354 |  6176 | `	if( !pInstr->iP1 ){` |
|     5136 |  6177 | `		pNos = &pTos[-1];` |
|        - |  6178 | `#ifdef UNTRUST` |
|        - |  6179 | `		if( pNos < pStack ){` |
|        - |  6180 | `			goto Abort;` |
|        - |  6181 | `		}` |
|        - |  6182 | `#endif` |
|     5136 |  6183 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6184 | `			ph7_class *pClass;` |
|        - |  6185 | `			/* Class already instantiated */` |
|     5136 |  6186 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6187 | `			/* Point to the instantiated class */` |
|     5136 |  6188 | `			pClass = pThis->pClass;` |
|        - |  6189 | `			/* Extract attribute name first */` |
|     5136 |  6190 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     5136 |  6191 | `			if( pInstr->iP2 ){` |
|        - |  6192 | `				/* Method call */` |
|      538 |  6193 | `				ph7_class_method *pMeth = 0;` |
|      538 |  6194 | `				if( sName.nByte > 0 ){` |
|        - |  6195 | `					/* Extract the target method */` |
|      538 |  6196 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      268 |  6197 | `				}` |
|      538 |  6198 | `				if( pMeth == 0 ){` |
|      ! 0 |  6199 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6200 | `						&pClass->sName,&sName` |
|        - |  6201 | `						);` |
|        - |  6202 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6203 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6204 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6205 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6206 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6207 | `				}else{` |
|        - |  6208 | `					/* Push method name on the stack */` |
|      538 |  6209 | `					PH7_MemObjRelease(pTos);` |
|      538 |  6210 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      538 |  6211 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6212 | `				}` |
|      538 |  6213 | `				pTos->nIdx = SXU32_HIGH;` |
|      270 |  6214 | `			}else{` |
|        - |  6215 | `				/* Attribute access */` |
|     4600 |  6216 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6217 | `				SyHashEntry *pEntry;` |
|        - |  6218 | `				/* Extract the target attribute */` |
|     4600 |  6219 | `				if( sName.nByte > 0 ){` |
|     4600 |  6220 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4600 |  6221 | `					if( pEntry ){` |
|        - |  6222 | `						/* Point to the attribute value */` |
|     4598 |  6223 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2298 |  6224 | `					}` |
|     2299 |  6225 | `				}` |
|     4600 |  6226 | `				if( pObjAttr == 0 ){` |
|        - |  6227 | `					/* No such attribute,load null */` |
|        4 |  6228 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6229 | `						&pClass->sName,&sName);` |
|        - |  6230 | `					/* Call the __get magic method if available */` |
|        3 |  6231 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6232 | `				}` |
|     4600 |  6233 | `				VmPopOperand(&pTos,1);` |
|        - |  6234 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6235 | `				 * This is due to the following case:` |
|        - |  6236 | `				 *     (new TestClass())->foo;` |
|        - |  6237 | `				 */` |
|     4600 |  6238 | `				pThis->iRef++;` |
|     4600 |  6239 | `				PH7_MemObjRelease(pTos);` |
|     4600 |  6240 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4600 |  6241 | `				if( pObjAttr ){` |
|     4598 |  6242 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6243 | `					/* Check attribute access */` |
|     4598 |  6244 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6245 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6246 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6247 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6248 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6249 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     4596 |  6250 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2315 |  6251 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       32 |  6252 | `							VmInstr *pNext = pInstr + 1;` |
|       32 |  6253 | `							int bIsLhs = 0;` |
|       32 |  6254 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       30 |  6255 | `								bIsLhs = 1;` |
|       14 |  6256 | `							}` |
|       32 |  6257 | `							if( !bIsLhs ){` |
|        3 |  6258 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6259 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6260 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6261 | `									goto Abort;` |
|        - |  6262 | `								}` |
|        - |  6263 | `								{` |
|        3 |  6264 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6265 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6266 | `										pc = pFrm2->iExceptionJump - 1;` |
|     2676 |  6267 | `										break;` |
|        - |  6268 | `									}` |
|        - |  6269 | `								}` |
|      ! 0 |  6270 | `								goto Exception;` |
|        - |  6271 | `							}` |
|       14 |  6272 | `						}` |
|        - |  6273 | `						/* Load attribute */` |
|     4596 |  6274 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4596 |  6275 | `						if( pValue ){` |
|     4596 |  6276 | `							if( pThis->iRef < 2 ){` |
|        - |  6277 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6278 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6279 | `								 */` |
|        3 |  6280 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  6281 | `							}else{` |
|        - |  6282 | `								/* Simple load */` |
|     4594 |  6283 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6284 | `							}` |
|     4596 |  6285 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4594 |  6286 | `								if( pThis->iRef > 1 ){` |
|        - |  6287 | `									/* Load attribute index */` |
|     4592 |  6288 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2295 |  6289 | `								}` |
|     2296 |  6290 | `							}` |
|     2297 |  6291 | `						}` |
|     2299 |  6292 | `					}else{` |
|        - |  6293 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6294 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6295 | `						char zMsg[256];` |
|      ! 0 |  6296 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6297 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6298 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6299 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6300 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6301 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6302 | `						goto Abort;` |
|        - |  6303 | `					}` |
|     2297 |  6304 | `				}` |
|        - |  6305 | `				/* Safely unreference the object */` |
|     4598 |  6306 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6307 | `			}` |
|     2568 |  6308 | `		}else{` |
|      ! 0 |  6309 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  6310 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6311 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6312 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6313 | `		}` |
|     2568 |  6314 | `	}else{` |
|        - |  6315 | `		/* Static member access using class name */` |
|      220 |  6316 | `		pNos = pTos;` |
|      220 |  6317 | `		pThis = 0;` |
|      220 |  6318 | `		if( !pInstr->p3 ){` |
|      186 |  6319 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      186 |  6320 | `			pNos--;` |
|        - |  6321 | `#ifdef UNTRUST` |
|        - |  6322 | `			if( pNos < pStack ){` |
|        - |  6323 | `				goto Abort;` |
|        - |  6324 | `			}` |
|        - |  6325 | `#endif` |
|       94 |  6326 | `		}else{` |
|        - |  6327 | `			/* Attribute name already computed */` |
|       36 |  6328 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6329 | `		}` |
|      220 |  6330 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      220 |  6331 | `			ph7_class *pClass = 0;` |
|      220 |  6332 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6333 | `				/* Class already instantiated */` |
|        5 |  6334 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6335 | `				pClass = pThis->pClass;` |
|        5 |  6336 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6337 | `			}else{` |
|        - |  6338 | `				/* Try to extract the target class */` |
|      216 |  6339 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      216 |  6340 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      216 |  6341 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6342 | `					/* Handle self/static/parent keywords */` |
|      216 |  6343 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6344 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6345 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6346 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6347 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6348 | `						}` |
|      186 |  6349 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6350 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      155 |  6351 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  6352 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  6353 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  6354 | `							pClass = pSelf->pBase;` |
|       12 |  6355 | `						}` |
|       14 |  6356 | `					}else{` |
|      106 |  6357 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6358 | `					}` |
|      107 |  6359 | `				}` |
|        - |  6360 | `			}` |
|      220 |  6361 | `			if( pClass == 0 ){` |
|        - |  6362 | `				/* Undefined class */` |
|      ! 0 |  6363 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6364 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6365 | `					);` |
|      ! 0 |  6366 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6367 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6368 | `				}` |
|      ! 0 |  6369 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6370 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6371 | `			}else{` |
|      220 |  6372 | `				if( pInstr->iP2 ){` |
|        - |  6373 | `					/* Method call */` |
|       82 |  6374 | `					ph7_class_method *pMeth = 0;` |
|       82 |  6375 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6376 | `						/* Extract the target method */` |
|       82 |  6377 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       40 |  6378 | `					}` |
|       82 |  6379 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6380 | `						if( pMeth ){` |
|      ! 0 |  6381 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6382 | `								&pClass->sName,&sName` |
|        - |  6383 | `								);` |
|      ! 0 |  6384 | `						}else{` |
|      ! 0 |  6385 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6386 | `								&pClass->sName,&sName` |
|        - |  6387 | `								);` |
|        - |  6388 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6389 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6390 | `						}` |
|        - |  6391 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6392 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6393 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6394 | `						}` |
|      ! 0 |  6395 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6396 | `					}else{` |
|        - |  6397 | `						/* Push method name on the stack */` |
|       82 |  6398 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6399 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       82 |  6400 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6401 | `					}` |
|       82 |  6402 | `					pTos->nIdx = SXU32_HIGH;` |
|       42 |  6403 | `				}else{` |
|        - |  6404 | `					/* Attribute access */` |
|      140 |  6405 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6406 | `					/* Check for special ::class pseudo-constant */` |
|      186 |  6407 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6408 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6409 | `						/* ::class returns the fully qualified class name */` |
|        - |  6410 | `						/* Pop the attribute name from the stack */` |
|       60 |  6411 | `						if( !pInstr->p3 ){` |
|       60 |  6412 | `							VmPopOperand(&pTos,1);` |
|       29 |  6413 | `						}` |
|       60 |  6414 | `						PH7_MemObjRelease(pTos);` |
|        - |  6415 | `						/* Load the class name */` |
|       60 |  6416 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6417 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6418 | `					}else{` |
|        - |  6419 | `						/* Extract the target attribute */` |
|       82 |  6420 | `						if( sName.nByte > 0 ){` |
|       82 |  6421 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       40 |  6422 | `						}` |
|       82 |  6423 | `						if( pAttr == 0 ){` |
|        - |  6424 | `							/* No such attribute,load null */` |
|      ! 0 |  6425 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6426 | `								&pClass->sName,&sName);` |
|        - |  6427 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6428 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6429 | `						}` |
|        - |  6430 | `						/* Pop the attribute name from the stack */` |
|       82 |  6431 | `						if( !pInstr->p3 ){` |
|       48 |  6432 | `							VmPopOperand(&pTos,1);` |
|       23 |  6433 | `						}` |
|       82 |  6434 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6435 | `						pTos->nIdx = SXU32_HIGH;` |
|       82 |  6436 | `						if( pAttr ){` |
|       82 |  6437 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6438 | `								/* Access to a non static attribute */` |
|      ! 0 |  6439 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6440 | `									&pClass->sName,&pAttr->sName` |
|        - |  6441 | `									);` |
|      ! 0 |  6442 | `							}else{` |
|        - |  6443 | `								ph7_value *pValue;` |
|        - |  6444 | `								/* Check if the access to the attribute is allowed */` |
|       82 |  6445 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6446 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6447 | `									 * Same LHS-of-store peek as the instance path. */` |
|       76 |  6448 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       51 |  6449 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       35 |  6450 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       22 |  6451 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 |  6452 | `										if( pS ){` |
|       24 |  6453 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       24 |  6454 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        5 |  6455 | `												VmInstr *pNext = pInstr + 1;` |
|        5 |  6456 | `												int bIsLhs = 0;` |
|        5 |  6457 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        3 |  6458 | `													bIsLhs = 1;` |
|        1 |  6459 | `												}` |
|        5 |  6460 | `												if( !bIsLhs ){` |
|        3 |  6461 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6462 | `													if( pThis ){` |
|      ! 0 |  6463 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6464 | `													}` |
|        3 |  6465 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6466 | `														goto Abort;` |
|        - |  6467 | `													}` |
|        - |  6468 | `													{` |
|        3 |  6469 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6470 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6471 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6472 | `															break;` |
|        - |  6473 | `														}` |
|        - |  6474 | `													}` |
|      ! 0 |  6475 | `													goto Exception;` |
|        - |  6476 | `												}` |
|        1 |  6477 | `											}` |
|       10 |  6478 | `										}` |
|       10 |  6479 | `									}` |
|        - |  6480 | `									/* Load the desired attribute */` |
|       76 |  6481 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       76 |  6482 | `									if( pValue ){` |
|       76 |  6483 | `										PH7_MemObjLoad(pValue,pTos);` |
|       76 |  6484 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6485 | `											/* Load index number */` |
|       34 |  6486 | `											pTos->nIdx = pAttr->nIdx;` |
|       16 |  6487 | `										}` |
|       37 |  6488 | `									}` |
|       39 |  6489 | `								}else{` |
|        - |  6490 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6491 | `									char zMsg[256];` |
|        5 |  6492 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6493 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6494 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6495 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6496 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6497 | `									}else{` |
|      ! 0 |  6498 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6499 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6500 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6501 | `									}` |
|        5 |  6502 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6503 | `									goto Abort;` |
|        - |  6504 | `								}` |
|        - |  6505 | `							}` |
|       37 |  6506 | `						}` |
|        - |  6507 | `					}` |
|        - |  6508 | `				}` |
|      214 |  6509 | `				if( pThis ){` |
|        - |  6510 | `					/* Safely unreference the object */` |
|        5 |  6511 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6512 | `				}` |
|        - |  6513 | `			}` |
|      108 |  6514 | `		}else{` |
|        - |  6515 | `			/* Pop operands */` |
|      ! 0 |  6516 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6517 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6518 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6519 | `			}` |
|      ! 0 |  6520 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6521 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6522 | `		}` |
|        - |  6523 | `	}` |
|     5346 |  6524 | `	break;` |
|        - |  6525 | `					}` |
|        - |  6526 | `/*` |
|        - |  6527 | ` * OP_NEW P1 * * *` |
|        - |  6528 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6529 | ` */` |
|      401 |  6530 | `case PH7_OP_NEW: {` |
|      804 |  6531 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      804 |  6532 | `	ph7_class *pClass = 0;` |
|        - |  6533 | `	ph7_class_instance *pNew;` |
|      804 |  6534 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6535 | `		/* Try to extract the desired class */` |
|     1205 |  6536 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      802 |  6537 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      401 |  6538 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6539 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6540 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6541 | `	}` |
|      804 |  6542 | `	if( pClass == 0 ){` |
|        - |  6543 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6544 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6545 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6546 | `			);` |
|        - |  6547 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6548 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6549 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6550 | `			/* Pop given arguments */` |
|      ! 0 |  6551 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6552 | `		}` |
|      ! 0 |  6553 | `		goto Abort;` |
|      ! 0 |  6554 | `	}else{` |
|        - |  6555 | `		ph7_class_method *pCons;` |
|        - |  6556 | `		/* Create a new class instance */` |
|      804 |  6557 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      804 |  6558 | `		if( pNew == 0 ){` |
|      ! 0 |  6559 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6560 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6561 | `				&pClass->sName` |
|        - |  6562 | `			);` |
|      ! 0 |  6563 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6564 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6565 | `				/* Pop given arguments */` |
|      ! 0 |  6566 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6567 | `			}` |
|      ! 0 |  6568 | `			break;` |
|        - |  6569 | `		}` |
|        - |  6570 | `		/* Check if a constructor is available */` |
|      804 |  6571 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      804 |  6572 | `		if( pCons == 0 ){` |
|      658 |  6573 | `			SyString *pName = &pClass->sName;` |
|        - |  6574 | `			/* Check for a constructor with the same base class name */` |
|      658 |  6575 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      328 |  6576 | `		}` |
|      804 |  6577 | `		if( pCons ){` |
|        - |  6578 | `			/* Call the class constructor */` |
|      148 |  6579 | `			SySetReset(&aArg);` |
|      286 |  6580 | `			while( pArg < pTos ){` |
|      140 |  6581 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      140 |  6582 | `				pArg++;` |
|        2 |  6583 | `			}` |
|      148 |  6584 | `			if( pVm->bErrReport ){` |
|        - |  6585 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6586 | `				sxu32 n;` |
|       57 |  6587 | `				n = SySetUsed(&aArg);` |
|        - |  6588 | `				/* Emit a notice for missing arguments */` |
|      101 |  6589 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6590 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6591 | `					if( pFuncArg ){` |
|       45 |  6592 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6593 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6594 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6595 | `						}` |
|       22 |  6596 | `					}` |
|       45 |  6597 | `					n++;` |
|        1 |  6598 | `				}` |
|       28 |  6599 | `			}` |
|      148 |  6600 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6601 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      148 |  6602 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6603 | `				pNew->iRef = 1;` |
|      ! 0 |  6604 | `			}` |
|       73 |  6605 | `		}` |
|      804 |  6606 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6607 | `			/* Pop given arguments */` |
|      130 |  6608 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       64 |  6609 | `		}` |
|      804 |  6610 | `		PH7_MemObjRelease(pTos);` |
|      804 |  6611 | `		pTos->x.pOther = pNew;` |
|      804 |  6612 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6613 | `	}` |
|      804 |  6614 | `	break;` |
|        - |  6615 | `				 }` |
|        - |  6616 | `/*` |
|        - |  6617 | ` * OP_CLONE * * *` |
|        - |  6618 | ` * Perfome a clone operation.` |
|        - |  6619 | ` */` |
|       23 |  6620 | `case PH7_OP_CLONE: {` |
|        - |  6621 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6622 | `#ifdef UNTRUST` |
|        - |  6623 | `	if( pTos < pStack ){` |
|        - |  6624 | `		goto Abort;` |
|        - |  6625 | `	}` |
|        - |  6626 | `#endif` |
|        - |  6627 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6628 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6629 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6630 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6631 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6632 | `		break;` |
|        - |  6633 | `	}` |
|        - |  6634 | `	/* Point to the source */` |
|       44 |  6635 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6636 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6637 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6638 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6639 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6640 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6641 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6642 | `		break;` |
|        - |  6643 | `	}` |
|        - |  6644 | `	/* Perform the clone operation */` |
|       44 |  6645 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6646 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6647 | `	if( pClone == 0 ){` |
|      ! 0 |  6648 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6649 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6650 | `	}else{` |
|        - |  6651 | `		/* Load the cloned object */` |
|       44 |  6652 | `		pTos->x.pOther = pClone;` |
|       44 |  6653 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6654 | `	}` |
|       44 |  6655 | `	break;` |
|        - |  6656 | `				   }` |
|        - |  6657 | `/*` |
|        - |  6658 | ` * OP_SWITCH * * P3` |
|        - |  6659 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6660 | ` */` |
|       26 |  6661 | `case PH7_OP_SWITCH: {` |
|       54 |  6662 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6663 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6664 | `	ph7_value sValue,sCaseValue;` |
|        - |  6665 | `	sxu32 n,nEntry;` |
|        - |  6666 | `#ifdef UNTRUST` |
|        - |  6667 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6668 | `		goto Abort;` |
|        - |  6669 | `	}` |
|        - |  6670 | `#endif` |
|        - |  6671 | `	/* Point to the case table  */` |
|       54 |  6672 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6673 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6674 | `	/* Select the appropriate case block to execute */` |
|       54 |  6675 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6676 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6677 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6678 | `		pCase = &aCase[n];` |
|      130 |  6679 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6680 | `		/* Execute the case expression first */` |
|      130 |  6681 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6682 | `		/* Compare the two expression */` |
|      130 |  6683 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6684 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6685 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6686 | `		if( rc == 0 ){` |
|        - |  6687 | `			/* Value match,jump to this block */` |
|       52 |  6688 | `			pc = pCase->nStart - 1;` |
|       52 |  6689 | `			break;` |
|        - |  6690 | `		}` |
|       41 |  6691 | `	}` |
|       54 |  6692 | `	VmPopOperand(&pTos,1);` |
|       54 |  6693 | `	if( n >= nEntry ){` |
|        - |  6694 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6695 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6696 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6697 | `		}else{` |
|        - |  6698 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6699 | `			pc = pSwitch->nOut - 1;` |
|        - |  6700 | `		}` |
|        1 |  6701 | `	}` |
|       54 |  6702 | `	break;` |
|        - |  6703 | `					}` |
|        - |  6704 | `/*` |
|        - |  6705 | ` * OP_YIELD P1 P2 *` |
|        - |  6706 | ` *  Yield a value from a generator function.` |
|        - |  6707 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6708 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6709 | ` */` |
|       28 |  6710 | `case PH7_OP_YIELD: {` |
|        - |  6711 | `	ph7_generator *pGen;` |
|       58 |  6712 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6713 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6714 | `		goto Abort;` |
|        - |  6715 | `	}` |
|       58 |  6716 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6717 | `	if( pInstr->iP2 ){` |
|        - |  6718 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6719 | `#ifdef UNTRUST` |
|        - |  6720 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6721 | `#endif` |
|        7 |  6722 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6723 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6724 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6725 | `		VmPopOperand(&pTos, 1);` |
|        - |  6726 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6727 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6728 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6729 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6730 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6731 | `			}` |
|        1 |  6732 | `		}` |
|       55 |  6733 | `	}else if( pInstr->iP1 ){` |
|        - |  6734 | `		/* yield $value */` |
|        - |  6735 | `#ifdef UNTRUST` |
|        - |  6736 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6737 | `#endif` |
|       52 |  6738 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6739 | `		VmPopOperand(&pTos, 1);` |
|        - |  6740 | `		/* Auto-increment key */` |
|       52 |  6741 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6742 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6743 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6744 | `	}else{` |
|        - |  6745 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6746 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6747 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6748 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6749 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6750 | `	}` |
|        - |  6751 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6752 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6753 | `	goto Suspend;` |
|        - |  6754 |  |
|        - |  6755 | `/*` |
|        - |  6756 | ` * OP_CALL P1 * *` |
|        - |  6757 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6758 | ` *  function on the stack.` |
|        - |  6759 | ` */` |
|   314131 |  6760 | `case PH7_OP_CALL: {` |
|   628308 |  6761 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6762 | `	ph7_value *pArg;` |
|   628308 |  6763 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   628308 |  6764 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6765 | `	SyHashEntry *pEntry;` |
|        - |  6766 | `	SyString sName;` |
|        - |  6767 | `	/* Extract function name */` |
|   628308 |  6768 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6769 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6770 | `			ph7_value sResult;` |
|      ! 0 |  6771 | `			SySetReset(&aArg);` |
|      ! 0 |  6772 | `			while( pArg < pTos ){` |
|      ! 0 |  6773 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6774 | `				pArg++;` |
|      ! 0 |  6775 | `			}` |
|      ! 0 |  6776 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6777 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6778 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6779 | `			SySetReset(&aArg);` |
|        - |  6780 | `			/* Pop given arguments */` |
|      ! 0 |  6781 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6782 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6783 | `			}` |
|        - |  6784 | `			/* Copy result */` |
|      ! 0 |  6785 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6786 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6787 | `		}else{` |
|        3 |  6788 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6789 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6790 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6791 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6792 | `			}else{` |
|        - |  6793 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6794 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6795 | `			}` |
|        - |  6796 | `			/* Pop given arguments */` |
|        3 |  6797 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6798 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6799 | `			}` |
|        - |  6800 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6801 | `			PH7_MemObjRelease(pTos);` |
|        - |  6802 | `		}` |
|   313853 |  6803 | `		break;` |
|        - |  6804 | `	}` |
|   628306 |  6805 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6806 | `	/* Check for a compiled function first.` |
|        - |  6807 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6808 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   628306 |  6809 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6810 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6811 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6812 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6813 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6814 | `	 * function calls inside namespaces. */` |
|   628306 |  6815 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6816 | `		const char *zFunc;` |
|        - |  6817 | `		const char *zEnd;` |
|        - |  6818 | `		const char *z;` |
|        - |  6819 | `		SyString sGlobal;` |
|       20 |  6820 | `		zFunc = sName.zString;` |
|       20 |  6821 | `		zEnd  = zFunc + sName.nByte;` |
|       20 |  6822 | `		z = zEnd;` |
|        - |  6823 | `		/* Find last namespace separator */` |
|      174 |  6824 | `		while( z > zFunc ){` |
|      174 |  6825 | `			if( z[-1] == '\\' ){` |
|       20 |  6826 | `				break;` |
|        - |  6827 | `			}` |
|      156 |  6828 | `			z--;` |
|        2 |  6829 | `		}` |
|       20 |  6830 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6831 | `			/* Retry lookup using the unqualified/global function name */` |
|       20 |  6832 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       20 |  6833 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        9 |  6834 | `		}` |
|        9 |  6835 | `	}` |
|   628306 |  6836 | `	if( pEntry ){` |
|        - |  6837 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6838 | `		ph7_class_instance *pThis;` |
|        - |  6839 | `		ph7_value *pFrameStack;` |
|        - |  6840 | `		ph7_vm_func *pVmFunc;` |
|        - |  6841 | `		ph7_class *pSelf;` |
|        - |  6842 | `		VmFrame *pFrame;` |
|        - |  6843 | `		ph7_value *pObj;` |
|        - |  6844 | `		VmSlot sArg;` |
|        - |  6845 | `		sxu32 n;` |
|        - |  6846 | `		/* initialize fields */` |
|    14396 |  6847 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    14396 |  6848 | `		pThis = 0;` |
|    14396 |  6849 | `		pSelf = 0;` |
|    14396 |  6850 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6851 | `			ph7_class_method *pMeth;` |
|        - |  6852 | `			/* Class method call */` |
|     2202 |  6853 | `			ph7_value *pTarget = &pTos[-1];` |
|     2202 |  6854 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6855 | `				/* Extract the 'this' pointer */` |
|     2202 |  6856 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6857 | `					/* Instance already loaded */` |
|     2116 |  6858 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2116 |  6859 | `					pThis->iRef++;` |
|     2116 |  6860 | `					pSelf = pThis->pClass;` |
|     1057 |  6861 | `				}` |
|     2202 |  6862 | `				if( pSelf == 0 ){` |
|       88 |  6863 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6864 | `						/* "Late Static Binding" class name */` |
|      122 |  6865 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       40 |  6866 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       40 |  6867 | `					}` |
|       88 |  6868 | `					if( pSelf == 0 ){` |
|       19 |  6869 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  6870 | `					}` |
|       43 |  6871 | `				}` |
|     2202 |  6872 | `				if( pThis == 0  ){` |
|       88 |  6873 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       88 |  6874 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       88 |  6875 | `					if( pFrameLocal->pParent ){` |
|        - |  6876 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  6877 | `						pThis = pFrameLocal->pThis;` |
|       64 |  6878 | `						if( pThis ){` |
|       19 |  6879 | `							pThis->iRef++;` |
|        9 |  6880 | `						}` |
|       31 |  6881 | `					}` |
|       43 |  6882 | `				}` |
|     2202 |  6883 | `				VmPopOperand(&pTos,1);` |
|     2202 |  6884 | `				PH7_MemObjRelease(pTos);` |
|        - |  6885 | `				/* Synchronize pointers */` |
|     2202 |  6886 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6887 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6888 | `				 * user have already computed the random generated unique class method name` |
|        - |  6889 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6890 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6891 | `				 */` |
|     2202 |  6892 | `				while( pArg < pStack ){` |
|      ! 0 |  6893 | `					pArg++;` |
|      ! 0 |  6894 | `				}` |
|     2202 |  6895 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6896 | `					/* Check if the call is allowed */` |
|     2202 |  6897 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2202 |  6898 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  6899 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  6900 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  6901 | `							char zMsg[256];` |
|      ! 0 |  6902 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6903 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  6904 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  6905 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  6906 | `							/* Pop given arguments */` |
|      ! 0 |  6907 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6908 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6909 | `							}` |
|      ! 0 |  6910 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6911 | `							goto Abort;` |
|        - |  6912 | `						}` |
|        6 |  6913 | `					}` |
|     1100 |  6914 | `				}` |
|     1100 |  6915 | `			}` |
|     1100 |  6916 | `		}` |
|        - |  6917 | `		/* Check The recursion limit */` |
|    14396 |  6918 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6919 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6920 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6921 | `				&pVmFunc->sName);` |
|        - |  6922 | `			/* Pop given arguments */` |
|        3 |  6923 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6924 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6925 | `			}` |
|        - |  6926 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6927 | `			PH7_MemObjRelease(pTos);` |
|       12 |  6928 | `			break;` |
|        - |  6929 | `		}` |
|    14394 |  6930 | `		if( pVmFunc->pNextName ){` |
|        - |  6931 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  6932 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  6933 | `		}` |
|    14394 |  6934 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6935 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6936 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6937 | `			ph7_generator *pGenerator;` |
|        - |  6938 | `			ph7_class_instance *pGenObj;` |
|        - |  6939 | `			ph7_value *pCtxAttr;` |
|        - |  6940 | `			SyString sAttrName;` |
|        - |  6941 | `			ph7_value **apCallArgs;` |
|        - |  6942 | `			int nGenArgs, iArg;` |
|        - |  6943 | `			/* Collect arguments from the operand stack */` |
|       20 |  6944 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6945 | `			apCallArgs = 0;` |
|       20 |  6946 | `			if( nGenArgs > 0 ){` |
|        8 |  6947 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6948 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6949 | `				if( apCallArgs == 0 ){` |
|        - |  6950 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6951 | `					nGenArgs = 0;` |
|      ! 0 |  6952 | `				}else{` |
|       12 |  6953 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6954 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6955 | `					}` |
|        - |  6956 | `				}` |
|        2 |  6957 | `			}` |
|        - |  6958 | `			/* Create execution context and generator wrapper */` |
|       20 |  6959 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6960 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6961 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6962 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6963 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6964 | `				break;` |
|        - |  6965 | `			}` |
|       20 |  6966 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6967 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6968 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6969 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6970 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6971 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6972 | `				break;` |
|        - |  6973 | `			}` |
|        - |  6974 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6975 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6976 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6977 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6978 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6979 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6980 | `			if( apCallArgs ){` |
|        6 |  6981 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6982 | `			}` |
|       20 |  6983 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6984 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6985 | `				if( pThis ){` |
|      ! 0 |  6986 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6987 | `				}` |
|      ! 0 |  6988 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6989 | `					goto Abort;` |
|        - |  6990 | `				}` |
|      ! 0 |  6991 | `				break;` |
|        - |  6992 | `			}` |
|        - |  6993 | `			/* Create Generator class instance */` |
|       20 |  6994 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6995 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6996 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6997 | `				break;` |
|        - |  6998 | `			}` |
|        - |  6999 | `			/* Store generator in __ctx attribute */` |
|       20 |  7000 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  7001 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  7002 | `			if( pCtxAttr ){` |
|       20 |  7003 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  7004 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  7005 | `			}` |
|        - |  7006 | `			/* Pop args and function name, push Generator object */` |
|       20 |  7007 | `			PH7_MemObjRelease(pTos);` |
|       20 |  7008 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  7009 | `			pTos->x.pOther = pGenObj;` |
|       20 |  7010 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  7011 | `			pGenObj->iRef++;` |
|       20 |  7012 | `			if( pThis ){` |
|      ! 0 |  7013 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7014 | `			}` |
|       20 |  7015 | `			break;` |
|        - |  7016 | `		}` |
|        - |  7017 | `		/* Extract the formal argument set */` |
|    14376 |  7018 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7019 | `		/* Create a new VM frame  */` |
|    14376 |  7020 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    14376 |  7021 | `		if( rc != SXRET_OK ){` |
|        - |  7022 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7023 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7024 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7025 | `				&pVmFunc->sName);` |
|        - |  7026 | `			/* Pop given arguments */` |
|      ! 0 |  7027 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7028 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7029 | `			}` |
|        - |  7030 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7031 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7032 | `			break;` |
|        - |  7033 | `		}` |
|    14376 |  7034 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7035 | `			/* Install the '$this' variable */` |
|        - |  7036 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2132 |  7037 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2132 |  7038 | `			if( pObj ){` |
|        - |  7039 | `				/* Reflect the change */` |
|     2132 |  7040 | `				pObj->x.pOther = pThis;` |
|     2132 |  7041 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1065 |  7042 | `			}` |
|     1065 |  7043 | `		}` |
|    14376 |  7044 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7045 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7046 | `			/* Install static variables */` |
|      ! 0 |  7047 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7048 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7049 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7050 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7051 | `					/* Initialize the static variables */` |
|      ! 0 |  7052 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7053 | `					if( pObj ){` |
|        - |  7054 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7055 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7056 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7057 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7058 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7059 | `						}` |
|      ! 0 |  7060 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7061 | `					}else{` |
|      ! 0 |  7062 | `						continue;` |
|        - |  7063 | `					}` |
|      ! 0 |  7064 | `				}` |
|        - |  7065 | `				/* Install in the current frame */` |
|      ! 0 |  7066 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7067 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7068 | `			}` |
|      ! 0 |  7069 | `		}` |
|        - |  7070 | `		/* Push arguments in the local frame */` |
|    14376 |  7071 | `		n = 0;` |
|    38728 |  7072 | `		while( pArg < pTos ){` |
|    24392 |  7073 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7074 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       30 |  7075 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       30 |  7076 | `				if( pObj ){` |
|        - |  7077 | `					/* Initialize as empty array */` |
|       30 |  7078 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7079 | `					{` |
|       30 |  7080 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      114 |  7081 | `						while( pArg < pTos ){` |
|        - |  7082 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7083 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      100 |  7084 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7085 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7086 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7087 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7088 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7089 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7090 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7091 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7092 | `										goto Abort;` |
|        - |  7093 | `									}` |
|        - |  7094 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7095 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7096 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7097 | `									pFrameStack = 0;` |
|      ! 0 |  7098 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7099 | `									goto SkipFuncBody;` |
|      ! 0 |  7100 | `								}else{` |
|       13 |  7101 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7102 | `									if( xCast ){` |
|       13 |  7103 | `										xCast(pArg);` |
|        6 |  7104 | `									}` |
|        - |  7105 | `								}` |
|        6 |  7106 | `							}` |
|       86 |  7107 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       86 |  7108 | `							pArg++;` |
|        2 |  7109 | `						}` |
|        - |  7110 | `					}` |
|       30 |  7111 | `					sArg.nIdx = pObj->nIdx;` |
|       30 |  7112 | `					sArg.pUserData = 0;` |
|       30 |  7113 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       14 |  7114 | `				}` |
|       30 |  7115 | `				break; /* All remaining args consumed */` |
|        - |  7116 | `			}` |
|    24364 |  7117 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    24208 |  7118 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       11 |  7119 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7120 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7121 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7122 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7123 | `						goto Abort;` |
|        - |  7124 | `					}` |
|      ! 0 |  7125 | `				}` |
|        - |  7126 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7127 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    24222 |  7128 | `				if( aFormalArg[n].nType > 0` |
|    12711 |  7129 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1198 |  7130 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7131 | `						/* Argument must be a class instance [i.e: object] */` |
|       16 |  7132 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7133 | `						ph7_class *pClass;` |
|        - |  7134 | `						/* Try to extract the desired class */` |
|       16 |  7135 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  7136 | `						if( pClass ){` |
|       16 |  7137 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7138 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7139 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7140 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7141 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7142 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7143 | `								}` |
|      ! 0 |  7144 | `							}else{` |
|        - |  7145 | `								/* reuse pThis declared in outer scope */` |
|       16 |  7146 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7147 | `								/* Make sure the object is an instance of the given class */` |
|       16 |  7148 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7149 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7150 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7151 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7152 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7153 | `								}` |
|        - |  7154 | `							}` |
|        9 |  7155 | `						}` |
|     1191 |  7156 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7157 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7158 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7159 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7160 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7161 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7162 | `								goto Abort;` |
|        - |  7163 | `							}` |
|        - |  7164 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7165 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7166 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7167 | `							pFrameStack = 0;` |
|       11 |  7168 | `							rc = PH7_EXCEPTION;` |
|       11 |  7169 | `							goto SkipFuncBody;` |
|      ! 0 |  7170 | `						}else{` |
|      ! 0 |  7171 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7172 | `							/* Cast to the desired type */` |
|      ! 0 |  7173 | `							xCast(pArg);` |
|        - |  7174 | `						}` |
|      ! 0 |  7175 | `					}` |
|      593 |  7176 | `				}` |
|    24200 |  7177 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  7178 | `					/* Pass by reference */` |
|       54 |  7179 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  7180 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  7181 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  7182 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7183 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7184 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7185 | `						}` |
|        - |  7186 | `						/* Switch to pass by value */` |
|      ! 0 |  7187 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7188 | `					}else{` |
|        - |  7189 | `						SyHashEntry *pRefEntry;` |
|        - |  7190 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  7191 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  7192 | `						if( pRefEntry == 0 ){` |
|       80 |  7193 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  7194 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  7195 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  7196 | `							sArg.pUserData = 0;` |
|       54 |  7197 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  7198 | `						}` |
|       54 |  7199 | `						pObj = 0;` |
|        - |  7200 | `					}` |
|       28 |  7201 | `				}else{` |
|        - |  7202 | `					/* Pass by value,make a copy of the given argument */` |
|    24148 |  7203 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7204 | `				}` |
|    12101 |  7205 | `			}else{` |
|        - |  7206 | `				char zName[32];` |
|        - |  7207 | `				SyString sArgName;` |
|        - |  7208 | `				/* Set a dummy name */` |
|      156 |  7209 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  7210 | `				sArgName.zString = zName;` |
|        - |  7211 | `				/* Annonymous argument */` |
|      156 |  7212 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  7213 | `			}` |
|    24354 |  7214 | `			if( pObj ){` |
|    24302 |  7215 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  7216 | `				/* Insert argument index  */` |
|    24302 |  7217 | `				sArg.nIdx = pObj->nIdx;` |
|    24302 |  7218 | `				sArg.pUserData = 0;` |
|    24302 |  7219 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    12150 |  7220 | `			}` |
|    24354 |  7221 | `			PH7_MemObjRelease(pArg);` |
|    24354 |  7222 | `			pArg++;` |
|    24354 |  7223 | `			++n;` |
|        2 |  7224 | `		}` |
|        - |  7225 | `		/* Set up closure environment */` |
|    14366 |  7226 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7227 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  7228 | `			ph7_value *pValue;` |
|        - |  7229 | `			sxu32 iEnv;` |
|      111 |  7230 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      287 |  7231 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      177 |  7232 | `				pEnv = &aEnv[iEnv];` |
|      177 |  7233 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  7234 | `					/* Do not install null value */` |
|      105 |  7235 | `					continue;` |
|        - |  7236 | `				}` |
|       73 |  7237 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  7238 | `				if( pValue == 0 ){` |
|      ! 0 |  7239 | `					continue;` |
|        - |  7240 | `				}` |
|        - |  7241 | `				/* Invalidate any prior representation */` |
|       73 |  7242 | `				PH7_MemObjRelease(pValue);` |
|        - |  7243 | `				/* Duplicate bound variable value */` |
|       73 |  7244 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  7245 | `			}` |
|       55 |  7246 | `		}` |
|        - |  7247 | `		/* Process default values for remaining formal parameters */` |
|    16482 |  7248 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2154 |  7249 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7250 | `				/* Variadic parameter with no extra args — create empty array */` |
|       38 |  7251 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       38 |  7252 | `				if( pObj ){` |
|       38 |  7253 | `					PH7_MemObjToHashmap(pObj);` |
|       38 |  7254 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  7255 | `					sArg.pUserData = 0;` |
|       38 |  7256 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  7257 | `				}` |
|       38 |  7258 | `				n++;` |
|       38 |  7259 | `				break; /* Variadic is always last */` |
|        - |  7260 | `			}` |
|     2118 |  7261 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2112 |  7262 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2112 |  7263 | `				if( pObj ){` |
|        - |  7264 | `					/* Evaluate the default value and extract it's result */` |
|     2112 |  7265 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2112 |  7266 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7267 | `						goto Abort;` |
|        - |  7268 | `					}` |
|        - |  7269 | `					/* Insert argument index */` |
|     2112 |  7270 | `					sArg.nIdx = pObj->nIdx;` |
|     2112 |  7271 | `					sArg.pUserData = 0;` |
|     2112 |  7272 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  7273 | `					/* Make sure the default argument is of the correct type */` |
|     2110 |  7274 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1478 |  7275 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  7276 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7277 | `						/* Cast to the desired type */` |
|      ! 0 |  7278 | `						xCast(pObj);` |
|      ! 0 |  7279 | `					}` |
|     1055 |  7280 | `				}` |
|     1055 |  7281 | `			}` |
|     2118 |  7282 | `			++n;` |
|        2 |  7283 | `		}` |
|        - |  7284 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  7285 | `		 * does not return anything.` |
|        - |  7286 | `		 */` |
|    14366 |  7287 | `		PH7_MemObjRelease(pTos);` |
|    14366 |  7288 | `		pTos = &pTos[-nCallArgs];` |
|        - |  7289 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    14366 |  7290 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    14366 |  7291 | `		if( pFrameStack == 0 ){` |
|        - |  7292 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7293 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7294 | `				&pVmFunc->sName);` |
|      ! 0 |  7295 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7296 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7297 | `			}` |
|      ! 0 |  7298 | `			break;` |
|        - |  7299 | `		}` |
|     7182 |  7300 | `SkipFuncBody:` |
|    14376 |  7301 | `		if( pSelf ){` |
|        - |  7302 | `			/* Push class name */` |
|     2200 |  7303 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1099 |  7304 | `		}` |
|        - |  7305 | `		/* Increment nesting level */` |
|    14376 |  7306 | `		pVm->nRecursionDepth++;` |
|    14376 |  7307 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  7308 | `			/* Execute function body */` |
|    14366 |  7309 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     7182 |  7310 | `		}` |
|        - |  7311 | `		/* Decrement nesting level */` |
|    14376 |  7312 | `		pVm->nRecursionDepth--;` |
|    14376 |  7313 | `		if( pSelf ){` |
|        - |  7314 | `			/* Pop class name */` |
|     2200 |  7315 | `			(void)SySetPop(&pVm->aSelf);` |
|     1099 |  7316 | `		}` |
|        - |  7317 | `		/* Cleanup the mess left behind */` |
|    14376 |  7318 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  7319 | `			/* Return by reference,reflect that */` |
|        9 |  7320 | `			if( n != SXU32_HIGH ){` |
|        9 |  7321 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  7322 | `				sxu32 i;` |
|        - |  7323 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  7324 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  7325 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  7326 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  7327 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7328 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7329 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  7330 | `								&pVmFunc->sName);` |
|      ! 0 |  7331 | `						}` |
|      ! 0 |  7332 | `						n = SXU32_HIGH;` |
|      ! 0 |  7333 | `						break;` |
|        - |  7334 | `					}` |
|        3 |  7335 | `				}` |
|        5 |  7336 | `			}else{` |
|      ! 0 |  7337 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7338 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7339 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  7340 | `						&pVmFunc->sName);` |
|      ! 0 |  7341 | `				}` |
|        - |  7342 | `			}` |
|        9 |  7343 | `			pTos->nIdx = n;` |
|        4 |  7344 | `		}` |
|        - |  7345 | `		/* Cleanup the mess left behind */` |
|    14376 |  7346 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  7347 | `			/* An exception was throw in this frame */` |
|       22 |  7348 | `			pFrame = pFrame->pParent;` |
|       22 |  7349 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  7350 | `				/* Pop the resutlt */` |
|       20 |  7351 | `				VmPopOperand(&pTos,1);` |
|        - |  7352 | `				/* Jump to this destination */` |
|       20 |  7353 | `				pc = pFrame->iExceptionJump - 1;` |
|       20 |  7354 | `				rc = PH7_OK;` |
|       11 |  7355 | `			}else{` |
|        3 |  7356 | `				if( pFrame->pParent ){` |
|        3 |  7357 | `					rc = PH7_EXCEPTION;` |
|        2 |  7358 | `				}else{` |
|        - |  7359 | `					/* Continue normal execution */` |
|      ! 0 |  7360 | `					rc = PH7_OK;` |
|        - |  7361 | `				}` |
|        - |  7362 | `			}` |
|       10 |  7363 | `		}` |
|        - |  7364 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    14376 |  7365 | `		if( pFrameStack ){` |
|    14366 |  7366 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     7182 |  7367 | `		}` |
|        - |  7368 | `		/* Leave the frame */` |
|    14376 |  7369 | `		VmLeaveFrame(&(*pVm));` |
|    14376 |  7370 | `		if( rc == PH7_ABORT ){` |
|        - |  7371 | `			/* Abort processing immeditaley */` |
|        9 |  7372 | `			goto Abort;` |
|    14368 |  7373 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  7374 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  7375 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  7376 | `			 * overwriting the state saved by the inner level.` |
|        - |  7377 | `			 * pTos points to the result slot (not yet written).` |
|        - |  7378 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  7379 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  7380 | `			goto Suspend;` |
|    14330 |  7381 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  7382 | `			goto Exception;` |
|        - |  7383 | `		}` |
|     7165 |  7384 | `	}else{` |
|        - |  7385 | `		ph7_user_func *pFunc;` |
|        - |  7386 | `		ph7_context sCtx;` |
|        - |  7387 | `		ph7_value sRet;` |
|        - |  7388 | `		/* Look for an installed foreign function.` |
|        - |  7389 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  7390 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  7391 | `		 * extract the short name (last component after \) and try that.` |
|        - |  7392 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  7393 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  7394 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   613912 |  7395 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   613912 |  7396 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  7397 | `			/* Compiler-qualified: try short name as global fallback */` |
|       20 |  7398 | `			const char *zShort = sName.zString;` |
|        - |  7399 | `			sxu32 i;` |
|      296 |  7400 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      278 |  7401 | `				if( sName.zString[i] == '\\' ){` |
|       24 |  7402 | `					zShort = &sName.zString[i + 1];` |
|       11 |  7403 | `				}` |
|      140 |  7404 | `			}` |
|       20 |  7405 | `			if( zShort != sName.zString ){` |
|       20 |  7406 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       20 |  7407 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        9 |  7408 | `			}` |
|        9 |  7409 | `		}` |
|   613912 |  7410 | `		if( pEntry == 0 ){` |
|        - |  7411 | `			/* Call to undefined function */` |
|        5 |  7412 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  7413 | `			/* Pop given arguments */` |
|        5 |  7414 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  7415 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7416 | `			}` |
|        - |  7417 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  7418 | `			PH7_MemObjRelease(pTos);` |
|        8 |  7419 | `			break;` |
|        - |  7420 | `		}` |
|   613908 |  7421 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  7422 | `		/* Start collecting function arguments */` |
|   613908 |  7423 | `		SySetReset(&aArg);` |
|  1650894 |  7424 | `		while( pArg < pTos ){` |
|  1036988 |  7425 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1036988 |  7426 | `			pArg++;` |
|        2 |  7427 | `		}` |
|        - |  7428 | `		/* Assume a null return value */` |
|   613908 |  7429 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  7430 | `		/* Init the call context */` |
|   613908 |  7431 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  7432 | `		/* Call the foreign function */` |
|   613908 |  7433 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  7434 | `		/* Release the call context */` |
|   613908 |  7435 | `		VmReleaseCallContext(&sCtx);` |
|   613908 |  7436 | `		if( rc == PH7_ABORT ){` |
|      471 |  7437 | `			goto Abort;` |
|   613438 |  7438 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  7439 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  7440 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  7441 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  7442 | `				/* Exception was NOT caught, propagate */` |
|        5 |  7443 | `				goto Exception;` |
|        - |  7444 | `			}` |
|        - |  7445 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  7446 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  7447 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  7448 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  7449 | `			}` |
|        - |  7450 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  7451 | `			VmPopOperand(&pTos,1);` |
|        - |  7452 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  7453 | `			pFrm = pVm->pFrame;` |
|        7 |  7454 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  7455 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  7456 | `			}` |
|        7 |  7457 | `			break;` |
|        - |  7458 | `		}` |
|   613428 |  7459 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  7460 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  7461 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  7462 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  7463 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  7464 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  7465 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  7466 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  7467 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  7468 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  7469 | `			}` |
|        - |  7470 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  7471 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  7472 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  7473 | `			goto Suspend;` |
|        - |  7474 | `		}` |
|   613390 |  7475 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7476 | `			/* Pop function name and arguments */` |
|   593908 |  7477 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   296975 |  7478 | `		}` |
|        - |  7479 | `		/* Save foreign function return value */` |
|   613390 |  7480 | `		PH7_MemObjStore(&sRet,pTos);` |
|   613390 |  7481 | `		PH7_MemObjRelease(&sRet);` |
|        - |  7482 | `	}` |
|   627716 |  7483 | `	break;` |
|        - |  7484 | `				  }` |
|        - |  7485 | `/*` |
|        - |  7486 | ` * OP_CONSUME: P1 * *` |
|        - |  7487 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  7488 | ` */` |
|    12759 |  7489 | `case PH7_OP_CONSUME: {` |
|    25520 |  7490 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    25520 |  7491 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  7492 |  |
|    25520 |  7493 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    25520 |  7494 | `	pCur = pOut;` |
|        - |  7495 | `	/* Start the consume process  */` |
|    51038 |  7496 | `	while( pOut <= pTos ){` |
|        - |  7497 | `		/* Force a string cast */` |
|    25520 |  7498 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      438 |  7499 | `			PH7_MemObjToString(pOut);` |
|      218 |  7500 | `		}` |
|    25520 |  7501 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  7502 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  7503 | `			/* Invoke the output consumer callback */` |
|    14536 |  7504 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    14536 |  7505 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    14536 |  7506 | `			SyBlobRelease(&pOut->sBlob);` |
|    14536 |  7507 | `			if( rc == SXERR_ABORT ){` |
|        - |  7508 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  7509 | `				goto Abort;` |
|        - |  7510 | `			}` |
|     7267 |  7511 | `		}` |
|    25520 |  7512 | `		pOut++;` |
|        2 |  7513 | `	}` |
|    25520 |  7514 | `	pTos = &pCur[-1];` |
|    25518 |  7515 | `	break;` |
|        - |  7516 | `					 }` |
|        - |  7517 |  |
|        - |  7518 | `		} /* Switch() */` |
| 10526994 |  7519 | `		pc++; /* Next instruction in the stream */` |
|        2 |  7520 | `	} /* For(;;) */` |
|    17481 |  7521 | `Done:` |
|    34964 |  7522 | `	SySetRelease(&aArg);` |
|    34964 |  7523 | `	return SXRET_OK;` |
|       66 |  7524 | `Suspend:` |
|      134 |  7525 | `	SySetRelease(&aArg);` |
|      134 |  7526 | `	return PH7_SUSPEND;` |
|      245 |  7527 | `Abort:` |
|      491 |  7528 | `	SySetRelease(&aArg);` |
|     1697 |  7529 | `	while( pTos >= pStack ){` |
|     1207 |  7530 | `		PH7_MemObjRelease(pTos);` |
|     1207 |  7531 | `		pTos--;` |
|        1 |  7532 | `	}` |
|      491 |  7533 | `	return PH7_ABORT;` |
|        3 |  7534 | `Exception:` |
|        8 |  7535 | `	SySetRelease(&aArg);` |
|       22 |  7536 | `	while( pTos >= pStack ){` |
|       16 |  7537 | `		PH7_MemObjRelease(pTos);` |
|       16 |  7538 | `		pTos--;` |
|        2 |  7539 | `	}` |
|        8 |  7540 | `	return PH7_EXCEPTION;` |
|    17797 |  7541 |  |
|        - |  7542 | `/*` |
|        - |  7543 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  7544 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7545 | ` * See block-comment on that function for additional information.` |
|        - |  7546 | ` */` |
|    16570 |  7547 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  7548 |  |
|        - |  7549 | `	ph7_value *pStack;` |
|        - |  7550 | `	sxi32 rc;` |
|        - |  7551 | `	/* Allocate a new operand stack */` |
|    16572 |  7552 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    16572 |  7553 | `	if( pStack == 0 ){` |
|      ! 0 |  7554 | `		return SXERR_MEM;` |
|        - |  7555 | `	}` |
|        - |  7556 | `	/* Execute the program */` |
|    16572 |  7557 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  7558 | `	/* Free the operand stack */` |
|    16572 |  7559 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  7560 | `	/* Execution result */` |
|    16572 |  7561 | `	return rc;` |
|     8287 |  7562 |  |
|        - |  7563 | `/*` |
|        - |  7564 | ` * Invoke any installed shutdown callbacks.` |
|        - |  7565 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  7566 | ` * or more calls to [register_shutdown_function()].` |
|        - |  7567 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  7568 | ` * execution ends.` |
|        - |  7569 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  7570 | ` * additional information.` |
|        - |  7571 | ` */` |
|     2466 |  7572 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  7573 |  |
|        - |  7574 | `	VmShutdownCB *pEntry;` |
|        - |  7575 | `	ph7_value *apArg[10];` |
|        - |  7576 | `	sxu32 n,nEntry;` |
|        - |  7577 | `	int i;` |
|        - |  7578 | `	/* Point to the stack of registered callbacks */` |
|     2468 |  7579 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27128 |  7580 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    24662 |  7581 | `		apArg[i] = 0;` |
|    12332 |  7582 | `	}` |
|     2470 |  7583 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  7584 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7585 | `		if( pEntry ){` |
|        - |  7586 | `			/* Prepare callback arguments if any */` |
|        3 |  7587 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  7588 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  7589 | `					break;` |
|        - |  7590 | `				}` |
|      ! 0 |  7591 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  7592 | `			}` |
|        - |  7593 | `			/* Invoke the callback */` |
|        3 |  7594 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  7595 | `			/*` |
|        - |  7596 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  7597 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  7598 | `			 */` |
|        3 |  7599 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7600 | `			if( pEntry ){` |
|        3 |  7601 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  7602 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  7603 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  7604 | `				}` |
|        1 |  7605 | `			}` |
|        1 |  7606 | `		}` |
|        2 |  7607 | `	}` |
|     2468 |  7608 | `	SySetReset(&pVm->aShutdown);` |
|     2468 |  7609 |  |
|        - |  7610 | `/*` |
|        - |  7611 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7612 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7613 | ` * See block-comment on that function for additional information.` |
|        - |  7614 | ` */` |
|     2474 |  7615 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7616 |  |
|        - |  7617 | `	/* Make sure we are ready to execute this program */` |
|     2476 |  7618 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7619 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7620 | `	}` |
|        - |  7621 | `	/* Set the execution magic number  */` |
|     2476 |  7622 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7623 | `	/* Execute the program */` |
|     2476 |  7624 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7625 | `	/* Invoke any shutdown callbacks */` |
|     2472 |  7626 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7627 | `	/*` |
|        - |  7628 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7629 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7630 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7631 | `	 */` |
|     2472 |  7632 | `	return SXRET_OK;` |
|     1239 |  7633 |  |
|        - |  7634 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7635 | `/*` |
|        - |  7636 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7637 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7638 | ` */` |
|       42 |  7639 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7640 |  |
|        - |  7641 | `	ph7_exec_ctx *pCtx;` |
|        - |  7642 | `	ph7_value *pStack;` |
|        - |  7643 | `	VmFrame *pFrame;` |
|       44 |  7644 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7645 | `	if( pCtx == 0 ){` |
|      ! 0 |  7646 | `		return 0;` |
|        - |  7647 | `	}` |
|       44 |  7648 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7649 | `	pCtx->pVm = pVm;` |
|       44 |  7650 | `	pCtx->pFunc = pFunc;` |
|       44 |  7651 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7652 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7653 | `	pCtx->pc = 0;` |
|       44 |  7654 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7655 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7656 | `	/* Allocate a private operand stack */` |
|       44 |  7657 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7658 | `	if( pStack == 0 ){` |
|      ! 0 |  7659 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7660 | `		return 0;` |
|        - |  7661 | `	}` |
|       44 |  7662 | `	pCtx->pStack = pStack;` |
|        - |  7663 | `	/* Create a detached frame for the fiber */` |
|       44 |  7664 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7665 | `	if( pFrame == 0 ){` |
|      ! 0 |  7666 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7667 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7668 | `		return 0;` |
|        - |  7669 | `	}` |
|       44 |  7670 | `	pCtx->pFrame = pFrame;` |
|       44 |  7671 | `	return pCtx;` |
|       23 |  7672 |  |
|        - |  7673 | `/*` |
|        - |  7674 | ` * Start executing a fiber context for the first time.` |
|        - |  7675 | ` */` |
|       42 |  7676 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7677 |  |
|        - |  7678 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7679 | `	sxi32 rc;` |
|       44 |  7680 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7681 | `		return SXERR_INVALID;` |
|        - |  7682 | `	}` |
|        - |  7683 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7684 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7685 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7686 | `	/* Save and set the active context */` |
|       44 |  7687 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7688 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7689 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7690 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7691 | `	pVm->nRecursionDepth++;` |
|        - |  7692 | `	/* Execute from the beginning */` |
|       65 |  7693 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7694 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7695 | `	pVm->nRecursionDepth--;` |
|        - |  7696 | `	/* Restore the previous context */` |
|       44 |  7697 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7698 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7699 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7700 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7701 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7702 | `		if( pResult ){` |
|       24 |  7703 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7704 | `		}` |
|       42 |  7705 | `		return SXRET_OK;` |
|        - |  7706 | `	}` |
|        - |  7707 | `	/* Detach frame */` |
|        3 |  7708 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7709 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7710 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7711 | `	}` |
|        3 |  7712 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7713 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7714 | `		return PH7_ABORT;` |
|        - |  7715 | `	}` |
|        3 |  7716 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7717 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7718 | `		return PH7_EXCEPTION;` |
|        - |  7719 | `	}` |
|        - |  7720 | `	/* Normal completion */` |
|        3 |  7721 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7722 | `	if( pResult ){` |
|        3 |  7723 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7724 | `	}` |
|        3 |  7725 | `	return SXRET_OK;` |
|       23 |  7726 |  |
|        - |  7727 | `/*` |
|        - |  7728 | ` * Resume a suspended fiber context.` |
|        - |  7729 | ` */` |
|       86 |  7730 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7731 |  |
|        - |  7732 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7733 | `	sxi32 rc;` |
|       88 |  7734 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7735 | `		return SXERR_INVALID;` |
|        - |  7736 | `	}` |
|        - |  7737 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7738 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7739 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7740 | `	if( pResumeValue ){` |
|       40 |  7741 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7742 | `	}else{` |
|       50 |  7743 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7744 | `	}` |
|       88 |  7745 | `	pCtx->nTos++;` |
|        - |  7746 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7747 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7748 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7749 | `	/* Save and set the active context */` |
|       88 |  7750 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7751 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7752 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7753 | `	pVm->nRecursionDepth++;` |
|        - |  7754 | `	/* Resume execution from saved PC */` |
|      131 |  7755 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7756 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7757 | `	pVm->nRecursionDepth--;` |
|        - |  7758 | `	/* Restore the previous context */` |
|       88 |  7759 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7760 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7761 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7762 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7763 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7764 | `		if( pResult ){` |
|       18 |  7765 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7766 | `		}` |
|       56 |  7767 | `		return SXRET_OK;` |
|        - |  7768 | `	}` |
|        - |  7769 | `	/* Detach frame */` |
|       34 |  7770 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7771 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7772 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7773 | `	}` |
|       34 |  7774 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7775 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7776 | `		return PH7_ABORT;` |
|        - |  7777 | `	}` |
|       34 |  7778 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7779 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7780 | `		return PH7_EXCEPTION;` |
|        - |  7781 | `	}` |
|        - |  7782 | `	/* Normal completion */` |
|       34 |  7783 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7784 | `	if( pResult ){` |
|       20 |  7785 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7786 | `	}` |
|       34 |  7787 | `	return SXRET_OK;` |
|       45 |  7788 |  |
|        - |  7789 | `/*` |
|        - |  7790 | ` * Release an execution context and all its resources.` |
|        - |  7791 | ` */` |
|        4 |  7792 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7793 |  |
|        5 |  7794 | `	if( pCtx == 0 ){` |
|      ! 0 |  7795 | `		return;` |
|        - |  7796 | `	}` |
|        5 |  7797 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7798 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7799 | `		return;` |
|        - |  7800 | `	}` |
|        5 |  7801 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7802 | `	/* Release values */` |
|        5 |  7803 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7804 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7805 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7806 | `	if( pCtx->pFrame ){` |
|        - |  7807 | `		VmSlot *aSlot;` |
|        - |  7808 | `		sxu32 n;` |
|        - |  7809 | `		/* Free local variables */` |
|        5 |  7810 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7811 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7812 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7813 | `		}` |
|        - |  7814 | `		/* Remove local references */` |
|        5 |  7815 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7816 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7817 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7818 | `		}` |
|        5 |  7819 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7820 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7821 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7822 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7823 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7824 | `		pCtx->pFrame = 0;` |
|        2 |  7825 | `	}` |
|        - |  7826 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7827 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7828 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7829 | `	if( pCtx->pStack ){` |
|        5 |  7830 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7831 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7832 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7833 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7834 | `				pTos--;` |
|        1 |  7835 | `			}` |
|        2 |  7836 | `		}` |
|        5 |  7837 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7838 | `		pCtx->pStack = 0;` |
|        2 |  7839 | `	}` |
|        - |  7840 | `	/* Free the context itself */` |
|        5 |  7841 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7842 |  |
|        - |  7843 | `/*` |
|        - |  7844 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7845 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7846 | ` */` |
|       90 |  7847 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7848 |  |
|        - |  7849 | `	ph7_class_instance *pThis;` |
|        - |  7850 | `	SyString sAttr;` |
|        - |  7851 | `	ph7_value *pAttr;` |
|       92 |  7852 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7853 | `		return 0;` |
|        - |  7854 | `	}` |
|       92 |  7855 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7856 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7857 | `		return 0;` |
|        - |  7858 | `	}` |
|       92 |  7859 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7860 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7861 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7862 | `		return 0;` |
|        - |  7863 | `	}` |
|       62 |  7864 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7865 |  |
|        - |  7866 | `/*` |
|        - |  7867 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7868 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7869 | ` */` |
|       38 |  7870 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7871 |  |
|       40 |  7872 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7873 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7874 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7875 | `			"Cannot suspend outside of a fiber");` |
|        - |  7876 | `	}` |
|       40 |  7877 | `	if( nArg > 0 ){` |
|       40 |  7878 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7879 | `	}else{` |
|      ! 0 |  7880 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7881 | `	}` |
|       40 |  7882 | `	return PH7_SUSPEND;` |
|       21 |  7883 |  |
|        - |  7884 | `/*` |
|        - |  7885 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7886 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7887 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7888 | ` */` |
|       24 |  7889 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7890 |  |
|        - |  7891 | `	ph7_class_instance *pThis;` |
|        - |  7892 | `	ph7_value *pAttr;` |
|        - |  7893 | `	SyString sAttrName;` |
|       26 |  7894 | `	if( nArg < 2 ){` |
|      ! 0 |  7895 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7896 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7897 | `	}` |
|       26 |  7898 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7899 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7900 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7901 | `	}` |
|       26 |  7902 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7903 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7904 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7905 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7906 | `	}` |
|        - |  7907 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7908 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7909 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7910 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7911 | `	}` |
|        - |  7912 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7913 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7914 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7915 | `	if( pAttr ){` |
|       26 |  7916 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7917 | `	}` |
|       26 |  7918 | `	return PH7_OK;` |
|       14 |  7919 |  |
|        - |  7920 | `/*` |
|        - |  7921 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7922 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7923 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7924 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7925 | ` */` |
|       24 |  7926 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7927 | `	ph7_class_instance **ppThis)` |
|        2 |  7928 |  |
|       26 |  7929 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7930 | `	ph7_value *pCallable;` |
|        - |  7931 | `	SyString sAttrName;` |
|       26 |  7932 | `	*ppThis = 0;` |
|       26 |  7933 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7934 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7935 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7936 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7937 | `		return 0;` |
|        - |  7938 | `	}` |
|       26 |  7939 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7940 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7941 | `		SyString sName;` |
|        - |  7942 | `		SyHashEntry *pEntry;` |
|        - |  7943 | `		ph7_vm_func *pFunc;` |
|       26 |  7944 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7945 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7946 | `		if( pEntry == 0 ){` |
|      ! 0 |  7947 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7948 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7949 | `			return 0;` |
|        - |  7950 | `		}` |
|       26 |  7951 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7952 | `		return pFunc;` |
|      ! 0 |  7953 | `	}else{` |
|        - |  7954 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7955 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7956 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7957 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7958 | `		if( pMethod == 0 ){` |
|      ! 0 |  7959 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7960 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7961 | `			return 0;` |
|        - |  7962 | `		}` |
|      ! 0 |  7963 | `		*ppThis = pClosure;` |
|      ! 0 |  7964 | `		return &pMethod->sFunc;` |
|        - |  7965 | `	}` |
|       14 |  7966 |  |
|        - |  7967 | `/*` |
|        - |  7968 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7969 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7970 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7971 | ` */` |
|       42 |  7972 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7973 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7974 |  |
|       44 |  7975 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7976 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7977 | `	sxu32 nFormal, n;` |
|        - |  7978 | `	VmSlot sSlot;` |
|        - |  7979 | `	sxi32 rc;` |
|        - |  7980 | `	/* Install $this for closure/method callables */` |
|       44 |  7981 | `	if( pClosureThis ){` |
|        - |  7982 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7983 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7984 | `		if( pObj ){` |
|      ! 0 |  7985 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7986 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7987 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7988 | `		}` |
|      ! 0 |  7989 | `	}` |
|        - |  7990 | `	/* Install static variables */` |
|       44 |  7991 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7992 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7993 | `		ph7_value *pVal;` |
|      ! 0 |  7994 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7995 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7996 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7997 | `			if( pVal ){` |
|      ! 0 |  7998 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7999 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8000 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8001 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8002 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8003 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8004 | `				}` |
|      ! 0 |  8005 | `			}` |
|      ! 0 |  8006 | `		}` |
|      ! 0 |  8007 | `	}` |
|        - |  8008 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  8009 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  8010 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  8011 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8012 | `		ph7_value *pObj;` |
|       12 |  8013 | `		if( n < (sxu32)nArg ){` |
|        - |  8014 | `			/* Argument provided — install with type casting */` |
|       12 |  8015 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  8016 | `			if( pObj ){` |
|       12 |  8017 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8018 | `				/* Type casting */` |
|       12 |  8019 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8020 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8021 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8022 | `						if( xCast ){` |
|      ! 0 |  8023 | `							xCast(pObj);` |
|      ! 0 |  8024 | `						}` |
|      ! 0 |  8025 | `					}` |
|      ! 0 |  8026 | `				}` |
|       12 |  8027 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  8028 | `				sSlot.pUserData = 0;` |
|       12 |  8029 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  8030 | `			}` |
|        5 |  8031 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8032 | `			/* Default value */` |
|      ! 0 |  8033 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8034 | `			if( pObj ){` |
|      ! 0 |  8035 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8036 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8037 | `					return rc;` |
|        - |  8038 | `				}` |
|      ! 0 |  8039 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8040 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8041 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8042 | `						if( xCast ){` |
|      ! 0 |  8043 | `							xCast(pObj);` |
|      ! 0 |  8044 | `						}` |
|      ! 0 |  8045 | `					}` |
|      ! 0 |  8046 | `				}` |
|      ! 0 |  8047 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8048 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8049 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8050 | `			}` |
|      ! 0 |  8051 | `		}` |
|        7 |  8052 | `	}` |
|        - |  8053 | `	/* Install closure environment (captured variables) */` |
|       44 |  8054 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8055 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8056 | `		ph7_value *pValue;` |
|        - |  8057 | `		sxu32 iEnv;` |
|        3 |  8058 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8059 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8060 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8061 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8062 | `				continue;` |
|        - |  8063 | `			}` |
|        5 |  8064 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8065 | `			if( pValue == 0 ){` |
|      ! 0 |  8066 | `				continue;` |
|        - |  8067 | `			}` |
|        5 |  8068 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8069 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8070 | `		}` |
|        1 |  8071 | `	}` |
|       44 |  8072 | `	return SXRET_OK;` |
|       23 |  8073 |  |
|        - |  8074 | `/*` |
|        - |  8075 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8076 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8077 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8078 | ` */` |
|       26 |  8079 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8080 |  |
|       28 |  8081 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8082 | `	ph7_class_instance *pThis;` |
|        - |  8083 | `	ph7_class_instance *pClosureThis;` |
|        - |  8084 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8085 | `	ph7_vm_func *pFunc;` |
|        - |  8086 | `	ph7_value sResult;` |
|        - |  8087 | `	ph7_value *pCtxAttr;` |
|        - |  8088 | `	SyString sAttrName;` |
|        - |  8089 | `	sxi32 rc;` |
|       28 |  8090 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8091 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8092 | `	}` |
|       28 |  8093 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8094 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8095 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8096 | `	if( pExecCtx != 0 ){` |
|        3 |  8097 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8098 | `			"Cannot start a fiber that has already been started");` |
|        - |  8099 | `	}` |
|        - |  8100 | `	/* Resolve callable */` |
|       26 |  8101 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8102 | `	if( pFunc == 0 ){` |
|      ! 0 |  8103 | `		return PH7_EXCEPTION;` |
|        - |  8104 | `	}` |
|        - |  8105 | `	/* Create execution context now that we know the function */` |
|       26 |  8106 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8107 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8108 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8109 | `			"Fiber::start(): out of memory");` |
|        - |  8110 | `	}` |
|        - |  8111 | `	/* Store context in $this->__ctx */` |
|       26 |  8112 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8113 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8114 | `	if( pCtxAttr ){` |
|       26 |  8115 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8116 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8117 | `	}` |
|        - |  8118 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8119 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8120 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8121 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8122 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8123 | `	/* Unpack the args array and install into the frame */` |
|        - |  8124 | `	{` |
|       26 |  8125 | `		ph7_value **apValues = 0;` |
|       26 |  8126 | `		int nActual = 0;` |
|       26 |  8127 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8128 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8129 | `			ph7_hashmap_node *pNode;` |
|       26 |  8130 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8131 | `			if( nCount > 0 ){` |
|        3 |  8132 | `				sxu32 idx = 0;` |
|        4 |  8133 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8134 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8135 | `				if( apValues ){` |
|        3 |  8136 | `					pNode = pMap->pFirst;` |
|        7 |  8137 | `					while( pNode && idx < nCount ){` |
|        5 |  8138 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8139 | `						idx++;` |
|        5 |  8140 | `						pNode = pNode->pPrev;` |
|        1 |  8141 | `					}` |
|        3 |  8142 | `					nActual = (int)idx;` |
|        1 |  8143 | `				}` |
|        1 |  8144 | `			}` |
|       12 |  8145 | `		}` |
|       26 |  8146 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8147 | `		if( apValues ){` |
|        3 |  8148 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8149 | `		}` |
|        - |  8150 | `	}` |
|        - |  8151 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8152 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8153 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8154 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8155 | `		return PH7_ABORT;` |
|        - |  8156 | `	}` |
|       26 |  8157 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8158 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8159 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8160 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8161 | `		return PH7_ABORT;` |
|        - |  8162 | `	}` |
|       26 |  8163 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8164 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8165 | `		return PH7_EXCEPTION;` |
|        - |  8166 | `	}` |
|       26 |  8167 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8168 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8169 | `	return PH7_OK;` |
|       15 |  8170 |  |
|        - |  8171 | `/*` |
|        - |  8172 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  8173 | ` */` |
|       36 |  8174 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8175 |  |
|       38 |  8176 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8177 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8178 | `	ph7_value sResult;` |
|        - |  8179 | `	ph7_value *pResumeVal;` |
|        - |  8180 | `	sxi32 rc;` |
|       38 |  8181 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8182 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  8183 | `		return PH7_OK;` |
|        - |  8184 | `	}` |
|       38 |  8185 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  8186 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8187 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  8188 | `		return PH7_OK;` |
|        - |  8189 | `	}` |
|       38 |  8190 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8191 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8192 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  8193 | `	}` |
|       36 |  8194 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  8195 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  8196 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  8197 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8198 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8199 | `		return PH7_ABORT;` |
|        - |  8200 | `	}` |
|       36 |  8201 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8202 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8203 | `		return PH7_EXCEPTION;` |
|        - |  8204 | `	}` |
|       36 |  8205 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  8206 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  8207 | `	return PH7_OK;` |
|       20 |  8208 |  |
|        - |  8209 | `/*` |
|        - |  8210 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  8211 | ` */` |
|        6 |  8212 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8213 |  |
|        8 |  8214 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8215 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  8216 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8217 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8218 | `		return PH7_OK;` |
|        - |  8219 | `	}` |
|        8 |  8220 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  8221 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8222 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8223 | `		return PH7_OK;` |
|        - |  8224 | `	}` |
|        8 |  8225 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8226 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8227 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8228 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  8229 | `		}` |
|      ! 0 |  8230 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8231 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  8232 | `	}` |
|        8 |  8233 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  8234 | `	return PH7_OK;` |
|        5 |  8235 |  |
|        - |  8236 | `/*` |
|        - |  8237 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  8238 | ` */` |
|        6 |  8239 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8240 |  |
|        - |  8241 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8242 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8243 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8244 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  8245 | `	return PH7_OK;` |
|        4 |  8246 |  |
|      ! 0 |  8247 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8248 |  |
|        - |  8249 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  8250 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  8251 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8252 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  8253 | `	return PH7_OK;` |
|      ! 0 |  8254 |  |
|        6 |  8255 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8256 |  |
|        - |  8257 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8258 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8259 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8260 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  8261 | `	return PH7_OK;` |
|        4 |  8262 |  |
|        6 |  8263 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8264 |  |
|        - |  8265 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8266 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8267 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8268 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  8269 | `	return PH7_OK;` |
|        4 |  8270 |  |
|        - |  8271 | `/*` |
|        - |  8272 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  8273 | ` */` |
|        4 |  8274 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8275 |  |
|        5 |  8276 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8277 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  8278 | `	if( nArg < 1 ){` |
|      ! 0 |  8279 | `		return PH7_OK;` |
|        - |  8280 | `	}` |
|        5 |  8281 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  8282 | `	if( pExecCtx ){` |
|        5 |  8283 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  8284 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  8285 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  8286 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8287 | `			SyString sAttrName;` |
|        - |  8288 | `			ph7_value *pAttr;` |
|        5 |  8289 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  8290 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  8291 | `			if( pAttr ){` |
|        5 |  8292 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  8293 | `			}` |
|        2 |  8294 | `		}` |
|        2 |  8295 | `	}` |
|        5 |  8296 | `	return PH7_OK;` |
|        3 |  8297 |  |
|        - |  8298 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  8299 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  8300 |  |
|        - |  8301 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8302 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  8303 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8304 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  8305 |  |
|      ! 0 |  8306 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  8307 |  |
|        - |  8308 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8309 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  8310 | `	ph7_exec_ctx *pCtx;` |
|        - |  8311 | `	ph7_vm_func *pFunc;` |
|        - |  8312 | `	ph7_value *pCallable;` |
|        - |  8313 | `	ph7_value *pCtxAttr;` |
|        - |  8314 | `	SyString sAttrName;` |
|        - |  8315 | `	/* Must not already be started */` |
|      ! 0 |  8316 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8317 | `	if( pCtx != 0 ){` |
|      ! 0 |  8318 | `		return SXERR_INVALID;` |
|        - |  8319 | `	}` |
|      ! 0 |  8320 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8321 | `		return SXERR_INVALID;` |
|        - |  8322 | `	}` |
|      ! 0 |  8323 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  8324 | `	/* Get the callable */` |
|      ! 0 |  8325 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  8326 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8327 | `	if( pCallable == 0 ){` |
|      ! 0 |  8328 | `		return SXERR_INVALID;` |
|        - |  8329 | `	}` |
|        - |  8330 | `	/* Resolve callable */` |
|      ! 0 |  8331 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8332 | `		SyString sName;` |
|        - |  8333 | `		SyHashEntry *pEntry;` |
|      ! 0 |  8334 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  8335 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  8336 | `		if( pEntry == 0 ){` |
|      ! 0 |  8337 | `			return SXERR_NOTFOUND;` |
|        - |  8338 | `		}` |
|      ! 0 |  8339 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  8340 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8341 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8342 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8343 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8344 | `		if( pMethod == 0 ){` |
|      ! 0 |  8345 | `			return SXERR_INVALID;` |
|        - |  8346 | `		}` |
|      ! 0 |  8347 | `		pClosureThis = pClosure;` |
|      ! 0 |  8348 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  8349 | `	}else{` |
|      ! 0 |  8350 | `		return SXERR_INVALID;` |
|        - |  8351 | `	}` |
|        - |  8352 | `	/* Create context */` |
|      ! 0 |  8353 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  8354 | `	if( pCtx == 0 ){` |
|      ! 0 |  8355 | `		return SXERR_MEM;` |
|        - |  8356 | `	}` |
|        - |  8357 | `	/* Store in __ctx */` |
|      ! 0 |  8358 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8359 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8360 | `	if( pCtxAttr ){` |
|      ! 0 |  8361 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  8362 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  8363 | `	}` |
|        - |  8364 | `	/* Set up frame with args */` |
|      ! 0 |  8365 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  8366 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  8367 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  8368 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  8369 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  8370 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  8371 |  |
|      ! 0 |  8372 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  8373 |  |
|      ! 0 |  8374 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8375 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  8376 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  8377 |  |
|      ! 0 |  8378 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8379 |  |
|      ! 0 |  8380 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8381 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  8382 |  |
|      ! 0 |  8383 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8384 |  |
|      ! 0 |  8385 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8386 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  8387 |  |
|      ! 0 |  8388 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8389 |  |
|      ! 0 |  8390 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8391 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  8392 | `	return &pCtx->sRetValue;` |
|      ! 0 |  8393 |  |
|        - |  8394 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  8395 | `/*` |
|        - |  8396 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  8397 | ` */` |
|       18 |  8398 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  8399 |  |
|        - |  8400 | `	ph7_generator *pGen;` |
|       20 |  8401 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  8402 | `	if( pGen == 0 ){` |
|      ! 0 |  8403 | `		return 0;` |
|        - |  8404 | `	}` |
|       20 |  8405 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  8406 | `	pGen->pCtx = pCtx;` |
|       20 |  8407 | `	pGen->iImplicitKey = 0;` |
|       20 |  8408 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  8409 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  8410 | `	/* Link the generator back to the exec context */` |
|       20 |  8411 | `	pCtx->pPrivate = pGen;` |
|       20 |  8412 | `	return pGen;` |
|       11 |  8413 |  |
|        - |  8414 | `/*` |
|        - |  8415 | ` * Release a generator and its execution context.` |
|        - |  8416 | ` */` |
|      ! 0 |  8417 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  8418 |  |
|      ! 0 |  8419 | `	if( pGen == 0 ){` |
|      ! 0 |  8420 | `		return;` |
|        - |  8421 | `	}` |
|      ! 0 |  8422 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8423 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8424 | `	if( pGen->pCtx ){` |
|      ! 0 |  8425 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  8426 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  8427 | `		pGen->pCtx = 0;` |
|      ! 0 |  8428 | `	}` |
|      ! 0 |  8429 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  8430 |  |
|        - |  8431 | `/*` |
|        - |  8432 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  8433 | ` */` |
|      192 |  8434 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  8435 |  |
|        - |  8436 | `	ph7_class_instance *pThis;` |
|        - |  8437 | `	SyString sAttr;` |
|        - |  8438 | `	ph7_value *pAttr;` |
|      194 |  8439 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8440 | `		return 0;` |
|        - |  8441 | `	}` |
|      194 |  8442 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  8443 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  8444 | `		return 0;` |
|        - |  8445 | `	}` |
|      194 |  8446 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  8447 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  8448 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  8449 | `		return 0;` |
|        - |  8450 | `	}` |
|      194 |  8451 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  8452 |  |
|        - |  8453 | `/*` |
|        - |  8454 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  8455 | ` */` |
|       18 |  8456 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8457 |  |
|        - |  8458 | `	ph7_generator *pGen;` |
|        - |  8459 | `	sxi32 rc;` |
|       20 |  8460 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  8461 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  8462 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  8463 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  8464 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  8465 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  8466 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  8467 | `	}` |
|       20 |  8468 | `	return PH7_OK;` |
|       11 |  8469 |  |
|        - |  8470 | `/*` |
|        - |  8471 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  8472 | ` */` |
|       52 |  8473 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8474 |  |
|        - |  8475 | `	ph7_generator *pGen;` |
|       54 |  8476 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  8477 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  8478 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  8479 | `	return PH7_OK;` |
|       28 |  8480 |  |
|        - |  8481 | `/*` |
|        - |  8482 | ` * Generator::current() — return the last yielded value.` |
|        - |  8483 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8484 | ` */` |
|       56 |  8485 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8486 |  |
|        - |  8487 | `	ph7_generator *pGen;` |
|        - |  8488 | `	sxi32 rc;` |
|       58 |  8489 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8490 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  8491 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8492 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8493 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8494 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8495 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8496 | `	}` |
|       58 |  8497 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  8498 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  8499 | `	}else{` |
|      ! 0 |  8500 | `		ph7_result_null(pCtx);` |
|        - |  8501 | `	}` |
|       58 |  8502 | `	return PH7_OK;` |
|       30 |  8503 |  |
|        - |  8504 | `/*` |
|        - |  8505 | ` * Generator::key() — return the last yielded key.` |
|        - |  8506 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8507 | ` */` |
|       12 |  8508 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8509 |  |
|        - |  8510 | `	ph7_generator *pGen;` |
|        - |  8511 | `	sxi32 rc;` |
|       13 |  8512 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8513 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  8514 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8515 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8516 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8517 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8518 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8519 | `	}` |
|       13 |  8520 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  8521 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  8522 | `	}else{` |
|      ! 0 |  8523 | `		ph7_result_null(pCtx);` |
|        - |  8524 | `	}` |
|       13 |  8525 | `	return PH7_OK;` |
|        7 |  8526 |  |
|        - |  8527 | `/*` |
|        - |  8528 | ` * Generator::next() — advance to the next yield point.` |
|        - |  8529 | ` */` |
|       48 |  8530 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8531 |  |
|        - |  8532 | `	ph7_generator *pGen;` |
|        - |  8533 | `	sxi32 rc;` |
|       50 |  8534 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  8535 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  8536 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  8537 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8538 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  8539 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  8540 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  8541 | `	}else{` |
|      ! 0 |  8542 | `		return PH7_OK;` |
|        - |  8543 | `	}` |
|       50 |  8544 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  8545 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  8546 | `	return PH7_OK;` |
|       26 |  8547 |  |
|        - |  8548 | `/*` |
|        - |  8549 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  8550 | ` */` |
|        4 |  8551 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8552 |  |
|        - |  8553 | `	ph7_generator *pGen;` |
|        - |  8554 | `	ph7_value *pSendVal;` |
|        - |  8555 | `	sxi32 rc;` |
|        5 |  8556 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  8557 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  8558 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  8559 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  8560 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  8561 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  8562 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  8563 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  8564 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  8565 | `	}else{` |
|      ! 0 |  8566 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8567 | `		return PH7_OK;` |
|        - |  8568 | `	}` |
|        5 |  8569 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  8570 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  8571 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8572 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  8573 | `	}else{` |
|        3 |  8574 | `		ph7_result_null(pCtx);` |
|        - |  8575 | `	}` |
|        5 |  8576 | `	return PH7_OK;` |
|        3 |  8577 |  |
|        - |  8578 | `/*` |
|        - |  8579 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  8580 | ` *` |
|        - |  8581 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  8582 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  8583 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  8584 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  8585 | ` * the exception to the caller.` |
|        - |  8586 | ` */` |
|      ! 0 |  8587 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8588 |  |
|        - |  8589 | `	ph7_generator *pGen;` |
|        - |  8590 | `	const char *zMsg;` |
|        - |  8591 | `	int nLen;` |
|      ! 0 |  8592 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  8593 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8594 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  8595 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  8596 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  8597 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8598 | `			"Cannot throw into a closed generator");` |
|        - |  8599 | `	}` |
|        - |  8600 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  8601 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  8602 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  8603 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  8604 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8605 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  8606 | `	nLen = 0;` |
|      ! 0 |  8607 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  8608 | `		/* Try to get the exception's message */` |
|        - |  8609 | `		SyString sAttr;` |
|        - |  8610 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8611 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8612 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8613 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8614 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8615 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8616 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8617 | `		}` |
|      ! 0 |  8618 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8619 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8620 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8621 | `	}` |
|      ! 0 |  8622 | `	(void)nLen;` |
|      ! 0 |  8623 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8624 |  |
|        - |  8625 | `/*` |
|        - |  8626 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8627 | ` */` |
|        2 |  8628 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8629 |  |
|        - |  8630 | `	ph7_generator *pGen;` |
|        3 |  8631 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8632 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8633 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8634 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8635 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8636 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8637 | `	}` |
|        3 |  8638 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8639 | `	return PH7_OK;` |
|        2 |  8640 |  |
|        - |  8641 | `/*` |
|        - |  8642 | ` * Generator::__destruct() — clean up.` |
|        - |  8643 | ` */` |
|      ! 0 |  8644 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8645 |  |
|        - |  8646 | `	ph7_generator *pGen;` |
|      ! 0 |  8647 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8648 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8649 | `	if( pGen ){` |
|      ! 0 |  8650 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8651 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8652 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8653 | `			SyString sAttrName;` |
|        - |  8654 | `			ph7_value *pAttr;` |
|      ! 0 |  8655 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8656 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8657 | `			if( pAttr ){` |
|      ! 0 |  8658 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8659 | `			}` |
|      ! 0 |  8660 | `		}` |
|      ! 0 |  8661 | `	}` |
|      ! 0 |  8662 | `	return PH7_OK;` |
|      ! 0 |  8663 |  |
|        - |  8664 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8665 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8666 | `/*` |
|        - |  8667 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8668 | ` * the desired message.` |
|        - |  8669 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8670 | ` * in 'api.c' for additional information.` |
|        - |  8671 | ` */` |
|      370 |  8672 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8673 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8674 | `	SyString *pString /* Message to output */` |
|        - |  8675 | `	)` |
|        2 |  8676 |  |
|      372 |  8677 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8678 | `	sxi32 rc = SXRET_OK;` |
|        - |  8679 | `	/* Call the output consumer */` |
|      372 |  8680 | `	if( pString->nByte > 0 ){` |
|      372 |  8681 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8682 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8683 | `	}` |
|      372 |  8684 | `	return rc;` |
|        2 |  8685 |  |
|        - |  8686 | `/*` |
|        - |  8687 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8688 | ` * callback to consume the formatted message.` |
|        - |  8689 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8690 | ` * in 'api.c' for additional information.` |
|        - |  8691 | ` */` |
|        2 |  8692 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8693 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8694 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8695 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8696 | `	)` |
|        1 |  8697 |  |
|        3 |  8698 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8699 | `	sxi32 rc = SXRET_OK;` |
|        - |  8700 | `	SyBlob sWorker;` |
|        - |  8701 | `	/* Format the message and call the output consumer */` |
|        3 |  8702 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8703 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8704 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8705 | `		/* Consume the formatted message */` |
|        3 |  8706 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8707 | `	}` |
|        3 |  8708 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8709 | `	/* Release the working buffer */` |
|        3 |  8710 | `	SyBlobRelease(&sWorker);` |
|        3 |  8711 | `	return rc;` |
|        1 |  8712 |  |
|        - |  8713 | `/*` |
|        - |  8714 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8715 | ` * This function never fail and always return a pointer` |
|        - |  8716 | ` * to a null terminated string.` |
|        - |  8717 | ` */` |
|       12 |  8718 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8719 |  |
|       13 |  8720 | `	const char *zOp = "Unknown     ";` |
|       13 |  8721 | `	switch(nOp){` |
|        3 |  8722 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8723 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8724 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8725 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8726 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8727 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8728 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8729 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8730 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8731 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8732 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8733 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8734 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8735 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8736 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8737 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8738 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8739 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8740 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8741 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8742 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8743 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8744 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8745 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8746 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8747 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8748 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8749 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8750 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8751 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8752 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8753 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8754 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8755 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8756 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8757 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8758 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8759 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8760 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8761 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8762 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8763 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8764 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8765 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8766 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8767 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8768 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8769 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8770 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8771 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8772 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8773 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8774 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8775 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8776 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8777 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8778 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  8779 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  8780 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8781 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8782 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8783 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8784 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8785 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8786 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8787 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8788 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8789 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8790 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8791 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8792 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8793 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8794 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8795 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8796 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8797 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8798 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8799 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8800 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8801 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8802 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8803 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8804 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8805 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8806 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8807 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8808 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8809 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8810 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8811 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8812 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8813 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8814 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8815 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8816 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8817 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8818 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8819 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8820 | `	default:` |
|      ! 0 |  8821 | `		break;` |
|        - |  8822 | `	}` |
|       13 |  8823 | `	return zOp;` |
|        1 |  8824 |  |
|        - |  8825 | `/*` |
|        - |  8826 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8827 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8828 | ` * is responsible of consuming the generated dump.` |
|        - |  8829 | ` */` |
|        2 |  8830 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8831 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8832 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8833 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8834 | `	)` |
|        1 |  8835 |  |
|        - |  8836 | `	sxi32 rc;` |
|        3 |  8837 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8838 | `	return rc;` |
|        1 |  8839 |  |
|        - |  8840 | `/*` |
|        - |  8841 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8842 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8843 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8844 | ` * in 'compile.c' for additional information.` |
|        - |  8845 | ` */` |
|       14 |  8846 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8847 |  |
|       15 |  8848 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8849 | `	/* Evaluate and expand constant value */` |
|       15 |  8850 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8851 |  |
|        - |  8852 | `/*` |
|        - |  8853 | ` * Section:` |
|        - |  8854 | ` *  Function handling functions.` |
|        - |  8855 | ` * Status:` |
|        - |  8856 | ` *    Stable.` |
|        - |  8857 | ` */` |
|        - |  8858 | `/*` |
|        - |  8859 | ` * int func_num_args(void)` |
|        - |  8860 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8861 | ` * Parameters` |
|        - |  8862 | ` *   None.` |
|        - |  8863 | ` * Return` |
|        - |  8864 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8865 | ` *  or -1 if called from the globe scope.` |
|        - |  8866 | ` */` |
|      944 |  8867 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8868 |  |
|        - |  8869 | `	VmFrame *pFrame;` |
|        - |  8870 | `	ph7_vm *pVm;` |
|        - |  8871 | `	/* Point to the target VM */` |
|      946 |  8872 | `	pVm = pCtx->pVm;` |
|        - |  8873 | `	/* Current frame */` |
|      946 |  8874 | `	pFrame = pVm->pFrame;` |
|      946 |  8875 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8876 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8877 | `		SXUNUSED(nArg);` |
|      ! 0 |  8878 | `		SXUNUSED(apArg);` |
|        - |  8879 | `		/* Global frame,return -1 */` |
|      ! 0 |  8880 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8881 | `		return SXRET_OK;` |
|        - |  8882 | `	}` |
|        - |  8883 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8884 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8885 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8886 | `	return SXRET_OK;` |
|      474 |  8887 |  |
|        - |  8888 | `/*` |
|        - |  8889 | ` * value func_get_arg(int $arg_num)` |
|        - |  8890 | ` *   Return an item from the argument list.` |
|        - |  8891 | ` * Parameters` |
|        - |  8892 | ` *  Argument number(index start from zero).` |
|        - |  8893 | ` * Return` |
|        - |  8894 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8895 | ` */` |
|       22 |  8896 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8897 |  |
|       24 |  8898 | `	ph7_value *pObj = 0;` |
|       24 |  8899 | `	VmSlot *pSlot = 0;` |
|        - |  8900 | `	VmFrame *pFrame;` |
|        - |  8901 | `	ph7_vm *pVm;` |
|        - |  8902 | `	/* Point to the target VM */` |
|       24 |  8903 | `	pVm = pCtx->pVm;` |
|        - |  8904 | `	/* Current frame */` |
|       24 |  8905 | `	pFrame = pVm->pFrame;` |
|       24 |  8906 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8907 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8908 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8909 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8910 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8911 | `		return SXRET_OK;` |
|        - |  8912 | `	}` |
|        - |  8913 | `	/* Extract the desired index */` |
|       21 |  8914 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8915 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8916 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8917 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8918 | `		return SXRET_OK;` |
|        - |  8919 | `	}` |
|        - |  8920 | `	/* Extract the desired argument */` |
|       21 |  8921 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8922 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8923 | `			/* Return the desired argument */` |
|       21 |  8924 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8925 | `		}else{` |
|        - |  8926 | `			/* No such argument,return false */` |
|      ! 0 |  8927 | `			ph7_result_bool(pCtx,0);` |
|        - |  8928 | `		}` |
|       11 |  8929 | `	}else{` |
|        - |  8930 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8931 | `		ph7_result_bool(pCtx,0);` |
|        - |  8932 | `	}` |
|       21 |  8933 | `	return SXRET_OK;` |
|       13 |  8934 |  |
|        - |  8935 | `/*` |
|        - |  8936 | ` * array func_get_args_byref(void)` |
|        - |  8937 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8938 | ` * Parameters` |
|        - |  8939 | ` *  None.` |
|        - |  8940 | ` * Return` |
|        - |  8941 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8942 | ` *  member of the current user-defined function's argument list.` |
|        - |  8943 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8944 | ` * NOTE:` |
|        - |  8945 | ` *  Arguments are returned to the array by reference.` |
|        - |  8946 | ` */` |
|        2 |  8947 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8948 |  |
|        - |  8949 | `	ph7_value *pArray;` |
|        - |  8950 | `	VmFrame *pFrame;` |
|        - |  8951 | `	VmSlot *aSlot;` |
|        - |  8952 | `	sxu32 n;` |
|        - |  8953 | `	/* Point to the current frame */` |
|        3 |  8954 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8955 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8956 | `	if( pFrame->pParent == 0 ){` |
|        - |  8957 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8958 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8959 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8960 | `		return SXRET_OK;` |
|        - |  8961 | `	}` |
|        - |  8962 | `	/* Create a new array */` |
|        3 |  8963 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8964 | `	if( pArray == 0 ){` |
|      ! 0 |  8965 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8966 | `		SXUNUSED(apArg);` |
|      ! 0 |  8967 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8968 | `		return SXRET_OK;` |
|        - |  8969 | `	}` |
|        - |  8970 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8971 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8972 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8973 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8974 | `	}` |
|        - |  8975 | `	/* Return the freshly created array */` |
|        3 |  8976 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8977 | `	return SXRET_OK;` |
|        2 |  8978 |  |
|        - |  8979 | `/*` |
|        - |  8980 | ` * array func_get_args(void)` |
|        - |  8981 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8982 | ` * Parameters` |
|        - |  8983 | ` *  None.` |
|        - |  8984 | ` * Return` |
|        - |  8985 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8986 | ` *  member of the current user-defined function's argument list.` |
|        - |  8987 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8988 | ` */` |
|       88 |  8989 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8990 |  |
|       90 |  8991 | `	ph7_value *pObj = 0;` |
|        - |  8992 | `	ph7_value *pArray;` |
|        - |  8993 | `	VmFrame *pFrame;` |
|        - |  8994 | `	VmSlot *aSlot;` |
|        - |  8995 | `	sxu32 n;` |
|        - |  8996 | `	/* Point to the current frame */` |
|       90 |  8997 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8998 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8999 | `	if( pFrame->pParent == 0 ){` |
|        - |  9000 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9001 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9002 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9003 | `		return SXRET_OK;` |
|        - |  9004 | `	}` |
|        - |  9005 | `	/* Create a new array */` |
|       90 |  9006 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9007 | `	if( pArray == 0 ){` |
|      ! 0 |  9008 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9009 | `		SXUNUSED(apArg);` |
|      ! 0 |  9010 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9011 | `		return SXRET_OK;` |
|        - |  9012 | `	}` |
|        - |  9013 | `	/* Start filling the array with the given arguments */` |
|       90 |  9014 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9015 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9016 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9017 | `		if( pObj ){` |
|      134 |  9018 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9019 | `		}` |
|       68 |  9020 | `	}` |
|        - |  9021 | `	/* Return the freshly created array */` |
|       90 |  9022 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9023 | `	return SXRET_OK;` |
|       46 |  9024 |  |
|        - |  9025 | `/*` |
|        - |  9026 | ` * bool function_exists(string $name)` |
|        - |  9027 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9028 | ` * Parameters` |
|        - |  9029 | ` *  The name of the desired function.` |
|        - |  9030 | ` * Return` |
|        - |  9031 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9032 | ` */` |
|     1682 |  9033 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9034 |  |
|        - |  9035 | `	const char *zName;` |
|        - |  9036 | `	ph7_vm *pVm;` |
|        - |  9037 | `	int nLen;` |
|        - |  9038 | `	int res;` |
|     1684 |  9039 | `	if( nArg < 1 ){` |
|        - |  9040 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9041 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9042 | `		return SXRET_OK;` |
|        - |  9043 | `	}` |
|        - |  9044 | `	/* Point to the target VM */` |
|     1684 |  9045 | `	pVm = pCtx->pVm;` |
|        - |  9046 | `	/* Extract the function name */` |
|     1684 |  9047 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9048 | `	/* Assume the function is not defined */` |
|     1684 |  9049 | `	res = 0;` |
|        - |  9050 | `	/* Perform the lookup */` |
|     2523 |  9051 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  9052 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9053 | `			/* Function is defined */` |
|      206 |  9054 | `			res = 1;` |
|      102 |  9055 | `	}` |
|     1684 |  9056 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  9057 | `	return SXRET_OK;` |
|      843 |  9058 |  |
|        - |  9059 | `/*` |
|        - |  9060 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9061 | ` * [i.e: Whether it is callable or not].` |
|        - |  9062 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9063 | ` */` |
|    18940 |  9064 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9065 |  |
|    18942 |  9066 | `	int res = 0;` |
|    18942 |  9067 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9068 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9069 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9070 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9071 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9072 | `		if( pMethod && CallInvoke ){` |
|        - |  9073 | `			ph7_value sResult;` |
|        - |  9074 | `			sxi32 rc;` |
|        - |  9075 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9076 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9077 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9078 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9079 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9080 | `			}` |
|      ! 0 |  9081 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9082 | `		}` |
|    18942 |  9083 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9084 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9085 | `		if( pMap->nEntry == 2 ){` |
|        - |  9086 | `			ph7_class *pClass;` |
|        - |  9087 | `			ph7_value *pV;` |
|        - |  9088 | `			/* Extract the target class */` |
|       12 |  9089 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9090 | `			if( pV ){` |
|       12 |  9091 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9092 | `				if( pClass ){` |
|        - |  9093 | `					ph7_class_method *pMethod;` |
|        - |  9094 | `					/* Extract the target method */` |
|       10 |  9095 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9096 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9097 | `						/* Perform the lookup */` |
|       10 |  9098 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9099 | `						if( pMethod ){` |
|        - |  9100 | `							/* Method is callable */` |
|        5 |  9101 | `							res = 1;` |
|        2 |  9102 | `						}` |
|        4 |  9103 | `					}` |
|        4 |  9104 | `				}` |
|        5 |  9105 | `			}` |
|        7 |  9106 | `		}` |
|    18929 |  9107 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9108 | `		const char *zName;` |
|        - |  9109 | `		int nLen;` |
|        - |  9110 | `		/* Extract the name */` |
|     5216 |  9111 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9112 | `		/* Perform the lookup */` |
|     5231 |  9113 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9114 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9115 | `				/* Function is callable */` |
|     5198 |  9116 | `				res = 1;` |
|     2598 |  9117 | `		}` |
|     2607 |  9118 | `	}` |
|    18942 |  9119 | `	return res;` |
|        2 |  9120 |  |
|        - |  9121 | `/*` |
|        - |  9122 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9123 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9124 | ` * Parameters` |
|        - |  9125 | ` * $name` |
|        - |  9126 | ` *    The callback function to check` |
|        - |  9127 | ` * $syntax_only` |
|        - |  9128 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9129 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9130 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9131 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9132 | ` *    a string.` |
|        - |  9133 | ` * Return` |
|        - |  9134 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9135 | ` */` |
|       14 |  9136 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9137 |  |
|        - |  9138 | `	ph7_vm *pVm;` |
|        - |  9139 | `	int res;` |
|       15 |  9140 | `	if( nArg < 1 ){` |
|        - |  9141 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9142 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9143 | `		return SXRET_OK;` |
|        - |  9144 | `	}` |
|        - |  9145 | `	/* Point to the target VM */` |
|       15 |  9146 | `	pVm = pCtx->pVm;` |
|        - |  9147 | `	/* Perform the requested operation */` |
|       15 |  9148 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9149 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9150 | `	return SXRET_OK;` |
|        8 |  9151 |  |
|        - |  9152 | `/*` |
|        - |  9153 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9154 | ` * defined below.` |
|        - |  9155 | ` */` |
|     1200 |  9156 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9157 |  |
|     1201 |  9158 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9159 | `	ph7_value sName;` |
|        - |  9160 | `	sxi32 rc;` |
|        - |  9161 | `	/* Prepare the function name for insertion */` |
|     1201 |  9162 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  9163 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9164 | `	/* Perform the insertion */` |
|     1201 |  9165 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  9166 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  9167 | `	return rc;` |
|        1 |  9168 |  |
|        - |  9169 | `/*` |
|        - |  9170 | ` * array get_defined_functions(void)` |
|        - |  9171 | ` *  Returns an array of all defined functions.` |
|        - |  9172 | ` * Parameter` |
|        - |  9173 | ` *  None.` |
|        - |  9174 | ` * Return` |
|        - |  9175 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  9176 | ` *  both built-in (internal) and user-defined.` |
|        - |  9177 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  9178 | ` *  defined ones using $arr["user"].` |
|        - |  9179 | ` * Note:` |
|        - |  9180 | ` *  NULL is returned on failure.` |
|        - |  9181 | ` */` |
|        2 |  9182 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9183 |  |
|        - |  9184 | `	ph7_value *pArray,*pEntry;` |
|        - |  9185 | `	/* NOTE:` |
|        - |  9186 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  9187 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  9188 | `	 */` |
|        3 |  9189 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9190 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9191 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9192 | `		SXUNUSED(apArg);` |
|        - |  9193 | `		/* Return NULL */` |
|      ! 0 |  9194 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9195 | `		return SXRET_OK;` |
|        - |  9196 | `	}` |
|        3 |  9197 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9198 | `	if( pEntry == 0 ){` |
|        - |  9199 | `		/* Return NULL */` |
|      ! 0 |  9200 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9201 | `		return SXRET_OK;` |
|        - |  9202 | `	}` |
|        - |  9203 | `	/* Fill with the appropriate information */` |
|        3 |  9204 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  9205 | `	/* Create the 'internal' index */` |
|        3 |  9206 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  9207 | `	/* Create the user-func array */` |
|        3 |  9208 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9209 | `	if( pEntry == 0 ){` |
|        - |  9210 | `		/* Return NULL */` |
|      ! 0 |  9211 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9212 | `		return SXRET_OK;` |
|        - |  9213 | `	}` |
|        - |  9214 | `	/* Fill with the appropriate information */` |
|        3 |  9215 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  9216 | `	/* Create the 'user' index */` |
|        3 |  9217 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  9218 | `	/* Return the multi-dimensional array */` |
|        3 |  9219 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9220 | `	return SXRET_OK;` |
|        2 |  9221 |  |
|        - |  9222 | `/*` |
|        - |  9223 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  9224 | ` *  Register a function for execution on shutdown.` |
|        - |  9225 | ` * Note` |
|        - |  9226 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  9227 | ` *  be called in the same order as they were registered.` |
|        - |  9228 | ` * Parameters` |
|        - |  9229 | ` *  $callback` |
|        - |  9230 | ` *   The shutdown callback to register.` |
|        - |  9231 | ` * $param` |
|        - |  9232 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  9233 | ` * Return` |
|        - |  9234 | ` *  Nothing.` |
|        - |  9235 | ` */` |
|        2 |  9236 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9237 |  |
|        - |  9238 | `	VmShutdownCB sEntry;` |
|        - |  9239 | `	int i,j;` |
|        3 |  9240 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9241 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  9242 | `		return PH7_OK;` |
|        - |  9243 | `	}` |
|        - |  9244 | `	/* Zero the Entry */` |
|        3 |  9245 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  9246 | `	/* Initialize fields */` |
|        3 |  9247 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  9248 | `	/* Save the callback name for later invocation name */` |
|        3 |  9249 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  9250 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  9251 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  9252 | `	}` |
|        - |  9253 | `	/* Copy arguments */` |
|        3 |  9254 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  9255 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  9256 | `			/* Limit reached */` |
|      ! 0 |  9257 | `			break;` |
|        - |  9258 | `		}` |
|      ! 0 |  9259 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  9260 | `	}` |
|        3 |  9261 | `	sEntry.nArg = j;` |
|        - |  9262 | `	/* Install the callback */` |
|        3 |  9263 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  9264 | `	return PH7_OK;` |
|        2 |  9265 |  |
|        - |  9266 | `/*` |
|        - |  9267 | ` * Section:` |
|        - |  9268 | ` *  Class handling functions.` |
|        - |  9269 | ` * Status:` |
|        - |  9270 | ` *    Stable.` |
|        - |  9271 | ` */` |
|        - |  9272 | `/*` |
|        - |  9273 | ` * Extract the top active class. NULL is returned` |
|        - |  9274 | ` * if the class stack is empty.` |
|        - |  9275 | ` */` |
|      646 |  9276 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  9277 |  |
|      648 |  9278 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  9279 | `	ph7_class **apClass;` |
|      648 |  9280 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  9281 | `		/* Empty stack,return NULL */` |
|       15 |  9282 | `		return 0;` |
|        - |  9283 | `	}` |
|        - |  9284 | `	/* Peek the last entry */` |
|      634 |  9285 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      634 |  9286 | `	return apClass[pSet->nUsed - 1];` |
|      325 |  9287 |  |
|        - |  9288 | `/*` |
|        - |  9289 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  9290 | ` *   Get the class that declared the currently executing method.` |
|        - |  9291 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  9292 | ` *` |
|        - |  9293 | ` * Parameters` |
|        - |  9294 | ` *   pVm: Target VM` |
|        - |  9295 | ` *` |
|        - |  9296 | ` * Return` |
|        - |  9297 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  9298 | ` *   - Not executing within a class method` |
|        - |  9299 | ` *` |
|        - |  9300 | ` * Note` |
|        - |  9301 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  9302 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  9303 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  9304 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  9305 | ` *   declaring class.` |
|        - |  9306 | ` */` |
|       96 |  9307 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  9308 |  |
|       98 |  9309 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9310 | `	ph7_vm_func *pVmFunc;` |
|        - |  9311 |  |
|        - |  9312 | `	/* Skip exception frames to find the actual method frame */` |
|       98 |  9313 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  9314 |  |
|        - |  9315 | `	/* Check if we're in a method context */` |
|       98 |  9316 | `	if( pFrame->pParent ){` |
|       94 |  9317 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       94 |  9318 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  9319 | `			/* Return the declaring class */` |
|       94 |  9320 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  9321 | `		}` |
|      ! 0 |  9322 | `	}` |
|        - |  9323 |  |
|        5 |  9324 | `	return 0;` |
|       50 |  9325 |  |
|        - |  9326 |  |
|        - |  9327 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  9328 | `/*` |
|        - |  9329 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  9330 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  9331 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  9332 | ` * return value indicates failure.` |
|        - |  9333 | ` */` |
|     1584 |  9334 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  9335 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  9336 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  9337 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  9338 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  9339 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  9340 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  9341 | `	)` |
|        2 |  9342 |  |
|        - |  9343 | `	ph7_value *aStack;` |
|        - |  9344 | `	VmInstr aInstr[2];` |
|        - |  9345 | `	int iCursor;` |
|        - |  9346 | `	int i;` |
|        - |  9347 | `	/* Create a new operand stack */` |
|     1586 |  9348 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1586 |  9349 | `	if( aStack == 0 ){` |
|      ! 0 |  9350 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9351 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  9352 | `		return SXERR_MEM;` |
|        - |  9353 | `	}` |
|        - |  9354 | `	/* Fill the operand stack with the given arguments */` |
|     2278 |  9355 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      694 |  9356 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  9357 | `		/*` |
|        - |  9358 | `		 * Symisc eXtension:` |
|        - |  9359 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  9360 | `		 */` |
|      694 |  9361 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      348 |  9362 | `	}` |
|     1586 |  9363 | `	iCursor = nArg + 1;` |
|     1586 |  9364 | `	if( pThis ){` |
|        - |  9365 | `		/*` |
|        - |  9366 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  9367 | `		 */` |
|     1580 |  9368 | `		pThis->iRef++; /* Increment reference count */` |
|     1580 |  9369 | `		aStack[i].x.pOther = pThis;` |
|     1580 |  9370 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      789 |  9371 | `	}` |
|     1586 |  9372 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1586 |  9373 | `	i++;` |
|        - |  9374 | `	/* Push method name */` |
|     1586 |  9375 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1586 |  9376 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1586 |  9377 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1586 |  9378 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  9379 | `	/* Emit the CALL istruction */` |
|     1586 |  9380 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1586 |  9381 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1586 |  9382 | `	aInstr[0].iP2 = 0;` |
|     1586 |  9383 | `	aInstr[0].p3  = 0;` |
|        - |  9384 | `	/* Emit the DONE instruction */` |
|     1586 |  9385 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1586 |  9386 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1586 |  9387 | `	aInstr[1].iP2 = 0;` |
|     1586 |  9388 | `	aInstr[1].p3  = 0;` |
|        - |  9389 | `	/* Execute the method body (if available) */` |
|     1586 |  9390 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  9391 | `	/* Clean up the mess left behind */` |
|     1586 |  9392 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1586 |  9393 | `	return PH7_OK;` |
|      794 |  9394 |  |
|        - |  9395 | `/*` |
|        - |  9396 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  9397 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  9398 | ` * in the apArg[] array.` |
|        - |  9399 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9400 | ` * return value indicates failure.` |
|        - |  9401 | ` */` |
|      966 |  9402 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  9403 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9404 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9405 | `	int nArg,          /* Total number of given arguments */` |
|        - |  9406 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  9407 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  9408 | `	)` |
|        2 |  9409 |  |
|        - |  9410 | `	ph7_value *aStack;` |
|        - |  9411 | `	VmInstr aInstr[2];` |
|        - |  9412 | `	int i;` |
|      968 |  9413 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9414 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  9415 | `		if( pResult ){` |
|        - |  9416 | `			/* Assume a null return value */` |
|      ! 0 |  9417 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9418 | `		}` |
|      479 |  9419 | `		return SXERR_INVALID;` |
|        - |  9420 | `	}` |
|      490 |  9421 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9422 | `		/* Class method */` |
|       11 |  9423 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  9424 | `		ph7_class_method *pMethod = 0;` |
|       11 |  9425 | `		ph7_class_instance *pThis = 0;` |
|       11 |  9426 | `		ph7_class *pClass = 0;` |
|        - |  9427 | `		ph7_value *pValue;` |
|        - |  9428 | `		sxi32 rc;` |
|       11 |  9429 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  9430 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  9431 | `			if( pResult ){` |
|        - |  9432 | `				/* Assume a null return value */` |
|      ! 0 |  9433 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9434 | `			}` |
|      ! 0 |  9435 | `			return SXRET_OK;` |
|        - |  9436 | `		}` |
|        - |  9437 | `		/* Extract the class name or an instance of it */` |
|       11 |  9438 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  9439 | `		if( pValue ){` |
|       11 |  9440 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  9441 | `		}` |
|       11 |  9442 | `		if( pClass == 0 ){` |
|        - |  9443 | `			/* No such class,return NULL */` |
|      ! 0 |  9444 | `			if( pResult ){` |
|      ! 0 |  9445 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9446 | `			}` |
|      ! 0 |  9447 | `			return SXRET_OK;` |
|        - |  9448 | `		}` |
|       11 |  9449 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9450 | `			/* Point to the class instance */` |
|        5 |  9451 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  9452 | `		}` |
|        - |  9453 | `		/* Try to extract the method */` |
|       11 |  9454 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  9455 | `		if( pValue ){` |
|       11 |  9456 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  9457 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  9458 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  9459 | `			}` |
|        5 |  9460 | `		}` |
|       11 |  9461 | `		if( pMethod == 0 ){` |
|        - |  9462 | `			/* No such method,return NULL */` |
|      ! 0 |  9463 | `			if( pResult ){` |
|      ! 0 |  9464 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9465 | `			}` |
|      ! 0 |  9466 | `			return SXRET_OK;` |
|        - |  9467 | `		}` |
|        - |  9468 | `		/* Call the class method */` |
|       11 |  9469 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  9470 | `		return rc;` |
|        - |  9471 | `	}` |
|        - |  9472 | `	/* Create a new operand stack */` |
|      480 |  9473 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      480 |  9474 | `	if( aStack == 0 ){` |
|      ! 0 |  9475 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9476 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  9477 | `		if( pResult ){` |
|        - |  9478 | `			/* Assume a null return value */` |
|      ! 0 |  9479 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9480 | `		}` |
|      ! 0 |  9481 | `		return SXERR_MEM;` |
|        - |  9482 | `	}` |
|        - |  9483 | `	/* Fill the operand stack with the given arguments */` |
|     1534 |  9484 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1056 |  9485 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  9486 | `		/*` |
|        - |  9487 | `		 * Symisc eXtension:` |
|        - |  9488 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  9489 | `		 */` |
|     1056 |  9490 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      529 |  9491 | `	}` |
|        - |  9492 | `	/* Push the function name */` |
|      480 |  9493 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      480 |  9494 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  9495 | `	/* Emit the CALL istruction */` |
|      480 |  9496 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      480 |  9497 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      480 |  9498 | `	aInstr[0].iP2 = 0;` |
|      480 |  9499 | `	aInstr[0].p3  = 0;` |
|        - |  9500 | `	/* Emit the DONE instruction */` |
|      480 |  9501 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      480 |  9502 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      480 |  9503 | `	aInstr[1].iP2 = 0;` |
|      480 |  9504 | `	aInstr[1].p3  = 0;` |
|        - |  9505 | `	/* Execute the function body (if available) */` |
|      480 |  9506 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  9507 | `	/* Clean up the mess left behind */` |
|      480 |  9508 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      480 |  9509 | `	return PH7_OK;` |
|      485 |  9510 |  |
|        - |  9511 | `/*` |
|        - |  9512 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  9513 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  9514 | ` * parameter.` |
|        - |  9515 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9516 | ` * return value indicates failure.` |
|        - |  9517 | ` */` |
|      236 |  9518 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  9519 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9520 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9521 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  9522 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  9523 | `	)` |
|        1 |  9524 |  |
|        - |  9525 | `	ph7_value *pArg;` |
|        - |  9526 | `	SySet aArg;` |
|        - |  9527 | `	va_list ap;` |
|        - |  9528 | `	sxi32 rc;` |
|      237 |  9529 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  9530 | `	/* Copy arguments one after one */` |
|      237 |  9531 | `	va_start(ap,pResult);` |
|      393 |  9532 | `	for(;;){` |
|      787 |  9533 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  9534 | `		if( pArg == 0 ){` |
|      237 |  9535 | `			break;` |
|        - |  9536 | `		}` |
|      551 |  9537 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  9538 | `	}` |
|        - |  9539 | `	/* Call the core routine */` |
|      237 |  9540 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  9541 | `	/* Cleanup */` |
|      237 |  9542 | `	SySetRelease(&aArg);` |
|      237 |  9543 | `	return rc;` |
|        1 |  9544 |  |
|        - |  9545 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  9546 | `/*` |
|        - |  9547 | ` * bool defined(string $name)` |
|        - |  9548 | ` *  Checks whether a given named constant exists.` |
|        - |  9549 | ` * Parameter:` |
|        - |  9550 | ` *  Name of the desired constant.` |
|        - |  9551 | ` * Return` |
|        - |  9552 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  9553 | ` */` |
|       14 |  9554 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9555 |  |
|        - |  9556 | `	const char *zName;` |
|       16 |  9557 | `	int nLen = 0;` |
|       16 |  9558 | `	int res = 0;` |
|       16 |  9559 | `	if( nArg < 1 ){` |
|        - |  9560 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  9561 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  9562 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9563 | `		return SXRET_OK;` |
|        - |  9564 | `	}` |
|        - |  9565 | `	/* Extract constant name */` |
|       16 |  9566 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9567 | `	/* Perform the lookup */` |
|       16 |  9568 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9569 | `		/* Already defined */` |
|       10 |  9570 | `		res = 1;` |
|        4 |  9571 | `	}` |
|       16 |  9572 | `	ph7_result_bool(pCtx,res);` |
|       16 |  9573 | `	return SXRET_OK;` |
|        9 |  9574 |  |
|        - |  9575 | `/*` |
|        - |  9576 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  9577 | ` * below.` |
|        - |  9578 | ` */` |
|       10 |  9579 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  9580 |  |
|       12 |  9581 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  9582 | `	/* Expand constant value */` |
|       12 |  9583 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  9584 |  |
|        - |  9585 | `/*` |
|        - |  9586 | ` * bool define(string $constant_name,expression value)` |
|        - |  9587 | ` *  Defines a named constant at runtime.` |
|        - |  9588 | ` * Parameter:` |
|        - |  9589 | ` *  $constant_name` |
|        - |  9590 | ` *   The name of the constant` |
|        - |  9591 | ` *  $value` |
|        - |  9592 | ` *   Constant value` |
|        - |  9593 | ` * Return:` |
|        - |  9594 | ` *   TRUE on success,FALSE on failure.` |
|        - |  9595 | ` */` |
|       12 |  9596 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9597 |  |
|        - |  9598 | `	const char *zName;  /* Constant name */` |
|        - |  9599 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  9600 | `	int nLen = 0;       /* Name length */` |
|        - |  9601 | `	sxi32 rc;` |
|       14 |  9602 | `	if( nArg < 2 ){` |
|        - |  9603 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  9604 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  9605 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9606 | `		return SXRET_OK;` |
|        - |  9607 | `	}` |
|       14 |  9608 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  9609 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  9610 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9611 | `		return SXRET_OK;` |
|        - |  9612 | `	}` |
|        - |  9613 | `	/* Extract constant name */` |
|       14 |  9614 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9615 | `	if( nLen < 1 ){` |
|      ! 0 |  9616 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9617 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9618 | `		return SXRET_OK;` |
|        - |  9619 | `	}` |
|        - |  9620 | `	/* Duplicate constant value */` |
|       14 |  9621 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9622 | `	if( pValue == 0 ){` |
|      ! 0 |  9623 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9624 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9625 | `		return SXRET_OK;` |
|        - |  9626 | `	}` |
|        - |  9627 | `	/* Initialize the memory object */` |
|       14 |  9628 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9629 | `	/* Register the constant */` |
|       14 |  9630 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9631 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9632 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9633 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9634 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9635 | `		return SXRET_OK;` |
|        - |  9636 | `	}` |
|        - |  9637 | `	/* Duplicate constant value */` |
|       14 |  9638 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9639 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9640 | `		/* Lower case the constant name */` |
|      ! 0 |  9641 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9642 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9643 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9644 | `				/* UTF-8 stream */` |
|      ! 0 |  9645 | `				zCur++;` |
|      ! 0 |  9646 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9647 | `					zCur++;` |
|      ! 0 |  9648 | `				}` |
|      ! 0 |  9649 | `				continue;` |
|        - |  9650 | `			}` |
|      ! 0 |  9651 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9652 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9653 | `				zCur[0] = (char)c;` |
|      ! 0 |  9654 | `			}` |
|      ! 0 |  9655 | `			zCur++;` |
|      ! 0 |  9656 | `		}` |
|        - |  9657 | `		/* Finally,register the constant */` |
|      ! 0 |  9658 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9659 | `	}` |
|        - |  9660 | `	/* All done,return TRUE */` |
|       14 |  9661 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9662 | `	return SXRET_OK;` |
|        8 |  9663 |  |
|        - |  9664 | `/*` |
|        - |  9665 | ` * value constant(string $name)` |
|        - |  9666 | ` *  Returns the value of a constant` |
|        - |  9667 | ` * Parameter` |
|        - |  9668 | ` *  $name` |
|        - |  9669 | ` *    Name of the constant.` |
|        - |  9670 | ` * Return` |
|        - |  9671 | ` *  Constant value or NULL if not defined.` |
|        - |  9672 | ` */` |
|        8 |  9673 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9674 |  |
|        - |  9675 | `	SyHashEntry *pEntry;` |
|        - |  9676 | `	ph7_constant *pCons;` |
|        - |  9677 | `	const char *zName; /* Constant name */` |
|        - |  9678 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9679 | `	int nLen;` |
|       10 |  9680 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9681 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9682 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9683 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9684 | `		return SXRET_OK;` |
|        - |  9685 | `	}` |
|        - |  9686 | `	/* Extract the constant name */` |
|       10 |  9687 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9688 | `	/* Perform the query */` |
|       10 |  9689 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9690 | `	if( pEntry == 0 ){` |
|        3 |  9691 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9692 | `		ph7_result_null(pCtx);` |
|        3 |  9693 | `		return SXRET_OK;` |
|        - |  9694 | `	}` |
|        8 |  9695 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9696 | `	/* Point to the structure that describe the constant */` |
|        8 |  9697 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9698 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9699 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9700 | `	/* Return that value */` |
|        8 |  9701 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9702 | `	/* Cleanup */` |
|        8 |  9703 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9704 | `	return SXRET_OK;` |
|        6 |  9705 |  |
|        - |  9706 | `/*` |
|        - |  9707 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9708 | ` * defined below.` |
|        - |  9709 | ` */` |
|      452 |  9710 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9711 |  |
|      453 |  9712 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9713 | `	ph7_value sName;` |
|        - |  9714 | `	sxi32 rc;` |
|        - |  9715 | `	/* Prepare the constant name for insertion */` |
|      453 |  9716 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9717 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9718 | `	/* Perform the insertion */` |
|      453 |  9719 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9720 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9721 | `	return rc;` |
|        1 |  9722 |  |
|        - |  9723 | `/*` |
|        - |  9724 | ` * array get_defined_constants(void)` |
|        - |  9725 | ` *  Returns an associative array with the names of all defined` |
|        - |  9726 | ` *  constants.` |
|        - |  9727 | ` * Parameters` |
|        - |  9728 | ` *  NONE.` |
|        - |  9729 | ` * Returns` |
|        - |  9730 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9731 | ` */` |
|        2 |  9732 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9733 |  |
|        - |  9734 | `	ph7_value *pArray;` |
|        - |  9735 | `	/* Create the array first*/` |
|        3 |  9736 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9737 | `	if( pArray == 0 ){` |
|      ! 0 |  9738 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9739 | `		SXUNUSED(apArg);` |
|        - |  9740 | `		/* Return NULL */` |
|      ! 0 |  9741 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9742 | `		return SXRET_OK;` |
|        - |  9743 | `	}` |
|        - |  9744 | `	/* Fill the array with the defined constants */` |
|        3 |  9745 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9746 | `	/* Return the created array */` |
|        3 |  9747 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9748 | `	return SXRET_OK;` |
|        2 |  9749 |  |
|        - |  9750 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9751 | `/*` |
|        - |  9752 | ` * Section:` |
|        - |  9753 | ` *  Random numbers/string generators.` |
|        - |  9754 | ` * Status:` |
|        - |  9755 | ` *    Stable.` |
|        - |  9756 | ` */` |
|        - |  9757 | `/*` |
|        - |  9758 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9759 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9760 | ` * used by te SQLite3 library.` |
|        - |  9761 | ` */` |
|     2547 |  9762 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9763 |  |
|        - |  9764 | `	sxu32 iNum;` |
|     2549 |  9765 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2549 |  9766 | `	return iNum;` |
|        2 |  9767 |  |
|        - |  9768 | `/*` |
|        - |  9769 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9770 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9771 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9772 | ` * by te SQLite3 library.` |
|        - |  9773 | ` */` |
|   132544 |  9774 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9775 |  |
|        - |  9776 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9777 | `	int i;` |
|        - |  9778 | `	/* Generate a binary string first */` |
|   132546 |  9779 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9780 | `	/* Turn the binary string into english based alphabet */` |
|  1458154 |  9781 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1325610 |  9782 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   662806 |  9783 | `	 }` |
|   132546 |  9784 |  |
|        - |  9785 | `/*` |
|        - |  9786 | ` * int rand()` |
|        - |  9787 | ` * int mt_rand()` |
|        - |  9788 | ` * int rand(int $min,int $max)` |
|        - |  9789 | ` * int mt_rand(int $min,int $max)` |
|        - |  9790 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9791 | ` * Parameter` |
|        - |  9792 | ` *  $min` |
|        - |  9793 | ` *    The lowest value to return (default: 0)` |
|        - |  9794 | ` *  $max` |
|        - |  9795 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9796 | ` * Return` |
|        - |  9797 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9798 | ` * Note:` |
|        - |  9799 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9800 | ` *  by te SQLite3 library.` |
|        - |  9801 | ` */` |
|       20 |  9802 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9803 |  |
|        - |  9804 | `	sxu32 iNum;` |
|        - |  9805 | `	/* Generate the random number */` |
|       21 |  9806 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9807 | `	if( nArg > 1 ){` |
|        - |  9808 | `		sxu32 iMin,iMax;` |
|        3 |  9809 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9810 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9811 | `		if( iMin < iMax ){` |
|        3 |  9812 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9813 | `			if( iDiv > 0 ){` |
|        3 |  9814 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9815 | `			}` |
|        1 |  9816 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9817 | `			iNum %= iMax;` |
|      ! 0 |  9818 | `		}` |
|        1 |  9819 | `	}` |
|        - |  9820 | `	/* Return the number */` |
|       21 |  9821 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9822 | `	return SXRET_OK;` |
|        1 |  9823 |  |
|        - |  9824 | `/*` |
|        - |  9825 | ` * int getrandmax(void)` |
|        - |  9826 | ` * int mt_getrandmax(void)` |
|        - |  9827 | ` * int rc4_getrandmax(void)` |
|        - |  9828 | ` *   Show largest possible random value` |
|        - |  9829 | ` * Return` |
|        - |  9830 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9831 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9832 | ` * Note:` |
|        - |  9833 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9834 | ` *  by te SQLite3 library.` |
|        - |  9835 | ` */` |
|        4 |  9836 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9837 |  |
|        2 |  9838 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9839 | `	SXUNUSED(apArg);` |
|        5 |  9840 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9841 | `	return SXRET_OK;` |
|        1 |  9842 |  |
|        - |  9843 | `/*` |
|        - |  9844 | ` * string rand_str()` |
|        - |  9845 | ` * string rand_str(int $len)` |
|        - |  9846 | ` *  Generate a random string (English alphabet).` |
|        - |  9847 | ` * Parameter` |
|        - |  9848 | ` *  $len` |
|        - |  9849 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9850 | ` * Return` |
|        - |  9851 | ` *   A pseudo random string.` |
|        - |  9852 | ` * Note:` |
|        - |  9853 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9854 | ` *  by te SQLite3 library.` |
|        - |  9855 | ` *  This function is a symisc extension.` |
|        - |  9856 | ` */` |
|      120 |  9857 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9858 |  |
|        - |  9859 | `	char zString[1024];` |
|      122 |  9860 | `	int iLen = 0x10;` |
|      122 |  9861 | `	if( nArg > 0 ){` |
|        - |  9862 | `		/* Get the desired length */` |
|      122 |  9863 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9864 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9865 | `			/* Default length */` |
|        3 |  9866 | `			iLen = 0x10;` |
|        1 |  9867 | `		}` |
|       60 |  9868 | `	}` |
|        - |  9869 | `	/* Generate the random string */` |
|      122 |  9870 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9871 | `	/* Return the generated string */` |
|      122 |  9872 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9873 | `	return SXRET_OK;` |
|        2 |  9874 |  |
|        - |  9875 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9876 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9877 | `/* Unique ID private data */` |
|        - |  9878 | `struct unique_id_data` |
|        - |  9879 |  |
|        - |  9880 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9881 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9882 | `};` |
|        - |  9883 | `/*` |
|        - |  9884 | ` * Binary to hex consumer callback.` |
|        - |  9885 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9886 | ` * defined below.` |
|        - |  9887 | ` */` |
|      192 |  9888 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9889 |  |
|      193 |  9890 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9891 | `	sxu32 nBuflen;` |
|        - |  9892 | `	/* Extract result buffer length */` |
|      193 |  9893 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9894 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9895 | `			/*` |
|        - |  9896 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9897 | `			 * string will be 13 characters long` |
|        - |  9898 | `			 */` |
|       25 |  9899 | `		return SXERR_ABORT;` |
|        - |  9900 | `	}` |
|      169 |  9901 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9902 | `		return SXERR_ABORT;` |
|        - |  9903 | `	}` |
|        - |  9904 | `	/* Safely Consume the hex stream */` |
|      169 |  9905 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9906 | `	return SXRET_OK;` |
|       97 |  9907 |  |
|        - |  9908 | `/*` |
|        - |  9909 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9910 | ` *  Generate a unique ID` |
|        - |  9911 | ` * Parameter` |
|        - |  9912 | ` * $prefix` |
|        - |  9913 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9914 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9915 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9916 | ` * $more_entropy` |
|        - |  9917 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9918 | ` *  that the result will be unique.` |
|        - |  9919 | ` * Return` |
|        - |  9920 | ` *  Returns the unique identifier, as a string.` |
|        - |  9921 | ` */` |
|       24 |  9922 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9923 |  |
|        - |  9924 | `	struct unique_id_data sUniq;` |
|        - |  9925 | `	unsigned char zDigest[20];` |
|       25 |  9926 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9927 | `	const char *zPrefix;` |
|        - |  9928 | `	SHA1Context sCtx;` |
|        - |  9929 | `	char zRandom[7];` |
|        - |  9930 | `	int nPrefix;` |
|        - |  9931 | `	int entropy;` |
|        - |  9932 | `	/* Generate a random string first */` |
|       25 |  9933 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9934 | `	/* Initialize fields */` |
|       25 |  9935 | `	zPrefix = 0;` |
|       25 |  9936 | `	nPrefix = 0;` |
|       25 |  9937 | `	entropy = 0;` |
|       25 |  9938 | `	if( nArg > 0 ){` |
|        - |  9939 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9940 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9941 | `		if( nArg > 1 ){` |
|      ! 0 |  9942 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9943 | `		}` |
|      ! 0 |  9944 | `	}` |
|       25 |  9945 | `	SHA1Init(&sCtx);` |
|        - |  9946 | `	/* Generate the random ID */` |
|       25 |  9947 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9948 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9949 | `	}` |
|        - |  9950 | `	/* Append the random ID */` |
|       25 |  9951 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9952 | `	/* Append the random string */` |
|       25 |  9953 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9954 | `	/* Increment the number */` |
|       25 |  9955 | `	pVm->unique_id++;` |
|       25 |  9956 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9957 | `	/* Hexify the digest */` |
|       25 |  9958 | `	sUniq.pCtx = pCtx;` |
|       25 |  9959 | `	sUniq.entropy = entropy;` |
|       25 |  9960 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9961 | `	/* All done */` |
|       25 |  9962 | `	return PH7_OK;` |
|        1 |  9963 |  |
|        - |  9964 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9965 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9966 | `/*` |
|        - |  9967 | ` * Section:` |
|        - |  9968 | ` *  Language construct implementation as foreign functions.` |
|        - |  9969 | ` * Status:` |
|        - |  9970 | ` *    Stable.` |
|        - |  9971 | ` */` |
|        - |  9972 | `/*` |
|        - |  9973 | ` * void echo($string...)` |
|        - |  9974 | ` *  Output one or more messages.` |
|        - |  9975 | ` * Parameters` |
|        - |  9976 | ` *  $string` |
|        - |  9977 | ` *   Message to output.` |
|        - |  9978 | ` * Return` |
|        - |  9979 | ` *  NULL.` |
|        - |  9980 | ` */` |
|      ! 0 |  9981 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9982 |  |
|        - |  9983 | `	const char *zData;` |
|      ! 0 |  9984 | `	int nDataLen = 0;` |
|        - |  9985 | `	ph7_vm *pVm;` |
|        - |  9986 | `	int i,rc;` |
|        - |  9987 | `	/* Point to the target VM */` |
|      ! 0 |  9988 | `	pVm = pCtx->pVm;` |
|        - |  9989 | `	/* Output */` |
|      ! 0 |  9990 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9991 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9992 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9993 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9994 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9995 | `			if( rc == SXERR_ABORT ){` |
|        - |  9996 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9997 | `				return PH7_ABORT;` |
|        - |  9998 | `			}` |
|      ! 0 |  9999 | `		}` |
|      ! 0 | 10000 | `	}` |
|      ! 0 | 10001 | `	return SXRET_OK;` |
|      ! 0 | 10002 |  |
|        - | 10003 | `/*` |
|        - | 10004 | ` * int print($string...)` |
|        - | 10005 | ` *  Output one or more messages.` |
|        - | 10006 | ` * Parameters` |
|        - | 10007 | ` *  $string` |
|        - | 10008 | ` *   Message to output.` |
|        - | 10009 | ` * Return` |
|        - | 10010 | ` *  1 always.` |
|        - | 10011 | ` */` |
|        2 | 10012 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10013 |  |
|        - | 10014 | `	const char *zData;` |
|        3 | 10015 | `	int nDataLen = 0;` |
|        - | 10016 | `	ph7_vm *pVm;` |
|        - | 10017 | `	int i,rc;` |
|        - | 10018 | `	/* Point to the target VM */` |
|        3 | 10019 | `	pVm = pCtx->pVm;` |
|        - | 10020 | `	/* Output */` |
|        5 | 10021 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10022 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10023 | `		if( nDataLen > 0 ){` |
|        3 | 10024 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10025 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10026 | `			if( rc == SXERR_ABORT ){` |
|        - | 10027 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10028 | `				return PH7_ABORT;` |
|        - | 10029 | `			}` |
|        1 | 10030 | `		}` |
|        2 | 10031 | `	}` |
|        - | 10032 | `	/* Return 1 */` |
|        3 | 10033 | `	ph7_result_int(pCtx,1);` |
|        3 | 10034 | `	return SXRET_OK;` |
|        2 | 10035 |  |
|        - | 10036 | `/*` |
|        - | 10037 | ` * void exit(string $msg)` |
|        - | 10038 | ` * void exit(int $status)` |
|        - | 10039 | ` * void die(string $ms)` |
|        - | 10040 | ` * void die(int $status)` |
|        - | 10041 | ` *   Output a message and terminate program execution.` |
|        - | 10042 | ` * Parameter` |
|        - | 10043 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10044 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10045 | ` *  and not printed` |
|        - | 10046 | ` * Return` |
|        - | 10047 | ` *  NULL` |
|        - | 10048 | ` */` |
|      ! 0 | 10049 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10050 |  |
|      ! 0 | 10051 | `	if( nArg > 0 ){` |
|      ! 0 | 10052 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10053 | `			const char *zData;` |
|      ! 0 | 10054 | `			int iLen = 0;` |
|        - | 10055 | `			/* Print exit message */` |
|      ! 0 | 10056 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10057 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10058 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10059 | `			sxi32 iExitStatus;` |
|        - | 10060 | `			/* Record exit status code */` |
|      ! 0 | 10061 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10062 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10063 | `		}` |
|      ! 0 | 10064 | `	}` |
|        - | 10065 | `	/* Check if we are in an included file */` |
|      ! 0 | 10066 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10067 | `		/* Exit the entire process */` |
|      ! 0 | 10068 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10069 | `	}` |
|        - | 10070 | `	/* Abort processing immediately */` |
|      ! 0 | 10071 | `	return PH7_ABORT;` |
|      ! 0 | 10072 |  |
|        - | 10073 | `/*` |
|        - | 10074 | ` * bool isset($var,...)` |
|        - | 10075 | ` *  Finds out whether a variable is set.` |
|        - | 10076 | ` * Parameters` |
|        - | 10077 | ` *  One or more variable to check.` |
|        - | 10078 | ` * Return` |
|        - | 10079 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10080 | ` */` |
|    80394 | 10081 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10082 |  |
|        - | 10083 | `	ph7_value *pObj;` |
|    80396 | 10084 | `	int res = 0;` |
|        - | 10085 | `	int i;` |
|    80396 | 10086 | `	if( nArg < 1 ){` |
|        - | 10087 | `		/* Missing arguments,return false */` |
|      ! 0 | 10088 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10089 | `		return SXRET_OK;` |
|        - | 10090 | `	}` |
|        - | 10091 | `	/* Iterate over available arguments */` |
|   105630 | 10092 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    80396 | 10093 | `		pObj = apArg[i];` |
|    80396 | 10094 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    54568 | 10095 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10096 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10097 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10098 | `			}` |
|    27283 | 10099 | `		}` |
|    80396 | 10100 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    80396 | 10101 | `		if( !res ){` |
|        - | 10102 | `			/* Variable not set,return FALSE */` |
|    55162 | 10103 | `			ph7_result_bool(pCtx,0);` |
|    55162 | 10104 | `			return SXRET_OK;` |
|        - | 10105 | `		}` |
|    12619 | 10106 | `	}` |
|        - | 10107 | `	/* All given variable are set,return TRUE */` |
|    25236 | 10108 | `	ph7_result_bool(pCtx,1);` |
|    25236 | 10109 | `	return SXRET_OK;` |
|    40199 | 10110 |  |
|        - | 10111 | `/*` |
|        - | 10112 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10113 | ` * frame,the reference table and discard it's contents.` |
|        - | 10114 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10115 | ` */` |
|  3055194 | 10116 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10117 |  |
|        - | 10118 | `	ph7_value *pObj;` |
|        - | 10119 | `	VmRefObj *pRef;` |
|  3055196 | 10120 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3055196 | 10121 | `	if( pObj ){` |
|        - | 10122 | `		/* Release the object */` |
|  3055196 | 10123 | `		PH7_MemObjRelease(pObj);` |
|  1527597 | 10124 | `	}` |
|        - | 10125 | `	/* Remove old reference links */` |
|  3055196 | 10126 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3055196 | 10127 | `	if( pRef ){` |
|  3055190 | 10128 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10129 | `		/* Unlink from the reference table */` |
|  3055190 | 10130 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3055190 | 10131 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10132 | `			VmSlot sFree;` |
|        - | 10133 | `			/* Restore to the free list */` |
|  3055184 | 10134 | `			sFree.nIdx = nObjIdx;` |
|  3055184 | 10135 | `			sFree.pUserData = 0;` |
|  3055184 | 10136 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1527591 | 10137 | `		}` |
|  1527594 | 10138 | `	}` |
|  3055196 | 10139 | `	return SXRET_OK;` |
|        2 | 10140 |  |
|        - | 10141 | `/*` |
|        - | 10142 | ` * void unset($var,...)` |
|        - | 10143 | ` *   Unset one or more given variable.` |
|        - | 10144 | ` * Parameters` |
|        - | 10145 | ` *  One or more variable to unset.` |
|        - | 10146 | ` * Return` |
|        - | 10147 | ` *  Nothing.` |
|        - | 10148 | ` */` |
|     7102 | 10149 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10150 |  |
|        - | 10151 | `	ph7_value *pObj;` |
|        - | 10152 | `	ph7_vm *pVm;` |
|        - | 10153 | `	int i;` |
|        - | 10154 | `	/* Point to the target VM */` |
|     7104 | 10155 | `	pVm = pCtx->pVm;` |
|        - | 10156 | `	/* Iterate and unset */` |
|    14206 | 10157 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7104 | 10158 | `		pObj = apArg[i];` |
|     7104 | 10159 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10160 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10161 | `				/* Throw an error */` |
|      ! 0 | 10162 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10163 | `			}` |
|      ! 0 | 10164 | `		}else{` |
|     7104 | 10165 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 10166 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7104 | 10167 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7098 | 10168 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3548 | 10169 | `			}` |
|        - | 10170 | `		}` |
|     3553 | 10171 | `	}` |
|     7104 | 10172 | `	return SXRET_OK;` |
|        2 | 10173 |  |
|        - | 10174 | `/*` |
|        - | 10175 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 10176 | ` */` |
|      110 | 10177 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10178 |  |
|      111 | 10179 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 10180 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10181 | `	ph7_value *pObj;` |
|        - | 10182 | `	sxu32 nIdx;` |
|        - | 10183 | `	/* Extract the memory object */` |
|      111 | 10184 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 10185 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 10186 | `	if( pObj ){` |
|      111 | 10187 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 10188 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 10189 | `				SyString sName;` |
|        - | 10190 | `				ph7_value sKey;` |
|        - | 10191 | `				/* Perform the insertion */` |
|      109 | 10192 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 10193 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 10194 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 10195 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 10196 | `			}` |
|       54 | 10197 | `		}` |
|       55 | 10198 | `	}` |
|      111 | 10199 | `	return SXRET_OK;` |
|        1 | 10200 |  |
|        - | 10201 | `/*` |
|        - | 10202 | ` * array get_defined_vars(void)` |
|        - | 10203 | ` *  Returns an array of all defined variables.` |
|        - | 10204 | ` * Parameter` |
|        - | 10205 | ` *  None` |
|        - | 10206 | ` * Return` |
|        - | 10207 | ` *  An array with all the variables defined in the current scope.` |
|        - | 10208 | ` */` |
|        2 | 10209 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10210 |  |
|        3 | 10211 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10212 | `	ph7_value *pArray;` |
|        - | 10213 | `	/* Create a new array */` |
|        3 | 10214 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10215 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10216 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10217 | `		SXUNUSED(apArg);` |
|        - | 10218 | `		/* Return NULL */` |
|      ! 0 | 10219 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10220 | `		return SXRET_OK;` |
|        - | 10221 | `	}` |
|        - | 10222 | `	/* Superglobals first */` |
|        3 | 10223 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 10224 | `	/* Then variable defined in the current frame */` |
|        3 | 10225 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 10226 | `	/* Finally,return the created array */` |
|        3 | 10227 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10228 | `	return SXRET_OK;` |
|        2 | 10229 |  |
|        - | 10230 | `/*` |
|        - | 10231 | ` * bool gettype($var)` |
|        - | 10232 | ` *  Get the type of a variable` |
|        - | 10233 | ` * Parameters` |
|        - | 10234 | ` *   $var` |
|        - | 10235 | ` *    The variable being type checked.` |
|        - | 10236 | ` * Return` |
|        - | 10237 | ` *   String representation of the given variable type.` |
|        - | 10238 | ` */` |
|       32 | 10239 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10240 |  |
|       34 | 10241 | `	const char *zType = "Empty";` |
|       34 | 10242 | `	if( nArg > 0 ){` |
|       34 | 10243 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 10244 | `	}` |
|        - | 10245 | `	/* Return the variable type */` |
|       34 | 10246 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 10247 | `	return SXRET_OK;` |
|        2 | 10248 |  |
|        - | 10249 | `/*` |
|        - | 10250 | ` * string get_resource_type(resource $handle)` |
|        - | 10251 | ` *  This function gets the type of the given resource.` |
|        - | 10252 | ` * Parameters` |
|        - | 10253 | ` *  $handle` |
|        - | 10254 | ` *  The evaluated resource handle.` |
|        - | 10255 | ` * Return` |
|        - | 10256 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 10257 | ` *  representing its type. If the type is not identified by this function` |
|        - | 10258 | ` *  the return value will be the string Unknown.` |
|        - | 10259 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 10260 | ` *  is not a resource.` |
|        - | 10261 | ` */` |
|        2 | 10262 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10263 |  |
|        3 | 10264 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 10265 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 10266 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10267 | `		return PH7_OK;` |
|        - | 10268 | `	}` |
|        3 | 10269 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 10270 | `	return SXRET_OK;` |
|        2 | 10271 |  |
|        - | 10272 | `/*` |
|        - | 10273 | ` * void var_dump(expression,....)` |
|        - | 10274 | ` *   var_dump � Dumps information about a variable` |
|        - | 10275 | ` * Parameters` |
|        - | 10276 | ` *   One or more expression to dump.` |
|        - | 10277 | ` * Returns` |
|        - | 10278 | ` *  Nothing.` |
|        - | 10279 | ` */` |
|      218 | 10280 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10281 |  |
|        - | 10282 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 10283 | `	int i;` |
|      220 | 10284 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 10285 | `	/* Dump one or more expressions */` |
|      444 | 10286 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 10287 | `		ph7_value *pObj = apArg[i];` |
|        - | 10288 | `		/* Reset the working buffer */` |
|      226 | 10289 | `		SyBlobReset(&sDump);` |
|        - | 10290 | `		/* Dump the given expression */` |
|      226 | 10291 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 10292 | `		/* Output */` |
|      226 | 10293 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 10294 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 10295 | `		}` |
|      114 | 10296 | `	}` |
|        - | 10297 | `	/* Release the working buffer */` |
|      220 | 10298 | `	SyBlobRelease(&sDump);` |
|      220 | 10299 | `	return SXRET_OK;` |
|        2 | 10300 |  |
|        - | 10301 | `/*` |
|        - | 10302 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 10303 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 10304 | ` * Parameters` |
|        - | 10305 | ` *   expression: Expression to dump` |
|        - | 10306 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 10307 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 10308 | ` *            print_r() will return the information rather than print it.` |
|        - | 10309 | ` * Return` |
|        - | 10310 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 10311 | ` *  Otherwise, the return value is TRUE.` |
|        - | 10312 | ` */` |
|       16 | 10313 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10314 |  |
|       17 | 10315 | `	int ret_string = 0;` |
|        - | 10316 | `	SyBlob sDump;` |
|       17 | 10317 | `	if( nArg < 1 ){` |
|        - | 10318 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 10319 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10320 | `		return SXRET_OK;` |
|        - | 10321 | `	}` |
|       17 | 10322 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 10323 | `	if ( nArg > 1 ){` |
|        - | 10324 | `		/* Where to redirect output */` |
|       11 | 10325 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 10326 | `	}` |
|        - | 10327 | `	/* Generate dump */` |
|       17 | 10328 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 10329 | `	if( !ret_string ){` |
|        - | 10330 | `		/* Output dump */` |
|        7 | 10331 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10332 | `		/* Return true */` |
|        7 | 10333 | `		ph7_result_bool(pCtx,1);` |
|        4 | 10334 | `	}else{` |
|        - | 10335 | `		/* Generated dump as return value */` |
|       11 | 10336 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10337 | `	}` |
|        - | 10338 | `	/* Release the working buffer */` |
|       17 | 10339 | `	SyBlobRelease(&sDump);` |
|       17 | 10340 | `	return SXRET_OK;` |
|        9 | 10341 |  |
|        - | 10342 | `/*` |
|        - | 10343 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 10344 | ` * Same job as print_r. (see coment above)` |
|        - | 10345 | ` */` |
|        2 | 10346 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10347 |  |
|        3 | 10348 | `	int ret_string = 0;` |
|        - | 10349 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 10350 | `	if( nArg < 1 ){` |
|        - | 10351 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 10352 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10353 | `		return SXRET_OK;` |
|        - | 10354 | `	}` |
|        3 | 10355 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 10356 | `	if ( nArg > 1 ){` |
|        - | 10357 | `		/* Where to redirect output */` |
|        3 | 10358 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 10359 | `	}` |
|        - | 10360 | `	/* Generate dump */` |
|        3 | 10361 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 10362 | `	if( !ret_string ){` |
|        - | 10363 | `		/* Output dump */` |
|      ! 0 | 10364 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10365 | `		/* Return NULL */` |
|      ! 0 | 10366 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10367 | `	}else{` |
|        - | 10368 | `		/* Generated dump as return value */` |
|        3 | 10369 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10370 | `	}` |
|        - | 10371 | `	/* Release the working buffer */` |
|        3 | 10372 | `	SyBlobRelease(&sDump);` |
|        3 | 10373 | `	return SXRET_OK;` |
|        2 | 10374 |  |
|        - | 10375 | `/*` |
|        - | 10376 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 10377 | ` *  Set/get the various assert flags.` |
|        - | 10378 | ` * Parameter` |
|        - | 10379 | ` * $what` |
|        - | 10380 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 10381 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 10382 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 10383 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 10384 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 10385 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 10386 | ` * $value` |
|        - | 10387 | ` *   An optional new value for the option.` |
|        - | 10388 | ` * Return` |
|        - | 10389 | ` *  Old setting on success or FALSE on failure.` |
|        - | 10390 | ` */` |
|       28 | 10391 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10392 |  |
|       30 | 10393 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10394 | `	int iOption;` |
|        - | 10395 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 10396 | `	if( nArg < 1 ){` |
|        3 | 10397 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10398 | `			"ArgumentCountError",` |
|        - | 10399 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 10400 | `			);` |
|        - | 10401 | `	}` |
|        - | 10402 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 10403 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 10404 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 10405 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10406 | `			"TypeError",` |
|        - | 10407 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 10408 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 10409 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 10410 | `			);` |
|        - | 10411 | `	}` |
|       28 | 10412 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 10413 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 10414 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 10415 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 10416 | `	switch( iOption ){` |
|        5 | 10417 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 10418 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 10419 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 10420 | `		if( nArg > 1 ){` |
|        5 | 10421 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10422 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 10423 | `			}else{` |
|        3 | 10424 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 10425 | `			}` |
|        2 | 10426 | `		}` |
|       12 | 10427 | `		break;` |
|        1 | 10428 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 10429 | `		/* Return old callback or null */` |
|        3 | 10430 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 10431 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 10432 | `		}else{` |
|        3 | 10433 | `			ph7_result_null(pCtx);` |
|        - | 10434 | `		}` |
|        3 | 10435 | `		if( nArg > 1 ){` |
|      ! 0 | 10436 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 10437 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 10438 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 10439 | `			}else{` |
|      ! 0 | 10440 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 10441 | `			}` |
|      ! 0 | 10442 | `		}` |
|        3 | 10443 | `		break;` |
|        5 | 10444 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 10445 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 10446 | `		if( nArg > 1 ){` |
|        5 | 10447 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10448 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 10449 | `			}else{` |
|        3 | 10450 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 10451 | `			}` |
|        2 | 10452 | `		}` |
|       11 | 10453 | `		break;` |
|      ! 0 | 10454 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 10455 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10456 | `		break;` |
|        1 | 10457 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 10458 | `		ph7_result_int(pCtx, 1);` |
|        3 | 10459 | `		break;` |
|      ! 0 | 10460 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 10461 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10462 | `		break;` |
|        1 | 10463 | `	default:` |
|        - | 10464 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 10465 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10466 | `			"ValueError",` |
|        - | 10467 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 10468 | `			);` |
|        - | 10469 | `	}` |
|       26 | 10470 | `	return PH7_OK;` |
|       16 | 10471 |  |
|        - | 10472 | `/*` |
|        - | 10473 | ` * bool assert(mixed $assertion)` |
|        - | 10474 | ` *  Checks if assertion is FALSE.` |
|        - | 10475 | ` * Parameter` |
|        - | 10476 | ` *  $assertion` |
|        - | 10477 | ` *    The assertion to test.` |
|        - | 10478 | ` * Return` |
|        - | 10479 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 10480 | ` */` |
|       24 | 10481 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10482 |  |
|       26 | 10483 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10484 | `	int iFlags,iResult;` |
|        - | 10485 | `	const char *zDesc;` |
|        - | 10486 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 10487 | `	if( nArg < 1 ){` |
|        3 | 10488 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10489 | `			"ArgumentCountError",` |
|        - | 10490 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 10491 | `			);` |
|        - | 10492 | `	}` |
|       24 | 10493 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 10494 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 10495 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 10496 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 10497 | `		return PH7_OK;` |
|        - | 10498 | `	}` |
|        - | 10499 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 10500 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 10501 | `	if( !iResult ){` |
|        - | 10502 | `		/* Assertion failed */` |
|        - | 10503 | `		/* Extract optional description */` |
|       13 | 10504 | `		zDesc = 0;` |
|       13 | 10505 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10506 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 10507 | `		}` |
|       13 | 10508 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 10509 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 10510 | `			ph7_value sFile,sLine;` |
|        - | 10511 | `			ph7_value *apCbArg[3];` |
|        - | 10512 | `			SyString *pFile;` |
|        - | 10513 | `			/* Extract the processed script */` |
|      ! 0 | 10514 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 10515 | `			if( pFile == 0 ){` |
|      ! 0 | 10516 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 10517 | `			}` |
|        - | 10518 | `			/* Invoke the callback */` |
|      ! 0 | 10519 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 10520 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 10521 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 10522 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 10523 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 10524 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 10525 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 10526 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 10527 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 10528 | `		}` |
|       13 | 10529 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 10530 | `			/* Abort VM execution immediately */` |
|      ! 0 | 10531 | `			return PH7_ABORT;` |
|        - | 10532 | `		}` |
|        - | 10533 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 10534 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 10535 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10536 | `				"AssertionError",` |
|        - | 10537 | `				"%s",` |
|        1 | 10538 | `				zDesc` |
|        - | 10539 | `				);` |
|      ! 0 | 10540 | `		}else{` |
|       11 | 10541 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10542 | `				"AssertionError",` |
|        - | 10543 | `				"assert(false)"` |
|        - | 10544 | `				);` |
|        - | 10545 | `		}` |
|        - | 10546 | `	}` |
|        - | 10547 | `	/* Assertion passed */` |
|       11 | 10548 | `	ph7_result_bool(pCtx,1);` |
|       11 | 10549 | `	return PH7_OK;` |
|       14 | 10550 |  |
|        - | 10551 | `/*` |
|        - | 10552 | ` * Section:` |
|        - | 10553 | ` *  Error reporting functions.` |
|        - | 10554 | ` * Status:` |
|        - | 10555 | ` *    Stable.` |
|        - | 10556 | ` */` |
|        - | 10557 | `/*` |
|        - | 10558 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 10559 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 10560 | ` * Parameters` |
|        - | 10561 | ` *  $error_msg` |
|        - | 10562 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 10563 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 10564 | ` * $error_type` |
|        - | 10565 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 10566 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 10567 | ` * Return` |
|        - | 10568 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 10569 | ` */` |
|       12 | 10570 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10571 |  |
|       14 | 10572 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 10573 | `	int rc = PH7_OK;` |
|       14 | 10574 | `	if( nArg > 0 ){` |
|        - | 10575 | `		const char *zErr;` |
|        - | 10576 | `		int nLen;` |
|        - | 10577 | `		/* Extract the error message */` |
|       12 | 10578 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 10579 | `		if( nArg > 1 ){` |
|        - | 10580 | `			/* Extract the error type */` |
|       12 | 10581 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 10582 | `			switch( nErr ){` |
|        1 | 10583 | `			case 1:   /* E_ERROR */` |
|        - | 10584 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 10585 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 10586 | `			case 256: /* E_USER_ERROR */` |
|        3 | 10587 | `				nErr = PH7_CTX_ERR;` |
|        3 | 10588 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 10589 | `				break;` |
|        1 | 10590 | `			case 2:   /* E_WARNING */` |
|        - | 10591 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 10592 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 10593 | `			case 512: /* E_USER_WARNING */` |
|        3 | 10594 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 10595 | `				break;` |
|        3 | 10596 | `			default:` |
|        8 | 10597 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 10598 | `				break;` |
|        - | 10599 | `			}` |
|        5 | 10600 | `		}` |
|        - | 10601 | `		/* Report error */` |
|       12 | 10602 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 10603 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 10604 | `			return rc;` |
|        - | 10605 | `		}` |
|        - | 10606 | `		/* Return true */` |
|       12 | 10607 | `		ph7_result_bool(pCtx,1);` |
|        7 | 10608 | `	}else{` |
|        - | 10609 | `		/* Missing arguments,return FALSE */` |
|        3 | 10610 | `		ph7_result_bool(pCtx,0);` |
|        - | 10611 | `	}` |
|       14 | 10612 | `	return rc;` |
|        8 | 10613 |  |
|        - | 10614 | `/*` |
|        - | 10615 | ` * int error_reporting([int $level])` |
|        - | 10616 | ` *  Sets which PHP errors are reported.` |
|        - | 10617 | ` * Parameters` |
|        - | 10618 | ` *  $level` |
|        - | 10619 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10620 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10621 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10622 | ` *   levels will not always behave as expected.` |
|        - | 10623 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10624 | ` *   in the predefined constants.` |
|        - | 10625 | ` * Return` |
|        - | 10626 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10627 | ` *   parameter is given.` |
|        - | 10628 | ` */` |
|       38 | 10629 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10630 |  |
|       40 | 10631 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10632 | `	int nOld;` |
|        - | 10633 | `	/* Extract the old reporting level */` |
|       40 | 10634 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10635 | `	if( nArg > 0 ){` |
|        - | 10636 | `		int nNew;` |
|        - | 10637 | `		/* Extract the desired error reporting level */` |
|       32 | 10638 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10639 | `		if( !nNew ){` |
|        - | 10640 | `			/* Do not report errors at all */` |
|        5 | 10641 | `			pVm->bErrReport = 0;` |
|        3 | 10642 | `		}else{` |
|        - | 10643 | `			/* Report all errors */` |
|       28 | 10644 | `			pVm->bErrReport = 1;` |
|        - | 10645 | `		}` |
|       15 | 10646 | `	}` |
|        - | 10647 | `	/* Return the old level */` |
|       40 | 10648 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10649 | `	return PH7_OK;` |
|        2 | 10650 |  |
|        - | 10651 | `/*` |
|        - | 10652 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10653 | ` *  Send an error message somewhere.` |
|        - | 10654 | ` * Parameter` |
|        - | 10655 | ` *  $message` |
|        - | 10656 | ` *   The error message that should be logged.` |
|        - | 10657 | ` *  $message_type` |
|        - | 10658 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10659 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10660 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10661 | ` *       This is the default option.` |
|        - | 10662 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10663 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10664 | ` *    2  No longer an option.` |
|        - | 10665 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10666 | ` *       to the end of the message string.` |
|        - | 10667 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10668 | ` *  $destination` |
|        - | 10669 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10670 | ` *  $extra_headers` |
|        - | 10671 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10672 | ` * Return` |
|        - | 10673 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10674 | ` * NOTE:` |
|        - | 10675 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10676 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10677 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10678 | ` *  Otherwise this function is no-op.` |
|        - | 10679 | ` */` |
|        4 | 10680 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10681 |  |
|        - | 10682 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10683 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10684 | `	int iType = 0;` |
|        5 | 10685 | `	if( nArg < 1 ){` |
|        - | 10686 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10687 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10688 | `		return PH7_OK;` |
|        - | 10689 | `	}` |
|        5 | 10690 | `	if( pVm->xErrLog  ){` |
|        - | 10691 | `		/* Invoke the user callback */` |
|      ! 0 | 10692 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10693 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10694 | `		if( nArg > 1 ){` |
|      ! 0 | 10695 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10696 | `			if( nArg > 2 ){` |
|      ! 0 | 10697 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10698 | `				if( nArg > 3 ){` |
|      ! 0 | 10699 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10700 | `				}` |
|      ! 0 | 10701 | `			}` |
|      ! 0 | 10702 | `		}` |
|      ! 0 | 10703 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10704 | `	}` |
|        - | 10705 | `	/* Retun TRUE */` |
|        5 | 10706 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10707 | `	return PH7_OK;` |
|        3 | 10708 |  |
|        - | 10709 | `/*` |
|        - | 10710 | ` * bool restore_exception_handler(void)` |
|        - | 10711 | ` *  Restores the previously defined exception handler function.` |
|        - | 10712 | ` * Parameter` |
|        - | 10713 | ` *  None` |
|        - | 10714 | ` * Return` |
|        - | 10715 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10716 | ` */` |
|        4 | 10717 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10718 |  |
|        5 | 10719 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10720 | `	ph7_value *pOld,*pNew;` |
|        - | 10721 | `	/* Point to the old and the new handler */` |
|        5 | 10722 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10723 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10724 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10725 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10726 | `		SXUNUSED(apArg);` |
|        - | 10727 | `		/* No installed handler,return FALSE */` |
|        5 | 10728 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10729 | `		return PH7_OK;` |
|        - | 10730 | `	}` |
|        - | 10731 | `	/* Copy the old handler */` |
|      ! 0 | 10732 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10733 | `	PH7_MemObjRelease(pOld);` |
|        - | 10734 | `	/* Return TRUE */` |
|      ! 0 | 10735 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10736 | `	return PH7_OK;` |
|        3 | 10737 |  |
|        - | 10738 | `/*` |
|        - | 10739 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10740 | ` *  Sets a user-defined exception handler function.` |
|        - | 10741 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10742 | ` * NOTE` |
|        - | 10743 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10744 | ` *  the satndard PHP engine.` |
|        - | 10745 | ` * Parameters` |
|        - | 10746 | ` *  $exception_handler` |
|        - | 10747 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10748 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10749 | ` *   that was thrown.` |
|        - | 10750 | ` *  Note:` |
|        - | 10751 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10752 | ` * Return` |
|        - | 10753 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10754 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10755 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10756 | ` */` |
|        4 | 10757 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10758 |  |
|        6 | 10759 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10760 | `	ph7_value *pOld,*pNew;` |
|        - | 10761 | `	/* Point to the old and the new handler */` |
|        6 | 10762 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10763 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10764 | `	/* Return the old handler */` |
|        6 | 10765 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10766 | `	if( nArg > 0 ){` |
|        6 | 10767 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10768 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10769 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10770 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10771 | `		}else{` |
|        6 | 10772 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10773 | `			/* Install the new handler */` |
|        6 | 10774 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10775 | `		}` |
|        2 | 10776 | `	}` |
|        6 | 10777 | `	return PH7_OK;` |
|        2 | 10778 |  |
|        - | 10779 | `/*` |
|        - | 10780 | ` * bool restore_error_handler(void)` |
|        - | 10781 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10782 | ` * Parameters:` |
|        - | 10783 | ` *  None.` |
|        - | 10784 | ` * Return` |
|        - | 10785 | ` *  Always TRUE.` |
|        - | 10786 | ` */` |
|        4 | 10787 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10788 |  |
|        5 | 10789 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10790 | `	ph7_value *pOld,*pNew;` |
|        - | 10791 | `	/* Point to the old and the new handler */` |
|        5 | 10792 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10793 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10794 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10795 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10796 | `		SXUNUSED(apArg);` |
|        - | 10797 | `		/* No installed callback,return FALSE */` |
|        5 | 10798 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10799 | `		return PH7_OK;` |
|        - | 10800 | `	}` |
|        - | 10801 | `	/* Copy the old callback */` |
|      ! 0 | 10802 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10803 | `	PH7_MemObjRelease(pOld);` |
|        - | 10804 | `	/* Return TRUE */` |
|      ! 0 | 10805 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10806 | `	return PH7_OK;` |
|        3 | 10807 |  |
|        - | 10808 | `/*` |
|        - | 10809 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10810 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10811 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10812 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10813 | ` *  Sets a user-defined error handler function.` |
|        - | 10814 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10815 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10816 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10817 | ` *  conditions (using trigger_error()).` |
|        - | 10818 | ` * Parameters` |
|        - | 10819 | ` *  $error_handler` |
|        - | 10820 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10821 | ` *   describing the error.` |
|        - | 10822 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10823 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10824 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10825 | ` *   The function can be shown as:` |
|        - | 10826 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10827 | ` *     errno` |
|        - | 10828 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10829 | ` *   errstr` |
|        - | 10830 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10831 | ` *   errfile` |
|        - | 10832 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10833 | ` *     was raised in, as a string.` |
|        - | 10834 | ` *  Note:` |
|        - | 10835 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10836 | ` * Return` |
|        - | 10837 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10838 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10839 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10840 | ` */` |
|     9694 | 10841 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10842 |  |
|     9696 | 10843 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10844 | `	ph7_value *pOld,*pNew;` |
|        - | 10845 | `	/* Point to the old and the new handler */` |
|     9696 | 10846 | `	pOld = &pVm->aErrCB[0];` |
|     9696 | 10847 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10848 | `	/* Return the old handler */` |
|     9696 | 10849 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9696 | 10850 | `	if( nArg > 0 ){` |
|     9696 | 10851 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10852 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4847 | 10853 | `			PH7_MemObjRelease(pNew);` |
|     4847 | 10854 | `			ph7_result_bool(pCtx,1);` |
|     2424 | 10855 | `		}else{` |
|     4850 | 10856 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10857 | `			/* Install the new handler */` |
|     4850 | 10858 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10859 | `		}` |
|     4847 | 10860 | `	}` |
|     9696 | 10861 | `	return PH7_OK;` |
|        2 | 10862 |  |
|        - | 10863 | `/*` |
|        - | 10864 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10865 | ` *  Generates a backtrace.` |
|        - | 10866 | ` * Paramaeter` |
|        - | 10867 | ` *  $options` |
|        - | 10868 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10869 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10870 | ` *   all the function/method arguments, to save memory.` |
|        - | 10871 | ` * $limit` |
|        - | 10872 | ` *   (Not Used)` |
|        - | 10873 | ` * Return` |
|        - | 10874 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10875 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10876 | ` *          Name        Type      Description` |
|        - | 10877 | ` *          ------      ------     -----------` |
|        - | 10878 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10879 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10880 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10881 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10882 | ` *          object      object    The current object.` |
|        - | 10883 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10884 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10885 | ` */` |
|      588 | 10886 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10887 |  |
|      590 | 10888 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10889 | `	ph7_value *pArray;` |
|        - | 10890 | `	ph7_class *pClass;` |
|        - | 10891 | `	ph7_value *pValue;` |
|        - | 10892 | `	SyString *pFile;` |
|        - | 10893 | `	/* Create a new array */` |
|      590 | 10894 | `	pArray = ph7_context_new_array(pCtx);` |
|      590 | 10895 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      590 | 10896 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10897 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10898 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10899 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10900 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10901 | `		SXUNUSED(apArg);` |
|      ! 0 | 10902 | `		return PH7_OK;` |
|        - | 10903 | `	}` |
|        - | 10904 | `	/* Dump running function name and it's arguments  */` |
|      590 | 10905 | `	if( pVm->pFrame->pParent ){` |
|      590 | 10906 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10907 | `		ph7_vm_func *pFunc;` |
|        - | 10908 | `		ph7_value *pArg;` |
|      590 | 10909 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      590 | 10910 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      590 | 10911 | `		if( pFrame->pParent && pFunc ){` |
|      590 | 10912 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      590 | 10913 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      590 | 10914 | `			ph7_value_reset_string_cursor(pValue);` |
|      294 | 10915 | `		}` |
|        - | 10916 | `		/* Function arguments */` |
|      590 | 10917 | `		pArg = ph7_context_new_array(pCtx);` |
|      590 | 10918 | `		if( pArg  ){` |
|        - | 10919 | `			ph7_value *pObj;` |
|        - | 10920 | `			VmSlot *aSlot;` |
|        - | 10921 | `			sxu32 n;` |
|        - | 10922 | `			/* Start filling the array with the given arguments */` |
|      590 | 10923 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2346 | 10924 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1758 | 10925 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1758 | 10926 | `				if( pObj ){` |
|     1758 | 10927 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      878 | 10928 | `				}` |
|      880 | 10929 | `			}` |
|        - | 10930 | `			/* Save the array */` |
|      590 | 10931 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      294 | 10932 | `		}` |
|      294 | 10933 | `	}` |
|      590 | 10934 | `	ph7_value_int(pValue,1);` |
|        - | 10935 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10936 | `	 * line numbers at run-time. )` |
|        - | 10937 | `	 */` |
|      590 | 10938 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10939 | `	/* Current processed script */` |
|      590 | 10940 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      590 | 10941 | `	if( pFile ){` |
|      590 | 10942 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      590 | 10943 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      590 | 10944 | `		ph7_value_reset_string_cursor(pValue);` |
|      294 | 10945 | `	}` |
|        - | 10946 | `	/* Top class */` |
|      590 | 10947 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      590 | 10948 | `	if( pClass ){` |
|      586 | 10949 | `		ph7_value_reset_string_cursor(pValue);` |
|      586 | 10950 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      586 | 10951 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      292 | 10952 | `	}` |
|        - | 10953 | `	/* Return the freshly created array */` |
|      590 | 10954 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10955 | `	/*` |
|        - | 10956 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10957 | `	 * as soon we return from this function.` |
|        - | 10958 | `	 */` |
|      590 | 10959 | `	return PH7_OK;` |
|      296 | 10960 |  |
|        - | 10961 | `/*` |
|        - | 10962 | ` * Generate a small backtrace.` |
|        - | 10963 | ` * Store the generated dump in the given BLOB` |
|        - | 10964 | ` */` |
|        4 | 10965 | `static int VmMiniBacktrace(` |
|        - | 10966 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10967 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10968 | `	)` |
|        1 | 10969 |  |
|        5 | 10970 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10971 | `	ph7_vm_func *pFunc;` |
|        - | 10972 | `	ph7_class *pClass;` |
|        - | 10973 | `	SyString *pFile;` |
|        - | 10974 | `	/* Called function */` |
|        5 | 10975 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10976 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10977 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10978 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10979 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10980 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10981 | `	}else{` |
|      ! 0 | 10982 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10983 | `	}` |
|        5 | 10984 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10985 | `	/* Current processed script */` |
|        5 | 10986 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10987 | `	if( pFile ){` |
|        5 | 10988 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10989 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10990 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10991 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10992 | `	}` |
|        - | 10993 | `	/* Top class */` |
|        5 | 10994 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10995 | `	if( pClass ){` |
|      ! 0 | 10996 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10997 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10998 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10999 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11000 | `	}` |
|        5 | 11001 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11002 | `	/* All done */` |
|        5 | 11003 | `	return SXRET_OK;` |
|        1 | 11004 |  |
|        - | 11005 | `/*` |
|        - | 11006 | ` * void debug_print_backtrace()` |
|        - | 11007 | ` *  Prints a backtrace` |
|        - | 11008 | ` * Parameters` |
|        - | 11009 | ` * None` |
|        - | 11010 | ` * Return` |
|        - | 11011 | ` * NULL` |
|        - | 11012 | ` */` |
|        2 | 11013 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11014 |  |
|        3 | 11015 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11016 | `	SyBlob sDump;` |
|        3 | 11017 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11018 | `	/* Generate the backtrace */` |
|        3 | 11019 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11020 | `	/* Output backtrace */` |
|        3 | 11021 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11022 | `	/* All done,cleanup */` |
|        3 | 11023 | `	SyBlobRelease(&sDump);` |
|        1 | 11024 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11025 | `	SXUNUSED(apArg);` |
|        3 | 11026 | `	return PH7_OK;` |
|        1 | 11027 |  |
|        - | 11028 | `/*` |
|        - | 11029 | ` * string debug_string_backtrace()` |
|        - | 11030 | ` *  Generate a backtrace` |
|        - | 11031 | ` * Parameters` |
|        - | 11032 | ` * None` |
|        - | 11033 | ` * Return` |
|        - | 11034 | ` *  A mini backtrace().` |
|        - | 11035 | ` * Note that this is a symisc extension.` |
|        - | 11036 | ` */` |
|        2 | 11037 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11038 |  |
|        3 | 11039 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11040 | `	SyBlob sDump;` |
|        3 | 11041 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11042 | `	/* Generate the backtrace */` |
|        3 | 11043 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11044 | `	/* Return the backtrace */` |
|        3 | 11045 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11046 | `	/* All done,cleanup */` |
|        3 | 11047 | `	SyBlobRelease(&sDump);` |
|        1 | 11048 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11049 | `	SXUNUSED(apArg);` |
|        3 | 11050 | `	return PH7_OK;` |
|        1 | 11051 |  |
|        - | 11052 | `/*` |
|        - | 11053 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11054 | ` * exception is triggered.` |
|        - | 11055 | ` */` |
|      480 | 11056 | `static sxi32 VmUncaughtException(` |
|        - | 11057 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11058 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11059 | `	)` |
|        1 | 11060 |  |
|        - | 11061 | `	ph7_value *apArg[2],sArg;` |
|      481 | 11062 | `	int nArg = 1;` |
|        - | 11063 | `	sxi32 rc;` |
|      481 | 11064 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11065 | `		/* Nesting limit reached */` |
|      ! 0 | 11066 | `		return SXRET_OK;` |
|        - | 11067 | `	}` |
|        - | 11068 | `	/* Call any exception handler if available */` |
|      481 | 11069 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 11070 | `	if( pThis ){` |
|        - | 11071 | `		/* Load the exception instance */` |
|      481 | 11072 | `		sArg.x.pOther = pThis;` |
|      481 | 11073 | `		pThis->iRef++;` |
|      481 | 11074 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 11075 | `	}else{` |
|      ! 0 | 11076 | `		nArg = 0;` |
|        - | 11077 | `	}` |
|      481 | 11078 | `	apArg[0] = &sArg;` |
|        - | 11079 | `	/* Call the exception handler if available */` |
|      481 | 11080 | `	pVm->nExceptDepth++;` |
|      481 | 11081 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 11082 | `	pVm->nExceptDepth--;` |
|      481 | 11083 | `	if( rc != SXRET_OK ){` |
|        - | 11084 | `		SyBlob sMsgBuf;` |
|      479 | 11085 | `		const char *zClass = "Exception";` |
|      479 | 11086 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11087 | `		const char *zMsg;` |
|        - | 11088 | `		sxu32 nMsg;` |
|        - | 11089 | `		const char *zFuncName;` |
|        - | 11090 | `		int nFuncLen;` |
|      479 | 11091 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 11092 | `		if( pThis ){` |
|        - | 11093 | `			ph7_class_method *pGetMessage;` |
|        - | 11094 | `			ph7_value sMsg;` |
|        - | 11095 | `			const char *zTmp;` |
|        - | 11096 | `			int nTmp;` |
|      479 | 11097 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 11098 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 11099 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 11100 | `			if( pGetMessage ){` |
|      479 | 11101 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 11102 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 11103 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 11104 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 11105 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 11106 | `					}` |
|      239 | 11107 | `				}` |
|      479 | 11108 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 11109 | `			}` |
|      239 | 11110 | `		}` |
|      479 | 11111 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 11112 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 11113 | `		}` |
|      479 | 11114 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 11115 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 11116 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 11117 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 11118 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11119 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 11120 | `		rc = SXERR_ABORT;` |
|      239 | 11121 | `	}` |
|      481 | 11122 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 11123 | `	return rc;` |
|      241 | 11124 |  |
|        - | 11125 | `/*` |
|        - | 11126 | ` * Throw a user exception.` |
|        - | 11127 | ` *` |
|        - | 11128 | ` * Exception dispatch follows this sequence:` |
|        - | 11129 | ` *` |
|        - | 11130 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11131 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11132 | ` *` |
|        - | 11133 | ` * 2. If NO catch matches:` |
|        - | 11134 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11135 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11136 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11137 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11138 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11139 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11140 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11141 | ` *` |
|        - | 11142 | ` * 3. If a catch DOES match:` |
|        - | 11143 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11144 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11145 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11146 | ` *       finally block.` |
|        - | 11147 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11148 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11149 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11150 | ` *       in pPendingException (step 2c).` |
|        - | 11151 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11152 | ` *    d. Run finally (if present).` |
|        - | 11153 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11154 | ` *       that handlers are restored and finally has run.` |
|        - | 11155 | ` */` |
|      592 | 11156 | `static sxi32 VmThrowException(` |
|        - | 11157 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11158 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11159 | `	)` |
|        2 | 11160 |  |
|        - | 11161 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11162 | `	ph7_exception **apException;` |
|        - | 11163 | `	ph7_exception *pException;` |
|        - | 11164 | `	/* Point to the stack of loaded exceptions */` |
|      594 | 11165 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      594 | 11166 | `	pException = 0;` |
|      594 | 11167 | `	pCatch = 0;` |
|      594 | 11168 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11169 | `		ph7_exception_block *aCatch;` |
|        - | 11170 | `		ph7_class *pClass;` |
|        - | 11171 | `		SyString *aNames;` |
|        - | 11172 | `		sxu32 nNames;` |
|        - | 11173 | `		int matched;` |
|        - | 11174 | `		sxu32 j,k;` |
|        - | 11175 | `		/* Locate the appropriate block to execute */` |
|      108 | 11176 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      108 | 11177 | `		(void)SySetPop(&pVm->aException);` |
|      108 | 11178 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      110 | 11179 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 11180 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      108 | 11181 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      108 | 11182 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      108 | 11183 | `			matched = 0;` |
|      122 | 11184 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 11185 | `				/* Extract the target class */` |
|      120 | 11186 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|      120 | 11187 | `				if( pClass == 0 ){` |
|        - | 11188 | `					/* No such class */` |
|      ! 0 | 11189 | `					continue;` |
|        - | 11190 | `				}` |
|      120 | 11191 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      106 | 11192 | `					matched = 1;` |
|      106 | 11193 | `					break;` |
|        - | 11194 | `				}` |
|        8 | 11195 | `			}` |
|      108 | 11196 | `			if( matched ){` |
|        - | 11197 | `				/* Catch block found,break immediately */` |
|      106 | 11198 | `				pCatch = &aCatch[j];` |
|      106 | 11199 | `				break;` |
|        - | 11200 | `			}` |
|        2 | 11201 | `		}` |
|       53 | 11202 | `	}` |
|        - | 11203 | `	/* Execute the cached block if available */` |
|      594 | 11204 | `	if( pCatch == 0 ){` |
|        - | 11205 | `		sxi32 rc;` |
|        - | 11206 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 11207 | `		if( pException && pException->iHasFinally ){` |
|        3 | 11208 | `			pException->iFinallyDone = 1;` |
|        3 | 11209 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 11210 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11211 | `				return SXERR_ABORT;` |
|        - | 11212 | `			}` |
|        1 | 11213 | `		}` |
|        - | 11214 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 11215 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11216 | `			/* Re-throw to the outer handler */` |
|        3 | 11217 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 11218 | `		}` |
|        - | 11219 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 11220 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 11221 | `		 * exception instead of reporting it uncaught.` |
|        - | 11222 | `		 */` |
|      488 | 11223 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 11224 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 11225 | `			 * by looking for a catch frame on the stack.` |
|        - | 11226 | `			 */` |
|      488 | 11227 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 11228 | `			int inCatch = 0;` |
|      974 | 11229 | `			while( pF ){` |
|      494 | 11230 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 11231 | `					inCatch = 1;` |
|        7 | 11232 | `					break;` |
|        - | 11233 | `				}` |
|      487 | 11234 | `				pF = pF->pParent;` |
|        1 | 11235 | `			}` |
|      488 | 11236 | `			if( inCatch ){` |
|        - | 11237 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 11238 | `				pThis->iRef++;` |
|        7 | 11239 | `				pVm->pPendingException = pThis;` |
|        7 | 11240 | `				return SXRET_OK;` |
|        - | 11241 | `			}` |
|      240 | 11242 | `		}` |
|        - | 11243 | `		/* Truly uncaught */` |
|      481 | 11244 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 11245 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 11246 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 11247 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 11248 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 11249 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 11250 | `			}` |
|      ! 0 | 11251 | `		}` |
|      481 | 11252 | `		return rc;` |
|      ! 0 | 11253 | `	}else{` |
|      106 | 11254 | `		VmFrame *pFrame = pVm->pFrame;` |
|      106 | 11255 | `		ph7_exception **apSaved = 0;` |
|        - | 11256 | `		sxu32 nSavedCount;` |
|        - | 11257 | `		sxi32 rc;` |
|      106 | 11258 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      106 | 11259 | `		if( pException->pFrame == pFrame ){` |
|       82 | 11260 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       40 | 11261 | `		}` |
|        - | 11262 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 11263 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 11264 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 11265 | `		 */` |
|      106 | 11266 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      106 | 11267 | `		if( nSavedCount > 0 ){` |
|       13 | 11268 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 11269 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11270 | `			if( apSaved ){` |
|       13 | 11271 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 11272 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11273 | `				SySetReset(&pVm->aException);` |
|        4 | 11274 | `			}` |
|        4 | 11275 | `		}` |
|        - | 11276 | `		/* Create a private frame first */` |
|      106 | 11277 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      106 | 11278 | `		if( rc == SXRET_OK ){` |
|      106 | 11279 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      106 | 11280 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      106 | 11281 | `			if( pObj ){` |
|      106 | 11282 | `				pThis->iRef++;` |
|      106 | 11283 | `				pObj->x.pOther = pThis;` |
|      106 | 11284 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       52 | 11285 | `			}` |
|        - | 11286 | `			/* Execute the catch block */` |
|      106 | 11287 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 11288 | `			/* Leave the frame */` |
|      106 | 11289 | `			VmLeaveFrame(&(*pVm));` |
|       52 | 11290 | `		}` |
|        - | 11291 | `		/* Restore the outer exception handlers */` |
|      106 | 11292 | `		if( apSaved ){` |
|        - | 11293 | `			sxu32 k;` |
|        - | 11294 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 11295 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 11296 | `			 * Restore the original outer entries.` |
|        - | 11297 | `			 */` |
|        9 | 11298 | `			SySetReset(&pVm->aException);` |
|       17 | 11299 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 11300 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 11301 | `			}` |
|        9 | 11302 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 11303 | `		}` |
|        - | 11304 | `		/* Execute the finally block after catch */` |
|      106 | 11305 | `		if( pException->iHasFinally ){` |
|       16 | 11306 | `			pException->iFinallyDone = 1;` |
|        - | 11307 | `			{` |
|       16 | 11308 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 11309 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 11310 | `					return SXERR_ABORT;` |
|        - | 11311 | `				}` |
|        - | 11312 | `			}` |
|        7 | 11313 | `		}` |
|      106 | 11314 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11315 | `			return SXERR_ABORT;` |
|        - | 11316 | `		}` |
|        - | 11317 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 11318 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 11319 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 11320 | `		 */` |
|      106 | 11321 | `		if( pVm->pPendingException ){` |
|        7 | 11322 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 11323 | `			pVm->pPendingException = 0;` |
|        7 | 11324 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 11325 | `		}` |
|        - | 11326 | `	}` |
|        - | 11327 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 11328 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 11329 | `	 */` |
|      100 | 11330 | `	return SXRET_OK;` |
|      298 | 11331 |  |
|        - | 11332 | `/*` |
|        - | 11333 | ` * Section:` |
|        - | 11334 | ` *  Version,Credits and Copyright related functions.` |
|        - | 11335 | ` * Status:` |
|        - | 11336 | ` *    Stable.` |
|        - | 11337 | ` */` |
|        - | 11338 | `/*` |
|        - | 11339 | ` * string ph7version(void)` |
|        - | 11340 | ` *  Returns the running version of the PH7 version.` |
|        - | 11341 | ` * Parameters` |
|        - | 11342 | ` *  None` |
|        - | 11343 | ` * Return` |
|        - | 11344 | ` * Current PH7 version.` |
|        - | 11345 | ` */` |
|        2 | 11346 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11347 |  |
|        1 | 11348 | `	SXUNUSED(nArg);` |
|        1 | 11349 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 11350 | `	/* Current engine version */` |
|        3 | 11351 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 11352 | `	return PH7_OK;` |
|        1 | 11353 |  |
|        - | 11354 | `/*` |
|        - | 11355 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 11356 | ` */` |
|        - | 11357 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 11358 | ` "<html><head>"\` |
|        - | 11359 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 11360 | ` "<style type=\"text/css\">"\` |
|        - | 11361 | ` "div {"\` |
|        - | 11362 | `     "border: 1px solid #cccccc;"\` |
|        - | 11363 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 11364 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 11365 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 11366 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 11367 | `     "-webkit-border-radius: 10px;"\` |
|        - | 11368 | `     "-o-border-radius: 10px;"\` |
|        - | 11369 | `     "border-radius: 10px;"\` |
|        - | 11370 | `     "padding-left: 2em;"\` |
|        - | 11371 | `     "background-color: white;"\` |
|        - | 11372 | `     "margin-left: auto;"\` |
|        - | 11373 | `     "font-family: verdana;"\` |
|        - | 11374 | `     "padding-right: 2em;"\` |
|        - | 11375 | `     "margin-right: auto;"\` |
|        - | 11376 | `     "}"\` |
|        - | 11377 | `     "body {"\` |
|        - | 11378 | `     "padding: 0.2em;"\` |
|        - | 11379 | `     "font-style: normal;"\` |
|        - | 11380 | `     "font-size: medium;"\` |
|        - | 11381 | `     "background-color: #f2f2f2;"\` |
|        - | 11382 | `     "}"\` |
|        - | 11383 | `     "hr {"\` |
|        - | 11384 | `     "border-style: solid none none;"\` |
|        - | 11385 | `     "border-width: 1px medium medium;"\` |
|        - | 11386 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 11387 | `     "height: 1px;"\` |
|        - | 11388 | `     "}"\` |
|        - | 11389 | `     "a {"\` |
|        - | 11390 | `     "color: #3366cc;"\` |
|        - | 11391 | `     "text-decoration: none;"\` |
|        - | 11392 | `     "}"\` |
|        - | 11393 | `     "a:hover {"\` |
|        - | 11394 | `     "color: #999999;"\` |
|        - | 11395 | `     "}"\` |
|        - | 11396 | `     "a:active {"\` |
|        - | 11397 | `     "color: #663399;"\` |
|        - | 11398 | `     "}"\` |
|        - | 11399 | `     "h1 {"\` |
|        - | 11400 | `     "margin: 0;"\` |
|        - | 11401 | `     "padding: 0;"\` |
|        - | 11402 | `     "font-family: Verdana;"\` |
|        - | 11403 | `     "font-weight: bold;"\` |
|        - | 11404 | `     "font-style: normal;"\` |
|        - | 11405 | `     "font-size: medium;"\` |
|        - | 11406 | `     "text-transform: capitalize;"\` |
|        - | 11407 | `     "color: #0a328c;"\` |
|        - | 11408 | `     "}"\` |
|        - | 11409 | `     "p {"\` |
|        - | 11410 | `     "margin: 0 auto;"\` |
|        - | 11411 | `     "font-size: medium;"\` |
|        - | 11412 | `     "font-style: normal;"\` |
|        - | 11413 | `     "font-family: verdana;"\` |
|        - | 11414 | `     "}"\` |
|        - | 11415 | `"</style></head><body>"\` |
|        - | 11416 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 11417 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 11418 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 11419 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 11420 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 11421 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 11422 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 11423 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 11424 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 11425 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 11426 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 11427 |  |
|        - | 11428 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11429 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 11430 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 11431 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 11432 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11433 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 11434 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11435 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 11436 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11437 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 11438 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11439 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 11440 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 11441 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 11442 |  |
|        - | 11443 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 11444 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 11445 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 11446 | `"&nbsp;*<br>"\` |
|        - | 11447 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 11448 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 11449 | `"&nbsp;* are met:<br>"\` |
|        - | 11450 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 11451 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 11452 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 11453 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 11454 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 11455 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 11456 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 11457 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 11458 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 11459 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 11460 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 11461 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 11462 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 11463 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 11464 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 11465 | `"&nbsp;*<br>"\` |
|        - | 11466 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 11467 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 11468 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 11469 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 11470 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 11471 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 11472 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 11473 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 11474 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 11475 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 11476 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 11477 | `"&nbsp;*/<br>"\` |
|        - | 11478 | `"</span></small></small></p>"\` |
|        - | 11479 | `"</div></body></html>"` |
|        - | 11480 | `/*` |
|        - | 11481 | ` * bool ph7credits(void)` |
|        - | 11482 | ` * bool ph7info(void)` |
|        - | 11483 | ` * bool ph7copyright(void)` |
|        - | 11484 | ` *  Prints out the credits for PH7 engine` |
|        - | 11485 | ` * Parameters` |
|        - | 11486 | ` *  None` |
|        - | 11487 | ` * Return` |
|        - | 11488 | ` *  Always TRUE` |
|        - | 11489 | ` */` |
|        2 | 11490 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11491 |  |
|        3 | 11492 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 11493 | `	/* Expand the HTML page above*/` |
|        3 | 11494 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 11495 | `	ph7_context_output_format(` |
|        1 | 11496 | `		pCtx,` |
|        - | 11497 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 11498 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 11499 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 11500 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 11501 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 11502 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 11503 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 11504 | `#ifdef __WINNT__` |
|        - | 11505 | `		"Windows NT"` |
|        - | 11506 | `#elif defined(__UNIXES__)` |
|        - | 11507 | `		"UNIX-Like"` |
|        - | 11508 | `#else` |
|        - | 11509 | `		"Other OS"` |
|        - | 11510 | `#endif` |
|        - | 11511 | `		);` |
|        3 | 11512 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 11513 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11514 | `	SXUNUSED(apArg);` |
|        - | 11515 | `	/* Return TRUE */` |
|        - | 11516 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 11517 | `	return PH7_OK;` |
|        1 | 11518 |  |
|        - | 11519 | `/*` |
|        - | 11520 | ` * Section:` |
|        - | 11521 | ` *    URL related routines.` |
|        - | 11522 | ` * Status:` |
|        - | 11523 | ` *    Stable.` |
|        - | 11524 | ` */` |
|        - | 11525 | `/*` |
|        - | 11526 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 11527 | ` *  Parse a URL and return its fields.` |
|        - | 11528 | ` * Parameters` |
|        - | 11529 | ` *  $url` |
|        - | 11530 | ` *   The URL to parse.` |
|        - | 11531 | ` * $component` |
|        - | 11532 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 11533 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 11534 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 11535 | ` *  in which case the return value will be an integer).` |
|        - | 11536 | ` * Return` |
|        - | 11537 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 11538 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 11539 | ` *  this array are:` |
|        - | 11540 | ` *   scheme - e.g. http` |
|        - | 11541 | ` *   host` |
|        - | 11542 | ` *   port` |
|        - | 11543 | ` *   user` |
|        - | 11544 | ` *   pass` |
|        - | 11545 | ` *   path` |
|        - | 11546 | ` *   query - after the question mark ?` |
|        - | 11547 | ` *   fragment - after the hashmark #` |
|        - | 11548 | ` * Note:` |
|        - | 11549 | ` *  FALSE is returned on failure.` |
|        - | 11550 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 11551 | ` *  with the standard PHP engine.` |
|        - | 11552 | ` */` |
|       28 | 11553 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11554 |  |
|        - | 11555 | `	const char *zStr; /* Input string */` |
|        - | 11556 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 11557 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 11558 | `	int nLen;` |
|        - | 11559 | `	sxi32 rc;` |
|       29 | 11560 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11561 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11562 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11563 | `		return PH7_OK;` |
|        - | 11564 | `	}` |
|        - | 11565 | `	/* Extract the given URI */` |
|       29 | 11566 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 11567 | `	if( nLen < 1 ){` |
|        - | 11568 | `		/* Nothing to process,return FALSE */` |
|        3 | 11569 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11570 | `		return PH7_OK;` |
|        - | 11571 | `	}` |
|        - | 11572 | `	/* Get a parse */` |
|       27 | 11573 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 11574 | `	if( rc != SXRET_OK ){` |
|        - | 11575 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 11576 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11577 | `		return PH7_OK;` |
|        - | 11578 | `	}` |
|       27 | 11579 | `	if( nArg > 1 ){` |
|      ! 0 | 11580 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 11581 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 11582 | `		switch(nComponent){` |
|      ! 0 | 11583 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 11584 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 11585 | `			if( pComp->nByte < 1 ){` |
|        - | 11586 | `				/* No available value,return NULL */` |
|      ! 0 | 11587 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11588 | `			}else{` |
|      ! 0 | 11589 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11590 | `			}` |
|      ! 0 | 11591 | `			break;` |
|      ! 0 | 11592 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 11593 | `			pComp = &sURI.sHost;` |
|      ! 0 | 11594 | `			if( pComp->nByte < 1 ){` |
|        - | 11595 | `				/* No available value,return NULL */` |
|      ! 0 | 11596 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11597 | `			}else{` |
|      ! 0 | 11598 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11599 | `			}` |
|      ! 0 | 11600 | `			break;` |
|      ! 0 | 11601 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 11602 | `			pComp = &sURI.sPort;` |
|      ! 0 | 11603 | `			if( pComp->nByte < 1 ){` |
|        - | 11604 | `				/* No available value,return NULL */` |
|      ! 0 | 11605 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11606 | `			}else{` |
|      ! 0 | 11607 | `				int iPort = 0;` |
|        - | 11608 | `				/* Cast the value to integer */` |
|      ! 0 | 11609 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 11610 | `				ph7_result_int(pCtx,iPort);` |
|        - | 11611 | `			}` |
|      ! 0 | 11612 | `			break;` |
|      ! 0 | 11613 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 11614 | `			pComp = &sURI.sUser;` |
|      ! 0 | 11615 | `			if( pComp->nByte < 1 ){` |
|        - | 11616 | `				/* No available value,return NULL */` |
|      ! 0 | 11617 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11618 | `			}else{` |
|      ! 0 | 11619 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11620 | `			}` |
|      ! 0 | 11621 | `			break;` |
|      ! 0 | 11622 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11623 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11624 | `			if( pComp->nByte < 1 ){` |
|        - | 11625 | `				/* No available value,return NULL */` |
|      ! 0 | 11626 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11627 | `			}else{` |
|      ! 0 | 11628 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11629 | `			}` |
|      ! 0 | 11630 | `			break;` |
|      ! 0 | 11631 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11632 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11633 | `			if( pComp->nByte < 1 ){` |
|        - | 11634 | `				/* No available value,return NULL */` |
|      ! 0 | 11635 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11636 | `			}else{` |
|      ! 0 | 11637 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11638 | `			}` |
|      ! 0 | 11639 | `			break;` |
|      ! 0 | 11640 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11641 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11642 | `			if( pComp->nByte < 1 ){` |
|        - | 11643 | `				/* No available value,return NULL */` |
|      ! 0 | 11644 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11645 | `			}else{` |
|      ! 0 | 11646 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11647 | `			}` |
|      ! 0 | 11648 | `			break;` |
|      ! 0 | 11649 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11650 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11651 | `			if( pComp->nByte < 1 ){` |
|        - | 11652 | `				/* No available value,return NULL */` |
|      ! 0 | 11653 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11654 | `			}else{` |
|      ! 0 | 11655 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11656 | `			}` |
|      ! 0 | 11657 | `			break;` |
|      ! 0 | 11658 | `		default:` |
|        - | 11659 | `			/* No such entry,return NULL */` |
|      ! 0 | 11660 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11661 | `			break;` |
|        - | 11662 | `		}` |
|      ! 0 | 11663 | `	}else{` |
|        - | 11664 | `		ph7_value *pArray,*pValue;` |
|        - | 11665 | `		/* Return an associative array */` |
|       27 | 11666 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11667 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11668 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11669 | `			/* Out of memory */` |
|      ! 0 | 11670 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11671 | `			/* Return false */` |
|      ! 0 | 11672 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11673 | `			return PH7_OK;` |
|        - | 11674 | `		}` |
|        - | 11675 | `		/* Fill the array */` |
|       27 | 11676 | `		pComp = &sURI.sScheme;` |
|       27 | 11677 | `		if( pComp->nByte > 0 ){` |
|       19 | 11678 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11679 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11680 | `		}` |
|        - | 11681 | `		/* Reset the string cursor */` |
|       27 | 11682 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11683 | `		pComp = &sURI.sHost;` |
|       27 | 11684 | `		if( pComp->nByte > 0 ){` |
|       25 | 11685 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11686 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11687 | `		}` |
|        - | 11688 | `		/* Reset the string cursor */` |
|       27 | 11689 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11690 | `		pComp = &sURI.sPort;` |
|       27 | 11691 | `		if( pComp->nByte > 0 ){` |
|       11 | 11692 | `			int iPort = 0;/* cc warning */` |
|        - | 11693 | `			/* Convert to integer */` |
|       11 | 11694 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11695 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11696 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11697 | `		}` |
|        - | 11698 | `		/* Reset the string cursor */` |
|       27 | 11699 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11700 | `		pComp = &sURI.sUser;` |
|       27 | 11701 | `		if( pComp->nByte > 0 ){` |
|        7 | 11702 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11703 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11704 | `		}` |
|        - | 11705 | `		/* Reset the string cursor */` |
|       27 | 11706 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11707 | `		pComp = &sURI.sPass;` |
|       27 | 11708 | `		if( pComp->nByte > 0 ){` |
|        7 | 11709 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11710 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11711 | `		}` |
|        - | 11712 | `		/* Reset the string cursor */` |
|       27 | 11713 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11714 | `		pComp = &sURI.sPath;` |
|       27 | 11715 | `		if( pComp->nByte > 0 ){` |
|       17 | 11716 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11717 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11718 | `		}` |
|        - | 11719 | `		/* Reset the string cursor */` |
|       27 | 11720 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11721 | `		pComp = &sURI.sQuery;` |
|       27 | 11722 | `		if( pComp->nByte > 0 ){` |
|        5 | 11723 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11724 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11725 | `		}` |
|        - | 11726 | `		/* Reset the string cursor */` |
|       27 | 11727 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11728 | `		pComp = &sURI.sFragment;` |
|       27 | 11729 | `		if( pComp->nByte > 0 ){` |
|        5 | 11730 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11731 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11732 | `		}` |
|        - | 11733 | `		/* Return the created array */` |
|       27 | 11734 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11735 | `		/* NOTE:` |
|        - | 11736 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11737 | `		 * automatically as soon we return from this function.` |
|        - | 11738 | `		 */` |
|        - | 11739 | `	}` |
|        - | 11740 | `	/* All done */` |
|       27 | 11741 | `	return PH7_OK;` |
|       15 | 11742 |  |
|        - | 11743 | `/*` |
|        - | 11744 | ` * Section:` |
|        - | 11745 | ` *   Array related routines.` |
|        - | 11746 | ` * Status:` |
|        - | 11747 | ` *    Stable.` |
|        - | 11748 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11749 | ` *  Array related functions that need access to the underlying` |
|        - | 11750 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11751 | ` */` |
|        - | 11752 | `/*` |
|        - | 11753 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11754 | ` * of the following structure.` |
|        - | 11755 | ` */` |
|        - | 11756 | `struct compact_data` |
|        - | 11757 |  |
|        - | 11758 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11759 | `	int nRecCount;      /* Recursion count */` |
|        - | 11760 | `};` |
|        - | 11761 | `/*` |
|        - | 11762 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11763 | ` */` |
|      ! 0 | 11764 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11765 |  |
|      ! 0 | 11766 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11767 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11768 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11769 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11770 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11771 | `		SyString sVar;` |
|      ! 0 | 11772 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11773 | `		if( sVar.nByte > 0 ){` |
|        - | 11774 | `			/* Query the current frame */` |
|      ! 0 | 11775 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11776 | `			/* ^` |
|        - | 11777 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11778 | `			 */` |
|      ! 0 | 11779 | `			if( pKey ){` |
|        - | 11780 | `				/* Perform the insertion */` |
|      ! 0 | 11781 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11782 | `			}` |
|      ! 0 | 11783 | `		}` |
|      ! 0 | 11784 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11785 | `		int rc;` |
|        - | 11786 | `		/* Recursively traverse this array */` |
|      ! 0 | 11787 | `		pData->nRecCount++;` |
|      ! 0 | 11788 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11789 | `		pData->nRecCount--;` |
|      ! 0 | 11790 | `		return rc;` |
|        - | 11791 | `	}` |
|      ! 0 | 11792 | `	return SXRET_OK;` |
|      ! 0 | 11793 |  |
|        - | 11794 | `/*` |
|        - | 11795 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11796 | ` *  Create array containing variables and their values.` |
|        - | 11797 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11798 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11799 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11800 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11801 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11802 | ` * Parameters` |
|        - | 11803 | ` *  $varname` |
|        - | 11804 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11805 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11806 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11807 | ` *   it recursively.` |
|        - | 11808 | ` * Return` |
|        - | 11809 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11810 | ` */` |
|        2 | 11811 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11812 |  |
|        - | 11813 | `	ph7_value *pArray,*pObj;` |
|        3 | 11814 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11815 | `	const char *zName;` |
|        - | 11816 | `	SyString sVar;` |
|        - | 11817 | `	int i,nLen;` |
|        3 | 11818 | `	if( nArg < 1 ){` |
|        - | 11819 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11820 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11821 | `		return PH7_OK;` |
|        - | 11822 | `	}` |
|        - | 11823 | `	/* Create the array */` |
|        3 | 11824 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11825 | `	if( pArray == 0 ){` |
|        - | 11826 | `		/* Out of memory */` |
|      ! 0 | 11827 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11828 | `		/* Return NULL */` |
|      ! 0 | 11829 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11830 | `		return PH7_OK;` |
|        - | 11831 | `	}` |
|        - | 11832 | `	/* Perform the requested operation */` |
|        7 | 11833 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11834 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11835 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11836 | `				struct compact_data sData;` |
|      ! 0 | 11837 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11838 | `				/* Recursively walk the array */` |
|      ! 0 | 11839 | `				sData.nRecCount = 0;` |
|      ! 0 | 11840 | `				sData.pArray = pArray;` |
|      ! 0 | 11841 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11842 | `			}` |
|      ! 0 | 11843 | `		}else{` |
|        - | 11844 | `			/* Extract variable name */` |
|        5 | 11845 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11846 | `			if( nLen > 0 ){` |
|        5 | 11847 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11848 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11849 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11850 | `				if( pObj ){` |
|        5 | 11851 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11852 | `				}` |
|        2 | 11853 | `			}` |
|        - | 11854 | `		}` |
|        3 | 11855 | `	}` |
|        - | 11856 | `	/* Return the array */` |
|        3 | 11857 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11858 | `	return PH7_OK;` |
|        2 | 11859 |  |
|        - | 11860 | `/*` |
|        - | 11861 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11862 | ` * of the following structure.` |
|        - | 11863 | ` */` |
|        - | 11864 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11865 | `struct extract_aux_data` |
|        - | 11866 |  |
|        - | 11867 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11868 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11869 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11870 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11871 | `	int iFlags;           /* Control flags */` |
|        - | 11872 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11873 | `};` |
|        - | 11874 | `/* Forward declaration */` |
|        - | 11875 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11876 | `/*` |
|        - | 11877 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11878 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11879 | ` * Parameters` |
|        - | 11880 | ` * $var_array` |
|        - | 11881 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11882 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11883 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11884 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11885 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11886 | ` * $extract_type` |
|        - | 11887 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11888 | ` *  It can be one of the following values:` |
|        - | 11889 | ` *   EXTR_OVERWRITE` |
|        - | 11890 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11891 | ` *   EXTR_SKIP` |
|        - | 11892 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11893 | ` *   EXTR_PREFIX_SAME` |
|        - | 11894 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11895 | ` *   EXTR_PREFIX_ALL` |
|        - | 11896 | ` *       Prefix all variable names with prefix.` |
|        - | 11897 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11898 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11899 | ` *   EXTR_IF_EXISTS` |
|        - | 11900 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11901 | ` *       otherwise do nothing.` |
|        - | 11902 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11903 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11904 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11905 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11906 | ` *      the current symbol table.` |
|        - | 11907 | ` * $prefix` |
|        - | 11908 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11909 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11910 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11911 | ` *  underscore character.` |
|        - | 11912 | ` * Return` |
|        - | 11913 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11914 | ` */` |
|        4 | 11915 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11916 |  |
|        - | 11917 | `	extract_aux_data sAux;` |
|        - | 11918 | `	ph7_hashmap *pMap;` |
|        5 | 11919 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11920 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11921 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11922 | `		return PH7_OK;` |
|        - | 11923 | `	}` |
|        - | 11924 | `	/* Point to the target hashmap */` |
|        5 | 11925 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11926 | `	if( pMap->nEntry < 1 ){` |
|        - | 11927 | `		/* Empty map,return  0 */` |
|      ! 0 | 11928 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11929 | `		return PH7_OK;` |
|        - | 11930 | `	}` |
|        - | 11931 | `	/* Prepare the aux data */` |
|        5 | 11932 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11933 | `	if( nArg > 1 ){` |
|        3 | 11934 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11935 | `		if( nArg > 2 ){` |
|      ! 0 | 11936 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11937 | `		}` |
|        1 | 11938 | `	}` |
|        5 | 11939 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11940 | `	/* Invoke the worker callback */` |
|        5 | 11941 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11942 | `	/* Number of variables successfully imported */` |
|        5 | 11943 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11944 | `	return PH7_OK;` |
|        3 | 11945 |  |
|        - | 11946 | `/*` |
|        - | 11947 | ` * Worker callback for the [extract()] function defined` |
|        - | 11948 | ` * below.` |
|        - | 11949 | ` */` |
|        8 | 11950 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11951 |  |
|        9 | 11952 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11953 | `	int iFlags = pAux->iFlags;` |
|        9 | 11954 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11955 | `	ph7_value *pObj;` |
|        - | 11956 | `	SyString sVar;` |
|        9 | 11957 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11958 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11959 | `	}` |
|        - | 11960 | `	/* Perform a string cast */` |
|        9 | 11961 | `	PH7_MemObjToString(pKey);` |
|        9 | 11962 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11963 | `		/* Unavailable variable name */` |
|      ! 0 | 11964 | `		return SXRET_OK;` |
|        - | 11965 | `	}` |
|        9 | 11966 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11967 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11968 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11969 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11970 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11971 | `			);` |
|      ! 0 | 11972 | `	}else{` |
|       13 | 11973 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11974 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11975 | `	}` |
|        9 | 11976 | `	sVar.zString = pAux->zWorker;` |
|        - | 11977 | `	/* Try to extract the variable */` |
|        9 | 11978 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11979 | `	if( pObj ){` |
|        - | 11980 | `		/* Collision */` |
|        5 | 11981 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11982 | `			return SXRET_OK;` |
|        - | 11983 | `		}` |
|        5 | 11984 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11985 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11986 | `				/* Already prefixed */` |
|      ! 0 | 11987 | `				return SXRET_OK;` |
|        - | 11988 | `			}` |
|      ! 0 | 11989 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11990 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11991 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11992 | `				);` |
|      ! 0 | 11993 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11994 | `		}` |
|        3 | 11995 | `	}else{` |
|        - | 11996 | `		/* Create the variable */` |
|        5 | 11997 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11998 | `	}` |
|        9 | 11999 | `	if( pObj ){` |
|        - | 12000 | `		/* Overwrite the old value */` |
|        9 | 12001 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12002 | `		/* Increment counter */` |
|        9 | 12003 | `		pAux->iCount++;` |
|        4 | 12004 | `	}` |
|        9 | 12005 | `	return SXRET_OK;` |
|        5 | 12006 |  |
|        - | 12007 | `/*` |
|        - | 12008 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12009 | ` * defined below.` |
|        - | 12010 | ` */` |
|        2 | 12011 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12012 |  |
|        3 | 12013 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12014 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12015 | `	ph7_value *pObj;` |
|        - | 12016 | `	SyString sVar;` |
|        - | 12017 | `	/* Perform a string cast */` |
|        3 | 12018 | `	PH7_MemObjToString(pKey);` |
|        3 | 12019 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12020 | `		/* Unavailable variable name */` |
|      ! 0 | 12021 | `		return SXRET_OK;` |
|        - | 12022 | `	}` |
|        3 | 12023 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12024 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12025 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12026 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12027 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12028 | `			);` |
|        2 | 12029 | `	}else{` |
|      ! 0 | 12030 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12031 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12032 | `	}` |
|        3 | 12033 | `	sVar.zString = pAux->zWorker;` |
|        - | 12034 | `	/* Extract the variable */` |
|        3 | 12035 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12036 | `	if( pObj ){` |
|        3 | 12037 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12038 | `	}` |
|        3 | 12039 | `	return SXRET_OK;` |
|        2 | 12040 |  |
|        - | 12041 | `/*` |
|        - | 12042 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12043 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12044 | ` * Parameters` |
|        - | 12045 | ` * $types` |
|        - | 12046 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12047 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12048 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12049 | ` *  POST includes the POST uploaded file information.` |
|        - | 12050 | ` *  Note:` |
|        - | 12051 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12052 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12053 | ` * $prefix` |
|        - | 12054 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12055 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12056 | ` *  variable named $pref_userid.` |
|        - | 12057 | ` * Return` |
|        - | 12058 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12059 | ` */` |
|        2 | 12060 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12061 |  |
|        - | 12062 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12063 | `	extract_aux_data sAux;` |
|        - | 12064 | `	int nLen,nPrefixLen;` |
|        - | 12065 | `	ph7_value *pSuper;` |
|        - | 12066 | `	ph7_vm *pVm;` |
|        - | 12067 | `	/* By default import only $_GET variables  */` |
|        3 | 12068 | `	zImport = "G";` |
|        3 | 12069 | `	nLen = (int)sizeof(char);` |
|        3 | 12070 | `	zPrefix = 0;` |
|        3 | 12071 | `	nPrefixLen = 0;` |
|        3 | 12072 | `	if( nArg > 0 ){` |
|        3 | 12073 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12074 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12075 | `		}` |
|        3 | 12076 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12077 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12078 | `		}` |
|        1 | 12079 | `	}` |
|        - | 12080 | `	/* Point to the underlying VM */` |
|        3 | 12081 | `	pVm = pCtx->pVm;` |
|        - | 12082 | `	/* Initialize the aux data */` |
|        3 | 12083 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12084 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12085 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12086 | `	sAux.pVm = pVm;` |
|        - | 12087 | `	/* Extract */` |
|        3 | 12088 | `	zEnd = &zImport[nLen];` |
|        5 | 12089 | `	while( zImport < zEnd ){` |
|        3 | 12090 | `		int c = zImport[0];` |
|        3 | 12091 | `		pSuper = 0;` |
|        3 | 12092 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12093 | `			/* Import $_GET variables */` |
|        3 | 12094 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12095 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12096 | `			/* Import $_POST variables */` |
|      ! 0 | 12097 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12098 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12099 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12100 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12101 | `		}` |
|        3 | 12102 | `		if( pSuper ){` |
|        - | 12103 | `			/* Iterate throw array entries */` |
|        3 | 12104 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12105 | `		}` |
|        - | 12106 | `		/* Advance the cursor */` |
|        3 | 12107 | `		zImport++;` |
|        1 | 12108 | `	}` |
|        - | 12109 | `	/* All done,return TRUE*/` |
|        3 | 12110 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12111 | `	return PH7_OK;` |
|        1 | 12112 |  |
|        - | 12113 | `/*` |
|        - | 12114 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12115 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12116 | ` * information.` |
|        - | 12117 | ` */` |
|    11268 | 12118 | `static sxi32 VmEvalChunk(` |
|        - | 12119 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12120 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12121 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12122 | `	int iFlags,         /* Compile flag */` |
|        - | 12123 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12124 | `	)` |
|        2 | 12125 |  |
|        - | 12126 | `	SySet *pByteCode,aByteCode;` |
|        - | 12127 | `	SyBlob sSavedNs;` |
|    11270 | 12128 | `	ProcConsumer xErr = 0;` |
|    11270 | 12129 | `	void *pErrData = 0;` |
|        - | 12130 | `	/* Initialize bytecode container */` |
|    11270 | 12131 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11270 | 12132 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12133 | `	/* Reset the code generator */` |
|    11270 | 12134 | `	if( bTrueReturn ){` |
|        - | 12135 | `		/* Included file,log compile-time errors */` |
|     8502 | 12136 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8502 | 12137 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4250 | 12138 | `	}` |
|    11270 | 12139 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12140 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12141 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12142 | `	 * the caller's namespace is restored. */` |
|    11270 | 12143 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11270 | 12144 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11270 | 12145 | `	if( bTrueReturn ){` |
|        - | 12146 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8502 | 12147 | `		SyBlobReset(&pVm->sNamespace);` |
|     4250 | 12148 | `	}` |
|        - | 12149 | `	/* Swap bytecode container */` |
|    11270 | 12150 | `	pByteCode = pVm->pByteContainer;` |
|    11270 | 12151 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12152 | `	/* Compile the chunk */` |
|    11270 | 12153 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16904 | 12154 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12155 | `		/* Compilation error,return false */` |
|        3 | 12156 | `		if( pCtx ){` |
|        3 | 12157 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12158 | `		}` |
|        2 | 12159 | `	}else{` |
|        - | 12160 | `		/* Mount any newly defined classes */` |
|        - | 12161 | `		SyHashEntry *pEntry;` |
|        - | 12162 | `		ph7_class *pClass;` |
|        - | 12163 | `		ph7_value sResult; /* Return value */` |
|        - | 12164 | `		sxi32 rc;` |
|    11268 | 12165 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   449471 | 12166 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   432572 | 12167 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 12168 | `			/* Only mount classes that haven't been mounted yet */` |
|   432572 | 12169 | `			if( !pClass->bMounted ){` |
|    88202 | 12170 | `				rc = VmMountUserClass(pVm,pClass);` |
|    88202 | 12171 | `				if( rc != SXRET_OK ){` |
|        - | 12172 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 12173 | `					if( pCtx ){` |
|      ! 0 | 12174 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 12175 | `					}` |
|      ! 0 | 12176 | `					goto Cleanup;` |
|        - | 12177 | `				}` |
|    44100 | 12178 | `			}` |
|        2 | 12179 | `		}` |
|    11268 | 12180 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 12181 | `			/* Out of memory */` |
|      ! 0 | 12182 | `			if( pCtx ){` |
|      ! 0 | 12183 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 12184 | `			}` |
|      ! 0 | 12185 | `			goto Cleanup;` |
|        - | 12186 | `		}` |
|    11268 | 12187 | `		if( bTrueReturn ){` |
|        - | 12188 | `			/* Assume a boolean true return value */` |
|     8502 | 12189 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4252 | 12190 | `		}else{` |
|        - | 12191 | `			/* Assume a null return value */` |
|     2768 | 12192 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 12193 | `		}` |
|        - | 12194 | `		/* Execute the compiled chunk */` |
|    11268 | 12195 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11268 | 12196 | `		if( pCtx ){` |
|        - | 12197 | `			/* Set the execution result */` |
|     8520 | 12198 | `			ph7_result_value(pCtx,&sResult);` |
|     4259 | 12199 | `		}` |
|    11268 | 12200 | `		PH7_MemObjRelease(&sResult);` |
|        - | 12201 | `	}` |
|     5634 | 12202 | `Cleanup:` |
|        - | 12203 | `	/* Cleanup the mess left behind */` |
|    11270 | 12204 | `	pVm->pByteContainer = pByteCode;` |
|    11270 | 12205 | `	SySetRelease(&aByteCode);` |
|        - | 12206 | `	/* Restore caller's namespace state */` |
|    11270 | 12207 | `	SyBlobReset(&pVm->sNamespace);` |
|    11270 | 12208 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11270 | 12209 | `	SyBlobRelease(&sSavedNs);` |
|    11270 | 12210 | `	return SXRET_OK;` |
|        2 | 12211 |  |
|        - | 12212 | `/*` |
|        - | 12213 | ` * value eval(string $code)` |
|        - | 12214 | ` *   Evaluate a string as PHP code.` |
|        - | 12215 | ` * Parameter` |
|        - | 12216 | ` *  code: PHP code to evaluate.` |
|        - | 12217 | ` * Return` |
|        - | 12218 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 12219 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 12220 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 12221 | ` */` |
|       22 | 12222 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12223 |  |
|        - | 12224 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 12225 | `	if( nArg < 1 ){` |
|        - | 12226 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12227 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12228 | `		return SXRET_OK;` |
|        - | 12229 | `	}` |
|        - | 12230 | `	/* Chunk to evaluate */` |
|       24 | 12231 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 12232 | `	if( sChunk.nByte < 1 ){` |
|        - | 12233 | `		/* Empty string,return NULL */` |
|        3 | 12234 | `		ph7_result_null(pCtx);` |
|        3 | 12235 | `		return SXRET_OK;` |
|        - | 12236 | `	}` |
|        - | 12237 | `	/* Eval the chunk */` |
|       22 | 12238 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 12239 | `	return SXRET_OK;` |
|       13 | 12240 |  |
|        - | 12241 | `/*` |
|        - | 12242 | ` * Check if a file path is already included.` |
|        - | 12243 | ` */` |
|    16996 | 12244 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 12245 |  |
|        - | 12246 | `	SyString *aEntries;` |
|        - | 12247 | `	sxu32 n;` |
|    16998 | 12248 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 12249 | `	/* Perform a linear search */` |
| 72167974 | 12250 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 72150984 | 12251 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 12252 | `			/* Already included */` |
|        7 | 12253 | `			return TRUE;` |
|        - | 12254 | `		}` |
| 36075490 | 12255 | `	}` |
|    16992 | 12256 | `	return FALSE;` |
|     8500 | 12257 |  |
|        - | 12258 | `/*` |
|        - | 12259 | ` * Push a file path in the appropriate VM container.` |
|        - | 12260 | ` */` |
|    19736 | 12261 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 12262 |  |
|        - | 12263 | `	SyString sPath;` |
|        - | 12264 | `	char *zDup;` |
|        - | 12265 | `#ifdef __WINNT__` |
|        - | 12266 | `	char *zCur;` |
|        - | 12267 | `#endif` |
|        - | 12268 | `	sxi32 rc;` |
|    19738 | 12269 | `	if( nLen < 0 ){` |
|     2742 | 12270 | `		nLen = SyStrlen(zPath);` |
|     1370 | 12271 | `	}` |
|        - | 12272 | `	/* Duplicate the file path first */` |
|    19738 | 12273 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    19738 | 12274 | `	if( zDup == 0 ){` |
|      ! 0 | 12275 | `		return SXERR_MEM;` |
|        - | 12276 | `	}` |
|        - | 12277 | `#ifdef __WINNT__` |
|        - | 12278 | `	/* Normalize path on windows` |
|        - | 12279 | `	 * Example:` |
|        - | 12280 | `	 *    Path/To/File.php` |
|        - | 12281 | `	 * becomes` |
|        - | 12282 | `	 *   path\to\file.php` |
|        - | 12283 | `	 */` |
|        2 | 12284 | `	zCur = zDup;` |
|        2 | 12285 | `	while( zCur[0] != 0 ){` |
|        2 | 12286 | `		if( zCur[0] == '/' ){` |
|        2 | 12287 | `			zCur[0] = '\\';` |
|        2 | 12288 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 12289 | `			int c = SyToLower(zCur[0]);` |
|        1 | 12290 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 12291 | `		}` |
|        2 | 12292 | `		zCur++;` |
|        2 | 12293 | `	}` |
|        - | 12294 | `#endif` |
|        - | 12295 | `	/* Install the file path */` |
|    19738 | 12296 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    19738 | 12297 | `	if( !bMain ){` |
|    16998 | 12298 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 12299 | `			/* Already included */` |
|        7 | 12300 | `			*pNew = 0;` |
|        4 | 12301 | `		}else{` |
|        - | 12302 | `			/* Insert in the corresponding container */` |
|    16992 | 12303 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16992 | 12304 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12305 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 12306 | `				return rc;` |
|        - | 12307 | `			}` |
|    16992 | 12308 | `			*pNew = 1;` |
|        - | 12309 | `		}` |
|     8498 | 12310 | `	}` |
|    19738 | 12311 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    19738 | 12312 | `	return SXRET_OK;` |
|     9870 | 12313 |  |
|        - | 12314 | `/*` |
|        - | 12315 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 12316 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 12317 | ` * indicates failure.` |
|        - | 12318 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 12319 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 12320 | ` * operations.` |
|        - | 12321 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 12322 | ` * this function is a no-op.` |
|        - | 12323 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 12324 | ` * constructs for more information.` |
|        - | 12325 | ` */` |
|     8510 | 12326 | `static sxi32 VmExecIncludedFile(` |
|        - | 12327 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 12328 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 12329 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 12330 | `	 )` |
|        2 | 12331 |  |
|        - | 12332 | `	sxi32 rc;` |
|        - | 12333 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12334 | `	const ph7_io_stream *pStream;` |
|        - | 12335 | `	SyBlob sContents;` |
|        - | 12336 | `	void *pHandle;` |
|        - | 12337 | `	ph7_vm *pVm;` |
|        - | 12338 | `	int isNew;` |
|        - | 12339 | `	/* Initialize fields */` |
|     8512 | 12340 | `	pVm = pCtx->pVm;` |
|     8512 | 12341 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8512 | 12342 | `	isNew = 0;` |
|        - | 12343 | `	/* Extract the associated stream */` |
|     8512 | 12344 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 12345 | `	/*` |
|        - | 12346 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 12347 | `	 * in a read-only mode.` |
|        - | 12348 | `	 */` |
|     8512 | 12349 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8512 | 12350 | `	if( pHandle == 0 ){` |
|        8 | 12351 | `		return SXERR_IO;` |
|        - | 12352 | `	}` |
|     8506 | 12353 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8506 | 12354 | `	if( IncludeOnce && !isNew ){` |
|        - | 12355 | `		/* Already included */` |
|        5 | 12356 | `		rc = SXERR_EXISTS;` |
|        3 | 12357 | `	}else{` |
|        - | 12358 | `		/* Read the whole file contents */` |
|     8502 | 12359 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8502 | 12360 | `		if( rc == SXRET_OK ){` |
|        - | 12361 | `			SyString sScript;` |
|        - | 12362 | `			/* Compile and execute the script */` |
|     8502 | 12363 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8502 | 12364 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4250 | 12365 | `		}` |
|        - | 12366 | `	}` |
|        - | 12367 | `	/* Pop from the set of included file */` |
|     8506 | 12368 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 12369 | `	/* Close the handle */` |
|     8506 | 12370 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 12371 | `	/* Release the working buffer */` |
|     8506 | 12372 | `	SyBlobRelease(&sContents);` |
|        - | 12373 | `#else` |
|        - | 12374 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12375 | `	SXUNUSED(pPath);` |
|        - | 12376 | `	SXUNUSED(IncludeOnce);` |
|        - | 12377 | `	rc = SXERR_IO;` |
|        - | 12378 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8506 | 12379 | `	return rc;` |
|     4257 | 12380 |  |
|        - | 12381 | `/*` |
|        - | 12382 | ` * string get_include_path(void)` |
|        - | 12383 | ` *  Gets the current include_path configuration option.` |
|        - | 12384 | ` * Parameter` |
|        - | 12385 | ` *  None` |
|        - | 12386 | ` * Return` |
|        - | 12387 | ` *  Included paths as a string` |
|        - | 12388 | ` */` |
|        2 | 12389 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12390 |  |
|        3 | 12391 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12392 | `	SyString *aEntry;` |
|        - | 12393 | `	int dir_sep;` |
|        - | 12394 | `	sxu32 n;` |
|        - | 12395 | `#ifdef __WINNT__` |
|        1 | 12396 | `	dir_sep = ';';` |
|        - | 12397 | `#else` |
|        - | 12398 | `	/* Assume UNIX path separator */` |
|        2 | 12399 | `	dir_sep = ':';` |
|        - | 12400 | `#endif` |
|        1 | 12401 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12402 | `	SXUNUSED(apArg);` |
|        - | 12403 | `	/* Point to the list of import paths */` |
|        3 | 12404 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 12405 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 12406 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 12407 | `		if( n > 0 ){` |
|        - | 12408 | `			/* Append dir seprator */` |
|      ! 0 | 12409 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 12410 | `		}` |
|        - | 12411 | `		/* Append path */` |
|        3 | 12412 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 12413 | `	}` |
|        3 | 12414 | `	return PH7_OK;` |
|        1 | 12415 |  |
|        - | 12416 | `/*` |
|        - | 12417 | ` * string get_get_included_files(void)` |
|        - | 12418 | ` *  Gets the current include_path configuration option.` |
|        - | 12419 | ` * Parameter` |
|        - | 12420 | ` *  None` |
|        - | 12421 | ` * Return` |
|        - | 12422 | ` *  Included paths as a string` |
|        - | 12423 | ` */` |
|        2 | 12424 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12425 |  |
|        3 | 12426 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 12427 | `	ph7_value *pArray,*pWorker;` |
|        - | 12428 | `	SyString *pEntry;` |
|        - | 12429 | `	int c,d;` |
|        - | 12430 | `	/* Create an array and a working value */` |
|        3 | 12431 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 12432 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 12433 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 12434 | `		/* Out of memory,return null */` |
|      ! 0 | 12435 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12436 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12437 | `		SXUNUSED(apArg);` |
|      ! 0 | 12438 | `		return PH7_OK;` |
|        - | 12439 | `	}` |
|        3 | 12440 | `	c = d = '/';` |
|        - | 12441 | `#ifdef __WINNT__` |
|        1 | 12442 | `	d = '\\';` |
|        - | 12443 | `#endif` |
|        - | 12444 | `	/* Iterate throw entries */` |
|        3 | 12445 | `	SySetResetCursor(pFiles);` |
|     3839 | 12446 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 12447 | `		const char *zBase,*zEnd;` |
|        - | 12448 | `		int iLen;` |
|        - | 12449 | `		/* reset the string cursor */` |
|     3837 | 12450 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 12451 | `		/* Extract base name */` |
|     3837 | 12452 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 12453 | `		/* Ignore trailing '/' */` |
|     5755 | 12454 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 12455 | `			zEnd--;` |
|      ! 0 | 12456 | `		}` |
|     3837 | 12457 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 12458 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 12459 | `			zEnd--;` |
|        1 | 12460 | `		}` |
|     3837 | 12461 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 12462 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 12463 | `		/* Copy entry name */` |
|     3837 | 12464 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 12465 | `		/* Perform the insertion */` |
|     3837 | 12466 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 12467 | `	}` |
|        - | 12468 | `	/* All done,return the created array */` |
|        3 | 12469 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12470 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 12471 | `	 * by the engine as soon we return from this foreign` |
|        - | 12472 | `	 * function.` |
|        - | 12473 | `	 */` |
|        3 | 12474 | `	return PH7_OK;` |
|        2 | 12475 |  |
|        - | 12476 | `/*` |
|        - | 12477 | ` * include:` |
|        - | 12478 | ` * According to the PHP reference manual.` |
|        - | 12479 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 12480 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 12481 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 12482 | ` *  include() will finally check in the calling script's own directory` |
|        - | 12483 | ` *  and the current working directory before failing. The include()` |
|        - | 12484 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 12485 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 12486 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 12487 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 12488 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 12489 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 12490 | ` *  directory to find the requested file.` |
|        - | 12491 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 12492 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 12493 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 12494 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 12495 | ` */` |
|     8492 | 12496 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12497 |  |
|        - | 12498 | `	SyString sFile;` |
|        - | 12499 | `	sxi32 rc;` |
|     8494 | 12500 | `	if( nArg < 1 ){` |
|        - | 12501 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12502 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12503 | `		return SXRET_OK;` |
|        - | 12504 | `	}` |
|        - | 12505 | `	/* File to include */` |
|     8494 | 12506 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8494 | 12507 | `	if( sFile.nByte < 1 ){` |
|        - | 12508 | `		/* Empty string,return NULL */` |
|      ! 0 | 12509 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12510 | `		return SXRET_OK;` |
|        - | 12511 | `	}` |
|        - | 12512 | `	/* Open,compile and execute the desired script */` |
|     8494 | 12513 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8494 | 12514 | `	if( rc != SXRET_OK ){` |
|        - | 12515 | `		/* Emit a warning and return false */` |
|        3 | 12516 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 12517 | `		ph7_result_bool(pCtx,0);` |
|        1 | 12518 | `	}` |
|     8494 | 12519 | `	return SXRET_OK;` |
|     4248 | 12520 |  |
|        - | 12521 | `/*` |
|        - | 12522 | ` * include_once:` |
|        - | 12523 | ` *  According to the PHP reference manual.` |
|        - | 12524 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 12525 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 12526 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 12527 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 12528 | ` *   just once.` |
|        - | 12529 | ` */` |
|        4 | 12530 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12531 |  |
|        - | 12532 | `	SyString sFile;` |
|        - | 12533 | `	sxi32 rc;` |
|        5 | 12534 | `	if( nArg < 1 ){` |
|        - | 12535 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12536 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12537 | `		return SXRET_OK;` |
|        - | 12538 | `	}` |
|        - | 12539 | `	/* File to include */` |
|        5 | 12540 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12541 | `	if( sFile.nByte < 1 ){` |
|        - | 12542 | `		/* Empty string,return NULL */` |
|      ! 0 | 12543 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12544 | `		return SXRET_OK;` |
|        - | 12545 | `	}` |
|        - | 12546 | `	/* Open,compile and execute the desired script */` |
|        5 | 12547 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12548 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12549 | `		/* File already included,return TRUE */` |
|        3 | 12550 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12551 | `		return SXRET_OK;` |
|        - | 12552 | `	}` |
|        3 | 12553 | `	if( rc != SXRET_OK ){` |
|        - | 12554 | `		/* Emit a warning and return false */` |
|      ! 0 | 12555 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12556 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12557 | ` 	}` |
|        3 | 12558 | `	return SXRET_OK;` |
|        3 | 12559 |  |
|        - | 12560 | `/*` |
|        - | 12561 | ` * require.` |
|        - | 12562 | ` *  According to the PHP reference manual.` |
|        - | 12563 | ` *   require() is identical to include() except upon failure it will` |
|        - | 12564 | ` *   also produce a fatal level error.` |
|        - | 12565 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 12566 | ` *   emits a warning  which allows the script to continue.` |
|        - | 12567 | ` */` |
|        6 | 12568 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12569 |  |
|        - | 12570 | `	SyString sFile;` |
|        - | 12571 | `	sxi32 rc;` |
|        8 | 12572 | `	if( nArg < 1 ){` |
|        - | 12573 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12574 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12575 | `		return SXRET_OK;` |
|        - | 12576 | `	}` |
|        - | 12577 | `	/* File to include */` |
|        8 | 12578 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 12579 | `	if( sFile.nByte < 1 ){` |
|        - | 12580 | `		/* Empty string,return NULL */` |
|      ! 0 | 12581 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12582 | `		return SXRET_OK;` |
|        - | 12583 | `	}` |
|        - | 12584 | `	/* Open,compile and execute the desired script */` |
|        8 | 12585 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 12586 | `	if( rc != SXRET_OK ){` |
|        - | 12587 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12588 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12589 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12590 | `		return PH7_ABORT;` |
|        - | 12591 | `	}` |
|        8 | 12592 | `	return SXRET_OK;` |
|        5 | 12593 |  |
|        - | 12594 | `/*` |
|        - | 12595 | ` * require_once:` |
|        - | 12596 | ` *  According to the PHP reference manual.` |
|        - | 12597 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 12598 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 12599 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 12600 | ` *   and how it differs from its non _once siblings.` |
|        - | 12601 | ` */` |
|        4 | 12602 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12603 |  |
|        - | 12604 | `	SyString sFile;` |
|        - | 12605 | `	sxi32 rc;` |
|        5 | 12606 | `	if( nArg < 1 ){` |
|        - | 12607 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12608 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12609 | `		return SXRET_OK;` |
|        - | 12610 | `	}` |
|        - | 12611 | `	/* File to include */` |
|        5 | 12612 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12613 | `	if( sFile.nByte < 1 ){` |
|        - | 12614 | `		/* Empty string,return NULL */` |
|      ! 0 | 12615 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12616 | `		return SXRET_OK;` |
|        - | 12617 | `	}` |
|        - | 12618 | `	/* Open,compile and execute the desired script */` |
|        5 | 12619 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12620 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12621 | `		/* File already included,return TRUE */` |
|        3 | 12622 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12623 | `		return SXRET_OK;` |
|        - | 12624 | `	}` |
|        3 | 12625 | `	if( rc != SXRET_OK ){` |
|        - | 12626 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12627 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12628 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12629 | `		return PH7_ABORT;` |
|        - | 12630 | `	}` |
|        3 | 12631 | `	return SXRET_OK;` |
|        3 | 12632 |  |
|        - | 12633 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12634 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12635 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12636 | `/*` |
|        - | 12637 | ` * Section:` |
|        - | 12638 | ` *  SPL Autoloading functions.` |
|        - | 12639 | ` * Status:` |
|        - | 12640 | ` *  Stable.` |
|        - | 12641 | ` */` |
|        - | 12642 | `/*` |
|        - | 12643 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12644 | ` *  Register given function as __autoload() implementation.` |
|        - | 12645 | ` * Parameters` |
|        - | 12646 | ` *  callback` |
|        - | 12647 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12648 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12649 | ` *  throw` |
|        - | 12650 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12651 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12652 | ` *  prepend` |
|        - | 12653 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12654 | ` *   autoload stack instead of appending it.` |
|        - | 12655 | ` * Return` |
|        - | 12656 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12657 | ` */` |
|       34 | 12658 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12659 |  |
|        - | 12660 | `	VmAutoloadCB sEntry;` |
|       36 | 12661 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12662 | `	int iPrepend = 0;` |
|        - | 12663 | `	sxu32 n;` |
|       36 | 12664 | `	if( nArg < 1 ){` |
|        - | 12665 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12666 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12667 | `		/* Check for duplicates first */` |
|        9 | 12668 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12669 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12670 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12671 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12672 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12673 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12674 | `				return SXRET_OK;` |
|        - | 12675 | `			}` |
|      ! 0 | 12676 | `		}` |
|        5 | 12677 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12678 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12679 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12680 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12681 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12682 | `		return SXRET_OK;` |
|        - | 12683 | `	}` |
|        - | 12684 | `	/* Validate that the callback is callable */` |
|       28 | 12685 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12686 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12687 | `		if( nArg >= 2 ){` |
|      ! 0 | 12688 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12689 | `		}` |
|      ! 0 | 12690 | `		if( iThrow ){` |
|      ! 0 | 12691 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12692 | `				"Argument is not callable");` |
|      ! 0 | 12693 | `		}` |
|      ! 0 | 12694 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12695 | `		return SXRET_OK;` |
|        - | 12696 | `	}` |
|        - | 12697 | `	/* Check for duplicates */` |
|       46 | 12698 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12699 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12700 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12701 | `			/* Already registered */` |
|      ! 0 | 12702 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12703 | `			return SXRET_OK;` |
|        - | 12704 | `		}` |
|       11 | 12705 | `	}` |
|        - | 12706 | `	/* Check prepend flag */` |
|       28 | 12707 | `	if( nArg >= 3 ){` |
|        3 | 12708 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12709 | `	}` |
|        - | 12710 | `	/* Store the callback */` |
|       28 | 12711 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12712 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12713 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12714 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12715 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12716 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12717 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12718 | `		VmAutoloadCB *aBase;` |
|        3 | 12719 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12720 | `		/* Rotate: move last entry to front */` |
|        3 | 12721 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12722 | `		if( aBase ){` |
|        - | 12723 | `			VmAutoloadCB sTemp;` |
|        - | 12724 | `			sxu32 i;` |
|        3 | 12725 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12726 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12727 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12728 | `			}` |
|        3 | 12729 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12730 | `		}` |
|        2 | 12731 | `	}else{` |
|       26 | 12732 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12733 | `	}` |
|       28 | 12734 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12735 | `	return SXRET_OK;` |
|       19 | 12736 |  |
|        - | 12737 | `/*` |
|        - | 12738 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12739 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12740 | ` * Parameters` |
|        - | 12741 | ` *  callback` |
|        - | 12742 | ` *   The autoload function being unregistered.` |
|        - | 12743 | ` * Return` |
|        - | 12744 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12745 | ` */` |
|       32 | 12746 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12747 |  |
|       34 | 12748 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12749 | `	sxu32 n,nEntry;` |
|       34 | 12750 | `	if( nArg < 1 ){` |
|      ! 0 | 12751 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12752 | `		return SXRET_OK;` |
|        - | 12753 | `	}` |
|       34 | 12754 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12755 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12756 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12757 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12758 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12759 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12760 | `			sxu32 i;` |
|       32 | 12761 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12762 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12763 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12764 | `			}` |
|        - | 12765 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12766 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12767 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12768 | `			return SXRET_OK;` |
|        - | 12769 | `		}` |
|        3 | 12770 | `	}` |
|        3 | 12771 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12772 | `	return SXRET_OK;` |
|       18 | 12773 |  |
|        - | 12774 | `/*` |
|        - | 12775 | ` * array spl_autoload_functions(void)` |
|        - | 12776 | ` *  Return all registered __autoload() functions.` |
|        - | 12777 | ` * Return` |
|        - | 12778 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12779 | ` *  an empty array is returned.` |
|        - | 12780 | ` */` |
|       20 | 12781 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12782 |  |
|       21 | 12783 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12784 | `	ph7_value *pArray;` |
|        - | 12785 | `	sxu32 n,nEntry;` |
|       10 | 12786 | `	SXUNUSED(nArg);` |
|       10 | 12787 | `	SXUNUSED(apArg);` |
|       21 | 12788 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12789 | `	if( pArray == 0 ){` |
|      ! 0 | 12790 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12791 | `		return SXRET_OK;` |
|        - | 12792 | `	}` |
|       21 | 12793 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12794 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12795 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12796 | `		if( pEntry ){` |
|       15 | 12797 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12798 | `		}` |
|        8 | 12799 | `	}` |
|       21 | 12800 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12801 | `	return SXRET_OK;` |
|       11 | 12802 |  |
|        - | 12803 | `/*` |
|        - | 12804 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12805 | ` *  Default implementation of __autoload().` |
|        - | 12806 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12807 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12808 | ` * Parameters` |
|        - | 12809 | ` *  class` |
|        - | 12810 | ` *   The class name being searched.` |
|        - | 12811 | ` *  file_extensions` |
|        - | 12812 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12813 | ` */` |
|        2 | 12814 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12815 |  |
|        - | 12816 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12817 | `	SyBlob sPath;` |
|        - | 12818 | `	int nClass;` |
|        - | 12819 | `	sxi32 rc;` |
|        3 | 12820 | `	if( nArg < 1 ){` |
|      ! 0 | 12821 | `		return SXRET_OK;` |
|        - | 12822 | `	}` |
|        3 | 12823 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12824 | `	if( nClass < 1 ){` |
|      ! 0 | 12825 | `		return SXRET_OK;` |
|        - | 12826 | `	}` |
|        - | 12827 | `	/* Default extensions */` |
|        3 | 12828 | `	zExt = ".php,.inc";` |
|        3 | 12829 | `	if( nArg >= 2 ){` |
|        - | 12830 | `		int nExt;` |
|      ! 0 | 12831 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12832 | `		if( nExt < 1 ){` |
|      ! 0 | 12833 | `			zExt = ".php,.inc";` |
|      ! 0 | 12834 | `		}` |
|      ! 0 | 12835 | `	}` |
|        3 | 12836 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12837 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12838 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12839 | `	zCur = zExt;` |
|        7 | 12840 | `	while( zCur < zEnd ){` |
|        - | 12841 | `		const char *zComma;` |
|        - | 12842 | `		SyString sFile;` |
|        - | 12843 | `		int i;` |
|        - | 12844 | `		/* Find next comma or end */` |
|        5 | 12845 | `		zComma = zCur;` |
|       21 | 12846 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12847 | `			zComma++;` |
|        1 | 12848 | `		}` |
|        - | 12849 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12850 | `		SyBlobReset(&sPath);` |
|       69 | 12851 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12852 | `			char c = zClass[i];` |
|       65 | 12853 | `			if( c == '\\' ){` |
|      ! 0 | 12854 | `				c = '/';` |
|       65 | 12855 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12856 | `				c = c + ('a' - 'A');` |
|        6 | 12857 | `			}` |
|       65 | 12858 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12859 | `		}` |
|        - | 12860 | `		/* Append extension */` |
|        5 | 12861 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12862 | `		/* Try to include the file */` |
|        5 | 12863 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12864 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12865 | `		if( rc == SXRET_OK ){` |
|        - | 12866 | `			/* File included successfully */` |
|      ! 0 | 12867 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12868 | `			return SXRET_OK;` |
|        - | 12869 | `		}` |
|        - | 12870 | `		/* Move past the comma */` |
|        5 | 12871 | `		zCur = zComma;` |
|        5 | 12872 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12873 | `			zCur++;` |
|        1 | 12874 | `		}` |
|        1 | 12875 | `	}` |
|        3 | 12876 | `	SyBlobRelease(&sPath);` |
|        3 | 12877 | `	return SXRET_OK;` |
|        2 | 12878 |  |
|        - | 12879 | `/* Table of built-in VM functions. */` |
|        - | 12880 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12881 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12882 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12883 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12884 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12885 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12886 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12887 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12888 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12889 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12890 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12891 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12892 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12893 | `	    /* Constants management */` |
|        - | 12894 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12895 | `	{ "define",   vm_builtin_define               },` |
|        - | 12896 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12897 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12898 | `	   /* Class/Object functions */` |
|        - | 12899 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12900 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12901 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12902 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12903 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12904 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12905 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12906 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12907 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12908 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12909 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12910 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12911 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12912 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12913 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12914 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12915 | `	   /* SPL Autoloading */` |
|        - | 12916 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12917 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12918 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12919 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12920 | `	   /* Random numbers/strings generators */` |
|        - | 12921 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12922 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12923 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12924 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12925 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12926 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12927 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12928 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12929 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12930 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12931 | `	   /* Language constructs functions */` |
|        - | 12932 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12933 | `	{ "print", vm_builtin_print                   },` |
|        - | 12934 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12935 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12936 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12937 | `	  /* Variable handling functions */` |
|        - | 12938 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12939 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12940 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12941 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12942 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12943 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12944 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12945 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12946 | `	  /* Ouput control functions */` |
|        - | 12947 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12948 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12949 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12950 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12951 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12952 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12953 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12954 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12955 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12956 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12957 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12958 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12959 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12960 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12961 | `	  /* Assertion functions */` |
|        - | 12962 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12963 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12964 | `	  /* Error reporting functions */` |
|        - | 12965 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12966 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12967 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12968 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12969 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12970 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12971 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12972 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12973 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12974 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12975 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12976 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12977 | `	  /* Release info */` |
|        - | 12978 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12979 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12980 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12981 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12982 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12983 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12984 | `	  /* hashmap */` |
|        - | 12985 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12986 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12987 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12988 | `	  /* URL related function */` |
|        - | 12989 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12990 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12991 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12992 | `	   /* XML processing functions */` |
|        - | 12993 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12994 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12995 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12996 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12997 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12998 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12999 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13000 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13001 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13002 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13003 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13004 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13005 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13006 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13007 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13008 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13009 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13010 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13011 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13012 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13013 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13014 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13015 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13016 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13017 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13018 | `	   /* Command line processing */` |
|        - | 13019 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13020 | `	   /* JSON encoding/decoding */` |
|        - | 13021 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13022 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13023 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13024 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13025 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13026 | `	   /* Files/URI inclusion facility */` |
|        - | 13027 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13028 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13029 | `	{ "include",      vm_builtin_include          },` |
|        - | 13030 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13031 | `	{ "require",      vm_builtin_require          },` |
|        - | 13032 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13033 | `};` |
|        - | 13034 | `/*` |
|        - | 13035 | ` * Register the built-in VM functions defined above.` |
|        - | 13036 | ` */` |
|     2474 | 13037 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13038 |  |
|        - | 13039 | `	sxi32 rc;` |
|        - | 13040 | `	sxu32 n;` |
|   319148 | 13041 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13042 | `		/* Note that these special functions have access` |
|        - | 13043 | `		 * to the underlying virtual machine as their` |
|        - | 13044 | `		 * private data.` |
|        - | 13045 | `		 */` |
|   316674 | 13046 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   316674 | 13047 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13048 | `			return rc;` |
|        - | 13049 | `		}` |
|   158338 | 13050 | `	}` |
|     2476 | 13051 | `	return SXRET_OK;` |
|     1239 | 13052 |  |
|        - | 13053 | `/*` |
|        - | 13054 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13055 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13056 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13057 | ` */` |
|    34980 | 13058 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13059 |  |
|    34982 | 13060 | `	if( !iLoadable ){` |
|    33450 | 13061 | `		return pClass;` |
|        - | 13062 | `	}` |
|     1534 | 13063 | `	while(pClass){` |
|     1534 | 13064 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1534 | 13065 | `			return pClass;` |
|        - | 13066 | `		}` |
|      ! 0 | 13067 | `		pClass = pClass->pNextName;` |
|      ! 0 | 13068 | `	}` |
|      ! 0 | 13069 | `	return 0;` |
|    17492 | 13070 |  |
|        - | 13071 | `/*` |
|        - | 13072 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13073 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13074 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13075 | ` * registered in the VM's class table.` |
|        - | 13076 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13077 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13078 | ` */` |
|       36 | 13079 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13080 |  |
|        - | 13081 | `	VmAutoloadCB *pEntry;` |
|        - | 13082 | `	ph7_value sArg,sResult;` |
|        - | 13083 | `	SyHashEntry *pHashEntry;` |
|        - | 13084 | `	ph7_class *pClass;` |
|        - | 13085 | `	sxu32 n,nEntry;` |
|       38 | 13086 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13087 | `	if( nEntry < 1 ){` |
|       24 | 13088 | `		return 0;` |
|        - | 13089 | `	}` |
|        - | 13090 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13091 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13092 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13093 | `	}` |
|        - | 13094 | `	/* Mark this class as being autoloaded */` |
|       14 | 13095 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13096 | `	/* Prepare the class name argument */` |
|       14 | 13097 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13098 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13099 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13100 | `	pClass = 0;` |
|       28 | 13101 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13102 | `		ph7_value *apArg[1];` |
|       24 | 13103 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13104 | `		if( pEntry == 0 ){` |
|      ! 0 | 13105 | `			continue;` |
|        - | 13106 | `		}` |
|       24 | 13107 | `		apArg[0] = &sArg;` |
|       24 | 13108 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13109 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13110 | `			continue;` |
|        - | 13111 | `		}` |
|        - | 13112 | `		/* Check if the class is now available */` |
|       24 | 13113 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13114 | `		if( pHashEntry ){` |
|       10 | 13115 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13116 | `			if( pClass ){` |
|       10 | 13117 | `				break;` |
|        - | 13118 | `			}` |
|      ! 0 | 13119 | `		}` |
|        9 | 13120 | `	}` |
|       14 | 13121 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13122 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13123 | `	/* Remove reentrancy guard */` |
|       14 | 13124 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13125 | `	return pClass;` |
|       20 | 13126 |  |
|        - | 13127 | `/*` |
|        - | 13128 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13129 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13130 | ` */` |
|       18 | 13131 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13132 |  |
|       20 | 13133 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13134 |  |
|        - | 13135 | `/*` |
|        - | 13136 | ` * Check if the given name refer to an installed class.` |
|        - | 13137 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13138 | ` */` |
|    34990 | 13139 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13140 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13141 | `	const char *zName,  /* Name of the target class */` |
|        - | 13142 | `	sxu32 nByte,        /* zName length */` |
|        - | 13143 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13144 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13145 | `						 */` |
|        - | 13146 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13147 | `	)` |
|        2 | 13148 |  |
|        - | 13149 | `	SyHashEntry *pEntry;` |
|        - | 13150 | `	ph7_class *pClass;` |
|    17495 | 13151 | `	SXUNUSED(iNest);` |
|        - | 13152 | `	/* Exact class lookup.` |
|        - | 13153 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13154 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    34992 | 13155 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    34992 | 13156 | `	if( pEntry == 0 ){` |
|        - | 13157 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 13158 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13159 | `	}` |
|    34974 | 13160 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    34974 | 13161 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    17497 | 13162 |  |
|        - | 13163 | `/*` |
|        - | 13164 | ` * Reference Table Implementation` |
|        - | 13165 | ` * Status: stable <chm@symisc.net>` |
|        - | 13166 | ` * Intro` |
|        - | 13167 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13168 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13169 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13170 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13171 | ` *  Refer to the official for more information on this powerful` |
|        - | 13172 | ` *  extension.` |
|        - | 13173 | ` */` |
|        - | 13174 | `/*` |
|        - | 13175 | ` * Allocate a new reference entry.` |
|        - | 13176 | ` */` |
|  3090882 | 13177 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13178 |  |
|        - | 13179 | `	VmRefObj *pRef;` |
|        - | 13180 | `	/* Allocate a new instance */` |
|  3090884 | 13181 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3090884 | 13182 | `	if( pRef == 0 ){` |
|      ! 0 | 13183 | `		return 0;` |
|        - | 13184 | `	}` |
|        - | 13185 | `	/* Zero the structure */` |
|  3090884 | 13186 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13187 | `	/* Initialize fields */` |
|  3090884 | 13188 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3090884 | 13189 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3090884 | 13190 | `	pRef->nIdx = nIdx;` |
|  3090884 | 13191 | `	return pRef;` |
|  1545443 | 13192 |  |
|        - | 13193 | `/*` |
|        - | 13194 | ` * Default hash function used by the reference table` |
|        - | 13195 | ` * for lookup/insertion operations.` |
|        - | 13196 | ` */` |
| 17039353 | 13197 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13198 |  |
|        - | 13199 | `	/* Calculate the hash based on the memory object index */` |
| 17039355 | 13200 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13201 |  |
|        - | 13202 | `/*` |
|        - | 13203 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13204 | ` * in the reference table.` |
|        - | 13205 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13206 | ` * otherwise.` |
|        - | 13207 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13208 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13209 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13210 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13211 | ` * Refer to the official for more information on this powerful` |
|        - | 13212 | ` * extension.` |
|        - | 13213 | ` */` |
|  9222530 | 13214 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13215 |  |
|        - | 13216 | `	VmRefObj *pRef;` |
|        - | 13217 | `	sxu32 nBucket;` |
|        - | 13218 | `	/* Point to the appropriate bucket */` |
|  9222532 | 13219 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13220 | `	/* Perform the lookup */` |
|  9222532 | 13221 | `	pRef = pVm->apRefObj[nBucket];` |
| 20062312 | 13222 | `	for(;;){` |
| 40114501 | 13223 | `		if( pRef == 0 ){` |
|  3176042 | 13224 | `			break;` |
|        - | 13225 | `		}` |
| 36938461 | 13226 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13227 | `			/* Entry found */` |
|  6046492 | 13228 | `			return pRef;` |
|        - | 13229 | `		}` |
|        - | 13230 | `		/* Point to the next entry */` |
| 30891971 | 13231 | `		pRef = pRef->pNextCollide;` |
|        2 | 13232 | `	}` |
|        - | 13233 | `	/* No such entry,return NULL */` |
|  3176042 | 13234 | `	return 0;` |
|  4611267 | 13235 |  |
|        - | 13236 | `/*` |
|        - | 13237 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13238 | ` *` |
|        - | 13239 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13240 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13241 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13242 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13243 | ` * Refer to the official for more information on this powerful` |
|        - | 13244 | ` * extension.` |
|        - | 13245 | ` */` |
|  3090882 | 13246 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13247 |  |
|        - | 13248 | `	sxu32 nBucket;` |
|  3090884 | 13249 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 13250 | `		VmRefObj **apNew;` |
|        - | 13251 | `		sxu32 nNew;` |
|        - | 13252 | `		/* Allocate a larger table */` |
|     4224 | 13253 | `		nNew = pVm->nRefSize << 1;` |
|     4224 | 13254 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4224 | 13255 | `		if( apNew ){` |
|     4224 | 13256 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 13257 | `			sxu32 n;` |
|        - | 13258 | `			/* Zero the structure */` |
|     4224 | 13259 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 13260 | `			/* Rehash all referenced entries */` |
|  2843182 | 13261 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 13262 | `				/* Remove old collision links */` |
|  2838960 | 13263 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 13264 | `				/* Point to the appropriate bucket */` |
|  2838960 | 13265 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 13266 | `				/* Insert the entry  */` |
|  2838960 | 13267 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2838960 | 13268 | `				if( apNew[nBucket] ){` |
|  2298896 | 13269 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 13270 | `				}` |
|  2838960 | 13271 | `				apNew[nBucket] = pEntry;` |
|        - | 13272 | `				/* Point to the next entry */` |
|  2838960 | 13273 | `				pEntry = pEntry->pNext;` |
|  1419481 | 13274 | `			}` |
|        - | 13275 | `			/* Release the old table */` |
|     4224 | 13276 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 13277 | `			/* Install the new one */` |
|     4224 | 13278 | `			pVm->apRefObj = apNew;` |
|     4224 | 13279 | `			pVm->nRefSize = nNew;` |
|     2111 | 13280 | `		}` |
|     2111 | 13281 | `	}` |
|        - | 13282 | `	/* Point to the appropriate bucket */` |
|  3090884 | 13283 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 13284 | `	/* Insert the entry */` |
|  3090884 | 13285 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3090884 | 13286 | `	if( pVm->apRefObj[nBucket] ){` |
|  2544613 | 13287 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1272115 | 13288 | `	}` |
|  3090884 | 13289 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3090884 | 13290 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3090884 | 13291 | `	pVm->nRefUsed++;` |
|  3090884 | 13292 | `	return SXRET_OK;` |
|        2 | 13293 |  |
|        - | 13294 | `/*` |
|        - | 13295 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 13296 | ` * the reference table.` |
|        - | 13297 | ` * This function is invoked when the user perform an unset` |
|        - | 13298 | ` * call [i.e: unset($var); ].` |
|        - | 13299 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13300 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13301 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13302 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13303 | ` * Refer to the official for more information on this powerful` |
|        - | 13304 | ` * extension.` |
|        - | 13305 | ` */` |
|  3055188 | 13306 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13307 |  |
|        - | 13308 | `	ph7_hashmap_node **apNode;` |
|        - | 13309 | `	SyHashEntry **apEntry;` |
|        - | 13310 | `	sxu32 n;` |
|        - | 13311 | `	/* Point to the reference table */` |
|  3055190 | 13312 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3055190 | 13313 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 13314 | `	/* Unlink the entry from the reference table */` |
|  3146640 | 13315 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    91452 | 13316 | `		if( apEntry[n] ){` |
|    91402 | 13317 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    45700 | 13318 | `		}` |
|    45727 | 13319 | `	}` |
|  6021488 | 13320 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2966300 | 13321 | `		if( apNode[n] ){` |
|     7220 | 13322 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3609 | 13323 | `		}` |
|  1483151 | 13324 | `	}` |
|  3055190 | 13325 | `	if( pRef->pPrevCollide ){` |
|  1168207 | 13326 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   584410 | 13327 | `	}else{` |
|  1886985 | 13328 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 13329 | `	}` |
|  3055190 | 13330 | `	if( pRef->pNextCollide ){` |
|  1732777 | 13331 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   866163 | 13332 | `	}` |
|  3055190 | 13333 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 13334 | `	/* Release the node */` |
|  3055190 | 13335 | `	SySetRelease(&pRef->aReference);` |
|  3055190 | 13336 | `	SySetRelease(&pRef->aArrEntries);` |
|  3055190 | 13337 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3055190 | 13338 | `	pVm->nRefUsed--;` |
|  3055190 | 13339 | `	return SXRET_OK;` |
|        2 | 13340 |  |
|        - | 13341 | `/*` |
|        - | 13342 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13343 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13344 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13345 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13346 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13347 | ` * Refer to the official for more information on this powerful` |
|        - | 13348 | ` * extension.` |
|        - | 13349 | ` */` |
|  3123044 | 13350 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 13351 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 13352 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 13353 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 13354 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 13355 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 13356 | `	)` |
|        2 | 13357 |  |
|  3123046 | 13358 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13359 | `	VmRefObj *pRef;` |
|        - | 13360 | `	/* Check if the referenced object already exists */` |
|  3123046 | 13361 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3123046 | 13362 | `	if( pRef == 0 ){` |
|        - | 13363 | `		/* Create a new entry */` |
|  3090884 | 13364 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3090884 | 13365 | `		if( pRef == 0 ){` |
|      ! 0 | 13366 | `			return SXERR_MEM;` |
|        - | 13367 | `		}` |
|  3090884 | 13368 | `		pRef->iFlags = iFlags;` |
|        - | 13369 | `		/* Install the entry */` |
|  3090884 | 13370 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1545441 | 13371 | `	}` |
|  3123046 | 13372 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3123046 | 13373 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 13374 | `		VmSlot sRef;` |
|        - | 13375 | `		/* Local frame,record referenced entry so that it can` |
|        - | 13376 | `		 * be deleted when we leave this frame.` |
|        - | 13377 | `		 */` |
|    85238 | 13378 | `		sRef.nIdx = nIdx;` |
|    85238 | 13379 | `		sRef.pUserData = pEntry;` |
|    85238 | 13380 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 13381 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 13382 | `		}` |
|    42618 | 13383 | `	}` |
|  3123046 | 13384 | `	if( pEntry ){` |
|        - | 13385 | `		/* Address of the hash-entry */` |
|   117206 | 13386 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    58602 | 13387 | `	}` |
|  3123046 | 13388 | `	if( pMapEntry ){` |
|        - | 13389 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3000094 | 13390 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1500046 | 13391 | `	}` |
|  3123046 | 13392 | `	return SXRET_OK;` |
|  1561524 | 13393 |  |
|        - | 13394 | `/*` |
|        - | 13395 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 13396 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13397 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13398 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13399 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13400 | ` * Refer to the official for more information on this powerful` |
|        - | 13401 | ` * extension.` |
|        - | 13402 | ` */` |
|  3044292 | 13403 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 13404 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 13405 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 13406 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 13407 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 13408 | `	)` |
|        2 | 13409 |  |
|        - | 13410 | `	VmRefObj *pRef;` |
|        - | 13411 | `	sxu32 n;` |
|        - | 13412 | `	/* Check if the referenced object already exists */` |
|  3044294 | 13413 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3044294 | 13414 | `	if( pRef == 0 ){` |
|        - | 13415 | `		/* Not such entry */` |
|    85154 | 13416 | `		return SXERR_NOTFOUND;` |
|        - | 13417 | `	}` |
|        - | 13418 | `	/* Remove the desired entry */` |
|  2959142 | 13419 | `	if( pEntry ){` |
|        - | 13420 | `		SyHashEntry **apEntry;` |
|       56 | 13421 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 13422 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 13423 | `			if( apEntry[n] == pEntry ){` |
|        - | 13424 | `				/* Nullify the entry */` |
|       56 | 13425 | `				apEntry[n] = 0;` |
|        - | 13426 | `				/*` |
|        - | 13427 | `				 * NOTE:` |
|        - | 13428 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 13429 | `				 * we avoid wasting spaces.` |
|        - | 13430 | `				 */` |
|       27 | 13431 | `			}` |
|       79 | 13432 | `		}` |
|       27 | 13433 | `	}` |
|  2959142 | 13434 | `	if( pMapEntry ){` |
|        - | 13435 | `		ph7_hashmap_node **apNode;` |
|  2959088 | 13436 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5918268 | 13437 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2959182 | 13438 | `			if( apNode[n] == pMapEntry ){` |
|        - | 13439 | `				/* nullify the entry */` |
|  2959088 | 13440 | `				apNode[n] = 0;` |
|  1479543 | 13441 | `			}` |
|  1479592 | 13442 | `		}` |
|  1479543 | 13443 | `	}` |
|  2959142 | 13444 | `	return SXRET_OK;` |
|  1522148 | 13445 |  |
|        - | 13446 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 13447 | `/*` |
|        - | 13448 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 13449 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 13450 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 13451 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 13452 | ` * For more information on how to register IO stream devices,please` |
|        - | 13453 | ` * refer to the official documentation.` |
|        - | 13454 | ` */` |
|    25812 | 13455 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 13456 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 13457 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 13458 | `	int nByte              /* *pzDevice length*/` |
|        - | 13459 | `	)` |
|        2 | 13460 |  |
|        - | 13461 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 13462 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 13463 | `	SyString sDev,sCur;` |
|        - | 13464 | `	sxu32 n,nEntry;` |
|        - | 13465 | `	int rc;` |
|        - | 13466 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    25814 | 13467 | `	zNext = zCur = zIn = *pzDevice;` |
|    25814 | 13468 | `	zEnd = &zIn[nByte];` |
|  1642630 | 13469 | `	while( zIn < zEnd ){` |
|  1616820 | 13470 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 13471 | `			/* Got one */` |
|        3 | 13472 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 13473 | `			break;` |
|        - | 13474 | `		}` |
|        - | 13475 | `		/* Advance the cursor */` |
|  1616818 | 13476 | `		zIn++;` |
|        2 | 13477 | `	}` |
|    25814 | 13478 | `	if( zIn >= zEnd ){` |
|        - | 13479 | `		/* No such scheme,return the default stream */` |
|    25812 | 13480 | `		return pVm->pDefStream;` |
|        - | 13481 | `	}` |
|        3 | 13482 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 13483 | `	/* Remove leading and trailing white spaces */` |
|        3 | 13484 | `	SyStringFullTrim(&sDev);` |
|        - | 13485 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 13486 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 13487 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 13488 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 13489 | `		pStream = apStream[n];` |
|        3 | 13490 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 13491 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 13492 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 13493 | `		if( rc == 0 ){` |
|        - | 13494 | `			/* Stream device found */` |
|        3 | 13495 | `			*pzDevice = zNext;` |
|        3 | 13496 | `			return pStream;` |
|        - | 13497 | `		}` |
|      ! 0 | 13498 | `	}` |
|        - | 13499 | `	/* No such stream,return NULL */` |
|      ! 0 | 13500 | `	return 0;` |
|    12908 | 13501 |  |
|        - | 13502 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 13503 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 13504 |  |
