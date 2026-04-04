# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4711/6287 lines (74.93%)

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
|   781280 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   781282 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   781252 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   781244 |    94 | `	return FALSE;` |
|   390664 |    95 |  |
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
|   550330 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   550332 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   550332 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   550328 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   550328 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   550328 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   550328 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   550328 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   550328 |   142 | `	pCons->xExpand = xExpand;` |
|   550328 |   143 | `	pCons->pUserData = pUserData;` |
|   550328 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   550328 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   550328 |   151 | `	return SXRET_OK;` |
|   275167 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1194324 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1194326 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1194326 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1194326 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1194326 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1194326 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1194326 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1194326 |   185 | `	pFunc->pVm   = pVm;` |
|  1194326 |   186 | `	pFunc->xFunc = xFunc;` |
|  1194326 |   187 | `	pFunc->pUserData = pUserData;` |
|  1194326 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1194326 |   190 | `	*ppOut = pFunc;` |
|  1194326 |   191 | `	return SXRET_OK;` |
|   597164 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1196860 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1196862 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1196862 |   213 | `	if( pEntry ){` |
|     2538 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2538 |   215 | `		pFunc->pUserData = pUserData;` |
|     2538 |   216 | `		pFunc->xFunc = xFunc;` |
|     2538 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2538 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1194326 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1194326 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1194326 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1194326 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1194326 |   233 | `	return SXRET_OK;` |
|   598432 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   171574 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   171576 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   171576 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   171576 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   171576 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   171576 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   171576 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   171576 |   260 | `	pFunc->iFlags = iFlags;` |
|   171576 |   261 | `	pFunc->pUserData = pUserData;` |
|   171576 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   171576 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   581214 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   581216 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    36922 |   283 | `		pName = &pFunc->sName;` |
|    18460 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   581216 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   581216 |   287 | `	if( pEntry ){` |
|   432274 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   432274 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   432274 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|   148944 |   297 | `	pFunc->pNextName = 0;` |
|   148944 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   148944 |   299 | `	return rc;` |
|   290609 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    42408 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    42410 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    42410 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    42410 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    42380 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    42380 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    42380 |   324 | `	return rc;` |
|    21206 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  3444630 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3444632 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  3444632 |   342 | `	sInstr.iP1 = iP1;` |
|  3444632 |   343 | `	sInstr.iP2 = iP2;` |
|  3444632 |   344 | `	sInstr.p3  = p3;` |
|  3444632 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   199070 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    99534 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  3444632 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3444632 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  3444632 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   410832 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   410834 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   410834 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   410834 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   205416 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   205418 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   196196 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   196198 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   196198 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|  1036008 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|  1036010 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   186638 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   186640 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   669222 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   669224 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    28672 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    28674 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    28674 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    28674 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    28674 |   417 | `	return &aInstr[n - 2];` |
|    14338 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    16222 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    16224 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16224 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    16224 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    16224 |   437 | `	pFrame->pUserData = pUserData;` |
|    16224 |   438 | `	pFrame->pThis = pThis;` |
|    16224 |   439 | `	pFrame->pVm = pVm;` |
|    16224 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16224 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16224 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16224 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16224 |   444 | `	return pFrame;` |
|     8113 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    16180 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    16182 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16182 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    16182 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    16182 |   466 | `	pVm->pFrame = pFrame;` |
|    16182 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    13386 |   469 | `		*ppFrame = pFrame;` |
|     6692 |   470 | `	}` |
|    16182 |   471 | `	return SXRET_OK;` |
|     8092 |   472 |  |
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
|    13384 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    13386 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13386 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    13386 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13386 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13322 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    92746 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    79426 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39714 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    13322 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    92802 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    79482 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39742 |   535 | `			}` |
|     6660 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    13386 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13386 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    13386 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13386 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    13386 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6692 |   544 | `	}` |
|    13386 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6313098 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6313376 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6313100 |   556 | `	return pFrame;` |
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
|   112760 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|   112762 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   463918 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   294778 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   294778 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   112762 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    48444 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    64320 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|     5626 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5626 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|     2812 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    64320 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   640781 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   544304 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   544304 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   544296 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   544296 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   272147 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    64320 |   738 | `	pClass->bMounted = TRUE;` |
|    64320 |   739 | `	return SXRET_OK;` |
|    56382 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1190 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1192 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1192 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4880 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3690 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3690 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3690 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3690 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3690 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3684 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3684 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3684 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3684 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1188 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      593 |   777 | `			}` |
|     3684 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3684 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3684 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1843 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1192 |   800 | `	return SXRET_OK;` |
|      597 |   801 |  |
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
|   393144 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   393146 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   384758 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   192378 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   393146 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   393146 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   393146 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   393146 |   830 | `	return pObj;` |
|   196574 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2145286 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2145288 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2145288 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1072643 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2145288 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2145288 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2145288 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2145288 |   853 | `	return pObj;` |
|  1072645 |   854 |  |
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
|        - |   868 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   869 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   870 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   871 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   872 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   873 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   874 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   875 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   876 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   879 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   880 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   881 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   882 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   883 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   884 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   885 | `/*` |
|        - |   886 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   887 | ` * directly as foreign functions.` |
|        - |   888 | ` */` |
|        - |   889 | `#define PH7_BUILTIN_LIB \` |
|        - |   890 | `	"class Exception { "\` |
|        - |   891 | `    "protected $message = 'Unknown exception';"\` |
|        - |   892 | `    "protected $code = 0;"\` |
|        - |   893 | `    "protected $file;"\` |
|        - |   894 | `    "protected $line;"\` |
|        - |   895 | `    "protected $trace;"\` |
|        - |   896 | `    "protected $previous;"\` |
|        - |   897 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   898 | `	"   if( isset($message) ){"\` |
|        - |   899 | `	"	  $this->message = $message;"\` |
|        - |   900 | `	"   }"\` |
|        - |   901 | `	"   $this->code = $code;"\` |
|        - |   902 | `	"   $this->file = __FILE__;"\` |
|        - |   903 | `	"   $this->line = __LINE__;"\` |
|        - |   904 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   905 | `	"   if( isset($previous) ){"\` |
|        - |   906 | `	"     $this->previous = $previous;"\` |
|        - |   907 | `	"   }"\` |
|        - |   908 | `	"}"\` |
|        - |   909 | `	"public function getMessage(){"\` |
|        - |   910 | `	"   return $this->message;"\` |
|        - |   911 | `	"}"\` |
|        - |   912 | `	" public function getCode(){"\` |
|        - |   913 | `	"  return $this->code;"\` |
|        - |   914 | `	"}"\` |
|        - |   915 | `	"public function getFile(){"\` |
|        - |   916 | `	"  return $this->file;"\` |
|        - |   917 | `	"}"\` |
|        - |   918 | `	"public function getLine(){"\` |
|        - |   919 | `	"  return $this->line;"\` |
|        - |   920 | `	"}"\` |
|        - |   921 | `	"public function getTrace(){"\` |
|        - |   922 | `	"   return $this->trace;"\` |
|        - |   923 | `	"}"\` |
|        - |   924 | `	"public function getTraceAsString(){"\` |
|        - |   925 | `	"  return debug_string_backtrace();"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getPrevious(){"\` |
|        - |   928 | `	"    return $this->previous;"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"public function __toString(){"\` |
|        - |   931 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   932 | `    "}"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"class Error extends Exception { }"\` |
|        - |   935 | `	"class TypeError extends Error { }"\` |
|        - |   936 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   937 | `	"class ValueError extends Error { }"\` |
|        - |   938 | `	"class FiberError extends Error { }"\` |
|        - |   939 | `	"class AssertionError extends Error { }"\` |
|        - |   940 | `	"class ErrorException extends Exception { "\` |
|        - |   941 | `	"protected $severity;"\` |
|        - |   942 | `	"public function __construct(string $message = null,"\` |
|        - |   943 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   944 | `	"   if( isset($message) ){"\` |
|        - |   945 | `	"	  $this->message = $message;"\` |
|        - |   946 | `	"   }"\` |
|        - |   947 | `	"   $this->severity = $severity;"\` |
|        - |   948 | `	"   $this->code = $code;"\` |
|        - |   949 | `	"   $this->file = $filename;"\` |
|        - |   950 | `	"   $this->line = $lineno;"\` |
|        - |   951 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   952 | `	"   if( isset($previous) ){"\` |
|        - |   953 | `	"     $this->previous = $previous;"\` |
|        - |   954 | `	"   }"\` |
|        - |   955 | `	"}"\` |
|        - |   956 | `	"public function getSeverity(){"\` |
|        - |   957 | `	"   return $this->severity;"\` |
|        - |   958 | `    "}"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"interface Iterator {"\` |
|        - |   961 | `	"public function current();"\` |
|        - |   962 | `	"public function key();"\` |
|        - |   963 | `	"public function next();"\` |
|        - |   964 | `	"public function rewind();"\` |
|        - |   965 | `	"public function valid();"\` |
|        - |   966 | `	"}"\` |
|        - |   967 | `	"interface IteratorAggregate {"\` |
|        - |   968 | `	"public function getIterator();"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"interface Serializable {"\` |
|        - |   971 | `	"public function serialize();"\` |
|        - |   972 | `	"public function unserialize(string $serialized);"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"/* Directory releated IO */"\` |
|        - |   975 | `	"class Directory {"\` |
|        - |   976 | `	"public $handle = null;"\` |
|        - |   977 | `	"public $path  = null;"\` |
|        - |   978 | `	"public function __construct(string $path)"\` |
|        - |   979 | `	"{"\` |
|        - |   980 | `	"   $this->handle = opendir($path);"\` |
|        - |   981 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   982 | `	"      $this->path = $path;"\` |
|        - |   983 | `	"   }"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"public function __destruct()"\` |
|        - |   986 | `	"{"\` |
|        - |   987 | `	"  if( $this->handle != null ){"\` |
|        - |   988 | `	"       closedir($this->handle);"\` |
|        - |   989 | `	"  }"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"public function read()"\` |
|        - |   992 | `	"{"\` |
|        - |   993 | `	"    return readdir($this->handle);"\` |
|        - |   994 | `	"}"\` |
|        - |   995 | `	"public function rewind()"\` |
|        - |   996 | `	"{"\` |
|        - |   997 | `	"    rewinddir($this->handle);"\` |
|        - |   998 | `	"}"\` |
|        - |   999 | `	"public function close()"\` |
|        - |  1000 | `	"{"\` |
|        - |  1001 | `	"    closedir($this->handle);"\` |
|        - |  1002 | `	"    $this->handle = null;"\` |
|        - |  1003 | `	"}"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"class Fiber {"\` |
|        - |  1006 | `	"  private $__ctx;"\` |
|        - |  1007 | `	"  private $__callable;"\` |
|        - |  1008 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1009 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1010 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1011 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1012 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1013 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1014 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1015 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1016 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1017 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"class Generator implements Iterator {"\` |
|        - |  1020 | `	"  private $__ctx;"\` |
|        - |  1021 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1022 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1023 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1024 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1025 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1026 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1027 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1028 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1029 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"class stdClass{"\` |
|        - |  1032 | `	"  public $value;"\` |
|        - |  1033 | `	" /* Magic methods */"\` |
|        - |  1034 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1035 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1036 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1037 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1038 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1039 | `	"}"\` |
|        - |  1040 | `	"function dir(string $path){"\` |
|        - |  1041 | `	"   return new Directory($path);"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"function Dir(string $path){"\` |
|        - |  1044 | `	"   return new Directory($path);"\` |
|        - |  1045 | `	"}"\` |
|        - |  1046 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1047 | `    "{"\` |
|        - |  1048 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1049 | `	"  $aDir = array();"\` |
|        - |  1050 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1051 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1052 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1053 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1054 | `	"   }"\` |
|        - |  1055 | `	"  closedir($pHandle);"\` |
|        - |  1056 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1057 | `	"      rsort($aDir);"\` |
|        - |  1058 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1059 | `	"      sort($aDir);"\` |
|        - |  1060 | `	"  }"\` |
|        - |  1061 | `	"  return $aDir;"\` |
|        - |  1062 | `	"}"\` |
|        - |  1063 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1064 | `	"/* Open the target directory */"\` |
|        - |  1065 | `	"$zDir = dirname($pattern);"\` |
|        - |  1066 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1067 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1068 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1069 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1070 | `	"	return FALSE;"\` |
|        - |  1071 | `	"}"\` |
|        - |  1072 | `	"$pattern = basename($pattern);"\` |
|        - |  1073 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1074 | `	"/* Loop throw available entries */"\` |
|        - |  1075 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1076 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1077 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1078 | `	"	if( $rc ){"\` |
|        - |  1079 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1080 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1081 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1082 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1083 | `	"		  }"\` |
|        - |  1084 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1085 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1086 | `	"		 continue;"\` |
|        - |  1087 | `	"	   }"\` |
|        - |  1088 | `	"	   /* Add the entry */"\` |
|        - |  1089 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1090 | `	"	}"\` |
|        - |  1091 | `	" }"\` |
|        - |  1092 | `	"/* Close the handle */"\` |
|        - |  1093 | `	"closedir($pHandle);"\` |
|        - |  1094 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1095 | `	"  /* Sort the array */"\` |
|        - |  1096 | `	"  sort($pArray);"\` |
|        - |  1097 | `	"}"\` |
|        - |  1098 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1099 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1100 | `	"  $pArray[] = $pattern;"\` |
|        - |  1101 | `	"}"\` |
|        - |  1102 | `	"/* Return the created array */"\` |
|        - |  1103 | `	"return $pArray;"\` |
|        - |  1104 | `   "}"\` |
|        - |  1105 | `   "/* Creates a temporary file */"\` |
|        - |  1106 | `   "function tmpfile(){"\` |
|        - |  1107 | `   "  /* Extract the temp directory */"\` |
|        - |  1108 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1109 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1110 | `   "    /* Use the current dir */"\` |
|        - |  1111 | `   "    $zTempDir = '.';"\` |
|        - |  1112 | `   "  }"\` |
|        - |  1113 | `   "  /* Create the file */"\` |
|        - |  1114 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1115 | `   "  return $pHandle;"\` |
|        - |  1116 | `   "}"\` |
|        - |  1117 | `   "/* Creates a temporary filename */"\` |
|        - |  1118 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1119 | `   "{"\` |
|        - |  1120 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1121 | `   "}"\` |
|        - |  1122 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1123 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1124 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1125 | `   "/* Copy arguments */"\` |
|        - |  1126 | `   "$nArgs = func_num_args();"\` |
|        - |  1127 | `   "$pNew = array();"\` |
|        - |  1128 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1129 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1130 | `    "}"\` |
|        - |  1131 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1132 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1133 | `	"/* Erase */"\` |
|        - |  1134 | `	"array_erase($pArray);"\` |
|        - |  1135 | `	"/* Unshift */"\` |
|        - |  1136 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1137 | `	"return sizeof($pArray);"\` |
|        - |  1138 | `    "}"\` |
|        - |  1139 | `	"function array_merge_recursive(){"\` |
|        - |  1140 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1141 | `    "$arrays = func_get_args();"\` |
|        - |  1142 | `    "$narrays = count($arrays);"\` |
|        - |  1143 | `    "$ret = array();"\` |
|        - |  1144 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1145 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1146 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1147 | `	 " }"\` |
|        - |  1148 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1149 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1150 | `     "  if( $keyIsInt ) {"\` |
|        - |  1151 | `     "   $ret[] = $value;"\` |
|        - |  1152 | `     "  } else {"\` |
|        - |  1153 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1154 | `     "    $cur = $ret[$key];"\` |
|        - |  1155 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1156 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1157 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1158 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1159 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1160 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1161 | `     "    } else {"\` |
|        - |  1162 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1163 | `     "    }"\` |
|        - |  1164 | `     "   } else {"\` |
|        - |  1165 | `     "    $ret[$key] = $value;"\` |
|        - |  1166 | `     "   }"\` |
|        - |  1167 | `     "  }"\` |
|        - |  1168 | `     " }"\` |
|        - |  1169 | `	 " }"\` |
|        - |  1170 | `	 " return $ret;"\` |
|        - |  1171 | `    "}"\` |
|        - |  1172 | `	"function max(){"\` |
|        - |  1173 | `    "  $pArgs = func_get_args();"\` |
|        - |  1174 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1175 | `	"  return null;"\` |
|        - |  1176 | `    " }"\` |
|        - |  1177 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1178 | `    " $pArg = $pArgs[0];"\` |
|        - |  1179 | `	" if( !is_array($pArg) ){"\` |
|        - |  1180 | `	"   return $pArg; "\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1183 | `	"   return null;"\` |
|        - |  1184 | `	" }"\` |
|        - |  1185 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1186 | `	" reset($pArg);"\` |
|        - |  1187 | `	" $max = current($pArg);"\` |
|        - |  1188 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1189 | `	"   if( $val > $max ){"\` |
|        - |  1190 | `	"     $max = $val;"\` |
|        - |  1191 | `    " }"\` |
|        - |  1192 | `	" }"\` |
|        - |  1193 | `	" return $max;"\` |
|        - |  1194 | `    " }"\` |
|        - |  1195 | `    " $max = $pArgs[0];"\` |
|        - |  1196 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1197 | `    " $val = $pArgs[$i];"\` |
|        - |  1198 | `	"if( $val > $max ){"\` |
|        - |  1199 | `	" $max = $val;"\` |
|        - |  1200 | `	"}"\` |
|        - |  1201 | `    " }"\` |
|        - |  1202 | `	" return $max;"\` |
|        - |  1203 | `    "}"\` |
|        - |  1204 | `	"function min(){"\` |
|        - |  1205 | `    "  $pArgs = func_get_args();"\` |
|        - |  1206 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1207 | `	"  return null;"\` |
|        - |  1208 | `    " }"\` |
|        - |  1209 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1210 | `    " $pArg = $pArgs[0];"\` |
|        - |  1211 | `	" if( !is_array($pArg) ){"\` |
|        - |  1212 | `	"   return $pArg; "\` |
|        - |  1213 | `	" }"\` |
|        - |  1214 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1215 | `	"   return null;"\` |
|        - |  1216 | `	" }"\` |
|        - |  1217 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1218 | `	" reset($pArg);"\` |
|        - |  1219 | `	" $min = current($pArg);"\` |
|        - |  1220 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1221 | `	"   if( $val < $min ){"\` |
|        - |  1222 | `	"     $min = $val;"\` |
|        - |  1223 | `    " }"\` |
|        - |  1224 | `	" }"\` |
|        - |  1225 | `	" return $min;"\` |
|        - |  1226 | `    " }"\` |
|        - |  1227 | `    " $min = $pArgs[0];"\` |
|        - |  1228 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1229 | `    " $val = $pArgs[$i];"\` |
|        - |  1230 | `	"if( $val < $min ){"\` |
|        - |  1231 | `	" $min = $val;"\` |
|        - |  1232 | `	" }"\` |
|        - |  1233 | `    " }"\` |
|        - |  1234 | `	" return $min;"\` |
|        - |  1235 | `	"}"\` |
|        - |  1236 | `	"function fileowner(string $file){"\` |
|        - |  1237 | `    " $a = stat($file);"\` |
|        - |  1238 | `	" if( !is_array($a) ){"\` |
|        - |  1239 | `	"	return false;"\` |
|        - |  1240 | `	" }"\` |
|        - |  1241 | `	" return $a['uid'];"\` |
|        - |  1242 | `    "}"\` |
|        - |  1243 | `    "function filegroup(string $file){"\` |
|        - |  1244 | `	" $a = stat($file);"\` |
|        - |  1245 | `	" if( !is_array($a) ){"\` |
|        - |  1246 | `	"	return false;"\` |
|        - |  1247 | `	" }"\` |
|        - |  1248 | `	" return $a['gid'];"\` |
|        - |  1249 | `    "}"\` |
|        - |  1250 | `	 "function fileinode(string $file){"\` |
|        - |  1251 | `	" $a = stat($file);"\` |
|        - |  1252 | `	" if( !is_array($a) ){"\` |
|        - |  1253 | `	"	return false;"\` |
|        - |  1254 | `	" }"\` |
|        - |  1255 | `	" return $a['ino'];"\` |
|        - |  1256 | `    "}"` |
|        - |  1257 |  |
|        - |  1258 | `/*` |
|        - |  1259 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1260 | ` * start compiling the target PHP program.` |
|        - |  1261 | ` */` |
|     2796 |  1262 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1263 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1264 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1265 | `	 )` |
|        2 |  1266 |  |
|        - |  1267 | `	SyString sBuiltin;` |
|        - |  1268 | `	ph7_value *pObj;` |
|        - |  1269 | `	sxi32 rc;` |
|        - |  1270 | `	/* Zero the structure */` |
|     2798 |  1271 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1272 | `	/* Initialize VM fields */` |
|     2798 |  1273 | `	pVm->pEngine = &(*pEngine);` |
|     2798 |  1274 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1275 | `	/* Instructions containers */` |
|     2798 |  1276 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2798 |  1277 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2798 |  1278 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1279 | `	/* Object containers */` |
|     2798 |  1280 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2798 |  1281 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1282 | `	/* Virtual machine internal containers */` |
|     2798 |  1283 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2798 |  1284 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2798 |  1285 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2798 |  1286 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2798 |  1287 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2798 |  1288 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2798 |  1289 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2798 |  1290 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2798 |  1291 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2798 |  1292 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2798 |  1293 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2798 |  1294 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2798 |  1295 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2798 |  1296 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2798 |  1297 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2798 |  1298 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2798 |  1299 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2798 |  1300 | `	pVm->pPendingException = 0;` |
|        - |  1301 | `	/* Configuration containers */` |
|     2798 |  1302 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2798 |  1303 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2798 |  1304 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2798 |  1305 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2798 |  1306 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2798 |  1307 | `	pVm->iResponseStatus = 200;` |
|     2798 |  1308 | `	pVm->bHeadersSent = 0;` |
|     2798 |  1309 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1310 | `	/* Error callbacks containers */` |
|     2798 |  1311 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2798 |  1312 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2798 |  1313 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2798 |  1314 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2798 |  1315 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1316 | `	/* Set a default recursion limit */` |
|        - |  1317 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2798 |  1318 | `	pVm->nMaxDepth = 32;` |
|        - |  1319 | `#else` |
|        - |  1320 | `	pVm->nMaxDepth = 16;` |
|        - |  1321 | `#endif` |
|        - |  1322 | `	/* Default assertion flags */` |
|     2798 |  1323 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1324 | `	/* JSON return status */` |
|     2798 |  1325 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1326 | `	/* PRNG context */` |
|     2798 |  1327 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1328 | `	/* Install the null constant */` |
|     2798 |  1329 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2798 |  1330 | `	if( pObj == 0 ){` |
|      ! 0 |  1331 | `		rc = SXERR_MEM;` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|     2798 |  1334 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1335 | `	/* Install the boolean TRUE constant */` |
|     2798 |  1336 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2798 |  1337 | `	if( pObj == 0 ){` |
|      ! 0 |  1338 | `		rc = SXERR_MEM;` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|     2798 |  1341 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1342 | `	/* Install the boolean FALSE constant */` |
|     2798 |  1343 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2798 |  1344 | `	if( pObj == 0 ){` |
|      ! 0 |  1345 | `		rc = SXERR_MEM;` |
|      ! 0 |  1346 | `		goto Err;` |
|        - |  1347 | `	}` |
|     2798 |  1348 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1349 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1350 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1351 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2798 |  1352 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2798 |  1353 | `	if( pObj == 0 ){` |
|      ! 0 |  1354 | `		rc = SXERR_MEM;` |
|      ! 0 |  1355 | `		goto Err;` |
|        - |  1356 | `	}` |
|     2798 |  1357 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1358 | `	/* Create the global frame */` |
|     2798 |  1359 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2798 |  1360 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|        - |  1363 | `	/* Initialize the code generator */` |
|     2798 |  1364 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2798 |  1365 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1366 | `		goto Err;` |
|        - |  1367 | `	}` |
|        - |  1368 | `	/* VM correctly initialized,set the magic number */` |
|     2798 |  1369 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2798 |  1370 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1371 | `	/* Compile the built-in library */` |
|     2798 |  1372 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1373 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2798 |  1374 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1375 | `	/* Register Fiber internal C functions */` |
|     2798 |  1376 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2798 |  1377 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2798 |  1378 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2798 |  1379 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2798 |  1380 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2798 |  1381 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2798 |  1382 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2798 |  1383 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2798 |  1384 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2798 |  1385 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1386 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2798 |  1387 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2798 |  1388 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2798 |  1389 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2798 |  1390 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2798 |  1391 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2798 |  1392 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2798 |  1393 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2798 |  1394 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2798 |  1395 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2798 |  1396 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1397 | `	/* Reset the code generator */` |
|     2798 |  1398 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2798 |  1399 | `	return SXRET_OK;` |
|      ! 0 |  1400 | `Err:` |
|      ! 0 |  1401 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1402 | `	return rc;` |
|     1400 |  1403 |  |
|        - |  1404 | `/*` |
|        - |  1405 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1406 | ` * routine which store the output in an internal blob.` |
|        - |  1407 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1408 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1409 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1410 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1411 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1412 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1413 | ` * to finish executing and extracting the output.` |
|        - |  1414 | ` */` |
|       38 |  1415 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1416 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1417 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1418 | `	void *pUserData     /* User private data */` |
|        - |  1419 | `	)` |
|      ! 0 |  1420 |  |
|        - |  1421 | `	 sxi32 rc;` |
|        - |  1422 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1423 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1424 | `	 return rc;` |
|      ! 0 |  1425 |  |
|        - |  1426 | `/*` |
|        - |  1427 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1428 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1429 | ` */` |
|    13526 |  1430 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1431 |  |
|    13528 |  1432 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13528 |  1433 | `	if( xCons != VmObConsumer ){` |
|     6616 |  1434 | `		pVm->nOutputLen += nLen;` |
|     6616 |  1435 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      926 |  1436 | `			pVm->bHeadersSent = 1;` |
|      462 |  1437 | `		}` |
|     3307 |  1438 | `	}` |
|    13528 |  1439 |  |
|        - |  1440 | `#define VM_STACK_GUARD 16` |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1443 | ` * our compiled PHP program.` |
|        - |  1444 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1445 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1446 | ` */` |
|    32812 |  1447 | `static ph7_value * VmNewOperandStack(` |
|        - |  1448 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1449 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1450 | `	)` |
|        2 |  1451 |  |
|        - |  1452 | `	ph7_value *pStack;` |
|        - |  1453 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1454 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1455 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1456 | `  ** on the maximum stack depth required.` |
|        - |  1457 | `  **` |
|        - |  1458 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1459 | `  */` |
|    32814 |  1460 | `	nInstr += VM_STACK_GUARD;` |
|    32814 |  1461 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32814 |  1462 | `	if( pStack == 0 ){` |
|      ! 0 |  1463 | `		return 0;` |
|        - |  1464 | `	}` |
|        - |  1465 | `	/* Initialize the operand stack */` |
|  2054488 |  1466 | `	while( nInstr > 0 ){` |
|  2021676 |  1467 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2021676 |  1468 | `		--nInstr;` |
|        2 |  1469 | `	}` |
|        - |  1470 | `	/* Ready for bytecode execution */` |
|    32814 |  1471 | `	return pStack;` |
|    16408 |  1472 |  |
|        - |  1473 | `/* Forward declaration */` |
|        - |  1474 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1475 | `/*` |
|        - |  1476 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1477 | ` * This routine gets called by the PH7 engine after` |
|        - |  1478 | ` * successful compilation of the target PHP program.` |
|        - |  1479 | ` */` |
|     2536 |  1480 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1481 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1482 | `	)` |
|        2 |  1483 |  |
|        - |  1484 | `	SyHashEntry *pEntry;` |
|        - |  1485 | `	sxi32 rc;` |
|     2538 |  1486 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1487 | `		/* Initialize your VM first */` |
|      ! 0 |  1488 | `		return SXERR_CORRUPT;` |
|        - |  1489 | `	}` |
|        - |  1490 | `	/* Mark the VM ready for byte-code execution */` |
|     2538 |  1491 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1492 | `	/* Release the code generator now we have compiled our program */` |
|     2538 |  1493 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1494 | `	/* Emit the DONE instruction */` |
|     2538 |  1495 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2538 |  1496 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1497 | `		return SXERR_MEM;` |
|        - |  1498 | `	}` |
|        - |  1499 | `	/* Script return value */` |
|     2538 |  1500 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1501 | `	/* Allocate a new operand stack */` |
|     2538 |  1502 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2538 |  1503 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1504 | `		return SXERR_MEM;` |
|        - |  1505 | `	}` |
|        - |  1506 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1507 | `	 * private data. */` |
|     2538 |  1508 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2538 |  1509 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1510 | `	/* Allocate the reference table */` |
|     2538 |  1511 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2538 |  1512 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2538 |  1513 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1514 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1515 | `		return SXERR_MEM;` |
|        - |  1516 | `	}` |
|        - |  1517 | `	/* Zero the reference table */` |
|     2538 |  1518 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1519 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2538 |  1520 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2538 |  1521 | `	if( rc != SXRET_OK ){` |
|        - |  1522 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1523 | `		return rc;` |
|        - |  1524 | `	}` |
|        - |  1525 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2538 |  1526 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2538 |  1527 | `	if( rc != SXRET_OK ){` |
|        - |  1528 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1529 | `		return rc;` |
|        - |  1530 | `	}` |
|        - |  1531 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2538 |  1532 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1533 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2538 |  1534 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1535 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2538 |  1536 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1537 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1538 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2538 |  1539 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2538 |  1540 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1541 | `#endif` |
|        - |  1542 | `	/* Initialize and install static and constants class attributes */` |
|     2538 |  1543 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    40752 |  1544 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    38216 |  1545 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    38216 |  1546 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1547 | `			return rc;` |
|        - |  1548 | `		}` |
|        2 |  1549 | `	}` |
|        - |  1550 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2538 |  1551 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1552 | `	/* VM is ready for bytecode execution */` |
|     2538 |  1553 | `	return SXRET_OK;` |
|     1270 |  1554 |  |
|        - |  1555 | `/*` |
|        - |  1556 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1557 | ` */` |
|      ! 0 |  1558 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1559 |  |
|      ! 0 |  1560 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1561 | `		return SXERR_CORRUPT;` |
|        - |  1562 | `	}` |
|        - |  1563 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1564 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1565 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1566 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1567 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1568 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1569 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1570 | `	pVm->bHttpContext = 0;` |
|        - |  1571 | `	/* Set the ready flag */` |
|      ! 0 |  1572 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1573 | `	return SXRET_OK;` |
|      ! 0 |  1574 |  |
|        - |  1575 | `/*` |
|        - |  1576 | ` * Release a Virtual Machine.` |
|        - |  1577 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1578 | ` */` |
|     2528 |  1579 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1580 |  |
|        - |  1581 | `	/* Set the stale magic number */` |
|     2530 |  1582 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1583 | `	/* Release the private memory subsystem */` |
|     2530 |  1584 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2530 |  1585 | `	return SXRET_OK;` |
|        2 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Initialize a foreign function call context.` |
|        - |  1589 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1590 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1591 | ` * functions.` |
|        - |  1592 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1593 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1594 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1595 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1596 | ` */` |
|   569650 |  1597 | `static sxi32 VmInitCallContext(` |
|        - |  1598 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1599 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1600 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1601 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1602 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1603 | `	)` |
|        2 |  1604 |  |
|   569652 |  1605 | `	pOut->pFunc = pFunc;` |
|   569652 |  1606 | `	pOut->pVm   = pVm;` |
|   569652 |  1607 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   569652 |  1608 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1609 | `	/* Assume a null return value */` |
|   569652 |  1610 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   569652 |  1611 | `	pOut->pRet = pRet;` |
|   569652 |  1612 | `	pOut->iFlags = iFlags;` |
|   569652 |  1613 | `	return SXRET_OK;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1617 | ` * left behind.` |
|        - |  1618 | ` */` |
|   569650 |  1619 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1620 |  |
|        - |  1621 | `	sxu32 n;` |
|   569652 |  1622 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6924 |  1623 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19762 |  1624 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12840 |  1625 | `			if( apObj[n] == 0 ){` |
|        - |  1626 | `				/* Already released */` |
|      298 |  1627 | `				continue;` |
|        - |  1628 | `			}` |
|    12544 |  1629 | `			PH7_MemObjRelease(apObj[n]);` |
|    12544 |  1630 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6273 |  1631 | `		}` |
|     6924 |  1632 | `		SySetRelease(&pCtx->sVar);` |
|     3461 |  1633 | `	}` |
|   569652 |  1634 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1635 | `		ph7_aux_data *aAux;` |
|        - |  1636 | `		void *pChunk;` |
|        - |  1637 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1638 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1639 | `		 */` |
|        9 |  1640 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1641 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1642 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1643 | `			/* Release the chunk */` |
|       25 |  1644 | `			if( pChunk ){` |
|       25 |  1645 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1646 | `			}` |
|       13 |  1647 | `		}` |
|        9 |  1648 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1649 | `	}` |
|   569652 |  1650 |  |
|        - |  1651 | `/*` |
|        - |  1652 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1653 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1654 | ` */` |
|      296 |  1655 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1656 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1657 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1658 | `	)` |
|        2 |  1659 |  |
|      298 |  1660 | `	if( pValue == 0 ){` |
|        - |  1661 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1662 | `		return;` |
|        - |  1663 | `	}` |
|      298 |  1664 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1665 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1666 | `		sxu32 n;` |
|     1054 |  1667 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1668 | `			if( apObj[n] == pValue ){` |
|      298 |  1669 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1670 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1671 | `				/* Mark as released */` |
|      298 |  1672 | `				apObj[n] = 0;` |
|      298 |  1673 | `				break;` |
|        - |  1674 | `			}` |
|      380 |  1675 | `		}` |
|      148 |  1676 | `	}` |
|      150 |  1677 |  |
|        - |  1678 | `/*` |
|        - |  1679 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1680 | ` */` |
|  3316882 |  1681 | `static void VmPopOperand(` |
|        - |  1682 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1683 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1684 | `	)` |
|        2 |  1685 |  |
|  3316884 |  1686 | `	ph7_value *pTos = *ppTos;` |
|  7047046 |  1687 | `	while( nPop > 0 ){` |
|  3730164 |  1688 | `		PH7_MemObjRelease(pTos);` |
|  3730164 |  1689 | `		pTos--;` |
|  3730164 |  1690 | `		nPop--;` |
|        2 |  1691 | `	}` |
|        - |  1692 | `	/* Top of the stack */` |
|  3316884 |  1693 | `	*ppTos = pTos;` |
|  3316884 |  1694 |  |
|        - |  1695 | `/*` |
|        - |  1696 | ` * Reserve a memory object.` |
|        - |  1697 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1698 | ` */` |
|  3023964 |  1699 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1700 |  |
|  3023966 |  1701 | `	ph7_value *pObj = 0;` |
|        - |  1702 | `	VmSlot *pSlot;` |
|        - |  1703 | `	sxu32 nIdx;` |
|        - |  1704 | `	/* Check for a free slot */` |
|  3023966 |  1705 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3023966 |  1706 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3023966 |  1707 | `	if( pSlot ){` |
|   878680 |  1708 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   878680 |  1709 | `		nIdx = pSlot->nIdx;` |
|   439339 |  1710 | `	}` |
|  3023966 |  1711 | `	if( pObj == 0 ){` |
|        - |  1712 | `		/* Reserve a new memory object */` |
|  2145288 |  1713 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2145288 |  1714 | `		if( pObj == 0 ){` |
|      ! 0 |  1715 | `			return 0;` |
|        - |  1716 | `		}` |
|  1072643 |  1717 | `	}` |
|        - |  1718 | `	/* Set a null default value */` |
|  3023966 |  1719 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3023966 |  1720 | `	pObj->nIdx = nIdx;` |
|  3023966 |  1721 | `	return pObj;` |
|  1511984 |  1722 |  |
|        - |  1723 | `/*` |
|        - |  1724 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1725 | ` */` |
|    32286 |  1726 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1727 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1728 | `	const char *zKey,  /* Entry key */` |
|        - |  1729 | `	sxu32 nByte,       /* Key length */` |
|        - |  1730 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1731 | `	)` |
|        2 |  1732 |  |
|        - |  1733 | `	ph7_value sKey;` |
|        - |  1734 | `	sxi32 rc;` |
|    32288 |  1735 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32288 |  1736 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1737 | `	/* Perform the insertion */` |
|    32288 |  1738 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32288 |  1739 | `	PH7_MemObjRelease(&sKey);` |
|    32288 |  1740 | `	return rc;` |
|        2 |  1741 |  |
|        - |  1742 | `/*` |
|        - |  1743 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1744 | ` * Return a pointer to the variable value on success.` |
|        - |  1745 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1746 | ` */` |
|  3095516 |  1747 | `static ph7_value * VmExtractMemObj(` |
|        - |  1748 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1749 | `	const SyString *pName, /* Variable name */` |
|        - |  1750 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1751 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1752 | `	)` |
|        2 |  1753 |  |
|  3095518 |  1754 | `	int bNullify = FALSE;` |
|        - |  1755 | `	SyHashEntry *pEntry;` |
|        - |  1756 | `	VmFrame *pFrame;` |
|        - |  1757 | `	ph7_value *pObj;` |
|        - |  1758 | `	sxu32 nIdx;` |
|        - |  1759 | `	sxi32 rc;` |
|        - |  1760 | `	/* Point to the top active frame */` |
|  3095518 |  1761 | `	pFrame = pVm->pFrame;` |
|  3095518 |  1762 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1763 | `	/* Perform the lookup */` |
|  3095518 |  1764 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1765 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1766 | `		pName = &sAnnon;` |
|        - |  1767 | `		/* Always nullify the object */` |
|      ! 0 |  1768 | `		bNullify = TRUE;` |
|      ! 0 |  1769 | `		bDup = FALSE;` |
|      ! 0 |  1770 | `	}` |
|        - |  1771 | `	/* Check the superglobals table first */` |
|  3095518 |  1772 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3095518 |  1773 | `	if( pEntry == 0 ){` |
|        - |  1774 | `		/* Query the top active frame */` |
|  3095478 |  1775 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3095478 |  1776 | `		if( pEntry == 0 ){` |
|    86398 |  1777 | `			char *zName = (char *)pName->zString;` |
|        - |  1778 | `			VmSlot sLocal;` |
|    86398 |  1779 | `			if( !bCreate ){` |
|        - |  1780 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1781 | `				return 0;` |
|        - |  1782 | `			}` |
|        - |  1783 | `			/* No such variable,automatically create a new one and install` |
|        - |  1784 | `			 * it in the current frame.` |
|        - |  1785 | `			 */` |
|    86362 |  1786 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86362 |  1787 | `			if( pObj == 0 ){` |
|      ! 0 |  1788 | `				return 0;` |
|        - |  1789 | `			}` |
|    86362 |  1790 | `			nIdx = pObj->nIdx;` |
|    86362 |  1791 | `			if( bDup ){` |
|        - |  1792 | `				/* Duplicate name */` |
|      168 |  1793 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1794 | `				if( zName == 0 ){` |
|      ! 0 |  1795 | `					return 0;` |
|        - |  1796 | `				}` |
|       83 |  1797 | `			}` |
|        - |  1798 | `			/* Link to the top active VM frame */` |
|    86362 |  1799 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86362 |  1800 | `			if( rc != SXRET_OK ){` |
|        - |  1801 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1802 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1803 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1804 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1805 | `				return 0;` |
|        - |  1806 | `			}` |
|    86362 |  1807 | `			if( pFrame->pParent != 0 ){` |
|        - |  1808 | `				/* Local variable */` |
|    79462 |  1809 | `				sLocal.nIdx = nIdx;` |
|    79462 |  1810 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39732 |  1811 | `			}else{` |
|        - |  1812 | `				/* Register in the $GLOBALS array */` |
|     6902 |  1813 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1814 | `			}` |
|        - |  1815 | `			/* Install in the reference table */` |
|    86362 |  1816 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1817 | `			/* Save object index */` |
|    86362 |  1818 | `			pObj->nIdx = nIdx;` |
|    43182 |  1819 | `		}else{` |
|        - |  1820 | `			/* Extract variable contents */` |
|  3009082 |  1821 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3009082 |  1822 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3009082 |  1823 | `			if( bNullify && pObj ){` |
|      ! 0 |  1824 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1825 | `			}` |
|        - |  1826 | `		}` |
|  1547832 |  1827 | `	}else{` |
|        - |  1828 | `		/* Superglobal */` |
|       42 |  1829 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1830 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1831 | `	}` |
|  3095482 |  1832 | `	return pObj;` |
|  1547870 |  1833 |  |
|        - |  1834 | `/*` |
|        - |  1835 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1836 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1837 | ` */` |
|     2840 |  1838 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1839 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1840 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1841 | `	sxu32 nByte        /* zName length */` |
|        - |  1842 | `	)` |
|        2 |  1843 |  |
|        - |  1844 | `	SyHashEntry *pEntry;` |
|        - |  1845 | `	ph7_value *pValue;` |
|        - |  1846 | `	sxu32 nIdx;` |
|        - |  1847 | `	/* Query the superglobal table */` |
|     2842 |  1848 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2842 |  1849 | `	if( pEntry == 0 ){` |
|        - |  1850 | `		/* No such entry */` |
|      ! 0 |  1851 | `		return 0;` |
|        - |  1852 | `	}` |
|        - |  1853 | `	/* Extract the superglobal index in the global object pool */` |
|     2842 |  1854 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1855 | `	/* Extract the variable value  */` |
|     2842 |  1856 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2842 |  1857 | `	return pValue;` |
|     1422 |  1858 |  |
|        - |  1859 | `/*` |
|        - |  1860 | ` * Perform a raw hashmap insertion.` |
|        - |  1861 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1862 | ` */` |
|     2870 |  1863 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1864 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1865 | `	const char *zKey,   /* Entry key */` |
|        - |  1866 | `	int nKeylen,        /* zKey length*/` |
|        - |  1867 | `	const char *zData,  /* Entry data */` |
|        - |  1868 | `	int nLen            /* zData length */` |
|        - |  1869 | `	)` |
|        2 |  1870 |  |
|        - |  1871 | `	ph7_value sKey,sValue;` |
|        - |  1872 | `	sxi32 rc;` |
|     2872 |  1873 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2872 |  1874 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2872 |  1875 | `	if( zKey ){` |
|     2850 |  1876 | `		if( nKeylen < 0 ){` |
|     2798 |  1877 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1398 |  1878 | `		}` |
|     2850 |  1879 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1424 |  1880 | `	}` |
|     2872 |  1881 | `	if( zData ){` |
|     2872 |  1882 | `		if( nLen < 0 ){` |
|        - |  1883 | `			/* Compute length automatically */` |
|      144 |  1884 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1885 | `		}` |
|     2872 |  1886 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1435 |  1887 | `	}` |
|        - |  1888 | `	/* Perform the insertion */` |
|     2872 |  1889 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2872 |  1890 | `	PH7_MemObjRelease(&sKey);` |
|     2872 |  1891 | `	PH7_MemObjRelease(&sValue);` |
|     2872 |  1892 | `	return rc;` |
|        2 |  1893 |  |
|        - |  1894 | `/*` |
|        - |  1895 | ` * Configure a working virtual machine instance.` |
|        - |  1896 | ` *` |
|        - |  1897 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1898 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1899 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1900 | ` * The second argument to this function is an integer configuration option` |
|        - |  1901 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1902 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1903 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1904 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1905 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1906 | ` */` |
|    40906 |  1907 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1908 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1909 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1910 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|    40908 |  1913 | `	sxi32 rc = SXRET_OK;` |
|    40908 |  1914 | `	switch(nOp){` |
|     1260 |  1915 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2522 |  1916 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2522 |  1917 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1918 | `		/* VM output consumer callback */` |
|        - |  1919 | `#ifdef UNTRUST` |
|        - |  1920 | `		if( xConsumer == 0 ){` |
|        - |  1921 | `			rc = SXERR_CORRUPT;` |
|        - |  1922 | `			break;` |
|        - |  1923 | `		}` |
|        - |  1924 | `#endif` |
|        - |  1925 | `		/* Install the output consumer */` |
|     2522 |  1926 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2522 |  1927 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2522 |  1928 | `		break;` |
|        - |  1929 | `							   }` |
|     1268 |  1930 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1931 | `		/* Import path */` |
|        - |  1932 | `		  const char *zPath;` |
|        - |  1933 | `		  SyString sPath;` |
|     2538 |  1934 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1935 | `#if defined(UNTRUST)` |
|        - |  1936 | `		  if( zPath == 0 ){` |
|        - |  1937 | `			  rc = SXERR_EMPTY;` |
|        - |  1938 | `			  break;` |
|        - |  1939 | `		  }` |
|        - |  1940 | `#endif` |
|     2538 |  1941 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1942 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1943 | `#ifdef __WINNT__` |
|        2 |  1944 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1945 | `#endif` |
|     5074 |  1946 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1947 | `		  /* Remove leading and trailing white spaces */` |
|     2538 |  1948 | `		  SyStringFullTrim(&sPath);` |
|     2538 |  1949 | `		  if( sPath.nByte > 0 ){` |
|        - |  1950 | `			  /* Store the path in the corresponding conatiner */` |
|     2538 |  1951 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1268 |  1952 | `		  }` |
|     2538 |  1953 | `		  break;` |
|        - |  1954 | `									 }` |
|     1268 |  1955 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1956 | `		/* Run-Time Error report */` |
|     2538 |  1957 | `		pVm->bErrReport = 1;` |
|     2538 |  1958 | `		break;` |
|      ! 0 |  1959 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1960 | `		/* Recursion depth */` |
|      ! 0 |  1961 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1962 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1963 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1964 | `		}` |
|      ! 0 |  1965 | `		break;` |
|        - |  1966 | `									   }` |
|      ! 0 |  1967 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1968 | `		/* VM output length in bytes */` |
|      ! 0 |  1969 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1970 | `#ifdef UNTRUST` |
|        - |  1971 | `		if( pOut == 0 ){` |
|        - |  1972 | `			rc = SXERR_CORRUPT;` |
|        - |  1973 | `			break;` |
|        - |  1974 | `		}` |
|        - |  1975 | `#endif` |
|      ! 0 |  1976 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1977 | `		break;` |
|        - |  1978 | `							   }` |
|        - |  1979 |  |
|    12680 |  1980 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1981 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1982 | `		/* Create a new superglobal/global variable */` |
|    25362 |  1983 | `		const char *zName = va_arg(ap,const char *);` |
|    25362 |  1984 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1985 | `		SyHashEntry *pEntry;` |
|        - |  1986 | `		ph7_value *pObj;` |
|        - |  1987 | `		sxu32 nByte;` |
|        - |  1988 | `		sxu32 nIdx;` |
|        - |  1989 | `#ifdef UNTRUST` |
|        - |  1990 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1991 | `			rc = SXERR_CORRUPT;` |
|        - |  1992 | `			break;` |
|        - |  1993 | `		}` |
|        - |  1994 | `#endif` |
|    25362 |  1995 | `		nByte = SyStrlen(zName);` |
|    25362 |  1996 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1997 | `			/* Check if the superglobal is already installed */` |
|    25362 |  1998 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12682 |  1999 | `		}else{` |
|        - |  2000 | `			/* Query the top active VM frame */` |
|      ! 0 |  2001 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2002 | `		}` |
|    25362 |  2003 | `		if( pEntry ){` |
|        - |  2004 | `			/* Variable already installed */` |
|      ! 0 |  2005 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2006 | `			/* Extract contents */` |
|      ! 0 |  2007 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2008 | `			if( pObj ){` |
|        - |  2009 | `				/* Overwrite old contents */` |
|      ! 0 |  2010 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2011 | `			}` |
|      ! 0 |  2012 | `		}else{` |
|        - |  2013 | `			/* Install a new variable */` |
|    25362 |  2014 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25362 |  2015 | `			if( pObj == 0 ){` |
|      ! 0 |  2016 | `				rc = SXERR_MEM;` |
|      ! 0 |  2017 | `				break;` |
|        - |  2018 | `			}` |
|    25362 |  2019 | `			nIdx = pObj->nIdx;` |
|        - |  2020 | `			/* Copy value */` |
|    25362 |  2021 | `			PH7_MemObjStore(pValue,pObj);` |
|    25362 |  2022 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2023 | `				/* Install the superglobal */` |
|    25362 |  2024 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12682 |  2025 | `			}else{` |
|        - |  2026 | `				/* Install in the current frame */` |
|      ! 0 |  2027 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2028 | `			}` |
|    25362 |  2029 | `			if( rc == SXRET_OK ){` |
|        - |  2030 | `				SyHashEntry *pRef;` |
|    25362 |  2031 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25362 |  2032 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12682 |  2033 | `				}else{` |
|      ! 0 |  2034 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2035 | `				}` |
|        - |  2036 | `				/* Install in the reference table */` |
|    25362 |  2037 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25362 |  2038 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2039 | `					/* Register in the $GLOBALS array */` |
|    25362 |  2040 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12680 |  2041 | `				}` |
|    12680 |  2042 | `			}` |
|        - |  2043 | `		}` |
|    25362 |  2044 | `		break;` |
|        - |  2045 | `									}` |
|     1398 |  2046 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2047 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2048 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2049 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2050 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2051 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2052 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2798 |  2053 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2798 |  2054 | `		const char *zValue = va_arg(ap,const char *);` |
|     2798 |  2055 | `		int nLen = va_arg(ap,int);` |
|        - |  2056 | `		ph7_hashmap *pMap;` |
|        - |  2057 | `		ph7_value *pValue;` |
|     2798 |  2058 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2059 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2060 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2797 |  2061 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2062 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2063 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2796 |  2064 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2065 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2066 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2796 |  2067 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2068 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2069 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2796 |  2070 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2071 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2072 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2796 |  2073 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2074 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2076 | `		}else{` |
|        - |  2077 | `			/* Extract the $_SERVER superglobal */` |
|     2796 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2079 | `		}` |
|     2798 |  2080 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2081 | `			/* No such entry */` |
|      ! 0 |  2082 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2083 | `			break;` |
|        - |  2084 | `		}` |
|        - |  2085 | `		/* Point to the hashmap */` |
|     2798 |  2086 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2087 | `		/* Perform the insertion */` |
|     2798 |  2088 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2798 |  2089 | `		break;` |
|        - |  2090 | `								   }` |
|       11 |  2091 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2092 | `		/* Script arguments */` |
|       24 |  2093 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2094 | `		ph7_hashmap *pMap;` |
|        - |  2095 | `		ph7_value *pValue;` |
|        - |  2096 | `		sxu32 n;` |
|       24 |  2097 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2098 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2099 | `			break;` |
|        - |  2100 | `		}` |
|        - |  2101 | `		/* Extract the $argv array */` |
|       24 |  2102 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2103 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2104 | `			/* No such entry */` |
|      ! 0 |  2105 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2106 | `			break;` |
|        - |  2107 | `		}` |
|        - |  2108 | `		/* Point to the hashmap */` |
|       24 |  2109 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2110 | `		/* Perform the insertion */` |
|       24 |  2111 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2112 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2113 | `		if( rc == SXRET_OK ){` |
|       24 |  2114 | `			if( pMap->nEntry > 1 ){` |
|        - |  2115 | `				/* Append space separator first */` |
|       18 |  2116 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2117 | `			}` |
|       24 |  2118 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2119 | `		}` |
|       24 |  2120 | `		break;` |
|        - |  2121 | `								  }` |
|      ! 0 |  2122 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2123 | `		/* error_log() consumer */` |
|      ! 0 |  2124 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2125 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2126 | `		break;` |
|        - |  2127 | `										}` |
|      ! 0 |  2128 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2129 | `		/* Script return value */` |
|      ! 0 |  2130 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2131 | `#ifdef UNTRUST` |
|        - |  2132 | `		if( ppValue == 0 ){` |
|        - |  2133 | `			rc = SXERR_CORRUPT;` |
|        - |  2134 | `			break;` |
|        - |  2135 | `		}` |
|        - |  2136 | `#endif` |
|      ! 0 |  2137 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2138 | `		break;` |
|        - |  2139 | `								   }` |
|     2536 |  2140 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2141 | `		/* Register an IO stream device */` |
|     5074 |  2142 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2143 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7608 |  2144 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5074 |  2145 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2146 | `				/* Invalid stream */` |
|      ! 0 |  2147 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2148 | `				break;` |
|        - |  2149 | `		}` |
|     5074 |  2150 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2151 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2538 |  2152 | `			pVm->pDefStream = pStream;` |
|     1268 |  2153 | `		}` |
|        - |  2154 | `		/* Insert in the appropriate container */` |
|     5074 |  2155 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5074 |  2156 | `		break;` |
|        - |  2157 | `								  }` |
|        8 |  2158 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2159 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2160 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2161 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2162 | `#ifdef UNTRUST` |
|        - |  2163 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2164 | `			rc = SXERR_CORRUPT;` |
|        - |  2165 | `			break;` |
|        - |  2166 | `		}` |
|        - |  2167 | `#endif` |
|       16 |  2168 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2169 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2170 | `		break;` |
|        - |  2171 | `									   }` |
|        8 |  2172 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2173 | `		/* Raw HTTP request*/` |
|       16 |  2174 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2175 | `		int nByte = va_arg(ap,int);` |
|       16 |  2176 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2177 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2178 | `			break;` |
|        - |  2179 | `		}` |
|       16 |  2180 | `		if( nByte < 0 ){` |
|        - |  2181 | `			/* Compute length automatically */` |
|      ! 0 |  2182 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2183 | `		}` |
|        - |  2184 | `		/* Process the request */` |
|       16 |  2185 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2186 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2187 | `		if( rc == SXRET_OK ){` |
|       16 |  2188 | `			pVm->bHttpContext = 1;` |
|        8 |  2189 | `		}` |
|       16 |  2190 | `		break;` |
|        - |  2191 | `									}` |
|        8 |  2192 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2193 | `		/* Extract HTTP response status code */` |
|       16 |  2194 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2195 | `		if( pStatus ){` |
|       16 |  2196 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2197 | `		}` |
|       16 |  2198 | `		break;` |
|        - |  2199 | `										}` |
|        8 |  2200 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2201 | `		/* Iterate response headers via callback */` |
|        - |  2202 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2203 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2204 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2205 | `		if( xCallback ){` |
|       16 |  2206 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2207 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2208 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2209 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2210 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2211 | `							   pUserData);` |
|       12 |  2212 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2213 | `					break;` |
|        - |  2214 | `				}` |
|        6 |  2215 | `			}` |
|        8 |  2216 | `		}` |
|       16 |  2217 | `		break;` |
|        - |  2218 | `										 }` |
|      ! 0 |  2219 | `	default:` |
|        - |  2220 | `		/* Unknown configuration option */` |
|      ! 0 |  2221 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2222 | `		break;` |
|        - |  2223 | `	}` |
|    40908 |  2224 | `	return rc;` |
|        2 |  2225 |  |
|        - |  2226 | `/* Forward declaration */` |
|        - |  2227 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2228 | `/*` |
|        - |  2229 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2230 | ` * format.` |
|        - |  2231 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2232 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2233 | ` * (STDOUT).` |
|        - |  2234 | ` */` |
|        2 |  2235 | `static sxi32 VmByteCodeDump(` |
|        - |  2236 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2237 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2238 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2239 | `	)` |
|        1 |  2240 |  |
|        - |  2241 | `	static const char zDump[] = {` |
|        - |  2242 | `		"====================================================\n"` |
|        - |  2243 | `		"PH7 VM Dump\n"` |
|        - |  2244 | `		"====================================================\n"` |
|        - |  2245 | `	};` |
|        - |  2246 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2247 | `	sxi32 rc = SXRET_OK;` |
|        - |  2248 | `	sxu32 n;` |
|        - |  2249 | `	/* Point to the PH7 instructions */` |
|        3 |  2250 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2251 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2252 | `	n = 0;` |
|        3 |  2253 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2254 | `	/* Dump instructions */` |
|        7 |  2255 | `	for(;;){` |
|       15 |  2256 | `		if( pInstr >= pEnd ){` |
|        - |  2257 | `			/* No more instructions */` |
|        3 |  2258 | `			break;` |
|        - |  2259 | `		}` |
|        - |  2260 | `		/* Format and call the consumer callback */` |
|       19 |  2261 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2262 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2263 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2264 | `		if( rc != SXRET_OK ){` |
|        - |  2265 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2266 | `			return rc;` |
|        - |  2267 | `		}` |
|       13 |  2268 | `		++n;` |
|       13 |  2269 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2270 | `	}` |
|        3 |  2271 | `	return rc;` |
|        2 |  2272 |  |
|        - |  2273 | `/* Forward declaration */` |
|        - |  2274 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2275 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2276 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2277 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2278 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2279 | `/*` |
|        - |  2280 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2281 | ` * consumer callback.` |
|        - |  2282 | ` */` |
|      544 |  2283 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2284 |  |
|      545 |  2285 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      545 |  2286 | `	sxi32 rc = SXRET_OK;` |
|        - |  2287 | `	/* Append a new line */` |
|        - |  2288 | `#ifdef __WINNT__` |
|        1 |  2289 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2290 | `#else` |
|      544 |  2291 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2292 | `#endif` |
|        - |  2293 | `	/* Invoke the output consumer callback */` |
|      545 |  2294 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      545 |  2295 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      545 |  2296 | `	return rc;` |
|        1 |  2297 |  |
|        - |  2298 | `/*` |
|        - |  2299 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2300 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2301 | ` * information.` |
|        - |  2302 | ` */` |
|      132 |  2303 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2304 |  |
|      134 |  2305 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2306 | `		ph7_value apArg[4];` |
|        - |  2307 | `		ph7_value *apArgPtr[4];` |
|        - |  2308 | `		ph7_value sResult;` |
|        - |  2309 | `		SyString sErr;` |
|        - |  2310 | `		/* Prepare arguments */` |
|       61 |  2311 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2312 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2313 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2314 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2315 | `		if( pFile ){` |
|       61 |  2316 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2317 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2318 | `		}else{` |
|      ! 0 |  2319 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2320 | `		}` |
|       61 |  2321 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2322 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2323 | `		/* Set up pointer array */` |
|       61 |  2324 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2325 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2326 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2327 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2328 | `		/* Call the handler */` |
|       61 |  2329 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2330 | `		/* Check return value */` |
|       61 |  2331 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2332 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2333 | `		}` |
|        - |  2334 | `		/* Release */` |
|       61 |  2335 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2336 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2337 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2338 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2339 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2340 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2341 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2342 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2343 | `	}` |
|        - |  2344 | `	/* No handler, always call error handler */` |
|       73 |  2345 | `	return TRUE;` |
|       68 |  2346 |  |
|       96 |  2347 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2348 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2349 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2350 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2351 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2352 | `	)` |
|        2 |  2353 |  |
|       98 |  2354 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2355 | `	SyString *pFile;` |
|        - |  2356 | `	char *zErr;` |
|       98 |  2357 | `	sxi32 rc = SXRET_OK;` |
|       98 |  2358 | `	if( !pVm->bErrReport ){` |
|        - |  2359 | `		/* Don't bother reporting errors */` |
|        3 |  2360 | `		return SXRET_OK;` |
|        - |  2361 | `	}` |
|        - |  2362 | `	/* Reset the working buffer */` |
|       96 |  2363 | `	SyBlobReset(pWorker);` |
|        - |  2364 | `	/* Peek the processed file if available */` |
|       96 |  2365 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       96 |  2366 | `	if( pFile ){` |
|        - |  2367 | `		/* Append file name */` |
|       96 |  2368 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       96 |  2369 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       47 |  2370 | `	}` |
|        - |  2371 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2372 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2373 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2374 | `	 * E_DEPRECATED). */` |
|       96 |  2375 | `	zErr = "Error:  ";` |
|       96 |  2376 | `	switch(iErr){` |
|       18 |  2377 | `	case PH7_CTX_WARNING:` |
|       38 |  2378 | `		zErr = "Warning:  ";` |
|       38 |  2379 | `		break;` |
|        6 |  2380 | `	case PH7_CTX_NOTICE:` |
|       14 |  2381 | `		zErr = "Notice:  ";` |
|       12 |  2382 | `		break;` |
|       23 |  2383 | `	default:` |
|        - |  2384 | `		/* keep iErr unchanged */` |
|       46 |  2385 | `		break;` |
|        - |  2386 | `	}` |
|       96 |  2387 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       96 |  2388 | `	if( pFuncName ){` |
|        - |  2389 | `		/* Append function name first */` |
|       23 |  2390 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2391 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2392 | `	}` |
|       96 |  2393 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2394 | `	/* Check for user error handler.  compute length of C string */` |
|       96 |  2395 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       47 |  2396 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       23 |  2397 | `	}` |
|       96 |  2398 | `	return rc;` |
|       50 |  2399 |  |
|        - |  2400 | `/*` |
|        - |  2401 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2402 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2403 | ` * information.` |
|        - |  2404 | ` */` |
|       38 |  2405 | `static sxi32 VmThrowErrorAp(` |
|        - |  2406 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2407 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2408 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2409 | `	const char *zFormat, /* Format message */` |
|        - |  2410 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2411 | `	)` |
|        2 |  2412 |  |
|       40 |  2413 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2414 | `	SyBlob sMsg;` |
|        - |  2415 | `	SyString *pFile;` |
|        - |  2416 | `	char *zErr;` |
|       40 |  2417 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2418 | `	if( !pVm->bErrReport ){` |
|        - |  2419 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2420 | `		return SXRET_OK;` |
|        - |  2421 | `	}` |
|        - |  2422 | `	/* Reset the working buffer */` |
|       40 |  2423 | `	SyBlobReset(pWorker);` |
|        - |  2424 | `	/* Peek the processed file if available */` |
|       40 |  2425 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2426 | `	if( pFile ){` |
|        - |  2427 | `		/* Append file name */` |
|       40 |  2428 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2429 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2430 | `	}` |
|        - |  2431 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2432 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2433 | `	 * the correct errno value. */` |
|       40 |  2434 | `	zErr = "Error:  ";` |
|       40 |  2435 | `	switch(iErr){` |
|        4 |  2436 | `	case PH7_CTX_WARNING:` |
|        9 |  2437 | `		zErr = "Warning:  ";` |
|        9 |  2438 | `		break;` |
|        3 |  2439 | `	case PH7_CTX_NOTICE:` |
|        7 |  2440 | `		zErr = "Notice:  ";` |
|        6 |  2441 | `		break;` |
|       12 |  2442 | `	default:` |
|        - |  2443 | `		/* do not change iErr */` |
|       24 |  2444 | `		break;` |
|        - |  2445 | `	}` |
|       40 |  2446 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2447 | `	if( pFuncName ){` |
|        - |  2448 | `		/* Append function name first */` |
|       26 |  2449 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2450 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2451 | `	}` |
|        - |  2452 | `	/* Format the raw message */` |
|       40 |  2453 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2454 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2455 | `	/* Check if a user error handler is installed */` |
|       40 |  2456 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2457 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2458 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2459 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2460 | `	}` |
|       40 |  2461 | `	SyBlobRelease(&sMsg);` |
|       40 |  2462 | `	return rc;` |
|       21 |  2463 |  |
|        - |  2464 | `/*` |
|        - |  2465 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2466 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2467 | ` * information.` |
|        - |  2468 | ` * ------------------------------------` |
|        - |  2469 | ` * Simple boring wrapper function.` |
|        - |  2470 | ` * ------------------------------------` |
|        - |  2471 | ` */` |
|       14 |  2472 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2473 |  |
|        - |  2474 | `	va_list ap;` |
|        - |  2475 | `	sxi32 rc;` |
|       15 |  2476 | `	va_start(ap,zFormat);` |
|       15 |  2477 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2478 | `	va_end(ap);` |
|       15 |  2479 | `	return rc;` |
|        1 |  2480 |  |
|        - |  2481 | `/*` |
|        - |  2482 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2483 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2484 | ` * information.` |
|        - |  2485 | ` * ------------------------------------` |
|        - |  2486 | ` * Simple boring wrapper function.` |
|        - |  2487 | ` * ------------------------------------` |
|        - |  2488 | ` */` |
|       24 |  2489 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2490 |  |
|        - |  2491 | `	sxi32 rc;` |
|       26 |  2492 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2493 | `	return rc;` |
|        2 |  2494 |  |
|        - |  2495 | `/*` |
|        - |  2496 | ` * Resolve function context from the current frame.` |
|        - |  2497 | ` */` |
|      934 |  2498 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2499 |  |
|        - |  2500 | `	VmFrame *pFrame;` |
|        - |  2501 | `	ph7_vm_func *pFunc;` |
|      935 |  2502 | `	*pzFuncName = 0;` |
|      935 |  2503 | `	*pnFuncLen = 0;` |
|      935 |  2504 | `	pFrame = pVm->pFrame;` |
|      935 |  2505 | `	if( pFrame == 0 ){` |
|      ! 0 |  2506 | `		return;` |
|        - |  2507 | `	}` |
|      935 |  2508 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2509 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2510 | `		return;` |
|        - |  2511 | `	}` |
|        7 |  2512 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2513 | `	if( pFunc == 0 ){` |
|      ! 0 |  2514 | `		return;` |
|        - |  2515 | `	}` |
|        7 |  2516 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2517 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2518 |  |
|        - |  2519 | `/*` |
|        - |  2520 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2521 | ` */` |
|      470 |  2522 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2523 |  |
|        - |  2524 | `	SyBlob sOut;` |
|        - |  2525 | `	SyString *pFile;` |
|      471 |  2526 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2527 | `		return PH7_OK;` |
|        - |  2528 | `	}` |
|      471 |  2529 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2530 | `		zClass = "Exception";` |
|      ! 0 |  2531 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2532 | `	}` |
|      471 |  2533 | `	if( zMsg == 0 ){` |
|      ! 0 |  2534 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2535 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2536 | `	}` |
|      471 |  2537 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2538 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2539 | `	}` |
|      471 |  2540 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2541 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2542 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2543 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2544 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2545 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2546 | `	if( pFile ){` |
|      471 |  2547 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2548 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2549 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2550 | `	}` |
|      471 |  2551 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2552 | `	if( pFile ){` |
|      471 |  2553 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2554 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2555 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2556 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2557 | `		}else{` |
|      465 |  2558 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2559 | `		}` |
|      235 |  2560 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2561 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2562 | `	}else{` |
|      ! 0 |  2563 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2564 | `	}` |
|      471 |  2565 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2566 | `	if( pFile ){` |
|      471 |  2567 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2568 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2569 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2570 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2571 | `	}` |
|      471 |  2572 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2573 | `	SyBlobRelease(&sOut);` |
|      471 |  2574 | `	return PH7_ABORT;` |
|      236 |  2575 |  |
|        - |  2576 | `/*` |
|        - |  2577 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2578 | ` */` |
|      472 |  2579 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2580 |  |
|        - |  2581 | `	ph7_vm *pVm;` |
|        - |  2582 | `	ph7_class *pClass;` |
|        - |  2583 | `	ph7_class_instance *pThis;` |
|        - |  2584 | `	ph7_class_method *pCons;` |
|        - |  2585 | `	ph7_value sArg;` |
|        - |  2586 | `	ph7_value *apArg[1];` |
|        - |  2587 | `	SyBlob sMsg;` |
|        - |  2588 | `	SyString sMsgStr;` |
|        - |  2589 | `	VmFrame *pFrame;` |
|        - |  2590 | `	va_list ap;` |
|        - |  2591 | `	sxi32 rc;` |
|        - |  2592 |  |
|      474 |  2593 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2594 | `		return PH7_ABORT;` |
|        - |  2595 | `	}` |
|      474 |  2596 | `	pVm = pCtx->pVm;` |
|      474 |  2597 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2598 | `		zClass = "Error";` |
|      ! 0 |  2599 | `	}` |
|      474 |  2600 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      474 |  2601 | `	if( pClass == 0 ){` |
|      ! 0 |  2602 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2603 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2604 | `			zClass` |
|        - |  2605 | `			);` |
|        - |  2606 | `	}` |
|      474 |  2607 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      474 |  2608 | `	if( pThis == 0 ){` |
|      ! 0 |  2609 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2610 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2611 | `			);` |
|        - |  2612 | `	}` |
|        - |  2613 |  |
|      474 |  2614 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      474 |  2615 | `	va_start(ap,zFormat);` |
|      474 |  2616 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      474 |  2617 | `	va_end(ap);` |
|        - |  2618 |  |
|      474 |  2619 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      474 |  2620 | `	if( pCons ){` |
|      474 |  2621 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      474 |  2622 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      474 |  2623 | `		apArg[0] = &sArg;` |
|      474 |  2624 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      474 |  2625 | `		PH7_MemObjRelease(&sArg);` |
|      236 |  2626 | `	}` |
|      474 |  2627 | `	SyBlobRelease(&sMsg);` |
|        - |  2628 |  |
|      474 |  2629 | `	pFrame = pVm->pFrame;` |
|      474 |  2630 | `	if( pFrame ){` |
|      474 |  2631 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      474 |  2632 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      236 |  2633 | `	}` |
|      474 |  2634 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      474 |  2635 | `	PH7_ClassInstanceUnref(pThis);` |
|      474 |  2636 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2637 | `		return PH7_ABORT;` |
|        - |  2638 | `	}` |
|       12 |  2639 | `	return PH7_EXCEPTION;` |
|      238 |  2640 |  |
|        - |  2641 | `/*` |
|        - |  2642 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2643 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2644 | ` */` |
|      ! 0 |  2645 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2646 |  |
|        - |  2647 | `	ph7_vm *pVm;` |
|        - |  2648 | `	SyBlob sMsg;` |
|      ! 0 |  2649 | `	const char *zFuncName = 0;` |
|      ! 0 |  2650 | `	int nFuncLen = 0;` |
|        - |  2651 | `	va_list ap;` |
|        - |  2652 | `	sxi32 rc;` |
|        - |  2653 |  |
|      ! 0 |  2654 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2655 | `		return PH7_OK;` |
|        - |  2656 | `	}` |
|      ! 0 |  2657 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2658 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2659 | `		zClass = "Error";` |
|      ! 0 |  2660 | `	}` |
|        - |  2661 |  |
|      ! 0 |  2662 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2663 |  |
|      ! 0 |  2664 | `	va_start(ap,zFormat);` |
|      ! 0 |  2665 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2666 | `	va_end(ap);` |
|        - |  2667 |  |
|      ! 0 |  2668 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2669 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2670 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2671 | `	}` |
|      ! 0 |  2672 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2673 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2674 | `	}` |
|      ! 0 |  2675 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2676 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2677 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2678 | `	return rc;` |
|      ! 0 |  2679 |  |
|        - |  2680 | `/*` |
|        - |  2681 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2682 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2683 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2684 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2685 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2686 | ` * when VmByteCodeExec returns.` |
|        - |  2687 | ` */` |
|      132 |  2688 | `static sxi32 VmSuspendCtx(` |
|        - |  2689 | `	ph7_vm *pVm,` |
|        - |  2690 | `	ph7_exec_ctx *pCtx,` |
|        - |  2691 | `	sxi32 pc,` |
|        - |  2692 | `	sxi32 nTos` |
|        - |  2693 | `	)` |
|        1 |  2694 |  |
|       66 |  2695 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      133 |  2696 | `	pCtx->pc = pc;` |
|      133 |  2697 | `	pCtx->nTos = nTos;` |
|      133 |  2698 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      133 |  2699 | `	return PH7_SUSPEND;` |
|        1 |  2700 |  |
|        - |  2701 | `/*` |
|        - |  2702 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2703 | ` *` |
|        - |  2704 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2705 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2706 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2707 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2708 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2709 | ` * then the program execution is halted.` |
|        - |  2710 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2711 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2712 | ` * or to reset the VM to it's initial state.` |
|        - |  2713 | ` */` |
|    32898 |  2714 | `static sxi32 VmByteCodeExec(` |
|        - |  2715 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2716 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2717 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2718 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2719 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2720 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2721 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2722 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2723 | `	)` |
|        2 |  2724 |  |
|        - |  2725 | `	VmInstr *pInstr;` |
|        - |  2726 | `	ph7_value *pTos;` |
|        - |  2727 | `	SySet aArg;` |
|        - |  2728 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2729 | `	sxi32 pc;` |
|        - |  2730 | `	sxi32 rc;` |
|        - |  2731 | `	/* Argument container */` |
|    32900 |  2732 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32900 |  2733 | `	if( nTos < 0 ){` |
|    30880 |  2734 | `		pTos = &pStack[-1];` |
|    15441 |  2735 | `	}else{` |
|     2022 |  2736 | `		pTos = &pStack[nTos];` |
|        - |  2737 | `	}` |
|    32900 |  2738 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32900 |  2739 | `	pc = nPc;` |
|        - |  2740 | `	/* Execute as much as we can */` |
|  4963372 |  2741 | `	for(;;){` |
|        - |  2742 | `		/* Fetch the instruction to execute */` |
|  9926042 |  2743 | `		pInstr = &aInstr[pc];` |
|  9926042 |  2744 | `		rc = SXRET_OK;` |
|        - |  2745 | `/*` |
|        - |  2746 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2747 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2748 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2749 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2750 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2751 | ` */` |
|  9926042 |  2752 | `		switch(pInstr->iOp){` |
|        - |  2753 | `/*` |
|        - |  2754 | ` * DONE: P1 * *` |
|        - |  2755 | ` *` |
|        - |  2756 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2757 | ` * and return immediately.` |
|        - |  2758 | ` */` |
|    16138 |  2759 | `case PH7_OP_DONE:` |
|    32278 |  2760 | `	if( pInstr->iP1 ){` |
|        - |  2761 | `#ifdef UNTRUST` |
|        - |  2762 | `		if( pTos < pStack ){` |
|        - |  2763 | `			goto Abort;` |
|        - |  2764 | `		}` |
|        - |  2765 | `#endif` |
|    18706 |  2766 | `		if( pLastRef ){` |
|    12248 |  2767 | `			*pLastRef = pTos->nIdx;` |
|     6123 |  2768 | `		}` |
|    18706 |  2769 | `		if( pResult ){` |
|        - |  2770 | `			/* Execution result */` |
|    17770 |  2771 | `			PH7_MemObjStore(pTos,pResult);` |
|     8884 |  2772 | `		}` |
|    18706 |  2773 | `		VmPopOperand(&pTos,1);` |
|    22926 |  2774 | `	}else if( pLastRef ){` |
|        - |  2775 | `		/* Nothing referenced */` |
|      992 |  2776 | `		*pLastRef = SXU32_HIGH;` |
|      495 |  2777 | `	}` |
|        - |  2778 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2779 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2780 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2781 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2782 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2783 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2784 | `	 * block can override it.` |
|        - |  2785 | `	 */` |
|    32280 |  2786 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2787 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2788 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2789 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2790 | `		pExc->pFrame = 0;` |
|        3 |  2791 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2792 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2793 | `			pExc->iFinallyDone = 1;` |
|        - |  2794 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2795 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2796 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2797 | `				goto Abort;` |
|        - |  2798 | `			}` |
|        1 |  2799 | `		}` |
|        1 |  2800 | `	}` |
|    32278 |  2801 | `	goto Done;` |
|        - |  2802 | `/*` |
|        - |  2803 | ` * HALT: P1 * *` |
|        - |  2804 | ` *` |
|        - |  2805 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2806 | ` * and abort immediately.` |
|        - |  2807 | ` */` |
|        4 |  2808 | `case PH7_OP_HALT:` |
|        9 |  2809 | `	if( pInstr->iP1 ){` |
|        - |  2810 | `#ifdef UNTRUST` |
|        - |  2811 | `		if( pTos < pStack ){` |
|        - |  2812 | `			goto Abort;` |
|        - |  2813 | `		}` |
|        - |  2814 | `#endif` |
|        9 |  2815 | `		if( pLastRef ){` |
|      ! 0 |  2816 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2817 | `		}` |
|        9 |  2818 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2819 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2820 | `				/* Output the exit message */` |
|        7 |  2821 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2822 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2823 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2824 | `			}` |
|        7 |  2825 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2826 | `			/* Record exit status */` |
|        5 |  2827 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2828 | `		}` |
|        9 |  2829 | `		VmPopOperand(&pTos,1);` |
|        4 |  2830 | `	}else if( pLastRef ){` |
|        - |  2831 | `		/* Nothing referenced */` |
|      ! 0 |  2832 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2833 | `	}` |
|        - |  2834 | `	/* Check if we're in an included file context */` |
|        9 |  2835 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2836 | `		/* Terminate the entire process */` |
|        9 |  2837 | `		exit(pVm->iExitStatus);` |
|        - |  2838 | `	}` |
|      ! 0 |  2839 | `	goto Abort;` |
|        - |  2840 | `/*` |
|        - |  2841 | ` * JMP: * P2 *` |
|        - |  2842 | ` *` |
|        - |  2843 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2844 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2845 | ` */` |
|   213937 |  2846 | `case PH7_OP_JMP:` |
|   427920 |  2847 | `	pc = pInstr->iP2 - 1;` |
|   427920 |  2848 | `	break;` |
|        - |  2849 | `/*` |
|        - |  2850 | ` * JZ: P1 P2 *` |
|        - |  2851 | ` *` |
|        - |  2852 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2853 | ` * entry in the stack if P1 is zero.` |
|        - |  2854 | ` */` |
|   499870 |  2855 | `case PH7_OP_JZ:` |
|        - |  2856 | `#ifdef UNTRUST` |
|        - |  2857 | `	if( pTos < pStack ){` |
|        - |  2858 | `		goto Abort;` |
|        - |  2859 | `	}` |
|        - |  2860 | `#endif` |
|        - |  2861 | `	/* Get a boolean value */` |
|   999830 |  2862 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2863 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2864 | `	}` |
|   999830 |  2865 | `	if( !pTos->x.iVal ){` |
|        - |  2866 | `		/* Take the jump */` |
|   504838 |  2867 | `		pc = pInstr->iP2 - 1;` |
|   252418 |  2868 | `	}` |
|   999830 |  2869 | `	if( !pInstr->iP1 ){` |
|   796378 |  2870 | `		VmPopOperand(&pTos,1);` |
|   398210 |  2871 | `	}` |
|   999830 |  2872 | `	break;` |
|        - |  2873 | `/*` |
|        - |  2874 | ` * JNZ: P1 P2 *` |
|        - |  2875 | ` *` |
|        - |  2876 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2877 | ` * entry in the stack if P1 is zero.` |
|        - |  2878 | ` */` |
|    53433 |  2879 | `case PH7_OP_JNZ:` |
|        - |  2880 | `#ifdef UNTRUST` |
|        - |  2881 | `	if( pTos < pStack ){` |
|        - |  2882 | `		goto Abort;` |
|        - |  2883 | `	}` |
|        - |  2884 | `#endif` |
|        - |  2885 | `	/* Get a boolean value */` |
|   106868 |  2886 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2887 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2888 | `	}` |
|   106868 |  2889 | `	if( pTos->x.iVal ){` |
|        - |  2890 | `		/* Take the jump */` |
|     4460 |  2891 | `		pc = pInstr->iP2 - 1;` |
|     2229 |  2892 | `	}` |
|   106868 |  2893 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2894 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2895 | `	}` |
|   106868 |  2896 | `	break;` |
|        - |  2897 | `/*` |
|        - |  2898 | ` * NOOP: * * *` |
|        - |  2899 | ` *` |
|        - |  2900 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2901 | ` * destination.` |
|        - |  2902 | ` */` |
|      ! 0 |  2903 | `case PH7_OP_NOOP:` |
|      ! 0 |  2904 | `	break;` |
|        - |  2905 | `/*` |
|        - |  2906 | ` * POP: P1 * *` |
|        - |  2907 | ` *` |
|        - |  2908 | ` * Pop P1 elements from the operand stack.` |
|        - |  2909 | ` */` |
|   390489 |  2910 | `case PH7_OP_POP: {` |
|   781024 |  2911 | `	sxi32 n = pInstr->iP1;` |
|   781024 |  2912 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2913 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2914 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2915 | `	}` |
|   781024 |  2916 | `	VmPopOperand(&pTos,n);` |
|   781024 |  2917 | `	break;` |
|        - |  2918 | `				 }` |
|        - |  2919 | `/*` |
|        - |  2920 | ` * DUP: * * *` |
|        - |  2921 | ` *` |
|        - |  2922 | ` * Duplicate the top of the stack.` |
|        - |  2923 | ` */` |
|       35 |  2924 | `case PH7_OP_DUP:` |
|        - |  2925 | `#ifdef UNTRUST` |
|        - |  2926 | `	if( pTos < pStack ){` |
|        - |  2927 | `		goto Abort;` |
|        - |  2928 | `	}` |
|        - |  2929 | `#endif` |
|       72 |  2930 | `	pTos++;` |
|       72 |  2931 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2932 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2933 | `	break;` |
|        - |  2934 | `/*` |
|        - |  2935 | ` * NSSWITCH: * * P3` |
|        - |  2936 | ` *` |
|        - |  2937 | ` * Switch the active namespace at runtime.` |
|        - |  2938 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2939 | ` */` |
|     6516 |  2940 | `case PH7_OP_NSSWITCH:` |
|    13034 |  2941 | `	SyBlobReset(&pVm->sNamespace);` |
|    13034 |  2942 | `	if( pInstr->p3 ){` |
|       51 |  2943 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2944 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2945 | `	}` |
|    13034 |  2946 | `	break;` |
|        - |  2947 | `/*` |
|        - |  2948 | ` * CVT_INT: * * *` |
|        - |  2949 | ` *` |
|        - |  2950 | ` * Force the top of the stack to be an integer.` |
|        - |  2951 | ` */` |
|       35 |  2952 | `case PH7_OP_CVT_INT:` |
|        - |  2953 | `#ifdef UNTRUST` |
|        - |  2954 | `	if( pTos < pStack ){` |
|        - |  2955 | `		goto Abort;` |
|        - |  2956 | `	}` |
|        - |  2957 | `#endif` |
|       72 |  2958 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2959 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2960 | `	}` |
|        - |  2961 | `	/* Invalidate any prior representation */` |
|       72 |  2962 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2963 | `	break;` |
|        - |  2964 | `/*` |
|        - |  2965 | ` * CVT_REAL: * * *` |
|        - |  2966 | ` *` |
|        - |  2967 | ` * Force the top of the stack to be a real.` |
|        - |  2968 | ` */` |
|        4 |  2969 | `case PH7_OP_CVT_REAL:` |
|        - |  2970 | `#ifdef UNTRUST` |
|        - |  2971 | `	if( pTos < pStack ){` |
|        - |  2972 | `		goto Abort;` |
|        - |  2973 | `	}` |
|        - |  2974 | `#endif` |
|        9 |  2975 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2976 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2977 | `	}` |
|        - |  2978 | `	/* Invalidate any prior representation */` |
|        9 |  2979 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2980 | `	break;` |
|        - |  2981 | `/*` |
|        - |  2982 | ` * CVT_STR: * * *` |
|        - |  2983 | ` *` |
|        - |  2984 | ` * Force the top of the stack to be a string.` |
|        - |  2985 | ` */` |
|      146 |  2986 | `case PH7_OP_CVT_STR:` |
|        - |  2987 | `#ifdef UNTRUST` |
|        - |  2988 | `	if( pTos < pStack ){` |
|        - |  2989 | `		goto Abort;` |
|        - |  2990 | `	}` |
|        - |  2991 | `#endif` |
|      294 |  2992 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2993 | `		PH7_MemObjToString(pTos);` |
|      146 |  2994 | `	}` |
|      294 |  2995 | `	break;` |
|        - |  2996 | `/*` |
|        - |  2997 | ` * CVT_BOOL: * * *` |
|        - |  2998 | ` *` |
|        - |  2999 | ` * Force the top of the stack to be a boolean.` |
|        - |  3000 | ` */` |
|        5 |  3001 | `case PH7_OP_CVT_BOOL:` |
|        - |  3002 | `#ifdef UNTRUST` |
|        - |  3003 | `	if( pTos < pStack ){` |
|        - |  3004 | `		goto Abort;` |
|        - |  3005 | `	}` |
|        - |  3006 | `#endif` |
|       11 |  3007 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3008 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3009 | `	}` |
|       11 |  3010 | `	break;` |
|        - |  3011 | `/*` |
|        - |  3012 | ` * CVT_NULL: * * *` |
|        - |  3013 | ` *` |
|        - |  3014 | ` * Nullify the top of the stack.` |
|        - |  3015 | ` */` |
|        3 |  3016 | `case PH7_OP_CVT_NULL:` |
|        - |  3017 | `#ifdef UNTRUST` |
|        - |  3018 | `	if( pTos < pStack ){` |
|        - |  3019 | `		goto Abort;` |
|        - |  3020 | `	}` |
|        - |  3021 | `#endif` |
|        7 |  3022 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3023 | `	break;` |
|        - |  3024 | `/*` |
|        - |  3025 | ` * CVT_NUMC: * * *` |
|        - |  3026 | ` *` |
|        - |  3027 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3028 | ` */` |
|      ! 0 |  3029 | `case PH7_OP_CVT_NUMC:` |
|        - |  3030 | `#ifdef UNTRUST` |
|        - |  3031 | `	if( pTos < pStack ){` |
|        - |  3032 | `		goto Abort;` |
|        - |  3033 | `	}` |
|        - |  3034 | `#endif` |
|        - |  3035 | `	/* Force a numeric cast */` |
|      ! 0 |  3036 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3037 | `	break;` |
|        - |  3038 | `/*` |
|        - |  3039 | ` * CVT_ARRAY: * * *` |
|        - |  3040 | ` *` |
|        - |  3041 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3042 | ` */` |
|       10 |  3043 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3044 | `#ifdef UNTRUST` |
|        - |  3045 | `	if( pTos < pStack ){` |
|        - |  3046 | `		goto Abort;` |
|        - |  3047 | `	}` |
|        - |  3048 | `#endif` |
|        - |  3049 | `	/* Force a hashmap cast */` |
|       21 |  3050 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3051 | `	if( rc != SXRET_OK ){` |
|        - |  3052 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3053 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3054 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3055 | `	}` |
|       21 |  3056 | `	break;` |
|        - |  3057 | `/*` |
|        - |  3058 | ` * CVT_OBJ: * * *` |
|        - |  3059 | ` *` |
|        - |  3060 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3061 | ` */` |
|        8 |  3062 | `case PH7_OP_CVT_OBJ:` |
|        - |  3063 | `#ifdef UNTRUST` |
|        - |  3064 | `	if( pTos < pStack ){` |
|        - |  3065 | `		goto Abort;` |
|        - |  3066 | `	}` |
|        - |  3067 | `#endif` |
|       17 |  3068 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3069 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3070 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3071 | `	}` |
|       17 |  3072 | `	break;` |
|        - |  3073 | `/*` |
|        - |  3074 | ` * ERR_CTRL * * *` |
|        - |  3075 | ` *` |
|        - |  3076 | ` * Error control operator.` |
|        - |  3077 | ` */` |
|    12749 |  3078 | `case PH7_OP_ERR_CTRL:` |
|        - |  3079 | `	/*` |
|        - |  3080 | `	 * TICKET 1433-038:` |
|        - |  3081 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3082 | `	 * use the public API,to control error output.` |
|        - |  3083 | `	 */` |
|    25498 |  3084 | `	break;` |
|        - |  3085 | `/*` |
|        - |  3086 | ` * IS_A * * *` |
|        - |  3087 | ` *` |
|        - |  3088 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3089 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3090 | ` * holding a class name or an object).` |
|        - |  3091 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3092 | ` */` |
|       23 |  3093 | `case PH7_OP_IS_A:{` |
|       48 |  3094 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3095 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3096 | `#ifdef UNTRUST` |
|        - |  3097 | `	if( pNos < pStack ){` |
|        - |  3098 | `		goto Abort;` |
|        - |  3099 | `	}` |
|        - |  3100 | `#endif` |
|       48 |  3101 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3102 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3103 | `		ph7_class *pClass = 0;` |
|        - |  3104 | `		/* Extract the target class */` |
|       46 |  3105 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3106 | `			/* Instance already loaded */` |
|      ! 0 |  3107 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3108 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3109 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3110 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3111 | `			/* Handle self/static/parent keywords */` |
|       46 |  3112 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3113 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3114 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3115 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3116 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3117 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3118 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3119 | `					pClass = pSelf->pBase;` |
|        2 |  3120 | `				}` |
|        3 |  3121 | `			}else{` |
|       36 |  3122 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3123 | `			}` |
|       22 |  3124 | `		}` |
|       46 |  3125 | `		if( pClass ){` |
|        - |  3126 | `			/* Perform the query */` |
|       46 |  3127 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3128 | `		}` |
|       22 |  3129 | `	}` |
|        - |  3130 | `	/* Push result */` |
|       48 |  3131 | `	VmPopOperand(&pTos,1);` |
|       48 |  3132 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3133 | `	pTos->x.iVal = iRes;` |
|       48 |  3134 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3135 | `	break;` |
|        - |  3136 | `				 }` |
|        - |  3137 |  |
|        - |  3138 | `/*` |
|        - |  3139 | ` * LOADC P1 P2 *` |
|        - |  3140 | ` *` |
|        - |  3141 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3142 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3143 | ` */` |
|   830760 |  3144 | `case PH7_OP_LOADC: {` |
|        - |  3145 | `	ph7_value *pObj;` |
|        - |  3146 | `	/* Reserve a room */` |
|  1661566 |  3147 | `	pTos++;` |
|  2484133 |  3148 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1661566 |  3149 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3150 | `			SyHashEntry *pEntry;` |
|        - |  3151 | `			/* Candidate for expansion via user defined callbacks */` |
|    16478 |  3152 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16478 |  3153 | `			if( pEntry ){` |
|    16474 |  3154 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3155 | `				/* Set a NULL default value */` |
|    16474 |  3156 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16474 |  3157 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3158 | `				/* Invoke the callback and deal with the expanded value */` |
|    16474 |  3159 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3160 | `				/* Mark as constant */` |
|    16474 |  3161 | `				pTos->nIdx = SXU32_HIGH;` |
|    16474 |  3162 | `				break;` |
|        - |  3163 | `			}` |
|        - |  3164 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3165 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3166 | `			 * through to string value for backward compatibility. */` |
|        - |  3167 | `			{` |
|        6 |  3168 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3169 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3170 | `				sxu32 j;` |
|       32 |  3171 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3172 | `					if( zLit[j] == '\\' ){` |
|        - |  3173 | `						/* Qualified name: must be a real constant.` |
|        - |  3174 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3175 | `						{` |
|        3 |  3176 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3177 | `							SyBlob sErr;` |
|        3 |  3178 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3179 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3180 | `							if( pErrFile ){` |
|        3 |  3181 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3182 | `							}` |
|        3 |  3183 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3184 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3185 | `							SyBlobRelease(&sErr);` |
|        - |  3186 | `						}` |
|        3 |  3187 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3188 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3189 | `						goto LoadC_Done;` |
|        - |  3190 | `					}` |
|       15 |  3191 | `				}` |
|        - |  3192 | `			}` |
|        1 |  3193 | `		}` |
|  1645092 |  3194 | `		PH7_MemObjLoad(pObj,pTos);` |
|   822569 |  3195 | `	}else{` |
|        - |  3196 | `		/* Set a NULL value */` |
|      ! 0 |  3197 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3198 | `	}` |
|   822524 |  3199 | `LoadC_Done:` |
|        - |  3200 | `	/* Mark as constant */` |
|  1645094 |  3201 | `	pTos->nIdx = SXU32_HIGH;` |
|  1645094 |  3202 | `	break;` |
|        - |  3203 | `				  }` |
|        - |  3204 | `/*` |
|        - |  3205 | ` * LOAD: P1 * P3` |
|        - |  3206 | ` *` |
|        - |  3207 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3208 | ` * from the P3 operand.` |
|        - |  3209 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3210 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3211 | ` */` |
|  1346068 |  3212 | `case PH7_OP_LOAD:{` |
|        - |  3213 | `	ph7_value *pObj;` |
|        - |  3214 | `	SyString sName;` |
|  2692358 |  3215 | `	if( pInstr->p3 == 0 ){` |
|        - |  3216 | `		/* Take the variable name from the top of the stack */` |
|        - |  3217 | `#ifdef UNTRUST` |
|        - |  3218 | `		if( pTos < pStack ){` |
|        - |  3219 | `			goto Abort;` |
|        - |  3220 | `		}` |
|        - |  3221 | `#endif` |
|        - |  3222 | `		/* Force a string cast */` |
|       19 |  3223 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3224 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3225 | `		}` |
|       19 |  3226 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3227 | `	}else{` |
|  2692340 |  3228 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3229 | `		/* Reserve a room for the target object */` |
|  2692340 |  3230 | `		pTos++;` |
|        - |  3231 | `	}` |
|        - |  3232 | `	/* Extract the requested memory object */` |
|  2692358 |  3233 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2692358 |  3234 | `	if( pObj == 0 ){` |
|       26 |  3235 | `		if( pInstr->iP1 ){` |
|        - |  3236 | `			/* Variable not found,load NULL */` |
|       26 |  3237 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3238 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3239 | `			}else{` |
|       26 |  3240 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3241 | `			}` |
|       26 |  3242 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1346082 |  3243 | `			break;` |
|      ! 0 |  3244 | `		}else{` |
|        - |  3245 | `			/* Fatal error */` |
|      ! 0 |  3246 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3247 | `			goto Abort;` |
|        - |  3248 | `		}` |
|        - |  3249 | `	}` |
|        - |  3250 | `	/* Load variable contents */` |
|  2692334 |  3251 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2692334 |  3252 | `	pTos->nIdx = pObj->nIdx;` |
|  2692334 |  3253 | `	break;` |
|        - |  3254 | `				   }` |
|        - |  3255 | `/*` |
|        - |  3256 | ` * LOAD_MAP P1 * *` |
|        - |  3257 | ` *` |
|        - |  3258 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3259 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3260 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3261 | ` */` |
|    18453 |  3262 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3263 | `	ph7_hashmap *pMap;` |
|        - |  3264 | `	/* Allocate a new hashmap instance */` |
|    36908 |  3265 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36908 |  3266 | `	if( pMap == 0 ){` |
|      ! 0 |  3267 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3268 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3269 | `		goto Abort;` |
|        - |  3270 | `	}` |
|    36908 |  3271 | `	if( pInstr->iP1 > 0 ){` |
|     2240 |  3272 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3273 | `		/* Perform the insertion */` |
|     6846 |  3274 | `		while( pEntry < pTos ){` |
|     4608 |  3275 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3276 | `				/* Insertion by reference */` |
|      142 |  3277 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3278 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3279 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3280 | `					);` |
|       48 |  3281 | `			}else{` |
|        - |  3282 | `				/* Standard insertion */` |
|     6770 |  3283 | `				PH7_HashmapInsert(pMap,` |
|     4512 |  3284 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2256 |  3285 | `					&pEntry[1]` |
|        - |  3286 | `				);` |
|        - |  3287 | `			}` |
|        - |  3288 | `			/* Next pair on the stack */` |
|     4608 |  3289 | `			pEntry += 2;` |
|        2 |  3290 | `		}` |
|        - |  3291 | `		/* Pop P1 elements */` |
|     2240 |  3292 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1119 |  3293 | `	}` |
|        - |  3294 | `	/* Push the hashmap */` |
|    36908 |  3295 | `	pTos++;` |
|    36908 |  3296 | `	pTos->nIdx = SXU32_HIGH;` |
|    36908 |  3297 | `	pTos->x.pOther = pMap;` |
|    36908 |  3298 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36908 |  3299 | `	break;` |
|        - |  3300 | `					  }` |
|        - |  3301 | `/*` |
|        - |  3302 | ` * LOAD_LIST: P1 * *` |
|        - |  3303 | ` *` |
|        - |  3304 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3305 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3306 | ` * Caveats:` |
|        - |  3307 | ` *  This implementation support only a single nesting level.` |
|        - |  3308 | ` */` |
|       26 |  3309 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3310 | `	ph7_value *pEntry;` |
|       54 |  3311 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3312 | `		/* Empty list,break immediately */` |
|      ! 0 |  3313 | `		break;` |
|        - |  3314 | `	}` |
|       54 |  3315 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3316 | `#ifdef UNTRUST` |
|        - |  3317 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3318 | `		goto Abort;` |
|        - |  3319 | `	}` |
|        - |  3320 | `#endif` |
|       54 |  3321 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3322 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3323 | `		ph7_hashmap_node *pNode;` |
|        - |  3324 | `		ph7_value sKey,*pObj;` |
|        - |  3325 | `		/* Start Copying */` |
|       50 |  3326 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3327 | `		while( pEntry <= pTos ){` |
|      106 |  3328 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3329 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3330 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3331 | `					if( rc == SXRET_OK ){` |
|        - |  3332 | `						/* Store node value */` |
|       98 |  3333 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3334 | `					}else{` |
|        - |  3335 | `						/* Nullify the variable */` |
|      ! 0 |  3336 | `						PH7_MemObjRelease(pObj);` |
|        - |  3337 | `					}` |
|       48 |  3338 | `				}` |
|       48 |  3339 | `			}` |
|      106 |  3340 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3341 | `			pEntry++;` |
|        2 |  3342 | `		}` |
|       24 |  3343 | `	}` |
|       54 |  3344 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3345 | `	break;` |
|        - |  3346 | `					   }` |
|        - |  3347 | `/*` |
|        - |  3348 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3349 | ` *` |
|        - |  3350 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3351 | ` * from the stack.` |
|        - |  3352 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3353 | ` * instead.` |
|        - |  3354 | ` */` |
|   216145 |  3355 | `case PH7_OP_LOAD_IDX: {` |
|   432336 |  3356 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   432336 |  3357 | `	ph7_hashmap *pMap = 0;` |
|        - |  3358 | `	ph7_value *pIdx;` |
|   432336 |  3359 | `	pIdx = 0;` |
|   432336 |  3360 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3361 | `		if( !pInstr->iP2){` |
|        - |  3362 | `			/* No available index,load NULL */` |
|      ! 0 |  3363 | `			if( pTos >= pStack ){` |
|      ! 0 |  3364 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3365 | `			}else{` |
|        - |  3366 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3367 | `				pTos++;` |
|      ! 0 |  3368 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3369 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3370 | `			}` |
|        - |  3371 | `			/* Emit a notice */` |
|      ! 0 |  3372 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3373 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3374 | `			break;` |
|        - |  3375 | `		}` |
|      ! 0 |  3376 | `	}else{` |
|   432336 |  3377 | `		pIdx = pTos;` |
|   432336 |  3378 | `		pTos--;` |
|        - |  3379 | `	}` |
|   432336 |  3380 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3381 | `		/* String access */` |
|   340676 |  3382 | `		if( pIdx ){` |
|        - |  3383 | `			sxu32 nOfft;` |
|   340676 |  3384 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3385 | `				/* Force an int cast */` |
|      ! 0 |  3386 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3387 | `			}` |
|   340676 |  3388 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340676 |  3389 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3390 | `				/* Invalid offset,load null */` |
|      ! 0 |  3391 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3392 | `			}else{` |
|   340676 |  3393 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340676 |  3394 | `				int c = zData[nOfft];` |
|   340676 |  3395 | `				PH7_MemObjRelease(pTos);` |
|   340676 |  3396 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340676 |  3397 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3398 | `			}` |
|   170361 |  3399 | `		}else{` |
|        - |  3400 | `			/* No available index,load NULL */` |
|      ! 0 |  3401 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3402 | `		}` |
|   340676 |  3403 | `		break;` |
|        - |  3404 | `	}` |
|    91662 |  3405 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3406 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3407 | `			ph7_value *pObj;` |
|      ! 0 |  3408 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3409 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3410 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3411 | `			}` |
|      ! 0 |  3412 | `		}` |
|      ! 0 |  3413 | `	}` |
|    91662 |  3414 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    91662 |  3415 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    91662 |  3416 | `		if( pInstr->iP2 ){` |
|        - |  3417 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3418 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3419 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3420 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3421 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3422 | `		}` |
|        - |  3423 | `		/* Point to the hashmap */` |
|    91662 |  3424 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    91662 |  3425 | `		if( pIdx ){` |
|        - |  3426 | `			/* Load the desired entry */` |
|    91662 |  3427 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    45830 |  3428 | `		}` |
|    91662 |  3429 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3430 | `			/* Create a new empty entry */` |
|      265 |  3431 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3432 | `			if( rc == SXRET_OK ){` |
|        - |  3433 | `				/* Point to the last inserted entry */` |
|      265 |  3434 | `				pNode = pMap->pLast;` |
|      132 |  3435 | `			}` |
|      132 |  3436 | `		}` |
|    45830 |  3437 | `	}` |
|    91662 |  3438 | `	if( pIdx ){` |
|    91662 |  3439 | `		PH7_MemObjRelease(pIdx);` |
|    45830 |  3440 | `	}` |
|    91662 |  3441 | `	if( rc == SXRET_OK ){` |
|        - |  3442 | `		/* Load entry contents */` |
|    41994 |  3443 | `		if( pMap->iRef < 2 ){` |
|        - |  3444 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3445 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3446 | `			 */` |
|       24 |  3447 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3448 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3449 | `		}else{` |
|    41972 |  3450 | `			pTos->nIdx = pNode->nValIdx;` |
|    41972 |  3451 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41972 |  3452 | `			PH7_HashmapUnref(pMap);` |
|        - |  3453 | `		}` |
|    20998 |  3454 | `	}else{` |
|        - |  3455 | `		/* No such entry,load NULL */` |
|    49670 |  3456 | `		PH7_MemObjRelease(pTos);` |
|    49670 |  3457 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3458 | `	}` |
|    91662 |  3459 | `	break;` |
|        - |  3460 | `					  }` |
|        - |  3461 | `/*` |
|        - |  3462 | ` * LOAD_CLOSURE * * P3` |
|        - |  3463 | ` *` |
|        - |  3464 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3465 | ` * name in the stack.` |
|        - |  3466 | ` */` |
|        4 |  3467 | `case PH7_OP_LOAD_CLOSURE:{` |
|       10 |  3468 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       10 |  3469 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3470 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3471 | `		ph7_vm_func *pClosure;` |
|        - |  3472 | `		char *zName;` |
|        - |  3473 | `		sxu32 mLen;` |
|        - |  3474 | `		sxu32 n;` |
|        - |  3475 | `		/* Create a new VM function */` |
|       10 |  3476 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3477 | `		/* Generate an unique closure name */` |
|       10 |  3478 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       10 |  3479 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3480 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3481 | `			goto Abort;` |
|        - |  3482 | `		}` |
|       10 |  3483 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       10 |  3484 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3485 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3486 | `		}` |
|        - |  3487 | `		/* Zero the stucture */` |
|       10 |  3488 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3489 | `		/* Perform a structure assignment on read-only items */` |
|       10 |  3490 | `		pClosure->aArgs = pFunc->aArgs;` |
|       10 |  3491 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       10 |  3492 | `		pClosure->aStatic = pFunc->aStatic;` |
|       10 |  3493 | `		pClosure->iFlags = pFunc->iFlags;` |
|       10 |  3494 | `		pClosure->pUserData = pFunc->pUserData;` |
|       10 |  3495 | `		pClosure->sSignature = pFunc->sSignature;` |
|       10 |  3496 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       10 |  3497 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       10 |  3498 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3499 | `		/* Register the closure */` |
|       10 |  3500 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3501 | `		/* Set up closure environment */` |
|       10 |  3502 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       10 |  3503 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       28 |  3504 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3505 | `			ph7_value *pValue;` |
|       20 |  3506 | `			pEnv = &aEnv[n];` |
|       20 |  3507 | `			sEnv.sName  = pEnv->sName;` |
|       20 |  3508 | `			sEnv.iFlags = pEnv->iFlags;` |
|       20 |  3509 | `			sEnv.nIdx = SXU32_HIGH;` |
|       20 |  3510 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       20 |  3511 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3512 | `				/* Pass by reference */` |
|      ! 0 |  3513 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3514 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3515 | `					);` |
|      ! 0 |  3516 | `			}` |
|        - |  3517 | `			/* Standard pass by value */` |
|       20 |  3518 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       20 |  3519 | `			if( pValue ){` |
|        - |  3520 | `				/* Copy imported value */` |
|       12 |  3521 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3522 | `			}` |
|        - |  3523 | `			/* Insert the imported variable */` |
|       20 |  3524 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       11 |  3525 | `		}` |
|        - |  3526 | `		/* Finally,load the closure name on the stack */` |
|       10 |  3527 | `		pTos++;` |
|       10 |  3528 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3529 | `	}` |
|       10 |  3530 | `	break;` |
|        - |  3531 | `						 }` |
|        - |  3532 | `/*` |
|        - |  3533 | ` * STORE * P2 P3` |
|        - |  3534 | ` *` |
|        - |  3535 | ` * Perform a store (Assignment) operation.` |
|        - |  3536 | ` */` |
|   114256 |  3537 | `case PH7_OP_STORE: {` |
|        - |  3538 | `	ph7_value *pObj;` |
|        - |  3539 | `	SyString sName;` |
|        - |  3540 | `#ifdef UNTRUST` |
|        - |  3541 | `	if( pTos < pStack ){` |
|        - |  3542 | `		goto Abort;` |
|        - |  3543 | `	}` |
|        - |  3544 | `#endif` |
|   228514 |  3545 | `	if( pInstr->iP2 ){` |
|        - |  3546 | `		sxu32 nIdx;` |
|        - |  3547 | `		/* Member store operation */` |
|     2954 |  3548 | `		nIdx = pTos->nIdx;` |
|     2954 |  3549 | `		VmPopOperand(&pTos,1);` |
|     2954 |  3550 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3551 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3552 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3553 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3554 | `		}else{` |
|        - |  3555 | `			/* Point to the desired memory object */` |
|     2950 |  3556 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2950 |  3557 | `			if( pObj ){` |
|        - |  3558 | `				/* Perform the store operation */` |
|     2950 |  3559 | `				PH7_MemObjStore(pTos,pObj);` |
|     1474 |  3560 | `			}` |
|        - |  3561 | `		}` |
|   115734 |  3562 | `		break;` |
|   225562 |  3563 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3564 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3565 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3566 | `			/* Force a string cast */` |
|      ! 0 |  3567 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3568 | `		}` |
|        7 |  3569 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3570 | `		pTos--;` |
|        - |  3571 | `#ifdef UNTRUST` |
|        - |  3572 | `		if( pTos < pStack  ){` |
|        - |  3573 | `			goto Abort;` |
|        - |  3574 | `		}` |
|        - |  3575 | `#endif` |
|        4 |  3576 | `	}else{` |
|   225556 |  3577 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3578 | `	}` |
|        - |  3579 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   225562 |  3580 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   225562 |  3581 | `	if( pObj == 0 ){` |
|      ! 0 |  3582 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3583 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3584 | `		goto Abort;` |
|        - |  3585 | `	}` |
|   225562 |  3586 | `	if( !pInstr->p3 ){` |
|        7 |  3587 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3588 | `	}` |
|        - |  3589 | `	/* Perform the store operation */` |
|   225562 |  3590 | `	PH7_MemObjStore(pTos,pObj);` |
|   225562 |  3591 | `	break;` |
|        - |  3592 | `				   }` |
|        - |  3593 | `/*` |
|        - |  3594 | ` * STORE_IDX:   P1 * P3` |
|        - |  3595 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3596 | ` *` |
|        - |  3597 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3598 | ` */` |
|    82169 |  3599 | `case PH7_OP_STORE_IDX:` |
|        - |  3600 | `case PH7_OP_STORE_IDX_REF: {` |
|   164340 |  3601 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3602 | `	ph7_value *pKey;` |
|        - |  3603 | `	sxu32 nIdx;` |
|   164340 |  3604 | `	if( pInstr->iP1 ){` |
|        - |  3605 | `		/* Key is next on stack */` |
|    57572 |  3606 | `		pKey = pTos;` |
|    57572 |  3607 | `		pTos--;` |
|    28787 |  3608 | `	}else{` |
|   106770 |  3609 | `		pKey = 0;` |
|        - |  3610 | `	}` |
|   164340 |  3611 | `	nIdx = pTos->nIdx;` |
|   164340 |  3612 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3613 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3614 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3615 | `		 * checking true sharing count, then re-add after separation. */` |
|   164288 |  3616 | `		if( nIdx != SXU32_HIGH ){` |
|   164288 |  3617 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   246431 |  3618 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   164288 |  3619 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3620 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3621 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3622 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3623 | `				 * refcounts if the backing array was already separated. */` |
|   164288 |  3624 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   164288 |  3625 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   164288 |  3626 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   164288 |  3627 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   164288 |  3628 | `					pTos->x.pOther = pMap;` |
|    82145 |  3629 | `				}else{` |
|        - |  3630 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3631 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3632 | `					pMap = pCur;` |
|        - |  3633 | `				}` |
|    82145 |  3634 | `			}else{` |
|      ! 0 |  3635 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3636 | `			}` |
|    82145 |  3637 | `		}else{` |
|      ! 0 |  3638 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3639 | `		}` |
|   164288 |  3640 | `		if( pMap->iRef < 2 ){` |
|        - |  3641 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3642 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3643 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3644 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3645 | `			pMap->iRef = 2;` |
|      ! 0 |  3646 | `		}` |
|    82145 |  3647 | `	}else{` |
|        - |  3648 | `		ph7_value *pObj;` |
|       53 |  3649 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3650 | `		if( pObj == 0 ){` |
|      ! 0 |  3651 | `			if( pKey ){` |
|      ! 0 |  3652 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3653 | `			}` |
|      ! 0 |  3654 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3655 | `			break;` |
|        - |  3656 | `		}` |
|        - |  3657 | `		/* Phase#1: Load the array */` |
|       53 |  3658 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3659 | `			VmPopOperand(&pTos,1);` |
|       53 |  3660 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3661 | `				/* Force a string cast */` |
|      ! 0 |  3662 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3663 | `			}` |
|       53 |  3664 | `			if( pKey == 0 ){` |
|        - |  3665 | `				/* Append string */` |
|        3 |  3666 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3667 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3668 | `				}` |
|        2 |  3669 | `			}else{` |
|        - |  3670 | `				sxu32 nOfft;` |
|       51 |  3671 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3672 | `					/* Force an int cast */` |
|       51 |  3673 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3674 | `				}` |
|       51 |  3675 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3676 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3677 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3678 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3679 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3680 | `				}else{` |
|      ! 0 |  3681 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3682 | `						/* Perform an append operation */` |
|      ! 0 |  3683 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3684 | `					}` |
|        - |  3685 | `				}` |
|        - |  3686 | `			}` |
|       53 |  3687 | `			if( pKey ){` |
|       51 |  3688 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3689 | `			}` |
|       53 |  3690 | `			break;` |
|      ! 0 |  3691 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3692 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3693 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3694 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3695 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3696 | `				goto Abort;` |
|        - |  3697 | `			}` |
|      ! 0 |  3698 | `		}` |
|        - |  3699 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3700 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3701 | `	}` |
|   164288 |  3702 | `	VmPopOperand(&pTos,1);` |
|        - |  3703 | `	/* Phase#2: Perform the insertion */` |
|   164288 |  3704 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3705 | `		/* Insertion by reference */` |
|       15 |  3706 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3707 | `	}else{` |
|   164274 |  3708 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3709 | `	}` |
|   164288 |  3710 | `	if( pKey ){` |
|    57522 |  3711 | `		PH7_MemObjRelease(pKey);` |
|    28760 |  3712 | `	}` |
|   164288 |  3713 | `	break;` |
|        - |  3714 | `					   }` |
|        - |  3715 | `/*` |
|        - |  3716 | ` * INCR: P1 * *` |
|        - |  3717 | ` *` |
|        - |  3718 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3719 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3720 | ` * the stack and increment after that.` |
|        - |  3721 | ` */` |
|   151336 |  3722 | `case PH7_OP_INCR:` |
|        - |  3723 | `#ifdef UNTRUST` |
|        - |  3724 | `	if( pTos < pStack ){` |
|        - |  3725 | `		goto Abort;` |
|        - |  3726 | `	}` |
|        - |  3727 | `#endif` |
|   302718 |  3728 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302718 |  3729 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3730 | `			ph7_value *pObj;` |
|   302718 |  3731 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3732 | `				/* Force a numeric cast */` |
|   302718 |  3733 | `				PH7_MemObjToNumeric(pObj);` |
|   302718 |  3734 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3735 | `					pObj->rVal++;` |
|        - |  3736 | `					/* Try to get an integer representation */` |
|      ! 0 |  3737 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3738 | `				}else{` |
|   302718 |  3739 | `					pObj->x.iVal++;` |
|   302718 |  3740 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3741 | `				}` |
|   302718 |  3742 | `				if( pInstr->iP1 ){` |
|        - |  3743 | `					/* Pre-icrement */` |
|       71 |  3744 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3745 | `				}` |
|   151380 |  3746 | `			}` |
|   151382 |  3747 | `		}else{` |
|      ! 0 |  3748 | `			if( pInstr->iP1 ){` |
|        - |  3749 | `				/* Force a numeric cast */` |
|      ! 0 |  3750 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3751 | `				/* Pre-increment */` |
|      ! 0 |  3752 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3753 | `					pTos->rVal++;` |
|        - |  3754 | `					/* Try to get an integer representation */` |
|      ! 0 |  3755 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3756 | `				}else{` |
|      ! 0 |  3757 | `					pTos->x.iVal++;` |
|      ! 0 |  3758 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3759 | `				}` |
|      ! 0 |  3760 | `			}` |
|        - |  3761 | `		}` |
|   151380 |  3762 | `	}` |
|   302718 |  3763 | `	break;` |
|        - |  3764 | `/*` |
|        - |  3765 | ` * DECR: P1 * *` |
|        - |  3766 | ` *` |
|        - |  3767 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3768 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3769 | ` * and decrement after that.` |
|        - |  3770 | ` */` |
|        2 |  3771 | `case PH7_OP_DECR:` |
|        - |  3772 | `#ifdef UNTRUST` |
|        - |  3773 | `	if( pTos < pStack ){` |
|        - |  3774 | `		goto Abort;` |
|        - |  3775 | `	}` |
|        - |  3776 | `#endif` |
|        5 |  3777 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3778 | `		/* Force a numeric cast */` |
|        5 |  3779 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3780 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3781 | `			ph7_value *pObj;` |
|        5 |  3782 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3783 | `				/* Force a numeric cast */` |
|        5 |  3784 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3785 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3786 | `					pObj->rVal--;` |
|        - |  3787 | `					/* Try to get an integer representation */` |
|      ! 0 |  3788 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3789 | `				}else{` |
|        5 |  3790 | `					pObj->x.iVal--;` |
|        5 |  3791 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3792 | `				}` |
|        5 |  3793 | `				if( pInstr->iP1 ){` |
|        - |  3794 | `					/* Pre-icrement */` |
|      ! 0 |  3795 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3796 | `				}` |
|        2 |  3797 | `			}` |
|        3 |  3798 | `		}else{` |
|      ! 0 |  3799 | `			if( pInstr->iP1 ){` |
|        - |  3800 | `				/* Pre-increment */` |
|      ! 0 |  3801 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3802 | `					pTos->rVal--;` |
|        - |  3803 | `					/* Try to get an integer representation */` |
|      ! 0 |  3804 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3805 | `				}else{` |
|      ! 0 |  3806 | `					pTos->x.iVal--;` |
|      ! 0 |  3807 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3808 | `				}` |
|      ! 0 |  3809 | `			}` |
|        - |  3810 | `		}` |
|        2 |  3811 | `	}` |
|        5 |  3812 | `	break;` |
|        - |  3813 | `/*` |
|        - |  3814 | ` * UMINUS: * * *` |
|        - |  3815 | ` *` |
|        - |  3816 | ` * Perform a unary minus operation.` |
|        - |  3817 | ` */` |
|    23875 |  3818 | `case PH7_OP_UMINUS:` |
|        - |  3819 | `#ifdef UNTRUST` |
|        - |  3820 | `	if( pTos < pStack ){` |
|        - |  3821 | `		goto Abort;` |
|        - |  3822 | `	}` |
|        - |  3823 | `#endif` |
|        - |  3824 | `	/* Force a numeric (integer,real or both) cast */` |
|    47752 |  3825 | `	PH7_MemObjToNumeric(pTos);` |
|    47752 |  3826 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3827 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3828 | `	}` |
|    47752 |  3829 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    47722 |  3830 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23860 |  3831 | `	}` |
|    47752 |  3832 | `	break;` |
|        - |  3833 | `/*` |
|        - |  3834 | ` * UPLUS: * * *` |
|        - |  3835 | ` *` |
|        - |  3836 | ` * Perform a unary plus operation.` |
|        - |  3837 | ` */` |
|       16 |  3838 | `case PH7_OP_UPLUS:` |
|        - |  3839 | `#ifdef UNTRUST` |
|        - |  3840 | `	if( pTos < pStack ){` |
|        - |  3841 | `		goto Abort;` |
|        - |  3842 | `	}` |
|        - |  3843 | `#endif` |
|        - |  3844 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3845 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3846 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3847 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3848 | `	}` |
|       33 |  3849 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3850 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3851 | `	}` |
|       33 |  3852 | `	break;` |
|        - |  3853 | `/*` |
|        - |  3854 | ` * OP_LNOT: * * *` |
|        - |  3855 | ` *` |
|        - |  3856 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3857 | ` * with its complement.` |
|        - |  3858 | ` */` |
|    40159 |  3859 | `case PH7_OP_LNOT:` |
|        - |  3860 | `#ifdef UNTRUST` |
|        - |  3861 | `	if( pTos < pStack ){` |
|        - |  3862 | `		goto Abort;` |
|        - |  3863 | `	}` |
|        - |  3864 | `#endif` |
|        - |  3865 | `	/* Force a boolean cast */` |
|    80364 |  3866 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3867 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3868 | `	}` |
|    80364 |  3869 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80364 |  3870 | `	break;` |
|        - |  3871 | `/*` |
|        - |  3872 | ` * OP_BITNOT: * * *` |
|        - |  3873 | ` *` |
|        - |  3874 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3875 | ` * with its ones-complement.` |
|        - |  3876 | ` */` |
|       14 |  3877 | `case PH7_OP_BITNOT:` |
|        - |  3878 | `#ifdef UNTRUST` |
|        - |  3879 | `	if( pTos < pStack ){` |
|        - |  3880 | `		goto Abort;` |
|        - |  3881 | `	}` |
|        - |  3882 | `#endif` |
|        - |  3883 | `	/* Force an integer cast */` |
|       30 |  3884 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3885 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3886 | `	}` |
|       30 |  3887 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3888 | `	break;` |
|        - |  3889 | `/* OP_MUL * * *` |
|        - |  3890 | ` * OP_MUL_STORE * * *` |
|        - |  3891 | ` *` |
|        - |  3892 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3893 | ` * and push the result back onto the stack.` |
|        - |  3894 | ` */` |
|     1247 |  3895 | `case PH7_OP_MUL:` |
|        - |  3896 | `case PH7_OP_MUL_STORE: {` |
|     2496 |  3897 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3898 | `	/* Force the operand to be numeric */` |
|        - |  3899 | `#ifdef UNTRUST` |
|        - |  3900 | `	if( pNos < pStack ){` |
|        - |  3901 | `		goto Abort;` |
|        - |  3902 | `	}` |
|        - |  3903 | `#endif` |
|     2496 |  3904 | `	PH7_MemObjToNumeric(pTos);` |
|     2496 |  3905 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3906 | `	/* Perform the requested operation */` |
|     2496 |  3907 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3908 | `		/* Floating point arithemic */` |
|        - |  3909 | `		ph7_real a,b,r;` |
|       17 |  3910 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3911 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3912 | `		}` |
|       17 |  3913 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3914 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3915 | `		}` |
|       17 |  3916 | `		a = pNos->rVal;` |
|       17 |  3917 | `		b = pTos->rVal;` |
|       17 |  3918 | `		r = a * b;` |
|        - |  3919 | `		/* Push the result */` |
|       17 |  3920 | `		pNos->rVal = r;` |
|       17 |  3921 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3922 | `		/* Try to get an integer representation */` |
|       17 |  3923 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3924 | `	}else{` |
|        - |  3925 | `		/* Integer arithmetic */` |
|        - |  3926 | `		sxi64 a,b,r;` |
|     2480 |  3927 | `		a = pNos->x.iVal;` |
|     2480 |  3928 | `		b = pTos->x.iVal;` |
|     2480 |  3929 | `		r = a * b;` |
|        - |  3930 | `		/* Push the result */` |
|     2480 |  3931 | `		pNos->x.iVal = r;` |
|     2480 |  3932 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3933 | `	}` |
|     2496 |  3934 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3935 | `		ph7_value *pObj;` |
|       25 |  3936 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3937 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3938 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3939 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3940 | `		}` |
|       12 |  3941 | `	}` |
|     2496 |  3942 | `	VmPopOperand(&pTos,1);` |
|     2496 |  3943 | `	break;` |
|        - |  3944 | `				 }` |
|        - |  3945 | `/* OP_ADD * * *` |
|        - |  3946 | ` *` |
|        - |  3947 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3948 | ` * and push the result back onto the stack.` |
|        - |  3949 | ` */` |
|      439 |  3950 | `case PH7_OP_ADD:{` |
|      880 |  3951 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3952 | `#ifdef UNTRUST` |
|        - |  3953 | `	if( pNos < pStack ){` |
|        - |  3954 | `		goto Abort;` |
|        - |  3955 | `	}` |
|        - |  3956 | `#endif` |
|        - |  3957 | `	/* Perform the addition */` |
|      880 |  3958 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      880 |  3959 | `	VmPopOperand(&pTos,1);` |
|      880 |  3960 | `	break;` |
|        - |  3961 | `				}` |
|        - |  3962 | `/*` |
|        - |  3963 | ` * OP_ADD_STORE * * *` |
|        - |  3964 | ` *` |
|        - |  3965 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3966 | ` * and push the result back onto the stack.` |
|        - |  3967 | ` */` |
|      483 |  3968 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3969 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3970 | `	ph7_value *pObj;` |
|        - |  3971 | `	sxu32 nIdx;` |
|        - |  3972 | `#ifdef UNTRUST` |
|        - |  3973 | `	if( pNos < pStack ){` |
|        - |  3974 | `		goto Abort;` |
|        - |  3975 | `	}` |
|        - |  3976 | `#endif` |
|        - |  3977 | `	/* Perform the addition */` |
|      968 |  3978 | `	nIdx = pTos->nIdx;` |
|      968 |  3979 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3980 | `	/* Peform the store operation */` |
|      968 |  3981 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3982 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3983 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3984 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3985 | `	}` |
|        - |  3986 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3987 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3988 | `	VmPopOperand(&pTos,1);` |
|      968 |  3989 | `	break;` |
|        - |  3990 | `				}` |
|        - |  3991 | `/* OP_SUB * * *` |
|        - |  3992 | ` *` |
|        - |  3993 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3994 | ` * first (what was next on the stack) from the second (the` |
|        - |  3995 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3996 | ` */` |
|      299 |  3997 | `case PH7_OP_SUB: {` |
|      600 |  3998 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3999 | `#ifdef UNTRUST` |
|        - |  4000 | `	if( pNos < pStack ){` |
|        - |  4001 | `		goto Abort;` |
|        - |  4002 | `	}` |
|        - |  4003 | `#endif` |
|      600 |  4004 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4005 | `		/* Floating point arithemic */` |
|        - |  4006 | `		ph7_real a,b,r;` |
|       95 |  4007 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4008 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4009 | `		}` |
|       95 |  4010 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4011 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4012 | `		}` |
|       95 |  4013 | `		a = pNos->rVal;` |
|       95 |  4014 | `		b = pTos->rVal;` |
|       95 |  4015 | `		r = a - b;` |
|        - |  4016 | `		/* Push the result */` |
|       95 |  4017 | `		pNos->rVal = r;` |
|       95 |  4018 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4019 | `		/* Try to get an integer representation */` |
|       95 |  4020 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4021 | `	}else{` |
|        - |  4022 | `		/* Integer arithmetic */` |
|        - |  4023 | `		sxi64 a,b,r;` |
|      506 |  4024 | `		a = pNos->x.iVal;` |
|      506 |  4025 | `		b = pTos->x.iVal;` |
|      506 |  4026 | `		r = a - b;` |
|        - |  4027 | `		/* Push the result */` |
|      506 |  4028 | `		pNos->x.iVal = r;` |
|      506 |  4029 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4030 | `	}` |
|      600 |  4031 | `	VmPopOperand(&pTos,1);` |
|      600 |  4032 | `	break;` |
|        - |  4033 | `				 }` |
|        - |  4034 | `/* OP_SUB_STORE * * *` |
|        - |  4035 | ` *` |
|        - |  4036 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4037 | ` * first (what was next on the stack) from the second (the` |
|        - |  4038 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4039 | ` */` |
|        1 |  4040 | `case PH7_OP_SUB_STORE: {` |
|        3 |  4041 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4042 | `	ph7_value *pObj;` |
|        - |  4043 | `#ifdef UNTRUST` |
|        - |  4044 | `	if( pNos < pStack ){` |
|        - |  4045 | `		goto Abort;` |
|        - |  4046 | `	}` |
|        - |  4047 | `#endif` |
|        3 |  4048 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4049 | `		/* Floating point arithemic */` |
|        - |  4050 | `		ph7_real a,b,r;` |
|      ! 0 |  4051 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4052 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4053 | `		}` |
|      ! 0 |  4054 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4055 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4056 | `		}` |
|      ! 0 |  4057 | `		a = pTos->rVal;` |
|      ! 0 |  4058 | `		b = pNos->rVal;` |
|      ! 0 |  4059 | `		r = a - b;` |
|        - |  4060 | `		/* Push the result */` |
|      ! 0 |  4061 | `		pNos->rVal = r;` |
|      ! 0 |  4062 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4063 | `		/* Try to get an integer representation */` |
|      ! 0 |  4064 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4065 | `	}else{` |
|        - |  4066 | `		/* Integer arithmetic */` |
|        - |  4067 | `		sxi64 a,b,r;` |
|        3 |  4068 | `		a = pTos->x.iVal;` |
|        3 |  4069 | `		b = pNos->x.iVal;` |
|        3 |  4070 | `		r = a - b;` |
|        - |  4071 | `		/* Push the result */` |
|        3 |  4072 | `		pNos->x.iVal = r;` |
|        3 |  4073 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4074 | `	}` |
|        3 |  4075 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4076 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4077 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4078 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4079 | `	}` |
|        3 |  4080 | `	VmPopOperand(&pTos,1);` |
|        3 |  4081 | `	break;` |
|        - |  4082 | `				 }` |
|        - |  4083 |  |
|        - |  4084 | `/*` |
|        - |  4085 | ` * OP_MOD * * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4088 | ` * first (what was next on the stack) from the second (the` |
|        - |  4089 | ` * top of the stack) and push the remainder after division` |
|        - |  4090 | ` * onto the stack.` |
|        - |  4091 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4092 | ` */` |
|      305 |  4093 | `case PH7_OP_MOD:{` |
|      612 |  4094 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4095 | `	sxi64 a,b,r;` |
|        - |  4096 | `#ifdef UNTRUST` |
|        - |  4097 | `	if( pNos < pStack ){` |
|        - |  4098 | `		goto Abort;` |
|        - |  4099 | `	}` |
|        - |  4100 | `#endif` |
|        - |  4101 | `	/* Force the operands to be integer */` |
|      612 |  4102 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4103 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4104 | `	}` |
|      612 |  4105 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4106 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4107 | `	}` |
|        - |  4108 | `	/* Perform the requested operation */` |
|      612 |  4109 | `	a = pNos->x.iVal;` |
|      612 |  4110 | `	b = pTos->x.iVal;` |
|      612 |  4111 | `	if( b == 0 ){` |
|        3 |  4112 | `		r = 0;` |
|        3 |  4113 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4114 | `		/* goto Abort; */` |
|        2 |  4115 | `	}else{` |
|      609 |  4116 | `		r = a%b;` |
|        - |  4117 | `	}` |
|        - |  4118 | `	/* Push the result */` |
|      612 |  4119 | `	pNos->x.iVal = r;` |
|      612 |  4120 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      612 |  4121 | `	VmPopOperand(&pTos,1);` |
|      612 |  4122 | `	break;` |
|        - |  4123 | `				}` |
|        - |  4124 | `/*` |
|        - |  4125 | ` * OP_MOD_STORE * * *` |
|        - |  4126 | ` *` |
|        - |  4127 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4128 | ` * first (what was next on the stack) from the second (the` |
|        - |  4129 | ` * top of the stack) and push the remainder after division` |
|        - |  4130 | ` * onto the stack.` |
|        - |  4131 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4132 | ` */` |
|        1 |  4133 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4134 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4135 | `	ph7_value *pObj;` |
|        - |  4136 | `	sxi64 a,b,r;` |
|        - |  4137 | `#ifdef UNTRUST` |
|        - |  4138 | `	if( pNos < pStack ){` |
|        - |  4139 | `		goto Abort;` |
|        - |  4140 | `	}` |
|        - |  4141 | `#endif` |
|        - |  4142 | `	/* Force the operands to be integer */` |
|        3 |  4143 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4144 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4145 | `	}` |
|        3 |  4146 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4147 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4148 | `	}` |
|        - |  4149 | `	/* Perform the requested operation */` |
|        3 |  4150 | `	a = pTos->x.iVal;` |
|        3 |  4151 | `	b = pNos->x.iVal;` |
|        3 |  4152 | `	if( b == 0 ){` |
|      ! 0 |  4153 | `		r = 0;` |
|      ! 0 |  4154 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4155 | `		/* goto Abort; */` |
|      ! 0 |  4156 | `	}else{` |
|        3 |  4157 | `		r = a%b;` |
|        - |  4158 | `	}` |
|        - |  4159 | `	/* Push the result */` |
|        3 |  4160 | `	pNos->x.iVal = r;` |
|        3 |  4161 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4162 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4163 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4164 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4165 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4166 | `	}` |
|        3 |  4167 | `	VmPopOperand(&pTos,1);` |
|        3 |  4168 | `	break;` |
|        - |  4169 | `				}` |
|        - |  4170 | `/*` |
|        - |  4171 | ` * OP_DIV * * *` |
|        - |  4172 | ` *` |
|        - |  4173 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4174 | ` * first (what was next on the stack) from the second (the` |
|        - |  4175 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4176 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4177 | ` */` |
|       28 |  4178 | `case PH7_OP_DIV:{` |
|       58 |  4179 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4180 | `	ph7_real a,b,r;` |
|        - |  4181 | `#ifdef UNTRUST` |
|        - |  4182 | `	if( pNos < pStack ){` |
|        - |  4183 | `		goto Abort;` |
|        - |  4184 | `	}` |
|        - |  4185 | `#endif` |
|        - |  4186 | `	/* Force the operands to be real */` |
|       58 |  4187 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4188 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4189 | `	}` |
|       58 |  4190 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4191 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4192 | `	}` |
|        - |  4193 | `	/* Perform the requested operation */` |
|       58 |  4194 | `	a = pNos->rVal;` |
|       58 |  4195 | `	b = pTos->rVal;` |
|       58 |  4196 | `	if( b == 0 ){` |
|        - |  4197 | `		/* Division by zero */` |
|        3 |  4198 | `		pNos->rVal = 0;` |
|        3 |  4199 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4200 | `		/* goto Abort; */` |
|        2 |  4201 | `	}else{` |
|       55 |  4202 | `		r = a/b;` |
|        - |  4203 | `		/* Push the result */` |
|       55 |  4204 | `		pNos->rVal = r;` |
|       55 |  4205 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4206 | `		/* Try to get an integer representation */` |
|       55 |  4207 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4208 | `	}` |
|       58 |  4209 | `	VmPopOperand(&pTos,1);` |
|       58 |  4210 | `	break;` |
|        - |  4211 | `				}` |
|        - |  4212 | `/*` |
|        - |  4213 | ` * OP_DIV_STORE * * *` |
|        - |  4214 | ` *` |
|        - |  4215 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4216 | ` * first (what was next on the stack) from the second (the` |
|        - |  4217 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4218 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4219 | ` */` |
|        1 |  4220 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4221 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4222 | `	ph7_value *pObj;` |
|        - |  4223 | `	ph7_real a,b,r;` |
|        - |  4224 | `#ifdef UNTRUST` |
|        - |  4225 | `	if( pNos < pStack ){` |
|        - |  4226 | `		goto Abort;` |
|        - |  4227 | `	}` |
|        - |  4228 | `#endif` |
|        - |  4229 | `	/* Force the operands to be real */` |
|        3 |  4230 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4231 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4232 | `	}` |
|        3 |  4233 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4234 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4235 | `	}` |
|        - |  4236 | `	/* Perform the requested operation */` |
|        3 |  4237 | `	a = pTos->rVal;` |
|        3 |  4238 | `	b = pNos->rVal;` |
|        3 |  4239 | `	if( b == 0 ){` |
|        - |  4240 | `		/* Division by zero */` |
|      ! 0 |  4241 | `		r = 0;` |
|      ! 0 |  4242 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4243 | `		/* goto Abort; */` |
|      ! 0 |  4244 | `	}else{` |
|        3 |  4245 | `		r = a/b;` |
|        - |  4246 | `		/* Push the result */` |
|        3 |  4247 | `		pNos->rVal = r;` |
|        3 |  4248 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4249 | `		/* Try to get an integer representation */` |
|        3 |  4250 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4251 | `	}` |
|        3 |  4252 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4253 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4254 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4255 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4256 | `	}` |
|        3 |  4257 | `	VmPopOperand(&pTos,1);` |
|        3 |  4258 | `	break;` |
|        - |  4259 | `				}` |
|        - |  4260 | `/* OP_BAND * * *` |
|        - |  4261 | ` *` |
|        - |  4262 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4263 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4264 | ` * two elements.` |
|        - |  4265 | `*/` |
|        - |  4266 | `/* OP_BOR * * *` |
|        - |  4267 | ` *` |
|        - |  4268 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4269 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4270 | ` * two elements.` |
|        - |  4271 | ` */` |
|        - |  4272 | `/* OP_BXOR * * *` |
|        - |  4273 | ` *` |
|        - |  4274 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4275 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4276 | ` * two elements.` |
|        - |  4277 | ` */` |
|       30 |  4278 | `case PH7_OP_BAND:` |
|        - |  4279 | `case PH7_OP_BOR:` |
|        - |  4280 | `case PH7_OP_BXOR:{` |
|       62 |  4281 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4282 | `	sxi64 a,b,r;` |
|        - |  4283 | `#ifdef UNTRUST` |
|        - |  4284 | `	if( pNos < pStack ){` |
|        - |  4285 | `		goto Abort;` |
|        - |  4286 | `	}` |
|        - |  4287 | `#endif` |
|        - |  4288 | `	/* Force the operands to be integer */` |
|       62 |  4289 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4290 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4291 | `	}` |
|       62 |  4292 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4293 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4294 | `	}` |
|        - |  4295 | `	/* Perform the requested operation */` |
|       62 |  4296 | `	a = pNos->x.iVal;` |
|       62 |  4297 | `	b = pTos->x.iVal;` |
|       62 |  4298 | `	switch(pInstr->iOp){` |
|        6 |  4299 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4300 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4301 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4302 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4303 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4304 | `	case PH7_OP_BAND:` |
|       38 |  4305 | `	default:          r = a&b; break;` |
|        - |  4306 | `	}` |
|        - |  4307 | `	/* Push the result */` |
|       62 |  4308 | `	pNos->x.iVal = r;` |
|       62 |  4309 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4310 | `	VmPopOperand(&pTos,1);` |
|       62 |  4311 | `	break;` |
|        - |  4312 | `				 }` |
|        - |  4313 | `/* OP_BAND_STORE * * *` |
|        - |  4314 | ` *` |
|        - |  4315 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4316 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4317 | ` * two elements.` |
|        - |  4318 | `*/` |
|        - |  4319 | `/* OP_BOR_STORE * * *` |
|        - |  4320 | ` *` |
|        - |  4321 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4322 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4323 | ` * two elements.` |
|        - |  4324 | ` */` |
|        - |  4325 | `/* OP_BXOR_STORE * * *` |
|        - |  4326 | ` *` |
|        - |  4327 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4328 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4329 | ` * two elements.` |
|        - |  4330 | ` */` |
|        7 |  4331 | `case PH7_OP_BAND_STORE:` |
|        - |  4332 | `case PH7_OP_BOR_STORE:` |
|        - |  4333 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4334 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4335 | `	ph7_value *pObj;` |
|        - |  4336 | `	sxi64 a,b,r;` |
|        - |  4337 | `#ifdef UNTRUST` |
|        - |  4338 | `	if( pNos < pStack ){` |
|        - |  4339 | `		goto Abort;` |
|        - |  4340 | `	}` |
|        - |  4341 | `#endif` |
|        - |  4342 | `	/* Force the operands to be integer */` |
|       15 |  4343 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4344 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4345 | `	}` |
|       15 |  4346 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4347 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4348 | `	}` |
|        - |  4349 | `	/* Perform the requested operation */` |
|       15 |  4350 | `	a = pTos->x.iVal;` |
|       15 |  4351 | `	b = pNos->x.iVal;` |
|       15 |  4352 | `	switch(pInstr->iOp){` |
|        2 |  4353 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4354 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4355 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4356 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4357 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4358 | `	case PH7_OP_BAND:` |
|        5 |  4359 | `	default:          r = a&b; break;` |
|        - |  4360 | `	}` |
|        - |  4361 | `	/* Push the result */` |
|       15 |  4362 | `	pNos->x.iVal = r;` |
|       15 |  4363 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4364 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4365 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4366 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4367 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4368 | `	}` |
|       15 |  4369 | `	VmPopOperand(&pTos,1);` |
|       15 |  4370 | `	break;` |
|        - |  4371 | `				 }` |
|        - |  4372 | `/* OP_SHL * * *` |
|        - |  4373 | ` *` |
|        - |  4374 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4375 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4376 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4377 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4378 | ` */` |
|        - |  4379 | `/* OP_SHR * * *` |
|        - |  4380 | ` *` |
|        - |  4381 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4382 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4383 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4384 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4385 | ` */` |
|        9 |  4386 | `case PH7_OP_SHL:` |
|        - |  4387 | `case PH7_OP_SHR: {` |
|       19 |  4388 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4389 | `	sxi64 a,r;` |
|        - |  4390 | `	sxi32 b;` |
|        - |  4391 | `#ifdef UNTRUST` |
|        - |  4392 | `	if( pNos < pStack ){` |
|        - |  4393 | `		goto Abort;` |
|        - |  4394 | `	}` |
|        - |  4395 | `#endif` |
|        - |  4396 | `	/* Force the operands to be integer */` |
|       19 |  4397 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4398 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4399 | `	}` |
|       19 |  4400 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4401 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4402 | `	}` |
|        - |  4403 | `	/* Perform the requested operation */` |
|       19 |  4404 | `	a = pNos->x.iVal;` |
|       19 |  4405 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4406 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4407 | `		r = a << b;` |
|        6 |  4408 | `	}else{` |
|        9 |  4409 | `		r = a >> b;` |
|        - |  4410 | `	}` |
|        - |  4411 | `	/* Push the result */` |
|       19 |  4412 | `	pNos->x.iVal = r;` |
|       19 |  4413 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4414 | `	VmPopOperand(&pTos,1);` |
|       19 |  4415 | `	break;` |
|        - |  4416 | `				 }` |
|        - |  4417 | `/*  OP_SHL_STORE * * *` |
|        - |  4418 | ` *` |
|        - |  4419 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4420 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4421 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4422 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4423 | ` */` |
|        - |  4424 | `/* OP_SHR_STORE * * *` |
|        - |  4425 | ` *` |
|        - |  4426 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4427 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4428 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4429 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4430 | ` */` |
|        7 |  4431 | `case PH7_OP_SHL_STORE:` |
|        - |  4432 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4433 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4434 | `	ph7_value *pObj;` |
|        - |  4435 | `	sxi64 a,r;` |
|        - |  4436 | `	sxi32 b;` |
|        - |  4437 | `#ifdef UNTRUST` |
|        - |  4438 | `	if( pNos < pStack ){` |
|        - |  4439 | `		goto Abort;` |
|        - |  4440 | `	}` |
|        - |  4441 | `#endif` |
|        - |  4442 | `	/* Force the operands to be integer */` |
|       15 |  4443 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4444 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4445 | `	}` |
|       15 |  4446 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4447 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4448 | `	}` |
|        - |  4449 | `	/* Perform the requested operation */` |
|       15 |  4450 | `	a = pTos->x.iVal;` |
|       15 |  4451 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4452 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4453 | `		r = a << b;` |
|        4 |  4454 | `	}else{` |
|        9 |  4455 | `		r = a >> b;` |
|        - |  4456 | `	}` |
|        - |  4457 | `	/* Push the result */` |
|       15 |  4458 | `	pNos->x.iVal = r;` |
|       15 |  4459 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4460 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4461 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4462 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4463 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4464 | `	}` |
|       15 |  4465 | `	VmPopOperand(&pTos,1);` |
|       15 |  4466 | `	break;` |
|        - |  4467 | `				 }` |
|        - |  4468 | `/* CAT:  P1 * *` |
|        - |  4469 | ` *` |
|        - |  4470 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4471 | ` * back.` |
|        - |  4472 | ` */` |
|    63491 |  4473 | `case PH7_OP_CAT:{` |
|        - |  4474 | `	ph7_value *pNos,*pCur;` |
|   126984 |  4475 | `	if( pInstr->iP1 < 1 ){` |
|    99950 |  4476 | `		pNos = &pTos[-1];` |
|    49976 |  4477 | `	}else{` |
|    27036 |  4478 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4479 | `	}` |
|        - |  4480 | `#ifdef UNTRUST` |
|        - |  4481 | `	if( pNos < pStack ){` |
|        - |  4482 | `		goto Abort;` |
|        - |  4483 | `	}` |
|        - |  4484 | `#endif` |
|        - |  4485 | `	/* Force a string cast */` |
|   126984 |  4486 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1222 |  4487 | `		PH7_MemObjToString(pNos);` |
|      610 |  4488 | `	}` |
|   126984 |  4489 | `	pCur = &pNos[1];` |
|   255990 |  4490 | `	while( pCur <= pTos ){` |
|   129008 |  4491 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50638 |  4492 | `			PH7_MemObjToString(pCur);` |
|    25318 |  4493 | `		}` |
|        - |  4494 | `		/* Perform the concatenation */` |
|   129008 |  4495 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   128970 |  4496 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64484 |  4497 | `		}` |
|   129008 |  4498 | `		SyBlobRelease(&pCur->sBlob);` |
|   129008 |  4499 | `		pCur++;` |
|        2 |  4500 | `	}` |
|   126984 |  4501 | `	pTos = pNos;` |
|   126984 |  4502 | `	break;` |
|        - |  4503 | `				}` |
|        - |  4504 | `/*  CAT_STORE: * * *` |
|        - |  4505 | ` *` |
|        - |  4506 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4507 | ` * back.` |
|        - |  4508 | ` */` |
|     3678 |  4509 | `case PH7_OP_CAT_STORE:{` |
|     7358 |  4510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4511 | `	ph7_value *pObj;` |
|        - |  4512 | `#ifdef UNTRUST` |
|        - |  4513 | `	if( pNos < pStack ){` |
|        - |  4514 | `		goto Abort;` |
|        - |  4515 | `	}` |
|        - |  4516 | `#endif` |
|     7358 |  4517 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4518 | `		/* Force a string cast */` |
|      ! 0 |  4519 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4520 | `	}` |
|     7358 |  4521 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4522 | `		/* Force a string cast */` |
|      ! 0 |  4523 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4524 | `	}` |
|        - |  4525 | `	/* Perform the concatenation (Reverse order) */` |
|     7358 |  4526 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7358 |  4527 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3678 |  4528 | `	}` |
|        - |  4529 | `	/* Perform the store operation */` |
|     7358 |  4530 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4531 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7358 |  4532 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7358 |  4533 | `		PH7_MemObjStore(pTos,pObj);` |
|     3678 |  4534 | `	}` |
|     7358 |  4535 | `	PH7_MemObjStore(pTos,pNos);` |
|     7358 |  4536 | `	VmPopOperand(&pTos,1);` |
|     7358 |  4537 | `	break;` |
|        - |  4538 | `				}` |
|        - |  4539 | `/* OP_AND: * * *` |
|        - |  4540 | ` *` |
|        - |  4541 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4542 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4543 | ` * stack.` |
|        - |  4544 | ` */` |
|        - |  4545 | `/* OP_OR: * * *` |
|        - |  4546 | ` *` |
|        - |  4547 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4548 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4549 | ` * stack.` |
|        - |  4550 | ` */` |
|    94549 |  4551 | `case PH7_OP_LAND:` |
|        - |  4552 | `case PH7_OP_LOR: {` |
|   189144 |  4553 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4554 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4555 | `#ifdef UNTRUST` |
|        - |  4556 | `	if( pNos < pStack ){` |
|        - |  4557 | `		goto Abort;` |
|        - |  4558 | `	}` |
|        - |  4559 | `#endif` |
|        - |  4560 | `	/* Force a boolean cast */` |
|   189144 |  4561 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4562 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4563 | `	}` |
|   189144 |  4564 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4565 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4566 | `	}` |
|   189144 |  4567 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189144 |  4568 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189144 |  4569 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4570 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86736 |  4571 | `		v1 = and_logic[v1*3+v2];` |
|    43391 |  4572 | `	}else{` |
|        - |  4573 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102410 |  4574 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4575 | `	}` |
|   189144 |  4576 | `	if( v1 == 2 ){` |
|      ! 0 |  4577 | `		v1 = 1;` |
|      ! 0 |  4578 | `	}` |
|   189144 |  4579 | `	VmPopOperand(&pTos,1);` |
|   189144 |  4580 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189144 |  4581 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189144 |  4582 | `	break;` |
|        - |  4583 | `				 }` |
|        - |  4584 | `/* OP_LXOR: * * *` |
|        - |  4585 | ` *` |
|        - |  4586 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4587 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4588 | ` * stack.` |
|        - |  4589 | ` * According to the PHP language reference manual:` |
|        - |  4590 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4591 | ` *  TRUE,but not both.` |
|        - |  4592 | ` */` |
|        5 |  4593 | `case PH7_OP_LXOR:{` |
|       11 |  4594 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4595 | `	sxi32 v = 0;` |
|        - |  4596 | `#ifdef UNTRUST` |
|        - |  4597 | `	if( pNos < pStack ){` |
|        - |  4598 | `		goto Abort;` |
|        - |  4599 | `	}` |
|        - |  4600 | `#endif` |
|        - |  4601 | `	/* Force a boolean cast */` |
|       11 |  4602 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4603 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4604 | `	}` |
|       11 |  4605 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4606 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4607 | `	}` |
|       11 |  4608 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4609 | `		v = 1;` |
|        3 |  4610 | `	}` |
|       11 |  4611 | `	VmPopOperand(&pTos,1);` |
|       11 |  4612 | `	pTos->x.iVal = v;` |
|       11 |  4613 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4614 | `	break;` |
|        - |  4615 | `				 }` |
|        - |  4616 | `/* OP_EQ P1 P2 P3` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4619 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4620 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4621 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4622 | ` */` |
|        - |  4623 | `/* OP_NEQ P1 P2 P3` |
|        - |  4624 | ` *` |
|        - |  4625 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4626 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4627 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4628 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4629 | ` */` |
|     3933 |  4630 | `case PH7_OP_EQ:` |
|        - |  4631 | `case PH7_OP_NEQ: {` |
|     7868 |  4632 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4633 | `	/* Perform the comparison and act accordingly */` |
|        - |  4634 | `#ifdef UNTRUST` |
|        - |  4635 | `	if( pNos < pStack ){` |
|        - |  4636 | `		goto Abort;` |
|        - |  4637 | `	}` |
|        - |  4638 | `#endif` |
|     7868 |  4639 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7868 |  4640 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4641 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7859 |  4642 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7824 |  4643 | `		rc = rc == 0;` |
|     3913 |  4644 | `	}else{` |
|       28 |  4645 | `		rc = rc != 0;` |
|        - |  4646 | `	}` |
|     7868 |  4647 | `	VmPopOperand(&pTos,1);` |
|     7868 |  4648 | `	if( !pInstr->iP2 ){` |
|        - |  4649 | `		/* Push comparison result without taking the jump */` |
|     7868 |  4650 | `		PH7_MemObjRelease(pTos);` |
|     7868 |  4651 | `		pTos->x.iVal = rc;` |
|        - |  4652 | `		/* Invalidate any prior representation */` |
|     7868 |  4653 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3935 |  4654 | `	}else{` |
|      ! 0 |  4655 | `		if( rc ){` |
|        - |  4656 | `			/* Jump to the desired location */` |
|      ! 0 |  4657 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4658 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4659 | `		}` |
|        - |  4660 | `	}` |
|     7868 |  4661 | `	break;` |
|        - |  4662 | `				 }` |
|        - |  4663 | `/* OP_TEQ P1 P2 *` |
|        - |  4664 | ` *` |
|        - |  4665 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4666 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4667 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4668 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4669 | ` */` |
|   132489 |  4670 | `case PH7_OP_TEQ: {` |
|   264980 |  4671 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4672 | `	/* Perform the comparison and act accordingly */` |
|        - |  4673 | `#ifdef UNTRUST` |
|        - |  4674 | `	if( pNos < pStack ){` |
|        - |  4675 | `		goto Abort;` |
|        - |  4676 | `	}` |
|        - |  4677 | `#endif` |
|   264980 |  4678 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   264980 |  4679 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4680 | `		rc = 0;` |
|        2 |  4681 | `	}else{` |
|   264978 |  4682 | `		rc = rc == 0;` |
|        - |  4683 | `	}` |
|   264980 |  4684 | `	VmPopOperand(&pTos,1);` |
|   264980 |  4685 | `	if( !pInstr->iP2 ){` |
|        - |  4686 | `		/* Push comparison result without taking the jump */` |
|   264980 |  4687 | `		PH7_MemObjRelease(pTos);` |
|   264980 |  4688 | `		pTos->x.iVal = rc;` |
|        - |  4689 | `		/* Invalidate any prior representation */` |
|   264980 |  4690 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   132491 |  4691 | `	}else{` |
|      ! 0 |  4692 | `		if( rc ){` |
|        - |  4693 | `			/* Jump to the desired location */` |
|      ! 0 |  4694 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4695 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4696 | `		}` |
|        - |  4697 | `	}` |
|   264980 |  4698 | `	break;` |
|        - |  4699 | `				 }` |
|        - |  4700 | `/* OP_TNE P1 P2 *` |
|        - |  4701 | ` *` |
|        - |  4702 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4703 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4704 | ` * instruction.` |
|        - |  4705 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4706 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4707 | ` *` |
|        - |  4708 | ` */` |
|   102976 |  4709 | `case PH7_OP_TNE: {` |
|   205954 |  4710 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4711 | `	/* Perform the comparison and act accordingly */` |
|        - |  4712 | `#ifdef UNTRUST` |
|        - |  4713 | `	if( pNos < pStack ){` |
|        - |  4714 | `		goto Abort;` |
|        - |  4715 | `	}` |
|        - |  4716 | `#endif` |
|   205954 |  4717 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   205954 |  4718 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4719 | `		rc = 1;` |
|        2 |  4720 | `	}else{` |
|   205952 |  4721 | `		rc = rc != 0;` |
|        - |  4722 | `	}` |
|   205954 |  4723 | `	VmPopOperand(&pTos,1);` |
|   205954 |  4724 | `	if( !pInstr->iP2 ){` |
|        - |  4725 | `		/* Push comparison result without taking the jump */` |
|   205954 |  4726 | `		PH7_MemObjRelease(pTos);` |
|   205954 |  4727 | `		pTos->x.iVal = rc;` |
|        - |  4728 | `		/* Invalidate any prior representation */` |
|   205954 |  4729 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102978 |  4730 | `	}else{` |
|      ! 0 |  4731 | `		if( rc ){` |
|        - |  4732 | `			/* Jump to the desired location */` |
|      ! 0 |  4733 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4734 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4735 | `		}` |
|        - |  4736 | `	}` |
|   205954 |  4737 | `	break;` |
|        - |  4738 | `				 }` |
|        - |  4739 | `/* OP_LT P1 P2 P3` |
|        - |  4740 | ` *` |
|        - |  4741 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4742 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4743 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4744 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4745 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4746 | ` *` |
|        - |  4747 | ` */` |
|        - |  4748 | `/* OP_LE P1 P2 P3` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4751 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4752 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4753 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4754 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4755 | ` *` |
|        - |  4756 | ` */` |
|   102449 |  4757 | `case PH7_OP_LT:` |
|        - |  4758 | `case PH7_OP_LE: {` |
|   204944 |  4759 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4760 | `	/* Perform the comparison and act accordingly */` |
|        - |  4761 | `#ifdef UNTRUST` |
|        - |  4762 | `	if( pNos < pStack ){` |
|        - |  4763 | `		goto Abort;` |
|        - |  4764 | `	}` |
|        - |  4765 | `#endif` |
|   204944 |  4766 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204944 |  4767 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4768 | `		rc = 0;` |
|   204940 |  4769 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      430 |  4770 | `		rc = rc < 1;` |
|      216 |  4771 | `	}else{` |
|   204508 |  4772 | `		rc = rc < 0;` |
|        - |  4773 | `	}` |
|   204944 |  4774 | `	VmPopOperand(&pTos,1);` |
|   204944 |  4775 | `	if( !pInstr->iP2 ){` |
|        - |  4776 | `		/* Push comparison result without taking the jump */` |
|   204944 |  4777 | `		PH7_MemObjRelease(pTos);` |
|   204944 |  4778 | `		pTos->x.iVal = rc;` |
|        - |  4779 | `		/* Invalidate any prior representation */` |
|   204944 |  4780 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102495 |  4781 | `	}else{` |
|      ! 0 |  4782 | `		if( rc ){` |
|        - |  4783 | `			/* Jump to the desired location */` |
|      ! 0 |  4784 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4785 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4786 | `		}` |
|        - |  4787 | `	}` |
|   204944 |  4788 | `	break;` |
|        - |  4789 | `				}` |
|        - |  4790 | `/* OP_GT P1 P2 P3` |
|        - |  4791 | ` *` |
|        - |  4792 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4793 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4794 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4795 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4796 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4797 | ` *` |
|        - |  4798 | ` */` |
|        - |  4799 | `/* OP_GE P1 P2 P3` |
|        - |  4800 | ` *` |
|        - |  4801 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4802 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4803 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4804 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4805 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4806 | ` *` |
|        - |  4807 | ` */` |
|    48771 |  4808 | `case PH7_OP_GT:` |
|        - |  4809 | `case PH7_OP_GE: {` |
|    97544 |  4810 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4811 | `	/* Perform the comparison and act accordingly */` |
|        - |  4812 | `#ifdef UNTRUST` |
|        - |  4813 | `	if( pNos < pStack ){` |
|        - |  4814 | `		goto Abort;` |
|        - |  4815 | `	}` |
|        - |  4816 | `#endif` |
|    97544 |  4817 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97544 |  4818 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4819 | `		rc = 0;` |
|    97540 |  4820 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97388 |  4821 | `		rc = rc >= 0;` |
|    48695 |  4822 | `	}else{` |
|      150 |  4823 | `		rc = rc > 0;` |
|        - |  4824 | `	}` |
|    97544 |  4825 | `	VmPopOperand(&pTos,1);` |
|    97544 |  4826 | `	if( !pInstr->iP2 ){` |
|        - |  4827 | `		/* Push comparison result without taking the jump */` |
|    97544 |  4828 | `		PH7_MemObjRelease(pTos);` |
|    97544 |  4829 | `		pTos->x.iVal = rc;` |
|        - |  4830 | `		/* Invalidate any prior representation */` |
|    97544 |  4831 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48773 |  4832 | `	}else{` |
|      ! 0 |  4833 | `		if( rc ){` |
|        - |  4834 | `			/* Jump to the desired location */` |
|      ! 0 |  4835 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4836 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4837 | `		}` |
|        - |  4838 | `	}` |
|    97544 |  4839 | `	break;` |
|        - |  4840 | `				}` |
|        - |  4841 | `/* OP_SEQ P1 P2 *` |
|        - |  4842 | ` * Strict string comparison.` |
|        - |  4843 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4844 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4845 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4846 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4847 | ` * use PH7_OP_EQ.` |
|        - |  4848 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4849 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4850 | ` */` |
|        - |  4851 | `/* OP_SNE P1 P2 *` |
|        - |  4852 | ` * Strict string comparison.` |
|        - |  4853 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4854 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4855 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4856 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4857 | ` * use PH7_OP_EQ.` |
|        - |  4858 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4859 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4860 | ` */` |
|       18 |  4861 | `case PH7_OP_SEQ:` |
|        - |  4862 | `case PH7_OP_SNE: {` |
|       38 |  4863 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4864 | `	SyString s1,s2;` |
|        - |  4865 | `	/* Perform the comparison and act accordingly */` |
|        - |  4866 | `#ifdef UNTRUST` |
|        - |  4867 | `	if( pNos < pStack ){` |
|        - |  4868 | `		goto Abort;` |
|        - |  4869 | `	}` |
|        - |  4870 | `#endif` |
|        - |  4871 | `	/* Force a string cast */` |
|       38 |  4872 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4873 | `		PH7_MemObjToString(pTos);` |
|        2 |  4874 | `	}` |
|       38 |  4875 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4876 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4877 | `	}` |
|       38 |  4878 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4879 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4880 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4881 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4882 | `		rc = rc != 0;` |
|      ! 0 |  4883 | `	}else{` |
|       38 |  4884 | `		rc = rc == 0;` |
|        - |  4885 | `	}` |
|       38 |  4886 | `	VmPopOperand(&pTos,1);` |
|       38 |  4887 | `	if( !pInstr->iP2 ){` |
|        - |  4888 | `		/* Push comparison result without taking the jump */` |
|       38 |  4889 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4890 | `		pTos->x.iVal = rc;` |
|        - |  4891 | `		/* Invalidate any prior representation */` |
|       38 |  4892 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4893 | `	}else{` |
|      ! 0 |  4894 | `		if( rc ){` |
|        - |  4895 | `			/* Jump to the desired location */` |
|      ! 0 |  4896 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4897 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4898 | `		}` |
|        - |  4899 | `	}` |
|       38 |  4900 | `	break;` |
|        - |  4901 | `				 }` |
|        - |  4902 | `/*` |
|        - |  4903 | ` * OP_LOAD_REF * * *` |
|        - |  4904 | ` * Push the index of a referenced object on the stack.` |
|        - |  4905 | ` */` |
|       57 |  4906 | `case PH7_OP_LOAD_REF: {` |
|        - |  4907 | `	sxu32 nIdx;` |
|        - |  4908 | `#ifdef UNTRUST` |
|        - |  4909 | `	if( pTos < pStack ){` |
|        - |  4910 | `		goto Abort;` |
|        - |  4911 | `	}` |
|        - |  4912 | `#endif` |
|        - |  4913 | `	/* Extract memory object index */` |
|      115 |  4914 | `	nIdx = pTos->nIdx;` |
|      115 |  4915 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4916 | `		/* Nullify the object */` |
|       95 |  4917 | `		PH7_MemObjRelease(pTos);` |
|        - |  4918 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4919 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4920 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4921 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4922 | `	}` |
|      115 |  4923 | `	break;` |
|        - |  4924 | `					  }` |
|        - |  4925 | `/*` |
|        - |  4926 | ` * OP_STORE_REF * * P3` |
|        - |  4927 | ` * Perform an assignment operation by reference.` |
|        - |  4928 | ` */` |
|       15 |  4929 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4930 | `	 SyString sName = { 0 , 0 };` |
|        - |  4931 | `	 VmFrame *pFrameLocal;` |
|        - |  4932 | `	SyHashEntry *pEntry;` |
|        - |  4933 | `	sxu32 nIdx;` |
|        - |  4934 | `#ifdef UNTRUST` |
|        - |  4935 | `	if( pTos < pStack ){` |
|        - |  4936 | `		goto Abort;` |
|        - |  4937 | `	}` |
|        - |  4938 | `#endif` |
|       32 |  4939 | `	if( pInstr->p3 == 0 ){` |
|        - |  4940 | `		char *zName;` |
|        - |  4941 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4942 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4943 | `			/* Force a string cast */` |
|      ! 0 |  4944 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4945 | `		}` |
|      ! 0 |  4946 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4947 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4948 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4949 | `			if( zName ){` |
|      ! 0 |  4950 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4951 | `			}` |
|      ! 0 |  4952 | `		}` |
|      ! 0 |  4953 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4954 | `		pTos--;` |
|      ! 0 |  4955 | `	}else{` |
|       32 |  4956 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4957 | `	}` |
|       32 |  4958 | `	nIdx = pTos->nIdx;` |
|       32 |  4959 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4960 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4961 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4962 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4963 | `		}else{` |
|        - |  4964 | `			ph7_value *pObj;` |
|        - |  4965 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4966 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4967 | `			if( pObj == 0 ){` |
|      ! 0 |  4968 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4969 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4970 | `				goto Abort;` |
|        - |  4971 | `			}` |
|        - |  4972 | `			/* Perform the store operation */` |
|      ! 0 |  4973 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4974 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4975 | `		}` |
|       32 |  4976 | `	}else if( sName.nByte > 0){` |
|       32 |  4977 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4978 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4979 | `		}else{` |
|       32 |  4980 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4981 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4982 | `			/* Query the local frame */` |
|       32 |  4983 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4984 | `			if( pEntry ){` |
|      ! 0 |  4985 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4986 | `			}else{` |
|       32 |  4987 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4988 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4989 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4990 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4991 | `				}` |
|       32 |  4992 | `				if( rc == SXRET_OK ){` |
|       32 |  4993 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4994 | `				}` |
|        - |  4995 | `			}` |
|        - |  4996 | `		}` |
|       15 |  4997 | `	}` |
|       32 |  4998 | `	break;` |
|        - |  4999 | `				 }` |
|        - |  5000 | `/*` |
|        - |  5001 | ` * OP_UPLINK P1 * *` |
|        - |  5002 | ` * Link a variable to the top active VM frame.` |
|        - |  5003 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5004 | ` */` |
|       25 |  5005 | `case PH7_OP_UPLINK: {` |
|       52 |  5006 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5007 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5008 | `		SyString sName;` |
|        - |  5009 | `		/* Perform the link */` |
|      104 |  5010 | `		while( pLink <= pTos ){` |
|       54 |  5011 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5012 | `				/* Force a string cast */` |
|      ! 0 |  5013 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5014 | `			}` |
|       54 |  5015 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5016 | `			if( sName.nByte > 0 ){` |
|       54 |  5017 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5018 | `			}` |
|       54 |  5019 | `			pLink++;` |
|        2 |  5020 | `		}` |
|       25 |  5021 | `	}` |
|       52 |  5022 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5023 | `	break;` |
|        - |  5024 | `					}` |
|        - |  5025 | `/*` |
|        - |  5026 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5027 | ` * Push an exception in the corresponding container so that` |
|        - |  5028 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5029 | ` */` |
|       32 |  5030 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5031 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5032 | `	VmFrame *pFrameLocal;` |
|        - |  5033 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5034 | `	pException->iFinallyDone = 0;` |
|       66 |  5035 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5036 | `	/* Create the exception frame */` |
|       66 |  5037 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5038 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5039 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5040 | `		goto Abort;` |
|        - |  5041 | `	}` |
|        - |  5042 | `	/* Mark the special frame */` |
|       66 |  5043 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5044 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5045 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5046 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5047 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5048 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5049 | `	break;` |
|        - |  5050 | `							}` |
|        - |  5051 | `/*` |
|        - |  5052 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5053 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5054 | ` */` |
|       31 |  5055 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5056 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5057 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5058 | `		ph7_exception **apException;` |
|        - |  5059 | `		/* Pop the loaded exception */` |
|       28 |  5060 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5061 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5062 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5063 | `		}` |
|       13 |  5064 | `	}` |
|       64 |  5065 | `	pException->pFrame = 0;` |
|        - |  5066 | `	/* Leave the exception frame */` |
|       64 |  5067 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5068 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5069 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5070 | `		sxi32 rcFinally;` |
|       19 |  5071 | `		pException->iFinallyDone = 1;` |
|       19 |  5072 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  5073 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5074 | `			goto Abort;` |
|        - |  5075 | `		}` |
|        9 |  5076 | `	}` |
|       64 |  5077 | `	break;` |
|        - |  5078 | `							}` |
|        - |  5079 |  |
|        - |  5080 | `/*` |
|        - |  5081 | ` * OP_THROW * P2 *` |
|        - |  5082 | ` * Throw an user exception.` |
|        - |  5083 | ` */` |
|       18 |  5084 | `case PH7_OP_THROW: {` |
|       38 |  5085 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5086 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5087 | `#ifdef UNTRUST` |
|        - |  5088 | `	if( pTos < pStack ){` |
|        - |  5089 | `		goto Abort;` |
|        - |  5090 | `	}` |
|        - |  5091 | `#endif` |
|       38 |  5092 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5093 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5094 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5095 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5096 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5097 | `		ph7_class *pException;` |
|        - |  5098 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5099 | `		 */` |
|       38 |  5100 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5101 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5102 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5103 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5104 | `			if( rc == SXERR_ABORT ){` |
|        - |  5105 | `				/* Abort processing immediately */` |
|      ! 0 |  5106 | `				goto Abort;` |
|        - |  5107 | `			}` |
|      ! 0 |  5108 | `		}else{` |
|        - |  5109 | `			/* Throw the exception */` |
|       38 |  5110 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5111 | `			if( rc == SXERR_ABORT ){` |
|        - |  5112 | `				/* Abort processing immediately */` |
|        9 |  5113 | `				goto Abort;` |
|        - |  5114 | `			}` |
|        - |  5115 | `		}` |
|       16 |  5116 | `	}else{` |
|        - |  5117 | `		/* Expecting a class instance */` |
|      ! 0 |  5118 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5119 | `		if( rc == SXERR_ABORT ){` |
|        - |  5120 | `			/* Abort processing immediately */` |
|      ! 0 |  5121 | `			goto Abort;` |
|        - |  5122 | `		}` |
|        - |  5123 | `	}` |
|        - |  5124 | `	/* Pop the top entry */` |
|       30 |  5125 | `	VmPopOperand(&pTos,1);` |
|        - |  5126 | `	/* Perform an unconditional jump */` |
|       30 |  5127 | `	pc = nJump - 1;` |
|       30 |  5128 | `	break;` |
|        - |  5129 | `				   }` |
|        - |  5130 | `/*` |
|        - |  5131 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5132 | ` * Prepare a foreach step.` |
|        - |  5133 | ` */` |
|     4971 |  5134 | `case PH7_OP_FOREACH_INIT: {` |
|     9944 |  5135 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5136 | `	void *pName;` |
|        - |  5137 | `#ifdef UNTRUST` |
|        - |  5138 | `	if( pTos < pStack ){` |
|        - |  5139 | `		goto Abort;` |
|        - |  5140 | `	}` |
|        - |  5141 | `#endif` |
|     9944 |  5142 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5143 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5144 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5145 | `			/* Force a string cast */` |
|      ! 0 |  5146 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5147 | `		}` |
|        - |  5148 | `		/* Duplicate name */` |
|      ! 0 |  5149 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5150 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5151 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5152 | `		}` |
|      ! 0 |  5153 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5154 | `	}` |
|     9944 |  5155 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5156 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5157 | `			/* Force a string cast */` |
|      ! 0 |  5158 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5159 | `		}` |
|        - |  5160 | `		/* Duplicate name */` |
|      ! 0 |  5161 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5162 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5163 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5164 | `		}` |
|      ! 0 |  5165 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5166 | `	}` |
|        - |  5167 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9944 |  5168 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5169 | `		/* Jump out of the loop */` |
|      ! 0 |  5170 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5171 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5172 | `		}` |
|      ! 0 |  5173 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5174 | `	}else{` |
|        - |  5175 | `		ph7_foreach_step *pStep;` |
|     9944 |  5176 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9944 |  5177 | `		if( pStep == 0 ){` |
|      ! 0 |  5178 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5179 | `			/* Jump out of the loop */` |
|      ! 0 |  5180 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5181 | `		}else{` |
|        - |  5182 | `			/* Zero the structure */` |
|     9944 |  5183 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5184 | `			/* Prepare the step */` |
|     9944 |  5185 | `			pStep->iFlags = pInfo->iFlags;` |
|     9944 |  5186 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5187 | `				ph7_hashmap *pMap;` |
|        - |  5188 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5189 | `				 * source array so mutations don't affect other sharers. */` |
|     9916 |  5190 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5191 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5192 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5193 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5194 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5195 | `						 * variable still points at the same hashmap as` |
|        - |  5196 | `						 * the stack value. */` |
|       10 |  5197 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5198 | `							pCur->iRef--;` |
|       10 |  5199 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5200 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5201 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5202 | `						}` |
|        4 |  5203 | `					}` |
|        4 |  5204 | `				}` |
|     9916 |  5205 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5206 | `				/* Reset the internal loop cursor */` |
|     9916 |  5207 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5208 | `				/* Mark the step */` |
|     9916 |  5209 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9916 |  5210 | `				pStep->xIter.pMap = pMap;` |
|     9916 |  5211 | `				pMap->iRef++;` |
|     4959 |  5212 | `			}else{` |
|       30 |  5213 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5214 | `				ph7_class *pIteratorClass;` |
|        - |  5215 | `				/* Check if the object implements Iterator */` |
|       30 |  5216 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5217 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5218 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5219 | `					ph7_class_method *pRewind;` |
|       19 |  5220 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       19 |  5221 | `					pStep->xIter.pThis = pThis;` |
|       19 |  5222 | `					pThis->iRef++;` |
|       19 |  5223 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       19 |  5224 | `					if( pRewind ){` |
|       19 |  5225 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5226 | `					}` |
|       10 |  5227 | `				}else{` |
|        - |  5228 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5229 | `					ph7_class *pIterAggClass;` |
|       12 |  5230 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5231 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5232 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5233 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5234 | `						ph7_class_method *pGetIter;` |
|        3 |  5235 | `						int iterAggOk = 0;` |
|        3 |  5236 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5237 | `						if( pGetIter ){` |
|        - |  5238 | `							ph7_value sResult;` |
|        3 |  5239 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5240 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5241 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5242 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5243 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5244 | `									ph7_class_method *pRewind;` |
|        3 |  5245 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5246 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5247 | `									pIterObj->iRef++;` |
|        - |  5248 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5249 | `									pStep->pOwner = pThis;` |
|        3 |  5250 | `									pThis->iRef++;` |
|        3 |  5251 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5252 | `									if( pRewind ){` |
|        3 |  5253 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5254 | `									}` |
|        3 |  5255 | `									iterAggOk = 1;` |
|        1 |  5256 | `								}` |
|        1 |  5257 | `							}` |
|        3 |  5258 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5259 | `						}` |
|        3 |  5260 | `						if( !iterAggOk ){` |
|        - |  5261 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5262 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5263 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5264 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5265 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5266 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5267 | `						}` |
|        2 |  5268 | `					}else{` |
|        - |  5269 | `						/* Plain object iteration via hAttr */` |
|        9 |  5270 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5271 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5272 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5273 | `						pThis->iRef++;` |
|        - |  5274 | `					}` |
|        - |  5275 | `				}` |
|        - |  5276 | `			}` |
|        - |  5277 | `		}` |
|     9944 |  5278 | `		if( pStep ){` |
|     9944 |  5279 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5280 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5281 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5282 | `				/* Jump out of the loop */` |
|      ! 0 |  5283 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5284 | `			}` |
|     4971 |  5285 | `		}` |
|        - |  5286 | `	}` |
|     9944 |  5287 | `	VmPopOperand(&pTos,1);` |
|     9944 |  5288 | `	break;` |
|        - |  5289 | `						  }` |
|        - |  5290 | `/*` |
|        - |  5291 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5292 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5293 | ` */` |
|    80182 |  5294 | `case PH7_OP_FOREACH_STEP: {` |
|   160366 |  5295 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5296 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5297 | `	ph7_value *pValue;` |
|        - |  5298 | `	VmFrame *pFrameLocal;` |
|        - |  5299 | `	/* Peek the last step */` |
|   160366 |  5300 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   160366 |  5301 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   160366 |  5302 | `	pFrameLocal = pVm->pFrame;` |
|   160366 |  5303 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   160366 |  5304 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   160254 |  5305 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5306 | `		ph7_hashmap_node *pNode;` |
|        - |  5307 | `		/* Extract the current node value */` |
|   160254 |  5308 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   160254 |  5309 | `		if( pNode == 0 ){` |
|        - |  5310 | `			/* No more entry to process */` |
|     9914 |  5311 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9914 |  5312 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5313 | `				/* Break the reference with the last element */` |
|        7 |  5314 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5315 | `			}` |
|        - |  5316 | `			/* Automatically reset the loop cursor */` |
|     9914 |  5317 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5318 | `			/* Cleanup the mess left behind */` |
|     9914 |  5319 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9914 |  5320 | `			SySetPop(&pInfo->aStep);` |
|     9914 |  5321 | `			PH7_HashmapUnref(pMap);` |
|     4958 |  5322 | `		}else{` |
|   150342 |  5323 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5324 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5325 | `				if( pKey ){` |
|      416 |  5326 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5327 | `				}` |
|      207 |  5328 | `			}` |
|   150342 |  5329 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5330 | `				SyHashEntry *pEntry;` |
|        - |  5331 | `				/* Pass by reference */` |
|       24 |  5332 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5333 | `				if( pEntry ){` |
|       22 |  5334 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5335 | `				}else{` |
|        4 |  5336 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5337 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5338 | `				}` |
|       13 |  5339 | `			}else{` |
|        - |  5340 | `				/* Make a copy of the entry value */` |
|   150320 |  5341 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   150320 |  5342 | `				if( pValue ){` |
|   150320 |  5343 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    75159 |  5344 | `				}` |
|        - |  5345 | `			}` |
|        2 |  5346 | `		}` |
|    80240 |  5347 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5348 | `		/* Iterator-based iteration.` |
|        - |  5349 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5350 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5351 | `		 */` |
|       89 |  5352 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5353 | `		ph7_class_method *pMethod;` |
|        - |  5354 | `		ph7_value sResult;` |
|       89 |  5355 | `		int isValid = 0;` |
|        - |  5356 | `		/* Call next() to advance — but skip on the first iteration */` |
|       89 |  5357 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       21 |  5358 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       11 |  5359 | `		}else{` |
|       69 |  5360 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       69 |  5361 | `			if( pMethod ){` |
|       69 |  5362 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5363 | `			}` |
|        - |  5364 | `		}` |
|        - |  5365 | `		/* Call valid() */` |
|       89 |  5366 | `		PH7_MemObjInit(pVm,&sResult);` |
|       89 |  5367 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       89 |  5368 | `		if( pMethod ){` |
|       89 |  5369 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       89 |  5370 | `			PH7_MemObjToBool(&sResult);` |
|       89 |  5371 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5372 | `		}` |
|       89 |  5373 | `		PH7_MemObjRelease(&sResult);` |
|       89 |  5374 | `		if( !isValid ){` |
|        - |  5375 | `			/* Iterator exhausted */` |
|       19 |  5376 | `			pc = pInstr->iP2 - 1;` |
|        - |  5377 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       19 |  5378 | `			if( pStep->pOwner ){` |
|        3 |  5379 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5380 | `			}` |
|       19 |  5381 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       19 |  5382 | `			SySetPop(&pInfo->aStep);` |
|       19 |  5383 | `			PH7_ClassInstanceUnref(pThis);` |
|       10 |  5384 | `		}else{` |
|        - |  5385 | `			/* Call current() to get value */` |
|       71 |  5386 | `			PH7_MemObjInit(pVm,&sResult);` |
|       71 |  5387 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       71 |  5388 | `			if( pMethod ){` |
|       71 |  5389 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5390 | `			}` |
|       71 |  5391 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       71 |  5392 | `			if( pValue ){` |
|       71 |  5393 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5394 | `			}` |
|       71 |  5395 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5396 | `			/* Call key() if needed */` |
|       71 |  5397 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5398 | `				ph7_value sKey;` |
|       35 |  5399 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5400 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5401 | `				if( pMethod ){` |
|       35 |  5402 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5403 | `				}` |
|       35 |  5404 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5405 | `				if( pValue ){` |
|       35 |  5406 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5407 | `				}` |
|       35 |  5408 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5409 | `			}` |
|        - |  5410 | `		}` |
|       45 |  5411 | `	}else{` |
|       25 |  5412 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5413 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5414 | `		SyHashEntry *pEntry;` |
|        - |  5415 | `		/* Point to the next attribute */` |
|       29 |  5416 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5417 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5418 | `			/* Check access permission */` |
|       31 |  5419 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5420 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5421 | `					break; /* Access is granted */` |
|        - |  5422 | `			}` |
|        1 |  5423 | `		}` |
|       25 |  5424 | `		if( pEntry == 0 ){` |
|        - |  5425 | `			/* Clean up the mess left behind */` |
|        9 |  5426 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5427 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5428 | `				/* Break the reference with the last element */` |
|        3 |  5429 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5430 | `			}` |
|        9 |  5431 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5432 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5433 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5434 | `		}else{` |
|       17 |  5435 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5436 | `			ph7_value *pAttrValue;` |
|       17 |  5437 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5438 | `				/* Fill with the current attribute name */` |
|       17 |  5439 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5440 | `				if( pKey ){` |
|       17 |  5441 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5442 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5443 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5444 | `				}` |
|        8 |  5445 | `			}` |
|        - |  5446 | `			/* Extract attribute value */` |
|       17 |  5447 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5448 | `			if( pAttrValue ){` |
|       17 |  5449 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5450 | `					/* Pass by reference */` |
|        3 |  5451 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5452 | `					if( pEntry ){` |
|        3 |  5453 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5454 | `					}else{` |
|      ! 0 |  5455 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5456 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5457 | `					}` |
|        2 |  5458 | `				}else{` |
|        - |  5459 | `					/* Make a copy of the attribute value */` |
|       15 |  5460 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5461 | `					if( pValue ){` |
|       15 |  5462 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5463 | `					}` |
|        - |  5464 | `				}` |
|        8 |  5465 | `			}` |
|        - |  5466 | `		}` |
|        - |  5467 | `	}` |
|   160366 |  5468 | `	break;` |
|        - |  5469 | `						  }` |
|        - |  5470 | `/*` |
|        - |  5471 | ` * OP_MEMBER P1 P2` |
|        - |  5472 | ` * Load class attribute/method on the stack.` |
|        - |  5473 | ` */` |
|     2184 |  5474 | `case PH7_OP_MEMBER: {` |
|        - |  5475 | `	ph7_class_instance *pThis;` |
|        - |  5476 | `	ph7_value *pNos;` |
|        - |  5477 | `	SyString sName;` |
|     4370 |  5478 | `	if( !pInstr->iP1 ){` |
|     4234 |  5479 | `		pNos = &pTos[-1];` |
|        - |  5480 | `#ifdef UNTRUST` |
|        - |  5481 | `		if( pNos < pStack ){` |
|        - |  5482 | `			goto Abort;` |
|        - |  5483 | `		}` |
|        - |  5484 | `#endif` |
|     4234 |  5485 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5486 | `			ph7_class *pClass;` |
|        - |  5487 | `			/* Class already instantiated */` |
|     4234 |  5488 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5489 | `			/* Point to the instantiated class */` |
|     4234 |  5490 | `			pClass = pThis->pClass;` |
|        - |  5491 | `			/* Extract attribute name first */` |
|     4234 |  5492 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4234 |  5493 | `			if( pInstr->iP2 ){` |
|        - |  5494 | `				/* Method call */` |
|      414 |  5495 | `				ph7_class_method *pMeth = 0;` |
|      414 |  5496 | `				if( sName.nByte > 0 ){` |
|        - |  5497 | `					/* Extract the target method */` |
|      414 |  5498 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      206 |  5499 | `				}` |
|      414 |  5500 | `				if( pMeth == 0 ){` |
|      ! 0 |  5501 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5502 | `						&pClass->sName,&sName` |
|        - |  5503 | `						);` |
|        - |  5504 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5505 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5506 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5507 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5508 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5509 | `				}else{` |
|        - |  5510 | `					/* Push method name on the stack */` |
|      414 |  5511 | `					PH7_MemObjRelease(pTos);` |
|      414 |  5512 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      414 |  5513 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5514 | `				}` |
|      414 |  5515 | `				pTos->nIdx = SXU32_HIGH;` |
|      208 |  5516 | `			}else{` |
|        - |  5517 | `				/* Attribute access */` |
|     3822 |  5518 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5519 | `				SyHashEntry *pEntry;` |
|        - |  5520 | `				/* Extract the target attribute */` |
|     3822 |  5521 | `				if( sName.nByte > 0 ){` |
|     3822 |  5522 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3822 |  5523 | `					if( pEntry ){` |
|        - |  5524 | `						/* Point to the attribute value */` |
|     3820 |  5525 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1909 |  5526 | `					}` |
|     1910 |  5527 | `				}` |
|     3822 |  5528 | `				if( pObjAttr == 0 ){` |
|        - |  5529 | `					/* No such attribute,load null */` |
|        4 |  5530 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5531 | `						&pClass->sName,&sName);` |
|        - |  5532 | `					/* Call the __get magic method if available */` |
|        3 |  5533 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5534 | `				}` |
|     3822 |  5535 | `				VmPopOperand(&pTos,1);` |
|        - |  5536 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5537 | `				 * This is due to the following case:` |
|        - |  5538 | `				 *     (new TestClass())->foo;` |
|        - |  5539 | `				 */` |
|     3822 |  5540 | `				pThis->iRef++;` |
|     3822 |  5541 | `				PH7_MemObjRelease(pTos);` |
|     3822 |  5542 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3822 |  5543 | `				if( pObjAttr ){` |
|     3820 |  5544 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5545 | `					/* Check attribute access */` |
|     3820 |  5546 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5547 | `						/* Load attribute */` |
|     3820 |  5548 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3820 |  5549 | `						if( pValue ){` |
|     3820 |  5550 | `							if( pThis->iRef < 2 ){` |
|        - |  5551 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5552 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5553 | `								 */` |
|        3 |  5554 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5555 | `							}else{` |
|        - |  5556 | `								/* Simple load */` |
|     3818 |  5557 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5558 | `							}` |
|     3820 |  5559 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3818 |  5560 | `								if( pThis->iRef > 1 ){` |
|        - |  5561 | `									/* Load attribute index */` |
|     3816 |  5562 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1907 |  5563 | `								}` |
|     1908 |  5564 | `							}` |
|     1909 |  5565 | `						}` |
|     1909 |  5566 | `					}` |
|     1909 |  5567 | `				}` |
|        - |  5568 | `				/* Safely unreference the object */` |
|     3822 |  5569 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5570 | `			}` |
|     2118 |  5571 | `		}else{` |
|      ! 0 |  5572 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5573 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5574 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5575 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5576 | `		}` |
|     2118 |  5577 | `	}else{` |
|        - |  5578 | `		/* Static member access using class name */` |
|      138 |  5579 | `		pNos = pTos;` |
|      138 |  5580 | `		pThis = 0;` |
|      138 |  5581 | `		if( !pInstr->p3 ){` |
|      126 |  5582 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      126 |  5583 | `			pNos--;` |
|        - |  5584 | `#ifdef UNTRUST` |
|        - |  5585 | `			if( pNos < pStack ){` |
|        - |  5586 | `				goto Abort;` |
|        - |  5587 | `			}` |
|        - |  5588 | `#endif` |
|       64 |  5589 | `		}else{` |
|        - |  5590 | `			/* Attribute name already computed */` |
|       14 |  5591 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5592 | `		}` |
|      138 |  5593 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      138 |  5594 | `			ph7_class *pClass = 0;` |
|      138 |  5595 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5596 | `				/* Class already instantiated */` |
|      ! 0 |  5597 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5598 | `				pClass = pThis->pClass;` |
|      ! 0 |  5599 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5600 | `			}else{` |
|        - |  5601 | `				/* Try to extract the target class */` |
|      138 |  5602 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      138 |  5603 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      138 |  5604 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5605 | `					/* Handle self/static/parent keywords */` |
|      138 |  5606 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5607 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5608 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5609 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5610 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5611 | `						}` |
|      124 |  5612 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5613 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      109 |  5614 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5615 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5616 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5617 | `							pClass = pSelf->pBase;` |
|        6 |  5618 | `						}` |
|        8 |  5619 | `					}else{` |
|       84 |  5620 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5621 | `					}` |
|       68 |  5622 | `				}` |
|        - |  5623 | `			}` |
|      138 |  5624 | `			if( pClass == 0 ){` |
|        - |  5625 | `				/* Undefined class */` |
|      ! 0 |  5626 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5627 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5628 | `					);` |
|      ! 0 |  5629 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5630 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5631 | `				}` |
|      ! 0 |  5632 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5633 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5634 | `			}else{` |
|      138 |  5635 | `				if( pInstr->iP2 ){` |
|        - |  5636 | `					/* Method call */` |
|       68 |  5637 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5638 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5639 | `						/* Extract the target method */` |
|       68 |  5640 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5641 | `					}` |
|       68 |  5642 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5643 | `						if( pMeth ){` |
|      ! 0 |  5644 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5645 | `								&pClass->sName,&sName` |
|        - |  5646 | `								);` |
|      ! 0 |  5647 | `						}else{` |
|      ! 0 |  5648 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5649 | `								&pClass->sName,&sName` |
|        - |  5650 | `								);` |
|        - |  5651 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5652 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5653 | `						}` |
|        - |  5654 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5655 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5656 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5657 | `						}` |
|      ! 0 |  5658 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5659 | `					}else{` |
|        - |  5660 | `						/* Push method name on the stack */` |
|       68 |  5661 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5662 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5663 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5664 | `					}` |
|       68 |  5665 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5666 | `				}else{` |
|        - |  5667 | `					/* Attribute access */` |
|       72 |  5668 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5669 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5670 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5671 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5672 | `						/* ::class returns the fully qualified class name */` |
|        - |  5673 | `						/* Pop the attribute name from the stack */` |
|       54 |  5674 | `						if( !pInstr->p3 ){` |
|       54 |  5675 | `							VmPopOperand(&pTos,1);` |
|       26 |  5676 | `						}` |
|       54 |  5677 | `						PH7_MemObjRelease(pTos);` |
|        - |  5678 | `						/* Load the class name */` |
|       54 |  5679 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5680 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5681 | `					}else{` |
|        - |  5682 | `						/* Extract the target attribute */` |
|       20 |  5683 | `						if( sName.nByte > 0 ){` |
|       20 |  5684 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5685 | `						}` |
|       20 |  5686 | `						if( pAttr == 0 ){` |
|        - |  5687 | `							/* No such attribute,load null */` |
|      ! 0 |  5688 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5689 | `								&pClass->sName,&sName);` |
|        - |  5690 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5691 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5692 | `						}` |
|        - |  5693 | `						/* Pop the attribute name from the stack */` |
|       20 |  5694 | `						if( !pInstr->p3 ){` |
|        7 |  5695 | `							VmPopOperand(&pTos,1);` |
|        3 |  5696 | `						}` |
|       20 |  5697 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5698 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5699 | `						if( pAttr ){` |
|       20 |  5700 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5701 | `								/* Access to a non static attribute */` |
|      ! 0 |  5702 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5703 | `									&pClass->sName,&pAttr->sName` |
|        - |  5704 | `									);` |
|      ! 0 |  5705 | `							}else{` |
|        - |  5706 | `								ph7_value *pValue;` |
|        - |  5707 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5708 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5709 | `									/* Load the desired attribute */` |
|       20 |  5710 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5711 | `									if( pValue ){` |
|       20 |  5712 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5713 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5714 | `											/* Load index number */` |
|       14 |  5715 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5716 | `										}` |
|        9 |  5717 | `									}` |
|        9 |  5718 | `								}` |
|        - |  5719 | `							}` |
|        9 |  5720 | `						}` |
|        - |  5721 | `					}` |
|        - |  5722 | `				}` |
|      138 |  5723 | `				if( pThis ){` |
|        - |  5724 | `					/* Safely unreference the object */` |
|      ! 0 |  5725 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5726 | `				}` |
|        - |  5727 | `			}` |
|       70 |  5728 | `		}else{` |
|        - |  5729 | `			/* Pop operands */` |
|      ! 0 |  5730 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5731 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5732 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5733 | `			}` |
|      ! 0 |  5734 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5735 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5736 | `		}` |
|        - |  5737 | `	}` |
|     4370 |  5738 | `	break;` |
|        - |  5739 | `					}` |
|        - |  5740 | `/*` |
|        - |  5741 | ` * OP_NEW P1 * * *` |
|        - |  5742 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5743 | ` */` |
|      321 |  5744 | `case PH7_OP_NEW: {` |
|      644 |  5745 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      644 |  5746 | `	ph7_class *pClass = 0;` |
|        - |  5747 | `	ph7_class_instance *pNew;` |
|      644 |  5748 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5749 | `		/* Try to extract the desired class */` |
|      965 |  5750 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      642 |  5751 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      321 |  5752 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5753 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5754 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5755 | `	}` |
|      644 |  5756 | `	if( pClass == 0 ){` |
|        - |  5757 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5758 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5759 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5760 | `			);` |
|        - |  5761 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5762 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5763 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5764 | `			/* Pop given arguments */` |
|      ! 0 |  5765 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5766 | `		}` |
|      ! 0 |  5767 | `		goto Abort;` |
|      ! 0 |  5768 | `	}else{` |
|        - |  5769 | `		ph7_class_method *pCons;` |
|        - |  5770 | `		/* Create a new class instance */` |
|      644 |  5771 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      644 |  5772 | `		if( pNew == 0 ){` |
|      ! 0 |  5773 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5774 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5775 | `				&pClass->sName` |
|        - |  5776 | `			);` |
|      ! 0 |  5777 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5778 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5779 | `				/* Pop given arguments */` |
|      ! 0 |  5780 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5781 | `			}` |
|      ! 0 |  5782 | `			break;` |
|        - |  5783 | `		}` |
|        - |  5784 | `		/* Check if a constructor is available */` |
|      644 |  5785 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      644 |  5786 | `		if( pCons == 0 ){` |
|      534 |  5787 | `			SyString *pName = &pClass->sName;` |
|        - |  5788 | `			/* Check for a constructor with the same base class name */` |
|      534 |  5789 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      266 |  5790 | `		}` |
|      644 |  5791 | `		if( pCons ){` |
|        - |  5792 | `			/* Call the class constructor */` |
|      112 |  5793 | `			SySetReset(&aArg);` |
|      212 |  5794 | `			while( pArg < pTos ){` |
|      102 |  5795 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      102 |  5796 | `				pArg++;` |
|        2 |  5797 | `			}` |
|      112 |  5798 | `			if( pVm->bErrReport ){` |
|        - |  5799 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5800 | `				sxu32 n;` |
|       69 |  5801 | `				n = SySetUsed(&aArg);` |
|        - |  5802 | `				/* Emit a notice for missing arguments */` |
|      125 |  5803 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       57 |  5804 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       57 |  5805 | `					if( pFuncArg ){` |
|       57 |  5806 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5807 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5808 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5809 | `						}` |
|       28 |  5810 | `					}` |
|       57 |  5811 | `					n++;` |
|        1 |  5812 | `				}` |
|       34 |  5813 | `			}` |
|      112 |  5814 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5815 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      112 |  5816 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5817 | `				pNew->iRef = 1;` |
|      ! 0 |  5818 | `			}` |
|       55 |  5819 | `		}` |
|      644 |  5820 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5821 | `			/* Pop given arguments */` |
|       94 |  5822 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       46 |  5823 | `		}` |
|      644 |  5824 | `		PH7_MemObjRelease(pTos);` |
|      644 |  5825 | `		pTos->x.pOther = pNew;` |
|      644 |  5826 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5827 | `	}` |
|      644 |  5828 | `	break;` |
|        - |  5829 | `				 }` |
|        - |  5830 | `/*` |
|        - |  5831 | ` * OP_CLONE * * *` |
|        - |  5832 | ` * Perfome a clone operation.` |
|        - |  5833 | ` */` |
|       23 |  5834 | `case PH7_OP_CLONE: {` |
|        - |  5835 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5836 | `#ifdef UNTRUST` |
|        - |  5837 | `	if( pTos < pStack ){` |
|        - |  5838 | `		goto Abort;` |
|        - |  5839 | `	}` |
|        - |  5840 | `#endif` |
|        - |  5841 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5842 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5843 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5844 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5845 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5846 | `		break;` |
|        - |  5847 | `	}` |
|        - |  5848 | `	/* Point to the source */` |
|       44 |  5849 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5850 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5851 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5852 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5853 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5854 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5855 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5856 | `		break;` |
|        - |  5857 | `	}` |
|        - |  5858 | `	/* Perform the clone operation */` |
|       44 |  5859 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5860 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5861 | `	if( pClone == 0 ){` |
|      ! 0 |  5862 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5863 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5864 | `	}else{` |
|        - |  5865 | `		/* Load the cloned object */` |
|       44 |  5866 | `		pTos->x.pOther = pClone;` |
|       44 |  5867 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5868 | `	}` |
|       44 |  5869 | `	break;` |
|        - |  5870 | `				   }` |
|        - |  5871 | `/*` |
|        - |  5872 | ` * OP_SWITCH * * P3` |
|        - |  5873 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5874 | ` */` |
|       18 |  5875 | `case PH7_OP_SWITCH: {` |
|       38 |  5876 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5877 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5878 | `	ph7_value sValue,sCaseValue;` |
|        - |  5879 | `	sxu32 n,nEntry;` |
|        - |  5880 | `#ifdef UNTRUST` |
|        - |  5881 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5882 | `		goto Abort;` |
|        - |  5883 | `	}` |
|        - |  5884 | `#endif` |
|        - |  5885 | `	/* Point to the case table  */` |
|       38 |  5886 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5887 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5888 | `	/* Select the appropriate case block to execute */` |
|       38 |  5889 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5890 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5891 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5892 | `		pCase = &aCase[n];` |
|       92 |  5893 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5894 | `		/* Execute the case expression first */` |
|       92 |  5895 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5896 | `		/* Compare the two expression */` |
|       92 |  5897 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5898 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5899 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5900 | `		if( rc == 0 ){` |
|        - |  5901 | `			/* Value match,jump to this block */` |
|       38 |  5902 | `			pc = pCase->nStart - 1;` |
|       38 |  5903 | `			break;` |
|        - |  5904 | `		}` |
|       29 |  5905 | `	}` |
|       38 |  5906 | `	VmPopOperand(&pTos,1);` |
|       38 |  5907 | `	if( n >= nEntry ){` |
|        - |  5908 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5909 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5910 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5911 | `		}else{` |
|        - |  5912 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5913 | `			pc = pSwitch->nOut - 1;` |
|        - |  5914 | `		}` |
|      ! 0 |  5915 | `	}` |
|       38 |  5916 | `	break;` |
|        - |  5917 | `					}` |
|        - |  5918 | `/*` |
|        - |  5919 | ` * OP_YIELD P1 P2 *` |
|        - |  5920 | ` *  Yield a value from a generator function.` |
|        - |  5921 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  5922 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  5923 | ` */` |
|       28 |  5924 | `case PH7_OP_YIELD: {` |
|        - |  5925 | `	ph7_generator *pGen;` |
|       57 |  5926 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  5927 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  5928 | `		goto Abort;` |
|        - |  5929 | `	}` |
|       57 |  5930 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       57 |  5931 | `	if( pInstr->iP2 ){` |
|        - |  5932 | `		/* yield $key => $value: value on top, key below */` |
|        - |  5933 | `#ifdef UNTRUST` |
|        - |  5934 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  5935 | `#endif` |
|        7 |  5936 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  5937 | `		VmPopOperand(&pTos, 1);` |
|        7 |  5938 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  5939 | `		VmPopOperand(&pTos, 1);` |
|        - |  5940 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  5941 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5942 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  5943 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  5944 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  5945 | `			}` |
|        1 |  5946 | `		}` |
|       54 |  5947 | `	}else if( pInstr->iP1 ){` |
|        - |  5948 | `		/* yield $value */` |
|        - |  5949 | `#ifdef UNTRUST` |
|        - |  5950 | `		if( pTos < pStack ) goto Abort;` |
|        - |  5951 | `#endif` |
|       51 |  5952 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       51 |  5953 | `		VmPopOperand(&pTos, 1);` |
|        - |  5954 | `		/* Auto-increment key */` |
|       51 |  5955 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       51 |  5956 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       51 |  5957 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       26 |  5958 | `	}else{` |
|        - |  5959 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  5960 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  5961 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  5962 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  5963 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  5964 | `	}` |
|        - |  5965 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       57 |  5966 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       57 |  5967 | `	goto Suspend;` |
|        - |  5968 |  |
|        - |  5969 | `/*` |
|        - |  5970 | ` * OP_CALL P1 * *` |
|        - |  5971 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5972 | ` *  function on the stack.` |
|        - |  5973 | ` */` |
|   291458 |  5974 | `case PH7_OP_CALL: {` |
|   582962 |  5975 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5976 | `	SyHashEntry *pEntry;` |
|        - |  5977 | `	SyString sName;` |
|        - |  5978 | `	/* Extract function name */` |
|   582962 |  5979 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5980 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5981 | `			ph7_value sResult;` |
|      ! 0 |  5982 | `			SySetReset(&aArg);` |
|      ! 0 |  5983 | `			while( pArg < pTos ){` |
|      ! 0 |  5984 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5985 | `				pArg++;` |
|      ! 0 |  5986 | `			}` |
|      ! 0 |  5987 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5988 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5989 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5990 | `			SySetReset(&aArg);` |
|        - |  5991 | `			/* Pop given arguments */` |
|      ! 0 |  5992 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5993 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5994 | `			}` |
|        - |  5995 | `			/* Copy result */` |
|      ! 0 |  5996 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5997 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5998 | `		}else{` |
|        3 |  5999 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6000 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6001 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6002 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6003 | `			}else{` |
|        - |  6004 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6005 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6006 | `			}` |
|        - |  6007 | `			/* Pop given arguments */` |
|        3 |  6008 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6009 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6010 | `			}` |
|        - |  6011 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6012 | `			PH7_MemObjRelease(pTos);` |
|        - |  6013 | `		}` |
|   291185 |  6014 | `		break;` |
|        - |  6015 | `	}` |
|   582960 |  6016 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6017 | `	/* Check for a compiled function first.` |
|        - |  6018 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6019 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   582960 |  6020 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6021 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6022 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6023 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6024 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6025 | `	 * function calls inside namespaces. */` |
|   582960 |  6026 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6027 | `		const char *zFunc;` |
|        - |  6028 | `		const char *zEnd;` |
|        - |  6029 | `		const char *z;` |
|        - |  6030 | `		SyString sGlobal;` |
|       15 |  6031 | `		zFunc = sName.zString;` |
|       15 |  6032 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6033 | `		z = zEnd;` |
|        - |  6034 | `		/* Find last namespace separator */` |
|      133 |  6035 | `		while( z > zFunc ){` |
|      133 |  6036 | `			if( z[-1] == '\\' ){` |
|       15 |  6037 | `				break;` |
|        - |  6038 | `			}` |
|      119 |  6039 | `			z--;` |
|        1 |  6040 | `		}` |
|       15 |  6041 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6042 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6043 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6044 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6045 | `		}` |
|        7 |  6046 | `	}` |
|   582960 |  6047 | `	if( pEntry ){` |
|        - |  6048 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6049 | `		ph7_class_instance *pThis;` |
|        - |  6050 | `		ph7_value *pFrameStack;` |
|        - |  6051 | `		ph7_vm_func *pVmFunc;` |
|        - |  6052 | `		ph7_class *pSelf;` |
|        - |  6053 | `		VmFrame *pFrame;` |
|        - |  6054 | `		ph7_value *pObj;` |
|        - |  6055 | `		VmSlot sArg;` |
|        - |  6056 | `		sxu32 n;` |
|        - |  6057 | `		/* initialize fields */` |
|    13306 |  6058 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13306 |  6059 | `		pThis = 0;` |
|    13306 |  6060 | `		pSelf = 0;` |
|    13306 |  6061 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6062 | `			ph7_class_method *pMeth;` |
|        - |  6063 | `			/* Class method call */` |
|     1964 |  6064 | `			ph7_value *pTarget = &pTos[-1];` |
|     1964 |  6065 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6066 | `				/* Extract the 'this' pointer */` |
|     1964 |  6067 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6068 | `					/* Instance already loaded */` |
|     1892 |  6069 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1892 |  6070 | `					pThis->iRef++;` |
|     1892 |  6071 | `					pSelf = pThis->pClass;` |
|      945 |  6072 | `				}` |
|     1964 |  6073 | `				if( pSelf == 0 ){` |
|       74 |  6074 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6075 | `						/* "Late Static Binding" class name */` |
|      101 |  6076 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6077 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6078 | `					}` |
|       74 |  6079 | `					if( pSelf == 0 ){` |
|       13 |  6080 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6081 | `					}` |
|       36 |  6082 | `				}` |
|     1964 |  6083 | `				if( pThis == 0  ){` |
|       74 |  6084 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6085 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6086 | `					if( pFrameLocal->pParent ){` |
|        - |  6087 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6088 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6089 | `						if( pThis ){` |
|       13 |  6090 | `							pThis->iRef++;` |
|        6 |  6091 | `						}` |
|       28 |  6092 | `					}` |
|       36 |  6093 | `				}` |
|     1964 |  6094 | `				VmPopOperand(&pTos,1);` |
|     1964 |  6095 | `				PH7_MemObjRelease(pTos);` |
|        - |  6096 | `				/* Synchronize pointers */` |
|     1964 |  6097 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  6098 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6099 | `				 * user have already computed the random generated unique class method name` |
|        - |  6100 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6101 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6102 | `				 */` |
|     1964 |  6103 | `				while( pArg < pStack ){` |
|      ! 0 |  6104 | `					pArg++;` |
|      ! 0 |  6105 | `				}` |
|     1964 |  6106 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6107 | `					/* Check if the call is allowed */` |
|     1964 |  6108 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1964 |  6109 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6110 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6111 | `							/* Pop given arguments */` |
|      ! 0 |  6112 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6113 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6114 | `							}` |
|        - |  6115 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6116 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6117 | `							break;` |
|        - |  6118 | `						}` |
|        3 |  6119 | `					}` |
|      981 |  6120 | `				}` |
|      981 |  6121 | `			}` |
|      981 |  6122 | `		}` |
|        - |  6123 | `		/* Check The recursion limit */` |
|    13306 |  6124 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6125 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6126 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6127 | `				&pVmFunc->sName);` |
|        - |  6128 | `			/* Pop given arguments */` |
|        3 |  6129 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6130 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6131 | `			}` |
|        - |  6132 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6133 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6134 | `			break;` |
|        - |  6135 | `		}` |
|    13304 |  6136 | `		if( pVmFunc->pNextName ){` |
|        - |  6137 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6138 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6139 | `		}` |
|    13304 |  6140 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6141 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6142 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6143 | `			ph7_generator *pGenerator;` |
|        - |  6144 | `			ph7_class_instance *pGenObj;` |
|        - |  6145 | `			ph7_value *pCtxAttr;` |
|        - |  6146 | `			SyString sAttrName;` |
|        - |  6147 | `			ph7_value **apCallArgs;` |
|        - |  6148 | `			int nCallArgs, iArg;` |
|        - |  6149 | `			/* Collect arguments from the operand stack */` |
|       19 |  6150 | `			nCallArgs = (int)(pTos - pArg);` |
|       19 |  6151 | `			apCallArgs = 0;` |
|       19 |  6152 | `			if( nCallArgs > 0 ){` |
|        7 |  6153 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6154 | `					nCallArgs * sizeof(ph7_value *));` |
|        5 |  6155 | `				if( apCallArgs == 0 ){` |
|        - |  6156 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6157 | `					nCallArgs = 0;` |
|      ! 0 |  6158 | `				}else{` |
|       11 |  6159 | `					for( iArg = 0; iArg < nCallArgs; iArg++ ){` |
|        7 |  6160 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        4 |  6161 | `					}` |
|        - |  6162 | `				}` |
|        2 |  6163 | `			}` |
|        - |  6164 | `			/* Create execution context and generator wrapper */` |
|       19 |  6165 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       19 |  6166 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6167 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6168 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6169 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6170 | `				break;` |
|        - |  6171 | `			}` |
|       19 |  6172 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       19 |  6173 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6174 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6175 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6176 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6177 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6178 | `				break;` |
|        - |  6179 | `			}` |
|        - |  6180 | `			/* Set up the frame with arguments, closure env, $this */` |
|       19 |  6181 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       19 |  6182 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       19 |  6183 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nCallArgs, apCallArgs);` |
|       19 |  6184 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       19 |  6185 | `			pExecCtx->pFrame->pParent = 0;` |
|       19 |  6186 | `			if( apCallArgs ){` |
|        5 |  6187 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6188 | `			}` |
|       19 |  6189 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6190 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6191 | `				if( pThis ){` |
|      ! 0 |  6192 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6193 | `				}` |
|      ! 0 |  6194 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6195 | `					goto Abort;` |
|        - |  6196 | `				}` |
|      ! 0 |  6197 | `				break;` |
|        - |  6198 | `			}` |
|        - |  6199 | `			/* Create Generator class instance */` |
|       19 |  6200 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       19 |  6201 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6202 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6203 | `				break;` |
|        - |  6204 | `			}` |
|        - |  6205 | `			/* Store generator in __ctx attribute */` |
|       19 |  6206 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       19 |  6207 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       19 |  6208 | `			if( pCtxAttr ){` |
|       19 |  6209 | `				pCtxAttr->x.pOther = pGenerator;` |
|       19 |  6210 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6211 | `			}` |
|        - |  6212 | `			/* Pop args and function name, push Generator object */` |
|       19 |  6213 | `			PH7_MemObjRelease(pTos);` |
|       19 |  6214 | `			pTos = &pTos[-pInstr->iP1];` |
|       19 |  6215 | `			pTos->x.pOther = pGenObj;` |
|       19 |  6216 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 |  6217 | `			pGenObj->iRef++;` |
|       19 |  6218 | `			if( pThis ){` |
|      ! 0 |  6219 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6220 | `			}` |
|       19 |  6221 | `			break;` |
|        - |  6222 | `		}` |
|        - |  6223 | `		/* Extract the formal argument set */` |
|    13286 |  6224 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6225 | `		/* Create a new VM frame  */` |
|    13286 |  6226 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13286 |  6227 | `		if( rc != SXRET_OK ){` |
|        - |  6228 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6229 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6230 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6231 | `				&pVmFunc->sName);` |
|        - |  6232 | `			/* Pop given arguments */` |
|      ! 0 |  6233 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6234 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6235 | `			}` |
|        - |  6236 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6237 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6238 | `			break;` |
|        - |  6239 | `		}` |
|    13286 |  6240 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6241 | `			/* Install the '$this' variable */` |
|        - |  6242 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1902 |  6243 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1902 |  6244 | `			if( pObj ){` |
|        - |  6245 | `				/* Reflect the change */` |
|     1902 |  6246 | `				pObj->x.pOther = pThis;` |
|     1902 |  6247 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      950 |  6248 | `			}` |
|      950 |  6249 | `		}` |
|    13286 |  6250 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6251 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6252 | `			/* Install static variables */` |
|      ! 0 |  6253 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6254 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6255 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6256 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6257 | `					/* Initialize the static variables */` |
|      ! 0 |  6258 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6259 | `					if( pObj ){` |
|        - |  6260 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6261 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6262 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6263 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6264 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6265 | `						}` |
|      ! 0 |  6266 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6267 | `					}else{` |
|      ! 0 |  6268 | `						continue;` |
|        - |  6269 | `					}` |
|      ! 0 |  6270 | `				}` |
|        - |  6271 | `				/* Install in the current frame */` |
|      ! 0 |  6272 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6273 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6274 | `			}` |
|      ! 0 |  6275 | `		}` |
|        - |  6276 | `		/* Push arguments in the local frame */` |
|    13286 |  6277 | `		n = 0;` |
|    36144 |  6278 | `		while( pArg < pTos ){` |
|    22860 |  6279 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22706 |  6280 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6281 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  6282 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6283 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6284 | `						goto Abort;` |
|        - |  6285 | `					}` |
|      ! 0 |  6286 | `				}` |
|        - |  6287 | `				/* Make sure the given arguments are of the correct type */` |
|    22706 |  6288 | `				if( aFormalArg[n].nType > 0 ){` |
|     1112 |  6289 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6290 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  6291 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6292 | `						ph7_class *pClass;` |
|        - |  6293 | `						/* Try to extract the desired class */` |
|      ! 0 |  6294 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  6295 | `						if( pClass ){` |
|      ! 0 |  6296 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6297 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6298 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6299 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6300 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6301 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6302 | `								}` |
|      ! 0 |  6303 | `							}else{` |
|        - |  6304 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6305 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6306 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6307 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6308 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6309 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6310 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6311 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6312 | `								}` |
|        - |  6313 | `							}` |
|      ! 0 |  6314 | `						}` |
|     1112 |  6315 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6316 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6317 | `						/* Cast to the desired type */` |
|      ! 0 |  6318 | `						xCast(pArg);` |
|      ! 0 |  6319 | `					}` |
|      555 |  6320 | `				}` |
|    22706 |  6321 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6322 | `					/* Pass by reference */` |
|       50 |  6323 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6324 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6325 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6326 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6327 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6328 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6329 | `						}` |
|        - |  6330 | `						/* Switch to pass by value */` |
|      ! 0 |  6331 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6332 | `					}else{` |
|        - |  6333 | `						SyHashEntry *pRefEntry;` |
|        - |  6334 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6335 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6336 | `						if( pRefEntry == 0 ){` |
|       74 |  6337 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6338 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6339 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6340 | `							sArg.pUserData = 0;` |
|       50 |  6341 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6342 | `						}` |
|       50 |  6343 | `						pObj = 0;` |
|        - |  6344 | `					}` |
|       26 |  6345 | `				}else{` |
|        - |  6346 | `					/* Pass by value,make a copy of the given argument */` |
|    22658 |  6347 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6348 | `				}` |
|    11354 |  6349 | `			}else{` |
|        - |  6350 | `				char zName[32];` |
|        - |  6351 | `				SyString sArgName;` |
|        - |  6352 | `				/* Set a dummy name */` |
|      156 |  6353 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6354 | `				sArgName.zString = zName;` |
|        - |  6355 | `				/* Annonymous argument */` |
|      156 |  6356 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6357 | `			}` |
|    22860 |  6358 | `			if( pObj ){` |
|    22812 |  6359 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6360 | `				/* Insert argument index  */` |
|    22812 |  6361 | `				sArg.nIdx = pObj->nIdx;` |
|    22812 |  6362 | `				sArg.pUserData = 0;` |
|    22812 |  6363 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11405 |  6364 | `			}` |
|    22860 |  6365 | `			PH7_MemObjRelease(pArg);` |
|    22860 |  6366 | `			pArg++;` |
|    22860 |  6367 | `			++n;` |
|        2 |  6368 | `		}` |
|        - |  6369 | `		/* Set up closure environment */` |
|    13286 |  6370 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6371 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6372 | `			ph7_value *pValue;` |
|        - |  6373 | `			sxu32 iEnv;` |
|       11 |  6374 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6375 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6376 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6377 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6378 | `					/* Do not install null value */` |
|       11 |  6379 | `					continue;` |
|        - |  6380 | `				}` |
|       11 |  6381 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6382 | `				if( pValue == 0 ){` |
|      ! 0 |  6383 | `					continue;` |
|        - |  6384 | `				}` |
|        - |  6385 | `				/* Invalidate any prior representation */` |
|       11 |  6386 | `				PH7_MemObjRelease(pValue);` |
|        - |  6387 | `				/* Duplicate bound variable value */` |
|       11 |  6388 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6389 | `			}` |
|        5 |  6390 | `		}` |
|        - |  6391 | `		/* Process default values */` |
|    15220 |  6392 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1936 |  6393 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1930 |  6394 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1930 |  6395 | `				if( pObj ){` |
|        - |  6396 | `					/* Evaluate the default value and extract it's result */` |
|     1930 |  6397 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1930 |  6398 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6399 | `						goto Abort;` |
|        - |  6400 | `					}` |
|        - |  6401 | `					/* Insert argument index */` |
|     1930 |  6402 | `					sArg.nIdx = pObj->nIdx;` |
|     1930 |  6403 | `					sArg.pUserData = 0;` |
|     1930 |  6404 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6405 | `					/* Make sure the default argument is of the correct type */` |
|     1930 |  6406 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6407 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6408 | `						/* Cast to the desired type */` |
|      ! 0 |  6409 | `						xCast(pObj);` |
|      ! 0 |  6410 | `					}` |
|      964 |  6411 | `				}` |
|      964 |  6412 | `			}` |
|     1936 |  6413 | `			++n;` |
|        2 |  6414 | `		}` |
|        - |  6415 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6416 | `		 * does not return anything.` |
|        - |  6417 | `		 */` |
|    13286 |  6418 | `		PH7_MemObjRelease(pTos);` |
|    13286 |  6419 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6420 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13286 |  6421 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13286 |  6422 | `		if( pFrameStack == 0 ){` |
|        - |  6423 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6424 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6425 | `				&pVmFunc->sName);` |
|      ! 0 |  6426 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6427 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6428 | `			}` |
|      ! 0 |  6429 | `			break;` |
|        - |  6430 | `		}` |
|    13286 |  6431 | `		if( pSelf ){` |
|        - |  6432 | `			/* Push class name */` |
|     1962 |  6433 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      980 |  6434 | `		}` |
|        - |  6435 | `		/* Increment nesting level */` |
|    13286 |  6436 | `		pVm->nRecursionDepth++;` |
|        - |  6437 | `		/* Execute function body */` |
|    13286 |  6438 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6439 | `		/* Decrement nesting level */` |
|    13286 |  6440 | `		pVm->nRecursionDepth--;` |
|    13286 |  6441 | `		if( pSelf ){` |
|        - |  6442 | `			/* Pop class name */` |
|     1962 |  6443 | `			(void)SySetPop(&pVm->aSelf);` |
|      980 |  6444 | `		}` |
|        - |  6445 | `		/* Cleanup the mess left behind */` |
|    13286 |  6446 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6447 | `			/* Return by reference,reflect that */` |
|        9 |  6448 | `			if( n != SXU32_HIGH ){` |
|        9 |  6449 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6450 | `				sxu32 i;` |
|        - |  6451 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6452 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6453 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6454 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6455 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6456 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6457 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6458 | `								&pVmFunc->sName);` |
|      ! 0 |  6459 | `						}` |
|      ! 0 |  6460 | `						n = SXU32_HIGH;` |
|      ! 0 |  6461 | `						break;` |
|        - |  6462 | `					}` |
|        3 |  6463 | `				}` |
|        5 |  6464 | `			}else{` |
|      ! 0 |  6465 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6466 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6467 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6468 | `						&pVmFunc->sName);` |
|      ! 0 |  6469 | `				}` |
|        - |  6470 | `			}` |
|        9 |  6471 | `			pTos->nIdx = n;` |
|        4 |  6472 | `		}` |
|        - |  6473 | `		/* Cleanup the mess left behind */` |
|    13286 |  6474 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6475 | `			/* An exception was throw in this frame */` |
|       12 |  6476 | `			pFrame = pFrame->pParent;` |
|       12 |  6477 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6478 | `				/* Pop the resutlt */` |
|       10 |  6479 | `				VmPopOperand(&pTos,1);` |
|        - |  6480 | `				/* Jump to this destination */` |
|       10 |  6481 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6482 | `				rc = PH7_OK;` |
|        6 |  6483 | `			}else{` |
|        3 |  6484 | `				if( pFrame->pParent ){` |
|        3 |  6485 | `					rc = PH7_EXCEPTION;` |
|        2 |  6486 | `				}else{` |
|        - |  6487 | `					/* Continue normal execution */` |
|      ! 0 |  6488 | `					rc = PH7_OK;` |
|        - |  6489 | `				}` |
|        - |  6490 | `			}` |
|        5 |  6491 | `		}` |
|        - |  6492 | `		/* Free the operand stack */` |
|    13286 |  6493 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6494 | `		/* Leave the frame */` |
|    13286 |  6495 | `		VmLeaveFrame(&(*pVm));` |
|    13286 |  6496 | `		if( rc == PH7_ABORT ){` |
|        - |  6497 | `			/* Abort processing immeditaley */` |
|        7 |  6498 | `			goto Abort;` |
|    13280 |  6499 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6500 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6501 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6502 | `			 * overwriting the state saved by the inner level.` |
|        - |  6503 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6504 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       39 |  6505 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       39 |  6506 | `			goto Suspend;` |
|    13242 |  6507 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6508 | `			goto Exception;` |
|        - |  6509 | `		}` |
|     6621 |  6510 | `	}else{` |
|        - |  6511 | `		ph7_user_func *pFunc;` |
|        - |  6512 | `		ph7_context sCtx;` |
|        - |  6513 | `		ph7_value sRet;` |
|        - |  6514 | `		/* Look for an installed foreign function.` |
|        - |  6515 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6516 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6517 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6518 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6519 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6520 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   569656 |  6521 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   569656 |  6522 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6523 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6524 | `			const char *zShort = sName.zString;` |
|        - |  6525 | `			sxu32 i;` |
|      217 |  6526 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6527 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6528 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6529 | `				}` |
|      102 |  6530 | `			}` |
|       15 |  6531 | `			if( zShort != sName.zString ){` |
|       15 |  6532 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6533 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6534 | `			}` |
|        7 |  6535 | `		}` |
|   569656 |  6536 | `		if( pEntry == 0 ){` |
|        - |  6537 | `			/* Call to undefined function */` |
|        5 |  6538 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6539 | `			/* Pop given arguments */` |
|        5 |  6540 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6541 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6542 | `			}` |
|        - |  6543 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6544 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6545 | `			break;` |
|        - |  6546 | `		}` |
|   569652 |  6547 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6548 | `		/* Start collecting function arguments */` |
|   569652 |  6549 | `		SySetReset(&aArg);` |
|  1528148 |  6550 | `		while( pArg < pTos ){` |
|   958498 |  6551 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   958498 |  6552 | `			pArg++;` |
|        2 |  6553 | `		}` |
|        - |  6554 | `		/* Assume a null return value */` |
|   569652 |  6555 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6556 | `		/* Init the call context */` |
|   569652 |  6557 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6558 | `		/* Call the foreign function */` |
|   569652 |  6559 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6560 | `		/* Release the call context */` |
|   569652 |  6561 | `		VmReleaseCallContext(&sCtx);` |
|   569652 |  6562 | `		if( rc == PH7_ABORT ){` |
|      463 |  6563 | `			goto Abort;` |
|   569190 |  6564 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6565 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6566 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6567 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6568 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6569 | `				goto Exception;` |
|        - |  6570 | `			}` |
|        - |  6571 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6572 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6573 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6574 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6575 | `			}` |
|        - |  6576 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6577 | `			VmPopOperand(&pTos,1);` |
|        - |  6578 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6579 | `			pFrm = pVm->pFrame;` |
|        7 |  6580 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6581 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6582 | `			}` |
|        7 |  6583 | `			break;` |
|        - |  6584 | `		}` |
|   569180 |  6585 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6586 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6587 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6588 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6589 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6590 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6591 | `			 * body), the user-function path above will handle re-saving. */` |
|       39 |  6592 | `			PH7_MemObjRelease(&sRet);` |
|       39 |  6593 | `			if( pInstr->iP1 > 0 ){` |
|       39 |  6594 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6595 | `			}` |
|        - |  6596 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6597 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       39 |  6598 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       39 |  6599 | `			goto Suspend;` |
|        - |  6600 | `		}` |
|   569142 |  6601 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6602 | `			/* Pop function name and arguments */` |
|   551506 |  6603 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   275774 |  6604 | `		}` |
|        - |  6605 | `		/* Save foreign function return value */` |
|   569142 |  6606 | `		PH7_MemObjStore(&sRet,pTos);` |
|   569142 |  6607 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6608 | `	}` |
|   582380 |  6609 | `	break;` |
|        - |  6610 | `				  }` |
|        - |  6611 | `/*` |
|        - |  6612 | ` * OP_CONSUME: P1 * *` |
|        - |  6613 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6614 | ` */` |
|    11401 |  6615 | `case PH7_OP_CONSUME: {` |
|    22804 |  6616 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22804 |  6617 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6618 |  |
|    22804 |  6619 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22804 |  6620 | `	pCur = pOut;` |
|        - |  6621 | `	/* Start the consume process  */` |
|    45606 |  6622 | `	while( pOut <= pTos ){` |
|        - |  6623 | `		/* Force a string cast */` |
|    22804 |  6624 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6625 | `			PH7_MemObjToString(pOut);` |
|      151 |  6626 | `		}` |
|    22804 |  6627 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6628 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6629 | `			/* Invoke the output consumer callback */` |
|    12626 |  6630 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12626 |  6631 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12626 |  6632 | `			SyBlobRelease(&pOut->sBlob);` |
|    12626 |  6633 | `			if( rc == SXERR_ABORT ){` |
|        - |  6634 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6635 | `				goto Abort;` |
|        - |  6636 | `			}` |
|     6312 |  6637 | `		}` |
|    22804 |  6638 | `		pOut++;` |
|        2 |  6639 | `	}` |
|    22804 |  6640 | `	pTos = &pCur[-1];` |
|    22802 |  6641 | `	break;` |
|        - |  6642 | `					 }` |
|        - |  6643 |  |
|        - |  6644 | `		} /* Switch() */` |
|  9893144 |  6645 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6646 | `	} /* For(;;) */` |
|    16138 |  6647 | `Done:` |
|    32278 |  6648 | `	SySetRelease(&aArg);` |
|    32278 |  6649 | `	return SXRET_OK;` |
|       66 |  6650 | `Suspend:` |
|      133 |  6651 | `	SySetRelease(&aArg);` |
|      133 |  6652 | `	return PH7_SUSPEND;` |
|      238 |  6653 | `Abort:` |
|      477 |  6654 | `	SySetRelease(&aArg);` |
|     1661 |  6655 | `	while( pTos >= pStack ){` |
|     1185 |  6656 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6657 | `		pTos--;` |
|        1 |  6658 | `	}` |
|      477 |  6659 | `	return PH7_ABORT;` |
|        3 |  6660 | `Exception:` |
|        8 |  6661 | `	SySetRelease(&aArg);` |
|       22 |  6662 | `	while( pTos >= pStack ){` |
|       16 |  6663 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6664 | `		pTos--;` |
|        2 |  6665 | `	}` |
|        8 |  6666 | `	return PH7_EXCEPTION;` |
|    16447 |  6667 |  |
|        - |  6668 | `/*` |
|        - |  6669 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6670 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6671 | ` * See block-comment on that function for additional information.` |
|        - |  6672 | ` */` |
|    15016 |  6673 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6674 |  |
|        - |  6675 | `	ph7_value *pStack;` |
|        - |  6676 | `	sxi32 rc;` |
|        - |  6677 | `	/* Allocate a new operand stack */` |
|    15018 |  6678 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15018 |  6679 | `	if( pStack == 0 ){` |
|      ! 0 |  6680 | `		return SXERR_MEM;` |
|        - |  6681 | `	}` |
|        - |  6682 | `	/* Execute the program */` |
|    15018 |  6683 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6684 | `	/* Free the operand stack */` |
|    15018 |  6685 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6686 | `	/* Execution result */` |
|    15018 |  6687 | `	return rc;` |
|     7510 |  6688 |  |
|        - |  6689 | `/*` |
|        - |  6690 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6691 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6692 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6693 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6694 | ` * execution ends.` |
|        - |  6695 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6696 | ` * additional information.` |
|        - |  6697 | ` */` |
|     2528 |  6698 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6699 |  |
|        - |  6700 | `	VmShutdownCB *pEntry;` |
|        - |  6701 | `	ph7_value *apArg[10];` |
|        - |  6702 | `	sxu32 n,nEntry;` |
|        - |  6703 | `	int i;` |
|        - |  6704 | `	/* Point to the stack of registered callbacks */` |
|     2530 |  6705 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27810 |  6706 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25282 |  6707 | `		apArg[i] = 0;` |
|    12642 |  6708 | `	}` |
|     2532 |  6709 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6710 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6711 | `		if( pEntry ){` |
|        - |  6712 | `			/* Prepare callback arguments if any */` |
|        3 |  6713 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6714 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6715 | `					break;` |
|        - |  6716 | `				}` |
|      ! 0 |  6717 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6718 | `			}` |
|        - |  6719 | `			/* Invoke the callback */` |
|        3 |  6720 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6721 | `			/*` |
|        - |  6722 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6723 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6724 | `			 */` |
|        3 |  6725 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6726 | `			if( pEntry ){` |
|        3 |  6727 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6728 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6729 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6730 | `				}` |
|        1 |  6731 | `			}` |
|        1 |  6732 | `		}` |
|        2 |  6733 | `	}` |
|     2530 |  6734 | `	SySetReset(&pVm->aShutdown);` |
|     2530 |  6735 |  |
|        - |  6736 | `/*` |
|        - |  6737 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6738 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6739 | ` * See block-comment on that function for additional information.` |
|        - |  6740 | ` */` |
|     2536 |  6741 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6742 |  |
|        - |  6743 | `	/* Make sure we are ready to execute this program */` |
|     2538 |  6744 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6745 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6746 | `	}` |
|        - |  6747 | `	/* Set the execution magic number  */` |
|     2538 |  6748 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6749 | `	/* Execute the program */` |
|     2538 |  6750 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6751 | `	/* Invoke any shutdown callbacks */` |
|     2534 |  6752 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6753 | `	/*` |
|        - |  6754 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6755 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6756 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6757 | `	 */` |
|     2534 |  6758 | `	return SXRET_OK;` |
|     1270 |  6759 |  |
|        - |  6760 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6761 | `/*` |
|        - |  6762 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6763 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6764 | ` */` |
|       42 |  6765 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        1 |  6766 |  |
|        - |  6767 | `	ph7_exec_ctx *pCtx;` |
|        - |  6768 | `	ph7_value *pStack;` |
|        - |  6769 | `	VmFrame *pFrame;` |
|       43 |  6770 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       43 |  6771 | `	if( pCtx == 0 ){` |
|      ! 0 |  6772 | `		return 0;` |
|        - |  6773 | `	}` |
|       43 |  6774 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       43 |  6775 | `	pCtx->pVm = pVm;` |
|       43 |  6776 | `	pCtx->pFunc = pFunc;` |
|       43 |  6777 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       43 |  6778 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       43 |  6779 | `	pCtx->pc = 0;` |
|       43 |  6780 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       43 |  6781 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6782 | `	/* Allocate a private operand stack */` |
|       43 |  6783 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       43 |  6784 | `	if( pStack == 0 ){` |
|      ! 0 |  6785 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6786 | `		return 0;` |
|        - |  6787 | `	}` |
|       43 |  6788 | `	pCtx->pStack = pStack;` |
|        - |  6789 | `	/* Create a detached frame for the fiber */` |
|       43 |  6790 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       43 |  6791 | `	if( pFrame == 0 ){` |
|      ! 0 |  6792 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6793 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6794 | `		return 0;` |
|        - |  6795 | `	}` |
|       43 |  6796 | `	pCtx->pFrame = pFrame;` |
|       43 |  6797 | `	return pCtx;` |
|       22 |  6798 |  |
|        - |  6799 | `/*` |
|        - |  6800 | ` * Start executing a fiber context for the first time.` |
|        - |  6801 | ` */` |
|       42 |  6802 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        1 |  6803 |  |
|        - |  6804 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6805 | `	sxi32 rc;` |
|       43 |  6806 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6807 | `		return SXERR_INVALID;` |
|        - |  6808 | `	}` |
|        - |  6809 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       43 |  6810 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       43 |  6811 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6812 | `	/* Save and set the active context */` |
|       43 |  6813 | `	pOldCtx = pVm->pActiveCtx;` |
|       43 |  6814 | `	pVm->pActiveCtx = pCtx;` |
|       43 |  6815 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       43 |  6816 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       43 |  6817 | `	pVm->nRecursionDepth++;` |
|        - |  6818 | `	/* Execute from the beginning */` |
|       64 |  6819 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6820 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       43 |  6821 | `	pVm->nRecursionDepth--;` |
|        - |  6822 | `	/* Restore the previous context */` |
|       43 |  6823 | `	pVm->pActiveCtx = pOldCtx;` |
|       43 |  6824 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6825 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       41 |  6826 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       41 |  6827 | `		pCtx->pFrame->pParent = 0;` |
|       41 |  6828 | `		if( pResult ){` |
|       23 |  6829 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6830 | `		}` |
|       41 |  6831 | `		return SXRET_OK;` |
|        - |  6832 | `	}` |
|        - |  6833 | `	/* Detach frame */` |
|        3 |  6834 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6835 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6836 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6837 | `	}` |
|        3 |  6838 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6839 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6840 | `		return PH7_ABORT;` |
|        - |  6841 | `	}` |
|        3 |  6842 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6843 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6844 | `		return PH7_EXCEPTION;` |
|        - |  6845 | `	}` |
|        - |  6846 | `	/* Normal completion */` |
|        3 |  6847 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6848 | `	if( pResult ){` |
|        3 |  6849 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6850 | `	}` |
|        3 |  6851 | `	return SXRET_OK;` |
|       22 |  6852 |  |
|        - |  6853 | `/*` |
|        - |  6854 | ` * Resume a suspended fiber context.` |
|        - |  6855 | ` */` |
|       86 |  6856 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        1 |  6857 |  |
|        - |  6858 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6859 | `	sxi32 rc;` |
|       87 |  6860 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  6861 | `		return SXERR_INVALID;` |
|        - |  6862 | `	}` |
|        - |  6863 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  6864 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  6865 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       87 |  6866 | `	if( pResumeValue ){` |
|       39 |  6867 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       20 |  6868 | `	}else{` |
|       49 |  6869 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  6870 | `	}` |
|       87 |  6871 | `	pCtx->nTos++;` |
|        - |  6872 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       87 |  6873 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       87 |  6874 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6875 | `	/* Save and set the active context */` |
|       87 |  6876 | `	pOldCtx = pVm->pActiveCtx;` |
|       87 |  6877 | `	pVm->pActiveCtx = pCtx;` |
|       87 |  6878 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       87 |  6879 | `	pVm->nRecursionDepth++;` |
|        - |  6880 | `	/* Resume execution from saved PC */` |
|      130 |  6881 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  6882 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       87 |  6883 | `	pVm->nRecursionDepth--;` |
|        - |  6884 | `	/* Restore the previous context */` |
|       87 |  6885 | `	pVm->pActiveCtx = pOldCtx;` |
|       87 |  6886 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6887 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       55 |  6888 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       55 |  6889 | `		pCtx->pFrame->pParent = 0;` |
|       55 |  6890 | `		if( pResult ){` |
|       17 |  6891 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  6892 | `		}` |
|       55 |  6893 | `		return SXRET_OK;` |
|        - |  6894 | `	}` |
|        - |  6895 | `	/* Detach frame */` |
|       33 |  6896 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       33 |  6897 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       33 |  6898 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  6899 | `	}` |
|       33 |  6900 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6901 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6902 | `		return PH7_ABORT;` |
|        - |  6903 | `	}` |
|       33 |  6904 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6905 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6906 | `		return PH7_EXCEPTION;` |
|        - |  6907 | `	}` |
|        - |  6908 | `	/* Normal completion */` |
|       33 |  6909 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       33 |  6910 | `	if( pResult ){` |
|       19 |  6911 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  6912 | `	}` |
|       33 |  6913 | `	return SXRET_OK;` |
|       44 |  6914 |  |
|        - |  6915 | `/*` |
|        - |  6916 | ` * Release an execution context and all its resources.` |
|        - |  6917 | ` */` |
|      ! 0 |  6918 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|      ! 0 |  6919 |  |
|      ! 0 |  6920 | `	if( pCtx == 0 ){` |
|      ! 0 |  6921 | `		return;` |
|        - |  6922 | `	}` |
|      ! 0 |  6923 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  6924 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  6925 | `		return;` |
|        - |  6926 | `	}` |
|      ! 0 |  6927 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  6928 | `	/* Release values */` |
|      ! 0 |  6929 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|      ! 0 |  6930 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  6931 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|      ! 0 |  6932 | `	if( pCtx->pFrame ){` |
|        - |  6933 | `		VmSlot *aSlot;` |
|        - |  6934 | `		sxu32 n;` |
|        - |  6935 | `		/* Free local variables */` |
|      ! 0 |  6936 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6937 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|      ! 0 |  6938 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      ! 0 |  6939 | `		}` |
|        - |  6940 | `		/* Remove local references */` |
|      ! 0 |  6941 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|      ! 0 |  6942 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|      ! 0 |  6943 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      ! 0 |  6944 | `		}` |
|      ! 0 |  6945 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|      ! 0 |  6946 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|      ! 0 |  6947 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6948 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|      ! 0 |  6949 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|      ! 0 |  6950 | `		pCtx->pFrame = 0;` |
|      ! 0 |  6951 | `	}` |
|        - |  6952 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  6953 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  6954 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|      ! 0 |  6955 | `	if( pCtx->pStack ){` |
|      ! 0 |  6956 | `		if( pCtx->nTos >= 0 ){` |
|      ! 0 |  6957 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|      ! 0 |  6958 | `			while( pTos >= pCtx->pStack ){` |
|      ! 0 |  6959 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6960 | `				pTos--;` |
|      ! 0 |  6961 | `			}` |
|      ! 0 |  6962 | `		}` |
|      ! 0 |  6963 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|      ! 0 |  6964 | `		pCtx->pStack = 0;` |
|      ! 0 |  6965 | `	}` |
|        - |  6966 | `	/* Free the context itself */` |
|      ! 0 |  6967 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6968 |  |
|        - |  6969 | `/*` |
|        - |  6970 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  6971 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  6972 | ` */` |
|       86 |  6973 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        1 |  6974 |  |
|        - |  6975 | `	ph7_class_instance *pThis;` |
|        - |  6976 | `	SyString sAttr;` |
|        - |  6977 | `	ph7_value *pAttr;` |
|       87 |  6978 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6979 | `		return 0;` |
|        - |  6980 | `	}` |
|       87 |  6981 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       87 |  6982 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  6983 | `		return 0;` |
|        - |  6984 | `	}` |
|       87 |  6985 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       87 |  6986 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       87 |  6987 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       31 |  6988 | `		return 0;` |
|        - |  6989 | `	}` |
|       57 |  6990 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       44 |  6991 |  |
|        - |  6992 | `/*` |
|        - |  6993 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  6994 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  6995 | ` */` |
|       38 |  6996 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  6997 |  |
|       39 |  6998 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 |  6999 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7000 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7001 | `			"Cannot suspend outside of a fiber");` |
|        - |  7002 | `	}` |
|       39 |  7003 | `	if( nArg > 0 ){` |
|       39 |  7004 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       20 |  7005 | `	}else{` |
|      ! 0 |  7006 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7007 | `	}` |
|       39 |  7008 | `	return PH7_SUSPEND;` |
|       20 |  7009 |  |
|        - |  7010 | `/*` |
|        - |  7011 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7012 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7013 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7014 | ` */` |
|       24 |  7015 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7016 |  |
|        - |  7017 | `	ph7_class_instance *pThis;` |
|        - |  7018 | `	ph7_value *pAttr;` |
|        - |  7019 | `	SyString sAttrName;` |
|       25 |  7020 | `	if( nArg < 2 ){` |
|      ! 0 |  7021 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7022 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7023 | `	}` |
|       25 |  7024 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7025 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7026 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7027 | `	}` |
|       25 |  7028 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       25 |  7029 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7030 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7031 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7032 | `	}` |
|        - |  7033 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       25 |  7034 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7035 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7036 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7037 | `	}` |
|        - |  7038 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       25 |  7039 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7040 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7041 | `	if( pAttr ){` |
|       25 |  7042 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7043 | `	}` |
|       25 |  7044 | `	return PH7_OK;` |
|       13 |  7045 |  |
|        - |  7046 | `/*` |
|        - |  7047 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7048 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7049 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7050 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7051 | ` */` |
|       24 |  7052 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7053 | `	ph7_class_instance **ppThis)` |
|        1 |  7054 |  |
|       25 |  7055 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7056 | `	ph7_value *pCallable;` |
|        - |  7057 | `	SyString sAttrName;` |
|       25 |  7058 | `	*ppThis = 0;` |
|       25 |  7059 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7060 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       25 |  7061 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7062 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7063 | `		return 0;` |
|        - |  7064 | `	}` |
|       25 |  7065 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7066 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7067 | `		SyString sName;` |
|        - |  7068 | `		SyHashEntry *pEntry;` |
|        - |  7069 | `		ph7_vm_func *pFunc;` |
|       25 |  7070 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       25 |  7071 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       25 |  7072 | `		if( pEntry == 0 ){` |
|      ! 0 |  7073 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7074 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7075 | `			return 0;` |
|        - |  7076 | `		}` |
|       25 |  7077 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       25 |  7078 | `		return pFunc;` |
|      ! 0 |  7079 | `	}else{` |
|        - |  7080 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7081 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7082 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7083 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7084 | `		if( pMethod == 0 ){` |
|      ! 0 |  7085 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7086 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7087 | `			return 0;` |
|        - |  7088 | `		}` |
|      ! 0 |  7089 | `		*ppThis = pClosure;` |
|      ! 0 |  7090 | `		return &pMethod->sFunc;` |
|        - |  7091 | `	}` |
|       13 |  7092 |  |
|        - |  7093 | `/*` |
|        - |  7094 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7095 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7096 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7097 | ` */` |
|       42 |  7098 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7099 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        1 |  7100 |  |
|       43 |  7101 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7102 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7103 | `	sxu32 nFormal, n;` |
|        - |  7104 | `	VmSlot sSlot;` |
|        - |  7105 | `	sxi32 rc;` |
|        - |  7106 | `	/* Install $this for closure/method callables */` |
|       43 |  7107 | `	if( pClosureThis ){` |
|        - |  7108 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7109 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7110 | `		if( pObj ){` |
|      ! 0 |  7111 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7112 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7113 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7114 | `		}` |
|      ! 0 |  7115 | `	}` |
|        - |  7116 | `	/* Install static variables */` |
|       43 |  7117 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7118 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7119 | `		ph7_value *pVal;` |
|      ! 0 |  7120 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7121 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7122 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7123 | `			if( pVal ){` |
|      ! 0 |  7124 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7125 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7126 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7127 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7128 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7129 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7130 | `				}` |
|      ! 0 |  7131 | `			}` |
|      ! 0 |  7132 | `		}` |
|      ! 0 |  7133 | `	}` |
|        - |  7134 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       43 |  7135 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       43 |  7136 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       53 |  7137 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7138 | `		ph7_value *pObj;` |
|       11 |  7139 | `		if( n < (sxu32)nArg ){` |
|        - |  7140 | `			/* Argument provided — install with type casting */` |
|       11 |  7141 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       11 |  7142 | `			if( pObj ){` |
|       11 |  7143 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7144 | `				/* Type casting */` |
|       11 |  7145 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7146 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7147 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7148 | `						if( xCast ){` |
|      ! 0 |  7149 | `							xCast(pObj);` |
|      ! 0 |  7150 | `						}` |
|      ! 0 |  7151 | `					}` |
|      ! 0 |  7152 | `				}` |
|       11 |  7153 | `				sSlot.nIdx = pObj->nIdx;` |
|       11 |  7154 | `				sSlot.pUserData = 0;` |
|       11 |  7155 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        6 |  7156 | `			}` |
|        5 |  7157 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7158 | `			/* Default value */` |
|      ! 0 |  7159 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7160 | `			if( pObj ){` |
|      ! 0 |  7161 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7162 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7163 | `					return rc;` |
|        - |  7164 | `				}` |
|      ! 0 |  7165 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7166 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7167 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7168 | `						if( xCast ){` |
|      ! 0 |  7169 | `							xCast(pObj);` |
|      ! 0 |  7170 | `						}` |
|      ! 0 |  7171 | `					}` |
|      ! 0 |  7172 | `				}` |
|      ! 0 |  7173 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7174 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7175 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7176 | `			}` |
|      ! 0 |  7177 | `		}` |
|        6 |  7178 | `	}` |
|        - |  7179 | `	/* Install closure environment (captured variables) */` |
|       43 |  7180 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7181 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7182 | `		ph7_value *pValue;` |
|        - |  7183 | `		sxu32 iEnv;` |
|        3 |  7184 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7185 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7186 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7187 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7188 | `				continue;` |
|        - |  7189 | `			}` |
|        5 |  7190 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7191 | `			if( pValue == 0 ){` |
|      ! 0 |  7192 | `				continue;` |
|        - |  7193 | `			}` |
|        5 |  7194 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7195 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7196 | `		}` |
|        1 |  7197 | `	}` |
|       43 |  7198 | `	return SXRET_OK;` |
|       22 |  7199 |  |
|        - |  7200 | `/*` |
|        - |  7201 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7202 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7203 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7204 | ` */` |
|       26 |  7205 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7206 |  |
|       27 |  7207 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7208 | `	ph7_class_instance *pThis;` |
|        - |  7209 | `	ph7_class_instance *pClosureThis;` |
|        - |  7210 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7211 | `	ph7_vm_func *pFunc;` |
|        - |  7212 | `	ph7_value sResult;` |
|        - |  7213 | `	ph7_value *pCtxAttr;` |
|        - |  7214 | `	SyString sAttrName;` |
|        - |  7215 | `	sxi32 rc;` |
|       27 |  7216 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7217 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7218 | `	}` |
|       27 |  7219 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7220 | `	/* Check if already started (has a __ctx) */` |
|       27 |  7221 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       27 |  7222 | `	if( pExecCtx != 0 ){` |
|        3 |  7223 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7224 | `			"Cannot start a fiber that has already been started");` |
|        - |  7225 | `	}` |
|        - |  7226 | `	/* Resolve callable */` |
|       25 |  7227 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       25 |  7228 | `	if( pFunc == 0 ){` |
|      ! 0 |  7229 | `		return PH7_EXCEPTION;` |
|        - |  7230 | `	}` |
|        - |  7231 | `	/* Create execution context now that we know the function */` |
|       25 |  7232 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       25 |  7233 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7234 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7235 | `			"Fiber::start(): out of memory");` |
|        - |  7236 | `	}` |
|        - |  7237 | `	/* Store context in $this->__ctx */` |
|       25 |  7238 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       25 |  7239 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7240 | `	if( pCtxAttr ){` |
|       25 |  7241 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       25 |  7242 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7243 | `	}` |
|        - |  7244 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7245 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7246 | `	 * into the fiber's frame, not the caller's. */` |
|       25 |  7247 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  7248 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7249 | `	/* Unpack the args array and install into the frame */` |
|        - |  7250 | `	{` |
|       25 |  7251 | `		ph7_value **apValues = 0;` |
|       25 |  7252 | `		int nActual = 0;` |
|       25 |  7253 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       25 |  7254 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7255 | `			ph7_hashmap_node *pNode;` |
|       25 |  7256 | `			sxu32 nCount = pMap->nEntry;` |
|       25 |  7257 | `			if( nCount > 0 ){` |
|        3 |  7258 | `				sxu32 idx = 0;` |
|        4 |  7259 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7260 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7261 | `				if( apValues ){` |
|        3 |  7262 | `					pNode = pMap->pFirst;` |
|        7 |  7263 | `					while( pNode && idx < nCount ){` |
|        5 |  7264 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7265 | `						idx++;` |
|        5 |  7266 | `						pNode = pNode->pPrev;` |
|        1 |  7267 | `					}` |
|        3 |  7268 | `					nActual = (int)idx;` |
|        1 |  7269 | `				}` |
|        1 |  7270 | `			}` |
|       12 |  7271 | `		}` |
|       25 |  7272 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       25 |  7273 | `		if( apValues ){` |
|        3 |  7274 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7275 | `		}` |
|        - |  7276 | `	}` |
|        - |  7277 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       25 |  7278 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       25 |  7279 | `	pExecCtx->pFrame->pParent = 0;` |
|       25 |  7280 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7281 | `		return PH7_ABORT;` |
|        - |  7282 | `	}` |
|       25 |  7283 | `	PH7_MemObjInit(pVm, &sResult);` |
|       25 |  7284 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       25 |  7285 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7286 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7287 | `		return PH7_ABORT;` |
|        - |  7288 | `	}` |
|       25 |  7289 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7290 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7291 | `		return PH7_EXCEPTION;` |
|        - |  7292 | `	}` |
|       25 |  7293 | `	ph7_result_value(pCtx, &sResult);` |
|       25 |  7294 | `	PH7_MemObjRelease(&sResult);` |
|       25 |  7295 | `	return PH7_OK;` |
|       14 |  7296 |  |
|        - |  7297 | `/*` |
|        - |  7298 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7299 | ` */` |
|       36 |  7300 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7301 |  |
|       37 |  7302 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7303 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7304 | `	ph7_value sResult;` |
|        - |  7305 | `	ph7_value *pResumeVal;` |
|        - |  7306 | `	sxi32 rc;` |
|       37 |  7307 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7308 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7309 | `		return PH7_OK;` |
|        - |  7310 | `	}` |
|       37 |  7311 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       37 |  7312 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7313 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7314 | `		return PH7_OK;` |
|        - |  7315 | `	}` |
|       37 |  7316 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7317 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7318 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7319 | `	}` |
|       35 |  7320 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       35 |  7321 | `	PH7_MemObjInit(pVm, &sResult);` |
|       35 |  7322 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       35 |  7323 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7324 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7325 | `		return PH7_ABORT;` |
|        - |  7326 | `	}` |
|       35 |  7327 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7328 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7329 | `		return PH7_EXCEPTION;` |
|        - |  7330 | `	}` |
|       35 |  7331 | `	ph7_result_value(pCtx, &sResult);` |
|       35 |  7332 | `	PH7_MemObjRelease(&sResult);` |
|       35 |  7333 | `	return PH7_OK;` |
|       19 |  7334 |  |
|        - |  7335 | `/*` |
|        - |  7336 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7337 | ` */` |
|        6 |  7338 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7339 |  |
|        7 |  7340 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7341 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7342 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7343 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7344 | `		return PH7_OK;` |
|        - |  7345 | `	}` |
|        7 |  7346 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        7 |  7347 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7348 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7349 | `		return PH7_OK;` |
|        - |  7350 | `	}` |
|        7 |  7351 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7352 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7353 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7354 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7355 | `		}` |
|      ! 0 |  7356 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7357 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7358 | `	}` |
|        7 |  7359 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        7 |  7360 | `	return PH7_OK;` |
|        4 |  7361 |  |
|        - |  7362 | `/*` |
|        - |  7363 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7364 | ` */` |
|        6 |  7365 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7366 |  |
|        - |  7367 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7368 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7369 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7370 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7371 | `	return PH7_OK;` |
|        4 |  7372 |  |
|      ! 0 |  7373 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7374 |  |
|        - |  7375 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7376 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7377 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7378 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7379 | `	return PH7_OK;` |
|      ! 0 |  7380 |  |
|        6 |  7381 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7382 |  |
|        - |  7383 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7384 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7385 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7386 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7387 | `	return PH7_OK;` |
|        4 |  7388 |  |
|        6 |  7389 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7390 |  |
|        - |  7391 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7392 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7393 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7394 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7395 | `	return PH7_OK;` |
|        4 |  7396 |  |
|        - |  7397 | `/*` |
|        - |  7398 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7399 | ` */` |
|      ! 0 |  7400 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7401 |  |
|      ! 0 |  7402 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7403 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7404 | `	if( nArg < 1 ){` |
|      ! 0 |  7405 | `		return PH7_OK;` |
|        - |  7406 | `	}` |
|      ! 0 |  7407 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      ! 0 |  7408 | `	if( pExecCtx ){` |
|      ! 0 |  7409 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7410 | `		/* Clear the attribute so double-free is prevented */` |
|      ! 0 |  7411 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7412 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7413 | `			SyString sAttrName;` |
|        - |  7414 | `			ph7_value *pAttr;` |
|      ! 0 |  7415 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7416 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7417 | `			if( pAttr ){` |
|      ! 0 |  7418 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7419 | `			}` |
|      ! 0 |  7420 | `		}` |
|      ! 0 |  7421 | `	}` |
|      ! 0 |  7422 | `	return PH7_OK;` |
|      ! 0 |  7423 |  |
|        - |  7424 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7425 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7426 |  |
|        - |  7427 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7428 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7429 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7430 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7431 |  |
|      ! 0 |  7432 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7433 |  |
|        - |  7434 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7435 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7436 | `	ph7_exec_ctx *pCtx;` |
|        - |  7437 | `	ph7_vm_func *pFunc;` |
|        - |  7438 | `	ph7_value *pCallable;` |
|        - |  7439 | `	ph7_value *pCtxAttr;` |
|        - |  7440 | `	SyString sAttrName;` |
|        - |  7441 | `	/* Must not already be started */` |
|      ! 0 |  7442 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7443 | `	if( pCtx != 0 ){` |
|      ! 0 |  7444 | `		return SXERR_INVALID;` |
|        - |  7445 | `	}` |
|      ! 0 |  7446 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7447 | `		return SXERR_INVALID;` |
|        - |  7448 | `	}` |
|      ! 0 |  7449 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7450 | `	/* Get the callable */` |
|      ! 0 |  7451 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7452 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7453 | `	if( pCallable == 0 ){` |
|      ! 0 |  7454 | `		return SXERR_INVALID;` |
|        - |  7455 | `	}` |
|        - |  7456 | `	/* Resolve callable */` |
|      ! 0 |  7457 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7458 | `		SyString sName;` |
|        - |  7459 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7460 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7461 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7462 | `		if( pEntry == 0 ){` |
|      ! 0 |  7463 | `			return SXERR_NOTFOUND;` |
|        - |  7464 | `		}` |
|      ! 0 |  7465 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7466 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7467 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7468 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7469 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7470 | `		if( pMethod == 0 ){` |
|      ! 0 |  7471 | `			return SXERR_INVALID;` |
|        - |  7472 | `		}` |
|      ! 0 |  7473 | `		pClosureThis = pClosure;` |
|      ! 0 |  7474 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7475 | `	}else{` |
|      ! 0 |  7476 | `		return SXERR_INVALID;` |
|        - |  7477 | `	}` |
|        - |  7478 | `	/* Create context */` |
|      ! 0 |  7479 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7480 | `	if( pCtx == 0 ){` |
|      ! 0 |  7481 | `		return SXERR_MEM;` |
|        - |  7482 | `	}` |
|        - |  7483 | `	/* Store in __ctx */` |
|      ! 0 |  7484 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7485 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7486 | `	if( pCtxAttr ){` |
|      ! 0 |  7487 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7488 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7489 | `	}` |
|        - |  7490 | `	/* Set up frame with args */` |
|      ! 0 |  7491 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7492 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7493 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7494 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7495 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7496 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7497 |  |
|      ! 0 |  7498 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7499 |  |
|      ! 0 |  7500 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7501 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7502 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7503 |  |
|      ! 0 |  7504 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7505 |  |
|      ! 0 |  7506 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7507 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7508 |  |
|      ! 0 |  7509 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7510 |  |
|      ! 0 |  7511 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7512 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7513 |  |
|      ! 0 |  7514 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7515 |  |
|      ! 0 |  7516 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7517 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7518 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7519 |  |
|        - |  7520 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7521 | `/*` |
|        - |  7522 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7523 | ` */` |
|       18 |  7524 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7525 |  |
|        - |  7526 | `	ph7_generator *pGen;` |
|       19 |  7527 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       19 |  7528 | `	if( pGen == 0 ){` |
|      ! 0 |  7529 | `		return 0;` |
|        - |  7530 | `	}` |
|       19 |  7531 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       19 |  7532 | `	pGen->pCtx = pCtx;` |
|       19 |  7533 | `	pGen->iImplicitKey = 0;` |
|       19 |  7534 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       19 |  7535 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7536 | `	/* Link the generator back to the exec context */` |
|       19 |  7537 | `	pCtx->pPrivate = pGen;` |
|       19 |  7538 | `	return pGen;` |
|       10 |  7539 |  |
|        - |  7540 | `/*` |
|        - |  7541 | ` * Release a generator and its execution context.` |
|        - |  7542 | ` */` |
|      ! 0 |  7543 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7544 |  |
|      ! 0 |  7545 | `	if( pGen == 0 ){` |
|      ! 0 |  7546 | `		return;` |
|        - |  7547 | `	}` |
|      ! 0 |  7548 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7549 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7550 | `	if( pGen->pCtx ){` |
|      ! 0 |  7551 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7552 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7553 | `		pGen->pCtx = 0;` |
|      ! 0 |  7554 | `	}` |
|      ! 0 |  7555 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7556 |  |
|        - |  7557 | `/*` |
|        - |  7558 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7559 | ` */` |
|      192 |  7560 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        1 |  7561 |  |
|        - |  7562 | `	ph7_class_instance *pThis;` |
|        - |  7563 | `	SyString sAttr;` |
|        - |  7564 | `	ph7_value *pAttr;` |
|      193 |  7565 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7566 | `		return 0;` |
|        - |  7567 | `	}` |
|      193 |  7568 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      193 |  7569 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7570 | `		return 0;` |
|        - |  7571 | `	}` |
|      193 |  7572 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      193 |  7573 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      193 |  7574 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7575 | `		return 0;` |
|        - |  7576 | `	}` |
|      193 |  7577 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       97 |  7578 |  |
|        - |  7579 | `/*` |
|        - |  7580 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7581 | ` */` |
|       18 |  7582 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7583 |  |
|        - |  7584 | `	ph7_generator *pGen;` |
|        - |  7585 | `	sxi32 rc;` |
|       19 |  7586 | `	if( nArg < 1 ) return PH7_OK;` |
|       19 |  7587 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       19 |  7588 | `	if( pGen == 0 ) return PH7_OK;` |
|       19 |  7589 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       19 |  7590 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       19 |  7591 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       19 |  7592 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7593 | `	}` |
|       19 |  7594 | `	return PH7_OK;` |
|       10 |  7595 |  |
|        - |  7596 | `/*` |
|        - |  7597 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7598 | ` */` |
|       52 |  7599 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7600 |  |
|        - |  7601 | `	ph7_generator *pGen;` |
|       53 |  7602 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       53 |  7603 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       53 |  7604 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       53 |  7605 | `	return PH7_OK;` |
|       27 |  7606 |  |
|        - |  7607 | `/*` |
|        - |  7608 | ` * Generator::current() — return the last yielded value.` |
|        - |  7609 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7610 | ` */` |
|       56 |  7611 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7612 |  |
|        - |  7613 | `	ph7_generator *pGen;` |
|        - |  7614 | `	sxi32 rc;` |
|       57 |  7615 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7616 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       57 |  7617 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7618 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7619 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7620 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7621 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7622 | `	}` |
|       57 |  7623 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       57 |  7624 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       29 |  7625 | `	}else{` |
|      ! 0 |  7626 | `		ph7_result_null(pCtx);` |
|        - |  7627 | `	}` |
|       57 |  7628 | `	return PH7_OK;` |
|       29 |  7629 |  |
|        - |  7630 | `/*` |
|        - |  7631 | ` * Generator::key() — return the last yielded key.` |
|        - |  7632 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7633 | ` */` |
|       12 |  7634 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7635 |  |
|        - |  7636 | `	ph7_generator *pGen;` |
|        - |  7637 | `	sxi32 rc;` |
|       13 |  7638 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7639 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7640 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7641 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7642 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7643 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7644 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7645 | `	}` |
|       13 |  7646 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7647 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7648 | `	}else{` |
|      ! 0 |  7649 | `		ph7_result_null(pCtx);` |
|        - |  7650 | `	}` |
|       13 |  7651 | `	return PH7_OK;` |
|        7 |  7652 |  |
|        - |  7653 | `/*` |
|        - |  7654 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7655 | ` */` |
|       48 |  7656 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7657 |  |
|        - |  7658 | `	ph7_generator *pGen;` |
|        - |  7659 | `	sxi32 rc;` |
|       49 |  7660 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 |  7661 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 |  7662 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 |  7663 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7664 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 |  7665 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       49 |  7666 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       25 |  7667 | `	}else{` |
|      ! 0 |  7668 | `		return PH7_OK;` |
|        - |  7669 | `	}` |
|       49 |  7670 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 |  7671 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       49 |  7672 | `	return PH7_OK;` |
|       25 |  7673 |  |
|        - |  7674 | `/*` |
|        - |  7675 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7676 | ` */` |
|        4 |  7677 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7678 |  |
|        - |  7679 | `	ph7_generator *pGen;` |
|        - |  7680 | `	ph7_value *pSendVal;` |
|        - |  7681 | `	sxi32 rc;` |
|        5 |  7682 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7683 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7684 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7685 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7686 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7687 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7688 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7689 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7690 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7691 | `	}else{` |
|      ! 0 |  7692 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7693 | `		return PH7_OK;` |
|        - |  7694 | `	}` |
|        5 |  7695 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7696 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7697 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7698 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7699 | `	}else{` |
|        3 |  7700 | `		ph7_result_null(pCtx);` |
|        - |  7701 | `	}` |
|        5 |  7702 | `	return PH7_OK;` |
|        3 |  7703 |  |
|        - |  7704 | `/*` |
|        - |  7705 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7706 | ` *` |
|        - |  7707 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7708 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7709 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7710 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7711 | ` * the exception to the caller.` |
|        - |  7712 | ` */` |
|      ! 0 |  7713 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7714 |  |
|        - |  7715 | `	ph7_generator *pGen;` |
|        - |  7716 | `	const char *zMsg;` |
|        - |  7717 | `	int nLen;` |
|      ! 0 |  7718 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7719 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7720 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7721 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7722 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7723 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7724 | `			"Cannot throw into a closed generator");` |
|        - |  7725 | `	}` |
|        - |  7726 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7727 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7728 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7729 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7730 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7731 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7732 | `	nLen = 0;` |
|      ! 0 |  7733 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7734 | `		/* Try to get the exception's message */` |
|        - |  7735 | `		SyString sAttr;` |
|        - |  7736 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7737 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7738 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7739 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7740 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7741 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7742 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7743 | `		}` |
|      ! 0 |  7744 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7745 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7746 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7747 | `	}` |
|      ! 0 |  7748 | `	(void)nLen;` |
|      ! 0 |  7749 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7750 |  |
|        - |  7751 | `/*` |
|        - |  7752 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7753 | ` */` |
|        2 |  7754 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7755 |  |
|        - |  7756 | `	ph7_generator *pGen;` |
|        3 |  7757 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7758 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7759 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7760 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7761 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7762 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7763 | `	}` |
|        3 |  7764 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7765 | `	return PH7_OK;` |
|        2 |  7766 |  |
|        - |  7767 | `/*` |
|        - |  7768 | ` * Generator::__destruct() — clean up.` |
|        - |  7769 | ` */` |
|      ! 0 |  7770 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7771 |  |
|        - |  7772 | `	ph7_generator *pGen;` |
|      ! 0 |  7773 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7774 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7775 | `	if( pGen ){` |
|      ! 0 |  7776 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7777 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7778 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7779 | `			SyString sAttrName;` |
|        - |  7780 | `			ph7_value *pAttr;` |
|      ! 0 |  7781 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7782 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7783 | `			if( pAttr ){` |
|      ! 0 |  7784 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7785 | `			}` |
|      ! 0 |  7786 | `		}` |
|      ! 0 |  7787 | `	}` |
|      ! 0 |  7788 | `	return PH7_OK;` |
|      ! 0 |  7789 |  |
|        - |  7790 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7791 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7792 | `/*` |
|        - |  7793 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7794 | ` * the desired message.` |
|        - |  7795 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7796 | ` * in 'api.c' for additional information.` |
|        - |  7797 | ` */` |
|      350 |  7798 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7799 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7800 | `	SyString *pString /* Message to output */` |
|        - |  7801 | `	)` |
|        2 |  7802 |  |
|      352 |  7803 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  7804 | `	sxi32 rc = SXRET_OK;` |
|        - |  7805 | `	/* Call the output consumer */` |
|      352 |  7806 | `	if( pString->nByte > 0 ){` |
|      352 |  7807 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  7808 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  7809 | `	}` |
|      352 |  7810 | `	return rc;` |
|        2 |  7811 |  |
|        - |  7812 | `/*` |
|        - |  7813 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7814 | ` * callback to consume the formatted message.` |
|        - |  7815 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7816 | ` * in 'api.c' for additional information.` |
|        - |  7817 | ` */` |
|        2 |  7818 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7819 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7820 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7821 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7822 | `	)` |
|        1 |  7823 |  |
|        3 |  7824 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7825 | `	sxi32 rc = SXRET_OK;` |
|        - |  7826 | `	SyBlob sWorker;` |
|        - |  7827 | `	/* Format the message and call the output consumer */` |
|        3 |  7828 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7829 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7830 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7831 | `		/* Consume the formatted message */` |
|        3 |  7832 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7833 | `	}` |
|        3 |  7834 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7835 | `	/* Release the working buffer */` |
|        3 |  7836 | `	SyBlobRelease(&sWorker);` |
|        3 |  7837 | `	return rc;` |
|        1 |  7838 |  |
|        - |  7839 | `/*` |
|        - |  7840 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7841 | ` * This function never fail and always return a pointer` |
|        - |  7842 | ` * to a null terminated string.` |
|        - |  7843 | ` */` |
|       12 |  7844 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7845 |  |
|       13 |  7846 | `	const char *zOp = "Unknown     ";` |
|       13 |  7847 | `	switch(nOp){` |
|        3 |  7848 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7849 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7850 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7851 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7852 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7853 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7854 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7855 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7856 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7857 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7858 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  7859 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  7860 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  7861 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  7862 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  7863 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  7864 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  7865 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  7866 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  7867 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  7868 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  7869 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  7870 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  7871 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  7872 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  7873 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  7874 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  7875 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  7876 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  7877 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  7878 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  7879 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  7880 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  7881 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  7882 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  7883 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  7884 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  7885 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  7886 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  7887 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  7888 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  7889 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  7890 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  7891 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  7892 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  7893 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  7894 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  7895 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  7896 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  7897 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  7898 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  7899 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  7900 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  7901 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  7902 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  7903 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  7904 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  7905 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  7906 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  7907 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  7908 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  7909 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  7910 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  7911 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  7912 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  7913 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  7914 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  7915 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  7916 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  7917 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  7918 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  7919 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  7920 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  7921 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  7922 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  7923 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  7924 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  7925 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  7926 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  7927 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  7928 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  7929 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  7930 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  7931 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  7932 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  7933 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  7934 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  7935 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  7936 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  7937 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  7938 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  7939 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  7940 | `	default:` |
|      ! 0 |  7941 | `		break;` |
|        - |  7942 | `	}` |
|       13 |  7943 | `	return zOp;` |
|        1 |  7944 |  |
|        - |  7945 | `/*` |
|        - |  7946 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  7947 | ` * The xConsumer() callback which is an used defined function` |
|        - |  7948 | ` * is responsible of consuming the generated dump.` |
|        - |  7949 | ` */` |
|        2 |  7950 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  7951 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  7952 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  7953 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  7954 | `	)` |
|        1 |  7955 |  |
|        - |  7956 | `	sxi32 rc;` |
|        3 |  7957 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  7958 | `	return rc;` |
|        1 |  7959 |  |
|        - |  7960 | `/*` |
|        - |  7961 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  7962 | ` * outside a class body [i.e: global or function scope].` |
|        - |  7963 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  7964 | ` * in 'compile.c' for additional information.` |
|        - |  7965 | ` */` |
|        8 |  7966 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  7967 |  |
|        9 |  7968 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  7969 | `	/* Evaluate and expand constant value */` |
|        9 |  7970 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  7971 |  |
|        - |  7972 | `/*` |
|        - |  7973 | ` * Section:` |
|        - |  7974 | ` *  Function handling functions.` |
|        - |  7975 | ` * Status:` |
|        - |  7976 | ` *    Stable.` |
|        - |  7977 | ` */` |
|        - |  7978 | `/*` |
|        - |  7979 | ` * int func_num_args(void)` |
|        - |  7980 | ` *   Returns the number of arguments passed to the function.` |
|        - |  7981 | ` * Parameters` |
|        - |  7982 | ` *   None.` |
|        - |  7983 | ` * Return` |
|        - |  7984 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  7985 | ` *  or -1 if called from the globe scope.` |
|        - |  7986 | ` */` |
|      928 |  7987 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7988 |  |
|        - |  7989 | `	VmFrame *pFrame;` |
|        - |  7990 | `	ph7_vm *pVm;` |
|        - |  7991 | `	/* Point to the target VM */` |
|      930 |  7992 | `	pVm = pCtx->pVm;` |
|        - |  7993 | `	/* Current frame */` |
|      930 |  7994 | `	pFrame = pVm->pFrame;` |
|      930 |  7995 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 |  7996 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  7997 | `		SXUNUSED(nArg);` |
|      ! 0 |  7998 | `		SXUNUSED(apArg);` |
|        - |  7999 | `		/* Global frame,return -1 */` |
|      ! 0 |  8000 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8001 | `		return SXRET_OK;` |
|        - |  8002 | `	}` |
|        - |  8003 | `	/* Total number of arguments passed to the enclosing function */` |
|      930 |  8004 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      930 |  8005 | `	ph7_result_int(pCtx,nArg);` |
|      930 |  8006 | `	return SXRET_OK;` |
|      466 |  8007 |  |
|        - |  8008 | `/*` |
|        - |  8009 | ` * value func_get_arg(int $arg_num)` |
|        - |  8010 | ` *   Return an item from the argument list.` |
|        - |  8011 | ` * Parameters` |
|        - |  8012 | ` *  Argument number(index start from zero).` |
|        - |  8013 | ` * Return` |
|        - |  8014 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8015 | ` */` |
|       22 |  8016 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8017 |  |
|       24 |  8018 | `	ph7_value *pObj = 0;` |
|       24 |  8019 | `	VmSlot *pSlot = 0;` |
|        - |  8020 | `	VmFrame *pFrame;` |
|        - |  8021 | `	ph7_vm *pVm;` |
|        - |  8022 | `	/* Point to the target VM */` |
|       24 |  8023 | `	pVm = pCtx->pVm;` |
|        - |  8024 | `	/* Current frame */` |
|       24 |  8025 | `	pFrame = pVm->pFrame;` |
|       24 |  8026 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8027 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8028 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8029 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8030 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8031 | `		return SXRET_OK;` |
|        - |  8032 | `	}` |
|        - |  8033 | `	/* Extract the desired index */` |
|       21 |  8034 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8035 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8036 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8037 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8038 | `		return SXRET_OK;` |
|        - |  8039 | `	}` |
|        - |  8040 | `	/* Extract the desired argument */` |
|       21 |  8041 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8042 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8043 | `			/* Return the desired argument */` |
|       21 |  8044 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8045 | `		}else{` |
|        - |  8046 | `			/* No such argument,return false */` |
|      ! 0 |  8047 | `			ph7_result_bool(pCtx,0);` |
|        - |  8048 | `		}` |
|       11 |  8049 | `	}else{` |
|        - |  8050 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8051 | `		ph7_result_bool(pCtx,0);` |
|        - |  8052 | `	}` |
|       21 |  8053 | `	return SXRET_OK;` |
|       13 |  8054 |  |
|        - |  8055 | `/*` |
|        - |  8056 | ` * array func_get_args_byref(void)` |
|        - |  8057 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8058 | ` * Parameters` |
|        - |  8059 | ` *  None.` |
|        - |  8060 | ` * Return` |
|        - |  8061 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8062 | ` *  member of the current user-defined function's argument list.` |
|        - |  8063 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8064 | ` * NOTE:` |
|        - |  8065 | ` *  Arguments are returned to the array by reference.` |
|        - |  8066 | ` */` |
|        2 |  8067 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8068 |  |
|        - |  8069 | `	ph7_value *pArray;` |
|        - |  8070 | `	VmFrame *pFrame;` |
|        - |  8071 | `	VmSlot *aSlot;` |
|        - |  8072 | `	sxu32 n;` |
|        - |  8073 | `	/* Point to the current frame */` |
|        3 |  8074 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8075 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8076 | `	if( pFrame->pParent == 0 ){` |
|        - |  8077 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8078 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8079 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8080 | `		return SXRET_OK;` |
|        - |  8081 | `	}` |
|        - |  8082 | `	/* Create a new array */` |
|        3 |  8083 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8084 | `	if( pArray == 0 ){` |
|      ! 0 |  8085 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8086 | `		SXUNUSED(apArg);` |
|      ! 0 |  8087 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8088 | `		return SXRET_OK;` |
|        - |  8089 | `	}` |
|        - |  8090 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8091 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8092 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8093 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8094 | `	}` |
|        - |  8095 | `	/* Return the freshly created array */` |
|        3 |  8096 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8097 | `	return SXRET_OK;` |
|        2 |  8098 |  |
|        - |  8099 | `/*` |
|        - |  8100 | ` * array func_get_args(void)` |
|        - |  8101 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8102 | ` * Parameters` |
|        - |  8103 | ` *  None.` |
|        - |  8104 | ` * Return` |
|        - |  8105 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8106 | ` *  member of the current user-defined function's argument list.` |
|        - |  8107 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8108 | ` */` |
|       88 |  8109 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8110 |  |
|       90 |  8111 | `	ph7_value *pObj = 0;` |
|        - |  8112 | `	ph7_value *pArray;` |
|        - |  8113 | `	VmFrame *pFrame;` |
|        - |  8114 | `	VmSlot *aSlot;` |
|        - |  8115 | `	sxu32 n;` |
|        - |  8116 | `	/* Point to the current frame */` |
|       90 |  8117 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8118 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8119 | `	if( pFrame->pParent == 0 ){` |
|        - |  8120 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8121 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8122 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8123 | `		return SXRET_OK;` |
|        - |  8124 | `	}` |
|        - |  8125 | `	/* Create a new array */` |
|       90 |  8126 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8127 | `	if( pArray == 0 ){` |
|      ! 0 |  8128 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8129 | `		SXUNUSED(apArg);` |
|      ! 0 |  8130 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8131 | `		return SXRET_OK;` |
|        - |  8132 | `	}` |
|        - |  8133 | `	/* Start filling the array with the given arguments */` |
|       90 |  8134 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8135 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8136 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8137 | `		if( pObj ){` |
|      134 |  8138 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8139 | `		}` |
|       68 |  8140 | `	}` |
|        - |  8141 | `	/* Return the freshly created array */` |
|       90 |  8142 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8143 | `	return SXRET_OK;` |
|       46 |  8144 |  |
|        - |  8145 | `/*` |
|        - |  8146 | ` * bool function_exists(string $name)` |
|        - |  8147 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8148 | ` * Parameters` |
|        - |  8149 | ` *  The name of the desired function.` |
|        - |  8150 | ` * Return` |
|        - |  8151 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8152 | ` */` |
|     1682 |  8153 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8154 |  |
|        - |  8155 | `	const char *zName;` |
|        - |  8156 | `	ph7_vm *pVm;` |
|        - |  8157 | `	int nLen;` |
|        - |  8158 | `	int res;` |
|     1684 |  8159 | `	if( nArg < 1 ){` |
|        - |  8160 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8161 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8162 | `		return SXRET_OK;` |
|        - |  8163 | `	}` |
|        - |  8164 | `	/* Point to the target VM */` |
|     1684 |  8165 | `	pVm = pCtx->pVm;` |
|        - |  8166 | `	/* Extract the function name */` |
|     1684 |  8167 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8168 | `	/* Assume the function is not defined */` |
|     1684 |  8169 | `	res = 0;` |
|        - |  8170 | `	/* Perform the lookup */` |
|     2523 |  8171 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8172 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8173 | `			/* Function is defined */` |
|      206 |  8174 | `			res = 1;` |
|      102 |  8175 | `	}` |
|     1684 |  8176 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8177 | `	return SXRET_OK;` |
|      843 |  8178 |  |
|        - |  8179 | `/*` |
|        - |  8180 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8181 | ` * [i.e: Whether it is callable or not].` |
|        - |  8182 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8183 | ` */` |
|    16236 |  8184 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8185 |  |
|    16238 |  8186 | `	int res = 0;` |
|    16238 |  8187 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8188 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8189 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8190 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8191 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8192 | `		if( pMethod && CallInvoke ){` |
|        - |  8193 | `			ph7_value sResult;` |
|        - |  8194 | `			sxi32 rc;` |
|        - |  8195 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8196 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8197 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8198 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8199 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8200 | `			}` |
|      ! 0 |  8201 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8202 | `		}` |
|    16238 |  8203 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8204 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8205 | `		if( pMap->nEntry == 2 ){` |
|        - |  8206 | `			ph7_class *pClass;` |
|        - |  8207 | `			ph7_value *pV;` |
|        - |  8208 | `			/* Extract the target class */` |
|       12 |  8209 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8210 | `			if( pV ){` |
|       12 |  8211 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8212 | `				if( pClass ){` |
|        - |  8213 | `					ph7_class_method *pMethod;` |
|        - |  8214 | `					/* Extract the target method */` |
|       10 |  8215 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8216 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8217 | `						/* Perform the lookup */` |
|       10 |  8218 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8219 | `						if( pMethod ){` |
|        - |  8220 | `							/* Method is callable */` |
|        5 |  8221 | `							res = 1;` |
|        2 |  8222 | `						}` |
|        4 |  8223 | `					}` |
|        4 |  8224 | `				}` |
|        5 |  8225 | `			}` |
|        7 |  8226 | `		}` |
|    16225 |  8227 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8228 | `		const char *zName;` |
|        - |  8229 | `		int nLen;` |
|        - |  8230 | `		/* Extract the name */` |
|     4752 |  8231 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8232 | `		/* Perform the lookup */` |
|     4767 |  8233 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8234 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8235 | `				/* Function is callable */` |
|     4734 |  8236 | `				res = 1;` |
|     2366 |  8237 | `		}` |
|     2375 |  8238 | `	}` |
|    16238 |  8239 | `	return res;` |
|        2 |  8240 |  |
|        - |  8241 | `/*` |
|        - |  8242 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8243 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8244 | ` * Parameters` |
|        - |  8245 | ` * $name` |
|        - |  8246 | ` *    The callback function to check` |
|        - |  8247 | ` * $syntax_only` |
|        - |  8248 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8249 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8250 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8251 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8252 | ` *    a string.` |
|        - |  8253 | ` * Return` |
|        - |  8254 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8255 | ` */` |
|       14 |  8256 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8257 |  |
|        - |  8258 | `	ph7_vm *pVm;` |
|        - |  8259 | `	int res;` |
|       15 |  8260 | `	if( nArg < 1 ){` |
|        - |  8261 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8262 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8263 | `		return SXRET_OK;` |
|        - |  8264 | `	}` |
|        - |  8265 | `	/* Point to the target VM */` |
|       15 |  8266 | `	pVm = pCtx->pVm;` |
|        - |  8267 | `	/* Perform the requested operation */` |
|       15 |  8268 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8269 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8270 | `	return SXRET_OK;` |
|        8 |  8271 |  |
|        - |  8272 | `/*` |
|        - |  8273 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8274 | ` * defined below.` |
|        - |  8275 | ` */` |
|     1188 |  8276 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8277 |  |
|     1189 |  8278 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8279 | `	ph7_value sName;` |
|        - |  8280 | `	sxi32 rc;` |
|        - |  8281 | `	/* Prepare the function name for insertion */` |
|     1189 |  8282 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1189 |  8283 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8284 | `	/* Perform the insertion */` |
|     1189 |  8285 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1189 |  8286 | `	PH7_MemObjRelease(&sName);` |
|     1189 |  8287 | `	return rc;` |
|        1 |  8288 |  |
|        - |  8289 | `/*` |
|        - |  8290 | ` * array get_defined_functions(void)` |
|        - |  8291 | ` *  Returns an array of all defined functions.` |
|        - |  8292 | ` * Parameter` |
|        - |  8293 | ` *  None.` |
|        - |  8294 | ` * Return` |
|        - |  8295 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8296 | ` *  both built-in (internal) and user-defined.` |
|        - |  8297 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8298 | ` *  defined ones using $arr["user"].` |
|        - |  8299 | ` * Note:` |
|        - |  8300 | ` *  NULL is returned on failure.` |
|        - |  8301 | ` */` |
|        2 |  8302 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8303 |  |
|        - |  8304 | `	ph7_value *pArray,*pEntry;` |
|        - |  8305 | `	/* NOTE:` |
|        - |  8306 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8307 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8308 | `	 */` |
|        3 |  8309 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8310 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8311 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8312 | `		SXUNUSED(apArg);` |
|        - |  8313 | `		/* Return NULL */` |
|      ! 0 |  8314 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8315 | `		return SXRET_OK;` |
|        - |  8316 | `	}` |
|        3 |  8317 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8318 | `	if( pEntry == 0 ){` |
|        - |  8319 | `		/* Return NULL */` |
|      ! 0 |  8320 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8321 | `		return SXRET_OK;` |
|        - |  8322 | `	}` |
|        - |  8323 | `	/* Fill with the appropriate information */` |
|        3 |  8324 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8325 | `	/* Create the 'internal' index */` |
|        3 |  8326 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8327 | `	/* Create the user-func array */` |
|        3 |  8328 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8329 | `	if( pEntry == 0 ){` |
|        - |  8330 | `		/* Return NULL */` |
|      ! 0 |  8331 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8332 | `		return SXRET_OK;` |
|        - |  8333 | `	}` |
|        - |  8334 | `	/* Fill with the appropriate information */` |
|        3 |  8335 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8336 | `	/* Create the 'user' index */` |
|        3 |  8337 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8338 | `	/* Return the multi-dimensional array */` |
|        3 |  8339 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8340 | `	return SXRET_OK;` |
|        2 |  8341 |  |
|        - |  8342 | `/*` |
|        - |  8343 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8344 | ` *  Register a function for execution on shutdown.` |
|        - |  8345 | ` * Note` |
|        - |  8346 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8347 | ` *  be called in the same order as they were registered.` |
|        - |  8348 | ` * Parameters` |
|        - |  8349 | ` *  $callback` |
|        - |  8350 | ` *   The shutdown callback to register.` |
|        - |  8351 | ` * $param` |
|        - |  8352 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8353 | ` * Return` |
|        - |  8354 | ` *  Nothing.` |
|        - |  8355 | ` */` |
|        2 |  8356 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8357 |  |
|        - |  8358 | `	VmShutdownCB sEntry;` |
|        - |  8359 | `	int i,j;` |
|        3 |  8360 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8361 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8362 | `		return PH7_OK;` |
|        - |  8363 | `	}` |
|        - |  8364 | `	/* Zero the Entry */` |
|        3 |  8365 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8366 | `	/* Initialize fields */` |
|        3 |  8367 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8368 | `	/* Save the callback name for later invocation name */` |
|        3 |  8369 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8370 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8371 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8372 | `	}` |
|        - |  8373 | `	/* Copy arguments */` |
|        3 |  8374 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8375 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8376 | `			/* Limit reached */` |
|      ! 0 |  8377 | `			break;` |
|        - |  8378 | `		}` |
|      ! 0 |  8379 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8380 | `	}` |
|        3 |  8381 | `	sEntry.nArg = j;` |
|        - |  8382 | `	/* Install the callback */` |
|        3 |  8383 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8384 | `	return PH7_OK;` |
|        2 |  8385 |  |
|        - |  8386 | `/*` |
|        - |  8387 | ` * Section:` |
|        - |  8388 | ` *  Class handling functions.` |
|        - |  8389 | ` * Status:` |
|        - |  8390 | ` *    Stable.` |
|        - |  8391 | ` */` |
|        - |  8392 | `/*` |
|        - |  8393 | ` * Extract the top active class. NULL is returned` |
|        - |  8394 | ` * if the class stack is empty.` |
|        - |  8395 | ` */` |
|      556 |  8396 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8397 |  |
|      558 |  8398 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8399 | `	ph7_class **apClass;` |
|      558 |  8400 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8401 | `		/* Empty stack,return NULL */` |
|       15 |  8402 | `		return 0;` |
|        - |  8403 | `	}` |
|        - |  8404 | `	/* Peek the last entry */` |
|      544 |  8405 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      544 |  8406 | `	return apClass[pSet->nUsed - 1];` |
|      280 |  8407 |  |
|        - |  8408 | `/*` |
|        - |  8409 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8410 | ` *   Get the class that declared the currently executing method.` |
|        - |  8411 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8412 | ` *` |
|        - |  8413 | ` * Parameters` |
|        - |  8414 | ` *   pVm: Target VM` |
|        - |  8415 | ` *` |
|        - |  8416 | ` * Return` |
|        - |  8417 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8418 | ` *   - Not executing within a class method` |
|        - |  8419 | ` *` |
|        - |  8420 | ` * Note` |
|        - |  8421 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8422 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8423 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8424 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8425 | ` *   declaring class.` |
|        - |  8426 | ` */` |
|       52 |  8427 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8428 |  |
|       54 |  8429 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8430 | `	ph7_vm_func *pVmFunc;` |
|        - |  8431 |  |
|        - |  8432 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  8433 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8434 |  |
|        - |  8435 | `	/* Check if we're in a method context */` |
|       54 |  8436 | `	if( pFrame->pParent ){` |
|       50 |  8437 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  8438 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8439 | `			/* Return the declaring class */` |
|       50 |  8440 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8441 | `		}` |
|      ! 0 |  8442 | `	}` |
|        - |  8443 |  |
|        5 |  8444 | `	return 0;` |
|       28 |  8445 |  |
|        - |  8446 |  |
|        - |  8447 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8448 | `/*` |
|        - |  8449 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8450 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8451 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8452 | ` * return value indicates failure.` |
|        - |  8453 | ` */` |
|     1484 |  8454 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8455 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8456 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8457 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8458 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8459 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8460 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8461 | `	)` |
|        2 |  8462 |  |
|        - |  8463 | `	ph7_value *aStack;` |
|        - |  8464 | `	VmInstr aInstr[2];` |
|        - |  8465 | `	int iCursor;` |
|        - |  8466 | `	int i;` |
|        - |  8467 | `	/* Create a new operand stack */` |
|     1486 |  8468 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1486 |  8469 | `	if( aStack == 0 ){` |
|      ! 0 |  8470 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8471 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8472 | `		return SXERR_MEM;` |
|        - |  8473 | `	}` |
|        - |  8474 | `	/* Fill the operand stack with the given arguments */` |
|     2088 |  8475 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      604 |  8476 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8477 | `		/*` |
|        - |  8478 | `		 * Symisc eXtension:` |
|        - |  8479 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8480 | `		 */` |
|      604 |  8481 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      303 |  8482 | `	}` |
|     1486 |  8483 | `	iCursor = nArg + 1;` |
|     1486 |  8484 | `	if( pThis ){` |
|        - |  8485 | `		/*` |
|        - |  8486 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8487 | `		 */` |
|     1480 |  8488 | `		pThis->iRef++; /* Increment reference count */` |
|     1480 |  8489 | `		aStack[i].x.pOther = pThis;` |
|     1480 |  8490 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      739 |  8491 | `	}` |
|     1486 |  8492 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1486 |  8493 | `	i++;` |
|        - |  8494 | `	/* Push method name */` |
|     1486 |  8495 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1486 |  8496 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1486 |  8497 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1486 |  8498 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8499 | `	/* Emit the CALL istruction */` |
|     1486 |  8500 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1486 |  8501 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1486 |  8502 | `	aInstr[0].iP2 = 0;` |
|     1486 |  8503 | `	aInstr[0].p3  = 0;` |
|        - |  8504 | `	/* Emit the DONE instruction */` |
|     1486 |  8505 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1486 |  8506 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1486 |  8507 | `	aInstr[1].iP2 = 0;` |
|     1486 |  8508 | `	aInstr[1].p3  = 0;` |
|        - |  8509 | `	/* Execute the method body (if available) */` |
|     1486 |  8510 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8511 | `	/* Clean up the mess left behind */` |
|     1486 |  8512 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1486 |  8513 | `	return PH7_OK;` |
|      744 |  8514 |  |
|        - |  8515 | `/*` |
|        - |  8516 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8517 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8518 | ` * in the apArg[] array.` |
|        - |  8519 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8520 | ` * return value indicates failure.` |
|        - |  8521 | ` */` |
|      930 |  8522 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8523 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8524 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8525 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8526 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8527 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8528 | `	)` |
|        2 |  8529 |  |
|        - |  8530 | `	ph7_value *aStack;` |
|        - |  8531 | `	VmInstr aInstr[2];` |
|        - |  8532 | `	int i;` |
|      932 |  8533 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8534 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8535 | `		if( pResult ){` |
|        - |  8536 | `			/* Assume a null return value */` |
|      ! 0 |  8537 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8538 | `		}` |
|      471 |  8539 | `		return SXERR_INVALID;` |
|        - |  8540 | `	}` |
|      462 |  8541 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8542 | `		/* Class method */` |
|       11 |  8543 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8544 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8545 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8546 | `		ph7_class *pClass = 0;` |
|        - |  8547 | `		ph7_value *pValue;` |
|        - |  8548 | `		sxi32 rc;` |
|       11 |  8549 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8550 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8551 | `			if( pResult ){` |
|        - |  8552 | `				/* Assume a null return value */` |
|      ! 0 |  8553 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8554 | `			}` |
|      ! 0 |  8555 | `			return SXRET_OK;` |
|        - |  8556 | `		}` |
|        - |  8557 | `		/* Extract the class name or an instance of it */` |
|       11 |  8558 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8559 | `		if( pValue ){` |
|       11 |  8560 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8561 | `		}` |
|       11 |  8562 | `		if( pClass == 0 ){` |
|        - |  8563 | `			/* No such class,return NULL */` |
|      ! 0 |  8564 | `			if( pResult ){` |
|      ! 0 |  8565 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8566 | `			}` |
|      ! 0 |  8567 | `			return SXRET_OK;` |
|        - |  8568 | `		}` |
|       11 |  8569 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8570 | `			/* Point to the class instance */` |
|        5 |  8571 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8572 | `		}` |
|        - |  8573 | `		/* Try to extract the method */` |
|       11 |  8574 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8575 | `		if( pValue ){` |
|       11 |  8576 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8577 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8578 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8579 | `			}` |
|        5 |  8580 | `		}` |
|       11 |  8581 | `		if( pMethod == 0 ){` |
|        - |  8582 | `			/* No such method,return NULL */` |
|      ! 0 |  8583 | `			if( pResult ){` |
|      ! 0 |  8584 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8585 | `			}` |
|      ! 0 |  8586 | `			return SXRET_OK;` |
|        - |  8587 | `		}` |
|        - |  8588 | `		/* Call the class method */` |
|       11 |  8589 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8590 | `		return rc;` |
|        - |  8591 | `	}` |
|        - |  8592 | `	/* Create a new operand stack */` |
|      452 |  8593 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      452 |  8594 | `	if( aStack == 0 ){` |
|      ! 0 |  8595 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8596 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8597 | `		if( pResult ){` |
|        - |  8598 | `			/* Assume a null return value */` |
|      ! 0 |  8599 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8600 | `		}` |
|      ! 0 |  8601 | `		return SXERR_MEM;` |
|        - |  8602 | `	}` |
|        - |  8603 | `	/* Fill the operand stack with the given arguments */` |
|     1478 |  8604 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1028 |  8605 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8606 | `		/*` |
|        - |  8607 | `		 * Symisc eXtension:` |
|        - |  8608 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8609 | `		 */` |
|     1028 |  8610 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      515 |  8611 | `	}` |
|        - |  8612 | `	/* Push the function name */` |
|      452 |  8613 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      452 |  8614 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8615 | `	/* Emit the CALL istruction */` |
|      452 |  8616 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      452 |  8617 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      452 |  8618 | `	aInstr[0].iP2 = 0;` |
|      452 |  8619 | `	aInstr[0].p3  = 0;` |
|        - |  8620 | `	/* Emit the DONE instruction */` |
|      452 |  8621 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      452 |  8622 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      452 |  8623 | `	aInstr[1].iP2 = 0;` |
|      452 |  8624 | `	aInstr[1].p3  = 0;` |
|        - |  8625 | `	/* Execute the function body (if available) */` |
|      452 |  8626 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8627 | `	/* Clean up the mess left behind */` |
|      452 |  8628 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      452 |  8629 | `	return PH7_OK;` |
|      467 |  8630 |  |
|        - |  8631 | `/*` |
|        - |  8632 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8633 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8634 | ` * parameter.` |
|        - |  8635 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8636 | ` * return value indicates failure.` |
|        - |  8637 | ` */` |
|      236 |  8638 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8639 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8640 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8641 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8642 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8643 | `	)` |
|        1 |  8644 |  |
|        - |  8645 | `	ph7_value *pArg;` |
|        - |  8646 | `	SySet aArg;` |
|        - |  8647 | `	va_list ap;` |
|        - |  8648 | `	sxi32 rc;` |
|      237 |  8649 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8650 | `	/* Copy arguments one after one */` |
|      237 |  8651 | `	va_start(ap,pResult);` |
|      393 |  8652 | `	for(;;){` |
|      787 |  8653 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8654 | `		if( pArg == 0 ){` |
|      237 |  8655 | `			break;` |
|        - |  8656 | `		}` |
|      551 |  8657 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8658 | `	}` |
|        - |  8659 | `	/* Call the core routine */` |
|      237 |  8660 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8661 | `	/* Cleanup */` |
|      237 |  8662 | `	SySetRelease(&aArg);` |
|      237 |  8663 | `	return rc;` |
|        1 |  8664 |  |
|        - |  8665 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8666 | `/*` |
|        - |  8667 | ` * bool defined(string $name)` |
|        - |  8668 | ` *  Checks whether a given named constant exists.` |
|        - |  8669 | ` * Parameter:` |
|        - |  8670 | ` *  Name of the desired constant.` |
|        - |  8671 | ` * Return` |
|        - |  8672 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8673 | ` */` |
|       14 |  8674 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8675 |  |
|        - |  8676 | `	const char *zName;` |
|       16 |  8677 | `	int nLen = 0;` |
|       16 |  8678 | `	int res = 0;` |
|       16 |  8679 | `	if( nArg < 1 ){` |
|        - |  8680 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8681 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8682 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8683 | `		return SXRET_OK;` |
|        - |  8684 | `	}` |
|        - |  8685 | `	/* Extract constant name */` |
|       16 |  8686 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8687 | `	/* Perform the lookup */` |
|       16 |  8688 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8689 | `		/* Already defined */` |
|       10 |  8690 | `		res = 1;` |
|        4 |  8691 | `	}` |
|       16 |  8692 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8693 | `	return SXRET_OK;` |
|        9 |  8694 |  |
|        - |  8695 | `/*` |
|        - |  8696 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8697 | ` * below.` |
|        - |  8698 | ` */` |
|        8 |  8699 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8700 |  |
|       10 |  8701 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8702 | `	/* Expand constant value */` |
|       10 |  8703 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8704 |  |
|        - |  8705 | `/*` |
|        - |  8706 | ` * bool define(string $constant_name,expression value)` |
|        - |  8707 | ` *  Defines a named constant at runtime.` |
|        - |  8708 | ` * Parameter:` |
|        - |  8709 | ` *  $constant_name` |
|        - |  8710 | ` *   The name of the constant` |
|        - |  8711 | ` *  $value` |
|        - |  8712 | ` *   Constant value` |
|        - |  8713 | ` * Return:` |
|        - |  8714 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8715 | ` */` |
|       10 |  8716 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8717 |  |
|        - |  8718 | `	const char *zName;  /* Constant name */` |
|        - |  8719 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8720 | `	int nLen = 0;       /* Name length */` |
|        - |  8721 | `	sxi32 rc;` |
|       12 |  8722 | `	if( nArg < 2 ){` |
|        - |  8723 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8724 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8725 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8726 | `		return SXRET_OK;` |
|        - |  8727 | `	}` |
|       12 |  8728 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8729 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8730 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8731 | `		return SXRET_OK;` |
|        - |  8732 | `	}` |
|        - |  8733 | `	/* Extract constant name */` |
|       12 |  8734 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8735 | `	if( nLen < 1 ){` |
|      ! 0 |  8736 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8737 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8738 | `		return SXRET_OK;` |
|        - |  8739 | `	}` |
|        - |  8740 | `	/* Duplicate constant value */` |
|       12 |  8741 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8742 | `	if( pValue == 0 ){` |
|      ! 0 |  8743 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8744 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8745 | `		return SXRET_OK;` |
|        - |  8746 | `	}` |
|        - |  8747 | `	/* Initialize the memory object */` |
|       12 |  8748 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8749 | `	/* Register the constant */` |
|       12 |  8750 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8751 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8752 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8753 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8754 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8755 | `		return SXRET_OK;` |
|        - |  8756 | `	}` |
|        - |  8757 | `	/* Duplicate constant value */` |
|       12 |  8758 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8759 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8760 | `		/* Lower case the constant name */` |
|      ! 0 |  8761 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8762 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8763 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8764 | `				/* UTF-8 stream */` |
|      ! 0 |  8765 | `				zCur++;` |
|      ! 0 |  8766 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8767 | `					zCur++;` |
|      ! 0 |  8768 | `				}` |
|      ! 0 |  8769 | `				continue;` |
|        - |  8770 | `			}` |
|      ! 0 |  8771 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8772 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8773 | `				zCur[0] = (char)c;` |
|      ! 0 |  8774 | `			}` |
|      ! 0 |  8775 | `			zCur++;` |
|      ! 0 |  8776 | `		}` |
|        - |  8777 | `		/* Finally,register the constant */` |
|      ! 0 |  8778 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8779 | `	}` |
|        - |  8780 | `	/* All done,return TRUE */` |
|       12 |  8781 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8782 | `	return SXRET_OK;` |
|        7 |  8783 |  |
|        - |  8784 | `/*` |
|        - |  8785 | ` * value constant(string $name)` |
|        - |  8786 | ` *  Returns the value of a constant` |
|        - |  8787 | ` * Parameter` |
|        - |  8788 | ` *  $name` |
|        - |  8789 | ` *    Name of the constant.` |
|        - |  8790 | ` * Return` |
|        - |  8791 | ` *  Constant value or NULL if not defined.` |
|        - |  8792 | ` */` |
|        8 |  8793 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8794 |  |
|        - |  8795 | `	SyHashEntry *pEntry;` |
|        - |  8796 | `	ph7_constant *pCons;` |
|        - |  8797 | `	const char *zName; /* Constant name */` |
|        - |  8798 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8799 | `	int nLen;` |
|       10 |  8800 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8801 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8802 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8803 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8804 | `		return SXRET_OK;` |
|        - |  8805 | `	}` |
|        - |  8806 | `	/* Extract the constant name */` |
|       10 |  8807 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8808 | `	/* Perform the query */` |
|       10 |  8809 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8810 | `	if( pEntry == 0 ){` |
|        3 |  8811 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8812 | `		ph7_result_null(pCtx);` |
|        3 |  8813 | `		return SXRET_OK;` |
|        - |  8814 | `	}` |
|        8 |  8815 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8816 | `	/* Point to the structure that describe the constant */` |
|        8 |  8817 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8818 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8819 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8820 | `	/* Return that value */` |
|        8 |  8821 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8822 | `	/* Cleanup */` |
|        8 |  8823 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8824 | `	return SXRET_OK;` |
|        6 |  8825 |  |
|        - |  8826 | `/*` |
|        - |  8827 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8828 | ` * defined below.` |
|        - |  8829 | ` */` |
|      444 |  8830 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8831 |  |
|      445 |  8832 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8833 | `	ph7_value sName;` |
|        - |  8834 | `	sxi32 rc;` |
|        - |  8835 | `	/* Prepare the constant name for insertion */` |
|      445 |  8836 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      445 |  8837 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8838 | `	/* Perform the insertion */` |
|      445 |  8839 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      445 |  8840 | `	PH7_MemObjRelease(&sName);` |
|      445 |  8841 | `	return rc;` |
|        1 |  8842 |  |
|        - |  8843 | `/*` |
|        - |  8844 | ` * array get_defined_constants(void)` |
|        - |  8845 | ` *  Returns an associative array with the names of all defined` |
|        - |  8846 | ` *  constants.` |
|        - |  8847 | ` * Parameters` |
|        - |  8848 | ` *  NONE.` |
|        - |  8849 | ` * Returns` |
|        - |  8850 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8851 | ` */` |
|        2 |  8852 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8853 |  |
|        - |  8854 | `	ph7_value *pArray;` |
|        - |  8855 | `	/* Create the array first*/` |
|        3 |  8856 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8857 | `	if( pArray == 0 ){` |
|      ! 0 |  8858 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8859 | `		SXUNUSED(apArg);` |
|        - |  8860 | `		/* Return NULL */` |
|      ! 0 |  8861 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8862 | `		return SXRET_OK;` |
|        - |  8863 | `	}` |
|        - |  8864 | `	/* Fill the array with the defined constants */` |
|        3 |  8865 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8866 | `	/* Return the created array */` |
|        3 |  8867 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8868 | `	return SXRET_OK;` |
|        2 |  8869 |  |
|        - |  8870 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  8871 | `/*` |
|        - |  8872 | ` * Section:` |
|        - |  8873 | ` *  Random numbers/string generators.` |
|        - |  8874 | ` * Status:` |
|        - |  8875 | ` *    Stable.` |
|        - |  8876 | ` */` |
|        - |  8877 | `/*` |
|        - |  8878 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8879 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8880 | ` * used by te SQLite3 library.` |
|        - |  8881 | ` */` |
|     2610 |  8882 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8883 |  |
|        - |  8884 | `	sxu32 iNum;` |
|     2612 |  8885 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2612 |  8886 | `	return iNum;` |
|        2 |  8887 |  |
|        - |  8888 | `/*` |
|        - |  8889 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8890 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8891 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8892 | ` * by te SQLite3 library.` |
|        - |  8893 | ` */` |
|   134792 |  8894 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8895 |  |
|        - |  8896 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8897 | `	int i;` |
|        - |  8898 | `	/* Generate a binary string first */` |
|   134794 |  8899 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8900 | `	/* Turn the binary string into english based alphabet */` |
|  1482882 |  8901 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1348090 |  8902 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   674046 |  8903 | `	 }` |
|   134794 |  8904 |  |
|        - |  8905 | `/*` |
|        - |  8906 | ` * int rand()` |
|        - |  8907 | ` * int mt_rand()` |
|        - |  8908 | ` * int rand(int $min,int $max)` |
|        - |  8909 | ` * int mt_rand(int $min,int $max)` |
|        - |  8910 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8911 | ` * Parameter` |
|        - |  8912 | ` *  $min` |
|        - |  8913 | ` *    The lowest value to return (default: 0)` |
|        - |  8914 | ` *  $max` |
|        - |  8915 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8916 | ` * Return` |
|        - |  8917 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8918 | ` * Note:` |
|        - |  8919 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8920 | ` *  by te SQLite3 library.` |
|        - |  8921 | ` */` |
|       20 |  8922 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8923 |  |
|        - |  8924 | `	sxu32 iNum;` |
|        - |  8925 | `	/* Generate the random number */` |
|       21 |  8926 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8927 | `	if( nArg > 1 ){` |
|        - |  8928 | `		sxu32 iMin,iMax;` |
|        3 |  8929 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8930 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8931 | `		if( iMin < iMax ){` |
|        3 |  8932 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8933 | `			if( iDiv > 0 ){` |
|        3 |  8934 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8935 | `			}` |
|        1 |  8936 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8937 | `			iNum %= iMax;` |
|      ! 0 |  8938 | `		}` |
|        1 |  8939 | `	}` |
|        - |  8940 | `	/* Return the number */` |
|       21 |  8941 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8942 | `	return SXRET_OK;` |
|        1 |  8943 |  |
|        - |  8944 | `/*` |
|        - |  8945 | ` * int getrandmax(void)` |
|        - |  8946 | ` * int mt_getrandmax(void)` |
|        - |  8947 | ` * int rc4_getrandmax(void)` |
|        - |  8948 | ` *   Show largest possible random value` |
|        - |  8949 | ` * Return` |
|        - |  8950 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8951 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8952 | ` * Note:` |
|        - |  8953 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8954 | ` *  by te SQLite3 library.` |
|        - |  8955 | ` */` |
|        4 |  8956 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8957 |  |
|        2 |  8958 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8959 | `	SXUNUSED(apArg);` |
|        5 |  8960 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8961 | `	return SXRET_OK;` |
|        1 |  8962 |  |
|        - |  8963 | `/*` |
|        - |  8964 | ` * string rand_str()` |
|        - |  8965 | ` * string rand_str(int $len)` |
|        - |  8966 | ` *  Generate a random string (English alphabet).` |
|        - |  8967 | ` * Parameter` |
|        - |  8968 | ` *  $len` |
|        - |  8969 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8970 | ` * Return` |
|        - |  8971 | ` *   A pseudo random string.` |
|        - |  8972 | ` * Note:` |
|        - |  8973 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8974 | ` *  by te SQLite3 library.` |
|        - |  8975 | ` *  This function is a symisc extension.` |
|        - |  8976 | ` */` |
|      120 |  8977 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8978 |  |
|        - |  8979 | `	char zString[1024];` |
|      122 |  8980 | `	int iLen = 0x10;` |
|      122 |  8981 | `	if( nArg > 0 ){` |
|        - |  8982 | `		/* Get the desired length */` |
|      122 |  8983 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8984 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8985 | `			/* Default length */` |
|        3 |  8986 | `			iLen = 0x10;` |
|        1 |  8987 | `		}` |
|       60 |  8988 | `	}` |
|        - |  8989 | `	/* Generate the random string */` |
|      122 |  8990 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8991 | `	/* Return the generated string */` |
|      122 |  8992 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8993 | `	return SXRET_OK;` |
|        2 |  8994 |  |
|        - |  8995 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8996 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8997 | `/* Unique ID private data */` |
|        - |  8998 | `struct unique_id_data` |
|        - |  8999 |  |
|        - |  9000 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9001 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9002 | `};` |
|        - |  9003 | `/*` |
|        - |  9004 | ` * Binary to hex consumer callback.` |
|        - |  9005 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9006 | ` * defined below.` |
|        - |  9007 | ` */` |
|      192 |  9008 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9009 |  |
|      193 |  9010 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9011 | `	sxu32 nBuflen;` |
|        - |  9012 | `	/* Extract result buffer length */` |
|      193 |  9013 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9014 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9015 | `			/*` |
|        - |  9016 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9017 | `			 * string will be 13 characters long` |
|        - |  9018 | `			 */` |
|       25 |  9019 | `		return SXERR_ABORT;` |
|        - |  9020 | `	}` |
|      169 |  9021 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9022 | `		return SXERR_ABORT;` |
|        - |  9023 | `	}` |
|        - |  9024 | `	/* Safely Consume the hex stream */` |
|      169 |  9025 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9026 | `	return SXRET_OK;` |
|       97 |  9027 |  |
|        - |  9028 | `/*` |
|        - |  9029 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9030 | ` *  Generate a unique ID` |
|        - |  9031 | ` * Parameter` |
|        - |  9032 | ` * $prefix` |
|        - |  9033 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9034 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9035 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9036 | ` * $more_entropy` |
|        - |  9037 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9038 | ` *  that the result will be unique.` |
|        - |  9039 | ` * Return` |
|        - |  9040 | ` *  Returns the unique identifier, as a string.` |
|        - |  9041 | ` */` |
|       24 |  9042 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9043 |  |
|        - |  9044 | `	struct unique_id_data sUniq;` |
|        - |  9045 | `	unsigned char zDigest[20];` |
|       25 |  9046 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9047 | `	const char *zPrefix;` |
|        - |  9048 | `	SHA1Context sCtx;` |
|        - |  9049 | `	char zRandom[7];` |
|        - |  9050 | `	int nPrefix;` |
|        - |  9051 | `	int entropy;` |
|        - |  9052 | `	/* Generate a random string first */` |
|       25 |  9053 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9054 | `	/* Initialize fields */` |
|       25 |  9055 | `	zPrefix = 0;` |
|       25 |  9056 | `	nPrefix = 0;` |
|       25 |  9057 | `	entropy = 0;` |
|       25 |  9058 | `	if( nArg > 0 ){` |
|        - |  9059 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9060 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9061 | `		if( nArg > 1 ){` |
|      ! 0 |  9062 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9063 | `		}` |
|      ! 0 |  9064 | `	}` |
|       25 |  9065 | `	SHA1Init(&sCtx);` |
|        - |  9066 | `	/* Generate the random ID */` |
|       25 |  9067 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9068 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9069 | `	}` |
|        - |  9070 | `	/* Append the random ID */` |
|       25 |  9071 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9072 | `	/* Append the random string */` |
|       25 |  9073 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9074 | `	/* Increment the number */` |
|       25 |  9075 | `	pVm->unique_id++;` |
|       25 |  9076 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9077 | `	/* Hexify the digest */` |
|       25 |  9078 | `	sUniq.pCtx = pCtx;` |
|       25 |  9079 | `	sUniq.entropy = entropy;` |
|       25 |  9080 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9081 | `	/* All done */` |
|       25 |  9082 | `	return PH7_OK;` |
|        1 |  9083 |  |
|        - |  9084 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9085 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9086 | `/*` |
|        - |  9087 | ` * Section:` |
|        - |  9088 | ` *  Language construct implementation as foreign functions.` |
|        - |  9089 | ` * Status:` |
|        - |  9090 | ` *    Stable.` |
|        - |  9091 | ` */` |
|        - |  9092 | `/*` |
|        - |  9093 | ` * void echo($string...)` |
|        - |  9094 | ` *  Output one or more messages.` |
|        - |  9095 | ` * Parameters` |
|        - |  9096 | ` *  $string` |
|        - |  9097 | ` *   Message to output.` |
|        - |  9098 | ` * Return` |
|        - |  9099 | ` *  NULL.` |
|        - |  9100 | ` */` |
|      ! 0 |  9101 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9102 |  |
|        - |  9103 | `	const char *zData;` |
|      ! 0 |  9104 | `	int nDataLen = 0;` |
|        - |  9105 | `	ph7_vm *pVm;` |
|        - |  9106 | `	int i,rc;` |
|        - |  9107 | `	/* Point to the target VM */` |
|      ! 0 |  9108 | `	pVm = pCtx->pVm;` |
|        - |  9109 | `	/* Output */` |
|      ! 0 |  9110 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9111 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9112 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9113 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9114 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9115 | `			if( rc == SXERR_ABORT ){` |
|        - |  9116 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9117 | `				return PH7_ABORT;` |
|        - |  9118 | `			}` |
|      ! 0 |  9119 | `		}` |
|      ! 0 |  9120 | `	}` |
|      ! 0 |  9121 | `	return SXRET_OK;` |
|      ! 0 |  9122 |  |
|        - |  9123 | `/*` |
|        - |  9124 | ` * int print($string...)` |
|        - |  9125 | ` *  Output one or more messages.` |
|        - |  9126 | ` * Parameters` |
|        - |  9127 | ` *  $string` |
|        - |  9128 | ` *   Message to output.` |
|        - |  9129 | ` * Return` |
|        - |  9130 | ` *  1 always.` |
|        - |  9131 | ` */` |
|        2 |  9132 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9133 |  |
|        - |  9134 | `	const char *zData;` |
|        3 |  9135 | `	int nDataLen = 0;` |
|        - |  9136 | `	ph7_vm *pVm;` |
|        - |  9137 | `	int i,rc;` |
|        - |  9138 | `	/* Point to the target VM */` |
|        3 |  9139 | `	pVm = pCtx->pVm;` |
|        - |  9140 | `	/* Output */` |
|        5 |  9141 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9142 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9143 | `		if( nDataLen > 0 ){` |
|        3 |  9144 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9145 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9146 | `			if( rc == SXERR_ABORT ){` |
|        - |  9147 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9148 | `				return PH7_ABORT;` |
|        - |  9149 | `			}` |
|        1 |  9150 | `		}` |
|        2 |  9151 | `	}` |
|        - |  9152 | `	/* Return 1 */` |
|        3 |  9153 | `	ph7_result_int(pCtx,1);` |
|        3 |  9154 | `	return SXRET_OK;` |
|        2 |  9155 |  |
|        - |  9156 | `/*` |
|        - |  9157 | ` * void exit(string $msg)` |
|        - |  9158 | ` * void exit(int $status)` |
|        - |  9159 | ` * void die(string $ms)` |
|        - |  9160 | ` * void die(int $status)` |
|        - |  9161 | ` *   Output a message and terminate program execution.` |
|        - |  9162 | ` * Parameter` |
|        - |  9163 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9164 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9165 | ` *  and not printed` |
|        - |  9166 | ` * Return` |
|        - |  9167 | ` *  NULL` |
|        - |  9168 | ` */` |
|      ! 0 |  9169 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9170 |  |
|      ! 0 |  9171 | `	if( nArg > 0 ){` |
|      ! 0 |  9172 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9173 | `			const char *zData;` |
|      ! 0 |  9174 | `			int iLen = 0;` |
|        - |  9175 | `			/* Print exit message */` |
|      ! 0 |  9176 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9177 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9178 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9179 | `			sxi32 iExitStatus;` |
|        - |  9180 | `			/* Record exit status code */` |
|      ! 0 |  9181 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9182 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9183 | `		}` |
|      ! 0 |  9184 | `	}` |
|        - |  9185 | `	/* Check if we are in an included file */` |
|      ! 0 |  9186 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9187 | `		/* Exit the entire process */` |
|      ! 0 |  9188 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9189 | `	}` |
|        - |  9190 | `	/* Abort processing immediately */` |
|      ! 0 |  9191 | `	return PH7_ABORT;` |
|      ! 0 |  9192 |  |
|        - |  9193 | `/*` |
|        - |  9194 | ` * bool isset($var,...)` |
|        - |  9195 | ` *  Finds out whether a variable is set.` |
|        - |  9196 | ` * Parameters` |
|        - |  9197 | ` *  One or more variable to check.` |
|        - |  9198 | ` * Return` |
|        - |  9199 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9200 | ` */` |
|    73702 |  9201 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9202 |  |
|        - |  9203 | `	ph7_value *pObj;` |
|    73704 |  9204 | `	int res = 0;` |
|        - |  9205 | `	int i;` |
|    73704 |  9206 | `	if( nArg < 1 ){` |
|        - |  9207 | `		/* Missing arguments,return false */` |
|      ! 0 |  9208 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9209 | `		return SXRET_OK;` |
|        - |  9210 | `	}` |
|        - |  9211 | `	/* Iterate over available arguments */` |
|    97230 |  9212 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    73704 |  9213 | `		pObj = apArg[i];` |
|    73704 |  9214 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    49664 |  9215 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9216 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9217 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9218 | `			}` |
|    24831 |  9219 | `		}` |
|    73704 |  9220 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    73704 |  9221 | `		if( !res ){` |
|        - |  9222 | `			/* Variable not set,return FALSE */` |
|    50178 |  9223 | `			ph7_result_bool(pCtx,0);` |
|    50178 |  9224 | `			return SXRET_OK;` |
|        - |  9225 | `		}` |
|    11765 |  9226 | `	}` |
|        - |  9227 | `	/* All given variable are set,return TRUE */` |
|    23528 |  9228 | `	ph7_result_bool(pCtx,1);` |
|    23528 |  9229 | `	return SXRET_OK;` |
|    36853 |  9230 |  |
|        - |  9231 | `/*` |
|        - |  9232 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9233 | ` * frame,the reference table and discard it's contents.` |
|        - |  9234 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9235 | ` */` |
|  2985342 |  9236 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9237 |  |
|        - |  9238 | `	ph7_value *pObj;` |
|        - |  9239 | `	VmRefObj *pRef;` |
|  2985344 |  9240 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2985344 |  9241 | `	if( pObj ){` |
|        - |  9242 | `		/* Release the object */` |
|  2985344 |  9243 | `		PH7_MemObjRelease(pObj);` |
|  1492671 |  9244 | `	}` |
|        - |  9245 | `	/* Remove old reference links */` |
|  2985344 |  9246 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2985344 |  9247 | `	if( pRef ){` |
|  2985338 |  9248 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9249 | `		/* Unlink from the reference table */` |
|  2985338 |  9250 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2985338 |  9251 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9252 | `			VmSlot sFree;` |
|        - |  9253 | `			/* Restore to the free list */` |
|  2985332 |  9254 | `			sFree.nIdx = nObjIdx;` |
|  2985332 |  9255 | `			sFree.pUserData = 0;` |
|  2985332 |  9256 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1492665 |  9257 | `		}` |
|  1492668 |  9258 | `	}` |
|  2985344 |  9259 | `	return SXRET_OK;` |
|        2 |  9260 |  |
|        - |  9261 | `/*` |
|        - |  9262 | ` * void unset($var,...)` |
|        - |  9263 | ` *   Unset one or more given variable.` |
|        - |  9264 | ` * Parameters` |
|        - |  9265 | ` *  One or more variable to unset.` |
|        - |  9266 | ` * Return` |
|        - |  9267 | ` *  Nothing.` |
|        - |  9268 | ` */` |
|     6678 |  9269 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9270 |  |
|        - |  9271 | `	ph7_value *pObj;` |
|        - |  9272 | `	ph7_vm *pVm;` |
|        - |  9273 | `	int i;` |
|        - |  9274 | `	/* Point to the target VM */` |
|     6680 |  9275 | `	pVm = pCtx->pVm;` |
|        - |  9276 | `	/* Iterate and unset */` |
|    13358 |  9277 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  9278 | `		pObj = apArg[i];` |
|     6680 |  9279 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9280 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9281 | `				/* Throw an error */` |
|      ! 0 |  9282 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9283 | `			}` |
|      ! 0 |  9284 | `		}else{` |
|     6680 |  9285 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9286 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  9287 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  9288 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  9289 | `			}` |
|        - |  9290 | `		}` |
|     3341 |  9291 | `	}` |
|     6680 |  9292 | `	return SXRET_OK;` |
|        2 |  9293 |  |
|        - |  9294 | `/*` |
|        - |  9295 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9296 | ` */` |
|      110 |  9297 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9298 |  |
|      111 |  9299 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9300 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9301 | `	ph7_value *pObj;` |
|        - |  9302 | `	sxu32 nIdx;` |
|        - |  9303 | `	/* Extract the memory object */` |
|      111 |  9304 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9305 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9306 | `	if( pObj ){` |
|      111 |  9307 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9308 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9309 | `				SyString sName;` |
|        - |  9310 | `				ph7_value sKey;` |
|        - |  9311 | `				/* Perform the insertion */` |
|      109 |  9312 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9313 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9314 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9315 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9316 | `			}` |
|       54 |  9317 | `		}` |
|       55 |  9318 | `	}` |
|      111 |  9319 | `	return SXRET_OK;` |
|        1 |  9320 |  |
|        - |  9321 | `/*` |
|        - |  9322 | ` * array get_defined_vars(void)` |
|        - |  9323 | ` *  Returns an array of all defined variables.` |
|        - |  9324 | ` * Parameter` |
|        - |  9325 | ` *  None` |
|        - |  9326 | ` * Return` |
|        - |  9327 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9328 | ` */` |
|        2 |  9329 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9330 |  |
|        3 |  9331 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9332 | `	ph7_value *pArray;` |
|        - |  9333 | `	/* Create a new array */` |
|        3 |  9334 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9335 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9336 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9337 | `		SXUNUSED(apArg);` |
|        - |  9338 | `		/* Return NULL */` |
|      ! 0 |  9339 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9340 | `		return SXRET_OK;` |
|        - |  9341 | `	}` |
|        - |  9342 | `	/* Superglobals first */` |
|        3 |  9343 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9344 | `	/* Then variable defined in the current frame */` |
|        3 |  9345 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9346 | `	/* Finally,return the created array */` |
|        3 |  9347 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9348 | `	return SXRET_OK;` |
|        2 |  9349 |  |
|        - |  9350 | `/*` |
|        - |  9351 | ` * bool gettype($var)` |
|        - |  9352 | ` *  Get the type of a variable` |
|        - |  9353 | ` * Parameters` |
|        - |  9354 | ` *   $var` |
|        - |  9355 | ` *    The variable being type checked.` |
|        - |  9356 | ` * Return` |
|        - |  9357 | ` *   String representation of the given variable type.` |
|        - |  9358 | ` */` |
|       32 |  9359 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9360 |  |
|       34 |  9361 | `	const char *zType = "Empty";` |
|       34 |  9362 | `	if( nArg > 0 ){` |
|       34 |  9363 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9364 | `	}` |
|        - |  9365 | `	/* Return the variable type */` |
|       34 |  9366 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9367 | `	return SXRET_OK;` |
|        2 |  9368 |  |
|        - |  9369 | `/*` |
|        - |  9370 | ` * string get_resource_type(resource $handle)` |
|        - |  9371 | ` *  This function gets the type of the given resource.` |
|        - |  9372 | ` * Parameters` |
|        - |  9373 | ` *  $handle` |
|        - |  9374 | ` *  The evaluated resource handle.` |
|        - |  9375 | ` * Return` |
|        - |  9376 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9377 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9378 | ` *  the return value will be the string Unknown.` |
|        - |  9379 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9380 | ` *  is not a resource.` |
|        - |  9381 | ` */` |
|        2 |  9382 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9383 |  |
|        3 |  9384 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9385 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9386 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9387 | `		return PH7_OK;` |
|        - |  9388 | `	}` |
|        3 |  9389 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9390 | `	return SXRET_OK;` |
|        2 |  9391 |  |
|        - |  9392 | `/*` |
|        - |  9393 | ` * void var_dump(expression,....)` |
|        - |  9394 | ` *   var_dump � Dumps information about a variable` |
|        - |  9395 | ` * Parameters` |
|        - |  9396 | ` *   One or more expression to dump.` |
|        - |  9397 | ` * Returns` |
|        - |  9398 | ` *  Nothing.` |
|        - |  9399 | ` */` |
|      218 |  9400 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9401 |  |
|        - |  9402 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9403 | `	int i;` |
|      220 |  9404 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9405 | `	/* Dump one or more expressions */` |
|      444 |  9406 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9407 | `		ph7_value *pObj = apArg[i];` |
|        - |  9408 | `		/* Reset the working buffer */` |
|      226 |  9409 | `		SyBlobReset(&sDump);` |
|        - |  9410 | `		/* Dump the given expression */` |
|      226 |  9411 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9412 | `		/* Output */` |
|      226 |  9413 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9414 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9415 | `		}` |
|      114 |  9416 | `	}` |
|        - |  9417 | `	/* Release the working buffer */` |
|      220 |  9418 | `	SyBlobRelease(&sDump);` |
|      220 |  9419 | `	return SXRET_OK;` |
|        2 |  9420 |  |
|        - |  9421 | `/*` |
|        - |  9422 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9423 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9424 | ` * Parameters` |
|        - |  9425 | ` *   expression: Expression to dump` |
|        - |  9426 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9427 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9428 | ` *            print_r() will return the information rather than print it.` |
|        - |  9429 | ` * Return` |
|        - |  9430 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9431 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9432 | ` */` |
|       16 |  9433 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9434 |  |
|       17 |  9435 | `	int ret_string = 0;` |
|        - |  9436 | `	SyBlob sDump;` |
|       17 |  9437 | `	if( nArg < 1 ){` |
|        - |  9438 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9439 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9440 | `		return SXRET_OK;` |
|        - |  9441 | `	}` |
|       17 |  9442 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9443 | `	if ( nArg > 1 ){` |
|        - |  9444 | `		/* Where to redirect output */` |
|       11 |  9445 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9446 | `	}` |
|        - |  9447 | `	/* Generate dump */` |
|       17 |  9448 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9449 | `	if( !ret_string ){` |
|        - |  9450 | `		/* Output dump */` |
|        7 |  9451 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9452 | `		/* Return true */` |
|        7 |  9453 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9454 | `	}else{` |
|        - |  9455 | `		/* Generated dump as return value */` |
|       11 |  9456 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9457 | `	}` |
|        - |  9458 | `	/* Release the working buffer */` |
|       17 |  9459 | `	SyBlobRelease(&sDump);` |
|       17 |  9460 | `	return SXRET_OK;` |
|        9 |  9461 |  |
|        - |  9462 | `/*` |
|        - |  9463 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9464 | ` * Same job as print_r. (see coment above)` |
|        - |  9465 | ` */` |
|        2 |  9466 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9467 |  |
|        3 |  9468 | `	int ret_string = 0;` |
|        - |  9469 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9470 | `	if( nArg < 1 ){` |
|        - |  9471 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9472 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9473 | `		return SXRET_OK;` |
|        - |  9474 | `	}` |
|        3 |  9475 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9476 | `	if ( nArg > 1 ){` |
|        - |  9477 | `		/* Where to redirect output */` |
|        3 |  9478 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9479 | `	}` |
|        - |  9480 | `	/* Generate dump */` |
|        3 |  9481 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9482 | `	if( !ret_string ){` |
|        - |  9483 | `		/* Output dump */` |
|      ! 0 |  9484 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9485 | `		/* Return NULL */` |
|      ! 0 |  9486 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9487 | `	}else{` |
|        - |  9488 | `		/* Generated dump as return value */` |
|        3 |  9489 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9490 | `	}` |
|        - |  9491 | `	/* Release the working buffer */` |
|        3 |  9492 | `	SyBlobRelease(&sDump);` |
|        3 |  9493 | `	return SXRET_OK;` |
|        2 |  9494 |  |
|        - |  9495 | `/*` |
|        - |  9496 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9497 | ` *  Set/get the various assert flags.` |
|        - |  9498 | ` * Parameter` |
|        - |  9499 | ` * $what` |
|        - |  9500 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9501 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9502 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9503 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9504 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9505 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9506 | ` * $value` |
|        - |  9507 | ` *   An optional new value for the option.` |
|        - |  9508 | ` * Return` |
|        - |  9509 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9510 | ` */` |
|       30 |  9511 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9512 |  |
|       32 |  9513 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9514 | `	int iOption;` |
|        - |  9515 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  9516 | `	if( nArg < 1 ){` |
|        3 |  9517 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9518 | `			"ArgumentCountError",` |
|        - |  9519 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9520 | `			);` |
|        - |  9521 | `	}` |
|        - |  9522 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  9523 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  9524 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9525 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9526 | `			"TypeError",` |
|        - |  9527 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9528 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9529 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9530 | `			);` |
|        - |  9531 | `	}` |
|       30 |  9532 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9533 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9534 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9535 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  9536 | `	switch( iOption ){` |
|        6 |  9537 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9538 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  9539 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  9540 | `		if( nArg > 1 ){` |
|        5 |  9541 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9542 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9543 | `			}else{` |
|        3 |  9544 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9545 | `			}` |
|        2 |  9546 | `		}` |
|       14 |  9547 | `		break;` |
|        1 |  9548 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9549 | `		/* Return old callback or null */` |
|        3 |  9550 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9551 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9552 | `		}else{` |
|        3 |  9553 | `			ph7_result_null(pCtx);` |
|        - |  9554 | `		}` |
|        3 |  9555 | `		if( nArg > 1 ){` |
|      ! 0 |  9556 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9557 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9558 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9559 | `			}else{` |
|      ! 0 |  9560 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9561 | `			}` |
|      ! 0 |  9562 | `		}` |
|        3 |  9563 | `		break;` |
|        5 |  9564 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9565 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9566 | `		if( nArg > 1 ){` |
|        5 |  9567 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9568 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9569 | `			}else{` |
|        3 |  9570 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9571 | `			}` |
|        2 |  9572 | `		}` |
|       11 |  9573 | `		break;` |
|      ! 0 |  9574 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9575 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9576 | `		break;` |
|        1 |  9577 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9578 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9579 | `		break;` |
|      ! 0 |  9580 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9581 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9582 | `		break;` |
|        1 |  9583 | `	default:` |
|        - |  9584 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9585 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9586 | `			"ValueError",` |
|        - |  9587 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9588 | `			);` |
|        - |  9589 | `	}` |
|       28 |  9590 | `	return PH7_OK;` |
|       17 |  9591 |  |
|        - |  9592 | `/*` |
|        - |  9593 | ` * bool assert(mixed $assertion)` |
|        - |  9594 | ` *  Checks if assertion is FALSE.` |
|        - |  9595 | ` * Parameter` |
|        - |  9596 | ` *  $assertion` |
|        - |  9597 | ` *    The assertion to test.` |
|        - |  9598 | ` * Return` |
|        - |  9599 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9600 | ` */` |
|       26 |  9601 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9602 |  |
|       28 |  9603 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9604 | `	int iFlags,iResult;` |
|        - |  9605 | `	const char *zDesc;` |
|        - |  9606 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  9607 | `	if( nArg < 1 ){` |
|        3 |  9608 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9609 | `			"ArgumentCountError",` |
|        - |  9610 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9611 | `			);` |
|        - |  9612 | `	}` |
|       26 |  9613 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  9614 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9615 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9616 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9617 | `		return PH7_OK;` |
|        - |  9618 | `	}` |
|        - |  9619 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  9620 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  9621 | `	if( !iResult ){` |
|        - |  9622 | `		/* Assertion failed */` |
|        - |  9623 | `		/* Extract optional description */` |
|       13 |  9624 | `		zDesc = 0;` |
|       13 |  9625 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9626 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9627 | `		}` |
|       13 |  9628 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9629 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9630 | `			ph7_value sFile,sLine;` |
|        - |  9631 | `			ph7_value *apCbArg[3];` |
|        - |  9632 | `			SyString *pFile;` |
|        - |  9633 | `			/* Extract the processed script */` |
|      ! 0 |  9634 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9635 | `			if( pFile == 0 ){` |
|      ! 0 |  9636 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9637 | `			}` |
|        - |  9638 | `			/* Invoke the callback */` |
|      ! 0 |  9639 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9640 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9641 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9642 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9643 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9644 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9645 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9646 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9647 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9648 | `		}` |
|       13 |  9649 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9650 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9651 | `			return PH7_ABORT;` |
|        - |  9652 | `		}` |
|        - |  9653 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9654 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9655 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9656 | `				"AssertionError",` |
|        - |  9657 | `				"%s",` |
|        1 |  9658 | `				zDesc` |
|        - |  9659 | `				);` |
|      ! 0 |  9660 | `		}else{` |
|       11 |  9661 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9662 | `				"AssertionError",` |
|        - |  9663 | `				"assert(false)"` |
|        - |  9664 | `				);` |
|        - |  9665 | `		}` |
|        - |  9666 | `	}` |
|        - |  9667 | `	/* Assertion passed */` |
|       14 |  9668 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9669 | `	return PH7_OK;` |
|       15 |  9670 |  |
|        - |  9671 | `/*` |
|        - |  9672 | ` * Section:` |
|        - |  9673 | ` *  Error reporting functions.` |
|        - |  9674 | ` * Status:` |
|        - |  9675 | ` *    Stable.` |
|        - |  9676 | ` */` |
|        - |  9677 | `/*` |
|        - |  9678 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9679 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9680 | ` * Parameters` |
|        - |  9681 | ` *  $error_msg` |
|        - |  9682 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9683 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9684 | ` * $error_type` |
|        - |  9685 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9686 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9687 | ` * Return` |
|        - |  9688 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9689 | ` */` |
|       12 |  9690 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9691 |  |
|       14 |  9692 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9693 | `	int rc = PH7_OK;` |
|       14 |  9694 | `	if( nArg > 0 ){` |
|        - |  9695 | `		const char *zErr;` |
|        - |  9696 | `		int nLen;` |
|        - |  9697 | `		/* Extract the error message */` |
|       12 |  9698 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9699 | `		if( nArg > 1 ){` |
|        - |  9700 | `			/* Extract the error type */` |
|       12 |  9701 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9702 | `			switch( nErr ){` |
|        1 |  9703 | `			case 1:   /* E_ERROR */` |
|        - |  9704 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9705 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9706 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9707 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9708 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9709 | `				break;` |
|        1 |  9710 | `			case 2:   /* E_WARNING */` |
|        - |  9711 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9712 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9713 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9714 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9715 | `				break;` |
|        3 |  9716 | `			default:` |
|        8 |  9717 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9718 | `				break;` |
|        - |  9719 | `			}` |
|        5 |  9720 | `		}` |
|        - |  9721 | `		/* Report error */` |
|       12 |  9722 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9723 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9724 | `			return rc;` |
|        - |  9725 | `		}` |
|        - |  9726 | `		/* Return true */` |
|       12 |  9727 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9728 | `	}else{` |
|        - |  9729 | `		/* Missing arguments,return FALSE */` |
|        3 |  9730 | `		ph7_result_bool(pCtx,0);` |
|        - |  9731 | `	}` |
|       14 |  9732 | `	return rc;` |
|        8 |  9733 |  |
|        - |  9734 | `/*` |
|        - |  9735 | ` * int error_reporting([int $level])` |
|        - |  9736 | ` *  Sets which PHP errors are reported.` |
|        - |  9737 | ` * Parameters` |
|        - |  9738 | ` *  $level` |
|        - |  9739 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9740 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9741 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9742 | ` *   levels will not always behave as expected.` |
|        - |  9743 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9744 | ` *   in the predefined constants.` |
|        - |  9745 | ` * Return` |
|        - |  9746 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9747 | ` *   parameter is given.` |
|        - |  9748 | ` */` |
|       42 |  9749 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9750 |  |
|       44 |  9751 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9752 | `	int nOld;` |
|        - |  9753 | `	/* Extract the old reporting level */` |
|       44 |  9754 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  9755 | `	if( nArg > 0 ){` |
|        - |  9756 | `		int nNew;` |
|        - |  9757 | `		/* Extract the desired error reporting level */` |
|       36 |  9758 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  9759 | `		if( !nNew ){` |
|        - |  9760 | `			/* Do not report errors at all */` |
|        5 |  9761 | `			pVm->bErrReport = 0;` |
|        3 |  9762 | `		}else{` |
|        - |  9763 | `			/* Report all errors */` |
|       32 |  9764 | `			pVm->bErrReport = 1;` |
|        - |  9765 | `		}` |
|       17 |  9766 | `	}` |
|        - |  9767 | `	/* Return the old level */` |
|       44 |  9768 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  9769 | `	return PH7_OK;` |
|        2 |  9770 |  |
|        - |  9771 | `/*` |
|        - |  9772 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9773 | ` *  Send an error message somewhere.` |
|        - |  9774 | ` * Parameter` |
|        - |  9775 | ` *  $message` |
|        - |  9776 | ` *   The error message that should be logged.` |
|        - |  9777 | ` *  $message_type` |
|        - |  9778 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9779 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9780 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9781 | ` *       This is the default option.` |
|        - |  9782 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9783 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9784 | ` *    2  No longer an option.` |
|        - |  9785 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9786 | ` *       to the end of the message string.` |
|        - |  9787 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9788 | ` *  $destination` |
|        - |  9789 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9790 | ` *  $extra_headers` |
|        - |  9791 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9792 | ` * Return` |
|        - |  9793 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9794 | ` * NOTE:` |
|        - |  9795 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9796 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9797 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9798 | ` *  Otherwise this function is no-op.` |
|        - |  9799 | ` */` |
|        4 |  9800 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9801 |  |
|        - |  9802 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9803 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9804 | `	int iType = 0;` |
|        5 |  9805 | `	if( nArg < 1 ){` |
|        - |  9806 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9807 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9808 | `		return PH7_OK;` |
|        - |  9809 | `	}` |
|        5 |  9810 | `	if( pVm->xErrLog  ){` |
|        - |  9811 | `		/* Invoke the user callback */` |
|      ! 0 |  9812 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9813 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9814 | `		if( nArg > 1 ){` |
|      ! 0 |  9815 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9816 | `			if( nArg > 2 ){` |
|      ! 0 |  9817 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9818 | `				if( nArg > 3 ){` |
|      ! 0 |  9819 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9820 | `				}` |
|      ! 0 |  9821 | `			}` |
|      ! 0 |  9822 | `		}` |
|      ! 0 |  9823 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9824 | `	}` |
|        - |  9825 | `	/* Retun TRUE */` |
|        5 |  9826 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9827 | `	return PH7_OK;` |
|        3 |  9828 |  |
|        - |  9829 | `/*` |
|        - |  9830 | ` * bool restore_exception_handler(void)` |
|        - |  9831 | ` *  Restores the previously defined exception handler function.` |
|        - |  9832 | ` * Parameter` |
|        - |  9833 | ` *  None` |
|        - |  9834 | ` * Return` |
|        - |  9835 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9836 | ` */` |
|        4 |  9837 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9838 |  |
|        5 |  9839 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9840 | `	ph7_value *pOld,*pNew;` |
|        - |  9841 | `	/* Point to the old and the new handler */` |
|        5 |  9842 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9843 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9844 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9845 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9846 | `		SXUNUSED(apArg);` |
|        - |  9847 | `		/* No installed handler,return FALSE */` |
|        5 |  9848 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9849 | `		return PH7_OK;` |
|        - |  9850 | `	}` |
|        - |  9851 | `	/* Copy the old handler */` |
|      ! 0 |  9852 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9853 | `	PH7_MemObjRelease(pOld);` |
|        - |  9854 | `	/* Return TRUE */` |
|      ! 0 |  9855 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9856 | `	return PH7_OK;` |
|        3 |  9857 |  |
|        - |  9858 | `/*` |
|        - |  9859 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9860 | ` *  Sets a user-defined exception handler function.` |
|        - |  9861 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9862 | ` * NOTE` |
|        - |  9863 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9864 | ` *  the satndard PHP engine.` |
|        - |  9865 | ` * Parameters` |
|        - |  9866 | ` *  $exception_handler` |
|        - |  9867 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9868 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9869 | ` *   that was thrown.` |
|        - |  9870 | ` *  Note:` |
|        - |  9871 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9872 | ` * Return` |
|        - |  9873 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9874 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9875 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9876 | ` */` |
|        4 |  9877 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9878 |  |
|        6 |  9879 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9880 | `	ph7_value *pOld,*pNew;` |
|        - |  9881 | `	/* Point to the old and the new handler */` |
|        6 |  9882 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9883 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9884 | `	/* Return the old handler */` |
|        6 |  9885 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9886 | `	if( nArg > 0 ){` |
|        6 |  9887 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9888 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9889 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9890 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9891 | `		}else{` |
|        6 |  9892 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9893 | `			/* Install the new handler */` |
|        6 |  9894 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9895 | `		}` |
|        2 |  9896 | `	}` |
|        6 |  9897 | `	return PH7_OK;` |
|        2 |  9898 |  |
|        - |  9899 | `/*` |
|        - |  9900 | ` * bool restore_error_handler(void)` |
|        - |  9901 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9902 | ` * Parameters:` |
|        - |  9903 | ` *  None.` |
|        - |  9904 | ` * Return` |
|        - |  9905 | ` *  Always TRUE.` |
|        - |  9906 | ` */` |
|        4 |  9907 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9908 |  |
|        5 |  9909 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9910 | `	ph7_value *pOld,*pNew;` |
|        - |  9911 | `	/* Point to the old and the new handler */` |
|        5 |  9912 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9913 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9914 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9915 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9916 | `		SXUNUSED(apArg);` |
|        - |  9917 | `		/* No installed callback,return FALSE */` |
|        5 |  9918 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9919 | `		return PH7_OK;` |
|        - |  9920 | `	}` |
|        - |  9921 | `	/* Copy the old callback */` |
|      ! 0 |  9922 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9923 | `	PH7_MemObjRelease(pOld);` |
|        - |  9924 | `	/* Return TRUE */` |
|      ! 0 |  9925 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9926 | `	return PH7_OK;` |
|        3 |  9927 |  |
|        - |  9928 | `/*` |
|        - |  9929 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9930 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9931 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9932 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9933 | ` *  Sets a user-defined error handler function.` |
|        - |  9934 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9935 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9936 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9937 | ` *  conditions (using trigger_error()).` |
|        - |  9938 | ` * Parameters` |
|        - |  9939 | ` *  $error_handler` |
|        - |  9940 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9941 | ` *   describing the error.` |
|        - |  9942 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9943 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9944 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9945 | ` *   The function can be shown as:` |
|        - |  9946 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9947 | ` *     errno` |
|        - |  9948 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9949 | ` *   errstr` |
|        - |  9950 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9951 | ` *   errfile` |
|        - |  9952 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9953 | ` *     was raised in, as a string.` |
|        - |  9954 | ` *  Note:` |
|        - |  9955 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9956 | ` * Return` |
|        - |  9957 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9958 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9959 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9960 | ` */` |
|     8822 |  9961 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9962 |  |
|     8824 |  9963 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9964 | `	ph7_value *pOld,*pNew;` |
|        - |  9965 | `	/* Point to the old and the new handler */` |
|     8824 |  9966 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  9967 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9968 | `	/* Return the old handler */` |
|     8824 |  9969 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  9970 | `	if( nArg > 0 ){` |
|     8824 |  9971 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9972 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  9973 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  9974 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  9975 | `		}else{` |
|     4414 |  9976 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9977 | `			/* Install the new handler */` |
|     4414 |  9978 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9979 | `		}` |
|     4411 |  9980 | `	}` |
|     8824 |  9981 | `	return PH7_OK;` |
|        2 |  9982 |  |
|        - |  9983 | `/*` |
|        - |  9984 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9985 | ` *  Generates a backtrace.` |
|        - |  9986 | ` * Paramaeter` |
|        - |  9987 | ` *  $options` |
|        - |  9988 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9989 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9990 | ` *   all the function/method arguments, to save memory.` |
|        - |  9991 | ` * $limit` |
|        - |  9992 | ` *   (Not Used)` |
|        - |  9993 | ` * Return` |
|        - |  9994 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9995 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9996 | ` *          Name        Type      Description` |
|        - |  9997 | ` *          ------      ------     -----------` |
|        - |  9998 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9999 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10000 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10001 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10002 | ` *          object      object    The current object.` |
|        - | 10003 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10004 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10005 | ` */` |
|      510 | 10006 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10007 |  |
|      512 | 10008 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10009 | `	ph7_value *pArray;` |
|        - | 10010 | `	ph7_class *pClass;` |
|        - | 10011 | `	ph7_value *pValue;` |
|        - | 10012 | `	SyString *pFile;` |
|        - | 10013 | `	/* Create a new array */` |
|      512 | 10014 | `	pArray = ph7_context_new_array(pCtx);` |
|      512 | 10015 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      512 | 10016 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10017 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10018 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10019 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10020 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10021 | `		SXUNUSED(apArg);` |
|      ! 0 | 10022 | `		return PH7_OK;` |
|        - | 10023 | `	}` |
|        - | 10024 | `	/* Dump running function name and it's arguments  */` |
|      512 | 10025 | `	if( pVm->pFrame->pParent ){` |
|      512 | 10026 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10027 | `		ph7_vm_func *pFunc;` |
|        - | 10028 | `		ph7_value *pArg;` |
|      512 | 10029 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      512 | 10030 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      512 | 10031 | `		if( pFrame->pParent && pFunc ){` |
|      512 | 10032 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      512 | 10033 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      512 | 10034 | `			ph7_value_reset_string_cursor(pValue);` |
|      255 | 10035 | `		}` |
|        - | 10036 | `		/* Function arguments */` |
|      512 | 10037 | `		pArg = ph7_context_new_array(pCtx);` |
|      512 | 10038 | `		if( pArg  ){` |
|        - | 10039 | `			ph7_value *pObj;` |
|        - | 10040 | `			VmSlot *aSlot;` |
|        - | 10041 | `			sxu32 n;` |
|        - | 10042 | `			/* Start filling the array with the given arguments */` |
|      512 | 10043 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2034 | 10044 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1524 | 10045 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1524 | 10046 | `				if( pObj ){` |
|     1524 | 10047 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      761 | 10048 | `				}` |
|      763 | 10049 | `			}` |
|        - | 10050 | `			/* Save the array */` |
|      512 | 10051 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      255 | 10052 | `		}` |
|      255 | 10053 | `	}` |
|      512 | 10054 | `	ph7_value_int(pValue,1);` |
|        - | 10055 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10056 | `	 * line numbers at run-time. )` |
|        - | 10057 | `	 */` |
|      512 | 10058 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10059 | `	/* Current processed script */` |
|      512 | 10060 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      512 | 10061 | `	if( pFile ){` |
|      512 | 10062 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      512 | 10063 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      512 | 10064 | `		ph7_value_reset_string_cursor(pValue);` |
|      255 | 10065 | `	}` |
|        - | 10066 | `	/* Top class */` |
|      512 | 10067 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      512 | 10068 | `	if( pClass ){` |
|      508 | 10069 | `		ph7_value_reset_string_cursor(pValue);` |
|      508 | 10070 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      508 | 10071 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      253 | 10072 | `	}` |
|        - | 10073 | `	/* Return the freshly created array */` |
|      512 | 10074 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10075 | `	/*` |
|        - | 10076 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10077 | `	 * as soon we return from this function.` |
|        - | 10078 | `	 */` |
|      512 | 10079 | `	return PH7_OK;` |
|      257 | 10080 |  |
|        - | 10081 | `/*` |
|        - | 10082 | ` * Generate a small backtrace.` |
|        - | 10083 | ` * Store the generated dump in the given BLOB` |
|        - | 10084 | ` */` |
|        4 | 10085 | `static int VmMiniBacktrace(` |
|        - | 10086 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10087 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10088 | `	)` |
|        1 | 10089 |  |
|        5 | 10090 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10091 | `	ph7_vm_func *pFunc;` |
|        - | 10092 | `	ph7_class *pClass;` |
|        - | 10093 | `	SyString *pFile;` |
|        - | 10094 | `	/* Called function */` |
|        5 | 10095 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10096 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10097 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10098 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10099 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10100 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10101 | `	}else{` |
|      ! 0 | 10102 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10103 | `	}` |
|        5 | 10104 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10105 | `	/* Current processed script */` |
|        5 | 10106 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10107 | `	if( pFile ){` |
|        5 | 10108 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10109 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10110 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10111 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10112 | `	}` |
|        - | 10113 | `	/* Top class */` |
|        5 | 10114 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10115 | `	if( pClass ){` |
|      ! 0 | 10116 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10117 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10118 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10119 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10120 | `	}` |
|        5 | 10121 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10122 | `	/* All done */` |
|        5 | 10123 | `	return SXRET_OK;` |
|        1 | 10124 |  |
|        - | 10125 | `/*` |
|        - | 10126 | ` * void debug_print_backtrace()` |
|        - | 10127 | ` *  Prints a backtrace` |
|        - | 10128 | ` * Parameters` |
|        - | 10129 | ` * None` |
|        - | 10130 | ` * Return` |
|        - | 10131 | ` * NULL` |
|        - | 10132 | ` */` |
|        2 | 10133 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10134 |  |
|        3 | 10135 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10136 | `	SyBlob sDump;` |
|        3 | 10137 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10138 | `	/* Generate the backtrace */` |
|        3 | 10139 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10140 | `	/* Output backtrace */` |
|        3 | 10141 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10142 | `	/* All done,cleanup */` |
|        3 | 10143 | `	SyBlobRelease(&sDump);` |
|        1 | 10144 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10145 | `	SXUNUSED(apArg);` |
|        3 | 10146 | `	return PH7_OK;` |
|        1 | 10147 |  |
|        - | 10148 | `/*` |
|        - | 10149 | ` * string debug_string_backtrace()` |
|        - | 10150 | ` *  Generate a backtrace` |
|        - | 10151 | ` * Parameters` |
|        - | 10152 | ` * None` |
|        - | 10153 | ` * Return` |
|        - | 10154 | ` *  A mini backtrace().` |
|        - | 10155 | ` * Note that this is a symisc extension.` |
|        - | 10156 | ` */` |
|        2 | 10157 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10158 |  |
|        3 | 10159 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10160 | `	SyBlob sDump;` |
|        3 | 10161 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10162 | `	/* Generate the backtrace */` |
|        3 | 10163 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10164 | `	/* Return the backtrace */` |
|        3 | 10165 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10166 | `	/* All done,cleanup */` |
|        3 | 10167 | `	SyBlobRelease(&sDump);` |
|        1 | 10168 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10169 | `	SXUNUSED(apArg);` |
|        3 | 10170 | `	return PH7_OK;` |
|        1 | 10171 |  |
|        - | 10172 | `/*` |
|        - | 10173 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10174 | ` * exception is triggered.` |
|        - | 10175 | ` */` |
|      472 | 10176 | `static sxi32 VmUncaughtException(` |
|        - | 10177 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10178 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10179 | `	)` |
|        1 | 10180 |  |
|        - | 10181 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10182 | `	int nArg = 1;` |
|        - | 10183 | `	sxi32 rc;` |
|      473 | 10184 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10185 | `		/* Nesting limit reached */` |
|      ! 0 | 10186 | `		return SXRET_OK;` |
|        - | 10187 | `	}` |
|        - | 10188 | `	/* Call any exception handler if available */` |
|      473 | 10189 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10190 | `	if( pThis ){` |
|        - | 10191 | `		/* Load the exception instance */` |
|      473 | 10192 | `		sArg.x.pOther = pThis;` |
|      473 | 10193 | `		pThis->iRef++;` |
|      473 | 10194 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10195 | `	}else{` |
|      ! 0 | 10196 | `		nArg = 0;` |
|        - | 10197 | `	}` |
|      473 | 10198 | `	apArg[0] = &sArg;` |
|        - | 10199 | `	/* Call the exception handler if available */` |
|      473 | 10200 | `	pVm->nExceptDepth++;` |
|      473 | 10201 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10202 | `	pVm->nExceptDepth--;` |
|      473 | 10203 | `	if( rc != SXRET_OK ){` |
|        - | 10204 | `		SyBlob sMsgBuf;` |
|      471 | 10205 | `		const char *zClass = "Exception";` |
|      471 | 10206 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10207 | `		const char *zMsg;` |
|        - | 10208 | `		sxu32 nMsg;` |
|        - | 10209 | `		const char *zFuncName;` |
|        - | 10210 | `		int nFuncLen;` |
|      471 | 10211 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10212 | `		if( pThis ){` |
|        - | 10213 | `			ph7_class_method *pGetMessage;` |
|        - | 10214 | `			ph7_value sMsg;` |
|        - | 10215 | `			const char *zTmp;` |
|        - | 10216 | `			int nTmp;` |
|      471 | 10217 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10218 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10219 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10220 | `			if( pGetMessage ){` |
|      471 | 10221 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10222 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10223 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10224 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10225 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10226 | `					}` |
|      235 | 10227 | `				}` |
|      471 | 10228 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10229 | `			}` |
|      235 | 10230 | `		}` |
|      471 | 10231 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10232 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10233 | `		}` |
|      471 | 10234 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10235 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10236 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10237 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10238 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10239 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10240 | `		rc = SXERR_ABORT;` |
|      235 | 10241 | `	}` |
|      473 | 10242 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10243 | `	return rc;` |
|      237 | 10244 |  |
|        - | 10245 | `/*` |
|        - | 10246 | ` * Throw a user exception.` |
|        - | 10247 | ` *` |
|        - | 10248 | ` * Exception dispatch follows this sequence:` |
|        - | 10249 | ` *` |
|        - | 10250 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10251 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10252 | ` *` |
|        - | 10253 | ` * 2. If NO catch matches:` |
|        - | 10254 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10255 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10256 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10257 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10258 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10259 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10260 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10261 | ` *` |
|        - | 10262 | ` * 3. If a catch DOES match:` |
|        - | 10263 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10264 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10265 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10266 | ` *       finally block.` |
|        - | 10267 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10268 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10269 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10270 | ` *       in pPendingException (step 2c).` |
|        - | 10271 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10272 | ` *    d. Run finally (if present).` |
|        - | 10273 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10274 | ` *       that handlers are restored and finally has run.` |
|        - | 10275 | ` */` |
|      514 | 10276 | `static sxi32 VmThrowException(` |
|        - | 10277 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10278 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10279 | `	)` |
|        2 | 10280 |  |
|        - | 10281 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10282 | `	ph7_exception **apException;` |
|        - | 10283 | `	ph7_exception *pException;` |
|        - | 10284 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10285 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10286 | `	pException = 0;` |
|      516 | 10287 | `	pCatch = 0;` |
|      516 | 10288 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10289 | `		ph7_exception_block *aCatch;` |
|        - | 10290 | `		ph7_class *pClass;` |
|        - | 10291 | `		sxu32 j;` |
|        - | 10292 | `		/* Locate the appropriate block to execute */` |
|       40 | 10293 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10294 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10295 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10296 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10297 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10298 | `			/* Extract the target class */` |
|       38 | 10299 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10300 | `			if( pClass == 0 ){` |
|        - | 10301 | `				/* No such class */` |
|      ! 0 | 10302 | `				continue;` |
|        - | 10303 | `			}` |
|       38 | 10304 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10305 | `				/* Catch block found,break immeditaley */` |
|       38 | 10306 | `				pCatch = &aCatch[j];` |
|       38 | 10307 | `				break;` |
|        - | 10308 | `			}` |
|      ! 0 | 10309 | `		}` |
|       19 | 10310 | `	}` |
|        - | 10311 | `	/* Execute the cached block if available */` |
|      516 | 10312 | `	if( pCatch == 0 ){` |
|        - | 10313 | `		sxi32 rc;` |
|        - | 10314 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10315 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10316 | `			pException->iFinallyDone = 1;` |
|        3 | 10317 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10318 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10319 | `				return SXERR_ABORT;` |
|        - | 10320 | `			}` |
|        1 | 10321 | `		}` |
|        - | 10322 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10323 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10324 | `			/* Re-throw to the outer handler */` |
|        3 | 10325 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10326 | `		}` |
|        - | 10327 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10328 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10329 | `		 * exception instead of reporting it uncaught.` |
|        - | 10330 | `		 */` |
|      478 | 10331 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10332 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10333 | `			 * by looking for a catch frame on the stack.` |
|        - | 10334 | `			 */` |
|      478 | 10335 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10336 | `			int inCatch = 0;` |
|      956 | 10337 | `			while( pF ){` |
|      484 | 10338 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 | 10339 | `					inCatch = 1;` |
|        6 | 10340 | `					break;` |
|        - | 10341 | `				}` |
|      479 | 10342 | `				pF = pF->pParent;` |
|        1 | 10343 | `			}` |
|      478 | 10344 | `			if( inCatch ){` |
|        - | 10345 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 | 10346 | `				pThis->iRef++;` |
|        6 | 10347 | `				pVm->pPendingException = pThis;` |
|        6 | 10348 | `				return SXRET_OK;` |
|        - | 10349 | `			}` |
|      236 | 10350 | `		}` |
|        - | 10351 | `		/* Truly uncaught */` |
|      473 | 10352 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10353 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10354 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10355 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10356 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10357 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10358 | `			}` |
|      ! 0 | 10359 | `		}` |
|      473 | 10360 | `		return rc;` |
|      ! 0 | 10361 | `	}else{` |
|       38 | 10362 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10363 | `		ph7_exception **apSaved = 0;` |
|        - | 10364 | `		sxu32 nSavedCount;` |
|        - | 10365 | `		sxi32 rc;` |
|       38 | 10366 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10367 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10368 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10369 | `		}` |
|        - | 10370 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10371 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10372 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10373 | `		 */` |
|       38 | 10374 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10375 | `		if( nSavedCount > 0 ){` |
|       11 | 10376 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10377 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10378 | `			if( apSaved ){` |
|       11 | 10379 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10380 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10381 | `				SySetReset(&pVm->aException);` |
|        3 | 10382 | `			}` |
|        3 | 10383 | `		}` |
|        - | 10384 | `		/* Create a private frame first */` |
|       38 | 10385 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10386 | `		if( rc == SXRET_OK ){` |
|       38 | 10387 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10388 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10389 | `			if( pObj ){` |
|       38 | 10390 | `				pThis->iRef++;` |
|       38 | 10391 | `				pObj->x.pOther = pThis;` |
|       38 | 10392 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10393 | `			}` |
|        - | 10394 | `			/* Execute the catch block */` |
|       38 | 10395 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10396 | `			/* Leave the frame */` |
|       38 | 10397 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10398 | `		}` |
|        - | 10399 | `		/* Restore the outer exception handlers */` |
|       38 | 10400 | `		if( apSaved ){` |
|        - | 10401 | `			sxu32 k;` |
|        - | 10402 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10403 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10404 | `			 * Restore the original outer entries.` |
|        - | 10405 | `			 */` |
|        8 | 10406 | `			SySetReset(&pVm->aException);` |
|       14 | 10407 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 | 10408 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10409 | `			}` |
|        8 | 10410 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10411 | `		}` |
|        - | 10412 | `		/* Execute the finally block after catch */` |
|       38 | 10413 | `		if( pException->iHasFinally ){` |
|       11 | 10414 | `			pException->iFinallyDone = 1;` |
|        - | 10415 | `			{` |
|       11 | 10416 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 | 10417 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10418 | `					return SXERR_ABORT;` |
|        - | 10419 | `				}` |
|        - | 10420 | `			}` |
|        5 | 10421 | `		}` |
|       38 | 10422 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10423 | `			return SXERR_ABORT;` |
|        - | 10424 | `		}` |
|        - | 10425 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10426 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10427 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10428 | `		 */` |
|       38 | 10429 | `		if( pVm->pPendingException ){` |
|        6 | 10430 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 | 10431 | `			pVm->pPendingException = 0;` |
|        6 | 10432 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10433 | `		}` |
|        - | 10434 | `	}` |
|        - | 10435 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10436 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10437 | `	 */` |
|       34 | 10438 | `	return SXRET_OK;` |
|      259 | 10439 |  |
|        - | 10440 | `/*` |
|        - | 10441 | ` * Section:` |
|        - | 10442 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10443 | ` * Status:` |
|        - | 10444 | ` *    Stable.` |
|        - | 10445 | ` */` |
|        - | 10446 | `/*` |
|        - | 10447 | ` * string ph7version(void)` |
|        - | 10448 | ` *  Returns the running version of the PH7 version.` |
|        - | 10449 | ` * Parameters` |
|        - | 10450 | ` *  None` |
|        - | 10451 | ` * Return` |
|        - | 10452 | ` * Current PH7 version.` |
|        - | 10453 | ` */` |
|        2 | 10454 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10455 |  |
|        1 | 10456 | `	SXUNUSED(nArg);` |
|        1 | 10457 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10458 | `	/* Current engine version */` |
|        3 | 10459 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10460 | `	return PH7_OK;` |
|        1 | 10461 |  |
|        - | 10462 | `/*` |
|        - | 10463 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10464 | ` */` |
|        - | 10465 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10466 | ` "<html><head>"\` |
|        - | 10467 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10468 | ` "<style type=\"text/css\">"\` |
|        - | 10469 | ` "div {"\` |
|        - | 10470 | `     "border: 1px solid #cccccc;"\` |
|        - | 10471 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10472 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10473 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10474 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10475 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10476 | `     "-o-border-radius: 10px;"\` |
|        - | 10477 | `     "border-radius: 10px;"\` |
|        - | 10478 | `     "padding-left: 2em;"\` |
|        - | 10479 | `     "background-color: white;"\` |
|        - | 10480 | `     "margin-left: auto;"\` |
|        - | 10481 | `     "font-family: verdana;"\` |
|        - | 10482 | `     "padding-right: 2em;"\` |
|        - | 10483 | `     "margin-right: auto;"\` |
|        - | 10484 | `     "}"\` |
|        - | 10485 | `     "body {"\` |
|        - | 10486 | `     "padding: 0.2em;"\` |
|        - | 10487 | `     "font-style: normal;"\` |
|        - | 10488 | `     "font-size: medium;"\` |
|        - | 10489 | `     "background-color: #f2f2f2;"\` |
|        - | 10490 | `     "}"\` |
|        - | 10491 | `     "hr {"\` |
|        - | 10492 | `     "border-style: solid none none;"\` |
|        - | 10493 | `     "border-width: 1px medium medium;"\` |
|        - | 10494 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10495 | `     "height: 1px;"\` |
|        - | 10496 | `     "}"\` |
|        - | 10497 | `     "a {"\` |
|        - | 10498 | `     "color: #3366cc;"\` |
|        - | 10499 | `     "text-decoration: none;"\` |
|        - | 10500 | `     "}"\` |
|        - | 10501 | `     "a:hover {"\` |
|        - | 10502 | `     "color: #999999;"\` |
|        - | 10503 | `     "}"\` |
|        - | 10504 | `     "a:active {"\` |
|        - | 10505 | `     "color: #663399;"\` |
|        - | 10506 | `     "}"\` |
|        - | 10507 | `     "h1 {"\` |
|        - | 10508 | `     "margin: 0;"\` |
|        - | 10509 | `     "padding: 0;"\` |
|        - | 10510 | `     "font-family: Verdana;"\` |
|        - | 10511 | `     "font-weight: bold;"\` |
|        - | 10512 | `     "font-style: normal;"\` |
|        - | 10513 | `     "font-size: medium;"\` |
|        - | 10514 | `     "text-transform: capitalize;"\` |
|        - | 10515 | `     "color: #0a328c;"\` |
|        - | 10516 | `     "}"\` |
|        - | 10517 | `     "p {"\` |
|        - | 10518 | `     "margin: 0 auto;"\` |
|        - | 10519 | `     "font-size: medium;"\` |
|        - | 10520 | `     "font-style: normal;"\` |
|        - | 10521 | `     "font-family: verdana;"\` |
|        - | 10522 | `     "}"\` |
|        - | 10523 | `"</style></head><body>"\` |
|        - | 10524 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10525 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10526 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10527 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10528 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10529 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10530 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10531 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10532 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10533 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10534 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10535 |  |
|        - | 10536 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10537 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10538 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10539 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10540 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10541 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10542 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10543 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10544 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10545 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10546 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10547 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10548 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10549 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10550 |  |
|        - | 10551 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10552 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10553 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10554 | `"&nbsp;*<br>"\` |
|        - | 10555 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10556 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10557 | `"&nbsp;* are met:<br>"\` |
|        - | 10558 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10559 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10560 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10561 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10562 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10563 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10564 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10565 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10566 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10567 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10568 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10569 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10570 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10571 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10572 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10573 | `"&nbsp;*<br>"\` |
|        - | 10574 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10575 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10576 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10577 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10578 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10579 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10580 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10581 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10582 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10583 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10584 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10585 | `"&nbsp;*/<br>"\` |
|        - | 10586 | `"</span></small></small></p>"\` |
|        - | 10587 | `"</div></body></html>"` |
|        - | 10588 | `/*` |
|        - | 10589 | ` * bool ph7credits(void)` |
|        - | 10590 | ` * bool ph7info(void)` |
|        - | 10591 | ` * bool ph7copyright(void)` |
|        - | 10592 | ` *  Prints out the credits for PH7 engine` |
|        - | 10593 | ` * Parameters` |
|        - | 10594 | ` *  None` |
|        - | 10595 | ` * Return` |
|        - | 10596 | ` *  Always TRUE` |
|        - | 10597 | ` */` |
|        2 | 10598 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10599 |  |
|        3 | 10600 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10601 | `	/* Expand the HTML page above*/` |
|        3 | 10602 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10603 | `	ph7_context_output_format(` |
|        1 | 10604 | `		pCtx,` |
|        - | 10605 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10606 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10607 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10608 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10609 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10610 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10611 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10612 | `#ifdef __WINNT__` |
|        - | 10613 | `		"Windows NT"` |
|        - | 10614 | `#elif defined(__UNIXES__)` |
|        - | 10615 | `		"UNIX-Like"` |
|        - | 10616 | `#else` |
|        - | 10617 | `		"Other OS"` |
|        - | 10618 | `#endif` |
|        - | 10619 | `		);` |
|        3 | 10620 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10621 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10622 | `	SXUNUSED(apArg);` |
|        - | 10623 | `	/* Return TRUE */` |
|        - | 10624 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10625 | `	return PH7_OK;` |
|        1 | 10626 |  |
|        - | 10627 | `/*` |
|        - | 10628 | ` * Section:` |
|        - | 10629 | ` *    URL related routines.` |
|        - | 10630 | ` * Status:` |
|        - | 10631 | ` *    Stable.` |
|        - | 10632 | ` */` |
|        - | 10633 | `/*` |
|        - | 10634 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10635 | ` *  Parse a URL and return its fields.` |
|        - | 10636 | ` * Parameters` |
|        - | 10637 | ` *  $url` |
|        - | 10638 | ` *   The URL to parse.` |
|        - | 10639 | ` * $component` |
|        - | 10640 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10641 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10642 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10643 | ` *  in which case the return value will be an integer).` |
|        - | 10644 | ` * Return` |
|        - | 10645 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10646 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10647 | ` *  this array are:` |
|        - | 10648 | ` *   scheme - e.g. http` |
|        - | 10649 | ` *   host` |
|        - | 10650 | ` *   port` |
|        - | 10651 | ` *   user` |
|        - | 10652 | ` *   pass` |
|        - | 10653 | ` *   path` |
|        - | 10654 | ` *   query - after the question mark ?` |
|        - | 10655 | ` *   fragment - after the hashmark #` |
|        - | 10656 | ` * Note:` |
|        - | 10657 | ` *  FALSE is returned on failure.` |
|        - | 10658 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10659 | ` *  with the standard PHP engine.` |
|        - | 10660 | ` */` |
|       28 | 10661 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10662 |  |
|        - | 10663 | `	const char *zStr; /* Input string */` |
|        - | 10664 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10665 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10666 | `	int nLen;` |
|        - | 10667 | `	sxi32 rc;` |
|       29 | 10668 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10669 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10670 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10671 | `		return PH7_OK;` |
|        - | 10672 | `	}` |
|        - | 10673 | `	/* Extract the given URI */` |
|       29 | 10674 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10675 | `	if( nLen < 1 ){` |
|        - | 10676 | `		/* Nothing to process,return FALSE */` |
|        3 | 10677 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10678 | `		return PH7_OK;` |
|        - | 10679 | `	}` |
|        - | 10680 | `	/* Get a parse */` |
|       27 | 10681 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10682 | `	if( rc != SXRET_OK ){` |
|        - | 10683 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10684 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10685 | `		return PH7_OK;` |
|        - | 10686 | `	}` |
|       27 | 10687 | `	if( nArg > 1 ){` |
|      ! 0 | 10688 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10689 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10690 | `		switch(nComponent){` |
|      ! 0 | 10691 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10692 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10693 | `			if( pComp->nByte < 1 ){` |
|        - | 10694 | `				/* No available value,return NULL */` |
|      ! 0 | 10695 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10696 | `			}else{` |
|      ! 0 | 10697 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10698 | `			}` |
|      ! 0 | 10699 | `			break;` |
|      ! 0 | 10700 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10701 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10702 | `			if( pComp->nByte < 1 ){` |
|        - | 10703 | `				/* No available value,return NULL */` |
|      ! 0 | 10704 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10705 | `			}else{` |
|      ! 0 | 10706 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10707 | `			}` |
|      ! 0 | 10708 | `			break;` |
|      ! 0 | 10709 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10710 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10711 | `			if( pComp->nByte < 1 ){` |
|        - | 10712 | `				/* No available value,return NULL */` |
|      ! 0 | 10713 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10714 | `			}else{` |
|      ! 0 | 10715 | `				int iPort = 0;` |
|        - | 10716 | `				/* Cast the value to integer */` |
|      ! 0 | 10717 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10718 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10719 | `			}` |
|      ! 0 | 10720 | `			break;` |
|      ! 0 | 10721 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10722 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10723 | `			if( pComp->nByte < 1 ){` |
|        - | 10724 | `				/* No available value,return NULL */` |
|      ! 0 | 10725 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10726 | `			}else{` |
|      ! 0 | 10727 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10728 | `			}` |
|      ! 0 | 10729 | `			break;` |
|      ! 0 | 10730 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10731 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10732 | `			if( pComp->nByte < 1 ){` |
|        - | 10733 | `				/* No available value,return NULL */` |
|      ! 0 | 10734 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10735 | `			}else{` |
|      ! 0 | 10736 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10737 | `			}` |
|      ! 0 | 10738 | `			break;` |
|      ! 0 | 10739 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10740 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10741 | `			if( pComp->nByte < 1 ){` |
|        - | 10742 | `				/* No available value,return NULL */` |
|      ! 0 | 10743 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10744 | `			}else{` |
|      ! 0 | 10745 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10746 | `			}` |
|      ! 0 | 10747 | `			break;` |
|      ! 0 | 10748 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10749 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10750 | `			if( pComp->nByte < 1 ){` |
|        - | 10751 | `				/* No available value,return NULL */` |
|      ! 0 | 10752 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10753 | `			}else{` |
|      ! 0 | 10754 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10755 | `			}` |
|      ! 0 | 10756 | `			break;` |
|      ! 0 | 10757 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10758 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10759 | `			if( pComp->nByte < 1 ){` |
|        - | 10760 | `				/* No available value,return NULL */` |
|      ! 0 | 10761 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10762 | `			}else{` |
|      ! 0 | 10763 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10764 | `			}` |
|      ! 0 | 10765 | `			break;` |
|      ! 0 | 10766 | `		default:` |
|        - | 10767 | `			/* No such entry,return NULL */` |
|      ! 0 | 10768 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10769 | `			break;` |
|        - | 10770 | `		}` |
|      ! 0 | 10771 | `	}else{` |
|        - | 10772 | `		ph7_value *pArray,*pValue;` |
|        - | 10773 | `		/* Return an associative array */` |
|       27 | 10774 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10775 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10776 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10777 | `			/* Out of memory */` |
|      ! 0 | 10778 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10779 | `			/* Return false */` |
|      ! 0 | 10780 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10781 | `			return PH7_OK;` |
|        - | 10782 | `		}` |
|        - | 10783 | `		/* Fill the array */` |
|       27 | 10784 | `		pComp = &sURI.sScheme;` |
|       27 | 10785 | `		if( pComp->nByte > 0 ){` |
|       19 | 10786 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10787 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10788 | `		}` |
|        - | 10789 | `		/* Reset the string cursor */` |
|       27 | 10790 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10791 | `		pComp = &sURI.sHost;` |
|       27 | 10792 | `		if( pComp->nByte > 0 ){` |
|       25 | 10793 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10794 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10795 | `		}` |
|        - | 10796 | `		/* Reset the string cursor */` |
|       27 | 10797 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10798 | `		pComp = &sURI.sPort;` |
|       27 | 10799 | `		if( pComp->nByte > 0 ){` |
|       11 | 10800 | `			int iPort = 0;/* cc warning */` |
|        - | 10801 | `			/* Convert to integer */` |
|       11 | 10802 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10803 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10804 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10805 | `		}` |
|        - | 10806 | `		/* Reset the string cursor */` |
|       27 | 10807 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10808 | `		pComp = &sURI.sUser;` |
|       27 | 10809 | `		if( pComp->nByte > 0 ){` |
|        7 | 10810 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10811 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10812 | `		}` |
|        - | 10813 | `		/* Reset the string cursor */` |
|       27 | 10814 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10815 | `		pComp = &sURI.sPass;` |
|       27 | 10816 | `		if( pComp->nByte > 0 ){` |
|        7 | 10817 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10818 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10819 | `		}` |
|        - | 10820 | `		/* Reset the string cursor */` |
|       27 | 10821 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10822 | `		pComp = &sURI.sPath;` |
|       27 | 10823 | `		if( pComp->nByte > 0 ){` |
|       17 | 10824 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10825 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10826 | `		}` |
|        - | 10827 | `		/* Reset the string cursor */` |
|       27 | 10828 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10829 | `		pComp = &sURI.sQuery;` |
|       27 | 10830 | `		if( pComp->nByte > 0 ){` |
|        5 | 10831 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10832 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10833 | `		}` |
|        - | 10834 | `		/* Reset the string cursor */` |
|       27 | 10835 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10836 | `		pComp = &sURI.sFragment;` |
|       27 | 10837 | `		if( pComp->nByte > 0 ){` |
|        5 | 10838 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10839 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10840 | `		}` |
|        - | 10841 | `		/* Return the created array */` |
|       27 | 10842 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10843 | `		/* NOTE:` |
|        - | 10844 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10845 | `		 * automatically as soon we return from this function.` |
|        - | 10846 | `		 */` |
|        - | 10847 | `	}` |
|        - | 10848 | `	/* All done */` |
|       27 | 10849 | `	return PH7_OK;` |
|       15 | 10850 |  |
|        - | 10851 | `/*` |
|        - | 10852 | ` * Section:` |
|        - | 10853 | ` *   Array related routines.` |
|        - | 10854 | ` * Status:` |
|        - | 10855 | ` *    Stable.` |
|        - | 10856 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10857 | ` *  Array related functions that need access to the underlying` |
|        - | 10858 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10859 | ` */` |
|        - | 10860 | `/*` |
|        - | 10861 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10862 | ` * of the following structure.` |
|        - | 10863 | ` */` |
|        - | 10864 | `struct compact_data` |
|        - | 10865 |  |
|        - | 10866 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10867 | `	int nRecCount;      /* Recursion count */` |
|        - | 10868 | `};` |
|        - | 10869 | `/*` |
|        - | 10870 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10871 | ` */` |
|      ! 0 | 10872 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10873 |  |
|      ! 0 | 10874 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10875 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10876 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10877 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10878 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10879 | `		SyString sVar;` |
|      ! 0 | 10880 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10881 | `		if( sVar.nByte > 0 ){` |
|        - | 10882 | `			/* Query the current frame */` |
|      ! 0 | 10883 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10884 | `			/* ^` |
|        - | 10885 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10886 | `			 */` |
|      ! 0 | 10887 | `			if( pKey ){` |
|        - | 10888 | `				/* Perform the insertion */` |
|      ! 0 | 10889 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10890 | `			}` |
|      ! 0 | 10891 | `		}` |
|      ! 0 | 10892 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10893 | `		int rc;` |
|        - | 10894 | `		/* Recursively traverse this array */` |
|      ! 0 | 10895 | `		pData->nRecCount++;` |
|      ! 0 | 10896 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10897 | `		pData->nRecCount--;` |
|      ! 0 | 10898 | `		return rc;` |
|        - | 10899 | `	}` |
|      ! 0 | 10900 | `	return SXRET_OK;` |
|      ! 0 | 10901 |  |
|        - | 10902 | `/*` |
|        - | 10903 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10904 | ` *  Create array containing variables and their values.` |
|        - | 10905 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10906 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10907 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10908 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10909 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10910 | ` * Parameters` |
|        - | 10911 | ` *  $varname` |
|        - | 10912 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10913 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10914 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10915 | ` *   it recursively.` |
|        - | 10916 | ` * Return` |
|        - | 10917 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10918 | ` */` |
|        2 | 10919 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10920 |  |
|        - | 10921 | `	ph7_value *pArray,*pObj;` |
|        3 | 10922 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10923 | `	const char *zName;` |
|        - | 10924 | `	SyString sVar;` |
|        - | 10925 | `	int i,nLen;` |
|        3 | 10926 | `	if( nArg < 1 ){` |
|        - | 10927 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10928 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10929 | `		return PH7_OK;` |
|        - | 10930 | `	}` |
|        - | 10931 | `	/* Create the array */` |
|        3 | 10932 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10933 | `	if( pArray == 0 ){` |
|        - | 10934 | `		/* Out of memory */` |
|      ! 0 | 10935 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10936 | `		/* Return NULL */` |
|      ! 0 | 10937 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10938 | `		return PH7_OK;` |
|        - | 10939 | `	}` |
|        - | 10940 | `	/* Perform the requested operation */` |
|        7 | 10941 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10942 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10943 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10944 | `				struct compact_data sData;` |
|      ! 0 | 10945 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10946 | `				/* Recursively walk the array */` |
|      ! 0 | 10947 | `				sData.nRecCount = 0;` |
|      ! 0 | 10948 | `				sData.pArray = pArray;` |
|      ! 0 | 10949 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10950 | `			}` |
|      ! 0 | 10951 | `		}else{` |
|        - | 10952 | `			/* Extract variable name */` |
|        5 | 10953 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10954 | `			if( nLen > 0 ){` |
|        5 | 10955 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10956 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10957 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10958 | `				if( pObj ){` |
|        5 | 10959 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10960 | `				}` |
|        2 | 10961 | `			}` |
|        - | 10962 | `		}` |
|        3 | 10963 | `	}` |
|        - | 10964 | `	/* Return the array */` |
|        3 | 10965 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10966 | `	return PH7_OK;` |
|        2 | 10967 |  |
|        - | 10968 | `/*` |
|        - | 10969 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10970 | ` * of the following structure.` |
|        - | 10971 | ` */` |
|        - | 10972 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10973 | `struct extract_aux_data` |
|        - | 10974 |  |
|        - | 10975 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10976 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10977 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10978 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10979 | `	int iFlags;           /* Control flags */` |
|        - | 10980 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10981 | `};` |
|        - | 10982 | `/* Forward declaration */` |
|        - | 10983 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10984 | `/*` |
|        - | 10985 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10986 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10987 | ` * Parameters` |
|        - | 10988 | ` * $var_array` |
|        - | 10989 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10990 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10991 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10992 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10993 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10994 | ` * $extract_type` |
|        - | 10995 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10996 | ` *  It can be one of the following values:` |
|        - | 10997 | ` *   EXTR_OVERWRITE` |
|        - | 10998 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10999 | ` *   EXTR_SKIP` |
|        - | 11000 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11001 | ` *   EXTR_PREFIX_SAME` |
|        - | 11002 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11003 | ` *   EXTR_PREFIX_ALL` |
|        - | 11004 | ` *       Prefix all variable names with prefix.` |
|        - | 11005 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11006 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11007 | ` *   EXTR_IF_EXISTS` |
|        - | 11008 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11009 | ` *       otherwise do nothing.` |
|        - | 11010 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11011 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11012 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11013 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11014 | ` *      the current symbol table.` |
|        - | 11015 | ` * $prefix` |
|        - | 11016 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11017 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11018 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11019 | ` *  underscore character.` |
|        - | 11020 | ` * Return` |
|        - | 11021 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11022 | ` */` |
|        4 | 11023 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11024 |  |
|        - | 11025 | `	extract_aux_data sAux;` |
|        - | 11026 | `	ph7_hashmap *pMap;` |
|        5 | 11027 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11028 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11029 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11030 | `		return PH7_OK;` |
|        - | 11031 | `	}` |
|        - | 11032 | `	/* Point to the target hashmap */` |
|        5 | 11033 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11034 | `	if( pMap->nEntry < 1 ){` |
|        - | 11035 | `		/* Empty map,return  0 */` |
|      ! 0 | 11036 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11037 | `		return PH7_OK;` |
|        - | 11038 | `	}` |
|        - | 11039 | `	/* Prepare the aux data */` |
|        5 | 11040 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11041 | `	if( nArg > 1 ){` |
|        3 | 11042 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11043 | `		if( nArg > 2 ){` |
|      ! 0 | 11044 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11045 | `		}` |
|        1 | 11046 | `	}` |
|        5 | 11047 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11048 | `	/* Invoke the worker callback */` |
|        5 | 11049 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11050 | `	/* Number of variables successfully imported */` |
|        5 | 11051 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11052 | `	return PH7_OK;` |
|        3 | 11053 |  |
|        - | 11054 | `/*` |
|        - | 11055 | ` * Worker callback for the [extract()] function defined` |
|        - | 11056 | ` * below.` |
|        - | 11057 | ` */` |
|        8 | 11058 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11059 |  |
|        9 | 11060 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11061 | `	int iFlags = pAux->iFlags;` |
|        9 | 11062 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11063 | `	ph7_value *pObj;` |
|        - | 11064 | `	SyString sVar;` |
|        9 | 11065 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11066 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11067 | `	}` |
|        - | 11068 | `	/* Perform a string cast */` |
|        9 | 11069 | `	PH7_MemObjToString(pKey);` |
|        9 | 11070 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11071 | `		/* Unavailable variable name */` |
|      ! 0 | 11072 | `		return SXRET_OK;` |
|        - | 11073 | `	}` |
|        9 | 11074 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11075 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11076 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11077 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11078 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11079 | `			);` |
|      ! 0 | 11080 | `	}else{` |
|       13 | 11081 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11082 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11083 | `	}` |
|        9 | 11084 | `	sVar.zString = pAux->zWorker;` |
|        - | 11085 | `	/* Try to extract the variable */` |
|        9 | 11086 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11087 | `	if( pObj ){` |
|        - | 11088 | `		/* Collision */` |
|        5 | 11089 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11090 | `			return SXRET_OK;` |
|        - | 11091 | `		}` |
|        5 | 11092 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11093 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11094 | `				/* Already prefixed */` |
|      ! 0 | 11095 | `				return SXRET_OK;` |
|        - | 11096 | `			}` |
|      ! 0 | 11097 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11098 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11099 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11100 | `				);` |
|      ! 0 | 11101 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11102 | `		}` |
|        3 | 11103 | `	}else{` |
|        - | 11104 | `		/* Create the variable */` |
|        5 | 11105 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11106 | `	}` |
|        9 | 11107 | `	if( pObj ){` |
|        - | 11108 | `		/* Overwrite the old value */` |
|        9 | 11109 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11110 | `		/* Increment counter */` |
|        9 | 11111 | `		pAux->iCount++;` |
|        4 | 11112 | `	}` |
|        9 | 11113 | `	return SXRET_OK;` |
|        5 | 11114 |  |
|        - | 11115 | `/*` |
|        - | 11116 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11117 | ` * defined below.` |
|        - | 11118 | ` */` |
|        2 | 11119 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11120 |  |
|        3 | 11121 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11122 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11123 | `	ph7_value *pObj;` |
|        - | 11124 | `	SyString sVar;` |
|        - | 11125 | `	/* Perform a string cast */` |
|        3 | 11126 | `	PH7_MemObjToString(pKey);` |
|        3 | 11127 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11128 | `		/* Unavailable variable name */` |
|      ! 0 | 11129 | `		return SXRET_OK;` |
|        - | 11130 | `	}` |
|        3 | 11131 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11132 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11133 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11134 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11135 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11136 | `			);` |
|        2 | 11137 | `	}else{` |
|      ! 0 | 11138 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11139 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11140 | `	}` |
|        3 | 11141 | `	sVar.zString = pAux->zWorker;` |
|        - | 11142 | `	/* Extract the variable */` |
|        3 | 11143 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11144 | `	if( pObj ){` |
|        3 | 11145 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11146 | `	}` |
|        3 | 11147 | `	return SXRET_OK;` |
|        2 | 11148 |  |
|        - | 11149 | `/*` |
|        - | 11150 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11151 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11152 | ` * Parameters` |
|        - | 11153 | ` * $types` |
|        - | 11154 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11155 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11156 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11157 | ` *  POST includes the POST uploaded file information.` |
|        - | 11158 | ` *  Note:` |
|        - | 11159 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11160 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11161 | ` * $prefix` |
|        - | 11162 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11163 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11164 | ` *  variable named $pref_userid.` |
|        - | 11165 | ` * Return` |
|        - | 11166 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11167 | ` */` |
|        2 | 11168 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11169 |  |
|        - | 11170 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11171 | `	extract_aux_data sAux;` |
|        - | 11172 | `	int nLen,nPrefixLen;` |
|        - | 11173 | `	ph7_value *pSuper;` |
|        - | 11174 | `	ph7_vm *pVm;` |
|        - | 11175 | `	/* By default import only $_GET variables  */` |
|        3 | 11176 | `	zImport = "G";` |
|        3 | 11177 | `	nLen = (int)sizeof(char);` |
|        3 | 11178 | `	zPrefix = 0;` |
|        3 | 11179 | `	nPrefixLen = 0;` |
|        3 | 11180 | `	if( nArg > 0 ){` |
|        3 | 11181 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11182 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11183 | `		}` |
|        3 | 11184 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11185 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11186 | `		}` |
|        1 | 11187 | `	}` |
|        - | 11188 | `	/* Point to the underlying VM */` |
|        3 | 11189 | `	pVm = pCtx->pVm;` |
|        - | 11190 | `	/* Initialize the aux data */` |
|        3 | 11191 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11192 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11193 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11194 | `	sAux.pVm = pVm;` |
|        - | 11195 | `	/* Extract */` |
|        3 | 11196 | `	zEnd = &zImport[nLen];` |
|        5 | 11197 | `	while( zImport < zEnd ){` |
|        3 | 11198 | `		int c = zImport[0];` |
|        3 | 11199 | `		pSuper = 0;` |
|        3 | 11200 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11201 | `			/* Import $_GET variables */` |
|        3 | 11202 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11203 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11204 | `			/* Import $_POST variables */` |
|      ! 0 | 11205 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11206 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11207 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11208 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11209 | `		}` |
|        3 | 11210 | `		if( pSuper ){` |
|        - | 11211 | `			/* Iterate throw array entries */` |
|        3 | 11212 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11213 | `		}` |
|        - | 11214 | `		/* Advance the cursor */` |
|        3 | 11215 | `		zImport++;` |
|        1 | 11216 | `	}` |
|        - | 11217 | `	/* All done,return TRUE*/` |
|        3 | 11218 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11219 | `	return PH7_OK;` |
|        1 | 11220 |  |
|        - | 11221 | `/*` |
|        - | 11222 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11223 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11224 | ` * information.` |
|        - | 11225 | ` */` |
|    10446 | 11226 | `static sxi32 VmEvalChunk(` |
|        - | 11227 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11228 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11229 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11230 | `	int iFlags,         /* Compile flag */` |
|        - | 11231 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11232 | `	)` |
|        2 | 11233 |  |
|        - | 11234 | `	SySet *pByteCode,aByteCode;` |
|        - | 11235 | `	SyBlob sSavedNs;` |
|    10448 | 11236 | `	ProcConsumer xErr = 0;` |
|    10448 | 11237 | `	void *pErrData = 0;` |
|        - | 11238 | `	/* Initialize bytecode container */` |
|    10448 | 11239 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10448 | 11240 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11241 | `	/* Reset the code generator */` |
|    10448 | 11242 | `	if( bTrueReturn ){` |
|        - | 11243 | `		/* Included file,log compile-time errors */` |
|     7637 | 11244 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 | 11245 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 | 11246 | `	}` |
|    10448 | 11247 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11248 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11249 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11250 | `	 * the caller's namespace is restored. */` |
|    10448 | 11251 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10448 | 11252 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10448 | 11253 | `	if( bTrueReturn ){` |
|        - | 11254 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 | 11255 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 | 11256 | `	}` |
|        - | 11257 | `	/* Swap bytecode container */` |
|    10448 | 11258 | `	pByteCode = pVm->pByteContainer;` |
|    10448 | 11259 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11260 | `	/* Compile the chunk */` |
|    10448 | 11261 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15671 | 11262 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11263 | `		/* Compilation error,return false */` |
|        3 | 11264 | `		if( pCtx ){` |
|        3 | 11265 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11266 | `		}` |
|        2 | 11267 | `	}else{` |
|        - | 11268 | `		/* Mount any newly defined classes */` |
|        - | 11269 | `		SyHashEntry *pEntry;` |
|        - | 11270 | `		ph7_class *pClass;` |
|        - | 11271 | `		ph7_value sResult; /* Return value */` |
|        - | 11272 | `		sxi32 rc;` |
|    10446 | 11273 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   316150 | 11274 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   300484 | 11275 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11276 | `			/* Only mount classes that haven't been mounted yet */` |
|   300484 | 11277 | `			if( !pClass->bMounted ){` |
|    74548 | 11278 | `				rc = VmMountUserClass(pVm,pClass);` |
|    74548 | 11279 | `				if( rc != SXRET_OK ){` |
|        - | 11280 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11281 | `					if( pCtx ){` |
|      ! 0 | 11282 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11283 | `					}` |
|      ! 0 | 11284 | `					goto Cleanup;` |
|        - | 11285 | `				}` |
|    37273 | 11286 | `			}` |
|        2 | 11287 | `		}` |
|    10446 | 11288 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11289 | `			/* Out of memory */` |
|      ! 0 | 11290 | `			if( pCtx ){` |
|      ! 0 | 11291 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11292 | `			}` |
|      ! 0 | 11293 | `			goto Cleanup;` |
|        - | 11294 | `		}` |
|    10446 | 11295 | `		if( bTrueReturn ){` |
|        - | 11296 | `			/* Assume a boolean true return value */` |
|     7637 | 11297 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 | 11298 | `		}else{` |
|        - | 11299 | `			/* Assume a null return value */` |
|     2810 | 11300 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11301 | `		}` |
|        - | 11302 | `		/* Execute the compiled chunk */` |
|    10446 | 11303 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10446 | 11304 | `		if( pCtx ){` |
|        - | 11305 | `			/* Set the execution result */` |
|     7650 | 11306 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 | 11307 | `		}` |
|    10446 | 11308 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11309 | `	}` |
|     5223 | 11310 | `Cleanup:` |
|        - | 11311 | `	/* Cleanup the mess left behind */` |
|    10448 | 11312 | `	pVm->pByteContainer = pByteCode;` |
|    10448 | 11313 | `	SySetRelease(&aByteCode);` |
|        - | 11314 | `	/* Restore caller's namespace state */` |
|    10448 | 11315 | `	SyBlobReset(&pVm->sNamespace);` |
|    10448 | 11316 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10448 | 11317 | `	SyBlobRelease(&sSavedNs);` |
|    10448 | 11318 | `	return SXRET_OK;` |
|        2 | 11319 |  |
|        - | 11320 | `/*` |
|        - | 11321 | ` * value eval(string $code)` |
|        - | 11322 | ` *   Evaluate a string as PHP code.` |
|        - | 11323 | ` * Parameter` |
|        - | 11324 | ` *  code: PHP code to evaluate.` |
|        - | 11325 | ` * Return` |
|        - | 11326 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11327 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11328 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11329 | ` */` |
|       16 | 11330 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11331 |  |
|        - | 11332 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 11333 | `	if( nArg < 1 ){` |
|        - | 11334 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11335 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11336 | `		return SXRET_OK;` |
|        - | 11337 | `	}` |
|        - | 11338 | `	/* Chunk to evaluate */` |
|       18 | 11339 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 11340 | `	if( sChunk.nByte < 1 ){` |
|        - | 11341 | `		/* Empty string,return NULL */` |
|        3 | 11342 | `		ph7_result_null(pCtx);` |
|        3 | 11343 | `		return SXRET_OK;` |
|        - | 11344 | `	}` |
|        - | 11345 | `	/* Eval the chunk */` |
|       16 | 11346 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 11347 | `	return SXRET_OK;` |
|       10 | 11348 |  |
|        - | 11349 | `/*` |
|        - | 11350 | ` * Check if a file path is already included.` |
|        - | 11351 | ` */` |
|    15268 | 11352 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 11353 |  |
|        - | 11354 | `	SyString *aEntries;` |
|        - | 11355 | `	sxu32 n;` |
|    15269 | 11356 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11357 | `	/* Perform a linear search */` |
| 58267061 | 11358 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 11359 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11360 | `			/* Already included */` |
|        7 | 11361 | `			return TRUE;` |
|        - | 11362 | `		}` |
| 29125897 | 11363 | `	}` |
|    15263 | 11364 | `	return FALSE;` |
|     7635 | 11365 |  |
|        - | 11366 | `/*` |
|        - | 11367 | ` * Push a file path in the appropriate VM container.` |
|        - | 11368 | ` */` |
|    18056 | 11369 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11370 |  |
|        - | 11371 | `	SyString sPath;` |
|        - | 11372 | `	char *zDup;` |
|        - | 11373 | `#ifdef __WINNT__` |
|        - | 11374 | `	char *zCur;` |
|        - | 11375 | `#endif` |
|        - | 11376 | `	sxi32 rc;` |
|    18058 | 11377 | `	if( nLen < 0 ){` |
|     2790 | 11378 | `		nLen = SyStrlen(zPath);` |
|     1394 | 11379 | `	}` |
|        - | 11380 | `	/* Duplicate the file path first */` |
|    18058 | 11381 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18058 | 11382 | `	if( zDup == 0 ){` |
|      ! 0 | 11383 | `		return SXERR_MEM;` |
|        - | 11384 | `	}` |
|        - | 11385 | `#ifdef __WINNT__` |
|        - | 11386 | `	/* Normalize path on windows` |
|        - | 11387 | `	 * Example:` |
|        - | 11388 | `	 *    Path/To/File.php` |
|        - | 11389 | `	 * becomes` |
|        - | 11390 | `	 *   path\to\file.php` |
|        - | 11391 | `	 */` |
|        2 | 11392 | `	zCur = zDup;` |
|        2 | 11393 | `	while( zCur[0] != 0 ){` |
|        2 | 11394 | `		if( zCur[0] == '/' ){` |
|        2 | 11395 | `			zCur[0] = '\\';` |
|        2 | 11396 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11397 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11398 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11399 | `		}` |
|        2 | 11400 | `		zCur++;` |
|        2 | 11401 | `	}` |
|        - | 11402 | `#endif` |
|        - | 11403 | `	/* Install the file path */` |
|    18058 | 11404 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18058 | 11405 | `	if( !bMain ){` |
|    15269 | 11406 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11407 | `			/* Already included */` |
|        7 | 11408 | `			*pNew = 0;` |
|        4 | 11409 | `		}else{` |
|        - | 11410 | `			/* Insert in the corresponding container */` |
|    15263 | 11411 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 11412 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11413 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11414 | `				return rc;` |
|        - | 11415 | `			}` |
|    15263 | 11416 | `			*pNew = 1;` |
|        - | 11417 | `		}` |
|     7634 | 11418 | `	}` |
|    18058 | 11419 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18058 | 11420 | `	return SXRET_OK;` |
|     9030 | 11421 |  |
|        - | 11422 | `/*` |
|        - | 11423 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11424 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11425 | ` * indicates failure.` |
|        - | 11426 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11427 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11428 | ` * operations.` |
|        - | 11429 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11430 | ` * this function is a no-op.` |
|        - | 11431 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11432 | ` * constructs for more information.` |
|        - | 11433 | ` */` |
|     7642 | 11434 | `static sxi32 VmExecIncludedFile(` |
|        - | 11435 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11436 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11437 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11438 | `	 )` |
|        2 | 11439 |  |
|        - | 11440 | `	sxi32 rc;` |
|        - | 11441 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11442 | `	const ph7_io_stream *pStream;` |
|        - | 11443 | `	SyBlob sContents;` |
|        - | 11444 | `	void *pHandle;` |
|        - | 11445 | `	ph7_vm *pVm;` |
|        - | 11446 | `	int isNew;` |
|        - | 11447 | `	/* Initialize fields */` |
|     7644 | 11448 | `	pVm = pCtx->pVm;` |
|     7644 | 11449 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 11450 | `	isNew = 0;` |
|        - | 11451 | `	/* Extract the associated stream */` |
|     7644 | 11452 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11453 | `	/*` |
|        - | 11454 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11455 | `	 * in a read-only mode.` |
|        - | 11456 | `	 */` |
|     7644 | 11457 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 11458 | `	if( pHandle == 0 ){` |
|        3 | 11459 | `		return SXERR_IO;` |
|        - | 11460 | `	}` |
|     7641 | 11461 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 11462 | `	if( IncludeOnce && !isNew ){` |
|        - | 11463 | `		/* Already included */` |
|        5 | 11464 | `		rc = SXERR_EXISTS;` |
|        3 | 11465 | `	}else{` |
|        - | 11466 | `		/* Read the whole file contents */` |
|     7637 | 11467 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 11468 | `		if( rc == SXRET_OK ){` |
|        - | 11469 | `			SyString sScript;` |
|        - | 11470 | `			/* Compile and execute the script */` |
|     7637 | 11471 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 11472 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 11473 | `		}` |
|        - | 11474 | `	}` |
|        - | 11475 | `	/* Pop from the set of included file */` |
|     7641 | 11476 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11477 | `	/* Close the handle */` |
|     7641 | 11478 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11479 | `	/* Release the working buffer */` |
|     7641 | 11480 | `	SyBlobRelease(&sContents);` |
|        - | 11481 | `#else` |
|        - | 11482 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11483 | `	SXUNUSED(pPath);` |
|        - | 11484 | `	SXUNUSED(IncludeOnce);` |
|        - | 11485 | `	rc = SXERR_IO;` |
|        - | 11486 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 11487 | `	return rc;` |
|     3823 | 11488 |  |
|        - | 11489 | `/*` |
|        - | 11490 | ` * string get_include_path(void)` |
|        - | 11491 | ` *  Gets the current include_path configuration option.` |
|        - | 11492 | ` * Parameter` |
|        - | 11493 | ` *  None` |
|        - | 11494 | ` * Return` |
|        - | 11495 | ` *  Included paths as a string` |
|        - | 11496 | ` */` |
|        2 | 11497 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11498 |  |
|        3 | 11499 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11500 | `	SyString *aEntry;` |
|        - | 11501 | `	int dir_sep;` |
|        - | 11502 | `	sxu32 n;` |
|        - | 11503 | `#ifdef __WINNT__` |
|        1 | 11504 | `	dir_sep = ';';` |
|        - | 11505 | `#else` |
|        - | 11506 | `	/* Assume UNIX path separator */` |
|        2 | 11507 | `	dir_sep = ':';` |
|        - | 11508 | `#endif` |
|        1 | 11509 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11510 | `	SXUNUSED(apArg);` |
|        - | 11511 | `	/* Point to the list of import paths */` |
|        3 | 11512 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11513 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11514 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11515 | `		if( n > 0 ){` |
|        - | 11516 | `			/* Append dir seprator */` |
|      ! 0 | 11517 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11518 | `		}` |
|        - | 11519 | `		/* Append path */` |
|        3 | 11520 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11521 | `	}` |
|        3 | 11522 | `	return PH7_OK;` |
|        1 | 11523 |  |
|        - | 11524 | `/*` |
|        - | 11525 | ` * string get_get_included_files(void)` |
|        - | 11526 | ` *  Gets the current include_path configuration option.` |
|        - | 11527 | ` * Parameter` |
|        - | 11528 | ` *  None` |
|        - | 11529 | ` * Return` |
|        - | 11530 | ` *  Included paths as a string` |
|        - | 11531 | ` */` |
|        2 | 11532 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11533 |  |
|        3 | 11534 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11535 | `	ph7_value *pArray,*pWorker;` |
|        - | 11536 | `	SyString *pEntry;` |
|        - | 11537 | `	int c,d;` |
|        - | 11538 | `	/* Create an array and a working value */` |
|        3 | 11539 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11540 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11541 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11542 | `		/* Out of memory,return null */` |
|      ! 0 | 11543 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11544 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11545 | `		SXUNUSED(apArg);` |
|      ! 0 | 11546 | `		return PH7_OK;` |
|        - | 11547 | `	}` |
|        3 | 11548 | `	c = d = '/';` |
|        - | 11549 | `#ifdef __WINNT__` |
|        1 | 11550 | `	d = '\\';` |
|        - | 11551 | `#endif` |
|        - | 11552 | `	/* Iterate throw entries */` |
|        3 | 11553 | `	SySetResetCursor(pFiles);` |
|     3689 | 11554 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11555 | `		const char *zBase,*zEnd;` |
|        - | 11556 | `		int iLen;` |
|        - | 11557 | `		/* reset the string cursor */` |
|     3687 | 11558 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11559 | `		/* Extract base name */` |
|     3687 | 11560 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11561 | `		/* Ignore trailing '/' */` |
|     5530 | 11562 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11563 | `			zEnd--;` |
|      ! 0 | 11564 | `		}` |
|     3687 | 11565 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 11566 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 11567 | `			zEnd--;` |
|        1 | 11568 | `		}` |
|     3687 | 11569 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 11570 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11571 | `		/* Copy entry name */` |
|     3687 | 11572 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11573 | `		/* Perform the insertion */` |
|     3687 | 11574 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11575 | `	}` |
|        - | 11576 | `	/* All done,return the created array */` |
|        3 | 11577 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11578 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11579 | `	 * by the engine as soon we return from this foreign` |
|        - | 11580 | `	 * function.` |
|        - | 11581 | `	 */` |
|        3 | 11582 | `	return PH7_OK;` |
|        2 | 11583 |  |
|        - | 11584 | `/*` |
|        - | 11585 | ` * include:` |
|        - | 11586 | ` * According to the PHP reference manual.` |
|        - | 11587 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11588 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11589 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11590 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11591 | ` *  and the current working directory before failing. The include()` |
|        - | 11592 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11593 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11594 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11595 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11596 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11597 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11598 | ` *  directory to find the requested file.` |
|        - | 11599 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11600 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11601 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11602 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11603 | ` */` |
|     7630 | 11604 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11605 |  |
|        - | 11606 | `	SyString sFile;` |
|        - | 11607 | `	sxi32 rc;` |
|     7632 | 11608 | `	if( nArg < 1 ){` |
|        - | 11609 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11610 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11611 | `		return SXRET_OK;` |
|        - | 11612 | `	}` |
|        - | 11613 | `	/* File to include */` |
|     7632 | 11614 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 11615 | `	if( sFile.nByte < 1 ){` |
|        - | 11616 | `		/* Empty string,return NULL */` |
|      ! 0 | 11617 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11618 | `		return SXRET_OK;` |
|        - | 11619 | `	}` |
|        - | 11620 | `	/* Open,compile and execute the desired script */` |
|     7632 | 11621 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 11622 | `	if( rc != SXRET_OK ){` |
|        - | 11623 | `		/* Emit a warning and return false */` |
|        3 | 11624 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11625 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11626 | `	}` |
|     7632 | 11627 | `	return SXRET_OK;` |
|     3817 | 11628 |  |
|        - | 11629 | `/*` |
|        - | 11630 | ` * include_once:` |
|        - | 11631 | ` *  According to the PHP reference manual.` |
|        - | 11632 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11633 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11634 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11635 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11636 | ` *   just once.` |
|        - | 11637 | ` */` |
|        4 | 11638 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11639 |  |
|        - | 11640 | `	SyString sFile;` |
|        - | 11641 | `	sxi32 rc;` |
|        5 | 11642 | `	if( nArg < 1 ){` |
|        - | 11643 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11644 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11645 | `		return SXRET_OK;` |
|        - | 11646 | `	}` |
|        - | 11647 | `	/* File to include */` |
|        5 | 11648 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11649 | `	if( sFile.nByte < 1 ){` |
|        - | 11650 | `		/* Empty string,return NULL */` |
|      ! 0 | 11651 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11652 | `		return SXRET_OK;` |
|        - | 11653 | `	}` |
|        - | 11654 | `	/* Open,compile and execute the desired script */` |
|        5 | 11655 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11656 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11657 | `		/* File already included,return TRUE */` |
|        3 | 11658 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11659 | `		return SXRET_OK;` |
|        - | 11660 | `	}` |
|        3 | 11661 | `	if( rc != SXRET_OK ){` |
|        - | 11662 | `		/* Emit a warning and return false */` |
|      ! 0 | 11663 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11664 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11665 | ` 	}` |
|        3 | 11666 | `	return SXRET_OK;` |
|        3 | 11667 |  |
|        - | 11668 | `/*` |
|        - | 11669 | ` * require.` |
|        - | 11670 | ` *  According to the PHP reference manual.` |
|        - | 11671 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11672 | ` *   also produce a fatal level error.` |
|        - | 11673 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11674 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11675 | ` */` |
|        4 | 11676 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11677 |  |
|        - | 11678 | `	SyString sFile;` |
|        - | 11679 | `	sxi32 rc;` |
|        5 | 11680 | `	if( nArg < 1 ){` |
|        - | 11681 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11682 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11683 | `		return SXRET_OK;` |
|        - | 11684 | `	}` |
|        - | 11685 | `	/* File to include */` |
|        5 | 11686 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11687 | `	if( sFile.nByte < 1 ){` |
|        - | 11688 | `		/* Empty string,return NULL */` |
|      ! 0 | 11689 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11690 | `		return SXRET_OK;` |
|        - | 11691 | `	}` |
|        - | 11692 | `	/* Open,compile and execute the desired script */` |
|        5 | 11693 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11694 | `	if( rc != SXRET_OK ){` |
|        - | 11695 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11696 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11697 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11698 | `		return PH7_ABORT;` |
|        - | 11699 | `	}` |
|        5 | 11700 | `	return SXRET_OK;` |
|        3 | 11701 |  |
|        - | 11702 | `/*` |
|        - | 11703 | ` * require_once:` |
|        - | 11704 | ` *  According to the PHP reference manual.` |
|        - | 11705 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11706 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11707 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11708 | ` *   and how it differs from its non _once siblings.` |
|        - | 11709 | ` */` |
|        4 | 11710 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11711 |  |
|        - | 11712 | `	SyString sFile;` |
|        - | 11713 | `	sxi32 rc;` |
|        5 | 11714 | `	if( nArg < 1 ){` |
|        - | 11715 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11716 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11717 | `		return SXRET_OK;` |
|        - | 11718 | `	}` |
|        - | 11719 | `	/* File to include */` |
|        5 | 11720 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11721 | `	if( sFile.nByte < 1 ){` |
|        - | 11722 | `		/* Empty string,return NULL */` |
|      ! 0 | 11723 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11724 | `		return SXRET_OK;` |
|        - | 11725 | `	}` |
|        - | 11726 | `	/* Open,compile and execute the desired script */` |
|        5 | 11727 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11728 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11729 | `		/* File already included,return TRUE */` |
|        3 | 11730 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11731 | `		return SXRET_OK;` |
|        - | 11732 | `	}` |
|        3 | 11733 | `	if( rc != SXRET_OK ){` |
|        - | 11734 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11735 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11736 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11737 | `		return PH7_ABORT;` |
|        - | 11738 | `	}` |
|        3 | 11739 | `	return SXRET_OK;` |
|        3 | 11740 |  |
|        - | 11741 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11742 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11743 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11744 | `/* Table of built-in VM functions. */` |
|        - | 11745 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11746 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11747 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11748 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11749 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11750 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11751 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11752 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11753 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11754 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11755 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11756 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11757 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11758 | `	    /* Constants management */` |
|        - | 11759 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11760 | `	{ "define",   vm_builtin_define               },` |
|        - | 11761 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11762 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11763 | `	   /* Class/Object functions */` |
|        - | 11764 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11765 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11766 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11767 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11768 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11769 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11770 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11771 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11772 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11773 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11774 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11775 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11776 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11777 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11778 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11779 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11780 | `	   /* Random numbers/strings generators */` |
|        - | 11781 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11782 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11783 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11784 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11785 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11786 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11787 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11788 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11789 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11790 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11791 | `	   /* Language constructs functions */` |
|        - | 11792 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11793 | `	{ "print", vm_builtin_print                   },` |
|        - | 11794 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11795 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11796 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11797 | `	  /* Variable handling functions */` |
|        - | 11798 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11799 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11800 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11801 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11802 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11803 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11804 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11805 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11806 | `	  /* Ouput control functions */` |
|        - | 11807 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11808 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11809 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11810 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11811 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11812 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11813 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11814 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11815 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11816 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11817 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11818 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11819 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11820 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11821 | `	  /* Assertion functions */` |
|        - | 11822 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11823 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11824 | `	  /* Error reporting functions */` |
|        - | 11825 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11826 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11827 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11828 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11829 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11830 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11831 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11832 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11833 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11834 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11835 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11836 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11837 | `	  /* Release info */` |
|        - | 11838 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11839 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11840 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11841 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11842 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11843 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11844 | `	  /* hashmap */` |
|        - | 11845 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11846 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11847 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11848 | `	  /* URL related function */` |
|        - | 11849 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11850 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11851 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11852 | `	   /* XML processing functions */` |
|        - | 11853 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11854 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11855 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11856 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11857 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11858 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11859 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11860 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11861 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11862 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11863 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11864 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11865 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11866 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11867 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11868 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 11869 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 11870 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 11871 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 11872 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 11873 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 11874 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11875 | `	   /* UTF-8 encoding/decoding */` |
|        - | 11876 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 11877 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 11878 | `	   /* Command line processing */` |
|        - | 11879 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 11880 | `	   /* JSON encoding/decoding */` |
|        - | 11881 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 11882 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 11883 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 11884 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 11885 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 11886 | `	   /* Files/URI inclusion facility */` |
|        - | 11887 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 11888 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 11889 | `	{ "include",      vm_builtin_include          },` |
|        - | 11890 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 11891 | `	{ "require",      vm_builtin_require          },` |
|        - | 11892 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 11893 | `};` |
|        - | 11894 | `/*` |
|        - | 11895 | ` * Register the built-in VM functions defined above.` |
|        - | 11896 | ` */` |
|     2536 | 11897 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 11898 |  |
|        - | 11899 | `	sxi32 rc;` |
|        - | 11900 | `	sxu32 n;` |
|   317002 | 11901 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 11902 | `		/* Note that these special functions have access` |
|        - | 11903 | `		 * to the underlying virtual machine as their` |
|        - | 11904 | `		 * private data.` |
|        - | 11905 | `		 */` |
|   314466 | 11906 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   314466 | 11907 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11908 | `			return rc;` |
|        - | 11909 | `		}` |
|   157234 | 11910 | `	}` |
|     2538 | 11911 | `	return SXRET_OK;` |
|     1270 | 11912 |  |
|        - | 11913 | `/*` |
|        - | 11914 | ` * Check if the given name refer to an installed class.` |
|        - | 11915 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 11916 | ` */` |
|    29536 | 11917 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 11918 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 11919 | `	const char *zName,  /* Name of the target class */` |
|        - | 11920 | `	sxu32 nByte,        /* zName length */` |
|        - | 11921 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 11922 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 11923 | `						 */` |
|        - | 11924 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 11925 | `	)` |
|        2 | 11926 |  |
|        - | 11927 | `	SyHashEntry *pEntry;` |
|        - | 11928 | `	ph7_class *pClass;` |
|    14768 | 11929 | `	SXUNUSED(iNest);` |
|        - | 11930 | `	/* Exact class lookup.` |
|        - | 11931 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 11932 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    29538 | 11933 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    29538 | 11934 | `	if( pEntry == 0 ){` |
|       10 | 11935 | `		return 0;` |
|        - | 11936 | `	}` |
|    29530 | 11937 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    29530 | 11938 | `	if( !iLoadable ){` |
|    28344 | 11939 | `		return pClass;` |
|        - | 11940 | `	}` |
|        - | 11941 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1188 | 11942 | `	while(pClass){` |
|     1188 | 11943 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1188 | 11944 | `			return pClass;` |
|        - | 11945 | `		}` |
|      ! 0 | 11946 | `		pClass = pClass->pNextName;` |
|      ! 0 | 11947 | `	}` |
|      ! 0 | 11948 | `	return 0;` |
|    14770 | 11949 |  |
|        - | 11950 | `/*` |
|        - | 11951 | ` * Reference Table Implementation` |
|        - | 11952 | ` * Status: stable <chm@symisc.net>` |
|        - | 11953 | ` * Intro` |
|        - | 11954 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 11955 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 11956 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 11957 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 11958 | ` *  Refer to the official for more information on this powerful` |
|        - | 11959 | ` *  extension.` |
|        - | 11960 | ` */` |
|        - | 11961 | `/*` |
|        - | 11962 | ` * Allocate a new reference entry.` |
|        - | 11963 | ` */` |
|  3021428 | 11964 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 11965 |  |
|        - | 11966 | `	VmRefObj *pRef;` |
|        - | 11967 | `	/* Allocate a new instance */` |
|  3021430 | 11968 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3021430 | 11969 | `	if( pRef == 0 ){` |
|      ! 0 | 11970 | `		return 0;` |
|        - | 11971 | `	}` |
|        - | 11972 | `	/* Zero the structure */` |
|  3021430 | 11973 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 11974 | `	/* Initialize fields */` |
|  3021430 | 11975 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3021430 | 11976 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3021430 | 11977 | `	pRef->nIdx = nIdx;` |
|  3021430 | 11978 | `	return pRef;` |
|  1510716 | 11979 |  |
|        - | 11980 | `/*` |
|        - | 11981 | ` * Default hash function used by the reference table` |
|        - | 11982 | ` * for lookup/insertion operations.` |
|        - | 11983 | ` */` |
| 16736343 | 11984 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 11985 |  |
|        - | 11986 | `	/* Calculate the hash based on the memory object index */` |
| 16736345 | 11987 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 11988 |  |
|        - | 11989 | `/*` |
|        - | 11990 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 11991 | ` * in the reference table.` |
|        - | 11992 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 11993 | ` * otherwise.` |
|        - | 11994 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11995 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11996 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11997 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11998 | ` * Refer to the official for more information on this powerful` |
|        - | 11999 | ` * extension.` |
|        - | 12000 | ` */` |
|  9014674 | 12001 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12002 |  |
|        - | 12003 | `	VmRefObj *pRef;` |
|        - | 12004 | `	sxu32 nBucket;` |
|        - | 12005 | `	/* Point to the appropriate bucket */` |
|  9014676 | 12006 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12007 | `	/* Perform the lookup */` |
|  9014676 | 12008 | `	pRef = pVm->apRefObj[nBucket];` |
| 19332665 | 12009 | `	for(;;){` |
| 38659150 | 12010 | `		if( pRef == 0 ){` |
|  3100862 | 12011 | `			break;` |
|        - | 12012 | `		}` |
| 35558290 | 12013 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12014 | `			/* Entry found */` |
|  5913816 | 12015 | `			return pRef;` |
|        - | 12016 | `		}` |
|        - | 12017 | `		/* Point to the next entry */` |
| 29644476 | 12018 | `		pRef = pRef->pNextCollide;` |
|        2 | 12019 | `	}` |
|        - | 12020 | `	/* No such entry,return NULL */` |
|  3100862 | 12021 | `	return 0;` |
|  4507339 | 12022 |  |
|        - | 12023 | `/*` |
|        - | 12024 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12025 | ` *` |
|        - | 12026 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12027 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12028 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12029 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12030 | ` * Refer to the official for more information on this powerful` |
|        - | 12031 | ` * extension.` |
|        - | 12032 | ` */` |
|  3021428 | 12033 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12034 |  |
|        - | 12035 | `	sxu32 nBucket;` |
|  3021430 | 12036 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12037 | `		VmRefObj **apNew;` |
|        - | 12038 | `		sxu32 nNew;` |
|        - | 12039 | `		/* Allocate a larger table */` |
|     4314 | 12040 | `		nNew = pVm->nRefSize << 1;` |
|     4314 | 12041 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4314 | 12042 | `		if( apNew ){` |
|     4314 | 12043 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12044 | `			sxu32 n;` |
|        - | 12045 | `			/* Zero the structure */` |
|     4314 | 12046 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12047 | `			/* Rehash all referenced entries */` |
|  2843730 | 12048 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12049 | `				/* Remove old collision links */` |
|  2839418 | 12050 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12051 | `				/* Point to the appropriate bucket */` |
|  2839418 | 12052 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12053 | `				/* Insert the entry  */` |
|  2839418 | 12054 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2839418 | 12055 | `				if( apNew[nBucket] ){` |
|  2298896 | 12056 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12057 | `				}` |
|  2839418 | 12058 | `				apNew[nBucket] = pEntry;` |
|        - | 12059 | `				/* Point to the next entry */` |
|  2839418 | 12060 | `				pEntry = pEntry->pNext;` |
|  1419710 | 12061 | `			}` |
|        - | 12062 | `			/* Release the old table */` |
|     4314 | 12063 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12064 | `			/* Install the new one */` |
|     4314 | 12065 | `			pVm->apRefObj = apNew;` |
|     4314 | 12066 | `			pVm->nRefSize = nNew;` |
|     2156 | 12067 | `		}` |
|     2156 | 12068 | `	}` |
|        - | 12069 | `	/* Point to the appropriate bucket */` |
|  3021430 | 12070 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12071 | `	/* Insert the entry */` |
|  3021430 | 12072 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3021430 | 12073 | `	if( pVm->apRefObj[nBucket] ){` |
|  2502947 | 12074 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1251470 | 12075 | `	}` |
|  3021430 | 12076 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3021430 | 12077 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3021430 | 12078 | `	pVm->nRefUsed++;` |
|  3021430 | 12079 | `	return SXRET_OK;` |
|        2 | 12080 |  |
|        - | 12081 | `/*` |
|        - | 12082 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12083 | ` * the reference table.` |
|        - | 12084 | ` * This function is invoked when the user perform an unset` |
|        - | 12085 | ` * call [i.e: unset($var); ].` |
|        - | 12086 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12087 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12088 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12089 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12090 | ` * Refer to the official for more information on this powerful` |
|        - | 12091 | ` * extension.` |
|        - | 12092 | ` */` |
|  2985336 | 12093 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12094 |  |
|        - | 12095 | `	ph7_hashmap_node **apNode;` |
|        - | 12096 | `	SyHashEntry **apEntry;` |
|        - | 12097 | `	sxu32 n;` |
|        - | 12098 | `	/* Point to the reference table */` |
|  2985338 | 12099 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2985338 | 12100 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12101 | `	/* Unlink the entry from the reference table */` |
|  3070636 | 12102 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85300 | 12103 | `		if( apEntry[n] ){` |
|    85250 | 12104 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42624 | 12105 | `		}` |
|    42651 | 12106 | `	}` |
|  5888068 | 12107 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2902732 | 12108 | `		if( apNode[n] ){` |
|     6794 | 12109 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 12110 | `		}` |
|  1451367 | 12111 | `	}` |
|  2985338 | 12112 | `	if( pRef->pPrevCollide ){` |
|  1124513 | 12113 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   562601 | 12114 | `	}else{` |
|  1860827 | 12115 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12116 | `	}` |
|  2985338 | 12117 | `	if( pRef->pNextCollide ){` |
|  1691678 | 12118 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   845824 | 12119 | `	}` |
|  2985338 | 12120 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12121 | `	/* Release the node */` |
|  2985338 | 12122 | `	SySetRelease(&pRef->aReference);` |
|  2985338 | 12123 | `	SySetRelease(&pRef->aArrEntries);` |
|  2985338 | 12124 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2985338 | 12125 | `	pVm->nRefUsed--;` |
|  2985338 | 12126 | `	return SXRET_OK;` |
|        2 | 12127 |  |
|        - | 12128 | `/*` |
|        - | 12129 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12130 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12131 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12132 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12133 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12134 | ` * Refer to the official for more information on this powerful` |
|        - | 12135 | ` * extension.` |
|        - | 12136 | ` */` |
|  3053906 | 12137 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12138 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12139 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12140 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12141 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12142 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12143 | `	)` |
|        2 | 12144 |  |
|  3053908 | 12145 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12146 | `	VmRefObj *pRef;` |
|        - | 12147 | `	/* Check if the referenced object already exists */` |
|  3053908 | 12148 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3053908 | 12149 | `	if( pRef == 0 ){` |
|        - | 12150 | `		/* Create a new entry */` |
|  3021430 | 12151 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3021430 | 12152 | `		if( pRef == 0 ){` |
|      ! 0 | 12153 | `			return SXERR_MEM;` |
|        - | 12154 | `		}` |
|  3021430 | 12155 | `		pRef->iFlags = iFlags;` |
|        - | 12156 | `		/* Install the entry */` |
|  3021430 | 12157 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1510714 | 12158 | `	}` |
|  3053908 | 12159 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3053908 | 12160 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12161 | `		VmSlot sRef;` |
|        - | 12162 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12163 | `		 * be deleted when we leave this frame.` |
|        - | 12164 | `		 */` |
|    79518 | 12165 | `		sRef.nIdx = nIdx;` |
|    79518 | 12166 | `		sRef.pUserData = pEntry;` |
|    79518 | 12167 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12168 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12169 | `		}` |
|    39758 | 12170 | `	}` |
|  3053908 | 12171 | `	if( pEntry ){` |
|        - | 12172 | `		/* Address of the hash-entry */` |
|   111804 | 12173 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55901 | 12174 | `	}` |
|  3053908 | 12175 | `	if( pMapEntry ){` |
|        - | 12176 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2937132 | 12177 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1468565 | 12178 | `	}` |
|  3053908 | 12179 | `	return SXRET_OK;` |
|  1526955 | 12180 |  |
|        - | 12181 | `/*` |
|        - | 12182 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12183 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12184 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12185 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12186 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12187 | ` * Refer to the official for more information on this powerful` |
|        - | 12188 | ` * extension.` |
|        - | 12189 | ` */` |
|  2975426 | 12190 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12191 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12192 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12193 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12194 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12195 | `	)` |
|        2 | 12196 |  |
|        - | 12197 | `	VmRefObj *pRef;` |
|        - | 12198 | `	sxu32 n;` |
|        - | 12199 | `	/* Check if the referenced object already exists */` |
|  2975428 | 12200 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2975428 | 12201 | `	if( pRef == 0 ){` |
|        - | 12202 | `		/* Not such entry */` |
|    79428 | 12203 | `		return SXERR_NOTFOUND;` |
|        - | 12204 | `	}` |
|        - | 12205 | `	/* Remove the desired entry */` |
|  2896002 | 12206 | `	if( pEntry ){` |
|        - | 12207 | `		SyHashEntry **apEntry;` |
|       56 | 12208 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12209 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12210 | `			if( apEntry[n] == pEntry ){` |
|        - | 12211 | `				/* Nullify the entry */` |
|       56 | 12212 | `				apEntry[n] = 0;` |
|        - | 12213 | `				/*` |
|        - | 12214 | `				 * NOTE:` |
|        - | 12215 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12216 | `				 * we avoid wasting spaces.` |
|        - | 12217 | `				 */` |
|       27 | 12218 | `			}` |
|       79 | 12219 | `		}` |
|       27 | 12220 | `	}` |
|  2896002 | 12221 | `	if( pMapEntry ){` |
|        - | 12222 | `		ph7_hashmap_node **apNode;` |
|  2895948 | 12223 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5791988 | 12224 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2896042 | 12225 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12226 | `				/* nullify the entry */` |
|  2895948 | 12227 | `				apNode[n] = 0;` |
|  1447973 | 12228 | `			}` |
|  1448022 | 12229 | `		}` |
|  1447973 | 12230 | `	}` |
|  2896002 | 12231 | `	return SXRET_OK;` |
|  1487715 | 12232 |  |
|        - | 12233 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12234 | `/*` |
|        - | 12235 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12236 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12237 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12238 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12239 | ` * For more information on how to register IO stream devices,please` |
|        - | 12240 | ` * refer to the official documentation.` |
|        - | 12241 | ` */` |
|    23706 | 12242 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12243 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12244 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12245 | `	int nByte              /* *pzDevice length*/` |
|        - | 12246 | `	)` |
|        2 | 12247 |  |
|        - | 12248 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12249 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12250 | `	SyString sDev,sCur;` |
|        - | 12251 | `	sxu32 n,nEntry;` |
|        - | 12252 | `	int rc;` |
|        - | 12253 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23708 | 12254 | `	zNext = zCur = zIn = *pzDevice;` |
|    23708 | 12255 | `	zEnd = &zIn[nByte];` |
|  1513861 | 12256 | `	while( zIn < zEnd ){` |
|  1490157 | 12257 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12258 | `			/* Got one */` |
|        3 | 12259 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12260 | `			break;` |
|        - | 12261 | `		}` |
|        - | 12262 | `		/* Advance the cursor */` |
|  1490155 | 12263 | `		zIn++;` |
|        2 | 12264 | `	}` |
|    23708 | 12265 | `	if( zIn >= zEnd ){` |
|        - | 12266 | `		/* No such scheme,return the default stream */` |
|    23706 | 12267 | `		return pVm->pDefStream;` |
|        - | 12268 | `	}` |
|        3 | 12269 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12270 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12271 | `	SyStringFullTrim(&sDev);` |
|        - | 12272 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12273 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12274 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12275 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12276 | `		pStream = apStream[n];` |
|        3 | 12277 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12278 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12279 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12280 | `		if( rc == 0 ){` |
|        - | 12281 | `			/* Stream device found */` |
|        3 | 12282 | `			*pzDevice = zNext;` |
|        3 | 12283 | `			return pStream;` |
|        - | 12284 | `		}` |
|      ! 0 | 12285 | `	}` |
|        - | 12286 | `	/* No such stream,return NULL */` |
|      ! 0 | 12287 | `	return 0;` |
|    11855 | 12288 |  |
|        - | 12289 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12290 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12291 |  |
