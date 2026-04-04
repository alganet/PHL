# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4534/5998 lines (75.59%)

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
|        - |    80 | `/* Uncaught exception code value */` |
|        - |    81 | `#define PH7_EXCEPTION -255` |
|        - |    82 |  |
|        - |    83 | `/*` |
|        - |    84 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |    85 | ` */` |
|   778012 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   778014 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   777984 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   777976 |    94 | `	return FALSE;` |
|   389030 |    95 |  |
|        - |    96 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |    97 | `/*` |
|        - |    98 | ` * Register a constant and it's associated expansion callback so that` |
|        - |    99 | ` * it can be expanded from the target PHP program.` |
|        - |   100 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   101 | ` * simple and work as follows:` |
|        - |   102 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   103 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   104 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   105 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   106 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   107 | ` * (Windows,Linux,...) and so on.` |
|        - |   108 | ` * Please refer to the official documentation for additional information.` |
|        - |   109 | ` */` |
|   493714 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   111 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   112 | `	const SyString *pName,  /* Constant name */` |
|        - |   113 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   114 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   115 | `	)` |
|        2 |   116 |  |
|        - |   117 | `	ph7_constant *pCons;` |
|        - |   118 | `	SyHashEntry *pEntry;` |
|        - |   119 | `	char *zDupName;` |
|        - |   120 | `	sxi32 rc;` |
|   493716 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   493716 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   493712 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   493712 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   493712 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   493712 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   493712 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   493712 |   142 | `	pCons->xExpand = xExpand;` |
|   493712 |   143 | `	pCons->pUserData = pUserData;` |
|   493712 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   493712 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   493712 |   151 | `	return SXRET_OK;` |
|   246859 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1101884 |   160 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   161 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   162 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   163 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   164 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   165 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   166 | `	)` |
|        2 |   167 |  |
|        - |   168 | `	ph7_user_func *pFunc;` |
|        - |   169 | `	char *zDup;` |
|        - |   170 | `	/* Allocate a new user function */` |
|  1101886 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1101886 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1101886 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1101886 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1101886 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1101886 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1101886 |   185 | `	pFunc->pVm   = pVm;` |
|  1101886 |   186 | `	pFunc->xFunc = xFunc;` |
|  1101886 |   187 | `	pFunc->pUserData = pUserData;` |
|  1101886 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1101886 |   190 | `	*ppOut = pFunc;` |
|  1101886 |   191 | `	return SXRET_OK;` |
|   550944 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1104316 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   202 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   203 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   204 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   205 | `	void *pUserData           /* Foreign function private data */` |
|        - |   206 | `	)` |
|        2 |   207 |  |
|        - |   208 | `	ph7_user_func *pFunc;` |
|        - |   209 | `	SyHashEntry *pEntry;` |
|        - |   210 | `	sxi32 rc;` |
|        - |   211 | `	/* Overwrite any previously registered function with the same name */` |
|  1104318 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1104318 |   213 | `	if( pEntry ){` |
|     2434 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2434 |   215 | `		pFunc->pUserData = pUserData;` |
|     2434 |   216 | `		pFunc->xFunc = xFunc;` |
|     2434 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2434 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1101886 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1101886 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1101886 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1101886 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1101886 |   233 | `	return SXRET_OK;` |
|   552160 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   141054 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   141056 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   141056 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   141056 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   141056 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   141056 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   141056 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   141056 |   260 | `	pFunc->iFlags = iFlags;` |
|   141056 |   261 | `	pFunc->pUserData = pUserData;` |
|   141056 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   141056 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   512694 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   512696 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    35556 |   283 | `		pName = &pFunc->sName;` |
|    17777 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   512696 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   512696 |   287 | `	if( pEntry ){` |
|   393454 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   393454 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   393454 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|   119244 |   297 | `	pFunc->pNextName = 0;` |
|   119244 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   119244 |   299 | `	return rc;` |
|   256349 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    38174 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    38176 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    38176 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    38176 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    38146 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    38146 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    38146 |   324 | `	return rc;` |
|    19089 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  3192080 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   330 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   331 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   332 | `	sxi32 iP1,    /* First operand */` |
|        - |   333 | `	sxu32 iP2,    /* Second operand */` |
|        - |   334 | `	void *p3,     /* Third operand */` |
|        - |   335 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   336 | `	)` |
|        2 |   337 |  |
|        - |   338 | `	VmInstr sInstr;` |
|        - |   339 | `	sxi32 rc;` |
|        - |   340 | `	/* Fill the VM instruction */` |
|  3192082 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  3192082 |   342 | `	sInstr.iP1 = iP1;` |
|  3192082 |   343 | `	sInstr.iP2 = iP2;` |
|  3192082 |   344 | `	sInstr.p3  = p3;` |
|  3192082 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   192066 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    96032 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  3192082 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3192082 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  3192082 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   341964 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   341966 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   341966 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   341966 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   170982 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   170984 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   189294 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   189296 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   189296 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   926216 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   926218 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   180038 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   180040 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   621036 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   621038 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    27640 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    27642 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    27642 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    27642 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    27642 |   417 | `	return &aInstr[n - 2];` |
|    13822 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    15706 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    15708 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    15708 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    15708 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    15708 |   437 | `	pFrame->pUserData = pUserData;` |
|    15708 |   438 | `	pFrame->pThis = pThis;` |
|    15708 |   439 | `	pFrame->pVm = pVm;` |
|    15708 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    15708 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    15708 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    15708 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    15708 |   444 | `	return pFrame;` |
|     7855 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    15682 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    15684 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15684 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    15684 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    15684 |   466 | `	pVm->pFrame = pFrame;` |
|    15684 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    12990 |   469 | `		*ppFrame = pFrame;` |
|     6494 |   470 | `	}` |
|    15684 |   471 | `	return SXRET_OK;` |
|     7843 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   475 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   476 | ` * information.` |
|        - |   477 | ` */` |
|       52 |   478 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   479 |  |
|        - |   480 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   481 | `	SyHashEntry *pEntry = 0;` |
|        - |   482 | `	sxi32 rc;` |
|        - |   483 | `	/* Point to the upper frame */` |
|       54 |   484 | `	pFrame = pVm->pFrame;` |
|       54 |   485 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       54 |   486 | `	pTarget = pFrame;` |
|       54 |   487 | `	pFrame = pTarget->pParent;` |
|       54 |   488 | `	while( pFrame ){` |
|       54 |   489 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   490 | `			/* Query the current frame */` |
|       54 |   491 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   492 | `			if( pEntry ){` |
|        - |   493 | `				/* Variable found */` |
|       54 |   494 | `				break;` |
|        - |   495 | `			}` |
|      ! 0 |   496 | `		}` |
|        - |   497 | `		/* Point to the upper frame */` |
|      ! 0 |   498 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   499 | `	}` |
|       54 |   500 | `	if( pEntry == 0 ){` |
|        - |   501 | `		/* Inexistant variable */` |
|      ! 0 |   502 | `		return SXERR_NOTFOUND;` |
|        - |   503 | `	}` |
|        - |   504 | `	/* Link to the current frame */` |
|       54 |   505 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   506 | `	if( rc == SXRET_OK ){` |
|        - |   507 | `		sxu32 nIdx;` |
|       54 |   508 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   509 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   510 | `	}` |
|       54 |   511 | `	return rc;` |
|       28 |   512 |  |
|        - |   513 | `/*` |
|        - |   514 | ` * Leave the top-most active frame.` |
|        - |   515 | ` */` |
|    12988 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    12990 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12990 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    12990 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12990 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12926 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    91046 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    78122 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39062 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    12926 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    91102 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    78178 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39090 |   535 | `			}` |
|     6462 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    12990 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12990 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    12990 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12990 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    12990 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6494 |   544 | `	}` |
|    12990 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6284400 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6284678 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6284402 |   556 | `	return pFrame;` |
|        2 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Compare two functions signature and return the comparison result.` |
|        - |   560 | ` */` |
|      818 |   561 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   562 |  |
|      819 |   563 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   564 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   565 | `	const char *zSin = pSecond->zString;` |
|      819 |   566 | `	const char *zFin = pFirst->zString;` |
|      819 |   567 | `	const char *zPtr = zFin;` |
|      409 |   568 | `	for(;;){` |
|      819 |   569 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   570 | `			break;` |
|        - |   571 | `		}` |
|      ! 0 |   572 | `		if( zFin[0] != zSin[0] ){` |
|        - |   573 | `			/* mismatch */` |
|      ! 0 |   574 | `			break;` |
|        - |   575 | `		}` |
|      ! 0 |   576 | `		zFin++;` |
|      ! 0 |   577 | `		zSin++;` |
|      ! 0 |   578 | `	}` |
|      819 |   579 | `	return (int)(zFin-zPtr);` |
|        1 |   580 |  |
|        - |   581 | `/*` |
|        - |   582 | ` * Select the appropriate VM function for the current call context.` |
|        - |   583 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   584 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   585 | ` * Refer to the official documentation for more information.` |
|        - |   586 | ` */` |
|      132 |   587 | `static ph7_vm_func * VmOverload(` |
|        - |   588 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   589 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   590 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   591 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   592 | `	)` |
|        2 |   593 |  |
|        - |   594 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   595 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   596 | `	ph7_vm_func *pLink;` |
|        - |   597 | `	SyString sArgSig;` |
|        - |   598 | `	SyBlob sSig;` |
|        - |   599 |  |
|      134 |   600 | `	pLink = pList;` |
|      134 |   601 | `	i = 0;` |
|        - |   602 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   603 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   604 | `		if( pLink == 0 ){` |
|       72 |   605 | `			break;` |
|        - |   606 | `		}` |
|      930 |   607 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   608 | `			/* Candidate for overloading */` |
|      884 |   609 | `			apSet[i++] = pLink;` |
|      441 |   610 | `		}` |
|        - |   611 | `		/* Point to the next entry */` |
|      930 |   612 | `		pLink = pLink->pNextName;` |
|        2 |   613 | `	}` |
|      134 |   614 | `	if( i < 1 ){` |
|        - |   615 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   616 | `		return pList;` |
|        - |   617 | `	}` |
|      134 |   618 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   619 | `		/* Return the only candidate */` |
|       32 |   620 | `		return apSet[0];` |
|        - |   621 | `	}` |
|        - |   622 | `	/* Calculate function signature */` |
|      103 |   623 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   624 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   625 | `		int c = 'n'; /* null */` |
|      253 |   626 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   627 | `			/* Hashmap */` |
|       45 |   628 | `			c = 'h';` |
|      231 |   629 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   630 | `			/* bool */` |
|      ! 0 |   631 | `			c = 'b';` |
|      209 |   632 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   633 | `			/* int */` |
|        5 |   634 | `			c = 'i';` |
|      207 |   635 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   636 | `			/* String */` |
|      105 |   637 | `			c = 's';` |
|      153 |   638 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   639 | `			/* Float */` |
|      ! 0 |   640 | `			c = 'f';` |
|      101 |   641 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   642 | `			/* Class instance */` |
|      ! 0 |   643 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   644 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   645 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   646 | `			c = -1;` |
|      ! 0 |   647 | `		}` |
|      253 |   648 | `		if( c > 0 ){` |
|      253 |   649 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   650 | `		}` |
|      127 |   651 | `	}` |
|      103 |   652 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   653 | `	iTarget = 0;` |
|      103 |   654 | `	iMax = -1;` |
|        - |   655 | `	/* Select the appropriate function */` |
|      921 |   656 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   657 | `		/* Compare the two signatures */` |
|      819 |   658 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   659 | `		if( iCur > iMax ){` |
|      103 |   660 | `			iMax = iCur;` |
|      103 |   661 | `			iTarget = j;` |
|       51 |   662 | `		}` |
|      410 |   663 | `	}` |
|      103 |   664 | `	SyBlobRelease(&sSig);` |
|        - |   665 | `	/* Appropriate function for the current call context */` |
|      103 |   666 | `	return apSet[iTarget];` |
|       68 |   667 |  |
|        - |   668 | `/* Forward declaration */` |
|        - |   669 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   670 | `/*` |
|        - |   671 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   672 | ` * it can be instanciated from the executed PHP script.` |
|        - |   673 | ` */` |
|   104534 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|   104536 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   435123 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   278322 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   278322 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   690 | `			ph7_value *pMemObj;` |
|        - |   691 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   692 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   693 | `			if( pMemObj == 0 ){` |
|      ! 0 |   694 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   695 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   696 | `					&pClass->sName,&pAttr->sName` |
|        - |   697 | `					);` |
|      ! 0 |   698 | `				return SXERR_MEM;` |
|        - |   699 | `			}` |
|     1294 |   700 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   701 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   702 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   703 | `			}` |
|        - |   704 | `			/* Record attribute index */` |
|     1294 |   705 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   706 | `			/* Install static attribute in the reference table */` |
|     1294 |   707 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   708 | `		}` |
|        2 |   709 | `	}` |
|        - |   710 | `	/* Install class methods */` |
|   104536 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    47824 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    56714 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|      286 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      286 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|      142 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    56714 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   562216 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   477148 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   477148 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   477142 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   477142 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   238570 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    56714 |   738 | `	pClass->bMounted = TRUE;` |
|    56714 |   739 | `	return SXRET_OK;` |
|    52269 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1166 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1168 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1168 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4838 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3672 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3672 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3672 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3672 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3672 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3666 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3666 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3666 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3666 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1188 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      593 |   777 | `			}` |
|     3666 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3666 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3666 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1834 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1168 |   800 | `	return SXRET_OK;` |
|      585 |   801 |  |
|        - |   802 | `/* Forward declaration */` |
|        - |   803 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   804 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   805 | `/*` |
|        - |   806 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   807 | ` */` |
|        - |   808 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   809 | `/*` |
|        - |   810 | ` * Reserve a constant memory object.` |
|        - |   811 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   812 | ` */` |
|   355354 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   355356 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   347274 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   173636 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   355356 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   355356 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   355356 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   355356 |   830 | `	return pObj;` |
|   177679 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2143768 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2143770 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2143770 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071884 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2143770 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2143770 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2143770 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2143770 |   853 | `	return pObj;` |
|  1071886 |   854 |  |
|        - |   855 | `/* Forward declaration */` |
|        - |   856 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   857 | `/* Forward declarations for Fiber C functions */` |
|        - |   858 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   859 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   860 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   861 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   862 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   863 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   864 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   865 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   866 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   867 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   868 | `/*` |
|        - |   869 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   870 | ` * directly as foreign functions.` |
|        - |   871 | ` */` |
|        - |   872 | `#define PH7_BUILTIN_LIB \` |
|        - |   873 | `	"class Exception { "\` |
|        - |   874 | `    "protected $message = 'Unknown exception';"\` |
|        - |   875 | `    "protected $code = 0;"\` |
|        - |   876 | `    "protected $file;"\` |
|        - |   877 | `    "protected $line;"\` |
|        - |   878 | `    "protected $trace;"\` |
|        - |   879 | `    "protected $previous;"\` |
|        - |   880 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   881 | `	"   if( isset($message) ){"\` |
|        - |   882 | `	"	  $this->message = $message;"\` |
|        - |   883 | `	"   }"\` |
|        - |   884 | `	"   $this->code = $code;"\` |
|        - |   885 | `	"   $this->file = __FILE__;"\` |
|        - |   886 | `	"   $this->line = __LINE__;"\` |
|        - |   887 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   888 | `	"   if( isset($previous) ){"\` |
|        - |   889 | `	"     $this->previous = $previous;"\` |
|        - |   890 | `	"   }"\` |
|        - |   891 | `	"}"\` |
|        - |   892 | `	"public function getMessage(){"\` |
|        - |   893 | `	"   return $this->message;"\` |
|        - |   894 | `	"}"\` |
|        - |   895 | `	" public function getCode(){"\` |
|        - |   896 | `	"  return $this->code;"\` |
|        - |   897 | `	"}"\` |
|        - |   898 | `	"public function getFile(){"\` |
|        - |   899 | `	"  return $this->file;"\` |
|        - |   900 | `	"}"\` |
|        - |   901 | `	"public function getLine(){"\` |
|        - |   902 | `	"  return $this->line;"\` |
|        - |   903 | `	"}"\` |
|        - |   904 | `	"public function getTrace(){"\` |
|        - |   905 | `	"   return $this->trace;"\` |
|        - |   906 | `	"}"\` |
|        - |   907 | `	"public function getTraceAsString(){"\` |
|        - |   908 | `	"  return debug_string_backtrace();"\` |
|        - |   909 | `	"}"\` |
|        - |   910 | `	"public function getPrevious(){"\` |
|        - |   911 | `	"    return $this->previous;"\` |
|        - |   912 | `	"}"\` |
|        - |   913 | `	"public function __toString(){"\` |
|        - |   914 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   915 | `    "}"\` |
|        - |   916 | `	"}"\` |
|        - |   917 | `	"class Error extends Exception { }"\` |
|        - |   918 | `	"class TypeError extends Error { }"\` |
|        - |   919 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   920 | `	"class ValueError extends Error { }"\` |
|        - |   921 | `	"class FiberError extends Error { }"\` |
|        - |   922 | `	"class AssertionError extends Error { }"\` |
|        - |   923 | `	"class ErrorException extends Exception { "\` |
|        - |   924 | `	"protected $severity;"\` |
|        - |   925 | `	"public function __construct(string $message = null,"\` |
|        - |   926 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   927 | `	"   if( isset($message) ){"\` |
|        - |   928 | `	"	  $this->message = $message;"\` |
|        - |   929 | `	"   }"\` |
|        - |   930 | `	"   $this->severity = $severity;"\` |
|        - |   931 | `	"   $this->code = $code;"\` |
|        - |   932 | `	"   $this->file = $filename;"\` |
|        - |   933 | `	"   $this->line = $lineno;"\` |
|        - |   934 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   935 | `	"   if( isset($previous) ){"\` |
|        - |   936 | `	"     $this->previous = $previous;"\` |
|        - |   937 | `	"   }"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getSeverity(){"\` |
|        - |   940 | `	"   return $this->severity;"\` |
|        - |   941 | `    "}"\` |
|        - |   942 | `	"}"\` |
|        - |   943 | `	"interface Iterator {"\` |
|        - |   944 | `	"public function current();"\` |
|        - |   945 | `	"public function key();"\` |
|        - |   946 | `	"public function next();"\` |
|        - |   947 | `	"public function rewind();"\` |
|        - |   948 | `	"public function valid();"\` |
|        - |   949 | `	"}"\` |
|        - |   950 | `	"interface IteratorAggregate {"\` |
|        - |   951 | `	"public function getIterator();"\` |
|        - |   952 | `	"}"\` |
|        - |   953 | `	"interface Serializable {"\` |
|        - |   954 | `	"public function serialize();"\` |
|        - |   955 | `	"public function unserialize(string $serialized);"\` |
|        - |   956 | `	"}"\` |
|        - |   957 | `	"/* Directory releated IO */"\` |
|        - |   958 | `	"class Directory {"\` |
|        - |   959 | `	"public $handle = null;"\` |
|        - |   960 | `	"public $path  = null;"\` |
|        - |   961 | `	"public function __construct(string $path)"\` |
|        - |   962 | `	"{"\` |
|        - |   963 | `	"   $this->handle = opendir($path);"\` |
|        - |   964 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   965 | `	"      $this->path = $path;"\` |
|        - |   966 | `	"   }"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"public function __destruct()"\` |
|        - |   969 | `	"{"\` |
|        - |   970 | `	"  if( $this->handle != null ){"\` |
|        - |   971 | `	"       closedir($this->handle);"\` |
|        - |   972 | `	"  }"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"public function read()"\` |
|        - |   975 | `	"{"\` |
|        - |   976 | `	"    return readdir($this->handle);"\` |
|        - |   977 | `	"}"\` |
|        - |   978 | `	"public function rewind()"\` |
|        - |   979 | `	"{"\` |
|        - |   980 | `	"    rewinddir($this->handle);"\` |
|        - |   981 | `	"}"\` |
|        - |   982 | `	"public function close()"\` |
|        - |   983 | `	"{"\` |
|        - |   984 | `	"    closedir($this->handle);"\` |
|        - |   985 | `	"    $this->handle = null;"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"class Fiber {"\` |
|        - |   989 | `	"  private $__ctx;"\` |
|        - |   990 | `	"  private $__callable;"\` |
|        - |   991 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |   992 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |   993 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |   994 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |   995 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |   996 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |   997 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |   998 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |   999 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1000 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1001 | `	"}"\` |
|        - |  1002 | `	"class stdClass{"\` |
|        - |  1003 | `	"  public $value;"\` |
|        - |  1004 | `	" /* Magic methods */"\` |
|        - |  1005 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1006 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1007 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1008 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1009 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1010 | `	"}"\` |
|        - |  1011 | `	"function dir(string $path){"\` |
|        - |  1012 | `	"   return new Directory($path);"\` |
|        - |  1013 | `	"}"\` |
|        - |  1014 | `	"function Dir(string $path){"\` |
|        - |  1015 | `	"   return new Directory($path);"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1018 | `    "{"\` |
|        - |  1019 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1020 | `	"  $aDir = array();"\` |
|        - |  1021 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1022 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1023 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1024 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1025 | `	"   }"\` |
|        - |  1026 | `	"  closedir($pHandle);"\` |
|        - |  1027 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1028 | `	"      rsort($aDir);"\` |
|        - |  1029 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1030 | `	"      sort($aDir);"\` |
|        - |  1031 | `	"  }"\` |
|        - |  1032 | `	"  return $aDir;"\` |
|        - |  1033 | `	"}"\` |
|        - |  1034 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1035 | `	"/* Open the target directory */"\` |
|        - |  1036 | `	"$zDir = dirname($pattern);"\` |
|        - |  1037 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1038 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1039 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1040 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1041 | `	"	return FALSE;"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"$pattern = basename($pattern);"\` |
|        - |  1044 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1045 | `	"/* Loop throw available entries */"\` |
|        - |  1046 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1047 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1048 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1049 | `	"	if( $rc ){"\` |
|        - |  1050 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1051 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1052 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1053 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1054 | `	"		  }"\` |
|        - |  1055 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1056 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1057 | `	"		 continue;"\` |
|        - |  1058 | `	"	   }"\` |
|        - |  1059 | `	"	   /* Add the entry */"\` |
|        - |  1060 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1061 | `	"	}"\` |
|        - |  1062 | `	" }"\` |
|        - |  1063 | `	"/* Close the handle */"\` |
|        - |  1064 | `	"closedir($pHandle);"\` |
|        - |  1065 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1066 | `	"  /* Sort the array */"\` |
|        - |  1067 | `	"  sort($pArray);"\` |
|        - |  1068 | `	"}"\` |
|        - |  1069 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1070 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1071 | `	"  $pArray[] = $pattern;"\` |
|        - |  1072 | `	"}"\` |
|        - |  1073 | `	"/* Return the created array */"\` |
|        - |  1074 | `	"return $pArray;"\` |
|        - |  1075 | `   "}"\` |
|        - |  1076 | `   "/* Creates a temporary file */"\` |
|        - |  1077 | `   "function tmpfile(){"\` |
|        - |  1078 | `   "  /* Extract the temp directory */"\` |
|        - |  1079 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1080 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1081 | `   "    /* Use the current dir */"\` |
|        - |  1082 | `   "    $zTempDir = '.';"\` |
|        - |  1083 | `   "  }"\` |
|        - |  1084 | `   "  /* Create the file */"\` |
|        - |  1085 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1086 | `   "  return $pHandle;"\` |
|        - |  1087 | `   "}"\` |
|        - |  1088 | `   "/* Creates a temporary filename */"\` |
|        - |  1089 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1090 | `   "{"\` |
|        - |  1091 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1092 | `   "}"\` |
|        - |  1093 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1094 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1095 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1096 | `   "/* Copy arguments */"\` |
|        - |  1097 | `   "$nArgs = func_num_args();"\` |
|        - |  1098 | `   "$pNew = array();"\` |
|        - |  1099 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1100 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1101 | `    "}"\` |
|        - |  1102 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1103 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1104 | `	"/* Erase */"\` |
|        - |  1105 | `	"array_erase($pArray);"\` |
|        - |  1106 | `	"/* Unshift */"\` |
|        - |  1107 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1108 | `	"return sizeof($pArray);"\` |
|        - |  1109 | `    "}"\` |
|        - |  1110 | `	"function array_merge_recursive(){"\` |
|        - |  1111 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1112 | `    "$arrays = func_get_args();"\` |
|        - |  1113 | `    "$narrays = count($arrays);"\` |
|        - |  1114 | `    "$ret = array();"\` |
|        - |  1115 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1116 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1117 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1118 | `	 " }"\` |
|        - |  1119 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1120 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1121 | `     "  if( $keyIsInt ) {"\` |
|        - |  1122 | `     "   $ret[] = $value;"\` |
|        - |  1123 | `     "  } else {"\` |
|        - |  1124 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1125 | `     "    $cur = $ret[$key];"\` |
|        - |  1126 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1127 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1128 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1129 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1130 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1131 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1132 | `     "    } else {"\` |
|        - |  1133 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1134 | `     "    }"\` |
|        - |  1135 | `     "   } else {"\` |
|        - |  1136 | `     "    $ret[$key] = $value;"\` |
|        - |  1137 | `     "   }"\` |
|        - |  1138 | `     "  }"\` |
|        - |  1139 | `     " }"\` |
|        - |  1140 | `	 " }"\` |
|        - |  1141 | `	 " return $ret;"\` |
|        - |  1142 | `    "}"\` |
|        - |  1143 | `	"function max(){"\` |
|        - |  1144 | `    "  $pArgs = func_get_args();"\` |
|        - |  1145 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1146 | `	"  return null;"\` |
|        - |  1147 | `    " }"\` |
|        - |  1148 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1149 | `    " $pArg = $pArgs[0];"\` |
|        - |  1150 | `	" if( !is_array($pArg) ){"\` |
|        - |  1151 | `	"   return $pArg; "\` |
|        - |  1152 | `	" }"\` |
|        - |  1153 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1154 | `	"   return null;"\` |
|        - |  1155 | `	" }"\` |
|        - |  1156 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1157 | `	" reset($pArg);"\` |
|        - |  1158 | `	" $max = current($pArg);"\` |
|        - |  1159 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1160 | `	"   if( $val > $max ){"\` |
|        - |  1161 | `	"     $max = $val;"\` |
|        - |  1162 | `    " }"\` |
|        - |  1163 | `	" }"\` |
|        - |  1164 | `	" return $max;"\` |
|        - |  1165 | `    " }"\` |
|        - |  1166 | `    " $max = $pArgs[0];"\` |
|        - |  1167 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1168 | `    " $val = $pArgs[$i];"\` |
|        - |  1169 | `	"if( $val > $max ){"\` |
|        - |  1170 | `	" $max = $val;"\` |
|        - |  1171 | `	"}"\` |
|        - |  1172 | `    " }"\` |
|        - |  1173 | `	" return $max;"\` |
|        - |  1174 | `    "}"\` |
|        - |  1175 | `	"function min(){"\` |
|        - |  1176 | `    "  $pArgs = func_get_args();"\` |
|        - |  1177 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1178 | `	"  return null;"\` |
|        - |  1179 | `    " }"\` |
|        - |  1180 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1181 | `    " $pArg = $pArgs[0];"\` |
|        - |  1182 | `	" if( !is_array($pArg) ){"\` |
|        - |  1183 | `	"   return $pArg; "\` |
|        - |  1184 | `	" }"\` |
|        - |  1185 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1186 | `	"   return null;"\` |
|        - |  1187 | `	" }"\` |
|        - |  1188 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1189 | `	" reset($pArg);"\` |
|        - |  1190 | `	" $min = current($pArg);"\` |
|        - |  1191 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1192 | `	"   if( $val < $min ){"\` |
|        - |  1193 | `	"     $min = $val;"\` |
|        - |  1194 | `    " }"\` |
|        - |  1195 | `	" }"\` |
|        - |  1196 | `	" return $min;"\` |
|        - |  1197 | `    " }"\` |
|        - |  1198 | `    " $min = $pArgs[0];"\` |
|        - |  1199 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1200 | `    " $val = $pArgs[$i];"\` |
|        - |  1201 | `	"if( $val < $min ){"\` |
|        - |  1202 | `	" $min = $val;"\` |
|        - |  1203 | `	" }"\` |
|        - |  1204 | `    " }"\` |
|        - |  1205 | `	" return $min;"\` |
|        - |  1206 | `	"}"\` |
|        - |  1207 | `	"function fileowner(string $file){"\` |
|        - |  1208 | `    " $a = stat($file);"\` |
|        - |  1209 | `	" if( !is_array($a) ){"\` |
|        - |  1210 | `	"	return false;"\` |
|        - |  1211 | `	" }"\` |
|        - |  1212 | `	" return $a['uid'];"\` |
|        - |  1213 | `    "}"\` |
|        - |  1214 | `    "function filegroup(string $file){"\` |
|        - |  1215 | `	" $a = stat($file);"\` |
|        - |  1216 | `	" if( !is_array($a) ){"\` |
|        - |  1217 | `	"	return false;"\` |
|        - |  1218 | `	" }"\` |
|        - |  1219 | `	" return $a['gid'];"\` |
|        - |  1220 | `    "}"\` |
|        - |  1221 | `	 "function fileinode(string $file){"\` |
|        - |  1222 | `	" $a = stat($file);"\` |
|        - |  1223 | `	" if( !is_array($a) ){"\` |
|        - |  1224 | `	"	return false;"\` |
|        - |  1225 | `	" }"\` |
|        - |  1226 | `	" return $a['ino'];"\` |
|        - |  1227 | `    "}"` |
|        - |  1228 |  |
|        - |  1229 | `/*` |
|        - |  1230 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1231 | ` * start compiling the target PHP program.` |
|        - |  1232 | ` */` |
|     2694 |  1233 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1234 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1235 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1236 | `	 )` |
|        2 |  1237 |  |
|        - |  1238 | `	SyString sBuiltin;` |
|        - |  1239 | `	ph7_value *pObj;` |
|        - |  1240 | `	sxi32 rc;` |
|        - |  1241 | `	/* Zero the structure */` |
|     2696 |  1242 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1243 | `	/* Initialize VM fields */` |
|     2696 |  1244 | `	pVm->pEngine = &(*pEngine);` |
|     2696 |  1245 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1246 | `	/* Instructions containers */` |
|     2696 |  1247 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2696 |  1248 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2696 |  1249 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1250 | `	/* Object containers */` |
|     2696 |  1251 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2696 |  1252 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1253 | `	/* Virtual machine internal containers */` |
|     2696 |  1254 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2696 |  1255 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2696 |  1256 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2696 |  1257 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2696 |  1258 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2696 |  1259 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2696 |  1260 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2696 |  1261 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2696 |  1262 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2696 |  1263 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2696 |  1264 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2696 |  1265 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2696 |  1266 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2696 |  1267 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2696 |  1268 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2696 |  1269 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2696 |  1270 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2696 |  1271 | `	pVm->pPendingException = 0;` |
|        - |  1272 | `	/* Configuration containers */` |
|     2696 |  1273 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2696 |  1274 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2696 |  1275 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2696 |  1276 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2696 |  1277 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2696 |  1278 | `	pVm->iResponseStatus = 200;` |
|     2696 |  1279 | `	pVm->bHeadersSent = 0;` |
|     2696 |  1280 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1281 | `	/* Error callbacks containers */` |
|     2696 |  1282 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2696 |  1283 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2696 |  1284 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2696 |  1285 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2696 |  1286 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1287 | `	/* Set a default recursion limit */` |
|        - |  1288 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2696 |  1289 | `	pVm->nMaxDepth = 32;` |
|        - |  1290 | `#else` |
|        - |  1291 | `	pVm->nMaxDepth = 16;` |
|        - |  1292 | `#endif` |
|        - |  1293 | `	/* Default assertion flags */` |
|     2696 |  1294 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1295 | `	/* JSON return status */` |
|     2696 |  1296 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1297 | `	/* PRNG context */` |
|     2696 |  1298 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1299 | `	/* Install the null constant */` |
|     2696 |  1300 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2696 |  1301 | `	if( pObj == 0 ){` |
|      ! 0 |  1302 | `		rc = SXERR_MEM;` |
|      ! 0 |  1303 | `		goto Err;` |
|        - |  1304 | `	}` |
|     2696 |  1305 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1306 | `	/* Install the boolean TRUE constant */` |
|     2696 |  1307 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2696 |  1308 | `	if( pObj == 0 ){` |
|      ! 0 |  1309 | `		rc = SXERR_MEM;` |
|      ! 0 |  1310 | `		goto Err;` |
|        - |  1311 | `	}` |
|     2696 |  1312 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1313 | `	/* Install the boolean FALSE constant */` |
|     2696 |  1314 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2696 |  1315 | `	if( pObj == 0 ){` |
|      ! 0 |  1316 | `		rc = SXERR_MEM;` |
|      ! 0 |  1317 | `		goto Err;` |
|        - |  1318 | `	}` |
|     2696 |  1319 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1320 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1321 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1322 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2696 |  1323 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2696 |  1324 | `	if( pObj == 0 ){` |
|      ! 0 |  1325 | `		rc = SXERR_MEM;` |
|      ! 0 |  1326 | `		goto Err;` |
|        - |  1327 | `	}` |
|     2696 |  1328 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1329 | `	/* Create the global frame */` |
|     2696 |  1330 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2696 |  1331 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|        - |  1334 | `	/* Initialize the code generator */` |
|     2696 |  1335 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2696 |  1336 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1337 | `		goto Err;` |
|        - |  1338 | `	}` |
|        - |  1339 | `	/* VM correctly initialized,set the magic number */` |
|     2696 |  1340 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2696 |  1341 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1342 | `	/* Compile the built-in library */` |
|     2696 |  1343 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1344 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2696 |  1345 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1346 | `	/* Register Fiber internal C functions */` |
|     2696 |  1347 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2696 |  1348 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2696 |  1349 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2696 |  1350 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2696 |  1351 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2696 |  1352 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2696 |  1353 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2696 |  1354 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2696 |  1355 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2696 |  1356 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1357 | `	/* Reset the code generator */` |
|     2696 |  1358 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2696 |  1359 | `	return SXRET_OK;` |
|      ! 0 |  1360 | `Err:` |
|      ! 0 |  1361 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1362 | `	return rc;` |
|     1349 |  1363 |  |
|        - |  1364 | `/*` |
|        - |  1365 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1366 | ` * routine which store the output in an internal blob.` |
|        - |  1367 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1368 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1369 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1370 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1371 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1372 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1373 | ` * to finish executing and extracting the output.` |
|        - |  1374 | ` */` |
|       38 |  1375 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1376 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1377 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1378 | `	void *pUserData     /* User private data */` |
|        - |  1379 | `	)` |
|      ! 0 |  1380 |  |
|        - |  1381 | `	 sxi32 rc;` |
|        - |  1382 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1383 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1384 | `	 return rc;` |
|      ! 0 |  1385 |  |
|        - |  1386 | `/*` |
|        - |  1387 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1388 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1389 | ` */` |
|    13302 |  1390 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1391 |  |
|    13304 |  1392 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13304 |  1393 | `	if( xCons != VmObConsumer ){` |
|     6392 |  1394 | `		pVm->nOutputLen += nLen;` |
|     6392 |  1395 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      880 |  1396 | `			pVm->bHeadersSent = 1;` |
|      439 |  1397 | `		}` |
|     3195 |  1398 | `	}` |
|    13304 |  1399 |  |
|        - |  1400 | `#define VM_STACK_GUARD 16` |
|        - |  1401 | `/*` |
|        - |  1402 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1403 | ` * our compiled PHP program.` |
|        - |  1404 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1405 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1406 | ` */` |
|    32020 |  1407 | `static ph7_value * VmNewOperandStack(` |
|        - |  1408 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1409 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1410 | `	)` |
|        2 |  1411 |  |
|        - |  1412 | `	ph7_value *pStack;` |
|        - |  1413 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1414 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1415 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1416 | `  ** on the maximum stack depth required.` |
|        - |  1417 | `  **` |
|        - |  1418 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1419 | `  */` |
|    32022 |  1420 | `	nInstr += VM_STACK_GUARD;` |
|    32022 |  1421 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32022 |  1422 | `	if( pStack == 0 ){` |
|      ! 0 |  1423 | `		return 0;` |
|        - |  1424 | `	}` |
|        - |  1425 | `	/* Initialize the operand stack */` |
|  2023486 |  1426 | `	while( nInstr > 0 ){` |
|  1991466 |  1427 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1991466 |  1428 | `		--nInstr;` |
|        2 |  1429 | `	}` |
|        - |  1430 | `	/* Ready for bytecode execution */` |
|    32022 |  1431 | `	return pStack;` |
|    16012 |  1432 |  |
|        - |  1433 | `/* Forward declaration */` |
|        - |  1434 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1435 | `/*` |
|        - |  1436 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1437 | ` * This routine gets called by the PH7 engine after` |
|        - |  1438 | ` * successful compilation of the target PHP program.` |
|        - |  1439 | ` */` |
|     2432 |  1440 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1441 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1442 | `	)` |
|        2 |  1443 |  |
|        - |  1444 | `	SyHashEntry *pEntry;` |
|        - |  1445 | `	sxi32 rc;` |
|     2434 |  1446 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1447 | `		/* Initialize your VM first */` |
|      ! 0 |  1448 | `		return SXERR_CORRUPT;` |
|        - |  1449 | `	}` |
|        - |  1450 | `	/* Mark the VM ready for byte-code execution */` |
|     2434 |  1451 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1452 | `	/* Release the code generator now we have compiled our program */` |
|     2434 |  1453 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1454 | `	/* Emit the DONE instruction */` |
|     2434 |  1455 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2434 |  1456 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1457 | `		return SXERR_MEM;` |
|        - |  1458 | `	}` |
|        - |  1459 | `	/* Script return value */` |
|     2434 |  1460 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1461 | `	/* Allocate a new operand stack */` |
|     2434 |  1462 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2434 |  1463 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1464 | `		return SXERR_MEM;` |
|        - |  1465 | `	}` |
|        - |  1466 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1467 | `	 * private data. */` |
|     2434 |  1468 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2434 |  1469 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1470 | `	/* Allocate the reference table */` |
|     2434 |  1471 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2434 |  1472 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2434 |  1473 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1474 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1475 | `		return SXERR_MEM;` |
|        - |  1476 | `	}` |
|        - |  1477 | `	/* Zero the reference table */` |
|     2434 |  1478 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1479 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2434 |  1480 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2434 |  1481 | `	if( rc != SXRET_OK ){` |
|        - |  1482 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1483 | `		return rc;` |
|        - |  1484 | `	}` |
|        - |  1485 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2434 |  1486 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2434 |  1487 | `	if( rc != SXRET_OK ){` |
|        - |  1488 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1489 | `		return rc;` |
|        - |  1490 | `	}` |
|        - |  1491 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2434 |  1492 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1493 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2434 |  1494 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1495 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2434 |  1496 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1497 | `	/* Initialize and install static and constants class attributes */` |
|     2434 |  1498 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    36646 |  1499 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    34214 |  1500 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    34214 |  1501 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1502 | `			return rc;` |
|        - |  1503 | `		}` |
|        2 |  1504 | `	}` |
|        - |  1505 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2434 |  1506 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1507 | `	/* VM is ready for bytecode execution */` |
|     2434 |  1508 | `	return SXRET_OK;` |
|     1218 |  1509 |  |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1512 | ` */` |
|      ! 0 |  1513 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1514 |  |
|      ! 0 |  1515 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1516 | `		return SXERR_CORRUPT;` |
|        - |  1517 | `	}` |
|        - |  1518 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1519 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1520 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1521 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1522 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1523 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1524 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1525 | `	pVm->bHttpContext = 0;` |
|        - |  1526 | `	/* Set the ready flag */` |
|      ! 0 |  1527 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1528 | `	return SXRET_OK;` |
|      ! 0 |  1529 |  |
|        - |  1530 | `/*` |
|        - |  1531 | ` * Release a Virtual Machine.` |
|        - |  1532 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1533 | ` */` |
|     2424 |  1534 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1535 |  |
|        - |  1536 | `	/* Set the stale magic number */` |
|     2426 |  1537 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1538 | `	/* Release the private memory subsystem */` |
|     2426 |  1539 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2426 |  1540 | `	return SXRET_OK;` |
|        2 |  1541 |  |
|        - |  1542 | `/*` |
|        - |  1543 | ` * Initialize a foreign function call context.` |
|        - |  1544 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1545 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1546 | ` * functions.` |
|        - |  1547 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1548 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1549 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1550 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1551 | ` */` |
|   564392 |  1552 | `static sxi32 VmInitCallContext(` |
|        - |  1553 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1554 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1555 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1556 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1557 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1558 | `	)` |
|        2 |  1559 |  |
|   564394 |  1560 | `	pOut->pFunc = pFunc;` |
|   564394 |  1561 | `	pOut->pVm   = pVm;` |
|   564394 |  1562 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   564394 |  1563 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1564 | `	/* Assume a null return value */` |
|   564394 |  1565 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   564394 |  1566 | `	pOut->pRet = pRet;` |
|   564394 |  1567 | `	pOut->iFlags = iFlags;` |
|   564394 |  1568 | `	return SXRET_OK;` |
|        2 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1572 | ` * left behind.` |
|        - |  1573 | ` */` |
|   564392 |  1574 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1575 |  |
|        - |  1576 | `	sxu32 n;` |
|   564394 |  1577 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6852 |  1578 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19542 |  1579 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12692 |  1580 | `			if( apObj[n] == 0 ){` |
|        - |  1581 | `				/* Already released */` |
|      250 |  1582 | `				continue;` |
|        - |  1583 | `			}` |
|    12444 |  1584 | `			PH7_MemObjRelease(apObj[n]);` |
|    12444 |  1585 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6223 |  1586 | `		}` |
|     6852 |  1587 | `		SySetRelease(&pCtx->sVar);` |
|     3425 |  1588 | `	}` |
|   564394 |  1589 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1590 | `		ph7_aux_data *aAux;` |
|        - |  1591 | `		void *pChunk;` |
|        - |  1592 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1593 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1594 | `		 */` |
|        9 |  1595 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1596 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1597 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1598 | `			/* Release the chunk */` |
|       25 |  1599 | `			if( pChunk ){` |
|       25 |  1600 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1601 | `			}` |
|       13 |  1602 | `		}` |
|        9 |  1603 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1604 | `	}` |
|   564394 |  1605 |  |
|        - |  1606 | `/*` |
|        - |  1607 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1608 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1609 | ` */` |
|      248 |  1610 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1611 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1612 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1613 | `	)` |
|        2 |  1614 |  |
|      250 |  1615 | `	if( pValue == 0 ){` |
|        - |  1616 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1617 | `		return;` |
|        - |  1618 | `	}` |
|      250 |  1619 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1620 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1621 | `		sxu32 n;` |
|      936 |  1622 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1623 | `			if( apObj[n] == pValue ){` |
|      250 |  1624 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1625 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1626 | `				/* Mark as released */` |
|      250 |  1627 | `				apObj[n] = 0;` |
|      250 |  1628 | `				break;` |
|        - |  1629 | `			}` |
|      345 |  1630 | `		}` |
|      124 |  1631 | `	}` |
|      126 |  1632 |  |
|        - |  1633 | `/*` |
|        - |  1634 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1635 | ` */` |
|  3296348 |  1636 | `static void VmPopOperand(` |
|        - |  1637 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1638 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1639 | `	)` |
|        2 |  1640 |  |
|  3296350 |  1641 | `	ph7_value *pTos = *ppTos;` |
|  7001686 |  1642 | `	while( nPop > 0 ){` |
|  3705338 |  1643 | `		PH7_MemObjRelease(pTos);` |
|  3705338 |  1644 | `		pTos--;` |
|  3705338 |  1645 | `		nPop--;` |
|        2 |  1646 | `	}` |
|        - |  1647 | `	/* Top of the stack */` |
|  3296350 |  1648 | `	*ppTos = pTos;` |
|  3296350 |  1649 |  |
|        - |  1650 | `/*` |
|        - |  1651 | ` * Reserve a memory object.` |
|        - |  1652 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1653 | ` */` |
|  3014658 |  1654 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1655 |  |
|  3014660 |  1656 | `	ph7_value *pObj = 0;` |
|        - |  1657 | `	VmSlot *pSlot;` |
|        - |  1658 | `	sxu32 nIdx;` |
|        - |  1659 | `	/* Check for a free slot */` |
|  3014660 |  1660 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3014660 |  1661 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3014660 |  1662 | `	if( pSlot ){` |
|   870892 |  1663 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   870892 |  1664 | `		nIdx = pSlot->nIdx;` |
|   435445 |  1665 | `	}` |
|  3014660 |  1666 | `	if( pObj == 0 ){` |
|        - |  1667 | `		/* Reserve a new memory object */` |
|  2143770 |  1668 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2143770 |  1669 | `		if( pObj == 0 ){` |
|      ! 0 |  1670 | `			return 0;` |
|        - |  1671 | `		}` |
|  1071884 |  1672 | `	}` |
|        - |  1673 | `	/* Set a null default value */` |
|  3014660 |  1674 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3014660 |  1675 | `	pObj->nIdx = nIdx;` |
|  3014660 |  1676 | `	return pObj;` |
|  1507331 |  1677 |  |
|        - |  1678 | `/*` |
|        - |  1679 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1680 | ` */` |
|    31192 |  1681 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1682 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1683 | `	const char *zKey,  /* Entry key */` |
|        - |  1684 | `	sxu32 nByte,       /* Key length */` |
|        - |  1685 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1686 | `	)` |
|        2 |  1687 |  |
|        - |  1688 | `	ph7_value sKey;` |
|        - |  1689 | `	sxi32 rc;` |
|    31194 |  1690 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    31194 |  1691 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1692 | `	/* Perform the insertion */` |
|    31194 |  1693 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    31194 |  1694 | `	PH7_MemObjRelease(&sKey);` |
|    31194 |  1695 | `	return rc;` |
|        2 |  1696 |  |
|        - |  1697 | `/*` |
|        - |  1698 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1699 | ` * Return a pointer to the variable value on success.` |
|        - |  1700 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1701 | ` */` |
|  3078952 |  1702 | `static ph7_value * VmExtractMemObj(` |
|        - |  1703 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1704 | `	const SyString *pName, /* Variable name */` |
|        - |  1705 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1706 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1707 | `	)` |
|        2 |  1708 |  |
|  3078954 |  1709 | `	int bNullify = FALSE;` |
|        - |  1710 | `	SyHashEntry *pEntry;` |
|        - |  1711 | `	VmFrame *pFrame;` |
|        - |  1712 | `	ph7_value *pObj;` |
|        - |  1713 | `	sxu32 nIdx;` |
|        - |  1714 | `	sxi32 rc;` |
|        - |  1715 | `	/* Point to the top active frame */` |
|  3078954 |  1716 | `	pFrame = pVm->pFrame;` |
|  3078954 |  1717 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1718 | `	/* Perform the lookup */` |
|  3078954 |  1719 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1720 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1721 | `		pName = &sAnnon;` |
|        - |  1722 | `		/* Always nullify the object */` |
|      ! 0 |  1723 | `		bNullify = TRUE;` |
|      ! 0 |  1724 | `		bDup = FALSE;` |
|      ! 0 |  1725 | `	}` |
|        - |  1726 | `	/* Check the superglobals table first */` |
|  3078954 |  1727 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3078954 |  1728 | `	if( pEntry == 0 ){` |
|        - |  1729 | `		/* Query the top active frame */` |
|  3078914 |  1730 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3078914 |  1731 | `		if( pEntry == 0 ){` |
|    85024 |  1732 | `			char *zName = (char *)pName->zString;` |
|        - |  1733 | `			VmSlot sLocal;` |
|    85024 |  1734 | `			if( !bCreate ){` |
|        - |  1735 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1736 | `				return 0;` |
|        - |  1737 | `			}` |
|        - |  1738 | `			/* No such variable,automatically create a new one and install` |
|        - |  1739 | `			 * it in the current frame.` |
|        - |  1740 | `			 */` |
|    84988 |  1741 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    84988 |  1742 | `			if( pObj == 0 ){` |
|      ! 0 |  1743 | `				return 0;` |
|        - |  1744 | `			}` |
|    84988 |  1745 | `			nIdx = pObj->nIdx;` |
|    84988 |  1746 | `			if( bDup ){` |
|        - |  1747 | `				/* Duplicate name */` |
|      168 |  1748 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1749 | `				if( zName == 0 ){` |
|      ! 0 |  1750 | `					return 0;` |
|        - |  1751 | `				}` |
|       83 |  1752 | `			}` |
|        - |  1753 | `			/* Link to the top active VM frame */` |
|    84988 |  1754 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    84988 |  1755 | `			if( rc != SXRET_OK ){` |
|        - |  1756 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1757 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1758 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1759 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1760 | `				return 0;` |
|        - |  1761 | `			}` |
|    84988 |  1762 | `			if( pFrame->pParent != 0 ){` |
|        - |  1763 | `				/* Local variable */` |
|    78142 |  1764 | `				sLocal.nIdx = nIdx;` |
|    78142 |  1765 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39072 |  1766 | `			}else{` |
|        - |  1767 | `				/* Register in the $GLOBALS array */` |
|     6848 |  1768 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1769 | `			}` |
|        - |  1770 | `			/* Install in the reference table */` |
|    84988 |  1771 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1772 | `			/* Save object index */` |
|    84988 |  1773 | `			pObj->nIdx = nIdx;` |
|    42495 |  1774 | `		}else{` |
|        - |  1775 | `			/* Extract variable contents */` |
|  2993892 |  1776 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2993892 |  1777 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2993892 |  1778 | `			if( bNullify && pObj ){` |
|      ! 0 |  1779 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1780 | `			}` |
|        - |  1781 | `		}` |
|  1539550 |  1782 | `	}else{` |
|        - |  1783 | `		/* Superglobal */` |
|       42 |  1784 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1785 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1786 | `	}` |
|  3078918 |  1787 | `	return pObj;` |
|  1539588 |  1788 |  |
|        - |  1789 | `/*` |
|        - |  1790 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1791 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1792 | ` */` |
|     2736 |  1793 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1794 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1795 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1796 | `	sxu32 nByte        /* zName length */` |
|        - |  1797 | `	)` |
|        2 |  1798 |  |
|        - |  1799 | `	SyHashEntry *pEntry;` |
|        - |  1800 | `	ph7_value *pValue;` |
|        - |  1801 | `	sxu32 nIdx;` |
|        - |  1802 | `	/* Query the superglobal table */` |
|     2738 |  1803 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2738 |  1804 | `	if( pEntry == 0 ){` |
|        - |  1805 | `		/* No such entry */` |
|      ! 0 |  1806 | `		return 0;` |
|        - |  1807 | `	}` |
|        - |  1808 | `	/* Extract the superglobal index in the global object pool */` |
|     2738 |  1809 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1810 | `	/* Extract the variable value  */` |
|     2738 |  1811 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2738 |  1812 | `	return pValue;` |
|     1370 |  1813 |  |
|        - |  1814 | `/*` |
|        - |  1815 | ` * Perform a raw hashmap insertion.` |
|        - |  1816 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1817 | ` */` |
|     2766 |  1818 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1819 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1820 | `	const char *zKey,   /* Entry key */` |
|        - |  1821 | `	int nKeylen,        /* zKey length*/` |
|        - |  1822 | `	const char *zData,  /* Entry data */` |
|        - |  1823 | `	int nLen            /* zData length */` |
|        - |  1824 | `	)` |
|        2 |  1825 |  |
|        - |  1826 | `	ph7_value sKey,sValue;` |
|        - |  1827 | `	sxi32 rc;` |
|     2768 |  1828 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2768 |  1829 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2768 |  1830 | `	if( zKey ){` |
|     2746 |  1831 | `		if( nKeylen < 0 ){` |
|     2694 |  1832 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1346 |  1833 | `		}` |
|     2746 |  1834 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1372 |  1835 | `	}` |
|     2768 |  1836 | `	if( zData ){` |
|     2768 |  1837 | `		if( nLen < 0 ){` |
|        - |  1838 | `			/* Compute length automatically */` |
|      144 |  1839 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1840 | `		}` |
|     2768 |  1841 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1383 |  1842 | `	}` |
|        - |  1843 | `	/* Perform the insertion */` |
|     2768 |  1844 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2768 |  1845 | `	PH7_MemObjRelease(&sKey);` |
|     2768 |  1846 | `	PH7_MemObjRelease(&sValue);` |
|     2768 |  1847 | `	return rc;` |
|        2 |  1848 |  |
|        - |  1849 | `/*` |
|        - |  1850 | ` * Configure a working virtual machine instance.` |
|        - |  1851 | ` *` |
|        - |  1852 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1853 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1854 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1855 | ` * The second argument to this function is an integer configuration option` |
|        - |  1856 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1857 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1858 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1859 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1860 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1861 | ` */` |
|    39242 |  1862 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1863 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1864 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1865 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1866 | `	)` |
|        2 |  1867 |  |
|    39244 |  1868 | `	sxi32 rc = SXRET_OK;` |
|    39244 |  1869 | `	switch(nOp){` |
|     1208 |  1870 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2418 |  1871 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2418 |  1872 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1873 | `		/* VM output consumer callback */` |
|        - |  1874 | `#ifdef UNTRUST` |
|        - |  1875 | `		if( xConsumer == 0 ){` |
|        - |  1876 | `			rc = SXERR_CORRUPT;` |
|        - |  1877 | `			break;` |
|        - |  1878 | `		}` |
|        - |  1879 | `#endif` |
|        - |  1880 | `		/* Install the output consumer */` |
|     2418 |  1881 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2418 |  1882 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2418 |  1883 | `		break;` |
|        - |  1884 | `							   }` |
|     1216 |  1885 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1886 | `		/* Import path */` |
|        - |  1887 | `		  const char *zPath;` |
|        - |  1888 | `		  SyString sPath;` |
|     2434 |  1889 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1890 | `#if defined(UNTRUST)` |
|        - |  1891 | `		  if( zPath == 0 ){` |
|        - |  1892 | `			  rc = SXERR_EMPTY;` |
|        - |  1893 | `			  break;` |
|        - |  1894 | `		  }` |
|        - |  1895 | `#endif` |
|     2434 |  1896 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1897 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1898 | `#ifdef __WINNT__` |
|        2 |  1899 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1900 | `#endif` |
|     4866 |  1901 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1902 | `		  /* Remove leading and trailing white spaces */` |
|     2434 |  1903 | `		  SyStringFullTrim(&sPath);` |
|     2434 |  1904 | `		  if( sPath.nByte > 0 ){` |
|        - |  1905 | `			  /* Store the path in the corresponding conatiner */` |
|     2434 |  1906 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1216 |  1907 | `		  }` |
|     2434 |  1908 | `		  break;` |
|        - |  1909 | `									 }` |
|     1216 |  1910 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1911 | `		/* Run-Time Error report */` |
|     2434 |  1912 | `		pVm->bErrReport = 1;` |
|     2434 |  1913 | `		break;` |
|      ! 0 |  1914 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1915 | `		/* Recursion depth */` |
|      ! 0 |  1916 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1917 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1918 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1919 | `		}` |
|      ! 0 |  1920 | `		break;` |
|        - |  1921 | `									   }` |
|      ! 0 |  1922 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1923 | `		/* VM output length in bytes */` |
|      ! 0 |  1924 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1925 | `#ifdef UNTRUST` |
|        - |  1926 | `		if( pOut == 0 ){` |
|        - |  1927 | `			rc = SXERR_CORRUPT;` |
|        - |  1928 | `			break;` |
|        - |  1929 | `		}` |
|        - |  1930 | `#endif` |
|      ! 0 |  1931 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1932 | `		break;` |
|        - |  1933 | `							   }` |
|        - |  1934 |  |
|    12160 |  1935 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1936 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1937 | `		/* Create a new superglobal/global variable */` |
|    24322 |  1938 | `		const char *zName = va_arg(ap,const char *);` |
|    24322 |  1939 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1940 | `		SyHashEntry *pEntry;` |
|        - |  1941 | `		ph7_value *pObj;` |
|        - |  1942 | `		sxu32 nByte;` |
|        - |  1943 | `		sxu32 nIdx;` |
|        - |  1944 | `#ifdef UNTRUST` |
|        - |  1945 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1946 | `			rc = SXERR_CORRUPT;` |
|        - |  1947 | `			break;` |
|        - |  1948 | `		}` |
|        - |  1949 | `#endif` |
|    24322 |  1950 | `		nByte = SyStrlen(zName);` |
|    24322 |  1951 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1952 | `			/* Check if the superglobal is already installed */` |
|    24322 |  1953 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12162 |  1954 | `		}else{` |
|        - |  1955 | `			/* Query the top active VM frame */` |
|      ! 0 |  1956 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1957 | `		}` |
|    24322 |  1958 | `		if( pEntry ){` |
|        - |  1959 | `			/* Variable already installed */` |
|      ! 0 |  1960 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1961 | `			/* Extract contents */` |
|      ! 0 |  1962 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1963 | `			if( pObj ){` |
|        - |  1964 | `				/* Overwrite old contents */` |
|      ! 0 |  1965 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1966 | `			}` |
|      ! 0 |  1967 | `		}else{` |
|        - |  1968 | `			/* Install a new variable */` |
|    24322 |  1969 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    24322 |  1970 | `			if( pObj == 0 ){` |
|      ! 0 |  1971 | `				rc = SXERR_MEM;` |
|      ! 0 |  1972 | `				break;` |
|        - |  1973 | `			}` |
|    24322 |  1974 | `			nIdx = pObj->nIdx;` |
|        - |  1975 | `			/* Copy value */` |
|    24322 |  1976 | `			PH7_MemObjStore(pValue,pObj);` |
|    24322 |  1977 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1978 | `				/* Install the superglobal */` |
|    24322 |  1979 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12162 |  1980 | `			}else{` |
|        - |  1981 | `				/* Install in the current frame */` |
|      ! 0 |  1982 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1983 | `			}` |
|    24322 |  1984 | `			if( rc == SXRET_OK ){` |
|        - |  1985 | `				SyHashEntry *pRef;` |
|    24322 |  1986 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    24322 |  1987 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12162 |  1988 | `				}else{` |
|      ! 0 |  1989 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1990 | `				}` |
|        - |  1991 | `				/* Install in the reference table */` |
|    24322 |  1992 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    24322 |  1993 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1994 | `					/* Register in the $GLOBALS array */` |
|    24322 |  1995 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12160 |  1996 | `				}` |
|    12160 |  1997 | `			}` |
|        - |  1998 | `		}` |
|    24322 |  1999 | `		break;` |
|        - |  2000 | `									}` |
|     1346 |  2001 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2002 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2003 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2004 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2005 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2006 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2007 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2694 |  2008 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2694 |  2009 | `		const char *zValue = va_arg(ap,const char *);` |
|     2694 |  2010 | `		int nLen = va_arg(ap,int);` |
|        - |  2011 | `		ph7_hashmap *pMap;` |
|        - |  2012 | `		ph7_value *pValue;` |
|     2694 |  2013 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2014 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2015 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2693 |  2016 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2017 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2018 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2692 |  2019 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2020 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2021 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2692 |  2022 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2023 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2024 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2692 |  2025 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2026 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2027 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2692 |  2028 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2029 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2030 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2031 | `		}else{` |
|        - |  2032 | `			/* Extract the $_SERVER superglobal */` |
|     2692 |  2033 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2034 | `		}` |
|     2694 |  2035 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2036 | `			/* No such entry */` |
|      ! 0 |  2037 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2038 | `			break;` |
|        - |  2039 | `		}` |
|        - |  2040 | `		/* Point to the hashmap */` |
|     2694 |  2041 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2042 | `		/* Perform the insertion */` |
|     2694 |  2043 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2694 |  2044 | `		break;` |
|        - |  2045 | `								   }` |
|       11 |  2046 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2047 | `		/* Script arguments */` |
|       24 |  2048 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2049 | `		ph7_hashmap *pMap;` |
|        - |  2050 | `		ph7_value *pValue;` |
|        - |  2051 | `		sxu32 n;` |
|       24 |  2052 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2053 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2054 | `			break;` |
|        - |  2055 | `		}` |
|        - |  2056 | `		/* Extract the $argv array */` |
|       24 |  2057 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2058 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2059 | `			/* No such entry */` |
|      ! 0 |  2060 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2061 | `			break;` |
|        - |  2062 | `		}` |
|        - |  2063 | `		/* Point to the hashmap */` |
|       24 |  2064 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2065 | `		/* Perform the insertion */` |
|       24 |  2066 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2067 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2068 | `		if( rc == SXRET_OK ){` |
|       24 |  2069 | `			if( pMap->nEntry > 1 ){` |
|        - |  2070 | `				/* Append space separator first */` |
|       18 |  2071 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2072 | `			}` |
|       24 |  2073 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2074 | `		}` |
|       24 |  2075 | `		break;` |
|        - |  2076 | `								  }` |
|      ! 0 |  2077 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2078 | `		/* error_log() consumer */` |
|      ! 0 |  2079 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2080 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2081 | `		break;` |
|        - |  2082 | `										}` |
|      ! 0 |  2083 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2084 | `		/* Script return value */` |
|      ! 0 |  2085 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2086 | `#ifdef UNTRUST` |
|        - |  2087 | `		if( ppValue == 0 ){` |
|        - |  2088 | `			rc = SXERR_CORRUPT;` |
|        - |  2089 | `			break;` |
|        - |  2090 | `		}` |
|        - |  2091 | `#endif` |
|      ! 0 |  2092 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2093 | `		break;` |
|        - |  2094 | `								   }` |
|     2432 |  2095 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2096 | `		/* Register an IO stream device */` |
|     4866 |  2097 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2098 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7296 |  2099 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4866 |  2100 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2101 | `				/* Invalid stream */` |
|      ! 0 |  2102 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2103 | `				break;` |
|        - |  2104 | `		}` |
|     4866 |  2105 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2106 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2434 |  2107 | `			pVm->pDefStream = pStream;` |
|     1216 |  2108 | `		}` |
|        - |  2109 | `		/* Insert in the appropriate container */` |
|     4866 |  2110 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4866 |  2111 | `		break;` |
|        - |  2112 | `								  }` |
|        8 |  2113 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2114 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2115 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2116 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2117 | `#ifdef UNTRUST` |
|        - |  2118 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2119 | `			rc = SXERR_CORRUPT;` |
|        - |  2120 | `			break;` |
|        - |  2121 | `		}` |
|        - |  2122 | `#endif` |
|       16 |  2123 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2124 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2125 | `		break;` |
|        - |  2126 | `									   }` |
|        8 |  2127 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2128 | `		/* Raw HTTP request*/` |
|       16 |  2129 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2130 | `		int nByte = va_arg(ap,int);` |
|       16 |  2131 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2132 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2133 | `			break;` |
|        - |  2134 | `		}` |
|       16 |  2135 | `		if( nByte < 0 ){` |
|        - |  2136 | `			/* Compute length automatically */` |
|      ! 0 |  2137 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2138 | `		}` |
|        - |  2139 | `		/* Process the request */` |
|       16 |  2140 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2141 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2142 | `		if( rc == SXRET_OK ){` |
|       16 |  2143 | `			pVm->bHttpContext = 1;` |
|        8 |  2144 | `		}` |
|       16 |  2145 | `		break;` |
|        - |  2146 | `									}` |
|        8 |  2147 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2148 | `		/* Extract HTTP response status code */` |
|       16 |  2149 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2150 | `		if( pStatus ){` |
|       16 |  2151 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2152 | `		}` |
|       16 |  2153 | `		break;` |
|        - |  2154 | `										}` |
|        8 |  2155 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2156 | `		/* Iterate response headers via callback */` |
|        - |  2157 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2158 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2159 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2160 | `		if( xCallback ){` |
|       16 |  2161 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2162 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2163 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2164 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2165 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2166 | `							   pUserData);` |
|       12 |  2167 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2168 | `					break;` |
|        - |  2169 | `				}` |
|        6 |  2170 | `			}` |
|        8 |  2171 | `		}` |
|       16 |  2172 | `		break;` |
|        - |  2173 | `										 }` |
|      ! 0 |  2174 | `	default:` |
|        - |  2175 | `		/* Unknown configuration option */` |
|      ! 0 |  2176 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2177 | `		break;` |
|        - |  2178 | `	}` |
|    39244 |  2179 | `	return rc;` |
|        2 |  2180 |  |
|        - |  2181 | `/* Forward declaration */` |
|        - |  2182 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2183 | `/*` |
|        - |  2184 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2185 | ` * format.` |
|        - |  2186 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2187 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2188 | ` * (STDOUT).` |
|        - |  2189 | ` */` |
|        2 |  2190 | `static sxi32 VmByteCodeDump(` |
|        - |  2191 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2192 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2193 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2194 | `	)` |
|        1 |  2195 |  |
|        - |  2196 | `	static const char zDump[] = {` |
|        - |  2197 | `		"====================================================\n"` |
|        - |  2198 | `		"PH7 VM Dump\n"` |
|        - |  2199 | `		"====================================================\n"` |
|        - |  2200 | `	};` |
|        - |  2201 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2202 | `	sxi32 rc = SXRET_OK;` |
|        - |  2203 | `	sxu32 n;` |
|        - |  2204 | `	/* Point to the PH7 instructions */` |
|        3 |  2205 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2206 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2207 | `	n = 0;` |
|        3 |  2208 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2209 | `	/* Dump instructions */` |
|        7 |  2210 | `	for(;;){` |
|       15 |  2211 | `		if( pInstr >= pEnd ){` |
|        - |  2212 | `			/* No more instructions */` |
|        3 |  2213 | `			break;` |
|        - |  2214 | `		}` |
|        - |  2215 | `		/* Format and call the consumer callback */` |
|       19 |  2216 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2217 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2218 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2219 | `		if( rc != SXRET_OK ){` |
|        - |  2220 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2221 | `			return rc;` |
|        - |  2222 | `		}` |
|       13 |  2223 | `		++n;` |
|       13 |  2224 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2225 | `	}` |
|        3 |  2226 | `	return rc;` |
|        2 |  2227 |  |
|        - |  2228 | `/* Forward declaration */` |
|        - |  2229 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2230 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2231 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2232 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2233 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2234 | `/*` |
|        - |  2235 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2236 | ` * consumer callback.` |
|        - |  2237 | ` */` |
|      544 |  2238 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2239 |  |
|      545 |  2240 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2241 | `	sxi32 rc = SXRET_OK;` |
|        - |  2242 | `	/* Append a new line */` |
|        - |  2243 | `#ifdef __WINNT__` |
|        1 |  2244 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2245 | `#else` |
|      544 |  2246 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2247 | `#endif` |
|        - |  2248 | `	/* Invoke the output consumer callback */` |
|      545 |  2249 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2250 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2251 | `	return rc;` |
|        1 |  2252 |  |
|        - |  2253 | `/*` |
|        - |  2254 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2255 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2256 | ` * information.` |
|        - |  2257 | ` */` |
|      132 |  2258 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2259 |  |
|      134 |  2260 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2261 | `		ph7_value apArg[4];` |
|        - |  2262 | `		ph7_value *apArgPtr[4];` |
|        - |  2263 | `		ph7_value sResult;` |
|        - |  2264 | `		SyString sErr;` |
|        - |  2265 | `		/* Prepare arguments */` |
|       61 |  2266 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2267 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2268 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2269 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2270 | `		if( pFile ){` |
|       61 |  2271 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2272 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2273 | `		}else{` |
|      ! 0 |  2274 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2275 | `		}` |
|       61 |  2276 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2277 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2278 | `		/* Set up pointer array */` |
|       61 |  2279 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2280 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2281 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2282 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2283 | `		/* Call the handler */` |
|       61 |  2284 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2285 | `		/* Check return value */` |
|       61 |  2286 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2287 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2288 | `		}` |
|        - |  2289 | `		/* Release */` |
|       61 |  2290 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2291 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2292 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2293 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2294 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2295 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2296 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2297 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2298 | `	}` |
|        - |  2299 | `	/* No handler, always call error handler */` |
|       73 |  2300 | `	return TRUE;` |
|       68 |  2301 |  |
|       96 |  2302 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2303 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2304 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2305 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2306 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2307 | `	)` |
|        2 |  2308 |  |
|       98 |  2309 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2310 | `	SyString *pFile;` |
|        - |  2311 | `	char *zErr;` |
|       98 |  2312 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2313 | `	if( !pVm->bErrReport ){` |
|        - |  2314 | `		/* Don't bother reporting errors */` |
|        3 |  2315 | `		return SXRET_OK;` |
|        - |  2316 | `	}` |
|        - |  2317 | `	/* Reset the working buffer */` |
|       96 |  2318 | `	SyBlobReset(pWorker);` |
|        - |  2319 | `	/* Peek the processed file if available */` |
|       96 |  2320 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2321 | `	if( pFile ){` |
|        - |  2322 | `		/* Append file name */` |
|       96 |  2323 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2324 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2325 | `	}` |
|        - |  2326 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2327 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2328 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2329 | `	 * E_DEPRECATED). */` |
|       96 |  2330 | `	zErr = "Error:  ";` |
|       96 |  2331 | `	switch(iErr){` |
|       18 |  2332 | `	case PH7_CTX_WARNING:` |
|       38 |  2333 | `		zErr = "Warning:  ";` |
|       38 |  2334 | `		break;` |
|        6 |  2335 | `	case PH7_CTX_NOTICE:` |
|       14 |  2336 | `		zErr = "Notice:  ";` |
|       12 |  2337 | `		break;` |
|       23 |  2338 | `	default:` |
|        - |  2339 | `		/* keep iErr unchanged */` |
|       46 |  2340 | `		break;` |
|        - |  2341 | `	}` |
|       96 |  2342 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2343 | `	if( pFuncName ){` |
|        - |  2344 | `		/* Append function name first */` |
|       23 |  2345 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2346 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2347 | `	}` |
|       96 |  2348 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2349 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2350 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2351 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2352 | `	}` |
|       96 |  2353 | `	return rc;` |
|       50 |  2354 |  |
|        - |  2355 | `/*` |
|        - |  2356 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2357 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2358 | ` * information.` |
|        - |  2359 | ` */` |
|       38 |  2360 | `static sxi32 VmThrowErrorAp(` |
|        - |  2361 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2362 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2363 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2364 | `	const char *zFormat, /* Format message */` |
|        - |  2365 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2366 | `	)` |
|        2 |  2367 |  |
|       40 |  2368 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2369 | `	SyBlob sMsg;` |
|        - |  2370 | `	SyString *pFile;` |
|        - |  2371 | `	char *zErr;` |
|       40 |  2372 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2373 | `	if( !pVm->bErrReport ){` |
|        - |  2374 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2375 | `		return SXRET_OK;` |
|        - |  2376 | `	}` |
|        - |  2377 | `	/* Reset the working buffer */` |
|       40 |  2378 | `	SyBlobReset(pWorker);` |
|        - |  2379 | `	/* Peek the processed file if available */` |
|       40 |  2380 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2381 | `	if( pFile ){` |
|        - |  2382 | `		/* Append file name */` |
|       40 |  2383 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2384 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2385 | `	}` |
|        - |  2386 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2387 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2388 | `	 * the correct errno value. */` |
|       40 |  2389 | `	zErr = "Error:  ";` |
|       40 |  2390 | `	switch(iErr){` |
|        4 |  2391 | `	case PH7_CTX_WARNING:` |
|        9 |  2392 | `		zErr = "Warning:  ";` |
|        9 |  2393 | `		break;` |
|        3 |  2394 | `	case PH7_CTX_NOTICE:` |
|        7 |  2395 | `		zErr = "Notice:  ";` |
|        6 |  2396 | `		break;` |
|       12 |  2397 | `	default:` |
|        - |  2398 | `		/* do not change iErr */` |
|       24 |  2399 | `		break;` |
|        - |  2400 | `	}` |
|       40 |  2401 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2402 | `	if( pFuncName ){` |
|        - |  2403 | `		/* Append function name first */` |
|       26 |  2404 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2405 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2406 | `	}` |
|        - |  2407 | `	/* Format the raw message */` |
|       40 |  2408 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2409 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2410 | `	/* Check if a user error handler is installed */` |
|       40 |  2411 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2412 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2413 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2414 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2415 | `	}` |
|       40 |  2416 | `	SyBlobRelease(&sMsg);` |
|       40 |  2417 | `	return rc;` |
|       21 |  2418 |  |
|        - |  2419 | `/*` |
|        - |  2420 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2421 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2422 | ` * information.` |
|        - |  2423 | ` * ------------------------------------` |
|        - |  2424 | ` * Simple boring wrapper function.` |
|        - |  2425 | ` * ------------------------------------` |
|        - |  2426 | ` */` |
|       14 |  2427 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2428 |  |
|        - |  2429 | `	va_list ap;` |
|        - |  2430 | `	sxi32 rc;` |
|       15 |  2431 | `	va_start(ap,zFormat);` |
|       15 |  2432 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2433 | `	va_end(ap);` |
|       15 |  2434 | `	return rc;` |
|        1 |  2435 |  |
|        - |  2436 | `/*` |
|        - |  2437 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2438 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2439 | ` * information.` |
|        - |  2440 | ` * ------------------------------------` |
|        - |  2441 | ` * Simple boring wrapper function.` |
|        - |  2442 | ` * ------------------------------------` |
|        - |  2443 | ` */` |
|       24 |  2444 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2445 |  |
|        - |  2446 | `	sxi32 rc;` |
|       26 |  2447 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2448 | `	return rc;` |
|        2 |  2449 |  |
|        - |  2450 | `/*` |
|        - |  2451 | ` * Resolve function context from the current frame.` |
|        - |  2452 | ` */` |
|      934 |  2453 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2454 |  |
|        - |  2455 | `	VmFrame *pFrame;` |
|        - |  2456 | `	ph7_vm_func *pFunc;` |
|      935 |  2457 | `	*pzFuncName = 0;` |
|      935 |  2458 | `	*pnFuncLen = 0;` |
|      935 |  2459 | `	pFrame = pVm->pFrame;` |
|      935 |  2460 | `	if( pFrame == 0 ){` |
|      ! 0 |  2461 | `		return;` |
|        - |  2462 | `	}` |
|      935 |  2463 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2464 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2465 | `		return;` |
|        - |  2466 | `	}` |
|        7 |  2467 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2468 | `	if( pFunc == 0 ){` |
|      ! 0 |  2469 | `		return;` |
|        - |  2470 | `	}` |
|        7 |  2471 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2472 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2473 |  |
|        - |  2474 | `/*` |
|        - |  2475 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2476 | ` */` |
|      470 |  2477 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2478 |  |
|        - |  2479 | `	SyBlob sOut;` |
|        - |  2480 | `	SyString *pFile;` |
|      471 |  2481 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2482 | `		return PH7_OK;` |
|        - |  2483 | `	}` |
|      471 |  2484 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2485 | `		zClass = "Exception";` |
|      ! 0 |  2486 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2487 | `	}` |
|      471 |  2488 | `	if( zMsg == 0 ){` |
|      ! 0 |  2489 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2490 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2491 | `	}` |
|      471 |  2492 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2493 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2494 | `	}` |
|      471 |  2495 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2496 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2497 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2498 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2499 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2500 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2501 | `	if( pFile ){` |
|      471 |  2502 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2503 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2504 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2505 | `	}` |
|      471 |  2506 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2507 | `	if( pFile ){` |
|      471 |  2508 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2509 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2510 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2511 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2512 | `		}else{` |
|      465 |  2513 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2514 | `		}` |
|      235 |  2515 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2516 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2517 | `	}else{` |
|      ! 0 |  2518 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2519 | `	}` |
|      471 |  2520 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2521 | `	if( pFile ){` |
|      471 |  2522 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2523 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2524 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2525 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2526 | `	}` |
|      471 |  2527 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2528 | `	SyBlobRelease(&sOut);` |
|      471 |  2529 | `	return PH7_ABORT;` |
|      236 |  2530 |  |
|        - |  2531 | `/*` |
|        - |  2532 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2533 | ` */` |
|      472 |  2534 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2535 |  |
|        - |  2536 | `	ph7_vm *pVm;` |
|        - |  2537 | `	ph7_class *pClass;` |
|        - |  2538 | `	ph7_class_instance *pThis;` |
|        - |  2539 | `	ph7_class_method *pCons;` |
|        - |  2540 | `	ph7_value sArg;` |
|        - |  2541 | `	ph7_value *apArg[1];` |
|        - |  2542 | `	SyBlob sMsg;` |
|        - |  2543 | `	SyString sMsgStr;` |
|        - |  2544 | `	VmFrame *pFrame;` |
|        - |  2545 | `	va_list ap;` |
|        - |  2546 | `	sxi32 rc;` |
|        - |  2547 |  |
|      474 |  2548 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2549 | `		return PH7_ABORT;` |
|        - |  2550 | `	}` |
|      474 |  2551 | `	pVm = pCtx->pVm;` |
|      474 |  2552 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2553 | `		zClass = "Error";` |
|      ! 0 |  2554 | `	}` |
|      474 |  2555 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      474 |  2556 | `	if( pClass == 0 ){` |
|      ! 0 |  2557 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2558 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2559 | `			zClass` |
|        - |  2560 | `			);` |
|        - |  2561 | `	}` |
|      474 |  2562 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      474 |  2563 | `	if( pThis == 0 ){` |
|      ! 0 |  2564 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2565 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2566 | `			);` |
|        - |  2567 | `	}` |
|        - |  2568 |  |
|      474 |  2569 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      474 |  2570 | `	va_start(ap,zFormat);` |
|      474 |  2571 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      474 |  2572 | `	va_end(ap);` |
|        - |  2573 |  |
|      474 |  2574 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      474 |  2575 | `	if( pCons ){` |
|      474 |  2576 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      474 |  2577 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      474 |  2578 | `		apArg[0] = &sArg;` |
|      474 |  2579 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      474 |  2580 | `		PH7_MemObjRelease(&sArg);` |
|      236 |  2581 | `	}` |
|      474 |  2582 | `	SyBlobRelease(&sMsg);` |
|        - |  2583 |  |
|      474 |  2584 | `	pFrame = pVm->pFrame;` |
|      474 |  2585 | `	if( pFrame ){` |
|      474 |  2586 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      474 |  2587 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      236 |  2588 | `	}` |
|      474 |  2589 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      474 |  2590 | `	PH7_ClassInstanceUnref(pThis);` |
|      474 |  2591 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2592 | `		return PH7_ABORT;` |
|        - |  2593 | `	}` |
|       12 |  2594 | `	return PH7_EXCEPTION;` |
|      238 |  2595 |  |
|        - |  2596 | `/*` |
|        - |  2597 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2598 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2599 | ` */` |
|      ! 0 |  2600 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2601 |  |
|        - |  2602 | `	ph7_vm *pVm;` |
|        - |  2603 | `	SyBlob sMsg;` |
|      ! 0 |  2604 | `	const char *zFuncName = 0;` |
|      ! 0 |  2605 | `	int nFuncLen = 0;` |
|        - |  2606 | `	va_list ap;` |
|        - |  2607 | `	sxi32 rc;` |
|        - |  2608 |  |
|      ! 0 |  2609 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2610 | `		return PH7_OK;` |
|        - |  2611 | `	}` |
|      ! 0 |  2612 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2613 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2614 | `		zClass = "Error";` |
|      ! 0 |  2615 | `	}` |
|        - |  2616 |  |
|      ! 0 |  2617 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2618 |  |
|      ! 0 |  2619 | `	va_start(ap,zFormat);` |
|      ! 0 |  2620 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2621 | `	va_end(ap);` |
|        - |  2622 |  |
|      ! 0 |  2623 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2624 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2625 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2626 | `	}` |
|      ! 0 |  2627 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2628 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2629 | `	}` |
|      ! 0 |  2630 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2631 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2632 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2633 | `	return rc;` |
|      ! 0 |  2634 |  |
|        - |  2635 | `/*` |
|        - |  2636 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2637 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2638 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2639 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2640 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2641 | ` * when VmByteCodeExec returns.` |
|        - |  2642 | ` */` |
|       76 |  2643 | `static sxi32 VmSuspendCtx(` |
|        - |  2644 | `	ph7_vm *pVm,` |
|        - |  2645 | `	ph7_exec_ctx *pCtx,` |
|        - |  2646 | `	sxi32 pc,` |
|        - |  2647 | `	sxi32 nTos` |
|        - |  2648 | `	)` |
|        1 |  2649 |  |
|       38 |  2650 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|       77 |  2651 | `	pCtx->pc = pc;` |
|       77 |  2652 | `	pCtx->nTos = nTos;` |
|       77 |  2653 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|       77 |  2654 | `	return PH7_SUSPEND;` |
|        1 |  2655 |  |
|        - |  2656 | `/*` |
|        - |  2657 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2658 | ` *` |
|        - |  2659 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2660 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2661 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2662 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2663 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2664 | ` * then the program execution is halted.` |
|        - |  2665 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2666 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2667 | ` * or to reset the VM to it's initial state.` |
|        - |  2668 | ` */` |
|    32054 |  2669 | `static sxi32 VmByteCodeExec(` |
|        - |  2670 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2671 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2672 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2673 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2674 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2675 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2676 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2677 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2678 | `	)` |
|        2 |  2679 |  |
|        - |  2680 | `	VmInstr *pInstr;` |
|        - |  2681 | `	ph7_value *pTos;` |
|        - |  2682 | `	SySet aArg;` |
|        - |  2683 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2684 | `	sxi32 pc;` |
|        - |  2685 | `	sxi32 rc;` |
|        - |  2686 | `	/* Argument container */` |
|    32056 |  2687 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32056 |  2688 | `	if( nTos < 0 ){` |
|    30248 |  2689 | `		pTos = &pStack[-1];` |
|    15125 |  2690 | `	}else{` |
|     1810 |  2691 | `		pTos = &pStack[nTos];` |
|        - |  2692 | `	}` |
|    32056 |  2693 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32056 |  2694 | `	pc = nPc;` |
|        - |  2695 | `	/* Execute as much as we can */` |
|  4933033 |  2696 | `	for(;;){` |
|        - |  2697 | `		/* Fetch the instruction to execute */` |
|  9865364 |  2698 | `		pInstr = &aInstr[pc];` |
|  9865364 |  2699 | `		rc = SXRET_OK;` |
|        - |  2700 | `/*` |
|        - |  2701 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2702 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2703 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2704 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2705 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2706 | ` */` |
|  9865364 |  2707 | `		switch(pInstr->iOp){` |
|        - |  2708 | `/*` |
|        - |  2709 | ` * DONE: P1 * *` |
|        - |  2710 | ` *` |
|        - |  2711 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2712 | ` * and return immediately.` |
|        - |  2713 | ` */` |
|    15744 |  2714 | `case PH7_OP_DONE:` |
|    31490 |  2715 | `	if( pInstr->iP1 ){` |
|        - |  2716 | `#ifdef UNTRUST` |
|        - |  2717 | `		if( pTos < pStack ){` |
|        - |  2718 | `			goto Abort;` |
|        - |  2719 | `		}` |
|        - |  2720 | `#endif` |
|    18140 |  2721 | `		if( pLastRef ){` |
|    11856 |  2722 | `			*pLastRef = pTos->nIdx;` |
|     5927 |  2723 | `		}` |
|    18140 |  2724 | `		if( pResult ){` |
|        - |  2725 | `			/* Execution result */` |
|    17256 |  2726 | `			PH7_MemObjStore(pTos,pResult);` |
|     8627 |  2727 | `		}` |
|    18140 |  2728 | `		VmPopOperand(&pTos,1);` |
|    22421 |  2729 | `	}else if( pLastRef ){` |
|        - |  2730 | `		/* Nothing referenced */` |
|      988 |  2731 | `		*pLastRef = SXU32_HIGH;` |
|      493 |  2732 | `	}` |
|        - |  2733 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2734 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2735 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2736 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2737 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2738 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2739 | `	 * block can override it.` |
|        - |  2740 | `	 */` |
|    31492 |  2741 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2742 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2743 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2744 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2745 | `		pExc->pFrame = 0;` |
|        3 |  2746 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2747 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2748 | `			pExc->iFinallyDone = 1;` |
|        - |  2749 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2750 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2751 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2752 | `				goto Abort;` |
|        - |  2753 | `			}` |
|        1 |  2754 | `		}` |
|        1 |  2755 | `	}` |
|    31490 |  2756 | `	goto Done;` |
|        - |  2757 | `/*` |
|        - |  2758 | ` * HALT: P1 * *` |
|        - |  2759 | ` *` |
|        - |  2760 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2761 | ` * and abort immediately.` |
|        - |  2762 | ` */` |
|        4 |  2763 | `case PH7_OP_HALT:` |
|        9 |  2764 | `	if( pInstr->iP1 ){` |
|        - |  2765 | `#ifdef UNTRUST` |
|        - |  2766 | `		if( pTos < pStack ){` |
|        - |  2767 | `			goto Abort;` |
|        - |  2768 | `		}` |
|        - |  2769 | `#endif` |
|        9 |  2770 | `		if( pLastRef ){` |
|      ! 0 |  2771 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2772 | `		}` |
|        9 |  2773 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2774 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2775 | `				/* Output the exit message */` |
|        7 |  2776 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2777 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2778 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2779 | `			}` |
|        7 |  2780 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2781 | `			/* Record exit status */` |
|        5 |  2782 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2783 | `		}` |
|        9 |  2784 | `		VmPopOperand(&pTos,1);` |
|        4 |  2785 | `	}else if( pLastRef ){` |
|        - |  2786 | `		/* Nothing referenced */` |
|      ! 0 |  2787 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2788 | `	}` |
|        - |  2789 | `	/* Check if we're in an included file context */` |
|        9 |  2790 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2791 | `		/* Terminate the entire process */` |
|        9 |  2792 | `		exit(pVm->iExitStatus);` |
|        - |  2793 | `	}` |
|      ! 0 |  2794 | `	goto Abort;` |
|        - |  2795 | `/*` |
|        - |  2796 | ` * JMP: * P2 *` |
|        - |  2797 | ` *` |
|        - |  2798 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2799 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2800 | ` */` |
|   212664 |  2801 | `case PH7_OP_JMP:` |
|   425374 |  2802 | `	pc = pInstr->iP2 - 1;` |
|   425374 |  2803 | `	break;` |
|        - |  2804 | `/*` |
|        - |  2805 | ` * JZ: P1 P2 *` |
|        - |  2806 | ` *` |
|        - |  2807 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2808 | ` * entry in the stack if P1 is zero.` |
|        - |  2809 | ` */` |
|   496734 |  2810 | `case PH7_OP_JZ:` |
|        - |  2811 | `#ifdef UNTRUST` |
|        - |  2812 | `	if( pTos < pStack ){` |
|        - |  2813 | `		goto Abort;` |
|        - |  2814 | `	}` |
|        - |  2815 | `#endif` |
|        - |  2816 | `	/* Get a boolean value */` |
|   993558 |  2817 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2818 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2819 | `	}` |
|   993558 |  2820 | `	if( !pTos->x.iVal ){` |
|        - |  2821 | `		/* Take the jump */` |
|   501194 |  2822 | `		pc = pInstr->iP2 - 1;` |
|   250596 |  2823 | `	}` |
|   993558 |  2824 | `	if( !pInstr->iP1 ){` |
|   791770 |  2825 | `		VmPopOperand(&pTos,1);` |
|   395906 |  2826 | `	}` |
|   993558 |  2827 | `	break;` |
|        - |  2828 | `/*` |
|        - |  2829 | ` * JNZ: P1 P2 *` |
|        - |  2830 | ` *` |
|        - |  2831 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2832 | ` * entry in the stack if P1 is zero.` |
|        - |  2833 | ` */` |
|    53421 |  2834 | `case PH7_OP_JNZ:` |
|        - |  2835 | `#ifdef UNTRUST` |
|        - |  2836 | `	if( pTos < pStack ){` |
|        - |  2837 | `		goto Abort;` |
|        - |  2838 | `	}` |
|        - |  2839 | `#endif` |
|        - |  2840 | `	/* Get a boolean value */` |
|   106844 |  2841 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2842 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2843 | `	}` |
|   106844 |  2844 | `	if( pTos->x.iVal ){` |
|        - |  2845 | `		/* Take the jump */` |
|     4416 |  2846 | `		pc = pInstr->iP2 - 1;` |
|     2207 |  2847 | `	}` |
|   106844 |  2848 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2849 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2850 | `	}` |
|   106844 |  2851 | `	break;` |
|        - |  2852 | `/*` |
|        - |  2853 | ` * NOOP: * * *` |
|        - |  2854 | ` *` |
|        - |  2855 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2856 | ` * destination.` |
|        - |  2857 | ` */` |
|      ! 0 |  2858 | `case PH7_OP_NOOP:` |
|      ! 0 |  2859 | `	break;` |
|        - |  2860 | `/*` |
|        - |  2861 | ` * POP: P1 * *` |
|        - |  2862 | ` *` |
|        - |  2863 | ` * Pop P1 elements from the operand stack.` |
|        - |  2864 | ` */` |
|   388178 |  2865 | `case PH7_OP_POP: {` |
|   776402 |  2866 | `	sxi32 n = pInstr->iP1;` |
|   776402 |  2867 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2868 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2869 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2870 | `	}` |
|   776402 |  2871 | `	VmPopOperand(&pTos,n);` |
|   776402 |  2872 | `	break;` |
|        - |  2873 | `				 }` |
|        - |  2874 | `/*` |
|        - |  2875 | ` * DUP: * * *` |
|        - |  2876 | ` *` |
|        - |  2877 | ` * Duplicate the top of the stack.` |
|        - |  2878 | ` */` |
|       35 |  2879 | `case PH7_OP_DUP:` |
|        - |  2880 | `#ifdef UNTRUST` |
|        - |  2881 | `	if( pTos < pStack ){` |
|        - |  2882 | `		goto Abort;` |
|        - |  2883 | `	}` |
|        - |  2884 | `#endif` |
|       72 |  2885 | `	pTos++;` |
|       72 |  2886 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2887 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2888 | `	break;` |
|        - |  2889 | `/*` |
|        - |  2890 | ` * NSSWITCH: * * P3` |
|        - |  2891 | ` *` |
|        - |  2892 | ` * Switch the active namespace at runtime.` |
|        - |  2893 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2894 | ` */` |
|     6413 |  2895 | `case PH7_OP_NSSWITCH:` |
|    12828 |  2896 | `	SyBlobReset(&pVm->sNamespace);` |
|    12828 |  2897 | `	if( pInstr->p3 ){` |
|       51 |  2898 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2899 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2900 | `	}` |
|    12828 |  2901 | `	break;` |
|        - |  2902 | `/*` |
|        - |  2903 | ` * CVT_INT: * * *` |
|        - |  2904 | ` *` |
|        - |  2905 | ` * Force the top of the stack to be an integer.` |
|        - |  2906 | ` */` |
|       35 |  2907 | `case PH7_OP_CVT_INT:` |
|        - |  2908 | `#ifdef UNTRUST` |
|        - |  2909 | `	if( pTos < pStack ){` |
|        - |  2910 | `		goto Abort;` |
|        - |  2911 | `	}` |
|        - |  2912 | `#endif` |
|       72 |  2913 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2914 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2915 | `	}` |
|        - |  2916 | `	/* Invalidate any prior representation */` |
|       72 |  2917 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2918 | `	break;` |
|        - |  2919 | `/*` |
|        - |  2920 | ` * CVT_REAL: * * *` |
|        - |  2921 | ` *` |
|        - |  2922 | ` * Force the top of the stack to be a real.` |
|        - |  2923 | ` */` |
|        4 |  2924 | `case PH7_OP_CVT_REAL:` |
|        - |  2925 | `#ifdef UNTRUST` |
|        - |  2926 | `	if( pTos < pStack ){` |
|        - |  2927 | `		goto Abort;` |
|        - |  2928 | `	}` |
|        - |  2929 | `#endif` |
|        9 |  2930 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2931 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2932 | `	}` |
|        - |  2933 | `	/* Invalidate any prior representation */` |
|        9 |  2934 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2935 | `	break;` |
|        - |  2936 | `/*` |
|        - |  2937 | ` * CVT_STR: * * *` |
|        - |  2938 | ` *` |
|        - |  2939 | ` * Force the top of the stack to be a string.` |
|        - |  2940 | ` */` |
|      146 |  2941 | `case PH7_OP_CVT_STR:` |
|        - |  2942 | `#ifdef UNTRUST` |
|        - |  2943 | `	if( pTos < pStack ){` |
|        - |  2944 | `		goto Abort;` |
|        - |  2945 | `	}` |
|        - |  2946 | `#endif` |
|      294 |  2947 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2948 | `		PH7_MemObjToString(pTos);` |
|      146 |  2949 | `	}` |
|      294 |  2950 | `	break;` |
|        - |  2951 | `/*` |
|        - |  2952 | ` * CVT_BOOL: * * *` |
|        - |  2953 | ` *` |
|        - |  2954 | ` * Force the top of the stack to be a boolean.` |
|        - |  2955 | ` */` |
|        5 |  2956 | `case PH7_OP_CVT_BOOL:` |
|        - |  2957 | `#ifdef UNTRUST` |
|        - |  2958 | `	if( pTos < pStack ){` |
|        - |  2959 | `		goto Abort;` |
|        - |  2960 | `	}` |
|        - |  2961 | `#endif` |
|       11 |  2962 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2963 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2964 | `	}` |
|       11 |  2965 | `	break;` |
|        - |  2966 | `/*` |
|        - |  2967 | ` * CVT_NULL: * * *` |
|        - |  2968 | ` *` |
|        - |  2969 | ` * Nullify the top of the stack.` |
|        - |  2970 | ` */` |
|        3 |  2971 | `case PH7_OP_CVT_NULL:` |
|        - |  2972 | `#ifdef UNTRUST` |
|        - |  2973 | `	if( pTos < pStack ){` |
|        - |  2974 | `		goto Abort;` |
|        - |  2975 | `	}` |
|        - |  2976 | `#endif` |
|        7 |  2977 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2978 | `	break;` |
|        - |  2979 | `/*` |
|        - |  2980 | ` * CVT_NUMC: * * *` |
|        - |  2981 | ` *` |
|        - |  2982 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2983 | ` */` |
|      ! 0 |  2984 | `case PH7_OP_CVT_NUMC:` |
|        - |  2985 | `#ifdef UNTRUST` |
|        - |  2986 | `	if( pTos < pStack ){` |
|        - |  2987 | `		goto Abort;` |
|        - |  2988 | `	}` |
|        - |  2989 | `#endif` |
|        - |  2990 | `	/* Force a numeric cast */` |
|      ! 0 |  2991 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2992 | `	break;` |
|        - |  2993 | `/*` |
|        - |  2994 | ` * CVT_ARRAY: * * *` |
|        - |  2995 | ` *` |
|        - |  2996 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2997 | ` */` |
|       10 |  2998 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2999 | `#ifdef UNTRUST` |
|        - |  3000 | `	if( pTos < pStack ){` |
|        - |  3001 | `		goto Abort;` |
|        - |  3002 | `	}` |
|        - |  3003 | `#endif` |
|        - |  3004 | `	/* Force a hashmap cast */` |
|       21 |  3005 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3006 | `	if( rc != SXRET_OK ){` |
|        - |  3007 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3008 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3009 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3010 | `	}` |
|       21 |  3011 | `	break;` |
|        - |  3012 | `/*` |
|        - |  3013 | ` * CVT_OBJ: * * *` |
|        - |  3014 | ` *` |
|        - |  3015 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3016 | ` */` |
|        8 |  3017 | `case PH7_OP_CVT_OBJ:` |
|        - |  3018 | `#ifdef UNTRUST` |
|        - |  3019 | `	if( pTos < pStack ){` |
|        - |  3020 | `		goto Abort;` |
|        - |  3021 | `	}` |
|        - |  3022 | `#endif` |
|       17 |  3023 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3024 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3025 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3026 | `	}` |
|       17 |  3027 | `	break;` |
|        - |  3028 | `/*` |
|        - |  3029 | ` * ERR_CTRL * * *` |
|        - |  3030 | ` *` |
|        - |  3031 | ` * Error control operator.` |
|        - |  3032 | ` */` |
|    12639 |  3033 | `case PH7_OP_ERR_CTRL:` |
|        - |  3034 | `	/*` |
|        - |  3035 | `	 * TICKET 1433-038:` |
|        - |  3036 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3037 | `	 * use the public API,to control error output.` |
|        - |  3038 | `	 */` |
|    25278 |  3039 | `	break;` |
|        - |  3040 | `/*` |
|        - |  3041 | ` * IS_A * * *` |
|        - |  3042 | ` *` |
|        - |  3043 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3044 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3045 | ` * holding a class name or an object).` |
|        - |  3046 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3047 | ` */` |
|       23 |  3048 | `case PH7_OP_IS_A:{` |
|       48 |  3049 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3050 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3051 | `#ifdef UNTRUST` |
|        - |  3052 | `	if( pNos < pStack ){` |
|        - |  3053 | `		goto Abort;` |
|        - |  3054 | `	}` |
|        - |  3055 | `#endif` |
|       48 |  3056 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3057 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3058 | `		ph7_class *pClass = 0;` |
|        - |  3059 | `		/* Extract the target class */` |
|       46 |  3060 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3061 | `			/* Instance already loaded */` |
|      ! 0 |  3062 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3063 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3064 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3065 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3066 | `			/* Handle self/static/parent keywords */` |
|       46 |  3067 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3068 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3069 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3070 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3071 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3072 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3073 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3074 | `					pClass = pSelf->pBase;` |
|        2 |  3075 | `				}` |
|        3 |  3076 | `			}else{` |
|       36 |  3077 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3078 | `			}` |
|       22 |  3079 | `		}` |
|       46 |  3080 | `		if( pClass ){` |
|        - |  3081 | `			/* Perform the query */` |
|       46 |  3082 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3083 | `		}` |
|       22 |  3084 | `	}` |
|        - |  3085 | `	/* Push result */` |
|       48 |  3086 | `	VmPopOperand(&pTos,1);` |
|       48 |  3087 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3088 | `	pTos->x.iVal = iRes;` |
|       48 |  3089 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3090 | `	break;` |
|        - |  3091 | `				 }` |
|        - |  3092 |  |
|        - |  3093 | `/*` |
|        - |  3094 | ` * LOADC P1 P2 *` |
|        - |  3095 | ` *` |
|        - |  3096 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3097 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3098 | ` */` |
|   823084 |  3099 | `case PH7_OP_LOADC: {` |
|        - |  3100 | `	ph7_value *pObj;` |
|        - |  3101 | `	/* Reserve a room */` |
|  1646214 |  3102 | `	pTos++;` |
|  2461219 |  3103 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1646214 |  3104 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3105 | `			SyHashEntry *pEntry;` |
|        - |  3106 | `			/* Candidate for expansion via user defined callbacks */` |
|    16250 |  3107 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16250 |  3108 | `			if( pEntry ){` |
|    16246 |  3109 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3110 | `				/* Set a NULL default value */` |
|    16246 |  3111 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16246 |  3112 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3113 | `				/* Invoke the callback and deal with the expanded value */` |
|    16246 |  3114 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3115 | `				/* Mark as constant */` |
|    16246 |  3116 | `				pTos->nIdx = SXU32_HIGH;` |
|    16246 |  3117 | `				break;` |
|        - |  3118 | `			}` |
|        - |  3119 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3120 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3121 | `			 * through to string value for backward compatibility. */` |
|        - |  3122 | `			{` |
|        6 |  3123 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3124 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3125 | `				sxu32 j;` |
|       32 |  3126 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3127 | `					if( zLit[j] == '\\' ){` |
|        - |  3128 | `						/* Qualified name: must be a real constant.` |
|        - |  3129 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3130 | `						{` |
|        3 |  3131 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3132 | `							SyBlob sErr;` |
|        3 |  3133 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3134 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3135 | `							if( pErrFile ){` |
|        3 |  3136 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3137 | `							}` |
|        3 |  3138 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3139 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3140 | `							SyBlobRelease(&sErr);` |
|        - |  3141 | `						}` |
|        3 |  3142 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3143 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3144 | `						goto LoadC_Done;` |
|        - |  3145 | `					}` |
|       15 |  3146 | `				}` |
|        - |  3147 | `			}` |
|        1 |  3148 | `		}` |
|  1629968 |  3149 | `		PH7_MemObjLoad(pObj,pTos);` |
|   815007 |  3150 | `	}else{` |
|        - |  3151 | `		/* Set a NULL value */` |
|      ! 0 |  3152 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3153 | `	}` |
|   814962 |  3154 | `LoadC_Done:` |
|        - |  3155 | `	/* Mark as constant */` |
|  1629970 |  3156 | `	pTos->nIdx = SXU32_HIGH;` |
|  1629970 |  3157 | `	break;` |
|        - |  3158 | `				  }` |
|        - |  3159 | `/*` |
|        - |  3160 | ` * LOAD: P1 * P3` |
|        - |  3161 | ` *` |
|        - |  3162 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3163 | ` * from the P3 operand.` |
|        - |  3164 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3165 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3166 | ` */` |
|  1340213 |  3167 | `case PH7_OP_LOAD:{` |
|        - |  3168 | `	ph7_value *pObj;` |
|        - |  3169 | `	SyString sName;` |
|  2680648 |  3170 | `	if( pInstr->p3 == 0 ){` |
|        - |  3171 | `		/* Take the variable name from the top of the stack */` |
|        - |  3172 | `#ifdef UNTRUST` |
|        - |  3173 | `		if( pTos < pStack ){` |
|        - |  3174 | `			goto Abort;` |
|        - |  3175 | `		}` |
|        - |  3176 | `#endif` |
|        - |  3177 | `		/* Force a string cast */` |
|       19 |  3178 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3179 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3180 | `		}` |
|       19 |  3181 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3182 | `	}else{` |
|  2680630 |  3183 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3184 | `		/* Reserve a room for the target object */` |
|  2680630 |  3185 | `		pTos++;` |
|        - |  3186 | `	}` |
|        - |  3187 | `	/* Extract the requested memory object */` |
|  2680648 |  3188 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2680648 |  3189 | `	if( pObj == 0 ){` |
|       26 |  3190 | `		if( pInstr->iP1 ){` |
|        - |  3191 | `			/* Variable not found,load NULL */` |
|       26 |  3192 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3193 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3194 | `			}else{` |
|       26 |  3195 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3196 | `			}` |
|       26 |  3197 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1340227 |  3198 | `			break;` |
|      ! 0 |  3199 | `		}else{` |
|        - |  3200 | `			/* Fatal error */` |
|      ! 0 |  3201 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3202 | `			goto Abort;` |
|        - |  3203 | `		}` |
|        - |  3204 | `	}` |
|        - |  3205 | `	/* Load variable contents */` |
|  2680624 |  3206 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2680624 |  3207 | `	pTos->nIdx = pObj->nIdx;` |
|  2680624 |  3208 | `	break;` |
|        - |  3209 | `				   }` |
|        - |  3210 | `/*` |
|        - |  3211 | ` * LOAD_MAP P1 * *` |
|        - |  3212 | ` *` |
|        - |  3213 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3214 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3215 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3216 | ` */` |
|    18279 |  3217 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3218 | `	ph7_hashmap *pMap;` |
|        - |  3219 | `	/* Allocate a new hashmap instance */` |
|    36560 |  3220 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36560 |  3221 | `	if( pMap == 0 ){` |
|      ! 0 |  3222 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3223 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3224 | `		goto Abort;` |
|        - |  3225 | `	}` |
|    36560 |  3226 | `	if( pInstr->iP1 > 0 ){` |
|     2238 |  3227 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3228 | `		/* Perform the insertion */` |
|     6838 |  3229 | `		while( pEntry < pTos ){` |
|     4602 |  3230 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3231 | `				/* Insertion by reference */` |
|      142 |  3232 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3233 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3234 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3235 | `					);` |
|       48 |  3236 | `			}else{` |
|        - |  3237 | `				/* Standard insertion */` |
|     6761 |  3238 | `				PH7_HashmapInsert(pMap,` |
|     4506 |  3239 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2253 |  3240 | `					&pEntry[1]` |
|        - |  3241 | `				);` |
|        - |  3242 | `			}` |
|        - |  3243 | `			/* Next pair on the stack */` |
|     4602 |  3244 | `			pEntry += 2;` |
|        2 |  3245 | `		}` |
|        - |  3246 | `		/* Pop P1 elements */` |
|     2238 |  3247 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1118 |  3248 | `	}` |
|        - |  3249 | `	/* Push the hashmap */` |
|    36560 |  3250 | `	pTos++;` |
|    36560 |  3251 | `	pTos->nIdx = SXU32_HIGH;` |
|    36560 |  3252 | `	pTos->x.pOther = pMap;` |
|    36560 |  3253 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36560 |  3254 | `	break;` |
|        - |  3255 | `					  }` |
|        - |  3256 | `/*` |
|        - |  3257 | ` * LOAD_LIST: P1 * *` |
|        - |  3258 | ` *` |
|        - |  3259 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3260 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3261 | ` * Caveats:` |
|        - |  3262 | ` *  This implementation support only a single nesting level.` |
|        - |  3263 | ` */` |
|       26 |  3264 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3265 | `	ph7_value *pEntry;` |
|       54 |  3266 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3267 | `		/* Empty list,break immediately */` |
|      ! 0 |  3268 | `		break;` |
|        - |  3269 | `	}` |
|       54 |  3270 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3271 | `#ifdef UNTRUST` |
|        - |  3272 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3273 | `		goto Abort;` |
|        - |  3274 | `	}` |
|        - |  3275 | `#endif` |
|       54 |  3276 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3277 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3278 | `		ph7_hashmap_node *pNode;` |
|        - |  3279 | `		ph7_value sKey,*pObj;` |
|        - |  3280 | `		/* Start Copying */` |
|       50 |  3281 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3282 | `		while( pEntry <= pTos ){` |
|      106 |  3283 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3284 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3285 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3286 | `					if( rc == SXRET_OK ){` |
|        - |  3287 | `						/* Store node value */` |
|       98 |  3288 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3289 | `					}else{` |
|        - |  3290 | `						/* Nullify the variable */` |
|      ! 0 |  3291 | `						PH7_MemObjRelease(pObj);` |
|        - |  3292 | `					}` |
|       48 |  3293 | `				}` |
|       48 |  3294 | `			}` |
|      106 |  3295 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3296 | `			pEntry++;` |
|        2 |  3297 | `		}` |
|       24 |  3298 | `	}` |
|       54 |  3299 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3300 | `	break;` |
|        - |  3301 | `					   }` |
|        - |  3302 | `/*` |
|        - |  3303 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3304 | ` *` |
|        - |  3305 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3306 | ` * from the stack.` |
|        - |  3307 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3308 | ` * instead.` |
|        - |  3309 | ` */` |
|   215804 |  3310 | `case PH7_OP_LOAD_IDX: {` |
|   431654 |  3311 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   431654 |  3312 | `	ph7_hashmap *pMap = 0;` |
|        - |  3313 | `	ph7_value *pIdx;` |
|   431654 |  3314 | `	pIdx = 0;` |
|   431654 |  3315 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3316 | `		if( !pInstr->iP2){` |
|        - |  3317 | `			/* No available index,load NULL */` |
|      ! 0 |  3318 | `			if( pTos >= pStack ){` |
|      ! 0 |  3319 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3320 | `			}else{` |
|        - |  3321 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3322 | `				pTos++;` |
|      ! 0 |  3323 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3324 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3325 | `			}` |
|        - |  3326 | `			/* Emit a notice */` |
|      ! 0 |  3327 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3328 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3329 | `			break;` |
|        - |  3330 | `		}` |
|      ! 0 |  3331 | `	}else{` |
|   431654 |  3332 | `		pIdx = pTos;` |
|   431654 |  3333 | `		pTos--;` |
|        - |  3334 | `	}` |
|   431654 |  3335 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3336 | `		/* String access */` |
|   340922 |  3337 | `		if( pIdx ){` |
|        - |  3338 | `			sxu32 nOfft;` |
|   340922 |  3339 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3340 | `				/* Force an int cast */` |
|      ! 0 |  3341 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3342 | `			}` |
|   340922 |  3343 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340922 |  3344 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3345 | `				/* Invalid offset,load null */` |
|      ! 0 |  3346 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3347 | `			}else{` |
|   340922 |  3348 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340922 |  3349 | `				int c = zData[nOfft];` |
|   340922 |  3350 | `				PH7_MemObjRelease(pTos);` |
|   340922 |  3351 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340922 |  3352 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3353 | `			}` |
|   170484 |  3354 | `		}else{` |
|        - |  3355 | `			/* No available index,load NULL */` |
|      ! 0 |  3356 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3357 | `		}` |
|   340922 |  3358 | `		break;` |
|        - |  3359 | `	}` |
|    90734 |  3360 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3361 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3362 | `			ph7_value *pObj;` |
|      ! 0 |  3363 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3364 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3365 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3366 | `			}` |
|      ! 0 |  3367 | `		}` |
|      ! 0 |  3368 | `	}` |
|    90734 |  3369 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    90734 |  3370 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    90734 |  3371 | `		if( pInstr->iP2 ){` |
|        - |  3372 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3373 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3374 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3375 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3376 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3377 | `		}` |
|        - |  3378 | `		/* Point to the hashmap */` |
|    90734 |  3379 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    90734 |  3380 | `		if( pIdx ){` |
|        - |  3381 | `			/* Load the desired entry */` |
|    90734 |  3382 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    45366 |  3383 | `		}` |
|    90734 |  3384 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3385 | `			/* Create a new empty entry */` |
|      265 |  3386 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3387 | `			if( rc == SXRET_OK ){` |
|        - |  3388 | `				/* Point to the last inserted entry */` |
|      265 |  3389 | `				pNode = pMap->pLast;` |
|      132 |  3390 | `			}` |
|      132 |  3391 | `		}` |
|    45366 |  3392 | `	}` |
|    90734 |  3393 | `	if( pIdx ){` |
|    90734 |  3394 | `		PH7_MemObjRelease(pIdx);` |
|    45366 |  3395 | `	}` |
|    90734 |  3396 | `	if( rc == SXRET_OK ){` |
|        - |  3397 | `		/* Load entry contents */` |
|    41566 |  3398 | `		if( pMap->iRef < 2 ){` |
|        - |  3399 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3400 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3401 | `			 */` |
|       24 |  3402 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3403 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3404 | `		}else{` |
|    41544 |  3405 | `			pTos->nIdx = pNode->nValIdx;` |
|    41544 |  3406 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41544 |  3407 | `			PH7_HashmapUnref(pMap);` |
|        - |  3408 | `		}` |
|    20784 |  3409 | `	}else{` |
|        - |  3410 | `		/* No such entry,load NULL */` |
|    49170 |  3411 | `		PH7_MemObjRelease(pTos);` |
|    49170 |  3412 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3413 | `	}` |
|    90734 |  3414 | `	break;` |
|        - |  3415 | `					  }` |
|        - |  3416 | `/*` |
|        - |  3417 | ` * LOAD_CLOSURE * * P3` |
|        - |  3418 | ` *` |
|        - |  3419 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3420 | ` * name in the stack.` |
|        - |  3421 | ` */` |
|        4 |  3422 | `case PH7_OP_LOAD_CLOSURE:{` |
|       10 |  3423 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       10 |  3424 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3425 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3426 | `		ph7_vm_func *pClosure;` |
|        - |  3427 | `		char *zName;` |
|        - |  3428 | `		sxu32 mLen;` |
|        - |  3429 | `		sxu32 n;` |
|        - |  3430 | `		/* Create a new VM function */` |
|       10 |  3431 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3432 | `		/* Generate an unique closure name */` |
|       10 |  3433 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       10 |  3434 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3435 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3436 | `			goto Abort;` |
|        - |  3437 | `		}` |
|       10 |  3438 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       10 |  3439 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3440 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3441 | `		}` |
|        - |  3442 | `		/* Zero the stucture */` |
|       10 |  3443 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3444 | `		/* Perform a structure assignment on read-only items */` |
|       10 |  3445 | `		pClosure->aArgs = pFunc->aArgs;` |
|       10 |  3446 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       10 |  3447 | `		pClosure->aStatic = pFunc->aStatic;` |
|       10 |  3448 | `		pClosure->iFlags = pFunc->iFlags;` |
|       10 |  3449 | `		pClosure->pUserData = pFunc->pUserData;` |
|       10 |  3450 | `		pClosure->sSignature = pFunc->sSignature;` |
|       10 |  3451 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3452 | `		/* Register the closure */` |
|       10 |  3453 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3454 | `		/* Set up closure environment */` |
|       10 |  3455 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       10 |  3456 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       28 |  3457 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3458 | `			ph7_value *pValue;` |
|       20 |  3459 | `			pEnv = &aEnv[n];` |
|       20 |  3460 | `			sEnv.sName  = pEnv->sName;` |
|       20 |  3461 | `			sEnv.iFlags = pEnv->iFlags;` |
|       20 |  3462 | `			sEnv.nIdx = SXU32_HIGH;` |
|       20 |  3463 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       20 |  3464 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3465 | `				/* Pass by reference */` |
|      ! 0 |  3466 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3467 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3468 | `					);` |
|      ! 0 |  3469 | `			}` |
|        - |  3470 | `			/* Standard pass by value */` |
|       20 |  3471 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       20 |  3472 | `			if( pValue ){` |
|        - |  3473 | `				/* Copy imported value */` |
|       12 |  3474 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3475 | `			}` |
|        - |  3476 | `			/* Insert the imported variable */` |
|       20 |  3477 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       11 |  3478 | `		}` |
|        - |  3479 | `		/* Finally,load the closure name on the stack */` |
|       10 |  3480 | `		pTos++;` |
|       10 |  3481 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3482 | `	}` |
|       10 |  3483 | `	break;` |
|        - |  3484 | `						 }` |
|        - |  3485 | `/*` |
|        - |  3486 | ` * STORE * P2 P3` |
|        - |  3487 | ` *` |
|        - |  3488 | ` * Perform a store (Assignment) operation.` |
|        - |  3489 | ` */` |
|   112974 |  3490 | `case PH7_OP_STORE: {` |
|        - |  3491 | `	ph7_value *pObj;` |
|        - |  3492 | `	SyString sName;` |
|        - |  3493 | `#ifdef UNTRUST` |
|        - |  3494 | `	if( pTos < pStack ){` |
|        - |  3495 | `		goto Abort;` |
|        - |  3496 | `	}` |
|        - |  3497 | `#endif` |
|   225950 |  3498 | `	if( pInstr->iP2 ){` |
|        - |  3499 | `		sxu32 nIdx;` |
|        - |  3500 | `		/* Member store operation */` |
|     2954 |  3501 | `		nIdx = pTos->nIdx;` |
|     2954 |  3502 | `		VmPopOperand(&pTos,1);` |
|     2954 |  3503 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3504 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3505 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3506 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3507 | `		}else{` |
|        - |  3508 | `			/* Point to the desired memory object */` |
|     2950 |  3509 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2950 |  3510 | `			if( pObj ){` |
|        - |  3511 | `				/* Perform the store operation */` |
|     2950 |  3512 | `				PH7_MemObjStore(pTos,pObj);` |
|     1474 |  3513 | `			}` |
|        - |  3514 | `		}` |
|   114452 |  3515 | `		break;` |
|   222998 |  3516 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3517 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3518 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3519 | `			/* Force a string cast */` |
|      ! 0 |  3520 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3521 | `		}` |
|        7 |  3522 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3523 | `		pTos--;` |
|        - |  3524 | `#ifdef UNTRUST` |
|        - |  3525 | `		if( pTos < pStack  ){` |
|        - |  3526 | `			goto Abort;` |
|        - |  3527 | `		}` |
|        - |  3528 | `#endif` |
|        4 |  3529 | `	}else{` |
|   222992 |  3530 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3531 | `	}` |
|        - |  3532 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   222998 |  3533 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   222998 |  3534 | `	if( pObj == 0 ){` |
|      ! 0 |  3535 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3536 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3537 | `		goto Abort;` |
|        - |  3538 | `	}` |
|   222998 |  3539 | `	if( !pInstr->p3 ){` |
|        7 |  3540 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3541 | `	}` |
|        - |  3542 | `	/* Perform the store operation */` |
|   222998 |  3543 | `	PH7_MemObjStore(pTos,pObj);` |
|   222998 |  3544 | `	break;` |
|        - |  3545 | `				   }` |
|        - |  3546 | `/*` |
|        - |  3547 | ` * STORE_IDX:   P1 * P3` |
|        - |  3548 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3549 | ` *` |
|        - |  3550 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3551 | ` */` |
|    81530 |  3552 | `case PH7_OP_STORE_IDX:` |
|        - |  3553 | `case PH7_OP_STORE_IDX_REF: {` |
|   163062 |  3554 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3555 | `	ph7_value *pKey;` |
|        - |  3556 | `	sxu32 nIdx;` |
|   163062 |  3557 | `	if( pInstr->iP1 ){` |
|        - |  3558 | `		/* Key is next on stack */` |
|    57338 |  3559 | `		pKey = pTos;` |
|    57338 |  3560 | `		pTos--;` |
|    28670 |  3561 | `	}else{` |
|   105726 |  3562 | `		pKey = 0;` |
|        - |  3563 | `	}` |
|   163062 |  3564 | `	nIdx = pTos->nIdx;` |
|   163062 |  3565 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3566 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3567 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3568 | `		 * checking true sharing count, then re-add after separation. */` |
|   163010 |  3569 | `		if( nIdx != SXU32_HIGH ){` |
|   163010 |  3570 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   244514 |  3571 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   163010 |  3572 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3573 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3574 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3575 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3576 | `				 * refcounts if the backing array was already separated. */` |
|   163010 |  3577 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   163010 |  3578 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   163010 |  3579 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   163010 |  3580 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   163010 |  3581 | `					pTos->x.pOther = pMap;` |
|    81506 |  3582 | `				}else{` |
|        - |  3583 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3584 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3585 | `					pMap = pCur;` |
|        - |  3586 | `				}` |
|    81506 |  3587 | `			}else{` |
|      ! 0 |  3588 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3589 | `			}` |
|    81506 |  3590 | `		}else{` |
|      ! 0 |  3591 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3592 | `		}` |
|   163010 |  3593 | `		if( pMap->iRef < 2 ){` |
|        - |  3594 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3595 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3596 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3597 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3598 | `			pMap->iRef = 2;` |
|      ! 0 |  3599 | `		}` |
|    81506 |  3600 | `	}else{` |
|        - |  3601 | `		ph7_value *pObj;` |
|       53 |  3602 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3603 | `		if( pObj == 0 ){` |
|      ! 0 |  3604 | `			if( pKey ){` |
|      ! 0 |  3605 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3606 | `			}` |
|      ! 0 |  3607 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3608 | `			break;` |
|        - |  3609 | `		}` |
|        - |  3610 | `		/* Phase#1: Load the array */` |
|       53 |  3611 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3612 | `			VmPopOperand(&pTos,1);` |
|       53 |  3613 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3614 | `				/* Force a string cast */` |
|      ! 0 |  3615 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3616 | `			}` |
|       53 |  3617 | `			if( pKey == 0 ){` |
|        - |  3618 | `				/* Append string */` |
|        3 |  3619 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3620 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3621 | `				}` |
|        2 |  3622 | `			}else{` |
|        - |  3623 | `				sxu32 nOfft;` |
|       51 |  3624 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3625 | `					/* Force an int cast */` |
|       51 |  3626 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3627 | `				}` |
|       51 |  3628 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3629 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3630 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3631 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3632 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3633 | `				}else{` |
|      ! 0 |  3634 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3635 | `						/* Perform an append operation */` |
|      ! 0 |  3636 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3637 | `					}` |
|        - |  3638 | `				}` |
|        - |  3639 | `			}` |
|       53 |  3640 | `			if( pKey ){` |
|       51 |  3641 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3642 | `			}` |
|       53 |  3643 | `			break;` |
|      ! 0 |  3644 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3645 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3646 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3647 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3648 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3649 | `				goto Abort;` |
|        - |  3650 | `			}` |
|      ! 0 |  3651 | `		}` |
|        - |  3652 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3653 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3654 | `	}` |
|   163010 |  3655 | `	VmPopOperand(&pTos,1);` |
|        - |  3656 | `	/* Phase#2: Perform the insertion */` |
|   163010 |  3657 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3658 | `		/* Insertion by reference */` |
|       15 |  3659 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3660 | `	}else{` |
|   162996 |  3661 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3662 | `	}` |
|   163010 |  3663 | `	if( pKey ){` |
|    57288 |  3664 | `		PH7_MemObjRelease(pKey);` |
|    28643 |  3665 | `	}` |
|   163010 |  3666 | `	break;` |
|        - |  3667 | `					   }` |
|        - |  3668 | `/*` |
|        - |  3669 | ` * INCR: P1 * *` |
|        - |  3670 | ` *` |
|        - |  3671 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3672 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3673 | ` * the stack and increment after that.` |
|        - |  3674 | ` */` |
|   151360 |  3675 | `case PH7_OP_INCR:` |
|        - |  3676 | `#ifdef UNTRUST` |
|        - |  3677 | `	if( pTos < pStack ){` |
|        - |  3678 | `		goto Abort;` |
|        - |  3679 | `	}` |
|        - |  3680 | `#endif` |
|   302766 |  3681 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302766 |  3682 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3683 | `			ph7_value *pObj;` |
|   302766 |  3684 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3685 | `				/* Force a numeric cast */` |
|   302766 |  3686 | `				PH7_MemObjToNumeric(pObj);` |
|   302766 |  3687 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3688 | `					pObj->rVal++;` |
|        - |  3689 | `					/* Try to get an integer representation */` |
|      ! 0 |  3690 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3691 | `				}else{` |
|   302766 |  3692 | `					pObj->x.iVal++;` |
|   302766 |  3693 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3694 | `				}` |
|   302766 |  3695 | `				if( pInstr->iP1 ){` |
|        - |  3696 | `					/* Pre-icrement */` |
|       71 |  3697 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3698 | `				}` |
|   151404 |  3699 | `			}` |
|   151406 |  3700 | `		}else{` |
|      ! 0 |  3701 | `			if( pInstr->iP1 ){` |
|        - |  3702 | `				/* Force a numeric cast */` |
|      ! 0 |  3703 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3704 | `				/* Pre-increment */` |
|      ! 0 |  3705 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3706 | `					pTos->rVal++;` |
|        - |  3707 | `					/* Try to get an integer representation */` |
|      ! 0 |  3708 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3709 | `				}else{` |
|      ! 0 |  3710 | `					pTos->x.iVal++;` |
|      ! 0 |  3711 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3712 | `				}` |
|      ! 0 |  3713 | `			}` |
|        - |  3714 | `		}` |
|   151404 |  3715 | `	}` |
|   302766 |  3716 | `	break;` |
|        - |  3717 | `/*` |
|        - |  3718 | ` * DECR: P1 * *` |
|        - |  3719 | ` *` |
|        - |  3720 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3721 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3722 | ` * and decrement after that.` |
|        - |  3723 | ` */` |
|        2 |  3724 | `case PH7_OP_DECR:` |
|        - |  3725 | `#ifdef UNTRUST` |
|        - |  3726 | `	if( pTos < pStack ){` |
|        - |  3727 | `		goto Abort;` |
|        - |  3728 | `	}` |
|        - |  3729 | `#endif` |
|        5 |  3730 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3731 | `		/* Force a numeric cast */` |
|        5 |  3732 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3733 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3734 | `			ph7_value *pObj;` |
|        5 |  3735 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3736 | `				/* Force a numeric cast */` |
|        5 |  3737 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3738 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3739 | `					pObj->rVal--;` |
|        - |  3740 | `					/* Try to get an integer representation */` |
|      ! 0 |  3741 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3742 | `				}else{` |
|        5 |  3743 | `					pObj->x.iVal--;` |
|        5 |  3744 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3745 | `				}` |
|        5 |  3746 | `				if( pInstr->iP1 ){` |
|        - |  3747 | `					/* Pre-icrement */` |
|      ! 0 |  3748 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3749 | `				}` |
|        2 |  3750 | `			}` |
|        3 |  3751 | `		}else{` |
|      ! 0 |  3752 | `			if( pInstr->iP1 ){` |
|        - |  3753 | `				/* Pre-increment */` |
|      ! 0 |  3754 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3755 | `					pTos->rVal--;` |
|        - |  3756 | `					/* Try to get an integer representation */` |
|      ! 0 |  3757 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3758 | `				}else{` |
|      ! 0 |  3759 | `					pTos->x.iVal--;` |
|      ! 0 |  3760 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3761 | `				}` |
|      ! 0 |  3762 | `			}` |
|        - |  3763 | `		}` |
|        2 |  3764 | `	}` |
|        5 |  3765 | `	break;` |
|        - |  3766 | `/*` |
|        - |  3767 | ` * UMINUS: * * *` |
|        - |  3768 | ` *` |
|        - |  3769 | ` * Perform a unary minus operation.` |
|        - |  3770 | ` */` |
|    23641 |  3771 | `case PH7_OP_UMINUS:` |
|        - |  3772 | `#ifdef UNTRUST` |
|        - |  3773 | `	if( pTos < pStack ){` |
|        - |  3774 | `		goto Abort;` |
|        - |  3775 | `	}` |
|        - |  3776 | `#endif` |
|        - |  3777 | `	/* Force a numeric (integer,real or both) cast */` |
|    47284 |  3778 | `	PH7_MemObjToNumeric(pTos);` |
|    47284 |  3779 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3780 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3781 | `	}` |
|    47284 |  3782 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    47254 |  3783 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23626 |  3784 | `	}` |
|    47284 |  3785 | `	break;` |
|        - |  3786 | `/*` |
|        - |  3787 | ` * UPLUS: * * *` |
|        - |  3788 | ` *` |
|        - |  3789 | ` * Perform a unary plus operation.` |
|        - |  3790 | ` */` |
|       16 |  3791 | `case PH7_OP_UPLUS:` |
|        - |  3792 | `#ifdef UNTRUST` |
|        - |  3793 | `	if( pTos < pStack ){` |
|        - |  3794 | `		goto Abort;` |
|        - |  3795 | `	}` |
|        - |  3796 | `#endif` |
|        - |  3797 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3798 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3799 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3800 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3801 | `	}` |
|       33 |  3802 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3803 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3804 | `	}` |
|       33 |  3805 | `	break;` |
|        - |  3806 | `/*` |
|        - |  3807 | ` * OP_LNOT: * * *` |
|        - |  3808 | ` *` |
|        - |  3809 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3810 | ` * with its complement.` |
|        - |  3811 | ` */` |
|    39943 |  3812 | `case PH7_OP_LNOT:` |
|        - |  3813 | `#ifdef UNTRUST` |
|        - |  3814 | `	if( pTos < pStack ){` |
|        - |  3815 | `		goto Abort;` |
|        - |  3816 | `	}` |
|        - |  3817 | `#endif` |
|        - |  3818 | `	/* Force a boolean cast */` |
|    79932 |  3819 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3820 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3821 | `	}` |
|    79932 |  3822 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    79932 |  3823 | `	break;` |
|        - |  3824 | `/*` |
|        - |  3825 | ` * OP_BITNOT: * * *` |
|        - |  3826 | ` *` |
|        - |  3827 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3828 | ` * with its ones-complement.` |
|        - |  3829 | ` */` |
|       14 |  3830 | `case PH7_OP_BITNOT:` |
|        - |  3831 | `#ifdef UNTRUST` |
|        - |  3832 | `	if( pTos < pStack ){` |
|        - |  3833 | `		goto Abort;` |
|        - |  3834 | `	}` |
|        - |  3835 | `#endif` |
|        - |  3836 | `	/* Force an integer cast */` |
|       30 |  3837 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3838 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3839 | `	}` |
|       30 |  3840 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3841 | `	break;` |
|        - |  3842 | `/* OP_MUL * * *` |
|        - |  3843 | ` * OP_MUL_STORE * * *` |
|        - |  3844 | ` *` |
|        - |  3845 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3846 | ` * and push the result back onto the stack.` |
|        - |  3847 | ` */` |
|     1243 |  3848 | `case PH7_OP_MUL:` |
|        - |  3849 | `case PH7_OP_MUL_STORE: {` |
|     2488 |  3850 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3851 | `	/* Force the operand to be numeric */` |
|        - |  3852 | `#ifdef UNTRUST` |
|        - |  3853 | `	if( pNos < pStack ){` |
|        - |  3854 | `		goto Abort;` |
|        - |  3855 | `	}` |
|        - |  3856 | `#endif` |
|     2488 |  3857 | `	PH7_MemObjToNumeric(pTos);` |
|     2488 |  3858 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3859 | `	/* Perform the requested operation */` |
|     2488 |  3860 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3861 | `		/* Floating point arithemic */` |
|        - |  3862 | `		ph7_real a,b,r;` |
|       17 |  3863 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3864 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3865 | `		}` |
|       17 |  3866 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3867 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3868 | `		}` |
|       17 |  3869 | `		a = pNos->rVal;` |
|       17 |  3870 | `		b = pTos->rVal;` |
|       17 |  3871 | `		r = a * b;` |
|        - |  3872 | `		/* Push the result */` |
|       17 |  3873 | `		pNos->rVal = r;` |
|       17 |  3874 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3875 | `		/* Try to get an integer representation */` |
|       17 |  3876 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3877 | `	}else{` |
|        - |  3878 | `		/* Integer arithmetic */` |
|        - |  3879 | `		sxi64 a,b,r;` |
|     2472 |  3880 | `		a = pNos->x.iVal;` |
|     2472 |  3881 | `		b = pTos->x.iVal;` |
|     2472 |  3882 | `		r = a * b;` |
|        - |  3883 | `		/* Push the result */` |
|     2472 |  3884 | `		pNos->x.iVal = r;` |
|     2472 |  3885 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3886 | `	}` |
|     2488 |  3887 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3888 | `		ph7_value *pObj;` |
|       25 |  3889 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3890 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3891 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3892 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3893 | `		}` |
|       12 |  3894 | `	}` |
|     2488 |  3895 | `	VmPopOperand(&pTos,1);` |
|     2488 |  3896 | `	break;` |
|        - |  3897 | `				 }` |
|        - |  3898 | `/* OP_ADD * * *` |
|        - |  3899 | ` *` |
|        - |  3900 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3901 | ` * and push the result back onto the stack.` |
|        - |  3902 | ` */` |
|      439 |  3903 | `case PH7_OP_ADD:{` |
|      880 |  3904 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3905 | `#ifdef UNTRUST` |
|        - |  3906 | `	if( pNos < pStack ){` |
|        - |  3907 | `		goto Abort;` |
|        - |  3908 | `	}` |
|        - |  3909 | `#endif` |
|        - |  3910 | `	/* Perform the addition */` |
|      880 |  3911 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      880 |  3912 | `	VmPopOperand(&pTos,1);` |
|      880 |  3913 | `	break;` |
|        - |  3914 | `				}` |
|        - |  3915 | `/*` |
|        - |  3916 | ` * OP_ADD_STORE * * *` |
|        - |  3917 | ` *` |
|        - |  3918 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3919 | ` * and push the result back onto the stack.` |
|        - |  3920 | ` */` |
|      483 |  3921 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3922 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3923 | `	ph7_value *pObj;` |
|        - |  3924 | `	sxu32 nIdx;` |
|        - |  3925 | `#ifdef UNTRUST` |
|        - |  3926 | `	if( pNos < pStack ){` |
|        - |  3927 | `		goto Abort;` |
|        - |  3928 | `	}` |
|        - |  3929 | `#endif` |
|        - |  3930 | `	/* Perform the addition */` |
|      968 |  3931 | `	nIdx = pTos->nIdx;` |
|      968 |  3932 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3933 | `	/* Peform the store operation */` |
|      968 |  3934 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3935 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3936 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3937 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3938 | `	}` |
|        - |  3939 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3940 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3941 | `	VmPopOperand(&pTos,1);` |
|      968 |  3942 | `	break;` |
|        - |  3943 | `				}` |
|        - |  3944 | `/* OP_SUB * * *` |
|        - |  3945 | ` *` |
|        - |  3946 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3947 | ` * first (what was next on the stack) from the second (the` |
|        - |  3948 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3949 | ` */` |
|      299 |  3950 | `case PH7_OP_SUB: {` |
|      600 |  3951 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3952 | `#ifdef UNTRUST` |
|        - |  3953 | `	if( pNos < pStack ){` |
|        - |  3954 | `		goto Abort;` |
|        - |  3955 | `	}` |
|        - |  3956 | `#endif` |
|      600 |  3957 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3958 | `		/* Floating point arithemic */` |
|        - |  3959 | `		ph7_real a,b,r;` |
|       95 |  3960 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3961 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3962 | `		}` |
|       95 |  3963 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3964 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3965 | `		}` |
|       95 |  3966 | `		a = pNos->rVal;` |
|       95 |  3967 | `		b = pTos->rVal;` |
|       95 |  3968 | `		r = a - b;` |
|        - |  3969 | `		/* Push the result */` |
|       95 |  3970 | `		pNos->rVal = r;` |
|       95 |  3971 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3972 | `		/* Try to get an integer representation */` |
|       95 |  3973 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3974 | `	}else{` |
|        - |  3975 | `		/* Integer arithmetic */` |
|        - |  3976 | `		sxi64 a,b,r;` |
|      506 |  3977 | `		a = pNos->x.iVal;` |
|      506 |  3978 | `		b = pTos->x.iVal;` |
|      506 |  3979 | `		r = a - b;` |
|        - |  3980 | `		/* Push the result */` |
|      506 |  3981 | `		pNos->x.iVal = r;` |
|      506 |  3982 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3983 | `	}` |
|      600 |  3984 | `	VmPopOperand(&pTos,1);` |
|      600 |  3985 | `	break;` |
|        - |  3986 | `				 }` |
|        - |  3987 | `/* OP_SUB_STORE * * *` |
|        - |  3988 | ` *` |
|        - |  3989 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3990 | ` * first (what was next on the stack) from the second (the` |
|        - |  3991 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3992 | ` */` |
|        1 |  3993 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3994 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3995 | `	ph7_value *pObj;` |
|        - |  3996 | `#ifdef UNTRUST` |
|        - |  3997 | `	if( pNos < pStack ){` |
|        - |  3998 | `		goto Abort;` |
|        - |  3999 | `	}` |
|        - |  4000 | `#endif` |
|        3 |  4001 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4002 | `		/* Floating point arithemic */` |
|        - |  4003 | `		ph7_real a,b,r;` |
|      ! 0 |  4004 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4005 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4006 | `		}` |
|      ! 0 |  4007 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4008 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4009 | `		}` |
|      ! 0 |  4010 | `		a = pTos->rVal;` |
|      ! 0 |  4011 | `		b = pNos->rVal;` |
|      ! 0 |  4012 | `		r = a - b;` |
|        - |  4013 | `		/* Push the result */` |
|      ! 0 |  4014 | `		pNos->rVal = r;` |
|      ! 0 |  4015 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4016 | `		/* Try to get an integer representation */` |
|      ! 0 |  4017 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4018 | `	}else{` |
|        - |  4019 | `		/* Integer arithmetic */` |
|        - |  4020 | `		sxi64 a,b,r;` |
|        3 |  4021 | `		a = pTos->x.iVal;` |
|        3 |  4022 | `		b = pNos->x.iVal;` |
|        3 |  4023 | `		r = a - b;` |
|        - |  4024 | `		/* Push the result */` |
|        3 |  4025 | `		pNos->x.iVal = r;` |
|        3 |  4026 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4027 | `	}` |
|        3 |  4028 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4029 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4030 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4031 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4032 | `	}` |
|        3 |  4033 | `	VmPopOperand(&pTos,1);` |
|        3 |  4034 | `	break;` |
|        - |  4035 | `				 }` |
|        - |  4036 |  |
|        - |  4037 | `/*` |
|        - |  4038 | ` * OP_MOD * * *` |
|        - |  4039 | ` *` |
|        - |  4040 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4041 | ` * first (what was next on the stack) from the second (the` |
|        - |  4042 | ` * top of the stack) and push the remainder after division` |
|        - |  4043 | ` * onto the stack.` |
|        - |  4044 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4045 | ` */` |
|      305 |  4046 | `case PH7_OP_MOD:{` |
|      612 |  4047 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4048 | `	sxi64 a,b,r;` |
|        - |  4049 | `#ifdef UNTRUST` |
|        - |  4050 | `	if( pNos < pStack ){` |
|        - |  4051 | `		goto Abort;` |
|        - |  4052 | `	}` |
|        - |  4053 | `#endif` |
|        - |  4054 | `	/* Force the operands to be integer */` |
|      612 |  4055 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4056 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4057 | `	}` |
|      612 |  4058 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4059 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4060 | `	}` |
|        - |  4061 | `	/* Perform the requested operation */` |
|      612 |  4062 | `	a = pNos->x.iVal;` |
|      612 |  4063 | `	b = pTos->x.iVal;` |
|      612 |  4064 | `	if( b == 0 ){` |
|        3 |  4065 | `		r = 0;` |
|        3 |  4066 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4067 | `		/* goto Abort; */` |
|        2 |  4068 | `	}else{` |
|      609 |  4069 | `		r = a%b;` |
|        - |  4070 | `	}` |
|        - |  4071 | `	/* Push the result */` |
|      612 |  4072 | `	pNos->x.iVal = r;` |
|      612 |  4073 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      612 |  4074 | `	VmPopOperand(&pTos,1);` |
|      612 |  4075 | `	break;` |
|        - |  4076 | `				}` |
|        - |  4077 | `/*` |
|        - |  4078 | ` * OP_MOD_STORE * * *` |
|        - |  4079 | ` *` |
|        - |  4080 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4081 | ` * first (what was next on the stack) from the second (the` |
|        - |  4082 | ` * top of the stack) and push the remainder after division` |
|        - |  4083 | ` * onto the stack.` |
|        - |  4084 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4085 | ` */` |
|        1 |  4086 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4087 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4088 | `	ph7_value *pObj;` |
|        - |  4089 | `	sxi64 a,b,r;` |
|        - |  4090 | `#ifdef UNTRUST` |
|        - |  4091 | `	if( pNos < pStack ){` |
|        - |  4092 | `		goto Abort;` |
|        - |  4093 | `	}` |
|        - |  4094 | `#endif` |
|        - |  4095 | `	/* Force the operands to be integer */` |
|        3 |  4096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4098 | `	}` |
|        3 |  4099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4101 | `	}` |
|        - |  4102 | `	/* Perform the requested operation */` |
|        3 |  4103 | `	a = pTos->x.iVal;` |
|        3 |  4104 | `	b = pNos->x.iVal;` |
|        3 |  4105 | `	if( b == 0 ){` |
|      ! 0 |  4106 | `		r = 0;` |
|      ! 0 |  4107 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4108 | `		/* goto Abort; */` |
|      ! 0 |  4109 | `	}else{` |
|        3 |  4110 | `		r = a%b;` |
|        - |  4111 | `	}` |
|        - |  4112 | `	/* Push the result */` |
|        3 |  4113 | `	pNos->x.iVal = r;` |
|        3 |  4114 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4115 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4116 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4117 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4118 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4119 | `	}` |
|        3 |  4120 | `	VmPopOperand(&pTos,1);` |
|        3 |  4121 | `	break;` |
|        - |  4122 | `				}` |
|        - |  4123 | `/*` |
|        - |  4124 | ` * OP_DIV * * *` |
|        - |  4125 | ` *` |
|        - |  4126 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4127 | ` * first (what was next on the stack) from the second (the` |
|        - |  4128 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4129 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4130 | ` */` |
|       28 |  4131 | `case PH7_OP_DIV:{` |
|       58 |  4132 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4133 | `	ph7_real a,b,r;` |
|        - |  4134 | `#ifdef UNTRUST` |
|        - |  4135 | `	if( pNos < pStack ){` |
|        - |  4136 | `		goto Abort;` |
|        - |  4137 | `	}` |
|        - |  4138 | `#endif` |
|        - |  4139 | `	/* Force the operands to be real */` |
|       58 |  4140 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4141 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4142 | `	}` |
|       58 |  4143 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4144 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4145 | `	}` |
|        - |  4146 | `	/* Perform the requested operation */` |
|       58 |  4147 | `	a = pNos->rVal;` |
|       58 |  4148 | `	b = pTos->rVal;` |
|       58 |  4149 | `	if( b == 0 ){` |
|        - |  4150 | `		/* Division by zero */` |
|        3 |  4151 | `		pNos->rVal = 0;` |
|        3 |  4152 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4153 | `		/* goto Abort; */` |
|        2 |  4154 | `	}else{` |
|       55 |  4155 | `		r = a/b;` |
|        - |  4156 | `		/* Push the result */` |
|       55 |  4157 | `		pNos->rVal = r;` |
|       55 |  4158 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4159 | `		/* Try to get an integer representation */` |
|       55 |  4160 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4161 | `	}` |
|       58 |  4162 | `	VmPopOperand(&pTos,1);` |
|       58 |  4163 | `	break;` |
|        - |  4164 | `				}` |
|        - |  4165 | `/*` |
|        - |  4166 | ` * OP_DIV_STORE * * *` |
|        - |  4167 | ` *` |
|        - |  4168 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4169 | ` * first (what was next on the stack) from the second (the` |
|        - |  4170 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4171 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4172 | ` */` |
|        1 |  4173 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4174 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4175 | `	ph7_value *pObj;` |
|        - |  4176 | `	ph7_real a,b,r;` |
|        - |  4177 | `#ifdef UNTRUST` |
|        - |  4178 | `	if( pNos < pStack ){` |
|        - |  4179 | `		goto Abort;` |
|        - |  4180 | `	}` |
|        - |  4181 | `#endif` |
|        - |  4182 | `	/* Force the operands to be real */` |
|        3 |  4183 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4184 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4185 | `	}` |
|        3 |  4186 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4187 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4188 | `	}` |
|        - |  4189 | `	/* Perform the requested operation */` |
|        3 |  4190 | `	a = pTos->rVal;` |
|        3 |  4191 | `	b = pNos->rVal;` |
|        3 |  4192 | `	if( b == 0 ){` |
|        - |  4193 | `		/* Division by zero */` |
|      ! 0 |  4194 | `		r = 0;` |
|      ! 0 |  4195 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4196 | `		/* goto Abort; */` |
|      ! 0 |  4197 | `	}else{` |
|        3 |  4198 | `		r = a/b;` |
|        - |  4199 | `		/* Push the result */` |
|        3 |  4200 | `		pNos->rVal = r;` |
|        3 |  4201 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4202 | `		/* Try to get an integer representation */` |
|        3 |  4203 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4204 | `	}` |
|        3 |  4205 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4206 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4207 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4208 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4209 | `	}` |
|        3 |  4210 | `	VmPopOperand(&pTos,1);` |
|        3 |  4211 | `	break;` |
|        - |  4212 | `				}` |
|        - |  4213 | `/* OP_BAND * * *` |
|        - |  4214 | ` *` |
|        - |  4215 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4216 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4217 | ` * two elements.` |
|        - |  4218 | `*/` |
|        - |  4219 | `/* OP_BOR * * *` |
|        - |  4220 | ` *` |
|        - |  4221 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4222 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4223 | ` * two elements.` |
|        - |  4224 | ` */` |
|        - |  4225 | `/* OP_BXOR * * *` |
|        - |  4226 | ` *` |
|        - |  4227 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4228 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4229 | ` * two elements.` |
|        - |  4230 | ` */` |
|       30 |  4231 | `case PH7_OP_BAND:` |
|        - |  4232 | `case PH7_OP_BOR:` |
|        - |  4233 | `case PH7_OP_BXOR:{` |
|       62 |  4234 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4235 | `	sxi64 a,b,r;` |
|        - |  4236 | `#ifdef UNTRUST` |
|        - |  4237 | `	if( pNos < pStack ){` |
|        - |  4238 | `		goto Abort;` |
|        - |  4239 | `	}` |
|        - |  4240 | `#endif` |
|        - |  4241 | `	/* Force the operands to be integer */` |
|       62 |  4242 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4243 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4244 | `	}` |
|       62 |  4245 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4246 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4247 | `	}` |
|        - |  4248 | `	/* Perform the requested operation */` |
|       62 |  4249 | `	a = pNos->x.iVal;` |
|       62 |  4250 | `	b = pTos->x.iVal;` |
|       62 |  4251 | `	switch(pInstr->iOp){` |
|        6 |  4252 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4253 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4254 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4255 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4256 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4257 | `	case PH7_OP_BAND:` |
|       38 |  4258 | `	default:          r = a&b; break;` |
|        - |  4259 | `	}` |
|        - |  4260 | `	/* Push the result */` |
|       62 |  4261 | `	pNos->x.iVal = r;` |
|       62 |  4262 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4263 | `	VmPopOperand(&pTos,1);` |
|       62 |  4264 | `	break;` |
|        - |  4265 | `				 }` |
|        - |  4266 | `/* OP_BAND_STORE * * *` |
|        - |  4267 | ` *` |
|        - |  4268 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4269 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4270 | ` * two elements.` |
|        - |  4271 | `*/` |
|        - |  4272 | `/* OP_BOR_STORE * * *` |
|        - |  4273 | ` *` |
|        - |  4274 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4275 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4276 | ` * two elements.` |
|        - |  4277 | ` */` |
|        - |  4278 | `/* OP_BXOR_STORE * * *` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4281 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4282 | ` * two elements.` |
|        - |  4283 | ` */` |
|        7 |  4284 | `case PH7_OP_BAND_STORE:` |
|        - |  4285 | `case PH7_OP_BOR_STORE:` |
|        - |  4286 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4287 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4288 | `	ph7_value *pObj;` |
|        - |  4289 | `	sxi64 a,b,r;` |
|        - |  4290 | `#ifdef UNTRUST` |
|        - |  4291 | `	if( pNos < pStack ){` |
|        - |  4292 | `		goto Abort;` |
|        - |  4293 | `	}` |
|        - |  4294 | `#endif` |
|        - |  4295 | `	/* Force the operands to be integer */` |
|       15 |  4296 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4297 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4298 | `	}` |
|       15 |  4299 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4300 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4301 | `	}` |
|        - |  4302 | `	/* Perform the requested operation */` |
|       15 |  4303 | `	a = pTos->x.iVal;` |
|       15 |  4304 | `	b = pNos->x.iVal;` |
|       15 |  4305 | `	switch(pInstr->iOp){` |
|        2 |  4306 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4307 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4308 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4309 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4310 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4311 | `	case PH7_OP_BAND:` |
|        5 |  4312 | `	default:          r = a&b; break;` |
|        - |  4313 | `	}` |
|        - |  4314 | `	/* Push the result */` |
|       15 |  4315 | `	pNos->x.iVal = r;` |
|       15 |  4316 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4317 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4318 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4319 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4320 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4321 | `	}` |
|       15 |  4322 | `	VmPopOperand(&pTos,1);` |
|       15 |  4323 | `	break;` |
|        - |  4324 | `				 }` |
|        - |  4325 | `/* OP_SHL * * *` |
|        - |  4326 | ` *` |
|        - |  4327 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4328 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4329 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4330 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4331 | ` */` |
|        - |  4332 | `/* OP_SHR * * *` |
|        - |  4333 | ` *` |
|        - |  4334 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4335 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4336 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4337 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4338 | ` */` |
|        9 |  4339 | `case PH7_OP_SHL:` |
|        - |  4340 | `case PH7_OP_SHR: {` |
|       19 |  4341 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4342 | `	sxi64 a,r;` |
|        - |  4343 | `	sxi32 b;` |
|        - |  4344 | `#ifdef UNTRUST` |
|        - |  4345 | `	if( pNos < pStack ){` |
|        - |  4346 | `		goto Abort;` |
|        - |  4347 | `	}` |
|        - |  4348 | `#endif` |
|        - |  4349 | `	/* Force the operands to be integer */` |
|       19 |  4350 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4351 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4352 | `	}` |
|       19 |  4353 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4354 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4355 | `	}` |
|        - |  4356 | `	/* Perform the requested operation */` |
|       19 |  4357 | `	a = pNos->x.iVal;` |
|       19 |  4358 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4359 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4360 | `		r = a << b;` |
|        6 |  4361 | `	}else{` |
|        9 |  4362 | `		r = a >> b;` |
|        - |  4363 | `	}` |
|        - |  4364 | `	/* Push the result */` |
|       19 |  4365 | `	pNos->x.iVal = r;` |
|       19 |  4366 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4367 | `	VmPopOperand(&pTos,1);` |
|       19 |  4368 | `	break;` |
|        - |  4369 | `				 }` |
|        - |  4370 | `/*  OP_SHL_STORE * * *` |
|        - |  4371 | ` *` |
|        - |  4372 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4373 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4374 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4375 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4376 | ` */` |
|        - |  4377 | `/* OP_SHR_STORE * * *` |
|        - |  4378 | ` *` |
|        - |  4379 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4380 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4381 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4382 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4383 | ` */` |
|        7 |  4384 | `case PH7_OP_SHL_STORE:` |
|        - |  4385 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4386 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4387 | `	ph7_value *pObj;` |
|        - |  4388 | `	sxi64 a,r;` |
|        - |  4389 | `	sxi32 b;` |
|        - |  4390 | `#ifdef UNTRUST` |
|        - |  4391 | `	if( pNos < pStack ){` |
|        - |  4392 | `		goto Abort;` |
|        - |  4393 | `	}` |
|        - |  4394 | `#endif` |
|        - |  4395 | `	/* Force the operands to be integer */` |
|       15 |  4396 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4397 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4398 | `	}` |
|       15 |  4399 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4400 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4401 | `	}` |
|        - |  4402 | `	/* Perform the requested operation */` |
|       15 |  4403 | `	a = pTos->x.iVal;` |
|       15 |  4404 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4405 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4406 | `		r = a << b;` |
|        4 |  4407 | `	}else{` |
|        9 |  4408 | `		r = a >> b;` |
|        - |  4409 | `	}` |
|        - |  4410 | `	/* Push the result */` |
|       15 |  4411 | `	pNos->x.iVal = r;` |
|       15 |  4412 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4413 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4414 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4415 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4416 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4417 | `	}` |
|       15 |  4418 | `	VmPopOperand(&pTos,1);` |
|       15 |  4419 | `	break;` |
|        - |  4420 | `				 }` |
|        - |  4421 | `/* CAT:  P1 * *` |
|        - |  4422 | ` *` |
|        - |  4423 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4424 | ` * back.` |
|        - |  4425 | ` */` |
|    62825 |  4426 | `case PH7_OP_CAT:{` |
|        - |  4427 | `	ph7_value *pNos,*pCur;` |
|   125652 |  4428 | `	if( pInstr->iP1 < 1 ){` |
|    98664 |  4429 | `		pNos = &pTos[-1];` |
|    49333 |  4430 | `	}else{` |
|    26990 |  4431 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4432 | `	}` |
|        - |  4433 | `#ifdef UNTRUST` |
|        - |  4434 | `	if( pNos < pStack ){` |
|        - |  4435 | `		goto Abort;` |
|        - |  4436 | `	}` |
|        - |  4437 | `#endif` |
|        - |  4438 | `	/* Force a string cast */` |
|   125652 |  4439 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1156 |  4440 | `		PH7_MemObjToString(pNos);` |
|      577 |  4441 | `	}` |
|   125652 |  4442 | `	pCur = &pNos[1];` |
|   253298 |  4443 | `	while( pCur <= pTos ){` |
|   127648 |  4444 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50618 |  4445 | `			PH7_MemObjToString(pCur);` |
|    25308 |  4446 | `		}` |
|        - |  4447 | `		/* Perform the concatenation */` |
|   127648 |  4448 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   127610 |  4449 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    63804 |  4450 | `		}` |
|   127648 |  4451 | `		SyBlobRelease(&pCur->sBlob);` |
|   127648 |  4452 | `		pCur++;` |
|        2 |  4453 | `	}` |
|   125652 |  4454 | `	pTos = pNos;` |
|   125652 |  4455 | `	break;` |
|        - |  4456 | `				}` |
|        - |  4457 | `/*  CAT_STORE: * * *` |
|        - |  4458 | ` *` |
|        - |  4459 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4460 | ` * back.` |
|        - |  4461 | ` */` |
|     3541 |  4462 | `case PH7_OP_CAT_STORE:{` |
|     7084 |  4463 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4464 | `	ph7_value *pObj;` |
|        - |  4465 | `#ifdef UNTRUST` |
|        - |  4466 | `	if( pNos < pStack ){` |
|        - |  4467 | `		goto Abort;` |
|        - |  4468 | `	}` |
|        - |  4469 | `#endif` |
|     7084 |  4470 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4471 | `		/* Force a string cast */` |
|      ! 0 |  4472 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4473 | `	}` |
|     7084 |  4474 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4475 | `		/* Force a string cast */` |
|      ! 0 |  4476 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4477 | `	}` |
|        - |  4478 | `	/* Perform the concatenation (Reverse order) */` |
|     7084 |  4479 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7084 |  4480 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3541 |  4481 | `	}` |
|        - |  4482 | `	/* Perform the store operation */` |
|     7084 |  4483 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4484 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7084 |  4485 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7084 |  4486 | `		PH7_MemObjStore(pTos,pObj);` |
|     3541 |  4487 | `	}` |
|     7084 |  4488 | `	PH7_MemObjStore(pTos,pNos);` |
|     7084 |  4489 | `	VmPopOperand(&pTos,1);` |
|     7084 |  4490 | `	break;` |
|        - |  4491 | `				}` |
|        - |  4492 | `/* OP_AND: * * *` |
|        - |  4493 | ` *` |
|        - |  4494 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4495 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4496 | ` * stack.` |
|        - |  4497 | ` */` |
|        - |  4498 | `/* OP_OR: * * *` |
|        - |  4499 | ` *` |
|        - |  4500 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4501 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4502 | ` * stack.` |
|        - |  4503 | ` */` |
|    94405 |  4504 | `case PH7_OP_LAND:` |
|        - |  4505 | `case PH7_OP_LOR: {` |
|   188856 |  4506 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4507 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4508 | `#ifdef UNTRUST` |
|        - |  4509 | `	if( pNos < pStack ){` |
|        - |  4510 | `		goto Abort;` |
|        - |  4511 | `	}` |
|        - |  4512 | `#endif` |
|        - |  4513 | `	/* Force a boolean cast */` |
|   188856 |  4514 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4515 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4516 | `	}` |
|   188856 |  4517 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4518 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4519 | `	}` |
|   188856 |  4520 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188856 |  4521 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188856 |  4522 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4523 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86428 |  4524 | `		v1 = and_logic[v1*3+v2];` |
|    43237 |  4525 | `	}else{` |
|        - |  4526 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102430 |  4527 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4528 | `	}` |
|   188856 |  4529 | `	if( v1 == 2 ){` |
|      ! 0 |  4530 | `		v1 = 1;` |
|      ! 0 |  4531 | `	}` |
|   188856 |  4532 | `	VmPopOperand(&pTos,1);` |
|   188856 |  4533 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188856 |  4534 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188856 |  4535 | `	break;` |
|        - |  4536 | `				 }` |
|        - |  4537 | `/* OP_LXOR: * * *` |
|        - |  4538 | ` *` |
|        - |  4539 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4540 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4541 | ` * stack.` |
|        - |  4542 | ` * According to the PHP language reference manual:` |
|        - |  4543 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4544 | ` *  TRUE,but not both.` |
|        - |  4545 | ` */` |
|        5 |  4546 | `case PH7_OP_LXOR:{` |
|       11 |  4547 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4548 | `	sxi32 v = 0;` |
|        - |  4549 | `#ifdef UNTRUST` |
|        - |  4550 | `	if( pNos < pStack ){` |
|        - |  4551 | `		goto Abort;` |
|        - |  4552 | `	}` |
|        - |  4553 | `#endif` |
|        - |  4554 | `	/* Force a boolean cast */` |
|       11 |  4555 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4556 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4557 | `	}` |
|       11 |  4558 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4559 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4560 | `	}` |
|       11 |  4561 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4562 | `		v = 1;` |
|        3 |  4563 | `	}` |
|       11 |  4564 | `	VmPopOperand(&pTos,1);` |
|       11 |  4565 | `	pTos->x.iVal = v;` |
|       11 |  4566 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4567 | `	break;` |
|        - |  4568 | `				 }` |
|        - |  4569 | `/* OP_EQ P1 P2 P3` |
|        - |  4570 | ` *` |
|        - |  4571 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4572 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4573 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4574 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4575 | ` */` |
|        - |  4576 | `/* OP_NEQ P1 P2 P3` |
|        - |  4577 | ` *` |
|        - |  4578 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4579 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4580 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4581 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4582 | ` */` |
|     3893 |  4583 | `case PH7_OP_EQ:` |
|        - |  4584 | `case PH7_OP_NEQ: {` |
|     7788 |  4585 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4586 | `	/* Perform the comparison and act accordingly */` |
|        - |  4587 | `#ifdef UNTRUST` |
|        - |  4588 | `	if( pNos < pStack ){` |
|        - |  4589 | `		goto Abort;` |
|        - |  4590 | `	}` |
|        - |  4591 | `#endif` |
|     7788 |  4592 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7788 |  4593 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4594 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7779 |  4595 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7744 |  4596 | `		rc = rc == 0;` |
|     3873 |  4597 | `	}else{` |
|       28 |  4598 | `		rc = rc != 0;` |
|        - |  4599 | `	}` |
|     7788 |  4600 | `	VmPopOperand(&pTos,1);` |
|     7788 |  4601 | `	if( !pInstr->iP2 ){` |
|        - |  4602 | `		/* Push comparison result without taking the jump */` |
|     7788 |  4603 | `		PH7_MemObjRelease(pTos);` |
|     7788 |  4604 | `		pTos->x.iVal = rc;` |
|        - |  4605 | `		/* Invalidate any prior representation */` |
|     7788 |  4606 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3895 |  4607 | `	}else{` |
|      ! 0 |  4608 | `		if( rc ){` |
|        - |  4609 | `			/* Jump to the desired location */` |
|      ! 0 |  4610 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4611 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4612 | `		}` |
|        - |  4613 | `	}` |
|     7788 |  4614 | `	break;` |
|        - |  4615 | `				 }` |
|        - |  4616 | `/* OP_TEQ P1 P2 *` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4619 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4620 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4621 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4622 | ` */` |
|   131435 |  4623 | `case PH7_OP_TEQ: {` |
|   262872 |  4624 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4625 | `	/* Perform the comparison and act accordingly */` |
|        - |  4626 | `#ifdef UNTRUST` |
|        - |  4627 | `	if( pNos < pStack ){` |
|        - |  4628 | `		goto Abort;` |
|        - |  4629 | `	}` |
|        - |  4630 | `#endif` |
|   262872 |  4631 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   262872 |  4632 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4633 | `		rc = 0;` |
|        2 |  4634 | `	}else{` |
|   262870 |  4635 | `		rc = rc == 0;` |
|        - |  4636 | `	}` |
|   262872 |  4637 | `	VmPopOperand(&pTos,1);` |
|   262872 |  4638 | `	if( !pInstr->iP2 ){` |
|        - |  4639 | `		/* Push comparison result without taking the jump */` |
|   262872 |  4640 | `		PH7_MemObjRelease(pTos);` |
|   262872 |  4641 | `		pTos->x.iVal = rc;` |
|        - |  4642 | `		/* Invalidate any prior representation */` |
|   262872 |  4643 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   131437 |  4644 | `	}else{` |
|      ! 0 |  4645 | `		if( rc ){` |
|        - |  4646 | `			/* Jump to the desired location */` |
|      ! 0 |  4647 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4648 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4649 | `		}` |
|        - |  4650 | `	}` |
|   262872 |  4651 | `	break;` |
|        - |  4652 | `				 }` |
|        - |  4653 | `/* OP_TNE P1 P2 *` |
|        - |  4654 | ` *` |
|        - |  4655 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4656 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4657 | ` * instruction.` |
|        - |  4658 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4659 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4660 | ` *` |
|        - |  4661 | ` */` |
|   102367 |  4662 | `case PH7_OP_TNE: {` |
|   204736 |  4663 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4664 | `	/* Perform the comparison and act accordingly */` |
|        - |  4665 | `#ifdef UNTRUST` |
|        - |  4666 | `	if( pNos < pStack ){` |
|        - |  4667 | `		goto Abort;` |
|        - |  4668 | `	}` |
|        - |  4669 | `#endif` |
|   204736 |  4670 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   204736 |  4671 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4672 | `		rc = 1;` |
|        2 |  4673 | `	}else{` |
|   204734 |  4674 | `		rc = rc != 0;` |
|        - |  4675 | `	}` |
|   204736 |  4676 | `	VmPopOperand(&pTos,1);` |
|   204736 |  4677 | `	if( !pInstr->iP2 ){` |
|        - |  4678 | `		/* Push comparison result without taking the jump */` |
|   204736 |  4679 | `		PH7_MemObjRelease(pTos);` |
|   204736 |  4680 | `		pTos->x.iVal = rc;` |
|        - |  4681 | `		/* Invalidate any prior representation */` |
|   204736 |  4682 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102369 |  4683 | `	}else{` |
|      ! 0 |  4684 | `		if( rc ){` |
|        - |  4685 | `			/* Jump to the desired location */` |
|      ! 0 |  4686 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4687 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4688 | `		}` |
|        - |  4689 | `	}` |
|   204736 |  4690 | `	break;` |
|        - |  4691 | `				 }` |
|        - |  4692 | `/* OP_LT P1 P2 P3` |
|        - |  4693 | ` *` |
|        - |  4694 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4695 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4696 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4697 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4698 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4699 | ` *` |
|        - |  4700 | ` */` |
|        - |  4701 | `/* OP_LE P1 P2 P3` |
|        - |  4702 | ` *` |
|        - |  4703 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4704 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4705 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4706 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4707 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4708 | ` *` |
|        - |  4709 | ` */` |
|   102478 |  4710 | `case PH7_OP_LT:` |
|        - |  4711 | `case PH7_OP_LE: {` |
|   205002 |  4712 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4713 | `	/* Perform the comparison and act accordingly */` |
|        - |  4714 | `#ifdef UNTRUST` |
|        - |  4715 | `	if( pNos < pStack ){` |
|        - |  4716 | `		goto Abort;` |
|        - |  4717 | `	}` |
|        - |  4718 | `#endif` |
|   205002 |  4719 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   205002 |  4720 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4721 | `		rc = 0;` |
|   204998 |  4722 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4723 | `		rc = rc < 1;` |
|      205 |  4724 | `	}else{` |
|   204588 |  4725 | `		rc = rc < 0;` |
|        - |  4726 | `	}` |
|   205002 |  4727 | `	VmPopOperand(&pTos,1);` |
|   205002 |  4728 | `	if( !pInstr->iP2 ){` |
|        - |  4729 | `		/* Push comparison result without taking the jump */` |
|   205002 |  4730 | `		PH7_MemObjRelease(pTos);` |
|   205002 |  4731 | `		pTos->x.iVal = rc;` |
|        - |  4732 | `		/* Invalidate any prior representation */` |
|   205002 |  4733 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102524 |  4734 | `	}else{` |
|      ! 0 |  4735 | `		if( rc ){` |
|        - |  4736 | `			/* Jump to the desired location */` |
|      ! 0 |  4737 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4738 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4739 | `		}` |
|        - |  4740 | `	}` |
|   205002 |  4741 | `	break;` |
|        - |  4742 | `				}` |
|        - |  4743 | `/* OP_GT P1 P2 P3` |
|        - |  4744 | ` *` |
|        - |  4745 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4746 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4747 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4748 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4749 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4750 | ` *` |
|        - |  4751 | ` */` |
|        - |  4752 | `/* OP_GE P1 P2 P3` |
|        - |  4753 | ` *` |
|        - |  4754 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4755 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4756 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4757 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4758 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4759 | ` *` |
|        - |  4760 | ` */` |
|    48811 |  4761 | `case PH7_OP_GT:` |
|        - |  4762 | `case PH7_OP_GE: {` |
|    97624 |  4763 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4764 | `	/* Perform the comparison and act accordingly */` |
|        - |  4765 | `#ifdef UNTRUST` |
|        - |  4766 | `	if( pNos < pStack ){` |
|        - |  4767 | `		goto Abort;` |
|        - |  4768 | `	}` |
|        - |  4769 | `#endif` |
|    97624 |  4770 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97624 |  4771 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4772 | `		rc = 0;` |
|    97620 |  4773 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97468 |  4774 | `		rc = rc >= 0;` |
|    48735 |  4775 | `	}else{` |
|      150 |  4776 | `		rc = rc > 0;` |
|        - |  4777 | `	}` |
|    97624 |  4778 | `	VmPopOperand(&pTos,1);` |
|    97624 |  4779 | `	if( !pInstr->iP2 ){` |
|        - |  4780 | `		/* Push comparison result without taking the jump */` |
|    97624 |  4781 | `		PH7_MemObjRelease(pTos);` |
|    97624 |  4782 | `		pTos->x.iVal = rc;` |
|        - |  4783 | `		/* Invalidate any prior representation */` |
|    97624 |  4784 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48813 |  4785 | `	}else{` |
|      ! 0 |  4786 | `		if( rc ){` |
|        - |  4787 | `			/* Jump to the desired location */` |
|      ! 0 |  4788 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4789 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4790 | `		}` |
|        - |  4791 | `	}` |
|    97624 |  4792 | `	break;` |
|        - |  4793 | `				}` |
|        - |  4794 | `/* OP_SEQ P1 P2 *` |
|        - |  4795 | ` * Strict string comparison.` |
|        - |  4796 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4797 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4798 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4799 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4800 | ` * use PH7_OP_EQ.` |
|        - |  4801 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4802 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4803 | ` */` |
|        - |  4804 | `/* OP_SNE P1 P2 *` |
|        - |  4805 | ` * Strict string comparison.` |
|        - |  4806 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4807 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4808 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4809 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4810 | ` * use PH7_OP_EQ.` |
|        - |  4811 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4812 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4813 | ` */` |
|       18 |  4814 | `case PH7_OP_SEQ:` |
|        - |  4815 | `case PH7_OP_SNE: {` |
|       38 |  4816 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4817 | `	SyString s1,s2;` |
|        - |  4818 | `	/* Perform the comparison and act accordingly */` |
|        - |  4819 | `#ifdef UNTRUST` |
|        - |  4820 | `	if( pNos < pStack ){` |
|        - |  4821 | `		goto Abort;` |
|        - |  4822 | `	}` |
|        - |  4823 | `#endif` |
|        - |  4824 | `	/* Force a string cast */` |
|       38 |  4825 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4826 | `		PH7_MemObjToString(pTos);` |
|        2 |  4827 | `	}` |
|       38 |  4828 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4829 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4830 | `	}` |
|       38 |  4831 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4832 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4833 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4834 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4835 | `		rc = rc != 0;` |
|      ! 0 |  4836 | `	}else{` |
|       38 |  4837 | `		rc = rc == 0;` |
|        - |  4838 | `	}` |
|       38 |  4839 | `	VmPopOperand(&pTos,1);` |
|       38 |  4840 | `	if( !pInstr->iP2 ){` |
|        - |  4841 | `		/* Push comparison result without taking the jump */` |
|       38 |  4842 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4843 | `		pTos->x.iVal = rc;` |
|        - |  4844 | `		/* Invalidate any prior representation */` |
|       38 |  4845 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4846 | `	}else{` |
|      ! 0 |  4847 | `		if( rc ){` |
|        - |  4848 | `			/* Jump to the desired location */` |
|      ! 0 |  4849 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4850 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4851 | `		}` |
|        - |  4852 | `	}` |
|       38 |  4853 | `	break;` |
|        - |  4854 | `				 }` |
|        - |  4855 | `/*` |
|        - |  4856 | ` * OP_LOAD_REF * * *` |
|        - |  4857 | ` * Push the index of a referenced object on the stack.` |
|        - |  4858 | ` */` |
|       57 |  4859 | `case PH7_OP_LOAD_REF: {` |
|        - |  4860 | `	sxu32 nIdx;` |
|        - |  4861 | `#ifdef UNTRUST` |
|        - |  4862 | `	if( pTos < pStack ){` |
|        - |  4863 | `		goto Abort;` |
|        - |  4864 | `	}` |
|        - |  4865 | `#endif` |
|        - |  4866 | `	/* Extract memory object index */` |
|      115 |  4867 | `	nIdx = pTos->nIdx;` |
|      115 |  4868 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4869 | `		/* Nullify the object */` |
|       95 |  4870 | `		PH7_MemObjRelease(pTos);` |
|        - |  4871 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4872 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4873 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4874 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4875 | `	}` |
|      115 |  4876 | `	break;` |
|        - |  4877 | `					  }` |
|        - |  4878 | `/*` |
|        - |  4879 | ` * OP_STORE_REF * * P3` |
|        - |  4880 | ` * Perform an assignment operation by reference.` |
|        - |  4881 | ` */` |
|       15 |  4882 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4883 | `	 SyString sName = { 0 , 0 };` |
|        - |  4884 | `	 VmFrame *pFrameLocal;` |
|        - |  4885 | `	SyHashEntry *pEntry;` |
|        - |  4886 | `	sxu32 nIdx;` |
|        - |  4887 | `#ifdef UNTRUST` |
|        - |  4888 | `	if( pTos < pStack ){` |
|        - |  4889 | `		goto Abort;` |
|        - |  4890 | `	}` |
|        - |  4891 | `#endif` |
|       32 |  4892 | `	if( pInstr->p3 == 0 ){` |
|        - |  4893 | `		char *zName;` |
|        - |  4894 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4895 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4896 | `			/* Force a string cast */` |
|      ! 0 |  4897 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4898 | `		}` |
|      ! 0 |  4899 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4900 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4901 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4902 | `			if( zName ){` |
|      ! 0 |  4903 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4904 | `			}` |
|      ! 0 |  4905 | `		}` |
|      ! 0 |  4906 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4907 | `		pTos--;` |
|      ! 0 |  4908 | `	}else{` |
|       32 |  4909 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4910 | `	}` |
|       32 |  4911 | `	nIdx = pTos->nIdx;` |
|       32 |  4912 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4913 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4914 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4915 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4916 | `		}else{` |
|        - |  4917 | `			ph7_value *pObj;` |
|        - |  4918 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4919 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4920 | `			if( pObj == 0 ){` |
|      ! 0 |  4921 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4922 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4923 | `				goto Abort;` |
|        - |  4924 | `			}` |
|        - |  4925 | `			/* Perform the store operation */` |
|      ! 0 |  4926 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4927 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4928 | `		}` |
|       32 |  4929 | `	}else if( sName.nByte > 0){` |
|       32 |  4930 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4931 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4932 | `		}else{` |
|       32 |  4933 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4934 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4935 | `			/* Query the local frame */` |
|       32 |  4936 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4937 | `			if( pEntry ){` |
|      ! 0 |  4938 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4939 | `			}else{` |
|       32 |  4940 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4941 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4942 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4943 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4944 | `				}` |
|       32 |  4945 | `				if( rc == SXRET_OK ){` |
|       32 |  4946 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4947 | `				}` |
|        - |  4948 | `			}` |
|        - |  4949 | `		}` |
|       15 |  4950 | `	}` |
|       32 |  4951 | `	break;` |
|        - |  4952 | `				 }` |
|        - |  4953 | `/*` |
|        - |  4954 | ` * OP_UPLINK P1 * *` |
|        - |  4955 | ` * Link a variable to the top active VM frame.` |
|        - |  4956 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4957 | ` */` |
|       25 |  4958 | `case PH7_OP_UPLINK: {` |
|       52 |  4959 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4960 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4961 | `		SyString sName;` |
|        - |  4962 | `		/* Perform the link */` |
|      104 |  4963 | `		while( pLink <= pTos ){` |
|       54 |  4964 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4965 | `				/* Force a string cast */` |
|      ! 0 |  4966 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4967 | `			}` |
|       54 |  4968 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4969 | `			if( sName.nByte > 0 ){` |
|       54 |  4970 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4971 | `			}` |
|       54 |  4972 | `			pLink++;` |
|        2 |  4973 | `		}` |
|       25 |  4974 | `	}` |
|       52 |  4975 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4976 | `	break;` |
|        - |  4977 | `					}` |
|        - |  4978 | `/*` |
|        - |  4979 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4980 | ` * Push an exception in the corresponding container so that` |
|        - |  4981 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4982 | ` */` |
|       32 |  4983 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  4984 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4985 | `	VmFrame *pFrameLocal;` |
|        - |  4986 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  4987 | `	pException->iFinallyDone = 0;` |
|       66 |  4988 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4989 | `	/* Create the exception frame */` |
|       66 |  4990 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  4991 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4992 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4993 | `		goto Abort;` |
|        - |  4994 | `	}` |
|        - |  4995 | `	/* Mark the special frame */` |
|       66 |  4996 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  4997 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4998 | `	/* Point to the frame that trigger the exception */` |
|       66 |  4999 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5000 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5001 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5002 | `	break;` |
|        - |  5003 | `							}` |
|        - |  5004 | `/*` |
|        - |  5005 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5006 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5007 | ` */` |
|       31 |  5008 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5009 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5010 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5011 | `		ph7_exception **apException;` |
|        - |  5012 | `		/* Pop the loaded exception */` |
|       28 |  5013 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5014 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5015 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5016 | `		}` |
|       13 |  5017 | `	}` |
|       64 |  5018 | `	pException->pFrame = 0;` |
|        - |  5019 | `	/* Leave the exception frame */` |
|       64 |  5020 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5021 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5022 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5023 | `		sxi32 rcFinally;` |
|       19 |  5024 | `		pException->iFinallyDone = 1;` |
|       19 |  5025 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  5026 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5027 | `			goto Abort;` |
|        - |  5028 | `		}` |
|        9 |  5029 | `	}` |
|       64 |  5030 | `	break;` |
|        - |  5031 | `							}` |
|        - |  5032 |  |
|        - |  5033 | `/*` |
|        - |  5034 | ` * OP_THROW * P2 *` |
|        - |  5035 | ` * Throw an user exception.` |
|        - |  5036 | ` */` |
|       18 |  5037 | `case PH7_OP_THROW: {` |
|       38 |  5038 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5039 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5040 | `#ifdef UNTRUST` |
|        - |  5041 | `	if( pTos < pStack ){` |
|        - |  5042 | `		goto Abort;` |
|        - |  5043 | `	}` |
|        - |  5044 | `#endif` |
|       38 |  5045 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5046 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5047 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5048 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5049 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5050 | `		ph7_class *pException;` |
|        - |  5051 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5052 | `		 */` |
|       38 |  5053 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5054 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5055 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5056 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5057 | `			if( rc == SXERR_ABORT ){` |
|        - |  5058 | `				/* Abort processing immediately */` |
|      ! 0 |  5059 | `				goto Abort;` |
|        - |  5060 | `			}` |
|      ! 0 |  5061 | `		}else{` |
|        - |  5062 | `			/* Throw the exception */` |
|       38 |  5063 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5064 | `			if( rc == SXERR_ABORT ){` |
|        - |  5065 | `				/* Abort processing immediately */` |
|        9 |  5066 | `				goto Abort;` |
|        - |  5067 | `			}` |
|        - |  5068 | `		}` |
|       16 |  5069 | `	}else{` |
|        - |  5070 | `		/* Expecting a class instance */` |
|      ! 0 |  5071 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5072 | `		if( rc == SXERR_ABORT ){` |
|        - |  5073 | `			/* Abort processing immediately */` |
|      ! 0 |  5074 | `			goto Abort;` |
|        - |  5075 | `		}` |
|        - |  5076 | `	}` |
|        - |  5077 | `	/* Pop the top entry */` |
|       30 |  5078 | `	VmPopOperand(&pTos,1);` |
|        - |  5079 | `	/* Perform an unconditional jump */` |
|       30 |  5080 | `	pc = nJump - 1;` |
|       30 |  5081 | `	break;` |
|        - |  5082 | `				   }` |
|        - |  5083 | `/*` |
|        - |  5084 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5085 | ` * Prepare a foreach step.` |
|        - |  5086 | ` */` |
|     4915 |  5087 | `case PH7_OP_FOREACH_INIT: {` |
|     9832 |  5088 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5089 | `	void *pName;` |
|        - |  5090 | `#ifdef UNTRUST` |
|        - |  5091 | `	if( pTos < pStack ){` |
|        - |  5092 | `		goto Abort;` |
|        - |  5093 | `	}` |
|        - |  5094 | `#endif` |
|     9832 |  5095 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5096 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5097 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5098 | `			/* Force a string cast */` |
|      ! 0 |  5099 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5100 | `		}` |
|        - |  5101 | `		/* Duplicate name */` |
|      ! 0 |  5102 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5103 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5104 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5105 | `		}` |
|      ! 0 |  5106 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5107 | `	}` |
|     9832 |  5108 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5109 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5110 | `			/* Force a string cast */` |
|      ! 0 |  5111 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5112 | `		}` |
|        - |  5113 | `		/* Duplicate name */` |
|      ! 0 |  5114 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5115 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5116 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5117 | `		}` |
|      ! 0 |  5118 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5119 | `	}` |
|        - |  5120 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9832 |  5121 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5122 | `		/* Jump out of the loop */` |
|      ! 0 |  5123 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5124 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5125 | `		}` |
|      ! 0 |  5126 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5127 | `	}else{` |
|        - |  5128 | `		ph7_foreach_step *pStep;` |
|     9832 |  5129 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9832 |  5130 | `		if( pStep == 0 ){` |
|      ! 0 |  5131 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5132 | `			/* Jump out of the loop */` |
|      ! 0 |  5133 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5134 | `		}else{` |
|        - |  5135 | `			/* Zero the structure */` |
|     9832 |  5136 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5137 | `			/* Prepare the step */` |
|     9832 |  5138 | `			pStep->iFlags = pInfo->iFlags;` |
|     9832 |  5139 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5140 | `				ph7_hashmap *pMap;` |
|        - |  5141 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5142 | `				 * source array so mutations don't affect other sharers. */` |
|     9816 |  5143 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5144 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5145 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5146 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5147 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5148 | `						 * variable still points at the same hashmap as` |
|        - |  5149 | `						 * the stack value. */` |
|       10 |  5150 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5151 | `							pCur->iRef--;` |
|       10 |  5152 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5153 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5154 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5155 | `						}` |
|        4 |  5156 | `					}` |
|        4 |  5157 | `				}` |
|     9816 |  5158 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5159 | `				/* Reset the internal loop cursor */` |
|     9816 |  5160 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5161 | `				/* Mark the step */` |
|     9816 |  5162 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9816 |  5163 | `				pStep->xIter.pMap = pMap;` |
|     9816 |  5164 | `				pMap->iRef++;` |
|     4909 |  5165 | `			}else{` |
|       18 |  5166 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5167 | `				ph7_class *pIteratorClass;` |
|        - |  5168 | `				/* Check if the object implements Iterator */` |
|       18 |  5169 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  5170 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5171 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5172 | `					ph7_class_method *pRewind;` |
|        7 |  5173 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  5174 | `					pStep->xIter.pThis = pThis;` |
|        7 |  5175 | `					pThis->iRef++;` |
|        7 |  5176 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  5177 | `					if( pRewind ){` |
|        7 |  5178 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  5179 | `					}` |
|        4 |  5180 | `				}else{` |
|        - |  5181 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5182 | `					ph7_class *pIterAggClass;` |
|       12 |  5183 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5184 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5185 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5186 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5187 | `						ph7_class_method *pGetIter;` |
|        3 |  5188 | `						int iterAggOk = 0;` |
|        3 |  5189 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5190 | `						if( pGetIter ){` |
|        - |  5191 | `							ph7_value sResult;` |
|        3 |  5192 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5193 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5194 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5195 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5196 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5197 | `									ph7_class_method *pRewind;` |
|        3 |  5198 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5199 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5200 | `									pIterObj->iRef++;` |
|        - |  5201 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5202 | `									pStep->pOwner = pThis;` |
|        3 |  5203 | `									pThis->iRef++;` |
|        3 |  5204 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5205 | `									if( pRewind ){` |
|        3 |  5206 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5207 | `									}` |
|        3 |  5208 | `									iterAggOk = 1;` |
|        1 |  5209 | `								}` |
|        1 |  5210 | `							}` |
|        3 |  5211 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5212 | `						}` |
|        3 |  5213 | `						if( !iterAggOk ){` |
|        - |  5214 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5215 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5216 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5217 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5218 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5219 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5220 | `						}` |
|        2 |  5221 | `					}else{` |
|        - |  5222 | `						/* Plain object iteration via hAttr */` |
|        9 |  5223 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5224 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5225 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5226 | `						pThis->iRef++;` |
|        - |  5227 | `					}` |
|        - |  5228 | `				}` |
|        - |  5229 | `			}` |
|        - |  5230 | `		}` |
|     9832 |  5231 | `		if( pStep ){` |
|     9832 |  5232 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5233 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5234 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5235 | `				/* Jump out of the loop */` |
|      ! 0 |  5236 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5237 | `			}` |
|     4915 |  5238 | `		}` |
|        - |  5239 | `	}` |
|     9832 |  5240 | `	VmPopOperand(&pTos,1);` |
|     9832 |  5241 | `	break;` |
|        - |  5242 | `						  }` |
|        - |  5243 | `/*` |
|        - |  5244 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5245 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5246 | ` */` |
|    79269 |  5247 | `case PH7_OP_FOREACH_STEP: {` |
|   158540 |  5248 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5249 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5250 | `	ph7_value *pValue;` |
|        - |  5251 | `	VmFrame *pFrameLocal;` |
|        - |  5252 | `	/* Peek the last step */` |
|   158540 |  5253 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   158540 |  5254 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   158540 |  5255 | `	pFrameLocal = pVm->pFrame;` |
|   158540 |  5256 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   158540 |  5257 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   158480 |  5258 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5259 | `		ph7_hashmap_node *pNode;` |
|        - |  5260 | `		/* Extract the current node value */` |
|   158480 |  5261 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   158480 |  5262 | `		if( pNode == 0 ){` |
|        - |  5263 | `			/* No more entry to process */` |
|     9814 |  5264 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9814 |  5265 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5266 | `				/* Break the reference with the last element */` |
|        7 |  5267 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5268 | `			}` |
|        - |  5269 | `			/* Automatically reset the loop cursor */` |
|     9814 |  5270 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5271 | `			/* Cleanup the mess left behind */` |
|     9814 |  5272 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9814 |  5273 | `			SySetPop(&pInfo->aStep);` |
|     9814 |  5274 | `			PH7_HashmapUnref(pMap);` |
|     4908 |  5275 | `		}else{` |
|   148668 |  5276 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5277 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5278 | `				if( pKey ){` |
|      416 |  5279 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5280 | `				}` |
|      207 |  5281 | `			}` |
|   148668 |  5282 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5283 | `				SyHashEntry *pEntry;` |
|        - |  5284 | `				/* Pass by reference */` |
|       24 |  5285 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5286 | `				if( pEntry ){` |
|       22 |  5287 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5288 | `				}else{` |
|        4 |  5289 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5290 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5291 | `				}` |
|       13 |  5292 | `			}else{` |
|        - |  5293 | `				/* Make a copy of the entry value */` |
|   148646 |  5294 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   148646 |  5295 | `				if( pValue ){` |
|   148646 |  5296 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    74322 |  5297 | `				}` |
|        - |  5298 | `			}` |
|        2 |  5299 | `		}` |
|    79301 |  5300 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5301 | `		/* Iterator-based iteration.` |
|        - |  5302 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5303 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5304 | `		 */` |
|       37 |  5305 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5306 | `		ph7_class_method *pMethod;` |
|        - |  5307 | `		ph7_value sResult;` |
|       37 |  5308 | `		int isValid = 0;` |
|        - |  5309 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5310 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5311 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5312 | `		}else{` |
|       29 |  5313 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5314 | `			if( pMethod ){` |
|       29 |  5315 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5316 | `			}` |
|        - |  5317 | `		}` |
|        - |  5318 | `		/* Call valid() */` |
|       37 |  5319 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5320 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5321 | `		if( pMethod ){` |
|       37 |  5322 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5323 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5324 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5325 | `		}` |
|       37 |  5326 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5327 | `		if( !isValid ){` |
|        - |  5328 | `			/* Iterator exhausted */` |
|        7 |  5329 | `			pc = pInstr->iP2 - 1;` |
|        - |  5330 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5331 | `			if( pStep->pOwner ){` |
|        3 |  5332 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5333 | `			}` |
|        7 |  5334 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5335 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5336 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5337 | `		}else{` |
|        - |  5338 | `			/* Call current() to get value */` |
|       31 |  5339 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5340 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5341 | `			if( pMethod ){` |
|       31 |  5342 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5343 | `			}` |
|       31 |  5344 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5345 | `			if( pValue ){` |
|       31 |  5346 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5347 | `			}` |
|       31 |  5348 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5349 | `			/* Call key() if needed */` |
|       31 |  5350 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5351 | `				ph7_value sKey;` |
|       23 |  5352 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5353 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5354 | `				if( pMethod ){` |
|       23 |  5355 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5356 | `				}` |
|       23 |  5357 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5358 | `				if( pValue ){` |
|       23 |  5359 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5360 | `				}` |
|       23 |  5361 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5362 | `			}` |
|        - |  5363 | `		}` |
|       19 |  5364 | `	}else{` |
|       25 |  5365 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5366 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5367 | `		SyHashEntry *pEntry;` |
|        - |  5368 | `		/* Point to the next attribute */` |
|       29 |  5369 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5370 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5371 | `			/* Check access permission */` |
|       31 |  5372 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5373 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5374 | `					break; /* Access is granted */` |
|        - |  5375 | `			}` |
|        1 |  5376 | `		}` |
|       25 |  5377 | `		if( pEntry == 0 ){` |
|        - |  5378 | `			/* Clean up the mess left behind */` |
|        9 |  5379 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5380 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5381 | `				/* Break the reference with the last element */` |
|        3 |  5382 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5383 | `			}` |
|        9 |  5384 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5385 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5386 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5387 | `		}else{` |
|       17 |  5388 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5389 | `			ph7_value *pAttrValue;` |
|       17 |  5390 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5391 | `				/* Fill with the current attribute name */` |
|       17 |  5392 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5393 | `				if( pKey ){` |
|       17 |  5394 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5395 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5396 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5397 | `				}` |
|        8 |  5398 | `			}` |
|        - |  5399 | `			/* Extract attribute value */` |
|       17 |  5400 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5401 | `			if( pAttrValue ){` |
|       17 |  5402 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5403 | `					/* Pass by reference */` |
|        3 |  5404 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5405 | `					if( pEntry ){` |
|        3 |  5406 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5407 | `					}else{` |
|      ! 0 |  5408 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5409 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5410 | `					}` |
|        2 |  5411 | `				}else{` |
|        - |  5412 | `					/* Make a copy of the attribute value */` |
|       15 |  5413 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5414 | `					if( pValue ){` |
|       15 |  5415 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5416 | `					}` |
|        - |  5417 | `				}` |
|        8 |  5418 | `			}` |
|        - |  5419 | `		}` |
|        - |  5420 | `	}` |
|   158540 |  5421 | `	break;` |
|        - |  5422 | `						  }` |
|        - |  5423 | `/*` |
|        - |  5424 | ` * OP_MEMBER P1 P2` |
|        - |  5425 | ` * Load class attribute/method on the stack.` |
|        - |  5426 | ` */` |
|     2162 |  5427 | `case PH7_OP_MEMBER: {` |
|        - |  5428 | `	ph7_class_instance *pThis;` |
|        - |  5429 | `	ph7_value *pNos;` |
|        - |  5430 | `	SyString sName;` |
|     4326 |  5431 | `	if( !pInstr->iP1 ){` |
|     4190 |  5432 | `		pNos = &pTos[-1];` |
|        - |  5433 | `#ifdef UNTRUST` |
|        - |  5434 | `		if( pNos < pStack ){` |
|        - |  5435 | `			goto Abort;` |
|        - |  5436 | `		}` |
|        - |  5437 | `#endif` |
|     4190 |  5438 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5439 | `			ph7_class *pClass;` |
|        - |  5440 | `			/* Class already instantiated */` |
|     4190 |  5441 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5442 | `			/* Point to the instantiated class */` |
|     4190 |  5443 | `			pClass = pThis->pClass;` |
|        - |  5444 | `			/* Extract attribute name first */` |
|     4190 |  5445 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4190 |  5446 | `			if( pInstr->iP2 ){` |
|        - |  5447 | `				/* Method call */` |
|      370 |  5448 | `				ph7_class_method *pMeth = 0;` |
|      370 |  5449 | `				if( sName.nByte > 0 ){` |
|        - |  5450 | `					/* Extract the target method */` |
|      370 |  5451 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      184 |  5452 | `				}` |
|      370 |  5453 | `				if( pMeth == 0 ){` |
|      ! 0 |  5454 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5455 | `						&pClass->sName,&sName` |
|        - |  5456 | `						);` |
|        - |  5457 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5458 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5459 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5460 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5461 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5462 | `				}else{` |
|        - |  5463 | `					/* Push method name on the stack */` |
|      370 |  5464 | `					PH7_MemObjRelease(pTos);` |
|      370 |  5465 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      370 |  5466 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5467 | `				}` |
|      370 |  5468 | `				pTos->nIdx = SXU32_HIGH;` |
|      186 |  5469 | `			}else{` |
|        - |  5470 | `				/* Attribute access */` |
|     3822 |  5471 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5472 | `				SyHashEntry *pEntry;` |
|        - |  5473 | `				/* Extract the target attribute */` |
|     3822 |  5474 | `				if( sName.nByte > 0 ){` |
|     3822 |  5475 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3822 |  5476 | `					if( pEntry ){` |
|        - |  5477 | `						/* Point to the attribute value */` |
|     3820 |  5478 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1909 |  5479 | `					}` |
|     1910 |  5480 | `				}` |
|     3822 |  5481 | `				if( pObjAttr == 0 ){` |
|        - |  5482 | `					/* No such attribute,load null */` |
|        4 |  5483 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5484 | `						&pClass->sName,&sName);` |
|        - |  5485 | `					/* Call the __get magic method if available */` |
|        3 |  5486 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5487 | `				}` |
|     3822 |  5488 | `				VmPopOperand(&pTos,1);` |
|        - |  5489 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5490 | `				 * This is due to the following case:` |
|        - |  5491 | `				 *     (new TestClass())->foo;` |
|        - |  5492 | `				 */` |
|     3822 |  5493 | `				pThis->iRef++;` |
|     3822 |  5494 | `				PH7_MemObjRelease(pTos);` |
|     3822 |  5495 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3822 |  5496 | `				if( pObjAttr ){` |
|     3820 |  5497 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5498 | `					/* Check attribute access */` |
|     3820 |  5499 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5500 | `						/* Load attribute */` |
|     3820 |  5501 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3820 |  5502 | `						if( pValue ){` |
|     3820 |  5503 | `							if( pThis->iRef < 2 ){` |
|        - |  5504 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5505 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5506 | `								 */` |
|        3 |  5507 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5508 | `							}else{` |
|        - |  5509 | `								/* Simple load */` |
|     3818 |  5510 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5511 | `							}` |
|     3820 |  5512 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3818 |  5513 | `								if( pThis->iRef > 1 ){` |
|        - |  5514 | `									/* Load attribute index */` |
|     3816 |  5515 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1907 |  5516 | `								}` |
|     1908 |  5517 | `							}` |
|     1909 |  5518 | `						}` |
|     1909 |  5519 | `					}` |
|     1909 |  5520 | `				}` |
|        - |  5521 | `				/* Safely unreference the object */` |
|     3822 |  5522 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5523 | `			}` |
|     2096 |  5524 | `		}else{` |
|      ! 0 |  5525 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5526 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5527 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5528 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5529 | `		}` |
|     2096 |  5530 | `	}else{` |
|        - |  5531 | `		/* Static member access using class name */` |
|      138 |  5532 | `		pNos = pTos;` |
|      138 |  5533 | `		pThis = 0;` |
|      138 |  5534 | `		if( !pInstr->p3 ){` |
|      126 |  5535 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      126 |  5536 | `			pNos--;` |
|        - |  5537 | `#ifdef UNTRUST` |
|        - |  5538 | `			if( pNos < pStack ){` |
|        - |  5539 | `				goto Abort;` |
|        - |  5540 | `			}` |
|        - |  5541 | `#endif` |
|       64 |  5542 | `		}else{` |
|        - |  5543 | `			/* Attribute name already computed */` |
|       14 |  5544 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5545 | `		}` |
|      138 |  5546 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      138 |  5547 | `			ph7_class *pClass = 0;` |
|      138 |  5548 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5549 | `				/* Class already instantiated */` |
|      ! 0 |  5550 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5551 | `				pClass = pThis->pClass;` |
|      ! 0 |  5552 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5553 | `			}else{` |
|        - |  5554 | `				/* Try to extract the target class */` |
|      138 |  5555 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      138 |  5556 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      138 |  5557 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5558 | `					/* Handle self/static/parent keywords */` |
|      138 |  5559 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5560 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5561 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5562 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5563 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5564 | `						}` |
|      124 |  5565 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5566 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      109 |  5567 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5568 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5569 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5570 | `							pClass = pSelf->pBase;` |
|        6 |  5571 | `						}` |
|        8 |  5572 | `					}else{` |
|       84 |  5573 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5574 | `					}` |
|       68 |  5575 | `				}` |
|        - |  5576 | `			}` |
|      138 |  5577 | `			if( pClass == 0 ){` |
|        - |  5578 | `				/* Undefined class */` |
|      ! 0 |  5579 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5580 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5581 | `					);` |
|      ! 0 |  5582 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5583 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5584 | `				}` |
|      ! 0 |  5585 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5586 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5587 | `			}else{` |
|      138 |  5588 | `				if( pInstr->iP2 ){` |
|        - |  5589 | `					/* Method call */` |
|       68 |  5590 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5591 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5592 | `						/* Extract the target method */` |
|       68 |  5593 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5594 | `					}` |
|       68 |  5595 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5596 | `						if( pMeth ){` |
|      ! 0 |  5597 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5598 | `								&pClass->sName,&sName` |
|        - |  5599 | `								);` |
|      ! 0 |  5600 | `						}else{` |
|      ! 0 |  5601 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5602 | `								&pClass->sName,&sName` |
|        - |  5603 | `								);` |
|        - |  5604 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5605 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5606 | `						}` |
|        - |  5607 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5608 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5609 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5610 | `						}` |
|      ! 0 |  5611 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5612 | `					}else{` |
|        - |  5613 | `						/* Push method name on the stack */` |
|       68 |  5614 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5615 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5616 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5617 | `					}` |
|       68 |  5618 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5619 | `				}else{` |
|        - |  5620 | `					/* Attribute access */` |
|       72 |  5621 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5622 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5623 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5624 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5625 | `						/* ::class returns the fully qualified class name */` |
|        - |  5626 | `						/* Pop the attribute name from the stack */` |
|       54 |  5627 | `						if( !pInstr->p3 ){` |
|       54 |  5628 | `							VmPopOperand(&pTos,1);` |
|       26 |  5629 | `						}` |
|       54 |  5630 | `						PH7_MemObjRelease(pTos);` |
|        - |  5631 | `						/* Load the class name */` |
|       54 |  5632 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5633 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5634 | `					}else{` |
|        - |  5635 | `						/* Extract the target attribute */` |
|       20 |  5636 | `						if( sName.nByte > 0 ){` |
|       20 |  5637 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5638 | `						}` |
|       20 |  5639 | `						if( pAttr == 0 ){` |
|        - |  5640 | `							/* No such attribute,load null */` |
|      ! 0 |  5641 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5642 | `								&pClass->sName,&sName);` |
|        - |  5643 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5644 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5645 | `						}` |
|        - |  5646 | `						/* Pop the attribute name from the stack */` |
|       20 |  5647 | `						if( !pInstr->p3 ){` |
|        7 |  5648 | `							VmPopOperand(&pTos,1);` |
|        3 |  5649 | `						}` |
|       20 |  5650 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5651 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5652 | `						if( pAttr ){` |
|       20 |  5653 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5654 | `								/* Access to a non static attribute */` |
|      ! 0 |  5655 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5656 | `									&pClass->sName,&pAttr->sName` |
|        - |  5657 | `									);` |
|      ! 0 |  5658 | `							}else{` |
|        - |  5659 | `								ph7_value *pValue;` |
|        - |  5660 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5661 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5662 | `									/* Load the desired attribute */` |
|       20 |  5663 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5664 | `									if( pValue ){` |
|       20 |  5665 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5666 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5667 | `											/* Load index number */` |
|       14 |  5668 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5669 | `										}` |
|        9 |  5670 | `									}` |
|        9 |  5671 | `								}` |
|        - |  5672 | `							}` |
|        9 |  5673 | `						}` |
|        - |  5674 | `					}` |
|        - |  5675 | `				}` |
|      138 |  5676 | `				if( pThis ){` |
|        - |  5677 | `					/* Safely unreference the object */` |
|      ! 0 |  5678 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5679 | `				}` |
|        - |  5680 | `			}` |
|       70 |  5681 | `		}else{` |
|        - |  5682 | `			/* Pop operands */` |
|      ! 0 |  5683 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5684 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5685 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5686 | `			}` |
|      ! 0 |  5687 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5688 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5689 | `		}` |
|        - |  5690 | `	}` |
|     4326 |  5691 | `	break;` |
|        - |  5692 | `					}` |
|        - |  5693 | `/*` |
|        - |  5694 | ` * OP_NEW P1 * * *` |
|        - |  5695 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5696 | ` */` |
|      318 |  5697 | `case PH7_OP_NEW: {` |
|      638 |  5698 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      638 |  5699 | `	ph7_class *pClass = 0;` |
|        - |  5700 | `	ph7_class_instance *pNew;` |
|      638 |  5701 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5702 | `		/* Try to extract the desired class */` |
|      956 |  5703 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      636 |  5704 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      318 |  5705 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5706 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5707 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5708 | `	}` |
|      638 |  5709 | `	if( pClass == 0 ){` |
|        - |  5710 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5711 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5712 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5713 | `			);` |
|        - |  5714 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5715 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5716 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5717 | `			/* Pop given arguments */` |
|      ! 0 |  5718 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5719 | `		}` |
|      ! 0 |  5720 | `		goto Abort;` |
|      ! 0 |  5721 | `	}else{` |
|        - |  5722 | `		ph7_class_method *pCons;` |
|        - |  5723 | `		/* Create a new class instance */` |
|      638 |  5724 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      638 |  5725 | `		if( pNew == 0 ){` |
|      ! 0 |  5726 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5727 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5728 | `				&pClass->sName` |
|        - |  5729 | `			);` |
|      ! 0 |  5730 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5731 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5732 | `				/* Pop given arguments */` |
|      ! 0 |  5733 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5734 | `			}` |
|      ! 0 |  5735 | `			break;` |
|        - |  5736 | `		}` |
|        - |  5737 | `		/* Check if a constructor is available */` |
|      638 |  5738 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      638 |  5739 | `		if( pCons == 0 ){` |
|      528 |  5740 | `			SyString *pName = &pClass->sName;` |
|        - |  5741 | `			/* Check for a constructor with the same base class name */` |
|      528 |  5742 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      263 |  5743 | `		}` |
|      638 |  5744 | `		if( pCons ){` |
|        - |  5745 | `			/* Call the class constructor */` |
|      112 |  5746 | `			SySetReset(&aArg);` |
|      212 |  5747 | `			while( pArg < pTos ){` |
|      102 |  5748 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      102 |  5749 | `				pArg++;` |
|        2 |  5750 | `			}` |
|      112 |  5751 | `			if( pVm->bErrReport ){` |
|        - |  5752 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5753 | `				sxu32 n;` |
|       69 |  5754 | `				n = SySetUsed(&aArg);` |
|        - |  5755 | `				/* Emit a notice for missing arguments */` |
|      125 |  5756 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       57 |  5757 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       57 |  5758 | `					if( pFuncArg ){` |
|       57 |  5759 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5760 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5761 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5762 | `						}` |
|       28 |  5763 | `					}` |
|       57 |  5764 | `					n++;` |
|        1 |  5765 | `				}` |
|       34 |  5766 | `			}` |
|      112 |  5767 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5768 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      112 |  5769 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5770 | `				pNew->iRef = 1;` |
|      ! 0 |  5771 | `			}` |
|       55 |  5772 | `		}` |
|      638 |  5773 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5774 | `			/* Pop given arguments */` |
|       94 |  5775 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       46 |  5776 | `		}` |
|      638 |  5777 | `		PH7_MemObjRelease(pTos);` |
|      638 |  5778 | `		pTos->x.pOther = pNew;` |
|      638 |  5779 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5780 | `	}` |
|      638 |  5781 | `	break;` |
|        - |  5782 | `				 }` |
|        - |  5783 | `/*` |
|        - |  5784 | ` * OP_CLONE * * *` |
|        - |  5785 | ` * Perfome a clone operation.` |
|        - |  5786 | ` */` |
|       23 |  5787 | `case PH7_OP_CLONE: {` |
|        - |  5788 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5789 | `#ifdef UNTRUST` |
|        - |  5790 | `	if( pTos < pStack ){` |
|        - |  5791 | `		goto Abort;` |
|        - |  5792 | `	}` |
|        - |  5793 | `#endif` |
|        - |  5794 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5795 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5796 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5797 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5798 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5799 | `		break;` |
|        - |  5800 | `	}` |
|        - |  5801 | `	/* Point to the source */` |
|       44 |  5802 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5803 | `	/* Perform the clone operation */` |
|       44 |  5804 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5805 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5806 | `	if( pClone == 0 ){` |
|      ! 0 |  5807 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5808 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5809 | `	}else{` |
|        - |  5810 | `		/* Load the cloned object */` |
|       44 |  5811 | `		pTos->x.pOther = pClone;` |
|       44 |  5812 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5813 | `	}` |
|       44 |  5814 | `	break;` |
|        - |  5815 | `				   }` |
|        - |  5816 | `/*` |
|        - |  5817 | ` * OP_SWITCH * * P3` |
|        - |  5818 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5819 | ` */` |
|       18 |  5820 | `case PH7_OP_SWITCH: {` |
|       38 |  5821 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5822 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5823 | `	ph7_value sValue,sCaseValue;` |
|        - |  5824 | `	sxu32 n,nEntry;` |
|        - |  5825 | `#ifdef UNTRUST` |
|        - |  5826 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5827 | `		goto Abort;` |
|        - |  5828 | `	}` |
|        - |  5829 | `#endif` |
|        - |  5830 | `	/* Point to the case table  */` |
|       38 |  5831 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5832 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5833 | `	/* Select the appropriate case block to execute */` |
|       38 |  5834 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5835 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5836 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5837 | `		pCase = &aCase[n];` |
|       92 |  5838 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5839 | `		/* Execute the case expression first */` |
|       92 |  5840 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5841 | `		/* Compare the two expression */` |
|       92 |  5842 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5843 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5844 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5845 | `		if( rc == 0 ){` |
|        - |  5846 | `			/* Value match,jump to this block */` |
|       38 |  5847 | `			pc = pCase->nStart - 1;` |
|       38 |  5848 | `			break;` |
|        - |  5849 | `		}` |
|       29 |  5850 | `	}` |
|       38 |  5851 | `	VmPopOperand(&pTos,1);` |
|       38 |  5852 | `	if( n >= nEntry ){` |
|        - |  5853 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5854 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5855 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5856 | `		}else{` |
|        - |  5857 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5858 | `			pc = pSwitch->nOut - 1;` |
|        - |  5859 | `		}` |
|      ! 0 |  5860 | `	}` |
|       38 |  5861 | `	break;` |
|        - |  5862 | `					}` |
|        - |  5863 | `/*` |
|        - |  5864 | ` * OP_CALL P1 * *` |
|        - |  5865 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5866 | ` *  function on the stack.` |
|        - |  5867 | ` */` |
|   288622 |  5868 | `case PH7_OP_CALL: {` |
|   577290 |  5869 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5870 | `	SyHashEntry *pEntry;` |
|        - |  5871 | `	SyString sName;` |
|        - |  5872 | `	/* Extract function name */` |
|   577290 |  5873 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5874 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5875 | `			ph7_value sResult;` |
|      ! 0 |  5876 | `			SySetReset(&aArg);` |
|      ! 0 |  5877 | `			while( pArg < pTos ){` |
|      ! 0 |  5878 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5879 | `				pArg++;` |
|      ! 0 |  5880 | `			}` |
|      ! 0 |  5881 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5882 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5883 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5884 | `			SySetReset(&aArg);` |
|        - |  5885 | `			/* Pop given arguments */` |
|      ! 0 |  5886 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5887 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5888 | `			}` |
|        - |  5889 | `			/* Copy result */` |
|      ! 0 |  5890 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5891 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5892 | `		}else{` |
|        3 |  5893 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5894 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5895 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5896 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5897 | `			}else{` |
|        - |  5898 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5899 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5900 | `			}` |
|        - |  5901 | `			/* Pop given arguments */` |
|        3 |  5902 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5903 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5904 | `			}` |
|        - |  5905 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5906 | `			PH7_MemObjRelease(pTos);` |
|        - |  5907 | `		}` |
|   288349 |  5908 | `		break;` |
|        - |  5909 | `	}` |
|   577288 |  5910 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5911 | `	/* Check for a compiled function first.` |
|        - |  5912 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5913 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   577288 |  5914 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5915 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5916 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5917 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5918 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5919 | `	 * function calls inside namespaces. */` |
|   577288 |  5920 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5921 | `		const char *zFunc;` |
|        - |  5922 | `		const char *zEnd;` |
|        - |  5923 | `		const char *z;` |
|        - |  5924 | `		SyString sGlobal;` |
|       15 |  5925 | `		zFunc = sName.zString;` |
|       15 |  5926 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5927 | `		z = zEnd;` |
|        - |  5928 | `		/* Find last namespace separator */` |
|      133 |  5929 | `		while( z > zFunc ){` |
|      133 |  5930 | `			if( z[-1] == '\\' ){` |
|       15 |  5931 | `				break;` |
|        - |  5932 | `			}` |
|      119 |  5933 | `			z--;` |
|        1 |  5934 | `		}` |
|       15 |  5935 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5936 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5937 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5938 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5939 | `		}` |
|        7 |  5940 | `	}` |
|   577288 |  5941 | `	if( pEntry ){` |
|        - |  5942 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5943 | `		ph7_class_instance *pThis;` |
|        - |  5944 | `		ph7_value *pFrameStack;` |
|        - |  5945 | `		ph7_vm_func *pVmFunc;` |
|        - |  5946 | `		ph7_class *pSelf;` |
|        - |  5947 | `		VmFrame *pFrame;` |
|        - |  5948 | `		ph7_value *pObj;` |
|        - |  5949 | `		VmSlot sArg;` |
|        - |  5950 | `		sxu32 n;` |
|        - |  5951 | `		/* initialize fields */` |
|    12892 |  5952 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12892 |  5953 | `		pThis = 0;` |
|    12892 |  5954 | `		pSelf = 0;` |
|    12892 |  5955 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5956 | `			ph7_class_method *pMeth;` |
|        - |  5957 | `			/* Class method call */` |
|     1764 |  5958 | `			ph7_value *pTarget = &pTos[-1];` |
|     1764 |  5959 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5960 | `				/* Extract the 'this' pointer */` |
|     1764 |  5961 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5962 | `					/* Instance already loaded */` |
|     1692 |  5963 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1692 |  5964 | `					pThis->iRef++;` |
|     1692 |  5965 | `					pSelf = pThis->pClass;` |
|      845 |  5966 | `				}` |
|     1764 |  5967 | `				if( pSelf == 0 ){` |
|       74 |  5968 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5969 | `						/* "Late Static Binding" class name */` |
|      101 |  5970 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  5971 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  5972 | `					}` |
|       74 |  5973 | `					if( pSelf == 0 ){` |
|       13 |  5974 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5975 | `					}` |
|       36 |  5976 | `				}` |
|     1764 |  5977 | `				if( pThis == 0  ){` |
|       74 |  5978 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  5979 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  5980 | `					if( pFrameLocal->pParent ){` |
|        - |  5981 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  5982 | `						pThis = pFrameLocal->pThis;` |
|       58 |  5983 | `						if( pThis ){` |
|       13 |  5984 | `							pThis->iRef++;` |
|        6 |  5985 | `						}` |
|       28 |  5986 | `					}` |
|       36 |  5987 | `				}` |
|     1764 |  5988 | `				VmPopOperand(&pTos,1);` |
|     1764 |  5989 | `				PH7_MemObjRelease(pTos);` |
|        - |  5990 | `				/* Synchronize pointers */` |
|     1764 |  5991 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5992 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5993 | `				 * user have already computed the random generated unique class method name` |
|        - |  5994 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5995 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5996 | `				 */` |
|     1764 |  5997 | `				while( pArg < pStack ){` |
|      ! 0 |  5998 | `					pArg++;` |
|      ! 0 |  5999 | `				}` |
|     1764 |  6000 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6001 | `					/* Check if the call is allowed */` |
|     1764 |  6002 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1764 |  6003 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6004 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6005 | `							/* Pop given arguments */` |
|      ! 0 |  6006 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6007 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6008 | `							}` |
|        - |  6009 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6010 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6011 | `							break;` |
|        - |  6012 | `						}` |
|        3 |  6013 | `					}` |
|      881 |  6014 | `				}` |
|      881 |  6015 | `			}` |
|      881 |  6016 | `		}` |
|        - |  6017 | `		/* Check The recursion limit */` |
|    12892 |  6018 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6019 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6020 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6021 | `				&pVmFunc->sName);` |
|        - |  6022 | `			/* Pop given arguments */` |
|        3 |  6023 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6024 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6025 | `			}` |
|        - |  6026 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6027 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6028 | `			break;` |
|        - |  6029 | `		}` |
|    12890 |  6030 | `		if( pVmFunc->pNextName ){` |
|        - |  6031 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6032 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6033 | `		}` |
|        - |  6034 | `		/* Extract the formal argument set */` |
|    12890 |  6035 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6036 | `		/* Create a new VM frame  */` |
|    12890 |  6037 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12890 |  6038 | `		if( rc != SXRET_OK ){` |
|        - |  6039 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6040 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6041 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6042 | `				&pVmFunc->sName);` |
|        - |  6043 | `			/* Pop given arguments */` |
|      ! 0 |  6044 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6045 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6046 | `			}` |
|        - |  6047 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6048 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6049 | `			break;` |
|        - |  6050 | `		}` |
|    12890 |  6051 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6052 | `			/* Install the '$this' variable */` |
|        - |  6053 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1702 |  6054 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1702 |  6055 | `			if( pObj ){` |
|        - |  6056 | `				/* Reflect the change */` |
|     1702 |  6057 | `				pObj->x.pOther = pThis;` |
|     1702 |  6058 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      850 |  6059 | `			}` |
|      850 |  6060 | `		}` |
|    12890 |  6061 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6062 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6063 | `			/* Install static variables */` |
|      ! 0 |  6064 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6065 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6066 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6067 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6068 | `					/* Initialize the static variables */` |
|      ! 0 |  6069 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6070 | `					if( pObj ){` |
|        - |  6071 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6072 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6073 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6074 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6075 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6076 | `						}` |
|      ! 0 |  6077 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6078 | `					}else{` |
|      ! 0 |  6079 | `						continue;` |
|        - |  6080 | `					}` |
|      ! 0 |  6081 | `				}` |
|        - |  6082 | `				/* Install in the current frame */` |
|      ! 0 |  6083 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6084 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6085 | `			}` |
|      ! 0 |  6086 | `		}` |
|        - |  6087 | `		/* Push arguments in the local frame */` |
|    12890 |  6088 | `		n = 0;` |
|    35402 |  6089 | `		while( pArg < pTos ){` |
|    22514 |  6090 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22360 |  6091 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6092 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  6093 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6094 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6095 | `						goto Abort;` |
|        - |  6096 | `					}` |
|      ! 0 |  6097 | `				}` |
|        - |  6098 | `				/* Make sure the given arguments are of the correct type */` |
|    22360 |  6099 | `				if( aFormalArg[n].nType > 0 ){` |
|     1098 |  6100 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6101 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  6102 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6103 | `						ph7_class *pClass;` |
|        - |  6104 | `						/* Try to extract the desired class */` |
|      ! 0 |  6105 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  6106 | `						if( pClass ){` |
|      ! 0 |  6107 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6108 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6109 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6110 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6111 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6112 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6113 | `								}` |
|      ! 0 |  6114 | `							}else{` |
|        - |  6115 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6116 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6117 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6118 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6119 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6120 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6121 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6122 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6123 | `								}` |
|        - |  6124 | `							}` |
|      ! 0 |  6125 | `						}` |
|     1098 |  6126 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6127 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6128 | `						/* Cast to the desired type */` |
|      ! 0 |  6129 | `						xCast(pArg);` |
|      ! 0 |  6130 | `					}` |
|      548 |  6131 | `				}` |
|    22360 |  6132 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6133 | `					/* Pass by reference */` |
|       50 |  6134 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6135 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6136 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6137 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6138 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6139 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6140 | `						}` |
|        - |  6141 | `						/* Switch to pass by value */` |
|      ! 0 |  6142 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6143 | `					}else{` |
|        - |  6144 | `						SyHashEntry *pRefEntry;` |
|        - |  6145 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6146 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6147 | `						if( pRefEntry == 0 ){` |
|       74 |  6148 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6149 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6150 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6151 | `							sArg.pUserData = 0;` |
|       50 |  6152 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6153 | `						}` |
|       50 |  6154 | `						pObj = 0;` |
|        - |  6155 | `					}` |
|       26 |  6156 | `				}else{` |
|        - |  6157 | `					/* Pass by value,make a copy of the given argument */` |
|    22312 |  6158 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6159 | `				}` |
|    11181 |  6160 | `			}else{` |
|        - |  6161 | `				char zName[32];` |
|        - |  6162 | `				SyString sArgName;` |
|        - |  6163 | `				/* Set a dummy name */` |
|      156 |  6164 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6165 | `				sArgName.zString = zName;` |
|        - |  6166 | `				/* Annonymous argument */` |
|      156 |  6167 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6168 | `			}` |
|    22514 |  6169 | `			if( pObj ){` |
|    22466 |  6170 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6171 | `				/* Insert argument index  */` |
|    22466 |  6172 | `				sArg.nIdx = pObj->nIdx;` |
|    22466 |  6173 | `				sArg.pUserData = 0;` |
|    22466 |  6174 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11232 |  6175 | `			}` |
|    22514 |  6176 | `			PH7_MemObjRelease(pArg);` |
|    22514 |  6177 | `			pArg++;` |
|    22514 |  6178 | `			++n;` |
|        2 |  6179 | `		}` |
|        - |  6180 | `		/* Set up closure environment */` |
|    12890 |  6181 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6182 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6183 | `			ph7_value *pValue;` |
|        - |  6184 | `			sxu32 iEnv;` |
|       11 |  6185 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6186 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6187 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6188 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6189 | `					/* Do not install null value */` |
|       11 |  6190 | `					continue;` |
|        - |  6191 | `				}` |
|       11 |  6192 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6193 | `				if( pValue == 0 ){` |
|      ! 0 |  6194 | `					continue;` |
|        - |  6195 | `				}` |
|        - |  6196 | `				/* Invalidate any prior representation */` |
|       11 |  6197 | `				PH7_MemObjRelease(pValue);` |
|        - |  6198 | `				/* Duplicate bound variable value */` |
|       11 |  6199 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6200 | `			}` |
|        5 |  6201 | `		}` |
|        - |  6202 | `		/* Process default values */` |
|    14812 |  6203 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1924 |  6204 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1918 |  6205 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1918 |  6206 | `				if( pObj ){` |
|        - |  6207 | `					/* Evaluate the default value and extract it's result */` |
|     1918 |  6208 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1918 |  6209 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6210 | `						goto Abort;` |
|        - |  6211 | `					}` |
|        - |  6212 | `					/* Insert argument index */` |
|     1918 |  6213 | `					sArg.nIdx = pObj->nIdx;` |
|     1918 |  6214 | `					sArg.pUserData = 0;` |
|     1918 |  6215 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6216 | `					/* Make sure the default argument is of the correct type */` |
|     1918 |  6217 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6218 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6219 | `						/* Cast to the desired type */` |
|      ! 0 |  6220 | `						xCast(pObj);` |
|      ! 0 |  6221 | `					}` |
|      958 |  6222 | `				}` |
|      958 |  6223 | `			}` |
|     1924 |  6224 | `			++n;` |
|        2 |  6225 | `		}` |
|        - |  6226 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6227 | `		 * does not return anything.` |
|        - |  6228 | `		 */` |
|    12890 |  6229 | `		PH7_MemObjRelease(pTos);` |
|    12890 |  6230 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6231 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12890 |  6232 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12890 |  6233 | `		if( pFrameStack == 0 ){` |
|        - |  6234 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6235 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6236 | `				&pVmFunc->sName);` |
|      ! 0 |  6237 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6238 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6239 | `			}` |
|      ! 0 |  6240 | `			break;` |
|        - |  6241 | `		}` |
|    12890 |  6242 | `		if( pSelf ){` |
|        - |  6243 | `			/* Push class name */` |
|     1762 |  6244 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      880 |  6245 | `		}` |
|        - |  6246 | `		/* Increment nesting level */` |
|    12890 |  6247 | `		pVm->nRecursionDepth++;` |
|        - |  6248 | `		/* Execute function body */` |
|    12890 |  6249 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6250 | `		/* Decrement nesting level */` |
|    12890 |  6251 | `		pVm->nRecursionDepth--;` |
|    12890 |  6252 | `		if( pSelf ){` |
|        - |  6253 | `			/* Pop class name */` |
|     1762 |  6254 | `			(void)SySetPop(&pVm->aSelf);` |
|      880 |  6255 | `		}` |
|        - |  6256 | `		/* Cleanup the mess left behind */` |
|    12890 |  6257 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6258 | `			/* Return by reference,reflect that */` |
|        9 |  6259 | `			if( n != SXU32_HIGH ){` |
|        9 |  6260 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6261 | `				sxu32 i;` |
|        - |  6262 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6263 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6264 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6265 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6266 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6267 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6268 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6269 | `								&pVmFunc->sName);` |
|      ! 0 |  6270 | `						}` |
|      ! 0 |  6271 | `						n = SXU32_HIGH;` |
|      ! 0 |  6272 | `						break;` |
|        - |  6273 | `					}` |
|        3 |  6274 | `				}` |
|        5 |  6275 | `			}else{` |
|      ! 0 |  6276 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6277 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6278 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6279 | `						&pVmFunc->sName);` |
|      ! 0 |  6280 | `				}` |
|        - |  6281 | `			}` |
|        9 |  6282 | `			pTos->nIdx = n;` |
|        4 |  6283 | `		}` |
|        - |  6284 | `		/* Cleanup the mess left behind */` |
|    12890 |  6285 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6286 | `			/* An exception was throw in this frame */` |
|       12 |  6287 | `			pFrame = pFrame->pParent;` |
|       12 |  6288 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6289 | `				/* Pop the resutlt */` |
|       10 |  6290 | `				VmPopOperand(&pTos,1);` |
|        - |  6291 | `				/* Jump to this destination */` |
|       10 |  6292 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6293 | `				rc = PH7_OK;` |
|        6 |  6294 | `			}else{` |
|        3 |  6295 | `				if( pFrame->pParent ){` |
|        3 |  6296 | `					rc = PH7_EXCEPTION;` |
|        2 |  6297 | `				}else{` |
|        - |  6298 | `					/* Continue normal execution */` |
|      ! 0 |  6299 | `					rc = PH7_OK;` |
|        - |  6300 | `				}` |
|        - |  6301 | `			}` |
|        5 |  6302 | `		}` |
|        - |  6303 | `		/* Free the operand stack */` |
|    12890 |  6304 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6305 | `		/* Leave the frame */` |
|    12890 |  6306 | `		VmLeaveFrame(&(*pVm));` |
|    12890 |  6307 | `		if( rc == PH7_ABORT ){` |
|        - |  6308 | `			/* Abort processing immeditaley */` |
|        7 |  6309 | `			goto Abort;` |
|    12884 |  6310 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6311 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6312 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6313 | `			 * overwriting the state saved by the inner level.` |
|        - |  6314 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6315 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       39 |  6316 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       39 |  6317 | `			goto Suspend;` |
|    12846 |  6318 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6319 | `			goto Exception;` |
|        - |  6320 | `		}` |
|     6423 |  6321 | `	}else{` |
|        - |  6322 | `		ph7_user_func *pFunc;` |
|        - |  6323 | `		ph7_context sCtx;` |
|        - |  6324 | `		ph7_value sRet;` |
|        - |  6325 | `		/* Look for an installed foreign function.` |
|        - |  6326 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6327 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6328 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6329 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6330 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6331 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   564398 |  6332 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   564398 |  6333 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6334 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6335 | `			const char *zShort = sName.zString;` |
|        - |  6336 | `			sxu32 i;` |
|      217 |  6337 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6338 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6339 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6340 | `				}` |
|      102 |  6341 | `			}` |
|       15 |  6342 | `			if( zShort != sName.zString ){` |
|       15 |  6343 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6344 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6345 | `			}` |
|        7 |  6346 | `		}` |
|   564398 |  6347 | `		if( pEntry == 0 ){` |
|        - |  6348 | `			/* Call to undefined function */` |
|        5 |  6349 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6350 | `			/* Pop given arguments */` |
|        5 |  6351 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6352 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6353 | `			}` |
|        - |  6354 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6355 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6356 | `			break;` |
|        - |  6357 | `		}` |
|   564394 |  6358 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6359 | `		/* Start collecting function arguments */` |
|   564394 |  6360 | `		SySetReset(&aArg);` |
|  1513362 |  6361 | `		while( pArg < pTos ){` |
|   948970 |  6362 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   948970 |  6363 | `			pArg++;` |
|        2 |  6364 | `		}` |
|        - |  6365 | `		/* Assume a null return value */` |
|   564394 |  6366 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6367 | `		/* Init the call context */` |
|   564394 |  6368 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6369 | `		/* Call the foreign function */` |
|   564394 |  6370 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6371 | `		/* Release the call context */` |
|   564394 |  6372 | `		VmReleaseCallContext(&sCtx);` |
|   564394 |  6373 | `		if( rc == PH7_ABORT ){` |
|      463 |  6374 | `			goto Abort;` |
|   563932 |  6375 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6376 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6377 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6378 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6379 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6380 | `				goto Exception;` |
|        - |  6381 | `			}` |
|        - |  6382 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6383 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6384 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6385 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6386 | `			}` |
|        - |  6387 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6388 | `			VmPopOperand(&pTos,1);` |
|        - |  6389 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6390 | `			pFrm = pVm->pFrame;` |
|        7 |  6391 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6392 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6393 | `			}` |
|        7 |  6394 | `			break;` |
|        - |  6395 | `		}` |
|   563922 |  6396 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6397 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6398 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6399 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6400 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6401 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6402 | `			 * body), the user-function path above will handle re-saving. */` |
|       39 |  6403 | `			PH7_MemObjRelease(&sRet);` |
|       39 |  6404 | `			if( pInstr->iP1 > 0 ){` |
|       39 |  6405 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6406 | `			}` |
|        - |  6407 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6408 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       39 |  6409 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       39 |  6410 | `			goto Suspend;` |
|        - |  6411 | `		}` |
|   563884 |  6412 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6413 | `			/* Pop function name and arguments */` |
|   546260 |  6414 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   273151 |  6415 | `		}` |
|        - |  6416 | `		/* Save foreign function return value */` |
|   563884 |  6417 | `		PH7_MemObjStore(&sRet,pTos);` |
|   563884 |  6418 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6419 | `	}` |
|   576726 |  6420 | `	break;` |
|        - |  6421 | `				  }` |
|        - |  6422 | `/*` |
|        - |  6423 | ` * OP_CONSUME: P1 * *` |
|        - |  6424 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6425 | ` */` |
|    11237 |  6426 | `case PH7_OP_CONSUME: {` |
|    22476 |  6427 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22476 |  6428 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6429 |  |
|    22476 |  6430 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22476 |  6431 | `	pCur = pOut;` |
|        - |  6432 | `	/* Start the consume process  */` |
|    44950 |  6433 | `	while( pOut <= pTos ){` |
|        - |  6434 | `		/* Force a string cast */` |
|    22476 |  6435 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6436 | `			PH7_MemObjToString(pOut);` |
|      151 |  6437 | `		}` |
|    22476 |  6438 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6439 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6440 | `			/* Invoke the output consumer callback */` |
|    12402 |  6441 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12402 |  6442 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12402 |  6443 | `			SyBlobRelease(&pOut->sBlob);` |
|    12402 |  6444 | `			if( rc == SXERR_ABORT ){` |
|        - |  6445 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6446 | `				goto Abort;` |
|        - |  6447 | `			}` |
|     6200 |  6448 | `		}` |
|    22476 |  6449 | `		pOut++;` |
|        2 |  6450 | `	}` |
|    22476 |  6451 | `	pTos = &pCur[-1];` |
|    22474 |  6452 | `	break;` |
|        - |  6453 | `					 }` |
|        - |  6454 |  |
|        - |  6455 | `		} /* Switch() */` |
|  9833310 |  6456 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6457 | `	} /* For(;;) */` |
|    15744 |  6458 | `Done:` |
|    31490 |  6459 | `	SySetRelease(&aArg);` |
|    31490 |  6460 | `	return SXRET_OK;` |
|       38 |  6461 | `Suspend:` |
|       77 |  6462 | `	SySetRelease(&aArg);` |
|       77 |  6463 | `	return PH7_SUSPEND;` |
|      238 |  6464 | `Abort:` |
|      477 |  6465 | `	SySetRelease(&aArg);` |
|     1661 |  6466 | `	while( pTos >= pStack ){` |
|     1185 |  6467 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6468 | `		pTos--;` |
|        1 |  6469 | `	}` |
|      477 |  6470 | `	return PH7_ABORT;` |
|        3 |  6471 | `Exception:` |
|        8 |  6472 | `	SySetRelease(&aArg);` |
|       22 |  6473 | `	while( pTos >= pStack ){` |
|       16 |  6474 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6475 | `		pTos--;` |
|        2 |  6476 | `	}` |
|        8 |  6477 | `	return PH7_EXCEPTION;` |
|    16025 |  6478 |  |
|        - |  6479 | `/*` |
|        - |  6480 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6481 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6482 | ` * See block-comment on that function for additional information.` |
|        - |  6483 | ` */` |
|    14902 |  6484 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6485 |  |
|        - |  6486 | `	ph7_value *pStack;` |
|        - |  6487 | `	sxi32 rc;` |
|        - |  6488 | `	/* Allocate a new operand stack */` |
|    14904 |  6489 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14904 |  6490 | `	if( pStack == 0 ){` |
|      ! 0 |  6491 | `		return SXERR_MEM;` |
|        - |  6492 | `	}` |
|        - |  6493 | `	/* Execute the program */` |
|    14904 |  6494 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6495 | `	/* Free the operand stack */` |
|    14904 |  6496 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6497 | `	/* Execution result */` |
|    14904 |  6498 | `	return rc;` |
|     7453 |  6499 |  |
|        - |  6500 | `/*` |
|        - |  6501 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6502 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6503 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6504 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6505 | ` * execution ends.` |
|        - |  6506 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6507 | ` * additional information.` |
|        - |  6508 | ` */` |
|     2424 |  6509 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6510 |  |
|        - |  6511 | `	VmShutdownCB *pEntry;` |
|        - |  6512 | `	ph7_value *apArg[10];` |
|        - |  6513 | `	sxu32 n,nEntry;` |
|        - |  6514 | `	int i;` |
|        - |  6515 | `	/* Point to the stack of registered callbacks */` |
|     2426 |  6516 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    26666 |  6517 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    24242 |  6518 | `		apArg[i] = 0;` |
|    12122 |  6519 | `	}` |
|     2428 |  6520 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6521 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6522 | `		if( pEntry ){` |
|        - |  6523 | `			/* Prepare callback arguments if any */` |
|        3 |  6524 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6525 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6526 | `					break;` |
|        - |  6527 | `				}` |
|      ! 0 |  6528 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6529 | `			}` |
|        - |  6530 | `			/* Invoke the callback */` |
|        3 |  6531 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6532 | `			/*` |
|        - |  6533 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6534 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6535 | `			 */` |
|        3 |  6536 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6537 | `			if( pEntry ){` |
|        3 |  6538 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6539 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6540 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6541 | `				}` |
|        1 |  6542 | `			}` |
|        1 |  6543 | `		}` |
|        2 |  6544 | `	}` |
|     2426 |  6545 | `	SySetReset(&pVm->aShutdown);` |
|     2426 |  6546 |  |
|        - |  6547 | `/*` |
|        - |  6548 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6549 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6550 | ` * See block-comment on that function for additional information.` |
|        - |  6551 | ` */` |
|     2432 |  6552 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6553 |  |
|        - |  6554 | `	/* Make sure we are ready to execute this program */` |
|     2434 |  6555 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6556 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6557 | `	}` |
|        - |  6558 | `	/* Set the execution magic number  */` |
|     2434 |  6559 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6560 | `	/* Execute the program */` |
|     2434 |  6561 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6562 | `	/* Invoke any shutdown callbacks */` |
|     2430 |  6563 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6564 | `	/*` |
|        - |  6565 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6566 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6567 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6568 | `	 */` |
|     2430 |  6569 | `	return SXRET_OK;` |
|     1218 |  6570 |  |
|        - |  6571 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6572 | `/*` |
|        - |  6573 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6574 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6575 | ` */` |
|       24 |  6576 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        1 |  6577 |  |
|        - |  6578 | `	ph7_exec_ctx *pCtx;` |
|        - |  6579 | `	ph7_value *pStack;` |
|        - |  6580 | `	VmFrame *pFrame;` |
|       25 |  6581 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       25 |  6582 | `	if( pCtx == 0 ){` |
|      ! 0 |  6583 | `		return 0;` |
|        - |  6584 | `	}` |
|       25 |  6585 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       25 |  6586 | `	pCtx->pVm = pVm;` |
|       25 |  6587 | `	pCtx->pFunc = pFunc;` |
|       25 |  6588 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       25 |  6589 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       25 |  6590 | `	pCtx->pc = 0;` |
|       25 |  6591 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       25 |  6592 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6593 | `	/* Allocate a private operand stack */` |
|       25 |  6594 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       25 |  6595 | `	if( pStack == 0 ){` |
|      ! 0 |  6596 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6597 | `		return 0;` |
|        - |  6598 | `	}` |
|       25 |  6599 | `	pCtx->pStack = pStack;` |
|        - |  6600 | `	/* Create a detached frame for the fiber */` |
|       25 |  6601 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       25 |  6602 | `	if( pFrame == 0 ){` |
|      ! 0 |  6603 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6604 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6605 | `		return 0;` |
|        - |  6606 | `	}` |
|       25 |  6607 | `	pCtx->pFrame = pFrame;` |
|       25 |  6608 | `	return pCtx;` |
|       13 |  6609 |  |
|        - |  6610 | `/*` |
|        - |  6611 | ` * Start executing a fiber context for the first time.` |
|        - |  6612 | ` */` |
|       24 |  6613 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        1 |  6614 |  |
|        - |  6615 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6616 | `	sxi32 rc;` |
|       25 |  6617 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6618 | `		return SXERR_INVALID;` |
|        - |  6619 | `	}` |
|        - |  6620 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       25 |  6621 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  6622 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6623 | `	/* Save and set the active context */` |
|       25 |  6624 | `	pOldCtx = pVm->pActiveCtx;` |
|       25 |  6625 | `	pVm->pActiveCtx = pCtx;` |
|       25 |  6626 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       25 |  6627 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       25 |  6628 | `	pVm->nRecursionDepth++;` |
|        - |  6629 | `	/* Execute from the beginning */` |
|       37 |  6630 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       12 |  6631 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       25 |  6632 | `	pVm->nRecursionDepth--;` |
|        - |  6633 | `	/* Restore the previous context */` |
|       25 |  6634 | `	pVm->pActiveCtx = pOldCtx;` |
|       25 |  6635 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6636 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       23 |  6637 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       23 |  6638 | `		pCtx->pFrame->pParent = 0;` |
|       23 |  6639 | `		if( pResult ){` |
|       23 |  6640 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6641 | `		}` |
|       23 |  6642 | `		return SXRET_OK;` |
|        - |  6643 | `	}` |
|        - |  6644 | `	/* Detach frame */` |
|        3 |  6645 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6646 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6647 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6648 | `	}` |
|        3 |  6649 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6650 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6651 | `		return PH7_ABORT;` |
|        - |  6652 | `	}` |
|        3 |  6653 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6654 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6655 | `		return PH7_EXCEPTION;` |
|        - |  6656 | `	}` |
|        - |  6657 | `	/* Normal completion */` |
|        3 |  6658 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6659 | `	if( pResult ){` |
|        3 |  6660 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6661 | `	}` |
|        3 |  6662 | `	return SXRET_OK;` |
|       13 |  6663 |  |
|        - |  6664 | `/*` |
|        - |  6665 | ` * Resume a suspended fiber context.` |
|        - |  6666 | ` */` |
|       34 |  6667 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        1 |  6668 |  |
|        - |  6669 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6670 | `	sxi32 rc;` |
|       35 |  6671 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  6672 | `		return SXERR_INVALID;` |
|        - |  6673 | `	}` |
|        - |  6674 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  6675 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  6676 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       35 |  6677 | `	if( pResumeValue ){` |
|       35 |  6678 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       18 |  6679 | `	}else{` |
|      ! 0 |  6680 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  6681 | `	}` |
|       35 |  6682 | `	pCtx->nTos++;` |
|        - |  6683 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       35 |  6684 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       35 |  6685 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6686 | `	/* Save and set the active context */` |
|       35 |  6687 | `	pOldCtx = pVm->pActiveCtx;` |
|       35 |  6688 | `	pVm->pActiveCtx = pCtx;` |
|       35 |  6689 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       35 |  6690 | `	pVm->nRecursionDepth++;` |
|        - |  6691 | `	/* Resume execution from saved PC */` |
|       52 |  6692 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       17 |  6693 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       35 |  6694 | `	pVm->nRecursionDepth--;` |
|        - |  6695 | `	/* Restore the previous context */` |
|       35 |  6696 | `	pVm->pActiveCtx = pOldCtx;` |
|       35 |  6697 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6698 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       17 |  6699 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       17 |  6700 | `		pCtx->pFrame->pParent = 0;` |
|       17 |  6701 | `		if( pResult ){` |
|       17 |  6702 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  6703 | `		}` |
|       17 |  6704 | `		return SXRET_OK;` |
|        - |  6705 | `	}` |
|        - |  6706 | `	/* Detach frame */` |
|       19 |  6707 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       19 |  6708 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       19 |  6709 | `		pCtx->pFrame->pParent = 0;` |
|        9 |  6710 | `	}` |
|       19 |  6711 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6712 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6713 | `		return PH7_ABORT;` |
|        - |  6714 | `	}` |
|       19 |  6715 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6716 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6717 | `		return PH7_EXCEPTION;` |
|        - |  6718 | `	}` |
|        - |  6719 | `	/* Normal completion */` |
|       19 |  6720 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       19 |  6721 | `	if( pResult ){` |
|       19 |  6722 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  6723 | `	}` |
|       19 |  6724 | `	return SXRET_OK;` |
|       18 |  6725 |  |
|        - |  6726 | `/*` |
|        - |  6727 | ` * Release an execution context and all its resources.` |
|        - |  6728 | ` */` |
|      ! 0 |  6729 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|      ! 0 |  6730 |  |
|      ! 0 |  6731 | `	if( pCtx == 0 ){` |
|      ! 0 |  6732 | `		return;` |
|        - |  6733 | `	}` |
|      ! 0 |  6734 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  6735 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  6736 | `		return;` |
|        - |  6737 | `	}` |
|      ! 0 |  6738 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  6739 | `	/* Release values */` |
|      ! 0 |  6740 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|      ! 0 |  6741 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  6742 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|      ! 0 |  6743 | `	if( pCtx->pFrame ){` |
|        - |  6744 | `		VmSlot *aSlot;` |
|        - |  6745 | `		sxu32 n;` |
|        - |  6746 | `		/* Free local variables */` |
|      ! 0 |  6747 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6748 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|      ! 0 |  6749 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      ! 0 |  6750 | `		}` |
|        - |  6751 | `		/* Remove local references */` |
|      ! 0 |  6752 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|      ! 0 |  6753 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|      ! 0 |  6754 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      ! 0 |  6755 | `		}` |
|      ! 0 |  6756 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|      ! 0 |  6757 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|      ! 0 |  6758 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6759 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|      ! 0 |  6760 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|      ! 0 |  6761 | `		pCtx->pFrame = 0;` |
|      ! 0 |  6762 | `	}` |
|        - |  6763 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  6764 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  6765 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|      ! 0 |  6766 | `	if( pCtx->pStack ){` |
|      ! 0 |  6767 | `		if( pCtx->nTos >= 0 ){` |
|      ! 0 |  6768 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|      ! 0 |  6769 | `			while( pTos >= pCtx->pStack ){` |
|      ! 0 |  6770 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6771 | `				pTos--;` |
|      ! 0 |  6772 | `			}` |
|      ! 0 |  6773 | `		}` |
|      ! 0 |  6774 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|      ! 0 |  6775 | `		pCtx->pStack = 0;` |
|      ! 0 |  6776 | `	}` |
|        - |  6777 | `	/* Free the context itself */` |
|      ! 0 |  6778 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6779 |  |
|        - |  6780 | `/*` |
|        - |  6781 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  6782 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  6783 | ` */` |
|       86 |  6784 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        1 |  6785 |  |
|        - |  6786 | `	ph7_class_instance *pThis;` |
|        - |  6787 | `	SyString sAttr;` |
|        - |  6788 | `	ph7_value *pAttr;` |
|       87 |  6789 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6790 | `		return 0;` |
|        - |  6791 | `	}` |
|       87 |  6792 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       87 |  6793 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  6794 | `		return 0;` |
|        - |  6795 | `	}` |
|       87 |  6796 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       87 |  6797 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       87 |  6798 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       31 |  6799 | `		return 0;` |
|        - |  6800 | `	}` |
|       57 |  6801 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       44 |  6802 |  |
|        - |  6803 | `/*` |
|        - |  6804 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  6805 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  6806 | ` */` |
|       38 |  6807 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  6808 |  |
|       39 |  6809 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 |  6810 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  6811 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6812 | `			"Cannot suspend outside of a fiber");` |
|        - |  6813 | `	}` |
|       39 |  6814 | `	if( nArg > 0 ){` |
|       39 |  6815 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       20 |  6816 | `	}else{` |
|      ! 0 |  6817 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  6818 | `	}` |
|       39 |  6819 | `	return PH7_SUSPEND;` |
|       20 |  6820 |  |
|        - |  6821 | `/*` |
|        - |  6822 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  6823 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  6824 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  6825 | ` */` |
|       24 |  6826 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  6827 |  |
|        - |  6828 | `	ph7_class_instance *pThis;` |
|        - |  6829 | `	ph7_value *pAttr;` |
|        - |  6830 | `	SyString sAttrName;` |
|       25 |  6831 | `	if( nArg < 2 ){` |
|      ! 0 |  6832 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6833 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  6834 | `	}` |
|       25 |  6835 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6836 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6837 | `			"Fiber::__construct(): invalid $this");` |
|        - |  6838 | `	}` |
|       25 |  6839 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       25 |  6840 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  6841 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6842 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  6843 | `	}` |
|        - |  6844 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       25 |  6845 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  6846 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6847 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  6848 | `	}` |
|        - |  6849 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       25 |  6850 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  6851 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  6852 | `	if( pAttr ){` |
|       25 |  6853 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  6854 | `	}` |
|       25 |  6855 | `	return PH7_OK;` |
|       13 |  6856 |  |
|        - |  6857 | `/*` |
|        - |  6858 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  6859 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  6860 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  6861 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  6862 | ` */` |
|       24 |  6863 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  6864 | `	ph7_class_instance **ppThis)` |
|        1 |  6865 |  |
|       25 |  6866 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  6867 | `	ph7_value *pCallable;` |
|        - |  6868 | `	SyString sAttrName;` |
|       25 |  6869 | `	*ppThis = 0;` |
|       25 |  6870 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  6871 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       25 |  6872 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  6873 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  6874 | `		return 0;` |
|        - |  6875 | `	}` |
|       25 |  6876 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  6877 | `		/* String callable — look up in user functions with overload support */` |
|        - |  6878 | `		SyString sName;` |
|        - |  6879 | `		SyHashEntry *pEntry;` |
|        - |  6880 | `		ph7_vm_func *pFunc;` |
|       25 |  6881 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       25 |  6882 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       25 |  6883 | `		if( pEntry == 0 ){` |
|      ! 0 |  6884 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  6885 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  6886 | `			return 0;` |
|        - |  6887 | `		}` |
|       25 |  6888 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       25 |  6889 | `		return pFunc;` |
|      ! 0 |  6890 | `	}else{` |
|        - |  6891 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  6892 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  6893 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  6894 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  6895 | `		if( pMethod == 0 ){` |
|      ! 0 |  6896 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6897 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  6898 | `			return 0;` |
|        - |  6899 | `		}` |
|      ! 0 |  6900 | `		*ppThis = pClosure;` |
|      ! 0 |  6901 | `		return &pMethod->sFunc;` |
|        - |  6902 | `	}` |
|       13 |  6903 |  |
|        - |  6904 | `/*` |
|        - |  6905 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  6906 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  6907 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  6908 | ` */` |
|       24 |  6909 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  6910 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        1 |  6911 |  |
|       25 |  6912 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  6913 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  6914 | `	sxu32 nFormal, n;` |
|        - |  6915 | `	VmSlot sSlot;` |
|        - |  6916 | `	sxi32 rc;` |
|        - |  6917 | `	/* Install $this for closure callables */` |
|       25 |  6918 | `	if( pClosureThis ){` |
|        - |  6919 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  6920 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  6921 | `		if( pObj ){` |
|      ! 0 |  6922 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  6923 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  6924 | `		}` |
|      ! 0 |  6925 | `	}` |
|        - |  6926 | `	/* Install static variables */` |
|       25 |  6927 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  6928 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  6929 | `		ph7_value *pVal;` |
|      ! 0 |  6930 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  6931 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  6932 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  6933 | `			if( pVal ){` |
|      ! 0 |  6934 | `				sSlot.pUserData = 0;` |
|      ! 0 |  6935 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  6936 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  6937 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  6938 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  6939 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  6940 | `				}` |
|      ! 0 |  6941 | `			}` |
|      ! 0 |  6942 | `		}` |
|      ! 0 |  6943 | `	}` |
|        - |  6944 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       25 |  6945 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       25 |  6946 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       29 |  6947 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  6948 | `		ph7_value *pObj;` |
|        5 |  6949 | `		if( n < (sxu32)nArg ){` |
|        - |  6950 | `			/* Argument provided — install with type casting */` |
|        5 |  6951 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|        5 |  6952 | `			if( pObj ){` |
|        5 |  6953 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  6954 | `				/* Type casting */` |
|        5 |  6955 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  6956 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  6957 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  6958 | `						if( xCast ){` |
|      ! 0 |  6959 | `							xCast(pObj);` |
|      ! 0 |  6960 | `						}` |
|      ! 0 |  6961 | `					}` |
|      ! 0 |  6962 | `				}` |
|        5 |  6963 | `				sSlot.nIdx = pObj->nIdx;` |
|        5 |  6964 | `				sSlot.pUserData = 0;` |
|        5 |  6965 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        3 |  6966 | `			}` |
|        2 |  6967 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6968 | `			/* Default value */` |
|      ! 0 |  6969 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  6970 | `			if( pObj ){` |
|      ! 0 |  6971 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  6972 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6973 | `					return rc;` |
|        - |  6974 | `				}` |
|      ! 0 |  6975 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  6976 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  6977 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  6978 | `						if( xCast ){` |
|      ! 0 |  6979 | `							xCast(pObj);` |
|      ! 0 |  6980 | `						}` |
|      ! 0 |  6981 | `					}` |
|      ! 0 |  6982 | `				}` |
|      ! 0 |  6983 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  6984 | `				sSlot.pUserData = 0;` |
|      ! 0 |  6985 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  6986 | `			}` |
|      ! 0 |  6987 | `		}` |
|        3 |  6988 | `	}` |
|        - |  6989 | `	/* Install closure environment (captured variables) */` |
|       25 |  6990 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6991 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  6992 | `		ph7_value *pValue;` |
|        - |  6993 | `		sxu32 iEnv;` |
|        3 |  6994 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  6995 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  6996 | `			pEnv = &aEnv[iEnv];` |
|        7 |  6997 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  6998 | `				continue;` |
|        - |  6999 | `			}` |
|        5 |  7000 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7001 | `			if( pValue == 0 ){` |
|      ! 0 |  7002 | `				continue;` |
|        - |  7003 | `			}` |
|        5 |  7004 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7005 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7006 | `		}` |
|        1 |  7007 | `	}` |
|       25 |  7008 | `	return SXRET_OK;` |
|       13 |  7009 |  |
|        - |  7010 | `/*` |
|        - |  7011 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7012 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7013 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7014 | ` */` |
|       26 |  7015 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7016 |  |
|       27 |  7017 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7018 | `	ph7_class_instance *pThis;` |
|        - |  7019 | `	ph7_class_instance *pClosureThis;` |
|        - |  7020 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7021 | `	ph7_vm_func *pFunc;` |
|        - |  7022 | `	ph7_value sResult;` |
|        - |  7023 | `	ph7_value *pCtxAttr;` |
|        - |  7024 | `	SyString sAttrName;` |
|        - |  7025 | `	sxi32 rc;` |
|       27 |  7026 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7027 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7028 | `	}` |
|       27 |  7029 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7030 | `	/* Check if already started (has a __ctx) */` |
|       27 |  7031 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       27 |  7032 | `	if( pExecCtx != 0 ){` |
|        3 |  7033 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7034 | `			"Cannot start a fiber that has already been started");` |
|        - |  7035 | `	}` |
|        - |  7036 | `	/* Resolve callable */` |
|       25 |  7037 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       25 |  7038 | `	if( pFunc == 0 ){` |
|      ! 0 |  7039 | `		return PH7_EXCEPTION;` |
|        - |  7040 | `	}` |
|        - |  7041 | `	/* Create execution context now that we know the function */` |
|       25 |  7042 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       25 |  7043 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7044 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7045 | `			"Fiber::start(): out of memory");` |
|        - |  7046 | `	}` |
|        - |  7047 | `	/* Store context in $this->__ctx */` |
|       25 |  7048 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       25 |  7049 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7050 | `	if( pCtxAttr ){` |
|       25 |  7051 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       25 |  7052 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7053 | `	}` |
|        - |  7054 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7055 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7056 | `	 * into the fiber's frame, not the caller's. */` |
|       25 |  7057 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  7058 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7059 | `	/* Unpack the args array and install into the frame */` |
|        - |  7060 | `	{` |
|       25 |  7061 | `		ph7_value **apValues = 0;` |
|       25 |  7062 | `		int nActual = 0;` |
|       25 |  7063 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       25 |  7064 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7065 | `			ph7_hashmap_node *pNode;` |
|       25 |  7066 | `			sxu32 nCount = pMap->nEntry;` |
|       25 |  7067 | `			if( nCount > 0 ){` |
|        3 |  7068 | `				sxu32 idx = 0;` |
|        4 |  7069 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7070 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7071 | `				if( apValues ){` |
|        3 |  7072 | `					pNode = pMap->pFirst;` |
|        7 |  7073 | `					while( pNode && idx < nCount ){` |
|        5 |  7074 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7075 | `						idx++;` |
|        5 |  7076 | `						pNode = pNode->pPrev;` |
|        1 |  7077 | `					}` |
|        3 |  7078 | `					nActual = (int)idx;` |
|        1 |  7079 | `				}` |
|        1 |  7080 | `			}` |
|       12 |  7081 | `		}` |
|       25 |  7082 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       25 |  7083 | `		if( apValues ){` |
|        3 |  7084 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7085 | `		}` |
|        - |  7086 | `	}` |
|        - |  7087 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       25 |  7088 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       25 |  7089 | `	pExecCtx->pFrame->pParent = 0;` |
|       25 |  7090 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7091 | `		return PH7_ABORT;` |
|        - |  7092 | `	}` |
|       25 |  7093 | `	PH7_MemObjInit(pVm, &sResult);` |
|       25 |  7094 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       25 |  7095 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7096 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7097 | `		return PH7_ABORT;` |
|        - |  7098 | `	}` |
|       25 |  7099 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7100 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7101 | `		return PH7_EXCEPTION;` |
|        - |  7102 | `	}` |
|       25 |  7103 | `	ph7_result_value(pCtx, &sResult);` |
|       25 |  7104 | `	PH7_MemObjRelease(&sResult);` |
|       25 |  7105 | `	return PH7_OK;` |
|       14 |  7106 |  |
|        - |  7107 | `/*` |
|        - |  7108 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7109 | ` */` |
|       36 |  7110 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7111 |  |
|       37 |  7112 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7113 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7114 | `	ph7_value sResult;` |
|        - |  7115 | `	ph7_value *pResumeVal;` |
|        - |  7116 | `	sxi32 rc;` |
|       37 |  7117 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7118 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7119 | `		return PH7_OK;` |
|        - |  7120 | `	}` |
|       37 |  7121 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       37 |  7122 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7123 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7124 | `		return PH7_OK;` |
|        - |  7125 | `	}` |
|       37 |  7126 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7127 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7128 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7129 | `	}` |
|       35 |  7130 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       35 |  7131 | `	PH7_MemObjInit(pVm, &sResult);` |
|       35 |  7132 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       35 |  7133 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7134 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7135 | `		return PH7_ABORT;` |
|        - |  7136 | `	}` |
|       35 |  7137 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7138 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7139 | `		return PH7_EXCEPTION;` |
|        - |  7140 | `	}` |
|       35 |  7141 | `	ph7_result_value(pCtx, &sResult);` |
|       35 |  7142 | `	PH7_MemObjRelease(&sResult);` |
|       35 |  7143 | `	return PH7_OK;` |
|       19 |  7144 |  |
|        - |  7145 | `/*` |
|        - |  7146 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7147 | ` */` |
|        6 |  7148 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7149 |  |
|        7 |  7150 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7151 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7152 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7153 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7154 | `		return PH7_OK;` |
|        - |  7155 | `	}` |
|        7 |  7156 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        7 |  7157 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7158 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7159 | `		return PH7_OK;` |
|        - |  7160 | `	}` |
|        7 |  7161 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7162 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7163 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7164 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7165 | `		}` |
|      ! 0 |  7166 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7167 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7168 | `	}` |
|        7 |  7169 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        7 |  7170 | `	return PH7_OK;` |
|        4 |  7171 |  |
|        - |  7172 | `/*` |
|        - |  7173 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7174 | ` */` |
|        6 |  7175 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7176 |  |
|        - |  7177 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7178 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7179 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7180 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7181 | `	return PH7_OK;` |
|        4 |  7182 |  |
|      ! 0 |  7183 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7184 |  |
|        - |  7185 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7186 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7187 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7188 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7189 | `	return PH7_OK;` |
|      ! 0 |  7190 |  |
|        6 |  7191 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7192 |  |
|        - |  7193 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7194 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7195 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7196 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7197 | `	return PH7_OK;` |
|        4 |  7198 |  |
|        6 |  7199 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7200 |  |
|        - |  7201 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7202 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7203 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7204 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7205 | `	return PH7_OK;` |
|        4 |  7206 |  |
|        - |  7207 | `/*` |
|        - |  7208 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7209 | ` */` |
|      ! 0 |  7210 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7211 |  |
|      ! 0 |  7212 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7213 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7214 | `	if( nArg < 1 ){` |
|      ! 0 |  7215 | `		return PH7_OK;` |
|        - |  7216 | `	}` |
|      ! 0 |  7217 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      ! 0 |  7218 | `	if( pExecCtx ){` |
|      ! 0 |  7219 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7220 | `		/* Clear the attribute so double-free is prevented */` |
|      ! 0 |  7221 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7222 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7223 | `			SyString sAttrName;` |
|        - |  7224 | `			ph7_value *pAttr;` |
|      ! 0 |  7225 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7226 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7227 | `			if( pAttr ){` |
|      ! 0 |  7228 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7229 | `			}` |
|      ! 0 |  7230 | `		}` |
|      ! 0 |  7231 | `	}` |
|      ! 0 |  7232 | `	return PH7_OK;` |
|      ! 0 |  7233 |  |
|        - |  7234 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7235 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7236 |  |
|        - |  7237 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7238 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7239 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7240 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7241 |  |
|      ! 0 |  7242 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7243 |  |
|        - |  7244 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7245 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7246 | `	ph7_exec_ctx *pCtx;` |
|        - |  7247 | `	ph7_vm_func *pFunc;` |
|        - |  7248 | `	ph7_value *pCallable;` |
|        - |  7249 | `	ph7_value *pCtxAttr;` |
|        - |  7250 | `	SyString sAttrName;` |
|        - |  7251 | `	/* Must not already be started */` |
|      ! 0 |  7252 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7253 | `	if( pCtx != 0 ){` |
|      ! 0 |  7254 | `		return SXERR_INVALID;` |
|        - |  7255 | `	}` |
|      ! 0 |  7256 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7257 | `		return SXERR_INVALID;` |
|        - |  7258 | `	}` |
|      ! 0 |  7259 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7260 | `	/* Get the callable */` |
|      ! 0 |  7261 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7262 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7263 | `	if( pCallable == 0 ){` |
|      ! 0 |  7264 | `		return SXERR_INVALID;` |
|        - |  7265 | `	}` |
|        - |  7266 | `	/* Resolve callable */` |
|      ! 0 |  7267 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7268 | `		SyString sName;` |
|        - |  7269 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7270 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7271 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7272 | `		if( pEntry == 0 ){` |
|      ! 0 |  7273 | `			return SXERR_NOTFOUND;` |
|        - |  7274 | `		}` |
|      ! 0 |  7275 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7276 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7277 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7278 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7279 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7280 | `		if( pMethod == 0 ){` |
|      ! 0 |  7281 | `			return SXERR_INVALID;` |
|        - |  7282 | `		}` |
|      ! 0 |  7283 | `		pClosureThis = pClosure;` |
|      ! 0 |  7284 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7285 | `	}else{` |
|      ! 0 |  7286 | `		return SXERR_INVALID;` |
|        - |  7287 | `	}` |
|        - |  7288 | `	/* Create context */` |
|      ! 0 |  7289 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7290 | `	if( pCtx == 0 ){` |
|      ! 0 |  7291 | `		return SXERR_MEM;` |
|        - |  7292 | `	}` |
|        - |  7293 | `	/* Store in __ctx */` |
|      ! 0 |  7294 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7295 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7296 | `	if( pCtxAttr ){` |
|      ! 0 |  7297 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7298 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7299 | `	}` |
|        - |  7300 | `	/* Set up frame with args */` |
|      ! 0 |  7301 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7302 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7303 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7304 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7305 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7306 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7307 |  |
|      ! 0 |  7308 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7309 |  |
|      ! 0 |  7310 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7311 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7312 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7313 |  |
|      ! 0 |  7314 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7315 |  |
|      ! 0 |  7316 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7317 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7318 |  |
|      ! 0 |  7319 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7320 |  |
|      ! 0 |  7321 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7322 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7323 |  |
|      ! 0 |  7324 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7325 |  |
|      ! 0 |  7326 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7327 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7328 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7329 |  |
|        - |  7330 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7331 | `/*` |
|        - |  7332 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7333 | ` * the desired message.` |
|        - |  7334 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7335 | ` * in 'api.c' for additional information.` |
|        - |  7336 | ` */` |
|      350 |  7337 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7338 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7339 | `	SyString *pString /* Message to output */` |
|        - |  7340 | `	)` |
|        2 |  7341 |  |
|      352 |  7342 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  7343 | `	sxi32 rc = SXRET_OK;` |
|        - |  7344 | `	/* Call the output consumer */` |
|      352 |  7345 | `	if( pString->nByte > 0 ){` |
|      352 |  7346 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  7347 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  7348 | `	}` |
|      352 |  7349 | `	return rc;` |
|        2 |  7350 |  |
|        - |  7351 | `/*` |
|        - |  7352 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7353 | ` * callback to consume the formatted message.` |
|        - |  7354 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7355 | ` * in 'api.c' for additional information.` |
|        - |  7356 | ` */` |
|        2 |  7357 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7358 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7359 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7360 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7361 | `	)` |
|        1 |  7362 |  |
|        3 |  7363 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7364 | `	sxi32 rc = SXRET_OK;` |
|        - |  7365 | `	SyBlob sWorker;` |
|        - |  7366 | `	/* Format the message and call the output consumer */` |
|        3 |  7367 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7368 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7369 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7370 | `		/* Consume the formatted message */` |
|        3 |  7371 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7372 | `	}` |
|        3 |  7373 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7374 | `	/* Release the working buffer */` |
|        3 |  7375 | `	SyBlobRelease(&sWorker);` |
|        3 |  7376 | `	return rc;` |
|        1 |  7377 |  |
|        - |  7378 | `/*` |
|        - |  7379 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7380 | ` * This function never fail and always return a pointer` |
|        - |  7381 | ` * to a null terminated string.` |
|        - |  7382 | ` */` |
|       12 |  7383 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7384 |  |
|       13 |  7385 | `	const char *zOp = "Unknown     ";` |
|       13 |  7386 | `	switch(nOp){` |
|        3 |  7387 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7388 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7389 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7390 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7391 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7392 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7393 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7394 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7395 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7396 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7397 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  7398 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  7399 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  7400 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  7401 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  7402 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  7403 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  7404 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  7405 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  7406 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  7407 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  7408 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  7409 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  7410 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  7411 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  7412 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  7413 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  7414 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  7415 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  7416 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  7417 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  7418 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  7419 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  7420 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  7421 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  7422 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  7423 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  7424 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  7425 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  7426 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  7427 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  7428 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  7429 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  7430 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  7431 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  7432 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  7433 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  7434 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  7435 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  7436 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  7437 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  7438 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  7439 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  7440 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  7441 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  7442 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  7443 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  7444 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  7445 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  7446 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  7447 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  7448 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  7449 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  7450 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  7451 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  7452 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  7453 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  7454 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  7455 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  7456 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  7457 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  7458 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  7459 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  7460 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  7461 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  7462 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  7463 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  7464 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  7465 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  7466 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  7467 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  7468 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  7469 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  7470 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  7471 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  7472 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  7473 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  7474 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  7475 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  7476 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  7477 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  7478 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  7479 | `	default:` |
|      ! 0 |  7480 | `		break;` |
|        - |  7481 | `	}` |
|       13 |  7482 | `	return zOp;` |
|        1 |  7483 |  |
|        - |  7484 | `/*` |
|        - |  7485 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  7486 | ` * The xConsumer() callback which is an used defined function` |
|        - |  7487 | ` * is responsible of consuming the generated dump.` |
|        - |  7488 | ` */` |
|        2 |  7489 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  7490 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  7491 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  7492 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  7493 | `	)` |
|        1 |  7494 |  |
|        - |  7495 | `	sxi32 rc;` |
|        3 |  7496 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  7497 | `	return rc;` |
|        1 |  7498 |  |
|        - |  7499 | `/*` |
|        - |  7500 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  7501 | ` * outside a class body [i.e: global or function scope].` |
|        - |  7502 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  7503 | ` * in 'compile.c' for additional information.` |
|        - |  7504 | ` */` |
|        8 |  7505 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  7506 |  |
|        9 |  7507 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  7508 | `	/* Evaluate and expand constant value */` |
|        9 |  7509 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  7510 |  |
|        - |  7511 | `/*` |
|        - |  7512 | ` * Section:` |
|        - |  7513 | ` *  Function handling functions.` |
|        - |  7514 | ` * Status:` |
|        - |  7515 | ` *    Stable.` |
|        - |  7516 | ` */` |
|        - |  7517 | `/*` |
|        - |  7518 | ` * int func_num_args(void)` |
|        - |  7519 | ` *   Returns the number of arguments passed to the function.` |
|        - |  7520 | ` * Parameters` |
|        - |  7521 | ` *   None.` |
|        - |  7522 | ` * Return` |
|        - |  7523 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  7524 | ` *  or -1 if called from the globe scope.` |
|        - |  7525 | ` */` |
|      916 |  7526 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7527 |  |
|        - |  7528 | `	VmFrame *pFrame;` |
|        - |  7529 | `	ph7_vm *pVm;` |
|        - |  7530 | `	/* Point to the target VM */` |
|      918 |  7531 | `	pVm = pCtx->pVm;` |
|        - |  7532 | `	/* Current frame */` |
|      918 |  7533 | `	pFrame = pVm->pFrame;` |
|      918 |  7534 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      918 |  7535 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  7536 | `		SXUNUSED(nArg);` |
|      ! 0 |  7537 | `		SXUNUSED(apArg);` |
|        - |  7538 | `		/* Global frame,return -1 */` |
|      ! 0 |  7539 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  7540 | `		return SXRET_OK;` |
|        - |  7541 | `	}` |
|        - |  7542 | `	/* Total number of arguments passed to the enclosing function */` |
|      918 |  7543 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      918 |  7544 | `	ph7_result_int(pCtx,nArg);` |
|      918 |  7545 | `	return SXRET_OK;` |
|      460 |  7546 |  |
|        - |  7547 | `/*` |
|        - |  7548 | ` * value func_get_arg(int $arg_num)` |
|        - |  7549 | ` *   Return an item from the argument list.` |
|        - |  7550 | ` * Parameters` |
|        - |  7551 | ` *  Argument number(index start from zero).` |
|        - |  7552 | ` * Return` |
|        - |  7553 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  7554 | ` */` |
|       22 |  7555 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7556 |  |
|       24 |  7557 | `	ph7_value *pObj = 0;` |
|       24 |  7558 | `	VmSlot *pSlot = 0;` |
|        - |  7559 | `	VmFrame *pFrame;` |
|        - |  7560 | `	ph7_vm *pVm;` |
|        - |  7561 | `	/* Point to the target VM */` |
|       24 |  7562 | `	pVm = pCtx->pVm;` |
|        - |  7563 | `	/* Current frame */` |
|       24 |  7564 | `	pFrame = pVm->pFrame;` |
|       24 |  7565 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  7566 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  7567 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  7568 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  7569 | `		ph7_result_bool(pCtx,0);` |
|        3 |  7570 | `		return SXRET_OK;` |
|        - |  7571 | `	}` |
|        - |  7572 | `	/* Extract the desired index */` |
|       21 |  7573 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  7574 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  7575 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  7576 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7577 | `		return SXRET_OK;` |
|        - |  7578 | `	}` |
|        - |  7579 | `	/* Extract the desired argument */` |
|       21 |  7580 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  7581 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  7582 | `			/* Return the desired argument */` |
|       21 |  7583 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  7584 | `		}else{` |
|        - |  7585 | `			/* No such argument,return false */` |
|      ! 0 |  7586 | `			ph7_result_bool(pCtx,0);` |
|        - |  7587 | `		}` |
|       11 |  7588 | `	}else{` |
|        - |  7589 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  7590 | `		ph7_result_bool(pCtx,0);` |
|        - |  7591 | `	}` |
|       21 |  7592 | `	return SXRET_OK;` |
|       13 |  7593 |  |
|        - |  7594 | `/*` |
|        - |  7595 | ` * array func_get_args_byref(void)` |
|        - |  7596 | ` *   Returns an array comprising a function's argument list.` |
|        - |  7597 | ` * Parameters` |
|        - |  7598 | ` *  None.` |
|        - |  7599 | ` * Return` |
|        - |  7600 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  7601 | ` *  member of the current user-defined function's argument list.` |
|        - |  7602 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  7603 | ` * NOTE:` |
|        - |  7604 | ` *  Arguments are returned to the array by reference.` |
|        - |  7605 | ` */` |
|        2 |  7606 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7607 |  |
|        - |  7608 | `	ph7_value *pArray;` |
|        - |  7609 | `	VmFrame *pFrame;` |
|        - |  7610 | `	VmSlot *aSlot;` |
|        - |  7611 | `	sxu32 n;` |
|        - |  7612 | `	/* Point to the current frame */` |
|        3 |  7613 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  7614 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  7615 | `	if( pFrame->pParent == 0 ){` |
|        - |  7616 | `		/* Global frame,return FALSE */` |
|      ! 0 |  7617 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  7618 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7619 | `		return SXRET_OK;` |
|        - |  7620 | `	}` |
|        - |  7621 | `	/* Create a new array */` |
|        3 |  7622 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7623 | `	if( pArray == 0 ){` |
|      ! 0 |  7624 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7625 | `		SXUNUSED(apArg);` |
|      ! 0 |  7626 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7627 | `		return SXRET_OK;` |
|        - |  7628 | `	}` |
|        - |  7629 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  7630 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  7631 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  7632 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  7633 | `	}` |
|        - |  7634 | `	/* Return the freshly created array */` |
|        3 |  7635 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7636 | `	return SXRET_OK;` |
|        2 |  7637 |  |
|        - |  7638 | `/*` |
|        - |  7639 | ` * array func_get_args(void)` |
|        - |  7640 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  7641 | ` * Parameters` |
|        - |  7642 | ` *  None.` |
|        - |  7643 | ` * Return` |
|        - |  7644 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  7645 | ` *  member of the current user-defined function's argument list.` |
|        - |  7646 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  7647 | ` */` |
|       88 |  7648 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7649 |  |
|       90 |  7650 | `	ph7_value *pObj = 0;` |
|        - |  7651 | `	ph7_value *pArray;` |
|        - |  7652 | `	VmFrame *pFrame;` |
|        - |  7653 | `	VmSlot *aSlot;` |
|        - |  7654 | `	sxu32 n;` |
|        - |  7655 | `	/* Point to the current frame */` |
|       90 |  7656 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  7657 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  7658 | `	if( pFrame->pParent == 0 ){` |
|        - |  7659 | `		/* Global frame,return FALSE */` |
|      ! 0 |  7660 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  7661 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7662 | `		return SXRET_OK;` |
|        - |  7663 | `	}` |
|        - |  7664 | `	/* Create a new array */` |
|       90 |  7665 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  7666 | `	if( pArray == 0 ){` |
|      ! 0 |  7667 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7668 | `		SXUNUSED(apArg);` |
|      ! 0 |  7669 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7670 | `		return SXRET_OK;` |
|        - |  7671 | `	}` |
|        - |  7672 | `	/* Start filling the array with the given arguments */` |
|       90 |  7673 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  7674 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  7675 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  7676 | `		if( pObj ){` |
|      134 |  7677 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  7678 | `		}` |
|       68 |  7679 | `	}` |
|        - |  7680 | `	/* Return the freshly created array */` |
|       90 |  7681 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  7682 | `	return SXRET_OK;` |
|       46 |  7683 |  |
|        - |  7684 | `/*` |
|        - |  7685 | ` * bool function_exists(string $name)` |
|        - |  7686 | ` *  Return TRUE if the given function has been defined.` |
|        - |  7687 | ` * Parameters` |
|        - |  7688 | ` *  The name of the desired function.` |
|        - |  7689 | ` * Return` |
|        - |  7690 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  7691 | ` */` |
|     1668 |  7692 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7693 |  |
|        - |  7694 | `	const char *zName;` |
|        - |  7695 | `	ph7_vm *pVm;` |
|        - |  7696 | `	int nLen;` |
|        - |  7697 | `	int res;` |
|     1670 |  7698 | `	if( nArg < 1 ){` |
|        - |  7699 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  7700 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7701 | `		return SXRET_OK;` |
|        - |  7702 | `	}` |
|        - |  7703 | `	/* Point to the target VM */` |
|     1670 |  7704 | `	pVm = pCtx->pVm;` |
|        - |  7705 | `	/* Extract the function name */` |
|     1670 |  7706 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7707 | `	/* Assume the function is not defined */` |
|     1670 |  7708 | `	res = 0;` |
|        - |  7709 | `	/* Perform the lookup */` |
|     2502 |  7710 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1664 |  7711 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7712 | `			/* Function is defined */` |
|      206 |  7713 | `			res = 1;` |
|      102 |  7714 | `	}` |
|     1670 |  7715 | `	ph7_result_bool(pCtx,res);` |
|     1670 |  7716 | `	return SXRET_OK;` |
|      836 |  7717 |  |
|        - |  7718 | `/*` |
|        - |  7719 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  7720 | ` * [i.e: Whether it is callable or not].` |
|        - |  7721 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  7722 | ` */` |
|    16234 |  7723 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  7724 |  |
|    16236 |  7725 | `	int res = 0;` |
|    16236 |  7726 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7727 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  7728 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  7729 | `		ph7_class_method *pMethod;` |
|      ! 0 |  7730 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  7731 | `		if( pMethod && CallInvoke ){` |
|        - |  7732 | `			ph7_value sResult;` |
|        - |  7733 | `			sxi32 rc;` |
|        - |  7734 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  7735 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  7736 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  7737 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  7738 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  7739 | `			}` |
|      ! 0 |  7740 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7741 | `		}` |
|    16236 |  7742 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  7743 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  7744 | `		if( pMap->nEntry == 2 ){` |
|        - |  7745 | `			ph7_class *pClass;` |
|        - |  7746 | `			ph7_value *pV;` |
|        - |  7747 | `			/* Extract the target class */` |
|       12 |  7748 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  7749 | `			if( pV ){` |
|       12 |  7750 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  7751 | `				if( pClass ){` |
|        - |  7752 | `					ph7_class_method *pMethod;` |
|        - |  7753 | `					/* Extract the target method */` |
|       10 |  7754 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  7755 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  7756 | `						/* Perform the lookup */` |
|       10 |  7757 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  7758 | `						if( pMethod ){` |
|        - |  7759 | `							/* Method is callable */` |
|        5 |  7760 | `							res = 1;` |
|        2 |  7761 | `						}` |
|        4 |  7762 | `					}` |
|        4 |  7763 | `				}` |
|        5 |  7764 | `			}` |
|        7 |  7765 | `		}` |
|    16223 |  7766 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  7767 | `		const char *zName;` |
|        - |  7768 | `		int nLen;` |
|        - |  7769 | `		/* Extract the name */` |
|     4750 |  7770 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  7771 | `		/* Perform the lookup */` |
|     4765 |  7772 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  7773 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7774 | `				/* Function is callable */` |
|     4732 |  7775 | `				res = 1;` |
|     2365 |  7776 | `		}` |
|     2374 |  7777 | `	}` |
|    16236 |  7778 | `	return res;` |
|        2 |  7779 |  |
|        - |  7780 | `/*` |
|        - |  7781 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  7782 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  7783 | ` * Parameters` |
|        - |  7784 | ` * $name` |
|        - |  7785 | ` *    The callback function to check` |
|        - |  7786 | ` * $syntax_only` |
|        - |  7787 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  7788 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  7789 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  7790 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  7791 | ` *    a string.` |
|        - |  7792 | ` * Return` |
|        - |  7793 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  7794 | ` */` |
|       14 |  7795 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7796 |  |
|        - |  7797 | `	ph7_vm *pVm;` |
|        - |  7798 | `	int res;` |
|       15 |  7799 | `	if( nArg < 1 ){` |
|        - |  7800 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  7801 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7802 | `		return SXRET_OK;` |
|        - |  7803 | `	}` |
|        - |  7804 | `	/* Point to the target VM */` |
|       15 |  7805 | `	pVm = pCtx->pVm;` |
|        - |  7806 | `	/* Perform the requested operation */` |
|       15 |  7807 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  7808 | `	ph7_result_bool(pCtx,res);` |
|       15 |  7809 | `	return SXRET_OK;` |
|        8 |  7810 |  |
|        - |  7811 | `/*` |
|        - |  7812 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  7813 | ` * defined below.` |
|        - |  7814 | ` */` |
|     1136 |  7815 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7816 |  |
|     1137 |  7817 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7818 | `	ph7_value sName;` |
|        - |  7819 | `	sxi32 rc;` |
|        - |  7820 | `	/* Prepare the function name for insertion */` |
|     1137 |  7821 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1137 |  7822 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7823 | `	/* Perform the insertion */` |
|     1137 |  7824 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1137 |  7825 | `	PH7_MemObjRelease(&sName);` |
|     1137 |  7826 | `	return rc;` |
|        1 |  7827 |  |
|        - |  7828 | `/*` |
|        - |  7829 | ` * array get_defined_functions(void)` |
|        - |  7830 | ` *  Returns an array of all defined functions.` |
|        - |  7831 | ` * Parameter` |
|        - |  7832 | ` *  None.` |
|        - |  7833 | ` * Return` |
|        - |  7834 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  7835 | ` *  both built-in (internal) and user-defined.` |
|        - |  7836 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  7837 | ` *  defined ones using $arr["user"].` |
|        - |  7838 | ` * Note:` |
|        - |  7839 | ` *  NULL is returned on failure.` |
|        - |  7840 | ` */` |
|        2 |  7841 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7842 |  |
|        - |  7843 | `	ph7_value *pArray,*pEntry;` |
|        - |  7844 | `	/* NOTE:` |
|        - |  7845 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  7846 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  7847 | `	 */` |
|        3 |  7848 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7849 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7850 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7851 | `		SXUNUSED(apArg);` |
|        - |  7852 | `		/* Return NULL */` |
|      ! 0 |  7853 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7854 | `		return SXRET_OK;` |
|        - |  7855 | `	}` |
|        3 |  7856 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  7857 | `	if( pEntry == 0 ){` |
|        - |  7858 | `		/* Return NULL */` |
|      ! 0 |  7859 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7860 | `		return SXRET_OK;` |
|        - |  7861 | `	}` |
|        - |  7862 | `	/* Fill with the appropriate information */` |
|        3 |  7863 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  7864 | `	/* Create the 'internal' index */` |
|        3 |  7865 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  7866 | `	/* Create the user-func array */` |
|        3 |  7867 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  7868 | `	if( pEntry == 0 ){` |
|        - |  7869 | `		/* Return NULL */` |
|      ! 0 |  7870 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7871 | `		return SXRET_OK;` |
|        - |  7872 | `	}` |
|        - |  7873 | `	/* Fill with the appropriate information */` |
|        3 |  7874 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  7875 | `	/* Create the 'user' index */` |
|        3 |  7876 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  7877 | `	/* Return the multi-dimensional array */` |
|        3 |  7878 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7879 | `	return SXRET_OK;` |
|        2 |  7880 |  |
|        - |  7881 | `/*` |
|        - |  7882 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  7883 | ` *  Register a function for execution on shutdown.` |
|        - |  7884 | ` * Note` |
|        - |  7885 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  7886 | ` *  be called in the same order as they were registered.` |
|        - |  7887 | ` * Parameters` |
|        - |  7888 | ` *  $callback` |
|        - |  7889 | ` *   The shutdown callback to register.` |
|        - |  7890 | ` * $param` |
|        - |  7891 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  7892 | ` * Return` |
|        - |  7893 | ` *  Nothing.` |
|        - |  7894 | ` */` |
|        2 |  7895 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7896 |  |
|        - |  7897 | `	VmShutdownCB sEntry;` |
|        - |  7898 | `	int i,j;` |
|        3 |  7899 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7900 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  7901 | `		return PH7_OK;` |
|        - |  7902 | `	}` |
|        - |  7903 | `	/* Zero the Entry */` |
|        3 |  7904 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  7905 | `	/* Initialize fields */` |
|        3 |  7906 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  7907 | `	/* Save the callback name for later invocation name */` |
|        3 |  7908 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  7909 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  7910 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  7911 | `	}` |
|        - |  7912 | `	/* Copy arguments */` |
|        3 |  7913 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  7914 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  7915 | `			/* Limit reached */` |
|      ! 0 |  7916 | `			break;` |
|        - |  7917 | `		}` |
|      ! 0 |  7918 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  7919 | `	}` |
|        3 |  7920 | `	sEntry.nArg = j;` |
|        - |  7921 | `	/* Install the callback */` |
|        3 |  7922 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  7923 | `	return PH7_OK;` |
|        2 |  7924 |  |
|        - |  7925 | `/*` |
|        - |  7926 | ` * Section:` |
|        - |  7927 | ` *  Class handling functions.` |
|        - |  7928 | ` * Status:` |
|        - |  7929 | ` *    Stable.` |
|        - |  7930 | ` */` |
|        - |  7931 | `/*` |
|        - |  7932 | ` * Extract the top active class. NULL is returned` |
|        - |  7933 | ` * if the class stack is empty.` |
|        - |  7934 | ` */` |
|      556 |  7935 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  7936 |  |
|      558 |  7937 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  7938 | `	ph7_class **apClass;` |
|      558 |  7939 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  7940 | `		/* Empty stack,return NULL */` |
|       15 |  7941 | `		return 0;` |
|        - |  7942 | `	}` |
|        - |  7943 | `	/* Peek the last entry */` |
|      544 |  7944 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      544 |  7945 | `	return apClass[pSet->nUsed - 1];` |
|      280 |  7946 |  |
|        - |  7947 | `/*` |
|        - |  7948 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7949 | ` *   Get the class that declared the currently executing method.` |
|        - |  7950 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7951 | ` *` |
|        - |  7952 | ` * Parameters` |
|        - |  7953 | ` *   pVm: Target VM` |
|        - |  7954 | ` *` |
|        - |  7955 | ` * Return` |
|        - |  7956 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7957 | ` *   - Not executing within a class method` |
|        - |  7958 | ` *` |
|        - |  7959 | ` * Note` |
|        - |  7960 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7961 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7962 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7963 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7964 | ` *   declaring class.` |
|        - |  7965 | ` */` |
|       52 |  7966 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7967 |  |
|       54 |  7968 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7969 | `	ph7_vm_func *pVmFunc;` |
|        - |  7970 |  |
|        - |  7971 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  7972 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  7973 |  |
|        - |  7974 | `	/* Check if we're in a method context */` |
|       54 |  7975 | `	if( pFrame->pParent ){` |
|       50 |  7976 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  7977 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7978 | `			/* Return the declaring class */` |
|       50 |  7979 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7980 | `		}` |
|      ! 0 |  7981 | `	}` |
|        - |  7982 |  |
|        5 |  7983 | `	return 0;` |
|       28 |  7984 |  |
|        - |  7985 |  |
|        - |  7986 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7987 | `/*` |
|        - |  7988 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7989 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7990 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7991 | ` * return value indicates failure.` |
|        - |  7992 | ` */` |
|     1328 |  7993 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7994 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7995 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7996 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7997 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7998 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7999 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8000 | `	)` |
|        2 |  8001 |  |
|        - |  8002 | `	ph7_value *aStack;` |
|        - |  8003 | `	VmInstr aInstr[2];` |
|        - |  8004 | `	int iCursor;` |
|        - |  8005 | `	int i;` |
|        - |  8006 | `	/* Create a new operand stack */` |
|     1330 |  8007 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1330 |  8008 | `	if( aStack == 0 ){` |
|      ! 0 |  8009 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8010 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8011 | `		return SXERR_MEM;` |
|        - |  8012 | `	}` |
|        - |  8013 | `	/* Fill the operand stack with the given arguments */` |
|     1932 |  8014 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      604 |  8015 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8016 | `		/*` |
|        - |  8017 | `		 * Symisc eXtension:` |
|        - |  8018 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8019 | `		 */` |
|      604 |  8020 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      303 |  8021 | `	}` |
|     1330 |  8022 | `	iCursor = nArg + 1;` |
|     1330 |  8023 | `	if( pThis ){` |
|        - |  8024 | `		/*` |
|        - |  8025 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8026 | `		 */` |
|     1324 |  8027 | `		pThis->iRef++; /* Increment reference count */` |
|     1324 |  8028 | `		aStack[i].x.pOther = pThis;` |
|     1324 |  8029 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      661 |  8030 | `	}` |
|     1330 |  8031 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1330 |  8032 | `	i++;` |
|        - |  8033 | `	/* Push method name */` |
|     1330 |  8034 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1330 |  8035 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1330 |  8036 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1330 |  8037 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8038 | `	/* Emit the CALL istruction */` |
|     1330 |  8039 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1330 |  8040 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1330 |  8041 | `	aInstr[0].iP2 = 0;` |
|     1330 |  8042 | `	aInstr[0].p3  = 0;` |
|        - |  8043 | `	/* Emit the DONE instruction */` |
|     1330 |  8044 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1330 |  8045 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1330 |  8046 | `	aInstr[1].iP2 = 0;` |
|     1330 |  8047 | `	aInstr[1].p3  = 0;` |
|        - |  8048 | `	/* Execute the method body (if available) */` |
|     1330 |  8049 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8050 | `	/* Clean up the mess left behind */` |
|     1330 |  8051 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1330 |  8052 | `	return PH7_OK;` |
|      666 |  8053 |  |
|        - |  8054 | `/*` |
|        - |  8055 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8056 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8057 | ` * in the apArg[] array.` |
|        - |  8058 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8059 | ` * return value indicates failure.` |
|        - |  8060 | ` */` |
|      926 |  8061 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8062 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8063 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8064 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8065 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8066 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8067 | `	)` |
|        2 |  8068 |  |
|        - |  8069 | `	ph7_value *aStack;` |
|        - |  8070 | `	VmInstr aInstr[2];` |
|        - |  8071 | `	int i;` |
|      928 |  8072 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8073 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8074 | `		if( pResult ){` |
|        - |  8075 | `			/* Assume a null return value */` |
|      ! 0 |  8076 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8077 | `		}` |
|      471 |  8078 | `		return SXERR_INVALID;` |
|        - |  8079 | `	}` |
|      458 |  8080 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8081 | `		/* Class method */` |
|       11 |  8082 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8083 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8084 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8085 | `		ph7_class *pClass = 0;` |
|        - |  8086 | `		ph7_value *pValue;` |
|        - |  8087 | `		sxi32 rc;` |
|       11 |  8088 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8089 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8090 | `			if( pResult ){` |
|        - |  8091 | `				/* Assume a null return value */` |
|      ! 0 |  8092 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8093 | `			}` |
|      ! 0 |  8094 | `			return SXRET_OK;` |
|        - |  8095 | `		}` |
|        - |  8096 | `		/* Extract the class name or an instance of it */` |
|       11 |  8097 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8098 | `		if( pValue ){` |
|       11 |  8099 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8100 | `		}` |
|       11 |  8101 | `		if( pClass == 0 ){` |
|        - |  8102 | `			/* No such class,return NULL */` |
|      ! 0 |  8103 | `			if( pResult ){` |
|      ! 0 |  8104 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8105 | `			}` |
|      ! 0 |  8106 | `			return SXRET_OK;` |
|        - |  8107 | `		}` |
|       11 |  8108 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8109 | `			/* Point to the class instance */` |
|        5 |  8110 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8111 | `		}` |
|        - |  8112 | `		/* Try to extract the method */` |
|       11 |  8113 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8114 | `		if( pValue ){` |
|       11 |  8115 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8116 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8117 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8118 | `			}` |
|        5 |  8119 | `		}` |
|       11 |  8120 | `		if( pMethod == 0 ){` |
|        - |  8121 | `			/* No such method,return NULL */` |
|      ! 0 |  8122 | `			if( pResult ){` |
|      ! 0 |  8123 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8124 | `			}` |
|      ! 0 |  8125 | `			return SXRET_OK;` |
|        - |  8126 | `		}` |
|        - |  8127 | `		/* Call the class method */` |
|       11 |  8128 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8129 | `		return rc;` |
|        - |  8130 | `	}` |
|        - |  8131 | `	/* Create a new operand stack */` |
|      448 |  8132 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  8133 | `	if( aStack == 0 ){` |
|      ! 0 |  8134 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8135 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8136 | `		if( pResult ){` |
|        - |  8137 | `			/* Assume a null return value */` |
|      ! 0 |  8138 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8139 | `		}` |
|      ! 0 |  8140 | `		return SXERR_MEM;` |
|        - |  8141 | `	}` |
|        - |  8142 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  8143 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  8144 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8145 | `		/*` |
|        - |  8146 | `		 * Symisc eXtension:` |
|        - |  8147 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8148 | `		 */` |
|     1024 |  8149 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  8150 | `	}` |
|        - |  8151 | `	/* Push the function name */` |
|      448 |  8152 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  8153 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8154 | `	/* Emit the CALL istruction */` |
|      448 |  8155 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  8156 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  8157 | `	aInstr[0].iP2 = 0;` |
|      448 |  8158 | `	aInstr[0].p3  = 0;` |
|        - |  8159 | `	/* Emit the DONE instruction */` |
|      448 |  8160 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  8161 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  8162 | `	aInstr[1].iP2 = 0;` |
|      448 |  8163 | `	aInstr[1].p3  = 0;` |
|        - |  8164 | `	/* Execute the function body (if available) */` |
|      448 |  8165 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8166 | `	/* Clean up the mess left behind */` |
|      448 |  8167 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  8168 | `	return PH7_OK;` |
|      465 |  8169 |  |
|        - |  8170 | `/*` |
|        - |  8171 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8172 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8173 | ` * parameter.` |
|        - |  8174 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8175 | ` * return value indicates failure.` |
|        - |  8176 | ` */` |
|      236 |  8177 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8178 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8179 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8180 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8181 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8182 | `	)` |
|        1 |  8183 |  |
|        - |  8184 | `	ph7_value *pArg;` |
|        - |  8185 | `	SySet aArg;` |
|        - |  8186 | `	va_list ap;` |
|        - |  8187 | `	sxi32 rc;` |
|      237 |  8188 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8189 | `	/* Copy arguments one after one */` |
|      237 |  8190 | `	va_start(ap,pResult);` |
|      393 |  8191 | `	for(;;){` |
|      787 |  8192 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8193 | `		if( pArg == 0 ){` |
|      237 |  8194 | `			break;` |
|        - |  8195 | `		}` |
|      551 |  8196 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8197 | `	}` |
|        - |  8198 | `	/* Call the core routine */` |
|      237 |  8199 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8200 | `	/* Cleanup */` |
|      237 |  8201 | `	SySetRelease(&aArg);` |
|      237 |  8202 | `	return rc;` |
|        1 |  8203 |  |
|        - |  8204 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8205 | `/*` |
|        - |  8206 | ` * bool defined(string $name)` |
|        - |  8207 | ` *  Checks whether a given named constant exists.` |
|        - |  8208 | ` * Parameter:` |
|        - |  8209 | ` *  Name of the desired constant.` |
|        - |  8210 | ` * Return` |
|        - |  8211 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8212 | ` */` |
|       14 |  8213 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8214 |  |
|        - |  8215 | `	const char *zName;` |
|       16 |  8216 | `	int nLen = 0;` |
|       16 |  8217 | `	int res = 0;` |
|       16 |  8218 | `	if( nArg < 1 ){` |
|        - |  8219 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8220 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8221 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8222 | `		return SXRET_OK;` |
|        - |  8223 | `	}` |
|        - |  8224 | `	/* Extract constant name */` |
|       16 |  8225 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8226 | `	/* Perform the lookup */` |
|       16 |  8227 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8228 | `		/* Already defined */` |
|       10 |  8229 | `		res = 1;` |
|        4 |  8230 | `	}` |
|       16 |  8231 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8232 | `	return SXRET_OK;` |
|        9 |  8233 |  |
|        - |  8234 | `/*` |
|        - |  8235 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8236 | ` * below.` |
|        - |  8237 | ` */` |
|        8 |  8238 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8239 |  |
|       10 |  8240 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8241 | `	/* Expand constant value */` |
|       10 |  8242 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8243 |  |
|        - |  8244 | `/*` |
|        - |  8245 | ` * bool define(string $constant_name,expression value)` |
|        - |  8246 | ` *  Defines a named constant at runtime.` |
|        - |  8247 | ` * Parameter:` |
|        - |  8248 | ` *  $constant_name` |
|        - |  8249 | ` *   The name of the constant` |
|        - |  8250 | ` *  $value` |
|        - |  8251 | ` *   Constant value` |
|        - |  8252 | ` * Return:` |
|        - |  8253 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8254 | ` */` |
|       10 |  8255 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8256 |  |
|        - |  8257 | `	const char *zName;  /* Constant name */` |
|        - |  8258 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8259 | `	int nLen = 0;       /* Name length */` |
|        - |  8260 | `	sxi32 rc;` |
|       12 |  8261 | `	if( nArg < 2 ){` |
|        - |  8262 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8263 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8264 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8265 | `		return SXRET_OK;` |
|        - |  8266 | `	}` |
|       12 |  8267 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8268 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8269 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8270 | `		return SXRET_OK;` |
|        - |  8271 | `	}` |
|        - |  8272 | `	/* Extract constant name */` |
|       12 |  8273 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8274 | `	if( nLen < 1 ){` |
|      ! 0 |  8275 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8276 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8277 | `		return SXRET_OK;` |
|        - |  8278 | `	}` |
|        - |  8279 | `	/* Duplicate constant value */` |
|       12 |  8280 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8281 | `	if( pValue == 0 ){` |
|      ! 0 |  8282 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8283 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8284 | `		return SXRET_OK;` |
|        - |  8285 | `	}` |
|        - |  8286 | `	/* Initialize the memory object */` |
|       12 |  8287 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8288 | `	/* Register the constant */` |
|       12 |  8289 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8290 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8291 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8292 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8293 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8294 | `		return SXRET_OK;` |
|        - |  8295 | `	}` |
|        - |  8296 | `	/* Duplicate constant value */` |
|       12 |  8297 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8298 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8299 | `		/* Lower case the constant name */` |
|      ! 0 |  8300 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8301 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8302 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8303 | `				/* UTF-8 stream */` |
|      ! 0 |  8304 | `				zCur++;` |
|      ! 0 |  8305 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8306 | `					zCur++;` |
|      ! 0 |  8307 | `				}` |
|      ! 0 |  8308 | `				continue;` |
|        - |  8309 | `			}` |
|      ! 0 |  8310 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8311 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8312 | `				zCur[0] = (char)c;` |
|      ! 0 |  8313 | `			}` |
|      ! 0 |  8314 | `			zCur++;` |
|      ! 0 |  8315 | `		}` |
|        - |  8316 | `		/* Finally,register the constant */` |
|      ! 0 |  8317 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8318 | `	}` |
|        - |  8319 | `	/* All done,return TRUE */` |
|       12 |  8320 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8321 | `	return SXRET_OK;` |
|        7 |  8322 |  |
|        - |  8323 | `/*` |
|        - |  8324 | ` * value constant(string $name)` |
|        - |  8325 | ` *  Returns the value of a constant` |
|        - |  8326 | ` * Parameter` |
|        - |  8327 | ` *  $name` |
|        - |  8328 | ` *    Name of the constant.` |
|        - |  8329 | ` * Return` |
|        - |  8330 | ` *  Constant value or NULL if not defined.` |
|        - |  8331 | ` */` |
|        8 |  8332 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8333 |  |
|        - |  8334 | `	SyHashEntry *pEntry;` |
|        - |  8335 | `	ph7_constant *pCons;` |
|        - |  8336 | `	const char *zName; /* Constant name */` |
|        - |  8337 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8338 | `	int nLen;` |
|       10 |  8339 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8340 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8341 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8342 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8343 | `		return SXRET_OK;` |
|        - |  8344 | `	}` |
|        - |  8345 | `	/* Extract the constant name */` |
|       10 |  8346 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8347 | `	/* Perform the query */` |
|       10 |  8348 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8349 | `	if( pEntry == 0 ){` |
|        3 |  8350 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8351 | `		ph7_result_null(pCtx);` |
|        3 |  8352 | `		return SXRET_OK;` |
|        - |  8353 | `	}` |
|        8 |  8354 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8355 | `	/* Point to the structure that describe the constant */` |
|        8 |  8356 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8357 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8358 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8359 | `	/* Return that value */` |
|        8 |  8360 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8361 | `	/* Cleanup */` |
|        8 |  8362 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8363 | `	return SXRET_OK;` |
|        6 |  8364 |  |
|        - |  8365 | `/*` |
|        - |  8366 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8367 | ` * defined below.` |
|        - |  8368 | ` */` |
|      416 |  8369 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8370 |  |
|      417 |  8371 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8372 | `	ph7_value sName;` |
|        - |  8373 | `	sxi32 rc;` |
|        - |  8374 | `	/* Prepare the constant name for insertion */` |
|      417 |  8375 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  8376 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8377 | `	/* Perform the insertion */` |
|      417 |  8378 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  8379 | `	PH7_MemObjRelease(&sName);` |
|      417 |  8380 | `	return rc;` |
|        1 |  8381 |  |
|        - |  8382 | `/*` |
|        - |  8383 | ` * array get_defined_constants(void)` |
|        - |  8384 | ` *  Returns an associative array with the names of all defined` |
|        - |  8385 | ` *  constants.` |
|        - |  8386 | ` * Parameters` |
|        - |  8387 | ` *  NONE.` |
|        - |  8388 | ` * Returns` |
|        - |  8389 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8390 | ` */` |
|        2 |  8391 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8392 |  |
|        - |  8393 | `	ph7_value *pArray;` |
|        - |  8394 | `	/* Create the array first*/` |
|        3 |  8395 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8396 | `	if( pArray == 0 ){` |
|      ! 0 |  8397 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8398 | `		SXUNUSED(apArg);` |
|        - |  8399 | `		/* Return NULL */` |
|      ! 0 |  8400 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8401 | `		return SXRET_OK;` |
|        - |  8402 | `	}` |
|        - |  8403 | `	/* Fill the array with the defined constants */` |
|        3 |  8404 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8405 | `	/* Return the created array */` |
|        3 |  8406 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8407 | `	return SXRET_OK;` |
|        2 |  8408 |  |
|        - |  8409 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  8410 | `/*` |
|        - |  8411 | ` * Section:` |
|        - |  8412 | ` *  Random numbers/string generators.` |
|        - |  8413 | ` * Status:` |
|        - |  8414 | ` *    Stable.` |
|        - |  8415 | ` */` |
|        - |  8416 | `/*` |
|        - |  8417 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8418 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8419 | ` * used by te SQLite3 library.` |
|        - |  8420 | ` */` |
|     2504 |  8421 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8422 |  |
|        - |  8423 | `	sxu32 iNum;` |
|     2506 |  8424 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2506 |  8425 | `	return iNum;` |
|        2 |  8426 |  |
|        - |  8427 | `/*` |
|        - |  8428 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8429 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8430 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8431 | ` * by te SQLite3 library.` |
|        - |  8432 | ` */` |
|   105638 |  8433 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8434 |  |
|        - |  8435 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8436 | `	int i;` |
|        - |  8437 | `	/* Generate a binary string first */` |
|   105640 |  8438 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8439 | `	/* Turn the binary string into english based alphabet */` |
|  1162188 |  8440 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1056550 |  8441 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   528276 |  8442 | `	 }` |
|   105640 |  8443 |  |
|        - |  8444 | `/*` |
|        - |  8445 | ` * int rand()` |
|        - |  8446 | ` * int mt_rand()` |
|        - |  8447 | ` * int rand(int $min,int $max)` |
|        - |  8448 | ` * int mt_rand(int $min,int $max)` |
|        - |  8449 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8450 | ` * Parameter` |
|        - |  8451 | ` *  $min` |
|        - |  8452 | ` *    The lowest value to return (default: 0)` |
|        - |  8453 | ` *  $max` |
|        - |  8454 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8455 | ` * Return` |
|        - |  8456 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8457 | ` * Note:` |
|        - |  8458 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8459 | ` *  by te SQLite3 library.` |
|        - |  8460 | ` */` |
|       20 |  8461 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8462 |  |
|        - |  8463 | `	sxu32 iNum;` |
|        - |  8464 | `	/* Generate the random number */` |
|       21 |  8465 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8466 | `	if( nArg > 1 ){` |
|        - |  8467 | `		sxu32 iMin,iMax;` |
|        3 |  8468 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8469 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8470 | `		if( iMin < iMax ){` |
|        3 |  8471 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8472 | `			if( iDiv > 0 ){` |
|        3 |  8473 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8474 | `			}` |
|        1 |  8475 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8476 | `			iNum %= iMax;` |
|      ! 0 |  8477 | `		}` |
|        1 |  8478 | `	}` |
|        - |  8479 | `	/* Return the number */` |
|       21 |  8480 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8481 | `	return SXRET_OK;` |
|        1 |  8482 |  |
|        - |  8483 | `/*` |
|        - |  8484 | ` * int getrandmax(void)` |
|        - |  8485 | ` * int mt_getrandmax(void)` |
|        - |  8486 | ` * int rc4_getrandmax(void)` |
|        - |  8487 | ` *   Show largest possible random value` |
|        - |  8488 | ` * Return` |
|        - |  8489 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8490 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8491 | ` * Note:` |
|        - |  8492 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8493 | ` *  by te SQLite3 library.` |
|        - |  8494 | ` */` |
|        4 |  8495 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8496 |  |
|        2 |  8497 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8498 | `	SXUNUSED(apArg);` |
|        5 |  8499 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8500 | `	return SXRET_OK;` |
|        1 |  8501 |  |
|        - |  8502 | `/*` |
|        - |  8503 | ` * string rand_str()` |
|        - |  8504 | ` * string rand_str(int $len)` |
|        - |  8505 | ` *  Generate a random string (English alphabet).` |
|        - |  8506 | ` * Parameter` |
|        - |  8507 | ` *  $len` |
|        - |  8508 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8509 | ` * Return` |
|        - |  8510 | ` *   A pseudo random string.` |
|        - |  8511 | ` * Note:` |
|        - |  8512 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8513 | ` *  by te SQLite3 library.` |
|        - |  8514 | ` *  This function is a symisc extension.` |
|        - |  8515 | ` */` |
|      120 |  8516 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8517 |  |
|        - |  8518 | `	char zString[1024];` |
|      122 |  8519 | `	int iLen = 0x10;` |
|      122 |  8520 | `	if( nArg > 0 ){` |
|        - |  8521 | `		/* Get the desired length */` |
|      122 |  8522 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8523 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8524 | `			/* Default length */` |
|        3 |  8525 | `			iLen = 0x10;` |
|        1 |  8526 | `		}` |
|       60 |  8527 | `	}` |
|        - |  8528 | `	/* Generate the random string */` |
|      122 |  8529 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8530 | `	/* Return the generated string */` |
|      122 |  8531 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8532 | `	return SXRET_OK;` |
|        2 |  8533 |  |
|        - |  8534 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8535 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8536 | `/* Unique ID private data */` |
|        - |  8537 | `struct unique_id_data` |
|        - |  8538 |  |
|        - |  8539 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8540 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  8541 | `};` |
|        - |  8542 | `/*` |
|        - |  8543 | ` * Binary to hex consumer callback.` |
|        - |  8544 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  8545 | ` * defined below.` |
|        - |  8546 | ` */` |
|      192 |  8547 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  8548 |  |
|      193 |  8549 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  8550 | `	sxu32 nBuflen;` |
|        - |  8551 | `	/* Extract result buffer length */` |
|      193 |  8552 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  8553 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  8554 | `			/*` |
|        - |  8555 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  8556 | `			 * string will be 13 characters long` |
|        - |  8557 | `			 */` |
|       25 |  8558 | `		return SXERR_ABORT;` |
|        - |  8559 | `	}` |
|      169 |  8560 | `	if( nBuflen > 22 ){` |
|      ! 0 |  8561 | `		return SXERR_ABORT;` |
|        - |  8562 | `	}` |
|        - |  8563 | `	/* Safely Consume the hex stream */` |
|      169 |  8564 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  8565 | `	return SXRET_OK;` |
|       97 |  8566 |  |
|        - |  8567 | `/*` |
|        - |  8568 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  8569 | ` *  Generate a unique ID` |
|        - |  8570 | ` * Parameter` |
|        - |  8571 | ` * $prefix` |
|        - |  8572 | ` *  Append this prefix to the generated unique ID.` |
|        - |  8573 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  8574 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  8575 | ` * $more_entropy` |
|        - |  8576 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  8577 | ` *  that the result will be unique.` |
|        - |  8578 | ` * Return` |
|        - |  8579 | ` *  Returns the unique identifier, as a string.` |
|        - |  8580 | ` */` |
|       24 |  8581 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8582 |  |
|        - |  8583 | `	struct unique_id_data sUniq;` |
|        - |  8584 | `	unsigned char zDigest[20];` |
|       25 |  8585 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8586 | `	const char *zPrefix;` |
|        - |  8587 | `	SHA1Context sCtx;` |
|        - |  8588 | `	char zRandom[7];` |
|        - |  8589 | `	int nPrefix;` |
|        - |  8590 | `	int entropy;` |
|        - |  8591 | `	/* Generate a random string first */` |
|       25 |  8592 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  8593 | `	/* Initialize fields */` |
|       25 |  8594 | `	zPrefix = 0;` |
|       25 |  8595 | `	nPrefix = 0;` |
|       25 |  8596 | `	entropy = 0;` |
|       25 |  8597 | `	if( nArg > 0 ){` |
|        - |  8598 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  8599 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  8600 | `		if( nArg > 1 ){` |
|      ! 0 |  8601 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  8602 | `		}` |
|      ! 0 |  8603 | `	}` |
|       25 |  8604 | `	SHA1Init(&sCtx);` |
|        - |  8605 | `	/* Generate the random ID */` |
|       25 |  8606 | `	if( nPrefix > 0 ){` |
|      ! 0 |  8607 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  8608 | `	}` |
|        - |  8609 | `	/* Append the random ID */` |
|       25 |  8610 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  8611 | `	/* Append the random string */` |
|       25 |  8612 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  8613 | `	/* Increment the number */` |
|       25 |  8614 | `	pVm->unique_id++;` |
|       25 |  8615 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  8616 | `	/* Hexify the digest */` |
|       25 |  8617 | `	sUniq.pCtx = pCtx;` |
|       25 |  8618 | `	sUniq.entropy = entropy;` |
|       25 |  8619 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  8620 | `	/* All done */` |
|       25 |  8621 | `	return PH7_OK;` |
|        1 |  8622 |  |
|        - |  8623 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  8624 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  8625 | `/*` |
|        - |  8626 | ` * Section:` |
|        - |  8627 | ` *  Language construct implementation as foreign functions.` |
|        - |  8628 | ` * Status:` |
|        - |  8629 | ` *    Stable.` |
|        - |  8630 | ` */` |
|        - |  8631 | `/*` |
|        - |  8632 | ` * void echo($string...)` |
|        - |  8633 | ` *  Output one or more messages.` |
|        - |  8634 | ` * Parameters` |
|        - |  8635 | ` *  $string` |
|        - |  8636 | ` *   Message to output.` |
|        - |  8637 | ` * Return` |
|        - |  8638 | ` *  NULL.` |
|        - |  8639 | ` */` |
|      ! 0 |  8640 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8641 |  |
|        - |  8642 | `	const char *zData;` |
|      ! 0 |  8643 | `	int nDataLen = 0;` |
|        - |  8644 | `	ph7_vm *pVm;` |
|        - |  8645 | `	int i,rc;` |
|        - |  8646 | `	/* Point to the target VM */` |
|      ! 0 |  8647 | `	pVm = pCtx->pVm;` |
|        - |  8648 | `	/* Output */` |
|      ! 0 |  8649 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  8650 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  8651 | `		if( nDataLen > 0 ){` |
|      ! 0 |  8652 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  8653 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  8654 | `			if( rc == SXERR_ABORT ){` |
|        - |  8655 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8656 | `				return PH7_ABORT;` |
|        - |  8657 | `			}` |
|      ! 0 |  8658 | `		}` |
|      ! 0 |  8659 | `	}` |
|      ! 0 |  8660 | `	return SXRET_OK;` |
|      ! 0 |  8661 |  |
|        - |  8662 | `/*` |
|        - |  8663 | ` * int print($string...)` |
|        - |  8664 | ` *  Output one or more messages.` |
|        - |  8665 | ` * Parameters` |
|        - |  8666 | ` *  $string` |
|        - |  8667 | ` *   Message to output.` |
|        - |  8668 | ` * Return` |
|        - |  8669 | ` *  1 always.` |
|        - |  8670 | ` */` |
|        2 |  8671 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8672 |  |
|        - |  8673 | `	const char *zData;` |
|        3 |  8674 | `	int nDataLen = 0;` |
|        - |  8675 | `	ph7_vm *pVm;` |
|        - |  8676 | `	int i,rc;` |
|        - |  8677 | `	/* Point to the target VM */` |
|        3 |  8678 | `	pVm = pCtx->pVm;` |
|        - |  8679 | `	/* Output */` |
|        5 |  8680 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  8681 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  8682 | `		if( nDataLen > 0 ){` |
|        3 |  8683 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  8684 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  8685 | `			if( rc == SXERR_ABORT ){` |
|        - |  8686 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  8687 | `				return PH7_ABORT;` |
|        - |  8688 | `			}` |
|        1 |  8689 | `		}` |
|        2 |  8690 | `	}` |
|        - |  8691 | `	/* Return 1 */` |
|        3 |  8692 | `	ph7_result_int(pCtx,1);` |
|        3 |  8693 | `	return SXRET_OK;` |
|        2 |  8694 |  |
|        - |  8695 | `/*` |
|        - |  8696 | ` * void exit(string $msg)` |
|        - |  8697 | ` * void exit(int $status)` |
|        - |  8698 | ` * void die(string $ms)` |
|        - |  8699 | ` * void die(int $status)` |
|        - |  8700 | ` *   Output a message and terminate program execution.` |
|        - |  8701 | ` * Parameter` |
|        - |  8702 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  8703 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  8704 | ` *  and not printed` |
|        - |  8705 | ` * Return` |
|        - |  8706 | ` *  NULL` |
|        - |  8707 | ` */` |
|      ! 0 |  8708 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  8709 |  |
|      ! 0 |  8710 | `	if( nArg > 0 ){` |
|      ! 0 |  8711 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  8712 | `			const char *zData;` |
|      ! 0 |  8713 | `			int iLen = 0;` |
|        - |  8714 | `			/* Print exit message */` |
|      ! 0 |  8715 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  8716 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  8717 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  8718 | `			sxi32 iExitStatus;` |
|        - |  8719 | `			/* Record exit status code */` |
|      ! 0 |  8720 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  8721 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  8722 | `		}` |
|      ! 0 |  8723 | `	}` |
|        - |  8724 | `	/* Check if we are in an included file */` |
|      ! 0 |  8725 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  8726 | `		/* Exit the entire process */` |
|      ! 0 |  8727 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  8728 | `	}` |
|        - |  8729 | `	/* Abort processing immediately */` |
|      ! 0 |  8730 | `	return PH7_ABORT;` |
|      ! 0 |  8731 |  |
|        - |  8732 | `/*` |
|        - |  8733 | ` * bool isset($var,...)` |
|        - |  8734 | ` *  Finds out whether a variable is set.` |
|        - |  8735 | ` * Parameters` |
|        - |  8736 | ` *  One or more variable to check.` |
|        - |  8737 | ` * Return` |
|        - |  8738 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  8739 | ` */` |
|    72998 |  8740 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8741 |  |
|        - |  8742 | `	ph7_value *pObj;` |
|    73000 |  8743 | `	int res = 0;` |
|        - |  8744 | `	int i;` |
|    73000 |  8745 | `	if( nArg < 1 ){` |
|        - |  8746 | `		/* Missing arguments,return false */` |
|      ! 0 |  8747 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  8748 | `		return SXRET_OK;` |
|        - |  8749 | `	}` |
|        - |  8750 | `	/* Iterate over available arguments */` |
|    96322 |  8751 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    73000 |  8752 | `		pObj = apArg[i];` |
|    73000 |  8753 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    49164 |  8754 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8755 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  8756 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  8757 | `			}` |
|    24581 |  8758 | `		}` |
|    73000 |  8759 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    73000 |  8760 | `		if( !res ){` |
|        - |  8761 | `			/* Variable not set,return FALSE */` |
|    49678 |  8762 | `			ph7_result_bool(pCtx,0);` |
|    49678 |  8763 | `			return SXRET_OK;` |
|        - |  8764 | `		}` |
|    11663 |  8765 | `	}` |
|        - |  8766 | `	/* All given variable are set,return TRUE */` |
|    23324 |  8767 | `	ph7_result_bool(pCtx,1);` |
|    23324 |  8768 | `	return SXRET_OK;` |
|    36501 |  8769 |  |
|        - |  8770 | `/*` |
|        - |  8771 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  8772 | ` * frame,the reference table and discard it's contents.` |
|        - |  8773 | ` * This function never fail and always return SXRET_OK.` |
|        - |  8774 | ` */` |
|  2977470 |  8775 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  8776 |  |
|        - |  8777 | `	ph7_value *pObj;` |
|        - |  8778 | `	VmRefObj *pRef;` |
|  2977472 |  8779 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2977472 |  8780 | `	if( pObj ){` |
|        - |  8781 | `		/* Release the object */` |
|  2977472 |  8782 | `		PH7_MemObjRelease(pObj);` |
|  1488735 |  8783 | `	}` |
|        - |  8784 | `	/* Remove old reference links */` |
|  2977472 |  8785 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2977472 |  8786 | `	if( pRef ){` |
|  2977466 |  8787 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  8788 | `		/* Unlink from the reference table */` |
|  2977466 |  8789 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2977466 |  8790 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  8791 | `			VmSlot sFree;` |
|        - |  8792 | `			/* Restore to the free list */` |
|  2977460 |  8793 | `			sFree.nIdx = nObjIdx;` |
|  2977460 |  8794 | `			sFree.pUserData = 0;` |
|  2977460 |  8795 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1488729 |  8796 | `		}` |
|  1488732 |  8797 | `	}` |
|  2977472 |  8798 | `	return SXRET_OK;` |
|        2 |  8799 |  |
|        - |  8800 | `/*` |
|        - |  8801 | ` * void unset($var,...)` |
|        - |  8802 | ` *   Unset one or more given variable.` |
|        - |  8803 | ` * Parameters` |
|        - |  8804 | ` *  One or more variable to unset.` |
|        - |  8805 | ` * Return` |
|        - |  8806 | ` *  Nothing.` |
|        - |  8807 | ` */` |
|     6678 |  8808 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8809 |  |
|        - |  8810 | `	ph7_value *pObj;` |
|        - |  8811 | `	ph7_vm *pVm;` |
|        - |  8812 | `	int i;` |
|        - |  8813 | `	/* Point to the target VM */` |
|     6680 |  8814 | `	pVm = pCtx->pVm;` |
|        - |  8815 | `	/* Iterate and unset */` |
|    13358 |  8816 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  8817 | `		pObj = apArg[i];` |
|     6680 |  8818 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8819 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  8820 | `				/* Throw an error */` |
|      ! 0 |  8821 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  8822 | `			}` |
|      ! 0 |  8823 | `		}else{` |
|     6680 |  8824 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  8825 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  8826 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  8827 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  8828 | `			}` |
|        - |  8829 | `		}` |
|     3341 |  8830 | `	}` |
|     6680 |  8831 | `	return SXRET_OK;` |
|        2 |  8832 |  |
|        - |  8833 | `/*` |
|        - |  8834 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  8835 | ` */` |
|      110 |  8836 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8837 |  |
|      111 |  8838 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  8839 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  8840 | `	ph7_value *pObj;` |
|        - |  8841 | `	sxu32 nIdx;` |
|        - |  8842 | `	/* Extract the memory object */` |
|      111 |  8843 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  8844 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  8845 | `	if( pObj ){` |
|      111 |  8846 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  8847 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  8848 | `				SyString sName;` |
|        - |  8849 | `				ph7_value sKey;` |
|        - |  8850 | `				/* Perform the insertion */` |
|      109 |  8851 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  8852 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  8853 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  8854 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  8855 | `			}` |
|       54 |  8856 | `		}` |
|       55 |  8857 | `	}` |
|      111 |  8858 | `	return SXRET_OK;` |
|        1 |  8859 |  |
|        - |  8860 | `/*` |
|        - |  8861 | ` * array get_defined_vars(void)` |
|        - |  8862 | ` *  Returns an array of all defined variables.` |
|        - |  8863 | ` * Parameter` |
|        - |  8864 | ` *  None` |
|        - |  8865 | ` * Return` |
|        - |  8866 | ` *  An array with all the variables defined in the current scope.` |
|        - |  8867 | ` */` |
|        2 |  8868 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8869 |  |
|        3 |  8870 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8871 | `	ph7_value *pArray;` |
|        - |  8872 | `	/* Create a new array */` |
|        3 |  8873 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8874 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8875 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8876 | `		SXUNUSED(apArg);` |
|        - |  8877 | `		/* Return NULL */` |
|      ! 0 |  8878 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8879 | `		return SXRET_OK;` |
|        - |  8880 | `	}` |
|        - |  8881 | `	/* Superglobals first */` |
|        3 |  8882 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  8883 | `	/* Then variable defined in the current frame */` |
|        3 |  8884 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  8885 | `	/* Finally,return the created array */` |
|        3 |  8886 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8887 | `	return SXRET_OK;` |
|        2 |  8888 |  |
|        - |  8889 | `/*` |
|        - |  8890 | ` * bool gettype($var)` |
|        - |  8891 | ` *  Get the type of a variable` |
|        - |  8892 | ` * Parameters` |
|        - |  8893 | ` *   $var` |
|        - |  8894 | ` *    The variable being type checked.` |
|        - |  8895 | ` * Return` |
|        - |  8896 | ` *   String representation of the given variable type.` |
|        - |  8897 | ` */` |
|       32 |  8898 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8899 |  |
|       34 |  8900 | `	const char *zType = "Empty";` |
|       34 |  8901 | `	if( nArg > 0 ){` |
|       34 |  8902 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  8903 | `	}` |
|        - |  8904 | `	/* Return the variable type */` |
|       34 |  8905 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  8906 | `	return SXRET_OK;` |
|        2 |  8907 |  |
|        - |  8908 | `/*` |
|        - |  8909 | ` * string get_resource_type(resource $handle)` |
|        - |  8910 | ` *  This function gets the type of the given resource.` |
|        - |  8911 | ` * Parameters` |
|        - |  8912 | ` *  $handle` |
|        - |  8913 | ` *  The evaluated resource handle.` |
|        - |  8914 | ` * Return` |
|        - |  8915 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  8916 | ` *  representing its type. If the type is not identified by this function` |
|        - |  8917 | ` *  the return value will be the string Unknown.` |
|        - |  8918 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  8919 | ` *  is not a resource.` |
|        - |  8920 | ` */` |
|        2 |  8921 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8922 |  |
|        3 |  8923 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  8924 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  8925 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8926 | `		return PH7_OK;` |
|        - |  8927 | `	}` |
|        3 |  8928 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  8929 | `	return SXRET_OK;` |
|        2 |  8930 |  |
|        - |  8931 | `/*` |
|        - |  8932 | ` * void var_dump(expression,....)` |
|        - |  8933 | ` *   var_dump � Dumps information about a variable` |
|        - |  8934 | ` * Parameters` |
|        - |  8935 | ` *   One or more expression to dump.` |
|        - |  8936 | ` * Returns` |
|        - |  8937 | ` *  Nothing.` |
|        - |  8938 | ` */` |
|      218 |  8939 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8940 |  |
|        - |  8941 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8942 | `	int i;` |
|      220 |  8943 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8944 | `	/* Dump one or more expressions */` |
|      444 |  8945 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8946 | `		ph7_value *pObj = apArg[i];` |
|        - |  8947 | `		/* Reset the working buffer */` |
|      226 |  8948 | `		SyBlobReset(&sDump);` |
|        - |  8949 | `		/* Dump the given expression */` |
|      226 |  8950 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8951 | `		/* Output */` |
|      226 |  8952 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8953 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8954 | `		}` |
|      114 |  8955 | `	}` |
|        - |  8956 | `	/* Release the working buffer */` |
|      220 |  8957 | `	SyBlobRelease(&sDump);` |
|      220 |  8958 | `	return SXRET_OK;` |
|        2 |  8959 |  |
|        - |  8960 | `/*` |
|        - |  8961 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8962 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8963 | ` * Parameters` |
|        - |  8964 | ` *   expression: Expression to dump` |
|        - |  8965 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8966 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8967 | ` *            print_r() will return the information rather than print it.` |
|        - |  8968 | ` * Return` |
|        - |  8969 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8970 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8971 | ` */` |
|       16 |  8972 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8973 |  |
|       17 |  8974 | `	int ret_string = 0;` |
|        - |  8975 | `	SyBlob sDump;` |
|       17 |  8976 | `	if( nArg < 1 ){` |
|        - |  8977 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8978 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8979 | `		return SXRET_OK;` |
|        - |  8980 | `	}` |
|       17 |  8981 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8982 | `	if ( nArg > 1 ){` |
|        - |  8983 | `		/* Where to redirect output */` |
|       11 |  8984 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8985 | `	}` |
|        - |  8986 | `	/* Generate dump */` |
|       17 |  8987 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8988 | `	if( !ret_string ){` |
|        - |  8989 | `		/* Output dump */` |
|        7 |  8990 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8991 | `		/* Return true */` |
|        7 |  8992 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8993 | `	}else{` |
|        - |  8994 | `		/* Generated dump as return value */` |
|       11 |  8995 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8996 | `	}` |
|        - |  8997 | `	/* Release the working buffer */` |
|       17 |  8998 | `	SyBlobRelease(&sDump);` |
|       17 |  8999 | `	return SXRET_OK;` |
|        9 |  9000 |  |
|        - |  9001 | `/*` |
|        - |  9002 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9003 | ` * Same job as print_r. (see coment above)` |
|        - |  9004 | ` */` |
|        2 |  9005 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9006 |  |
|        3 |  9007 | `	int ret_string = 0;` |
|        - |  9008 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9009 | `	if( nArg < 1 ){` |
|        - |  9010 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9011 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9012 | `		return SXRET_OK;` |
|        - |  9013 | `	}` |
|        3 |  9014 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9015 | `	if ( nArg > 1 ){` |
|        - |  9016 | `		/* Where to redirect output */` |
|        3 |  9017 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9018 | `	}` |
|        - |  9019 | `	/* Generate dump */` |
|        3 |  9020 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9021 | `	if( !ret_string ){` |
|        - |  9022 | `		/* Output dump */` |
|      ! 0 |  9023 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9024 | `		/* Return NULL */` |
|      ! 0 |  9025 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9026 | `	}else{` |
|        - |  9027 | `		/* Generated dump as return value */` |
|        3 |  9028 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9029 | `	}` |
|        - |  9030 | `	/* Release the working buffer */` |
|        3 |  9031 | `	SyBlobRelease(&sDump);` |
|        3 |  9032 | `	return SXRET_OK;` |
|        2 |  9033 |  |
|        - |  9034 | `/*` |
|        - |  9035 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9036 | ` *  Set/get the various assert flags.` |
|        - |  9037 | ` * Parameter` |
|        - |  9038 | ` * $what` |
|        - |  9039 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9040 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9041 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9042 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9043 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9044 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9045 | ` * $value` |
|        - |  9046 | ` *   An optional new value for the option.` |
|        - |  9047 | ` * Return` |
|        - |  9048 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9049 | ` */` |
|       30 |  9050 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9051 |  |
|       32 |  9052 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9053 | `	int iOption;` |
|        - |  9054 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  9055 | `	if( nArg < 1 ){` |
|        3 |  9056 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9057 | `			"ArgumentCountError",` |
|        - |  9058 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9059 | `			);` |
|        - |  9060 | `	}` |
|        - |  9061 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  9062 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  9063 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9064 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9065 | `			"TypeError",` |
|        - |  9066 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9067 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9068 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9069 | `			);` |
|        - |  9070 | `	}` |
|       30 |  9071 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9072 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9073 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9074 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  9075 | `	switch( iOption ){` |
|        6 |  9076 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9077 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  9078 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  9079 | `		if( nArg > 1 ){` |
|        5 |  9080 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9081 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9082 | `			}else{` |
|        3 |  9083 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9084 | `			}` |
|        2 |  9085 | `		}` |
|       14 |  9086 | `		break;` |
|        1 |  9087 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9088 | `		/* Return old callback or null */` |
|        3 |  9089 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9090 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9091 | `		}else{` |
|        3 |  9092 | `			ph7_result_null(pCtx);` |
|        - |  9093 | `		}` |
|        3 |  9094 | `		if( nArg > 1 ){` |
|      ! 0 |  9095 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9096 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9097 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9098 | `			}else{` |
|      ! 0 |  9099 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9100 | `			}` |
|      ! 0 |  9101 | `		}` |
|        3 |  9102 | `		break;` |
|        5 |  9103 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9104 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9105 | `		if( nArg > 1 ){` |
|        5 |  9106 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9107 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9108 | `			}else{` |
|        3 |  9109 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9110 | `			}` |
|        2 |  9111 | `		}` |
|       11 |  9112 | `		break;` |
|      ! 0 |  9113 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9114 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9115 | `		break;` |
|        1 |  9116 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9117 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9118 | `		break;` |
|      ! 0 |  9119 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9120 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9121 | `		break;` |
|        1 |  9122 | `	default:` |
|        - |  9123 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9124 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9125 | `			"ValueError",` |
|        - |  9126 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9127 | `			);` |
|        - |  9128 | `	}` |
|       28 |  9129 | `	return PH7_OK;` |
|       17 |  9130 |  |
|        - |  9131 | `/*` |
|        - |  9132 | ` * bool assert(mixed $assertion)` |
|        - |  9133 | ` *  Checks if assertion is FALSE.` |
|        - |  9134 | ` * Parameter` |
|        - |  9135 | ` *  $assertion` |
|        - |  9136 | ` *    The assertion to test.` |
|        - |  9137 | ` * Return` |
|        - |  9138 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9139 | ` */` |
|       26 |  9140 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9141 |  |
|       28 |  9142 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9143 | `	int iFlags,iResult;` |
|        - |  9144 | `	const char *zDesc;` |
|        - |  9145 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  9146 | `	if( nArg < 1 ){` |
|        3 |  9147 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9148 | `			"ArgumentCountError",` |
|        - |  9149 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9150 | `			);` |
|        - |  9151 | `	}` |
|       26 |  9152 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  9153 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9154 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9155 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9156 | `		return PH7_OK;` |
|        - |  9157 | `	}` |
|        - |  9158 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  9159 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  9160 | `	if( !iResult ){` |
|        - |  9161 | `		/* Assertion failed */` |
|        - |  9162 | `		/* Extract optional description */` |
|       13 |  9163 | `		zDesc = 0;` |
|       13 |  9164 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9165 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9166 | `		}` |
|       13 |  9167 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9168 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9169 | `			ph7_value sFile,sLine;` |
|        - |  9170 | `			ph7_value *apCbArg[3];` |
|        - |  9171 | `			SyString *pFile;` |
|        - |  9172 | `			/* Extract the processed script */` |
|      ! 0 |  9173 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9174 | `			if( pFile == 0 ){` |
|      ! 0 |  9175 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9176 | `			}` |
|        - |  9177 | `			/* Invoke the callback */` |
|      ! 0 |  9178 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9179 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9180 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9181 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9182 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9183 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9184 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9185 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9186 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9187 | `		}` |
|       13 |  9188 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9189 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9190 | `			return PH7_ABORT;` |
|        - |  9191 | `		}` |
|        - |  9192 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9193 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9194 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9195 | `				"AssertionError",` |
|        - |  9196 | `				"%s",` |
|        1 |  9197 | `				zDesc` |
|        - |  9198 | `				);` |
|      ! 0 |  9199 | `		}else{` |
|       11 |  9200 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9201 | `				"AssertionError",` |
|        - |  9202 | `				"assert(false)"` |
|        - |  9203 | `				);` |
|        - |  9204 | `		}` |
|        - |  9205 | `	}` |
|        - |  9206 | `	/* Assertion passed */` |
|       14 |  9207 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9208 | `	return PH7_OK;` |
|       15 |  9209 |  |
|        - |  9210 | `/*` |
|        - |  9211 | ` * Section:` |
|        - |  9212 | ` *  Error reporting functions.` |
|        - |  9213 | ` * Status:` |
|        - |  9214 | ` *    Stable.` |
|        - |  9215 | ` */` |
|        - |  9216 | `/*` |
|        - |  9217 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9218 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9219 | ` * Parameters` |
|        - |  9220 | ` *  $error_msg` |
|        - |  9221 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9222 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9223 | ` * $error_type` |
|        - |  9224 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9225 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9226 | ` * Return` |
|        - |  9227 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9228 | ` */` |
|       12 |  9229 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9230 |  |
|       14 |  9231 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9232 | `	int rc = PH7_OK;` |
|       14 |  9233 | `	if( nArg > 0 ){` |
|        - |  9234 | `		const char *zErr;` |
|        - |  9235 | `		int nLen;` |
|        - |  9236 | `		/* Extract the error message */` |
|       12 |  9237 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9238 | `		if( nArg > 1 ){` |
|        - |  9239 | `			/* Extract the error type */` |
|       12 |  9240 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9241 | `			switch( nErr ){` |
|        1 |  9242 | `			case 1:   /* E_ERROR */` |
|        - |  9243 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9244 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9245 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9246 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9247 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9248 | `				break;` |
|        1 |  9249 | `			case 2:   /* E_WARNING */` |
|        - |  9250 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9251 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9252 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9253 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9254 | `				break;` |
|        3 |  9255 | `			default:` |
|        8 |  9256 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9257 | `				break;` |
|        - |  9258 | `			}` |
|        5 |  9259 | `		}` |
|        - |  9260 | `		/* Report error */` |
|       12 |  9261 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9262 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9263 | `			return rc;` |
|        - |  9264 | `		}` |
|        - |  9265 | `		/* Return true */` |
|       12 |  9266 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9267 | `	}else{` |
|        - |  9268 | `		/* Missing arguments,return FALSE */` |
|        3 |  9269 | `		ph7_result_bool(pCtx,0);` |
|        - |  9270 | `	}` |
|       14 |  9271 | `	return rc;` |
|        8 |  9272 |  |
|        - |  9273 | `/*` |
|        - |  9274 | ` * int error_reporting([int $level])` |
|        - |  9275 | ` *  Sets which PHP errors are reported.` |
|        - |  9276 | ` * Parameters` |
|        - |  9277 | ` *  $level` |
|        - |  9278 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9279 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9280 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9281 | ` *   levels will not always behave as expected.` |
|        - |  9282 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9283 | ` *   in the predefined constants.` |
|        - |  9284 | ` * Return` |
|        - |  9285 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9286 | ` *   parameter is given.` |
|        - |  9287 | ` */` |
|       42 |  9288 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9289 |  |
|       44 |  9290 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9291 | `	int nOld;` |
|        - |  9292 | `	/* Extract the old reporting level */` |
|       44 |  9293 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  9294 | `	if( nArg > 0 ){` |
|        - |  9295 | `		int nNew;` |
|        - |  9296 | `		/* Extract the desired error reporting level */` |
|       36 |  9297 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  9298 | `		if( !nNew ){` |
|        - |  9299 | `			/* Do not report errors at all */` |
|        5 |  9300 | `			pVm->bErrReport = 0;` |
|        3 |  9301 | `		}else{` |
|        - |  9302 | `			/* Report all errors */` |
|       32 |  9303 | `			pVm->bErrReport = 1;` |
|        - |  9304 | `		}` |
|       17 |  9305 | `	}` |
|        - |  9306 | `	/* Return the old level */` |
|       44 |  9307 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  9308 | `	return PH7_OK;` |
|        2 |  9309 |  |
|        - |  9310 | `/*` |
|        - |  9311 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9312 | ` *  Send an error message somewhere.` |
|        - |  9313 | ` * Parameter` |
|        - |  9314 | ` *  $message` |
|        - |  9315 | ` *   The error message that should be logged.` |
|        - |  9316 | ` *  $message_type` |
|        - |  9317 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9318 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9319 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9320 | ` *       This is the default option.` |
|        - |  9321 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9322 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9323 | ` *    2  No longer an option.` |
|        - |  9324 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9325 | ` *       to the end of the message string.` |
|        - |  9326 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9327 | ` *  $destination` |
|        - |  9328 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9329 | ` *  $extra_headers` |
|        - |  9330 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9331 | ` * Return` |
|        - |  9332 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9333 | ` * NOTE:` |
|        - |  9334 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9335 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9336 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9337 | ` *  Otherwise this function is no-op.` |
|        - |  9338 | ` */` |
|        4 |  9339 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9340 |  |
|        - |  9341 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9342 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9343 | `	int iType = 0;` |
|        5 |  9344 | `	if( nArg < 1 ){` |
|        - |  9345 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9346 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9347 | `		return PH7_OK;` |
|        - |  9348 | `	}` |
|        5 |  9349 | `	if( pVm->xErrLog  ){` |
|        - |  9350 | `		/* Invoke the user callback */` |
|      ! 0 |  9351 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9352 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9353 | `		if( nArg > 1 ){` |
|      ! 0 |  9354 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9355 | `			if( nArg > 2 ){` |
|      ! 0 |  9356 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9357 | `				if( nArg > 3 ){` |
|      ! 0 |  9358 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9359 | `				}` |
|      ! 0 |  9360 | `			}` |
|      ! 0 |  9361 | `		}` |
|      ! 0 |  9362 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9363 | `	}` |
|        - |  9364 | `	/* Retun TRUE */` |
|        5 |  9365 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9366 | `	return PH7_OK;` |
|        3 |  9367 |  |
|        - |  9368 | `/*` |
|        - |  9369 | ` * bool restore_exception_handler(void)` |
|        - |  9370 | ` *  Restores the previously defined exception handler function.` |
|        - |  9371 | ` * Parameter` |
|        - |  9372 | ` *  None` |
|        - |  9373 | ` * Return` |
|        - |  9374 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9375 | ` */` |
|        4 |  9376 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9377 |  |
|        5 |  9378 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9379 | `	ph7_value *pOld,*pNew;` |
|        - |  9380 | `	/* Point to the old and the new handler */` |
|        5 |  9381 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9382 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9383 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9384 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9385 | `		SXUNUSED(apArg);` |
|        - |  9386 | `		/* No installed handler,return FALSE */` |
|        5 |  9387 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9388 | `		return PH7_OK;` |
|        - |  9389 | `	}` |
|        - |  9390 | `	/* Copy the old handler */` |
|      ! 0 |  9391 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9392 | `	PH7_MemObjRelease(pOld);` |
|        - |  9393 | `	/* Return TRUE */` |
|      ! 0 |  9394 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9395 | `	return PH7_OK;` |
|        3 |  9396 |  |
|        - |  9397 | `/*` |
|        - |  9398 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9399 | ` *  Sets a user-defined exception handler function.` |
|        - |  9400 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9401 | ` * NOTE` |
|        - |  9402 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9403 | ` *  the satndard PHP engine.` |
|        - |  9404 | ` * Parameters` |
|        - |  9405 | ` *  $exception_handler` |
|        - |  9406 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9407 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9408 | ` *   that was thrown.` |
|        - |  9409 | ` *  Note:` |
|        - |  9410 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9411 | ` * Return` |
|        - |  9412 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9413 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9414 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9415 | ` */` |
|        4 |  9416 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9417 |  |
|        6 |  9418 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9419 | `	ph7_value *pOld,*pNew;` |
|        - |  9420 | `	/* Point to the old and the new handler */` |
|        6 |  9421 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9422 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9423 | `	/* Return the old handler */` |
|        6 |  9424 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9425 | `	if( nArg > 0 ){` |
|        6 |  9426 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9427 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9428 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9429 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9430 | `		}else{` |
|        6 |  9431 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9432 | `			/* Install the new handler */` |
|        6 |  9433 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9434 | `		}` |
|        2 |  9435 | `	}` |
|        6 |  9436 | `	return PH7_OK;` |
|        2 |  9437 |  |
|        - |  9438 | `/*` |
|        - |  9439 | ` * bool restore_error_handler(void)` |
|        - |  9440 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9441 | ` * Parameters:` |
|        - |  9442 | ` *  None.` |
|        - |  9443 | ` * Return` |
|        - |  9444 | ` *  Always TRUE.` |
|        - |  9445 | ` */` |
|        4 |  9446 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9447 |  |
|        5 |  9448 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9449 | `	ph7_value *pOld,*pNew;` |
|        - |  9450 | `	/* Point to the old and the new handler */` |
|        5 |  9451 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9452 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9453 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9454 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9455 | `		SXUNUSED(apArg);` |
|        - |  9456 | `		/* No installed callback,return FALSE */` |
|        5 |  9457 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9458 | `		return PH7_OK;` |
|        - |  9459 | `	}` |
|        - |  9460 | `	/* Copy the old callback */` |
|      ! 0 |  9461 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9462 | `	PH7_MemObjRelease(pOld);` |
|        - |  9463 | `	/* Return TRUE */` |
|      ! 0 |  9464 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9465 | `	return PH7_OK;` |
|        3 |  9466 |  |
|        - |  9467 | `/*` |
|        - |  9468 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9469 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9470 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9471 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9472 | ` *  Sets a user-defined error handler function.` |
|        - |  9473 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9474 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9475 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9476 | ` *  conditions (using trigger_error()).` |
|        - |  9477 | ` * Parameters` |
|        - |  9478 | ` *  $error_handler` |
|        - |  9479 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9480 | ` *   describing the error.` |
|        - |  9481 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9482 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9483 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9484 | ` *   The function can be shown as:` |
|        - |  9485 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9486 | ` *     errno` |
|        - |  9487 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9488 | ` *   errstr` |
|        - |  9489 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9490 | ` *   errfile` |
|        - |  9491 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9492 | ` *     was raised in, as a string.` |
|        - |  9493 | ` *  Note:` |
|        - |  9494 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9495 | ` * Return` |
|        - |  9496 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9497 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9498 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9499 | ` */` |
|     8822 |  9500 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9501 |  |
|     8824 |  9502 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9503 | `	ph7_value *pOld,*pNew;` |
|        - |  9504 | `	/* Point to the old and the new handler */` |
|     8824 |  9505 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  9506 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9507 | `	/* Return the old handler */` |
|     8824 |  9508 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  9509 | `	if( nArg > 0 ){` |
|     8824 |  9510 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9511 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  9512 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  9513 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  9514 | `		}else{` |
|     4414 |  9515 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9516 | `			/* Install the new handler */` |
|     4414 |  9517 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9518 | `		}` |
|     4411 |  9519 | `	}` |
|     8824 |  9520 | `	return PH7_OK;` |
|        2 |  9521 |  |
|        - |  9522 | `/*` |
|        - |  9523 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9524 | ` *  Generates a backtrace.` |
|        - |  9525 | ` * Paramaeter` |
|        - |  9526 | ` *  $options` |
|        - |  9527 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9528 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9529 | ` *   all the function/method arguments, to save memory.` |
|        - |  9530 | ` * $limit` |
|        - |  9531 | ` *   (Not Used)` |
|        - |  9532 | ` * Return` |
|        - |  9533 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9534 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9535 | ` *          Name        Type      Description` |
|        - |  9536 | ` *          ------      ------     -----------` |
|        - |  9537 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9538 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9539 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9540 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  9541 | ` *          object      object    The current object.` |
|        - |  9542 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  9543 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  9544 | ` */` |
|      510 |  9545 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9546 |  |
|      512 |  9547 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9548 | `	ph7_value *pArray;` |
|        - |  9549 | `	ph7_class *pClass;` |
|        - |  9550 | `	ph7_value *pValue;` |
|        - |  9551 | `	SyString *pFile;` |
|        - |  9552 | `	/* Create a new array */` |
|      512 |  9553 | `	pArray = ph7_context_new_array(pCtx);` |
|      512 |  9554 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      512 |  9555 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9556 | `		/* Out of memory,return NULL */` |
|      ! 0 |  9557 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  9558 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9559 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9560 | `		SXUNUSED(apArg);` |
|      ! 0 |  9561 | `		return PH7_OK;` |
|        - |  9562 | `	}` |
|        - |  9563 | `	/* Dump running function name and it's arguments  */` |
|      512 |  9564 | `	if( pVm->pFrame->pParent ){` |
|      512 |  9565 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  9566 | `		ph7_vm_func *pFunc;` |
|        - |  9567 | `		ph7_value *pArg;` |
|      512 |  9568 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      512 |  9569 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      512 |  9570 | `		if( pFrame->pParent && pFunc ){` |
|      512 |  9571 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      512 |  9572 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      512 |  9573 | `			ph7_value_reset_string_cursor(pValue);` |
|      255 |  9574 | `		}` |
|        - |  9575 | `		/* Function arguments */` |
|      512 |  9576 | `		pArg = ph7_context_new_array(pCtx);` |
|      512 |  9577 | `		if( pArg  ){` |
|        - |  9578 | `			ph7_value *pObj;` |
|        - |  9579 | `			VmSlot *aSlot;` |
|        - |  9580 | `			sxu32 n;` |
|        - |  9581 | `			/* Start filling the array with the given arguments */` |
|      512 |  9582 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2034 |  9583 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1524 |  9584 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1524 |  9585 | `				if( pObj ){` |
|     1524 |  9586 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      761 |  9587 | `				}` |
|      763 |  9588 | `			}` |
|        - |  9589 | `			/* Save the array */` |
|      512 |  9590 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      255 |  9591 | `		}` |
|      255 |  9592 | `	}` |
|      512 |  9593 | `	ph7_value_int(pValue,1);` |
|        - |  9594 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  9595 | `	 * line numbers at run-time. )` |
|        - |  9596 | `	 */` |
|      512 |  9597 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  9598 | `	/* Current processed script */` |
|      512 |  9599 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      512 |  9600 | `	if( pFile ){` |
|      512 |  9601 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      512 |  9602 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      512 |  9603 | `		ph7_value_reset_string_cursor(pValue);` |
|      255 |  9604 | `	}` |
|        - |  9605 | `	/* Top class */` |
|      512 |  9606 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      512 |  9607 | `	if( pClass ){` |
|      508 |  9608 | `		ph7_value_reset_string_cursor(pValue);` |
|      508 |  9609 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      508 |  9610 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      253 |  9611 | `	}` |
|        - |  9612 | `	/* Return the freshly created array */` |
|      512 |  9613 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9614 | `	/*` |
|        - |  9615 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  9616 | `	 * as soon we return from this function.` |
|        - |  9617 | `	 */` |
|      512 |  9618 | `	return PH7_OK;` |
|      257 |  9619 |  |
|        - |  9620 | `/*` |
|        - |  9621 | ` * Generate a small backtrace.` |
|        - |  9622 | ` * Store the generated dump in the given BLOB` |
|        - |  9623 | ` */` |
|        4 |  9624 | `static int VmMiniBacktrace(` |
|        - |  9625 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9626 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  9627 | `	)` |
|        1 |  9628 |  |
|        5 |  9629 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9630 | `	ph7_vm_func *pFunc;` |
|        - |  9631 | `	ph7_class *pClass;` |
|        - |  9632 | `	SyString *pFile;` |
|        - |  9633 | `	/* Called function */` |
|        5 |  9634 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  9635 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  9636 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9637 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  9638 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  9639 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  9640 | `	}else{` |
|      ! 0 |  9641 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  9642 | `	}` |
|        5 |  9643 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  9644 | `	/* Current processed script */` |
|        5 |  9645 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  9646 | `	if( pFile ){` |
|        5 |  9647 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  9648 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  9649 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  9650 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  9651 | `	}` |
|        - |  9652 | `	/* Top class */` |
|        5 |  9653 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  9654 | `	if( pClass ){` |
|      ! 0 |  9655 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  9656 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  9657 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  9658 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  9659 | `	}` |
|        5 |  9660 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  9661 | `	/* All done */` |
|        5 |  9662 | `	return SXRET_OK;` |
|        1 |  9663 |  |
|        - |  9664 | `/*` |
|        - |  9665 | ` * void debug_print_backtrace()` |
|        - |  9666 | ` *  Prints a backtrace` |
|        - |  9667 | ` * Parameters` |
|        - |  9668 | ` * None` |
|        - |  9669 | ` * Return` |
|        - |  9670 | ` * NULL` |
|        - |  9671 | ` */` |
|        2 |  9672 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9673 |  |
|        3 |  9674 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9675 | `	SyBlob sDump;` |
|        3 |  9676 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9677 | `	/* Generate the backtrace */` |
|        3 |  9678 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9679 | `	/* Output backtrace */` |
|        3 |  9680 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9681 | `	/* All done,cleanup */` |
|        3 |  9682 | `	SyBlobRelease(&sDump);` |
|        1 |  9683 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9684 | `	SXUNUSED(apArg);` |
|        3 |  9685 | `	return PH7_OK;` |
|        1 |  9686 |  |
|        - |  9687 | `/*` |
|        - |  9688 | ` * string debug_string_backtrace()` |
|        - |  9689 | ` *  Generate a backtrace` |
|        - |  9690 | ` * Parameters` |
|        - |  9691 | ` * None` |
|        - |  9692 | ` * Return` |
|        - |  9693 | ` *  A mini backtrace().` |
|        - |  9694 | ` * Note that this is a symisc extension.` |
|        - |  9695 | ` */` |
|        2 |  9696 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9697 |  |
|        3 |  9698 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9699 | `	SyBlob sDump;` |
|        3 |  9700 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  9701 | `	/* Generate the backtrace */` |
|        3 |  9702 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  9703 | `	/* Return the backtrace */` |
|        3 |  9704 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  9705 | `	/* All done,cleanup */` |
|        3 |  9706 | `	SyBlobRelease(&sDump);` |
|        1 |  9707 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9708 | `	SXUNUSED(apArg);` |
|        3 |  9709 | `	return PH7_OK;` |
|        1 |  9710 |  |
|        - |  9711 | `/*` |
|        - |  9712 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  9713 | ` * exception is triggered.` |
|        - |  9714 | ` */` |
|      472 |  9715 | `static sxi32 VmUncaughtException(` |
|        - |  9716 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  9717 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9718 | `	)` |
|        1 |  9719 |  |
|        - |  9720 | `	ph7_value *apArg[2],sArg;` |
|      473 |  9721 | `	int nArg = 1;` |
|        - |  9722 | `	sxi32 rc;` |
|      473 |  9723 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  9724 | `		/* Nesting limit reached */` |
|      ! 0 |  9725 | `		return SXRET_OK;` |
|        - |  9726 | `	}` |
|        - |  9727 | `	/* Call any exception handler if available */` |
|      473 |  9728 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  9729 | `	if( pThis ){` |
|        - |  9730 | `		/* Load the exception instance */` |
|      473 |  9731 | `		sArg.x.pOther = pThis;` |
|      473 |  9732 | `		pThis->iRef++;` |
|      473 |  9733 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  9734 | `	}else{` |
|      ! 0 |  9735 | `		nArg = 0;` |
|        - |  9736 | `	}` |
|      473 |  9737 | `	apArg[0] = &sArg;` |
|        - |  9738 | `	/* Call the exception handler if available */` |
|      473 |  9739 | `	pVm->nExceptDepth++;` |
|      473 |  9740 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  9741 | `	pVm->nExceptDepth--;` |
|      473 |  9742 | `	if( rc != SXRET_OK ){` |
|        - |  9743 | `		SyBlob sMsgBuf;` |
|      471 |  9744 | `		const char *zClass = "Exception";` |
|      471 |  9745 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  9746 | `		const char *zMsg;` |
|        - |  9747 | `		sxu32 nMsg;` |
|        - |  9748 | `		const char *zFuncName;` |
|        - |  9749 | `		int nFuncLen;` |
|      471 |  9750 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  9751 | `		if( pThis ){` |
|        - |  9752 | `			ph7_class_method *pGetMessage;` |
|        - |  9753 | `			ph7_value sMsg;` |
|        - |  9754 | `			const char *zTmp;` |
|        - |  9755 | `			int nTmp;` |
|      471 |  9756 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  9757 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  9758 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  9759 | `			if( pGetMessage ){` |
|      471 |  9760 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  9761 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  9762 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  9763 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  9764 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  9765 | `					}` |
|      235 |  9766 | `				}` |
|      471 |  9767 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  9768 | `			}` |
|      235 |  9769 | `		}` |
|      471 |  9770 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  9771 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  9772 | `		}` |
|      471 |  9773 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  9774 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  9775 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  9776 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  9777 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  9778 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  9779 | `		rc = SXERR_ABORT;` |
|      235 |  9780 | `	}` |
|      473 |  9781 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  9782 | `	return rc;` |
|      237 |  9783 |  |
|        - |  9784 | `/*` |
|        - |  9785 | ` * Throw a user exception.` |
|        - |  9786 | ` *` |
|        - |  9787 | ` * Exception dispatch follows this sequence:` |
|        - |  9788 | ` *` |
|        - |  9789 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - |  9790 | ` *    try/catch whose catch block matches the exception class.` |
|        - |  9791 | ` *` |
|        - |  9792 | ` * 2. If NO catch matches:` |
|        - |  9793 | ` *    a. Run finally (if present) for the current try block.` |
|        - |  9794 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - |  9795 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - |  9796 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - |  9797 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - |  9798 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - |  9799 | ` *    d. Otherwise, report as truly uncaught.` |
|        - |  9800 | ` *` |
|        - |  9801 | ` * 3. If a catch DOES match:` |
|        - |  9802 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - |  9803 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - |  9804 | ` *       inside the catch body from immediately propagating past our` |
|        - |  9805 | ` *       finally block.` |
|        - |  9806 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - |  9807 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - |  9808 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - |  9809 | ` *       in pPendingException (step 2c).` |
|        - |  9810 | ` *    c. Restore outer handlers from the saved copy.` |
|        - |  9811 | ` *    d. Run finally (if present).` |
|        - |  9812 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - |  9813 | ` *       that handlers are restored and finally has run.` |
|        - |  9814 | ` */` |
|      514 |  9815 | `static sxi32 VmThrowException(` |
|        - |  9816 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  9817 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  9818 | `	)` |
|        2 |  9819 |  |
|        - |  9820 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  9821 | `	ph7_exception **apException;` |
|        - |  9822 | `	ph7_exception *pException;` |
|        - |  9823 | `	/* Point to the stack of loaded exceptions */` |
|      516 |  9824 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 |  9825 | `	pException = 0;` |
|      516 |  9826 | `	pCatch = 0;` |
|      516 |  9827 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9828 | `		ph7_exception_block *aCatch;` |
|        - |  9829 | `		ph7_class *pClass;` |
|        - |  9830 | `		sxu32 j;` |
|        - |  9831 | `		/* Locate the appropriate block to execute */` |
|       40 |  9832 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 |  9833 | `		(void)SySetPop(&pVm->aException);` |
|       40 |  9834 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 |  9835 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 |  9836 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  9837 | `			/* Extract the target class */` |
|       38 |  9838 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 |  9839 | `			if( pClass == 0 ){` |
|        - |  9840 | `				/* No such class */` |
|      ! 0 |  9841 | `				continue;` |
|        - |  9842 | `			}` |
|       38 |  9843 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  9844 | `				/* Catch block found,break immeditaley */` |
|       38 |  9845 | `				pCatch = &aCatch[j];` |
|       38 |  9846 | `				break;` |
|        - |  9847 | `			}` |
|      ! 0 |  9848 | `		}` |
|       19 |  9849 | `	}` |
|        - |  9850 | `	/* Execute the cached block if available */` |
|      516 |  9851 | `	if( pCatch == 0 ){` |
|        - |  9852 | `		sxi32 rc;` |
|        - |  9853 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  9854 | `		if( pException && pException->iHasFinally ){` |
|        3 |  9855 | `			pException->iFinallyDone = 1;` |
|        3 |  9856 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  9857 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  9858 | `				return SXERR_ABORT;` |
|        - |  9859 | `			}` |
|        1 |  9860 | `		}` |
|        - |  9861 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  9862 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  9863 | `			/* Re-throw to the outer handler */` |
|        3 |  9864 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  9865 | `		}` |
|        - |  9866 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  9867 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  9868 | `		 * exception instead of reporting it uncaught.` |
|        - |  9869 | `		 */` |
|      478 |  9870 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  9871 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  9872 | `			 * by looking for a catch frame on the stack.` |
|        - |  9873 | `			 */` |
|      478 |  9874 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  9875 | `			int inCatch = 0;` |
|      956 |  9876 | `			while( pF ){` |
|      484 |  9877 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  9878 | `					inCatch = 1;` |
|        6 |  9879 | `					break;` |
|        - |  9880 | `				}` |
|      479 |  9881 | `				pF = pF->pParent;` |
|        1 |  9882 | `			}` |
|      478 |  9883 | `			if( inCatch ){` |
|        - |  9884 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  9885 | `				pThis->iRef++;` |
|        6 |  9886 | `				pVm->pPendingException = pThis;` |
|        6 |  9887 | `				return SXRET_OK;` |
|        - |  9888 | `			}` |
|      236 |  9889 | `		}` |
|        - |  9890 | `		/* Truly uncaught */` |
|      473 |  9891 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  9892 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  9893 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  9894 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 |  9895 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  9896 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  9897 | `			}` |
|      ! 0 |  9898 | `		}` |
|      473 |  9899 | `		return rc;` |
|      ! 0 |  9900 | `	}else{` |
|       38 |  9901 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 |  9902 | `		ph7_exception **apSaved = 0;` |
|        - |  9903 | `		sxu32 nSavedCount;` |
|        - |  9904 | `		sxi32 rc;` |
|       38 |  9905 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 |  9906 | `		if( pException->pFrame == pFrame ){` |
|       24 |  9907 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 |  9908 | `		}` |
|        - |  9909 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  9910 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  9911 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  9912 | `		 */` |
|       38 |  9913 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 |  9914 | `		if( nSavedCount > 0 ){` |
|       11 |  9915 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  9916 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9917 | `			if( apSaved ){` |
|       11 |  9918 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  9919 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  9920 | `				SySetReset(&pVm->aException);` |
|        3 |  9921 | `			}` |
|        3 |  9922 | `		}` |
|        - |  9923 | `		/* Create a private frame first */` |
|       38 |  9924 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 |  9925 | `		if( rc == SXRET_OK ){` |
|       38 |  9926 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 |  9927 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 |  9928 | `			if( pObj ){` |
|       38 |  9929 | `				pThis->iRef++;` |
|       38 |  9930 | `				pObj->x.pOther = pThis;` |
|       38 |  9931 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 |  9932 | `			}` |
|        - |  9933 | `			/* Execute the catch block */` |
|       38 |  9934 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  9935 | `			/* Leave the frame */` |
|       38 |  9936 | `			VmLeaveFrame(&(*pVm));` |
|       18 |  9937 | `		}` |
|        - |  9938 | `		/* Restore the outer exception handlers */` |
|       38 |  9939 | `		if( apSaved ){` |
|        - |  9940 | `			sxu32 k;` |
|        - |  9941 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  9942 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  9943 | `			 * Restore the original outer entries.` |
|        - |  9944 | `			 */` |
|        8 |  9945 | `			SySetReset(&pVm->aException);` |
|       14 |  9946 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  9947 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  9948 | `			}` |
|        8 |  9949 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  9950 | `		}` |
|        - |  9951 | `		/* Execute the finally block after catch */` |
|       38 |  9952 | `		if( pException->iHasFinally ){` |
|       11 |  9953 | `			pException->iFinallyDone = 1;` |
|        - |  9954 | `			{` |
|       11 |  9955 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 |  9956 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9957 | `					return SXERR_ABORT;` |
|        - |  9958 | `				}` |
|        - |  9959 | `			}` |
|        5 |  9960 | `		}` |
|       38 |  9961 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9962 | `			return SXERR_ABORT;` |
|        - |  9963 | `		}` |
|        - |  9964 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9965 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9966 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9967 | `		 */` |
|       38 |  9968 | `		if( pVm->pPendingException ){` |
|        6 |  9969 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9970 | `			pVm->pPendingException = 0;` |
|        6 |  9971 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9972 | `		}` |
|        - |  9973 | `	}` |
|        - |  9974 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9975 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9976 | `	 */` |
|       34 |  9977 | `	return SXRET_OK;` |
|      259 |  9978 |  |
|        - |  9979 | `/*` |
|        - |  9980 | ` * Section:` |
|        - |  9981 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9982 | ` * Status:` |
|        - |  9983 | ` *    Stable.` |
|        - |  9984 | ` */` |
|        - |  9985 | `/*` |
|        - |  9986 | ` * string ph7version(void)` |
|        - |  9987 | ` *  Returns the running version of the PH7 version.` |
|        - |  9988 | ` * Parameters` |
|        - |  9989 | ` *  None` |
|        - |  9990 | ` * Return` |
|        - |  9991 | ` * Current PH7 version.` |
|        - |  9992 | ` */` |
|        2 |  9993 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9994 |  |
|        1 |  9995 | `	SXUNUSED(nArg);` |
|        1 |  9996 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9997 | `	/* Current engine version */` |
|        3 |  9998 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9999 | `	return PH7_OK;` |
|        1 | 10000 |  |
|        - | 10001 | `/*` |
|        - | 10002 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10003 | ` */` |
|        - | 10004 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10005 | ` "<html><head>"\` |
|        - | 10006 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10007 | ` "<style type=\"text/css\">"\` |
|        - | 10008 | ` "div {"\` |
|        - | 10009 | `     "border: 1px solid #cccccc;"\` |
|        - | 10010 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10011 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10012 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10013 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10014 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10015 | `     "-o-border-radius: 10px;"\` |
|        - | 10016 | `     "border-radius: 10px;"\` |
|        - | 10017 | `     "padding-left: 2em;"\` |
|        - | 10018 | `     "background-color: white;"\` |
|        - | 10019 | `     "margin-left: auto;"\` |
|        - | 10020 | `     "font-family: verdana;"\` |
|        - | 10021 | `     "padding-right: 2em;"\` |
|        - | 10022 | `     "margin-right: auto;"\` |
|        - | 10023 | `     "}"\` |
|        - | 10024 | `     "body {"\` |
|        - | 10025 | `     "padding: 0.2em;"\` |
|        - | 10026 | `     "font-style: normal;"\` |
|        - | 10027 | `     "font-size: medium;"\` |
|        - | 10028 | `     "background-color: #f2f2f2;"\` |
|        - | 10029 | `     "}"\` |
|        - | 10030 | `     "hr {"\` |
|        - | 10031 | `     "border-style: solid none none;"\` |
|        - | 10032 | `     "border-width: 1px medium medium;"\` |
|        - | 10033 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10034 | `     "height: 1px;"\` |
|        - | 10035 | `     "}"\` |
|        - | 10036 | `     "a {"\` |
|        - | 10037 | `     "color: #3366cc;"\` |
|        - | 10038 | `     "text-decoration: none;"\` |
|        - | 10039 | `     "}"\` |
|        - | 10040 | `     "a:hover {"\` |
|        - | 10041 | `     "color: #999999;"\` |
|        - | 10042 | `     "}"\` |
|        - | 10043 | `     "a:active {"\` |
|        - | 10044 | `     "color: #663399;"\` |
|        - | 10045 | `     "}"\` |
|        - | 10046 | `     "h1 {"\` |
|        - | 10047 | `     "margin: 0;"\` |
|        - | 10048 | `     "padding: 0;"\` |
|        - | 10049 | `     "font-family: Verdana;"\` |
|        - | 10050 | `     "font-weight: bold;"\` |
|        - | 10051 | `     "font-style: normal;"\` |
|        - | 10052 | `     "font-size: medium;"\` |
|        - | 10053 | `     "text-transform: capitalize;"\` |
|        - | 10054 | `     "color: #0a328c;"\` |
|        - | 10055 | `     "}"\` |
|        - | 10056 | `     "p {"\` |
|        - | 10057 | `     "margin: 0 auto;"\` |
|        - | 10058 | `     "font-size: medium;"\` |
|        - | 10059 | `     "font-style: normal;"\` |
|        - | 10060 | `     "font-family: verdana;"\` |
|        - | 10061 | `     "}"\` |
|        - | 10062 | `"</style></head><body>"\` |
|        - | 10063 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10064 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10065 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10066 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10067 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10068 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10069 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10070 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10071 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10072 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10073 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10074 |  |
|        - | 10075 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10076 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10077 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10078 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10079 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10080 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10081 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10082 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10083 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10084 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10085 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10086 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10087 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10088 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10089 |  |
|        - | 10090 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10091 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10092 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10093 | `"&nbsp;*<br>"\` |
|        - | 10094 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10095 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10096 | `"&nbsp;* are met:<br>"\` |
|        - | 10097 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10098 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10099 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10100 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10101 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10102 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10103 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10104 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10105 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10106 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10107 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10108 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10109 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10110 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10111 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10112 | `"&nbsp;*<br>"\` |
|        - | 10113 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10114 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10115 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10116 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10117 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10118 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10119 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10120 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10121 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10122 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10123 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10124 | `"&nbsp;*/<br>"\` |
|        - | 10125 | `"</span></small></small></p>"\` |
|        - | 10126 | `"</div></body></html>"` |
|        - | 10127 | `/*` |
|        - | 10128 | ` * bool ph7credits(void)` |
|        - | 10129 | ` * bool ph7info(void)` |
|        - | 10130 | ` * bool ph7copyright(void)` |
|        - | 10131 | ` *  Prints out the credits for PH7 engine` |
|        - | 10132 | ` * Parameters` |
|        - | 10133 | ` *  None` |
|        - | 10134 | ` * Return` |
|        - | 10135 | ` *  Always TRUE` |
|        - | 10136 | ` */` |
|        2 | 10137 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10138 |  |
|        3 | 10139 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10140 | `	/* Expand the HTML page above*/` |
|        3 | 10141 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10142 | `	ph7_context_output_format(` |
|        1 | 10143 | `		pCtx,` |
|        - | 10144 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10145 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10146 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10147 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10148 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10149 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10150 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10151 | `#ifdef __WINNT__` |
|        - | 10152 | `		"Windows NT"` |
|        - | 10153 | `#elif defined(__UNIXES__)` |
|        - | 10154 | `		"UNIX-Like"` |
|        - | 10155 | `#else` |
|        - | 10156 | `		"Other OS"` |
|        - | 10157 | `#endif` |
|        - | 10158 | `		);` |
|        3 | 10159 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10160 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10161 | `	SXUNUSED(apArg);` |
|        - | 10162 | `	/* Return TRUE */` |
|        - | 10163 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10164 | `	return PH7_OK;` |
|        1 | 10165 |  |
|        - | 10166 | `/*` |
|        - | 10167 | ` * Section:` |
|        - | 10168 | ` *    URL related routines.` |
|        - | 10169 | ` * Status:` |
|        - | 10170 | ` *    Stable.` |
|        - | 10171 | ` */` |
|        - | 10172 | `/*` |
|        - | 10173 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10174 | ` *  Parse a URL and return its fields.` |
|        - | 10175 | ` * Parameters` |
|        - | 10176 | ` *  $url` |
|        - | 10177 | ` *   The URL to parse.` |
|        - | 10178 | ` * $component` |
|        - | 10179 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10180 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10181 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10182 | ` *  in which case the return value will be an integer).` |
|        - | 10183 | ` * Return` |
|        - | 10184 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10185 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10186 | ` *  this array are:` |
|        - | 10187 | ` *   scheme - e.g. http` |
|        - | 10188 | ` *   host` |
|        - | 10189 | ` *   port` |
|        - | 10190 | ` *   user` |
|        - | 10191 | ` *   pass` |
|        - | 10192 | ` *   path` |
|        - | 10193 | ` *   query - after the question mark ?` |
|        - | 10194 | ` *   fragment - after the hashmark #` |
|        - | 10195 | ` * Note:` |
|        - | 10196 | ` *  FALSE is returned on failure.` |
|        - | 10197 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10198 | ` *  with the standard PHP engine.` |
|        - | 10199 | ` */` |
|       28 | 10200 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10201 |  |
|        - | 10202 | `	const char *zStr; /* Input string */` |
|        - | 10203 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10204 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10205 | `	int nLen;` |
|        - | 10206 | `	sxi32 rc;` |
|       29 | 10207 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10208 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10210 | `		return PH7_OK;` |
|        - | 10211 | `	}` |
|        - | 10212 | `	/* Extract the given URI */` |
|       29 | 10213 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10214 | `	if( nLen < 1 ){` |
|        - | 10215 | `		/* Nothing to process,return FALSE */` |
|        3 | 10216 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10217 | `		return PH7_OK;` |
|        - | 10218 | `	}` |
|        - | 10219 | `	/* Get a parse */` |
|       27 | 10220 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10221 | `	if( rc != SXRET_OK ){` |
|        - | 10222 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10223 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10224 | `		return PH7_OK;` |
|        - | 10225 | `	}` |
|       27 | 10226 | `	if( nArg > 1 ){` |
|      ! 0 | 10227 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10228 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10229 | `		switch(nComponent){` |
|      ! 0 | 10230 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10231 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10232 | `			if( pComp->nByte < 1 ){` |
|        - | 10233 | `				/* No available value,return NULL */` |
|      ! 0 | 10234 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10235 | `			}else{` |
|      ! 0 | 10236 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10237 | `			}` |
|      ! 0 | 10238 | `			break;` |
|      ! 0 | 10239 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10240 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10241 | `			if( pComp->nByte < 1 ){` |
|        - | 10242 | `				/* No available value,return NULL */` |
|      ! 0 | 10243 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10244 | `			}else{` |
|      ! 0 | 10245 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10246 | `			}` |
|      ! 0 | 10247 | `			break;` |
|      ! 0 | 10248 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10249 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10250 | `			if( pComp->nByte < 1 ){` |
|        - | 10251 | `				/* No available value,return NULL */` |
|      ! 0 | 10252 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10253 | `			}else{` |
|      ! 0 | 10254 | `				int iPort = 0;` |
|        - | 10255 | `				/* Cast the value to integer */` |
|      ! 0 | 10256 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10257 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10258 | `			}` |
|      ! 0 | 10259 | `			break;` |
|      ! 0 | 10260 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10261 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10262 | `			if( pComp->nByte < 1 ){` |
|        - | 10263 | `				/* No available value,return NULL */` |
|      ! 0 | 10264 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10265 | `			}else{` |
|      ! 0 | 10266 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10267 | `			}` |
|      ! 0 | 10268 | `			break;` |
|      ! 0 | 10269 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10270 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10271 | `			if( pComp->nByte < 1 ){` |
|        - | 10272 | `				/* No available value,return NULL */` |
|      ! 0 | 10273 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10274 | `			}else{` |
|      ! 0 | 10275 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10276 | `			}` |
|      ! 0 | 10277 | `			break;` |
|      ! 0 | 10278 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10279 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10280 | `			if( pComp->nByte < 1 ){` |
|        - | 10281 | `				/* No available value,return NULL */` |
|      ! 0 | 10282 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10283 | `			}else{` |
|      ! 0 | 10284 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10285 | `			}` |
|      ! 0 | 10286 | `			break;` |
|      ! 0 | 10287 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10288 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10289 | `			if( pComp->nByte < 1 ){` |
|        - | 10290 | `				/* No available value,return NULL */` |
|      ! 0 | 10291 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10292 | `			}else{` |
|      ! 0 | 10293 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10294 | `			}` |
|      ! 0 | 10295 | `			break;` |
|      ! 0 | 10296 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10297 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10298 | `			if( pComp->nByte < 1 ){` |
|        - | 10299 | `				/* No available value,return NULL */` |
|      ! 0 | 10300 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10301 | `			}else{` |
|      ! 0 | 10302 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10303 | `			}` |
|      ! 0 | 10304 | `			break;` |
|      ! 0 | 10305 | `		default:` |
|        - | 10306 | `			/* No such entry,return NULL */` |
|      ! 0 | 10307 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10308 | `			break;` |
|        - | 10309 | `		}` |
|      ! 0 | 10310 | `	}else{` |
|        - | 10311 | `		ph7_value *pArray,*pValue;` |
|        - | 10312 | `		/* Return an associative array */` |
|       27 | 10313 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10314 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10315 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10316 | `			/* Out of memory */` |
|      ! 0 | 10317 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10318 | `			/* Return false */` |
|      ! 0 | 10319 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10320 | `			return PH7_OK;` |
|        - | 10321 | `		}` |
|        - | 10322 | `		/* Fill the array */` |
|       27 | 10323 | `		pComp = &sURI.sScheme;` |
|       27 | 10324 | `		if( pComp->nByte > 0 ){` |
|       19 | 10325 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10326 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10327 | `		}` |
|        - | 10328 | `		/* Reset the string cursor */` |
|       27 | 10329 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10330 | `		pComp = &sURI.sHost;` |
|       27 | 10331 | `		if( pComp->nByte > 0 ){` |
|       25 | 10332 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10333 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10334 | `		}` |
|        - | 10335 | `		/* Reset the string cursor */` |
|       27 | 10336 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10337 | `		pComp = &sURI.sPort;` |
|       27 | 10338 | `		if( pComp->nByte > 0 ){` |
|       11 | 10339 | `			int iPort = 0;/* cc warning */` |
|        - | 10340 | `			/* Convert to integer */` |
|       11 | 10341 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10342 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10343 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10344 | `		}` |
|        - | 10345 | `		/* Reset the string cursor */` |
|       27 | 10346 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10347 | `		pComp = &sURI.sUser;` |
|       27 | 10348 | `		if( pComp->nByte > 0 ){` |
|        7 | 10349 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10350 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10351 | `		}` |
|        - | 10352 | `		/* Reset the string cursor */` |
|       27 | 10353 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10354 | `		pComp = &sURI.sPass;` |
|       27 | 10355 | `		if( pComp->nByte > 0 ){` |
|        7 | 10356 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10357 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10358 | `		}` |
|        - | 10359 | `		/* Reset the string cursor */` |
|       27 | 10360 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10361 | `		pComp = &sURI.sPath;` |
|       27 | 10362 | `		if( pComp->nByte > 0 ){` |
|       17 | 10363 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10364 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10365 | `		}` |
|        - | 10366 | `		/* Reset the string cursor */` |
|       27 | 10367 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10368 | `		pComp = &sURI.sQuery;` |
|       27 | 10369 | `		if( pComp->nByte > 0 ){` |
|        5 | 10370 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10371 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10372 | `		}` |
|        - | 10373 | `		/* Reset the string cursor */` |
|       27 | 10374 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10375 | `		pComp = &sURI.sFragment;` |
|       27 | 10376 | `		if( pComp->nByte > 0 ){` |
|        5 | 10377 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10378 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10379 | `		}` |
|        - | 10380 | `		/* Return the created array */` |
|       27 | 10381 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10382 | `		/* NOTE:` |
|        - | 10383 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10384 | `		 * automatically as soon we return from this function.` |
|        - | 10385 | `		 */` |
|        - | 10386 | `	}` |
|        - | 10387 | `	/* All done */` |
|       27 | 10388 | `	return PH7_OK;` |
|       15 | 10389 |  |
|        - | 10390 | `/*` |
|        - | 10391 | ` * Section:` |
|        - | 10392 | ` *   Array related routines.` |
|        - | 10393 | ` * Status:` |
|        - | 10394 | ` *    Stable.` |
|        - | 10395 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10396 | ` *  Array related functions that need access to the underlying` |
|        - | 10397 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10398 | ` */` |
|        - | 10399 | `/*` |
|        - | 10400 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10401 | ` * of the following structure.` |
|        - | 10402 | ` */` |
|        - | 10403 | `struct compact_data` |
|        - | 10404 |  |
|        - | 10405 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10406 | `	int nRecCount;      /* Recursion count */` |
|        - | 10407 | `};` |
|        - | 10408 | `/*` |
|        - | 10409 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10410 | ` */` |
|      ! 0 | 10411 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10412 |  |
|      ! 0 | 10413 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10414 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10415 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10416 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10417 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10418 | `		SyString sVar;` |
|      ! 0 | 10419 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10420 | `		if( sVar.nByte > 0 ){` |
|        - | 10421 | `			/* Query the current frame */` |
|      ! 0 | 10422 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10423 | `			/* ^` |
|        - | 10424 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10425 | `			 */` |
|      ! 0 | 10426 | `			if( pKey ){` |
|        - | 10427 | `				/* Perform the insertion */` |
|      ! 0 | 10428 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10429 | `			}` |
|      ! 0 | 10430 | `		}` |
|      ! 0 | 10431 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10432 | `		int rc;` |
|        - | 10433 | `		/* Recursively traverse this array */` |
|      ! 0 | 10434 | `		pData->nRecCount++;` |
|      ! 0 | 10435 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10436 | `		pData->nRecCount--;` |
|      ! 0 | 10437 | `		return rc;` |
|        - | 10438 | `	}` |
|      ! 0 | 10439 | `	return SXRET_OK;` |
|      ! 0 | 10440 |  |
|        - | 10441 | `/*` |
|        - | 10442 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10443 | ` *  Create array containing variables and their values.` |
|        - | 10444 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10445 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10446 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10447 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10448 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10449 | ` * Parameters` |
|        - | 10450 | ` *  $varname` |
|        - | 10451 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10452 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10453 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10454 | ` *   it recursively.` |
|        - | 10455 | ` * Return` |
|        - | 10456 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10457 | ` */` |
|        2 | 10458 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10459 |  |
|        - | 10460 | `	ph7_value *pArray,*pObj;` |
|        3 | 10461 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10462 | `	const char *zName;` |
|        - | 10463 | `	SyString sVar;` |
|        - | 10464 | `	int i,nLen;` |
|        3 | 10465 | `	if( nArg < 1 ){` |
|        - | 10466 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10467 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10468 | `		return PH7_OK;` |
|        - | 10469 | `	}` |
|        - | 10470 | `	/* Create the array */` |
|        3 | 10471 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10472 | `	if( pArray == 0 ){` |
|        - | 10473 | `		/* Out of memory */` |
|      ! 0 | 10474 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10475 | `		/* Return NULL */` |
|      ! 0 | 10476 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10477 | `		return PH7_OK;` |
|        - | 10478 | `	}` |
|        - | 10479 | `	/* Perform the requested operation */` |
|        7 | 10480 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10481 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10482 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10483 | `				struct compact_data sData;` |
|      ! 0 | 10484 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10485 | `				/* Recursively walk the array */` |
|      ! 0 | 10486 | `				sData.nRecCount = 0;` |
|      ! 0 | 10487 | `				sData.pArray = pArray;` |
|      ! 0 | 10488 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10489 | `			}` |
|      ! 0 | 10490 | `		}else{` |
|        - | 10491 | `			/* Extract variable name */` |
|        5 | 10492 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10493 | `			if( nLen > 0 ){` |
|        5 | 10494 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10495 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10496 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10497 | `				if( pObj ){` |
|        5 | 10498 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10499 | `				}` |
|        2 | 10500 | `			}` |
|        - | 10501 | `		}` |
|        3 | 10502 | `	}` |
|        - | 10503 | `	/* Return the array */` |
|        3 | 10504 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10505 | `	return PH7_OK;` |
|        2 | 10506 |  |
|        - | 10507 | `/*` |
|        - | 10508 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10509 | ` * of the following structure.` |
|        - | 10510 | ` */` |
|        - | 10511 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10512 | `struct extract_aux_data` |
|        - | 10513 |  |
|        - | 10514 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10515 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10516 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10517 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10518 | `	int iFlags;           /* Control flags */` |
|        - | 10519 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10520 | `};` |
|        - | 10521 | `/* Forward declaration */` |
|        - | 10522 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10523 | `/*` |
|        - | 10524 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10525 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10526 | ` * Parameters` |
|        - | 10527 | ` * $var_array` |
|        - | 10528 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10529 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10530 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10531 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10532 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10533 | ` * $extract_type` |
|        - | 10534 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10535 | ` *  It can be one of the following values:` |
|        - | 10536 | ` *   EXTR_OVERWRITE` |
|        - | 10537 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10538 | ` *   EXTR_SKIP` |
|        - | 10539 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10540 | ` *   EXTR_PREFIX_SAME` |
|        - | 10541 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 10542 | ` *   EXTR_PREFIX_ALL` |
|        - | 10543 | ` *       Prefix all variable names with prefix.` |
|        - | 10544 | ` *   EXTR_PREFIX_INVALID` |
|        - | 10545 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 10546 | ` *   EXTR_IF_EXISTS` |
|        - | 10547 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 10548 | ` *       otherwise do nothing.` |
|        - | 10549 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 10550 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 10551 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 10552 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 10553 | ` *      the current symbol table.` |
|        - | 10554 | ` * $prefix` |
|        - | 10555 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 10556 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 10557 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 10558 | ` *  underscore character.` |
|        - | 10559 | ` * Return` |
|        - | 10560 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 10561 | ` */` |
|        4 | 10562 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10563 |  |
|        - | 10564 | `	extract_aux_data sAux;` |
|        - | 10565 | `	ph7_hashmap *pMap;` |
|        5 | 10566 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 10567 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 10568 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10569 | `		return PH7_OK;` |
|        - | 10570 | `	}` |
|        - | 10571 | `	/* Point to the target hashmap */` |
|        5 | 10572 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 10573 | `	if( pMap->nEntry < 1 ){` |
|        - | 10574 | `		/* Empty map,return  0 */` |
|      ! 0 | 10575 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 10576 | `		return PH7_OK;` |
|        - | 10577 | `	}` |
|        - | 10578 | `	/* Prepare the aux data */` |
|        5 | 10579 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 10580 | `	if( nArg > 1 ){` |
|        3 | 10581 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 10582 | `		if( nArg > 2 ){` |
|      ! 0 | 10583 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 10584 | `		}` |
|        1 | 10585 | `	}` |
|        5 | 10586 | `	sAux.pVm = pCtx->pVm;` |
|        - | 10587 | `	/* Invoke the worker callback */` |
|        5 | 10588 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 10589 | `	/* Number of variables successfully imported */` |
|        5 | 10590 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 10591 | `	return PH7_OK;` |
|        3 | 10592 |  |
|        - | 10593 | `/*` |
|        - | 10594 | ` * Worker callback for the [extract()] function defined` |
|        - | 10595 | ` * below.` |
|        - | 10596 | ` */` |
|        8 | 10597 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10598 |  |
|        9 | 10599 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 10600 | `	int iFlags = pAux->iFlags;` |
|        9 | 10601 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10602 | `	ph7_value *pObj;` |
|        - | 10603 | `	SyString sVar;` |
|        9 | 10604 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 10605 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 10606 | `	}` |
|        - | 10607 | `	/* Perform a string cast */` |
|        9 | 10608 | `	PH7_MemObjToString(pKey);` |
|        9 | 10609 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10610 | `		/* Unavailable variable name */` |
|      ! 0 | 10611 | `		return SXRET_OK;` |
|        - | 10612 | `	}` |
|        9 | 10613 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 10614 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 10615 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10616 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10617 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10618 | `			);` |
|      ! 0 | 10619 | `	}else{` |
|       13 | 10620 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 10621 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10622 | `	}` |
|        9 | 10623 | `	sVar.zString = pAux->zWorker;` |
|        - | 10624 | `	/* Try to extract the variable */` |
|        9 | 10625 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 10626 | `	if( pObj ){` |
|        - | 10627 | `		/* Collision */` |
|        5 | 10628 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 10629 | `			return SXRET_OK;` |
|        - | 10630 | `		}` |
|        5 | 10631 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 10632 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 10633 | `				/* Already prefixed */` |
|      ! 0 | 10634 | `				return SXRET_OK;` |
|        - | 10635 | `			}` |
|      ! 0 | 10636 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 10637 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 10638 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10639 | `				);` |
|      ! 0 | 10640 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 10641 | `		}` |
|        3 | 10642 | `	}else{` |
|        - | 10643 | `		/* Create the variable */` |
|        5 | 10644 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 10645 | `	}` |
|        9 | 10646 | `	if( pObj ){` |
|        - | 10647 | `		/* Overwrite the old value */` |
|        9 | 10648 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 10649 | `		/* Increment counter */` |
|        9 | 10650 | `		pAux->iCount++;` |
|        4 | 10651 | `	}` |
|        9 | 10652 | `	return SXRET_OK;` |
|        5 | 10653 |  |
|        - | 10654 | `/*` |
|        - | 10655 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 10656 | ` * defined below.` |
|        - | 10657 | ` */` |
|        2 | 10658 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 10659 |  |
|        3 | 10660 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 10661 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 10662 | `	ph7_value *pObj;` |
|        - | 10663 | `	SyString sVar;` |
|        - | 10664 | `	/* Perform a string cast */` |
|        3 | 10665 | `	PH7_MemObjToString(pKey);` |
|        3 | 10666 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 10667 | `		/* Unavailable variable name */` |
|      ! 0 | 10668 | `		return SXRET_OK;` |
|        - | 10669 | `	}` |
|        3 | 10670 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 10671 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 10672 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 10673 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 10674 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 10675 | `			);` |
|        2 | 10676 | `	}else{` |
|      ! 0 | 10677 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 10678 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 10679 | `	}` |
|        3 | 10680 | `	sVar.zString = pAux->zWorker;` |
|        - | 10681 | `	/* Extract the variable */` |
|        3 | 10682 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 10683 | `	if( pObj ){` |
|        3 | 10684 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 10685 | `	}` |
|        3 | 10686 | `	return SXRET_OK;` |
|        2 | 10687 |  |
|        - | 10688 | `/*` |
|        - | 10689 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 10690 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 10691 | ` * Parameters` |
|        - | 10692 | ` * $types` |
|        - | 10693 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 10694 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 10695 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 10696 | ` *  POST includes the POST uploaded file information.` |
|        - | 10697 | ` *  Note:` |
|        - | 10698 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 10699 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 10700 | ` * $prefix` |
|        - | 10701 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 10702 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 10703 | ` *  variable named $pref_userid.` |
|        - | 10704 | ` * Return` |
|        - | 10705 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10706 | ` */` |
|        2 | 10707 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10708 |  |
|        - | 10709 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 10710 | `	extract_aux_data sAux;` |
|        - | 10711 | `	int nLen,nPrefixLen;` |
|        - | 10712 | `	ph7_value *pSuper;` |
|        - | 10713 | `	ph7_vm *pVm;` |
|        - | 10714 | `	/* By default import only $_GET variables  */` |
|        3 | 10715 | `	zImport = "G";` |
|        3 | 10716 | `	nLen = (int)sizeof(char);` |
|        3 | 10717 | `	zPrefix = 0;` |
|        3 | 10718 | `	nPrefixLen = 0;` |
|        3 | 10719 | `	if( nArg > 0 ){` |
|        3 | 10720 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 10721 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 10722 | `		}` |
|        3 | 10723 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10724 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 10725 | `		}` |
|        1 | 10726 | `	}` |
|        - | 10727 | `	/* Point to the underlying VM */` |
|        3 | 10728 | `	pVm = pCtx->pVm;` |
|        - | 10729 | `	/* Initialize the aux data */` |
|        3 | 10730 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 10731 | `	sAux.zPrefix = zPrefix;` |
|        3 | 10732 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 10733 | `	sAux.pVm = pVm;` |
|        - | 10734 | `	/* Extract */` |
|        3 | 10735 | `	zEnd = &zImport[nLen];` |
|        5 | 10736 | `	while( zImport < zEnd ){` |
|        3 | 10737 | `		int c = zImport[0];` |
|        3 | 10738 | `		pSuper = 0;` |
|        3 | 10739 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 10740 | `			/* Import $_GET variables */` |
|        3 | 10741 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 10742 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 10743 | `			/* Import $_POST variables */` |
|      ! 0 | 10744 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 10745 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 10746 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 10747 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 10748 | `		}` |
|        3 | 10749 | `		if( pSuper ){` |
|        - | 10750 | `			/* Iterate throw array entries */` |
|        3 | 10751 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 10752 | `		}` |
|        - | 10753 | `		/* Advance the cursor */` |
|        3 | 10754 | `		zImport++;` |
|        1 | 10755 | `	}` |
|        - | 10756 | `	/* All done,return TRUE*/` |
|        3 | 10757 | `	ph7_result_bool(pCtx,0);` |
|        3 | 10758 | `	return PH7_OK;` |
|        1 | 10759 |  |
|        - | 10760 | `/*` |
|        - | 10761 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 10762 | ` * Refer to the eval() language construct implementation for more` |
|        - | 10763 | ` * information.` |
|        - | 10764 | ` */` |
|    10344 | 10765 | `static sxi32 VmEvalChunk(` |
|        - | 10766 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 10767 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 10768 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 10769 | `	int iFlags,         /* Compile flag */` |
|        - | 10770 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 10771 | `	)` |
|        2 | 10772 |  |
|        - | 10773 | `	SySet *pByteCode,aByteCode;` |
|        - | 10774 | `	SyBlob sSavedNs;` |
|    10346 | 10775 | `	ProcConsumer xErr = 0;` |
|    10346 | 10776 | `	void *pErrData = 0;` |
|        - | 10777 | `	/* Initialize bytecode container */` |
|    10346 | 10778 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10346 | 10779 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 10780 | `	/* Reset the code generator */` |
|    10346 | 10781 | `	if( bTrueReturn ){` |
|        - | 10782 | `		/* Included file,log compile-time errors */` |
|     7637 | 10783 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 | 10784 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 | 10785 | `	}` |
|    10346 | 10786 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 10787 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 10788 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 10789 | `	 * the caller's namespace is restored. */` |
|    10346 | 10790 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10346 | 10791 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10346 | 10792 | `	if( bTrueReturn ){` |
|        - | 10793 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 | 10794 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 | 10795 | `	}` |
|        - | 10796 | `	/* Swap bytecode container */` |
|    10346 | 10797 | `	pByteCode = pVm->pByteContainer;` |
|    10346 | 10798 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 10799 | `	/* Compile the chunk */` |
|    10346 | 10800 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15518 | 10801 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 10802 | `		/* Compilation error,return false */` |
|        3 | 10803 | `		if( pCtx ){` |
|        3 | 10804 | `			ph7_result_bool(pCtx,0);` |
|        1 | 10805 | `		}` |
|        2 | 10806 | `	}else{` |
|        - | 10807 | `		/* Mount any newly defined classes */` |
|        - | 10808 | `		SyHashEntry *pEntry;` |
|        - | 10809 | `		ph7_class *pClass;` |
|        - | 10810 | `		ph7_value sResult; /* Return value */` |
|        - | 10811 | `		sxi32 rc;` |
|    10344 | 10812 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   304125 | 10813 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   288612 | 10814 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 10815 | `			/* Only mount classes that haven't been mounted yet */` |
|   288612 | 10816 | `			if( !pClass->bMounted ){` |
|    70324 | 10817 | `				rc = VmMountUserClass(pVm,pClass);` |
|    70324 | 10818 | `				if( rc != SXRET_OK ){` |
|        - | 10819 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 10820 | `					if( pCtx ){` |
|      ! 0 | 10821 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 10822 | `					}` |
|      ! 0 | 10823 | `					goto Cleanup;` |
|        - | 10824 | `				}` |
|    35161 | 10825 | `			}` |
|        2 | 10826 | `		}` |
|    10344 | 10827 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 10828 | `			/* Out of memory */` |
|      ! 0 | 10829 | `			if( pCtx ){` |
|      ! 0 | 10830 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 10831 | `			}` |
|      ! 0 | 10832 | `			goto Cleanup;` |
|        - | 10833 | `		}` |
|    10344 | 10834 | `		if( bTrueReturn ){` |
|        - | 10835 | `			/* Assume a boolean true return value */` |
|     7637 | 10836 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 | 10837 | `		}else{` |
|        - | 10838 | `			/* Assume a null return value */` |
|     2708 | 10839 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 10840 | `		}` |
|        - | 10841 | `		/* Execute the compiled chunk */` |
|    10344 | 10842 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10344 | 10843 | `		if( pCtx ){` |
|        - | 10844 | `			/* Set the execution result */` |
|     7650 | 10845 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 | 10846 | `		}` |
|    10344 | 10847 | `		PH7_MemObjRelease(&sResult);` |
|        - | 10848 | `	}` |
|     5172 | 10849 | `Cleanup:` |
|        - | 10850 | `	/* Cleanup the mess left behind */` |
|    10346 | 10851 | `	pVm->pByteContainer = pByteCode;` |
|    10346 | 10852 | `	SySetRelease(&aByteCode);` |
|        - | 10853 | `	/* Restore caller's namespace state */` |
|    10346 | 10854 | `	SyBlobReset(&pVm->sNamespace);` |
|    10346 | 10855 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10346 | 10856 | `	SyBlobRelease(&sSavedNs);` |
|    10346 | 10857 | `	return SXRET_OK;` |
|        2 | 10858 |  |
|        - | 10859 | `/*` |
|        - | 10860 | ` * value eval(string $code)` |
|        - | 10861 | ` *   Evaluate a string as PHP code.` |
|        - | 10862 | ` * Parameter` |
|        - | 10863 | ` *  code: PHP code to evaluate.` |
|        - | 10864 | ` * Return` |
|        - | 10865 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 10866 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 10867 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 10868 | ` */` |
|       16 | 10869 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10870 |  |
|        - | 10871 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 10872 | `	if( nArg < 1 ){` |
|        - | 10873 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10874 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10875 | `		return SXRET_OK;` |
|        - | 10876 | `	}` |
|        - | 10877 | `	/* Chunk to evaluate */` |
|       18 | 10878 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 10879 | `	if( sChunk.nByte < 1 ){` |
|        - | 10880 | `		/* Empty string,return NULL */` |
|        3 | 10881 | `		ph7_result_null(pCtx);` |
|        3 | 10882 | `		return SXRET_OK;` |
|        - | 10883 | `	}` |
|        - | 10884 | `	/* Eval the chunk */` |
|       16 | 10885 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 10886 | `	return SXRET_OK;` |
|       10 | 10887 |  |
|        - | 10888 | `/*` |
|        - | 10889 | ` * Check if a file path is already included.` |
|        - | 10890 | ` */` |
|    15268 | 10891 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 10892 |  |
|        - | 10893 | `	SyString *aEntries;` |
|        - | 10894 | `	sxu32 n;` |
|    15269 | 10895 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 10896 | `	/* Perform a linear search */` |
| 58267061 | 10897 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 10898 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 10899 | `			/* Already included */` |
|        7 | 10900 | `			return TRUE;` |
|        - | 10901 | `		}` |
| 29125897 | 10902 | `	}` |
|    15263 | 10903 | `	return FALSE;` |
|     7635 | 10904 |  |
|        - | 10905 | `/*` |
|        - | 10906 | ` * Push a file path in the appropriate VM container.` |
|        - | 10907 | ` */` |
|    17954 | 10908 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 10909 |  |
|        - | 10910 | `	SyString sPath;` |
|        - | 10911 | `	char *zDup;` |
|        - | 10912 | `#ifdef __WINNT__` |
|        - | 10913 | `	char *zCur;` |
|        - | 10914 | `#endif` |
|        - | 10915 | `	sxi32 rc;` |
|    17956 | 10916 | `	if( nLen < 0 ){` |
|     2688 | 10917 | `		nLen = SyStrlen(zPath);` |
|     1343 | 10918 | `	}` |
|        - | 10919 | `	/* Duplicate the file path first */` |
|    17956 | 10920 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17956 | 10921 | `	if( zDup == 0 ){` |
|      ! 0 | 10922 | `		return SXERR_MEM;` |
|        - | 10923 | `	}` |
|        - | 10924 | `#ifdef __WINNT__` |
|        - | 10925 | `	/* Normalize path on windows` |
|        - | 10926 | `	 * Example:` |
|        - | 10927 | `	 *    Path/To/File.php` |
|        - | 10928 | `	 * becomes` |
|        - | 10929 | `	 *   path\to\file.php` |
|        - | 10930 | `	 */` |
|        2 | 10931 | `	zCur = zDup;` |
|        2 | 10932 | `	while( zCur[0] != 0 ){` |
|        2 | 10933 | `		if( zCur[0] == '/' ){` |
|        2 | 10934 | `			zCur[0] = '\\';` |
|        2 | 10935 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10936 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10937 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10938 | `		}` |
|        2 | 10939 | `		zCur++;` |
|        2 | 10940 | `	}` |
|        - | 10941 | `#endif` |
|        - | 10942 | `	/* Install the file path */` |
|    17956 | 10943 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17956 | 10944 | `	if( !bMain ){` |
|    15269 | 10945 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10946 | `			/* Already included */` |
|        7 | 10947 | `			*pNew = 0;` |
|        4 | 10948 | `		}else{` |
|        - | 10949 | `			/* Insert in the corresponding container */` |
|    15263 | 10950 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 10951 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10952 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10953 | `				return rc;` |
|        - | 10954 | `			}` |
|    15263 | 10955 | `			*pNew = 1;` |
|        - | 10956 | `		}` |
|     7634 | 10957 | `	}` |
|    17956 | 10958 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17956 | 10959 | `	return SXRET_OK;` |
|     8979 | 10960 |  |
|        - | 10961 | `/*` |
|        - | 10962 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10963 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10964 | ` * indicates failure.` |
|        - | 10965 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10966 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10967 | ` * operations.` |
|        - | 10968 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10969 | ` * this function is a no-op.` |
|        - | 10970 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10971 | ` * constructs for more information.` |
|        - | 10972 | ` */` |
|     7642 | 10973 | `static sxi32 VmExecIncludedFile(` |
|        - | 10974 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10975 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10976 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10977 | `	 )` |
|        2 | 10978 |  |
|        - | 10979 | `	sxi32 rc;` |
|        - | 10980 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10981 | `	const ph7_io_stream *pStream;` |
|        - | 10982 | `	SyBlob sContents;` |
|        - | 10983 | `	void *pHandle;` |
|        - | 10984 | `	ph7_vm *pVm;` |
|        - | 10985 | `	int isNew;` |
|        - | 10986 | `	/* Initialize fields */` |
|     7644 | 10987 | `	pVm = pCtx->pVm;` |
|     7644 | 10988 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 10989 | `	isNew = 0;` |
|        - | 10990 | `	/* Extract the associated stream */` |
|     7644 | 10991 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10992 | `	/*` |
|        - | 10993 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10994 | `	 * in a read-only mode.` |
|        - | 10995 | `	 */` |
|     7644 | 10996 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 10997 | `	if( pHandle == 0 ){` |
|        3 | 10998 | `		return SXERR_IO;` |
|        - | 10999 | `	}` |
|     7641 | 11000 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 11001 | `	if( IncludeOnce && !isNew ){` |
|        - | 11002 | `		/* Already included */` |
|        5 | 11003 | `		rc = SXERR_EXISTS;` |
|        3 | 11004 | `	}else{` |
|        - | 11005 | `		/* Read the whole file contents */` |
|     7637 | 11006 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 11007 | `		if( rc == SXRET_OK ){` |
|        - | 11008 | `			SyString sScript;` |
|        - | 11009 | `			/* Compile and execute the script */` |
|     7637 | 11010 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 11011 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 11012 | `		}` |
|        - | 11013 | `	}` |
|        - | 11014 | `	/* Pop from the set of included file */` |
|     7641 | 11015 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11016 | `	/* Close the handle */` |
|     7641 | 11017 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11018 | `	/* Release the working buffer */` |
|     7641 | 11019 | `	SyBlobRelease(&sContents);` |
|        - | 11020 | `#else` |
|        - | 11021 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11022 | `	SXUNUSED(pPath);` |
|        - | 11023 | `	SXUNUSED(IncludeOnce);` |
|        - | 11024 | `	rc = SXERR_IO;` |
|        - | 11025 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 11026 | `	return rc;` |
|     3823 | 11027 |  |
|        - | 11028 | `/*` |
|        - | 11029 | ` * string get_include_path(void)` |
|        - | 11030 | ` *  Gets the current include_path configuration option.` |
|        - | 11031 | ` * Parameter` |
|        - | 11032 | ` *  None` |
|        - | 11033 | ` * Return` |
|        - | 11034 | ` *  Included paths as a string` |
|        - | 11035 | ` */` |
|        2 | 11036 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11037 |  |
|        3 | 11038 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11039 | `	SyString *aEntry;` |
|        - | 11040 | `	int dir_sep;` |
|        - | 11041 | `	sxu32 n;` |
|        - | 11042 | `#ifdef __WINNT__` |
|        1 | 11043 | `	dir_sep = ';';` |
|        - | 11044 | `#else` |
|        - | 11045 | `	/* Assume UNIX path separator */` |
|        2 | 11046 | `	dir_sep = ':';` |
|        - | 11047 | `#endif` |
|        1 | 11048 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11049 | `	SXUNUSED(apArg);` |
|        - | 11050 | `	/* Point to the list of import paths */` |
|        3 | 11051 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11052 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11053 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11054 | `		if( n > 0 ){` |
|        - | 11055 | `			/* Append dir seprator */` |
|      ! 0 | 11056 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11057 | `		}` |
|        - | 11058 | `		/* Append path */` |
|        3 | 11059 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11060 | `	}` |
|        3 | 11061 | `	return PH7_OK;` |
|        1 | 11062 |  |
|        - | 11063 | `/*` |
|        - | 11064 | ` * string get_get_included_files(void)` |
|        - | 11065 | ` *  Gets the current include_path configuration option.` |
|        - | 11066 | ` * Parameter` |
|        - | 11067 | ` *  None` |
|        - | 11068 | ` * Return` |
|        - | 11069 | ` *  Included paths as a string` |
|        - | 11070 | ` */` |
|        2 | 11071 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11072 |  |
|        3 | 11073 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11074 | `	ph7_value *pArray,*pWorker;` |
|        - | 11075 | `	SyString *pEntry;` |
|        - | 11076 | `	int c,d;` |
|        - | 11077 | `	/* Create an array and a working value */` |
|        3 | 11078 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11079 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11080 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11081 | `		/* Out of memory,return null */` |
|      ! 0 | 11082 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11083 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11084 | `		SXUNUSED(apArg);` |
|      ! 0 | 11085 | `		return PH7_OK;` |
|        - | 11086 | `	}` |
|        3 | 11087 | `	c = d = '/';` |
|        - | 11088 | `#ifdef __WINNT__` |
|        1 | 11089 | `	d = '\\';` |
|        - | 11090 | `#endif` |
|        - | 11091 | `	/* Iterate throw entries */` |
|        3 | 11092 | `	SySetResetCursor(pFiles);` |
|     3689 | 11093 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11094 | `		const char *zBase,*zEnd;` |
|        - | 11095 | `		int iLen;` |
|        - | 11096 | `		/* reset the string cursor */` |
|     3687 | 11097 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11098 | `		/* Extract base name */` |
|     3687 | 11099 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11100 | `		/* Ignore trailing '/' */` |
|     5530 | 11101 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11102 | `			zEnd--;` |
|      ! 0 | 11103 | `		}` |
|     3687 | 11104 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 11105 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 11106 | `			zEnd--;` |
|        1 | 11107 | `		}` |
|     3687 | 11108 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 11109 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11110 | `		/* Copy entry name */` |
|     3687 | 11111 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11112 | `		/* Perform the insertion */` |
|     3687 | 11113 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11114 | `	}` |
|        - | 11115 | `	/* All done,return the created array */` |
|        3 | 11116 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11117 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11118 | `	 * by the engine as soon we return from this foreign` |
|        - | 11119 | `	 * function.` |
|        - | 11120 | `	 */` |
|        3 | 11121 | `	return PH7_OK;` |
|        2 | 11122 |  |
|        - | 11123 | `/*` |
|        - | 11124 | ` * include:` |
|        - | 11125 | ` * According to the PHP reference manual.` |
|        - | 11126 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11127 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11128 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11129 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11130 | ` *  and the current working directory before failing. The include()` |
|        - | 11131 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11132 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11133 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11134 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11135 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11136 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11137 | ` *  directory to find the requested file.` |
|        - | 11138 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11139 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11140 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11141 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11142 | ` */` |
|     7630 | 11143 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11144 |  |
|        - | 11145 | `	SyString sFile;` |
|        - | 11146 | `	sxi32 rc;` |
|     7632 | 11147 | `	if( nArg < 1 ){` |
|        - | 11148 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11149 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11150 | `		return SXRET_OK;` |
|        - | 11151 | `	}` |
|        - | 11152 | `	/* File to include */` |
|     7632 | 11153 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 11154 | `	if( sFile.nByte < 1 ){` |
|        - | 11155 | `		/* Empty string,return NULL */` |
|      ! 0 | 11156 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11157 | `		return SXRET_OK;` |
|        - | 11158 | `	}` |
|        - | 11159 | `	/* Open,compile and execute the desired script */` |
|     7632 | 11160 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 11161 | `	if( rc != SXRET_OK ){` |
|        - | 11162 | `		/* Emit a warning and return false */` |
|        3 | 11163 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11164 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11165 | `	}` |
|     7632 | 11166 | `	return SXRET_OK;` |
|     3817 | 11167 |  |
|        - | 11168 | `/*` |
|        - | 11169 | ` * include_once:` |
|        - | 11170 | ` *  According to the PHP reference manual.` |
|        - | 11171 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11172 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11173 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11174 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11175 | ` *   just once.` |
|        - | 11176 | ` */` |
|        4 | 11177 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11178 |  |
|        - | 11179 | `	SyString sFile;` |
|        - | 11180 | `	sxi32 rc;` |
|        5 | 11181 | `	if( nArg < 1 ){` |
|        - | 11182 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11183 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11184 | `		return SXRET_OK;` |
|        - | 11185 | `	}` |
|        - | 11186 | `	/* File to include */` |
|        5 | 11187 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11188 | `	if( sFile.nByte < 1 ){` |
|        - | 11189 | `		/* Empty string,return NULL */` |
|      ! 0 | 11190 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11191 | `		return SXRET_OK;` |
|        - | 11192 | `	}` |
|        - | 11193 | `	/* Open,compile and execute the desired script */` |
|        5 | 11194 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11195 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11196 | `		/* File already included,return TRUE */` |
|        3 | 11197 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11198 | `		return SXRET_OK;` |
|        - | 11199 | `	}` |
|        3 | 11200 | `	if( rc != SXRET_OK ){` |
|        - | 11201 | `		/* Emit a warning and return false */` |
|      ! 0 | 11202 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11203 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11204 | ` 	}` |
|        3 | 11205 | `	return SXRET_OK;` |
|        3 | 11206 |  |
|        - | 11207 | `/*` |
|        - | 11208 | ` * require.` |
|        - | 11209 | ` *  According to the PHP reference manual.` |
|        - | 11210 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11211 | ` *   also produce a fatal level error.` |
|        - | 11212 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11213 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11214 | ` */` |
|        4 | 11215 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11216 |  |
|        - | 11217 | `	SyString sFile;` |
|        - | 11218 | `	sxi32 rc;` |
|        5 | 11219 | `	if( nArg < 1 ){` |
|        - | 11220 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11221 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11222 | `		return SXRET_OK;` |
|        - | 11223 | `	}` |
|        - | 11224 | `	/* File to include */` |
|        5 | 11225 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11226 | `	if( sFile.nByte < 1 ){` |
|        - | 11227 | `		/* Empty string,return NULL */` |
|      ! 0 | 11228 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11229 | `		return SXRET_OK;` |
|        - | 11230 | `	}` |
|        - | 11231 | `	/* Open,compile and execute the desired script */` |
|        5 | 11232 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11233 | `	if( rc != SXRET_OK ){` |
|        - | 11234 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11235 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11236 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11237 | `		return PH7_ABORT;` |
|        - | 11238 | `	}` |
|        5 | 11239 | `	return SXRET_OK;` |
|        3 | 11240 |  |
|        - | 11241 | `/*` |
|        - | 11242 | ` * require_once:` |
|        - | 11243 | ` *  According to the PHP reference manual.` |
|        - | 11244 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11245 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11246 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11247 | ` *   and how it differs from its non _once siblings.` |
|        - | 11248 | ` */` |
|        4 | 11249 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11250 |  |
|        - | 11251 | `	SyString sFile;` |
|        - | 11252 | `	sxi32 rc;` |
|        5 | 11253 | `	if( nArg < 1 ){` |
|        - | 11254 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11255 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11256 | `		return SXRET_OK;` |
|        - | 11257 | `	}` |
|        - | 11258 | `	/* File to include */` |
|        5 | 11259 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11260 | `	if( sFile.nByte < 1 ){` |
|        - | 11261 | `		/* Empty string,return NULL */` |
|      ! 0 | 11262 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11263 | `		return SXRET_OK;` |
|        - | 11264 | `	}` |
|        - | 11265 | `	/* Open,compile and execute the desired script */` |
|        5 | 11266 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11267 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11268 | `		/* File already included,return TRUE */` |
|        3 | 11269 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11270 | `		return SXRET_OK;` |
|        - | 11271 | `	}` |
|        3 | 11272 | `	if( rc != SXRET_OK ){` |
|        - | 11273 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11274 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11275 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11276 | `		return PH7_ABORT;` |
|        - | 11277 | `	}` |
|        3 | 11278 | `	return SXRET_OK;` |
|        3 | 11279 |  |
|        - | 11280 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11281 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11282 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11283 | `/* Table of built-in VM functions. */` |
|        - | 11284 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11285 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11286 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11287 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11288 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11289 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11290 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11291 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11292 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11293 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11294 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11295 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11296 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11297 | `	    /* Constants management */` |
|        - | 11298 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11299 | `	{ "define",   vm_builtin_define               },` |
|        - | 11300 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11301 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11302 | `	   /* Class/Object functions */` |
|        - | 11303 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11304 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11305 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11306 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11307 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11308 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11309 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11310 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11311 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11312 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11313 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11314 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11315 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11316 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11317 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11318 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11319 | `	   /* Random numbers/strings generators */` |
|        - | 11320 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11321 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11322 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11323 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11324 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11325 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11326 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11327 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11328 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11329 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11330 | `	   /* Language constructs functions */` |
|        - | 11331 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11332 | `	{ "print", vm_builtin_print                   },` |
|        - | 11333 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11334 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11335 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11336 | `	  /* Variable handling functions */` |
|        - | 11337 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11338 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11339 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11340 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11341 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11342 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11343 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11344 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11345 | `	  /* Ouput control functions */` |
|        - | 11346 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11347 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11348 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11349 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11350 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11351 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11352 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11353 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11354 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11355 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11356 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11357 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11358 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11359 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11360 | `	  /* Assertion functions */` |
|        - | 11361 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11362 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11363 | `	  /* Error reporting functions */` |
|        - | 11364 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11365 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11366 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11367 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11368 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11369 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11370 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11371 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11372 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11373 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11374 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11375 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11376 | `	  /* Release info */` |
|        - | 11377 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11378 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11379 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11380 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11381 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11382 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11383 | `	  /* hashmap */` |
|        - | 11384 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11385 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11386 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11387 | `	  /* URL related function */` |
|        - | 11388 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11389 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11390 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11391 | `	   /* XML processing functions */` |
|        - | 11392 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11393 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11394 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11395 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11396 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11397 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11398 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11399 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11400 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11401 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11402 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11403 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11404 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11405 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11406 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11407 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 11408 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 11409 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 11410 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 11411 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 11412 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 11413 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11414 | `	   /* UTF-8 encoding/decoding */` |
|        - | 11415 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 11416 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 11417 | `	   /* Command line processing */` |
|        - | 11418 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 11419 | `	   /* JSON encoding/decoding */` |
|        - | 11420 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 11421 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 11422 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 11423 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 11424 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 11425 | `	   /* Files/URI inclusion facility */` |
|        - | 11426 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 11427 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 11428 | `	{ "include",      vm_builtin_include          },` |
|        - | 11429 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 11430 | `	{ "require",      vm_builtin_require          },` |
|        - | 11431 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 11432 | `};` |
|        - | 11433 | `/*` |
|        - | 11434 | ` * Register the built-in VM functions defined above.` |
|        - | 11435 | ` */` |
|     2432 | 11436 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 11437 |  |
|        - | 11438 | `	sxi32 rc;` |
|        - | 11439 | `	sxu32 n;` |
|   304002 | 11440 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 11441 | `		/* Note that these special functions have access` |
|        - | 11442 | `		 * to the underlying virtual machine as their` |
|        - | 11443 | `		 * private data.` |
|        - | 11444 | `		 */` |
|   301570 | 11445 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   301570 | 11446 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11447 | `			return rc;` |
|        - | 11448 | `		}` |
|   150786 | 11449 | `	}` |
|     2434 | 11450 | `	return SXRET_OK;` |
|     1218 | 11451 |  |
|        - | 11452 | `/*` |
|        - | 11453 | ` * Check if the given name refer to an installed class.` |
|        - | 11454 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 11455 | ` */` |
|    23106 | 11456 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 11457 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 11458 | `	const char *zName,  /* Name of the target class */` |
|        - | 11459 | `	sxu32 nByte,        /* zName length */` |
|        - | 11460 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 11461 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 11462 | `						 */` |
|        - | 11463 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 11464 | `	)` |
|        2 | 11465 |  |
|        - | 11466 | `	SyHashEntry *pEntry;` |
|        - | 11467 | `	ph7_class *pClass;` |
|    11553 | 11468 | `	SXUNUSED(iNest);` |
|        - | 11469 | `	/* Exact class lookup.` |
|        - | 11470 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 11471 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    23108 | 11472 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    23108 | 11473 | `	if( pEntry == 0 ){` |
|       10 | 11474 | `		return 0;` |
|        - | 11475 | `	}` |
|    23100 | 11476 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    23100 | 11477 | `	if( !iLoadable ){` |
|    21920 | 11478 | `		return pClass;` |
|        - | 11479 | `	}` |
|        - | 11480 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1182 | 11481 | `	while(pClass){` |
|     1182 | 11482 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1182 | 11483 | `			return pClass;` |
|        - | 11484 | `		}` |
|      ! 0 | 11485 | `		pClass = pClass->pNextName;` |
|      ! 0 | 11486 | `	}` |
|      ! 0 | 11487 | `	return 0;` |
|    11555 | 11488 |  |
|        - | 11489 | `/*` |
|        - | 11490 | ` * Reference Table Implementation` |
|        - | 11491 | ` * Status: stable <chm@symisc.net>` |
|        - | 11492 | ` * Intro` |
|        - | 11493 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 11494 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 11495 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 11496 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 11497 | ` *  Refer to the official for more information on this powerful` |
|        - | 11498 | ` *  extension.` |
|        - | 11499 | ` */` |
|        - | 11500 | `/*` |
|        - | 11501 | ` * Allocate a new reference entry.` |
|        - | 11502 | ` */` |
|  3012226 | 11503 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 11504 |  |
|        - | 11505 | `	VmRefObj *pRef;` |
|        - | 11506 | `	/* Allocate a new instance */` |
|  3012228 | 11507 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3012228 | 11508 | `	if( pRef == 0 ){` |
|      ! 0 | 11509 | `		return 0;` |
|        - | 11510 | `	}` |
|        - | 11511 | `	/* Zero the structure */` |
|  3012228 | 11512 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 11513 | `	/* Initialize fields */` |
|  3012228 | 11514 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3012228 | 11515 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3012228 | 11516 | `	pRef->nIdx = nIdx;` |
|  3012228 | 11517 | `	return pRef;` |
|  1506115 | 11518 |  |
|        - | 11519 | `/*` |
|        - | 11520 | ` * Default hash function used by the reference table` |
|        - | 11521 | ` * for lookup/insertion operations.` |
|        - | 11522 | ` */` |
| 16696340 | 11523 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 11524 |  |
|        - | 11525 | `	/* Calculate the hash based on the memory object index */` |
| 16696342 | 11526 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 11527 |  |
|        - | 11528 | `/*` |
|        - | 11529 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 11530 | ` * in the reference table.` |
|        - | 11531 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 11532 | ` * otherwise.` |
|        - | 11533 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11534 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11535 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11536 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11537 | ` * Refer to the official for more information on this powerful` |
|        - | 11538 | ` * extension.` |
|        - | 11539 | ` */` |
|  8988634 | 11540 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 11541 |  |
|        - | 11542 | `	VmRefObj *pRef;` |
|        - | 11543 | `	sxu32 nBucket;` |
|        - | 11544 | `	/* Point to the appropriate bucket */` |
|  8988636 | 11545 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 11546 | `	/* Perform the lookup */` |
|  8988636 | 11547 | `	pRef = pVm->apRefObj[nBucket];` |
| 19311139 | 11548 | `	for(;;){` |
| 38608042 | 11549 | `		if( pRef == 0 ){` |
|  3090356 | 11550 | `			break;` |
|        - | 11551 | `		}` |
| 35517688 | 11552 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 11553 | `			/* Entry found */` |
|  5898282 | 11554 | `			return pRef;` |
|        - | 11555 | `		}` |
|        - | 11556 | `		/* Point to the next entry */` |
| 29619408 | 11557 | `		pRef = pRef->pNextCollide;` |
|        2 | 11558 | `	}` |
|        - | 11559 | `	/* No such entry,return NULL */` |
|  3090356 | 11560 | `	return 0;` |
|  4494319 | 11561 |  |
|        - | 11562 | `/*` |
|        - | 11563 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 11564 | ` *` |
|        - | 11565 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11566 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11567 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11568 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11569 | ` * Refer to the official for more information on this powerful` |
|        - | 11570 | ` * extension.` |
|        - | 11571 | ` */` |
|  3012226 | 11572 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 11573 |  |
|        - | 11574 | `	sxu32 nBucket;` |
|  3012228 | 11575 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 11576 | `		VmRefObj **apNew;` |
|        - | 11577 | `		sxu32 nNew;` |
|        - | 11578 | `		/* Allocate a larger table */` |
|     4172 | 11579 | `		nNew = pVm->nRefSize << 1;` |
|     4172 | 11580 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4172 | 11581 | `		if( apNew ){` |
|     4172 | 11582 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 11583 | `			sxu32 n;` |
|        - | 11584 | `			/* Zero the structure */` |
|     4172 | 11585 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 11586 | `			/* Rehash all referenced entries */` |
|  2842546 | 11587 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 11588 | `				/* Remove old collision links */` |
|  2838376 | 11589 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 11590 | `				/* Point to the appropriate bucket */` |
|  2838376 | 11591 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 11592 | `				/* Insert the entry  */` |
|  2838376 | 11593 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2838376 | 11594 | `				if( apNew[nBucket] ){` |
|  2298896 | 11595 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 11596 | `				}` |
|  2838376 | 11597 | `				apNew[nBucket] = pEntry;` |
|        - | 11598 | `				/* Point to the next entry */` |
|  2838376 | 11599 | `				pEntry = pEntry->pNext;` |
|  1419189 | 11600 | `			}` |
|        - | 11601 | `			/* Release the old table */` |
|     4172 | 11602 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 11603 | `			/* Install the new one */` |
|     4172 | 11604 | `			pVm->apRefObj = apNew;` |
|     4172 | 11605 | `			pVm->nRefSize = nNew;` |
|     2085 | 11606 | `		}` |
|     2085 | 11607 | `	}` |
|        - | 11608 | `	/* Point to the appropriate bucket */` |
|  3012228 | 11609 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 11610 | `	/* Insert the entry */` |
|  3012228 | 11611 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3012228 | 11612 | `	if( pVm->apRefObj[nBucket] ){` |
|  2493306 | 11613 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1246649 | 11614 | `	}` |
|  3012228 | 11615 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3012228 | 11616 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3012228 | 11617 | `	pVm->nRefUsed++;` |
|  3012228 | 11618 | `	return SXRET_OK;` |
|        2 | 11619 |  |
|        - | 11620 | `/*` |
|        - | 11621 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 11622 | ` * the reference table.` |
|        - | 11623 | ` * This function is invoked when the user perform an unset` |
|        - | 11624 | ` * call [i.e: unset($var); ].` |
|        - | 11625 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11626 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11627 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11628 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11629 | ` * Refer to the official for more information on this powerful` |
|        - | 11630 | ` * extension.` |
|        - | 11631 | ` */` |
|  2977464 | 11632 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 11633 |  |
|        - | 11634 | `	ph7_hashmap_node **apNode;` |
|        - | 11635 | `	SyHashEntry **apEntry;` |
|        - | 11636 | `	sxu32 n;` |
|        - | 11637 | `	/* Point to the reference table */` |
|  2977466 | 11638 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2977466 | 11639 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 11640 | `	/* Unlink the entry from the reference table */` |
|  3061460 | 11641 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    83996 | 11642 | `		if( apEntry[n] ){` |
|    83946 | 11643 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    41972 | 11644 | `		}` |
|    41999 | 11645 | `	}` |
|  5873628 | 11646 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2896164 | 11647 | `		if( apNode[n] ){` |
|     6794 | 11648 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 11649 | `		}` |
|  1448083 | 11650 | `	}` |
|  2977466 | 11651 | `	if( pRef->pPrevCollide ){` |
|  1120360 | 11652 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   560414 | 11653 | `	}else{` |
|  1857108 | 11654 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 11655 | `	}` |
|  2977466 | 11656 | `	if( pRef->pNextCollide ){` |
|  1682149 | 11657 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   841109 | 11658 | `	}` |
|  2977466 | 11659 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 11660 | `	/* Release the node */` |
|  2977466 | 11661 | `	SySetRelease(&pRef->aReference);` |
|  2977466 | 11662 | `	SySetRelease(&pRef->aArrEntries);` |
|  2977466 | 11663 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2977466 | 11664 | `	pVm->nRefUsed--;` |
|  2977466 | 11665 | `	return SXRET_OK;` |
|        2 | 11666 |  |
|        - | 11667 | `/*` |
|        - | 11668 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 11669 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11670 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11671 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11672 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11673 | ` * Refer to the official for more information on this powerful` |
|        - | 11674 | ` * extension.` |
|        - | 11675 | ` */` |
|  3043610 | 11676 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 11677 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 11678 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 11679 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 11680 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 11681 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 11682 | `	)` |
|        2 | 11683 |  |
|  3043612 | 11684 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11685 | `	VmRefObj *pRef;` |
|        - | 11686 | `	/* Check if the referenced object already exists */` |
|  3043612 | 11687 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3043612 | 11688 | `	if( pRef == 0 ){` |
|        - | 11689 | `		/* Create a new entry */` |
|  3012228 | 11690 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3012228 | 11691 | `		if( pRef == 0 ){` |
|      ! 0 | 11692 | `			return SXERR_MEM;` |
|        - | 11693 | `		}` |
|  3012228 | 11694 | `		pRef->iFlags = iFlags;` |
|        - | 11695 | `		/* Install the entry */` |
|  3012228 | 11696 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1506113 | 11697 | `	}` |
|  3043612 | 11698 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3043612 | 11699 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 11700 | `		VmSlot sRef;` |
|        - | 11701 | `		/* Local frame,record referenced entry so that it can` |
|        - | 11702 | `		 * be deleted when we leave this frame.` |
|        - | 11703 | `		 */` |
|    78198 | 11704 | `		sRef.nIdx = nIdx;` |
|    78198 | 11705 | `		sRef.pUserData = pEntry;` |
|    78198 | 11706 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 11707 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 11708 | `		}` |
|    39098 | 11709 | `	}` |
|  3043612 | 11710 | `	if( pEntry ){` |
|        - | 11711 | `		/* Address of the hash-entry */` |
|   109390 | 11712 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    54694 | 11713 | `	}` |
|  3043612 | 11714 | `	if( pMapEntry ){` |
|        - | 11715 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2929268 | 11716 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1464633 | 11717 | `	}` |
|  3043612 | 11718 | `	return SXRET_OK;` |
|  1521807 | 11719 |  |
|        - | 11720 | `/*` |
|        - | 11721 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 11722 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11723 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11724 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11725 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11726 | ` * Refer to the official for more information on this powerful` |
|        - | 11727 | ` * extension.` |
|        - | 11728 | ` */` |
|  2967554 | 11729 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 11730 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 11731 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 11732 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 11733 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 11734 | `	)` |
|        2 | 11735 |  |
|        - | 11736 | `	VmRefObj *pRef;` |
|        - | 11737 | `	sxu32 n;` |
|        - | 11738 | `	/* Check if the referenced object already exists */` |
|  2967556 | 11739 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2967556 | 11740 | `	if( pRef == 0 ){` |
|        - | 11741 | `		/* Not such entry */` |
|    78124 | 11742 | `		return SXERR_NOTFOUND;` |
|        - | 11743 | `	}` |
|        - | 11744 | `	/* Remove the desired entry */` |
|  2889434 | 11745 | `	if( pEntry ){` |
|        - | 11746 | `		SyHashEntry **apEntry;` |
|       56 | 11747 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 11748 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 11749 | `			if( apEntry[n] == pEntry ){` |
|        - | 11750 | `				/* Nullify the entry */` |
|       56 | 11751 | `				apEntry[n] = 0;` |
|        - | 11752 | `				/*` |
|        - | 11753 | `				 * NOTE:` |
|        - | 11754 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 11755 | `				 * we avoid wasting spaces.` |
|        - | 11756 | `				 */` |
|       27 | 11757 | `			}` |
|       79 | 11758 | `		}` |
|       27 | 11759 | `	}` |
|  2889434 | 11760 | `	if( pMapEntry ){` |
|        - | 11761 | `		ph7_hashmap_node **apNode;` |
|  2889380 | 11762 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5778852 | 11763 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2889474 | 11764 | `			if( apNode[n] == pMapEntry ){` |
|        - | 11765 | `				/* nullify the entry */` |
|  2889380 | 11766 | `				apNode[n] = 0;` |
|  1444689 | 11767 | `			}` |
|  1444738 | 11768 | `		}` |
|  1444689 | 11769 | `	}` |
|  2889434 | 11770 | `	return SXRET_OK;` |
|  1483779 | 11771 |  |
|        - | 11772 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 11773 | `/*` |
|        - | 11774 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 11775 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 11776 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 11777 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 11778 | ` * For more information on how to register IO stream devices,please` |
|        - | 11779 | ` * refer to the official documentation.` |
|        - | 11780 | ` */` |
|    23548 | 11781 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 11782 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 11783 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 11784 | `	int nByte              /* *pzDevice length*/` |
|        - | 11785 | `	)` |
|        2 | 11786 |  |
|        - | 11787 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 11788 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 11789 | `	SyString sDev,sCur;` |
|        - | 11790 | `	sxu32 n,nEntry;` |
|        - | 11791 | `	int rc;` |
|        - | 11792 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23550 | 11793 | `	zNext = zCur = zIn = *pzDevice;` |
|    23550 | 11794 | `	zEnd = &zIn[nByte];` |
|  1503696 | 11795 | `	while( zIn < zEnd ){` |
|  1480150 | 11796 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 11797 | `			/* Got one */` |
|        3 | 11798 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 11799 | `			break;` |
|        - | 11800 | `		}` |
|        - | 11801 | `		/* Advance the cursor */` |
|  1480148 | 11802 | `		zIn++;` |
|        2 | 11803 | `	}` |
|    23550 | 11804 | `	if( zIn >= zEnd ){` |
|        - | 11805 | `		/* No such scheme,return the default stream */` |
|    23548 | 11806 | `		return pVm->pDefStream;` |
|        - | 11807 | `	}` |
|        3 | 11808 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 11809 | `	/* Remove leading and trailing white spaces */` |
|        3 | 11810 | `	SyStringFullTrim(&sDev);` |
|        - | 11811 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 11812 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 11813 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 11814 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 11815 | `		pStream = apStream[n];` |
|        3 | 11816 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 11817 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 11818 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 11819 | `		if( rc == 0 ){` |
|        - | 11820 | `			/* Stream device found */` |
|        3 | 11821 | `			*pzDevice = zNext;` |
|        3 | 11822 | `			return pStream;` |
|        - | 11823 | `		}` |
|      ! 0 | 11824 | `	}` |
|        - | 11825 | `	/* No such stream,return NULL */` |
|      ! 0 | 11826 | `	return 0;` |
|    11776 | 11827 |  |
|        - | 11828 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 11829 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 11830 |  |
